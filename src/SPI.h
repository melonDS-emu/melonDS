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

#ifndef SPI_H
#define SPI_H

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <string.h>

#include "Savestate.h"
#include "SPI_Firmware.h"

namespace melonDS
{
enum
{
    SPIDevice_PowerMan = 0,
    SPIDevice_FirmwareMem,
    SPIDevice_TSC,

    SPIDevice_MAX
};

class SPIHost;
class NDS;
struct InitArgs;

class SPIDevice
{
public:
    explicit SPIDevice(melonDS::NDS& nds) : NDS(nds) {}
    virtual ~SPIDevice() = default;
    virtual void Reset() = 0;
    virtual void DoSavestate(Savestate* file) = 0;

    [[nodiscard]] u8 Read() const { return Data; }
    virtual void Write(u8 val) = 0;
    virtual void Release() { Hold = false; DataPos = 0; }

protected:
    melonDS::NDS& NDS;

    bool Hold = false;
    u32 DataPos = 0;
    u8 Data = 0;
};

class FirmwareMem final : public SPIDevice
{
public:
    FirmwareMem(Firmware&& firmware, melonDS::NDS& nds);
    ~FirmwareMem() override;
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    void SetupDirectBoot();

    [[nodiscard]] Firmware& GetFirmware() noexcept { return Firmware; }
    [[nodiscard]] const Firmware& GetFirmware() const noexcept { return Firmware; }
    void SetFirmware(melonDS::Firmware&& firmware) noexcept
    {
        Firmware = std::move(firmware);
    }

    [[nodiscard]] bool IsLoadedFirmwareBuiltIn() const noexcept
    {
        return Firmware.GetHeader().Identifier == GENERATED_FIRMWARE_IDENTIFIER;
    }

    void Write(u8 val) override;
    void Release() override;

private:
    melonDS::Firmware Firmware;
    u8 CurCmd = 0;
    u8 StatusReg = 0;
    u32 Addr = 0;

    bool VerifyCRC16(u32 start, u32 offset, u32 len, u32 crcoffset) const;
};

class PowerMan final : public SPIDevice
{
public:
    explicit PowerMan(melonDS::NDS& nds);
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    [[nodiscard]] bool GetBatteryLevelOkay() const;
    void SetBatteryLevelOkay(bool okay);

    void Write(u8 val) override;

private:
    u8 Index = 0;
    std::array<u8, 8> Registers { 0, 0, 0, 0, 0x40, 0, 0, 0 };
    std::array<u8, 8> RegMasks { 0x7F, 0x00, 0x01, 0x03, 0x0F, 0, 0, 0 };
};

class TSC : public SPIDevice
{
public:
    explicit TSC(melonDS::NDS& nds);
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    virtual void SetTouchCoords(u16 x, u16 y) noexcept;
    virtual void MicInputFrame(const s16* data, int samples) noexcept;

    void Write(u8 val) override;

protected:
    u8 ControlByte = 0;
    u16 ConvResult = 0;

    u16 TouchX = 0, TouchY = 0;

    std::array<s16, 1024> MicBuffer {};
    int MicBufferLen = 0;
};


class SPIHost
{
public:
    SPIHost(Firmware&& firmware, melonDS::NDS& nds);
    ~SPIHost();
    void Reset();
    void DoSavestate(Savestate* file);

    [[nodiscard]] FirmwareMem& GetFirmwareMem() { return FirmwareMem; }
    [[nodiscard]] const FirmwareMem& GetFirmwareMem() const { return FirmwareMem; }
    [[nodiscard]] PowerMan& GetPowerMan() { return PowerMan; }
    [[nodiscard]] const PowerMan& GetPowerMan() const { return PowerMan; }
    [[nodiscard]] TSC& GetTSC() { return *TSC; }
    [[nodiscard]] const TSC& GetTSC() const { return *TSC; }
    [[nodiscard]] const Firmware& GetFirmware() const { return FirmwareMem.GetFirmware(); }
    [[nodiscard]] Firmware& GetFirmware() { return FirmwareMem.GetFirmware(); }
    void SetFirmware(Firmware&& firmware) noexcept
    {
        FirmwareMem.SetFirmware(std::move(firmware));
    }

    [[nodiscard]] u16 ReadCnt() const noexcept { return Cnt; }
    void WriteCnt(u16 val);

    [[nodiscard]] u8 ReadData() const;
    void WriteData(u8 val);

private:
    void TransferDone(u32 param);
    melonDS::NDS& NDS;
    u16 Cnt = 0;

    melonDS::FirmwareMem FirmwareMem;
    melonDS::PowerMan PowerMan;
    const std::unique_ptr<melonDS::TSC> TSC;
};

}
#endif
