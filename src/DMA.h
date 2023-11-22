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

#ifndef DMA_H
#define DMA_H

#include <array>
#include "types.h"
#include "Savestate.h"
#include "DMA_Timings.h"

namespace melonDS
{
class GPU;

class DMA
{
public:
    DMA(u32 cpu, u32 num, NDS& nds);
    ~DMA() = default;

    void Reset();

    void DoSavestate(Savestate* file);

    void WriteCnt(u32 val);
    void Start();

    void Run();

    [[nodiscard]] bool IsInMode(u32 mode) const noexcept
    {
        return ((mode == StartMode) && (Cnt & 0x80000000));
    }

    [[nodiscard]] bool IsRunning() const noexcept { return Running!=0; }

    [[nodiscard]] u32 GetCnt() const noexcept { return Cnt; }
    [[nodiscard]] u32 GetSrcAddr() const noexcept { return SrcAddr; }
    void SetSrcAddr(u32 srcaddr) noexcept { SrcAddr = srcaddr; }
    [[nodiscard]] u32 GetDstAddr() const noexcept { return DstAddr; }
    void SetDstAddr(u32 dstaddr) noexcept { DstAddr = dstaddr; }

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

private:
    void Run9();
    void Run7();
    u32 UnitTimings9_16(bool burststart);
    u32 UnitTimings9_32(bool burststart);
    u32 UnitTimings7_16(bool burststart);
    u32 UnitTimings7_32(bool burststart);

    melonDS::NDS& NDS;
    u32 CPU;
    u32 Num;

    u32 SrcAddr = 0;
    u32 DstAddr = 0;
    u32 Cnt = 0;
    u32 StartMode = 0;
    u32 CurSrcAddr = 0;
    u32 CurDstAddr = 0;
    u32 RemCount = 0;
    u32 IterCount = 0;
    s32 SrcAddrInc = 0;
    s32 DstAddrInc = 0;
    u32 CountMask = 0;

    u32 Running = 0;
    bool InProgress = false;

    bool Executing = false;
    bool Stall = false;

    bool IsGXFIFODMA = false;

    u32 MRAMBurstCount = 0;
    std::array<u8, 256> MRAMBurstTable = DMATiming::MRAMDummy;
};

}
#endif
