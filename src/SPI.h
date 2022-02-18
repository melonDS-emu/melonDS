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

#ifndef SPI_H
#define SPI_H

#include "Savestate.h"

namespace SPI_Firmware
{

void SetupDirectBoot(bool dsi);

u32 FixFirmwareLength(u32 originalLength);

u32 GetFirmwareLength();
u8 GetConsoleType();
u8 GetWifiVersion();
u8 GetNWifiVersion();
u8 GetRFVersion();
u8* GetWifiMAC();

}

namespace SPI_Powerman
{

bool GetBatteryLevelOkay();
void SetBatteryLevelOkay(bool okay);

}

namespace SPI_TSC
{

void SetTouchCoords(u16 x, u16 y);
void MicInputFrame(s16* data, int samples);

u8 Read();
void Write(u8 val, u32 hold);

}

namespace SPI
{

extern u16 Cnt;

bool Init();
void DeInit();
void Reset();
void DoSavestate(Savestate* file);

void WriteCnt(u16 val);

u8 ReadData();
void WriteData(u8 val);

void TransferDone(u32 param);

}

#endif
