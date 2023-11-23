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

#ifndef DSI_I2C_H
#define DSI_I2C_H

#include "types.h"
#include "Savestate.h"

namespace melonDS
{
class DSi_I2CHost;
class DSi;
class DSi_I2CDevice
{
public:
    DSi_I2CDevice(melonDS::DSi& dsi, DSi_I2CHost& host) : DSi(dsi), Host(&host) {}
    virtual ~DSi_I2CDevice() = default;
    virtual void Reset() = 0;
    virtual void DoSavestate(Savestate* file) = 0;

    virtual void Acquire() = 0;
    virtual u8 Read(bool last) = 0;
    virtual void Write(u8 val, bool last) = 0;

protected:
    melonDS::DSi& DSi;
    DSi_I2CHost* Host;
};

class DSi_BPTWL : public DSi_I2CDevice
{
public:

    enum
    {
        batteryLevel_Critical = 0x0,
        batteryLevel_AlmostEmpty = 0x1,
        batteryLevel_Low = 0x3,
        batteryLevel_Half = 0x7,
        batteryLevel_ThreeQuarters = 0xB,
        batteryLevel_Full = 0xF
    };

    enum
    {
        volumeKey_Up,
        volumeKey_Down,
    };

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

    DSi_BPTWL(melonDS::DSi& dsi, DSi_I2CHost& host);
    ~DSi_BPTWL() override = default;
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    [[nodiscard]] u8 GetBootFlag() const noexcept;

    [[nodiscard]] bool GetBatteryCharging() const noexcept;
    void SetBatteryCharging(bool charging);

    [[nodiscard]] u8 GetBatteryLevel() const noexcept;
    void SetBatteryLevel(u8 batteryLevel);

    // 0-31
    [[nodiscard]] u8 GetVolumeLevel() const noexcept;
    void SetVolumeLevel(u8 volume);

    // 0-4
    [[nodiscard]] u8 GetBacklightLevel() const noexcept;
    void SetBacklightLevel(u8 backlight);

    void DoHardwareReset(bool direct);
    void DoShutdown();

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

    void SetIRQ(u8 irqFlag);

    void Acquire() override;
    u8 Read(bool last) override;
    void Write(u8 val, bool last) override;

private:
    static const double PowerButtonShutdownTime;
    static const double PowerButtonForcedShutdownTime;
    static const double VolumeSwitchRepeatStart;
    static const double VolumeSwitchRepeatRate;

    static const u8 VolumeDownTable[32];
    static const u8 VolumeUpTable[32];

    double PowerButtonTime = 0.0;
    bool PowerButtonDownFlag = false;
    bool PowerButtonShutdownFlag = false;
    double VolumeSwitchTime = 0.0;
    double VolumeSwitchRepeatTime = 0.0;
    bool VolumeSwitchDownFlag = false;
    u32 VolumeSwitchKeysDown = 0;

    u8 Registers[0x100] {};
    u32 CurPos = -1;

    [[nodiscard]] bool GetIRQMode() const noexcept;

    void ResetButtonState();
    [[nodiscard]] bool CheckVolumeSwitchKeysValid() const noexcept;
};

}
#endif // DSI_I2C_H
