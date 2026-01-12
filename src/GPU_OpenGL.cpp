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

#include "GPU_OpenGL.h"

namespace melonDS
{

GLRenderer::GLRenderer(melonDS::GPU& gpu, bool compute)
    : Renderer(gpu)
{
    // TODO init GL shit here

    Rend2D_A = std::make_unique<GPU2D::GLRenderer2D>(GPU.GPU2D_A, *this);
    Rend2D_B = std::make_unique<GPU2D::GLRenderer2D>(GPU.GPU2D_B, *this);

    IsCompute = compute;
    if (IsCompute)
        Rend3D = std::make_unique<ComputeRenderer>();
    else
        Rend3D = std::make_unique<GLRenderer3D>(GPU.GPU3D, *this);
}

GLRenderer::~GLRenderer()
{
    //
}

bool GLRenderer::Init()
{
    // TODO
    // init OutputTex2D and the other shit

    if (!Rend2D_A->Init()) return false;
    if (!Rend2D_B->Init()) return false;
    if (!Rend3D->Init()) return false;
    return true;
}

void GLRenderer::Reset()
{
    //
}

void GLRenderer::Stop()
{
    //
}


void GLRenderer::SetScaleFactor(int scale)
{
    // todo
}


void GLRenderer::DrawScanline(u32 line)
{
    //
}

void GLRenderer::DrawSprites(u32 line)
{
    //
}


void GLRenderer::VBlank()
{
    // TODO!!! do this more nicely!!!
    GLuint fart;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&fart);
    _3DLayerTex = fart;

    DoRenderSprites(unitA, 192);
    RenderScreen(unitA, UnitState[0].LastLine, 192);

    DoRenderSprites(unitB, 192);
    RenderScreen(unitB, UnitState[1].LastLine, 192);

    UnitState[0].LastSpriteLine = 0;
    UnitState[0].LastLine = 0;
    UnitState[1].LastSpriteLine = 0;
    UnitState[1].LastLine = 0;


    int backbuf = BackBuffer;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FPOutputFB[backbuf]);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(0, 0, ScreenW, ScreenH);

    glUseProgram(FPShaderID);

    FinalPassConfig.uScaleFactor = ScaleFactor;
    FinalPassConfig.uDispModeA = (GPU.GPU2D_A.DispCnt >> 16) & 0x3;
    FinalPassConfig.uDispModeB = (GPU.GPU2D_B.DispCnt >> 16) & 0x1;
    FinalPassConfig.uBrightModeA = (GPU.MasterBrightnessA >> 14) & 0x3;
    FinalPassConfig.uBrightModeB = (GPU.MasterBrightnessB >> 14) & 0x3;
    FinalPassConfig.uBrightFactorA = std::min(GPU.MasterBrightnessA & 0x1F, 16);
    FinalPassConfig.uBrightFactorB = std::min(GPU.MasterBrightnessB & 0x1F, 16);

    int vramcap = -1;
    if (AuxUsageMask & (1<<0))
    {
        u32 vrambank = (GPU.GPU2D_A.DispCnt >> 18) & 0x3;
        if (GPU.VRAMMap_LCDC & (1<<vrambank))
            vramcap = GPU.GetCaptureBlock_LCDC(vrambank << 17);
    }

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
    u32 modeA = (GPU.GPU2D_A.DispCnt >> 16) & 0x3;
    if ((modeA == 2) && (vramcap != -1))
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);
        FinalPassConfig.uAuxLayer = vramcap >> 2;
    }
    else if (modeA >= 2)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, AuxInputTex);
        FinalPassConfig.uAuxLayer = (modeA - 2);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, FPConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FinalPassConfig), &FinalPassConfig);

    glBindBuffer(GL_ARRAY_BUFFER, FPVertexBufferID);
    glBindVertexArray(FPVertexArrayID);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);

    if (GPU.CaptureEnable)
        DoCapture(vramcap);
}

void GLRenderer::VBlankEnd()
{
    //
}


void GLRenderer::DoCapture(int vramcap)
{
    glUseProgram(CaptureShader);

    u32 dispcnt = GPU.GPU2D_A.DispCnt;
    u32 capcnt = GPU.CaptureCnt;
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

    GLuint inputA;
    if (srcA)
    {
        inputA = _3DLayerTex;
        int xpos = GPU.GPU3D.GetRenderXPos() & 0x1FF;
        CaptureConfig.uSrcAOffset = xpos - ((xpos & 0x100) << 1);
    }
    else
    {
        inputA = UnitState[unit->Num].OutputTex;
        CaptureConfig.uSrcAOffset = 0;
    }

    bool useSrcB = (dstmode == 1) || (dstmode == 2 && evb > 0);

    GLuint inputB = AuxInputTex;
    u32 layerB = srcB;

    if (useSrcB && (vramcap != -1))
    {
        // hi-res VRAM
        if (dstblock == srcBblock)
        {
            // we are reading from the same block we are capturing to
            // on hardware, it would read the old VRAM contents, then write new stuff
            // but we can't do that with OpenGL
            // so we need to blit it to a temporary framebuffer

            if (dstoffset != srcBoffset)
                printf("GPU2D_OpenGL: MISMATCHED VRAM OFFSETS ON SAME BANK!!! bank=%d src=%d dst=%d\n",
                       dstblock, srcBoffset, dstoffset);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureOutput256FB[srcBblock]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureVRAMFB);
            glBlitFramebuffer(0, 0, 256 * ScaleFactor, 256 * ScaleFactor,
                              0, 0, 256 * ScaleFactor, 256 * ScaleFactor,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);

            inputB = CaptureVRAMTex;
            layerB = 0;
        }
        else
        {
            // if it's a different bank, we can just use it as-is
            inputB = CaptureOutput256Tex;
            layerB = srcBblock;
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if (capsize == 0)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureOutput128FB[(dstblock << 2) | dstoffset]);
        glViewport(0, 0, 128*ScaleFactor, 128*ScaleFactor);

        CaptureConfig.uCaptureSize[0] = 128;
        CaptureConfig.uCaptureSize[1] = 128;
    }
    else
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureOutput256FB[dstblock]);
        glViewport(0, 0, 256*ScaleFactor, 256*ScaleFactor);

        CaptureConfig.uCaptureSize[0] = 256;
        CaptureConfig.uCaptureSize[1] = 64 * capsize;
    }

    CaptureConfig.uScaleFactor = ScaleFactor;

    if (srcB == 0)
        CaptureConfig.uSrcBOffset = 64 * srcBoffset;
    else
        CaptureConfig.uSrcBOffset = 0;

    CaptureConfig.uSrcBLayer = layerB;

    CaptureConfig.uDstOffset = 64 * dstoffset;
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

    if (capsize == 0)
    {
        // 128x128
        *vptr++ = 0;   *vptr++ = 128; *vptr++ = 0;   *vptr++ = 128;
        *vptr++ = 128; *vptr++ = 0;   *vptr++ = 128; *vptr++ = 0;
        *vptr++ = 128; *vptr++ = 128; *vptr++ = 128; *vptr++ = 128;
        *vptr++ = 0;   *vptr++ = 128; *vptr++ = 0;   *vptr++ = 128;
        *vptr++ = 0;   *vptr++ = 0;   *vptr++ = 0;   *vptr++ = 0;
        *vptr++ = 128; *vptr++ = 0;   *vptr++ = 128; *vptr++ = 0;

        numvtx = 6;
    }
    else if ((dstoffset + capsize) > 4)
    {
        // 256xN, wraparound
        u16 y0, y1, t0, t1;
        u32 h0 = 4 - dstoffset;

        y0 = dstoffset * 64;
        y1 = 256;
        t0 = 0;
        t1 = h0 * 64;
        *vptr++ = 0;   *vptr++ = y1; *vptr++ = 0;   *vptr++ = t1;
        *vptr++ = 256; *vptr++ = y0; *vptr++ = 256; *vptr++ = t0;
        *vptr++ = 256; *vptr++ = y1; *vptr++ = 256; *vptr++ = t1;
        *vptr++ = 0;   *vptr++ = y1; *vptr++ = 0;   *vptr++ = t1;
        *vptr++ = 0;   *vptr++ = y0; *vptr++ = 0;   *vptr++ = t0;
        *vptr++ = 256; *vptr++ = y0; *vptr++ = 256; *vptr++ = t0;

        y0 = 0;
        y1 = (capsize - h0) * 64;
        t0 = h0 * 64;
        t1 = capsize * 64;
        *vptr++ = 0;   *vptr++ = y1; *vptr++ = 0;   *vptr++ = t1;
        *vptr++ = 256; *vptr++ = y0; *vptr++ = 256; *vptr++ = t0;
        *vptr++ = 256; *vptr++ = y1; *vptr++ = 256; *vptr++ = t1;
        *vptr++ = 0;   *vptr++ = y1; *vptr++ = 0;   *vptr++ = t1;
        *vptr++ = 0;   *vptr++ = y0; *vptr++ = 0;   *vptr++ = t0;
        *vptr++ = 256; *vptr++ = y0; *vptr++ = 256; *vptr++ = t0;

        numvtx = 12;
    }
    else
    {
        // 256xN, no wraparound
        u16 y0, y1, t0, t1;

        y0 = dstoffset * 64;
        y1 = (dstoffset + capsize) * 64;
        t0 = 0;
        t1 = capsize * 64;
        *vptr++ = 0;   *vptr++ = y1; *vptr++ = 0;   *vptr++ = t1;
        *vptr++ = 256; *vptr++ = y0; *vptr++ = 256; *vptr++ = t0;
        *vptr++ = 256; *vptr++ = y1; *vptr++ = 256; *vptr++ = t1;
        *vptr++ = 0;   *vptr++ = y1; *vptr++ = 0;   *vptr++ = t1;
        *vptr++ = 0;   *vptr++ = y0; *vptr++ = 0;   *vptr++ = t0;
        *vptr++ = 256; *vptr++ = y0; *vptr++ = 256; *vptr++ = t0;

        numvtx = 6;
    }

    glBindBuffer(GL_ARRAY_BUFFER, CaptureVtxBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, numvtx * 4 * sizeof(u16), vtxbuf);

    glBindVertexArray(CaptureVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, numvtx);
}


void GLRenderer::AllocCapture(u32 bank, u32 start, u32 len)
{
    //
}

void GLRenderer::SyncVRAMCapture(u32 bank, u32 start, u32 len, bool complete)
{
    //
}


bool GLRenderer::GetFramebuffers(u32** top, u32** bottom)
{
    int frontbuf = BackBuffer ^ 1;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, FPOutputTex[frontbuf]);
    // TODO return the texture ID in *top or something?
    return false;
}

}
