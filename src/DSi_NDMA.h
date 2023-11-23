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

#ifndef DSI_NDMA_H
#define DSI_NDMA_H

#include "types.h"
#include "Savestate.h"

namespace melonDS
{
class GPU;
class DSi;

class DSi_NDMA
{
public:
    DSi_NDMA(u32 cpu, u32 num, GPU& gpu, DSi& dsi);

    void Reset();

    void DoSavestate(Savestate* file);

    void WriteCnt(u32 val);
    void Start();

    void Run();
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

    u32 SrcAddr = 0;
    u32 DstAddr = 0;
    u32 TotalLength = 0; // total length, when transferring multiple blocks
    u32 BlockLength = 0; // length of one transfer
    u32 SubblockTimer = 0; // optional delay between subblocks (only in round-robin mode)
    u32 FillData = 0;
    u32 Cnt = 0;

private:
    void Run9();
    void Run7();

    melonDS::DSi& DSi;
    melonDS::GPU& GPU;
    u32 CPU = 0, Num = 0;

    u32 StartMode = 0;
    u32 CurSrcAddr = 0;
    u32 CurDstAddr = 0;
    u32 SubblockLength = 0; // length transferred per run when delay is used
    u32 RemCount = 0;
    u32 IterCount = 0;
    u32 TotalRemCount = 0;
    u32 SrcAddrInc = 0;
    u32 DstAddrInc = 0;

    u32 Running = 0;
    bool InProgress = false;

    bool Executing = false;
    bool Stall = false;

    bool IsGXFIFODMA = false;
};

}
#endif // DSI_NDMA_H
