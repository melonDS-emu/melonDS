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

#ifndef GPU3D_COMPUTE
#define GPU3D_COMPUTE

#include "GPU3D.h"

#include "OpenGLSupport.h"

#include "NonStupidBitfield.h"

#include <unordered_map>

namespace GPU3D
{

class ComputeRenderer : public Renderer3D
{
public:
    ComputeRenderer();
    ~ComputeRenderer() override;

    bool Init() override;
    void DeInit() override;
    void Reset() override;

    void SetRenderSettings(GPU::RenderSettings& settings) override;

    void VCount144() override;

    void RenderFrame() override;
    void RestartFrame() override;
    u32* GetLine(int line) override;

    //dk::Fence FrameReady = {};
    //dk::Fence FrameReserveFence = {};
private:
    GLuint ShaderInterpXSpans[2];
    GLuint ShaderBinCombined;
    GLuint ShaderDepthBlend[2];
    GLuint ShaderRasteriseNoTexture[2];
    GLuint ShaderRasteriseNoTextureToon[2];
    GLuint ShaderRasteriseNoTextureHighlight[2];
    GLuint ShaderRasteriseUseTextureDecal[2];
    GLuint ShaderRasteriseUseTextureModulate[2];
    GLuint ShaderRasteriseUseTextureToon[2];
    GLuint ShaderRasteriseUseTextureHighlight[2];
    GLuint ShaderRasteriseShadowMask[2];
    GLuint ShaderClearCoarseBinMask;
    GLuint ShaderClearIndirectWorkCount;
    GLuint ShaderCalculateWorkListOffset;
    GLuint ShaderSortWork;
    GLuint ShaderFinalPass[8];

    GLuint YSpanIndicesTextureMemory;
    GLuint YSpanIndicesTexture;
    GLuint YSpanSetupMemory;
    GLuint XSpanSetupMemory;
    GLuint BinResultMemory;
    GLuint RenderPolygonMemory;
    GLuint TileMemory;
    GLuint FinalTileMemory;

    u32 DummyLine[256] = {};

    struct SpanSetupY
    {
        // Attributes
        s32 Z0, Z1, W0, W1;
        s32 ColorR0, ColorG0, ColorB0;
        s32 ColorR1, ColorG1, ColorB1;
        s32 TexcoordU0, TexcoordV0;
        s32 TexcoordU1, TexcoordV1;

        // Interpolator
        s32 I0, I1;
        s32 Linear;
        s32 IRecip;
        s32 W0n, W0d, W1d;

        // Slope
        s32 Increment;

        s32 X0, X1, Y0, Y1;
        s32 XMin, XMax;
        s32 DxInitial;

        s32 XCovIncr;
        u32 IsDummy, __pad1;
    };
    struct SpanSetupX
    {
        s32 X0, X1;

        s32 EdgeLenL, EdgeLenR, EdgeCovL, EdgeCovR;

        s32 XRecip;

        u32 Flags;

        s32 Z0, Z1, W0, W1;
        s32 ColorR0, ColorG0, ColorB0;
        s32 ColorR1, ColorG1, ColorB1;
        s32 TexcoordU0, TexcoordV0;
        s32 TexcoordU1, TexcoordV1;

        s32 CovLInitial, CovRInitial;
    };
    struct SetupIndices
    {
        u16 PolyIdx, SpanIdxL, SpanIdxR, Y;
    };
    struct RenderPolygon
    {
        u32 FirstXSpan;
        s32 YTop, YBot;

        s32 XMin, XMax;
        s32 XMinY, XMaxY;

        u32 Variant;
        u32 Attr;

        float TextureLayer;
        u32 __pad0, __pad1;
    };

    static const int TileSize = 8;
    static const int CoarseTileCountX = 8;
    static const int CoarseTileCountY = 4;
    static const int CoarseTileW = CoarseTileCountX * TileSize;
    static const int CoarseTileH = CoarseTileCountY * TileSize;

    static const int TilesPerLine = 256/TileSize;
    static const int TileLines = 192/TileSize;

    static const int BinStride = 2048/32;
    static const int CoarseBinStride = BinStride/32;

    static const int MaxWorkTiles = TilesPerLine*TileLines*48;
    static const int MaxVariants = 256;

    static const int UniformIdxCurVariant = 0;
    static const int UniformIdxTextureSize = 1;

    struct BinResult
    {
        u32 VariantWorkCount[MaxVariants*4];
        u32 SortedWorkOffset[MaxVariants];

        u32 SortWorkWorkCount[4];
        u32 UnsortedWorkDescs[MaxWorkTiles*2];
        u32 SortedWork[MaxWorkTiles*2];

        u32 BinnedMaskCoarse[TilesPerLine*TileLines*CoarseBinStride];
        u32 BinnedMask[TilesPerLine*TileLines*BinStride];
        u32 WorkOffsets[TilesPerLine*TileLines*BinStride];
    };

    struct Tiles
    {
        u32 ColorTiles[MaxWorkTiles*TileSize*TileSize];
        u32 DepthTiles[MaxWorkTiles*TileSize*TileSize];
        u32 AttrStencilTiles[MaxWorkTiles*TileSize*TileSize];
    };

    struct FinalTiles
    {
        u32 ColorResult[256*192*2];
        u32 DepthResult[256*192*2];
        u32 AttrResult[256*192*2];
    };

    // eh those are pretty bad guesses
    // though real hw shouldn't be eable to render all 2048 polygons on every line either
    static const int MaxYSpanIndices = 64*2048;
    static const int MaxYSpanSetups = 6144*2;
    SetupIndices YSpanIndices[MaxYSpanIndices];
    SpanSetupY YSpanSetups[MaxYSpanSetups];
    RenderPolygon RenderPolygons[2048];

    struct TexArrayEntry
    {
        u16 TexArrayIdx;
        u16 LayerIdx;
    };
    struct TexArray
    {
        GLuint Image;
        u32 ImageDescriptor;
    };

    struct TexCacheEntry
    {
        u32 DescriptorIdx;
        u32 LastVariant; // very cheap way to make variant lookup faster

        u32 TextureRAMStart[2], TextureRAMSize[2];
        u32 TexPalStart, TexPalSize;
        u8 WidthLog2, HeightLog2;
        TexArrayEntry Texture;

        u64 TextureHash[2];
        u64 TexPalHash;
    };
    std::unordered_map<u64, TexCacheEntry> TexCache;

    struct MetaUniform
    {
        u32 NumPolygons;
        u32 NumVariants;

        u32 AlphaRef;
        u32 DispCnt;

        u32 ToonTable[4*34];

        u32 ClearColor, ClearDepth, ClearAttr;

        u32 FogOffset, FogShift, FogColor;

        u32 XScroll;
    };
    GLuint MetaUniformMemory;

    static const u32 TexCacheMaxImages = 4096;

    u32 FreeImageDescriptorsCount = 0;
    u32 FreeImageDescriptors[TexCacheMaxImages];

    std::vector<TexArrayEntry> FreeTextures[8][8];
    std::vector<TexArray> TexArrays[8][8];

    u32 TextureDecodingBuffer[1024*1024];

    TexCacheEntry& GetTexture(u32 textureParam, u32 paletteParam);

    void SetupAttrs(SpanSetupY* span, Polygon* poly, int from, int to);
    void SetupYSpan(int polynum, SpanSetupY* span, Polygon* poly, int from, int to, u32 y, int side);
    void SetupYSpanDummy(SpanSetupY* span, Polygon* poly, int vertex, int side);

    bool CompileShader(GLuint& shader, const char* source, const std::initializer_list<const char*>& defines);
};

}

#endif