/*
    Copyright 2016-2017 StapleButter

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


DMA::DMA(u32 cpu, u32 num)
{
    CPU = cpu;
    Num = num;

    if (cpu == 0)
        CountMask = 0x001FFFFF;
    else
        CountMask = (num==3 ? 0x0000FFFF : 0x00003FFF);

    // TODO: merge with the one in ARM.cpp, somewhere
    for (int i = 0; i < 16; i++)
    {
        Waitstates[0][i] = 1;
        Waitstates[1][i] = 1;
    }

    if (!cpu)
    {
        // ARM9
        // note: 33MHz cycles
        Waitstates[0][0x2] = 1;
        Waitstates[0][0x3] = 1;
        Waitstates[0][0x4] = 1;
        Waitstates[0][0x5] = 1;
        Waitstates[0][0x6] = 1;
        Waitstates[0][0x7] = 1;
        Waitstates[0][0x8] = 6;
        Waitstates[0][0x9] = 6;
        Waitstates[0][0xA] = 10;
        Waitstates[0][0xF] = 1;

        Waitstates[1][0x2] = 2;
        Waitstates[1][0x3] = 1;
        Waitstates[1][0x4] = 1;
        Waitstates[1][0x5] = 2;
        Waitstates[1][0x6] = 2;
        Waitstates[1][0x7] = 1;
        Waitstates[1][0x8] = 12;
        Waitstates[1][0x9] = 12;
        Waitstates[1][0xA] = 10;
        Waitstates[1][0xF] = 1;
    }
    else
    {
        // ARM7
        Waitstates[0][0x0] = 1;
        Waitstates[0][0x2] = 1;
        Waitstates[0][0x3] = 1;
        Waitstates[0][0x4] = 1;
        Waitstates[0][0x6] = 1;
        Waitstates[0][0x8] = 6;
        Waitstates[0][0x9] = 6;
        Waitstates[0][0xA] = 10;

        Waitstates[1][0x0] = 1;
        Waitstates[1][0x2] = 2;
        Waitstates[1][0x3] = 1;
        Waitstates[1][0x4] = 1;
        Waitstates[1][0x6] = 2;
        Waitstates[1][0x8] = 12;
        Waitstates[1][0x9] = 12;
        Waitstates[1][0xA] = 10;
    }

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

    // special path for the display FIFO. another gross hack.
    // the display FIFO seems to be more like a circular buffer that holds 16 pixels
    // from which the display controller reads. DMA is triggered every 8 pixels to fill it
    // as it is being read from. emulating it properly would be resource intensive.
    // proper emulation would only matter if something is trying to feed the FIFO manually
    // instead of using the DMA. which is probably never happening. the only thing I know of
    // that even uses the display FIFO is the aging cart.
    if (StartMode == 0x04)
    {
        GPU::GPU2D_A->FIFODMA(CurSrcAddr);
        CurSrcAddr += 256*2;
        return;
    }

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

    if (!(Cnt & 0x04000000))
    {
        u16 (*readfn)(u32) = CPU ? NDS::ARM7Read16 : NDS::ARM9Read16;
        void (*writefn)(u32,u16) = CPU ? NDS::ARM7Write16 : NDS::ARM9Write16;

        while (IterCount > 0 && cycles > 0)
        {
            writefn(CurDstAddr, readfn(CurSrcAddr));

            s32 c = (Waitstates[0][(CurSrcAddr >> 24) & 0xF] + Waitstates[0][(CurDstAddr >> 24) & 0xF]);
            cycles -= c;
            NDS::RunTimingCriticalDevices(CPU, c);

            CurSrcAddr += SrcAddrInc<<1;
            CurDstAddr += DstAddrInc<<1;
            IterCount--;
            RemCount--;
        }
    }
    else
    {
        // optimized path for typical GXFIFO DMA
        if (IsGXFIFODMA)
        {
            while (IterCount > 0 && cycles > 0)
            {
                GPU3D::WriteToGXFIFO(*(u32*)&NDS::MainRAM[CurSrcAddr&0x3FFFFF]);

                s32 c = (Waitstates[1][0x2] + Waitstates[1][0x4]);
                cycles -= c;
                NDS::RunTimingCriticalDevices(0, c);

                CurSrcAddr += SrcAddrInc<<2;
                IterCount--;
                RemCount--;
            }
        }

        u32 (*readfn)(u32) = CPU ? NDS::ARM7Read32 : NDS::ARM9Read32;
        void (*writefn)(u32,u32) = CPU ? NDS::ARM7Write32 : NDS::ARM9Write32;

        while (IterCount > 0 && cycles > 0)
        {
            writefn(CurDstAddr, readfn(CurSrcAddr));

            s32 c = (Waitstates[1][(CurSrcAddr >> 24) & 0xF] + Waitstates[1][(CurDstAddr >> 24) & 0xF]);
            cycles -= c;
            NDS::RunTimingCriticalDevices(CPU, c);

            CurSrcAddr += SrcAddrInc<<2;
            CurDstAddr += DstAddrInc<<2;
            IterCount--;
            RemCount--;
        }
    }

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
