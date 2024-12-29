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
#include "NDS.h"
#include "DSi.h"
#include "DMA.h"
#include "GPU.h"
#include "ARM.h"
#include "GPU3D.h"
#include "DMA_Timings.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

// DMA TIMINGS
//
// sequential timing:
// * 1 cycle per read or write
// * in 32bit mode, accessing a 16bit bus (mainRAM, palette, VRAM) incurs 1 cycle of penalty
// * in 32bit mode, transferring from mainRAM to another bank is 1 cycle faster
// * if source and destination are the same memory bank, there is a 1 cycle penalty
// * transferring from mainRAM to mainRAM is a trainwreck (all accesses are made nonsequential)
//
// nonsequential timing:
// * nonseq penalty is applied to the first read and write
// * I also figure it gets nonseq penalty again when resuming, after having been interrupted by
//   another DMA (TODO: check)
// * applied to all accesses for mainRAM->mainRAM, resulting in timings of 16-18 cycles per unit
//
// TODO: GBA slot
// TODO: re-add initial NS delay
// TODO: timings are nonseq when address is fixed/decrementing


DMA::DMA(u32 cpu, u32 num, melonDS::NDS& nds) :
    CPU(cpu),
    Num(num),
    NDS(nds)
{
    if (cpu == 0)
        CountMask = 0x001FFFFF;
    else
        CountMask = (num==3 ? 0x0000FFFF : 0x00003FFF);
}

void DMA::Reset()
{
    SrcAddr = 0;
    DstAddr = 0;
    Cnt = 0;

    StartMode = 0;
    CurSrcAddr = 0;
    CurDstAddr = 0;
    RemCount = 0;
    IterCount = 0;
    SrcAddrInc = 0;
    DstAddrInc = 0;

    Stall = false;

    Running = false;
    Executing = false;
    InProgress = false;
    DMAQueued = false;
    MRAMBurstCount = 0;
    MRAMBurstTable = DMATiming::MRAMDummy;
}

void DMA::DoSavestate(Savestate* file)
{
    char magic[5] = "DMAx";
    magic[3] = '0' + Num + (CPU*4);
    file->Section(magic);

    file->Var32(&SrcAddr);
    file->Var32(&DstAddr);
    file->Var32(&Cnt);

    file->Var32(&StartMode);
    file->Var32(&CurSrcAddr);
    file->Var32(&CurDstAddr);
    file->Var32(&RemCount);
    file->Var32(&IterCount);
    file->Var32((u32*)&SrcAddrInc);
    file->Var32((u32*)&DstAddrInc);

    file->Var32(&Running);
    file->Bool32(&InProgress);
    file->Bool32(&IsGXFIFODMA);
    file->Var32(&MRAMBurstCount);
    file->Bool32(&Executing);
    file->Bool32(&Stall);

    file->VarArray(MRAMBurstTable.data(), sizeof(MRAMBurstTable));
}

void DMA::WriteCnt(u32 val)
{
    u32 oldcnt = Cnt;
    Cnt = val;

    if ((!(oldcnt & 0x80000000)) && (val & 0x80000000))
    {
        CurSrcAddr = SrcAddr;
        CurDstAddr = DstAddr;

        switch (Cnt & 0x00600000)
        {
        case 0x00000000: DstAddrInc = 1; break;
        case 0x00200000: DstAddrInc = -1; break;
        case 0x00400000: DstAddrInc = 0; break;
        case 0x00600000: DstAddrInc = 1; break;
        }

        switch (Cnt & 0x01800000)
        {
        case 0x00000000: SrcAddrInc = 1; break;
        case 0x00800000: SrcAddrInc = -1; break;
        case 0x01000000: SrcAddrInc = 0; break;
        case 0x01800000: SrcAddrInc = 1; break;
        }
        u32 oldstartmode = StartMode;
        if (CPU == 0)
            StartMode = (Cnt >> 27) & 0x7;
        else
            StartMode = ((Cnt >> 28) & 0x3) | 0x10;

        if ((StartMode & 0x7) == 0)
        {
            NDS.DMAsQueued[NDS.DMAQueuePtr++] = (CPU*4)+Num;
            if (!(NDS.SchedListMask & (1<<Event_DMA))) NDS.ScheduleEvent(Event_DMA, false, 1, 0, 0);
        }
        else if (StartMode == 0x07)
            NDS.GPU.GPU3D.CheckFIFODMA();

        if (StartMode==0x06 || StartMode==0x13)
            Log(LogLevel::Warn, "UNIMPLEMENTED ARM%d DMA%d START MODE %02X, %08X->%08X\n", CPU?7:9, Num, StartMode, SrcAddr, DstAddr);
    }
}

void DMA::Start()
{
    if (Running)
    {
        if (CPU ? StartMode == 0x12 : StartMode == 0x05)
        {
            DMAQueued = true;
        }
        else
        {
            DMAQueued = false;
        }
        return;
    }
    else
    {
        DMAQueued = false;
    }

    if (!InProgress)
    {
        u32 countmask;
        if (CPU == 0)
            countmask = 0x001FFFFF;
        else
            countmask = (Num==3 ? 0x0000FFFF : 0x00003FFF);

        RemCount = Cnt & countmask;
        if (!RemCount)
            RemCount = countmask+1;
    }

    if (StartMode == 0x07 && RemCount > 112)
        IterCount = 112;
    else
        IterCount = RemCount;

    if ((Cnt & 0x01800000) == 0x01800000)
        CurSrcAddr = SrcAddr;

    if ((Cnt & 0x00600000) == 0x00600000)
        CurDstAddr = DstAddr;

    //printf("ARM%d DMA%d %08X %02X %08X->%08X %d bytes %dbit\n", CPU?7:9, Num, Cnt, StartMode, CurSrcAddr, CurDstAddr, RemCount*((Cnt&0x04000000)?4:2), (Cnt&0x04000000)?32:16);

    IsGXFIFODMA = (CPU == 0 && (CurSrcAddr>>24) == 0x02 && CurDstAddr == 0x04000400 && DstAddrInc == 0);

    // TODO eventually: not stop if we're running code in ITCM

    Running = 3;

    // safety measure
    MRAMBurstTable = DMATiming::MRAMDummy;

    InProgress = true;
    NDS.StopCPU(CPU, 1<<Num);

    if (CPU == 0)
    {
        u64 ts = NDS.SysTimestamp << NDS.ARM9ClockShift;

        if (NDS.DMA9Timestamp < ts) NDS.DMA9Timestamp = ts;
    }

    if (Num == 0) NDS.DMAs[(CPU*4)+1].ResetBurst();
    if (Num <= 1) NDS.DMAs[(CPU*4)+2].ResetBurst();
    if (Num <= 2) NDS.DMAs[(CPU*4)+3].ResetBurst();
}

u32 DMA::UnitTimings9_16(int burststart)
{
    u32 src_id = CurSrcAddr >> 14;
    u32 dst_id = CurDstAddr >> 14;

    u32 src_rgn = NDS.ARM9Regions[src_id];
    u32 dst_rgn = NDS.ARM9Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS.ARM9MemTimings[src_id][4];
    src_s = NDS.ARM9MemTimings[src_id][5];
    dst_n = NDS.ARM9MemTimings[dst_id][4];
    dst_s = NDS.ARM9MemTimings[dst_id][5];
    
    if (src_rgn & dst_rgn)
    {
        if (burststart != 1)
            return src_n + dst_n + (src_n == 1 || burststart <= 0);
        else
            return src_n + dst_n + (src_n != 1);
    }
    else
    {
        if (burststart == 2)
            return src_n + dst_n + (src_n == 1);
        else
            return src_s + dst_s;
    }
}

u32 DMA::UnitTimings9_32(int burststart)
{
    u32 src_id = CurSrcAddr >> 14;
    u32 dst_id = CurDstAddr >> 14;

    u32 src_rgn = NDS.ARM9Regions[src_id];
    u32 dst_rgn = NDS.ARM9Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS.ARM9MemTimings[src_id][6];
    src_s = NDS.ARM9MemTimings[src_id][7];
    dst_n = NDS.ARM9MemTimings[dst_id][6];
    dst_s = NDS.ARM9MemTimings[dst_id][7];

    if (src_rgn & dst_rgn)
    {
        if (burststart != 1)
            return src_n + dst_n + (src_n == 1 || burststart <= 0);
        else
            return src_n + dst_n + (src_n != 1);
    }
    else
    {
        if (burststart == 2)
            return src_n + dst_n + (src_n == 1);
        else
            return src_s + dst_s;
    }
}

// TODO: the ARM7 ones don't take into account that the two wifi regions have different timings

u32 DMA::UnitTimings7_16(int burststart)
{
    u32 src_id = CurSrcAddr >> 15;
    u32 dst_id = CurDstAddr >> 15;

    u32 src_rgn = NDS.ARM7Regions[src_id];
    u32 dst_rgn = NDS.ARM7Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS.ARM7MemTimings[src_id][0];
    src_s = NDS.ARM7MemTimings[src_id][1];
    dst_n = NDS.ARM7MemTimings[dst_id][0];
    dst_s = NDS.ARM7MemTimings[dst_id][1];

    if (src_rgn & dst_rgn)
    {
        if (burststart != 1)
            return src_n + dst_n + (src_n == 1 || burststart <= 0);
        else
            return src_n + dst_n + (src_n != 1);
    }
    else
    {
        if (burststart == 2)
            return src_n + dst_n + (src_n == 1);
        else
            return src_s + dst_s;
    }
}

u32 DMA::UnitTimings7_32(int burststart)
{
    u32 src_id = CurSrcAddr >> 15;
    u32 dst_id = CurDstAddr >> 15;

    u32 src_rgn = NDS.ARM7Regions[src_id];
    u32 dst_rgn = NDS.ARM7Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS.ARM7MemTimings[src_id][2];
    src_s = NDS.ARM7MemTimings[src_id][3];
    dst_n = NDS.ARM7MemTimings[dst_id][2];
    dst_s = NDS.ARM7MemTimings[dst_id][3];

    if (src_rgn & dst_rgn)
    {
        if (burststart != 1)
            return src_n + dst_n + (src_n == 1 || burststart <= 0);
        else
            return src_n + dst_n + (src_n != 1);
    }
    else
    {
        if (burststart == 2)
            return src_n + dst_n + (src_n == 1);
        else
            return src_s + dst_s;
    }
}

void DMA::Run9()
{
    //NDS.DMA9Timestamp = std::max(NDS.DMA9Timestamp, NDS.SysTimestamp << NDS.ARM9ClockShift);
    //NDS.DMA9Timestamp = (NDS.DMA9Timestamp + ((1<<NDS.ARM9ClockShift)-1)) & ~((1<<NDS.ARM9ClockShift)-1);

    if (NDS.DMA9Timestamp >= NDS.ARM9Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    int burststart = Running-1;

    if (!(Cnt & (1<<26)))
    {
        while (IterCount > 0 && !Stall)
        {
            u32 rgn = NDS.ARM9Regions[CurSrcAddr>>14] | NDS.ARM9Regions[CurDstAddr>>14];
            if (rgn & Mem9_MainRAM)
            {
                NDS.ARM9.MRTrack.Type = MainRAMType::DMA16;
                NDS.ARM9.MRTrack.Var = Num;
                return;
            }

            NDS.DMA9Timestamp += (UnitTimings9_16(burststart) << NDS.ARM9ClockShift);
            burststart -= 1;

            NDS.ARM9Write16(CurDstAddr, NDS.ARM9Read16(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<1;
            CurDstAddr += DstAddrInc<<1;
            IterCount--;
            RemCount--;

            if (NDS.DMA9Timestamp >= NDS.ARM9Target) break;
        }
    }
    else
    {
        while (IterCount > 0 && !Stall)
        {
            u32 rgn = NDS.ARM9Regions[CurSrcAddr>>14] | NDS.ARM9Regions[CurDstAddr>>14];
            if (rgn & Mem9_MainRAM)
            {
                NDS.ARM9.MRTrack.Type = MainRAMType::DMA32;
                NDS.ARM9.MRTrack.Var = Num;
                return;
            }

            NDS.DMA9Timestamp += (UnitTimings9_32(burststart) << NDS.ARM9ClockShift);
            burststart -= 1;

            NDS.ARM9Write32(CurDstAddr, NDS.ARM9Read32(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<2;
            CurDstAddr += DstAddrInc<<2;
            IterCount--;
            RemCount--;

            if (NDS.DMA9Timestamp >= NDS.ARM9Target) break;
        }
    }

    NDS.DMA9Timestamp -= 1;

    if (burststart <= 0) Running = 1;
    else Running = 2;

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            NDS.ResumeCPU(0, 1<<Num);

            if (StartMode == 0x07)
                NDS.GPU.GPU3D.CheckFIFODMA();
        }

        return;
    }

    if (!(Cnt & (1<<25)))
        Cnt &= ~(1<<31);

    if (Cnt & (1<<30))
        NDS.SetIRQ(0, IRQ_DMA0 + Num);

    Running = 0;
    InProgress = false;
    NDS.ResumeCPU(0, 1<<Num);

    if (DMAQueued) Start();
}

void DMA::Run7()
{
    if (NDS.ARM7Timestamp >= NDS.ARM7Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    int burststart = Running - 1;

    if (!(Cnt & (1<<26)))
    {
        while (IterCount > 0 && !Stall)
        {
            u32 rgn = NDS.ARM7Regions[CurSrcAddr>>15] | NDS.ARM7Regions[CurDstAddr>>15];
            if (rgn & Mem7_MainRAM)
            {
                NDS.ARM7.MRTrack.Type = MainRAMType::DMA16;
                NDS.ARM7.MRTrack.Var = Num+4;
                return;
            }

            NDS.ARM7Timestamp += UnitTimings7_16(burststart);
            burststart = false;

            NDS.ARM7Write16(CurDstAddr, NDS.ARM7Read16(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<1;
            CurDstAddr += DstAddrInc<<1;
            IterCount--;
            RemCount--;

            if (NDS.ARM7Timestamp >= NDS.ARM7Target) break;
        }
    }
    else
    {
        while (IterCount > 0 && !Stall)
        {
            u32 rgn = NDS.ARM7Regions[CurSrcAddr>>15] | NDS.ARM7Regions[CurDstAddr>>15];
            if (rgn & Mem7_MainRAM)
            {
                NDS.ARM7.MRTrack.Type = MainRAMType::DMA32;
                NDS.ARM7.MRTrack.Var = Num+4;
                return;
            }

            NDS.ARM7Timestamp += UnitTimings7_32(burststart);
            burststart = false;

            NDS.ARM7Write32(CurDstAddr, NDS.ARM7Read32(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<2;
            CurDstAddr += DstAddrInc<<2;
            IterCount--;
            RemCount--;

            if (NDS.ARM7Timestamp >= NDS.ARM7Target) break;
        }
    }

    if (burststart <= 0) Running = 1;
    else Running = 2;

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            NDS.ResumeCPU(1, 1<<Num);
        }

        return;
    }

    if (!(Cnt & (1<<25)))
        Cnt &= ~(1<<31);

    if (Cnt & (1<<30))
        NDS.SetIRQ(1, IRQ_DMA0 + Num);

    Running = 0;
    InProgress = false;
    NDS.ResumeCPU(1, 1<<Num);

    if (DMAQueued) Start();
}

void DMA::Run()
{
    if (!Running) return;
    if (CPU == 0) return Run9();
    else          return Run7();
}

}