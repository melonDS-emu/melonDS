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

#ifndef DMA_H
#define DMA_H

#include "types.h"

class DMA
{
public:
    DMA(u32 cpu, u32 num);
    ~DMA();

    void Reset();

    void DoSavestate(Savestate* file);

    void WriteCnt(u32 val);
    void Start();

    template <int ConsoleType>
    void Run();

    template <int ConsoleType>
    void Run9();
    template <int ConsoleType>
    void Run7();

    bool IsInMode(u32 mode)
    {
        return ((mode == StartMode) && (Cnt & 0x80000000));
    }

    bool IsRunning() { return Running!=0; }

    void StartIfNeeded(u32 mode)
    {
        if ((mode == StartMode) && (Cnt & 0x80000000))
            Start();
    }

    void StopIfNeeded(u32 mode)
    {
        if (mode == StartMode)
            Cnt &= ~0x80000000;
    }

    void StallIfRunning()
    {
        if (Executing) Stall = true;
    }

    u32 SrcAddr;
    u32 DstAddr;
    u32 Cnt;

private:
    u32 CPU, Num;

    u32 StartMode;
    u32 CurSrcAddr;
    u32 CurDstAddr;
    u32 RemCount;
    u32 IterCount;
    u32 SrcAddrInc;
    u32 DstAddrInc;
    u32 CountMask;

    u32 Running;
    bool InProgress;

    bool Executing;
    bool Stall;

    bool IsGXFIFODMA;
};

#endif
