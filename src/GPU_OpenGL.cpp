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

#include <string.h>
#include "NDS.h"
#include "GPU_OpenGL.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

#include "OpenGL_shaders/FinalPassVS.h"
#include "OpenGL_shaders/FinalPassFS.h"
#include "OpenGL_shaders/CaptureVS.h"
#include "OpenGL_shaders/CaptureFS.h"
#include "OpenGL_shaders/CaptureDownscaleVS.h"
#include "OpenGL_shaders/CaptureDownscaleFS.h"


GLRenderer::GLRenderer(melonDS::NDS& nds, bool compute)
    : Renderer(nds.GPU)
{
    AuxInputBuffer[0] = new u16[256 * 256];
    AuxInputBuffer[1] = new u16[256 * 192];

    Rend2D_A = std::make_unique<GLRenderer2D>(GPU.GPU2D_A, *this);
    Rend2D_B = std::make_unique<GLRenderer2D>(GPU.GPU2D_B, *this);

    // TODO, eventually: figure out a nicer way to support different 3D renderers?
    IsCompute = compute;
    if (IsCompute)
        Rend3D = std::make_unique<ComputeRenderer3D>(GPU.GPU3D, *this);
    else
        Rend3D = std::make_unique<GLRenderer3D>(GPU.GPU3D, *this);

    ScaleFactor = 0;
}

#define glTexParams(target, wrap) \
    glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap); \
    glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap); \
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST); \
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

bool GLRenderer::Init()
{
    assert(glEnable != nullptr);

    GLint uniloc;

    // compile shaders

    if (!OpenGL::CompileVertexFragmentProgram(FPShader,
                                              kFinalPassVS, kFinalPassFS,
                                              "2DFinalPassShader",
                                              {{"vPosition", 0}},
                                              {{"oTopColor", 0}, {"oBottomColor", 1}}))
        return false;

    if (!OpenGL::CompileVertexFragmentProgram(CaptureShader,
                                              kCaptureVS, kCaptureFS,
                                              "2DCaptureShader",
                                              {{"vPosition", 0}, {"vTexcoord", 1}},
                                              {{"oColor", 0}}))
        return false;

    if (!OpenGL::CompileVertexFragmentProgram(CapDownShader,
                                              kCaptureDownscaleVS, kCaptureDownscaleFS,
                                              "2DCaptureDownscaleShader",
                                              {{"vPosition", 0}},
                                              {{"oColor", 0}}))
        return false;

    // vertex buffers

    const float rectvertices[2*2*3] = {
            0, 1,   1, 0,   1, 1,
            0, 1,   0, 0,   1, 0
    };

    glGenBuffers(1, &RectVtxBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, RectVtxBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectvertices), rectvertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &RectVtxArray);
    glBindVertexArray(RectVtxArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    float vertices[12][2];
#define SETVERTEX(i, x, y) \
    vertices[i][0] = x; \
    vertices[i][1] = y;

    SETVERTEX(0, -1, 1);
    SETVERTEX(1, 1, -1);
    SETVERTEX(2, 1, 1);
    SETVERTEX(3, -1, 1);
    SETVERTEX(4, -1, -1);
    SETVERTEX(5, 1, -1);

#undef SETVERTEX

    // final pass vertex data: 2x position, 2x texcoord
    glGenBuffers(1, &FPVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, FPVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

    glGenVertexArrays(1, &FPVertexArrayID);
    glBindVertexArray(FPVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glGenFramebuffers(2, &FPOutputFB[0]);

    // capture vertex data: 2x position, 2x texcoord
    glGenBuffers(1, &CaptureVtxBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, CaptureVtxBuffer);
    glBufferData(GL_ARRAY_BUFFER, 2 * 6 * 4 * sizeof(u16), nullptr, GL_STREAM_DRAW);

    glGenVertexArrays(1, &CaptureVtxArray);
    glBindVertexArray(CaptureVtxArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribIPointer(0, 2, GL_SHORT, 4 * sizeof(u16), (void*)0);
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribIPointer(1, 2, GL_SHORT, 4 * sizeof(u16), (void*)(2 * sizeof(u16)));

    // textures / framebuffers

    glGenTextures(1, &AuxInputTex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, AuxInputTex);
    glTexParams(GL_TEXTURE_2D_ARRAY, GL_REPEAT);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB5_A1, 256, 256, 2, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);

    glGenTextures(1, &CaptureVRAMTex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureVRAMTex);
    glTexParams(GL_TEXTURE_2D_ARRAY, GL_REPEAT);
    glGenFramebuffers(1, &CaptureVRAMFB);

    glGenTextures(2, FPOutputTex);
    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, FPOutputTex[i]);
        glTexParams(GL_TEXTURE_2D_ARRAY, GL_CLAMP_TO_EDGE);
    }

    glGenTextures(1, &CaptureOutput256Tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);
    glTexParams(GL_TEXTURE_2D_ARRAY, GL_REPEAT);
    glGenFramebuffers(4, CaptureOutput256FB);

    glGenTextures(1, &CaptureOutput128Tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput128Tex);
    glTexParams(GL_TEXTURE_2D_ARRAY, GL_REPEAT);
    glGenFramebuffers(16, CaptureOutput128FB);

    glGenTextures(1, &CaptureSyncTex);
    glBindTexture(GL_TEXTURE_2D, CaptureSyncTex);
    glTexParams(GL_TEXTURE_2D, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);

    glGenFramebuffers(1, &CaptureSyncFB);
    glBindFramebuffer(GL_FRAMEBUFFER, CaptureSyncFB);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CaptureSyncTex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // UBOs

    glGenBuffers(1, &FPConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, FPConfigUBO);
    static_assert((sizeof(sFinalPassConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sFinalPassConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 30, FPConfigUBO);

    glGenBuffers(1, &CaptureConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, CaptureConfigUBO);
    static_assert((sizeof(sCaptureConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sCaptureConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 31, CaptureConfigUBO);

    // shader config

    glUseProgram(FPShader);

    uniloc = glGetUniformLocation(FPShader, "MainInputTexA");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(FPShader, "MainInputTexB");
    glUniform1i(uniloc, 1);
    uniloc = glGetUniformLocation(FPShader, "AuxInputTex");
    glUniform1i(uniloc, 2);

    uniloc = glGetUniformBlockIndex(FPShader, "ubFinalPassConfig");
    glUniformBlockBinding(FPShader, uniloc, 30);


    glUseProgram(CaptureShader);

    uniloc = glGetUniformLocation(CaptureShader, "InputTexA");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(CaptureShader, "InputTexB");
    glUniform1i(uniloc, 1);

    uniloc = glGetUniformBlockIndex(CaptureShader, "ubCaptureConfig");
    glUniformBlockBinding(CaptureShader, uniloc, 31);


    glUseProgram(CapDownShader);

    uniloc = glGetUniformLocation(CapDownShader, "InputTex");
    glUniform1i(uniloc, 0);

    CapDownInputLayerULoc = glGetUniformLocation(CapDownShader, "uInputLayer");


    if (!Rend2D_A->Init()) return false;
    if (!Rend2D_B->Init()) return false;
    if (!Rend3D->Init()) return false;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

GLRenderer::~GLRenderer()
{
    glDeleteProgram(FPShader);
    glDeleteProgram(CaptureShader);
    glDeleteProgram(CapDownShader);

    glDeleteBuffers(1, &RectVtxBuffer);
    glDeleteVertexArrays(1, &RectVtxArray);

    glDeleteBuffers(1, &FPVertexBufferID);
    glDeleteVertexArrays(1, &FPVertexArrayID);

    glDeleteBuffers(1, &CaptureVtxBuffer);
    glDeleteVertexArrays(1, &CaptureVtxArray);

    glDeleteFramebuffers(2, FPOutputFB);
    glDeleteTextures(1, &AuxInputTex);
    glDeleteTextures(1, &CaptureVRAMTex);
    glDeleteTextures(2, FPOutputTex);

    delete[] AuxInputBuffer[0];
    delete[] AuxInputBuffer[1];

    glDeleteTextures(1, &CaptureOutput256Tex);
    glDeleteTextures(1, &CaptureOutput128Tex);
    glDeleteTextures(1, &CaptureSyncTex);
    glDeleteFramebuffers(1, &CaptureSyncFB);

    glDeleteBuffers(1, &FPConfigUBO);
    glDeleteBuffers(1, &CaptureConfigUBO);
}

void GLRenderer::Reset()
{
    memset(&FinalPassConfig, 0, sizeof(FinalPassConfig));
    memset(&CaptureConfig, 0, sizeof(CaptureConfig));

    AuxUsageMask = 0;

    DispCntA = 0;
    DispCntB = 0;
    MasterBrightnessA = 0;
    MasterBrightnessB = 0;
    CaptureCnt = 0;

    NeedPartialRender = false;
    LastLine = 0;
    LastCapLine = 0;
    Aux0VRAMCap = -1;

    Rend2D_A->Reset();
    Rend2D_B->Reset();
    Rend3D->Reset();
}

void GLRenderer::Stop()
{
    // TODO clear buffers
    // TODO: do we even need this anymore?
}

void GLRenderer::PostSavestate()
{
    Reset();

    auto rend2D = dynamic_cast<GLRenderer2D*>(Rend2D_A.get());
    rend2D->PostSavestate();
    rend2D = dynamic_cast<GLRenderer2D*>(Rend2D_B.get());
    rend2D->PostSavestate();
}


void GLRenderer::SetRenderSettings(RendererSettings& settings)
{
    SetScaleFactor(settings.ScaleFactor);

    auto rend2d = dynamic_cast<GLRenderer2D*>(Rend2D_A.get());
    rend2d->SetScaleFactor(settings.ScaleFactor);

    rend2d = dynamic_cast<GLRenderer2D*>(Rend2D_B.get());
    rend2d->SetScaleFactor(settings.ScaleFactor);

    if (IsCompute)
    {
        auto rend3d = dynamic_cast<ComputeRenderer3D *>(Rend3D.get());
        rend3d->SetRenderSettings(settings.ScaleFactor, settings.HiresCoordinates);
    }
    else
    {
        auto rend3d = dynamic_cast<GLRenderer3D *>(Rend3D.get());
        rend3d->SetRenderSettings(settings.ScaleFactor, settings.BetterPolygons);
    }
}


void GLRenderer::SetScaleFactor(int scale)
{
    if (scale == ScaleFactor)
        return;

    ScaleFactor = scale;
    ScreenW = 256 * scale;
    ScreenH = 192 * scale;

    const GLenum fbassign2[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 256*ScaleFactor, 256*ScaleFactor, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    for (int i = 0; i < 4; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, CaptureOutput256FB[i]);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CaptureOutput256Tex, 0, i);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput128Tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 128*ScaleFactor, 128*ScaleFactor, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    for (int i = 0; i < 16; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, CaptureOutput128FB[i]);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CaptureOutput128Tex, 0, i);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureVRAMTex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 256*ScaleFactor, 256*ScaleFactor, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, CaptureVRAMFB);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CaptureVRAMTex, 0, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, FPOutputTex[i]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, ScreenW, ScreenH, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, FPOutputFB[i]);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FPOutputTex[i], 0, 0);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, FPOutputTex[i], 0, 1);
        glDrawBuffers(2, fbassign2);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void GLRenderer::DrawScanline(u32 line)
{
    u32 dispcnt_a_diff = DispCntA ^ GPU.GPU2D_A.DispCnt;
    u32 dispcnt_b_diff = DispCntB ^ GPU.GPU2D_B.DispCnt;
    u32 capturecnt_diff = CaptureCnt ^ GPU.CaptureCnt;

    bool need_render = false;
    bool need_capture = false;

    if (dispcnt_a_diff & 0xF0000)
        need_render = true;
    else if (dispcnt_b_diff & 0x10000)
        need_render = true;
    else if (MasterBrightnessA != GPU.MasterBrightnessA ||
             MasterBrightnessB != GPU.MasterBrightnessB)
        need_render = true;

    if (GPU.CaptureEnable && (capturecnt_diff & 0x7FFFFFFF))
    {
        need_render = true;
        need_capture = true;
    }

    NeedPartialRender = need_render;
    Rend2D_A->DrawScanline(line);
    Rend2D_B->DrawScanline(line);

    if (need_render && (line > 0))
    {
        RenderScreen(LastLine, line);
        LastLine = line;
    }

    if (need_capture && (line > 0))
    {
        DoCapture(LastCapLine, line);
        LastCapLine = line;
    }

    DispCntA = GPU.GPU2D_A.DispCnt;
    DispCntB = GPU.GPU2D_B.DispCnt;
    MasterBrightnessA = GPU.MasterBrightnessA;
    MasterBrightnessB = GPU.MasterBrightnessB;
    CaptureCnt = GPU.CaptureCnt;

    FinalPassConfig.uScreenSwap[line] = GPU.ScreenSwap;

    u32 dispcnt = GPU.GPU2D_A.DispCnt;
    u32 dispmode = (dispcnt >> 16) & 0x3;
    u32 capcnt = GPU.CaptureCnt;
    u32 capsel = (capcnt >> 29) & 0x3;
    u32 capA = (capcnt >> 24) & 0x1;
    u32 capB = (capcnt >> 25) & 0x1;
    bool checkcap = GPU.CaptureEnable && (capsel != 0);

    if (GPU.CaptureEnable && (capsel != 1))
    {
        if (capA == 0)
            CaptureConfig.uSrcAOffset[line] = 0;
        else
        {
            int xpos = GPU.GPU3D.GetRenderXPos() & 0x1FF;
            xpos -= ((xpos & 0x100) << 1);
            CaptureConfig.uSrcAOffset[line] = (float)xpos / 256.f;
        }
    }

    if ((dispmode == 2) || (checkcap && (capB == 0)))
    {
        AuxUsageMask |= (1<<0);

        u32 vrambank = (dispcnt >> 18) & 0x3;
        u32 vramoffset = line * 256;
        u32 outoffset = line * 256;
        if (dispmode != 2)
        {
            u32 yoff = ((capcnt >> 26) & 0x3) << 14;
            vramoffset += yoff;
            outoffset += yoff;
        }

        vramoffset &= 0xFFFF;
        outoffset &= 0xFFFF;

        u16* adst = &AuxInputBuffer[0][outoffset];

        if (GPU.VRAMMap_LCDC & (1<<vrambank))
        {
            u16* vram = (u16*)GPU.VRAM[vrambank];

            for (int i = 0; i < 256; i++)
            {
                adst[i] = vram[vramoffset];
                vramoffset++;
            }
        }
        else
        {
            for (int i = 0; i < 256; i++)
            {
                adst[i] = 0;
            }
        }
    }

    if ((dispmode == 3) || (checkcap && (capB == 1)))
    {
        AuxUsageMask |= (1<<1);

        u16* adst = &AuxInputBuffer[1][line * 256];
        for (int i = 0; i < 256; i++)
        {
            adst[i] = GPU.DispFIFOBuffer[i];
        }
    }
}

void GLRenderer::DrawSprites(u32 line)
{
    Rend2D_A->DrawSprites(line);
    Rend2D_B->DrawSprites(line);
}


void GLRenderer::RenderScreen(int ystart, int yend)
{
    int backbuf = BackBuffer;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FPOutputFB[backbuf]);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);

    glViewport(0, 0, ScreenW, ScreenH);

    // TODO: adjust incoming vertices instead of doing this?
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, ystart * ScaleFactor, ScreenW, (yend-ystart) * ScaleFactor);

    int vramcap = -1;
    if (AuxUsageMask & (1<<0))
    {
        u32 vrambank = (DispCntA >> 18) & 0x3;
        if (GPU.VRAMMap_LCDC & (1<<vrambank))
            vramcap = GPU.GetCaptureBlock_LCDC(vrambank << 17);
    }
    Aux0VRAMCap = vramcap;

    if (!GPU.ScreensEnabled)
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else
    {
        glUseProgram(FPShader);

        FinalPassConfig.uScaleFactor = ScaleFactor;
        FinalPassConfig.uDispModeA = (DispCntA >> 16) & 0x3;
        FinalPassConfig.uDispModeB = (DispCntB >> 16) & 0x1;
        FinalPassConfig.uBrightModeA = (MasterBrightnessA >> 14) & 0x3;
        FinalPassConfig.uBrightModeB = (MasterBrightnessB >> 14) & 0x3;
        FinalPassConfig.uBrightFactorA = std::min(MasterBrightnessA & 0x1F, 16);
        FinalPassConfig.uBrightFactorB = std::min(MasterBrightnessB & 0x1F, 16);

        if (AuxUsageMask)
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY, AuxInputTex);
            if ((AuxUsageMask & (1<<0)) && (vramcap == -1))
            {
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 256, 256, 1, GL_RGBA,
                                GL_UNSIGNED_SHORT_1_5_5_5_REV, AuxInputBuffer[0]);
            }
            if (AuxUsageMask & (1<<1))
            {
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 256, 192, 1, GL_RGBA,
                                GL_UNSIGNED_SHORT_1_5_5_5_REV, AuxInputBuffer[1]);
            }
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, OutputTex2D[0]);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, OutputTex2D[1]);

        glActiveTexture(GL_TEXTURE2);
        u32 modeA = (DispCntA >> 16) & 0x3;
        if ((modeA == 2) && (vramcap != -1))
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);
            FinalPassConfig.uAuxLayer = vramcap >> 2;
            FinalPassConfig.uAuxColorFactor = 63.75f;
        }
        else if (modeA >= 2)
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY, AuxInputTex);
            FinalPassConfig.uAuxLayer = (modeA - 2);
            FinalPassConfig.uAuxColorFactor = 62.f;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, FPConfigUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FinalPassConfig), &FinalPassConfig);

        glBindBuffer(GL_ARRAY_BUFFER, FPVertexBufferID);
        glBindVertexArray(FPVertexArrayID);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);
    }

    glDisable(GL_SCISSOR_TEST);
}

void GLRenderer::VBlank()
{
    Rend2D_A->VBlank();
    Rend2D_B->VBlank();

    RenderScreen(LastLine, 192);

    if (GPU.CaptureEnable)
        DoCapture(LastCapLine, 192);

    LastLine = 0;
    LastCapLine = 0;
}

void GLRenderer::VBlankEnd()
{
    AuxUsageMask = 0;
}


void GLRenderer::DoCapture(int ystart, int yend)
{
    u32 dispcnt = DispCntA;
    u32 capcnt = CaptureCnt;
    u32 dispmode = (dispcnt >> 16) & 0x3;
    u32 srcA = (capcnt >> 24) & 0x1;
    u32 srcB = (capcnt >> 25) & 0x1;
    u32 srcBblock = (dispcnt >> 18) & 0x3;
    u32 srcBoffset = (dispmode == 2) ? 0 : ((capcnt >> 26) & 0x3);
    u32 dstblock = (capcnt >> 16) & 0x3;
    u32 dstoffset = (capcnt >> 18) & 0x3;
    u32 capsize = (capcnt >> 20) & 0x3;
    u32 dstmode = (capcnt >> 29) & 0x3;
    u32 eva = std::min(capcnt & 0x1F, 16u);
    u32 evb = std::min((capcnt >> 8) & 0x1F, 16u);

    // determine the region we're going to capture to

    int dstwidth, dstheight;

    if (capsize == 0)
    {
        dstwidth = 128;
        dstheight = 128;
    }
    else
    {
        dstwidth = 256;
        dstheight = 64 * capsize;
    }

    if (ystart >= dstheight)
        return;
    if (yend > dstheight)
        yend = dstheight;

    glUseProgram(CaptureShader);

    GLuint inputA;
    if (srcA)
        inputA = OutputTex3D;
    else
        inputA = OutputTex2D[0];

    bool useSrcB = (dstmode == 1) || (dstmode == 2 && evb > 0);

    GLuint inputB = AuxInputTex;
    u32 layerB = srcB;
    CaptureConfig.uSrcBColorFactor = 248.f;

    if (useSrcB && (Aux0VRAMCap != -1))
    {
        // hi-res VRAM
        if (dstblock == srcBblock)
        {
            // we are reading from the same block we are capturing to
            // on hardware, it would read the old VRAM contents, then write new stuff
            // but we can't do that with OpenGL
            // so we need to blit it to a temporary framebuffer

            int blitY0 = (srcBoffset * 64) + ystart;
            int blitY1 = (srcBoffset * 64) + yend;

            if (dstoffset != srcBoffset)
                Log(LogLevel::Error, "GPU_OpenGL: MISMATCHED VRAM OFFSETS ON SAME BANK!!! bank=%d src=%d dst=%d\n",
                       dstblock, srcBoffset, dstoffset);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureOutput256FB[srcBblock]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureVRAMFB);

            if (blitY1 > 256)
            {
                // wraparound
                glBlitFramebuffer(0, blitY0*ScaleFactor, 256*ScaleFactor, 256*ScaleFactor,
                                  0, blitY0*ScaleFactor, 256*ScaleFactor, 256*ScaleFactor,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBlitFramebuffer(0, 0, 256*ScaleFactor, (blitY1-256)*ScaleFactor,
                                  0, 0, 256*ScaleFactor, (blitY1-256)*ScaleFactor,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
            else
            {
                // straightforward
                glBlitFramebuffer(0, blitY0*ScaleFactor, 256*ScaleFactor, blitY1*ScaleFactor,
                                  0, blitY0*ScaleFactor, 256*ScaleFactor, blitY1*ScaleFactor,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

            inputB = CaptureVRAMTex;
            layerB = 0;
        }
        else
        {
            // if it's a different bank, we can just use it as-is
            inputB = CaptureOutput256Tex;
            layerB = srcBblock;
        }

        CaptureConfig.uSrcBColorFactor = 255.f;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if (capsize == 0)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureOutput128FB[(dstblock << 2) | dstoffset]);
        glViewport(0, 0, 128*ScaleFactor, 128*ScaleFactor);
    }
    else
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureOutput256FB[dstblock]);
        glViewport(0, 0, 256*ScaleFactor, 256*ScaleFactor);
    }

    CaptureConfig.uInvCaptureSize[0] = 1.f / (float)dstwidth;
    CaptureConfig.uInvCaptureSize[1] = 1.f / (float)dstheight;

    CaptureConfig.uSrcALayer = srcA;

    if (srcB == 0)
        CaptureConfig.uSrcBOffset = 64 * srcBoffset;
    else
        CaptureConfig.uSrcBOffset = 0;

    CaptureConfig.uSrcBLayer = layerB;

    CaptureConfig.uDstMode = dstmode;
    CaptureConfig.uBlendFactors[0] = eva;
    CaptureConfig.uBlendFactors[1] = evb;

    glBindBuffer(GL_UNIFORM_BUFFER, CaptureConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CaptureConfig), &CaptureConfig);

    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, inputB);

    u16 vtxbuf[12 * 4];
    u16* vptr = vtxbuf;
    int numvtx;

    // y0/y1 = coordinates in destination buffer
    // t0/t1 = coordinates in source buffers
    if (capsize == 0) dstoffset = 0;
    int y0 = (dstoffset * 64) + ystart;
    int y1 = (dstoffset * 64) + yend;
    int t0 = ystart;
    int t1 = yend;

    int bufferheight = (capsize == 0) ? 128 : 256;
    if (y1 > bufferheight)
    {
        // wraparound
        int y2 = bufferheight;
        int t2 = t0 + (y2 - y0);
        *vptr++ = 0;        *vptr++ = y2; *vptr++ = 0;         *vptr++ = t2;
        *vptr++ = dstwidth; *vptr++ = y0; *vptr++ = dstwidth;  *vptr++ = t0;
        *vptr++ = dstwidth; *vptr++ = y2; *vptr++ = dstwidth;  *vptr++ = t2;
        *vptr++ = 0;        *vptr++ = y2; *vptr++ = 0;         *vptr++ = t2;
        *vptr++ = 0;        *vptr++ = y0; *vptr++ = 0;         *vptr++ = t0;
        *vptr++ = dstwidth; *vptr++ = y0; *vptr++ = dstwidth;  *vptr++ = t0;

        y2 = y1 - bufferheight;
        *vptr++ = 0;        *vptr++ = y2; *vptr++ = 0;         *vptr++ = t1;
        *vptr++ = dstwidth; *vptr++ = 0;  *vptr++ = dstwidth;  *vptr++ = t2;
        *vptr++ = dstwidth; *vptr++ = y2; *vptr++ = dstwidth;  *vptr++ = t1;
        *vptr++ = 0;        *vptr++ = y2; *vptr++ = 0;         *vptr++ = t1;
        *vptr++ = 0;        *vptr++ = 0;  *vptr++ = 0;         *vptr++ = t2;
        *vptr++ = dstwidth; *vptr++ = 0;  *vptr++ = dstwidth;  *vptr++ = t2;

        numvtx = 12;
    }
    else
    {
        // straightforward
        *vptr++ = 0;        *vptr++ = y1; *vptr++ = 0;         *vptr++ = t1;
        *vptr++ = dstwidth; *vptr++ = y0; *vptr++ = dstwidth;  *vptr++ = t0;
        *vptr++ = dstwidth; *vptr++ = y1; *vptr++ = dstwidth;  *vptr++ = t1;
        *vptr++ = 0;        *vptr++ = y1; *vptr++ = 0;         *vptr++ = t1;
        *vptr++ = 0;        *vptr++ = y0; *vptr++ = 0;         *vptr++ = t0;
        *vptr++ = dstwidth; *vptr++ = y0; *vptr++ = dstwidth;  *vptr++ = t0;

        numvtx = 6;
    }

    glBindBuffer(GL_ARRAY_BUFFER, CaptureVtxBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, numvtx * 4 * sizeof(u16), vtxbuf);

    glBindVertexArray(CaptureVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, numvtx);
}


void GLRenderer::AllocCapture(u32 bank, u32 start, u32 len)
{
    auto rend2D = dynamic_cast<GLRenderer2D*>(Rend2D_A.get());
    rend2D->LayerConfigDirty = true;
    rend2D->SpriteConfigDirty = true;
    rend2D = dynamic_cast<GLRenderer2D*>(Rend2D_B.get());
    rend2D->LayerConfigDirty = true;
    rend2D->SpriteConfigDirty = true;
}

void GLRenderer::DownscaleCapture(int width, int height, int layer)
{
    // downscale a hi-res capture buffer to 1x IR, and convert to RGBA5551
    // we need to do this with a shader so we can accurately downscale color components

    glUseProgram(CapDownShader);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureSyncFB);

    glViewport(0, 0, width, height);

    glActiveTexture(GL_TEXTURE0);
    if (width == 128)
        glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput128Tex);
    else
        glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);
    glUniform1i(CapDownInputLayerULoc, layer);

    glBindBuffer(GL_ARRAY_BUFFER, RectVtxBuffer);
    glBindVertexArray(RectVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);
}

void GLRenderer::SyncVRAMCapture(u32 bank, u32 start, u32 len, bool complete)
{
    if (!complete)
        Log(LogLevel::Error, "GPU_OpenGL: !!! READING VRAM AS IT IS BEING CAPTURED TO\n");

    u8* vram = GPU.VRAM[bank];

    glDisable(GL_DITHER);

    if (len == 0) // 128x128
    {
        DownscaleCapture(128, 128, (bank<<2) | start);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureSyncFB);

        glReadPixels(0, 0, 128, 128,
                     GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, &vram[start * 64 * 512]);

        for (u32 j = start * 64; j < (start+1) * 64; j++)
            GPU.VRAMDirty[bank][j] = true;
    }
    else
    {
        DownscaleCapture(256, 256, bank);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureSyncFB);

        u32 pos = start;
        for (u32 i = 0; i < len;)
        {
            u32 end = pos + len;
            if (end > 4)
                end = 4;

            glReadPixels(0, pos * 64, 256, (end - pos) * 64,
                         GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, &vram[pos * 64 * 512]);

            for (u32 j = pos * 64; j < end * 64; j++)
                GPU.VRAMDirty[bank][j] = true;

            i += (end - pos);
            pos += (end - pos);
            pos &= 3;
        }
    }
}


bool GLRenderer::GetFramebuffers(void** top, void** bottom)
{
    // since we use an array texture, we only need one of the pointer fields
    int frontbuf = BackBuffer ^ 1;
    *top = &FPOutputTex[frontbuf];
    *bottom = nullptr;
    return false;
}


bool GLRenderer::NeedsShaderCompile()
{
    return Rend3D->NeedsShaderCompile();
}

void GLRenderer::ShaderCompileStep(int& current, int& count)
{
    return Rend3D->ShaderCompileStep(current, count);
}

}
