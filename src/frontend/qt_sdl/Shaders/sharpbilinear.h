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

#ifndef SHARPBILINEAR_H
#define SHARPBILINEAR_H

const char* sharpbilinear_VS = R"(#version 140
uniform vec2 uScreenSize;
uniform mat2x3 uTransform;
uniform vec4 i_resolution;
uniform vec4 o_resolution;
in vec2 vPosition;
in vec3 vTexcoord;
out vec2 fTexcoord;
out vec2 precalc_texel;
out vec2 precalc_scale;

void main()
{
    vec4 fpos;

    fpos.xy = vec3(vPosition, 1.0) * uTransform;

    fpos.xy = ((fpos.xy * 2.0) / uScreenSize) - 1.0;
    fpos.y *= -1;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
    fTexcoord = vTexcoord.xy;
    precalc_scale = ceil(o_resolution.xy / i_resolution.xy);
    precalc_texel = vTexcoord.xy * i_resolution.xy;
}
)";

const char* sharpbilinear_FS = R"(#version 140
/*
   Author: KojoZero (based on rsn8887's shader)
   License: Public domain

   This is an integer prescale filter that should be combined
   with a bilinear hardware filtering (GL_BILINEAR filter or some such) to achieve
   a smooth scaling result with minimum blur. This is good for pixelgraphics
   that are scaled by non-integer factors.

   This is a modified version rsn8887's shader which has been modified to scale
   until above the output resolution, rather than right below the output resolution.
   
   The prescale factor and texel coordinates are precalculated
   in the vertex shader for speed.
*/

uniform vec4 i_resolution;
uniform vec4 o_resolution;
in vec2 fTexcoord;
in vec2 precalc_texel;
in vec2 precalc_scale;
out vec4 oColor;
uniform sampler2D ScreenTex;
uniform int convert_colors;

vec3 LinearTosRGB(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0/2.4)) - 0.055, step(0.0031308, c));
}


void main()
{
  vec2 texel = precalc_texel;
  vec2 scale = precalc_scale;
  vec2 texel_floored = floor(texel);
  vec2 s = fract(texel);
  vec2 region_range = 0.5 - 0.5 / scale;

  // Figure out where in the texel to sample to get correct pre-scaled bilinear.
  // Uses the hardware bilinear interpolator to avoid having to sample 4 times manually.

  vec2 center_dist = s - 0.5;
  vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * scale + 0.5;

  vec2 mod_texel = texel_floored + f;

  vec4 pixel = vec4(texture(ScreenTex, mod_texel / i_resolution.xy).rgb, 1.0);
  if (convert_colors == 2){
      pixel = vec4(LinearTosRGB(pixel.rgb), pixel.a);
  }
  oColor = pixel;
}
)";

#endif // SHARPBILINEAR_H
