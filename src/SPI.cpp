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
#include "NDS.h"
#include "DSi.h"
#include "SPI.h"
#include "DSi_SPI_TSC.h"
#include "Platform.h"

using namespace Platform;

namespace SPI_Firmware
{

std::string FirmwarePath;
u8* Firmware;
u32 FirmwareLength;
u32 FirmwareMask;

u32 UserSettings;

u32 Hold;
u8 CurCmd;
u32 DataPos;
u8 Data;

u8 StatusReg;
u32 Addr;


u16 CRC16(u8* data, u32 len, u32 start)
{
    u16 blarg[8] = {0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001};

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
    u16 crc_stored = *(u16*)&Firmware[crcoffset];
    u16 crc_calced = CRC16(&Firmware[offset], len, start);
    return (crc_stored == crc_calced);
}


bool Init()
{
    FirmwarePath = "";
    Firmware = nullptr;
    return true;
}

void DeInit()
{
    if (Firmware) delete[] Firmware;
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

void LoadDefaultFirmware()
{
    Log(LogLevel::Debug, "Using default firmware image...\n");

    FirmwareLength = 0x20000;
    Firmware = new u8[FirmwareLength];
    memset(Firmware, 0xFF, FirmwareLength);
    FirmwareMask = FirmwareLength - 1;

    memset(Firmware, 0, 0x1D);

    if (NDS::ConsoleType == 1)
    {
        Firmware[0x1D] = 0x57; // DSi
        Firmware[0x2F] = 0x0F;
        Firmware[0x1FD] = 0x01;
        Firmware[0x1FE] = 0x20;
        Firmware[0x2FF] = 0x80; // boot0: use NAND as stage2 medium

        // these need to be zero (part of the stage2 firmware signature!)
        memset(&Firmware[0x22], 0, 8);
    }
    else
    {
        Firmware[0x1D] = 0x20; // DS Lite (TODO: make configurable?)
        Firmware[0x2F] = 0x06;
    }

    // wifi calibration

    const u8 defaultmac[6] = {0x00, 0x09, 0xBF, 0x11, 0x22, 0x33};
    const u8 bbinit[0x69] =
    {
        0x03, 0x17, 0x40, 0x00, 0x1B, 0x6C, 0x48, 0x80, 0x38, 0x00, 0x35, 0x07, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0xBB, 0x01, 0x24, 0x7F,
        0x5A, 0x01, 0x3F, 0x01, 0x3F, 0x36, 0x1D, 0x00, 0x78, 0x35, 0x55, 0x12, 0x34, 0x1C, 0x00, 0x01,
        0x0E, 0x38, 0x03, 0x70, 0xC5, 0x2A, 0x0A, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFE,
        0xFE, 0xFE, 0xFE, 0xFC, 0xFC, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0xF8, 0xF8, 0xF6, 0x00, 0x12, 0x14,
        0x12, 0x41, 0x23, 0x03, 0x04, 0x70, 0x35, 0x0E, 0x2C, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x12, 0x28, 0x1C
    };
    const u8 rfinit[0x29] =
    {
        0x31, 0x4C, 0x4F, 0x21, 0x00, 0x10, 0xB0, 0x08, 0xFA, 0x15, 0x26, 0xE6, 0xC1, 0x01, 0x0E, 0x50,
        0x05, 0x00, 0x6D, 0x12, 0x00, 0x00, 0x01, 0xFF, 0x0E, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x06,
        0x06, 0x00, 0x00, 0x00, 0x18, 0x00, 0x02, 0x00, 0x00
    };
    const u8 chandata[0x3C] =
    {
        0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x16,
        0x26, 0x1C, 0x1C, 0x1C, 0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1E, 0x1F, 0x1E, 0x1F, 0x18,
        0x01, 0x4B, 0x4B, 0x4B, 0x4B, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4D, 0x4D, 0x4D,
        0x02, 0x6C, 0x71, 0x76, 0x5B, 0x40, 0x45, 0x4A, 0x2F, 0x34, 0x39, 0x3E, 0x03, 0x08, 0x14
    };

    *(u16*)&Firmware[0x2C] = 0x138;
    Firmware[0x2E] = 0;
    *(u32*)&Firmware[0x30] = 0xFFFFFFFF;
    *(u16*)&Firmware[0x34] = 0x00FF;
    memcpy(&Firmware[0x36], defaultmac, 6);
    *(u16*)&Firmware[0x3C] = 0x3FFE;
    *(u16*)&Firmware[0x3E] = 0xFFFF;
    Firmware[0x40] = 0x03;
    Firmware[0x41] = 0x94;
    Firmware[0x42] = 0x29;
    Firmware[0x43] = 0x02;
    *(u16*)&Firmware[0x44] = 0x0002;
    *(u16*)&Firmware[0x46] = 0x0017;
    *(u16*)&Firmware[0x48] = 0x0026;
    *(u16*)&Firmware[0x4A] = 0x1818;
    *(u16*)&Firmware[0x4C] = 0x0048;
    *(u16*)&Firmware[0x4E] = 0x4840;
    *(u16*)&Firmware[0x50] = 0x0058;
    *(u16*)&Firmware[0x52] = 0x0042;
    *(u16*)&Firmware[0x54] = 0x0146;
    *(u16*)&Firmware[0x56] = 0x8064;
    *(u16*)&Firmware[0x58] = 0xE6E6;
    *(u16*)&Firmware[0x5A] = 0x2443;
    *(u16*)&Firmware[0x5C] = 0x000E;
    *(u16*)&Firmware[0x5E] = 0x0001;
    *(u16*)&Firmware[0x60] = 0x0001;
    *(u16*)&Firmware[0x62] = 0x0402;
    memcpy(&Firmware[0x64], bbinit, 0x69);
    Firmware[0xCD] = 0;
    memcpy(&Firmware[0xCE], rfinit, 0x29);
    Firmware[0xF7] = 0x02;
    memcpy(&Firmware[0xF8], chandata, 0x3C);

    *(u16*)&Firmware[0x2A] = CRC16(&Firmware[0x2C], *(u16*)&Firmware[0x2C], 0x0000);

    // user data

    u32 userdata = 0x7FE00 & FirmwareMask;
    *(u16*)&Firmware[0x20] = userdata >> 3;

    memset(Firmware + userdata, 0, 0x74);
    Firmware[userdata+0x00] = 5; // version
    Firmware[userdata+0x03] = 1;
    Firmware[userdata+0x04] = 1;
    *(u16*)&Firmware[userdata+0x64] = 0x0031;

    *(u16*)&Firmware[userdata+0x72] = CRC16(&Firmware[userdata], 0x70, 0xFFFF);

    // wifi access points
    // TODO: WFC ID??

    std::string wfcsettings = Platform::GetConfigString(ConfigEntry::WifiSettingsPath);

    FileHandle* f = Platform::OpenLocalFile(wfcsettings + Platform::InstanceFileSuffix(), FileMode::Read);
    if (!f) f = Platform::OpenLocalFile(wfcsettings, FileMode::Read);
    if (f)
    {
        u32 apdata = userdata - 0xA00;
        FileRead(&Firmware[apdata], 0x900, 1, f);
        CloseFile(f);
    }
    else
    {
        u32 apdata = userdata - 0x400;
        memset(&Firmware[apdata], 0, 0x300);

        strcpy((char*)&Firmware[apdata+0x40], "melonAP");
        if (NDS::ConsoleType == 1) *(u16*)&Firmware[apdata+0xEA] = 1400;
        Firmware[apdata+0xEF] = 0x01;
        *(u16*)&Firmware[apdata+0xFE] = CRC16(&Firmware[apdata], 0xFE, 0x0000);

        apdata += 0x100;
        Firmware[apdata+0xE7] = 0xFF;
        Firmware[apdata+0xEF] = 0x01;
        *(u16*)&Firmware[apdata+0xFE] = CRC16(&Firmware[apdata], 0xFE, 0x0000);

        apdata += 0x100;
        Firmware[apdata+0xE7] = 0xFF;
        Firmware[apdata+0xEF] = 0x01;
        *(u16*)&Firmware[apdata+0xFE] = CRC16(&Firmware[apdata], 0xFE, 0x0000);

        if (NDS::ConsoleType == 1)
        {
            apdata = userdata - 0xA00;
            Firmware[apdata+0xE7] = 0xFF;
            *(u16*)&Firmware[apdata+0xFE] = CRC16(&Firmware[apdata], 0xFE, 0x0000);

            apdata += 0x200;
            Firmware[apdata+0xE7] = 0xFF;
            *(u16*)&Firmware[apdata+0xFE] = CRC16(&Firmware[apdata], 0xFE, 0x0000);

            apdata += 0x200;
            Firmware[apdata+0xE7] = 0xFF;
            *(u16*)&Firmware[apdata+0xFE] = CRC16(&Firmware[apdata], 0xFE, 0x0000);
        }
    }
}

void LoadFirmwareFromFile(FileHandle* f, bool makecopy)
{
    FirmwareLength = FixFirmwareLength(FileLength(f));

    Firmware = new u8[FirmwareLength];

    FileRewind(f);
    FileRead(Firmware, 1, FirmwareLength, f);

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
            FileWrite(Firmware, 1, FirmwareLength, bf);
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
    // setting up username
    std::string orig_username = Platform::GetConfigString(Platform::Firm_Username);
    std::u16string username = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_username);
    size_t usernameLength = std::min(username.length(), (size_t) 10);
    memcpy(Firmware + UserSettings + 0x06, username.data(), usernameLength * sizeof(char16_t));
    Firmware[UserSettings+0x1A] = usernameLength;

    // setting language
    Firmware[UserSettings+0x64] = Platform::GetConfigInt(Platform::Firm_Language);

    // setting up color
    Firmware[UserSettings+0x02] = Platform::GetConfigInt(Platform::Firm_Color);

    // setting up birthday
    Firmware[UserSettings+0x03] = Platform::GetConfigInt(Platform::Firm_BirthdayMonth);
    Firmware[UserSettings+0x04] = Platform::GetConfigInt(Platform::Firm_BirthdayDay);

    // setup message
    std::string orig_message = Platform::GetConfigString(Platform::Firm_Message);
    std::u16string message = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_message);
    size_t messageLength = std::min(message.length(), (size_t) 26);
    memcpy(Firmware + UserSettings + 0x1C, message.data(), messageLength * sizeof(char16_t));
    Firmware[UserSettings+0x50] = messageLength;
}

void Reset()
{
    if (Firmware) delete[] Firmware;
    Firmware = nullptr;
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
        LoadDefaultFirmware();
        firmoverride = true;
    }
    else
    {
        firmoverride = Platform::GetConfigBool(Platform::Firm_OverrideSettings);
    }

    FirmwareMask = FirmwareLength - 1;

    u32 userdata = 0x7FE00 & FirmwareMask;
    if (*(u16*)&Firmware[userdata+0x170] == ((*(u16*)&Firmware[userdata+0x70] + 1) & 0x7F))
    {
        if (VerifyCRC16(0xFFFF, userdata+0x100, 0x70, userdata+0x172))
            userdata += 0x100;
    }

    UserSettings = userdata;

    if (firmoverride)
        LoadUserSettingsFromConfig();

    // fix touchscreen coords
    *(u16*)&Firmware[userdata+0x58] = 0;
    *(u16*)&Firmware[userdata+0x5A] = 0;
    Firmware[userdata+0x5C] = 0;
    Firmware[userdata+0x5D] = 0;
    *(u16*)&Firmware[userdata+0x5E] = 255<<4;
    *(u16*)&Firmware[userdata+0x60] = 191<<4;
    Firmware[userdata+0x62] = 255;
    Firmware[userdata+0x63] = 191;

    // disable autoboot
    //Firmware[userdata+0x64] &= 0xBF;

    *(u16*)&Firmware[userdata+0x72] = CRC16(&Firmware[userdata], 0x70, 0xFFFF);

    //if (firmoverride)
    {
        u8 mac[6];
        bool rep = false;

        memcpy(mac, &Firmware[0x36], 6);

        if (firmoverride)
            rep = Platform::GetConfigArray(Platform::Firm_MAC, mac);

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
            memcpy(&Firmware[0x36], mac, 6);

            *(u16*)&Firmware[0x2A] = CRC16(&Firmware[0x2C], *(u16*)&Firmware[0x2C], 0x0000);
        }
    }

    Log(LogLevel::Info, "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           Firmware[0x36], Firmware[0x37], Firmware[0x38],
           Firmware[0x39], Firmware[0x3A], Firmware[0x3B]);

    // verify shit
    Log(LogLevel::Debug, "FW: WIFI CRC16 = %s\n", VerifyCRC16(0x0000, 0x2C, *(u16*)&Firmware[0x2C], 0x2A)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: AP1 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FA00&FirmwareMask, 0xFE, 0x7FAFE&FirmwareMask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: AP2 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FB00&FirmwareMask, 0xFE, 0x7FBFE&FirmwareMask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: AP3 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FC00&FirmwareMask, 0xFE, 0x7FCFE&FirmwareMask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: USER0 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x7FE00&FirmwareMask, 0x70, 0x7FE72&FirmwareMask)?"GOOD":"BAD");
    Log(LogLevel::Debug, "FW: USER1 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x7FF00&FirmwareMask, 0x70, 0x7FF72&FirmwareMask)?"GOOD":"BAD");

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
    file->Var32(&FirmwareLength);
    if (file->Saving)
    {
        file->VarArray(Firmware, FirmwareLength);
    }
    else
    {
        if (Firmware) delete[] Firmware;
        Firmware = new u8[FirmwareLength];
        file->VarArray(Firmware, FirmwareLength);

        FirmwareMask = FirmwareLength - 1;

        u32 userdata = 0x7FE00 & FirmwareMask;
        if (*(u16*)&Firmware[userdata+0x170] == ((*(u16*)&Firmware[userdata+0x70] + 1) & 0x7F))
        {
            if (VerifyCRC16(0xFFFF, userdata+0x100, 0x70, userdata+0x172))
                userdata += 0x100;
        }

        UserSettings = userdata;
    }

    file->Var32(&Hold);
    file->Var8(&CurCmd);
    file->Var32(&DataPos);
    file->Var8(&Data);

    file->Var8(&StatusReg);
    file->Var32(&Addr);
}

void SetupDirectBoot(bool dsi)
{
    if (dsi)
    {
        for (u32 i = 0; i < 6; i += 2)
            DSi::ARM9Write16(0x02FFFCF4, *(u16*)&Firmware[0x36+i]); // MAC address

        // checkme
        DSi::ARM9Write16(0x02FFFCFA, *(u16*)&Firmware[0x3C]); // enabled channels

        for (u32 i = 0; i < 0x70; i += 4)
            DSi::ARM9Write32(0x02FFFC80+i, *(u32*)&Firmware[UserSettings+i]);
    }
    else
    {
        NDS::ARM9Write32(0x027FF864, 0);
        NDS::ARM9Write32(0x027FF868, *(u16*)&Firmware[0x20] << 3); // user settings offset

        NDS::ARM9Write16(0x027FF874, *(u16*)&Firmware[0x26]); // CRC16 for data/gfx
        NDS::ARM9Write16(0x027FF876, *(u16*)&Firmware[0x04]); // CRC16 for GUI/wifi code

        for (u32 i = 0; i < 0x70; i += 4)
            NDS::ARM9Write32(0x027FFC80+i, *(u32*)&Firmware[UserSettings+i]);
    }
}

u32 GetFirmwareLength() { return FirmwareLength; }
u8 GetConsoleType() { return Firmware[0x1D]; }
u8 GetWifiVersion() { return Firmware[0x2F]; }
u8 GetNWifiVersion() { return Firmware[0x1FD]; } // for DSi; will return 0xFF on a DS
u8 GetRFVersion() { return Firmware[0x40]; }
u8* GetWifiMAC() { return &Firmware[0x36]; }

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
                Data = Firmware[Addr & FirmwareMask];
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
                Firmware[Addr & FirmwareMask] = val;
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
                u32 cutoff = ((NDS::ConsoleType==1) ? 0x7F400 : 0x7FA00) & FirmwareMask;
                FileSeek(f, cutoff, FileSeekOrigin::Start);
                FileWrite(&Firmware[cutoff], FirmwareLength-cutoff, 1, f);
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
                u32 cutoff = 0x7F400 & FirmwareMask;
                FileWrite(&Firmware[cutoff], 0x900, 1, f);
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
