/*
    Copyright 2016-2021 Arisotura

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
#include "NDS.h"
#include "DSi.h"
#include "ARM.h"
#include "ARMInterpreter.h"
#include "Config.h"
#include "AREngine.h"
#include "ARMJIT.h"
#include "Config.h"

#ifdef JIT_ENABLED
#include "ARMJIT.h"
#include "ARMJIT_Memory.h"
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
{
    // well uh
    Num = num;
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

    ExceptionBase = Num ? 0x00000000 : 0xFFFF0000;

    CodeMem.Mem = NULL;

#ifdef JIT_ENABLED
    FastBlockLookup = NULL;
    FastBlockLookupStart = 0;
    FastBlockLookupSize = 0;
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
    if (!file->Saving && Config::JIT_Enable)
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
        if (!Num)
        {
            SetupCodeMem(R[15]); // should fix it
            ((ARMv5*)this)->RegionCodeCycles = ((ARMv5*)this)->MemTimings[R[15] >> 12][0];
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

    /*if (!(PU_Map[addr>>12] & 0x04))
    {
        printf("jumped to %08X. very bad\n", addr);
        PrefetchAbort();
        return;
    }*/

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

    case 0x17:
        CPSR = R_ABT[2];
        break;

    case 0x1B:
        CPSR = R_UND[2];
        break;

    default:
        printf("!! attempt to restore CPSR under bad mode %02X, %08X\n", CPSR&0x1F, R[15]);
        break;
    }

    UpdateMode(oldcpsr, CPSR);
}

void ARM::UpdateMode(u32 oldmode, u32 newmode)
{
    u32 temp;
    #define SWAP(a, b)  temp = a; a = b; b = temp;

    if ((oldmode & 0x1F) == (newmode & 0x1F)) return;

    switch (oldmode & 0x1F)
    {
    case 0x11:
        SWAP(R[8], R_FIQ[0]);
        SWAP(R[9], R_FIQ[1]);
        SWAP(R[10], R_FIQ[2]);
        SWAP(R[11], R_FIQ[3]);
        SWAP(R[12], R_FIQ[4]);
        SWAP(R[13], R_FIQ[5]);
        SWAP(R[14], R_FIQ[6]);
        break;

    case 0x12:
        SWAP(R[13], R_IRQ[0]);
        SWAP(R[14], R_IRQ[1]);
        break;

    case 0x13:
        SWAP(R[13], R_SVC[0]);
        SWAP(R[14], R_SVC[1]);
        break;

    case 0x17:
        SWAP(R[13], R_ABT[0]);
        SWAP(R[14], R_ABT[1]);
        break;

    case 0x1B:
        SWAP(R[13], R_UND[0]);
        SWAP(R[14], R_UND[1]);
        break;
    }

    switch (newmode & 0x1F)
    {
    case 0x11:
        SWAP(R[8], R_FIQ[0]);
        SWAP(R[9], R_FIQ[1]);
        SWAP(R[10], R_FIQ[2]);
        SWAP(R[11], R_FIQ[3]);
        SWAP(R[12], R_FIQ[4]);
        SWAP(R[13], R_FIQ[5]);
        SWAP(R[14], R_FIQ[6]);
        break;

    case 0x12:
        SWAP(R[13], R_IRQ[0]);
        SWAP(R[14], R_IRQ[1]);
        break;

    case 0x13:
        SWAP(R[13], R_SVC[0]);
        SWAP(R[14], R_SVC[1]);
        break;

    case 0x17:
        SWAP(R[13], R_ABT[0]);
        SWAP(R[14], R_ABT[1]);
        break;

    case 0x1B:
        SWAP(R[13], R_UND[0]);
        SWAP(R[14], R_UND[1]);
        break;
    }

    #undef SWAP

    if (Num == 0)
    {
        /*if ((newmode & 0x1F) == 0x10)
            ((ARMv5*)this)->PU_Map = ((ARMv5*)this)->PU_UserMap;
        else
            ((ARMv5*)this)->PU_Map = ((ARMv5*)this)->PU_PrivMap;*/
        //if ((newmode & 0x1F) == 0x10) printf("!! USER MODE\n");
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
    printf("prefetch abort\n");

    u32 oldcpsr = CPSR;
    CPSR &= ~0xBF;
    CPSR |= 0x97;
    UpdateMode(oldcpsr, CPSR);

    // this shouldn't happen, but if it does, we're stuck in some nasty endless loop
    // so better take care of it
    if (!(PU_Map[ExceptionBase>>12] & 0x04))
    {
        printf("!!!!! EXCEPTION REGION NOT READABLE. THIS IS VERY BAD!!\n");
        NDS::Stop();
        return;
    }

    R_ABT[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 2 : 0);
    JumpTo(ExceptionBase + 0x0C);
}

void ARMv5::DataAbort()
{
    printf("data abort\n");

    u32 oldcpsr = CPSR;
    CPSR &= ~0xBF;
    CPSR |= 0x97;
    UpdateMode(oldcpsr, CPSR);

    R_ABT[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 6 : 4);
    JumpTo(ExceptionBase + 0x10);
}

void ARMv5::Execute()
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
        if (CPSR & 0x20) // THUMB
        {
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
            printf("ARMv5 PC in non executable region %08X\n", R[15]);
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
            printf("ARMv4 PC in non executable region %08X\n", R[15]);
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
