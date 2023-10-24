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

#ifndef DMA_H
#define DMA_H

#include <array>
#include "types.h"
#include "Savestate.h"
#include "DMA_Timings.h"

class DMA
{
public:
    DMA(u32 cpu, u32 num);
    ~DMA() = default;

    void Reset();

    void DoSavestate(Savestate* file);

    void WriteCnt(u32 val);
    void Start();

    u32 UnitTimings9_16(bool burststart);
    u32 UnitTimings9_32(bool burststart);
    u32 UnitTimings7_16(bool burststart);
    u32 UnitTimings7_32(bool burststart);

    template <int ConsoleType>
    void Run();

    template <int ConsoleType>
    void Run9();
    template <int ConsoleType>
    void Run7();

    bool IsInMode(u32 mode) const noexcept
    {
        return ((mode == StartMode) && (Cnt & 0x80000000));
    }

    bool IsRunning() const noexcept { return Running!=0; }

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

    u32 SrcAddr {};
    u32 DstAddr {};
    u32 Cnt {};

private:
    u32 CPU {};
    u32 Num {};

    u32 StartMode {};
    u32 CurSrcAddr {};
    u32 CurDstAddr {};
    u32 RemCount {};
    u32 IterCount {};
    s32 SrcAddrInc {};
    s32 DstAddrInc {};
    u32 CountMask {};

    u32 Running {};
    bool InProgress {};

    bool Executing {};
    bool Stall {};

    bool IsGXFIFODMA {};

    u32 MRAMBurstCount {};
    std::array<u8, 256> MRAMBurstTable;
};

#endif
