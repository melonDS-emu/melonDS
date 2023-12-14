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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "DSi.h"
#include "DSi_I2C.h"
#include "DSi_Camera.h"
#include "ARM.h"
#include "SPI.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


// TODO: These are purely approximations
const double DSi_BPTWL::PowerButtonShutdownTime = 0.5;
const double DSi_BPTWL::PowerButtonForcedShutdownTime = 5.0;
const double DSi_BPTWL::VolumeSwitchRepeatStart = 0.5;
const double DSi_BPTWL::VolumeSwitchRepeatRate = 1.0 / 6;

// Could not find a pattern or a decent formula for these,
// regardless, they're only 64 bytes in size
const u8 DSi_BPTWL::VolumeDownTable[32] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
};

const u8 DSi_BPTWL::VolumeUpTable[32] =
{
    0x02, 0x03, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
    0x1D, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
};


DSi_BPTWL::DSi_BPTWL(melonDS::DSi& dsi, DSi_I2CHost* host) : DSi_I2CDevice(dsi, host)
{
}

DSi_BPTWL::~DSi_BPTWL()
{
}

void DSi_BPTWL::Reset()
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
    VolumeSwitchDownFlag = false;

}

void DSi_BPTWL::DoSavestate(Savestate* file)
{
    file->Section("I2BP");

    file->VarArray(Registers, 0x100);
    file->Var32(&CurPos);
}

// TODO: Needs more investigation on the other bits
inline bool DSi_BPTWL::GetIRQMode() const
{
    return Registers[0x12] & 0x01;
}

u8 DSi_BPTWL::GetBootFlag() const { return Registers[0x70]; }

bool DSi_BPTWL::GetBatteryCharging() const { return Registers[0x20] >> 7; }
void DSi_BPTWL::SetBatteryCharging(bool charging)
{
    Registers[0x20] = (((charging ? 0x8 : 0x0) << 4) | (Registers[0x20] & 0x0F));
}

u8 DSi_BPTWL::GetBatteryLevel() const { return Registers[0x20] & 0xF; }
void DSi_BPTWL::SetBatteryLevel(u8 batteryLevel)
{
    Registers[0x20] = ((Registers[0x20] & 0xF0) | (batteryLevel & 0x0F));
    //SPI_Powerman::SetBatteryLevelOkay(batteryLevel > batteryLevel_Low ? true : false);

    if (batteryLevel <= 1)
    {
        SetIRQ(batteryLevel ? IRQ_BatteryLow : IRQ_BatteryEmpty);
    }

}

u8 DSi_BPTWL::GetVolumeLevel() const { return Registers[0x40]; }
void DSi_BPTWL::SetVolumeLevel(u8 volume)
{
    Registers[0x40] = volume & 0x1F;
}

u8 DSi_BPTWL::GetBacklightLevel() const { return Registers[0x41]; }
void DSi_BPTWL::SetBacklightLevel(u8 backlight)
{
    Registers[0x41] = backlight > 4 ? 4 : backlight;
}


void DSi_BPTWL::ResetButtonState()
{
    PowerButtonTime = 0.0;
    PowerButtonDownFlag = false;
    PowerButtonShutdownFlag = false;

    VolumeSwitchKeysDown = 0;
    VolumeSwitchDownFlag = false;
    VolumeSwitchTime = 0.0;
    VolumeSwitchRepeatTime = 0.0;
}

void DSi_BPTWL::DoHardwareReset(bool direct)
{
    ResetButtonState();

    Log(LogLevel::Debug, "BPTWL: soft-reset\n");

    if (direct)
    {
        // TODO: This doesn't seem to stop the SPU
        DSi.SoftReset();
        return;
    }

    // TODO: soft-reset might need to be scheduled later!
    // TODO: this has been moved for the JIT to work, nothing is confirmed here
    DSi.ARM7.Halt(4);
}

void DSi_BPTWL::DoShutdown()
{
    ResetButtonState();
    DSi.Stop(Platform::StopReason::PowerOff);
}


void DSi_BPTWL::SetPowerButtonHeld(double time)
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

void DSi_BPTWL::SetPowerButtonReleased(double time)
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

void DSi_BPTWL::SetVolumeSwitchHeld(u32 key)
{
    VolumeSwitchKeysDown |= (1 << key);
}

void DSi_BPTWL::SetVolumeSwitchReleased(u32 key)
{
    VolumeSwitchKeysDown &= ~(1 << key);
    VolumeSwitchDownFlag = false;
    VolumeSwitchTime = 0.0;
    VolumeSwitchRepeatTime = 0.0;
}

inline bool DSi_BPTWL::CheckVolumeSwitchKeysValid() const
{
    bool up = VolumeSwitchKeysDown & (1 << volumeKey_Up);
    bool down = VolumeSwitchKeysDown & (1 << volumeKey_Down);

    return up != down;
}

s32 DSi_BPTWL::ProcessVolumeSwitchInput(double time)
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


void DSi_BPTWL::DoPowerButtonPress()
{
    // Set button pressed IRQ
    SetIRQ(IRQ_PowerButtonPressed);

    // There is no default hardware behavior for pressing the power button
}

void DSi_BPTWL::DoPowerButtonReset()
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

void DSi_BPTWL::DoPowerButtonShutdown()
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

void DSi_BPTWL::DoPowerButtonForceShutdown()
{
    DoShutdown();
}

void DSi_BPTWL::DoVolumeSwitchPress(u32 key)
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

void DSi_BPTWL::SetIRQ(u8 irqFlag)
{
    Registers[0x10] |= irqFlag & IRQ_ValidMask;

    if (GetIRQMode())
    {
        DSi.SetIRQ2(IRQ2_DSi_BPTWL);
    }
}

void DSi_BPTWL::Acquire()
{
    //printf("BPTWL: start\n");
}

u8 DSi_BPTWL::Read(bool last)
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

void DSi_BPTWL::Write(u8 val, bool last)
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


DSi_I2CHost::DSi_I2CHost(melonDS::DSi& dsi) : DSi(dsi)
{
    BPTWL = new DSi_BPTWL(dsi, this);
    Camera0 = new DSi_Camera(dsi, this, 0);
    Camera1 = new DSi_Camera(dsi, this, 1);
}

DSi_I2CHost::~DSi_I2CHost()
{
    delete BPTWL; BPTWL = nullptr;
    delete Camera0; Camera0 = nullptr;
    delete Camera1; Camera1 = nullptr;
}

void DSi_I2CHost::Reset()
{
    Cnt = 0;
    Data = 0;

    CurDeviceID = 0;
    CurDevice = nullptr;

    BPTWL->Reset();
    Camera0->Reset();
    Camera1->Reset();
}

void DSi_I2CHost::DoSavestate(Savestate* file)
{
    file->Section("I2Ci");

    file->Var8(&Cnt);
    file->Var8(&Data);
    file->Var8(&CurDeviceID);

    if (!file->Saving)
    {
        GetCurDevice();
    }

    BPTWL->DoSavestate(file);
    Camera0->DoSavestate(file);
    Camera1->DoSavestate(file);
}

void DSi_I2CHost::GetCurDevice()
{
    switch (CurDeviceID)
    {
    case 0x4A: CurDevice = BPTWL; break;
    case 0x78: CurDevice = Camera0; break;
    case 0x7A: CurDevice = Camera1; break;
    case 0xA0:
    case 0xE0: CurDevice = nullptr; break;
    default:
        Log(LogLevel::Warn, "I2C: unknown device %02X\n", CurDeviceID);
        CurDevice = nullptr;
        break;
    }
}

void DSi_I2CHost::WriteCnt(u8 val)
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

            if (CurDevice)
            {
                Data = CurDevice->Read(islast);
            }
            else
            {
                Data = 0xFF;
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
                CurDeviceID = Data & 0xFE;
                //printf("I2C: %s start, device=%02X\n", (Data&0x01)?"read":"write", Device);

                GetCurDevice();
                if (CurDevice)
                {
                    CurDevice->Acquire();
                }
                else
                {
                    ack = false;
                }
            }
            else
            {
                //printf("I2C write, device=%02X, cnt=%02X, data=%02X, last=%d\n", Device, val, Data, islast);

                if (CurDevice)
                {
                    CurDevice->Write(Data, islast);
                }
                else
                {
                    ack = false;
                }
            }

            if (ack) val |= (1<<4);
        }

        val &= 0x7F;
    }

    Cnt = val;
}

u8 DSi_I2CHost::ReadData()
{
    return Data;
}

void DSi_I2CHost::WriteData(u8 val)
{
    Data = val;
}

}