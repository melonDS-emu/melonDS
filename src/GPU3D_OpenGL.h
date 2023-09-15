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

#pragma once

#include "GPU3D.h"

#include "OpenGLSupport.h"


namespace GPU3D
{
class GLRenderer : public Renderer3D
{
public:
    virtual ~GLRenderer() override;
    virtual void Reset() override;

    virtual void SetRenderSettings(GPU::RenderSettings& settings) override;

    virtual void VCount144() override {};
    virtual void RenderFrame() override;
    virtual u32* GetLine(int line) override;

    void SetupAccelFrame();
    void PrepareCaptureFrame();

    static std::unique_ptr<GLRenderer> New() noexcept;
private:
    // Used by New()
    GLRenderer() noexcept;

    // GL version requirements
    // * texelFetch: 3.0 (GLSL 1.30)     (3.2/1.50 for MS)
    // * UBO: 3.1

    struct RendererPolygon
    {
        Polygon* PolyData;

        u32 NumIndices;
        u32 IndicesOffset;
        GLuint PrimType;

        u32 NumEdgeIndices;
        u32 EdgeIndicesOffset;

        u32 RenderKey;
    };

    RendererPolygon PolygonList[2048] {};

    bool BuildRenderShader(u32 flags, const char* vs, const char* fs);
    void UseRenderShader(u32 flags);
    void SetupPolygon(RendererPolygon* rp, Polygon* polygon);
    u32* SetupVertex(Polygon* poly, int vid, Vertex* vtx, u32 vtxattr, u32* vptr);
    void BuildPolygons(RendererPolygon* polygons, int npolys);
    int RenderSinglePolygon(int i);
    int RenderPolygonBatch(int i);
    int RenderPolygonEdgeBatch(int i);
    void RenderSceneChunk(int y, int h);

    enum
    {
        RenderFlag_WBuffer     = 0x01,
        RenderFlag_Trans       = 0x02,
        RenderFlag_ShadowMask  = 0x04,
        RenderFlag_Edge        = 0x08,
    };


    GLuint ClearShaderPlain[3] {};

    GLuint RenderShader[16][3] {};
    GLuint CurShaderID = -1;

    GLuint FinalPassEdgeShader[3] {};
    GLuint FinalPassFogShader[3] {};

    // std140 compliant structure
    struct
    {
        float uScreenSize[2];       // vec2       0 / 2
        u32 uDispCnt;               // int        2 / 1
        u32 __pad0;
        float uToonColors[32][4];   // vec4[32]   4 / 128
        float uEdgeColors[8][4];    // vec4[8]    132 / 32
        float uFogColor[4];         // vec4       164 / 4
        float uFogDensity[34][4];   // float[34]  168 / 136
        u32 uFogOffset;             // int        304 / 1
        u32 uFogShift;              // int        305 / 1
        u32 _pad1[2];               // int        306 / 2
    } ShaderConfig {};

    GLuint ShaderConfigUBO {};
    int NumFinalPolys {}, NumOpaqueFinalPolys {};

    GLuint ClearVertexBufferID = 0, ClearVertexArrayID {};
    GLint ClearUniformLoc[4] {};

    // vertex buffer
    // * XYZW: 4x16bit
    // * RGBA: 4x8bit
    // * ST: 2x16bit
    // * polygon data: 3x32bit (polygon/texture attributes)
    //
    // polygon attributes:
    // * bit4-7, 11, 14-15, 24-29: POLYGON_ATTR
    // * bit16-20: Z shift
    // * bit8: front-facing (?)
    // * bit9: W-buffering (?)

    GLuint VertexBufferID {};
    u32 VertexBuffer[10240 * 7] {};
    u32 NumVertices {};

    GLuint VertexArrayID {};
    GLuint IndexBufferID {};
    u16 IndexBuffer[2048 * 40] {};
    u32 NumIndices {}, NumEdgeIndices {};

    const u32 EdgeIndicesOffset = 2048 * 30;

    GLuint TexMemID {};
    GLuint TexPalMemID {};

    int ScaleFactor {};
    bool BetterPolygons {};
    int ScreenW {}, ScreenH {};

    GLuint FramebufferTex[8] {};
    int FrontBuffer {};
    GLuint FramebufferID[4] {}, PixelbufferID {};
    u32 Framebuffer[256*192] {};


};
}