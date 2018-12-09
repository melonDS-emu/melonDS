/*
    Copyright 2016-2019 StapleButter

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
#include "DMA.h"
#include "NDSCart.h"
#include "GPU.h"


// NOTES ON DMA SHIT
//
// * could use optimized code paths for common types of DMA transfers. for example, VRAM
//   have to profile it to see if it's actually worth doing


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


DMA::DMA(u32 cpu, u32 num)
{
    CPU = cpu;
    Num = num;

    if (cpu == 0)
        CountMask = 0x001FFFFF;
    else
        CountMask = (num==3 ? 0x0000FFFF : 0x00003FFF);

    Reset();
}

DMA::~DMA()
{
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

    Running = false;
    InProgress = false;
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
    file->Var32(&SrcAddrInc);
    file->Var32(&DstAddrInc);

    file->Var32((u32*)&Running);
    file->Var32((u32*)&InProgress);
    file->Var32((u32*)&IsGXFIFODMA);
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
        case 0x01800000: SrcAddrInc = 1; printf("BAD DMA SRC INC MODE 3\n"); break;
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
            printf("UNIMPLEMENTED ARM%d DMA%d START MODE %02X, %08X->%08X\n", CPU?7:9, Num, StartMode, SrcAddr, DstAddr);
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

    if ((Cnt & 0x00600000) == 0x00600000)
        CurDstAddr = DstAddr;

    //printf("ARM%d DMA%d %08X %02X %08X->%08X %d bytes %dbit\n", CPU?7:9, Num, Cnt, StartMode, CurSrcAddr, CurDstAddr, RemCount*((Cnt&0x04000000)?4:2), (Cnt&0x04000000)?32:16);

    IsGXFIFODMA = (CPU == 0 && (CurSrcAddr>>24) == 0x02 && CurDstAddr == 0x04000400 && DstAddrInc == 0);

    // TODO eventually: not stop if we're running code in ITCM

    Running = true;
    InProgress = true;
    NDS::StopCPU(CPU, 1<<Num);
}

s32 DMA::Run(s32 cycles)
{
    if (!Running)
        return cycles;

    Executing = true;

    // add NS penalty for first accesses in burst
    bool burststart;
    if (StartMode == 0x07 && RemCount > 112)
        burststart = (IterCount == 112);
    else
        burststart = (IterCount == RemCount);

    if (!(Cnt & 0x04000000))
    {
        int unitcycles;
        if (Num == 0)
        {
            if ((CurSrcAddr >> 24) == 0x02 && (CurDstAddr >> 24) == 0x02)
            {
                unitcycles = NDS::ARM9MemTimings[CurSrcAddr >> 14][0] + NDS::ARM9MemTimings[CurDstAddr >> 14][0];
            }
            else
            {
                unitcycles = NDS::ARM9MemTimings[CurSrcAddr >> 14][1] + NDS::ARM9MemTimings[CurDstAddr >> 14][1];
                if ((CurSrcAddr >> 24) == (CurDstAddr >> 24))
                    unitcycles++;

                if (burststart)
                {
                    cycles -= (NDS::ARM9MemTimings[CurSrcAddr >> 14][0] + NDS::ARM9MemTimings[CurDstAddr >> 14][0]);
                    cycles += unitcycles;
                }
            }
        }
        else
        {
            if ((CurSrcAddr >> 24) == 0x02 && (CurDstAddr >> 24) == 0x02)
            {
                unitcycles = NDS::ARM7MemTimings[CurSrcAddr >> 15][0] + NDS::ARM7MemTimings[CurDstAddr >> 15][0];
            }
            else
            {
                unitcycles = NDS::ARM7MemTimings[CurSrcAddr >> 15][1] + NDS::ARM7MemTimings[CurDstAddr >> 15][1];
                if ((CurSrcAddr >> 23) == (CurDstAddr >> 23))
                    unitcycles++;

                if (burststart)
                {
                    cycles -= (NDS::ARM7MemTimings[CurSrcAddr >> 15][0] + NDS::ARM7MemTimings[CurDstAddr >> 15][0]);
                    cycles += unitcycles;
                }
            }
        }

        u16 (*readfn)(u32) = CPU ? NDS::ARM7Read16 : NDS::ARM9Read16;
        void (*writefn)(u32,u16) = CPU ? NDS::ARM7Write16 : NDS::ARM9Write16;

        while (IterCount > 0 && !Stall)
        {
            writefn(CurDstAddr, readfn(CurSrcAddr));

            cycles -= unitcycles;
            NDS::RunTimingCriticalDevices(CPU, unitcycles);

            CurSrcAddr += SrcAddrInc<<1;
            CurDstAddr += DstAddrInc<<1;
            IterCount--;
            RemCount--;

            if (cycles < 0) break;
        }
    }
    else
    {
        int unitcycles;
        if (Num == 0)
        {
            if ((CurSrcAddr >> 24) == 0x02 && (CurDstAddr >> 24) == 0x02)
            {
                unitcycles = NDS::ARM9MemTimings[CurSrcAddr >> 14][2] + NDS::ARM9MemTimings[CurDstAddr >> 14][2];
            }
            else
            {
                unitcycles = NDS::ARM9MemTimings[CurSrcAddr >> 14][3] + NDS::ARM9MemTimings[CurDstAddr >> 14][3];
                if ((CurSrcAddr >> 24) == (CurDstAddr >> 24))
                    unitcycles++;
                else if ((CurSrcAddr >> 24) == 0x02)
                    unitcycles--;

                if (burststart)
                {
                    cycles -= (NDS::ARM9MemTimings[CurSrcAddr >> 14][2] + NDS::ARM9MemTimings[CurDstAddr >> 14][2]);
                    cycles += unitcycles;
                }
            }
        }
        else
        {
            if ((CurSrcAddr >> 24) == 0x02 && (CurDstAddr >> 24) == 0x02)
            {
                unitcycles = NDS::ARM7MemTimings[CurSrcAddr >> 15][2] + NDS::ARM7MemTimings[CurDstAddr >> 15][2];
            }
            else
            {
                unitcycles = NDS::ARM7MemTimings[CurSrcAddr >> 15][3] + NDS::ARM7MemTimings[CurDstAddr >> 15][3];
                if ((CurSrcAddr >> 23) == (CurDstAddr >> 23))
                    unitcycles++;
                else if ((CurSrcAddr >> 24) == 0x02)
                    unitcycles--;

                if (burststart)
                {
                    cycles -= (NDS::ARM7MemTimings[CurSrcAddr >> 15][2] + NDS::ARM7MemTimings[CurDstAddr >> 15][2]);
                    cycles += unitcycles;
                }
            }
        }

        u32 (*readfn)(u32) = CPU ? NDS::ARM7Read32 : NDS::ARM9Read32;
        void (*writefn)(u32,u32) = CPU ? NDS::ARM7Write32 : NDS::ARM9Write32;

        while (IterCount > 0 && !Stall)
        {
            writefn(CurDstAddr, readfn(CurSrcAddr));

            cycles -= unitcycles;
            NDS::RunTimingCriticalDevices(CPU, unitcycles);

            CurSrcAddr += SrcAddrInc<<2;
            CurDstAddr += DstAddrInc<<2;
            IterCount--;
            RemCount--;

            if (cycles < 0) break;
        }
    }

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = false;
            NDS::ResumeCPU(CPU, 1<<Num);

            if (StartMode == 0x07)
                GPU3D::CheckFIFODMA();
        }

        return cycles;
    }

    if (!(Cnt & 0x02000000))
        Cnt &= ~0x80000000;

    if (Cnt & 0x40000000)
        NDS::SetIRQ(CPU, NDS::IRQ_DMA0 + Num);

    Running = false;
    InProgress = false;
    NDS::ResumeCPU(CPU, 1<<Num);

    return cycles - 2;
}
