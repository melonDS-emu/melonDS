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

#ifndef DMA_H
#define DMA_H

#include <array>
#include "types.h"

namespace melonDS
{
class NDS;
class Savestate;

class DMA
{
public:
    DMA(u32 cpu, u32 num, NDS& nds);
    ~DMA() = default;

    void Reset();

    void DoSavestate(Savestate* file);

    void WriteCnt(u32 val);
    void Start();

    u32 UnitTimings9_16(int burststart);
    u32 UnitTimings9_32(int burststart);
    u32 UnitTimings7_16(int burststart);
    u32 UnitTimings7_32(int burststart);

    void Run();
    void Run9();
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

    void ResetBurst()
    {
        if (Running > 0) Running = (CPU ? 2 : 3);
    }

    u32 SrcAddr {};
    u32 DstAddr {};
    u32 Cnt {};
    u32 CurSrcAddr {};
    u32 CurDstAddr {};
    u32 RemCount {};
    u32 IterCount {};
    s32 SrcAddrInc {};
    s32 DstAddrInc {};
    u32 Running {};
    bool InProgress {};
    u32 Num {};
    u32 StartMode {};
    bool Executing {};
    bool Stall {};

private:
    melonDS::NDS& NDS;
    u32 CPU {};
    bool DMAQueued;

    u32 CountMask {};

    bool IsGXFIFODMA {};

    u32 MRAMBurstCount {};
    std::array<u8, 256> MRAMBurstTable;
};

}
#endif
