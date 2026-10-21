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

#ifndef MAIN_SHADERS_H
#define MAIN_SHADERS_H

const char* kScreenVS = R"(#version 140

uniform vec2 uScreenSize;
uniform mat2x3 uTransform;

in vec2 vPosition;
in vec3 vTexcoord;

smooth out vec3 fTexcoord;
out vec2 pixelUnit;
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
    pixelUnit.x = 1/256.0;
    pixelUnit.y = 1/192.0;
}
)";

const char* kScreenFS = R"(#version 140

uniform sampler2DArray ScreenTex;
uniform vec2 cursorPos;
uniform bool cursorEnable;
smooth in vec3 fTexcoord;
in vec2 pixelUnit;
out vec4 oColor;

void main()
{
    vec4 pixel = texture(ScreenTex, fTexcoord);
    if (cursorEnable){
        //Black Outline
        if (fTexcoord.x <= (cursorPos.x + (2.0*pixelUnit.x)) && 
            fTexcoord.x >= (cursorPos.x - (1.0*pixelUnit.x))) {
            if (fTexcoord.y <= (cursorPos.y + (5.0*pixelUnit.y)) && 
                fTexcoord.y >= (cursorPos.y - (4.0*pixelUnit.y))) {
                pixel = vec4(0.0, 0.0, 0.0, 1.0);
            }
        }

        if (fTexcoord.y <= (cursorPos.y + (2.0*pixelUnit.y)) && 
            fTexcoord.y >= (cursorPos.y - (1.0*pixelUnit.y))) {
            if (fTexcoord.x <= (cursorPos.x + (5.0*pixelUnit.x)) && 
                fTexcoord.x >= (cursorPos.x - (4.0*pixelUnit.x))) {
                pixel = vec4(0.0, 0.0, 0.0, 1.0);
            }
        }
        //White Cross
        if (fTexcoord.x <= (cursorPos.x + (1.0*pixelUnit.x)) && 
            fTexcoord.x >= (cursorPos.x - (0.0*pixelUnit.x))) {
            if (fTexcoord.y <= (cursorPos.y + (4.0*pixelUnit.y)) && 
                fTexcoord.y >= (cursorPos.y - (3.0*pixelUnit.y))) {
                pixel = vec4(1.0, 1.0, 1.0, 1.0);
            }
        }

        if (fTexcoord.y <= (cursorPos.y + (1.0*pixelUnit.y)) && 
            fTexcoord.y >= (cursorPos.y - (0.0*pixelUnit.y))) {
            if (fTexcoord.x <= (cursorPos.x + (4.0*pixelUnit.x)) && 
                fTexcoord.x >= (cursorPos.x - (3.0*pixelUnit.x))) {
                pixel = vec4(1.0, 1.0, 1.0, 1.0);
            }
        }
    }

    oColor = vec4(pixel.rgb, 1.0);
}
)";

#endif // MAIN_SHADERS_H
