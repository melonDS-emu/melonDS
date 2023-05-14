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

#include "GPU3D_Compute.h"

#include <assert.h>

#include "OpenGLSupport.h"

#include "GPU3D_Compute_shaders.h"

namespace GPU3D
{

ComputeRenderer::ComputeRenderer()
    : Renderer3D(true), Texcache(TexcacheOpenGLLoader())
{}

ComputeRenderer::~ComputeRenderer()
{}



bool ComputeRenderer::CompileShader(GLuint& shader, const std::string& source, const std::initializer_list<const char*>& defines)
{
    std::string shaderName;
    std::string shaderSource;
    shaderSource += "#version 430 core\n";
    for (const char* define : defines)
    {
        shaderSource += "#define ";
        shaderSource += define;
        shaderSource += '\n';
        shaderName += define;
        shaderName += ',';
    }
    shaderSource += "#define ScreenWidth ";
    shaderSource += std::to_string(ScreenWidth);
    shaderSource += "\n#define ScreenHeight ";
    shaderSource += std::to_string(ScreenHeight);
    shaderSource += "\n#define MaxWorkTiles ";
    shaderSource += std::to_string(MaxWorkTiles);

    shaderSource += ComputeRendererShaders::Common;
    shaderSource += source;

    return OpenGL::CompileComputeProgram(shader, shaderSource.c_str(), shaderName.c_str());
}

void blah(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam)
{
    printf("%s\n", message);
}

bool ComputeRenderer::Init()
{
    //glDebugMessageCallback(blah, NULL);
    //glEnable(GL_DEBUG_OUTPUT);
    glGenBuffers(1, &YSpanSetupMemory);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, YSpanSetupMemory);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(SpanSetupY)*MaxYSpanSetups, nullptr, GL_DYNAMIC_DRAW);
    
    glGenBuffers(1, &RenderPolygonMemory);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, RenderPolygonMemory);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(RenderPolygon)*2048, nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &XSpanSetupMemory);
    glGenBuffers(1, &BinResultMemory);
    glGenBuffers(1, &FinalTileMemory);
    glGenBuffers(1, &YSpanIndicesTextureMemory);
    glGenBuffers(tilememoryLayer_Num, TileMemory);
    glGenBuffers(1, &WorkDescMemory);

    glGenTextures(1, &YSpanIndicesTexture);
    glGenTextures(1, &LowResFramebuffer);
    glBindTexture(GL_TEXTURE_2D, LowResFramebuffer);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 256, 192);

    glGenBuffers(1, &MetaUniformMemory);
    glBindBuffer(GL_UNIFORM_BUFFER, MetaUniformMemory);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(MetaUniform), nullptr, GL_DYNAMIC_DRAW);

    glGenSamplers(9, Samplers);
    for (u32 j = 0; j < 3; j++)
    {
        for (u32 i = 0; i < 3; i++)
        {
            const GLenum translateWrapMode[3] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT};
            glSamplerParameteri(Samplers[i+j*3], GL_TEXTURE_WRAP_S, translateWrapMode[i]);
            glSamplerParameteri(Samplers[i+j*3], GL_TEXTURE_WRAP_T, translateWrapMode[j]);
            glSamplerParameteri(Samplers[i+j*3], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glSamplerParameterf(Samplers[i+j*3], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }

    glGenBuffers(1, &PixelBuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelBuffer);
    glBufferData(GL_PIXEL_PACK_BUFFER, 256*192*4, NULL, GL_DYNAMIC_READ);

    return true;
}

void ComputeRenderer::DeInit()
{
    Texcache.Reset();

    glDeleteBuffers(1, &YSpanSetupMemory);
    glDeleteBuffers(1, &RenderPolygonMemory);
    glDeleteBuffers(1, &XSpanSetupMemory);
    glDeleteBuffers(1, &BinResultMemory);
    glDeleteBuffers(tilememoryLayer_Num, TileMemory);
    glDeleteBuffers(1, &WorkDescMemory);
    glDeleteBuffers(1, &FinalTileMemory);
    glDeleteBuffers(1, &YSpanIndicesTextureMemory);
    glDeleteTextures(1, &YSpanIndicesTexture);
    glDeleteTextures(1, &Framebuffer);
    glDeleteBuffers(1, &MetaUniformMemory);

    glDeleteSamplers(9, Samplers);
    glDeleteBuffers(1, &PixelBuffer);
}

void ComputeRenderer::DeleteShaders()
{
    std::initializer_list<GLuint> allPrograms =
    {
        ShaderInterpXSpans[0],
        ShaderInterpXSpans[1],
        ShaderBinCombined,
        ShaderDepthBlend[0],
        ShaderDepthBlend[1],
        ShaderRasteriseNoTexture[0],
        ShaderRasteriseNoTexture[1],
        ShaderRasteriseNoTextureToon[0],
        ShaderRasteriseNoTextureToon[1],
        ShaderRasteriseNoTextureHighlight[0],
        ShaderRasteriseNoTextureHighlight[1],
        ShaderRasteriseUseTextureDecal[0],
        ShaderRasteriseUseTextureDecal[1],
        ShaderRasteriseUseTextureModulate[0],
        ShaderRasteriseUseTextureModulate[1],
        ShaderRasteriseUseTextureToon[0],
        ShaderRasteriseUseTextureToon[1],
        ShaderRasteriseUseTextureHighlight[0],
        ShaderRasteriseUseTextureHighlight[1],
        ShaderRasteriseShadowMask[0],
        ShaderRasteriseShadowMask[1],
        ShaderClearCoarseBinMask,
        ShaderClearIndirectWorkCount,
        ShaderCalculateWorkListOffset,
        ShaderSortWork,
        ShaderFinalPass[0],
        ShaderFinalPass[1],
        ShaderFinalPass[2],
        ShaderFinalPass[3],
        ShaderFinalPass[4],
        ShaderFinalPass[5],
        ShaderFinalPass[6],
        ShaderFinalPass[7],
    };
    for (GLuint program : allPrograms)
        glDeleteProgram(program);
}

void ComputeRenderer::Reset()
{
    Texcache.Reset();
}

void ComputeRenderer::SetRenderSettings(GPU::RenderSettings& settings)
{
    if (ScaleFactor != -1)
    {
        DeleteShaders();
    }

    ScaleFactor = settings.GL_ScaleFactor;
    ScreenWidth = 256 * ScaleFactor;
    ScreenHeight = 192 * ScaleFactor;

    TilesPerLine = ScreenWidth/TileSize;
    TileLines = ScreenHeight/TileSize;

    HiresCoordinates = settings.GL_HiresCoordinates;

    MaxWorkTiles = TilesPerLine*TileLines*8;

    for (int i = 0; i < tilememoryLayer_Num; i++)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, TileMemory[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 4*TileSize*TileSize*MaxWorkTiles, nullptr, GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, FinalTileMemory);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4*3*2*ScreenWidth*ScreenHeight, nullptr, GL_DYNAMIC_DRAW);

    int binResultSize = sizeof(BinResultHeader)
        + TilesPerLine*TileLines*CoarseBinStride*4 // BinnedMaskCoarse
        + TilesPerLine*TileLines*BinStride*4 // BinnedMask
        + TilesPerLine*TileLines*BinStride*4; // WorkOffsets
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, BinResultMemory);
    glBufferData(GL_SHADER_STORAGE_BUFFER, binResultSize, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, WorkDescMemory);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MaxWorkTiles*2*4*2, nullptr, GL_DYNAMIC_DRAW);

    if (Framebuffer != 0)
        glDeleteTextures(1, &Framebuffer);
    glGenTextures(1, &Framebuffer);
    glBindTexture(GL_TEXTURE_2D, Framebuffer);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, ScreenWidth, ScreenHeight);

    // eh those are pretty bad guesses
    // though real hw shouldn't be eable to render all 2048 polygons on every line either
    int maxYSpanIndices = 64*2048 * ScaleFactor;
    YSpanIndices.resize(maxYSpanIndices);

    glBindBuffer(GL_TEXTURE_BUFFER, YSpanIndicesTextureMemory);
    glBufferData(GL_TEXTURE_BUFFER, maxYSpanIndices*2*4, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, XSpanSetupMemory);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(SpanSetupX)*maxYSpanIndices, nullptr, GL_DYNAMIC_DRAW);

    glBindTexture(GL_TEXTURE_BUFFER, YSpanIndicesTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16UI, YSpanIndicesTextureMemory);

    CompileShader(ShaderInterpXSpans[0], ComputeRendererShaders::InterpSpans, {"InterpSpans", "ZBuffer"});
    CompileShader(ShaderInterpXSpans[1], ComputeRendererShaders::InterpSpans, {"InterpSpans", "WBuffer"});
    CompileShader(ShaderBinCombined, ComputeRendererShaders::BinCombined, {"BinCombined"});
    CompileShader(ShaderDepthBlend[0], ComputeRendererShaders::DepthBlend, {"DepthBlend", "ZBuffer"});
    CompileShader(ShaderDepthBlend[1], ComputeRendererShaders::DepthBlend, {"DepthBlend", "WBuffer"});
    CompileShader(ShaderRasteriseNoTexture[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "NoTexture"});
    CompileShader(ShaderRasteriseNoTexture[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "NoTexture"});
    CompileShader(ShaderRasteriseNoTextureToon[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "NoTexture", "Toon"});
    CompileShader(ShaderRasteriseNoTextureToon[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "NoTexture", "Toon"});
    CompileShader(ShaderRasteriseNoTextureHighlight[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "NoTexture", "Highlight"});
    CompileShader(ShaderRasteriseNoTextureHighlight[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "NoTexture", "Highlight"});
    CompileShader(ShaderRasteriseUseTextureDecal[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Decal"});
    CompileShader(ShaderRasteriseUseTextureDecal[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Decal"});
    CompileShader(ShaderRasteriseUseTextureModulate[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Modulate"});
    CompileShader(ShaderRasteriseUseTextureModulate[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Modulate"});
    CompileShader(ShaderRasteriseUseTextureToon[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Toon"});
    CompileShader(ShaderRasteriseUseTextureToon[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Toon"});
    CompileShader(ShaderRasteriseUseTextureHighlight[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Highlight"});
    CompileShader(ShaderRasteriseUseTextureHighlight[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Highlight"});
    CompileShader(ShaderRasteriseShadowMask[0], ComputeRendererShaders::Rasterise, {"Rasterise", "ZBuffer", "ShadowMask"});
    CompileShader(ShaderRasteriseShadowMask[1], ComputeRendererShaders::Rasterise, {"Rasterise", "WBuffer", "ShadowMask"});
    CompileShader(ShaderClearCoarseBinMask, ComputeRendererShaders::ClearCoarseBinMask, {"ClearCoarseBinMask"});
    CompileShader(ShaderClearIndirectWorkCount, ComputeRendererShaders::ClearIndirectWorkCount, {"ClearIndirectWorkCount"});
    CompileShader(ShaderCalculateWorkListOffset, ComputeRendererShaders::CalcOffsets, {"CalculateWorkOffsets"});
    CompileShader(ShaderSortWork, ComputeRendererShaders::SortWork, {"SortWork"});
    CompileShader(ShaderFinalPass[0], ComputeRendererShaders::FinalPass, {"FinalPass"});
    CompileShader(ShaderFinalPass[1], ComputeRendererShaders::FinalPass, {"FinalPass", "EdgeMarking"});
    CompileShader(ShaderFinalPass[2], ComputeRendererShaders::FinalPass, {"FinalPass", "Fog"});
    CompileShader(ShaderFinalPass[3], ComputeRendererShaders::FinalPass, {"FinalPass", "EdgeMarking", "Fog"});
    CompileShader(ShaderFinalPass[4], ComputeRendererShaders::FinalPass, {"FinalPass", "AntiAliasing"});
    CompileShader(ShaderFinalPass[5], ComputeRendererShaders::FinalPass, {"FinalPass", "AntiAliasing", "EdgeMarking"});
    CompileShader(ShaderFinalPass[6], ComputeRendererShaders::FinalPass, {"FinalPass", "AntiAliasing", "Fog"});
    CompileShader(ShaderFinalPass[7], ComputeRendererShaders::FinalPass, {"FinalPass", "AntiAliasing", "EdgeMarking", "Fog"});
}

void ComputeRenderer::VCount144()
{

}

void ComputeRenderer::SetupAttrs(SpanSetupY* span, Polygon* poly, int from, int to)
{
    span->Z0 = poly->FinalZ[from];
    span->W0 = poly->FinalW[from];
    span->Z1 = poly->FinalZ[to];
    span->W1 = poly->FinalW[to];
    span->ColorR0 = poly->Vertices[from]->FinalColor[0];
    span->ColorG0 = poly->Vertices[from]->FinalColor[1];
    span->ColorB0 = poly->Vertices[from]->FinalColor[2];
    span->ColorR1 = poly->Vertices[to]->FinalColor[0];
    span->ColorG1 = poly->Vertices[to]->FinalColor[1];
    span->ColorB1 = poly->Vertices[to]->FinalColor[2];
    span->TexcoordU0 = poly->Vertices[from]->TexCoords[0];
    span->TexcoordV0 = poly->Vertices[from]->TexCoords[1];
    span->TexcoordU1 = poly->Vertices[to]->TexCoords[0];
    span->TexcoordV1 = poly->Vertices[to]->TexCoords[1];
}

void ComputeRenderer::SetupYSpanDummy(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int vertex, int side, s32 positions[10][2])
{
    s32 x0 = positions[vertex][0];
    if (side)
    {
        span->DxInitial = -0x40000;
        x0--;
    }
    else
    {
        span->DxInitial = 0;
    }

    span->X0 = span->X1 = x0;
    span->XMin = x0;
    span->XMax = x0;
    span->Y0 = span->Y1 = positions[vertex][1];

    if (span->XMin < rp->XMin)
    {
        rp->XMin = span->XMin;
        rp->XMinY = span->Y0;
    }
    if (span->XMax > rp->XMax)
    {
        rp->XMax = span->XMax;
        rp->XMaxY = span->Y0;
    }

    span->Increment = 0;

    span->I0 = span->I1 = span->IRecip = 0;
    span->Linear = true;

    span->XCovIncr = 0;

    span->IsDummy = true;

    SetupAttrs(span, poly, vertex, vertex);
}

void ComputeRenderer::SetupYSpan(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int from, int to, int side, s32 positions[10][2])
{
    span->X0 = positions[from][0];
    span->X1 = positions[to][0];
    span->Y0 = positions[from][1];
    span->Y1 = positions[to][1];

    SetupAttrs(span, poly, from, to);

    s32 minXY, maxXY;
    bool negative = false;
    if (span->X1 > span->X0)
    {
        span->XMin = span->X0;
        span->XMax = span->X1-1;

        minXY = span->Y0;
        maxXY = span->Y1;
    }
    else if (span->X1 < span->X0)
    {
        span->XMin = span->X1;
        span->XMax = span->X0-1;
        negative = true;

        minXY = span->Y1;
        maxXY = span->Y0;
    }
    else
    {
        span->XMin = span->X0;
        if (side) span->XMin--;
        span->XMax = span->XMin;

        // doesn't matter for completely vertical slope
        minXY = span->Y0;
        maxXY = span->Y0;
    }

    if (span->XMin < rp->XMin)
    {
        rp->XMin = span->XMin;
        rp->XMinY = minXY;
    }
    if (span->XMax > rp->XMax)
    {
        rp->XMax = span->XMax;
        rp->XMaxY = maxXY;
    }

    span->IsDummy = false;

    s32 xlen = span->XMax+1 - span->XMin;
    s32 ylen = span->Y1 - span->Y0;

    // slope increment has a 18-bit fractional part
    // note: for some reason, x/y isn't calculated directly,
    // instead, 1/y is calculated and then multiplied by x
    // TODO: this is still not perfect (see for example x=169 y=33)
    if (ylen == 0)
    {
        span->Increment = 0;
    }
    else if (ylen == xlen)
    {
        span->Increment = 0x40000;
    }
    else
    {
        s32 yrecip = (1<<18) / ylen;
        span->Increment = (span->X1-span->X0) * yrecip;
        if (span->Increment < 0) span->Increment = -span->Increment;
    }

    bool xMajor = (span->Increment > 0x40000);

    if (side)
    {
        // right

        if (xMajor)
            span->DxInitial = negative ? (0x20000 + 0x40000) : (span->Increment - 0x20000);
        else if (span->Increment != 0)
            span->DxInitial = negative ? 0x40000 : 0;
        else
            span->DxInitial = -0x40000;
    }
    else
    {
        // left

        if (xMajor)
            span->DxInitial = negative ? ((span->Increment - 0x20000) + 0x40000) : 0x20000;
        else if (span->Increment != 0)
            span->DxInitial = negative ? 0x40000 : 0;
        else
            span->DxInitial = 0;
    }

    if (xMajor)
    {
        if (side)
        {
            span->I0 = span->X0 - 1;
            span->I1 = span->X1 - 1;
        }
        else
        {
            span->I0 = span->X0;
            span->I1 = span->X1;
        }

        // used for calculating AA coverage
        span->XCovIncr = (ylen << 10) / xlen;
    }
    else
    {
        span->I0 = span->Y0;
        span->I1 = span->Y1;
    }

    if (span->I0 != span->I1)
        span->IRecip = (1<<30) / (span->I1 - span->I0);
    else
        span->IRecip = 0;

    span->Linear = (span->W0 == span->W1) && !(span->W0 & 0x7E) && !(span->W1 & 0x7E);

    if ((span->W0 & 0x1) && !(span->W1 & 0x1))
    {
        span->W0n = (span->W0 - 1) >> 1;
        span->W0d = (span->W0 + 1) >> 1;
        span->W1d = span->W1 >> 1;
    }
    else
    {
        span->W0n = span->W0 >> 1;
        span->W0d = span->W0 >> 1;
        span->W1d = span->W1 >> 1;
    }
}

struct Variant
{
    GLuint Texture, Sampler;
    u16 Width, Height;
    u8 BlendMode;

    bool operator==(const Variant& other)
    {
        return Texture == other.Texture && Sampler == other.Sampler && BlendMode == other.BlendMode;
    }
};

/*
    Antialiasing
    W-Buffer
    With Texture
    0
    1, 3
    2
    without Texture
    2
    0, 1, 3

    => 20 Shader + 1x Shadow Mask
*/

void ComputeRenderer::RenderFrame()
{
    //printf("render frame\n");

    if (!Texcache.Update() && RenderFrameIdentical)
    {
        return;
    }

    int numYSpans = 0;
    int numSetupIndices = 0;

    /*
        Some games really like to spam small textures, often
        to store the data like PPU tiles. E.g. Shantae
        or some Mega Man game. Fortunately they are usually kind
        enough to not vary the texture size all too often (usually
        they just use 8x8 or 16x for everything).

        This is the reason we have this whole mess where textures of
        the same size are put into array textures. This allows
        to increase the batch size.
        Less variance between each Variant hah!
    */
    u32 numVariants = 0, prevVariant, prevTexLayer;
    Variant variants[MaxVariants];

    bool enableTextureMaps = RenderDispCnt & (1<<0);

    for (int i = 0; i < RenderNumPolygons; i++)
    {
        Polygon* polygon = RenderPolygonRAM[i];

        u32 nverts = polygon->NumVertices;
        u32 vtop = polygon->VTop, vbot = polygon->VBottom;

        u32 curVL = vtop, curVR = vtop;
        u32 nextVL, nextVR;

        RenderPolygons[i].FirstXSpan = numSetupIndices;
        RenderPolygons[i].Attr = polygon->Attr;

        bool foundVariant = false;
        if (i > 0)
        {
            // if the whole texture attribute matches
            // the texture layer will also match
            Polygon* prevPolygon = RenderPolygonRAM[i - 1];
            foundVariant = prevPolygon->TexParam == polygon->TexParam
                && prevPolygon->TexPalette == polygon->TexPalette
                && (prevPolygon->Attr & 0x30) == (polygon->Attr & 0x30)
                && prevPolygon->IsShadowMask == polygon->IsShadowMask;
        }

        if (!foundVariant)
        {
            Variant variant;
            variant.BlendMode = polygon->IsShadowMask ? 4 : ((polygon->Attr >> 4) & 0x3);
            variant.Texture = 0;
            variant.Sampler = 0;
            u32* textureLastVariant = nullptr;
            // we always need to look up the texture to get the layer of the array texture
            if (enableTextureMaps && (polygon->TexParam >> 26) & 0x7)
            {
                Texcache.GetTexture(polygon->TexParam, polygon->TexPalette, variant.Texture, prevTexLayer, textureLastVariant);
                bool wrapS = (polygon->TexParam >> 16) & 1;
                bool wrapT = (polygon->TexParam >> 17) & 1;
                bool mirrorS = (polygon->TexParam >> 18) & 1;
                bool mirrorT = (polygon->TexParam >> 19) & 1;
                variant.Sampler = Samplers[(wrapS ? (mirrorS ? 2 : 1) : 0) + (wrapT ? (mirrorT ? 2 : 1) : 0) * 3];

                if (*textureLastVariant < numVariants && variants[*textureLastVariant] == variant)
                {
                    foundVariant = true;
                    prevVariant = *textureLastVariant;
                }
            }

            if (!foundVariant)
            {
                for (int j = numVariants - 1; j >= 0; j--)
                {
                    if (variants[j] == variant)
                    {
                        foundVariant = true;
                        prevVariant = j;
                        goto foundVariant;
                    }
                }

                prevVariant = numVariants;
                variants[numVariants] = variant;
                variants[numVariants].Width = TextureWidth(polygon->TexParam);
                variants[numVariants].Height = TextureHeight(polygon->TexParam);
                numVariants++;
                assert(numVariants <= MaxVariants);
            foundVariant:;

                if (textureLastVariant)
                    *textureLastVariant = prevVariant;
            }
        }
        RenderPolygons[i].Variant = prevVariant;
        RenderPolygons[i].TextureLayer = (float)prevTexLayer;

        if (polygon->FacingView)
        {
            nextVL = curVL + 1;
            if (nextVL >= nverts) nextVL = 0;
            nextVR = curVR - 1;
            if ((s32)nextVR < 0) nextVR = nverts - 1;
        }
        else
        {
            nextVL = curVL - 1;
            if ((s32)nextVL < 0) nextVL = nverts - 1;
            nextVR = curVR + 1;
            if (nextVR >= nverts) nextVR = 0;
        }

        s32 scaledPositions[10][2];
        s32 ytop = ScreenHeight, ybot = 0;
        for (int i = 0; i < polygon->NumVertices; i++)
        {
            if (HiresCoordinates)
            {
                scaledPositions[i][0] = (polygon->Vertices[i]->HiresPosition[0] * ScaleFactor) >> 4;
                scaledPositions[i][1] = (polygon->Vertices[i]->HiresPosition[1] * ScaleFactor) >> 4;
            }
            else
            {
                scaledPositions[i][0] = polygon->Vertices[i]->FinalPosition[0] * ScaleFactor;
                scaledPositions[i][1] = polygon->Vertices[i]->FinalPosition[1] * ScaleFactor;
            }
            ytop = std::min(scaledPositions[i][1], ytop);
            ybot = std::max(scaledPositions[i][1], ybot);
        }
        RenderPolygons[i].YTop = ytop;
        RenderPolygons[i].YBot = ybot;
        RenderPolygons[i].XMin = ScreenWidth;
        RenderPolygons[i].XMax = 0;

        if (ybot == ytop)
        {
            vtop = 0; vbot = 0;

            RenderPolygons[i].YBot++;

            int j = 1;
            if (scaledPositions[j][0] < scaledPositions[vtop][0]) vtop = j;
            if (scaledPositions[j][0] > scaledPositions[vbot][0]) vbot = j;

            j = nverts - 1;
            if (scaledPositions[j][0] < scaledPositions[vtop][0]) vtop = j;
            if (scaledPositions[j][0] > scaledPositions[vbot][0]) vbot = j;

            assert(numYSpans < MaxYSpanSetups);
            u32 curSpanL = numYSpans;
            SetupYSpanDummy(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, vtop, 0, scaledPositions);
            assert(numYSpans < MaxYSpanSetups);
            u32 curSpanR = numYSpans;
            SetupYSpanDummy(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, vbot, 1, scaledPositions);

            YSpanIndices[numSetupIndices].PolyIdx = i;
            YSpanIndices[numSetupIndices].SpanIdxL = curSpanL;
            YSpanIndices[numSetupIndices].SpanIdxR = curSpanR;
            YSpanIndices[numSetupIndices].Y = ytop;
            numSetupIndices++;
        }
        else
        {
            u32 curSpanL = numYSpans;
            assert(numYSpans < MaxYSpanSetups);
            SetupYSpan(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, curVL, nextVL, 0, scaledPositions);
            u32 curSpanR = numYSpans;
            assert(numYSpans < MaxYSpanSetups);
            SetupYSpan(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, curVR, nextVR, 1, scaledPositions);

            for (u32 y = ytop; y < ybot; y++)
            {
                if (y >= scaledPositions[nextVL][1] && curVL != polygon->VBottom)
                {
                    while (y >= scaledPositions[nextVL][1] && curVL != polygon->VBottom)
                    {
                        curVL = nextVL;
                        if (polygon->FacingView)
                        {
                            nextVL = curVL + 1;
                            if (nextVL >= nverts)
                                nextVL = 0;
                        }
                        else
                        {
                            nextVL = curVL - 1;
                            if ((s32)nextVL < 0)
                                nextVL = nverts - 1;
                        }
                    }


                    assert(numYSpans < MaxYSpanSetups);
                    curSpanL = numYSpans;
                    SetupYSpan(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, curVL, nextVL, 0, scaledPositions);
                }
                if (y >= scaledPositions[nextVR][1] && curVR != polygon->VBottom)
                {
                    while (y >= scaledPositions[nextVR][1] && curVR != polygon->VBottom)
                    {
                        curVR = nextVR;
                        if (polygon->FacingView)
                        {
                            nextVR = curVR - 1;
                            if ((s32)nextVR < 0)
                                nextVR = nverts - 1;
                        }
                        else
                        {
                            nextVR = curVR + 1;
                            if (nextVR >= nverts)
                                nextVR = 0;
                        }
                    }

                    assert(numYSpans < MaxYSpanSetups);
                    curSpanR = numYSpans;
                    SetupYSpan(&RenderPolygons[i] ,&YSpanSetups[numYSpans++], polygon, curVR, nextVR, 1, scaledPositions);
                }

                YSpanIndices[numSetupIndices].PolyIdx = i;
                YSpanIndices[numSetupIndices].SpanIdxL = curSpanL;
                YSpanIndices[numSetupIndices].SpanIdxR = curSpanR;
                YSpanIndices[numSetupIndices].Y = y;
                numSetupIndices++;
            }
        }

        //printf("polygon min max %d %d | %d %d\n", RenderPolygons[i].XMin, RenderPolygons[i].XMinY, RenderPolygons[i].XMax, RenderPolygons[i].XMaxY);
    }

    /*for (u32 i = 0; i < RenderNumPolygons; i++)
    {
        if (RenderPolygons[i].Variant >= numVariants)
        {
            printf("blarb2 %d %d %d\n", RenderPolygons[i].Variant, i, RenderNumPolygons);
        }
        //assert(RenderPolygons[i].Variant < numVariants);
    }*/

    if (numYSpans > 0)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, YSpanSetupMemory);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(SpanSetupY)*numYSpans, YSpanSetups);

        glBindBuffer(GL_TEXTURE_BUFFER, YSpanIndicesTextureMemory);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, numSetupIndices*4*2, YSpanIndices.data());

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, RenderPolygonMemory);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, RenderNumPolygons*sizeof(RenderPolygon), RenderPolygons);
        // we haven't accessed image data yet, so we don't need to invalidate anything
    }

    //printf("found via %d %d %d of %d\n", foundviatexcache, foundviaprev, numslow, RenderNumPolygons);

    // bind everything
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, RenderPolygonMemory);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, XSpanSetupMemory);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, YSpanSetupMemory);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, FinalTileMemory);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, BinResultMemory);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, WorkDescMemory);

    MetaUniform meta;
    meta.DispCnt = RenderDispCnt;
    meta.NumPolygons = RenderNumPolygons;
    meta.NumVariants = numVariants;
    meta.AlphaRef = RenderAlphaRef;
    {
        u32 r = (RenderClearAttr1 << 1) & 0x3E; if (r) r++;
        u32 g = (RenderClearAttr1 >> 4) & 0x3E; if (g) g++;
        u32 b = (RenderClearAttr1 >> 9) & 0x3E; if (b) b++;
        u32 a = (RenderClearAttr1 >> 16) & 0x1F;
        meta.ClearColor = r | (g << 8) | (b << 16) | (a << 24);
        meta.ClearDepth = ((RenderClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;
        meta.ClearAttr = RenderClearAttr1 & 0x3F008000;
    }
    for (u32 i = 0; i < 32; i++)
    {
        u32 color = RenderToonTable[i];
        u32 r = (color << 1) & 0x3E;
        u32 g = (color >> 4) & 0x3E;
        u32 b = (color >> 9) & 0x3E;
        if (r) r++;
        if (g) g++;
        if (b) b++;

        meta.ToonTable[i*4+0] = r | (g << 8) | (b << 16);
    }
    for (u32 i = 0; i < 34; i++)
    {
        meta.ToonTable[i*4+1] = RenderFogDensityTable[i];
    }
    for (u32 i = 0; i < 8; i++)
    {
        u32 color = RenderEdgeTable[i];
        u32 r = (color << 1) & 0x3E;
        u32 g = (color >> 4) & 0x3E;
        u32 b = (color >> 9) & 0x3E;
        if (r) r++;
        if (g) g++;
        if (b) b++;

        meta.ToonTable[i*4+2] = r | (g << 8) | (b << 16);
    }
    meta.FogOffset = RenderFogOffset;
    meta.FogShift = RenderFogShift;
    {
        u32 fogR = (RenderFogColor << 1) & 0x3E; if (fogR) fogR++;
        u32 fogG = (RenderFogColor >> 4) & 0x3E; if (fogG) fogG++;
        u32 fogB = (RenderFogColor >> 9) & 0x3E; if (fogB) fogB++;
        u32 fogA = (RenderFogColor >> 16) & 0x1F;
        meta.FogColor = fogR | (fogG << 8) | (fogB << 16) | (fogA << 24);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, MetaUniformMemory);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MetaUniform), &meta);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, MetaUniformMemory);

    glUseProgram(ShaderClearCoarseBinMask);
    glDispatchCompute(TilesPerLine*TileLines/32, 1, 1);

    bool wbuffer = false;
    if (numYSpans > 0)
    {
        wbuffer = RenderPolygonRAM[0]->WBuffer;

        glUseProgram(ShaderClearIndirectWorkCount);
        glDispatchCompute((numVariants+31)/32, 1, 1);

        // calculate x-spans
        glBindImageTexture(0, YSpanIndicesTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16UI);
        glUseProgram(ShaderInterpXSpans[wbuffer]);
        glDispatchCompute((numSetupIndices + 31) / 32, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BUFFER);

        // bin polygons
        glUseProgram(ShaderBinCombined);
        glDispatchCompute(((RenderNumPolygons + 31) / 32), ScreenWidth/CoarseTileW, ScreenHeight/CoarseTileH);
        glMemoryBarrier(GL_SHADER_STORAGE_BUFFER);

        // calculate list offsets
        glUseProgram(ShaderCalculateWorkListOffset);
        glDispatchCompute((numVariants + 31) / 32, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BUFFER);

        // sort shader work
        glUseProgram(ShaderSortWork);
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, BinResultMemory);
        glDispatchComputeIndirect(offsetof(BinResultHeader, SortWorkWorkCount));
        glMemoryBarrier(GL_SHADER_STORAGE_BUFFER);

        glActiveTexture(GL_TEXTURE0);

        for (int i = 0; i < tilememoryLayer_Num; i++)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2+i, TileMemory[i]);

        // rasterise
        {
            bool highLightMode = RenderDispCnt & (1<<1);

            GLuint shadersNoTexture[] =
            {
                ShaderRasteriseNoTexture[wbuffer],
                ShaderRasteriseNoTexture[wbuffer],
                highLightMode
                    ? ShaderRasteriseNoTextureHighlight[wbuffer]
                    : ShaderRasteriseNoTextureToon[wbuffer],
                ShaderRasteriseNoTexture[wbuffer],
                ShaderRasteriseShadowMask[wbuffer]
            };
            GLuint shadersUseTexture[] =
            {
                ShaderRasteriseUseTextureModulate[wbuffer],
                ShaderRasteriseUseTextureDecal[wbuffer],
                highLightMode
                    ? ShaderRasteriseUseTextureHighlight[wbuffer]
                    : ShaderRasteriseUseTextureToon[wbuffer],
                ShaderRasteriseUseTextureDecal[wbuffer],
                ShaderRasteriseShadowMask[wbuffer]
            };

            GLuint prevShader = 0;
            s32 prevTexture = 0, prevSampler = 0;
            for (int i = 0; i < numVariants; i++)
            {
                GLuint shader = 0;
                if (variants[i].Texture == 0)
                {
                    shader = shadersNoTexture[variants[i].BlendMode];
                }
                else
                {
                    shader = shadersUseTexture[variants[i].BlendMode];
                    if (variants[i].Texture != prevTexture)
                    {
                        glBindTexture(GL_TEXTURE_2D_ARRAY, variants[i].Texture);
                        prevTexture = variants[i].Texture;
                    }
                    if (variants[i].Sampler != prevSampler)
                    {
                        glBindSampler(0, variants[i].Sampler);
                        prevSampler = variants[i].Sampler;
                    }
                }
                assert(shader != 0);
                if (shader != prevShader)
                {
                    glUseProgram(shader);
                    prevShader = shader;
                }

                glUniform1ui(UniformIdxCurVariant, i);
                glUniform2f(UniformIdxTextureSize, 1.f / variants[i].Width, 1.f / variants[i].Height);
                glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, BinResultMemory);
                glDispatchComputeIndirect(offsetof(BinResultHeader, VariantWorkCount) + i*4*4);
            }
        }
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // compose final image
    glUseProgram(ShaderDepthBlend[wbuffer]);
    glDispatchCompute(ScreenWidth/TileSize, ScreenHeight/TileSize, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindImageTexture(0, Framebuffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glBindImageTexture(1, LowResFramebuffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
    u32 finalPassShader = 0;
    if (RenderDispCnt & (1<<4))
        finalPassShader |= 0x4;
    if (RenderDispCnt & (1<<7))
        finalPassShader |= 0x2;
    if (RenderDispCnt & (1<<5))
        finalPassShader |= 0x1;
    
    glUseProgram(ShaderFinalPass[finalPassShader]);
    glDispatchCompute(ScreenWidth/32, ScreenHeight, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    /*u64 starttime = armGetSystemTick();
    EmuQueue.waitIdle();
    printf("total time %f\n", armTicksToNs(armGetSystemTick()-starttime)*0.000001f);*/

    /*for (u32 i = 0; i < RenderNumPolygons; i++)
    {
        if (RenderPolygons[i].Variant >= numVariants)
        {
            printf("blarb %d %d %d\n", RenderPolygons[i].Variant, i, RenderNumPolygons);
        }
        //assert(RenderPolygons[i].Variant < numVariants);
    }*/

    /*for (int i = 0; i < binresult->SortWorkWorkCount[0]*32; i++)
    {
        printf("sorted %x %x\n", binresult->SortedWork[i*2+0], binresult->SortedWork[i*2+1]);
    }*/
/*    if (polygonvisible != -1)
    {
        SpanSetupX* xspans = Gfx::DataHeap->CpuAddr<SpanSetupX>(XSpanSetupMemory);
        printf("span result\n");
        Polygon* poly = RenderPolygonRAM[polygonvisible];
        u32 xspanoffset = RenderPolygons[polygonvisible].FirstXSpan;
        for (u32 i = 0; i < (poly->YBottom - poly->YTop); i++)
        {
            printf("%d: %d - %d | %d %d | %d %d\n", i + poly->YTop, xspans[xspanoffset + i].X0, xspans[xspanoffset + i].X1, xspans[xspanoffset + i].__pad0, xspans[xspanoffset + i].__pad1, RenderPolygons[polygonvisible].YTop, RenderPolygons[polygonvisible].YBot);
        }
    }*/
/*
    printf("xspans: %d\n", numSetupIndices);
    SpanSetupX* xspans = Gfx::DataHeap->CpuAddr<SpanSetupX>(XSpanSetupMemory[curSlice]);
    for (int i = 0; i < numSetupIndices; i++)
    {
        printf("poly %d %d %d | line %d | %d to %d\n", YSpanIndices[i].PolyIdx, YSpanIndices[i].SpanIdxL, YSpanIndices[i].SpanIdxR, YSpanIndices[i].Y, xspans[i].X0, xspans[i].X1);
    }
    printf("bin result\n");
    BinResult* binresult = Gfx::DataHeap->CpuAddr<BinResult>(BinResultMemory);
    for (u32 y = 0; y < 192/8; y++)
    {
        for (u32 x = 0; x < 256/8; x++)
        {
            printf("%08x ", binresult->BinnedMaskCoarse[(x + y * (256/8)) * 2]);
        }
        printf("\n");
    }*/
}

void ComputeRenderer::RestartFrame()
{

}

u32* ComputeRenderer::GetLine(int line)
{
    int stride = 256;

    if (line == 0)
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelBuffer);
        u8* data = (u8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (data) memcpy(&FramebufferCPU[0], data, 4*stride*192);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    return &FramebufferCPU[stride * line];
}

void ComputeRenderer::SetupAccelFrame()
{
    glBindTexture(GL_TEXTURE_2D, Framebuffer);
}

void ComputeRenderer::PrepareCaptureFrame()
{
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelBuffer);
    glBindTexture(GL_TEXTURE_2D, LowResFramebuffer);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);
}

}