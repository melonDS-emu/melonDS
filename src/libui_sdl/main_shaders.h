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

const char* kScreenVS = R"(#version 420

layout(std140, binding=0) uniform uConfig
{
    vec2 uScreenSize;
    uint uFilterMode;
};

layout(location=0) in vec2 vPosition;
layout(location=1) in vec2 vTexcoord;

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

const char* kScreenFS = R"(#version 420

layout(std140, binding=0) uniform uConfig
{
    vec2 uScreenSize;
    uint uFilterMode;
};

layout(binding=0) uniform usampler2D ScreenTex;

smooth in vec2 fTexcoord;

layout(location=0) out vec4 oColor;

void main()
{
    uvec4 pixel = texelFetch(ScreenTex, ivec2(fTexcoord), 0);

    // TODO: filters

    oColor = vec4(vec3(pixel.bgr) / 255.0, 1.0);
}
)";

#endif // MAIN_SHADERS_H
