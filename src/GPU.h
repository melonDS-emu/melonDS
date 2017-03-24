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

#ifndef GPU_H
#define GPU_H

#include "GPU2D.h"
#include "GPU3D.h"

namespace GPU
{

extern u16 VCount;

extern u16 DispStat[2];

extern u8 VRAMCNT[9];
extern u8 VRAMSTAT;

extern u8 Palette[2*1024];
extern u8 OAM[2*1024];

extern u8 VRAM_A[128*1024];
extern u8 VRAM_B[128*1024];
extern u8 VRAM_C[128*1024];
extern u8 VRAM_D[128*1024];
extern u8 VRAM_E[ 64*1024];
extern u8 VRAM_F[ 16*1024];
extern u8 VRAM_G[ 16*1024];
extern u8 VRAM_H[ 32*1024];
extern u8 VRAM_I[ 16*1024];

extern u8* VRAM[9];

extern u32 VRAMMap_LCDC;
extern u32 VRAMMap_ABG[0x20];
extern u32 VRAMMap_AOBJ[0x10];
extern u32 VRAMMap_BBG[0x8];
extern u32 VRAMMap_BOBJ[0x8];
extern u32 VRAMMap_ABGExtPal[4];
extern u32 VRAMMap_AOBJExtPal;
extern u32 VRAMMap_BBGExtPal[4];
extern u32 VRAMMap_BOBJExtPal;
extern u32 VRAMMap_Texture[4];
extern u32 VRAMMap_TexPal[8];
extern u32 VRAMMap_ARM7[2];

extern u32 Framebuffer[256*192*2];

extern GPU2D* GPU2D_A;
extern GPU2D* GPU2D_B;


bool Init();
void DeInit();
void Reset();

void MapVRAM_AB(u32 bank, u8 cnt);
void MapVRAM_CD(u32 bank, u8 cnt);
void MapVRAM_E(u32 bank, u8 cnt);
void MapVRAM_FG(u32 bank, u8 cnt);
void MapVRAM_H(u32 bank, u8 cnt);
void MapVRAM_I(u32 bank, u8 cnt);


template<typename T>
T ReadVRAM_LCDC(u32 addr)
{
    int bank;

    switch (addr & 0xFF8FC000)
    {
    case 0x06800000: case 0x06804000: case 0x06808000: case 0x0680C000:
    case 0x06810000: case 0x06814000: case 0x06818000: case 0x0681C000:
        bank = 0;
        addr &= 0x1FFFF;
        break;

    case 0x06820000: case 0x06824000: case 0x06828000: case 0x0682C000:
    case 0x06830000: case 0x06834000: case 0x06838000: case 0x0683C000:
        bank = 1;
        addr &= 0x1FFFF;
        break;

    case 0x06840000: case 0x06844000: case 0x06848000: case 0x0684C000:
    case 0x06850000: case 0x06854000: case 0x06858000: case 0x0685C000:
        bank = 2;
        addr &= 0x1FFFF;
        break;

    case 0x06860000: case 0x06864000: case 0x06868000: case 0x0686C000:
    case 0x06870000: case 0x06874000: case 0x06878000: case 0x0687C000:
        bank = 3;
        addr &= 0x1FFFF;
        break;

    case 0x06880000: case 0x06884000: case 0x06888000: case 0x0688C000:
        bank = 4;
        addr &= 0xFFFF;
        break;

    case 0x06890000:
        bank = 5;
        addr &= 0x3FFF;
        break;

    case 0x06894000:
        bank = 6;
        addr &= 0x3FFF;
        break;

    case 0x06898000:
    case 0x0689C000:
        bank = 7;
        addr &= 0x7FFF;
        break;

    case 0x068A0000:
        bank = 8;
        addr &= 0x3FFF;
        break;

    default: return 0;
    }

    if (VRAMMap_LCDC & (1<<bank)) return *(T*)&VRAM[bank][addr];

    return 0;
}

template<typename T>
void WriteVRAM_LCDC(u32 addr, T val)
{
    int bank;

    switch (addr & 0xFF8FC000)
    {
    case 0x06800000: case 0x06804000: case 0x06808000: case 0x0680C000:
    case 0x06810000: case 0x06814000: case 0x06818000: case 0x0681C000:
        bank = 0;
        addr &= 0x1FFFF;
        break;

    case 0x06820000: case 0x06824000: case 0x06828000: case 0x0682C000:
    case 0x06830000: case 0x06834000: case 0x06838000: case 0x0683C000:
        bank = 1;
        addr &= 0x1FFFF;
        break;

    case 0x06840000: case 0x06844000: case 0x06848000: case 0x0684C000:
    case 0x06850000: case 0x06854000: case 0x06858000: case 0x0685C000:
        bank = 2;
        addr &= 0x1FFFF;
        break;

    case 0x06860000: case 0x06864000: case 0x06868000: case 0x0686C000:
    case 0x06870000: case 0x06874000: case 0x06878000: case 0x0687C000:
        bank = 3;
        addr &= 0x1FFFF;
        break;

    case 0x06880000: case 0x06884000: case 0x06888000: case 0x0688C000:
        bank = 4;
        addr &= 0xFFFF;
        break;

    case 0x06890000:
        bank = 5;
        addr &= 0x3FFF;
        break;

    case 0x06894000:
        bank = 6;
        addr &= 0x3FFF;
        break;

    case 0x06898000:
    case 0x0689C000:
        bank = 7;
        addr &= 0x7FFF;
        break;

    case 0x068A0000:
        bank = 8;
        addr &= 0x3FFF;
        break;

    default: return;
    }

    if (VRAMMap_LCDC & (1<<bank)) *(T*)&VRAM[bank][addr] = val;
}


template<typename T>
T ReadVRAM_ABG(u32 addr)
{
    u32 ret = 0;
    u32 mask = VRAMMap_ABG[(addr >> 14) & 0x1F];

    if (mask & (1<<0)) ret |= *(T*)&VRAM_A[addr & 0x1FFFF];
    if (mask & (1<<1)) ret |= *(T*)&VRAM_B[addr & 0x1FFFF];
    if (mask & (1<<2)) ret |= *(T*)&VRAM_C[addr & 0x1FFFF];
    if (mask & (1<<3)) ret |= *(T*)&VRAM_D[addr & 0x1FFFF];
    if (mask & (1<<4)) ret |= *(T*)&VRAM_E[addr & 0xFFFF];
    if (mask & (1<<5)) ret |= *(T*)&VRAM_F[addr & 0x3FFF];
    if (mask & (1<<6)) ret |= *(T*)&VRAM_G[addr & 0x3FFF];

    return ret;
}

template<typename T>
void WriteVRAM_ABG(u32 addr, T val)
{
    u32 mask = VRAMMap_ABG[(addr >> 14) & 0x1F];

    if (mask & (1<<0)) *(T*)&VRAM_A[addr & 0x1FFFF] = val;
    if (mask & (1<<1)) *(T*)&VRAM_B[addr & 0x1FFFF] = val;
    if (mask & (1<<2)) *(T*)&VRAM_C[addr & 0x1FFFF] = val;
    if (mask & (1<<3)) *(T*)&VRAM_D[addr & 0x1FFFF] = val;
    if (mask & (1<<4)) *(T*)&VRAM_E[addr & 0xFFFF] = val;
    if (mask & (1<<5)) *(T*)&VRAM_F[addr & 0x3FFF] = val;
    if (mask & (1<<6)) *(T*)&VRAM_G[addr & 0x3FFF] = val;
}


template<typename T>
T ReadVRAM_AOBJ(u32 addr)
{
    u32 ret = 0;
    u32 mask = VRAMMap_AOBJ[(addr >> 14) & 0xF];

    if (mask & (1<<0)) ret |= *(T*)&VRAM_A[addr & 0x1FFFF];
    if (mask & (1<<1)) ret |= *(T*)&VRAM_B[addr & 0x1FFFF];
    if (mask & (1<<4)) ret |= *(T*)&VRAM_E[addr & 0xFFFF];
    if (mask & (1<<5)) ret |= *(T*)&VRAM_F[addr & 0x3FFF];
    if (mask & (1<<6)) ret |= *(T*)&VRAM_G[addr & 0x3FFF];

    return ret;
}

template<typename T>
void WriteVRAM_AOBJ(u32 addr, T val)
{
    u32 mask = VRAMMap_AOBJ[(addr >> 14) & 0xF];

    if (mask & (1<<0)) *(T*)&VRAM_A[addr & 0x1FFFF] = val;
    if (mask & (1<<1)) *(T*)&VRAM_B[addr & 0x1FFFF] = val;
    if (mask & (1<<4)) *(T*)&VRAM_E[addr & 0xFFFF] = val;
    if (mask & (1<<5)) *(T*)&VRAM_F[addr & 0x3FFF] = val;
    if (mask & (1<<6)) *(T*)&VRAM_G[addr & 0x3FFF] = val;
}


template<typename T>
T ReadVRAM_BBG(u32 addr)
{
    u32 ret = 0;
    u32 mask = VRAMMap_BBG[(addr >> 14) & 0x7];

    if (mask & (1<<2)) ret |= *(T*)&VRAM_C[addr & 0x1FFFF];
    if (mask & (1<<7)) ret |= *(T*)&VRAM_H[addr & 0x7FFF];
    if (mask & (1<<8)) ret |= *(T*)&VRAM_I[addr & 0x3FFF];

    return ret;
}

template<typename T>
void WriteVRAM_BBG(u32 addr, T val)
{
    u32 mask = VRAMMap_BBG[(addr >> 14) & 0x7];

    if (mask & (1<<2)) *(T*)&VRAM_C[addr & 0x1FFFF] = val;
    if (mask & (1<<7)) *(T*)&VRAM_H[addr & 0x7FFF] = val;
    if (mask & (1<<8)) *(T*)&VRAM_I[addr & 0x3FFF] = val;
}


template<typename T>
T ReadVRAM_BOBJ(u32 addr)
{
    u32 ret = 0;
    u32 mask = VRAMMap_BOBJ[(addr >> 14) & 0x7];

    if (mask & (1<<3)) ret |= *(T*)&VRAM_D[addr & 0x1FFFF];
    if (mask & (1<<8)) ret |= *(T*)&VRAM_I[addr & 0x3FFF];

    return ret;
}

template<typename T>
void WriteVRAM_BOBJ(u32 addr, T val)
{
    u32 mask = VRAMMap_BOBJ[(addr >> 14) & 0x7];

    if (mask & (1<<3)) *(T*)&VRAM_D[addr & 0x1FFFF] = val;
    if (mask & (1<<8)) *(T*)&VRAM_I[addr & 0x3FFF] = val;
}


template<typename T>
T ReadVRAM_ARM7(u32 addr)
{
    u32 ret = 0;
    u32 mask = VRAMMap_ARM7[(addr >> 17) & 0x1];

    if (mask & (1<<2)) ret |= *(T*)&VRAM_C[addr & 0x1FFFF];
    if (mask & (1<<3)) ret |= *(T*)&VRAM_D[addr & 0x1FFFF];

    return ret;
}

template<typename T>
void WriteVRAM_ARM7(u32 addr, T val)
{
    u32 mask = VRAMMap_ARM7[(addr >> 17) & 0x1];

    if (mask & (1<<2)) *(T*)&VRAM_C[addr & 0x1FFFF] = val;
    if (mask & (1<<3)) *(T*)&VRAM_D[addr & 0x1FFFF] = val;
}


template<typename T>
T ReadVRAM_BG(u32 addr)
{
    if ((addr & 0xFFE00000) == 0x06000000)
        return ReadVRAM_ABG<T>(addr);
    else
        return ReadVRAM_BBG<T>(addr);
}

template<typename T>
T ReadVRAM_OBJ(u32 addr)
{
    if ((addr & 0xFFE00000) == 0x06400000)
        return ReadVRAM_AOBJ<T>(addr);
    else
        return ReadVRAM_BOBJ<T>(addr);
}


template<typename T>
T ReadVRAM_Texture(u32 addr)
{
    u32 ret = 0;
    u32 mask = VRAMMap_Texture[(addr >> 17) & 0x3];

    if (mask & (1<<0)) ret |= *(T*)&VRAM_A[addr & 0x1FFFF];
    if (mask & (1<<1)) ret |= *(T*)&VRAM_B[addr & 0x1FFFF];
    if (mask & (1<<2)) ret |= *(T*)&VRAM_C[addr & 0x1FFFF];
    if (mask & (1<<3)) ret |= *(T*)&VRAM_D[addr & 0x1FFFF];

    return ret;
}

template<typename T>
T ReadVRAM_TexPal(u32 addr)
{
    u32 ret = 0;
    u32 mask = VRAMMap_TexPal[(addr >> 14) & 0x7];

    if (mask & (1<<4)) ret |= *(T*)&VRAM_E[addr & 0xFFFF];
    if (mask & (1<<5)) ret |= *(T*)&VRAM_F[addr & 0x3FFF];
    if (mask & (1<<6)) ret |= *(T*)&VRAM_G[addr & 0x3FFF];

    return ret;
}


void DisplaySwap(u32 val);

void StartFrame();
void StartScanline(u32 line);

void SetDispStat(u32 cpu, u16 val);

}

#endif
