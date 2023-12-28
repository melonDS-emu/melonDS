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
class DSi;

class DSi_NDMA
{
public:
    DSi_NDMA(u32 cpu, u32 num, melonDS::DSi& dsi);
    ~DSi_NDMA();

    void Reset();

    void DoSavestate(Savestate* file);

    void WriteCnt(u32 val);
    void Start();

    void Run();

    void Run9();
    void Run7();

    bool IsInMode(u32 mode) const
    {
        return ((mode == StartMode) && (Cnt & 0x80000000));
    }

    bool IsRunning() const { return Running!=0; }

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
    u32 TotalLength; // total length, when transferring multiple blocks
    u32 BlockLength; // length of one transfer
    u32 SubblockTimer; // optional delay between subblocks (only in round-robin mode)
    u32 FillData;
    u32 Cnt;

private:
    melonDS::DSi& DSi;
    u32 CPU, Num;

    u32 StartMode;
    u32 CurSrcAddr;
    u32 CurDstAddr;
    u32 SubblockLength; // length transferred per run when delay is used
    u32 RemCount;
    u32 IterCount;
    u32 TotalRemCount;
    u32 SrcAddrInc;
    u32 DstAddrInc;

    u32 Running;
    bool InProgress;

    bool Executing;
    bool Stall;

    bool IsGXFIFODMA;
};

}
#endif // DSI_NDMA_H
