/*
    Copyright 2016-2025 melonDS team

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

#pragma once

namespace melonDS::GPU2D
{

const char* kLayersVS = R"(#version 140

in vec2 vPosition;
in vec2 vTexcoord;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;
    fpos.xy = vPosition;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
    fTexcoord = vTexcoord;
}
)";

const char* kLayersFS = R"(#version 140

// uniform shit goes here

smooth in vec2 fTexcoord;

// TODO have more so we can render more layers
out vec4 oBG0Color;

void main()
{
    oBG0Color = vec4(0,1,0,1);
}
)";



const char* kFinalPassVS = R"(#version 140

in vec2 vPosition;
in vec2 vTexcoord;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;
    fpos.xy = vPosition;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
    fTexcoord = vTexcoord;
}
)";

const char* kFinalPassFS = R"(#version 140

uniform uint u3DScale;
uniform int uCaptureReg;
uniform int uCaptureVRAMMask;

uniform sampler2D _3DTex;
uniform usampler1D LineAttribTex;
uniform usampler2D BGOBJTex;
uniform sampler2DArray AuxInputTex;
uniform sampler2D CaptureTex[16];

smooth in vec2 fTexcoord;

out vec4 oTopColor;
out vec4 oBottomColor;
out vec4 oCaptureColor;

vec4 SampleCaptureTex(int index, ivec2 pos)
{
    // blarg
    if (index < 8)
    {
        if (index < 4)
        {
            if (index < 2)
            {
                if (index == 0)
                    return texelFetch(CaptureTex[0], pos, 0);
                else
                    return texelFetch(CaptureTex[1], pos, 0);
            }
            else
            {
                if (index == 2)
                    return texelFetch(CaptureTex[2], pos, 0);
                else
                    return texelFetch(CaptureTex[3], pos, 0);
            }
        }
        else
        {
            if (index < 6)
            {
                if (index == 4)
                    return texelFetch(CaptureTex[4], pos, 0);
                else
                    return texelFetch(CaptureTex[5], pos, 0);
            }
            else
            {
                if (index == 6)
                    return texelFetch(CaptureTex[6], pos, 0);
                else
                    return texelFetch(CaptureTex[7], pos, 0);
            }
        }
    }
    else
    {
        if (index < 12)
        {
            if (index < 10)
            {
                if (index == 8)
                    return texelFetch(CaptureTex[8], pos, 0);
                else
                    return texelFetch(CaptureTex[9], pos, 0);
            }
            else
            {
                if (index == 10)
                    return texelFetch(CaptureTex[10], pos, 0);
                else
                    return texelFetch(CaptureTex[11], pos, 0);
            }
        }
        else
        {
            if (index < 14)
            {
                if (index == 12)
                    return texelFetch(CaptureTex[12], pos, 0);
                else
                    return texelFetch(CaptureTex[13], pos, 0);
            }
            else
            {
                if (index == 14)
                    return texelFetch(CaptureTex[14], pos, 0);
                else
                    return texelFetch(CaptureTex[15], pos, 0);
            }
        }
    }
}

ivec4 ResolveColor(ivec4 col, ivec4 col_3d)
{
    // TODO: for rotscale-able layers, the fractional position should be calculated
    // based on the rotscale params
    // this is a job for the next iteration, the "proper" renderer

    ivec4 ret;
    if ((col.a & 0x9F) == 0x80)
    {
        // 3D layer
        ret = col_3d;
        return ret;
    }
    else if ((col.a & 0x80) == 0x80)
    {
        // BG/OBJ capture
        int block = col.b & 0xF;
        ivec2 pos = ivec2((vec2(col.rg) + fract(fTexcoord.xy)) * u3DScale);
        //ret = ivec4(texelFetch(CaptureTex[block], pos, 0).bgra * vec4(63,63,63,31));
        ret = ivec4(SampleCaptureTex(block, pos).bgra * vec4(63,63,63,31));
    }
    else
    {
        ret.r = (col.r << 1);
        ret.g = (col.r >> 4) | (col.g << 4);
        ret.b = (col.g >> 1);
        ret.rgb = ret.rgb & 0x3E;
    }
    if ((col.a & 0x1F) == 0x14)
        ret.a = (col.b >> 4) + 1;
    else
        ret.a = 31;
    return ret;
}

ivec4 CalcScreenPixel(int screen, ivec4 attrib, ivec4 col_3d)
{
    ivec4 bgobj;

    ivec2 coord = ivec2(fTexcoord) + ivec2(0, screen*192);
    ivec4 val1 = ivec4(texelFetch(BGOBJTex, coord, 0));
    ivec4 val2 = ivec4(texelFetch(BGOBJTex, coord + ivec2(256, 0), 0));
    ivec4 val3 = ivec4(texelFetch(BGOBJTex, coord + ivec2(512, 0), 0));

    int flags1 = val1.a;
    int flags2 = val2.a;
    int flags3 = val3.a;
    bool colwin = (flags1 & 0x40) == 0x40;

    val1 = ResolveColor(val1, col_3d);
    val2 = ResolveColor(val2, col_3d);
    val3 = ResolveColor(val3, col_3d);

    // select the two topmost pixels

    if (val1.a == 0)
    {
        val1 = val2; flags1 = flags2;
        val2 = val3; flags2 = flags3;
    }
    else if (val2.a == 0)
    {
        val2 = val3; flags2 = flags3;
    }

    // apply color effect

    bool sel1 = (attrib.g & (1 << (flags1 & 0x7))) != 0;
    bool sel2 = (attrib.g & (0x100 << (flags2 & 0x7))) != 0;
    int coleffect = (attrib.g >> 6) & 0x3;
    int eva = (attrib.g >> 16) & 0xFF;
    int evb = (attrib.g >> 24) & 0xFF;
    int evy = (attrib.b >> 8) & 0xFF;

    if (((flags1 & 0x9F) == 0x80) && sel2)
    {
        // 3D layer blending
        eva = (val1.a & 0x1F) + 1;
        evb = 32 - eva;

        bgobj = ((val1 * eva) + (val2 * evb) + 0x10) >> 5;
        //bgobj = min(bgobj, 0x3F);
    }
    else if (((flags1 & 0x1F) == 0x14) && sel2)
    {
        // mode3 sprite blending
        eva = val1.a;
        evb = 16 - eva;

        bgobj = ((val1 * eva) + (val2 * evb) + 0x8) >> 4;
        //bgobj = min(bgobj, 0x3F);
    }
    else if (((flags1 & 0x1F) == 0x0C) && sel2)
    {
        // mode1 sprite blending
        bgobj = ((val1 * eva) + (val2 * evb) + 0x8) >> 4;
        bgobj = min(bgobj, 0x3F);
    }
    else if ((coleffect == 1) && sel1 && sel2 && colwin)
    {
        // regular blending
        bgobj = ((val1 * eva) + (val2 * evb) + 0x8) >> 4;
        bgobj = min(bgobj, 0x3F);
    }
    else if ((coleffect == 2) && sel1 && colwin)
    {
        // brightness up
        bgobj = val1 + ((((0x3F - val1) * evy) + 0x8) >> 4);
    }
    else if ((coleffect == 3) && sel1 && colwin)
    {
        // brightness down
        bgobj = val1 - (((val1 * evy) + 0x7) >> 4);
    }
    else
    {
        bgobj = val1;
    }

    return bgobj;
}

ivec3 MasterBrightness(ivec3 color, int regval)
{
    int brightmode = (regval >> 6) & 0x3;
    int evy = regval & 0x1F;
    if (evy > 16) evy = 16;

    if (brightmode == 1)
    {
        // up
        color += (((0x3F - color) * evy) >> 4);
    }
    else if (brightmode == 2)
    {
        // down
        color -= (((color * evy) + 0xF) >> 4);
    }

    return color;
}

void main()
{
    ivec4 attrib_main = ivec4(texelFetch(LineAttribTex, int(fTexcoord.y), 0));
    ivec4 attrib_sub = ivec4(texelFetch(LineAttribTex, int(fTexcoord.y) + 192, 0));

    int bg0hofs = (attrib_main.b >> 16) & 0x1FF;
    if ((bg0hofs & 0x100) != 0) bg0hofs -= 512;
    vec2 pos_3d = fTexcoord.xy + vec2(float(bg0hofs), 0);

    ivec4 col_3d = ivec4(texelFetch(_3DTex, ivec2(pos_3d * u3DScale), 0).bgra * vec4(63,63,63,31));

    ivec4 col_main = CalcScreenPixel(0, attrib_main, col_3d);
    ivec4 col_sub = CalcScreenPixel(1, attrib_sub, ivec4(0));

    ivec3 output_main, output_sub;
    int dispmode_main = (attrib_main.r >> 16) & 0x3;
    int dispmode_sub = (attrib_sub.r >> 16) & 0x1;

    // TODO not always sample those? (VRAM/FIFO)

    int capblock = 0;
    if (dispmode_main != 2)
        capblock = ((uCaptureReg >> 26) & 0x3);
    //capblock += int(fTexcoord.y / 64);
    capblock = (capblock & 0x3) | (((attrib_main.r >> 18) & 0x3) << 2);

    vec4 tmp_vram;
    if ((uCaptureVRAMMask & (1 << capblock)) != 0)
        tmp_vram = SampleCaptureTex(capblock, ivec2(fTexcoord.xy * u3DScale));
    else
        tmp_vram = texelFetch(AuxInputTex, ivec3(ivec2(fTexcoord.xy), 0), 0);

    ivec4 col_vram = ivec4(tmp_vram * vec4(63,63,63,31));
    ivec4 col_fifo = ivec4(texelFetch(AuxInputTex, ivec3(ivec2(fTexcoord.xy), 0), 1) * vec4(63,63,63,31));

    if (dispmode_main == 0)
    {
        // screen disabled (white)
        output_main = ivec3(63, 63, 63);
    }
    else if (dispmode_main == 1)
    {
        // BG/OBJ layers
        output_main = col_main.rgb;
    }
    else if (dispmode_main == 2)
    {
        // VRAM display
        output_main = col_vram.rgb;
    }
    else //if (dispmode_main == 3)
    {
        // mainmem FIFO
        output_main = col_fifo.rgb;
    }

    if (dispmode_sub == 0)
    {
        // screen disabled (white)
        output_sub = ivec3(63, 63, 63);
    }
    else //if (dispmode_sub == 1)
    {
        // BG/OBJ layers
        output_sub = col_sub.rgb;
    }

    if (dispmode_main != 0)
        output_main = MasterBrightness(output_main, attrib_main.b);
    if (dispmode_sub != 0)
        output_sub = MasterBrightness(output_sub, attrib_sub.b);

    if ((attrib_main.b & (1<<31)) != 0)
    {
        oTopColor = vec4(vec3(output_sub.bgr) / 63.0, 1.0);
        oBottomColor = vec4(vec3(output_main.bgr) / 63.0, 1.0);
    }
    else
    {
        oTopColor = vec4(vec3(output_main.bgr) / 63.0, 1.0);
        oBottomColor = vec4(vec3(output_sub.bgr) / 63.0, 1.0);
    }

    if ((uCaptureReg & (1<<31)) != 0)
    {
        int capwidth, capheight;
        int capsize = (uCaptureReg >> 20) & 0x3;
        if (capsize == 0)
        {
            capwidth = 128;
            capheight = 128;
        }
        else
        {
            capwidth = 256;
            capheight = 64 * capsize;
        }

        if ((fTexcoord.x < capwidth) && (fTexcoord.y < capheight))
        {
            ivec4 srcA;
            if ((uCaptureReg & (1<<24)) != 0)
                srcA = col_3d;
            else
                srcA = col_main;

            ivec4 srcB;
            if ((uCaptureReg & (1<<25)) != 0)
                srcB = col_fifo;
            else
                srcB = col_vram;

            ivec4 cap_out;
            int srcsel = (uCaptureReg >> 29) & 0x3;
            if (srcsel == 0)
                cap_out = srcA;
            else if (srcsel == 1)
                cap_out = srcB;
            else
            {
                int eva = min(uCaptureReg & 0x1F, 16);
                int evb = min((uCaptureReg >> 8) & 0x1F, 16);

                int aa = (srcA.a > 0) ? 1 : 0;
                int ab = (srcB.a > 0) ? 1 : 0;

                cap_out.rgb = ((srcA.rgb * aa * eva) + (srcB.rgb * ab * evb) + 0x8) >> 4;
                cap_out.rgb = min(cap_out.rgb, 0x3F);
                cap_out.a = (eva>0 ? aa : 0) | (evb>0 ? ab : 0);
            }

            oCaptureColor = vec4(vec3(cap_out.bgr) / 63.0, (cap_out.a>0) ? 1.0 : 0.0);
        }
    }
}
)";

}
