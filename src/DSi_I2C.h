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

// 0-31
u8 GetVolumeLevel();
void SetVolumeLevel(u8 volume);

// 0-4
u8 GetBacklightLevel();
void SetBacklightLevel(u8 backlight);

void DoHardwareReset(bool direct);
void DoShutdown();

enum
{
    volumeKey_Up,
    volumeKey_Down,
};

// Used by hotkeys
void SetPowerButtonHeld(double time);
void SetPowerButtonReleased(double time);
void SetVolumeSwitchHeld(u32 key);
void SetVolumeSwitchReleased(u32 key);
s32 ProcessVolumeSwitchInput(double time);

void DoPowerButtonPress();
void DoPowerButtonReset();
void DoPowerButtonShutdown();
void DoPowerButtonForceShutdown();
void DoVolumeSwitchPress(u32 key);

enum
{
    IRQ_PowerButtonReset    = 0x01, // Triggered after releasing the power button quickly
    IRQ_PowerButtonShutdown = 0x02, // Triggered after holding the power button for less than a second
    IRQ_PowerButtonPressed  = 0x08, // Triggered after pressing the power button
    IRQ_BatteryEmpty        = 0x10, //
    IRQ_BatteryLow          = 0x20, // Triggered when the battery level reaches 1
    IRQ_VolumeSwitchPressed = 0x40, // Triggered once when the volume sliders are first pressed and repeatedly when held down
    /*
        Bit 2 (0x04) could be set when holding the power button for more than 5 seconds? (forced power off)
        It is unknown whether it is set as the console powers off immediately.
        Bit 7 (0x80) is unused?
        Both bits are never used by the official ARM7 libraries, but could have some undocumented hardware functionality (?).
    */
    IRQ_ValidMask           = 0x7B,
};

void SetIRQ(u8 irqFlag);

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
