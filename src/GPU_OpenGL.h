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

#ifndef GPU_OPENGL_H
#define GPU_OPENGL_H

#include "OpenGLSupport.h"
#include "GPU.h"
#include "GPU2D_OpenGL.h"
#include "GPU3D_OpenGL.h"
#include "GPU3D_Compute.h"

namespace melonDS
{

class GLRenderer : public Renderer
{
public:
    GLRenderer(melonDS::NDS& nds, bool compute);
    ~GLRenderer() override = default;
    bool Init() override;
    void Reset() override;
    void Stop() override;

    void PostSavestate() override;

    void SetRenderSettings(RendererSettings& settings) override;

    void DrawScanline(u32 line) override;
    void DrawSprites(u32 line) override;

    void VBlank() override;
    void VBlankEnd() override;

    void AllocCapture(u32 bank, u32 start, u32 len) override;
    void SyncVRAMCapture(u32 bank, u32 start, u32 len, bool complete) override;

    bool GetFramebuffers(void** top, void** bottom) override;

    bool NeedsShaderCompile() override;
    void ShaderCompileStep(int& current, int& count) override;

private:
    friend class GLRenderer2D;
    friend class GLRenderer3D;
    friend class ComputeRenderer3D;

    bool IsCompute;

    int ScaleFactor;
    int ScreenW, ScreenH;

    OpenGL::BufferHandle RectVtxBuffer {};
    OpenGL::VertexArrayHandle RectVtxArray {};

    // Non-owning reference to the texture that'll be used for 3D output
    // (only one of the DS's screens can use the 3D GPU at once)
    GLuint OutputTex3D {};
    OpenGL::TextureHandles<2> OutputTex2D {};

    struct sFinalPassConfig
    {
        u32 uScreenSwap[192];
        u32 uScaleFactor;
        u32 uAuxLayer;
        u32 uDispModeA;
        u32 uDispModeB;
        u32 uBrightModeA;
        u32 uBrightModeB;
        u32 uBrightFactorA;
        u32 uBrightFactorB;
        float uAuxColorFactor;
        u32 __pad0[3];
    } FinalPassConfig;

    // Owned handles to the shaders/textures that both 2D renderers use.
    GLRenderer2D::SharedResources Renderer2DResources;

    OpenGL::ProgramHandle FPShader {};
    OpenGL::BufferHandle FPConfigUBO {};

    OpenGL::BufferHandle FPVertexBufferID {};
    OpenGL::VertexArrayHandle FPVertexArrayID {};

    OpenGL::TextureHandle AuxInputTex {};                 // aux input (VRAM and mainmem FIFO)

    // texture/fb for display capture VRAM input
    OpenGL::TextureHandle CaptureVRAMTex {};
    OpenGL::FramebufferHandle CaptureVRAMFB {};

    OpenGL::TextureHandles<2> FPOutputTex {};               // final output
    OpenGL::FramebufferHandles<2> FPOutputFB {};

    struct sCaptureConfig
    {
        float uInvCaptureSize[2];
        u32 uSrcALayer;
        u32 uSrcBLayer;
        u32 uSrcBOffset;
        u32 uDstMode;
        u32 uBlendFactors[2];
        float uSrcAOffset[192];
        float uSrcBColorFactor;
        u32 __pad0[3];
    } CaptureConfig;

    OpenGL::ProgramHandle CaptureShader {};
    OpenGL::BufferHandle CaptureConfigUBO {};

    OpenGL::BufferHandle CaptureVtxBuffer {};
    OpenGL::VertexArrayHandle CaptureVtxArray {};

    OpenGL::FramebufferHandles<4> CaptureOutput256FB {};
    OpenGL::TextureHandle CaptureOutput256Tex {};
    OpenGL::FramebufferHandles<16> CaptureOutput128FB {};
    OpenGL::TextureHandle CaptureOutput128Tex {};

    OpenGL::ProgramHandle CapDownShader {};
    GLint CapDownInputLayerULoc;

    OpenGL::FramebufferHandle CaptureSyncFB {};
    OpenGL::TextureHandle CaptureSyncTex {};

    std::array<u16, 256 * 256> AuxInputBufferA {};
    std::array<u16, 256 * 192> AuxInputBufferB {};
    u8 AuxUsageMask;

    u32 DispCntA, DispCntB;
    u16 MasterBrightnessA, MasterBrightnessB;
    u32 CaptureCnt;

    bool NeedPartialRender;
    int LastLine;
    int LastCapLine;
    int Aux0VRAMCap;

    void SetScaleFactor(int scale);

    void RenderScreen(int ystart, int yend);
    void DoCapture(int ystart, int yend);
    void DownscaleCapture(int width, int height, int layer);
};

}

#endif // GPU_OPENGL_H
