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

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <string.h>

#include "Savestate.h"
#include "SPI_Firmware.h"

namespace SPI_Firmware
{

u16 CRC16(const u8* data, u32 len, u32 start);
void SetupDirectBoot(bool dsi);

u32 FixFirmwareLength(u32 originalLength);

/// @return A pointer to the installed firmware blob if one exists, otherwise \c nullptr.
/// @warning The pointer refers to memory that melonDS owns. Do not deallocate it yourself.
/// @see InstallFirmware
const Firmware* GetFirmware();

bool IsLoadedFirmwareBuiltIn();
bool InstallFirmware(Firmware&& firmware);
bool InstallFirmware(std::unique_ptr<Firmware>&& firmware);
void RemoveFirmware();
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
