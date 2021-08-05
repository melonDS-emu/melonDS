/*
    Copyright 2016-2021 Arisotura

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
#include "Config.h"
#include "NDS.h"
#include "SPI.h"
#include "DSi_SPI_TSC.h"
#include "Platform.h"


namespace SPI_Firmware
{

char FirmwarePath[1024];
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
    memset(FirmwarePath, 0, sizeof(FirmwarePath));
    Firmware = NULL;
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
        printf("Bad firmware size %d, ", originalLength);

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

        printf("assuming %d\n", originalLength);
    }
    return originalLength;
}

void LoadDefaultFirmware()
{
    FirmwareLength = 0x20000;
    Firmware = new u8[FirmwareLength];
    memset(Firmware, 0xFF, FirmwareLength);
    FirmwareMask = FirmwareLength - 1;

    u32 userdata = 0x7FE00 & FirmwareMask;

    memset(Firmware + userdata, 0, 0x74);

    // user settings offset
    *(u16*)&Firmware[0x20] = (FirmwareLength - 0x200) >> 3;

    Firmware[userdata+0x00] = 5; // version
}

void LoadFirmwareFromFile(FILE* f)
{
    fseek(f, 0, SEEK_END);

    FirmwareLength = FixFirmwareLength((u32)ftell(f));

    Firmware = new u8[FirmwareLength];

    fseek(f, 0, SEEK_SET);
    fread(Firmware, 1, FirmwareLength, f);

    fclose(f);

    // take a backup
    char firmbkp[1028];
    int fplen = strlen(FirmwarePath);
    strncpy(&firmbkp[0], FirmwarePath, fplen);
    strncpy(&firmbkp[fplen], ".bak", 1028-fplen);
    firmbkp[fplen+4] = '\0';
    f = Platform::OpenLocalFile(firmbkp, "rb");
    if (f) fclose(f);
    else
    {
        f = Platform::OpenLocalFile(firmbkp, "wb");
        fwrite(Firmware, 1, FirmwareLength, f);
        fclose(f);
    }
}

void LoadUserSettingsFromConfig() {
    // setting up username
    std::string username(Config::FirmwareUsername);
    std::u16string u16Username(username.begin(), username.end());
    size_t usernameLength = std::min(u16Username.length(), (size_t) 10);
    memcpy(Firmware + UserSettings + 0x06, u16Username.data(), usernameLength * sizeof(char16_t));
    Firmware[UserSettings+0x1A] = usernameLength;

    // setting language
    Firmware[UserSettings+0x64] = Config::FirmwareLanguage;

    // setting up color
    Firmware[UserSettings+0x02] = Config::FirmwareFavouriteColour;

    // setting up birthday
    Firmware[UserSettings+0x03] = Config::FirmwareBirthdayMonth;
    Firmware[UserSettings+0x04] = Config::FirmwareBirthdayDay;

    // setup message
    std::string message(Config::FirmwareMessage);
    std::u16string u16message(message.begin(), message.end());
    size_t messageLength = std::min(u16message.length(), (size_t) 26);
    memcpy(Firmware + UserSettings + 0x1C, u16message.data(), messageLength * sizeof(char16_t));
    Firmware[UserSettings+0x50] = messageLength;
}

void Reset()
{
    if (Firmware) delete[] Firmware;
    Firmware = NULL;

    if (NDS::ConsoleType == 1)
        strncpy(FirmwarePath, Config::DSiFirmwarePath, 1023);
    else
        strncpy(FirmwarePath, Config::FirmwarePath, 1023);

    FILE* f = Platform::OpenLocalFile(FirmwarePath, "rb");
    if (!f)
    {
        printf("Firmware not found generating default one.\n");
        LoadDefaultFirmware();
    }
    else
    {
        LoadFirmwareFromFile(f);
    }

    FirmwareMask = FirmwareLength - 1;

    u32 userdata = 0x7FE00 & FirmwareMask;
    if (*(u16*)&Firmware[userdata+0x170] == ((*(u16*)&Firmware[userdata+0x70] + 1) & 0x7F))
    {
        if (VerifyCRC16(0xFFFF, userdata+0x100, 0x70, userdata+0x172))
            userdata += 0x100;
    }

    UserSettings = userdata;

    // TODO evetually: do this in DSi mode
    if (NDS::ConsoleType == 0)
    {
        if (!f || Config::FirmwareOverrideSettings)
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

        if (Config::RandomizeMAC)
        {
            // replace MAC address with random address
            Firmware[0x36] = 0x00;
            Firmware[0x37] = 0x09;
            Firmware[0x38] = 0xBF;
            Firmware[0x39] = rand()&0xFF;
            Firmware[0x3A] = rand()&0xFF;
            Firmware[0x3B] = rand()&0xFF;

            *(u16*)&Firmware[0x2A] = CRC16(&Firmware[0x2C], *(u16*)&Firmware[0x2C], 0x0000);
        }
    }

    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           Firmware[0x36], Firmware[0x37], Firmware[0x38],
           Firmware[0x39], Firmware[0x3A], Firmware[0x3B]);

    // verify shit
    printf("FW: WIFI CRC16 = %s\n", VerifyCRC16(0x0000, 0x2C, *(u16*)&Firmware[0x2C], 0x2A)?"GOOD":"BAD");
    printf("FW: AP1 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FA00&FirmwareMask, 0xFE, 0x7FAFE&FirmwareMask)?"GOOD":"BAD");
    printf("FW: AP2 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FB00&FirmwareMask, 0xFE, 0x7FBFE&FirmwareMask)?"GOOD":"BAD");
    printf("FW: AP3 CRC16 = %s\n", VerifyCRC16(0x0000, 0x7FC00&FirmwareMask, 0xFE, 0x7FCFE&FirmwareMask)?"GOOD":"BAD");
    printf("FW: USER0 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x7FE00&FirmwareMask, 0x70, 0x7FE72&FirmwareMask)?"GOOD":"BAD");
    printf("FW: USER1 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x7FF00&FirmwareMask, 0x70, 0x7FF72&FirmwareMask)?"GOOD":"BAD");

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

void SetupDirectBoot()
{
    NDS::ARM9Write32(0x027FF864, 0);
    NDS::ARM9Write32(0x027FF868, *(u16*)&Firmware[0x20] << 3);

    NDS::ARM9Write16(0x027FF874, *(u16*)&Firmware[0x26]);
    NDS::ARM9Write16(0x027FF876, *(u16*)&Firmware[0x04]);

    for (u32 i = 0; i < 0x70; i += 4)
        NDS::ARM9Write32(0x027FFC80+i, *(u32*)&Firmware[UserSettings+i]);
}

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
        printf("unknown firmware SPI command %02X\n", CurCmd);
        break;
    }

    if (!hold && (CurCmd == 0x02 || CurCmd == 0x0A))
    {
        FILE* f = Platform::OpenLocalFile(FirmwarePath, "r+b");
        if (f)
        {
            u32 cutoff = 0x7FA00 & FirmwareMask;
            fseek(f, cutoff, SEEK_SET);
            fwrite(&Firmware[cutoff], FirmwareLength-cutoff, 1, f);
            fclose(f);
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
    RegMasks[1] = 0x01;
    RegMasks[2] = 0x01;
    RegMasks[3] = 0x03;
    RegMasks[4] = 0x0F;
}

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
                if (val & 0x40) NDS::Stop(); // shutdown
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

    Cnt = (Cnt & 0x0080) | (val & 0xCF03);
    if (val & 0x0400) printf("!! CRAPOED 16BIT SPI MODE\n");
    if (Cnt & (1<<7)) printf("!! CHANGING SPICNT DURING TRANSFER: %04X\n", val);
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

    if (Cnt & (1<<7)) printf("!! WRITING AUXSPIDATA DURING PENDING TRANSFER\n");

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
    default: printf("SPI to unknown device %04X %02X\n", Cnt, val); break;
    }

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (Cnt & 0x3));
    NDS::ScheduleEvent(NDS::Event_SPITransfer, false, delay, TransferDone, 0);
}

}
