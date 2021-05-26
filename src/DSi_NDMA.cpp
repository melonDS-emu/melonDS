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
#include "DSi_NDMA.h"
#include "GPU.h"
#include "DSi_AES.h"



DSi_NDMA::DSi_NDMA(u32 cpu, u32 num)
{
    CPU = cpu;
    Num = num;

    Reset();
}

DSi_NDMA::~DSi_NDMA()
{
}

void DSi_NDMA::Reset()
{
    SrcAddr = 0;
    DstAddr = 0;
    TotalLength = 0;
    BlockLength = 0;
    SubblockTimer = 0;
    FillData = 0;
    Cnt = 0;

    StartMode = 0;
    CurSrcAddr = 0;
    CurDstAddr = 0;
    SubblockLength = 0;
    RemCount = 0;
    IterCount = 0;
    TotalRemCount = 0;
    SrcAddrInc = 0;
    DstAddrInc = 0;

    Running = false;
    InProgress = false;
}

void DSi_NDMA::DoSavestate(Savestate* file)
{
    // TODO!
}

void DSi_NDMA::WriteCnt(u32 val)
{
    u32 oldcnt = Cnt;
    Cnt = val;

    if ((!(oldcnt & 0x80000000)) && (val & 0x80000000)) // checkme
    {
        CurSrcAddr = SrcAddr;
        CurDstAddr = DstAddr;
        TotalRemCount = TotalLength;

        switch ((Cnt >> 10) & 0x3)
        {
        case 0: DstAddrInc = 1; break;
        case 1: DstAddrInc = -1; break;
        case 2: DstAddrInc = 0; break;
        case 3: DstAddrInc = 1; printf("BAD NDMA DST INC MODE 3\n"); break;
        }

        switch ((Cnt >> 13) & 0x3)
        {
        case 0: SrcAddrInc = 1; break;
        case 1: SrcAddrInc = -1; break;
        case 2: SrcAddrInc = 0; break;
        case 3: SrcAddrInc = 0; break; // fill mode
        }

        StartMode = (Cnt >> 24) & 0x1F;
        if (StartMode > 0x10) StartMode = 0x10;
        if (CPU == 1) StartMode |= 0x20;

        if ((StartMode & 0x1F) == 0x10)
            Start();

        if (StartMode != 0x10 && StartMode != 0x30 &&
            StartMode != 0x04 && StartMode != 0x06 && StartMode != 0x07 && StartMode != 0x08 && StartMode != 0x09 && StartMode != 0x0B &&
            StartMode != 0x24 && StartMode != 0x26 && StartMode != 0x28 && StartMode != 0x29 && StartMode != 0x2A && StartMode != 0x2B)
            printf("UNIMPLEMENTED ARM%d NDMA%d START MODE %02X, %08X->%08X LEN=%d BLK=%d CNT=%08X\n",
                   CPU?7:9, Num, StartMode, SrcAddr, DstAddr, TotalLength, BlockLength, Cnt);
    }
}

void DSi_NDMA::Start()
{
    if (Running) return;

    if (!InProgress)
    {
        RemCount = BlockLength;
        if (!RemCount)
            RemCount = 0x1000000;
    }

    // TODO: how does GXFIFO DMA work with all the block shito?
    IterCount = RemCount;

    if (((StartMode & 0x1F) != 0x10) && !(Cnt & (1<<29)))
    {
        if (IterCount > TotalRemCount)
        {
            IterCount = TotalRemCount;
            RemCount = IterCount;
        }
    }

    if (Cnt & (1<<12)) CurDstAddr = DstAddr;
    if (Cnt & (1<<15)) CurSrcAddr = SrcAddr;

    //printf("ARM%d NDMA%d %08X %02X %08X->%08X %d bytes, total=%d\n", CPU?7:9, Num, Cnt, StartMode, CurSrcAddr, CurDstAddr, RemCount*4, TotalRemCount*4);

    //IsGXFIFODMA = (CPU == 0 && (CurSrcAddr>>24) == 0x02 && CurDstAddr == 0x04000400 && DstAddrInc == 0);

    // TODO eventually: not stop if we're running code in ITCM

    //if (SubblockTimer & 0xFFFF)
    //    printf("TODO! NDMA SUBBLOCK TIMER: %08X\n", SubblockTimer);

    if (NDS::DMAsRunning(CPU))
        Running = 1;
    else
        Running = 2;

    InProgress = true;
    NDS::StopCPU(CPU, 1<<(Num+4));
}

void DSi_NDMA::Run()
{
    if (!Running) return;
    if (CPU == 0) return Run9();
    else          return Run7();
}

void DSi_NDMA::Run9()
{
    if (NDS::ARM9Timestamp >= NDS::ARM9Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    bool burststart = (Running == 2);
    Running = 1;

    s32 unitcycles;
    //s32 lastcycles = cycles;

    bool dofill = ((Cnt >> 13) & 0x3) == 3;

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

        /*if (burststart)
        {
            cycles -= 2;
            cycles -= (NDS::ARM9MemTimings[CurSrcAddr >> 14][2] + NDS::ARM9MemTimings[CurDstAddr >> 14][2]);
            cycles += unitcycles;
        }*/
    }

    while (IterCount > 0 && !Stall)
    {
        NDS::ARM9Timestamp += (unitcycles << NDS::ARM9ClockShift);

        if (dofill)
            DSi::ARM9Write32(CurDstAddr, FillData);
        else
            DSi::ARM9Write32(CurDstAddr, DSi::ARM9Read32(CurSrcAddr));

        CurSrcAddr += SrcAddrInc<<2;
        CurDstAddr += DstAddrInc<<2;
        IterCount--;
        RemCount--;
        TotalRemCount--;

        if (NDS::ARM9Timestamp >= NDS::ARM9Target) break;
    }

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            NDS::ResumeCPU(0, 1<<(Num+4));

            //if (StartMode == 0x07)
            //    GPU3D::CheckFIFODMA();
        }

        return;
    }

    if ((StartMode & 0x1F) == 0x10) // CHECKME
    {
        Cnt &= ~(1<<31);
        if (Cnt & (1<<30)) NDS::SetIRQ(0, NDS::IRQ_DSi_NDMA0 + Num);
    }
    else if (!(Cnt & (1<<29)))
    {
        if (TotalRemCount == 0)
        {
            Cnt &= ~(1<<31);
            if (Cnt & (1<<30)) NDS::SetIRQ(0, NDS::IRQ_DSi_NDMA0 + Num);
        }
    }

    Running = 0;
    InProgress = false;
    NDS::ResumeCPU(0, 1<<(Num+4));
}

void DSi_NDMA::Run7()
{
    if (NDS::ARM7Timestamp >= NDS::ARM7Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    bool burststart = (Running == 2);
    Running = 1;

    s32 unitcycles;
    //s32 lastcycles = cycles;

    bool dofill = ((Cnt >> 13) & 0x3) == 3;

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

        /*if (burststart)
        {
            cycles -= 2;
            cycles -= (NDS::ARM7MemTimings[CurSrcAddr >> 15][2] + NDS::ARM7MemTimings[CurDstAddr >> 15][2]);
            cycles += unitcycles;
        }*/
    }

    while (IterCount > 0 && !Stall)
    {
        NDS::ARM7Timestamp += unitcycles;

        if (dofill)
            DSi::ARM7Write32(CurDstAddr, FillData);
        else
            DSi::ARM7Write32(CurDstAddr, DSi::ARM7Read32(CurSrcAddr));

        CurSrcAddr += SrcAddrInc<<2;
        CurDstAddr += DstAddrInc<<2;
        IterCount--;
        RemCount--;
        TotalRemCount--;

        if (NDS::ARM7Timestamp >= NDS::ARM7Target) break;
    }

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            NDS::ResumeCPU(1, 1<<(Num+4));

            DSi_AES::CheckInputDMA();
            DSi_AES::CheckOutputDMA();
        }

        return;
    }

    if ((StartMode & 0x1F) == 0x10) // CHECKME
    {
        Cnt &= ~(1<<31);
        if (Cnt & (1<<30)) NDS::SetIRQ(1, NDS::IRQ_DSi_NDMA0 + Num);
    }
    else if (!(Cnt & (1<<29)))
    {
        if (TotalRemCount == 0)
        {
            Cnt &= ~(1<<31);
            if (Cnt & (1<<30)) NDS::SetIRQ(1, NDS::IRQ_DSi_NDMA0 + Num);
        }
    }

    Running = 0;
    InProgress = false;
    NDS::ResumeCPU(1, 1<<(Num+4));

    DSi_AES::CheckInputDMA();
    DSi_AES::CheckOutputDMA();
}
