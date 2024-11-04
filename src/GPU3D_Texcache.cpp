#include "GPU3D_Texcache.h"

namespace melonDS
{

inline u16 ColorAvg(u16 color0, u16 color1)
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

    return r | g | b;
}

inline u16 Color5of3(u16 color0, u16 color1)
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

    return r | g | b;
}

inline u16 Color3of5(u16 color0, u16 color1)
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

    return r | g | b;
}

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
    u8 r = (val & 0x1F) << 1;
    u8 g = (val & 0x3E0) >> 4;
    u8 b = (val & 0x7C00) >> 9;
    if (r) r++;
    if (g) g++;
    if (b) b++;
    return (u32)r | ((u32)g << 8) | ((u32)b << 16);
}

template <int outputFmt>
void ConvertBitmapTexture(u32 width, u32 height, u32* output, u32 addr, GPU& gpu)
{
    for (u32 i = 0; i < width*height; i++)
    {
        u16 value = gpu.ReadVRAMFlat_Texture<u16>(addr + i * 2);

        switch (outputFmt)
        {
        case outputFmt_RGB6A5:
            output[i] = ConvertRGB5ToRGB6(value) | (value & 0x8000 ? 0x1F000000 : 0);
            break;
        case outputFmt_RGBA8:
            output[i] = ConvertRGB5ToRGB8(value) | (value & 0x8000 ? 0xFF000000 : 0);
            break;
        case outputFmt_BGRA8:
            output[i] = ConvertRGB5ToBGR8(value) | (value & 0x8000 ? 0xFF000000 : 0);
            break;
        }
    }
}

template void ConvertBitmapTexture<outputFmt_RGB6A5>(u32 width, u32 height, u32* output, u32 addr, GPU& gpu);

template <int outputFmt>
void ConvertCompressedTexture(u32 width, u32 height, u32* output, u32 addr, u32 addrAux, u32 palAddr, GPU& gpu)
{
    // we process a whole block at the time
    for (int y = 0; y < height / 4; y++)
    {
        for (int x = 0; x < width / 4; x++)
        {
            u32 data = gpu.ReadVRAMFlat_Texture<u32>(addr + (x + y * (width / 4))*4);
            u16 auxData = gpu.ReadVRAMFlat_Texture<u16>(addrAux + (x + y * (width / 4))*2);

            u32 paletteOffset = palAddr + (auxData & 0x3FFF) * 4;
            u16 color0 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset) | 0x8000;
            u16 color1 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset+2) | 0x8000;
            u16 color2 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset+4) | 0x8000;
            u16 color3 = gpu.ReadVRAMFlat_TexPal<u16>(paletteOffset+6) | 0x8000;

            switch ((auxData >> 14) & 0x3)
            {
            case 0:
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
                    u32 colorIdx = 16 * ((data >> 2 * (i + j * 4)) & 0x3);
                    u16 color = (packed >> colorIdx) & 0xFFFF;
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

template void ConvertCompressedTexture<outputFmt_RGB6A5>(u32, u32, u32*, u32, u32, u32, GPU&);

template <int outputFmt, int X, int Y>
void ConvertAXIYTexture(u32 width, u32 height, u32* output, u32 addr, u32 palAddr, GPU& gpu)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            u8 val = gpu.ReadVRAMFlat_Texture<u8>(addr + x + y * width);

            u32 idx = val & ((1 << Y) - 1);

            u16 color = gpu.ReadVRAMFlat_TexPal<u16>(palAddr + idx * 2);
            u32 alpha = (val >> Y) & ((1 << X) - 1);
            if (X != 5)
                alpha = alpha * 4 + alpha / 2;

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

template void ConvertAXIYTexture<outputFmt_RGB6A5, 5, 3>(u32, u32, u32*, u32, u32, GPU&);
template void ConvertAXIYTexture<outputFmt_RGB6A5, 3, 5>(u32, u32, u32*, u32, u32, GPU&);

template <int outputFmt, int colorBits>
void ConvertNColorsTexture(u32 width, u32 height, u32* output, u32 addr, u32 palAddr, bool color0Transparent, GPU& gpu)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width / (16 / colorBits); x++)
        {
            // smallest possible row is 8 pixels with 2bpp => fits in u16
            u16 val = gpu.ReadVRAMFlat_Texture<u16>(addr + 2 * (x + y * (width / (16 / colorBits))));

            for (int i = 0; i < 16 / colorBits; i++)
            {
                u32 index = val & ((1 << colorBits) - 1);
                val >>= colorBits;
                u16 color = gpu.ReadVRAMFlat_TexPal<u16>(palAddr + index * 2);

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
                output[x * (16 / colorBits) + y * width + i] = res;
            }
        }
    }
}

template void ConvertNColorsTexture<outputFmt_RGB6A5, 2>(u32, u32, u32*, u32, u32, bool, GPU&);
template void ConvertNColorsTexture<outputFmt_RGB6A5, 4>(u32, u32, u32*, u32, u32, bool, GPU&);
template void ConvertNColorsTexture<outputFmt_RGB6A5, 8>(u32, u32, u32*, u32, u32, bool, GPU&);

}