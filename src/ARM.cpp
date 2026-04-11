/*
    Copyright 2016-2026 melonDS team

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
#include "ARMv4.h"
#include "ARMv5.h"
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
    SetGdbArgs(jit ? std::nullopt : gdb);
}

ARM::~ARM()
{
    // dorp
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

    //CodeMem.Mem = NULL;

#ifdef JIT_ENABLED
    FastBlockLookup = NULL;
    FastBlockLookupStart = 0;
    FastBlockLookupSize = 0;
#endif

#ifdef GDBSTUB_ENABLED
    IsSingleStep = false;
    BreakReq = false;
#endif

    // zorp
    JumpTo(ExceptionBase);
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
    if (file->Saving && NDS.IsJITEnabled())
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


void ARM::SetupCodeMem(u32 addr)
{
    if (!Num)
    {
        //((ARMv5*)this)->GetCodeMemRegion(addr, &CodeMem);
    }
    else
    {
        // not sure it's worth it for the ARM7
        // esp. as everything there generally runs on WRAM
        // and due to how it's mapped, we can't use this optimization
        //NDS::ARM7GetMemRegion(addr, false, &CodeMem);
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
        if ((NDS.IF[1] & NDS.IE[1]) & (1<<IRQ_VBlank))
            NDS.AREngine.RunCheats();
    }
}


void ARM::CheckGdbIncoming()
{
    GdbCheckA();
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
    if (Num == 0)
    {
        if (size == 8) return NDS.ARM9Read8(addr);
        else if (size == 16) return NDS.ARM9Read16(addr);
        else if (size == 32) return NDS.ARM9Read32(addr);
        else return 0xfeedface;
    }
    else
    {
        if (size == 8) return NDS.ARM7Read8(addr);
        else if (size == 16) return NDS.ARM7Read16(addr);
        else if (size == 32) return NDS.ARM7Read32(addr);
        else return 0xfeedface;
    }
}
void ARM::WriteMem(u32 addr, int size, u32 v)
{
    if (Num == 0)
    {
        if (size == 8) NDS.ARM9Write8(addr, (u8) v);
        else if (size == 16) NDS.ARM9Write16(addr, (u16) v);
        else if (size == 32) NDS.ARM9Write32(addr, v);
    }
    else
    {
        if (size == 8) NDS.ARM7Write8(addr, (u8) v);
        else if (size == 16) NDS.ARM7Write16(addr, (u16) v);
        else if (size == 32) NDS.ARM7Write32(addr, v);
    }
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

#endif

}
