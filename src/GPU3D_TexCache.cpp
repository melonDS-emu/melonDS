#include "GPU3D.h"
#include "NDS.h"
#include "GPU.h"
#include "FIFO.h"

#include "types.h"

#define XXH_STATIC_LINKING_ONLY
#include "xxhash/xxhash.h"
#include "stb/stb_image_write.h"

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <assert.h>
#include <string.h>

namespace GPU3D
{
namespace TexCache
{

inline u32 ConvertRGB5ToRGB8(u16 val)
{
    return (((u32)val & 0x1F) << 3)
        | (((u32)val & 0x3E0) << 6)
        | (((u32)val & 0x7C00) << 9);
}
inline u32 ConvertRGB5ToBGR8(u16 val)
{
    return (((u32)val & 0x1F) << 9)
        | (((u32)val & 0x3E0) << 6)
        | (((u32)val & 0x7C00) << 3);
}
inline u32 ConvertRGB5ToRGB6(u16 val)
{
    return (((u32)val & 0x1F) << 1)
        | (((u32)val & 0x3E0) << 4)
        | (((u32)val & 0x7C00) << 7);
}

template <int outputFmt>
void ConvertCompressedTexture(u32 width, u32 height, u32* output, u8* texData, u8* texAuxData, u16* palData)
{
    // we process a whole block at the time
    for (int y = 0; y < height / 4; y++)
    {
        for (int x = 0; x < width / 4; x++)
        {
            u32 data = ((u32*)texData)[x + y * (width / 4)];
            u16 auxData = ((u16*)texAuxData)[x + y * (width / 4)];

            u32 paletteOffset = auxData & 0x3FFF;
            u16 color0 = palData[paletteOffset*2] | 0x8000;
            u16 color1 = palData[paletteOffset*2+1] | 0x8000;
            u16 color2, color3;

            switch ((auxData >> 14) & 0x3)
            {
            case 0:
                color2 = palData[paletteOffset*2+2] | 0x8000;
                color3 = 0;
                break;
            case 1:
                {
                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0 + r1) >> 1;
                    u32 g = ((g0 + g1) >> 1) & 0x03E0;
                    u32 b = ((b0 + b1) >> 1) & 0x7C00;
                    color2 = r | g | b | 0x8000;
                }
                color3 = 0;
                break;
            case 2:
                color2 = palData[paletteOffset*2+2] | 0x8000;
                color3 = palData[paletteOffset*2+3] | 0x8000;
                break;
            case 3:
                {
                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0*5 + r1*3) >> 3;
                    u32 g = ((g0*5 + g1*3) >> 3) & 0x03E0;
                    u32 b = ((b0*5 + b1*3) >> 3) & 0x7C00;

                    color2 = r | g | b | 0x8000;
                }
                {
                    u32 r0 = color0 & 0x001F;
                    u32 g0 = color0 & 0x03E0;
                    u32 b0 = color0 & 0x7C00;
                    u32 r1 = color1 & 0x001F;
                    u32 g1 = color1 & 0x03E0;
                    u32 b1 = color1 & 0x7C00;

                    u32 r = (r0*3 + r1*5) >> 3;
                    u32 g = ((g0*3 + g1*5) >> 3) & 0x03E0;
                    u32 b = ((b0*3 + b1*5) >> 3) & 0x7C00;

                    color3 = r | g | b | 0x8000;
                }
                break;
            }

            // in 2020 our default data types are big enough to be used as lookup tables...
            u64 packed = color0 | ((u64)color1 << 16) | ((u64)color2 << 32) | ((u64)color3 << 48);

            for (int j = 0; j < 4; j++)
            {
                for (int i = 0; i < 4; i++)
                {
                    u16 color = (packed >> 16 * (data >> 2 * (i + j * 4))) & 0xFFFF;
                    u32 res;
                    switch (outputFmt)
                    {
                    case outputFmt_RGB6A5: res = ConvertRGB5ToRGB6(color)
                        | ((color & 0x8000) ? 0x1F000000 : 0); break;
                    case outputFmt_RGBA8: res = ConvertRGB5ToRGB8(color)
                        | ((color & 0x8000) ? 0xFF000000 : 0); break;
                    case outputFmt_BGRA8: res = ConvertRGB5ToBGR8(color)
                        | ((color & 0x8000) ? 0xFF000000 : 0); break;
                    }
                    output[x * 4 + i + (y * 4 + j) * width] = res;
                }
            }
        }
    }
}

template <int outputFmt>
void ConvertDirectColorTexture(u32 width, u32 height, u32* output, u8* texData)
{
    u16* texData16 = (u16*)texData;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            u16 color = texData16[x + y * width];

            u32 res;
            switch (outputFmt)
            {
            case outputFmt_RGB6A5: res = ConvertRGB5ToRGB6(color)
                    | ((color & 0x8000) ? 0x1F000000 : 0); break;
                case outputFmt_RGBA8: res = ConvertRGB5ToRGB8(color)
                    | ((color & 0x8000) ? 0xFF000000 : 0); break;
                case outputFmt_BGRA8: res = ConvertRGB5ToBGR8(color)
                    | ((color & 0x8000) ? 0xFF000000 : 0); break;
            }
            output[x + y * width] = res;
        }
    }
}

template <int outputFmt, int X, int Y>
void ConvertAXIYTexture(u32 width, u32 height, u32* output, u8* texData, u16* palData, bool color0Transparent)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            u8 val = texData[x + y * width];

            u32 idx = val & ((1 << Y) - 1);

            u16 color = palData[idx];
            u32 alpha = (val >> X) & ((1 << X) - 1);
            if (X != 5)
                alpha = alpha * 4 + alpha / 2;
            if (color0Transparent && idx == 0)
                alpha = 0;

            u32 res;
            switch (outputFmt)
            {
            case outputFmt_RGB6A5: res = ConvertRGB5ToRGB6(color) | alpha << 24; break;
            // make sure full alpha == 255
            case outputFmt_RGBA8: res = ConvertRGB5ToRGB8(color) | (alpha << 27 | (alpha & 0x1C) << 22); break;
            case outputFmt_BGRA8: res = ConvertRGB5ToBGR8(color) | (alpha << 27 | (alpha & 0x1C) << 22); break;
            }
            output[x + y * width] = res;
        }
    }
}

template <int outputFmt, int colorBits>
void ConvertNColorsTexture(u32 width, u32 height, u32* output, u8* texData, u16* palData, bool color0Transparent)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width / (8 / colorBits); x++)
        {
            u8 val = texData[x + y * (width / (8 / colorBits))];

            for (int i = 0; i < 8 / colorBits; i++)
            {
                u32 index = (val >> (i * colorBits)) & ((1 << colorBits) - 1);
                u16 color = palData[index];

                bool transparent = color0Transparent && index == 0;
                u32 res;
                switch (outputFmt)
                {
                case outputFmt_RGB6A5: res = ConvertRGB5ToRGB6(color)
                    | (transparent ? 0 : 0x1F000000); break;
                case outputFmt_RGBA8: res = ConvertRGB5ToRGB8(color)
                    | (transparent ? 0 : 0xFF000000); break;
                case outputFmt_BGRA8: res = ConvertRGB5ToBGR8(color)
                    | (transparent ? 0 : 0xFF000000); break;
                }
                output[x * (8 / colorBits) + y * width + i] = res;
            }
        }
    }
}

u8 UnpackBuffer[256*1024];

struct Texture
{
    // 1 bit ^= 1 KB
    u64 PaletteMask[2];
    u64 TextureMask[8];
    ExternalTexHandle Handle;
};

u64 PaletteCacheStatus;
u8 PaletteCache[128*1024];

u64 PaletteDirty[2];
u64 TexturesDirty[8];

std::unordered_map<u64, Texture> TextureCache;

int converted = 0;
int pixelsConverted = 0;
bool updatePalette = false;
bool copyTexture = false;
bool textureUpdated = false;

void Init()
{
}

void DeInit()
{
}

void Reset()
{
    PaletteCacheStatus = 0;

    memset(PaletteDirty, 0, 8*2);
    memset(TexturesDirty, 0, 8*8);
    TextureCache.clear();
}

u8* GetTexturePtr(u32 addr, u32 size, u8** unpackBuffer)
{
    u32 map = GPU::VRAMMap_Texture[addr >> 17];
    if (((addr + size) & ~0x1FFFF) == (addr & ~0x1FFFF) && map && (map & (map - 1)) == 0)
    {
        // fast path; inside a single block of vram and no overlapping blocks mapped
        // no copying necessary
        return GPU::VRAM[__builtin_ctz(map)] + (addr & 0x1FFFF);
    }
    else
    {
        copyTexture = true;
        u8* buffer = *unpackBuffer;
        *unpackBuffer += size;
        for (int i = 0; i < size; i += 8)
        {
            *(u64*)&buffer[i] = GPU::ReadVRAM_Texture<u64>(addr + i);
        }
        return buffer;
    }
}

void EnsurePaletteCoherent(u64 mask)
{
    if ((PaletteCacheStatus & mask) != mask)
    {
        u32 updateField = ~PaletteCacheStatus & mask;
        PaletteCacheStatus |= mask;
        while (updateField != 0)
        {
            updatePalette = true;
            int idx = __builtin_ctz(updateField);
            u32 map = GPU::VRAMMap_TexPal[idx >> 3];
            if (map && (map & (map - 1)) == 0)
            {
                u32 bank = __builtin_ctz(map);
                memcpy(
                    PaletteCache + idx * 0x800, 
                    GPU::VRAM[bank] + ((idx * 0x800) & GPU::VRAMMask[bank]), 
                    0x800);
            }
            else
                for (int i = 0; i < 0x800; i += 8)
                    *(u64*)&PaletteCache[idx * 0x800 + i] = GPU::ReadVRAM_TexPal<u64>(idx * 0x800 + i);
            updateField &= ~(1 << idx);
        }
    }
}

void UpdateTextures()
{
    converted = 0;
    pixelsConverted = 0;
    updatePalette = false;
    copyTexture = false;
    textureUpdated = false;

    bool paletteDirty = PaletteDirty[0] | PaletteDirty[1];
    bool textureDirty = false;
    for (int i = 0; i < 8; i++)
        textureDirty |= TexturesDirty[i] != 0;
    
    if (paletteDirty || textureDirty)
    {
        textureUpdated = true;
        for (auto it = TextureCache.begin(); it != TextureCache.end(); )
        {
            u64 dirty = (it->second.PaletteMask[0] & PaletteDirty[0])
                | (it->second.PaletteMask[1] | PaletteDirty[1]);

            for (int i = 0; i < 8; i++)
                dirty |= it->second.TextureMask[i] & TexturesDirty[i];

            if (dirty)
            {
                u32 width = 8 << ((it->first >> 20) & 0x7);
                u32 height = 8 << ((it->first >> 23) & 0x7);

                if (GPU3D::Renderer == 0)
                    SoftRenderer::FreeTexture(it->second.Handle, width, height);

                it = TextureCache.erase(it);
            }
            else
                it++;
        }
        memset(PaletteDirty, 0, 8*2);
        memset(TexturesDirty, 0, 8*8);
    }
}

inline u64 MakePaletteMask(u32 addr, u32 size)
{
    return ((1ULL << (((addr + size + 0x7FF & ~0x7FF) >> 11) - (addr >> 11))) - 1) << (addr >> 11);
}

inline void MakeDirtyMask(u64* out, u32 addr, u32 size)
{
    u32 start = addr >> 10;
    u32 count = (((addr + size + 0x3FF) & ~0x3FF) >> 10) - start;

    u32 firstIdx = start >> 6;
    u32 indicesCount = (((count + 0x3F) & ~0x3F) >> 6) - firstIdx;

    out[firstIdx] = (1ULL << (63 - (start & 0x3F))) - 1 << (start & 0x3F);
    out[firstIdx + indicesCount - 1] = (1ULL << (start & 0x3F)) - 1;
    for (int i = firstIdx + 1; i < firstIdx + indicesCount - 1; i++)
        out[i] |= 0xFFFFFFFFFFFFFFFF;
}

template <int format>
ExternalTexHandle GetTexture(u32 texParam, u32 palBase)
{
    u32 fmt = (texParam >> 26) & 0x7;
    u32 addr = (texParam & 0xFFFF) << 3;
    u32 width = 8 << ((texParam >> 20) & 0x7);
    u32 height = 8 << ((texParam >> 23) & 0x7);

    if (fmt == 7)
        palBase = 0;

    u64 key = (u64)(texParam & 0x3FF0FFFF) | ((u64)palBase << 32);

    auto lookup = TextureCache.emplace(std::make_pair(key, Texture()));
    bool inserted = lookup.second;
    Texture& texture = lookup.first->second;

    if (inserted)
    {
        converted++;
        pixelsConverted += width * height;
        u8* unpackBuffer = UnpackBuffer;

        u32* data = GPU3D::Renderer == 0
            ? SoftRenderer::AllocateTexture(&texture.Handle, width, height)
            : NULL;

        memset(texture.TextureMask, 0, 8*8);
        memset(texture.PaletteMask, 0, 8*2);

        if (fmt == 7)
        {
            u8* texData = GetTexturePtr(addr, width*height*2, &unpackBuffer);

            MakeDirtyMask(texture.TextureMask, addr, width*height*2);

            ConvertDirectColorTexture<format>(width, height, data, texData);
        }
        else if (fmt == 5)
        {
            u8* texData = GetTexturePtr(addr, width*height/16*4, &unpackBuffer);
            u32 slot1addr = 0x20000 + ((addr & 0x1FFFC) >> 1);
            if (addr >= 0x40000)
                slot1addr += 0x10000;
            u8* texAuxData = GetTexturePtr(slot1addr, width*height/16*2, &unpackBuffer);

            MakeDirtyMask(texture.TextureMask, addr, width*height/16*4);
            MakeDirtyMask(texture.TextureMask, slot1addr, width*height/16*2);
            MakeDirtyMask(texture.PaletteMask, palBase*16, 0x10000);

            u64 paletteMask = MakePaletteMask(palBase*16, 0x10000);
            EnsurePaletteCoherent(MakePaletteMask(palBase*16, 0x10000));
            u16* palData = (u16*)(PaletteCache + palBase*16);

            ConvertCompressedTexture<format>(width, height, data, texData, texAuxData, palData);
        }
        else
        {
            u32 texSize, palAddr = palBase*16, palSize;
            switch (fmt)
            {
            case 1: texSize = width*height; palSize = 32; break;
            case 6: texSize = width*height; palSize = 8; break;
            case 2: texSize = width*height/4; palSize = 4; palAddr >>= 1; break;
            case 3: texSize = width*height/2; palSize = 16; break;
            case 4: texSize = width*height; palSize = 256; break;
            }

            u8* texData = GetTexturePtr(addr, texSize, &unpackBuffer);
            EnsurePaletteCoherent(MakePaletteMask(palAddr, palSize*2));
            u16* palData = (u16*)(PaletteCache + palAddr);

            MakeDirtyMask(texture.TextureMask, addr, texSize);
            MakeDirtyMask(texture.PaletteMask, palAddr, palSize);

            bool color0Transparent = texParam & (1 << 29);

            switch (fmt)
            {
            case 1: ConvertAXIYTexture<format, 3, 5>(width, height, data, texData, palData, color0Transparent); break;
            case 6: ConvertAXIYTexture<format, 5, 3>(width, height, data, texData, palData, color0Transparent); break;
            case 2: ConvertNColorsTexture<format, 2>(width, height, data, texData, palData, color0Transparent); break;
            case 3: ConvertNColorsTexture<format, 4>(width, height, data, texData, palData, color0Transparent); break;
            case 4: ConvertNColorsTexture<format, 8>(width, height, data, texData, palData, color0Transparent); break;
            }
        }

        if (GPU3D::Renderer == 1)
        {}
    }

    return texture.Handle;
}

std::unordered_set<XXH64_hash_t> AlreadySavedTextures;

void SaveTextures()
{
    /*for (auto texture : TextureCache)
    {
        u32 width = 8 << ((texture.first >> 20) & 0x7);
        u32 height = 8 << ((texture.first >> 23) & 0x7);

        u32* data = &TextureMem[__builtin_ctz(width*height)-6].Pixels[texture.second.Index];
        XXH64_hash_t hash = XXH3_64bits(data, width*height*4);
        if (AlreadySavedTextures.count(hash) == 0)
        {
            char filename[128];
            sprintf(filename, "./textures/%016x.png", hash);
            stbi_write_png(filename, width, height, 4, data, width*4);
            AlreadySavedTextures.insert(hash);
        }
    }*/
    //printf("%d %d textures converted %d pixels %d %d %d\n", converted, TextureCache.size(), pixelsConverted, updatePalette, copyTexture, textureUpdated);
}

void InvalidateTexSlot(u32 base)
{
    TexturesDirty[(base << 1)] = 0xFFFFFFFFFFFFFFFF;
    TexturesDirty[(base << 1) + 1] = 0xFFFFFFFFFFFFFFFF;
}

void InvalidatePalSlot(u32 base)
{
    PaletteDirty[base >> 2] |= 0xFFFF << (base & 0x3) * 16;
    PaletteCacheStatus &= ~(0xFF << base * 8);
}

}
}

template GPU3D::TexCache::ExternalTexHandle
    GPU3D::TexCache::GetTexture<GPU3D::TexCache::outputFmt_RGB6A5>(u32, u32);
template GPU3D::TexCache::ExternalTexHandle
    GPU3D::TexCache::GetTexture<GPU3D::TexCache::outputFmt_RGBA8>(u32, u32);
template GPU3D::TexCache::ExternalTexHandle
    GPU3D::TexCache::GetTexture<GPU3D::TexCache::outputFmt_BGRA8>(u32, u32);