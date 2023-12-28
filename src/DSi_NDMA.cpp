/*
    Copyright 2016-2023 melonDS team

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

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

DSi_NDMA::DSi_NDMA(u32 cpu, u32 num, melonDS::DSi& dsi) : DSi(dsi), CPU(cpu), Num(num)
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
    char magic[5] = "NDMx";
    magic[3] = '0' + Num + (CPU*4);
    file->Section(magic);

    file->Var32(&SrcAddr);
    file->Var32(&DstAddr);
    file->Var32(&TotalLength);
    file->Var32(&BlockLength);
    file->Var32(&SubblockTimer);
    file->Var32(&FillData);
    file->Var32(&Cnt);

    file->Var32(&StartMode);
    file->Var32(&CurSrcAddr);
    file->Var32(&CurDstAddr);
    file->Var32(&SubblockLength);
    file->Var32(&RemCount);
    file->Var32(&IterCount);
    file->Var32(&TotalRemCount);
    file->Var32(&SrcAddrInc);
    file->Var32(&DstAddrInc);

    file->Var32(&Running);
    file->Bool32(&InProgress);
    file->Bool32(&IsGXFIFODMA);
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
        case 3: DstAddrInc = 1; Log(LogLevel::Warn, "BAD NDMA DST INC MODE 3\n"); break;
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
        else if (StartMode == 0x0A)
            DSi.GPU.GPU3D.CheckFIFODMA();

        // TODO: unsupported start modes:
        // * timers (00-03)
        // * camera (ARM9 0B)
        // * microphone (ARM7 0C)
        // * NDS-wifi?? (ARM7 07, likely not working)

        if (StartMode <= 0x03 || StartMode == 0x05 || (StartMode >= 0x0C && StartMode <= 0x0F) ||
            (StartMode >= 0x20 && StartMode <= 0x23) || StartMode == 0x25 || StartMode == 0x27 || (StartMode >= 0x2C && StartMode <= 0x2F))
            Log(LogLevel::Warn, "UNIMPLEMENTED ARM%d NDMA%d START MODE %02X, %08X->%08X LEN=%d BLK=%d CNT=%08X\n",
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

    // CHECKME: this is assumed to work the same as the old DMA version
    // also not really certain how this interacts with the block subdivision system here
    if (StartMode == 0x0A && RemCount > 112)
        IterCount = 112;
    else
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

    if (DSi.DMAsRunning(CPU))
        Running = 1;
    else
        Running = 2;

    InProgress = true;
    DSi.StopCPU(CPU, 1<<(Num+4));
}

void DSi_NDMA::Run()
{
    if (!Running) return;
    if (CPU == 0) return Run9();
    else          return Run7();
}

void DSi_NDMA::Run9()
{
    if (DSi.ARM9Timestamp >= DSi.ARM9Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    bool burststart = (Running == 2);
    Running = 1;

    s32 unitcycles;
    //s32 lastcycles = cycles;

    bool dofill = ((Cnt >> 13) & 0x3) == 3;

    if ((CurSrcAddr >> 24) == 0x02 && (CurDstAddr >> 24) == 0x02)
    {
        unitcycles = DSi.ARM9MemTimings[CurSrcAddr >> 14][2] + DSi.ARM9MemTimings[CurDstAddr >> 14][2];
    }
    else
    {
        unitcycles = DSi.ARM9MemTimings[CurSrcAddr >> 14][3] + DSi.ARM9MemTimings[CurDstAddr >> 14][3];
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
        DSi.ARM9Timestamp += (unitcycles << DSi.ARM9ClockShift);

        if (dofill)
            DSi.ARM9Write32(CurDstAddr, FillData);
        else
            DSi.ARM9Write32(CurDstAddr, DSi.ARM9Read32(CurSrcAddr));

        CurSrcAddr += SrcAddrInc<<2;
        CurDstAddr += DstAddrInc<<2;
        IterCount--;
        RemCount--;
        TotalRemCount--;

        if (DSi.ARM9Timestamp >= DSi.ARM9Target) break;
    }

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            DSi.ResumeCPU(0, 1<<(Num+4));

            if (StartMode == 0x0A)
                DSi.GPU.GPU3D.CheckFIFODMA();
        }

        return;
    }

    if ((StartMode & 0x1F) == 0x10) // CHECKME
    {
        Cnt &= ~(1<<31);
        if (Cnt & (1<<30)) DSi.SetIRQ(0, IRQ_DSi_NDMA0 + Num);
    }
    else if (!(Cnt & (1<<29)))
    {
        if (TotalRemCount == 0)
        {
            Cnt &= ~(1<<31);
            if (Cnt & (1<<30)) DSi.SetIRQ(0, IRQ_DSi_NDMA0 + Num);
        }
    }

    Running = 0;
    InProgress = false;
    DSi.ResumeCPU(0, 1<<(Num+4));
}

void DSi_NDMA::Run7()
{
    if (DSi.ARM7Timestamp >= DSi.ARM7Target) return;

    Executing = true;

    // add NS penalty for first accesses in burst
    bool burststart = (Running == 2);
    Running = 1;

    s32 unitcycles;
    //s32 lastcycles = cycles;

    bool dofill = ((Cnt >> 13) & 0x3) == 3;

    if ((CurSrcAddr >> 24) == 0x02 && (CurDstAddr >> 24) == 0x02)
    {
        unitcycles = DSi.ARM7MemTimings[CurSrcAddr >> 15][2] + DSi.ARM7MemTimings[CurDstAddr >> 15][2];
    }
    else
    {
        unitcycles = DSi.ARM7MemTimings[CurSrcAddr >> 15][3] + DSi.ARM7MemTimings[CurDstAddr >> 15][3];
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
        DSi.ARM7Timestamp += unitcycles;

        if (dofill)
            DSi.ARM7Write32(CurDstAddr, FillData);
        else
            DSi.ARM7Write32(CurDstAddr, DSi.ARM7Read32(CurSrcAddr));

        CurSrcAddr += SrcAddrInc<<2;
        CurDstAddr += DstAddrInc<<2;
        IterCount--;
        RemCount--;
        TotalRemCount--;

        if (DSi.ARM7Timestamp >= DSi.ARM7Target) break;
    }

    Executing = false;
    Stall = false;

    if (RemCount)
    {
        if (IterCount == 0)
        {
            Running = 0;
            DSi.ResumeCPU(1, 1<<(Num+4));

            DSi.AES.CheckInputDMA();
            DSi.AES.CheckOutputDMA();
        }

        return;
    }

    if ((StartMode & 0x1F) == 0x10) // CHECKME
    {
        Cnt &= ~(1<<31);
        if (Cnt & (1<<30)) DSi.SetIRQ(1, IRQ_DSi_NDMA0 + Num);
    }
    else if (!(Cnt & (1<<29)))
    {
        if (TotalRemCount == 0)
        {
            Cnt &= ~(1<<31);
            if (Cnt & (1<<30)) DSi.SetIRQ(1, IRQ_DSi_NDMA0 + Num);
        }
    }

    Running = 0;
    InProgress = false;
    DSi.ResumeCPU(1, 1<<(Num+4));

    DSi.AES.CheckInputDMA();
    DSi.AES.CheckOutputDMA();
}

}