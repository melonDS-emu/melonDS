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

#ifndef GPU_VULKAN_SHADERS
#define GPU_VULKAN_SHADERS

#include <string>

// Vulkan port of the compositing shaders from src/OpenGL_shaders
// (FinalPass*, Capture*, CaptureDownscale*). The algorithms are identical;
// the differences are purely in resource declarations:
//  - every resource lives in descriptor set 0 with a unique binding
//  - in/out varyings and vertex attributes carry explicit locations
//    (attribute locations match the GL glBindAttribLocation slots)
//  - the loose uniform uInputLayer became a push constant (PushCapDown)
//
// the C++ side prepends the "#version 460" line (same convention as
// GPU3D_ComputeVulkan_shaders.h)
//
// each pass gets its own descriptor set 0 layout:
//   FinalPass:        0: ubFinalPassConfig (UBO)
//                     1: MainInputTexA  2: MainInputTexB (sampler2D)
//                     3: AuxInputTex (sampler2DArray)
//   Capture:          0: ubCaptureConfig (UBO)
//                     1: InputTexA (sampler2D)  2: InputTexB (sampler2DArray)
//   CaptureDownscale: 0: InputTex (sampler2DArray)

namespace melonDS
{

namespace GPUShadersVulkan
{

const std::string FinalPassVS{R"(

layout(std140, set = 0, binding = 0) uniform ubFinalPassConfig
{
    bvec4 uScreenSwap[48]; // one bool per scanline
    ivec4 uVCount[48];
    int uScaleFactor;
    int uAuxLayer;
    int uDispModeA;
    int uDispModeB;
    int uBrightModeA;
    int uBrightModeB;
    int uBrightFactorA;
    int uBrightFactorB;
    float uAuxColorFactor;
    int uDither;
    int uAuxUseVCount;
};

layout(location = 0) in vec2 vPosition;

layout(location = 0) smooth out vec3 fTexcoord;

void main()
{
    gl_Position = vec4(vPosition, 0, 1);
    fTexcoord = (vPosition.xyy + 1) * vec3(0.5, 0.5, 0.375);
}
)"};

const std::string FinalPassFS{R"(

layout(set = 0, binding = 1) uniform sampler2D MainInputTexA;
layout(set = 0, binding = 2) uniform sampler2D MainInputTexB;
layout(set = 0, binding = 3) uniform sampler2DArray AuxInputTex;

layout(std140, set = 0, binding = 0) uniform ubFinalPassConfig
{
    bvec4 uScreenSwap[48]; // one bool per scanline
    ivec4 uVCount[48];
    int uScaleFactor;
    int uAuxLayer;
    int uDispModeA;
    int uDispModeB;
    int uBrightModeA;
    int uBrightModeB;
    int uBrightFactorA;
    int uBrightFactorB;
    float uAuxColorFactor;
    int uDither;
    int uAuxUseVCount;
};

layout(location = 0) smooth in vec3 fTexcoord;

layout(location = 0) out vec4 oTopColor;
layout(location = 1) out vec4 oBottomColor;

ivec3 MasterBrightness(ivec3 color, int brightmode, int evy)
{
    if (brightmode == 1)
    {
        // up
        color += (((0x3F - color) * evy) >> 4);
    }
    else if (brightmode == 2)
    {
        // down
        color -= (((color * evy) + 0xF) >> 4);
    }

    return color;
}

void main()
{
    int line = int(fTexcoord.y * 192);
    ivec4 col_main = ivec4(texture(MainInputTexA, fTexcoord.xy, 0) * 255.0) >> 2;
    ivec4 col_sub = ivec4(texture(MainInputTexB, fTexcoord.xy, 0) * 255.0) >> 2;

    ivec3 output_main, output_sub;

    if (uDispModeA == 0)
    {
        // screen disabled (white)
        output_main = ivec3(63, 63, 63);
    }
    else if (uDispModeA == 1)
    {
        // BG/OBJ layers
        output_main = col_main.rgb;
    }
    else
    {
        // VRAM display / mainmem FIFO
        vec2 auxCoord = fTexcoord.xz;
        if (uAuxUseVCount != 0)
        {
            int vcount = uVCount[line>>2][line&0x3];
            auxCoord.y = (float(vcount) + fract(fTexcoord.y * 192.0)) / 256.0;
        }
        output_main = ivec3(texture(AuxInputTex, vec3(auxCoord, uAuxLayer)).rgb * uAuxColorFactor);
    }

    if (uDispModeB == 0)
    {
        // screen disabled (white)
        output_sub = ivec3(63, 63, 63);
    }
    else
    {
        // BG/OBJ layers
        output_sub = col_sub.rgb;
    }

    if (uDispModeA != 0)
        output_main = MasterBrightness(output_main, uBrightModeA, uBrightFactorA);
    if (uDispModeB != 0)
        output_sub = MasterBrightness(output_sub, uBrightModeB, uBrightFactorB);

    output_main = (output_main << 2) | (output_main >> 6);
    output_sub = (output_sub << 2) | (output_sub >> 6);

    // optional ordered dither to mask the DS's 6-bit colour banding on
    // gradients: nudge each pixel by up to a fraction of a 6-bit step so hard
    // contours read as a soft transition. Purely at the output; off => exact.
    if (uDither != 0)
    {
        const int bayer[16] = int[16](0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5);
        ivec2 fc = ivec2(gl_FragCoord.xy);
        int d = (bayer[(fc.y & 3) * 4 + (fc.x & 3)] - 8) >> 2; // -2 .. +1
        output_main = clamp(output_main + d, 0, 255);
        output_sub = clamp(output_sub + d, 0, 255);
    }

    bool swapbit = uScreenSwap[line>>2][line&0x3];

    if (!swapbit)
    {
        oTopColor = vec4(vec3(output_sub) / 255.0, 1.0);
        oBottomColor = vec4(vec3(output_main) / 255.0, 1.0);
    }
    else
    {
        oTopColor = vec4(vec3(output_main) / 255.0, 1.0);
        oBottomColor = vec4(vec3(output_sub) / 255.0, 1.0);
    }
}
)"};

const std::string CaptureVS{R"(

layout(std140, set = 0, binding = 0) uniform ubCaptureConfig
{
    vec2 uInvCaptureSize;
    int uSrcALayer;
    int uSrcBLayer;
    int uSrcBOffset;
    int uDstMode;
    ivec2 uBlendFactors;
    vec4 uSrcAOffset[48];
    ivec4 uVCount[48];
    float uSrcBColorFactor;
    int uSrcBUseVCount;
};

layout(location = 0) in ivec2 vPosition;
layout(location = 1) in ivec2 vTexcoord;

layout(location = 0) smooth out vec4 fTexcoord;

void main()
{
    vec2 pos = vec2(vPosition) * uInvCaptureSize.xx;
    gl_Position = vec4((pos * 2) - 1, 0, 1);
    fTexcoord.xy = vec2(vTexcoord) / vec2(256,192);
    fTexcoord.z = vTexcoord.y;
    fTexcoord.w = float(vTexcoord.y + uSrcBOffset) / 256.0;
}
)"};

const std::string CaptureFS{R"(

layout(set = 0, binding = 1) uniform sampler2D InputTexA;
layout(set = 0, binding = 2) uniform sampler2DArray InputTexB;

layout(std140, set = 0, binding = 0) uniform ubCaptureConfig
{
    vec2 uInvCaptureSize;
    int uSrcALayer;
    int uSrcBLayer;
    int uSrcBOffset;
    int uDstMode;
    ivec2 uBlendFactors;
    vec4 uSrcAOffset[48];
    ivec4 uVCount[48];
    float uSrcBColorFactor;
    int uSrcBUseVCount;
};

layout(location = 0) smooth in vec4 fTexcoord;

layout(location = 0) out vec4 oColor;

float GetSrcAPos(float line)
{
    int iline = int(line);
    float a = uSrcAOffset[iline>>2][iline&0x3];
    iline++;
    float b = uSrcAOffset[iline>>2][iline&0x3];
    return mix(a, b, fract(line));
}

void main()
{
    vec2 coordA = fTexcoord.xy;
    vec3 coordB = vec3(fTexcoord.xw, uSrcBLayer);
    int line = int(fTexcoord.z);
    int vcount = uVCount[line>>2][line&0x3];
    float lineFrac = fract(fTexcoord.z);
    ivec4 cap_out;

    // apply scroll for 3D layer, if we're capturing that
    if (uSrcALayer == 1)
    {
        coordA.y = (float(vcount) + lineFrac) / 192.0;
        coordA.x += uSrcAOffset[line>>2][line&0x3];
        //coordA.x += GetSrcAPos(fTexcoord.z);
    }

    if (uSrcBUseVCount != 0)
        coordB.y = (float(vcount + uSrcBOffset) + lineFrac) / 256.0;

    if (uDstMode == 0)
    {
        // source A only
        cap_out = ivec4(texture(InputTexA, coordA) * 255.0);
    }
    else if (uDstMode == 1)
    {
        // source B only
        cap_out = ivec4(texture(InputTexB, coordB) * uSrcBColorFactor);
    }
    else
    {
        // sources A and B blended
        ivec4 srcA = ivec4(texture(InputTexA, coordA) * 255.0) >> 3;
        ivec4 srcB = ivec4(texture(InputTexB, coordB) * uSrcBColorFactor) >> 3;

        int eva = uBlendFactors[0];
        int evb = uBlendFactors[1];

        int aa = (srcA.a > 0) ? 1 : 0;
        int ab = (srcB.a > 0) ? 1 : 0;

        cap_out.rgb = ((srcA.rgb * aa * eva) + (srcB.rgb * ab * evb) + 0x8) >> 4;
        cap_out.rgb = min(cap_out.rgb, 0x1F) << 3;
        cap_out.a = (eva>0 ? aa : 0) | (evb>0 ? ab : 0);
    }

    oColor = vec4(vec3(cap_out.rgb) / 255.0, (cap_out.a>0) ? 1.0 : 0.0);
}
)"};

const std::string CaptureDownscaleVS{R"(

layout(location = 0) in vec2 vPosition;

layout(location = 0) smooth out vec2 fTexcoord;

void main()
{
    gl_Position = vec4((vPosition * 2) - 1, 0, 1);
    fTexcoord = vPosition;
}
)"};

const std::string CaptureDownscaleFS{R"(

layout(set = 0, binding = 0) uniform sampler2DArray InputTex;

layout(push_constant) uniform PushCapDown
{
    int uInputLayer;
};

layout(location = 0) smooth in vec2 fTexcoord;

layout(location = 0) out vec4 oColor;

void main()
{
    ivec4 col = ivec4(texture(InputTex, vec3(fTexcoord, uInputLayer)) * 255.0);
    oColor.rgb = vec3(col.rgb >> 3) / 31.0;
    oColor.a = (col.a>0) ? 1 : 0;
}
)"};

}

}

#endif
