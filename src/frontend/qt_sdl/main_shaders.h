/*
    Copyright 2016-2020 Arisotura

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

uniform vec2 uScreenSize;
uniform mat2x3 uTransform;

in vec2 vPosition;
in vec2 vTexcoord;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;

    fpos.xy = vec3(vPosition, 1.0) * uTransform;

    fpos.xy = ((fpos.xy * 2.0) / uScreenSize) - 1.0;
    fpos.y *= -1;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
    fTexcoord = vTexcoord;
}
)";

const char* kScreenFS = R"(#version 140

uniform sampler2D ScreenTex;

smooth in vec2 fTexcoord;

out vec4 oColor;

void main()
{
    vec4 pixel = texture(ScreenTex, fTexcoord);

    oColor = vec4(pixel.bgr, 1.0);
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
