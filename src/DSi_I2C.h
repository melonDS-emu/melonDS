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

#ifndef DSI_I2C_H
#define DSI_I2C_H

#include "types.h"
#include "Savestate.h"

namespace DSi_BPTWL
{

u8 GetBootFlag();

bool GetBatteryCharging();
void SetBatteryCharging(bool charging);

enum
{
    batteryLevel_Critical = 0x0,
    batteryLevel_AlmostEmpty = 0x1,
    batteryLevel_Low = 0x3,
    batteryLevel_Half = 0x7,
    batteryLevel_ThreeQuarters = 0xB,
    batteryLevel_Full = 0xF
};

u8 GetBatteryLevel();
void SetBatteryLevel(u8 batteryLevel);
}

namespace DSi_I2C
{

extern u8 Cnt;

bool Init();
void DeInit();
void Reset();
void DoSavestate(Savestate* file);

void WriteCnt(u8 val);

u8 ReadData();
void WriteData(u8 val);

//void TransferDone(u32 param);

}

#endif // DSI_I2C_H
