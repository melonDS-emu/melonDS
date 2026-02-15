/*
    Copyright 2016-2025 melonDS team

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

#include <memory>

#include "types.h"

#include "GPU3D.h"

#include "OpenGLSupport.h"

#include "GPU3D_TexcacheOpenGL.h"

#include "NonStupidBitfield.h"

namespace melonDS
{
class GLRenderer;

class ComputeRenderer3D : public Renderer3D
{
public:
    ComputeRenderer3D(melonDS::GPU3D& gpu3D, GLRenderer& parent);
    ~ComputeRenderer3D() override;
    bool Init() override;
    void Reset() override;

    void SetRenderSettings(int scale, bool highResolutionCoordinates);

    void RenderFrame() override;
    void RestartFrame() override;
    u32* GetLine(int line) override;

    bool NeedsShaderCompile() override { return ShaderStepIdx != 33; }
    void ShaderCompileStep(int& current, int& count) override;

private:
    GLRenderer& Parent;

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
    GLuint WorkDescMemory;

    enum
    {
        tilememoryLayer_Color,
        tilememoryLayer_Depth,
        tilememoryLayer_Attr,
        tilememoryLayer_Num,
    };

    GLuint TileMemory[tilememoryLayer_Num];
    GLuint FinalTileMemory;

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
        u32 IsDummy;
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
    };

    int TileSize;
    static constexpr int CoarseTileCountX = 8;
    int CoarseTileCountY;
    int CoarseTileArea;
    int CoarseTileW;
    int CoarseTileH;
    int ClearCoarseBinMaskLocalSize;

    static constexpr int BinStride = 2048/32;
    static constexpr int CoarseBinStride = BinStride/32;

    static constexpr int MaxVariants = 256;

    static constexpr int UniformIdxCurVariant = 0;
    static constexpr int UniformIdxTextureSize = 1;
    static constexpr int UniformIdxTexIsCapture = 2;
    static constexpr int UniformIdxCaptureYOffset = 3;

    static constexpr int MaxFullscreenLayers = 16;

    struct BinResultHeader
    {
        u32 VariantWorkCount[MaxVariants*4];
        u32 SortedWorkOffset[MaxVariants];

        u32 SortWorkWorkCount[4];
    };

    static const int MaxYSpanSetups = 6144*2;
    std::vector<SetupIndices> YSpanIndices;
    SpanSetupY YSpanSetups[MaxYSpanSetups];
    RenderPolygon RenderPolygons[2048];

    TexcacheOpenGL Texcache;

    struct MetaUniform
    {
        u32 NumPolygons;
        u32 NumVariants;

        u32 AlphaRef;
        u32 DispCnt;

        u32 ToonTable[4*34];

        u32 ClearColor, ClearDepth, ClearAttr;

        u32 FogOffset, FogShift, FogColor;

        float ClearBitmapOffset[2];
    };
    GLuint MetaUniformMemory;

    GLuint Samplers[9];

    GLuint ClearBitmapTex[2];
    u32* ClearBitmap[2];
    u8 ClearBitmapDirty;

    GLuint Framebuffer = 0;

    int ScreenWidth, ScreenHeight;
    int TilesPerLine, TileLines;
    int ScaleFactor = -1;
    int MaxWorkTiles;
    bool HiresCoordinates;

    int ShaderStepIdx = 0;

    void DeleteShaders();

    void SetupAttrs(SpanSetupY* span, Polygon* poly, int from, int to);
    void SetupYSpan(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int from, int to, int side, s32 positions[10][2]);
    void SetupYSpanDummy(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int vertex, int side, s32 positions[10][2]);

    bool CompileShader(GLuint& shader, const std::string& source, const std::initializer_list<const char*>& defines);
};

}

#endif