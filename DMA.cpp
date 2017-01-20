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


// NOTES ON DMA SHIT
//
// * could use optimized code paths for common types of DMA transfers. for example, VRAM
// * needs to eventually be made more accurate anyway. DMA isn't instant.


DMA::DMA(u32 cpu, u32 num)
{
    CPU = cpu;
    Num = num;

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
    SrcAddrInc = 0;
    DstAddrInc = 0;
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
            StartMode = ((Cnt >> 28) & 0x3) | 0x8;

        if ((StartMode & 0x7) == 0)
            Start();
        else
            printf("SPECIAL ARM%d DMA%d START MODE %02X\n", CPU?7:9, Num, StartMode);
    }
}

void DMA::Start()
{
    u32 countmask;
    if (CPU == 0)
        countmask = 0x001FFFFF;
    else
        countmask = (Num==3 ? 0x0000FFFF : 0x00003FFF);

    RemCount = Cnt & countmask;
    if (!RemCount)
        RemCount = countmask+1;

    if ((Cnt & 0x00060000) == 0x00060000)
        CurDstAddr = DstAddr;

    //printf("ARM%d DMA%d %08X %08X->%08X %d bytes %dbit\n", CPU?7:9, Num, Cnt, CurSrcAddr, CurDstAddr, RemCount*((Cnt&0x04000000)?4:2), (Cnt&0x04000000)?32:16);

    // TODO: NOT MAKE THE DMA INSTANT!!
    if (!(Cnt & 0x04000000))
    {
        u16 (*readfn)(u32) = CPU ? NDS::ARM7Read16 : NDS::ARM9Read16;
        void (*writefn)(u32,u16) = CPU ? NDS::ARM7Write16 : NDS::ARM9Write16;

        while (RemCount > 0)
        {
            writefn(CurDstAddr, readfn(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<1;
            CurDstAddr += DstAddrInc<<1;
            RemCount--;
        }
    }
    else
    {
        u32 (*readfn)(u32) = CPU ? NDS::ARM7Read32 : NDS::ARM9Read32;
        void (*writefn)(u32,u32) = CPU ? NDS::ARM7Write32 : NDS::ARM9Write32;

        while (RemCount > 0)
        {
            writefn(CurDstAddr, readfn(CurSrcAddr));

            CurSrcAddr += SrcAddrInc<<2;
            CurDstAddr += DstAddrInc<<2;
            RemCount--;
        }
    }

    if (!(Cnt & 0x02000000))
        Cnt &= ~0x80000000;

    if (Cnt & 0x40000000)
        NDS::TriggerIRQ(CPU, NDS::IRQ_DMA0 + Num);
}
