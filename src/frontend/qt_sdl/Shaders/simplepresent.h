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

#ifndef SIMPLEPRESENT_H
#define SIMPLEPRESENT_H

const char* simplepresent_VS = R"(#version 140

in vec2 vPosition;
in vec3 vTexcoord;

smooth out vec2 fTexcoord;

void main()
{
    gl_Position = vec4(vPosition, 0.0, 1.0);
    fTexcoord = vTexcoord.xy;
}

)";

const char* simplepresent_FS = R"(#version 140

uniform sampler2D ScreenTex;
uniform int convert_colors;
smooth in vec2 fTexcoord;

out vec4 oColor;

vec3 sRGBToLinear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

vec3 LinearTosRGB(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0/2.4)) - 0.055, step(0.0031308, c));
}


void main()
{
    vec4 pixel = texture(ScreenTex, fTexcoord);
    if (convert_colors == 2){
        pixel = vec4(LinearTosRGB(pixel.rgb), pixel.a);
    } else if (convert_colors == 1){
        pixel = vec4(sRGBToLinear(pixel.rgb), pixel.a);
    }
    oColor = pixel;
}
)";

#endif // SIMPLEPRESENT_H
