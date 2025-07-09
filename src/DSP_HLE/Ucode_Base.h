/*
    Copyright 2016-2025 melonDS team

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

#ifndef UCODE_BASE_H
#define UCODE_BASE_H

#include <functional>

#include "../types.h"
#include "../Savestate.h"

namespace melonDS
{

class DSPHLE_UcodeBase
{
public:
    DSPHLE_UcodeBase();
    ~DSPHLE_UcodeBase();
    void Reset();
    void DoSavestate(Savestate* file);

    //void SetRecvDataHandler(u8 index, std::function<void()> func);
    //void SetSemaphoreHandler(std::function<void()> func);

    bool SendDataIsEmpty(u8 index);
    bool RecvDataIsEmpty(u8 index);

    u16 DMAChan0GetDstHigh();
    u16 AHBMGetDmaChannel(u16 index);
    u16 AHBMGetDirection(u16 index);
    u16 AHBMGetUnitSize(u16 index);

    u16 DataReadA32(u32 addr);
    void DataWriteA32(u32 addr, u16 val);
    u16 MMIORead(u16 addr);
    void MMIOWrite(u16 addr, u16 val);
    u16 ProgramRead(u32 addr);
    void ProgramWrite(u32 addr, u16 val);
    u16 AHBMRead16(u32 addr);
    u32 AHBMRead32(u32 addr);
    void AHBMWrite16(u32 addr, u16 val);
    void AHBMWrite32(u32 addr, u32 val);

    u16 GetSemaphore();
    void SetSemaphore(u16 val);
    void ClearSemaphore(u16 val);
    void MaskSemaphore(u16 val);

    u16 RecvData(u8 index);
    void SendData(u8 index, u16 val);

    void Run(u32 cycles);
};

}

#endif // UCODE_BASE_H
