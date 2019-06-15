/*
    Copyright 2016-2019 Arisotura

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

#include "NDS.h"
#include "DSi.h"
#include "tiny-AES-c/aes.hpp"
#include "sha1/sha1.h"
#include "Platform.h"


namespace DSi
{

//


bool LoadBIOS()
{
    FILE* f;
    u32 i;

    f = Platform::OpenLocalFile("bios9i.bin", "rb");
    if (!f)
    {
        printf("ARM9i BIOS not found\n");

        for (i = 0; i < 16; i++)
            ((u32*)NDS::ARM9BIOS)[i] = 0xE7FFDEFF;
    }
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(NDS::ARM9BIOS, 0x10000, 1, f);

        printf("ARM9i BIOS loaded\n");
        fclose(f);
    }

    f = Platform::OpenLocalFile("bios7i.bin", "rb");
    if (!f)
    {
        printf("ARM7i BIOS not found\n");

        for (i = 0; i < 16; i++)
            ((u32*)NDS::ARM7BIOS)[i] = 0xE7FFDEFF;
    }
    else
    {
        // TODO: check if the first 32 bytes are crapoed

        fseek(f, 0, SEEK_SET);
        fread(NDS::ARM7BIOS, 0x10000, 1, f);

        printf("ARM7i BIOS loaded\n");
        fclose(f);
    }

    // herp
    *(u32*)&NDS::ARM9BIOS[0] = 0xEAFFFFFE;
    *(u32*)&NDS::ARM7BIOS[0] = 0xEAFFFFFE;

    return true;
}

bool LoadNAND()
{
    printf("Loading DSi NAND\n");

    FILE* f = Platform::OpenLocalFile("nand.bin", "rb");
    if (f)
    {
        u32 bootparams[8];
        fseek(f, 0x220, SEEK_SET);
        fread(bootparams, 4, 8, f);

        printf("ARM9: offset=%08X size=%08X RAM=%08X size_aligned=%08X\n",
               bootparams[0], bootparams[1], bootparams[2], bootparams[3]);
        printf("ARM7: offset=%08X size=%08X RAM=%08X size_aligned=%08X\n",
               bootparams[4], bootparams[5], bootparams[6], bootparams[7]);

#define printhex(str, size) { for (int z = 0; z < (size); z++) printf("%02X", (str)[z]); printf("\n"); }
#define printhex_rev(str, size) { for (int z = (size)-1; z >= 0; z--) printf("%02X", (str)[z]); printf("\n"); }

        u8 emmc_cid[16];
        u8 consoleid[8];
        fseek(f, 0xF000010, SEEK_SET);
        fread(emmc_cid, 1, 16, f);
        fread(consoleid, 1, 8, f);

        printf("eMMC CID: "); printhex(emmc_cid, 16);
        printf("Console ID: "); printhex_rev(consoleid, 8);

        fclose(f);
    }

    return true;
}


u8 ARM9Read8(u32 addr)
{
    switch (addr & 0xFF000000)
    {
    case 0x04000000:
        return ARM9IORead8(addr);
    }

    return NDS::ARM9Read8(addr);
}

u16 ARM9Read16(u32 addr)
{
    switch (addr & 0xFF000000)
    {
    case 0x04000000:
        return ARM9IORead16(addr);
    }

    return NDS::ARM9Read16(addr);
}

u32 ARM9Read32(u32 addr)
{
    switch (addr & 0xFF000000)
    {
    case 0x04000000:
        return ARM9IORead32(addr);
    }

    return NDS::ARM9Read32(addr);
}

void ARM9Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x04000000:
        ARM9IOWrite8(addr, val);
        return;
    }

    return NDS::ARM9Write8(addr, val);
}

void ARM9Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x04000000:
        ARM9IOWrite16(addr, val);
        return;
    }

    return NDS::ARM9Write16(addr, val);
}

void ARM9Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x04000000:
        ARM9IOWrite32(addr, val);
        return;
    }

    return NDS::ARM9Write32(addr, val);
}

bool ARM9GetMemRegion(u32 addr, bool write, NDS::MemRegion* region)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        region->Mem = NDS::MainRAM;
        region->Mask = MAIN_RAM_SIZE-1;
        return true;
    }

    if ((addr & 0xFFFF0000) == 0xFFFF0000 && !write)
    {
        region->Mem = NDS::ARM9BIOS;
        region->Mask = 0xFFFF;
        return true;
    }

    region->Mem = NULL;
    return false;
}



u8 ARM7Read8(u32 addr)
{
    switch (addr & 0xFF800000)
    {
    case 0x04000000:
        return ARM7IORead8(addr);
    }

    return NDS::ARM7Read8(addr);
}

u16 ARM7Read16(u32 addr)
{
    switch (addr & 0xFF800000)
    {
    case 0x04000000:
        return ARM7IORead16(addr);
    }

    return NDS::ARM7Read16(addr);
}

u32 ARM7Read32(u32 addr)
{
    switch (addr & 0xFF800000)
    {
    case 0x04000000:
        return ARM7IORead32(addr);
    }

    return NDS::ARM7Read32(addr);
}

void ARM7Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x04000000:
        ARM7IOWrite8(addr, val);
        return;
    }

    return NDS::ARM7Write8(addr, val);
}

void ARM7Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x04000000:
        ARM7IOWrite16(addr, val);
        return;
    }

    return NDS::ARM7Write16(addr, val);
}

void ARM7Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x04000000:
        ARM7IOWrite32(addr, val);
        return;
    }

    return NDS::ARM7Write32(addr, val);
}

bool ARM7GetMemRegion(u32 addr, bool write, NDS::MemRegion* region)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
        region->Mem = NDS::MainRAM;
        region->Mask = MAIN_RAM_SIZE-1;
        return true;
    }

    // BIOS. ARM7 PC has to be within range.
    /*if (addr < 0x00010000 && !write)
    {
        if (NDS::ARM7->R[15] < 0x00010000 && (addr >= NDS::ARM7BIOSProt || NDS::ARM7->R[15] < NDS::ARM7BIOSProt))
        {
            region->Mem = NDS::ARM7BIOS;
            region->Mask = 0xFFFF;
            return true;
        }
    }*/

    region->Mem = NULL;
    return false;
}




#define CASE_READ8_16BIT(addr, val) \
    case (addr): return (val) & 0xFF; \
    case (addr+1): return (val) >> 8;

#define CASE_READ8_32BIT(addr, val) \
    case (addr): return (val) & 0xFF; \
    case (addr+1): return ((val) >> 8) & 0xFF; \
    case (addr+2): return ((val) >> 16) & 0xFF; \
    case (addr+3): return (val) >> 24;

u8 ARM9IORead8(u32 addr)
{
    switch (addr)
    {
    }

    return NDS::ARM9IORead8(addr);
}

u16 ARM9IORead16(u32 addr)
{
    switch (addr)
    {
    }

    return NDS::ARM9IORead16(addr);
}

u32 ARM9IORead32(u32 addr)
{
    switch (addr)
    {
    }

    return NDS::ARM9IORead32(addr);
}

void ARM9IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    }

    return NDS::ARM9IOWrite8(addr, val);
}

void ARM9IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    }

    return NDS::ARM9IOWrite16(addr, val);
}

void ARM9IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    }

    return NDS::ARM9IOWrite32(addr, val);
}


u8 ARM7IORead8(u32 addr)
{
    switch (addr)
    {
    }

    return NDS::ARM7IORead8(addr);
}

u16 ARM7IORead16(u32 addr)
{
    switch (addr)
    {
    }

    return NDS::ARM7IORead16(addr);
}

u32 ARM7IORead32(u32 addr)
{
    switch (addr)
    {
    }

    return NDS::ARM7IORead32(addr);
}

void ARM7IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    }

    return NDS::ARM7IOWrite8(addr, val);
}

void ARM7IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    }

    return NDS::ARM7IOWrite16(addr, val);
}

void ARM7IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    }

    return NDS::ARM7IOWrite32(addr, val);
}

}
