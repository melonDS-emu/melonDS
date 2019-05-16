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

#ifndef GPU3D_OPENGL43_SHADERS_H
#define GPU3D_OPENGL43_SHADERS_H

#define kShaderHeader "#version 430"


const char* kClearVS = kShaderHeader R"(

layout(location=0) in vec2 vPosition;

layout(location=1) uniform uint uDepth;

void main()
{
    float fdepth = (float(uDepth) / 8388608.0) - 1.0;
    gl_Position = vec4(vPosition, fdepth, 1.0);
}
)";

const char* kClearFS = kShaderHeader R"(

layout(location=0) uniform uvec4 uColor;
layout(location=2) uniform uint uOpaquePolyID;
layout(location=3) uniform uint uFogFlag;

layout(location=0) out vec4 oColor;
layout(location=1) out uvec3 oAttr;

void main()
{
    oColor = vec4(uColor).bgra / 31.0;
    oAttr.r = 0;
    oAttr.g = uOpaquePolyID;
    oAttr.b = 0;
}
)";


const char* kRenderVSCommon = R"(

layout(std140, binding=0) uniform uConfig
{
    vec2 uScreenSize;
    uint uDispCnt;
    vec4 uToonColors[32];
};

layout(location=0) in uvec4 vPosition;
layout(location=1) in uvec4 vColor;
layout(location=2) in ivec2 vTexcoord;
layout(location=3) in uvec3 vPolygonAttr;

smooth out vec4 fColor;
smooth out vec2 fTexcoord;
flat out uvec3 fPolygonAttr;
)";

const char* kRenderFSCommon = R"(

layout(binding=0) uniform usampler2D TexMem;
layout(binding=1) uniform sampler2D TexPalMem;

layout(std140, binding=0) uniform uConfig
{
    vec2 uScreenSize;
    uint uDispCnt;
    vec4 uToonColors[32];
};

smooth in vec4 fColor;
smooth in vec2 fTexcoord;
flat in uvec3 fPolygonAttr;

layout(location=0) out vec4 oColor;
layout(location=1) out uvec3 oAttr;

int TexcoordWrap(int c, int maxc, uint mode)
{
    if ((mode & (1<<0)) != 0)
    {
        if ((mode & (1<<2)) != 0 && (c & maxc) != 0)
            return (maxc-1) - (c & (maxc-1));
        else
            return (c & (maxc-1));
    }
    else
        return clamp(c, 0, maxc-1);
}

vec4 TextureFetch_A3I5(ivec2 addr, ivec4 st, uint wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    uvec4 pixel = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);

    pixel.a = (pixel.r & 0xE0);
    pixel.a = (pixel.a >> 3) + (pixel.a >> 6);
    pixel.r &= 0x1F;

    addr.y = (addr.y << 3) + int(pixel.r);
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, float(pixel.a)/31.0);
}

vec4 TextureFetch_I2(ivec2 addr, ivec4 st, uint wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) >> 2;
    uvec4 pixel = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);
    pixel.r >>= (2 * (st.x & 3));
    pixel.r &= 0x03;

    addr.y = (addr.y << 2) + int(pixel.r);
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, max(step(1,pixel.r),alpha0));
}

vec4 TextureFetch_I4(ivec2 addr, ivec4 st, uint wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) >> 1;
    uvec4 pixel = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);
    if ((st.x & 1) != 0) pixel.r >>= 4;
    else                 pixel.r &= 0x0F;

    addr.y = (addr.y << 3) + int(pixel.r);
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, max(step(1,pixel.r),alpha0));
}

vec4 TextureFetch_I8(ivec2 addr, ivec4 st, uint wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    uvec4 pixel = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);

    addr.y = (addr.y << 3) + int(pixel.r);
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, max(step(1,pixel.r),alpha0));
}

vec4 TextureFetch_Compressed(ivec2 addr, ivec4 st, uint wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y & 0x3FC) * (st.z>>2)) + (st.x & 0x3FC) + (st.y & 0x3);
    uvec4 p = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);
    uint val = (p.r >> (2 * (st.x & 0x3))) & 0x3;

    int slot1addr = 0x20000 + ((addr.x & 0x1FFFC) >> 1);
    if (addr.x >= 0x40000) slot1addr += 0x10000;

    uint palinfo;
    p = texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0);
    palinfo = p.r;
    slot1addr++;
    p = texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0);
    palinfo |= (p.r << 8);

    addr.y = (addr.y << 3) + ((int(palinfo) & 0x3FFF) << 1);
    palinfo >>= 14;

    if (val == 0)
    {
        vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
        return vec4(color.rgb, 1.0);
    }
    else if (val == 1)
    {
        addr.y++;
        vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
        return vec4(color.rgb, 1.0);
    }
    else if (val == 2)
    {
        if (palinfo == 1)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb + color1.rgb) / 2.0, 1.0);
        }
        else if (palinfo == 3)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb*5.0 + color1.rgb*3.0) / 8.0, 1.0);
        }
        else
        {
            addr.y += 2;
            vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4(color.rgb, 1.0);
        }
    }
    else
    {
        if (palinfo == 2)
        {
            addr.y += 3;
            vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4(color.rgb, 1.0);
        }
        else if (palinfo == 3)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb*3.0 + color1.rgb*5.0) / 8.0, 1.0);
        }
        else
        {
            return vec4(0.0);
        }
    }
}

vec4 TextureFetch_A5I3(ivec2 addr, ivec4 st, uint wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    uvec4 pixel = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);

    pixel.a = (pixel.r & 0xF8) >> 3;
    pixel.r &= 0x07;

    addr.y = (addr.y << 3) + int(pixel.r);
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, float(pixel.a)/31.0);
}

vec4 TextureFetch_Direct(ivec2 addr, ivec4 st, uint wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) << 1;
    uvec4 pixelL = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);
    addr.x++;
    uvec4 pixelH = texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0);

    vec4 color;
    color.r = float(pixelL.r & 0x1F) / 31.0;
    color.g = float((pixelL.r >> 5) | ((pixelH.r & 0x03) << 3)) / 31.0;
    color.b = float((pixelH.r & 0x7C) >> 2) / 31.0;
    color.a = float(pixelH.r >> 7);

    return color;
}

vec4 TextureLookup_Nearest(vec2 st)
{
    uint attr = fPolygonAttr.y;
    uint paladdr = fPolygonAttr.z;

    float alpha0;
    if ((attr & (1<<29)) != 0) alpha0 = 0.0;
    else                       alpha0 = 1.0;

    int tw = 8 << int((attr >> 20) & 0x7);
    int th = 8 << int((attr >> 23) & 0x7);
    ivec4 st_full = ivec4(ivec2(st), tw, th);

    ivec2 vramaddr = ivec2(int(attr & 0xFFFF) << 3, int(paladdr));
    uint wrapmode = attr >> 16;

    uint type = (attr >> 26) & 0x7;
    if      (type == 5) return TextureFetch_Compressed(vramaddr, st_full, wrapmode);
    else if (type == 2) return TextureFetch_I2        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 3) return TextureFetch_I4        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 4) return TextureFetch_I8        (vramaddr, st_full, wrapmode, alpha0);
    else if (type == 1) return TextureFetch_A3I5      (vramaddr, st_full, wrapmode);
    else if (type == 6) return TextureFetch_A5I3      (vramaddr, st_full, wrapmode);
    else                return TextureFetch_Direct    (vramaddr, st_full, wrapmode);
}

vec4 TextureLookup_Linear(vec2 texcoord)
{
    ivec2 intpart = ivec2(texcoord);
    vec2 fracpart = fract(texcoord);

    uint attr = fPolygonAttr.y;
    uint paladdr = fPolygonAttr.z;

    float alpha0;
    if ((attr & (1<<29)) != 0) alpha0 = 0.0;
    else                       alpha0 = 1.0;

    int tw = 8 << int((attr >> 20) & 0x7);
    int th = 8 << int((attr >> 23) & 0x7);
    ivec4 st_full = ivec4(intpart, tw, th);

    ivec2 vramaddr = ivec2(int(attr & 0xFFFF) << 3, int(paladdr));
    uint wrapmode = attr >> 16;

    vec4 A, B, C, D;
    uint type = (attr >> 26) & 0x7;
    if (type == 5)
    {
        A = TextureFetch_Compressed(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_Compressed(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_Compressed(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_Compressed(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else if (type == 2)
    {
        A = TextureFetch_I2(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I2(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I2(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I2(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 3)
    {
        A = TextureFetch_I4(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I4(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I4(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I4(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 4)
    {
        A = TextureFetch_I8(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I8(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I8(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I8(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 1)
    {
        A = TextureFetch_A3I5(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_A3I5(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_A3I5(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_A3I5(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else if (type == 6)
    {
        A = TextureFetch_A5I3(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_A5I3(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_A5I3(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_A5I3(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else
    {
        A = TextureFetch_Direct(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_Direct(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_Direct(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_Direct(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }

    float fx = fracpart.x;
    vec4 AB;
    if (A.a < (0.5/31.0) && B.a < (0.5/31.0))
        AB = vec4(0);
    else
    {
        //if (A.a < (0.5/31.0) || B.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        AB = mix(A, B, fx);
    }

    fx = fracpart.x;
    vec4 CD;
    if (C.a < (0.5/31.0) && D.a < (0.5/31.0))
        CD = vec4(0);
    else
    {
        //if (C.a < (0.5/31.0) || D.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        CD = mix(C, D, fx);
    }

    fx = fracpart.y;
    vec4 ret;
    if (AB.a < (0.5/31.0) && CD.a < (0.5/31.0))
        ret = vec4(0);
    else
    {
        //if (AB.a < (0.5/31.0) || CD.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        ret = mix(AB, CD, fx);
    }

    return ret;
}

vec4 FinalColor()
{
    vec4 col;
    vec4 vcol = fColor;
    uint blendmode = (fPolygonAttr.x >> 4) & 0x3;

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
        vec4 tcol = TextureLookup_Nearest(fTexcoord);
        //vec4 tcol = TextureLookup_Linear(fTexcoord);

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
    uint attr = vPolygonAttr.x;
    uint zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = ((vec2(vPosition.xy) * 2.0) / uScreenSize) - 1.0;
    fpos.z = (float(vPosition.z << zshift) / 8388608.0) - 1.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xyz *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
    fPolygonAttr = vPolygonAttr;

    gl_Position = fpos;
}
)";

const char* kRenderVS_W = R"(

smooth out float fZ;

void main()
{
    uint attr = vPolygonAttr.x;
    uint zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = ((vec2(vPosition.xy) * 2.0) / uScreenSize) - 1.0;
    fZ = float(vPosition.z << zshift) / 16777216.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xy *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
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
    oAttr.g = (fPolygonAttr.x >> 24) & 0x3F;
}
)";

const char* kRenderFS_WO = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oColor = col;
    oAttr.g = (fPolygonAttr.x >> 24) & 0x3F;
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
    oAttr.g = 0xFF;
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
    oAttr.g = 0xFF;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZSM = R"(

void main()
{
    oColor = vec4(0,0,0,1);
    oAttr.g = 0xFF;
    oAttr.b = 1;
}
)";

const char* kRenderFS_WSM = R"(

smooth in float fZ;

void main()
{
    oColor = vec4(0,0,0,1);
    oAttr.g = 0xFF;
    oAttr.b = 1;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZS = R"(

layout(binding=2) uniform usampler2D iAttrTex;
//layout(origin_upper_left) in vec4 gl_FragCoord;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    uvec4 iAttr = texelFetch(iAttrTex, ivec2(gl_FragCoord.xy), 0);
    if (iAttr.b != 1) discard;
    if (iAttr.g == ((fPolygonAttr.x >> 24) & 0x3F)) discard;

    oColor = col;
}
)";

const char* kRenderFS_WS = R"(

layout(binding=2) uniform usampler2D iAttrTex;
//layout(origin_upper_left) in vec4 gl_FragCoord;

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    uvec4 iAttr = texelFetch(iAttrTex, ivec2(gl_FragCoord.xy), 0);
    if (iAttr.b != 1) discard;
    if (iAttr.g == ((fPolygonAttr.x >> 24) & 0x3F)) discard;

    oColor = col;
    gl_FragDepth = fZ;
}
)";

#endif // GPU3D_OPENGL43_SHADERS_H
