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
const char* kFinalPassVS = R"(#version 140

in vec2 vPosition;
in vec3 vTexcoord;

smooth out vec3 fTexcoord;

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

uniform sampler2D _3DTex;
uniform usampler1D LineAttribTex;
uniform usampler2D BGOBJTex;

smooth in vec3 fTexcoord;

out vec4 oColor;

ivec4 ResolveColor(ivec4 col, ivec4 col_3d)
{
    ivec4 ret;
    if ((col.a & 0x80) == 0x80)
    {
        // hi-res placeholder
        // TODO
        ret = col_3d;
        return ret;
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

void main()
{
    ivec4 attrib = ivec4(texelFetch(LineAttribTex, int(fTexcoord.y), 0));

    // FFD2 -> 1D2 -> -46
    // FFFF -> 1FF -> -1
    int bg0hofs = (attrib.b >> 16) & 0x1FF;
    if ((bg0hofs & 0x100) != 0) bg0hofs -= 512;
    vec2 pos_3d = fTexcoord.xz + vec2(float(bg0hofs), 0);

    ivec4 col_3d;
    if ((attrib.b & (1<<31)) != 0)
        col_3d = ivec4(texelFetch(_3DTex, ivec2(pos_3d * u3DScale), 0).bgra * vec4(63,63,63,31));
    else
        col_3d = ivec4(0);

    ivec4 bgobj;
    {
        ivec4 val1 = ivec4(texelFetch(BGOBJTex, ivec2(fTexcoord), 0));
        ivec4 val2 = ivec4(texelFetch(BGOBJTex, ivec2(fTexcoord) + ivec2(256, 0), 0));
        ivec4 val3 = ivec4(texelFetch(BGOBJTex, ivec2(fTexcoord) + ivec2(512, 0), 0));

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
    }

    // TODO do capture and shit here

    int dispmode = (attrib.r >> 16) & 0x3;
    ivec3 col_out;
    if (dispmode == 0)
    {
        // screen disabled (white)
        col_out = ivec3(63, 63, 63);
    }
    else if (dispmode == 1)
    {
        // BG/OBJ layers
        col_out = bgobj.rgb;
    }
    else if (dispmode == 2)
    {
        // VRAM display
        //
        col_out = ivec3(63, 63, 0);
    }
    else //if (dispmode == 3)
    {
        // mainmem FIFO
        //
        col_out = ivec3(0, 63, 63);
    }

    if (dispmode != 0)
    {
        // master brightness
        int brightmode = (attrib.b >> 6) & 0x3;
        int evy = attrib.b & 0x1F;
        if (evy > 16) evy = 16;

        if (brightmode == 1)
        {
            // up
            col_out += (((0x3F - col_out) * evy) >> 4);
        }
        else if (brightmode == 2)
        {
            // down
            col_out -= (((col_out * evy) + 0xF) >> 4);
        }
    }

    oColor = vec4(vec3(col_out.bgr) / 63.0, 1.0);
    return;
}
)";

}
