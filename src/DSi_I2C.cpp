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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "DSi.h"
#include "DSi_I2C.h"
#include "DSi_Camera.h"
#include "ARM.h"
#include "SPI.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace DSi_BPTWL
{

// TODO: These are purely approximations
const double PowerButtonShutdownTime = 0.5;
const double PowerButtonForcedShutdownTime = 5.0;
const double VolumeSwitchRepeatStart = 0.5;
const double VolumeSwitchRepeatRate = 1.0 / 6;

// Could not find a pattern or a decent formula for these,
// regardless, they're only 64 bytes in size
const u8 VolumeDownTable[32] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
};

const u8 VolumeUpTable[32] =
{
    0x02, 0x03, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
    0x1D, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
};

double PowerButtonTime = 0.0;
bool PowerButtonDownFlag = false;
bool PowerButtonShutdownFlag = false;
double VolumeSwitchTime = 0.0;
double VolumeSwitchRepeatTime = 0.0;
bool VolumeSwitchDownFlag = false;
u32 VolumeSwitchKeysDown = 0;

u8 Registers[0x100];
u32 CurPos;

bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    CurPos = -1;
    memset(Registers, 0x5A, 0x100);

    Registers[0x00] = 0x33; // TODO: support others??
    Registers[0x01] = 0x00;
    Registers[0x02] = 0x50;
    Registers[0x10] = 0x00; // irq flag
    Registers[0x11] = 0x00; // reset
    Registers[0x12] = 0x00; // irq mode
    Registers[0x20] = 0x8F; // battery
    Registers[0x21] = 0x07;
    Registers[0x30] = 0x13;
    Registers[0x31] = 0x00; // camera power
    Registers[0x40] = 0x1F; // volume
    Registers[0x41] = 0x04; // backlight
    Registers[0x60] = 0x00;
    Registers[0x61] = 0x01;
    Registers[0x62] = 0x50;
    Registers[0x63] = 0x00;
    Registers[0x70] = 0x00; // boot flag
    Registers[0x71] = 0x00;
    Registers[0x72] = 0x00;
    Registers[0x73] = 0x00;
    Registers[0x74] = 0x00;
    Registers[0x75] = 0x00;
    Registers[0x76] = 0x00;
    Registers[0x77] = 0x00;
    Registers[0x80] = 0x10;
    Registers[0x81] = 0x64;

    // Ideally these should be replaced by a proper BPTWL core emulator
    PowerButtonTime = 0.0;
    PowerButtonDownFlag = false;
    PowerButtonShutdownFlag = false;
    VolumeSwitchTime = 0.0;
    VolumeSwitchRepeatTime = 0.0;
    VolumeSwitchKeysDown = 0;

}

void DoSavestate(Savestate* file)
{
    file->Section("I2BP");

    file->VarArray(Registers, 0x100);
    file->Var32(&CurPos);
}

// TODO: Needs more investigation on the other bits
inline bool GetIRQMode()
{
    return Registers[0x12] & 0x01;
}

u8 GetBootFlag() { return Registers[0x70]; }

bool GetBatteryCharging() { return Registers[0x20] >> 7; }
void SetBatteryCharging(bool charging)
{
    Registers[0x20] = (((charging ? 0x8 : 0x0) << 4) | (Registers[0x20] & 0x0F));
}

u8 GetBatteryLevel() { return Registers[0x20] & 0xF; }
void SetBatteryLevel(u8 batteryLevel)
{
    Registers[0x20] = ((Registers[0x20] & 0xF0) | (batteryLevel & 0x0F));
    SPI_Powerman::SetBatteryLevelOkay(batteryLevel > batteryLevel_Low ? true : false);

    if (batteryLevel <= 1)
    {
        SetIRQ(batteryLevel ? IRQ_BatteryLow : IRQ_BatteryEmpty);
    }

}

u8 GetVolumeLevel() { return Registers[0x40]; }
void SetVolumeLevel(u8 volume)
{
    Registers[0x40] = volume & 0x1F;
}

u8 GetBacklightLevel() { return Registers[0x41]; }
void SetBacklightLevel(u8 backlight)
{
    Registers[0x41] = backlight > 4 ? 4 : backlight;
}


void ResetButtonState()
{
    PowerButtonTime = 0.0;
    PowerButtonDownFlag = false;
    PowerButtonShutdownFlag = false;

    VolumeSwitchKeysDown = 0;
    VolumeSwitchDownFlag = false;
    VolumeSwitchTime = 0.0;
    VolumeSwitchRepeatTime = 0.0;
}

void DoHardwareReset(bool direct)
{
    ResetButtonState();

    Log(LogLevel::Debug, "BPTWL: soft-reset\n");

    if (direct)
    {
        // TODO: This doesn't seem to stop the SPU
        DSi::SoftReset();
        return;
    }

    // TODO: soft-reset might need to be scheduled later!
    // TODO: this has been moved for the JIT to work, nothing is confirmed here
    NDS::ARM7->Halt(4);
}

void DoShutdown()
{
    ResetButtonState();
    NDS::Stop(Platform::StopReason::PowerOff);
}


void SetPowerButtonHeld(double time)
{
    if (!PowerButtonDownFlag)
    {
        PowerButtonDownFlag = true;
        PowerButtonTime = time;
        DoPowerButtonPress();
        return;
    }

    double elapsed = time - PowerButtonTime;
    if (elapsed < 0)
        return;

    if (elapsed >= PowerButtonForcedShutdownTime)
    {
        Log(LogLevel::Debug, "Force power off via DSi power button\n");
        DoPowerButtonForceShutdown();
        return;
    }

    if (elapsed >= PowerButtonShutdownTime)
    {
        DoPowerButtonShutdown();
    }
}

void SetPowerButtonReleased(double time)
{
    double elapsed = time - PowerButtonTime;
    if (elapsed >= 0 && elapsed < PowerButtonShutdownTime)
    {
        DoPowerButtonReset();
    }

    PowerButtonTime = 0.0;
    PowerButtonDownFlag = false;
    PowerButtonShutdownFlag = false;
}

void SetVolumeSwitchHeld(u32 key)
{
    VolumeSwitchKeysDown |= (1 << key);
}

void SetVolumeSwitchReleased(u32 key)
{
    VolumeSwitchKeysDown &= ~(1 << key);
    VolumeSwitchDownFlag = false;
    VolumeSwitchTime = 0.0;
    VolumeSwitchRepeatTime = 0.0;
}

inline bool CheckVolumeSwitchKeysValid()
{
    bool up = VolumeSwitchKeysDown & (1 << volumeKey_Up);
    bool down = VolumeSwitchKeysDown & (1 << volumeKey_Down);

    return up != down;
}

s32 ProcessVolumeSwitchInput(double time)
{
    if (!CheckVolumeSwitchKeysValid())
        return -1;

    s32 key = VolumeSwitchKeysDown & (1 << volumeKey_Up) ? volumeKey_Up : volumeKey_Down;

    // Always fire an IRQ when first pressed
    if (!VolumeSwitchDownFlag)
    {
        VolumeSwitchDownFlag = true;
        VolumeSwitchTime = time;
        DoVolumeSwitchPress(key);
        return key;
    }

    // Handle key repetition mechanic
    if (VolumeSwitchRepeatTime == 0)
    {
        double elapsed = time - VolumeSwitchTime;
        if (elapsed < VolumeSwitchRepeatStart)
            return -1;

        VolumeSwitchRepeatTime = time;
        DoVolumeSwitchPress(key);
        return key;
    }

    double elapsed = time - VolumeSwitchRepeatTime;
    if (elapsed < VolumeSwitchRepeatRate)
        return -1;

    double rem = fmod(elapsed, VolumeSwitchRepeatRate);
    VolumeSwitchRepeatTime = time - rem;
    DoVolumeSwitchPress(key);
    return key;
}


void DoPowerButtonPress()
{
    // Set button pressed IRQ
    SetIRQ(IRQ_PowerButtonPressed);

    // There is no default hardware behavior for pressing the power button
}

void DoPowerButtonReset()
{
    // Reset via IRQ, handled by software
    SetIRQ(IRQ_PowerButtonReset);

    // Reset automatically via hardware
    if (!GetIRQMode())
    {
        // Assumes this isn't called during normal CPU execution
        DoHardwareReset(true);
    }
}

void DoPowerButtonShutdown()
{
    // Shutdown via IRQ, handled by software
    if (!PowerButtonShutdownFlag)
    {
        SetIRQ(IRQ_PowerButtonShutdown);
    }

    PowerButtonShutdownFlag = true;

    // Shutdown automatically via hardware
    if (!GetIRQMode())
    {
        DoShutdown();
    }

    // The IRQ is only fired once (hence the need for an if guard),
    // but the hardware shutdown is continuously triggered.
    // That way when switching the IRQ mode while holding
    // down the power button, the DSi will still shut down
}

void DoPowerButtonForceShutdown()
{
    DoShutdown();
}

void DoVolumeSwitchPress(u32 key)
{
    u8 volume = Registers[0x40];

    switch (key)
    {

    case volumeKey_Up:
        volume = VolumeUpTable[volume];
        break;

    case volumeKey_Down:
        volume = VolumeDownTable[volume];
        break;

    }

    Registers[0x40] = volume;

    SetIRQ(IRQ_VolumeSwitchPressed);
}

void SetIRQ(u8 irqFlag)
{
    Registers[0x10] |= irqFlag & IRQ_ValidMask;

    if (GetIRQMode())
    {
        NDS::SetIRQ2(NDS::IRQ2_DSi_BPTWL);
    }
}

void Start()
{
    //printf("BPTWL: start\n");
}

u8 Read(bool last)
{
    //printf("BPTWL: read %02X -> %02X @ %08X\n", CurPos, Registers[CurPos], NDS::GetPC(1));
    u8 ret = Registers[CurPos];

    // IRQ flags are automatically cleared upon read
    if (CurPos == 0x10)
    {
        Registers[0x10] = 0;
    }

    CurPos++;

    if (last)
    {
        CurPos = -1;
    }

    return ret;
}

void Write(u8 val, bool last)
{
    if (last)
    {
        CurPos = -1;
        return;
    }

    if (CurPos == 0xFFFFFFFF)
    {
        CurPos = val;
        //printf("BPTWL: reg=%02X\n", val);
        return;
    }

    if (CurPos == 0x11 && val == 0x01)
    {
        // Assumes this is called during normal CPU execution
        DoHardwareReset(false);
        val = 0; // checkme
        CurPos = -1;
        return;
    }

    // Mask volume level
    if (CurPos == 0x40)
    {
        val &= 0x1F;
    }

    // Clamp backlight level
    if (CurPos == 0x41)
    {
        val = val > 4 ? 4 : val;
    }

    if (CurPos == 0x11 || CurPos == 0x12 ||
        CurPos == 0x21 ||
        CurPos == 0x30 || CurPos == 0x31 ||
        CurPos == 0x40 || CurPos == 0x41 ||
        CurPos == 0x60 || CurPos == 0x63 ||
        (CurPos >= 0x70 && CurPos <= 0x77) ||
        CurPos == 0x80 || CurPos == 0x81)
    {
        Registers[CurPos] = val;
    }

    //printf("BPTWL: write %02X -> %02X\n", CurPos, val);
    CurPos++; // CHECKME
}

}


namespace DSi_I2C
{

u8 Cnt;
u8 Data;

u32 Device;

bool Init()
{
    if (!DSi_BPTWL::Init()) return false;

    return true;
}

void DeInit()
{
    DSi_BPTWL::DeInit();
}

void Reset()
{
    Cnt = 0;
    Data = 0;

    Device = -1;

    DSi_BPTWL::Reset();
}

void DoSavestate(Savestate* file)
{
    file->Section("I2Ci");

    file->Var8(&Cnt);
    file->Var8(&Data);
    file->Var32(&Device);

    DSi_BPTWL::DoSavestate(file);
}

void WriteCnt(u8 val)
{
    //printf("I2C: write CNT %02X, %02X, %08X\n", val, Data, NDS::GetPC(1));

    // TODO: check ACK flag
    // TODO: transfer delay
    // TODO: IRQ
    // TODO: check read/write direction

    if (val & (1<<7))
    {
        bool islast = val & (1<<0);

        if (val & (1<<5))
        {
            // read
            val &= 0xF7;

            switch (Device)
            {
            case 0x4A: Data = DSi_BPTWL::Read(islast); break;
            case 0x78: Data = DSi_CamModule::Camera0->I2C_Read(islast); break;
            case 0x7A: Data = DSi_CamModule::Camera1->I2C_Read(islast); break;
            case 0xA0:
            case 0xE0: Data = 0xFF; break;
            default:
                Log(LogLevel::Warn, "I2C: read on unknown device %02X, cnt=%02X, data=%02X, last=%d\n", Device, val, 0, islast);
                Data = 0xFF;
                break;
            }

            //printf("I2C read, device=%02X, cnt=%02X, data=%02X, last=%d\n", Device, val, Data, islast);
        }
        else
        {
            // write
            val &= 0xE7;
            bool ack = true;

            if (val & (1<<1))
            {
                Device = Data & 0xFE;
                //printf("I2C: %s start, device=%02X\n", (Data&0x01)?"read":"write", Device);

                switch (Device)
                {
                case 0x4A: DSi_BPTWL::Start(); break;
                case 0x78: DSi_CamModule::Camera0->I2C_Start(); break;
                case 0x7A: DSi_CamModule::Camera1->I2C_Start(); break;
                case 0xA0:
                case 0xE0: ack = false; break;
                default:
                    Log(LogLevel::Warn, "I2C: %s start on unknown device %02X\n", (Data&0x01)?"read":"write", Device);
                    ack = false;
                    break;
                }
            }
            else
            {
                //printf("I2C write, device=%02X, cnt=%02X, data=%02X, last=%d\n", Device, val, Data, islast);

                switch (Device)
                {
                case 0x4A: DSi_BPTWL::Write(Data, islast); break;
                case 0x78: DSi_CamModule::Camera0->I2C_Write(Data, islast); break;
                case 0x7A: DSi_CamModule::Camera1->I2C_Write(Data, islast); break;
                case 0xA0:
                case 0xE0: ack = false; break;
                default:
                    Log(LogLevel::Warn, "I2C: write on unknown device %02X, cnt=%02X, data=%02X, last=%d\n", Device, val, Data, islast);
                    ack = false;
                    break;
                }
            }

            if (ack) val |= (1<<4);
        }

        val &= 0x7F;
    }

    Cnt = val;
}

u8 ReadData()
{
    return Data;
}

void WriteData(u8 val)
{
    Data = val;
}

}
