/*
    Copyright 2016-2026 melonDS team

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

#ifndef GPU2D_VULKAN_SHADERS
#define GPU2D_VULKAN_SHADERS

#include <string>

// Vulkan port of the 2D renderer shaders from src/OpenGL_shaders/2D*.glsl.
// The algorithms are identical; the differences are purely in resource
// declarations:
//  - every resource lives in descriptor set 0 with a unique binding
//  - in/out varyings and vertex attributes carry explicit locations
//    (attribute locations match the GL glBindAttribLocation slots)
//  - the GL sampler2D BGLayerTex[4] array is split into four separate
//    bindings, accessed through SampleBGLayer()
//  - the loose uniforms (uCurBG, uRenderTransparent, uScaleFactor) became
//    one shared push constant block (Push2D)
//
// the C++ side prepends the "#version 460" line (same convention as
// GPU3D_ComputeVulkan_shaders.h)
//
// descriptor set 0 layout (all stages):
//   0: ubBGConfig (UBO)      1: ubScanlineConfig   2: ubCompositorConfig
//   3: ubSpriteConfig        4: ubSpriteScanlineConfig
//   5: VRAMTex (usampler2D)  6: PalTex (sampler2D) 7: SpriteTex (sampler2D)
//   8: BGLayerTex0  9: BGLayerTex1  10: BGLayerTex2  11: BGLayerTex3 (sampler2D)
//  12: OBJLayerTex 13: Capture128Tex 14: Capture256Tex (sampler2DArray)
//  15: MosaicTex (isampler2D)

namespace melonDS
{

namespace GPU2DShadersVulkan
{

const std::string LayerPreVS{R"(

struct sBGConfig
{
    ivec2 Size;
    int Type;
    int PalOffset;
    int TileOffset;
    int MapOffset;
    bool Clamp;
};

layout(std140, set = 0, binding = 0) uniform ubBGConfig
{
    int uVRAMMask;
    sBGConfig uBGConfig[4];
};

layout(push_constant) uniform Push2D
{
    int uCurBG;
    int uRenderTransparent;
    int uScaleFactor;
};

layout(location = 0) in vec2 vPosition;

layout(location = 0) smooth out vec2 fTexcoord;

void main()
{
    gl_Position = vec4((vPosition * 2) - 1, 0, 1);
    fTexcoord = vPosition * vec2(uBGConfig[uCurBG].Size);
}
)"};

const std::string LayerPreFS{R"(

layout(set = 0, binding = 5) uniform usampler2D VRAMTex;
layout(set = 0, binding = 6) uniform sampler2D PalTex;

struct sBGConfig
{
    ivec2 Size;
    int Type;
    int PalOffset;
    int TileOffset;
    int MapOffset;
    bool Clamp;
};

layout(std140, set = 0, binding = 0) uniform ubBGConfig
{
    int uVRAMMask;
    sBGConfig uBGConfig[4];
};

layout(push_constant) uniform Push2D
{
    int uCurBG;
    int uRenderTransparent;
    int uScaleFactor;
};

layout(location = 0) smooth in vec2 fTexcoord;

layout(location = 0) out vec4 oColor;

vec4 GetBGPalEntry(int layer, int pal, int id)
{
    ivec2 coord = ivec2(id, uBGConfig[layer].PalOffset + pal);
    vec4 col = texelFetch(PalTex, coord, 0);
    col.rgb *= (62.0/63.0);
    col.g += (col.a * 1.0/63.0);
    return col;
}

int VRAMRead8(int addr)
{
    ivec2 coord = ivec2(addr & 0x3FF, (addr >> 10) & uVRAMMask);
    int val = int(texelFetch(VRAMTex, coord, 0).r);
    return val;
}

int VRAMRead16(int addr)
{
    ivec2 coord = ivec2(addr & 0x3FF, (addr >> 10) & uVRAMMask);
    int lo = int(texelFetch(VRAMTex, coord, 0).r);
    int hi = int(texelFetch(VRAMTex, coord+ivec2(1,0), 0).r);
    return lo | (hi << 8);
}

vec4 GetBGLayerPixel(int layer, ivec2 coord)
{
    vec4 ret;

    if (uBGConfig[layer].Type == 0)
    {
        // text - 16-color tiles

        int mapoffset = uBGConfig[layer].MapOffset +
            (((coord.x >> 3) & 0x1F) << 1) +
            (((coord.y >> 3) & 0x1F) << 6);

        if (uBGConfig[layer].Size.y == 512)
        {
            if (uBGConfig[layer].Size.x == 512)
            {
                mapoffset +=
                    (((coord.x >> 8) & 0x1) << 11) +
                    (((coord.y >> 8) & 0x1) << 12);
            }
            else
            {
                mapoffset +=
                    (((coord.y >> 8) & 0x1) << 11);
            }
        }
        else if (uBGConfig[layer].Size.x == 512)
        {
            mapoffset +=
                (((coord.x >> 8) & 0x1) << 11);
        }

        int mapval = VRAMRead16(mapoffset);
        int tileoffset = (uBGConfig[layer].TileOffset << 1) + ((mapval & 0x3FF) << 6);

        if ((mapval & (1<<10)) != 0)
            tileoffset += (7 - (coord.x & 0x7));
        else
            tileoffset += (coord.x & 0x7);

        if ((mapval & (1<<11)) != 0)
            tileoffset += ((7 - (coord.y & 0x7)) << 3);
        else
            tileoffset += ((coord.y & 0x7) << 3);

        int col = VRAMRead8(tileoffset >> 1);
        if ((tileoffset & 0x1) != 0)
            col >>= 4;
        else
            col &= 0xF;
        col += ((mapval >> 12) << 4);

        ret = GetBGPalEntry(layer, 0, col);
        ret.a = ((col & 0xF) == 0) ? 0 : 1;
    }
    else if (uBGConfig[layer].Type == 1)
    {
        // text - 256-color tiles

        int mapoffset = uBGConfig[layer].MapOffset +
            (((coord.x >> 3) & 0x1F) << 1) +
            (((coord.y >> 3) & 0x1F) << 6);

        if (uBGConfig[layer].Size.y == 512)
        {
            if (uBGConfig[layer].Size.x == 512)
            {
                mapoffset +=
                    (((coord.x >> 8) & 0x1) << 11) +
                    (((coord.y >> 8) & 0x1) << 12);
            }
            else
            {
                mapoffset +=
                    (((coord.y >> 8) & 0x1) << 11);
            }
        }
        else if (uBGConfig[layer].Size.x == 512)
        {
            mapoffset +=
                (((coord.x >> 8) & 0x1) << 11);
        }

        int mapval = VRAMRead16(mapoffset);
        int tileoffset = uBGConfig[layer].TileOffset + ((mapval & 0x3FF) << 6);

        if ((mapval & (1<<10)) != 0)
            tileoffset += (7 - (coord.x & 0x7));
        else
            tileoffset += (coord.x & 0x7);

        if ((mapval & (1<<11)) != 0)
            tileoffset += ((7 - (coord.y & 0x7)) << 3);
        else
            tileoffset += ((coord.y & 0x7) << 3);

        int col = VRAMRead8(tileoffset);
        int pal = (uBGConfig[layer].PalOffset != 0) ? (mapval >> 12) : 0;

        ret = GetBGPalEntry(layer, pal, col);
        ret.a = (col == 0) ? 0 : 1;
    }
    else if (uBGConfig[layer].Type == 2)
    {
        // affine - 256 color tiles

        int mapoffset = uBGConfig[layer].MapOffset +
            (coord.x >> 3) +
            ((coord.y >> 3) * (uBGConfig[layer].Size.x >> 3));

        int mapval = VRAMRead8(mapoffset);
        int tileoffset = uBGConfig[layer].TileOffset + (mapval << 6);

        tileoffset += ((coord.y & 0x7) << 3);
        tileoffset += (coord.x & 0x7);

        int col = VRAMRead8(tileoffset);

        ret = GetBGPalEntry(layer, 0, col);
        ret.a = (col == 0) ? 0 : 1;
    }
    else if (uBGConfig[layer].Type == 3)
    {
        // extended - 256 color tiles

        int mapoffset = uBGConfig[layer].MapOffset +
            (((coord.x >> 3) +
            ((coord.y >> 3) * (uBGConfig[layer].Size.x >> 3))) << 1);

        int mapval = VRAMRead16(mapoffset);
        int tileoffset = uBGConfig[layer].TileOffset + ((mapval & 0x3FF) << 6);

        if ((mapval & (1<<10)) != 0)
            tileoffset += (7 - (coord.x & 0x7));
        else
            tileoffset += (coord.x & 0x7);

        if ((mapval & (1<<11)) != 0)
            tileoffset += ((7 - (coord.y & 0x7)) << 3);
        else
            tileoffset += ((coord.y & 0x7) << 3);

        int col = VRAMRead8(tileoffset);
        int pal = (uBGConfig[layer].PalOffset != 0) ? (mapval >> 12) : 0;

        ret = GetBGPalEntry(layer, pal, col);
        ret.a = (col == 0) ? 0 : 1;
    }
    else if (uBGConfig[layer].Type == 4)
    {
        // extended - 256 color bitmap

        int mapoffset = uBGConfig[layer].MapOffset +
            coord.x +
            (coord.y * uBGConfig[layer].Size.x);

        int col = VRAMRead8(mapoffset);

        ret = GetBGPalEntry(layer, 0, col);
        ret.a = (col == 0) ? 0 : 1;
    }
    else if (uBGConfig[layer].Type == 5)
    {
        // extended - direct color bitmap

        int mapoffset = uBGConfig[layer].MapOffset +
            ((coord.x +
            (coord.y * uBGConfig[layer].Size.x)) << 1);

        int col = VRAMRead16(mapoffset);

        ret.r = float((col << 1) & 0x3E) / 63;
        ret.g = float((col >> 4) & 0x3E) / 63;
        ret.b = float((col >> 9) & 0x3E) / 63;
        ret.a = float(col >> 15);
    }

    return ret;
}

void main()
{
    oColor = GetBGLayerPixel(uCurBG, ivec2(fTexcoord));
}
)"};

const std::string SpritePreVS{R"(

struct sOAM
{
    ivec2 Position;
    bvec2 Flip;
    ivec2 Size;
    ivec2 BoundSize;
    int OBJMode;
    int Type;
    int PalOffset;
    int TileOffset;
    int TileStride;
    int Rotscale;
    int BGPrio;
    bool Mosaic;
};

layout(std140, set = 0, binding = 3) uniform ubSpriteConfig
{
    int uVRAMMask;
    ivec4 uRotscale[32];
    sOAM uOAM[128];
};

layout(location = 0) in ivec2 vPosition;
layout(location = 1) in int vSpriteIndex;

layout(location = 0) flat out int fSpriteIndex;
layout(location = 1) smooth out vec2 fTexcoord;

void main()
{
    ivec2 sprpos = ivec2((vSpriteIndex & 0xF) * 64, (vSpriteIndex >> 4) * 64);
    ivec2 sprsize = uOAM[vSpriteIndex].Size;
    vec2 vtxpos = vec2(sprpos) + (vPosition * vec2(sprsize));
    vec2 fbsize = vec2(1024, 512);

    gl_Position = vec4(((vtxpos * 2) / fbsize) - 1, 0, 1);
    fSpriteIndex = vSpriteIndex;
    fTexcoord = vPosition * vec2(sprsize);
}
)"};

const std::string SpritePreFS{R"(

layout(set = 0, binding = 5) uniform usampler2D VRAMTex;
layout(set = 0, binding = 6) uniform sampler2D PalTex;

struct sOAM
{
    ivec2 Position;
    bvec2 Flip;
    ivec2 Size;
    ivec2 BoundSize;
    int OBJMode;
    int Type;
    int PalOffset;
    int TileOffset;
    int TileStride;
    int Rotscale;
    int BGPrio;
    bool Mosaic;
};

layout(std140, set = 0, binding = 3) uniform ubSpriteConfig
{
    int uVRAMMask;
    ivec4 uRotscale[32];
    sOAM uOAM[128];
};

layout(location = 0) flat in int fSpriteIndex;
layout(location = 1) smooth in vec2 fTexcoord;

layout(location = 0) out vec4 oColor;

vec4 GetOBJPalEntry(int pal, int id)
{
    ivec2 coord = ivec2(id, pal);
    vec4 col = texelFetch(PalTex, coord, 0);
    col.rgb *= (62.0/63.0);
    col.g += (col.a * 1.0/63.0);
    return col;
}

int VRAMRead8(int addr)
{
    ivec2 coord = ivec2(addr & 0x3FF, (addr >> 10) & uVRAMMask);
    int val = int(texelFetch(VRAMTex, coord, 0).r);
    return val;
}

int VRAMRead16(int addr)
{
    ivec2 coord = ivec2(addr & 0x3FF, (addr >> 10) & uVRAMMask);
    int lo = int(texelFetch(VRAMTex, coord, 0).r);
    int hi = int(texelFetch(VRAMTex, coord+ivec2(1,0), 0).r);
    return lo | (hi << 8);
}

vec4 GetSpritePixel(int sprite, ivec2 coord)
{
    vec4 ret;

    if (uOAM[sprite].Type == 0)
    {
        // 16-color

        int tileoffset = uOAM[sprite].TileOffset +
            ((coord.x >> 3) * 32) +
            ((coord.y >> 3) * uOAM[sprite].TileStride) +
            ((coord.x & 0x7) >> 1) +
            ((coord.y & 0x7) << 2);

        int col = VRAMRead8(tileoffset);
        if ((coord.x & 1) != 0)
            col >>= 4;
        else
            col &= 0xF;
        col += uOAM[sprite].PalOffset;

        ret = GetOBJPalEntry(0, col);
        ret.a = ((col & 0xF) == 0) ? 0 : 1;
    }
    else if (uOAM[sprite].Type == 1)
    {
        // 256-color

        int tileoffset = uOAM[sprite].TileOffset +
            ((coord.x >> 3) * 64) +
            ((coord.y >> 3) * uOAM[sprite].TileStride) +
             (coord.x & 0x7) +
            ((coord.y & 0x7) << 3);

        int col = VRAMRead8(tileoffset);

        ret = GetOBJPalEntry(uOAM[sprite].PalOffset, col);
        ret.a = (col == 0) ? 0 : 1;
    }
    else //if (uOAM[sprite].Type == 2)
    {
        // direct color bitmap

        int tileoffset = uOAM[sprite].TileOffset +
            (coord.x * 2) +
            (coord.y * uOAM[sprite].TileStride);

        int col = VRAMRead16(tileoffset);

        ret.r = float((col << 1) & 0x3E) / 63;
        ret.g = float((col >> 4) & 0x3E) / 63;
        ret.b = float((col >> 9) & 0x3E) / 63;
        ret.a = float(col >> 15);
    }

    return ret;
}

void main()
{
    oColor = GetSpritePixel(fSpriteIndex, ivec2(fTexcoord));
}
)"};

const std::string SpriteVS{R"(

struct sOAM
{
    ivec2 Position;
    bvec2 Flip;
    ivec2 Size;
    ivec2 BoundSize;
    int OBJMode;
    int Type;
    int PalOffset;
    int TileOffset;
    int TileStride;
    int Rotscale;
    int BGPrio;
    bool Mosaic;
};

layout(std140, set = 0, binding = 3) uniform ubSpriteConfig
{
    int uVRAMMask;
    ivec4 uRotscale[32];
    sOAM uOAM[128];
};

layout(location = 0) in ivec2 vPosition;
layout(location = 1) in ivec2 vTexcoord;
layout(location = 2) in int vSpriteIndex;

layout(location = 0) flat out int fSpriteIndex;
layout(location = 1) smooth out vec2 fPosition;
layout(location = 2) smooth out vec2 fTexcoord;

void main()
{
    vec2 sprsize = vec2(uOAM[vSpriteIndex].BoundSize);
    vec2 fbsize = vec2(256, 192);

    int totalprio = (uOAM[vSpriteIndex].BGPrio * 128) + vSpriteIndex;
    float z = float(totalprio) / 512.0;
    gl_Position = vec4(((vec2(vPosition) * 2) / fbsize) - 1, z, 1);
    fPosition = vPosition;
    fSpriteIndex = vSpriteIndex;

    if (uOAM[vSpriteIndex].Rotscale == -1)
    {
        vec2 tmp = vec2(vTexcoord) * sprsize;
        fTexcoord = mix(tmp, (sprsize - tmp), uOAM[vSpriteIndex].Flip);
    }
    else
        fTexcoord = (vec2(vTexcoord) * sprsize) - (sprsize / 2);
}
)"};

const std::string SpriteFS{R"(

layout(set = 0, binding = 7) uniform sampler2D SpriteTex;
layout(set = 0, binding = 13) uniform sampler2DArray Capture128Tex;
layout(set = 0, binding = 14) uniform sampler2DArray Capture256Tex;

struct sOAM
{
    ivec2 Position;
    bvec2 Flip;
    ivec2 Size;
    ivec2 BoundSize;
    int OBJMode;
    int Type;
    int PalOffset;
    int TileOffset;
    int TileStride;
    int Rotscale;
    int BGPrio;
    bool Mosaic;
};

layout(std140, set = 0, binding = 3) uniform ubSpriteConfig
{
    int uVRAMMask;
    ivec4 uRotscale[32];
    sOAM uOAM[128];
};

layout(std140, set = 0, binding = 4) uniform ubSpriteScanlineConfig
{
    ivec4 uMosaicLine[48];
};

layout(push_constant) uniform Push2D
{
    int uCurBG;
    int uRenderTransparent;
    int uScaleFactor;
};

layout(location = 0) flat in int fSpriteIndex;
layout(location = 1) smooth in vec2 fPosition;
layout(location = 2) smooth in vec2 fTexcoord;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oFlags;

vec4 GetSpritePixel(int sprite, vec2 coord)
{
    ivec2 basecoord = ivec2((sprite & 0xF) * 64, (sprite >> 4) * 64);

    return texelFetch(SpriteTex, basecoord + ivec2(coord), 0);
}

void main()
{
    vec4 col, flags = vec4(0);
    vec2 coord = fTexcoord;

    if (uOAM[fSpriteIndex].Mosaic)
    {
        int line = int(fPosition.y);
        int mosline = uMosaicLine[line>>2][line&0x3];

        float ymin = 0;
        if (uOAM[fSpriteIndex].Rotscale != -1)
            ymin = -float(uOAM[fSpriteIndex].Size.y) / 2.0;

        float mosy = coord.y - (line - mosline);
        if (coord.y >= ymin)
            coord.y = max(mosy, ymin);
    }

    if (uOAM[fSpriteIndex].Rotscale != -1)
    {
        // rotscale sprite
        // fTexcoord is based on the sprite center

        vec2 sprsize = vec2(uOAM[fSpriteIndex].Size);
        vec4 rotscale = vec4(uRotscale[uOAM[fSpriteIndex].Rotscale]) / 256;
        mat2 rsmatrix = mat2(rotscale.xy, rotscale.zw);
        coord = (coord * rsmatrix) + (sprsize / 2);
        if (any(lessThan(coord, vec2(0)))) discard;
        if (any(greaterThanEqual(coord, sprsize))) discard;
    }

    if (uRenderTransparent != 0)
    {
        // set BG priority and mosaic flags for transparent pixels

        if (uOAM[fSpriteIndex].Mosaic)
            flags.g = 1;

        flags.a = float(uOAM[fSpriteIndex].BGPrio) / 255;

        oColor = vec4(0);
        oFlags = flags;
        return;
    }

    if (uOAM[fSpriteIndex].Type == 3)
    {
        coord += (ivec2(uOAM[fSpriteIndex].TileOffset) >> ivec2(1, 8));
        coord *= (1.0/128.0);
        col = texture(Capture256Tex, vec3(fract(coord), uOAM[fSpriteIndex].TileStride));
    }
    else if (uOAM[fSpriteIndex].Type == 4)
    {
        coord += (ivec2(uOAM[fSpriteIndex].TileOffset) >> ivec2(1, 9));
        coord *= (1.0/256.0);
        col = texture(Capture256Tex, vec3(fract(coord), uOAM[fSpriteIndex].TileStride));
    }
    else
    {
        col = GetSpritePixel(fSpriteIndex, coord);
    }

    if (col.a == 0) discard;

    // oFlags:
    // r = sprite blending flag
    // g = mosaic flag
    // b = OBJ window flag
    // a = BG prio

    if (uOAM[fSpriteIndex].OBJMode == 2)
    {
        // OBJ window
        // OBJ mosaic doesn't apply to "OBJ window" sprites
        flags.b = 1;
    }
    else
    {
        if (uOAM[fSpriteIndex].OBJMode == 1)
        {
            // semi-transparent sprite
            flags.r = 1.0 / 255;
        }
        else if (uOAM[fSpriteIndex].OBJMode == 3)
        {
            // bitmap sprite
            col.a = float(uOAM[fSpriteIndex].PalOffset) / 31;
            flags.r = 2.0 / 255;
        }

        if (uOAM[fSpriteIndex].Mosaic)
            flags.g = 1;

        flags.a = float(uOAM[fSpriteIndex].BGPrio) / 255;
    }

    oColor = col;
    oFlags = flags;
}
)"};

const std::string CompositorVS{R"(

layout(push_constant) uniform Push2D
{
    int uCurBG;
    int uRenderTransparent;
    int uScaleFactor;
};

layout(location = 0) in vec2 vPosition;

layout(location = 0) smooth out vec4 fTexcoord;

void main()
{
    gl_Position = vec4((vPosition * 2) - 1, 0, 1);
    fTexcoord.xy = vPosition * vec2(256, 192);
    fTexcoord.zw = fTexcoord.xy * uScaleFactor;
}
)"};

const std::string CompositorFS{R"(

layout(set = 0, binding = 8) uniform sampler2D BGLayerTex0;
layout(set = 0, binding = 9) uniform sampler2D BGLayerTex1;
layout(set = 0, binding = 10) uniform sampler2D BGLayerTex2;
layout(set = 0, binding = 11) uniform sampler2D BGLayerTex3;
layout(set = 0, binding = 12) uniform sampler2DArray OBJLayerTex;
layout(set = 0, binding = 13) uniform sampler2DArray Capture128Tex;
layout(set = 0, binding = 14) uniform sampler2DArray Capture256Tex;
layout(set = 0, binding = 15) uniform isampler2D MosaicTex;

struct sBGConfig
{
    ivec2 Size;
    int Type;
    int PalOffset;
    int TileOffset;
    int MapOffset;
    bool Clamp;
};

layout(std140, set = 0, binding = 0) uniform ubBGConfig
{
    int uVRAMMask;
    sBGConfig uBGConfig[4];
};

struct sScanline
{
    ivec2 BGOffset[4];
    ivec4 BGRotscale[2];
    int BackColor;
    uint WinRegs;
    int WinMask;
    ivec4 WinPos;
    bvec4 BGMosaicEnable;
    ivec4 MosaicSize;
};

layout(std140, set = 0, binding = 1) uniform ubScanlineConfig
{
    sScanline uScanline[192];
};

layout(std140, set = 0, binding = 2) uniform ubCompositorConfig
{
    ivec4 uBGPrio;
    bool uEnableOBJ;
    bool uEnable3D;
    int uBlendCnt;
    int uBlendEffect;
    ivec3 uBlendCoef;
};

layout(push_constant) uniform Push2D
{
    int uCurBG;
    int uRenderTransparent;
    int uScaleFactor;
};

layout(location = 0) smooth in vec4 fTexcoord;

layout(location = 0) out vec4 oColor;

int MosaicX = 0;

ivec3 ConvertColor(int col)
{
    ivec3 ret;
    ret.r = (col & 0x1F) << 1;
    ret.g = ((col & 0x3E0) >> 4) | (col >> 15);
    ret.b = (col & 0x7C00) >> 9;
    return ret;
}

vec4 SampleBGLayer(int layer, vec2 coord)
{
    switch (layer)
    {
    case 0: return texture(BGLayerTex0, coord);
    case 1: return texture(BGLayerTex1, coord);
    case 2: return texture(BGLayerTex2, coord);
    default: return texture(BGLayerTex3, coord);
    }
}

vec4 BG0Fetch(vec2 coord)
{
    return SampleBGLayer(0, coord);
}

vec4 BG1Fetch(vec2 coord)
{
    return SampleBGLayer(1, coord);
}

vec4 BG2Fetch(vec2 coord)
{
    return SampleBGLayer(2, coord);
}

vec4 BG3Fetch(vec2 coord)
{
    return SampleBGLayer(3, coord);
}

vec4 BG0CalcAndFetch(vec2 coord, int line)
{
    ivec2 bgoffset = uScanline[line].BGOffset[0];
    vec2 bgpos = vec2(bgoffset.xy) + coord;

    if (uScanline[line].BGMosaicEnable[0])
    {
        bgpos = floor(bgpos) - vec2(MosaicX, 0);
    }

    return BG0Fetch(bgpos / vec2(uBGConfig[0].Size));
}

vec4 BG1CalcAndFetch(vec2 coord, int line)
{
    ivec2 bgoffset = uScanline[line].BGOffset[1];
    vec2 bgpos = vec2(bgoffset.xy) + coord;

    if (uScanline[line].BGMosaicEnable[1])
    {
        bgpos = floor(bgpos) - vec2(MosaicX, 0);
    }

    return BG1Fetch(bgpos / vec2(uBGConfig[1].Size));
}

vec4 BG2CalcAndFetch(vec2 coord, int line)
{
    ivec2 bgoffset = uScanline[line].BGOffset[2];
    vec2 bgpos;
    if (uBGConfig[2].Type >= 2)
    {
        // rotscale BG
        bgpos = vec2(bgoffset.xy) / 256;
        vec4 rotscale = vec4(uScanline[line].BGRotscale[0]) / 256;
        mat2 rsmatrix = mat2(rotscale.xy, rotscale.zw);
        bgpos = bgpos + (coord * rsmatrix);
    }
    else
    {
        // text-mode BG
        bgpos = vec2(bgoffset.xy) + coord;
    }

    if (uScanline[line].BGMosaicEnable[2])
    {
        bgpos = floor(bgpos) - vec2(MosaicX, 0);
    }

    if (uBGConfig[2].Type >= 7)
    {
        // hi-res capture
        bgpos.y += uBGConfig[2].MapOffset;
        vec3 capcoord = vec3(bgpos / vec2(uBGConfig[2].Size), uBGConfig[2].TileOffset);

        // due to the possible weirdness of display capture buffers,
        // we need to do custom wraparound handling
        if (uBGConfig[2].Clamp)
        {
            if (any(lessThan(capcoord.xy, vec2(0))) || any(greaterThanEqual(capcoord.xy, vec2(1))))
                return vec4(0);
        }

        if (uBGConfig[2].Type == 7)
            return texture(Capture128Tex, capcoord);
        else
            return texture(Capture256Tex, capcoord);
    }

    return BG2Fetch(bgpos / vec2(uBGConfig[2].Size));
}

vec4 BG3CalcAndFetch(vec2 coord, int line)
{
    ivec2 bgoffset = uScanline[line].BGOffset[3];
    vec2 bgpos;
    if (uBGConfig[3].Type >= 2)
    {
        // rotscale BG
        bgpos = vec2(bgoffset.xy) / 256;
        vec4 rotscale = vec4(uScanline[line].BGRotscale[1]) / 256;
        mat2 rsmatrix = mat2(rotscale.xy, rotscale.zw);
        bgpos = bgpos + (coord * rsmatrix);
    }
    else
    {
        // text-mode BG
        bgpos = vec2(bgoffset.xy) + coord;
    }

    if (uScanline[line].BGMosaicEnable[3])
    {
        bgpos = floor(bgpos) - vec2(MosaicX, 0);
    }

    if (uBGConfig[3].Type >= 7)
    {
        // hi-res capture
        bgpos.y += uBGConfig[3].MapOffset;
        vec3 capcoord = vec3(bgpos / vec2(uBGConfig[3].Size), uBGConfig[3].TileOffset);

        // due to the possible weirdness of display capture buffers,
        // we need to do custom wraparound handling
        if (uBGConfig[3].Clamp)
        {
            if (any(lessThan(capcoord.xy, vec2(0))) || any(greaterThanEqual(capcoord.xy, vec2(1))))
                return vec4(0);
        }

        if (uBGConfig[3].Type == 7)
            return texture(Capture128Tex, capcoord);
        else
            return texture(Capture256Tex, capcoord);
    }

    return BG3Fetch(bgpos / vec2(uBGConfig[3].Size));
}

void CalcSpriteMosaic(in ivec2 coord, out ivec4 objflags, out vec4 objcolor)
{
    for (int i = 0; i < 16; i++)
    {
        ivec2 curpos = ivec2(coord.x - 15 + i, coord.y);

        if (curpos.x < 0)
        {
            objflags = ivec4(0);
            objcolor = vec4(0);
        }
        else
        {
            int mosx = texelFetch(MosaicTex, ivec2(curpos.x, uScanline[curpos.y].MosaicSize.z), 0).r;
            vec4 color = texelFetch(OBJLayerTex, ivec3(curpos * uScaleFactor, 0), 0);
            ivec4 flags = ivec4(texelFetch(OBJLayerTex, ivec3(curpos * uScaleFactor, 1), 0) * 255.0);

            bool latch = false;
            if (mosx == 0)
                latch = true;
            else if (flags.g == 0)
                latch = true;
            else if (objflags.g == 0)
                latch = true;
            else if (flags.a < objflags.a)
                latch = true;

            if (latch)
            {
                objflags = flags;
                objcolor = color;
            }
        }
    }
}

vec4 CompositeLayers()
{
    ivec2 coord = ivec2(fTexcoord.zw);
    vec2 bgcoord = vec2(fTexcoord.x, fract(fTexcoord.y));
    int xpos = int(fTexcoord.x);
    int line = int(fTexcoord.y);

    if (uScanline[line].MosaicSize.x > 0)
        MosaicX = texelFetch(MosaicTex, ivec2(bgcoord.x, uScanline[line].MosaicSize.x), 0).r;

    ivec4 col1 = ivec4(ConvertColor(uScanline[line].BackColor), 0x20);
    int mask1 = 0x20;
    ivec4 col2 = ivec4(0);
    int mask2 = 0;
    bool specialcase = false;

    vec4 layercol[6];
    layercol[0] = BG0CalcAndFetch(bgcoord, line);
    layercol[1] = BG1CalcAndFetch(bgcoord, line);
    layercol[2] = BG2CalcAndFetch(bgcoord, line);
    layercol[3] = BG3CalcAndFetch(bgcoord, line);

    ivec4 objflags;
    if (uScanline[line].MosaicSize.z > 0)
    {
        CalcSpriteMosaic(ivec2(fTexcoord.xy), objflags, layercol[4]);
    }
    else
    {
        layercol[4] = texelFetch(OBJLayerTex, ivec3(coord, 0), 0);
        layercol[5] = texelFetch(OBJLayerTex, ivec3(coord, 1), 0);
        objflags = ivec4(layercol[5] * 255.0);
    }

    int winmask = uScanline[line].WinMask;
    bool inside_win0, inside_win1;

    if (xpos < uScanline[line].WinPos[0])
        inside_win0 = ((winmask & (1<<0)) != 0);
    else if (xpos < uScanline[line].WinPos[1])
        inside_win0 = ((winmask & (1<<1)) != 0);
    else
        inside_win0 = ((winmask & (1<<2)) != 0);

    if (xpos < uScanline[line].WinPos[2])
        inside_win1 = ((winmask & (1<<3)) != 0);
    else if (xpos < uScanline[line].WinPos[3])
        inside_win1 = ((winmask & (1<<4)) != 0);
    else
        inside_win1 = ((winmask & (1<<5)) != 0);

    uint winregs = uScanline[line].WinRegs;
    uint winsel = winregs;
    if (objflags.b > 0)
        winsel = winregs >> 8;
    if (inside_win1)
        winsel = winregs >> 16;
    if (inside_win0)
        winsel = winregs >> 24;

    for (int prio = 3; prio >= 0; prio--)
    {
        for (int bg = 3; bg >= 0; bg--)
        {
            if ((uBGPrio[bg] == prio) && (layercol[bg].a > 0) && ((winsel & (1u << bg)) != 0u))
            {
                col2 = col1;
                mask2 = mask1 << 8;
                col1 = ivec4(layercol[bg] * 255.0) >> ivec4(2,2,2,3);
                mask1 = (1 << bg);
                specialcase = (bg == 0) && uEnable3D;
            }
        }

        if (uEnableOBJ && (objflags.a == prio) && (layercol[4].a > 0) && ((winsel & (1u << 4)) != 0u))
        {
            col2 = col1;
            mask2 = mask1 << 8;
            col1 = ivec4(layercol[4] * 255.0) >> ivec4(2,2,2,3);
            mask1 = (1 << 4);
            specialcase = (objflags.r != 0);
        }
    }

    int effect = 0;
    int eva, evb, evy = uBlendCoef[2];

    if (specialcase && (uBlendCnt & mask2) != 0)
    {
        if (mask1 == (1<<0))
        {
            // 3D layer blending
            effect = 4;
            eva = (col1.a & 0x1F) + 1;
            evb = 32 - eva;
        }
        else if (objflags.r == 1)
        {
            // semi-transparent sprite
            effect = 1;
            eva = uBlendCoef[0];
            evb = uBlendCoef[1];
        }
        else //if (objflags.r == 2)
        {
            // bitmap sprite
            effect = 1;
            eva = col1.a;
            evb = 16 - eva;
        }
    }
    else if (((uBlendCnt & mask1) != 0) && ((winsel & (1u << 5)) != 0u))
    {
        effect = uBlendEffect;
        if (effect == 1)
        {
            if ((uBlendCnt & mask2) != 0)
            {
                eva = uBlendCoef[0];
                evb = uBlendCoef[1];
            }
            else
                effect = 0;
        }
    }

    if (effect == 1)
    {
        // blending
        col1 = ((col1 * eva) + (col2 * evb) + 0x8) >> 4;
        col1 = min(col1, 0x3F);
    }
    else if (effect == 2)
    {
        // brightness up
        col1 = col1 + ((((0x3F - col1) * evy) + 0x8) >> 4);
    }
    else if (effect == 3)
    {
        // brightness down
        col1 = col1 - (((col1 * evy) + 0x7) >> 4);
    }
    else if (effect == 4)
    {
        // 3D layer blending
        col1 = ((col1 * eva) + (col2 * evb) + 0x10) >> 5;
    }

    return vec4(vec3(col1.rgb << 2) / 255.0, 1);
}

void main()
{
    oColor = CompositeLayers();
}
)"};

}

}

#endif
