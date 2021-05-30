/*
    Copyright 2016-2021 Arisotura

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

#ifndef OSD_SHADERS_H
#define OSD_SHADERS_H

#define kShaderHeader "#version 300 es\nprecision mediump float;"

const char* kScreenVS_OSD = kShaderHeader R"(

uniform vec2 uScreenSize;

uniform ivec2 uOSDPos;
uniform vec2 uOSDSize;
uniform float uScaleFactor;

in vec2 vPosition;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;

    vec2 osdpos = (vPosition * vec2(uOSDSize * uScaleFactor));
    fTexcoord = osdpos;
    osdpos += vec2(uOSDPos);

    fpos.xy = ((osdpos * 2.0) / uScreenSize * uScaleFactor) - 1.0;
    fpos.y *= -1.0;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
}
)";

const char* kScreenFS_OSD = kShaderHeader R"(

uniform sampler2D OSDTex;

smooth in vec2 fTexcoord;

layout (location = 0) out vec4 oColor;

void main()
{
    vec4 pixel = texelFetch(OSDTex, ivec2(fTexcoord), 0);
    oColor = pixel.bgra;
}
)";

#endif // OSD_SHADERS_H
