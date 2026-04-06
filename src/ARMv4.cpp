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

#include <assert.h>
#include <algorithm>
#include "NDS.h"
#include "DSi.h"
#include "ARMv4.h"
#include "ARMInterpreter.h"
#include "ARMJIT.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


ARMv4::ARMv4(melonDS::NDS& nds, std::optional<GDBArgs> gdb, bool jit) : ARM(1, jit, gdb, nds)
{
}

void ARMv4::Reset()
{
    // TODO unify these, add to savestate, etc
    CodeRegion = 0xFFFFFFFF;

    NextCodeFetchSeq = false;
    MainRAMStartAddr = 0;
    //MainRAMStart = 0;
    MainRAMCycles = 0;
    MainRAMTerminate = 0;

    ARM::Reset();
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
    NextCodeFetchSeq = false;

    if (addr & 0x1)
    {
        addr &= ~0x1;
        R[15] = addr+2;

        //if (newregion != oldregion) SetupCodeMem(addr);

        NextInstr[0] = CodeRead16(addr);   AddCycles_C();
        NextInstr[1] = CodeRead16(addr+2); AddCycles_C();
        //Cycles += NDS.ARM7MemTimings[CodeCycles][0] + NDS.ARM7MemTimings[CodeCycles][1];

        CPSR |= 0x20;
    }
    else
    {
        addr &= ~0x3;
        R[15] = addr+4;

        //if (newregion != oldregion) SetupCodeMem(addr);

        NextInstr[0] = CodeRead32(addr);   AddCycles_C();
        NextInstr[1] = CodeRead32(addr+4); AddCycles_C();
        //Cycles += NDS.ARM7MemTimings[CodeCycles][2] + NDS.ARM7MemTimings[CodeCycles][3];

        CPSR &= ~0x20;
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
                TriggerIRQ();
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
                    TriggerIRQ();

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
            if (CPSR & 0x20) // THUMB
            {
                if constexpr (mode == CPUExecuteMode::InterpreterGDB)
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
                if constexpr (mode == CPUExecuteMode::InterpreterGDB)
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
                if (Halted == 1 && NDS.ARM7Timestamp < NDS.ARM7Target)
                {
                    NDS.ARM7Timestamp = NDS.ARM7Target;
                }
                break;
            }
            /*if (NDS::IF[1] & NDS::IE[1])
            {
                if (NDS::IME[1] & 0x1)
                    TriggerIRQ();
            }*/
            if (IRQ) TriggerIRQ();
        }

        //NDS.ARM7Timestamp += Cycles;
        //Cycles = 0;
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

void ARMv4::BeginMainRAMBurst(int begin, int term)
{
    TerminateMainRAMBurst();

    //MainRAMStart = NDS.ARM7Timestamp;
    NDS.ARM7Timestamp += begin;
    MainRAMCycles = begin;
    MainRAMTerminate = NDS.ARM7Timestamp + term;
}

void ARMv4::TerminateMainRAMBurst()
{
    if (NDS.ARM7Timestamp < MainRAMTerminate)
        NDS.ARM7Timestamp = MainRAMTerminate;
}

u16 ARMv4::CodeRead16(const u32 addr)
{
    u32 rgn = addr & ~0x3FFF;
    if (rgn != CodeRegion)
    {
        NDS.ARM7GetMemInfo(rgn, CodeMem);
        //CodeMemTimings[0] = CodeMem.Cycles_N16;
        //CodeMemTimings[1] = CodeMem.Cycles_S16;
        CodeRegion = rgn;
    }

    if (CodeMem.Region & Mem7_MainRAM)
    {
        // TODO: assess whether the 241-cycle burst length limit really matters
        // in the case of code fetches it would only apply in very specific cases
        // and in the case of data fetches it would never apply
        //if ((NDS.ARM7Timestamp - MainRAMStart) >= 244)
        if (MainRAMCycles >= 241)
            NextCodeFetchSeq = false;
        else
        if ((MainRAMStartAddr & 0x1F) >= 0x1A && (addr & 0x1F) == 0)
            NextCodeFetchSeq = false;

        if (!NextCodeFetchSeq)
        {
            BeginMainRAMBurst(5, 3);
            MainRAMStartAddr = addr;
        }
        else
        {
            NDS.ARM7Timestamp++;
            MainRAMCycles++;
            MainRAMTerminate++;
        }
    }
    else
    {
        if (!NextCodeFetchSeq)
        {
            //TerminateMainRAMBurst();
            NDS.ARM7Timestamp += CodeMem.Cycles_N16;
        }
        else
            NDS.ARM7Timestamp += CodeMem.Cycles_S16;
    }

    return NDS.ARM7Read16(addr);
}

u32 ARMv4::CodeRead32(const u32 addr)
{
    u32 rgn = addr & ~0x3FFF;
    if (rgn != CodeRegion)
    {
        NDS.ARM7GetMemInfo(rgn, CodeMem);
        //CodeMemTimings[0] = CodeMem.Cycles_N32;
        //CodeMemTimings[1] = CodeMem.Cycles_S32;
        CodeRegion = rgn;
    }

    if (CodeMem.Region & Mem7_MainRAM)
    {
        //if ((NDS.ARM7Timestamp - MainRAMStart) >= 244)
        if (MainRAMCycles >= 241)
            NextCodeFetchSeq = false;
        else
        if ((MainRAMStartAddr & 0x1F) >= 0x1A && (addr & 0x1F) == 0)
            NextCodeFetchSeq = false;

        if (!NextCodeFetchSeq)
        {
            BeginMainRAMBurst(6, 3);
            MainRAMStartAddr = addr;
        }
        else
        {
            NDS.ARM7Timestamp += 2;
            MainRAMCycles++;
            MainRAMTerminate += 2;
        }
    }
    else
    {
        if (!NextCodeFetchSeq)
        {
            //TerminateMainRAMBurst();
            NDS.ARM7Timestamp += CodeMem.Cycles_N32;
        }
        else
            NDS.ARM7Timestamp += CodeMem.Cycles_S32;
    }

    return NDS.ARM7Read32(addr);
}

void ARMv4::DoDataAccessTimings(const u32 addr, bool write, int width)
{
    NDS.ARM7GetMemInfo(addr, DataMem);
    if (DataMem.Region & Mem7_MainRAM)
    {
        int begin, term;
        if (write)
        {
            switch (width)
            {
                case 8: begin = 4; term = 4; break;
                case 16: begin = 3; term = 5; break;
                case 32: begin = 4; term = 5; break;
            }
        }
        else
        {
            begin = (width == 32) ? 6 : 5;
            term = 3;
        }

        BeginMainRAMBurst(begin, term);
        MainRAMStartAddr = addr;
    }
    else
    {
        //TerminateMainRAMBurst();
        NDS.ARM7Timestamp += (width == 32) ? DataMem.Cycles_N32 : DataMem.Cycles_N16;

        // writing to the same region code runs from, adds one cycle of latency
        // only for internal memory
        // TODO: should not apply to SWPB/SWP
        if (write && (DataMem.Region & CodeMem.Region & Mem7_InternalRAM))
            NDS.ARM7Timestamp++;
    }

    NextCodeFetchSeq = false;
}

void ARMv4::DoDataAccessTimingsSeq(const u32 addr, bool write)
{
    bool seq = true;
    if (!(addr & 0x3FFFF))
    {
        u32 oldregion = DataMem.Region;
        NDS.ARM7GetMemInfo(addr, DataMem);
        if (DataMem.Region != oldregion)
            seq = false;
    }

    if (DataMem.Region & Mem7_MainRAM)
    {
        if (!write)
        {
            if ((MainRAMStartAddr & 0x1F) >= 0x1A && (addr & 0x1F) == 0)
                seq = false;
        }

        if (!seq)
        {
            if (write)
                BeginMainRAMBurst(4, 5);
            else
                BeginMainRAMBurst(6, 3);
            MainRAMStartAddr = addr;
        }
        else
        {
            NDS.ARM7Timestamp += 2;
            MainRAMTerminate += 2;
        }
    }
    else
    {
        if (seq)
            NDS.ARM7Timestamp += DataMem.Cycles_S32;
        else
        {
            NDS.ARM7Timestamp += DataMem.Cycles_N32;
            if (write && (DataMem.Region & CodeMem.Region & Mem7_InternalRAM))
                NDS.ARM7Timestamp++;
        }
    }
}

void ARMv4::DataRead8(const u32 addr, u32* val)
{
    DoDataAccessTimings(addr, false, 8);

    *val = NDS.ARM7Read8(addr);
    DataRegion = addr;
    //DataCycles = NDS.ARM7MemTimings[addr >> 15][0];
}

void ARMv4::DataRead16(const u32 addr, u32* val)
{
    DoDataAccessTimings(addr, false, 16);

    *val = NDS.ARM7Read16(addr & ~1);
    DataRegion = addr;
    //DataCycles = NDS.ARM7MemTimings[addr >> 15][0];
}

void ARMv4::DataRead32(const u32 addr, u32* val)
{
    DoDataAccessTimings(addr, false, 32);

    *val = NDS.ARM7Read32(addr & ~3);
    DataRegion = addr;
    //DataCycles = NDS.ARM7MemTimings[addr >> 15][2];
}

void ARMv4::DataRead32S(const u32 addr, u32* val)
{
    DoDataAccessTimingsSeq(addr, false);

    *val = NDS.ARM7Read32(addr & ~3);
    //DataCycles += NDS.ARM7MemTimings[addr >> 15][3];
}

void ARMv4::DataWrite8(const u32 addr, const u8 val)
{
    DoDataAccessTimings(addr, true, 8);

    NDS.ARM7Write8(addr, val);
    DataRegion = addr;
    //DataCycles = NDS.ARM7MemTimings[addr >> 15][0];
}

void ARMv4::DataWrite16(const u32 addr, const u16 val)
{
    DoDataAccessTimings(addr, true, 16);

    NDS.ARM7Write16(addr & ~1, val);
    DataRegion = addr;
    //DataCycles = NDS.ARM7MemTimings[addr >> 15][0];
}

void ARMv4::DataWrite32(const u32 addr, const u32 val)
{
    DoDataAccessTimings(addr, true, 32);

    NDS.ARM7Write32(addr & ~3, val);
    DataRegion = addr;
    //DataCycles = NDS.ARM7MemTimings[addr >> 15][2];
}

void ARMv4::DataWrite32S(const u32 addr, const u32 val)
{
    DoDataAccessTimingsSeq(addr, true);

    NDS.ARM7Write32(addr & ~3, val);
    //DataCycles += NDS.ARM7MemTimings[addr >> 15][3];
}

/*void ARMv4::DataRead32M(u32 addr, u32* data, u32 len)
{
    addr &= ~3;
    DoDataAccessTimings(addr, false, 32);
    u32 rgn = DataMem.Region;
    u32 endaddr = addr + len;

    for (;;)
    {
        // TODO fastmem optimization
        *data = NDS.ARM7Read32(addr);
        data++;
        addr += 4;
        if (addr >= endaddr)
            break;

        if (!(addr & 0x3FFF))
        {
            NDS.ARM7GetMemInfo(addr, DataMem);
            if (DataMem.Region != rgn)
            {
                DoDataAccessTimings(addr, false, 32);
                continue;
            }
        }

        DoDataAccessTimingsSeq(addr, false);
    }
}*/


void ARMv4::AddCycles_C()
{
    // code only. this code fetch is sequential.
    //Cycles += NDS.ARM7MemTimings[CodeCycles][(CPSR&0x20)?1:3];
    //Cycles += CodeCycles;
    NextCodeFetchSeq = true;
}

void ARMv4::AddCycles_CI(s32 num)
{
    // code+internal. results in a nonseq code fetch.
    //Cycles += NDS.ARM7MemTimings[CodeCycles][(CPSR&0x20)?0:2] + num;
    // TODO: the main RAM parallelization might be incorrect if done against a sequential fetch
    /*if (CodeMem.Region & Mem7_MainRAM)
        Cycles += std::max(CodeCycles + num - 3, std::max(CodeCycles, num));
    else
        Cycles += CodeCycles + num;*/
    NDS.ARM7Timestamp += num;
    NextCodeFetchSeq = false;
}

void ARMv4::AddCycles_CDI()
{
    // HACK TODO FIX FIX
    AddCycles_CI(1);
    return;
    // LDR/LDM cycles.
    s32 numC = NDS.ARM7MemTimings[CodeCycles][(CPSR&0x20)?0:2];
    s32 numD = DataCycles;

    if ((DataRegion >> 24) == 0x02) // mainRAM
    {
        if (CodeRegion == 0x02)
            Cycles += numC + numD;
        else
        {
            numC++;
            Cycles += std::max(numC + numD - 3, std::max(numC, numD));
        }
    }
    else if (CodeRegion == 0x02)
    {
        numD++;
        Cycles += std::max(numC + numD - 3, std::max(numC, numD));
    }
    else
    {
        Cycles += numC + numD + 1;
    }

    NextCodeFetchSeq = false;
}

void ARMv4::AddCycles_CD()
{
    // TODO: max gain should be 5c when writing to mainRAM
    s32 numC = NDS.ARM7MemTimings[CodeCycles][(CPSR&0x20)?0:2];
    s32 numD = DataCycles;

    if ((DataRegion >> 24) == 0x02)
    {
        if (CodeRegion == 0x02)
            Cycles += numC + numD;
        else
            Cycles += std::max(numC + numD - 3, std::max(numC, numD));
    }
    else if (CodeRegion == 0x02)
    {
        Cycles += std::max(numC + numD - 3, std::max(numC, numD));
    }
    else
    {
        Cycles += numC + numD;
    }

    NextCodeFetchSeq = false;
}

}
