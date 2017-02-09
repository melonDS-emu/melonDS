/*
    Copyright 2016-2017 StapleButter

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
#include "NDS.h"
#include "SPI.h"

#ifdef __LIBRETRO__
extern char retro_base_directory[4096];
#endif

namespace SPI_Firmware
{

u8* Firmware;
u32 FirmwareLength;

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
    //printf("%04X vs %04X\n", crc_stored, crc_calced);
    return (crc_stored == crc_calced);
}


bool Init()
{
    Firmware = NULL;
    return true;
}

void DeInit()
{
    if (Firmware) delete[] Firmware;
}

void Reset()
{
    if (Firmware) delete[] Firmware;
    Firmware = NULL;
#ifdef __LIBRETRO__
    char path[2047];
    snprintf(path, sizeof(path), "%s/firmware.bin", retro_base_directory);
    FILE* f = fopen(path, "rb");
#else
    FILE* f = fopen("firmware.bin", "rb");
#endif    
    if (!f)
    {
        printf("firmware.bin not found\n");

        // TODO: generate default firmware
        return;
    }

    fseek(f, 0, SEEK_END);
    FirmwareLength = (u32)ftell(f);
    Firmware = new u8[FirmwareLength];

    fseek(f, 0, SEEK_SET);
    fread(Firmware, FirmwareLength, 1, f);

    fclose(f);

    u32 userdata = 0x3FE00;
    if (*(u16*)&Firmware[0x3FF70] == ((*(u16*)&Firmware[0x3FE70] + 1) & 0x7F))
    {
        if (VerifyCRC16(0xFFFF, 0x3FF00, 0x70, 0x3FF72))
            userdata = 0x3FF00;
    }

    // fix touchscreen coords
    *(u16*)&Firmware[userdata+0x58] = 0;
    *(u16*)&Firmware[userdata+0x5A] = 0;
    Firmware[userdata+0x5C] = 1;
    Firmware[userdata+0x5D] = 1;
    *(u16*)&Firmware[userdata+0x5E] = 254<<4;
    *(u16*)&Firmware[userdata+0x60] = 190<<4;
    Firmware[userdata+0x62] = 255;
    Firmware[userdata+0x63] = 191;

    // disable autoboot
    //Firmware[userdata+0x64] &= 0xBF;

    *(u16*)&Firmware[userdata+0x72] = CRC16(&Firmware[userdata], 0x70, 0xFFFF);

    // verify shit
    printf("FW: WIFI CRC16 = %s\n", VerifyCRC16(0x0000, 0x2C, *(u16*)&Firmware[0x2C], 0x2A)?"GOOD":"BAD");
    printf("FW: AP1 CRC16 = %s\n", VerifyCRC16(0x0000, 0x3FA00, 0xFE, 0x3FAFE)?"GOOD":"BAD");
    printf("FW: AP2 CRC16 = %s\n", VerifyCRC16(0x0000, 0x3FB00, 0xFE, 0x3FBFE)?"GOOD":"BAD");
    printf("FW: AP3 CRC16 = %s\n", VerifyCRC16(0x0000, 0x3FC00, 0xFE, 0x3FCFE)?"GOOD":"BAD");
    printf("FW: USER0 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x3FE00, 0x70, 0x3FE72)?"GOOD":"BAD");
    printf("FW: USER1 CRC16 = %s\n", VerifyCRC16(0xFFFF, 0x3FF00, 0x70, 0x3FF72)?"GOOD":"BAD");

    Hold = 0;
    CurCmd = 0;
    Data = 0;
    StatusReg = 0x00;
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
        CurCmd = val;
        Hold = 1;
        Data = 0;
        DataPos = 1;
        Addr = 0;
        //printf("firmware SPI command %02X\n", CurCmd);
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

                //if (DataPos == 3) printf("firmware SPI read %08X\n", Addr);
            }
            else
            {
                if (Addr >= FirmwareLength)
                    Data = 0;
                else
                    Data = Firmware[Addr];

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
        if (Index & 0x80)
        {
            Data = Registers[Index & 0x07];
        }
        else
        {
            Registers[Index & 0x07] =
                (Registers[Index & 0x07] & ~RegMasks[Index & 0x07]) |
                (val & RegMasks[Index & 0x07]);
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

u32 CurDevice;


bool Init()
{
    if (!SPI_Firmware::Init()) return false;
    if (!SPI_Powerman::Init()) return false;
    if (!SPI_TSC::Init()) return false;

    return true;
}

void DeInit()
{
    SPI_Firmware::DeInit();
    SPI_Powerman::DeInit();
    SPI_TSC::DeInit();
}

void Reset()
{
    Cnt = 0;

    SPI_Firmware::Reset();
    SPI_Powerman::Reset();
    SPI_TSC::Init();
}


void WriteCnt(u16 val)
{
    Cnt = (Cnt & 0x0080) | (val & 0xCF03);
    if (val & 0x0400) printf("!! CRAPOED 16BIT SPI MODE\n");
}

u8 ReadData()
{
    if (!(Cnt & (1<<15))) return 0;

    switch (Cnt & 0x0300)
    {
    case 0x0000: return SPI_Powerman::Read();
    case 0x0100: return SPI_Firmware::Read();
    case 0x0200: return SPI_TSC::Read();
    default: return 0;
    }
}

void WriteData(u8 val)
{
    if (!(Cnt & (1<<15))) return;

    // TODO: take delays into account

    switch (Cnt & 0x0300)
    {
    case 0x0000: SPI_Powerman::Write(val, Cnt&(1<<11)); break;
    case 0x0100: SPI_Firmware::Write(val, Cnt&(1<<11)); break;
    case 0x0200: SPI_TSC::Write(val, Cnt&(1<<11)); break;
    default: printf("SPI to unknown device %04X %02X\n", Cnt, val); break;
    }

    if (Cnt & (1<<14))
        NDS::SetIRQ(1, NDS::IRQ_SPI);
}

}
