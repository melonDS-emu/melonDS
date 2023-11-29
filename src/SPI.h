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

u16 CRC16(const u8* data, u32 len, u32 start);

class SPIHost;
class NDS;
class SPIDevice
{
public:
    SPIDevice(melonDS::NDS& nds) : NDS(nds), Hold(false), DataPos(0) {}
    virtual ~SPIDevice() {}
    virtual void Reset() = 0;
    virtual void DoSavestate(Savestate* file) = 0;

    virtual u8 Read() { return Data; }
    virtual void Write(u8 val) = 0;
    virtual void Release() { Hold = false; DataPos = 0; }

protected:
    melonDS::NDS& NDS;

    bool Hold;
    u32 DataPos;
    u8 Data;
};

class FirmwareMem : public SPIDevice
{
public:
    FirmwareMem(melonDS::NDS& nds);
    ~FirmwareMem() override;
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    void SetupDirectBoot();

    const class Firmware* GetFirmware();
    bool IsLoadedFirmwareBuiltIn();
    bool InstallFirmware(class Firmware&& firmware);
    bool InstallFirmware(std::unique_ptr<class Firmware>&& firmware);
    void RemoveFirmware();

    void Write(u8 val) override;
    void Release() override;

private:
    std::unique_ptr<class Firmware> Firmware;

    u8 CurCmd;

    u8 StatusReg;
    u32 Addr;

    bool VerifyCRC16(u32 start, u32 offset, u32 len, u32 crcoffset);
};

class PowerMan : public SPIDevice
{
public:
    PowerMan(melonDS::NDS& nds);
    ~PowerMan() override;
    void Reset() override;
    void DoSavestate(Savestate* file) override;

    bool GetBatteryLevelOkay();
    void SetBatteryLevelOkay(bool okay);

    void Write(u8 val) override;

private:
    u8 Index;

    u8 Registers[8];
    u8 RegMasks[8];
};

class TSC : public SPIDevice
{
public:
    TSC(melonDS::NDS& nds);
    virtual ~TSC() override;
    virtual void Reset() override;
    virtual void DoSavestate(Savestate* file) override;

    virtual void SetTouchCoords(u16 x, u16 y);
    virtual void MicInputFrame(s16* data, int samples);

    virtual void Write(u8 val) override;

protected:
    u8 ControlByte;

    u16 ConvResult;

    u16 TouchX, TouchY;

    s16 MicBuffer[1024];
    int MicBufferLen;
};


class SPIHost
{
public:
    SPIHost(melonDS::NDS& nds);
    ~SPIHost();
    void Reset();
    void DoSavestate(Savestate* file);

    FirmwareMem* GetFirmwareMem() { return (FirmwareMem*)Devices[SPIDevice_FirmwareMem]; }
    PowerMan* GetPowerMan() { return (PowerMan*)Devices[SPIDevice_PowerMan]; }
    TSC* GetTSC() { return (TSC*)Devices[SPIDevice_TSC]; }

    const Firmware* GetFirmware() { return GetFirmwareMem()->GetFirmware(); }

    u16 ReadCnt() { return Cnt; }
    void WriteCnt(u16 val);

    u8 ReadData();
    void WriteData(u8 val);

    void TransferDone(u32 param);

private:
    melonDS::NDS& NDS;
    u16 Cnt;

    SPIDevice* Devices[3];
};

}
#endif
