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
#include "NDS.h"
#include "DSi.h"
#include "DMA.h"
#include "GPU.h"
#include "DMA_Timings.h"
#include "Platform.h"

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


DMA::DMA(u32 cpu, u32 num) :
    CPU(cpu),
    Num(num)
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

        if (CPU == 0)
            StartMode = (Cnt >> 27) & 0x7;
        else
            StartMode = ((Cnt >> 28) & 0x3) | 0x10;

        if ((StartMode & 0x7) == 0)
            Start();
        else if (StartMode == 0x07)
            GPU3D::CheckFIFODMA();

        if (StartMode==0x06 || StartMode==0x13)
            Log(LogLevel::Warn, "UNIMPLEMENTED ARM%d DMA%d START MODE %02X, %08X->%08X\n", CPU?7:9, Num, StartMode, SrcAddr, DstAddr);
    }
}

void DMA::Start()
{
    if (Running) return;

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

    Running = 2;

    // safety measure
    MRAMBurstTable = DMATiming::MRAMDummy;

    InProgress = true;
    NDS::StopCPU(CPU, 1<<Num);
}

u32 DMA::UnitTimings9_16(bool burststart)
{
    u32 src_id = CurSrcAddr >> 14;
    u32 dst_id = CurDstAddr >> 14;

    u32 src_rgn = NDS::ARM9Regions[src_id];
    u32 dst_rgn = NDS::ARM9Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS::ARM9MemTimings[src_id][4];
    src_s = NDS::ARM9MemTimings[src_id][5];
    dst_n = NDS::ARM9MemTimings[dst_id][4];
    dst_s = NDS::ARM9MemTimings[dst_id][5];

    if (src_rgn == NDS::Mem9_MainRAM)
    {
        if (dst_rgn == NDS::Mem9_MainRAM)
            return 16;

        if (SrcAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (dst_rgn == NDS::Mem9_GBAROM)
                {
                    if (dst_s == 4)
                        MRAMBurstTable = DMATiming::MRAMRead16Bursts[1];
                    else
                        MRAMBurstTable = DMATiming::MRAMRead16Bursts[2];
                }
                else
                    MRAMBurstTable = DMATiming::MRAMRead16Bursts[0];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            // TODO: not quite right for GBA slot
            return (((CurSrcAddr & 0x1F) == 0x1E) ? 7 : 8) +
                   (burststart ? dst_n : dst_s);
        }
    }
    else if (dst_rgn == NDS::Mem9_MainRAM)
    {
        if (DstAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (src_rgn == NDS::Mem9_GBAROM)
                {
                    if (src_s == 4)
                        MRAMBurstTable = DMATiming::MRAMWrite16Bursts[1];
                    else
                        MRAMBurstTable = DMATiming::MRAMWrite16Bursts[2];
                }
                else
                    MRAMBurstTable = DMATiming::MRAMWrite16Bursts[0];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            return (burststart ? src_n : src_s) + 7;
        }
    }
    else if (src_rgn & dst_rgn)
    {
        return src_n + dst_n + 1;
    }
    else
    {
        if (burststart)
            return src_n + dst_n;
        else
            return src_s + dst_s;
    }
}

u32 DMA::UnitTimings9_32(bool burststart)
{
    u32 src_id = CurSrcAddr >> 14;
    u32 dst_id = CurDstAddr >> 14;

    u32 src_rgn = NDS::ARM9Regions[src_id];
    u32 dst_rgn = NDS::ARM9Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS::ARM9MemTimings[src_id][6];
    src_s = NDS::ARM9MemTimings[src_id][7];
    dst_n = NDS::ARM9MemTimings[dst_id][6];
    dst_s = NDS::ARM9MemTimings[dst_id][7];

    if (src_rgn == NDS::Mem9_MainRAM)
    {
        if (dst_rgn == NDS::Mem9_MainRAM)
            return 18;

        if (SrcAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (dst_rgn == NDS::Mem9_GBAROM)
                {
                    if (dst_s == 8)
                        MRAMBurstTable = DMATiming::MRAMRead32Bursts[2];
                    else
                        MRAMBurstTable = DMATiming::MRAMRead32Bursts[3];
                }
                else if (dst_n == 2)
                    MRAMBurstTable = DMATiming::MRAMRead32Bursts[0];
                else
                    MRAMBurstTable = DMATiming::MRAMRead32Bursts[1];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            // TODO: not quite right for GBA slot
            return (((CurSrcAddr & 0x1F) == 0x1C) ? (dst_n==2 ? 7:8) : 9) +
                   (burststart ? dst_n : dst_s);
        }
    }
    else if (dst_rgn == NDS::Mem9_MainRAM)
    {
        if (DstAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (src_rgn == NDS::Mem9_GBAROM)
                {
                    if (src_s == 8)
                        MRAMBurstTable = DMATiming::MRAMWrite32Bursts[2];
                    else
                        MRAMBurstTable = DMATiming::MRAMWrite32Bursts[3];
                }
                else if (src_n == 2)
                    MRAMBurstTable = DMATiming::MRAMWrite32Bursts[0];
                else
                    MRAMBurstTable = DMATiming::MRAMWrite32Bursts[1];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            return (burststart ? src_n : src_s) + 8;
        }
    }
    else if (src_rgn & dst_rgn)
    {
        return src_n + dst_n + 1;
    }
    else
    {
        if (burststart)
            return src_n + dst_n;
        else
            return src_s + dst_s;
    }
}

// TODO: the ARM7 ones don't take into account that the two wifi regions have different timings

u32 DMA::UnitTimings7_16(bool burststart)
{
    u32 src_id = CurSrcAddr >> 15;
    u32 dst_id = CurDstAddr >> 15;

    u32 src_rgn = NDS::ARM7Regions[src_id];
    u32 dst_rgn = NDS::ARM7Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS::ARM7MemTimings[src_id][0];
    src_s = NDS::ARM7MemTimings[src_id][1];
    dst_n = NDS::ARM7MemTimings[dst_id][0];
    dst_s = NDS::ARM7MemTimings[dst_id][1];

    if (src_rgn == NDS::Mem7_MainRAM)
    {
        if (dst_rgn == NDS::Mem7_MainRAM)
            return 16;

        if (SrcAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (dst_rgn == NDS::Mem7_GBAROM || dst_rgn == NDS::Mem7_Wifi0 || dst_rgn == NDS::Mem7_Wifi1)
                {
                    if (dst_s == 4)
                        MRAMBurstTable = DMATiming::MRAMRead16Bursts[1];
                    else
                        MRAMBurstTable = DMATiming::MRAMRead16Bursts[2];
                }
                else
                    MRAMBurstTable = DMATiming::MRAMRead16Bursts[0];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            // TODO: not quite right for GBA slot
            return (((CurSrcAddr & 0x1F) == 0x1E) ? 7 : 8) +
                   (burststart ? dst_n : dst_s);
        }
    }
    else if (dst_rgn == NDS::Mem7_MainRAM)
    {
        if (DstAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (src_rgn == NDS::Mem7_GBAROM || src_rgn == NDS::Mem7_Wifi0 || src_rgn == NDS::Mem7_Wifi1)
                {
                    if (src_s == 4)
                        MRAMBurstTable = DMATiming::MRAMWrite16Bursts[1];
                    else
                        MRAMBurstTable = DMATiming::MRAMWrite16Bursts[2];
                }
                else
                    MRAMBurstTable = DMATiming::MRAMWrite16Bursts[0];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            return (burststart ? src_n : src_s) + 7;
        }
    }
    else if (src_rgn & dst_rgn)
    {
        return src_n + dst_n + 1;
    }
    else
    {
        if (burststart)
            return src_n + dst_n;
        else
            return src_s + dst_s;
    }
}

u32 DMA::UnitTimings7_32(bool burststart)
{
    u32 src_id = CurSrcAddr >> 15;
    u32 dst_id = CurDstAddr >> 15;

    u32 src_rgn = NDS::ARM7Regions[src_id];
    u32 dst_rgn = NDS::ARM7Regions[dst_id];

    u32 src_n, src_s, dst_n, dst_s;
    src_n = NDS::ARM7MemTimings[src_id][2];
    src_s = NDS::ARM7MemTimings[src_id][3];
    dst_n = NDS::ARM7MemTimings[dst_id][2];
    dst_s = NDS::ARM7MemTimings[dst_id][3];

    if (src_rgn == NDS::Mem7_MainRAM)
    {
        if (dst_rgn == NDS::Mem7_MainRAM)
            return 18;

        if (SrcAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (dst_rgn == NDS::Mem7_GBAROM || dst_rgn == NDS::Mem7_Wifi0 || dst_rgn == NDS::Mem7_Wifi1)
                {
                    if (dst_s == 8)
                        MRAMBurstTable = DMATiming::MRAMRead32Bursts[2];
                    else
                        MRAMBurstTable = DMATiming::MRAMRead32Bursts[3];
                }
                else if (dst_n == 2)
                    MRAMBurstTable = DMATiming::MRAMRead32Bursts[0];
                else
                    MRAMBurstTable = DMATiming::MRAMRead32Bursts[1];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            // TODO: not quite right for GBA slot
            return (((CurSrcAddr & 0x1F) == 0x1C) ? (dst_n==2 ? 7:8) : 9) +
                   (burststart ? dst_n : dst_s);
        }
    }
    else if (dst_rgn == NDS::Mem7_MainRAM)
    {
        if (DstAddrInc > 0)
        {
            if (burststart || MRAMBurstTable[MRAMBurstCount] == 0)
            {
                MRAMBurstCount = 0;

                if (src_rgn == NDS::Mem7_GBAROM || src_rgn == NDS::Mem7_Wifi0 || src_rgn == NDS::Mem7_Wifi1)
                {
                    if (src_s == 8)
                        MRAMBurstTable = DMATiming::MRAMWrite32Bursts[2];
                    else
                        MRAMBurstTable = DMATiming::MRAMWrite32Bursts[3];
                }
                else if (src_n == 2)
                    MRAMBurstTable = DMATiming::MRAMWrite32Bursts[0];
                else
                    MRAMBurstTable = DMATiming::MRAMWrite32Bursts[1];
            }

            u32 ret = MRAMBurstTable[MRAMBurstCount++];
            return ret;
        }
        else
        {
            return (burststart ? src_n : src_s) + 8;
        }
    }
    else if (src_rgn & dst_rgn)
    {
        return src_n + dst_n + 1;
    }
    else
    {
        if (burststart)
            return src_n + dst_n;
        else
            return src_s + dst_s;
    }
}

template <int ConsoleType>
void DMA::Run9()
{
    if (NDS::ARM9Timestamp >= NDS::ARM9Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    bool burststart = (Running == 2);
    Running = 1;

    if (!(Cnt & (1<<26)))
    {
        while (IterCount > 0 && !Stall)
        {
            NDS::ARM9Timestamp += (UnitTimings9_16(burststart) << NDS::ARM9ClockShift);
            burststart = false;

            if (ConsoleType == 1)
                DSi::ARM9Write16(CurDstAddr, DSi::ARM9Read16(CurSrcAddr));
            else
                NDS::ARM9Write16(CurDstAddr, NDS::ARM9Read16(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<1;
            CurDstAddr += DstAddrInc<<1;
            IterCount--;
            RemCount--;

            if (NDS::ARM9Timestamp >= NDS::ARM9Target) break;
        }
    }
    else
    {
        while (IterCount > 0 && !Stall)
        {
            NDS::ARM9Timestamp += (UnitTimings9_32(burststart) << NDS::ARM9ClockShift);
            burststart = false;

            if (ConsoleType == 1)
                DSi::ARM9Write32(CurDstAddr, DSi::ARM9Read32(CurSrcAddr));
            else
                NDS::ARM9Write32(CurDstAddr, NDS::ARM9Read32(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<2;
            CurDstAddr += DstAddrInc<<2;
            IterCount--;
            RemCount--;

            if (NDS::ARM9Timestamp >= NDS::ARM9Target) break;
        }
    }

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            NDS::ResumeCPU(0, 1<<Num);

            if (StartMode == 0x07)
                GPU3D::CheckFIFODMA();
        }

        return;
    }

    if (!(Cnt & (1<<25)))
        Cnt &= ~(1<<31);

    if (Cnt & (1<<30))
        NDS::SetIRQ(0, NDS::IRQ_DMA0 + Num);

    Running = 0;
    InProgress = false;
    NDS::ResumeCPU(0, 1<<Num);
}

template <int ConsoleType>
void DMA::Run7()
{
    if (NDS::ARM7Timestamp >= NDS::ARM7Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    bool burststart = (Running == 2);
    Running = 1;

    if (!(Cnt & (1<<26)))
    {
        while (IterCount > 0 && !Stall)
        {
            NDS::ARM7Timestamp += UnitTimings7_16(burststart);
            burststart = false;

            if (ConsoleType == 1)
                DSi::ARM7Write16(CurDstAddr, DSi::ARM7Read16(CurSrcAddr));
            else
                NDS::ARM7Write16(CurDstAddr, NDS::ARM7Read16(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<1;
            CurDstAddr += DstAddrInc<<1;
            IterCount--;
            RemCount--;

            if (NDS::ARM7Timestamp >= NDS::ARM7Target) break;
        }
    }
    else
    {
        while (IterCount > 0 && !Stall)
        {
            NDS::ARM7Timestamp += UnitTimings7_32(burststart);
            burststart = false;

            if (ConsoleType == 1)
                DSi::ARM7Write32(CurDstAddr, DSi::ARM7Read32(CurSrcAddr));
            else
                NDS::ARM7Write32(CurDstAddr, NDS::ARM7Read32(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<2;
            CurDstAddr += DstAddrInc<<2;
            IterCount--;
            RemCount--;

            if (NDS::ARM7Timestamp >= NDS::ARM7Target) break;
        }
    }

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            NDS::ResumeCPU(1, 1<<Num);
        }

        return;
    }

    if (!(Cnt & (1<<25)))
        Cnt &= ~(1<<31);

    if (Cnt & (1<<30))
        NDS::SetIRQ(1, NDS::IRQ_DMA0 + Num);

    Running = 0;
    InProgress = false;
    NDS::ResumeCPU(1, 1<<Num);
}

template <int ConsoleType>
void DMA::Run()
{
    if (!Running) return;
    if (CPU == 0) return Run9<ConsoleType>();
    else          return Run7<ConsoleType>();
}

template void DMA::Run<0>();
template void DMA::Run<1>();
