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
#include <stdlib.h>
#include <memory>
#include <utility>
#include "NDS.h"
#include "DSi.h"
#include "SPI.h"
#include "DSi_SPI_TSC.h"
#include "Platform.h"

namespace melonDS
{
using namespace Platform;


u16 CRC16(const u8* data, u32 len, u32 start)
{
    constexpr u16 blarg[8] = {0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001};

    for (u32 i = 0; i < len; i++)
    {
        start ^= data[i];

        for (int j = 0; j < 8; j++)
        {
            if (start & 0x1)
            {
                start >>= 1;
                start ^= (blarg[j] << (7-j));
            }
            else
                start >>= 1;
        }
    }

    return start & 0xFFFF;
}



bool FirmwareMem::VerifyCRC16(u32 start, u32 offset, u32 len, u32 crcoffset) const
{
    u16 crc_stored =  *(u16*)&FirmwareData.Buffer()[crcoffset];
    u16 crc_calced = CRC16(&FirmwareData.Buffer()[offset], len, start);
    return (crc_stored == crc_calced);
}


FirmwareMem::FirmwareMem(melonDS::NDS& nds, melonDS::Firmware&& firmware) : SPIDevice(nds), FirmwareData(std::move(firmware))
{
}

FirmwareMem::~FirmwareMem() = default;

void FirmwareMem::Reset()
{
    // fix touchscreen coords
    for (auto& u : FirmwareData.GetUserData())
    {
        u.TouchCalibrationADC1[0] = 0;
        u.TouchCalibrationADC1[1] = 0;
        u.TouchCalibrationPixel1[0] = 0;
        u.TouchCalibrationPixel1[1] = 0;
        u.TouchCalibrationADC2[0] = 255<<4;
        u.TouchCalibrationADC2[1] = 191<<4;
        u.TouchCalibrationPixel2[0] = 255;
        u.TouchCalibrationPixel2[1] = 191;
    }

    FirmwareData.UpdateChecksums();

    // disable autoboot
    //Firmware[userdata+0x64] &= 0xBF;

    MacAddress mac = FirmwareData.GetHeader().MacAddr;
    Log(LogLevel::Info, "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // verify shit
    u32 mask = FirmwareData.Mask();
    Log(LogLevel::Debug, "FW: WIFI CRC16 = %s\n", VerifyCRC16(0x0000, 0x2C, *(u16*)&FirmwareData.Buffer()[0x2C], 0x2A)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: AP1 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FA00&mask, 0xFE, 0x7FAFE&mask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: AP2 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FB00&mask, 0xFE, 0x7FBFE&mask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: AP3 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FC00&mask, 0xFE, 0x7FCFE&mask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: USER0 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x7FE00&mask, 0x70, 0x7FE72&mask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: USER1 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x7FF00&mask, 0x70, 0x7FF72&mask)?"GOOD":"BAD");

    Hold = 0;
    CurCmd = 0;
    Data = 0;
    StatusReg = 0x00;
}

void FirmwareMem::DoSavestate(Savestate* file)
{
    file->Section("SPFW");

    // CHECKME/TODO: trust the firmware to stay the same?????
    // embedding the whole firmware in the savestate would be derpo tho??

    file->Bool32(&Hold);
    file->Var8(&CurCmd);
    file->Var32(&DataPos);
    file->Var8(&Data);

    file->Var8(&StatusReg);
    file->Var32(&Addr);
}

void FirmwareMem::SetupDirectBoot()
{
    const auto& header = FirmwareData.GetHeader();
    const auto& userdata = FirmwareData.GetEffectiveUserData();
    if (NDS.ConsoleType == 1)
    {
        // The ARMWrite methods are virtual, they'll delegate to DSi if necessary
        for (u32 i = 0; i < 6; i += 2)
            NDS.ARM9Write16(0x02FFFCF4, *(u16*)&header.MacAddr[i]); // MAC address

        // checkme
        NDS.ARM9Write16(0x02FFFCFA, header.EnabledChannels); // enabled channels

        for (u32 i = 0; i < 0x70; i += 4)
            NDS.ARM9Write32(0x02FFFC80+i, *(u32*)&userdata.Bytes[i]);
    }
    else
    {
        NDS.ARM9Write32(0x027FF864, 0);
        NDS.ARM9Write32(0x027FF868, header.UserSettingsOffset << 3); // user settings offset

        NDS.ARM9Write16(0x027FF874, header.DataGfxChecksum); // CRC16 for data/gfx
        NDS.ARM9Write16(0x027FF876, header.GUIWifiCodeChecksum); // CRC16 for GUI/wifi code

        for (u32 i = 0; i < 0x70; i += 4)
            NDS.ARM9Write32(0x027FFC80+i, *(u32*)&userdata.Bytes[i]);
    }
}

bool FirmwareMem::IsLoadedFirmwareBuiltIn() const
{
    return FirmwareData.GetHeader().Identifier == GENERATED_FIRMWARE_IDENTIFIER;
}

void FirmwareMem::Write(u8 val)
{
    if (!Hold)
    {
        CurCmd = val;
        Hold = true;
        Data = 0;
        DataPos = 1;
        Addr = 0;

        // handle commands with no parameters
        switch (CurCmd)
        {
        case 0x04: // write disable
            StatusReg &= ~(1<<1);
            Data = 0;
            break;

        case 0x06: // write enable
            StatusReg |= (1<<1);
            Data = 0;
            break;
        }

        return;
    }

    switch (CurCmd)
    {
    case 0x03: // read
        {
            if (DataPos < 4)
            {
                Addr <<= 8;
                Addr |= val;
                Data = 0;
            }
            else
            {
                Data = FirmwareData.Buffer()[Addr & FirmwareData.Mask()];
                Addr++;
            }

            DataPos++;
        }
        break;

    case 0x05: // read status reg
        Data = StatusReg;
        break;

    case 0x0A: // write
        {
            // TODO: what happens if you write too many bytes? (max 256, they say)
            if (DataPos < 4)
            { // If we're in the middle of writing the address...
                Addr <<= 8;
                Addr |= val;
                Data = 0;
            }
            else
            {
                FirmwareData.Buffer()[Addr & FirmwareData.Mask()] = val;
                Data = val;
                Addr++;
            }

            DataPos++;
        }
        break;

    case 0x9F: // read JEDEC ID
        {
            switch (DataPos)
            {
            case 1: Data = 0x20; break;
            case 2: Data = 0x40; break;
            case 3: Data = 0x12; break;
            default: Data = 0; break;
            }
            DataPos++;
        }
        break;

    default:
        Log(LogLevel::Warn, "unknown firmware SPI command %02X\n", CurCmd);
        Data = 0xFF;
        break;
    }
}

void FirmwareMem::Release()
{
    if (CurCmd == 0x02 || CurCmd == 0x0A)
    { // If the SPI firmware chip just finished a write...
        // We only notify the frontend of changes to the Wi-fi/userdata settings region
        // (although it might still decide to flush the whole thing)
        u32 wifioffset = FirmwareData.GetWifiAccessPointOffset();

        // Request that the start of the Wi-fi/userdata settings region
        // through the end of the firmware blob be flushed to disk
        Platform::WriteFirmware(FirmwareData, wifioffset, FirmwareData.Length() - wifioffset);
    }

    SPIDevice::Release();
    CurCmd = 0;
}



PowerMan::PowerMan(melonDS::NDS& nds) : SPIDevice(nds)
{
}

PowerMan::~PowerMan()
{
}

void PowerMan::Reset()
{
    Hold = false;
    Index = 0;
    Data = 0;

    memset(Registers, 0, sizeof(Registers));
    memset(RegMasks, 0, sizeof(RegMasks));

    Registers[4] = 0x40;

    RegMasks[0] = 0x7F;
    RegMasks[1] = 0x00;
    RegMasks[2] = 0x01;
    RegMasks[3] = 0x03;
    RegMasks[4] = 0x0F;
}

void PowerMan::DoSavestate(Savestate* file)
{
    file->Section("SPPW");

    file->Bool32(&Hold);
    file->Var32(&DataPos);
    file->Var8(&Index);
    file->Var8(&Data);

    file->VarArray(Registers, 8);
    file->VarArray(RegMasks, 8); // is that needed??
}

bool PowerMan::GetBatteryLevelOkay() const { return !Registers[1]; }
void PowerMan::SetBatteryLevelOkay(bool okay) { Registers[1] = okay ? 0x00 : 0x01; }

void PowerMan::Write(u8 val)
{
    if (!Hold)
    {
        Index = val;
        Hold = true;
        Data = 0;
        DataPos = 1;
        return;
    }

    if (DataPos == 1)
    {
        // TODO: DSi-specific registers in DSi mode
        u32 regid = Index & 0x07;

        if (Index & 0x80)
        {
            Data = Registers[regid];
        }
        else
        {
            Registers[regid] = (Registers[regid] & ~RegMasks[regid]) | (val & RegMasks[regid]);

            switch (regid)
            {
            case 0:
                if (val & 0x40) NDS.Stop(StopReason::PowerOff); // shutdown
                //printf("power %02X\n", val);
                break;
            case 4:
                //printf("brightness %02X\n", val);
                break;
            }
        }
    }
    else
        Data = 0;
}



TSC::TSC(melonDS::NDS& nds) : SPIDevice(nds)
{
}

TSC::~TSC()
{
}

void TSC::Reset()
{
    Hold = false;
    ControlByte = 0;
    Data = 0;

    ConvResult = 0;

    MicBufferLen = 0;
}

void TSC::DoSavestate(Savestate* file)
{
    file->Section("SPTS");

    file->Var32(&DataPos);
    file->Var8(&ControlByte);
    file->Var8(&Data);

    file->Var16(&ConvResult);
}

void TSC::SetTouchCoords(u16 x, u16 y)
{
    // scr.x = (adc.x-adc.x1) * (scr.x2-scr.x1) / (adc.x2-adc.x1) + (scr.x1-1)
    // scr.y = (adc.y-adc.y1) * (scr.y2-scr.y1) / (adc.y2-adc.y1) + (scr.y1-1)
    // adc.x = ((scr.x * ((adc.x2-adc.x1) + (scr.x1-1))) / (scr.x2-scr.x1)) + adc.x1
    // adc.y = ((scr.y * ((adc.y2-adc.y1) + (scr.y1-1))) / (scr.y2-scr.y1)) + adc.y1
    TouchX = x;
    TouchY = y;

    if (y == 0xFFF)
    {
        // released
        NDS.KeyInput |= (1 << (16+6));
        return;
    }

    TouchX <<= 4;
    TouchY <<= 4;
    NDS.KeyInput &= ~(1 << (16+6));
}

void TSC::MicInputFrame(const s16* data, int samples)
{
    if (!data)
    {
        MicBufferLen = 0;
        return;
    }

    if (samples > 1024) samples = 1024;
    memcpy(MicBuffer, data, samples*sizeof(s16));
    MicBufferLen = samples;
}

void TSC::Write(u8 val)
{
    if (DataPos == 1)
        Data = (ConvResult >> 5) & 0xFF;
    else if (DataPos == 2)
        Data = (ConvResult << 3) & 0xFF;
    else
        Data = 0;

    if (val & 0x80)
    {
        ControlByte = val;
        DataPos = 1;

        switch (ControlByte & 0x70)
        {
        case 0x10: ConvResult = TouchY; break;
        case 0x50: ConvResult = TouchX; break;

        case 0x60:
            {
                if (MicBufferLen == 0)
                    ConvResult = 0x800;
                else
                {
                    // 560190 cycles per frame
                    u32 cyclepos = (u32)NDS.GetSysClockCycles(2);
                    u32 samplepos = (cyclepos * MicBufferLen) / 560190;
                    if (samplepos >= MicBufferLen) samplepos = MicBufferLen-1;
                    s16 sample = MicBuffer[samplepos];

                    // make it louder
                    //if (sample > 0x3FFF) sample = 0x7FFF;
                    //else if (sample < -0x4000) sample = -0x8000;
                    //else sample <<= 1;

                    // make it unsigned 12-bit
                    sample ^= 0x8000;
                    ConvResult = sample >> 4;
                }
            }
            break;

        default: ConvResult = 0xFFF; break;
        }

        if (ControlByte & 0x08)
            ConvResult &= 0x0FF0; // checkme
    }
    else
        DataPos++;
}



SPIHost::SPIHost(melonDS::NDS& nds, Firmware&& firmware) : NDS(nds)
{
    NDS.RegisterEventFunc(Event_SPITransfer, 0, MemberEventFunc(SPIHost, TransferDone));

    Devices[SPIDevice_FirmwareMem] = new FirmwareMem(NDS, std::move(firmware));
    Devices[SPIDevice_PowerMan] = new PowerMan(NDS);

    if (NDS.ConsoleType == 1)
        Devices[SPIDevice_TSC] = new DSi_TSC(static_cast<DSi&>(NDS));
    else
        Devices[SPIDevice_TSC] = new TSC(NDS);
}

SPIHost::~SPIHost()
{
    for (int i = 0; i < SPIDevice_MAX; i++)
    {
        if (Devices[i])
            delete Devices[i];

        Devices[i] = nullptr;
    }

    NDS.UnregisterEventFunc(Event_SPITransfer, 0);
}

void SPIHost::Reset()
{
    Cnt = 0;

    for (int i = 0; i < SPIDevice_MAX; i++)
    {
        Devices[i]->Reset();
    }
}

void SPIHost::DoSavestate(Savestate* file)
{
    file->Section("SPIG");

    file->Var16(&Cnt);

    for (int i = 0; i < SPIDevice_MAX; i++)
    {
        Devices[i]->DoSavestate(file);
    }
}


void SPIHost::WriteCnt(u16 val)
{
    // turning it off should clear chipselect
    // TODO: confirm on hardware. libnds expects this, though.
    if ((Cnt & (1<<15)) && !(val & (1<<15)))
    {
        int dev = (Cnt >> 8) & 0x3;
        if (dev < SPIDevice_MAX)
        {
            Devices[dev]->Release();
        }
    }

    // TODO: presumably the transfer speed can be changed during a transfer
    // like with the NDSCart SPI interface
    Cnt = (Cnt & 0x0080) | (val & 0xCF03);
    if (val & 0x0400) Log(LogLevel::Warn, "!! CRAPOED 16BIT SPI MODE\n");
    if (Cnt & (1<<7)) Log(LogLevel::Warn, "!! CHANGING SPICNT DURING TRANSFER: %04X\n", val);
}

void SPIHost::TransferDone(u32 param)
{
    Cnt &= ~(1<<7);

    if (Cnt & (1<<14))
        NDS.SetIRQ(1, IRQ_SPI);
}

u8 SPIHost::ReadData() const
{
    if (!(Cnt & (1<<15))) return 0;
    if (Cnt & (1<<7)) return 0; // checkme

    int dev = (Cnt >> 8) & 0x3;
    if (dev < SPIDevice_MAX)
    {
        return Devices[dev]->Read();
    }

    return 0;
}

void SPIHost::WriteData(u8 val)
{
    if (!(Cnt & (1<<15))) return;
    if (Cnt & (1<<7)) return;

    Cnt |= (1<<7);

    int dev = (Cnt >> 8) & 0x3;
    if (dev < SPIDevice_MAX)
    {
        Devices[dev]->Write(val);
        if (!(Cnt & (1<<11))) // release chipselect
            Devices[dev]->Release();
    }
    else
    {
        Log(LogLevel::Warn, "SPI to unknown device %04X %02X\n", Cnt, val);
    }

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (Cnt & 0x3));
    NDS.ScheduleEvent(Event_SPITransfer, false, delay, 0, 0);
}

}