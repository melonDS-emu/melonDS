/*
    Copyright 2016-2024 melonDS team

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

#include <assert.h>
#include "NDS.h"
#include "GBACart.h"
#include "Platform.h"
#include <algorithm>
#include "math.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace GBACart
{

CartMotionPakHomebrew::CartMotionPakHomebrew(void* userdata) : 
    CartCommon(MotionPakHomebrew),
    UserData(userdata)
{
}

CartMotionPakHomebrew::~CartMotionPakHomebrew() = default;

void CartMotionPakHomebrew::Reset()
{
    ShiftVal = 0;
}

void CartMotionPakHomebrew::DoSavestate(Savestate* file)
{
    CartCommon::DoSavestate(file);
    file->Var16(&ShiftVal);
}

u16 CartMotionPakHomebrew::ROMRead(u32 addr) const
{
    // CHECKME: Does this apply to the homebrew cart as well?
    return 0xFCFF;
}

static int AccelerationToMotionPak(float accel)
{
    const float GRAVITY_M_S2 = 9.80665f;

    return std::clamp(
        (int) ((accel / (5 * GRAVITY_M_S2) + 0.5) * 4096),
        0, 4095
    );
}

static int AccelerationToMotionPakRetail(float accel)
{
    const float GRAVITY_M_S2 = 9.80665f;

    return std::clamp(
        (int) ((accel / (5 * GRAVITY_M_S2) + 0.5) * 256),
        0, 254
    );
}

static int RotationToMotionPak(float rot)
{
    const float DEGREES_PER_RAD = 180 / M_PI;
    const float COUNTS_PER_DEG_PER_SEC = 0.825;
    const int CENTER = 1680;

    return std::clamp(
        (int) ((rot * DEGREES_PER_RAD * COUNTS_PER_DEG_PER_SEC) + CENTER + 0.5),
        0, 4095
    );
}

u8 CartMotionPakHomebrew::SRAMRead(u32 addr)
{
    // CHECKME: SRAM address mask
    addr &= 0xFFFF;

    switch (addr)
    {
    case 0:
        // Read next byte
        break;
    case 2:
        // Read X acceleration
        ShiftVal = AccelerationToMotionPak(Platform::Addon_MotionQuery(Platform::MotionAccelerationX, UserData)) << 4;
        // CHECKME: First byte returned when reading acceleration/rotation
        return 0;
    case 4:
        // Read Y acceleration
        ShiftVal = AccelerationToMotionPak(Platform::Addon_MotionQuery(Platform::MotionAccelerationY, UserData)) << 4;
        return 0;
    case 6:
        // Read Z acceleration
        ShiftVal = AccelerationToMotionPak(Platform::Addon_MotionQuery(Platform::MotionAccelerationZ, UserData)) << 4;
        return 0;
    case 8:
        // Read Z rotation
        // CHECKME: This is a guess, compare with real hardware
        ShiftVal = RotationToMotionPak(-Platform::Addon_MotionQuery(Platform::MotionRotationZ, UserData)) << 4;
        return 0;
    case 10:
        // Identify cart
        ShiftVal = 0xF00F;
        return 0;
    case 12:
    case 14:
    case 16:
    case 18:
        // Read/enable analog inputs
        //
        // These are not connected by defualt and require do-it-yourself cart
        // modification, so there is no reason to emulate them.
        ShiftVal = 0;
        break;
    }

    // Read high byte from the emulated shift register
    u8 val = ShiftVal >> 8;
    ShiftVal <<= 8;
    return val;
}

CartMotionPakRetail::CartMotionPakRetail(void* userdata) : 
    CartCommon(MotionPakRetail),
    UserData(userdata)
{
}

CartMotionPakRetail::~CartMotionPakRetail() = default;

void CartMotionPakRetail::Reset()
{
    Value = 0;
    Step = 16;
}

void CartMotionPakRetail::DoSavestate(Savestate* file)
{
    CartCommon::DoSavestate(file);
    file->Var8(&Value);
    file->Var8(&Step);
}

u16 CartMotionPakRetail::ROMRead(u32 addr) const
{
    // CHECKME: Retail cartridge seems to return 0x7C7C.
    return 0xFCFF;
}

u8 CartMotionPakRetail::SRAMRead(u32 addr)
{
    switch (Step)
    {
    case 0: // Synchronization - read 0xFF
        Value = 0xFF;
        break;
    case 4: // X acceleration
        Value = AccelerationToMotionPakRetail(Platform::Addon_MotionQuery(Platform::MotionAccelerationX, UserData));
        break;
    case 8: // Y acceleration
        Value = AccelerationToMotionPakRetail(Platform::Addon_MotionQuery(Platform::MotionAccelerationY, UserData));
        break;
    case 12: // Z acceleration
        Value = AccelerationToMotionPakRetail(Platform::Addon_MotionQuery(Platform::MotionAccelerationZ, UserData));
        break;
    case 16: // Synchronization - read 0b00
        Step = 0;
        return 0;
    }

    int shift = 6 - ((Step & 3) * 2);
    Step++;
    return (Value >> shift) & 0x03;
}

}

}
