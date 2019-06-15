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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "DSi.h"
#include "ARM.h"
#include "tiny-AES-c/aes.hpp"
#include "sha1/sha1.h"
#include "Platform.h"


namespace NDS
{

extern ARMv5* ARM9;
extern ARMv4* ARM7;

}


namespace DSi
{

u32 BootAddr[2];

u32 MBK[2][9];

u8 NWRAM_A[0x40000];
u8 NWRAM_B[0x40000];
u8 NWRAM_C[0x40000];

u8* NWRAMMap_A[2][4];
u8* NWRAMMap_B[3][4];
u8* NWRAMMap_C[3][4];

u32 NWRAMStart[2][3];
u32 NWRAMEnd[2][3];
u32 NWRAMMask[2][3];


void Reset()
{
    NDS::ARM9->JumpTo(BootAddr[0]);
    NDS::ARM7->JumpTo(BootAddr[1]);
}

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

    memset(NWRAM_A, 0, 0x40000);
    memset(NWRAM_B, 0, 0x40000);
    memset(NWRAM_C, 0, 0x40000);

    memset(MBK, 0, sizeof(MBK));
    memset(NWRAMMap_A, 0, sizeof(NWRAMMap_A));
    memset(NWRAMMap_B, 0, sizeof(NWRAMMap_B));
    memset(NWRAMMap_C, 0, sizeof(NWRAMMap_C));
    memset(NWRAMStart, 0, sizeof(NWRAMStart));
    memset(NWRAMEnd, 0, sizeof(NWRAMEnd));
    memset(NWRAMMask, 0, sizeof(NWRAMMask));

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

        // read and apply new-WRAM settings

        u32 mbk[12];
        fseek(f, 0x380, SEEK_SET);
        fread(mbk, 4, 12, f);

        MapNWRAM_A(0, mbk[0] & 0xFF);
        MapNWRAM_A(1, (mbk[0] >> 8) & 0xFF);
        MapNWRAM_A(2, (mbk[0] >> 16) & 0xFF);
        MapNWRAM_A(3, mbk[0] >> 24);

        MapNWRAM_B(0, mbk[1] & 0xFF);
        MapNWRAM_B(1, (mbk[1] >> 8) & 0xFF);
        MapNWRAM_B(2, (mbk[1] >> 16) & 0xFF);
        MapNWRAM_B(3, mbk[1] >> 24);
        MapNWRAM_B(4, mbk[2] & 0xFF);
        MapNWRAM_B(5, (mbk[2] >> 8) & 0xFF);
        MapNWRAM_B(6, (mbk[2] >> 16) & 0xFF);
        MapNWRAM_B(7, mbk[2] >> 24);

        MapNWRAM_C(0, mbk[3] & 0xFF);
        MapNWRAM_C(1, (mbk[3] >> 8) & 0xFF);
        MapNWRAM_C(2, (mbk[3] >> 16) & 0xFF);
        MapNWRAM_C(3, mbk[3] >> 24);
        MapNWRAM_C(4, mbk[4] & 0xFF);
        MapNWRAM_C(5, (mbk[4] >> 8) & 0xFF);
        MapNWRAM_C(6, (mbk[4] >> 16) & 0xFF);
        MapNWRAM_C(7, mbk[4] >> 24);

        MapNWRAMRange(0, 0, mbk[5]);
        MapNWRAMRange(0, 1, mbk[6]);
        MapNWRAMRange(0, 2, mbk[7]);

        MapNWRAMRange(1, 0, mbk[8]);
        MapNWRAMRange(1, 1, mbk[9]);
        MapNWRAMRange(1, 2, mbk[10]);

        // TODO: MBK9 protect thing

        // load binaries
        // TODO: optionally support loading from actual NAND?
        // currently decrypted binaries have to be provided
        // they can be decrypted with twltool

        FILE* bin;

        bin = Platform::OpenLocalFile("boot2_9.bin", "rb");
        if (bin)
        {
            u32 dstaddr = bootparams[2];
            for (u32 i = 0; i < bootparams[1]; i += 4)
            {
                u32 _tmp;
                fread(&_tmp, 4, 1, bin);
                ARM9Write32(dstaddr, _tmp);
                dstaddr += 4;
            }

            fclose(bin);
        }
        else
        {
            printf("ARM9 boot2 not found\n");
        }

        bin = Platform::OpenLocalFile("boot2_7.bin", "rb");
        if (bin)
        {
            u32 dstaddr = bootparams[6];
            for (u32 i = 0; i < bootparams[5]; i += 4)
            {
                u32 _tmp;
                fread(&_tmp, 4, 1, bin);
                ARM7Write32(dstaddr, _tmp);
                dstaddr += 4;
            }

            fclose(bin);
        }
        else
        {
            printf("ARM7 boot2 not found\n");
        }

        // repoint CPUs to the boot2 binaries

        BootAddr[0] = bootparams[2];
        BootAddr[1] = bootparams[6];

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


// new WRAM mapping
// TODO: find out what happens upon overlapping slots!!

void MapNWRAM_A(u32 num, u8 val)
{
    int mbkn = 0, mbks = 8*num;

    u8 oldval = (MBK[0][mbkn] >> mbks) & 0xFF;
    if (oldval == val) return;

    MBK[0][mbkn] &= ~(0xFF << mbks);
    MBK[0][mbkn] |= (val << mbks);
    MBK[1][mbkn] = MBK[0][mbkn];

    u8* ptr = &NWRAM_A[num << 16];

    if (oldval & 0x80)
    {
        if (NWRAMMap_A[oldval & 0x01][(oldval >> 2) & 0x3] == ptr)
            NWRAMMap_A[oldval & 0x01][(oldval >> 2) & 0x3] = NULL;
    }

    if (val & 0x80)
    {
        NWRAMMap_A[val & 0x01][(val >> 2) & 0x3] = ptr;
    }
}

void MapNWRAM_B(u32 num, u8 val)
{
    int mbkn = 1+(num>>2), mbks = 8*(num&3);

    u8 oldval = (MBK[0][mbkn] >> mbks) & 0xFF;
    if (oldval == val) return;

    MBK[0][mbkn] &= ~(0xFF << mbks);
    MBK[0][mbkn] |= (val << mbks);
    MBK[1][mbkn] = MBK[0][mbkn];

    u8* ptr = &NWRAM_B[num << 15];

    if (oldval & 0x80)
    {
        if (oldval & 0x02) oldval &= 0xFE;

        if (NWRAMMap_B[oldval & 0x03][(oldval >> 2) & 0x7] == ptr)
            NWRAMMap_B[oldval & 0x03][(oldval >> 2) & 0x7] = NULL;
    }

    if (val & 0x80)
    {
        if (val & 0x02) val &= 0xFE;

        NWRAMMap_B[val & 0x03][(val >> 2) & 0x7] = ptr;
    }
}

void MapNWRAM_C(u32 num, u8 val)
{
    int mbkn = 3+(num>>2), mbks = 8*(num&3);

    u8 oldval = (MBK[0][mbkn] >> mbks) & 0xFF;
    if (oldval == val) return;

    MBK[0][mbkn] &= ~(0xFF << mbks);
    MBK[0][mbkn] |= (val << mbks);
    MBK[1][mbkn] = MBK[0][mbkn];

    u8* ptr = &NWRAM_C[num << 15];

    if (oldval & 0x80)
    {
        if (oldval & 0x02) oldval &= 0xFE;

        if (NWRAMMap_C[oldval & 0x03][(oldval >> 2) & 0x7] == ptr)
            NWRAMMap_C[oldval & 0x03][(oldval >> 2) & 0x7] = NULL;
    }

    if (val & 0x80)
    {
        if (val & 0x02) val &= 0xFE;

        NWRAMMap_C[val & 0x03][(val >> 2) & 0x7] = ptr;
    }
}

void MapNWRAMRange(u32 cpu, u32 num, u32 val)
{
    u32 oldval = MBK[cpu][5+num];
    if (oldval == val) return;

    MBK[cpu][5+num] = val;

    // TODO: what happens when the ranges are 'out of range'????
    if (num == 0)
    {
        u32 start = 0x03000000 + (((val >> 4) & 0xFF) << 16);
        u32 end   = 0x03000000 + (((val >> 20) & 0x1FF) << 16);
        u32 size  = (val >> 12) & 0x3;

        printf("NWRAM-A: ARM%d range %08X-%08X, size %d\n", cpu?7:9, start, end, size);

        NWRAMStart[cpu][num] = start;
        NWRAMEnd[cpu][num] = end;

        switch (size)
        {
        case 0:
        case 1: NWRAMMask[cpu][num] = 0x0; break;
        case 2: NWRAMMask[cpu][num] = 0x1; break; // CHECKME
        case 3: NWRAMMask[cpu][num] = 0x3; break;
        }
    }
    else
    {
        u32 start = 0x03000000 + (((val >> 3) & 0x1FF) << 15);
        u32 end   = 0x03000000 + (((val >> 19) & 0x3FF) << 15);
        u32 size  = (val >> 12) & 0x3;

        printf("NWRAM-%c: ARM%d range %08X-%08X, size %d\n", 'A'+num, cpu?7:9, start, end, size);

        NWRAMStart[cpu][num] = start;
        NWRAMEnd[cpu][num] = end;

        switch (size)
        {
        case 0: NWRAMMask[cpu][num] = 0x0; break;
        case 1: NWRAMMask[cpu][num] = 0x1; break;
        case 2: NWRAMMask[cpu][num] = 0x3; break;
        case 3: NWRAMMask[cpu][num] = 0x7; break;
        }
    }
}


u8 ARM9Read8(u32 addr)
{
    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
        {
            u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
            return ptr ? *(u8*)&ptr[addr & 0xFFFF] : 0;
        }
        if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
        {
            u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
            return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
        }
        if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
        {
            u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
            return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
        }
        return NDS::ARM9Read8(addr);

    case 0x04000000:
        return ARM9IORead8(addr);
    }

    return NDS::ARM9Read8(addr);
}

u16 ARM9Read16(u32 addr)
{
    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
        {
            u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
            return ptr ? *(u16*)&ptr[addr & 0xFFFF] : 0;
        }
        if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
        {
            u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
            return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
        }
        if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
        {
            u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
            return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
        }
        return NDS::ARM9Read16(addr);

    case 0x04000000:
        return ARM9IORead16(addr);
    }

    return NDS::ARM9Read16(addr);
}

u32 ARM9Read32(u32 addr)
{
    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
        {
            u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
            return ptr ? *(u32*)&ptr[addr & 0xFFFF] : 0;
        }
        if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
        {
            u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
            return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
        }
        if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
        {
            u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
            return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
        }
        return NDS::ARM9Read32(addr);

    case 0x04000000:
        return ARM9IORead32(addr);
    }

    return NDS::ARM9Read32(addr);
}

void ARM9Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
        {
            u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
            if (ptr) *(u8*)&ptr[addr & 0xFFFF] = val;
        }
        if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
        {
            u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
            if (ptr) *(u8*)&ptr[addr & 0x7FFF] = val;
        }
        if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
        {
            u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
            if (ptr) *(u8*)&ptr[addr & 0x7FFF] = val;
        }
        return NDS::ARM9Write8(addr, val);

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
    case 0x03000000:
        if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
        {
            u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
            if (ptr) *(u16*)&ptr[addr & 0xFFFF] = val;
        }
        if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
        {
            u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
            if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
        }
        if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
        {
            u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
            if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
        }
        return NDS::ARM9Write16(addr, val);

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
    case 0x03000000:
        if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
        {
            u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
            if (ptr) *(u32*)&ptr[addr & 0xFFFF] = val;
        }
        if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
        {
            u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
            if (ptr) *(u32*)&ptr[addr & 0x7FFF] = val;
        }
        if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
        {
            u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
            if (ptr) *(u32*)&ptr[addr & 0x7FFF] = val;
        }
        return NDS::ARM9Write32(addr, val);

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
    case 0x03000000:
        if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
        {
            u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
            return ptr ? *(u8*)&ptr[addr & 0xFFFF] : 0;
        }
        if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
        {
            u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
            return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
        }
        if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
        {
            u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
            return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
        }
        return NDS::ARM7Read8(addr);

    case 0x04000000:
        return ARM7IORead8(addr);
    }

    return NDS::ARM7Read8(addr);
}

u16 ARM7Read16(u32 addr)
{
    switch (addr & 0xFF800000)
    {
    case 0x03000000:
        if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
        {
            u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
            return ptr ? *(u16*)&ptr[addr & 0xFFFF] : 0;
        }
        if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
        {
            u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
            return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
        }
        if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
        {
            u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
            return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
        }
        return NDS::ARM7Read16(addr);

    case 0x04000000:
        return ARM7IORead16(addr);
    }

    return NDS::ARM7Read16(addr);
}

u32 ARM7Read32(u32 addr)
{
    switch (addr & 0xFF800000)
    {
    case 0x03000000:
        if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
        {
            u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
            return ptr ? *(u32*)&ptr[addr & 0xFFFF] : 0;
        }
        if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
        {
            u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
            return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
        }
        if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
        {
            u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
            return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
        }
        return NDS::ARM7Read32(addr);

    case 0x04000000:
        return ARM7IORead32(addr);
    }

    return NDS::ARM7Read32(addr);
}

void ARM7Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x03000000:
        if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
        {
            u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
            if (ptr) *(u8*)&ptr[addr & 0xFFFF] = val;
        }
        if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
        {
            u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
            if (ptr) *(u8*)&ptr[addr & 0x7FFF] = val;
        }
        if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
        {
            u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
            if (ptr) *(u8*)&ptr[addr & 0x7FFF] = val;
        }
        return NDS::ARM7Write8(addr, val);

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
    case 0x03000000:
        if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
        {
            u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
            if (ptr) *(u16*)&ptr[addr & 0xFFFF] = val;
        }
        if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
        {
            u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
            if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
        }
        if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
        {
            u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
            if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
        }
        return NDS::ARM7Write16(addr, val);

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
    case 0x03000000:
        if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
        {
            u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
            if (ptr) *(u32*)&ptr[addr & 0xFFFF] = val;
        }
        if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
        {
            u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
            if (ptr) *(u32*)&ptr[addr & 0x7FFF] = val;
        }
        if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
        {
            u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
            if (ptr) *(u32*)&ptr[addr & 0x7FFF] = val;
        }
        return NDS::ARM7Write32(addr, val);

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
