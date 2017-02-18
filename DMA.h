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

#ifndef DMA_H
#define DMA_H

#include "types.h"

class DMA
{
public:
    DMA(u32 cpu, u32 num);
    ~DMA();

    void Reset();

    void WriteCnt(u32 val);
    void Start();

    s32 Run(s32 cycles);

    void StartIfNeeded(u32 mode)
    {
        if ((mode == StartMode) && (Cnt & 0x80000000))
            Start();
    }

    u32 SrcAddr;
    u32 DstAddr;
    u32 Cnt;

private:
    u32 CPU, Num;

    s32 Waitstates[2][16];

    u32 StartMode;
    u32 CurSrcAddr;
    u32 CurDstAddr;
    u32 RemCount;
    u32 IterCount;
    u32 SrcAddrInc;
    u32 DstAddrInc;
    u32 CountMask;

    bool Running;
};

#endif
