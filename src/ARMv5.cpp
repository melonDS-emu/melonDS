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
#include "NDS.h"
#include "ARMv5.h"
#include "ARMInterpreter.h"
#include "ARMJIT.h"
#include "Platform.h"
#include "GPU.h"
#include "ARMJIT_Memory.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


// access timing for cached regions
// this would be an average between cache hits and cache misses
// this was measured to be close to hardware average
// a value of 1 would represent a perfect cache, but that causes
// games to run too fast, causing a number of issues
// TODO REMOVE ME
const int kDataCacheTiming = 3;//2;
const int kCodeCacheTiming = 3;//5;


ARMv5::ARMv5(melonDS::NDS& nds, std::optional<GDBArgs> gdb, bool jit) : ARM(0, jit, gdb, nds)
{
    DTCM = NDS.JIT.Memory.GetARM9DTCM();

    PU_Map = PU_PrivMap;
}

ARMv5::~ARMv5()
{
    // DTCM is owned by Memory, not going to delete it
}

void ARMv5::Reset()
{
    // TODO unify these, add to savestate, etc
    CodeRegion = 0xFFFFFFFF;

    // DS-mode settings
    // for DSi mode, those will be changed later
    ClockShift = 1;
    ClockAlign = 1;
    BusAccessDelay = 3;

    //DTCMTimestamp = 0;
    //BusTimestamp = 0;
    CodeTimestamp = 0;
    DataTimestamp = 0;

    ICacheStreamTag = 0xFFFFFFFF;
    memset(ICacheStreamTime, 0, sizeof(ICacheStreamTime));

    MainRAMStartAddr = 0;
    //MainRAMStart = 0;
    MainRAMCycles = 0;
    MainRAMTerminate = 0;

    PU_Map = PU_PrivMap;

    ARM::Reset();
}

void ARMv5::DoSavestate(Savestate* file)
{
    ARM::DoSavestate(file);
    CP15DoSavestate(file);
}


void ARMv5::SetClockShift(int shift)
{
    // adjust timestamps back to 33MHz base
    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ClockAlign) >> ClockShift;
    NDS.ARM9Target = (NDS.ARM9Target + ClockAlign) >> ClockShift;
    //DTCMTimestamp = (DTCMTimestamp + ClockAlign) >> ClockShift;
    //BusTimestamp = (BusTimestamp + ClockAlign) >> ClockShift;
    CodeTimestamp = (CodeTimestamp + ClockAlign) >> ClockShift;
    DataTimestamp = (DataTimestamp + ClockAlign) >> ClockShift;
    for (int i = 0; i < 8; i++)
        ICacheStreamTime[i] = (ICacheStreamTime[i] + ClockAlign) >> ClockShift;
    MainRAMTerminate = (MainRAMTerminate + ClockAlign) >> ClockShift;

    // apply new parameters
    ClockShift = shift;
    ClockAlign = (1 << shift) - 1;
    BusAccessDelay = ((shift > 1) ? 2 : 3) << shift;

    // adjust timestamps again
    NDS.ARM9Timestamp <<= ClockShift;
    NDS.ARM9Target <<= ClockShift;
    //DTCMTimestamp <<= ClockShift;
    //BusTimestamp <<= ClockShift;
    CodeTimestamp <<= ClockShift;
    DataTimestamp <<= ClockShift;
    for (int i = 0; i < 8; i++)
        ICacheStreamTime[i] <<= ClockShift;
    MainRAMTerminate <<= ClockShift;
}

u64 barg = 0;
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
    if (R[15]==0x02000CC4)
    {
        printf("BLARG GO! addr=%08X bus=%016llX code=%016llX data=%016llX\n",
               addr, NDS.ARM9Timestamp, CodeTimestamp, DataTimestamp);
        barg = NDS.ARM9Timestamp;
    }
    if (addr==0x02000CC0)
    {
        printf("BLARG RET! addr=%08X bus=%016llX code=%016llX data=%016llX\n",
               addr, NDS.ARM9Timestamp, CodeTimestamp, DataTimestamp);
        printf("RETURN %08X BARG=%016llX\n", R[15], NDS.ARM9Timestamp-barg);
    }

    u32 oldregion = R[15] >> 24;
    u32 newregion = addr >> 24;

    RegionCodeCycles = MemTimings[addr >> 12][0];
    Cycles += CodeCycles;

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

    NDS.MonitorARM9Jump(addr);
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
        NDS.Stop(Platform::StopReason::BadExceptionRegion);
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
                TriggerIRQ();
        }
        else
        {
            NDS.ARM9Timestamp = NDS.ARM9Target;
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
                    TriggerIRQ();

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
            if (CPSR & 0x20) // THUMB
            {
                if constexpr (mode == CPUExecuteMode::InterpreterGDB)
                    GdbCheckC();

                // prefetch
                R[15] += 2;
                CurInstr = NextInstr[0];
                NextInstr[0] = NextInstr[1];
                if (R[15] & 0x2) { NextInstr[1] >>= 16; CodeCycles = 1; }
                else             NextInstr[1] = CodeRead32(R[15], false);

                // actually execute
                u32 icode = (CurInstr >> 6) & 0x3FF;
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
                if (Halted == 1 && NDS.ARM9Timestamp < NDS.ARM9Target)
                {
                    NDS.ARM9Timestamp = NDS.ARM9Target;
                }
                break;
            }
            /*if (NDS::IF[0] & NDS::IE[0])
            {
                if (NDS::IME[0] & 0x1)
                    TriggerIRQ();
            }*/
            if (IRQ) TriggerIRQ();

        }

        //NDS.ARM9Timestamp += Cycles;
        //Cycles = 0;
    }

    if (Halted == 2)
        Halted = 0;
}
template void ARMv5::Execute<CPUExecuteMode::Interpreter>();
template void ARMv5::Execute<CPUExecuteMode::InterpreterGDB>();
#ifdef JIT_ENABLED
template void ARMv5::Execute<CPUExecuteMode::JIT>();
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

void ARMv5::BeginMainRAMBurst(int begin, int term)
{
    TerminateMainRAMBurst();

    begin <<= ClockShift;
    term <<= ClockShift;

    NDS.ARM9Timestamp += begin;
    MainRAMCycles = begin;
    MainRAMTerminate = NDS.ARM9Timestamp + term;
}

void ARMv5::TerminateMainRAMBurst()
{
    if (NDS.ARM9Timestamp < MainRAMTerminate)
        NDS.ARM9Timestamp = MainRAMTerminate;
}

void ARMv5::DoCodeAccessTimings(const u32 addr)
{
    u32 rgn = addr & ~0xFFF;
    if (rgn != CodeRegion)
    {
        NDS.ARM9GetMemInfo(rgn, CodeMem);
        CodeRegion = rgn;
    }

    /*if (!(addr & 0xFFF))
    {
        printf("BARG  %08X, %016llX %016llX\n",
               addr, NDS.ARM9Timestamp, BusTimestamp);
    }*/

    // align the ARM9 to the start of this instruction
    //if (NDS.ARM9Timestamp < BusTimestamp)
    //    NDS.ARM9Timestamp = BusTimestamp;
    //else
    //if (NDS.ARM9Timestamp < CodeTimestamp)
    //    NDS.ARM9Timestamp = CodeTimestamp;

    // when using the bus, apply buffering delay: 3 cycles on DS, 2 cycles on DSi
    NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ClockAlign) & ~ClockAlign;
    NDS.ARM9Timestamp += BusAccessDelay;

    // ARM9 code fetches are always nonsequential

    if (CodeMem.Region & Mem9_MainRAM)
    {
        BeginMainRAMBurst(6, 3);
        //MainRAMStartAddr = addr;
    }
    else
    {
        NDS.ARM9Timestamp += (CodeMem.Cycles_N32 << ClockShift);
    }

    // when loading code from the bus, the ARM9 can run up to two internal cycles in parallel
    CodeTimestamp = NDS.ARM9Timestamp - 1;
    DataTimestamp = CodeTimestamp;
}

// TCM are handled here.

u32 ARMv5::CodeRead32(const u32 addr, bool const branch)
{
    /*if (NDS.ARM9Timestamp < DTCMTimestamp)
        NDS.ARM9Timestamp = DTCMTimestamp;
    else
        DTCMTimestamp = NDS.ARM9Timestamp;*/
    //CodeTimestamp = std::max(CodeTimestamp, DataTimestamp);
    CodeTimestamp = std::max(NDS.ARM9Timestamp, std::max(CodeTimestamp, DataTimestamp));
    DataTimestamp = CodeTimestamp;
    if (NDS.ARM9Timestamp < CodeTimestamp)
        NDS.ARM9Timestamp = CodeTimestamp;

    /*if (!(addr & 0xFFF))
    {
        printf("BLARG! addr=%08X bus=%016llX code=%016llX data=%016llX\n",
               addr, NDS.ARM9Timestamp, CodeTimestamp, DataTimestamp);
    }*/

    // always count one CPU cycle for this instruction
    CodeTimestamp++;

    if (addr < ITCMSize)
    {
        //CodeCycles = 1;
        CodeMem.Region = Mem9_ITCM;
        //CodeTimestamp++;
        return *(u32*)&ITCM[addr & (ITCMPhysicalSize - 1)];
    }

#if !DISABLE_ICACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        if (CP15Control & CP15_CACHE_CR_ICACHEENABLE)
        {
            if (IsAddressICachable(addr))
            {
                //if (branch || !(addr & (ICACHE_LINELENGTH-1)))
                    return ICacheLookup(addr);
                //else
                //    return ICacheLookupFast(addr);
            }
        }
    }
#endif

    /*CodeCycles = RegionCodeCycles;

    if (CodeCycles == 0xFF) // cached memory. hax
    {
        if (branch || !(addr & (ICACHE_LINELENGTH-1)))
            CodeCycles = kCodeCacheTiming;//ICacheLookup(addr);
        else
            CodeCycles = 1;
    }

    if (CodeMem.Mem) return *(u32*)&CodeMem.Mem[addr & CodeMem.Mask];*/

    DoCodeAccessTimings(addr);

    // count atleast one internal cycle for this instruction
    //CodeTimestamp++;

    return NDS.ARM9Read32(addr);
}


void ARMv5::DoDataAccessTimings(const u32 addr, bool write, int width)
{
    if (CodeMem.Region & Mem9_CPUInstrMem)
    {
        // if the code fetch was from internal memory, we need to get the bus ready
        // TODO flag for doing this forcefully
        // TODO what about cache misses?

        /*if (BusTimestamp < NDS.ARM9Timestamp)
        {
            BusTimestamp = NDS.ARM9Timestamp;
            BusTimestamp = (BusTimestamp + ClockAlign) & ~ClockAlign;
        }*/

        // when using the bus, apply buffering delay: 3 cycles on DS, 2 cycles on DSi
        NDS.ARM9Timestamp = (NDS.ARM9Timestamp + ClockAlign) & ~ClockAlign;
        NDS.ARM9Timestamp += BusAccessDelay;
    }

    NDS.ARM9GetMemInfo(addr, DataMem);
    if (DataMem.Region & Mem9_MainRAM)
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
        NDS.ARM9Timestamp += ((width == 32) ? DataMem.Cycles_N32 : DataMem.Cycles_N16) << ClockShift;
    }
}

void ARMv5::DoDataAccessTimingsSeq(const u32 addr, bool write)
{
    bool seq = true;
    if (!(addr & 0xFFF))
    {
        u32 oldregion = DataMem.Region;
        NDS.ARM9GetMemInfo(addr, DataMem);
        if (DataMem.Region != oldregion)
            seq = false;
    }

    if (DataMem.Region & Mem9_MainRAM)
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
            int cy = 2 << ClockShift;
            NDS.ARM9Timestamp += cy;
            MainRAMTerminate += cy;
        }
    }
    else
    {
        if (seq)
            NDS.ARM9Timestamp += DataMem.Cycles_S32 << ClockShift;
        else
            NDS.ARM9Timestamp += DataMem.Cycles_N32 << ClockShift;
    }
}

void ARMv5::DataRead8(const u32 addr, u32* val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_READABLE))
    {
        Log(LogLevel::Debug, "data8 abort @ %08lx\n", addr);
        DataAbort();
        return;
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *val = *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *val = *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)];
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                *val = (DCacheLookup(addr) >> (8 * (addr & 3))) & 0xff;
                return;
            }
        }*/
    }
#endif

    DoDataAccessTimings(addr, false, 8);

    *val = NDS.ARM9Read8(addr);
    //DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataRead16(const u32 addr, u32* val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_READABLE))
    {
        Log(LogLevel::Debug, "data16 abort @ %08lx\n", addr);
        DataAbort();
        return;
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *val = *(u16*)&ITCM[addr & (ITCMPhysicalSize - 2)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *val = *(u16*)&DTCM[addr & (DTCMPhysicalSize - 2)];
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                *val = (DCacheLookup(addr) >> (8* (addr & 2))) & 0xffff;
                return;
            }
        }*/
    }
#endif

    DoDataAccessTimings(addr, false, 16);

    *val = NDS.ARM9Read16(addr & ~1);
    //DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataRead32(const u32 addr, u32* val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_READABLE))
    {
        Log(LogLevel::Debug, "data32 abort @ %08lx\n", addr);
        DataAbort();
        return;
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 4)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 4)];
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                *val = DCacheLookup(addr);
                return;
            }
        }*/
    }
#endif

    DoDataAccessTimings(addr, false, 32);

    *val = NDS.ARM9Read32(addr & ~0x03);
    //DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_N32];
}

void ARMv5::DataRead32S(const u32 addr, u32* val)
{
    if (addr < ITCMSize)
    {
        //DataCycles += 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *val = *(u32*)&ITCM[addr & (ITCMPhysicalSize - 4)];
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles += 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *val = *(u32*)&DTCM[addr & (DTCMPhysicalSize - 4)];
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                s32 cycles = DataCycles;
                DataCycles = 0;
                *val = DCacheLookup(addr);
                DataCycles += cycles;
                return;
            }
        }*/
    }
#endif

    DoDataAccessTimingsSeq(addr, false);

    *val = NDS.ARM9Read32(addr & ~0x03);
    //DataCycles += MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S32];
}

void ARMv5::DataWrite8(const u32 addr, const u8 val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_WRITEABLE))
    {
        DataAbort();
        return;
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *(u8*)&ITCM[addr & (ITCMPhysicalSize - 1)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *(u8*)&DTCM[addr & (DTCMPhysicalSize - 1)] = val;
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                if (DCacheWrite8(addr, val))
                    return;
            }
        }*/
    }
#endif

    DoDataAccessTimings(addr, true, 8);

    NDS.ARM9Write8(addr, val);
    //DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataWrite16(const u32 addr, const u16 val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_WRITEABLE))
    {
        DataAbort();
        return;
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *(u16*)&ITCM[addr & (ITCMPhysicalSize - 2)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *(u16*)&DTCM[addr & (DTCMPhysicalSize - 2)] = val;
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                if (DCacheWrite16(addr, val))
                    return;
            }
        }*/
    }
#endif

    DoDataAccessTimings(addr, true, 16);

    NDS.ARM9Write16(addr & ~1, val);
    //DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S16];
}

void ARMv5::DataWrite32(const u32 addr, const u32 val)
{
    if (!(PU_Map[addr>>CP15_MAP_ENTRYSIZE_LOG2] & CP15_MAP_WRITEABLE))
    {
        DataAbort();
        return;
    }

    DataRegion = addr;

    if (addr < ITCMSize)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 4)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles = 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 4)] = val;
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                if (DCacheWrite32(addr, val))
                    return;
            }
        }*/
    }
#endif

    DoDataAccessTimings(addr, true, 32);

    NDS.ARM9Write32(addr & ~3, val);
    //DataCycles = MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_N32];
}

void ARMv5::DataWrite32S(const u32 addr, const u32 val)
{
    if (addr < ITCMSize)
    {
        //DataCycles += 1;
        DataMem.Region = Mem9_ITCM;
        CodeTimestamp++;
        *(u32*)&ITCM[addr & (ITCMPhysicalSize - 4)] = val;
#ifdef JIT_ENABLED
        NDS.JIT.CheckAndInvalidate<0, ARMJIT_Memory::memregion_ITCM>(addr);
#endif
        return;
    }
    if ((addr & DTCMMask) == DTCMBase)
    {
        //DataCycles += 1;
        DataMem.Region = Mem9_DTCM;
        DataTimestamp++;
        *(u32*)&DTCM[addr & (DTCMPhysicalSize - 4)] = val;
        return;
    }

#if !DISABLE_DCACHE
#ifdef JIT_ENABLED
    if (!NDS.IsJITEnabled())
#endif
    {
        /*if (CP15Control & CP15_CACHE_CR_DCACHEENABLE)
        {
            if (IsAddressDCachable(addr))
            {
                s32 cycles = DataCycles;
                DataCycles = 0;
                if (DCacheWrite32(addr, val))
                {
                    DataCycles += cycles;
                    return;
                }
                DataCycles = cycles;
            }
        }*/
    }
#endif

    DoDataAccessTimingsSeq(addr, true);

    NDS.ARM9Write32(addr & ~3, val);
    //DataCycles += MemTimings[addr >> BUSCYCLES_MAP_GRANULARITY_LOG2][BUSCYCLES_S32];
}


void ARMv5::AddCycles_C()
{
    // code only. always nonseq 32-bit for ARM9.
    //s32 numC = (R[15] & 0x2) ? 1 : CodeCycles;
    //Cycles += numC;
}

void ARMv5::AddCycles_CI(s32 numI)
{
    // code+internal
    //s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
    //Cycles += numC + numI;
    CodeTimestamp += numI;
}

void ARMv5::AddCycles_CDI()
{
    // LDR/LDM cycles. ARM9 seems to skip the internal cycle there.
    // TODO: ITCM data fetches shouldn't be parallelized, they say
    s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
    s32 numD = DataCycles;

    //if (DataRegion != CodeRegion)
    Cycles += std::max(numC + numD - 6, std::max(numC, numD));
    //else
    //    Cycles += numC + numD;
}

void ARMv5::AddCycles_CD()
{
    // TODO: ITCM data fetches shouldn't be parallelized, they say
    s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
    s32 numD = DataCycles;

    //if (DataRegion != CodeRegion)
    Cycles += std::max(numC + numD - 6, std::max(numC, numD));
    //else
    //    Cycles += numC + numD;
}

void ARMv5::AddCycles_Store()
{
    AddCycles_CD();
}

void ARMv5::GetCodeMemRegion(const u32 addr, MemRegion* region)
{
    //NDS.ARM9GetMemRegion(addr, false, &CodeMem);
}

#ifdef GDBSTUB_ENABLED
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

}
