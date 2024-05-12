/*
    Copyright 2016-2022 melonDS team

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

#ifndef GPU3D_COMPUTE_SHADERS
#define GPU3D_COMPUTE_SHADERS

#include <string>

namespace melonDS
{

namespace ComputeRendererShaders
{

// defines:
// InterpSpans
// BinCombined
// Rasterise
// DepthBlend
// ClearCoarseBinMask
// ClearIndirectWorkCount
// CalculateWorkOffsets
// SortWork
// FinalPass

// AntiAliasing
// EdgeMarking
// Fog

// ZBuffer
// WBuffer

// for Rasterise
// NoTexture
// UseTexture
// Decal
// Modulate
// Toon
// Highlight
// ShadowMask


/*
    Some notes on signed division:

    we want to avoid it, so we can avoid higher precision numbers
    in a few places.
    
    Fortunately all divisions *should* assuming I'm not mistaken
    have the same sign on the divisor and the dividend.

    Thus we apply:

    assuming n < 0 <=> d < 0
    n/d = abs(n)/abs(d)

*/

const std::string XSpanSetupBuffer{R"(

const uint XSpanSetup_Linear = 1U << 0;
const uint XSpanSetup_FillInside = 1U << 1;
const uint XSpanSetup_FillLeft = 1U << 2;
const uint XSpanSetup_FillRight = 1U << 3;

struct XSpanSetup
{
    int X0, X1;

    int InsideStart, InsideEnd, EdgeCovL, EdgeCovR;

    int XRecip;

    uint Flags;

    int Z0, Z1, W0, W1;
    int ColorR0, ColorG0, ColorB0;
    int ColorR1, ColorG1, ColorB1;
    int TexcoordU0, TexcoordV0;
    int TexcoordU1, TexcoordV1;

    int CovLInitial, CovRInitial;
};

#if defined(Rasterise)
int CalcYFactorX(XSpanSetup span, int x)
{
    x -= span.X0;

    if (span.X0 != span.X1)
    {
        uint numLo = uint(x) * uint(span.W0);
        uint numHi = 0U;
        numHi |= numLo >> (32U-YFactorShift);
        numLo <<= YFactorShift;

        uint den = uint(x) * uint(span.W0) + uint(span.X1 - span.X0 - x) * uint(span.W1);

        if (den == 0)
            return 0;
        else
            return int(Div64_32_32(numHi, numLo, den));
    }
    else
    {
        return 0;
    }
}
#endif

layout (std430, binding = 1) buffer XSpanSetupsBuffer
{
    XSpanSetup XSpanSetups[];
};

)"};

const std::string YSpanSetupBuffer{R"(

struct YSpanSetup
{
    // Attributes
    int Z0, Z1, W0, W1;
    int ColorR0, ColorG0, ColorB0;
    int ColorR1, ColorG1, ColorB1;
    int TexcoordU0, TexcoordV0;
    int TexcoordU1, TexcoordV1;

    // Interpolator
    int I0, I1;
    bool Linear;
    int IRecip;
    int W0n, W0d, W1d;

    // Slope
    int Increment;

    int X0, X1, Y0, Y1;
    int XMin, XMax;
    int DxInitial;

    int XCovIncr;

    bool IsDummy;
};

#if defined(InterpSpans)
int CalcYFactorY(YSpanSetup span, int i)
{
    /*
        maybe it would be better to do use a 32x32=64 multiplication?
    */
    uint numLo = uint(abs(i)) * uint(span.W0n);
    uint numHi = 0U;
    numHi |= numLo >> (32U-YFactorShift);
    numLo <<= YFactorShift;

    uint den = uint(abs(i)) * uint(span.W0d) + uint(abs(span.I1 - span.I0 - i)) * span.W1d;

    if (den == 0)
    {
        return 0;
    }
    else
    {
        return int(Div64_32_32(numHi, numLo, den));
    }
}

int CalculateDx(int y, YSpanSetup span)
{
    return span.DxInitial + (y - span.Y0) * span.Increment;
}

int CalculateX(int dx, YSpanSetup span)
{
    int x = span.X0;
    if (span.X1 < span.X0)
        x -= dx >> 18;
    else
        x += dx >> 18;
    return clamp(x, span.XMin, span.XMax);
}

void EdgeParams_XMajor(bool side, int dx, YSpanSetup span, out int edgelen, out int edgecov)
{
    bool negative = span.X1 < span.X0;
    int len;
    if (side != negative)
        len = (dx >> 18) - ((dx-span.Increment) >> 18);
    else
        len = ((dx+span.Increment) >> 18) - (dx >> 18);
    edgelen = len;

    int xlen = span.XMax + 1 - span.XMin;
    int startx = dx >> 18;
    if (negative) startx = xlen - startx;
    if (side) startx = startx - len + 1;

    uint r;
    int startcov = int(Div(uint(((startx << 10) + 0x1FF) * (span.Y1 - span.Y0)), uint(xlen), r));
    edgecov = (1<<31) | ((startcov & 0x3FF) << 12) | (span.XCovIncr & 0x3FF);
}

void EdgeParams_YMajor(bool side, int dx, YSpanSetup span, out int edgelen, out int edgecov)
{
    bool negative = span.X1 < span.X0;
    edgelen = 1;
    
    if (span.Increment == 0)
    {
        edgecov = 31;
    }
    else
    {
        int cov = ((dx >> 9) + (span.Increment >> 10)) >> 4;
        if ((cov >> 5) != (dx >> 18)) cov = 31;
        cov &= 0x1F;
        if (side == negative) cov = 0x1F - cov;

        edgecov = cov;
    }
}
#endif

layout (std430, binding = 2) buffer YSpanSetupsBuffer
{
    YSpanSetup YSpanSetups[];
};

)"};

const std::string PolygonBuffer{R"(
struct Polygon
{
    int FirstXSpan;
    int YTop, YBot;

    int XMin, XMax;
    int XMinY, XMaxY;

    int Variant;

    uint Attr;

    float TextureLayer;
};

layout (std430, binding = 0) readonly buffer PolygonBuffer
{
    Polygon Polygons[];
};
)"};

const std::string BinningBuffer{R"(

layout (std430, binding = 6) buffer BinResultBuffer
{
    uvec4 VariantWorkCount[MaxVariants];
    uint SortedWorkOffset[MaxVariants];

    uvec4 SortWorkWorkCount;

    uint BinningMaskAndOffset[];
    //uint BinnedMaskCoarse[TilesPerLine*TileLines*CoarseBinStride];
    //uint BinnedMask[TilesPerLine*TileLines*BinStride];
    //uint WorkOffsets[TilesPerLine*TileLines*BinStride];
};

const int BinningCoarseMaskStart = 0;
const int BinningMaskStart = BinningCoarseMaskStart+TilesPerLine*TileLines*CoarseBinStride;
const int BinningWorkOffsetsStart = BinningMaskStart+TilesPerLine*TileLines*BinStride;

)"};

/*
    structure of each WorkDesc item:
        x:
            bits 0-10: polygon idx
            bits 11-31: tile idx (before sorting within variant after sorting within all tiles)
        y:
            bits 0-15: X position on screen
            bits 15-31: Y position on screen
*/
const std::string WorkDescBuffer{R"(
layout (std430, binding = 7) buffer WorkDescBuffer
{
    //uvec2 UnsortedWorkDescs[MaxWorkTiles];
    //uvec2 SortedWorkDescs[MaxWorkTiles];
    uvec2 WorkDescs[];
};

const uint WorkDescsUnsortedStart = 0;
const uint WorkDescsSortedStart = WorkDescsUnsortedStart+MaxWorkTiles;

)"};

const std::string Tilebuffers{R"(
layout (std430, binding = 2) buffer ColorTileBuffer
{
    uint ColorTiles[];
};
layout (std430, binding = 3) buffer DepthTileBuffer
{
    uint DepthTiles[];
};
layout (std430, binding = 4) buffer AttrTileBuffer
{
    uint AttrTiles[];
};

)"};

const std::string ResultBuffer{R"(
layout (std430, binding = 5) buffer ResultBuffer
{
    uint ResultValue[];
};

const uint ResultColorStart = 0;
const uint ResultDepthStart = ResultColorStart+ScreenWidth*ScreenHeight*2;
const uint ResultAttrStart = ResultDepthStart+ScreenWidth*ScreenHeight*2;
)"};

const char* Common = R"(

#define TileSize 8
const int CoarseTileCountX = 8;
const int CoarseTileCountY = 4;
const int CoarseTileW = (CoarseTileCountX * TileSize);
const int CoarseTileH = (CoarseTileCountY * TileSize);

const int FramebufferStride = ScreenWidth*ScreenHeight;
const int TilesPerLine = ScreenWidth/TileSize;
const int TileLines = ScreenHeight/TileSize;

const int BinStride = 2048/32;
const int CoarseBinStride = BinStride/32;

const int MaxVariants = 256;

layout (std140, binding = 0) uniform MetaUniform
{
    uint NumPolygons;
    uint NumVariants;

    int AlphaRef;

    uint DispCnt;

    // r = Toon
    // g = Fog Density
    // b = Edge Color
    uvec4 ToonTable[34];

    uint ClearColor, ClearDepth, ClearAttr;

    uint FogOffset, FogShift, FogColor;
};

#ifdef InterpSpans
const int YFactorShift = 9;
#else
const int YFactorShift = 8;
#endif

#if defined(InterpSpans) || defined(Rasterise)
uint Umulh(uint a, uint b)
{
    uint lo, hi;
    umulExtended(a, b, hi, lo);
    return hi;
}

const uint startTable[256] = uint[256](
    254, 252, 250, 248, 246, 244, 242, 240, 238, 236, 234, 233, 231, 229, 227, 225, 224, 222, 220, 218, 217, 215, 213, 212, 210, 208, 207, 205, 203, 202, 200, 199, 197, 195, 194, 192, 191, 189, 188, 186, 185, 183, 182, 180, 179, 178, 176, 175, 173, 172, 170, 169, 168, 166, 165, 164, 162, 161, 160, 158, 
157, 156, 154, 153, 152, 151, 149, 148, 147, 146, 144, 143, 142, 141, 139, 138, 137, 136, 135, 134, 132, 131, 130, 129, 128, 127, 126, 125, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 88, 87, 86, 85, 84, 83, 82, 81, 80, 80, 79, 78, 77, 76, 75, 74, 74, 73, 72, 71, 70, 70, 69, 68, 67, 66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 59, 58, 57, 56, 56, 55, 54, 53, 53, 52, 51, 50, 50, 49, 48, 48, 47, 46, 46, 45, 44, 43, 43, 42, 41, 41, 40, 39, 39, 38, 37, 37, 36, 35, 35, 34, 33, 33, 32, 32, 31, 30, 30, 29, 28, 28, 27, 27, 26, 25, 25, 24, 24, 23, 22, 22, 21, 21, 20, 19, 19, 18, 18, 17, 17, 16, 15, 15, 14, 14, 13, 13, 12, 12, 11, 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0
);

uint Div(uint x, uint y, out uint r)
{
    // https://www.microsoft.com/en-us/research/publication/software-integer-division/
    uint k = 31 - findMSB(y);
    uint ty = (y << k) >> (32 - 9);
    uint t = startTable[ty - 256] + 256;
    uint z = (t << (32 - 9)) >> (32 - k - 1);
    uint my = 0 - y;

    z += Umulh(z, my * z);
    z += Umulh(z, my * z);

    uint q = Umulh(x, z);
    r = x - y * q;
    if(r >= y)
    {
        r = r - y;
        q = q + 1;
        if(r >= y)
        {
            r = r - y;
            q = q + 1;
        }
    }

    return q;
}

uint Div64_32_32(uint numHi, uint numLo, uint den)
{
    // based on https://github.com/ridiculousfish/libdivide/blob/3bd34388573681ce563348cdf04fe15d24770d04/libdivide.h#L469
    // modified to work with half the size 64/32=32 instead of 128/64=64
    // for further details see https://ridiculousfish.com/blog/posts/labor-of-division-episode-iv.html

    // We work in base 2**16.
    // A uint32 holds a single digit (in the lower 16 bit). A uint32 holds two digits.
    // Our numerator is conceptually [num3, num2, num1, num0].
    // Our denominator is [den1, den0].
    const uint b = (1U << 16);

    // Determine the normalization factor. We multiply den by this, so that its leading digit is at
    // least half b. In binary this means just shifting left by the number of leading zeros, so that
    // there's a 1 in the MSB.
    // We also shift numer by the same amount. This cannot overflow because numHi < den.
    // The expression (-shift & 63) is the same as (64 - shift), except it avoids the UB of shifting
    // by 64. (it's also UB in GLSL!!!!)
    uint shift = 31 - findMSB(den);
    den <<= shift;
    numHi <<= shift;
    numHi |= (numLo >> (-shift & 31U)) & uint(-int(shift) >> 31);
    numLo <<= shift;

    // Extract the low digits of the numerator and both digits of the denominator.
    uint num1 = (numLo >> 16);
    uint num0 = (numLo & 0xFFFFU);
    uint den1 = (den >> 16);
    uint den0 = (den & 0xFFFFU);

    // We wish to compute q1 = [n3 n2 n1] / [d1 d0].
    // Estimate q1 as [n3 n2] / [d1], and then correct it.
    // Note while qhat may be 2 digits, q1 is always 1 digit.

    uint rhat;
    uint qhat = Div(numHi, den1, rhat);
    uint c1 = qhat * den0;
    uint c2 = rhat * b + num1;
    if (c1 > c2) qhat -= (c1 - c2 > den) ? 2 : 1;
    uint q1 = qhat & 0xFFFFU;

    // Compute the true (partial) remainder.
    uint rem = numHi * b + num1 - q1 * den;

    // We wish to compute q0 = [rem1 rem0 n0] / [d1 d0].
    // Estimate q0 as [rem1 rem0] / [d1] and correct it.
    qhat = Div(rem, den1, rhat);
    c1 = qhat * den0;
    c2 = rhat * b + num0;
    if (c1 > c2) qhat -= (c1 - c2 > den) ? 2 : 1;

    return bitfieldInsert(qhat, q1, 16, 16);
}

int InterpolateAttrPersp(int y0, int y1, int ifactor)
{
    if (y0 == y1)
        return y0;

    if (y0 < y1)
        return y0 + (((y1-y0) * ifactor) >> YFactorShift);
    else
        return y1 + (((y0-y1) * ((1<<YFactorShift)-ifactor)) >> YFactorShift);
}

int InterpolateAttrLinear(int y0, int y1, int i, int irecip, int idiff)
{
    if (y0 == y1)
        return y0;

#ifndef Rasterise
    irecip = abs(irecip);
#endif

    uint mulLo, mulHi, carry;
    if (y0 < y1)
    {
#ifndef Rasterise
        uint offset = uint(abs(i));
#else
        uint offset = uint(i);
#endif
        umulExtended(uint(y1-y0)*offset, uint(irecip), mulHi, mulLo);
        mulLo = uaddCarry(mulLo, 3U<<24, carry);
        mulHi += carry;
        return y0 + int((mulLo >> 30) | (mulHi << (32 - 30)));
        //return y0 + int(((int64_t(y1-y0) * int64_t(offset) * int64_t(irecip)) + int64_t(3<<24)) >> 30);
    }
    else
    {
#ifndef Rasterise
        uint offset = uint(abs(idiff-i));
#else
        uint offset = uint(idiff-i);
#endif
        umulExtended(uint(y0-y1)*offset, uint(irecip), mulHi, mulLo);
        mulLo = uaddCarry(mulLo, 3<<24, carry);
        mulHi += carry;
        return y1 + int((mulLo >> 30) | (mulHi << (32 - 30)));
        //return y1 + int(((int64_t(y0-y1) * int64_t(offset) * int64_t(irecip)) + int64_t(3<<24)) >> 30);
    }
}

uint InterpolateZZBuffer(int z0, int z1, int i, int irecip, int idiff)
{
    if (z0 == z1)
        return z0;

    uint base, disp, factor;
    if (z0 < z1)
    {
        base = uint(z0);
        disp = uint(z1 - z0);
        factor = uint(abs(i));
    }
    else
    {
        base = uint(z1);
        disp = uint(z0 - z1),
        factor = uint(abs(idiff - i));
    }

#ifdef InterpSpans
    int shiftl = 0;
    const int shiftr = 22;
    if (disp > 0x3FF)
    {
        shiftl = findMSB(disp) - 9;
        disp >>= shiftl;
    }
#else
    disp >>= 9;
    const int shiftl = 0;
    const int shiftr = 13;
#endif
    uint mulLo, mulHi;

    umulExtended(disp * factor, abs(irecip) >> 8, mulHi, mulLo);

    return base + (((mulLo >> shiftr) | (mulHi << (32 - shiftr))) << shiftl);
/*
    int base, disp, factor;
    if (z0 < z1)
    {
        base = z0;
        disp = z1 - z0;
        factor = i;
    }
    else
    {
        base = z1;
        disp = z0 - z1,
        factor = idiff - i;
    }

#ifdef InterpSpans
    {
        int shift = 0;
        while (disp > 0x3FF)
        {
            disp >>= 1;
            shift++;
        }

        return base + int(((int64_t(disp) * int64_t(factor) * (int64_t(irecip) >> 8)) >> 22) << shift);
    }
#else
    {
        disp >>= 9;
        return base + int((int64_t(disp) * int64_t(factor) * (int64_t(irecip) >> 8)) >> 13);
    }
#endif*/
}

uint InterpolateZWBuffer(int z0, int z1, int ifactor)
{
    if (z0 == z1)
        return z0;

#ifdef Rasterise
    // since the precision along x spans is only 8 bit the result will always fit in 32-bit
    if (z0 < z1)
    {
        return uint(z0) + (((z1-z0) * ifactor) >> YFactorShift);
    }
    else
    {
        return uint(z1) + (((z0-z1) * ((1<<YFactorShift)-ifactor)) >> YFactorShift);
    }
#else
    uint mulLo, mulHi;
    if (z0 < z1)
    {
        umulExtended(z1-z0, ifactor, mulHi, mulLo);
        // 64-bit shift
        return uint(z0) + ((mulLo >> YFactorShift) | (mulHi << (32-YFactorShift)));
    }
    else
    {
        umulExtended(z0-z1, (1<<YFactorShift)-ifactor, mulHi, mulLo);
        return uint(z1) + ((mulLo >> YFactorShift) | (mulHi << (32-YFactorShift)));
    }
#endif
    /*if (z0 < z1)
    {
        return uint(z0) + uint((int64_t(z1-z0) * int64_t(ifactor)) >> YFactorShift);
    }
    else
    {
        return uint(z1) + uint((int64_t(z0-z1) * int64_t((1<<YFactorShift)-ifactor)) >> YFactorShift);
    }*/
}
#endif

)";

const std::string InterpSpans = 
    PolygonBuffer +
    XSpanSetupBuffer +
    YSpanSetupBuffer + R"(
layout (local_size_x = 32) in;

layout (binding = 0, rgba16ui) uniform readonly uimageBuffer SetupIndices;

void main()
{
    uvec4 setup = imageLoad(SetupIndices, int(gl_GlobalInvocationID.x));

    YSpanSetup spanL = YSpanSetups[setup.y];
    YSpanSetup spanR = YSpanSetups[setup.z];
    
    XSpanSetup xspan;
    xspan.Flags = 0U;

    int y = int(setup.w);

    int dxl = CalculateDx(y, spanL);
    int dxr = CalculateDx(y, spanR);

    int xl = CalculateX(dxl, spanL);
    int xr = CalculateX(dxr, spanR);

    Polygon polygon = Polygons[setup.x];

    int edgeLenL, edgeLenR;

    if (xl > xr)
    {
        YSpanSetup tmpSpan = spanL;
        spanL = spanR;
        spanR = tmpSpan;

        int tmp = xl;
        xl = xr;
        xr = tmp;
    
        EdgeParams_YMajor(false, dxr, spanL, edgeLenL, xspan.EdgeCovL);
        EdgeParams_YMajor(true, dxl, spanR, edgeLenR, xspan.EdgeCovR);
    }
    else
    {
        // edges are the right way
        if (spanL.Increment > 0x40000)
            EdgeParams_XMajor(false, dxl, spanL, edgeLenL, xspan.EdgeCovL);
        else
            EdgeParams_YMajor(false, dxl, spanL, edgeLenL, xspan.EdgeCovL);
        if (spanR.Increment > 0x40000)
            EdgeParams_XMajor(true, dxr, spanR, edgeLenR, xspan.EdgeCovR);
        else
            EdgeParams_YMajor(true, dxr, spanR, edgeLenR, xspan.EdgeCovR);
    }

    xspan.CovLInitial = (xspan.EdgeCovL >> 12) & 0x3FF;
    if (xspan.CovLInitial == 0x3FF)
        xspan.CovLInitial = 0;
    xspan.CovRInitial = (xspan.EdgeCovR >> 12) & 0x3FF;
    if (xspan.CovRInitial == 0x3FF)
        xspan.CovRInitial = 0;

    xspan.X0 = xl;
    xspan.X1 = xr + 1;

    uint polyalpha = ((polygon.Attr >> 16) & 0x1FU);
    bool isWireframe = polyalpha == 0U;

    if (!isWireframe || (y == polygon.YTop || y == polygon.YBot - 1))
        xspan.Flags |= XSpanSetup_FillInside;

    xspan.InsideStart = xspan.X0 + edgeLenL;
    if (xspan.InsideStart > xspan.X1)
        xspan.InsideStart = xspan.X1;
    xspan.InsideEnd = xspan.X1 - edgeLenR;
    if (xspan.InsideEnd > xspan.X1)
        xspan.InsideEnd = xspan.X1;

    bool isShadowMask = ((polygon.Attr & 0x3F000030U) == 0x00000030U);
    bool fillAllEdges = polyalpha < 31 || (DispCnt & (3U<<4)) != 0U;

    if (fillAllEdges || spanL.X1 < spanL.X0 || spanL.Increment <= 0x40000)
        xspan.Flags |= XSpanSetup_FillLeft;
    if (fillAllEdges || (spanR.X1 >= spanR.X0 && spanR.Increment > 0x40000) || spanR.Increment == 0)
        xspan.Flags |= XSpanSetup_FillRight;

    if (spanL.I0 == spanL.I1)
    {
        xspan.TexcoordU0 = spanL.TexcoordU0;
        xspan.TexcoordV0 = spanL.TexcoordV0;
        xspan.ColorR0 = spanL.ColorR0;
        xspan.ColorG0 = spanL.ColorG0;
        xspan.ColorB0 = spanL.ColorB0;
        xspan.Z0 = spanL.Z0;
        xspan.W0 = spanL.W0;
    }
    else
    {
        int i = (spanL.Increment > 0x40000 ? xl : y) - spanL.I0;
        int ifactor = CalcYFactorY(spanL, i);
        int idiff = spanL.I1 - spanL.I0;

#ifdef ZBuffer
        xspan.Z0 = int(InterpolateZZBuffer(spanL.Z0, spanL.Z1, i, spanL.IRecip, idiff));
#endif
#ifdef WBuffer
        xspan.Z0 = int(InterpolateZWBuffer(spanL.Z0, spanL.Z1, ifactor));
#endif

        if (!spanL.Linear)
        {
            xspan.TexcoordU0 = InterpolateAttrPersp(spanL.TexcoordU0, spanL.TexcoordU1, ifactor);
            xspan.TexcoordV0 = InterpolateAttrPersp(spanL.TexcoordV0, spanL.TexcoordV1, ifactor);

            xspan.ColorR0 = InterpolateAttrPersp(spanL.ColorR0, spanL.ColorR1, ifactor);
            xspan.ColorG0 = InterpolateAttrPersp(spanL.ColorG0, spanL.ColorG1, ifactor);
            xspan.ColorB0 = InterpolateAttrPersp(spanL.ColorB0, spanL.ColorB1, ifactor);

            xspan.W0 = InterpolateAttrPersp(spanL.W0, spanL.W1, ifactor);
        }
        else
        {
            xspan.TexcoordU0 = InterpolateAttrLinear(spanL.TexcoordU0, spanL.TexcoordU1, i, spanL.IRecip, idiff);
            xspan.TexcoordV0 = InterpolateAttrLinear(spanL.TexcoordV0, spanL.TexcoordV1, i, spanL.IRecip, idiff);

            xspan.ColorR0 = InterpolateAttrLinear(spanL.ColorR0, spanL.ColorR1, i, spanL.IRecip, idiff);
            xspan.ColorG0 = InterpolateAttrLinear(spanL.ColorG0, spanL.ColorG1, i, spanL.IRecip, idiff);
            xspan.ColorB0 = InterpolateAttrLinear(spanL.ColorB0, spanL.ColorB1, i, spanL.IRecip, idiff);

            xspan.W0 = spanL.W0; // linear mode is only taken if W0 == W1
        }
    }

    if (spanR.I0 == spanR.I1)
    {
        xspan.TexcoordU1 = spanR.TexcoordU0;
        xspan.TexcoordV1 = spanR.TexcoordV0;
        xspan.ColorR1 = spanR.ColorR0;
        xspan.ColorG1 = spanR.ColorG0;
        xspan.ColorB1 = spanR.ColorB0;
        xspan.Z1 = spanR.Z0;
        xspan.W1 = spanR.W0;
    }
    else
    {
        int i = (spanR.Increment > 0x40000 ? xr : y) - spanR.I0;
        int ifactor = CalcYFactorY(spanR, i);
        int idiff = spanR.I1 - spanR.I0;

    #ifdef ZBuffer
            xspan.Z1 = int(InterpolateZZBuffer(spanR.Z0, spanR.Z1, i, spanR.IRecip, idiff));
    #endif
    #ifdef WBuffer
            xspan.Z1 = int(InterpolateZWBuffer(spanR.Z0, spanR.Z1, ifactor));
    #endif

        if (!spanR.Linear)
        {
            xspan.TexcoordU1 = InterpolateAttrPersp(spanR.TexcoordU0, spanR.TexcoordU1, ifactor);
            xspan.TexcoordV1 = InterpolateAttrPersp(spanR.TexcoordV0, spanR.TexcoordV1, ifactor);

            xspan.ColorR1 = InterpolateAttrPersp(spanR.ColorR0, spanR.ColorR1, ifactor);
            xspan.ColorG1 = InterpolateAttrPersp(spanR.ColorG0, spanR.ColorG1, ifactor);
            xspan.ColorB1 = InterpolateAttrPersp(spanR.ColorB0, spanR.ColorB1, ifactor);

            xspan.W1 = int(InterpolateAttrPersp(spanR.W0, spanR.W1, ifactor));
        }
        else
        {
            xspan.TexcoordU1 = InterpolateAttrLinear(spanR.TexcoordU0, spanR.TexcoordU1, i, spanR.IRecip, idiff);
            xspan.TexcoordV1 = InterpolateAttrLinear(spanR.TexcoordV0, spanR.TexcoordV1, i, spanR.IRecip, idiff);

            xspan.ColorR1 = InterpolateAttrLinear(spanR.ColorR0, spanR.ColorR1, i, spanR.IRecip, idiff);
            xspan.ColorG1 = InterpolateAttrLinear(spanR.ColorG0, spanR.ColorG1, i, spanR.IRecip, idiff);
            xspan.ColorB1 = InterpolateAttrLinear(spanR.ColorB0, spanR.ColorB1, i, spanR.IRecip, idiff);

            xspan.W1 = spanR.W0;
        }
    }

    if (xspan.W0 == xspan.W1 && ((xspan.W0 | xspan.W1) & 0x7F) == 0)
    {
        xspan.Flags |= XSpanSetup_Linear;
// a bit hacky, but when wbuffering we only need to calculate xrecip for linear spans
#ifdef ZBuffer
    }
    {
#endif
        uint r;
        xspan.XRecip = int(Div(1U<<30, uint(xspan.X1 - xspan.X0), r));
    }

    XSpanSetups[gl_GlobalInvocationID.x] = xspan;
}

)";

const std::string ClearIndirectWorkCount =
    BinningBuffer + R"(

layout (local_size_x = 32) in;

void main()
{
    VariantWorkCount[gl_GlobalInvocationID.x] = uvec4(1, 1, 0, 0);
}

)";

const std::string ClearCoarseBinMask =
    BinningBuffer + R"(
layout (local_size_x = 32) in;

void main()
{
    BinningMaskAndOffset[BinningCoarseMaskStart + gl_GlobalInvocationID.x*CoarseBinStride+0] = 0;
    BinningMaskAndOffset[BinningCoarseMaskStart + gl_GlobalInvocationID.x*CoarseBinStride+1] = 0;
}

)";

const std::string BinCombined =
    PolygonBuffer +
    BinningBuffer +
    XSpanSetupBuffer +
    WorkDescBuffer + R"(

layout (local_size_x = 32) in;

bool BinPolygon(Polygon polygon, ivec2 topLeft, ivec2 botRight)
{
    if (polygon.YTop > botRight.y || polygon.YBot <= topLeft.y)
        return false;

    int polygonHeight = polygon.YBot - polygon.YTop;

    /*
        All (good) polygons are convex. So the following holds true:

        Starting from the top most point where both edges originate
        the X coordinate of the left edge will stay the same or falls until
        the minimum X-axis coordinate is reached. Then it stays the same or
        rises until the point it meets with the right edge.

        The same applies to the right edge, except that it first may rise or stay equal and
        after the maximum point may only fall or stay the same.

        This means that for every tile which doesn't contain the point where the direction changes
        we can just get the maximum point by sampling the top most and bottom most coordinate
        within the tile.

        For a tile which is that the height of the direction change

        As a sidenote another consequence of this design decision is
        that malformed polygons aren't binned properly.

        As a note bottom Y is exclusive!
    */
    int polyInnerTopY = clamp(topLeft.y - polygon.YTop, 0, max(polygonHeight-1, 0));
    int polyInnerBotY = clamp(botRight.y - polygon.YTop, 0, max(polygonHeight-1, 0));

    XSpanSetup xspanTop = XSpanSetups[polygon.FirstXSpan + polyInnerTopY];
    XSpanSetup xspanBot = XSpanSetups[polygon.FirstXSpan + polyInnerBotY];

    int minXL;
    if (polygon.XMinY >= topLeft.y && polygon.XMinY <= botRight.y)
        minXL = polygon.XMin;
    else
        minXL = min(xspanTop.X0, xspanBot.X0);

    if (minXL > botRight.x)
        return false;

    int maxXR;
    if (polygon.XMaxY >= topLeft.y && polygon.XMaxY <= botRight.y)
        maxXR = polygon.XMax;
    else
        maxXR = max(xspanTop.X1, xspanBot.X1) - 1;

    if (maxXR < topLeft.x)
        return false;

    return true;
}

shared uint mergedMaskShared;

void main()
{
    int groupIdx = int(gl_WorkGroupID.x);
    ivec2 coarseTile = ivec2(gl_WorkGroupID.yz);

#if 0
    int localIdx = int(gl_SubGroupInvocationARB);
#else
    int localIdx = int(gl_LocalInvocationIndex);

    if (localIdx == 0)
        mergedMaskShared = 0U;
    barrier();
#endif

    int polygonIdx = groupIdx * 32 + localIdx;

    ivec2 coarseTopLeft = coarseTile * ivec2(CoarseTileW, CoarseTileH);
    ivec2 coarseBotRight = coarseTopLeft + ivec2(CoarseTileW-1, CoarseTileH-1);

    bool binned = false;
    if (polygonIdx < NumPolygons)
    {
        binned = BinPolygon(Polygons[polygonIdx], coarseTopLeft, coarseBotRight);
    }

#if 0
    uint mergedMask = unpackUint2x32(ballotARB(binned)).x;
#else
    if (binned)
        atomicOr(mergedMaskShared, 1U << localIdx);
    barrier();
    uint mergedMask = mergedMaskShared;
#endif

    ivec2 fineTile = ivec2(localIdx & 0x7, localIdx >> 3);

    ivec2 fineTileTopLeft = coarseTopLeft + fineTile * ivec2(TileSize, TileSize);
    ivec2 fineTileBotRight = fineTileTopLeft + ivec2(TileSize-1, TileSize-1);

    uint binnedMask = 0U;
    while (mergedMask != 0U)
    {
        int bit = findLSB(mergedMask);
        mergedMask &= ~(1U << bit);

        int polygonIdx = groupIdx * 32 + bit;

        if (BinPolygon(Polygons[polygonIdx], fineTileTopLeft, fineTileBotRight))
            binnedMask |= 1U << bit;
    }

    int linearTile = fineTile.x + fineTile.y * TilesPerLine + coarseTile.x * CoarseTileCountX + coarseTile.y * TilesPerLine * CoarseTileCountY;

    BinningMaskAndOffset[BinningMaskStart + linearTile * BinStride + groupIdx] = binnedMask;
    int coarseMaskIdx = linearTile * CoarseBinStride + (groupIdx >> 5);
    if (binnedMask != 0U)
        atomicOr(BinningMaskAndOffset[BinningCoarseMaskStart + coarseMaskIdx], 1U << (groupIdx & 0x1F));

    if (binnedMask != 0U)
    {
        uint workOffset = atomicAdd(VariantWorkCount[0].w, uint(bitCount(binnedMask)));
        BinningMaskAndOffset[BinningWorkOffsetsStart + linearTile * BinStride + groupIdx] = workOffset;

        uint tilePositionCombined = bitfieldInsert(fineTileTopLeft.x, fineTileTopLeft.y, 16, 16);

        int idx = 0;
        while (binnedMask != 0U)
        {
            int bit = findLSB(binnedMask);
            binnedMask &= ~(1U << bit);

            int polygonIdx = groupIdx * 32 + bit;
            int variantIdx = Polygons[polygonIdx].Variant;

            int inVariantOffset = int(atomicAdd(VariantWorkCount[variantIdx].z, 1));
            WorkDescs[WorkDescsUnsortedStart + workOffset + idx] = uvec2(tilePositionCombined, bitfieldInsert(polygonIdx, inVariantOffset, 12, 20));

            idx++;
        }
    }
}

)";

const std::string CalcOffsets = 
    BinningBuffer + R"(

layout (local_size_x = 32) in;

void main()
{
    if (gl_GlobalInvocationID.x < NumVariants)
    {
        if (gl_GlobalInvocationID.x == 0)
        {
            // a bit of a cheat putting this here, but this shader won't run that often
            SortWorkWorkCount = uvec4((VariantWorkCount[0].w + 31) / 32, 1, 1, 0);
        }
        SortedWorkOffset[gl_GlobalInvocationID.x] = atomicAdd(VariantWorkCount[1].w, VariantWorkCount[gl_GlobalInvocationID.x].z);
    }
}


)";

const std::string SortWork =
    PolygonBuffer +
    BinningBuffer +
    WorkDescBuffer + R"(

layout (local_size_x = 32) in;

void main()
{
    if (gl_GlobalInvocationID.x < VariantWorkCount[0].w)
    {
        uvec2 workDesc = WorkDescs[WorkDescsUnsortedStart + gl_GlobalInvocationID.x];
        int inVariantOffset = int(bitfieldExtract(workDesc.y, 12, 20));
        int polygonIdx = int(bitfieldExtract(workDesc.y, 0, 12));
        int variantIdx = Polygons[polygonIdx].Variant;

        int sortedIndex = int(SortedWorkOffset[variantIdx]) + inVariantOffset;
        WorkDescs[WorkDescsSortedStart + sortedIndex] = uvec2(workDesc.x, bitfieldInsert(workDesc.y, gl_GlobalInvocationID.x, 12, 20));
    }
}

)";

const std::string Rasterise =
    PolygonBuffer +
    WorkDescBuffer +
    XSpanSetupBuffer +
    BinningBuffer +
    Tilebuffers + R"(

layout (local_size_x = TileSize, local_size_y = TileSize) in;

layout (binding = 0) uniform usampler2DArray CurrentTexture;

layout (location = 0) uniform uint CurVariant;
layout (location = 1) uniform vec2 InvTextureSize;

void main()
{
    uvec2 workDesc = WorkDescs[WorkDescsSortedStart + SortedWorkOffset[CurVariant] + gl_WorkGroupID.z];
    Polygon polygon = Polygons[bitfieldExtract(workDesc.y, 0, 12)];
    ivec2 position = ivec2(bitfieldExtract(workDesc.x, 0, 16), bitfieldExtract(workDesc.x, 16, 16)) + ivec2(gl_LocalInvocationID.xy);
    int tileOffset = int(bitfieldExtract(workDesc.y, 12, 20)) * TileSize * TileSize + TileSize * int(gl_LocalInvocationID.y) + int(gl_LocalInvocationID.x);

    uint color = 0U;
    if (position.y >= polygon.YTop && position.y < polygon.YBot)
    {
        XSpanSetup xspan = XSpanSetups[polygon.FirstXSpan + (position.y - polygon.YTop)];

        bool insideLeftEdge = position.x < xspan.InsideStart;
        bool insideRightEdge = position.x >= xspan.InsideEnd;
        bool insidePolygonInside = !insideLeftEdge && !insideRightEdge;

        if (position.x >= xspan.X0 && position.x < xspan.X1
            && ((insideLeftEdge && (xspan.Flags & XSpanSetup_FillLeft) != 0U)
                || (insideRightEdge && (xspan.Flags & XSpanSetup_FillRight) != 0U)
                || (insidePolygonInside && (xspan.Flags & XSpanSetup_FillInside) != 0U)))
        {
            uint attr = 0;
            if (position.y == polygon.YTop)
                attr |= 0x4U;
            else if (position.y == polygon.YBot - 1)
                attr |= 0x8U;

            if (insideLeftEdge)
            {
                attr |= 0x1U;

                int cov = xspan.EdgeCovL;
                if (cov < 0)
                {
                    int xcov = xspan.CovLInitial + (xspan.EdgeCovL & 0x3FF) * (position.x - xspan.X0);
                    cov = min(xcov >> 5, 31);
                }

                attr |= uint(cov) << 8;
            }
            else if (insideRightEdge)
            {
                attr |= 0x2U;

                int cov = xspan.EdgeCovR;
                if (cov < 0)
                {
                    int xcov = xspan.CovRInitial + (xspan.EdgeCovR & 0x3FF) * (position.x - xspan.InsideEnd);
                    cov = max(0x1F - (xcov >> 5), 0);
                }

                attr |= uint(cov) << 8;
            }

            uint z;
            int u, v, vr, vg, vb;

            if (xspan.X0 == xspan.X1)
            {
                z = xspan.Z0;
                u = xspan.TexcoordU0;
                v = xspan.TexcoordV0;
                vr = xspan.ColorR0;
                vg = xspan.ColorG0;
                vb = xspan.ColorB0;
            }
            else
            {
                int ifactor = CalcYFactorX(xspan, position.x);
                int idiff = xspan.X1 - xspan.X0;
                int i = position.x - xspan.X0;

#ifdef ZBuffer
                z = InterpolateZZBuffer(xspan.Z0, xspan.Z1, i, xspan.XRecip, idiff);
#endif
#ifdef WBuffer
                z = InterpolateZWBuffer(xspan.Z0, xspan.Z1, ifactor);
#endif
                if ((xspan.Flags & XSpanSetup_Linear) == 0U)
                {
                    u = InterpolateAttrPersp(xspan.TexcoordU0, xspan.TexcoordU1, ifactor);
                    v = InterpolateAttrPersp(xspan.TexcoordV0, xspan.TexcoordV1, ifactor);

                    vr = InterpolateAttrPersp(xspan.ColorR0, xspan.ColorR1, ifactor);
                    vg = InterpolateAttrPersp(xspan.ColorG0, xspan.ColorG1, ifactor);
                    vb = InterpolateAttrPersp(xspan.ColorB0, xspan.ColorB1, ifactor);
                }
                else
                {
                    u = InterpolateAttrLinear(xspan.TexcoordU0, xspan.TexcoordU1, i, xspan.XRecip, idiff);
                    v = InterpolateAttrLinear(xspan.TexcoordV0, xspan.TexcoordV1, i, xspan.XRecip, idiff);

                    vr = InterpolateAttrLinear(xspan.ColorR0, xspan.ColorR1, i, xspan.XRecip, idiff);
                    vg = InterpolateAttrLinear(xspan.ColorG0, xspan.ColorG1, i, xspan.XRecip, idiff);
                    vb = InterpolateAttrLinear(xspan.ColorB0, xspan.ColorB1, i, xspan.XRecip, idiff);
                }
            }

#ifndef ShadowMask
            vr >>= 3;
            vg >>= 3;
            vb >>= 3;

            uint r, g, b, a;
            uint polyalpha = bitfieldExtract(polygon.Attr, 16, 5);

#ifdef Toon
            uint tooncolor = ToonTable[vr >> 1].r;
            vr = int(bitfieldExtract(tooncolor, 0, 8));
            vg = int(bitfieldExtract(tooncolor, 8, 8));
            vb = int(bitfieldExtract(tooncolor, 16, 8));
#endif
#ifdef Highlight
            vg = vr;
            vb = vr;
#endif

#ifdef NoTexture
            a = int(polyalpha);
#endif
            r = vr;
            g = vg;
            b = vb;

#ifdef UseTexture
            vec2 uvf = vec2(ivec2(u, v)) * vec2(1.0 / 16.0) * InvTextureSize;

            uvec4 texcolor = texture(CurrentTexture, vec3(uvf, polygon.TextureLayer));
#ifdef Decal
            if (texcolor.a == 31)
            {
                r = int(texcolor.r);
                g = int(texcolor.g);
                b = int(texcolor.b);
            }
            else if (texcolor.a > 0)
            {
                r = int((texcolor.r * texcolor.a) + (vr * (31-texcolor.a))) >> 5;
                g = int((texcolor.g * texcolor.a) + (vg * (31-texcolor.a))) >> 5;
                b = int((texcolor.b * texcolor.a) + (vb * (31-texcolor.a))) >> 5;
            }
            a = int(polyalpha);
#endif
#if defined(Modulate) || defined(Toon) || defined(Highlight)
            r = int((texcolor.r+1) * (vr+1) - 1) >> 6;
            g = int((texcolor.g+1) * (vg+1) - 1) >> 6;
            b = int((texcolor.b+1) * (vb+1) - 1) >> 6;
            a = int((texcolor.a+1) * (polyalpha+1) - 1) >> 5;
#endif
#endif

#ifdef Highlight
            uint tooncolor = ToonTable[vr >> 1].r;

            r = min(r + int(bitfieldExtract(tooncolor, 0, 8)), 63);
            g = min(g + int(bitfieldExtract(tooncolor, 8, 8)), 63);
            b = min(b + int(bitfieldExtract(tooncolor, 16, 8)), 63);
#endif

            if (polyalpha == 0)
                a = 31;

            if (a > AlphaRef)
            {
                color = r | (g << 8) | (b << 16) | (a << 24);

                DepthTiles[tileOffset] = z;
                AttrTiles[tileOffset] = attr;
            }
#else
            color = 0xFFFFFFFF; // doesn't really matter as long as it's not 0
            DepthTiles[tileOffset] = z;
#endif
        }
    }

    ColorTiles[tileOffset] = color;
}

)";

const std::string DepthBlend =
    PolygonBuffer +
    Tilebuffers +
    ResultBuffer +
    BinningBuffer + R"(

layout (local_size_x = TileSize, local_size_y = TileSize) in;

void PlotTranslucent(inout uint color, inout uint depth, inout uint attr, bool isShadow, uint tileColor, uint srcA, uint tileDepth, uint srcAttr, bool writeDepth)
{
    uint blendAttr = (srcAttr & 0xE0F0U) | ((srcAttr >> 8) & 0xFF0000U) | (1U<<22) | (attr & 0xFF001F0FU);

    if ((!isShadow || (attr & (1U<<22)) != 0U)
        ? (attr & 0x007F0000U) != (blendAttr & 0x007F0000U)
        : (attr & 0x3F000000U) != (srcAttr & 0x3F000000U))
    {
        // le blend
        if (writeDepth)
            depth = tileDepth;

        if ((attr & (1U<<15)) == 0)
            blendAttr &= ~(1U<<15);
        attr = blendAttr;

        uint srcRB = tileColor & 0x3F003FU;
        uint srcG = tileColor & 0x003F00U;
        uint dstRB = color & 0x3F003FU;
        uint dstG = color & 0x003F00U;
        uint dstA = color & 0x1F000000U;

        uint alpha = (srcA >> 24) + 1;
        if (dstA != 0)
        {
            srcRB = ((srcRB * alpha) + (dstRB * (32-alpha))) >> 5;
            srcG = ((srcG * alpha) + (dstG * (32-alpha))) >> 5;
        }

        color = (srcRB & 0x3F003FU) | (srcG & 0x003F00U) | max(dstA, srcA);
    }
}

void ProcessCoarseMask(int linearTile, uint coarseMask, uint coarseOffset,
    inout uvec2 color, inout uvec2 depth, inout uvec2 attr, inout uint stencil,
    inout bool prevIsShadowMask)
{
    int tileInnerOffset = int(gl_LocalInvocationID.x) + int(gl_LocalInvocationID.y) * TileSize;

    while (coarseMask != 0U)
    {
        uint coarseBit = findLSB(coarseMask);
        coarseMask &= ~(1U << coarseBit);

        uint tileOffset = linearTile * BinStride + coarseBit + coarseOffset;

        uint fineMask = BinningMaskAndOffset[BinningMaskStart + tileOffset];
        uint workIdx = BinningMaskAndOffset[BinningWorkOffsetsStart + tileOffset];

        while (fineMask != 0U)
        {
            uint fineIdx = findLSB(fineMask);
            fineMask &= ~(1U << fineIdx);

            uint pixelindex = tileInnerOffset + workIdx * TileSize * TileSize;
            uint tileColor = ColorTiles[pixelindex];
            workIdx++;

            uint polygonIdx = fineIdx + (coarseBit + coarseOffset) * 32;

            if (tileColor != 0U)
            {
                uint polygonAttr = Polygons[polygonIdx].Attr;

                bool isShadowMask = ((polygonAttr & 0x3F000030U) == 0x00000030U);
                bool prevIsShadowMaskOld = prevIsShadowMask;
                prevIsShadowMask = isShadowMask;

                bool equalDepthTest = (polygonAttr & (1U << 14)) != 0U;

                uint tileDepth = DepthTiles[pixelindex];
                uint tileAttr = AttrTiles[pixelindex];

                uint dstattr = attr.x;

                if (!isShadowMask)
                {
                    bool isShadow = (polygonAttr & 0x30U) == 0x30U;

                    bool writeSecondLayer = false;

                    if (isShadow)
                    {
                        if (stencil == 0U)
                            continue;
                        if ((stencil & 1U) == 0U)
                            writeSecondLayer = true;
                        if ((stencil & 2U) == 0U)
                            dstattr &= ~0x3U;
                    }

                    uint dstDepth = writeSecondLayer ? depth.y : depth.x;
                    if (!(equalDepthTest
#ifdef WBuffer
                        ? dstDepth - tileDepth + 0xFFU <= 0x1FE
#endif
#ifdef ZBuffer
                        ? dstDepth - tileDepth + 0x200 <= 0x400
#endif
                        : tileDepth < dstDepth))
                    {
                        if ((dstattr & 0x3U) == 0U || writeSecondLayer)
                            continue;

                        writeSecondLayer = true;
                        dstattr = attr.y;
                        if (!(equalDepthTest
#ifdef WBuffer
                            ? depth.y - tileDepth + 0xFFU <= 0x1FE
#endif
#ifdef ZBuffer
                            ? depth.y - tileDepth + 0x200 <= 0x400
#endif
                            : tileDepth < depth.y))
                            continue;
                    }

                    uint srcAttr = (polygonAttr & 0x3F008000U);

                    uint srcA = tileColor & 0x1F000000U;
                    if (srcA == 0x1F000000U)
                    {
                        srcAttr |= tileAttr;

                        if (!writeSecondLayer)
                        {
                            if ((srcAttr & 0x3U) != 0U)
                            {
                                color.y = color.x;
                                depth.y = depth.x;
                                attr.y = attr.x;
                            }

                            color.x = tileColor;
                            depth.x = tileDepth;
                            attr.x = srcAttr;
                        }
                        else
                        {
                            color.y = tileColor;
                            depth.y = tileDepth;
                            attr.y = srcAttr;
                        }
                    }
                    else
                    {
                        bool writeDepth = (polygonAttr & (1U<<11)) != 0;

                        if (!writeSecondLayer)
                        {
                            // blend into both layers
                            PlotTranslucent(color.x, depth.x, attr.x, isShadow, tileColor, srcA, tileDepth, srcAttr, writeDepth);
                        }
                        if (writeSecondLayer || (dstattr & 0x3U) != 0U)
                        {
                            PlotTranslucent(color.y, depth.y, attr.y, isShadow, tileColor, srcA, tileDepth, srcAttr, writeDepth);
                        }
                    }
                }
                else
                {
                    if (!prevIsShadowMaskOld)
                        stencil = 0;

                    if (!(equalDepthTest
#ifdef WBuffer
                        ? depth.x - tileDepth + 0xFFU <= 0x1FE
#endif
#ifdef ZBuffer
                        ? depth.x - tileDepth + 0x200 <= 0x400
#endif
                        : tileDepth < depth.x))
                        stencil = 0x1U;

                    if ((dstattr & 0x3U) != 0U)
                    {
                        if (!(equalDepthTest
#ifdef WBuffer
                            ? depth.y - tileDepth + 0xFFU <= 0x1FE
#endif
#ifdef ZBuffer
                            ? depth.y - tileDepth + 0x200 <= 0x400
#endif
                            : tileDepth < depth.y))
                            stencil |= 0x2U;
                    }
                }
            }
        }
    }
}

void main()
{
    int linearTile = int(gl_WorkGroupID.x + (gl_WorkGroupID.y * TilesPerLine));

    uint coarseMaskLo = BinningMaskAndOffset[BinningCoarseMaskStart + linearTile*CoarseBinStride + 0];
    uint coarseMaskHi = BinningMaskAndOffset[BinningCoarseMaskStart + linearTile*CoarseBinStride + 1];

    uvec2 color = uvec2(ClearColor, 0U);
    uvec2 depth = uvec2(ClearDepth, 0U);
    uvec2 attr = uvec2(ClearAttr, 0U);
    uint stencil = 0U;
    bool prevIsShadowMask = false;

    ProcessCoarseMask(linearTile, coarseMaskLo, 0, color, depth, attr, stencil, prevIsShadowMask);
    ProcessCoarseMask(linearTile, coarseMaskHi, BinStride/2, color, depth, attr, stencil, prevIsShadowMask);

    int resultOffset = int(gl_GlobalInvocationID.x) + int(gl_GlobalInvocationID.y) * ScreenWidth;
    ResultValue[ResultColorStart+resultOffset] = color.x;
    ResultValue[ResultColorStart+resultOffset+FramebufferStride] = color.y;
    ResultValue[ResultDepthStart+resultOffset] = depth.x;
    ResultValue[ResultDepthStart+resultOffset+FramebufferStride] = depth.y;
    ResultValue[ResultAttrStart+resultOffset] = attr.x;
    ResultValue[ResultAttrStart+resultOffset+FramebufferStride] = attr.y;
}

)";

const std::string FinalPass =
    ResultBuffer + R"(

layout (local_size_x = 32) in;

layout (binding = 0, rgba8) writeonly uniform image2D FinalFB; 
layout (binding = 1, rgba8ui) writeonly uniform uimage2D LowResFB; 

uint BlendFog(uint color, uint depth)
{
    uint densityid = 0, densityfrac = 0;

    if (depth >= FogOffset)
    {
        depth -= FogOffset;
        depth = (depth >> 2) << FogShift;

        densityid = depth >> 17;
        if (densityid >= 32)
        {
            densityid = 32;
            densityfrac = 0;
        }
        else
        {
            densityfrac = depth & 0x1FFFFU;
        }
    }

    uint density =
        ((ToonTable[densityid].g * (0x20000U-densityfrac)) +
         (ToonTable[densityid+1].g * densityfrac)) >> 17;
    density = min(density, 128U);

    uint colorRB = color & 0x3F003FU;
    uint colorGA = (color >> 8) & 0x3F003FU;

    uint fogRB = FogColor & 0x3F003FU;
    uint fogGA = (FogColor >> 8) & 0x1F003FU;

    uint finalColorRB = ((fogRB * density) + (colorRB * (128-density))) >> 7;
    uint finalColorGA = ((fogGA * density) + (colorGA * (128-density))) >> 7;

    finalColorRB &= 0x3F003FU;
    finalColorGA &= 0x1F003FU;

    return (DispCnt & (1U<<6)) != 0
        ? (bitfieldInsert(color, finalColorGA >> 16, 24, 8))
        : (finalColorRB | (finalColorGA << 8));
}

void main()
{
    int srcX = int(gl_GlobalInvocationID.x);
    int resultOffset = int(srcX) + int(gl_GlobalInvocationID.y) * ScreenWidth;

    uvec2 color = uvec2(ResultValue[resultOffset+ResultColorStart], ResultValue[resultOffset+FramebufferStride+ResultColorStart]);
    uvec2 depth = uvec2(ResultValue[resultOffset+ResultDepthStart], ResultValue[resultOffset+FramebufferStride+ResultDepthStart]);
    uvec2 attr = uvec2(ResultValue[resultOffset+ResultAttrStart], ResultValue[resultOffset+FramebufferStride+ResultAttrStart]);

#ifdef EdgeMarking
    if ((attr.x & 0xFU) != 0U)
    {
        uvec4 otherAttr = uvec4(ClearAttr);
        uvec4 otherDepth = uvec4(ClearDepth);

        if (srcX > 0U)
        {
            otherAttr.x = ResultValue[resultOffset-1+ResultAttrStart];
            otherDepth.x = ResultValue[resultOffset-1+ResultDepthStart];
        }
        if (srcX < ScreenWidth-1)
        {
            otherAttr.y = ResultValue[resultOffset+1+ResultAttrStart];
            otherDepth.y = ResultValue[resultOffset+1+ResultDepthStart];
        }
        if (gl_GlobalInvocationID.y > 0U)
        {
            otherAttr.z = ResultValue[resultOffset-ScreenWidth+ResultAttrStart];
            otherDepth.z = ResultValue[resultOffset-ScreenWidth+ResultDepthStart];
        }
        if (gl_GlobalInvocationID.y < ScreenHeight-1)
        {
            otherAttr.w = ResultValue[resultOffset+ScreenWidth+ResultAttrStart];
            otherDepth.w = ResultValue[resultOffset+ScreenWidth+ResultDepthStart];
        }

        uint polyId = bitfieldExtract(attr.x, 24, 6);
        uvec4 otherPolyId = bitfieldExtract(otherAttr, 24, 6);

        bvec4 polyIdMismatch = notEqual(uvec4(polyId), otherPolyId);
        bvec4 nearer = lessThan(uvec4(depth.x), otherDepth);

        if ((polyIdMismatch.x && nearer.x)
            || (polyIdMismatch.y && nearer.y)
            || (polyIdMismatch.z && nearer.z)
            || (polyIdMismatch.w && nearer.w))
        {
            color.x = ToonTable[polyId >> 3].b | (color.x & 0xFF000000U);
            attr.x = (attr.x & 0xFFFFE0FFU) | 0x00001000U;
        }
    }
#endif

#ifdef Fog
    if ((attr.x & (1U<<15)) != 0U)
    {
        color.x = BlendFog(color.x, depth.x);
    }

    if ((attr.x & 0xFU) != 0 && (attr.y & (1U<<15)) != 0U)
    {
        color.y = BlendFog(color.y, depth.y);
    }
#endif

#ifdef AntiAliasing
    // resolve anti-aliasing
    if ((attr.x & 0x3U) != 0)
    {
        uint coverage = (attr.x >> 8) & 0x1FU;

        if (coverage != 0)
        {
            uint topRB = color.x & 0x3F003FU;
            uint topG = color.x & 0x003F00U;
            uint topA = bitfieldExtract(color.x, 24, 5);

            uint botRB = color.y & 0x3F003FU;
            uint botG = color.y & 0x003F00U;
            uint botA = bitfieldExtract(color.y, 24, 5);

            coverage++;

            if (botA > 0)
            {
                topRB = ((topRB * coverage) + (botRB * (32-coverage))) >> 5;
                topG = ((topG * coverage) + (botG * (32-coverage))) >> 5;

                topRB &= 0x3F003FU;
                topG &= 0x003F00U;
            }

            topA = ((topA * coverage) + (botA * (32-coverage))) >> 5;

            color.x = topRB | topG | (topA << 24);
        }
        else
        {
            color.x = color.y;
        }
    }
#endif

//    if (bitfieldExtract(color.x, 24, 8) != 0U)
//        color.x |= 0x40000000U;
//    else
//        color.x = 0U;

    //if ((gl_GlobalInvocationID.y % 8) == 7 || (gl_GlobalInvocationID.y % 8) == 7)
    //    color.x = 0x1F00001FU | 0x40000000U;

    vec4 result = vec4(bitfieldExtract(color.x, 16, 8), bitfieldExtract(color.x, 8, 8), color.x & 0x3FU, bitfieldExtract(color.x, 24, 8));
    result /= vec4(63.0, 63.0, 63.0, 31.0);
    imageStore(FinalFB, ivec2(gl_GlobalInvocationID.xy), result);

    // It's a division by constant, so using the builtin division is fine
    const int scale = ScreenWidth/256;
    ivec2 lowresCoordinate = ivec2(gl_GlobalInvocationID.xy) / scale;
    ivec2 lowresCoordinateRest = ivec2(gl_GlobalInvocationID.xy) % scale;
    if (lowresCoordinateRest == ivec2(0, 0))
    {
        uvec4 color8;
        color8.x = bitfieldExtract(color.x, 0, 8);
        color8.y = bitfieldExtract(color.x, 8, 8);
        color8.z = bitfieldExtract(color.x, 16, 8);
        color8.w = bitfieldExtract(color.x, 24, 8);
        imageStore(LowResFB, lowresCoordinate, color8);
    }
}

)";

}

}

#endif