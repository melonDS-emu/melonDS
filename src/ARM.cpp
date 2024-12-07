/*
    Copyright 2016-2024 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include "NDS.h"
#include "DSi.h"
#include "ARM.h"
#include "ARMInterpreter.h"
#include "AREngine.h"
#include "ARMJIT.h"
#include "Platform.h"
#include "GPU.h"
#include "ARMJIT_Memory.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

#ifdef GDBSTUB_ENABLED
void ARM::GdbCheckA()
{
    if (!IsSingleStep && !BreakReq)
    { // check if eg. break signal is incoming etc.
        Gdb::StubState st = GdbStub.Enter(false, Gdb::TgtStatus::NoEvent, ~(u32)0u, BreakOnStartup);
        BreakOnStartup = false;
        IsSingleStep = st == Gdb::StubState::Step;
        BreakReq = st == Gdb::StubState::Attach || st == Gdb::StubState::Break;
    }
}
void ARM::GdbCheckB()
{
    if (IsSingleStep || BreakReq)
    { // use else here or we single-step the same insn twice in gdb
        u32 pc_real = R[15] - ((CPSR & 0x20) ? 2 : 4);
        Gdb::StubState st = GdbStub.Enter(true, Gdb::TgtStatus::SingleStep, pc_real);
        IsSingleStep = st == Gdb::StubState::Step;
        BreakReq = st == Gdb::StubState::Attach || st == Gdb::StubState::Break;
    }
}
void ARM::GdbCheckC()
{
    u32 pc_real = R[15] - ((CPSR & 0x20) ? 2 : 4);
    Gdb::StubState st = GdbStub.CheckBkpt(pc_real, true, true);
    if (st != Gdb::StubState::CheckNoHit)
    {
        IsSingleStep = st == Gdb::StubState::Step;
        BreakReq = st == Gdb::StubState::Attach || st == Gdb::StubState::Break;
    }
    else GdbCheckB();
}
#else
void ARM::GdbCheckA() {}
void ARM::GdbCheckB() {}
void ARM::GdbCheckC() {}
#endif


// instruction timing notes
//
// * simple instruction: 1S (code)
// * LDR: 1N+1N+1I (code/data/internal)
// * STR: 1N+1N (code/data)
// * LDM: 1N+1N+(n-1)S+1I
// * STM: 1N+1N+(n-1)S
// * MUL/etc: 1N+xI (code/internal)
// * branch: 1N+1S (code/code) (pipeline refill)
//
// MUL/MLA seems to take 1I on ARM9



const u32 ARM::ConditionTable[16] =
{
    0xF0F0, // EQ
    0x0F0F, // NE
    0xCCCC, // CS
    0x3333, // CC
    0xFF00, // MI
    0x00FF, // PL
    0xAAAA, // VS
    0x5555, // VC
    0x0C0C, // HI
    0xF3F3, // LS
    0xAA55, // GE
    0x55AA, // LT
    0x0A05, // GT
    0xF5FA, // LE
    0xFFFF, // AL
    0x0000  // NE
};

ARM::ARM(u32 num, bool jit, std::optional<GDBArgs> gdb, melonDS::NDS& nds) :
#ifdef GDBSTUB_ENABLED
    GdbStub(this),
    BreakOnStartup(false),
#endif
    Num(num), // well uh
    NDS(nds)
{
    SetGdbArgs(jit ? gdb : std::nullopt);
}

ARM::~ARM()
{
    // dorp
}

ARMv5::ARMv5(melonDS::NDS& nds, std::optional<GDBArgs> gdb, bool jit) : ARM(0, jit, gdb, nds)
{
    DTCM = NDS.JIT.Memory.GetARM9DTCM();

    PU_Map = PU_PrivMap;
}

ARMv4::ARMv4(melonDS::NDS& nds, std::optional<GDBArgs> gdb, bool jit) : ARM(1, jit, gdb, nds)
{
    //
}

ARMv5::~ARMv5()
{
    // DTCM is owned by Memory, not going to delete it
}

void ARM::SetGdbArgs(std::optional<GDBArgs> gdb)
{
#ifdef GDBSTUB_ENABLED
    GdbStub.Close();
    if (gdb)
    {
        int port = Num ? gdb->PortARM7 : gdb->PortARM9;
        GdbStub.Init(port);
        BreakOnStartup = Num ? gdb->ARM7BreakOnStartup : gdb->ARM9BreakOnStartup;
    }
    IsSingleStep = false;
#endif
}

void ARM::Reset()
{
    Cycles = 0;
    Halted = 0;
    DataCycles = 0;

    IRQ = 0;

    for (int i = 0; i < 16; i++)
        R[i] = 0;

    CPSR = 0x000000D3;

    for (int i = 0; i < 7; i++)
        R_FIQ[i] = 0;
    for (int i = 0; i < 2; i++)
    {
        R_SVC[i] = 0;
        R_ABT[i] = 0;
        R_IRQ[i] = 0;
        R_UND[i] = 0;
    }

    R_FIQ[7] = 0x00000010;
    R_SVC[2] = 0x00000010;
    R_ABT[2] = 0x00000010;
    R_IRQ[2] = 0x00000010;
    R_UND[2] = 0x00000010;

    ExceptionBase = Num ? 0x00000000 : 0xFFFF0000;

    CodeMem.Mem = NULL;

#ifdef JIT_ENABLED
    FastBlockLookup = NULL;
    FastBlockLookupStart = 0;
    FastBlockLookupSize = 0;
#endif

#ifdef GDBSTUB_ENABLED
    IsSingleStep = false;
    BreakReq = false;
#endif

    MainRAMTimestamp = 0;

    memset(&MRTrack, 0, sizeof(MRTrack));

    FuncQueueFill = 0;
    FuncQueueEnd = 0;
    FuncQueueProg = 0;
    FuncQueueActive = false;
    ExecuteCycles = 0;

    // zorp
    JumpTo(ExceptionBase);
}

void ARMv5::Reset()
{
    FuncQueue[0] = &ARMv5::StartExec;

    PU_Map = PU_PrivMap;
    Store = false;
    
    ITCMTimestamp = 0;
    TimestampActual = 0;
    ILCurrReg = 16;
    ILPrevReg = 16;

    ICacheStreamPtr = 7;
    DCacheStreamPtr = 7;

    WBWritePointer = 16;
    WBFillPointer = 0;
    WBDelay = 0;
    WBTimestamp = 0;
    WBReleaseTS = 0;
    WBLastRegion = Mem9_Null;
    WBWriting = false;
    WBInitialTS = 0;

    ARM::Reset();
}

void ARMv4::Reset()
{
    FuncQueue[0] = &ARMv4::StartExec;
    Nonseq = true;

    ARM::Reset();
}


void ARM::DoSavestate(Savestate* file)
{
    file->Section((char*)(Num ? "ARM7" : "ARM9"));

    file->Var32((u32*)&Cycles);
    //file->Var32((u32*)&CyclesToRun);

    // hack to make save states compatible
    u32 halted = Halted;
    file->Var32(&halted);
    Halted = halted;

    file->VarArray(R, 16*sizeof(u32));
    file->Var32(&CPSR);
    file->VarArray(R_FIQ, 8*sizeof(u32));
    file->VarArray(R_SVC, 3*sizeof(u32));
    file->VarArray(R_ABT, 3*sizeof(u32));
    file->VarArray(R_IRQ, 3*sizeof(u32));
    file->VarArray(R_UND, 3*sizeof(u32));
    file->Var64(&CurInstr);
#ifdef JIT_ENABLED
    if (file->Saving && NDS.IsJITEnabled())
    {
        // hack, the JIT doesn't really pipeline
        // but we still want JIT save states to be
        // loaded while running the interpreter
        FillPipeline();
    }
#endif
    file->VarArray(NextInstr, 2*sizeof(u64));

    file->Var32(&ExceptionBase);

    if (!file->Saving)
    {
        CPSR |= 0x00000010;
        R_FIQ[7] |= 0x00000010;
        R_SVC[2] |= 0x00000010;
        R_ABT[2] |= 0x00000010;
        R_IRQ[2] |= 0x00000010;
        R_UND[2] |= 0x00000010;

        if (!Num)
        {
            SetupCodeMem(R[15]); // should fix it
            ((ARMv5*)this)->RegionCodeCycles = ((ARMv5*)this)->MemTimings[R[15] >> 12][2];

            if ((CPSR & 0x1F) == 0x10)
                ((ARMv5*)this)->PU_Map = ((ARMv5*)this)->PU_UserMap;
            else
                ((ARMv5*)this)->PU_Map = ((ARMv5*)this)->PU_PrivMap;
        }
        else
        {
            CodeRegion = R[15] >> 24;
            CodeCycles = R[15] >> 15; // cheato
        }
    }
}

void ARMv5::DoSavestate(Savestate* file)
{
    ARM::DoSavestate(file);
    CP15DoSavestate(file);
}


void ARM::SetupCodeMem(u32 addr)
{
    if (!Num)
    {
        ((ARMv5*)this)->GetCodeMemRegion(addr, &CodeMem);
    }
    else
    {
        // not sure it's worth it for the ARM7
        // esp. as everything there generally runs on WRAM
        // and due to how it's mapped, we can't use this optimization
        //NDS::ARM7GetMemRegion(addr, false, &CodeMem);
    }
}

void ARMv5::JumpTo(u32 addr, bool restorecpsr, u8 R15)
{
    //printf("JUMP! %08X %i %i\n", addr, restorecpsr, R15);
    NDS.MonitorARM9Jump(addr);
    
    BranchRestore = restorecpsr;
    BranchUpdate = R15;
    BranchAddr = addr;
    QueueFunction(&ARMv5::JumpTo_2);
}

void ARMv5::JumpTo_2()
{
    if (BranchUpdate)
    {
        if (CP15Control & (1<<15))
        {
            if (BranchUpdate == 1) BranchAddr = R[15] & ~1;
            else                   BranchAddr = R[15] |  1;
        }
        else BranchAddr = R[15];
    }

    if (BranchRestore)
    {
        RestoreCPSR();

        if (CPSR & 0x20) BranchAddr |=  0x1;
        else             BranchAddr &= ~0x1;
    }

    // aging cart debug crap
    //if (addr == 0x0201764C) printf("capture test %d: R1=%08X\n", R[6], R[1]);
    //if (addr == 0x020175D8) printf("capture test %d: res=%08X\n", R[6], R[0]);

    // jumps count as nonsequential accesses on the instruction bus on the arm9
    // thus it requires waiting for the current ICache line fill to complete before continuing
    if (ICacheStreamPtr < 7)
    {
        u64 fillend = ICacheStreamTimes[6] + 1;
        if (NDS.ARM9Timestamp < fillend) NDS.ARM9Timestamp = fillend;
        ICacheStreamPtr = 7;
    }

    if (BranchAddr & 0x1)
    {
        BranchAddr &= ~0x1;
        R[15] = BranchAddr+2;

        CPSR |= 0x20;

        // two-opcodes-at-once fetch
        // doesn't matter if we put garbage in the MSbs there
        if (BranchAddr & 0x2)
        {
            CodeRead32(BranchAddr-2);

            QueueFunction(&ARMv5::JumpTo_3A);
        }
        else
        {
            CodeRead32(BranchAddr);

            QueueFunction(&ARMv5::JumpTo_3B);
        }
    }
    else
    {
        BranchAddr &= ~0x3;
        R[15] = BranchAddr+4;

        CPSR &= ~0x20;

        CodeRead32(BranchAddr);

        QueueFunction(&ARMv5::JumpTo_3C);
    }
}

void ARMv5::JumpTo_3A()
{
    NextInstr[0] = RetVal >> 16;
    CodeRead32(BranchAddr+2);

    QueueFunction(&ARMv5::JumpTo_4);
}

void ARMv5::JumpTo_3B()
{
    NextInstr[0] = RetVal;
    NextInstr[1] = NextInstr[0] >> 16;
}

void ARMv5::JumpTo_3C()
{
    NextInstr[0] = RetVal;
    CodeRead32(BranchAddr+4);

    QueueFunction(&ARMv5::JumpTo_4);
}

void ARMv5::JumpTo_4()
{
    NextInstr[1] = RetVal;
}

void ARMv4::JumpTo(u32 addr, bool restorecpsr, u8 R15)
{
    //printf("JUMP! %08X %08X %i %i\n", addr, R[15], restorecpsr, R15);
    BranchRestore = restorecpsr;
    BranchUpdate = R15;
    BranchAddr = addr;
    QueueFunction(&ARMv4::JumpTo_2);
}

void ARMv4::JumpTo_2()
{
    if (BranchUpdate)
    {
        if (BranchUpdate == 1) BranchAddr = R[15] & ~1;
        else                   BranchAddr = R[15] |  1;
    }

    if (BranchRestore)
    {
        RestoreCPSR();

        if (CPSR & 0x20) BranchAddr |=  0x1;
        else             BranchAddr &= ~0x1;
    }
    
    //printf("JUMP2! %08X\n", BranchAddr);

    if (BranchAddr & 0x1)
    {
        BranchAddr &= ~0x1;
        R[15] = BranchAddr+2;

        CPSR |= 0x20;

        Nonseq = true;
        CodeRead16(BranchAddr);
        QueueFunction(&ARMv4::JumpTo_3A);
    }
    else
    {
        BranchAddr &= ~0x3;
        R[15] = BranchAddr+4;

        CPSR &= ~0x20;
        
        Nonseq = true;
        CodeRead32(BranchAddr);
        QueueFunction(&ARMv4::JumpTo_3B);
    }
}

void ARMv4::JumpTo_3A()
{
    NextInstr[0] = RetVal;
    Nonseq = false;
    CodeRead16(BranchAddr+2);
    QueueFunction(&ARMv4::UpdateNextInstr1);
}

void ARMv4::JumpTo_3B()
{
    NextInstr[0] = RetVal;
    Nonseq = false;
    CodeRead32(BranchAddr+4);
    QueueFunction(&ARMv4::UpdateNextInstr1);
}

void ARM::RestoreCPSR()
{
    u32 oldcpsr = CPSR;

    switch (CPSR & 0x1F)
    {
    case 0x11:
        CPSR = R_FIQ[7];
        break;

    case 0x12:
        CPSR = R_IRQ[2];
        break;

    case 0x13:
        CPSR = R_SVC[2];
        break;

    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        CPSR = R_ABT[2];
        break;

    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
        CPSR = R_UND[2];
        break;

    default:
        Log(LogLevel::Warn, "!! attempt to restore CPSR under bad mode %02X, %08X\n", CPSR&0x1F, R[15]);
        break;
    }

    CPSR |= 0x00000010;

    UpdateMode(oldcpsr, CPSR);
}

void ARM::UpdateMode(u32 oldmode, u32 newmode, bool phony)
{
    if ((oldmode & 0x1F) == (newmode & 0x1F)) return;

    switch (oldmode & 0x1F)
    {
    case 0x11:
        std::swap(R[8], R_FIQ[0]);
        std::swap(R[9], R_FIQ[1]);
        std::swap(R[10], R_FIQ[2]);
        std::swap(R[11], R_FIQ[3]);
        std::swap(R[12], R_FIQ[4]);
        std::swap(R[13], R_FIQ[5]);
        std::swap(R[14], R_FIQ[6]);
        break;

    case 0x12:
        std::swap(R[13], R_IRQ[0]);
        std::swap(R[14], R_IRQ[1]);
        break;

    case 0x13:
        std::swap(R[13], R_SVC[0]);
        std::swap(R[14], R_SVC[1]);
        break;

    case 0x17:
        std::swap(R[13], R_ABT[0]);
        std::swap(R[14], R_ABT[1]);
        break;

    case 0x1B:
        std::swap(R[13], R_UND[0]);
        std::swap(R[14], R_UND[1]);
        break;
    }

    switch (newmode & 0x1F)
    {
    case 0x11:
        std::swap(R[8], R_FIQ[0]);
        std::swap(R[9], R_FIQ[1]);
        std::swap(R[10], R_FIQ[2]);
        std::swap(R[11], R_FIQ[3]);
        std::swap(R[12], R_FIQ[4]);
        std::swap(R[13], R_FIQ[5]);
        std::swap(R[14], R_FIQ[6]);
        break;

    case 0x12:
        std::swap(R[13], R_IRQ[0]);
        std::swap(R[14], R_IRQ[1]);
        break;

    case 0x13:
        std::swap(R[13], R_SVC[0]);
        std::swap(R[14], R_SVC[1]);
        break;

    case 0x17:
        std::swap(R[13], R_ABT[0]);
        std::swap(R[14], R_ABT[1]);
        break;

    case 0x1B:
        std::swap(R[13], R_UND[0]);
        std::swap(R[14], R_UND[1]);
        break;
    }

    if ((!phony) && (Num == 0))
    {
        if ((newmode & 0x1F) == 0x10)
            ((ARMv5*)this)->PU_Map = ((ARMv5*)this)->PU_UserMap;
        else
            ((ARMv5*)this)->PU_Map = ((ARMv5*)this)->PU_PrivMap;
    }
}

template <CPUExecuteMode mode>
void ARM::TriggerIRQ()
{
    AddCycles_C();
    if (CPSR & 0x80)
        return;

    u32 oldcpsr = CPSR;
    CPSR &= ~0xFF;
    CPSR |= 0xD2;
    UpdateMode(oldcpsr, CPSR);

    R_IRQ[2] = oldcpsr;
#ifdef JIT_ENABLED
    if constexpr (mode == CPUExecuteMode::JIT)
        R[14] = R[15] + (oldcpsr & 0x20 ? 2 : 0);
    else
#endif
        R[14] = R[15] - (oldcpsr & 0x20 ? 0 : 4);
    JumpTo(ExceptionBase + 0x18);

    // ARDS cheat support
    // normally, those work by hijacking the ARM7 VBlank handler
    if (Num == 1)
    {
        if ((NDS.IF[1] & NDS.IE[1]) & (1<<IRQ_VBlank))
            NDS.AREngine.RunCheats();
    }
}
template void ARM::TriggerIRQ<CPUExecuteMode::Interpreter>();
template void ARM::TriggerIRQ<CPUExecuteMode::InterpreterGDB>();
#ifdef JIT_ENABLED
template void ARM::TriggerIRQ<CPUExecuteMode::JIT>();
#endif

void ARMv5::PrefetchAbort()
{
    AddCycles_C();
    Log(LogLevel::Warn, "ARM9: prefetch abort (%08X)\n", R[15]);

    u32 oldcpsr = CPSR;
    CPSR &= ~0xBF;
    CPSR |= 0x97;
    UpdateMode(oldcpsr, CPSR);

    R_ABT[2] = oldcpsr;
    R[14] = R[15] - (oldcpsr & 0x20 ? 0 : 4);
    JumpTo(ExceptionBase + 0x0C);
}

void ARMv5::DataAbort()
{
    Log(LogLevel::Warn, "ARM9: data abort (%08X) %08llX\n", R[15], CurInstr);

    u32 oldcpsr = CPSR;
    CPSR &= ~0xBF;
    CPSR |= 0x97;
    UpdateMode(oldcpsr, CPSR);

    R_ABT[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 4 : 0);
    JumpTo(ExceptionBase + 0x10);
}

void ARM::CheckGdbIncoming()
{
    GdbCheckA();
}

void ARMv5::StartExec()
{
    if (CPSR & 0x20) // THUMB
    {
        // prefetch
        R[15] += 2;
        CurInstr = NextInstr[0];
        NextInstr[0] = NextInstr[1];
        // code fetch is done during the execute stage cycle handling
        if (R[15] & 0x2) NullFetch = true;
        else NullFetch = false;
        PC = R[15];

        if (IRQ && !(CPSR & 0x80)) TriggerIRQ<CPUExecuteMode::Interpreter>();
        else if (CurInstr > 0xFFFFFFFF) [[unlikely]] // handle aborted instructions
        {
            PrefetchAbort();
        }
        else [[likely]] // actually execute
        {
            u32 icode = (CurInstr >> 6) & 0x3FF;
            ARMInterpreter::THUMBInstrTable[icode](this);
        }
    }
    else
    {
        // prefetch
        R[15] += 4;
        CurInstr = NextInstr[0];
        NextInstr[0] = NextInstr[1];
        // code fetch is done during the execute stage cycle handling
        NullFetch = false;
        PC = R[15];

        if (IRQ && !(CPSR & 0x80)) TriggerIRQ<CPUExecuteMode::Interpreter>();
        else if (CurInstr & ((u64)1<<63)) [[unlikely]] // handle aborted instructions
        {
            PrefetchAbort();
        }
        else if (CheckCondition(CurInstr >> 28)) [[likely]] // actually execute
        {
            u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
            ARMInterpreter::ARMInstrTable[icode](this);
        }
        else if ((CurInstr & 0xFE000000) == 0xFA000000)
        {
            ARMInterpreter::A_BLX_IMM(this);
        }
        else if ((CurInstr & 0x0FF000F0) == 0x01200070)
        {
            ARMInterpreter::A_BKPT(this); // always passes regardless of condition code
        }
        else
            AddCycles_C();
    }
}

template <CPUExecuteMode mode>
void ARMv5::Execute()
{
    if constexpr (mode == CPUExecuteMode::InterpreterGDB)
        GdbCheckB();

    if (Halted)
    {
        if (Halted == 2)
        {
            Halted = 0;
        }
        else if (NDS.HaltInterrupted(0))
        {
            Halted = 0;
            if (NDS.IME[0] & 0x1)
            {
#ifdef JIT_ENABLED
                if constexpr (mode == CPUExecuteMode::JIT) TriggerIRQ<mode>();
                else
#endif
                    IRQ = 1;
            }
        }
        else
        {
            NDS.ARM9Timestamp = NDS.ARM9Target;
            WriteBufferCheck<false>();
            return;
        }
    }

    while (NDS.ARM9Timestamp < NDS.ARM9Target)
    {
#ifdef JIT_ENABLED
        if constexpr (mode == CPUExecuteMode::JIT)
        {
            u32 instrAddr = R[15] - ((CPSR&0x20)?2:4);

            if ((instrAddr < FastBlockLookupStart || instrAddr >= (FastBlockLookupStart + FastBlockLookupSize))
                && !NDS.JIT.SetupExecutableRegion(0, instrAddr, FastBlockLookup, FastBlockLookupStart, FastBlockLookupSize))
            {
                NDS.ARM9Timestamp = NDS.ARM9Target;
                Log(LogLevel::Error, "ARMv5 PC in non executable region %08X\n", R[15]);
                return;
            }

            JitBlockEntry block = NDS.JIT.LookUpBlock(0, FastBlockLookup,
                instrAddr - FastBlockLookupStart, instrAddr);
            if (block)
                ARM_Dispatch(this, block);
            else
                NDS.JIT.CompileBlock(this);

            if (StopExecution)
            {
                // this order is crucial otherwise idle loops waiting for an IRQ won't function
                if (IRQ)
                    TriggerIRQ<mode>();

                if (Halted || IdleLoop)
                {
                    if ((Halted == 1 || IdleLoop) && NDS.ARM9Timestamp < NDS.ARM9Target)
                    {
                        Cycles = 0;
                        NDS.ARM9Timestamp = NDS.ARM9Target;
                    }
                    IdleLoop = 0;
                    break;
                }
            }
        }
        else
#endif
        {
            if constexpr (mode == CPUExecuteMode::InterpreterGDB)
                GdbCheckC(); // gdb might throw a hissy fit about this change but idc

            //printf("A:%i, F:%i, P:%i, E:%i, I:%08llX, P:%08X, 15:%08X\n", FuncQueueActive, FuncQueueFill, FuncQueueProg, FuncQueueEnd, CurInstr, PC, R[15]);

            (this->*FuncQueue[FuncQueueProg])();

            if (FuncQueueActive)
            {
                if (FuncQueueFill == FuncQueueProg)
                {
                    // we did not get a new addition to the queue; increment and reset ptrs
                    FuncQueueFill = ++FuncQueueProg;

                    // check if we're done with the queue, if so, reset everything
                    if (FuncQueueProg >= FuncQueueEnd)
                    {
                        FuncQueueFill = 0;
                        FuncQueueProg = 0;
                        FuncQueueEnd = 0;
                        FuncQueueActive = false;
                        FuncQueue[0] = &ARMv5::StartExec;
                    }
                }
                else
                {
                    // we got a new addition to the list; redo the current entry
                    FuncQueueFill = FuncQueueProg;
                }
            }
            else if (FuncQueueFill > 0) // check if we started the queue up
            {
                FuncQueueEnd = FuncQueueFill;
                FuncQueueFill = 0;
                FuncQueueActive = true;
            }
            if (MRTrack.Type != MainRAMType::Null) return; // check if we need to resolve main ram

            // TODO optimize this shit!!!
            if (!FuncQueueActive && Halted)
            {
                if (Halted == 1 && NDS.ARM9Timestamp < NDS.ARM9Target)
                {
                    NDS.ARM9Timestamp = NDS.ARM9Target;
                }
                break;
            }
            /*if (NDS::IF[0] & NDS::IE[0])
            {
                if (NDS::IME[0] & 0x1)
                    TriggerIRQ<mode>();
            }*/
        }

        //NDS.ARM9Timestamp += Cycles;
        //Cycles = 0;
    }
    WriteBufferCheck<false>();

    if (Halted == 2)
        Halted = 0;
}
template void ARMv5::Execute<CPUExecuteMode::Interpreter>();
template void ARMv5::Execute<CPUExecuteMode::InterpreterGDB>();
#ifdef JIT_ENABLED
template void ARMv5::Execute<CPUExecuteMode::JIT>();
#endif

void ARMv4::StartExec()
{
    if (CPSR & 0x20) // THUMB
    {
        // prefetch
        R[15] += 2;
        CurInstr = NextInstr[0];
        NextInstr[0] = NextInstr[1];
        CodeRead16(R[15]);
        QueueFunction(&ARMv4::UpdateNextInstr1);

        if (IRQ && !(CPSR & 0x80)) TriggerIRQ<CPUExecuteMode::Interpreter>();
        else
        {
            // actually execute
            u32 icode = (CurInstr >> 6);
            ARMInterpreter::THUMBInstrTable[icode](this);
        }
    }
    else
    {
        // prefetch
        R[15] += 4;
        CurInstr = NextInstr[0];
        NextInstr[0] = NextInstr[1];
        CodeRead32(R[15]);
        QueueFunction(&ARMv4::UpdateNextInstr1);

        if (IRQ && !(CPSR & 0x80)) TriggerIRQ<CPUExecuteMode::Interpreter>();
        else if (CheckCondition(CurInstr >> 28)) // actually execute
        {
            u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
            ARMInterpreter::ARMInstrTable[icode](this);
        }
        else
            AddCycles_C();
    }
}

template <CPUExecuteMode mode>
void ARMv4::Execute()
{
    if constexpr (mode == CPUExecuteMode::InterpreterGDB)
        GdbCheckB();
    
    if (Halted)
    {
        if (Halted == 2)
        {
            Halted = 0;
        }
        else if (NDS.HaltInterrupted(1))
        {
            Halted = 0;
            if (NDS.IME[1] & 0x1)
            {
#ifdef JIT_ENABLED
                if constexpr (mode == CPUExecuteMode::JIT) TriggerIRQ<mode>();
                else
#endif
                    IRQ = 1;
            }
        }
        else
        {
            NDS.ARM7Timestamp = NDS.ARM7Target;
            return;
        }
    }

    while (NDS.ARM7Timestamp < NDS.ARM7Target)
    {
#ifdef JIT_ENABLED
        if constexpr (mode == CPUExecuteMode::JIT)
        {
            u32 instrAddr = R[15] - ((CPSR&0x20)?2:4);

            if ((instrAddr < FastBlockLookupStart || instrAddr >= (FastBlockLookupStart + FastBlockLookupSize))
                && !NDS.JIT.SetupExecutableRegion(1, instrAddr, FastBlockLookup, FastBlockLookupStart, FastBlockLookupSize))
            {
                NDS.ARM7Timestamp = NDS.ARM7Target;
                Log(LogLevel::Error, "ARMv4 PC in non executable region %08X\n", R[15]);
                return;
            }

            JitBlockEntry block = NDS.JIT.LookUpBlock(1, FastBlockLookup,
                instrAddr - FastBlockLookupStart, instrAddr);
            if (block)
                ARM_Dispatch(this, block);
            else
                NDS.JIT.CompileBlock(this);

            if (StopExecution)
            {
                if (IRQ)
                    TriggerIRQ<mode>();

                if (Halted || IdleLoop)
                {
                    if ((Halted == 1 || IdleLoop) && NDS.ARM7Timestamp < NDS.ARM7Target)
                    {
                        Cycles = 0;
                        NDS.ARM7Timestamp = NDS.ARM7Target;
                    }
                    IdleLoop = 0;
                    break;
                }
            }
        }
        else
#endif
        {
            if constexpr (mode == CPUExecuteMode::InterpreterGDB)
                GdbCheckC();
                
            //printf("A:%i, F:%i, P:%i, E:%i, I:%08llX, 15:%08X\n", FuncQueueActive, FuncQueueFill, FuncQueueProg, FuncQueueEnd, CurInstr, R[15]);

            (this->*FuncQueue[FuncQueueProg])();

            if (FuncQueueActive)
            {
                if (FuncQueueFill == FuncQueueProg)
                {
                    // we did not get a new addition to the queue; increment and reset ptrs
                    FuncQueueFill = ++FuncQueueProg;

                    // check if we're done with the queue, if so, reset everything
                    if (FuncQueueProg >= FuncQueueEnd)
                    {
                        FuncQueueFill = 0;
                        FuncQueueProg = 0;
                        FuncQueueEnd = 0;
                        FuncQueueActive = false;
                        FuncQueue[0] = &ARMv4::StartExec;

                        /*
                        if (filey == NULL) filey = Platform::OpenFile("REGLOG.bin", Platform::FileMode::Read);
                        else
                        {
                            u32 regscmp[16];
                            Platform::FileRead(regscmp, 4, 16, filey);
                            if (iter > 471000 && memcmp(regscmp, R, 4*16))
                            {
                                printf("MISMATCH on iter: %lli!!!! %08llX\n", iter, CurInstr);
                                for (int i = 0; i < 16; i++)
                                {
                                    printf("R%i :%08X vs CMP:%08X\n", i, R[i], regscmp[i]);
                                }
                                //abt++;
                            }
                            iter++;
                        }*/
                    }
                }
                else
                {
                    // we got a new addition to the list; redo the current entry
                    FuncQueueFill = FuncQueueProg;
                }
            }
            else if (FuncQueueFill > 0) // check if we started the queue up
            {
                FuncQueueEnd = FuncQueueFill;
                FuncQueueFill = 0;
                FuncQueueActive = true;
            }
            else
            {
                /*
                if (filey == NULL) Platform::OpenFile("REGLOG.bin", Platform::FileMode::Read);
                else
                {
                    u32 regscmp[16];
                    Platform::FileRead(regscmp, 4, 16, filey);
                    if (iter > 471000 && memcmp(regscmp, R, 4*16))
                    {
                        printf("MISMATCH on iter: %lli!!!! %08llX\n", iter, CurInstr);
                        for (int i = 0; i < 16; i++)
                        {
                            printf("R%i :%08X vs CMP:%08X\n", i, R[i], regscmp[i]);
                        }
                        //abt++;
                        iter++;
                    }
                }*/
            }
            if (MRTrack.Type != MainRAMType::Null) return; // check if we need to resolve main ram

            // TODO optimize this shit!!!
            if (!FuncQueueActive && Halted)
            {
                if (Halted == 1 && NDS.ARM7Timestamp < NDS.ARM7Target)
                {
                    NDS.ARM7Timestamp = NDS.ARM7Target;
                }
                break;
            }
            /*if (NDS::IF[1] & NDS::IE[1])
            {
                if (NDS::IME[1] & 0x1)
                    TriggerIRQ<mode>();
            }*/
        }
    }

    if (Halted == 2)
        Halted = 0;

    if (Halted == 4)
    {
        assert(NDS.ConsoleType == 1);
        auto& dsi = dynamic_cast<melonDS::DSi&>(NDS);
        dsi.SoftReset();
        Halted = 2;
    }
}

template void ARMv4::Execute<CPUExecuteMode::Interpreter>();
template void ARMv4::Execute<CPUExecuteMode::InterpreterGDB>();
#ifdef JIT_ENABLED
template void ARMv4::Execute<CPUExecuteMode::JIT>();
#endif

void ARMv5::FillPipeline()
{
    /*SetupCodeMem(R[15]);

    if (CPSR & 0x20)
    {
        if ((R[15] - 2) & 0x2)
        {
            NextInstr[0] = CodeRead32(R[15] - 4) >> 16;
            NextInstr[1] = CodeRead32(R[15]);
        }
        else
        {
            NextInstr[0] = CodeRead32(R[15] - 2);
            NextInstr[1] = NextInstr[0] >> 16;
        }
    }
    else
    {
        NextInstr[0] = CodeRead32(R[15] - 4);
        NextInstr[1] = CodeRead32(R[15]);
    }*/
}

void ARMv4::FillPipeline()
{
    /*SetupCodeMem(R[15]);

    if (CPSR & 0x20)
    {
        NextInstr[0] = CodeRead16(R[15] - 2);
        NextInstr[1] = CodeRead16(R[15]);
    }
    else
    {
        NextInstr[0] = CodeRead32(R[15] - 4);
        NextInstr[1] = CodeRead32(R[15]);
    }*/
}

#ifdef GDBSTUB_ENABLED
u32 ARM::ReadReg(Gdb::Register reg)
{
    using Gdb::Register;
    int r = static_cast<int>(reg);

    if (reg < Register::pc) return R[r];
    else if (reg == Register::pc)
    {
        return R[r] - ((CPSR & 0x20) ? 2 : 4);
    }
    else if (reg == Register::cpsr) return CPSR;
    else if (reg == Register::sp_usr || reg == Register::lr_usr)
    {
        r -= static_cast<int>(Register::sp_usr);
        if (ModeIs(0x10) || ModeIs(0x1f))
        {
            return R[13 + r];
        }
        else switch (CPSR & 0x1f)
        {
        case 0x11: return R_FIQ[5 + r];
        case 0x12: return R_IRQ[0 + r];
        case 0x13: return R_SVC[0 + r];
        case 0x17: return R_ABT[0 + r];
        case 0x1b: return R_UND[0 + r];
        }
    }
    else if (reg >= Register::r8_fiq && reg <= Register::lr_fiq)
    {
        r -= static_cast<int>(Register::r8_fiq);
        return ModeIs(0x11) ? R[ 8 + r] : R_FIQ[r];
    }
    else if (reg == Register::sp_irq || reg == Register::lr_irq)
    {
        r -= static_cast<int>(Register::sp_irq);
        return ModeIs(0x12) ? R[13 + r] : R_IRQ[r];
    }
    else if (reg == Register::sp_svc || reg == Register::lr_svc)
    {
        r -= static_cast<int>(Register::sp_svc);
        return ModeIs(0x13) ? R[13 + r] : R_SVC[r];
    }
    else if (reg == Register::sp_abt || reg == Register::lr_abt)
    {
        r -= static_cast<int>(Register::sp_abt);
        return ModeIs(0x17) ? R[13 + r] : R_ABT[r];
    }
    else if (reg == Register::sp_und || reg == Register::lr_und)
    {
        r -= static_cast<int>(Register::sp_und);
        return ModeIs(0x1b) ? R[13 + r] : R_UND[r];
    }
    else if (reg == Register::spsr_fiq) return ModeIs(0x11) ? CPSR : R_FIQ[7];
    else if (reg == Register::spsr_irq) return ModeIs(0x12) ? CPSR : R_IRQ[2];
    else if (reg == Register::spsr_svc) return ModeIs(0x13) ? CPSR : R_SVC[2];
    else if (reg == Register::spsr_abt) return ModeIs(0x17) ? CPSR : R_ABT[2];
    else if (reg == Register::spsr_und) return ModeIs(0x1b) ? CPSR : R_UND[2];

    Log(LogLevel::Warn, "GDB reg read: unknown reg no %d\n", r);
    return 0xdeadbeef;
}
void ARM::WriteReg(Gdb::Register reg, u32 v)
{
    using Gdb::Register;
    int r = static_cast<int>(reg);

    if (reg < Register::pc) R[r] = v;
    else if (reg == Register::pc) JumpTo(v);
    else if (reg == Register::cpsr) CPSR = v;
    else if (reg == Register::sp_usr || reg == Register::lr_usr)
    {
        r -= static_cast<int>(Register::sp_usr);
        if (ModeIs(0x10) || ModeIs(0x1f))
        {
            R[13 + r] = v;
        }
        else switch (CPSR & 0x1f)
        {
        case 0x11: R_FIQ[5 + r] = v; break;
        case 0x12: R_IRQ[0 + r] = v; break;
        case 0x13: R_SVC[0 + r] = v; break;
        case 0x17: R_ABT[0 + r] = v; break;
        case 0x1b: R_UND[0 + r] = v; break;
        }
    }
    else if (reg >= Register::r8_fiq && reg <= Register::lr_fiq)
    {
        r -= static_cast<int>(Register::r8_fiq);
        *(ModeIs(0x11) ? &R[ 8 + r] : &R_FIQ[r]) = v;
    }
    else if (reg == Register::sp_irq || reg == Register::lr_irq)
    {
        r -= static_cast<int>(Register::sp_irq);
        *(ModeIs(0x12) ? &R[13 + r] : &R_IRQ[r]) = v;
    }
    else if (reg == Register::sp_svc || reg == Register::lr_svc)
    {
        r -= static_cast<int>(Register::sp_svc);
        *(ModeIs(0x13) ? &R[13 + r] : &R_SVC[r]) = v;
    }
    else if (reg == Register::sp_abt || reg == Register::lr_abt)
    {
        r -= static_cast<int>(Register::sp_abt);
        *(ModeIs(0x17) ? &R[13 + r] : &R_ABT[r]) = v;
    }
    else if (reg == Register::sp_und || reg == Register::lr_und)
    {
        r -= static_cast<int>(Register::sp_und);
        *(ModeIs(0x1b) ? &R[13 + r] : &R_UND[r]) = v;
    }
    else if (reg == Register::spsr_fiq)
    {
        *(ModeIs(0x11) ? &CPSR : &R_FIQ[7]) = v;
    }
    else if (reg == Register::spsr_irq)
    {
        *(ModeIs(0x12) ? &CPSR : &R_IRQ[2]) = v;
    }
    else if (reg == Register::spsr_svc)
    {
        *(ModeIs(0x13) ? &CPSR : &R_SVC[2]) = v;
    }
    else if (reg == Register::spsr_abt)
    {
        *(ModeIs(0x17) ? &CPSR : &R_ABT[2]) = v;
    }
    else if (reg == Register::spsr_und)
    {
        *(ModeIs(0x1b) ? &CPSR : &R_UND[2]) = v;
    }
    else Log(LogLevel::Warn, "GDB reg write: unknown reg no %d (write 0x%08x)\n", r, v);
}
u32 ARM::ReadMem(u32 addr, int size)
{
    if (size == 8) return BusRead8(addr);
    else if (size == 16) return BusRead16(addr);
    else if (size == 32) return BusRead32(addr);
    else return 0xfeedface;
}
void ARM::WriteMem(u32 addr, int size, u32 v)
{
    if (size == 8) BusWrite8(addr, (u8)v);
    else if (size == 16) BusWrite16(addr, (u16)v);
    else if (size == 32) BusWrite32(addr, v);
}

void ARM::ResetGdb()
{
    NDS.Reset();
    NDS.GPU.StartFrame(); // need this to properly kick off the scheduler & frame output
}
int ARM::RemoteCmd(const u8* cmd, size_t len)
{
    (void)len;

    Log(LogLevel::Info, "[ARMGDB] Rcmd: \"%s\"\n", cmd);
    if (!strcmp((const char*)cmd, "reset") || !strcmp((const char*)cmd, "r"))
    {
        Reset();
        return 0;
    }

    return 1; // not implemented (yet)
}

void ARMv5::WriteMem(u32 addr, int size, u32 v)
{
    if (addr < ITCMSize)
    {
        if (size == 8) *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)] = (u8)v;
        else if (size == 16) *(u16*)&ITCM[addr & (ITCMPhysicalSize - 1)] = (u16)v;
        else if (size == 32) *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)] = (u32)v;
        else {}
        return;
    }
    else if ((addr & DTCMMask) == DTCMBase)
    {
        if (size == 8) *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)] = (u8)v;
        else if (size == 16) *(u16*)&DTCM[addr & (DTCMPhysicalSize - 1)] = (u16)v;
        else if (size == 32) *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)] = (u32)v;
        else {}
        return;
    }

    ARM::WriteMem(addr, size, v);
}
u32 ARMv5::ReadMem(u32 addr, int size)
{
    if (addr < ITCMSize)
    {
        if (size == 8) return *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        else if (size == 16) return *(u16*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        else if (size == 32) return *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        else return 0xfeedface;
    }
    else if ((addr & DTCMMask) == DTCMBase)
    {
        if (size == 8) return *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        else if (size == 16) return *(u16*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        else if (size == 32) return *(u32*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        else return 0xfeedface;
    }

    return ARM::ReadMem(addr, size);
}
#endif


void ARMv5::CodeFetch()
{
    if (NullFetch)
    {
        // the value we need is cached by the bus
        // in practice we can treat this as a 1 cycle fetch, with no penalties
        RetVal = NextInstr[1] >> 16;
        NDS.ARM9Timestamp++;
        if (NDS.ARM9Timestamp < TimestampActual) NDS.ARM9Timestamp = TimestampActual;
        Store = false;
        DataRegion = Mem9_Null;
    }
    else
    {
        CodeRead32(PC);
    }
    QueueFunction(&ARMv5::AddExecute);
}

void ARMv5::AddExecute()
{
    NextInstr[1] = RetVal;

    NDS.ARM9Timestamp += ExecuteCycles;
}

void ARMv5::AddCycles_MW(s32 numM)
{
    DataCycles = numM;
    QueueFunction(&ARMv5::AddCycles_MW_2);
}

void ARMv5::AddCycles_MW_2()
{
    TimestampActual = NDS.ARM9Timestamp;

    NDS.ARM9Timestamp -= DataCycles;
}

template <bool bitfield>
void ARMv5::HandleInterlocksExecute(u16 ilmask, u8* times)
{
    /*
    if ((bitfield && (ilmask & (1<<ILCurrReg))) || (!bitfield && (ilmask == ILCurrReg)))
    {
        u64 time = ILCurrTime - (times ? times[ILCurrReg] : 0);
        if (NDS.ARM9Timestamp < time)
        {
            u64 diff = time - NDS.ARM9Timestamp;
            NDS.ARM9Timestamp = time;
            ITCMTimestamp += diff;

            ILCurrReg = 16;
            ILPrevReg = 16;
            return;
        }
    }
    if ((bitfield && (ilmask & (1<<ILPrevReg))) || (!bitfield && (ilmask == ILCurrReg)))
    {
        u64 time = ILPrevTime - (times ? times[ILPrevReg] : 0);
        if (NDS.ARM9Timestamp < time)
        {
            u64 diff = time - NDS.ARM9Timestamp; // should always be 1?
            NDS.ARM9Timestamp = time;
            ITCMTimestamp += diff;
        }
    }

    ILPrevReg = ILCurrReg;
    ILPrevTime = ILCurrTime;
    ILCurrReg = 16;*/
}
template void ARMv5::HandleInterlocksExecute<true>(u16 ilmask, u8* times);
template void ARMv5::HandleInterlocksExecute<false>(u16 ilmask, u8* times);

void ARMv5::HandleInterlocksMemory(u8 reg)
{
    /*
    if ((reg != ILPrevReg) || (NDS.ARM9Timestamp >= ILPrevTime)) return;
    
    u64 diff = ILPrevTime - NDS.ARM9Timestamp; // should always be 1?
    NDS.ARM9Timestamp = ILPrevTime;
    ITCMTimestamp += diff; // checkme
    ILPrevTime = 16;*/
}

void ARMv4::CodeRead16(u32 addr)
{
    if ((addr >> 24) == 0x02)
    {
        FetchAddr[16] = addr;
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRCodeFetch | MR16;
        if (!Nonseq) MRTrack.Var |= MRSequential;
    }
    else
    {
        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr>>15][Nonseq?0:1];
        RetVal = BusRead16(addr);
    }
}

void ARMv4::CodeRead32(u32 addr)
{
    if ((addr >> 24) == 0x02)
    {
        FetchAddr[16] = addr;
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRCodeFetch | MR32;
        if (!Nonseq) MRTrack.Var |= MRSequential;
    }
    else
    {
        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr>>15][Nonseq?2:3];
        RetVal = BusRead32(addr);
    }
}

bool ARMv4::DataRead8(u32 addr, u8 reg)
{
    FetchAddr[reg] = addr;
    LDRRegs = 1<<reg;

    QueueFunction(&ARMv4::DRead8_2);
    return true;
}

void ARMv4::DRead8_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MR8;
        MRTrack.Progress = reg;
    }
    else
    {
        u32 dummy; u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][0];
        *val = BusRead8(addr);
    }

}

bool ARMv4::DataRead16(u32 addr, u8 reg)
{
    FetchAddr[reg] = addr;
    LDRRegs = 1<<reg;

    QueueFunction(&ARMv4::DRead16_2);
    return true;
}

void ARMv4::DRead16_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MR16;
        MRTrack.Progress = reg;
    }
    else
    {
        u32 dummy;
        u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][0];
        *val = BusRead16(addr);
    }
}

bool ARMv4::DataRead32(u32 addr, u8 reg)
{
    FetchAddr[reg] = addr;
    LDRRegs = 1<<reg;

    QueueFunction(&ARMv4::DRead32_2);
    return true;
}

void ARMv4::DRead32_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MR32;
        MRTrack.Progress = reg;
    }
    else
    {
        u32 dummy;
        u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];
        
        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][2];
        *val = BusRead32(addr);
    }
    LDRRegs &= ~1<<reg;
}

bool ARMv4::DataRead32S(u32 addr, u8 reg)
{
    FetchAddr[reg] = addr;
    LDRRegs |= 1<<reg;

    QueueFunction(&ARMv4::DRead32S_2);
    return true;
}

void ARMv4::DRead32S_2()
{
    u8 reg = __builtin_ctz(LDRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MR32 | MRSequential;
        MRTrack.Progress = reg;
    }
    else
    {
        u32 dummy;
        u32* val = (LDRFailedRegs & (1<<reg)) ? &dummy : &R[reg];

        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][3];
        *val = BusRead32(addr);
    }
    LDRRegs &= ~1<<reg;
}

bool ARMv4::DataWrite8(u32 addr, u8 val, u8 reg)
{
    FetchAddr[reg] = addr;
    STRRegs = 1<<reg;
    STRVal[reg] = val;
    QueueFunction(&ARMv4::DWrite8_2);
    return true;
}

void ARMv4::DWrite8_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRWrite | MR8;
        MRTrack.Progress = reg;
    }
    else
    {
        u8 val = STRVal[reg];
    
        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][0];
        BusWrite8(addr, val);
    }
}

bool ARMv4::DataWrite16(u32 addr, u16 val, u8 reg)
{
    FetchAddr[reg] = addr;
    STRRegs = 1<<reg;
    STRVal[reg] = val;
    QueueFunction(&ARMv4::DWrite16_2);
    return true;
}

void ARMv4::DWrite16_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRWrite | MR16;
        MRTrack.Progress = reg;
    }
    else
    {
        u16 val = STRVal[reg];

        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][0];
        BusWrite16(addr, val);
    }
}

bool ARMv4::DataWrite32(u32 addr, u32 val, u8 reg)
{
    FetchAddr[reg] = addr;
    STRRegs = 1<<reg;
    STRVal[reg] = val;
    QueueFunction(&ARMv4::DWrite32_2);
    return true;
}

void ARMv4::DWrite32_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRWrite | MR32;
        MRTrack.Progress = reg;
    }
    else
    {
        u32 val = STRVal[reg];

        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][2];
        BusWrite32(addr, val);
    }
    STRRegs &= ~1<<reg;
}

bool ARMv4::DataWrite32S(u32 addr, u32 val, u8 reg)
{
    FetchAddr[reg] = addr;
    STRRegs |= 1<<reg;
    STRVal[reg] = val;
    QueueFunction(&ARMv4::DWrite32S_2);
    return true;
}

void ARMv4::DWrite32S_2()
{
    u8 reg = __builtin_ctz(STRRegs);
    u32 addr = FetchAddr[reg];

    if ((addr >> 24) == 0x02)
    {
        MRTrack.Type = MainRAMType::Fetch;
        MRTrack.Var = MRWrite | MR32 | MRSequential;
        MRTrack.Progress = reg;
    }
    else
    {
        u32 val = STRVal[reg];

        NDS.ARM7Timestamp += NDS.ARM7MemTimings[addr >> 15][3];
        BusWrite32(addr, val);
    }
    STRRegs &= ~1<<reg;
}


void ARMv4::AddCycles_C()
{
    // code only. this code fetch is sequential.
    Nonseq = false;
}

void ARMv4::AddCycles_CI(s32 num)
{
    // code+internal. results in a nonseq code fetch.
    ExecuteCycles = num;

    Nonseq = true;
    QueueFunction(&ARMv4::AddExecute);
}

void ARMv4::AddExecute()
{
    NDS.ARM7Timestamp += ExecuteCycles;
}

void ARMv4::AddCycles_CDI()
{
    // LDR/LDM cycles.

    Nonseq = true;
    QueueFunction(&ARMv4::AddExtraCycle);
}

void ARMv4::AddExtraCycle()
{
    NDS.ARM7Timestamp += 1;
}

void ARMv4::AddCycles_CD()
{
    // TODO: max gain should be 5c when writing to mainRAM
    
    Nonseq = true;
}

u8 ARMv5::BusRead8(u32 addr)
{
    return NDS.ARM9Read8(addr);
}

u16 ARMv5::BusRead16(u32 addr)
{
    return NDS.ARM9Read16(addr);
}

u32 ARMv5::BusRead32(u32 addr)
{
    return NDS.ARM9Read32(addr);
}

void ARMv5::BusWrite8(u32 addr, u8 val)
{
    NDS.ARM9Write8(addr, val);
}

void ARMv5::BusWrite16(u32 addr, u16 val)
{
    NDS.ARM9Write16(addr, val);
}

void ARMv5::BusWrite32(u32 addr, u32 val)
{
    NDS.ARM9Write32(addr, val);
}

u8 ARMv4::BusRead8(u32 addr)
{
    return NDS.ARM7Read8(addr);
}

u16 ARMv4::BusRead16(u32 addr)
{
    return NDS.ARM7Read16(addr);
}

u32 ARMv4::BusRead32(u32 addr)
{
    return NDS.ARM7Read32(addr);
}

void ARMv4::BusWrite8(u32 addr, u8 val)
{
    NDS.ARM7Write8(addr, val);
}

void ARMv4::BusWrite16(u32 addr, u16 val)
{
    NDS.ARM7Write16(addr, val);
}

void ARMv4::BusWrite32(u32 addr, u32 val)
{
    NDS.ARM7Write32(addr, val);
}
}

