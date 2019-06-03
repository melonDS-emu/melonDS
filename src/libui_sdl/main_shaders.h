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

#ifndef MAIN_SHADERS_H
#define MAIN_SHADERS_H

const char* kScreenVS = R"(#version 140

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    uint u3DScale;
    uint uFilterMode;
};

in vec2 vPosition;
in vec2 vTexcoord;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;
    fpos.xy = ((vPosition * 2.0) / uScreenSize) - 1.0;
    fpos.y *= -1;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
    fTexcoord = vTexcoord;
}
)";

const char* kScreenFS = R"(#version 140

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    uint u3DScale;
    uint uFilterMode;
};

uniform usampler2D ScreenTex;

smooth in vec2 fTexcoord;

out vec4 oColor;

void main()
{
    ivec4 pixel = ivec4(texelFetch(ScreenTex, ivec2(fTexcoord), 0));

    // TODO: filters

    oColor = vec4(vec3(pixel.bgr) / 255.0, 1.0);
}
)";

const char* kScreenFS_Accel = R"(#version 140

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    uint u3DScale;
    uint uFilterMode;
};

uniform usampler2D ScreenTex;
uniform sampler2D _3DTex;

smooth in vec2 fTexcoord;

out vec4 oColor;

void main()
{
    ivec4 pixel = ivec4(texelFetch(ScreenTex, ivec2(fTexcoord), 0));

    ivec4 mbright = ivec4(texelFetch(ScreenTex, ivec2(256*3, int(fTexcoord.y)), 0));
    int dispmode = mbright.b & 0x3;

    if (dispmode == 1)
    {
        ivec4 val1 = pixel;
        ivec4 val2 = ivec4(texelFetch(ScreenTex, ivec2(fTexcoord) + ivec2(256,0), 0));
        ivec4 val3 = ivec4(texelFetch(ScreenTex, ivec2(fTexcoord) + ivec2(512,0), 0));

        int compmode = val3.a & 0xF;
        int eva, evb, evy;

        if (compmode == 4)
        {
            // 3D on top, blending

            float xpos = val3.r + fract(fTexcoord.x);
            float ypos = mod(fTexcoord.y, 192);
            ivec4 _3dpix = ivec4(texelFetch(_3DTex, ivec2(vec2(xpos, ypos)*u3DScale), 0).bgra
                         * vec4(63,63,63,31));

            if (_3dpix.a > 0)
            {
                eva = (_3dpix.a & 0x1F) + 1;
                evb = 32 - eva;

                val1 = ((_3dpix * eva) + (val1 * evb)) >> 5;
                if (eva <= 16) val1 += ivec4(1,1,1,0);
                val1 = min(val1, 0x3F);
            }
            else
                val1 = val2;
        }
        else if (compmode == 1)
        {
            // 3D on bottom, blending

            float xpos = val3.r + fract(fTexcoord.x);
            float ypos = mod(fTexcoord.y, 192);
            ivec4 _3dpix = ivec4(texelFetch(_3DTex, ivec2(vec2(xpos, ypos)*u3DScale), 0).bgra
                         * vec4(63,63,63,31));

            if (_3dpix.a > 0)
            {
                eva = val3.g;
                evb = val3.b;

                val1 = ((val1 * eva) + (_3dpix * evb)) >> 4;
                val1 = min(val1, 0x3F);
            }
            else
                val1 = val2;
        }
        else if (compmode <= 3)
        {
            // 3D on top, normal/fade

            float xpos = val3.r + fract(fTexcoord.x);
            float ypos = mod(fTexcoord.y, 192);
            ivec4 _3dpix = ivec4(texelFetch(_3DTex, ivec2(vec2(xpos, ypos)*u3DScale), 0).bgra
                         * vec4(63,63,63,31));

            if (_3dpix.a > 0)
            {
                evy = val3.g;

                val1 = _3dpix;
                if      (compmode == 2) val1 += ((ivec4(0x3F,0x3F,0x3F,0) - val1) * evy) >> 4;
                else if (compmode == 3) val1 -= (val1 * evy) >> 4;
            }
            else
                val1 = val2;
        }

        pixel = val1;
    }

    if (dispmode != 0)
    {
        int brightmode = mbright.g >> 6;
        if (brightmode == 1)
        {
            // up
            int evy = mbright.r & 0x1F;
            if (evy > 16) evy = 16;

            pixel += ((ivec4(0x3F,0x3F,0x3F,0) - pixel) * evy) >> 4;
        }
        else if (brightmode == 2)
        {
            // down
            int evy = mbright.r & 0x1F;
            if (evy > 16) evy = 16;

            pixel -= (pixel * evy) >> 4;
        }
    }

    pixel.rgb <<= 2;
    pixel.rgb |= (pixel.rgb >> 6);

    // TODO: filters

    oColor = vec4(vec3(pixel.rgb) / 255.0, 1.0);
}
)";


const char* kScreenVS_OSD = R"(#version 140

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    uint u3DScale;
    uint uFilterMode;
};

uniform ivec2 uOSDPos;
uniform ivec2 uOSDSize;

in vec2 vPosition;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;

    vec2 osdpos = (vPosition * vec2(uOSDSize));
    fTexcoord = osdpos;
    osdpos += uOSDPos;

    fpos.xy = ((osdpos * 2.0) / uScreenSize) - 1.0;
    fpos.y *= -1;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
}
)";

const char* kScreenFS_OSD = R"(#version 140

uniform sampler2D OSDTex;

smooth in vec2 fTexcoord;

out vec4 oColor;

void main()
{
    vec4 pixel = texelFetch(OSDTex, ivec2(fTexcoord), 0);
    oColor = pixel.bgra;
}
)";

#endif // MAIN_SHADERS_H
