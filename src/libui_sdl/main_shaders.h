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
    fpos.xy = ((vPosition.xy * 2.0) / uScreenSize) - 1.0;
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
uniform sampler2D _3DTex;

smooth in vec2 fTexcoord;

out vec4 oColor;

void main()
{
    ivec4 pixel = ivec4(texelFetch(ScreenTex, ivec2(fTexcoord), 0));

    // bit0-13: BLDCNT
    // bit14-15: DISPCNT display mode
    // bit16-20: EVA
    // bit21-25: EVB
    // bit26-30: EVY
    ivec4 ctl = ivec4(texelFetch(ScreenTex, ivec2(256*3, int(fTexcoord.y)), 0));
    int dispmode = (ctl.g >> 6) & 0x3;

    if (dispmode == 1)
    {
        int eva = ctl.b & 0x1F;
        int evb = (ctl.b >> 5) | ((ctl.a & 0x03) << 3);
        int evy = ctl.a >> 2;

        ivec4 top = pixel;
        ivec4 mid = ivec4(texelFetch(ScreenTex, ivec2(fTexcoord) + ivec2(256,0), 0));
        ivec4 bot = ivec4(texelFetch(ScreenTex, ivec2(fTexcoord) + ivec2(512,0), 0));

        int winmask = top.b >> 7;

        if ((top.a & 0x40) != 0)
        {
            float xpos = top.r + fract(fTexcoord.x);
            float ypos = mod(fTexcoord.y, 768);
            ivec4 _3dpix = ivec4(texelFetch(_3DTex, ivec2(vec2(xpos, ypos)*u3DScale), 0).bgra
                         * vec4(63,63,63,31));

            if (_3dpix.a > 0) { top = _3dpix; top.a |= 0x40; bot = mid; }
            else              top = mid;
        }
        else if ((mid.a & 0x40) != 0)
        {
            float xpos = mid.r + fract(fTexcoord.x);
            float ypos = mod(fTexcoord.y, 768);
            ivec4 _3dpix = ivec4(texelFetch(_3DTex, ivec2(vec2(xpos, ypos)*u3DScale), 0).bgra
                         * vec4(63,63,63,31));

            if (_3dpix.a > 0) { bot = _3dpix; bot.a |= 0x40; }
        }
        else
        {
            // conditional texture fetch no good for performance, apparently
            //texelFetch(_3DTex, ivec2(0, fTexcoord.y*2), 0);
            bot = mid;
        }

        top.b &= 0x3F;
        bot.b &= 0x3F;

        int target2;
        if      ((bot.a & 0x80) != 0) target2 = 0x10;
        else if ((bot.a & 0x40) != 0) target2 = 0x01;
        else                          target2 = bot.a;
        bool t2pass = ((ctl.g & target2) != 0);

        int coloreffect = 0;

        if ((top.a & 0x80) != 0 && t2pass)
        {
            // sprite blending

            coloreffect = 1;

            if ((top.a & 0x40) != 0)
            {
                eva = top.a & 0x1F;
                evb = 16 - eva;
            }
        }
        else if ((top.a & 0x40) != 0 && t2pass)
        {
            // 3D layer blending

            coloreffect = 4;
            eva = (top.a & 0x1F) + 1;
            evb = 32 - eva;
        }
        else
        {
            if      ((top.a & 0x80) != 0) top.a = 0x10;
            else if ((top.a & 0x40) != 0) top.a = 0x01;

            if ((ctl.r & top.a) != 0 && winmask != 0)
            {
                int effect = ctl.r >> 6;
                if ((effect != 1) || t2pass) coloreffect = effect;
            }
        }

        if (coloreffect == 0)
        {
            pixel = top;
        }
        else if (coloreffect == 1)
        {
            pixel = ((top * eva) + (bot * evb)) >> 4;
            pixel = min(pixel, 0x3F);
        }
        else if (coloreffect == 2)
        {
            pixel = top;
            pixel += ((ivec4(0x3F,0x3F,0x3F,0) - pixel) * evy) >> 4;
        }
        else if (coloreffect == 3)
        {
            pixel = top;
            pixel -= (pixel * evy) >> 4;
        }
        else
        {
            pixel = ((top * eva) + (bot * evb)) >> 5;
            if (eva <= 16) pixel += ivec4(1,1,1,0);
            pixel = min(pixel, 0x3F);
        }
    }

    pixel.rgb <<= 2;
    pixel.rgb |= (pixel.rgb >> 6);

    // TODO: filters

    oColor = vec4(vec3(pixel.rgb) / 255.0, 1.0);
}
)";

#endif // MAIN_SHADERS_H
