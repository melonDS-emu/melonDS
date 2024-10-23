/*
    Copyright 2016-2024 melonDS team

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

const char* kScreenVS_OSD = R"(#version 140

uniform vec2 uScreenSize;

uniform ivec2 uOSDPos;
uniform ivec2 uOSDSize;
uniform float uScaleFactor;

in vec2 vPosition;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;

    vec2 osdpos = (vPosition * vec2(uOSDSize * uScaleFactor));
    fTexcoord = osdpos;
    osdpos += uOSDPos;

    fpos.xy = ((osdpos * 2.0) / uScreenSize * uScaleFactor) - 1.0;
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

//Fragment Shader for overlay copied from melonPrimeDS project.
const char* kScreenFS_overlay = R"(#version 140

uniform sampler2D OverlayTex;

smooth in vec2 fTexcoord;

uniform vec2 uOverlayPos;
uniform vec2 uOverlaySize;
uniform int uOverlayScreenType;

out vec4 oColor;

void main()
{
    const vec2 dsSize = vec2(256.0, 193.0); // +1 on y for pixel gap

    vec2 uv = fTexcoord * vec2(1.0, 2.0);

    if (uOverlayScreenType < 1) 
    {
        // top screen
        uv -= uOverlayPos / dsSize;
        uv *= dsSize / uOverlaySize;
    } else {
        // bottom screen
        uv -= vec2(0.0, 1.0);
        uv -= (uOverlayPos + vec2(0.0, 1.0)) / dsSize;
        uv *= dsSize / uOverlaySize;
    }

    vec4 pixel = texture(OverlayTex, uv);
    pixel.rgb *= pixel.a;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) 
    {
        oColor = vec4(0.0, 0.0, 0.0, 0.0);
    } else {
        oColor = pixel.bgra;
    }
}
)";


#endif // OSD_SHADERS_H
