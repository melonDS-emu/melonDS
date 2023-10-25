/*
    Copyright 2016-2022 melonDS team

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
#include <algorithm>
#include "NDS.h"
#include "DSi.h"
#include "ARM.h"
#include "ARMInterpreter.h"
#include "AREngine.h"
#include "ARMJIT.h"
#include "Platform.h"
#include "GPU.h"

#ifdef JIT_ENABLED
#include "ARMJIT.h"
#include "ARMJIT_Memory.h"
#endif

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
ARM::GdbCheckA() {}
ARM::GdbCheckB() {}
ARM::GdbCheckC() {}
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



u32 ARM::ConditionTable[16] =
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


ARM::ARM(u32 num)
#ifdef GDBSTUB_ENABLED
    : GdbStub(this, Platform::GetConfigInt(num ? Platform::GdbPortARM7 : Platform::GdbPortARM9))
#endif
{
    // well uh
    Num = num;

#ifdef GDBSTUB_ENABLED
    if (Platform::GetConfigBool(Platform::GdbEnabled)
#ifdef JIT_ENABLED
            && !Platform::GetConfigBool(Platform::JIT_Enable)
#endif
    )
        GdbStub.Init();
    IsSingleStep = false;
#endif
}

ARM::~ARM()
{
    // dorp
}

ARMv5::ARMv5() : ARM(0)
{
#ifndef JIT_ENABLED
    DTCM = new u8[DTCMPhysicalSize];
#endif

    PU_Map = PU_PrivMap;
}

ARMv4::ARMv4() : ARM(1)
{
    //
}

ARMv5::~ARMv5()
{
#ifndef JIT_ENABLED
    delete[] DTCM;
#endif
}

void ARM::Reset()
{
    Cycles = 0;
    Halted = 0;

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
    BreakOnStartup = Platform::GetConfigBool(
        Num ? Platform::GdbARM7BreakOnStartup : Platform::GdbARM9BreakOnStartup);
#endif

    // zorp
    JumpTo(ExceptionBase);
}

void ARMv5::Reset()
{
    if (NDS::ConsoleType == 1)
    {
        BusRead8 = DSi::ARM9Read8;
        BusRead16 = DSi::ARM9Read16;
        BusRead32 = DSi::ARM9Read32;
        BusWrite8 = DSi::ARM9Write8;
        BusWrite16 = DSi::ARM9Write16;
        BusWrite32 = DSi::ARM9Write32;
        GetMemRegion = DSi::ARM9GetMemRegion;
    }
    else
    {
        BusRead8 = NDS::ARM9Read8;
        BusRead16 = NDS::ARM9Read16;
        BusRead32 = NDS::ARM9Read32;
        BusWrite8 = NDS::ARM9Write8;
        BusWrite16 = NDS::ARM9Write16;
        BusWrite32 = NDS::ARM9Write32;
        GetMemRegion = NDS::ARM9GetMemRegion;
    }

    PU_Map = PU_PrivMap;

    ARM::Reset();
}

void ARMv4::Reset()
{
    if (NDS::ConsoleType)
    {
        BusRead8 = DSi::ARM7Read8;
        BusRead16 = DSi::ARM7Read16;
        BusRead32 = DSi::ARM7Read32;
        BusWrite8 = DSi::ARM7Write8;
        BusWrite16 = DSi::ARM7Write16;
        BusWrite32 = DSi::ARM7Write32;
    }
    else
    {
        BusRead8 = NDS::ARM7Read8;
        BusRead16 = NDS::ARM7Read16;
        BusRead32 = NDS::ARM7Read32;
        BusWrite8 = NDS::ARM7Write8;
        BusWrite16 = NDS::ARM7Write16;
        BusWrite32 = NDS::ARM7Write32;
    }

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
    file->Var32(&CurInstr);
#ifdef JIT_ENABLED
    if (file->Saving && NDS::EnableJIT)
    {
        // hack, the JIT doesn't really pipeline
        // but we still want JIT save states to be
        // loaded while running the interpreter
        FillPipeline();
    }
#endif
    file->VarArray(NextInstr, 2*sizeof(u32));

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
            ((ARMv5*)this)->RegionCodeCycles = ((ARMv5*)this)->MemTimings[R[15] >> 12][0];

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

void ARMv5::JumpTo(u32 addr, bool restorecpsr)
{
    if (restorecpsr)
    {
        RestoreCPSR();

        if (CPSR & 0x20)    addr |= 0x1;
        else                addr &= ~0x1;
    }

    // aging cart debug crap
    //if (addr == 0x0201764C) printf("capture test %d: R1=%08X\n", R[6], R[1]);
    //if (addr == 0x020175D8) printf("capture test %d: res=%08X\n", R[6], R[0]);

    u32 oldregion = R[15] >> 24;
    u32 newregion = addr >> 24;

    RegionCodeCycles = MemTimings[addr >> 12][0];

    if (addr & 0x1)
    {
        addr &= ~0x1;
        R[15] = addr+2;

        if (newregion != oldregion) SetupCodeMem(addr);

        // two-opcodes-at-once fetch
        // doesn't matter if we put garbage in the MSbs there
        if (addr & 0x2)
        {
            NextInstr[0] = CodeRead32(addr-2, true) >> 16;
            Cycles += CodeCycles;
            NextInstr[1] = CodeRead32(addr+2, false);
            Cycles += CodeCycles;
        }
        else
        {
            NextInstr[0] = CodeRead32(addr, true);
            NextInstr[1] = NextInstr[0] >> 16;
            Cycles += CodeCycles;
        }

        CPSR |= 0x20;
    }
    else
    {
        addr &= ~0x3;
        R[15] = addr+4;

        if (newregion != oldregion) SetupCodeMem(addr);

        NextInstr[0] = CodeRead32(addr, true);
        Cycles += CodeCycles;
        NextInstr[1] = CodeRead32(addr+4, false);
        Cycles += CodeCycles;

        CPSR &= ~0x20;
    }

    if (!(PU_Map[addr>>12] & 0x04))
    {
        PrefetchAbort();
        return;
    }

    NDS::MonitorARM9Jump(addr);
}

void ARMv4::JumpTo(u32 addr, bool restorecpsr)
{
    if (restorecpsr)
    {
        RestoreCPSR();

        if (CPSR & 0x20)    addr |= 0x1;
        else                addr &= ~0x1;
    }

    u32 oldregion = R[15] >> 23;
    u32 newregion = addr >> 23;

    CodeRegion = addr >> 24;
    CodeCycles = addr >> 15; // cheato

    if (addr & 0x1)
    {
        addr &= ~0x1;
        R[15] = addr+2;

        //if (newregion != oldregion) SetupCodeMem(addr);

        NextInstr[0] = CodeRead16(addr);
        NextInstr[1] = CodeRead16(addr+2);
        Cycles += NDS::ARM7MemTimings[CodeCycles][0] + NDS::ARM7MemTimings[CodeCycles][1];

        CPSR |= 0x20;
    }
    else
    {
        addr &= ~0x3;
        R[15] = addr+4;

        //if (newregion != oldregion) SetupCodeMem(addr);

        NextInstr[0] = CodeRead32(addr);
        NextInstr[1] = CodeRead32(addr+4);
        Cycles += NDS::ARM7MemTimings[CodeCycles][2] + NDS::ARM7MemTimings[CodeCycles][3];

        CPSR &= ~0x20;
    }
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

void ARM::TriggerIRQ()
{
    if (CPSR & 0x80)
        return;

    u32 oldcpsr = CPSR;
    CPSR &= ~0xFF;
    CPSR |= 0xD2;
    UpdateMode(oldcpsr, CPSR);

    R_IRQ[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 2 : 0);
    JumpTo(ExceptionBase + 0x18);

    // ARDS cheat support
    // normally, those work by hijacking the ARM7 VBlank handler
    if (Num == 1)
    {
        if ((NDS::IF[1] & NDS::IE[1]) & (1<<NDS::IRQ_VBlank))
            AREngine::RunCheats();
    }
}

void ARMv5::PrefetchAbort()
{
    Log(LogLevel::Warn, "ARM9: prefetch abort (%08X)\n", R[15]);

    u32 oldcpsr = CPSR;
    CPSR &= ~0xBF;
    CPSR |= 0x97;
    UpdateMode(oldcpsr, CPSR);

    // this shouldn't happen, but if it does, we're stuck in some nasty endless loop
    // so better take care of it
    if (!(PU_Map[ExceptionBase>>12] & 0x04))
    {
        Log(LogLevel::Error, "!!!!! EXCEPTION REGION NOT EXECUTABLE. THIS IS VERY BAD!!\n");
        NDS::Stop(Platform::StopReason::BadExceptionRegion);
        return;
    }

    R_ABT[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 2 : 0);
    JumpTo(ExceptionBase + 0x0C);
}

void ARMv5::DataAbort()
{
    Log(LogLevel::Warn, "ARM9: data abort (%08X)\n", R[15]);

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

void ARMv5::Execute()
{
    GdbCheckB();

    if (Halted)
    {
        if (Halted == 2)
        {
            Halted = 0;
        }
        else if (NDS::HaltInterrupted(0))
        {
            Halted = 0;
            if (NDS::IME[0] & 0x1)
                TriggerIRQ();
        }
        else
        {
            NDS::ARM9Timestamp = NDS::ARM9Target;
            return;
        }
    }

    while (NDS::ARM9Timestamp < NDS::ARM9Target)
    {
        if (CPSR & 0x20) // THUMB
        {
            GdbCheckC();

            // prefetch
            R[15] += 2;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            if (R[15] & 0x2) { NextInstr[1] >>= 16; CodeCycles = 0; }
            else             NextInstr[1] = CodeRead32(R[15], false);

            // actually execute
            u32 icode = (CurInstr >> 6) & 0x3FF;
            ARMInterpreter::THUMBInstrTable[icode](this);
        }
        else
        {
            GdbCheckC();

            // prefetch
            R[15] += 4;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            NextInstr[1] = CodeRead32(R[15], false);

            // actually execute
            if (CheckCondition(CurInstr >> 28))
            {
                u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
                ARMInterpreter::ARMInstrTable[icode](this);
            }
            else if ((CurInstr & 0xFE000000) == 0xFA000000)
            {
                ARMInterpreter::A_BLX_IMM(this);
            }
            else
                AddCycles_C();
        }

        // TODO optimize this shit!!!
        if (Halted)
        {
            if (Halted == 1 && NDS::ARM9Timestamp < NDS::ARM9Target)
            {
                NDS::ARM9Timestamp = NDS::ARM9Target;
            }
            break;
        }
        /*if (NDS::IF[0] & NDS::IE[0])
        {
            if (NDS::IME[0] & 0x1)
                TriggerIRQ();
        }*/
        if (IRQ) TriggerIRQ();

        NDS::ARM9Timestamp += Cycles;
        Cycles = 0;
    }

    if (Halted == 2)
        Halted = 0;
}

#ifdef JIT_ENABLED
void ARMv5::ExecuteJIT()
{
    if (Halted)
    {
        if (Halted == 2)
        {
            Halted = 0;
        }
        else if (NDS::HaltInterrupted(0))
        {
            Halted = 0;
            if (NDS::IME[0] & 0x1)
                TriggerIRQ();
        }
        else
        {
            NDS::ARM9Timestamp = NDS::ARM9Target;
            return;
        }
    }

    while (NDS::ARM9Timestamp < NDS::ARM9Target)
    {
        u32 instrAddr = R[15] - ((CPSR&0x20)?2:4);

        if ((instrAddr < FastBlockLookupStart || instrAddr >= (FastBlockLookupStart + FastBlockLookupSize))
            && !ARMJIT::SetupExecutableRegion(0, instrAddr, FastBlockLookup, FastBlockLookupStart, FastBlockLookupSize))
        {
            NDS::ARM9Timestamp = NDS::ARM9Target;
            Log(LogLevel::Error, "ARMv5 PC in non executable region %08X\n", R[15]);
            return;
        }

        ARMJIT::JitBlockEntry block = ARMJIT::LookUpBlock(0, FastBlockLookup,
            instrAddr - FastBlockLookupStart, instrAddr);
        if (block)
            ARM_Dispatch(this, block);
        else
            ARMJIT::CompileBlock(this);

        if (StopExecution)
        {
            // this order is crucial otherwise idle loops waiting for an IRQ won't function
            if (IRQ)
                TriggerIRQ();

            if (Halted || IdleLoop)
            {
                if ((Halted == 1 || IdleLoop) && NDS::ARM9Timestamp < NDS::ARM9Target)
                {
                    Cycles = 0;
                    NDS::ARM9Timestamp = NDS::ARM9Target;
                }
                IdleLoop = 0;
                break;
            }
        }

        NDS::ARM9Timestamp += Cycles;
        Cycles = 0;
    }

    if (Halted == 2)
        Halted = 0;
}
#endif

void ARMv4::Execute()
{
    GdbCheckB();

    if (Halted)
    {
        if (Halted == 2)
        {
            Halted = 0;
        }
        else if (NDS::HaltInterrupted(1))
        {
            Halted = 0;
            if (NDS::IME[1] & 0x1)
                TriggerIRQ();
        }
        else
        {
            NDS::ARM7Timestamp = NDS::ARM7Target;
            return;
        }
    }

    while (NDS::ARM7Timestamp < NDS::ARM7Target)
    {
        if (CPSR & 0x20) // THUMB
        {
            GdbCheckC();

            // prefetch
            R[15] += 2;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            NextInstr[1] = CodeRead16(R[15]);

            // actually execute
            u32 icode = (CurInstr >> 6);
            ARMInterpreter::THUMBInstrTable[icode](this);
        }
        else
        {
            GdbCheckC();

            // prefetch
            R[15] += 4;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            NextInstr[1] = CodeRead32(R[15]);

            // actually execute
            if (CheckCondition(CurInstr >> 28))
            {
                u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
                ARMInterpreter::ARMInstrTable[icode](this);
            }
            else
                AddCycles_C();
        }

        // TODO optimize this shit!!!
        if (Halted)
        {
            if (Halted == 1 && NDS::ARM7Timestamp < NDS::ARM7Target)
            {
                NDS::ARM7Timestamp = NDS::ARM7Target;
            }
            break;
        }
        /*if (NDS::IF[1] & NDS::IE[1])
        {
            if (NDS::IME[1] & 0x1)
                TriggerIRQ();
        }*/
        if (IRQ) TriggerIRQ();

        NDS::ARM7Timestamp += Cycles;
        Cycles = 0;
    }

    if (Halted == 2)
        Halted = 0;

    if (Halted == 4)
    {
        DSi::SoftReset();
        Halted = 2;
    }
}

#ifdef JIT_ENABLED
void ARMv4::ExecuteJIT()
{
    if (Halted)
    {
        if (Halted == 2)
        {
            Halted = 0;
        }
        else if (NDS::HaltInterrupted(1))
        {
            Halted = 0;
            if (NDS::IME[1] & 0x1)
                TriggerIRQ();
        }
        else
        {
            NDS::ARM7Timestamp = NDS::ARM7Target;
            return;
        }
    }

    while (NDS::ARM7Timestamp < NDS::ARM7Target)
    {
        u32 instrAddr = R[15] - ((CPSR&0x20)?2:4);

        if ((instrAddr < FastBlockLookupStart || instrAddr >= (FastBlockLookupStart + FastBlockLookupSize))
            && !ARMJIT::SetupExecutableRegion(1, instrAddr, FastBlockLookup, FastBlockLookupStart, FastBlockLookupSize))
        {
            NDS::ARM7Timestamp = NDS::ARM7Target;
            Log(LogLevel::Error, "ARMv4 PC in non executable region %08X\n", R[15]);
            return;
        }

        ARMJIT::JitBlockEntry block = ARMJIT::LookUpBlock(1, FastBlockLookup,
            instrAddr - FastBlockLookupStart, instrAddr);
        if (block)
            ARM_Dispatch(this, block);
        else
            ARMJIT::CompileBlock(this);

        if (StopExecution)
        {
            if (IRQ)
                TriggerIRQ();

            if (Halted || IdleLoop)
            {
                if ((Halted == 1 || IdleLoop) && NDS::ARM7Timestamp < NDS::ARM7Target)
                {
                    Cycles = 0;
                    NDS::ARM7Timestamp = NDS::ARM7Target;
                }
                IdleLoop = 0;
                break;
            }
        }

        NDS::ARM7Timestamp += Cycles;
        Cycles = 0;
    }

    if (Halted == 2)
        Halted = 0;

    if (Halted == 4)
    {
        DSi::SoftReset();
        Halted = 2;
    }
}
#endif

void ARMv5::FillPipeline()
{
    SetupCodeMem(R[15]);

    if (CPSR & 0x20)
    {
        if ((R[15] - 2) & 0x2)
        {
            NextInstr[0] = CodeRead32(R[15] - 4, false) >> 16;
            NextInstr[1] = CodeRead32(R[15], false);
        }
        else
        {
            NextInstr[0] = CodeRead32(R[15] - 2, false);
            NextInstr[1] = NextInstr[0] >> 16;
        }
    }
    else
    {
        NextInstr[0] = CodeRead32(R[15] - 4, false);
        NextInstr[1] = CodeRead32(R[15], false);
    }
}

void ARMv4::FillPipeline()
{
    SetupCodeMem(R[15]);

    if (CPSR & 0x20)
    {
        NextInstr[0] = CodeRead16(R[15] - 2);
        NextInstr[1] = CodeRead16(R[15]);
    }
    else
    {
        NextInstr[0] = CodeRead32(R[15] - 4);
        NextInstr[1] = CodeRead32(R[15]);
    }
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
    NDS::Reset();
    GPU::StartFrame(); // need this to properly kick off the scheduler & frame output
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

