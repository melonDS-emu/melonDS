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
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <memory>
#include "NDS.h"
#include "DSi.h"
#include "SPI.h"
#include "DSi_SPI_TSC.h"
#include "Platform.h"

using namespace Platform;

namespace SPI_Firmware
{

[[deprecated("Load firmware from memory instead of from disk")]] std::string FirmwarePath;
std::unique_ptr<Firmware> Firmware;

u32 Hold;
u8 CurCmd;
u32 DataPos;
u8 Data;

u8 StatusReg;
u32 Addr;

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

bool VerifyCRC16(u32 start, u32 offset, u32 len, u32 crcoffset)
{
    u16 crc_stored =  *(u16*)&Firmware->Buffer()[crcoffset];
    u16 crc_calced = CRC16(&Firmware->Buffer()[offset], len, start);
    return (crc_stored == crc_calced);
}


bool Init()
{
    FirmwarePath = "";
    Firmware.reset();
    return true;
}

void DeInit()
{
    Firmware.reset();
}

u32 FixFirmwareLength(u32 originalLength)
{
    if (originalLength != 0x20000 && originalLength != 0x40000 && originalLength != 0x80000)
    {
        Log(LogLevel::Warn, "Bad firmware size %d, ", originalLength);

        // pick the nearest power-of-two length
        originalLength |= (originalLength >> 1);
        originalLength |= (originalLength >> 2);
        originalLength |= (originalLength >> 4);
        originalLength |= (originalLength >> 8);
        originalLength |= (originalLength >> 16);
        originalLength++;

        // ensure it's a sane length
        if (originalLength > 0x80000) originalLength = 0x80000;
        else if (originalLength < 0x20000) originalLength = 0x20000;

        Log(LogLevel::Debug, "assuming %d\n", originalLength);
    }
    return originalLength;
}

[[deprecated("Load firmware from memory instead of from disk")]]
void LoadFirmwareFromFile(FileHandle* f, bool makecopy)
{
    Firmware = std::make_unique<class Firmware>(f);

    // take a backup
    std::string fwBackupPath;
    if (!makecopy) fwBackupPath = FirmwarePath + ".bak";
    else           fwBackupPath = FirmwarePath;
    FileHandle* bf = Platform::OpenLocalFile(fwBackupPath, FileMode::Read);
    if (!bf)
    {
        bf = Platform::OpenLocalFile(fwBackupPath, FileMode::Write);
        if (bf)
        {
            FileWrite(Firmware->Buffer(), 1, Firmware->Length(), bf);
            CloseFile(bf);
        }
        else
        {
            Log(LogLevel::Error, "Could not write firmware backup!\n");
        }
    }
    else
    {
        CloseFile(bf);
    }
}

void LoadUserSettingsFromConfig()
{
    UserData& currentData = Firmware->EffectiveUserData();

    // setting up username
    std::string orig_username = Platform::GetConfigString(Platform::Firm_Username);
    if (!orig_username.empty())
    { // If the frontend defines a username, take it. If not, leave the existing one.
        std::u16string username = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_username);
        size_t usernameLength = std::min(username.length(), (size_t) 10);
        currentData.NameLength = usernameLength;
        memcpy(currentData.Nickname, username.data(), usernameLength * sizeof(char16_t));
    }

    auto language = static_cast<Language>(Platform::GetConfigInt(Platform::Firm_Language));
    if (language != Language::Reserved)
    { // If the frontend specifies a language (rather than using the existing value)...
        currentData.Settings &= ~Language::Reserved; // ..clear the existing language...
        currentData.Settings |= language; // ...and set the new one.
    }

    // setting up color
    u8 favoritecolor = Platform::GetConfigInt(Platform::Firm_Color);
    if (favoritecolor != 0xFF)
    {
        currentData.FavoriteColor = favoritecolor;
    }

    u8 birthmonth = Platform::GetConfigInt(Platform::Firm_BirthdayMonth);
    if (birthmonth != 0)
    { // If the frontend specifies a birth month (rather than using the existing value)...
        currentData.BirthdayMonth = birthmonth;
    }

    u8 birthday = Platform::GetConfigInt(Platform::Firm_BirthdayDay);
    if (birthday != 0)
    { // If the frontend specifies a birthday (rather than using the existing value)...
        currentData.BirthdayDay = birthday;
    }

    // setup message
    std::string orig_message = Platform::GetConfigString(Platform::Firm_Message);
    if (!orig_message.empty())
    {
        std::u16string message = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_message);
        size_t messageLength = std::min(message.length(), (size_t) 26);
        currentData.MessageLength = messageLength;
        memcpy(currentData.Message, message.data(), messageLength * sizeof(char16_t));
    }
}

void Reset()
{
    Firmware.reset();
    FirmwarePath = "";
    bool firmoverride = false;

    if (Platform::GetConfigBool(Platform::ExternalBIOSEnable))
    {
        if (NDS::ConsoleType == 1)
            FirmwarePath = Platform::GetConfigString(Platform::DSi_FirmwarePath);
        else
            FirmwarePath = Platform::GetConfigString(Platform::FirmwarePath);

        Log(LogLevel::Debug, "SPI firmware: loading from file %s\n", FirmwarePath.c_str());

        bool makecopy = false;
        std::string origpath = FirmwarePath;
        FirmwarePath += Platform::InstanceFileSuffix();

        FileHandle* f = Platform::OpenLocalFile(FirmwarePath, FileMode::Read);
        if (!f)
        {
            f = Platform::OpenLocalFile(origpath, FileMode::Read);
            makecopy = true;
        }
        if (!f)
        {
            Log(LogLevel::Warn,"Firmware not found! Generating default firmware.\n");
            FirmwarePath = "";
        }
        else
        {
            LoadFirmwareFromFile(f, makecopy);
            CloseFile(f);
        }
    }

    if (FirmwarePath.empty())
    {
        Log(Debug, "Creating default firmware image...\n");

        Firmware = std::make_unique<class Firmware>(NDS::ConsoleType);
        firmoverride = true;
    }
    else
    {
        firmoverride = Platform::GetConfigBool(Platform::Firm_OverrideSettings);
    }

    u32 userdata = 0x7FE00 & Firmware->Mask();
    auto& userDataRegions = Firmware->UserData();
    if (*(u16*)&Firmware->Buffer()[userdata+0x170] == ((*(u16*)&Firmware->Buffer()[userdata+0x70] + 1) & 0x7F))
    { // If both user data regions have the same update counter...
        if (VerifyCRC16(0xFFFF, userdata+0x100, 0x70, userdata+0x172))
            userdata += 0x100;
    }

    if (firmoverride)
        LoadUserSettingsFromConfig();

    // fix touchscreen coords
    for (UserData& u : userDataRegions)
    {
        u.TouchCalibrationADC1[0] = 0;
        u.TouchCalibrationADC1[1] = 0;
        u.TouchCalibrationPixel1[0] = 0;
        u.TouchCalibrationPixel1[1] = 0;
        u.TouchCalibrationADC2[0] = 255<<4;
        u.TouchCalibrationADC2[1] = 191<<4;
        u.TouchCalibrationPixel2[0] = 255;
        u.TouchCalibrationPixel2[1] = 191;
        u.UpdateChecksum();
    }

    // disable autoboot
    //Firmware[userdata+0x64] &= 0xBF;

    //if (firmoverride)
    {
        MacAddress mac;
        bool rep = false;
        auto& header = Firmware->Header();

        memcpy(&mac, header.MacAddress.data(), sizeof(MacAddress));

        if (firmoverride)
            rep = Platform::GetConfigArray(Platform::Firm_MAC, &mac);

        int inst = Platform::InstanceID();
        if (inst > 0)
        {
            rep = true;
            mac[3] += inst;
            mac[4] += inst*0x44;
            mac[5] += inst*0x10;
        }

        if (rep)
        {
            mac[0] &= 0xFC; // ensure the MAC isn't a broadcast MAC
            header.MacAddress = mac;
            header.UpdateChecksum();
        }

        Log(LogLevel::Info, "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // verify shit
    u32 mask = Firmware->Mask();
    Log(LogLevel::Debug, "FW: WIFI CRC16 = %s\n", VerifyCRC16(0x0000, 0x2C, *(u16*)&Firmware->Buffer()[0x2C], 0x2A)?"GOOD":"BAD");
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

void DoSavestate(Savestate* file)
{
    file->Section("SPFW");

    // CHECKME/TODO: trust the firmware to stay the same?????
    // embedding the whole firmware in the savestate would be derpo tho??

    file->Var32(&Hold);
    file->Var8(&CurCmd);
    file->Var32(&DataPos);
    file->Var8(&Data);

    file->Var8(&StatusReg);
    file->Var32(&Addr);
}

void SetupDirectBoot(bool dsi)
{
    const FirmwareHeader& header = Firmware->Header();
    const UserData& userdata = Firmware->EffectiveUserData();
    if (dsi)
    {
        for (u32 i = 0; i < 6; i += 2)
            DSi::ARM9Write16(0x02FFFCF4, *(u16*)&header.MacAddress[i]); // MAC address

        // checkme
        DSi::ARM9Write16(0x02FFFCFA, header.EnabledChannels); // enabled channels

        for (u32 i = 0; i < 0x70; i += 4)
            DSi::ARM9Write32(0x02FFFC80+i, *(u32*)&userdata.Bytes[i]);
    }
    else
    {
        NDS::ARM9Write32(0x027FF864, 0);
        NDS::ARM9Write32(0x027FF868, header.UserSettingsOffset << 3); // user settings offset

        NDS::ARM9Write16(0x027FF874, header.DataGfxChecksum); // CRC16 for data/gfx
        NDS::ARM9Write16(0x027FF876, header.GUIWifiCodeChecksum); // CRC16 for GUI/wifi code

        for (u32 i = 0; i < 0x70; i += 4)
            NDS::ARM9Write32(0x027FFC80+i, *(u32*)&userdata.Bytes[i]);
    }
}

u32 GetFirmwareLength() { return Firmware ? Firmware->Length() : 0; }
u8 GetConsoleType() { return static_cast<u8>(GetFirmwareHeader()->ConsoleType); }
u8 GetWifiVersion() { return static_cast<u8>(GetFirmwareHeader()->WifiVersion); }
u8 GetNWifiVersion() { return static_cast<u8>(Firmware->Header().WifiBoard); } // for DSi; will return 0xFF on a DS
u8 GetRFVersion() { return static_cast<u8>(GetFirmwareHeader()->RFChipType); }
u8* GetWifiMAC() { return Firmware->Header().MacAddress.data(); }
const FirmwareHeader* GetFirmwareHeader() { return Firmware ? &Firmware->Header() : nullptr; }

bool InstallFirmware(class Firmware&& firmware)
{
    if (!firmware.Buffer())
    {
        Log(LogLevel::Error, "SPI firmware: firmware buffer is null!\n");
        return false;
    }

    Firmware = std::make_unique<class Firmware>(std::move(firmware));

    return true;
}

void RemoveFirmware()
{
    Firmware.reset();
}

u8 Read()
{
    return Data;
}

void Write(u8 val, u32 hold)
{
    if (!hold)
    {
        if (!Hold) // commands with no paramters
            CurCmd = val;

        Hold = 0;
    }

    if (hold && (!Hold))
    {
        CurCmd = val;
        Hold = 1;
        Data = 0;
        DataPos = 1;
        Addr = 0;
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
                Data = Firmware->Buffer()[Addr & Firmware->Mask()];
                Addr++;
            }

            DataPos++;
        }
        break;

    case 0x04: // write disable
        StatusReg &= ~(1<<1);
        Data = 0;
        break;

    case 0x05: // read status reg
        Data = StatusReg;
        break;

    case 0x06: // write enable
        StatusReg |= (1<<1);
        Data = 0;
        break;

    case 0x0A: // write
        {
            // TODO: what happens if you write too many bytes? (max 256, they say)
            if (DataPos < 4)
            {
                Addr <<= 8;
                Addr |= val;
                Data = 0;
            }
            else
            {
                Firmware->Buffer()[Addr & Firmware->Mask()] = val;
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

    if (!hold && (CurCmd == 0x02 || CurCmd == 0x0A))
    {
        if (!FirmwarePath.empty())
        {
            FileHandle* f = Platform::OpenLocalFile(FirmwarePath, FileMode::ReadWriteExisting);
            if (f)
            {
                u32 cutoff = ((NDS::ConsoleType==1) ? 0x7F400 : 0x7FA00) & Firmware->Mask();
                FileSeek(f, cutoff, FileSeekOrigin::Start);
                FileWrite(&Firmware->Buffer()[cutoff], Firmware->Length()-cutoff, 1, f);
                CloseFile(f);
            }
        }
        else
        {
            std::string wfcfile = Platform::GetConfigString(ConfigEntry::WifiSettingsPath);
            if (Platform::InstanceID() > 0) wfcfile += Platform::InstanceFileSuffix();

            FileHandle* f = Platform::OpenLocalFile(wfcfile, FileMode::Write);
            if (f)
            {
                u32 cutoff = 0x7F400 & Firmware->Mask();
                FileWrite(&Firmware->Buffer()[cutoff], 0x900, 1, f);
                CloseFile(f);
            }
        }
    }
}

}

namespace SPI_Powerman
{

u32 Hold;
u32 DataPos;
u8 Index;
u8 Data;

u8 Registers[8];
u8 RegMasks[8];


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    Hold = 0;
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

bool GetBatteryLevelOkay() { return !Registers[1]; }
void SetBatteryLevelOkay(bool okay) { Registers[1] = okay ? 0x00 : 0x01; }

void DoSavestate(Savestate* file)
{
    file->Section("SPPW");

    file->Var32(&Hold);
    file->Var32(&DataPos);
    file->Var8(&Index);
    file->Var8(&Data);

    file->VarArray(Registers, 8);
    file->VarArray(RegMasks, 8); // is that needed??
}

u8 Read()
{
    return Data;
}

void Write(u8 val, u32 hold)
{
    if (!hold)
    {
        Hold = 0;
    }

    if (hold && (!Hold))
    {
        Index = val;
        Hold = 1;
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
                if (val & 0x40) NDS::Stop(StopReason::PowerOff); // shutdown
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

}


namespace SPI_TSC
{

u32 DataPos;
u8 ControlByte;
u8 Data;

u16 ConvResult;

u16 TouchX, TouchY;

s16 MicBuffer[1024];
int MicBufferLen;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    ControlByte = 0;
    Data = 0;

    ConvResult = 0;

    MicBufferLen = 0;
}

void DoSavestate(Savestate* file)
{
    file->Section("SPTS");

    file->Var32(&DataPos);
    file->Var8(&ControlByte);
    file->Var8(&Data);

    file->Var16(&ConvResult);
}

void SetTouchCoords(u16 x, u16 y)
{
    // scr.x = (adc.x-adc.x1) * (scr.x2-scr.x1) / (adc.x2-adc.x1) + (scr.x1-1)
    // scr.y = (adc.y-adc.y1) * (scr.y2-scr.y1) / (adc.y2-adc.y1) + (scr.y1-1)
    // adc.x = ((scr.x * ((adc.x2-adc.x1) + (scr.x1-1))) / (scr.x2-scr.x1)) + adc.x1
    // adc.y = ((scr.y * ((adc.y2-adc.y1) + (scr.y1-1))) / (scr.y2-scr.y1)) + adc.y1
    TouchX = x;
    TouchY = y;

    if (y == 0xFFF) return;

    TouchX <<= 4;
    TouchY <<= 4;
}

void MicInputFrame(s16* data, int samples)
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

u8 Read()
{
    return Data;
}

void Write(u8 val, u32 hold)
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
                    u32 cyclepos = (u32)NDS::GetSysClockCycles(2);
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

}


namespace SPI
{

u16 Cnt;

u32 CurDevice; // remove me


bool Init()
{
    if (!SPI_Firmware::Init()) return false;
    if (!SPI_Powerman::Init()) return false;
    if (!SPI_TSC::Init()) return false;
    if (!DSi_SPI_TSC::Init()) return false;

    return true;
}

void DeInit()
{
    SPI_Firmware::DeInit();
    SPI_Powerman::DeInit();
    SPI_TSC::DeInit();
    DSi_SPI_TSC::DeInit();
}

void Reset()
{
    Cnt = 0;

    SPI_Firmware::Reset();
    SPI_Powerman::Reset();
    SPI_TSC::Reset();
    if (NDS::ConsoleType == 1) DSi_SPI_TSC::Reset();
}

void DoSavestate(Savestate* file)
{
    file->Section("SPIG");

    file->Var16(&Cnt);
    file->Var32(&CurDevice);

    SPI_Firmware::DoSavestate(file);
    SPI_Powerman::DoSavestate(file);
    SPI_TSC::DoSavestate(file);
    if (NDS::ConsoleType == 1) DSi_SPI_TSC::DoSavestate(file);
}


void WriteCnt(u16 val)
{
    // turning it off should clear chipselect
    // TODO: confirm on hardware. libnds expects this, though.
    if ((Cnt & (1<<15)) && !(val & (1<<15)))
    {
        switch (Cnt & 0x0300)
        {
        case 0x0000: SPI_Powerman::Hold = 0; break;
        case 0x0100: SPI_Firmware::Hold = 0; break;
        case 0x0200:
            if (NDS::ConsoleType == 1)
                DSi_SPI_TSC::DataPos = 0;
            else
                SPI_TSC::DataPos = 0;
            break;
        }
    }

    // TODO: presumably the transfer speed can be changed during a transfer
    // like with the NDSCart SPI interface
    Cnt = (Cnt & 0x0080) | (val & 0xCF03);
    if (val & 0x0400) Log(LogLevel::Warn, "!! CRAPOED 16BIT SPI MODE\n");
    if (Cnt & (1<<7)) Log(LogLevel::Warn, "!! CHANGING SPICNT DURING TRANSFER: %04X\n", val);
}

void TransferDone(u32 param)
{
    Cnt &= ~(1<<7);

    if (Cnt & (1<<14))
        NDS::SetIRQ(1, NDS::IRQ_SPI);
}

u8 ReadData()
{
    if (!(Cnt & (1<<15))) return 0;
    if (Cnt & (1<<7)) return 0; // checkme

    switch (Cnt & 0x0300)
    {
    case 0x0000: return SPI_Powerman::Read();
    case 0x0100: return SPI_Firmware::Read();
    case 0x0200:
        if (NDS::ConsoleType == 1)
            return DSi_SPI_TSC::Read();
        else
            return SPI_TSC::Read();
    default: return 0;
    }
}

void WriteData(u8 val)
{
    if (!(Cnt & (1<<15))) return;
    if (Cnt & (1<<7)) return;

    Cnt |= (1<<7);
    switch (Cnt & 0x0300)
    {
    case 0x0000: SPI_Powerman::Write(val, Cnt&(1<<11)); break;
    case 0x0100: SPI_Firmware::Write(val, Cnt&(1<<11)); break;
    case 0x0200:
        if (NDS::ConsoleType == 1)
            DSi_SPI_TSC::Write(val, Cnt&(1<<11));
        else
            SPI_TSC::Write(val, Cnt&(1<<11));
        break;
        default: Log(LogLevel::Warn, "SPI to unknown device %04X %02X\n", Cnt, val); break;
    }

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (Cnt & 0x3));
    NDS::ScheduleEvent(NDS::Event_SPITransfer, false, delay, TransferDone, 0);
}

}
