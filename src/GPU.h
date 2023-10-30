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

#ifndef GPU_H
#define GPU_H

#include <memory>

#include "GPU2D.h"
#include "NonStupidBitfield.h"

#ifdef OGLRENDERER_ENABLED
#include "GPU_OpenGL.h"
#endif

namespace GPU
{

extern u16 VCount;
extern u16 TotalScanlines;

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

extern u8* const VRAM[9];

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

extern u8* VRAMPtr_ABG[0x20];
extern u8* VRAMPtr_AOBJ[0x10];
extern u8* VRAMPtr_BBG[0x8];
extern u8* VRAMPtr_BOBJ[0x8];

extern int FrontBuffer;
extern u32* Framebuffer[2][2];

extern GPU2D::Unit GPU2D_A;
extern GPU2D::Unit GPU2D_B;

extern int Renderer;

const u32 VRAMDirtyGranularity = 512;

extern NonStupidBitField<128*1024/VRAMDirtyGranularity> VRAMDirty[9];

template <u32 Size, u32 MappingGranularity>
struct VRAMTrackingSet
{
    u16 Mapping[Size / MappingGranularity];

    const u32 VRAMBitsPerMapping = MappingGranularity / VRAMDirtyGranularity;

    void Reset()
    {
        for (u32 i = 0; i < Size / MappingGranularity; i++)
        {
            // this is not a real VRAM bank
            // so it will always be a mismatch => the bank will be completely invalidated
            Mapping[i] = 0x8000;
        }
    }
    NonStupidBitField<Size/VRAMDirtyGranularity> DeriveState(u32* currentMappings);
};

extern VRAMTrackingSet<512*1024, 16*1024> VRAMDirty_ABG;
extern VRAMTrackingSet<256*1024, 16*1024> VRAMDirty_AOBJ;
extern VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_BBG;
extern VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_BOBJ;

extern VRAMTrackingSet<32*1024, 8*1024> VRAMDirty_ABGExtPal;
extern VRAMTrackingSet<32*1024, 8*1024> VRAMDirty_BBGExtPal;
extern VRAMTrackingSet<8*1024, 8*1024> VRAMDirty_AOBJExtPal;
extern VRAMTrackingSet<8*1024, 8*1024> VRAMDirty_BOBJExtPal;

extern VRAMTrackingSet<512*1024, 128*1024> VRAMDirty_Texture;
extern VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_TexPal;

extern u8 VRAMFlat_ABG[512*1024];
extern u8 VRAMFlat_BBG[128*1024];
extern u8 VRAMFlat_AOBJ[256*1024];
extern u8 VRAMFlat_BOBJ[128*1024];

extern u8 VRAMFlat_ABGExtPal[32*1024];
extern u8 VRAMFlat_BBGExtPal[32*1024];

extern u8 VRAMFlat_AOBJExtPal[8*1024];
extern u8 VRAMFlat_BOBJExtPal[8*1024];

extern u8 VRAMFlat_Texture[512*1024];
extern u8 VRAMFlat_TexPal[128*1024];

bool MakeVRAMFlat_ABGCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty);
bool MakeVRAMFlat_BBGCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty);

bool MakeVRAMFlat_AOBJCoherent(NonStupidBitField<256*1024/VRAMDirtyGranularity>& dirty);
bool MakeVRAMFlat_BOBJCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty);

bool MakeVRAMFlat_ABGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty);
bool MakeVRAMFlat_BBGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty);

bool MakeVRAMFlat_AOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty);
bool MakeVRAMFlat_BOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty);

bool MakeVRAMFlat_TextureCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty);
bool MakeVRAMFlat_TexPalCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty);

void SyncDirtyFlags();

extern u32 OAMDirty;
extern u32 PaletteDirty;

#ifdef OGLRENDERER_ENABLED
extern std::unique_ptr<GLCompositor> CurGLCompositor;
#endif

struct RenderSettings
{
    bool Soft_Threaded;

    int GL_ScaleFactor;
    bool GL_BetterPolygons;
};


bool Init();
void DeInit();
void Reset();
void Stop();

void DoSavestate(Savestate* file);

void InitRenderer(int renderer);
void DeInitRenderer();
void ResetRenderer();

void SetRenderSettings(int renderer, RenderSettings& settings);


u8* GetUniqueBankPtr(u32 mask, u32 offset);

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

    if (VRAMMap_LCDC & (1<<bank))
    {
        *(T*)&VRAM[bank][addr] = val;
        VRAMDirty[bank][addr / VRAMDirtyGranularity] = true;
    }
}


template<typename T>
T ReadVRAM_ABG(u32 addr)
{
    u8* ptr = VRAMPtr_ABG[(addr >> 14) & 0x1F];
    if (ptr) return *(T*)&ptr[addr & 0x3FFF];

    T ret = 0;
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

    if (mask & (1<<0))
    {
        VRAMDirty[0][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_A[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<1))
    {
        VRAMDirty[1][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_B[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<2))
    {
        VRAMDirty[2][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_C[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<3))
    {
        VRAMDirty[3][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_D[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<4))
    {
        VRAMDirty[4][(addr & 0xFFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_E[addr & 0xFFFF] = val;
    }
    if (mask & (1<<5))
    {
        VRAMDirty[5][(addr & 0x3FFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_F[addr & 0x3FFF] = val;
    }
    if (mask & (1<<6))
    {
        VRAMDirty[6][(addr & 0x3FFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_G[addr & 0x3FFF] = val;
    }
}


template<typename T>
T ReadVRAM_AOBJ(u32 addr)
{
    u8* ptr = VRAMPtr_AOBJ[(addr >> 14) & 0xF];
    if (ptr) return *(T*)&ptr[addr & 0x3FFF];

    T ret = 0;
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

    if (mask & (1<<0))
    {
        VRAMDirty[0][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_A[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<1))
    {
        VRAMDirty[1][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_B[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<4))
    {
        VRAMDirty[4][(addr & 0xFFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_E[addr & 0xFFFF] = val;
    }
    if (mask & (1<<5))
    {
        VRAMDirty[5][(addr & 0x3FFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_F[addr & 0x3FFF] = val;
    }
    if (mask & (1<<6))
    {
        VRAMDirty[6][(addr & 0x3FFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_G[addr & 0x3FFF] = val;
    }
}


template<typename T>
T ReadVRAM_BBG(u32 addr)
{
    u8* ptr = VRAMPtr_BBG[(addr >> 14) & 0x7];
    if (ptr) return *(T*)&ptr[addr & 0x3FFF];

    T ret = 0;
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

    if (mask & (1<<2))
    {
        VRAMDirty[2][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_C[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<7))
    {
        VRAMDirty[7][(addr & 0x7FFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_H[addr & 0x7FFF] = val;
    }
    if (mask & (1<<8))
    {
        VRAMDirty[8][(addr & 0x3FFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_I[addr & 0x3FFF] = val;
    }
}


template<typename T>
T ReadVRAM_BOBJ(u32 addr)
{
    u8* ptr = VRAMPtr_BOBJ[(addr >> 14) & 0x7];
    if (ptr) return *(T*)&ptr[addr & 0x3FFF];

    T ret = 0;
    u32 mask = VRAMMap_BOBJ[(addr >> 14) & 0x7];

    if (mask & (1<<3)) ret |= *(T*)&VRAM_D[addr & 0x1FFFF];
    if (mask & (1<<8)) ret |= *(T*)&VRAM_I[addr & 0x3FFF];

    return ret;
}

template<typename T>
void WriteVRAM_BOBJ(u32 addr, T val)
{
    u32 mask = VRAMMap_BOBJ[(addr >> 14) & 0x7];

    if (mask & (1<<3))
    {
        VRAMDirty[3][(addr & 0x1FFFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_D[addr & 0x1FFFF] = val;
    }
    if (mask & (1<<8))
    {
        VRAMDirty[8][(addr & 0x3FFF) / VRAMDirtyGranularity] = true;
        *(T*)&VRAM_I[addr & 0x3FFF] = val;
    }
}

template<typename T>
T ReadVRAM_ARM7(u32 addr)
{
    T ret = 0;
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
    T ret = 0;
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
    T ret = 0;
    u32 mask = VRAMMap_TexPal[(addr >> 14) & 0x7];

    if (mask & (1<<4)) ret |= *(T*)&VRAM_E[addr & 0xFFFF];
    if (mask & (1<<5)) ret |= *(T*)&VRAM_F[addr & 0x3FFF];
    if (mask & (1<<6)) ret |= *(T*)&VRAM_G[addr & 0x3FFF];

    return ret;
}

template<typename T>
T ReadPalette(u32 addr)
{
    return *(T*)&Palette[addr & 0x7FF];
}

template<typename T>
void WritePalette(u32 addr, T val)
{
    addr &= 0x7FF;

    *(T*)&Palette[addr] = val;
    PaletteDirty |= 1 << (addr / VRAMDirtyGranularity);
}

template<typename T>
T ReadOAM(u32 addr)
{
    return *(T*)&OAM[addr & 0x7FF];
}

template<typename T>
void WriteOAM(u32 addr, T val)
{
    addr &= 0x7FF;

    *(T*)&OAM[addr] = val;
    OAMDirty |= 1 << (addr / 1024);
}

void SetPowerCnt(u32 val);

void StartFrame();
void FinishFrame(u32 lines);
void BlankFrame();
void StartScanline(u32 line);
void StartHBlank(u32 line);

void DisplayFIFO(u32 x);

void SetDispStat(u32 cpu, u16 val);

void SetVCount(u16 val);
}

#include "GPU3D.h"

#endif
