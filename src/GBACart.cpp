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

u8* SRAM;
u32 SRAMLength;

char SRAMPath[1024];


bool Init()
{
    SRAM = NULL;
    return true;
}

void DeInit()
{
    if (SRAM) delete[] SRAM;
}

void Reset()
{
    if (SRAM) delete[] SRAM;
    SRAM = NULL;
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

    FILE* f = Platform::OpenFile(path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        SRAMLength = (u32)ftell(f);
        SRAM = new u8[SRAMLength];

        fseek(f, 0, SEEK_SET);
        fread(SRAM, SRAMLength, 1, f);

        fclose(f);
    }
    else
    {
        int SRAMLength = 65536; // max GBA SRAM size

        SRAM = new u8[SRAMLength];
        memset(SRAM, 0xFF, SRAMLength);
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

    FILE* f = Platform::OpenFile(path, "wb");
    if (!f)
    {
        printf("GBACart_SRAM::RelocateSave: failed to create new file. fuck\n");
        return;
    }

    fwrite(SRAM, SRAMLength, 1, f);
    fclose(f);
}

void Write8(u32 addr, u8 val)
{
    u8 prev = *(u8*)&SRAM[addr];

    if (prev != val)
    {
        *(u8*)&SRAM[addr] = val;/*

        FILE* f = Platform::OpenFile(SRAMPath, "r+b");
        if (f)
        {
            fseek(f, addr, SEEK_SET);
            fwrite((u8*)&SRAM[addr], 1, 1, f);
            fclose(f);
        }*/
    }
}

void Write16(u32 addr, u16 val)
{
    u16 prev = *(u16*)&SRAM[addr];

    if (prev != val)
    {
        *(u16*)&SRAM[addr] = val;/*

        FILE* f = Platform::OpenFile(SRAMPath, "r+b");
        if (f)
        {
            fseek(f, addr, SEEK_SET);
            fwrite((u8*)&SRAM[addr], 2, 1, f);
            fclose(f);
        }*/
    }
}

void Write32(u32 addr, u32 val)
{
    u32 prev = *(u32*)&SRAM[addr];

    if (prev != val)
    {
        *(u32*)&SRAM[addr] = val;/*

        FILE* f = Platform::OpenFile(SRAMPath, "r+b");
        if (f)
        {
            fseek(f, addr, SEEK_SET);
            fwrite((u8*)&SRAM[addr], 3, 1, f);
            fclose(f);
        }*/
    }
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
    //CartROM = f;

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
