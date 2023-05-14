#ifndef GPU3D_TEXCACHE
#define GPU3D_TEXCACHE

#include "types.h"
#include "GPU.h"

#include <assert.h>
#include <unordered_map>

#define XXH_STATIC_LINKING_ONLY
#include "xxhash/xxhash.h"

namespace GPU3D
{

inline u32 TextureWidth(u32 texparam)
{
    return 8 << ((texparam >> 20) & 0x7);
}

inline u32 TextureHeight(u32 texparam)
{
    return 8 << ((texparam >> 23) & 0x7);
}

enum
{
    outputFmt_RGB6A5,
    outputFmt_RGBA8,
    outputFmt_BGRA8
};

template <int outputFmt>
void ConvertBitmapTexture(u32 width, u32 height, u32* output, u8* texData);
template <int outputFmt>
void ConvertCompressedTexture(u32 width, u32 height, u32* output, u8* texData, u8* texAuxData, u16* palData);
template <int outputFmt, int X, int Y>
void ConvertAXIYTexture(u32 width, u32 height, u32* output, u8* texData, u16* palData);
template <int outputFmt, int colorBits>
void ConvertNColorsTexture(u32 width, u32 height, u32* output, u8* texData, u16* palData, bool color0Transparent);

template <typename TexLoaderT, typename TexHandleT>
class Texcache
{
public:
    Texcache(const TexLoaderT& texloader)
        : TexLoader(texloader) // probably better if this would be a move constructor???
    {}

    bool Update()
    {
        auto textureDirty = GPU::VRAMDirty_Texture.DeriveState(GPU::VRAMMap_Texture);
        auto texPalDirty = GPU::VRAMDirty_TexPal.DeriveState(GPU::VRAMMap_TexPal);

        bool textureChanged = GPU::MakeVRAMFlat_TextureCoherent(textureDirty);
        bool texPalChanged = GPU::MakeVRAMFlat_TexPalCoherent(texPalDirty);

        if (textureChanged || texPalChanged)
        {
            //printf("check invalidation %d\n", TexCache.size());
            for (auto it = Cache.begin(); it != Cache.end();)
            {
                TexCacheEntry& entry = it->second;
                if (textureChanged)
                {
                    for (u32 i = 0; i < 2; i++)
                    {
                        u32 startBit = entry.TextureRAMStart[i] / GPU::VRAMDirtyGranularity;
                        u32 bitsCount = ((entry.TextureRAMStart[i] + entry.TextureRAMSize[i] + GPU::VRAMDirtyGranularity - 1) / GPU::VRAMDirtyGranularity) - startBit;

                        u32 startEntry = startBit >> 6;
                        u64 entriesCount = ((startBit + bitsCount + 0x3F) >> 6) - startEntry;
                        for (u32 j = startEntry; j < startEntry + entriesCount; j++)
                        {
                            if (GetRangedBitMask(j, startBit, bitsCount) & textureDirty.Data[j])
                            {
                                u64 newTexHash = XXH3_64bits(&GPU::VRAMFlat_Texture[entry.TextureRAMStart[i]], entry.TextureRAMSize[i]);

                                if (newTexHash != entry.TextureHash[i])
                                    goto invalidate;
                            }
                        }
                    }
                }

                if (texPalChanged && entry.TexPalSize > 0)
                {
                    u32 startBit = entry.TexPalStart / GPU::VRAMDirtyGranularity;
                    u32 bitsCount = ((entry.TexPalStart + entry.TexPalSize + GPU::VRAMDirtyGranularity - 1) / GPU::VRAMDirtyGranularity) - startBit;

                    u32 startEntry = startBit >> 6;
                    u64 entriesCount = ((startBit + bitsCount + 0x3F) >> 6) - startEntry;
                    for (u32 j = startEntry; j < startEntry + entriesCount; j++)
                    {
                        if (GetRangedBitMask(j, startBit, bitsCount) & texPalDirty.Data[j])
                        {
                            u64 newPalHash = XXH3_64bits(&GPU::VRAMFlat_TexPal[entry.TexPalStart], entry.TexPalSize);
                            if (newPalHash != entry.TexPalHash)
                                goto invalidate;
                        }
                    }
                }

                it++;
                continue;
            invalidate:
                FreeTextures[entry.WidthLog2][entry.HeightLog2].push_back(entry.Texture);

                //printf("invalidating texture %d\n", entry.ImageDescriptor);

                it = Cache.erase(it);
            }

            return true;
        }

        return false;
    }

    void GetTexture(u32 texParam, u32 palBase, TexHandleT& textureHandle, u32& layer, u32*& helper)
    {
        // remove sampling and texcoord gen params
        texParam &= ~0xC00F0000;

        u32 fmt = (texParam >> 26) & 0x7;
        u64 key = texParam;
        if (fmt != 7)
        {
            key |= (u64)palBase << 32;
            if (fmt == 5)
                key &= ~((u64)1 << 29);
        }
        //printf("%" PRIx64 " %" PRIx32 " %" PRIx32 "\n", key, texParam, palBase);

        assert(fmt != 0 && "no texture is not a texture format!");

        auto it = Cache.find(key);

        if (it != Cache.end())
        {
            textureHandle = it->second.Texture.TextureID;
            layer = it->second.Texture.Layer;
            helper = &it->second.LastVariant;
            return;
        }

        u32 widthLog2 = (texParam >> 20) & 0x7;
        u32 heightLog2 = (texParam >> 23) & 0x7;
        u32 width = 8 << widthLog2;
        u32 height = 8 << heightLog2;

        u32 addr = (texParam & 0xFFFF) * 8;

        TexCacheEntry entry = {0};

        entry.TextureRAMStart[0] = addr;
        entry.WidthLog2 = widthLog2;
        entry.HeightLog2 = heightLog2;

        // apparently a new texture
        if (fmt == 7)
        {
            entry.TextureRAMSize[0] = width*height*2;

            ConvertBitmapTexture<outputFmt_RGB6A5>(width, height, DecodingBuffer, &GPU::VRAMFlat_Texture[addr]);
        }
        else if (fmt == 5)
        {
            u8* texData = &GPU::VRAMFlat_Texture[addr];
            u32 slot1addr = 0x20000 + ((addr & 0x1FFFC) >> 1);
            if (addr >= 0x40000)
                slot1addr += 0x10000;
            u8* texAuxData = &GPU::VRAMFlat_Texture[slot1addr];

            u16* palData = (u16*)(GPU::VRAMFlat_TexPal + palBase*16);

            entry.TextureRAMSize[0] = width*height/16*4;
            entry.TextureRAMStart[1] = slot1addr;
            entry.TextureRAMSize[1] = width*height/16*2;
            entry.TexPalStart = palBase*16;
            entry.TexPalSize = 0x10000;

            ConvertCompressedTexture<outputFmt_RGB6A5>(width, height, DecodingBuffer, texData, texAuxData, palData);
        }
        else
        {
            u32 texSize, palAddr = palBase*16, numPalEntries;
            switch (fmt)
            {
            case 1: texSize = width*height; numPalEntries = 32; break;
            case 6: texSize = width*height; numPalEntries = 8; break;
            case 2: texSize = width*height/4; numPalEntries = 4; palAddr >>= 1; break;
            case 3: texSize = width*height/2; numPalEntries = 16; break;
            case 4: texSize = width*height; numPalEntries = 256; break;
            }

            palAddr &= 0x1FFFF;

            /*printf("creating texture | fmt: %d | %dx%d | %08x | %08x\n", fmt, width, height, addr, palAddr);
            svcSleepThread(1000*1000);*/

            entry.TextureRAMSize[0] = texSize;
            entry.TexPalStart = palAddr;
            entry.TexPalSize = numPalEntries*2;

            u8* texData = &GPU::VRAMFlat_Texture[addr];
            u16* palData = (u16*)(GPU::VRAMFlat_TexPal + palAddr);

            //assert(entry.TexPalStart+entry.TexPalSize <= 128*1024*1024);

            bool color0Transparent = texParam & (1 << 29);

            switch (fmt)
            {
            case 1: ConvertAXIYTexture<outputFmt_RGB6A5, 3, 5>(width, height, DecodingBuffer, texData, palData); break;
            case 6: ConvertAXIYTexture<outputFmt_RGB6A5, 5, 3>(width, height, DecodingBuffer, texData, palData); break;
            case 2: ConvertNColorsTexture<outputFmt_RGB6A5, 2>(width, height, DecodingBuffer, texData, palData, color0Transparent); break;
            case 3: ConvertNColorsTexture<outputFmt_RGB6A5, 4>(width, height, DecodingBuffer, texData, palData, color0Transparent); break;
            case 4: ConvertNColorsTexture<outputFmt_RGB6A5, 8>(width, height, DecodingBuffer, texData, palData, color0Transparent); break;
            }
        }

        for (int i = 0; i < 2; i++)
        {
            if (entry.TextureRAMSize[i])
                entry.TextureHash[i] = XXH3_64bits(&GPU::VRAMFlat_Texture[entry.TextureRAMStart[i]], entry.TextureRAMSize[i]);
        }
        if (entry.TexPalSize)
            entry.TexPalHash = XXH3_64bits(&GPU::VRAMFlat_TexPal[entry.TexPalStart], entry.TexPalSize);

        auto& texArrays = TexArrays[widthLog2][heightLog2];
        auto& freeTextures = FreeTextures[widthLog2][heightLog2];

        if (freeTextures.size() == 0)
        {
            texArrays.resize(texArrays.size()+1);
            GLuint& array = texArrays[texArrays.size()-1];

            u32 layers = std::min<u32>((8*1024*1024) / (width*height*4), 64);

            // allocate new array texture
            //printf("allocating new layer set for %d %d %d %d\n", width, height, texArrays.size()-1, array.ImageDescriptor);
            array = TexLoader.GenerateTexture(width, height, layers);

            for (u32 i = 0; i < layers; i++)
            {
                freeTextures.push_back(TexArrayEntry{array, i});
            }
        }

        TexArrayEntry storagePlace = freeTextures[freeTextures.size()-1];
        freeTextures.pop_back();

        entry.Texture = storagePlace;

        TexLoader.UploadTexture(storagePlace.TextureID, width, height, storagePlace.Layer, DecodingBuffer);
        //printf("using storage place %d %d | %d %d (%d)\n", width, height, storagePlace.TexArrayIdx, storagePlace.LayerIdx, array.ImageDescriptor);

        textureHandle = storagePlace.TextureID;
        layer = storagePlace.Layer;
        helper = &Cache.emplace(std::make_pair(key, entry)).first->second.LastVariant;
    }

    void Reset()
    {
        for (u32 i = 0; i < 8; i++)
        {
            for (u32 j = 0; j < 8; j++)
            {
                for (u32 k = 0; k < TexArrays[i][j].size(); k++)
                    TexLoader.DeleteTexture(TexArrays[i][j][k]);
                TexArrays[i][j].clear();
                FreeTextures[i][j].clear();
            }
        }
        Cache.clear();
    }
private:
    struct TexArrayEntry
    {
        TexHandleT TextureID;
        u32 Layer;
    };

    struct TexCacheEntry
    {
        u32 LastVariant; // very cheap way to make variant lookup faster

        u32 TextureRAMStart[2], TextureRAMSize[2];
        u32 TexPalStart, TexPalSize;
        u8 WidthLog2, HeightLog2;
        TexArrayEntry Texture;

        u64 TextureHash[2];
        u64 TexPalHash;
    };
    std::unordered_map<u64, TexCacheEntry> Cache;

    TexLoaderT TexLoader;

    std::vector<TexArrayEntry> FreeTextures[8][8];
    std::vector<TexHandleT> TexArrays[8][8];

    u32 DecodingBuffer[1024*1024];
};

}

#endif