/*
    Copyright 2016-2023 melonDS team

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
#include "GPU3D.h"
#include "NonStupidBitfield.h"

namespace melonDS
{
class GPU3D;
class ARMJIT;

static constexpr u32 VRAMDirtyGranularity = 512;
class GPU;

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
    NonStupidBitField<Size/VRAMDirtyGranularity> DeriveState(const u32* currentMappings, GPU& gpu);
};

class GPU
{
public:
    explicit GPU(melonDS::NDS& nds, std::unique_ptr<Renderer3D>&& renderer3d = nullptr, std::unique_ptr<GPU2D::Renderer2D>&& renderer2d = nullptr) noexcept;
    ~GPU() noexcept;
    void Reset() noexcept;
    void Stop() noexcept;

    void DoSavestate(Savestate* file) noexcept;

    /// Sets the active renderer to the renderer given in the provided pointer.
    /// The pointer is moved-from, so it will be \c nullptr after this method is called.
    /// If the pointer is \c nullptr, the renderer is reset to the default renderer.
    void SetRenderer3D(std::unique_ptr<Renderer3D>&& renderer) noexcept;
    [[nodiscard]] const Renderer3D& GetRenderer3D() const noexcept { return GPU3D.GetCurrentRenderer(); }
    [[nodiscard]] Renderer3D& GetRenderer3D() noexcept { return GPU3D.GetCurrentRenderer(); }

    u8* GetUniqueBankPtr(u32 mask, u32 offset) noexcept;
    const u8* GetUniqueBankPtr(u32 mask, u32 offset) const noexcept;

    void SetRenderer2D(std::unique_ptr<GPU2D::Renderer2D>&& renderer) noexcept { GPU2D_Renderer = std::move(renderer); }
    [[nodiscard]] const GPU2D::Renderer2D& GetRenderer2D() const noexcept { return *GPU2D_Renderer; }
    [[nodiscard]] GPU2D::Renderer2D& GetRenderer2D() noexcept { return *GPU2D_Renderer; }

    void MapVRAM_AB(u32 bank, u8 cnt) noexcept;
    void MapVRAM_CD(u32 bank, u8 cnt) noexcept;
    void MapVRAM_E(u32 bank, u8 cnt) noexcept;
    void MapVRAM_FG(u32 bank, u8 cnt) noexcept;
    void MapVRAM_H(u32 bank, u8 cnt) noexcept;
    void MapVRAM_I(u32 bank, u8 cnt) noexcept;

    template<typename T>
    T ReadVRAM_LCDC(u32 addr) const noexcept
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
    T ReadVRAM_ABG(u32 addr) const noexcept
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
    T ReadVRAM_AOBJ(u32 addr) const noexcept
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
    T ReadVRAM_BBG(u32 addr) const noexcept
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
    T ReadVRAM_BOBJ(u32 addr) const noexcept
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
    T ReadVRAM_ARM7(u32 addr) const noexcept
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
    T ReadVRAM_BG(u32 addr) const noexcept
    {
        if ((addr & 0xFFE00000) == 0x06000000)
            return ReadVRAM_ABG<T>(addr);
        else
            return ReadVRAM_BBG<T>(addr);
    }

    template<typename T>
    T ReadVRAM_OBJ(u32 addr) const noexcept
    {
        if ((addr & 0xFFE00000) == 0x06400000)
            return ReadVRAM_AOBJ<T>(addr);
        else
            return ReadVRAM_BOBJ<T>(addr);
    }


    template<typename T>
    T ReadVRAM_Texture(u32 addr) const noexcept
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
    T ReadVRAM_TexPal(u32 addr) const noexcept
    {
        T ret = 0;
        u32 mask = VRAMMap_TexPal[(addr >> 14) & 0x7];

        if (mask & (1<<4)) ret |= *(T*)&VRAM_E[addr & 0xFFFF];
        if (mask & (1<<5)) ret |= *(T*)&VRAM_F[addr & 0x3FFF];
        if (mask & (1<<6)) ret |= *(T*)&VRAM_G[addr & 0x3FFF];

        return ret;
    }

    template<typename T>
    T ReadPalette(u32 addr) const noexcept
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
    T ReadOAM(u32 addr) const noexcept
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

    void SetPowerCnt(u32 val) noexcept;

    void StartFrame() noexcept;
    void FinishFrame(u32 lines) noexcept;
    void BlankFrame() noexcept;
    void StartScanline(u32 line) noexcept;
    void StartHBlank(u32 line) noexcept;

    void DisplayFIFO(u32 x) noexcept;

    void SetDispStat(u32 cpu, u16 val) noexcept;

    void SetVCount(u16 val) noexcept;
    bool MakeVRAMFlat_ABGCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty) noexcept;
    bool MakeVRAMFlat_BBGCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty) noexcept;

    bool MakeVRAMFlat_AOBJCoherent(NonStupidBitField<256*1024/VRAMDirtyGranularity>& dirty) noexcept;
    bool MakeVRAMFlat_BOBJCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty) noexcept;

    bool MakeVRAMFlat_ABGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty) noexcept;
    bool MakeVRAMFlat_BBGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty) noexcept;

    bool MakeVRAMFlat_AOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty) noexcept;
    bool MakeVRAMFlat_BOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty) noexcept;

    bool MakeVRAMFlat_TextureCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty) noexcept;
    bool MakeVRAMFlat_TexPalCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty) noexcept;

    void SyncDirtyFlags() noexcept;

    melonDS::NDS& NDS;
    u16 VCount = 0;
    u16 TotalScanlines = 0;
    u16 DispStat[2] {};
    u8 VRAMCNT[9] {};
    u8 VRAMSTAT = 0;

    alignas(u64) u8 Palette[2*1024] {};
    alignas(u64) u8 OAM[2*1024] {};

    alignas(u64) u8 VRAM_A[128*1024] {};
    alignas(u64) u8 VRAM_B[128*1024] {};
    alignas(u64) u8 VRAM_C[128*1024] {};
    alignas(u64) u8 VRAM_D[128*1024] {};
    alignas(u64) u8 VRAM_E[ 64*1024] {};
    alignas(u64) u8 VRAM_F[ 16*1024] {};
    alignas(u64) u8 VRAM_G[ 16*1024] {};
    alignas(u64) u8 VRAM_H[ 32*1024] {};
    alignas(u64) u8 VRAM_I[ 16*1024] {};

    u8* const VRAM[9]     = {VRAM_A,  VRAM_B,  VRAM_C,  VRAM_D,  VRAM_E, VRAM_F, VRAM_G, VRAM_H, VRAM_I};
    u32 const VRAMMask[9] = {0x1FFFF, 0x1FFFF, 0x1FFFF, 0x1FFFF, 0xFFFF, 0x3FFF, 0x3FFF, 0x7FFF, 0x3FFF};

    u32 VRAMMap_LCDC = 0;
    u32 VRAMMap_ABG[0x20] {};
    u32 VRAMMap_AOBJ[0x10] {};
    u32 VRAMMap_BBG[0x8] {};
    u32 VRAMMap_BOBJ[0x8] {};
    u32 VRAMMap_ABGExtPal[4] {};
    u32 VRAMMap_AOBJExtPal {};
    u32 VRAMMap_BBGExtPal[4] {};
    u32 VRAMMap_BOBJExtPal {};
    u32 VRAMMap_Texture[4] {};
    u32 VRAMMap_TexPal[8] {};
    u32 VRAMMap_ARM7[2] {};

    u8* VRAMPtr_ABG[0x20] {};
    u8* VRAMPtr_AOBJ[0x10] {};
    u8* VRAMPtr_BBG[0x8] {};
    u8* VRAMPtr_BOBJ[0x8] {};

    int FrontBuffer = 0;
    std::unique_ptr<u32[]> Framebuffer[2][2] {};

    GPU2D::Unit GPU2D_A;
    GPU2D::Unit GPU2D_B;
    melonDS::GPU3D GPU3D;

    NonStupidBitField<128*1024/VRAMDirtyGranularity> VRAMDirty[9] {};
    VRAMTrackingSet<512*1024, 16*1024> VRAMDirty_ABG {};
    VRAMTrackingSet<256*1024, 16*1024> VRAMDirty_AOBJ {};
    VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_BBG {};
    VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_BOBJ {};

    VRAMTrackingSet<32*1024, 8*1024> VRAMDirty_ABGExtPal {};
    VRAMTrackingSet<32*1024, 8*1024> VRAMDirty_BBGExtPal {};
    VRAMTrackingSet<8*1024, 8*1024> VRAMDirty_AOBJExtPal {};
    VRAMTrackingSet<8*1024, 8*1024> VRAMDirty_BOBJExtPal {};

    VRAMTrackingSet<512*1024, 128*1024> VRAMDirty_Texture {};
    VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_TexPal {};

    u8 VRAMFlat_ABG[512*1024] {};
    u8 VRAMFlat_BBG[128*1024] {};
    u8 VRAMFlat_AOBJ[256*1024] {};
    u8 VRAMFlat_BOBJ[128*1024] {};

    alignas(u16) u8 VRAMFlat_ABGExtPal[32*1024] {};
    alignas(u16) u8 VRAMFlat_BBGExtPal[32*1024] {};

    alignas(u16) u8 VRAMFlat_AOBJExtPal[8*1024] {};
    alignas(u16) u8 VRAMFlat_BOBJExtPal[8*1024] {};

    alignas(u64) u8 VRAMFlat_Texture[512*1024] {};
    alignas(u64) u8 VRAMFlat_TexPal[128*1024] {};
private:
    void ResetVRAMCache() noexcept;
    void AssignFramebuffers() noexcept;
    void InitFramebuffers() noexcept;
    template<typename T>
    T ReadVRAM_ABGExtPal(u32 addr) const noexcept
    {
        u32 mask = VRAMMap_ABGExtPal[(addr >> 13) & 0x3];

        T ret = 0;
        if (mask & (1<<4)) ret |= *(T*)&VRAM_E[addr & 0x7FFF];
        if (mask & (1<<5)) ret |= *(T*)&VRAM_F[addr & 0x3FFF];
        if (mask & (1<<6)) ret |= *(T*)&VRAM_G[addr & 0x3FFF];

        return ret;
    }

    template<typename T>
    T ReadVRAM_BBGExtPal(u32 addr) const noexcept
    {
        u32 mask = VRAMMap_BBGExtPal[(addr >> 13) & 0x3];

        T ret = 0;
        if (mask & (1<<7)) ret |= *(T*)&VRAM_H[addr & 0x7FFF];

        return ret;
    }

    template<typename T>
    T ReadVRAM_AOBJExtPal(u32 addr) const noexcept
    {
        u32 mask = VRAMMap_AOBJExtPal;

        T ret = 0;
        if (mask & (1<<4)) ret |= *(T*)&VRAM_F[addr & 0x1FFF];
        if (mask & (1<<5)) ret |= *(T*)&VRAM_G[addr & 0x1FFF];

        return ret;
    }

    template<typename T>
    T ReadVRAM_BOBJExtPal(u32 addr) const noexcept
    {
        u32 mask = VRAMMap_BOBJExtPal;

        T ret = 0;
        if (mask & (1<<8)) ret |= *(T*)&VRAM_I[addr & 0x1FFF];

        return ret;
    }

    template <u32 MappingGranularity, u32 Size>
    constexpr bool CopyLinearVRAM(u8* flat, const u32* mappings, NonStupidBitField<Size>& dirty, u64 (GPU::* const slowAccess)(u32) const noexcept) noexcept
    {
        const u32 VRAMBitsPerMapping = MappingGranularity / VRAMDirtyGranularity;

        bool change = false;

        typename NonStupidBitField<Size>::Iterator it = dirty.Begin();
        while (it != dirty.End())
        {
            u32 offset = *it * VRAMDirtyGranularity;
            u8* dst = flat + offset;
            u8* fastAccess = GetUniqueBankPtr(mappings[*it / VRAMBitsPerMapping], offset);
            if (fastAccess)
            {
                memcpy(dst, fastAccess, VRAMDirtyGranularity);
            }
            else
            {
                for (u32 i = 0; i < VRAMDirtyGranularity; i += 8)
                    *(u64*)&dst[i] = (this->*slowAccess)(offset + i);
            }
            change = true;
            it++;
        }
        return change;
    }

    u32 NextVCount = 0;

    bool RunFIFO = false;

    u16 VMatch[2] {};

    std::unique_ptr<GPU2D::Renderer2D> GPU2D_Renderer = nullptr;

    u32 OAMDirty = 0;
    u32 PaletteDirty = 0;
};
}

#endif
