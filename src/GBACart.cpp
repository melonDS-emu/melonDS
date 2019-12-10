/*
    Copyright 2019 Arisotura, Raphaël Zumer

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
#include "GBACart.h"
#include "CRC32.h"
#include "Platform.h"


namespace GBACart_SRAM
{


enum SaveType {
    S_NULL,
    S_EEPROM4K,
    S_EEPROM64K,
    S_SRAM256K,
    S_FLASH512K,
    S_FLASH1M
};

// from DeSmuME
struct FlashProperties
{
    u8 state;
    u8 cmd;
    u8 device;
    u8 manufacturer;
    u8 bank;
};

u8* SRAM;
FILE* SRAMFile;
u32 SRAMLength;
SaveType SRAMType;
FlashProperties SRAMFlash;

char SRAMPath[1024];

void (*WriteFunc)(u32 addr, u8 val);


void Write_Null(u32 addr, u8 val);
void Write_EEPROM(u32 addr, u8 val);
void Write_SRAM(u32 addr, u8 val);
void Write_Flash(u32 addr, u8 val);


bool Init()
{
    SRAM = NULL;
    SRAMFile = NULL;
    return true;
}

void DeInit()
{
    if (SRAMFile) fclose(SRAMFile);
    if (SRAM) delete[] SRAM;
}

void Reset()
{
    if (SRAMFile) fclose(SRAMFile);
    if (SRAM) delete[] SRAM;

    SRAM = NULL;
    SRAMFile = NULL;
    SRAMLength = 0;
    SRAMType = S_NULL;
    SRAMFlash = {};
}

void DoSavestate(Savestate* file)
{
    // TODO?
}

void LoadSave(const char* path)
{
    if (SRAM) delete[] SRAM;

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';
    SRAMLength = 0;

    FILE* f = Platform::OpenFile(SRAMPath, "r+b");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        SRAMLength = (u32)ftell(f);
        SRAM = new u8[SRAMLength];

        fseek(f, 0, SEEK_SET);
        fread(SRAM, SRAMLength, 1, f);

        SRAMFile = f;
    }

    switch (SRAMLength)
    {
    case 512:
        SRAMType = S_EEPROM4K;
        WriteFunc = Write_EEPROM;
        break;
    case 8192:
        SRAMType = S_EEPROM64K;
        WriteFunc = Write_EEPROM;
        break;
    case 32768:
        SRAMType = S_SRAM256K;
        WriteFunc = Write_SRAM;
        break;
    case 65536:
        SRAMType = S_FLASH512K;
        WriteFunc = Write_Flash;
        break;
    case 128*1024:
        SRAMType = S_FLASH1M;
        WriteFunc = Write_Flash;
        break;
    default:
        printf("!! BAD SAVE LENGTH %d\n", SRAMLength);
    case 0:
        SRAMType = S_NULL;
        WriteFunc = Write_Null;
        break;
    }

    if (SRAMType == S_FLASH512K)
    {
        // Panasonic 64K chip
        SRAMFlash.device = 0x1B;
        SRAMFlash.manufacturer = 0x32;
    }
    else if (SRAMType == S_FLASH1M)
    {
        // Sanyo 128K chip
        SRAMFlash.device = 0x13;
        SRAMFlash.manufacturer = 0x62;
    }
}

void RelocateSave(const char* path, bool write)
{
    if (!write)
    {
        LoadSave(path); // lazy
        return;
    }

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    FILE *f = Platform::OpenFile(path, "r+b");
    if (!f)
    {
        printf("GBACart_SRAM::RelocateSave: failed to create new file. fuck\n");
        return;
    }

    SRAMFile = f;
    fwrite(SRAM, SRAMLength, 1, SRAMFile);
}

// mostly ported from DeSmuME
u8 Read_Flash(u32 addr)
{
    if (SRAMFlash.cmd == 0) // no cmd
    {
        return *(u8*)&SRAM[addr + 0x10000 * SRAMFlash.bank];
    }

    switch (SRAMFlash.cmd)
    {
        case 0x90: // chip ID
            if (addr == 0x0000) return SRAMFlash.manufacturer;
            if (addr == 0x0001) return SRAMFlash.device;
            break;
        case 0xF0: // terminate command (TODO: break if non-Macronix chip and not at the end of an ID call?)
            SRAMFlash.state = 0;
            SRAMFlash.cmd = 0;
            break;
        case 0xA0: // write command
            break; // ignore here, handled in Write_Flash()
        case 0xB0: // bank switching (128K only)
            break; // ignore here, handled in Write_Flash()
        default:
            printf("GBACart_SRAM::Read_Flash: unknown command 0x%02X @ 0x%04X\n", SRAMFlash.cmd, addr);
            break;
    }

    return 0xFF;
}

void Write_Null(u32 addr, u8 val) {}

void Write_EEPROM(u32 addr, u8 val)
{
    // TODO: could be used in homebrew?
}

// mostly ported from DeSmuME
void Write_Flash(u32 addr, u8 val)
{
    switch (SRAMFlash.state)
    {
        case 0x00:
            if (addr == 0x5555)
            {
                if (val == 0xF0)
                {
                    // reset
                    SRAMFlash.state = 0;
                    SRAMFlash.cmd = 0;
                    return;
                }
                else if (val == 0xAA)
                {
                    SRAMFlash.state = 1;
                    return;
                }
            }
            if (addr == 0x0000)
            {
                if (SRAMFlash.cmd == 0xB0)
                {
                    // bank switching
                    SRAMFlash.bank = val;
                    SRAMFlash.cmd = 0;
                    return;
                }
            }
            break;
        case 0x01:
            if (addr == 0x2AAA && val == 0x55)
            {
                SRAMFlash.state = 2;
                return;
            }
            SRAMFlash.state = 0;
            break;
        case 0x02:
            if (addr == 0x5555)
            {
                // send command
                switch (val)
                {
                    case 0x80: // erase
                        SRAMFlash.state = 0x80;
                        break;
                    case 0x90: // chip ID
                        SRAMFlash.state = 0x90;
                        break;
                    case 0xA0: // write
                        SRAMFlash.state = 0;
                        break;
                    default:
                        SRAMFlash.state = 0;
                        break;
                }

                SRAMFlash.cmd = val;
                return;
            }
            SRAMFlash.state = 0;
            break;
        // erase
        case 0x80:
            if (addr == 0x5555 && val == 0xAA)
            {
                SRAMFlash.state = 0x81;
                return;
            }
            SRAMFlash.state = 0;
            break;
        case 0x81:
            if (addr == 0x2AAA && val == 0x55)
            {
                SRAMFlash.state = 0x82;
                return;
            }
            SRAMFlash.state = 0;
            break;
        case 0x82:
            if (val == 0x30)
            {
                u32 start_addr = addr + 0x10000 * SRAMFlash.bank;
                memset((u8*)&SRAM[start_addr], 0xFF, 0x1000);

                if (SRAMFile)
                {
                    fseek(SRAMFile, start_addr, SEEK_SET);
                    fwrite((u8*)&SRAM[start_addr], 1, 0x1000, SRAMFile);
                }
            }
            SRAMFlash.state = 0;
            SRAMFlash.cmd = 0;
            return;
        // chip ID
        case 0x90:
            if (addr == 0x5555 && val == 0xAA)
            {
                SRAMFlash.state = 0x91;
                return;
            }
            SRAMFlash.state = 0;
            break;
        case 0x91:
            if (addr == 0x2AAA && val == 0x55)
            {
                SRAMFlash.state = 0x92;
                return;
            }
            SRAMFlash.state = 0;
            break;
        case 0x92:
            SRAMFlash.state = 0;
            SRAMFlash.cmd = 0;
            return;
        default:
            break;
    }

    if (SRAMFlash.cmd == 0xA0) // write
    {
        Write_SRAM(addr + 0x10000 * SRAMFlash.bank, val);
        SRAMFlash.state = 0;
        SRAMFlash.cmd = 0;
        return;
    }

    printf("GBACart_SRAM::Write_Flash: unknown write 0x%02X @ 0x%04X (state: 0x%02X)\n",
        val, addr, SRAMFlash.state);
}

void Write_SRAM(u32 addr, u8 val)
{
    u8 prev = *(u8*)&SRAM[addr];

    if (prev != val)
    {
        *(u8*)&SRAM[addr] = val;

        if (SRAMFile)
        {
            fseek(SRAMFile, addr, SEEK_SET);
            fwrite((u8*)&SRAM[addr], 1, 1, SRAMFile);
        }
    }
}

u8 Read8(u32 addr)
{
    if (SRAMType == S_NULL)
    {
        return 0xFF;
    }

    if (SRAMType == S_FLASH512K || SRAMType == S_FLASH1M)
    {
        return Read_Flash(addr);
    }

    return *(u8*)&SRAM[addr];
}

u16 Read16(u32 addr)
{
    if (SRAMType == S_NULL)
    {
        return 0xFFFF;
    }

    if (SRAMType == S_FLASH512K || SRAMType == S_FLASH1M)
    {
        u16 val = Read_Flash(addr + 0) |
            (Read_Flash(addr + 1) << 8);
        return val;
    }

    return *(u16*)&SRAM[addr];
}

u32 Read32(u32 addr)
{
    if (SRAMType == S_NULL)
    {
        return 0xFFFFFFFF;
    }

    if (SRAMType == S_FLASH512K || SRAMType == S_FLASH1M)
    {
        u32 val = Read_Flash(addr + 0) |
            (Read_Flash(addr + 1) << 8) |
            (Read_Flash(addr + 2) << 16) |
            (Read_Flash(addr + 3) << 24);
        return val;
    }

    return *(u32*)&SRAM[addr];
}

void Write8(u32 addr, u8 val)
{
    u8 prev = *(u8*)&SRAM[addr];

    WriteFunc(addr, val);
}

void Write16(u32 addr, u16 val)
{
    u16 prev = *(u16*)&SRAM[addr];

    WriteFunc(addr + 0, val & 0xFF);
    WriteFunc(addr + 1, val >> 8 & 0xFF);
}

void Write32(u32 addr, u32 val)
{
    u32 prev = *(u32*)&SRAM[addr];

    WriteFunc(addr + 0, val & 0xFF);
    WriteFunc(addr + 1, val >> 8 & 0xFF);
    WriteFunc(addr + 2, val >> 16 & 0xFF);
    WriteFunc(addr + 3, val >> 24 & 0xFF);
}

}


namespace GBACart
{

bool CartInserted;
u8* CartROM;
u32 CartROMSize;
u32 CartCRC;
u32 CartID;


bool Init()
{
    if (!GBACart_SRAM::Init()) return false;

    CartROM = NULL;

    return true;
}

void DeInit()
{
    if (CartROM) delete[] CartROM;

    GBACart_SRAM::DeInit();
}

void Reset()
{
    CartInserted = false;
    if (CartROM) delete[] CartROM;
    CartROM = NULL;
    CartROMSize = 0;

    GBACart_SRAM::Reset();
}

void DoSavestate(Savestate* file)
{
    // TODO?
}

bool LoadROM(const char* path, const char* sram)
{
    FILE* f = Platform::OpenFile(path, "rb");
    if (!f)
    {
        return false;
    }

    if (CartInserted)
    {
        Reset();
    }

    fseek(f, 0, SEEK_END);
    u32 len = (u32)ftell(f);

    CartROMSize = 0x200;
    while (CartROMSize < len)
        CartROMSize <<= 1;

    u32 gamecode;
    fseek(f, 0xAC, SEEK_SET);
    fread(&gamecode, 4, 1, f);
    printf("Game code: %c%c%c%c\n", gamecode&0xFF, (gamecode>>8)&0xFF, (gamecode>>16)&0xFF, gamecode>>24);

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    fseek(f, 0, SEEK_SET);
    fread(CartROM, 1, len, f);

    fclose(f);

    CartCRC = CRC32(CartROM, CartROMSize);
    printf("ROM CRC32: %08X\n", CartCRC);

    CartInserted = true;

    // save
    printf("Save file: %s\n", sram);
    GBACart_SRAM::LoadSave(sram);

    return true;
}

void RelocateSave(const char* path, bool write)
{
    // derp herp
    GBACart_SRAM::RelocateSave(path, write);
}

}
