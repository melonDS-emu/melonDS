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

#ifndef GPU3D_OPENGL_SHADERS_H
#define GPU3D_OPENGL_SHADERS_H

#define kShaderHeader "#version 140"


const char* kClearVS = kShaderHeader R"(

in vec2 vPosition;

uniform uint uDepth;

void main()
{
    float fdepth = (float(uDepth) / 8388608.0) - 1.0;
    gl_Position = vec4(vPosition, fdepth, 1.0);
}
)";

const char* kClearFS = kShaderHeader R"(

uniform uvec4 uColor;
uniform uint uOpaquePolyID;
uniform uint uFogFlag;

out vec4 oColor;
out vec4 oAttr;

void main()
{
    oColor = vec4(uColor).bgra / 31.0;
    oAttr.r = float(uOpaquePolyID) / 63.0;
    oAttr.g = 0;
    oAttr.b = float(uFogFlag);
    oAttr.a = 1;
}
)";



const char* kFinalPassVS = kShaderHeader R"(

in vec2 vPosition;

void main()
{
    // heh
    gl_Position = vec4(vPosition, 0.0, 1.0);
}
)";

const char* kFinalPassEdgeFS = kShaderHeader R"(

uniform sampler2D DepthBuffer;
uniform sampler2D AttrBuffer;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

out vec4 oColor;

// make up for crapo zbuffer precision
bool isless(float a, float b)
{
    return a < b;

    // a < b
    float diff = a - b;
    return diff < (256.0 / 16777216.0);
}

bool isgood(vec4 attr, float depth, int refPolyID, float refDepth)
{
    int polyid = int(attr.r * 63.0);

    if (polyid != refPolyID && isless(refDepth, depth))
        return true;

    return false;
}

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    int scale = 1;//int(uScreenSize.x / 256);

    vec4 ret = vec4(0,0,0,0);
    vec4 depth = texelFetch(DepthBuffer, coord, 0);
    vec4 attr = texelFetch(AttrBuffer, coord, 0);

    int polyid = int(attr.r * 63.0);

    if (attr.g != 0)
    {
        vec4 depthU = texelFetch(DepthBuffer, coord + ivec2(0,-scale), 0);
        vec4 attrU = texelFetch(AttrBuffer, coord + ivec2(0,-scale), 0);
        vec4 depthD = texelFetch(DepthBuffer, coord + ivec2(0,scale), 0);
        vec4 attrD = texelFetch(AttrBuffer, coord + ivec2(0,scale), 0);
        vec4 depthL = texelFetch(DepthBuffer, coord + ivec2(-scale,0), 0);
        vec4 attrL = texelFetch(AttrBuffer, coord + ivec2(-scale,0), 0);
        vec4 depthR = texelFetch(DepthBuffer, coord + ivec2(scale,0), 0);
        vec4 attrR = texelFetch(AttrBuffer, coord + ivec2(scale,0), 0);

        if (isgood(attrU, depthU.r, polyid, depth.r) ||
            isgood(attrD, depthD.r, polyid, depth.r) ||
            isgood(attrL, depthL.r, polyid, depth.r) ||
            isgood(attrR, depthR.r, polyid, depth.r))
        {
            // mark this pixel!

            ret.rgb = uEdgeColors[polyid >> 3].bgr;

            // this isn't quite accurate, but it will have to do
            if ((uDispCnt & (1<<4)) != 0)
                ret.a = 0.5;
            else
                ret.a = 1;
        }
    }

    oColor = ret;
}
)";

const char* kFinalPassFogFS = kShaderHeader R"(

uniform sampler2D DepthBuffer;
uniform sampler2D AttrBuffer;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

out vec4 oColor;

vec4 CalculateFog(float depth)
{
    int idepth = int(depth * 16777216.0);
    int densityid, densityfrac;

    if (idepth < uFogOffset)
    {
        densityid = 0;
        densityfrac = 0;
    }
    else
    {
        uint udepth = uint(idepth);
        udepth -= uint(uFogOffset);
        udepth = (udepth >> 2) << uint(uFogShift);

        densityid = int(udepth >> 17);
        if (densityid >= 32)
        {
            densityid = 32;
            densityfrac = 0;
        }
        else
            densityfrac = int(udepth & uint(0x1FFFF));
    }

    float density = mix(uFogDensity[densityid], uFogDensity[densityid+1], float(densityfrac)/131072.0);

    return vec4(density, density, density, density);
}

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);

    vec4 ret = vec4(0,0,0,0);
    vec4 depth = texelFetch(DepthBuffer, coord, 0);
    vec4 attr = texelFetch(AttrBuffer, coord, 0);

    if (attr.b != 0) ret = CalculateFog(depth.r);

    oColor = ret;
}
)";



const char* kRenderVSCommon = R"(

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

in uvec4 vPosition;
in uvec4 vColor;
in ivec2 vTexcoord;
in ivec3 vPolygonAttr;

smooth out vec4 fColor;
smooth out vec2 fTexcoord;
flat out ivec3 fPolygonAttr;
)";

const char* kRenderFSCommon = R"(

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

smooth in vec4 fColor;
smooth in vec2 fTexcoord;
flat in ivec3 fPolygonAttr;

uniform sampler2DArray Textures;

out vec4 oColor;
out vec4 oAttr;

float TexcoordWrap(float c, int mode)
{
    if ((mode & (1<<0)) != 0)
    {
        if ((mode & (1<<2)) == 0)
            return fract(c);
        else
            // mirrored repeat is the most complex one, so we let the hardware handle it
            return c;
    }
    else
        return clamp(c, 0, 1);
}

vec4 FinalColor()
{
    vec4 col;
    vec4 vcol = fColor;
    int blendmode = (fPolygonAttr.x >> 4) & 0x3;

    if (blendmode == 2)
    {
        if ((uDispCnt & (1<<1)) == 0)
        {
            // toon
            vec3 tooncolor = uToonColors[int(vcol.r * 31)].rgb;
            vcol.rgb = tooncolor;
        }
        else
        {
            // highlight
            vcol.rgb = vcol.rrr;
        }
    }

    if ((((fPolygonAttr.y >> 26) & 0x7) == 0) || ((uDispCnt & (1<<0)) == 0))
    {
        // no texture
        col = vcol;
    }
    else
    {
        int wrapmode = (fPolygonAttr.y >> 16);

        vec2 st = vec2(
            TexcoordWrap(fTexcoord.x, wrapmode>>0),
            TexcoordWrap(fTexcoord.y, wrapmode>>1));
        vec4 tcol = texture(Textures, vec3(st, float(fPolygonAttr.z)));

        if ((blendmode & 1) != 0)
        {
            // decal
            col.rgb = (tcol.rgb * tcol.a) + (vcol.rgb * (1.0-tcol.a));
            col.a = vcol.a;
        }
        else
        {
            // modulate
            col = vcol * tcol;
        }
    }

    if (blendmode == 2)
    {
        if ((uDispCnt & (1<<1)) != 0)
        {
            vec3 tooncolor = uToonColors[int(vcol.r * 31)].rgb;
            col.rgb = min(col.rgb + tooncolor, 1.0);
        }
    }

    return col.bgra;
}
)";


const char* kRenderVS_Z = R"(

void main()
{
    int attr = vPolygonAttr.x;
    int zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = (((vec2(vPosition.xy) ) * 2.0) / uScreenSize) - 1.0;
    fpos.z = (float(vPosition.z << zshift) / 8388608.0) - 1.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xyz *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    vec2 texSize = vec2(
            float(8 << ((vPolygonAttr.y >> 20) & 0x7)),
            float(8 << ((vPolygonAttr.y >> 23) & 0x7)));
    fTexcoord = vec2(vTexcoord) / (texSize * 16.0);
    fPolygonAttr = vPolygonAttr;

    gl_Position = fpos;
}
)";

const char* kRenderVS_W = R"(

smooth out float fZ;

void main()
{
    int attr = vPolygonAttr.x;
    int zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = (((vec2(vPosition.xy) ) * 2.0) / uScreenSize) - 1.0;
    fZ = float(vPosition.z << zshift) / 16777216.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xy *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);

    vec2 texSize = vec2(
            float(8 << ((vPolygonAttr.y >> 20) & 0x7)),
            float(8 << ((vPolygonAttr.y >> 23) & 0x7)));
    fTexcoord = vec2(vTexcoord) / (texSize * 16.0);
    fPolygonAttr = vPolygonAttr;

    gl_Position = fpos;
}
)";


const char* kRenderFS_ZO = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.g = 0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1;
}
)";

const char* kRenderFS_WO = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.g = 0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZE = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oAttr.g = 1;
    oAttr.a = 1;
}
)";

const char* kRenderFS_WE = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oAttr.g = 1;
    oAttr.a = 1;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZT = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    oColor = col;
    oAttr.b = 0;
    oAttr.a = 1;
}
)";

const char* kRenderFS_WT = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    oColor = col;
    oAttr.b = 0;
    oAttr.a = 1;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZSM = R"(

void main()
{
    oColor = vec4(0,0,0,1);
}
)";

const char* kRenderFS_WSM = R"(

smooth in float fZ;

void main()
{
    oColor = vec4(0,0,0,1);
    gl_FragDepth = fZ;
}
)";

#endif // GPU3D_OPENGL_SHADERS_H
