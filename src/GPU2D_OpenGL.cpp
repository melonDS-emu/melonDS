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

#include <assert.h>
#include "GPU2D_OpenGL.h"
#include "GPU2D_OpenGL_shaders.h"
#include "GPU.h"
#include "GPU3D.h"

namespace melonDS::GPU2D
{

// NOTE
// for now, this is largely a reimplementation of the software 2D renderer
// in the future, the rendering may be refined to involve more hardware processing
// and include features such as filtering/upscaling for 2D layers

std::unique_ptr<GLRenderer> GLRenderer::New(melonDS::GPU& gpu) noexcept
{
    assert(glBindAttribLocation != nullptr);
    GLuint shaderid;

    if (!OpenGL::CompileVertexFragmentProgram(shaderid,
                                              kFinalPassVS, kFinalPassFS,
                                              "2DFinalPassShader",
                                              {{"vPosition", 0}, {"vTexcoord", 1}},
                                              {{"oColor", 0}}))
        return nullptr;

    auto ret = std::unique_ptr<GLRenderer>(new GLRenderer(gpu));
    ret->FPShaderID = shaderid;
    return ret;
}

GLRenderer::GLRenderer(melonDS::GPU& gpu)
        : Renderer2D(), GPU(gpu)
{
    ScreenSwap = 0;

    BGOBJBuffer = new u32[256 * 3 * 192 * 2];
    //AuxInputBuffer = new u16[256 * 192];

    FPScaleULoc = glGetUniformLocation(FPShaderID, "u3DScale");

    glUseProgram(FPShaderID);
    GLuint screenTextureUniform = glGetUniformLocation(FPShaderID, "ScreenTex");
    glUniform1i(screenTextureUniform, 0);
    GLuint _3dTextureUniform = glGetUniformLocation(FPShaderID, "_3DTex");
    glUniform1i(_3dTextureUniform, 1);

    // all this mess is to prevent bleeding
    float vertices[12][4];
#define SETVERTEX(i, x, y, offset) \
    vertices[i][0] = x; \
    vertices[i][1] = y + offset; \
    vertices[i][2] = (x + 1.f) * (256.f / 2.f); \
    vertices[i][3] = (y + 1.f) * (384.f / 2.f)

    const float padOffset = 1.f/(192*2.f+2.f)*2.f;
    // top screen
    SETVERTEX(0, -1, 1, 0);
    SETVERTEX(1, 1, 0, padOffset);
    SETVERTEX(2, 1, 1, 0);
    SETVERTEX(3, -1, 1, 0);
    SETVERTEX(4, -1, 0, padOffset);
    SETVERTEX(5, 1, 0, padOffset);

    // bottom screen
    SETVERTEX(6, -1, 0, -padOffset);
    SETVERTEX(7, 1, -1, 0);
    SETVERTEX(8, 1, 0, -padOffset);
    SETVERTEX(9, -1, 0, -padOffset);
    SETVERTEX(10, -1, -1, 0);
    SETVERTEX(11, 1, -1, 0);

#undef SETVERTEX

    glGenBuffers(1, &FPVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, FPVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

    glGenVertexArrays(1, &FPVertexArrayID);
    glBindVertexArray(FPVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glGenFramebuffers(FPOutputFB.size(), &FPOutputFB[0]);

    glGenTextures(1, &BGOBJTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, BGOBJTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 256*3, 192*2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    /*glGenTextures(1, &AuxInputTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, AuxInputTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 192, 0, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);*/

    glGenTextures(FPOutputTex.size(), &FPOutputTex[0]);
    for (GLuint i : FPOutputTex)
    {
        glBindTexture(GL_TEXTURE_2D, i);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLRenderer::~GLRenderer()
{
    glDeleteFramebuffers(FPOutputFB.size(), &FPOutputFB[0]);
    glDeleteTextures(1, &BGOBJTex);
    glDeleteTextures(FPOutputTex.size(), &FPOutputTex[0]);

    glDeleteVertexArrays(1, &FPVertexArrayID);
    glDeleteBuffers(1, &FPVertexBufferID);

    glDeleteProgram(FPShaderID);

    //delete[] AuxInputBuffer;
    delete[] BGOBJBuffer;
}


void GLRenderer::SetScaleFactor(int scale)
{
    if (scale == ScaleFactor)
        return;

    ScaleFactor = scale;
    ScreenW = 256 * scale;
    ScreenH = (384+2) * scale;

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, FPOutputTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        // fill the padding
        u8* zeroPixels = (u8*) calloc(1, ScreenW*2*scale*4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192*scale, ScreenW, 2*scale, GL_RGBA, GL_UNSIGNED_BYTE, zeroPixels);

        GLenum fbassign[] = {GL_COLOR_ATTACHMENT0};
        glBindFramebuffer(GL_FRAMEBUFFER, FPOutputFB[i]);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FPOutputTex[i], 0);
        glDrawBuffers(1, fbassign);
        free(zeroPixels);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void GLRenderer::DrawScanline(u32 line, Unit* unit)
{
    CurUnit = unit;

    int screen = !(CurUnit->Num ^ ScreenSwap);
    int yoffset = (screen * 192) + line;
    u32* dst = &BGOBJBuffer[256 * yoffset];

    int n3dline = line;
    line = GPU.VCount;

    if (CurUnit->Num == 0)
    {
        auto bgDirty = GPU.VRAMDirty_ABG.DeriveState(GPU.VRAMMap_ABG, GPU);
        GPU.MakeVRAMFlat_ABGCoherent(bgDirty);
        auto bgExtPalDirty = GPU.VRAMDirty_ABGExtPal.DeriveState(GPU.VRAMMap_ABGExtPal, GPU);
        GPU.MakeVRAMFlat_ABGExtPalCoherent(bgExtPalDirty);
        auto objExtPalDirty = GPU.VRAMDirty_AOBJExtPal.DeriveState(&GPU.VRAMMap_AOBJExtPal, GPU);
        GPU.MakeVRAMFlat_AOBJExtPalCoherent(objExtPalDirty);
    }
    else
    {
        auto bgDirty = GPU.VRAMDirty_BBG.DeriveState(GPU.VRAMMap_BBG, GPU);
        GPU.MakeVRAMFlat_BBGCoherent(bgDirty);
        auto bgExtPalDirty = GPU.VRAMDirty_BBGExtPal.DeriveState(GPU.VRAMMap_BBGExtPal, GPU);
        GPU.MakeVRAMFlat_BBGExtPalCoherent(bgExtPalDirty);
        auto objExtPalDirty = GPU.VRAMDirty_BOBJExtPal.DeriveState(&GPU.VRAMMap_BOBJExtPal, GPU);
        GPU.MakeVRAMFlat_BOBJExtPalCoherent(objExtPalDirty);
    }

    bool forceblank = false;

    // scanlines that end up outside of the GPU drawing range
    // (as a result of writing to VCount) are filled white
    if (line > 192) forceblank = true;

    // GPU B can be completely disabled by POWCNT1
    // oddly that's not the case for GPU A
    if (CurUnit->Num && !CurUnit->Enabled) forceblank = true;

    if (line == 0 && CurUnit->CaptureCnt & (1 << 31) && !forceblank)
        CurUnit->CaptureLatch = true;

    if (forceblank)
    {
        for (int i = 0; i < 256; i++)
            dst[i] = 0xFFFFFFFF;

        return;
    }

    u32 dispmode = CurUnit->DispCnt >> 16;
    dispmode &= (CurUnit->Num ? 0x1 : 0x3);

    // always render regular graphics
    DrawScanline_BGOBJ(line);
    CurUnit->UpdateMosaicCounters(line);

    for (int i = 0; i < 256; i++)
    {
        // add window mask for color effects
        // TODO
    }

    // TODO: if needed, capture VRAM/mainmem FIFO
    // also do it for display capture if source B is used
}

void GLRenderer::VBlank(Unit* unitA, Unit* unitB)
{
    // TODO: do GL rendering here
}

void GLRenderer::VBlankEnd(Unit* unitA, Unit* unitB)
{
/*#ifdef OGLRENDERER_ENABLED
    if (Renderer3D& renderer3d = GPU.GPU3D.GetCurrentRenderer(); renderer3d.Accelerated)
    {
        if ((unitA->CaptureCnt & (1<<31)) && (((unitA->CaptureCnt >> 29) & 0x3) != 1))
        {
            renderer3d.PrepareCaptureFrame();
        }
    }
#endif*/
}


bool GLRenderer::GetFramebuffers(u32** top, u32** bottom)
{
    int frontbuf = BackBuffer ^ 1;

    // TODO map output texture here (BindOutputTexture())
    
    return false;
}

void GLRenderer::SwapBuffers()
{
    BackBuffer ^= 1;
}


void GLRenderer::DoCapture(u32 line, u32 width)
{
    // TODO
}

#define DoDrawBG(type, line, num) \
do \
{ \
    if ((bgCnt[num] & 0x0040) && (CurUnit->BGMosaicSize[0] > 0)) \
    { \
        DrawBG_##type<true>(line, num); \
    } \
    else \
    { \
        DrawBG_##type<false>(line, num); \
    } \
} while (false)

#define DoDrawBG_Large(line) \
do \
{ \
    if ((bgCnt[2] & 0x0040) && (CurUnit->BGMosaicSize[0] > 0)) \
    { \
        DrawBG_Large<true>(line); \
    } \
    else \
    { \
        DrawBG_Large<false>(line); \
    } \
} while (false)

template<u32 bgmode>
void GLRenderer::DrawScanlineBGMode(u32 line)
{
    u32 dispCnt = CurUnit->DispCnt;
    u16* bgCnt = CurUnit->BGCnt;
    for (int i = 3; i >= 0; i--)
    {
        if ((bgCnt[3] & 0x3) == i)
        {
            if (dispCnt & 0x0800)
            {
                if (bgmode >= 3)
                    DoDrawBG(Extended, line, 3);
                else if (bgmode >= 1)
                    DoDrawBG(Affine, line, 3);
                else
                    DoDrawBG(Text, line, 3);
            }
        }
        if ((bgCnt[2] & 0x3) == i)
        {
            if (dispCnt & 0x0400)
            {
                if (bgmode == 5)
                    DoDrawBG(Extended, line, 2);
                else if (bgmode == 4 || bgmode == 2)
                    DoDrawBG(Affine, line, 2);
                else
                    DoDrawBG(Text, line, 2);
            }
        }
        if ((bgCnt[1] & 0x3) == i)
        {
            if (dispCnt & 0x0200)
            {
                DoDrawBG(Text, line, 1);
            }
        }
        if ((bgCnt[0] & 0x3) == i)
        {
            if (dispCnt & 0x0100)
            {
                if (!CurUnit->Num && (dispCnt & 0x8))
                    DrawBG_3D();
                else
                    DoDrawBG(Text, line, 0);
            }
        }
        if ((dispCnt & 0x1000) && NumSprites[CurUnit->Num])
        {
            InterleaveSprites(0x40000 | (i<<16));
        }

    }
}

void GLRenderer::DrawScanlineBGMode6(u32 line)
{
    u32 dispCnt = CurUnit->DispCnt;
    u16* bgCnt = CurUnit->BGCnt;
    for (int i = 3; i >= 0; i--)
    {
        if ((bgCnt[2] & 0x3) == i)
        {
            if (dispCnt & 0x0400)
            {
                DoDrawBG_Large(line);
            }
        }
        if ((bgCnt[0] & 0x3) == i)
        {
            if (dispCnt & 0x0100)
            {
                if ((!CurUnit->Num) && (dispCnt & 0x8))
                    DrawBG_3D();
            }
        }
        if ((dispCnt & 0x1000) && NumSprites[CurUnit->Num])
        {
            InterleaveSprites(0x40000 | (i<<16));
        }
    }
}

void GLRenderer::DrawScanlineBGMode7(u32 line)
{
    u32 dispCnt = CurUnit->DispCnt;
    u16* bgCnt = CurUnit->BGCnt;
    // mode 7 only has text-mode BG0 and BG1

    for (int i = 3; i >= 0; i--)
    {
        if ((bgCnt[1] & 0x3) == i)
        {
            if (dispCnt & 0x0200)
            {
                DoDrawBG(Text, line, 1);
            }
        }
        if ((bgCnt[0] & 0x3) == i)
        {
            if (dispCnt & 0x0100)
            {
                if (!CurUnit->Num && (dispCnt & 0x8))
                    DrawBG_3D();
                else
                    DoDrawBG(Text, line, 0);
            }
        }
        if ((dispCnt & 0x1000) && NumSprites[CurUnit->Num])
        {
            InterleaveSprites(0x40000 | (i<<16));
        }
    }
}

void GLRenderer::DrawScanline_BGOBJ(u32 line)
{
    // forced blank disables BG/OBJ compositing
    if (CurUnit->DispCnt & (1<<7))
    {
        for (int i = 0; i < 256; i++)
            BGOBJLine[i] = 0xFF3F3F3F;

        return;
    }

    u64 backdrop;
    if (CurUnit->Num) backdrop = *(u16*)&GPU.Palette[0x400];
    else     backdrop = *(u16*)&GPU.Palette[0];

    {
        u8 r = (backdrop & 0x001F) << 1;
        u8 g = (backdrop & 0x03E0) >> 4;
        u8 b = (backdrop & 0x7C00) >> 9;

        backdrop = r | (g << 8) | (b << 16) | 0x20000000;
        backdrop |= (backdrop << 32);

        for (int i = 0; i < 256; i+=2)
            *(u64*)&BGOBJLine[i] = backdrop;
    }

    if (CurUnit->DispCnt & 0xE000)
        CurUnit->CalculateWindowMask(line, WindowMask, OBJWindow[CurUnit->Num]);
    else
        memset(WindowMask, 0xFF, 256);

    ApplySpriteMosaicX();
    CurBGXMosaicTable = MosaicTable[CurUnit->BGMosaicSize[0]].data();

    switch (CurUnit->DispCnt & 0x7)
    {
        case 0: DrawScanlineBGMode<0>(line); break;
        case 1: DrawScanlineBGMode<1>(line); break;
        case 2: DrawScanlineBGMode<2>(line); break;
        case 3: DrawScanlineBGMode<3>(line); break;
        case 4: DrawScanlineBGMode<4>(line); break;
        case 5: DrawScanlineBGMode<5>(line); break;
        case 6: DrawScanlineBGMode6(line); break;
        case 7: DrawScanlineBGMode7(line); break;
    }

    // color special effects
    // can likely be optimized

    // TODO actually do color effects

    if (CurUnit->BGMosaicY >= CurUnit->BGMosaicYMax)
    {
        CurUnit->BGMosaicY = 0;
        CurUnit->BGMosaicYMax = CurUnit->BGMosaicSize[1];
    }
    else
        CurUnit->BGMosaicY++;

    /*if (OBJMosaicY >= OBJMosaicYMax)
    {
        OBJMosaicY = 0;
        OBJMosaicYMax = OBJMosaicSize[1];
    }
    else
        OBJMosaicY++;*/
}



void GLRenderer::DrawPixel(u32* dst, u16 color, u32 flag)
{
    u8 r = (color & 0x001F) << 1;
    u8 g = (color & 0x03E0) >> 4;
    u8 b = (color & 0x7C00) >> 9;

    *(dst+512) = *(dst+256);
    *(dst+256) = *dst;
    *dst = r | (g << 8) | (b << 16) | flag;
}

void GLRenderer::DrawBG_3D()
{
    int i = 0;

    /*if (GPU.GPU3D.IsRendererAccelerated())
    {
        for (i = 0; i < 256; i++)
        {
            if (!(WindowMask[i] & 0x01)) continue;

            BGOBJLine[i+512] = BGOBJLine[i+256];
            BGOBJLine[i+256] = BGOBJLine[i];
            BGOBJLine[i] = 0x40000000; // 3D-layer placeholder
        }
    }
    else
    {
        for (i = 0; i < 256; i++)
        {
            u32 c = _3DLine[i];

            if ((c >> 24) == 0) continue;
            if (!(WindowMask[i] & 0x01)) continue;

            BGOBJLine[i+256] = BGOBJLine[i];
            BGOBJLine[i] = c | 0x40000000;
        }
    }*/
    // TODO placeholder for 3D
}

template<bool mosaic>
void GLRenderer::DrawBG_Text(u32 line, u32 bgnum)
{
    // workaround for backgrounds missing on aarch64 with lto build
    asm volatile ("" : : : "memory");

    u16 bgcnt = CurUnit->BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;
    u32 extpal, extpalslot;

    u16 xoff = CurUnit->BGXPos[bgnum];
    u16 yoff = CurUnit->BGYPos[bgnum] + line;

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        yoff -= CurUnit->BGMosaicY;
    }

    u32 widexmask = (bgcnt & 0x4000) ? 0x100 : 0;

    extpal = (CurUnit->DispCnt & 0x40000000);
    if (extpal) extpalslot = ((bgnum<2) && (bgcnt&0x2000)) ? (2+bgnum) : bgnum;

    u8* bgvram;
    u32 bgvrammask;
    CurUnit->GetBGVRAM(bgvram, bgvrammask);
    if (CurUnit->Num)
    {
        tilesetaddr = ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU.Palette[0x400];
    }
    else
    {
        tilesetaddr = ((CurUnit->DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((CurUnit->DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU.Palette[0];
    }

    // adjust Y position in tilemap
    if (bgcnt & 0x8000)
    {
        tilemapaddr += ((yoff & 0x1F8) << 3);
        if (bgcnt & 0x4000)
            tilemapaddr += ((yoff & 0x100) << 3);
    }
    else
        tilemapaddr += ((yoff & 0xF8) << 3);

    u16 curtile;
    u16* curpal;
    u32 pixelsaddr;
    u8 color;
    u32 lastxpos;

    if (bgcnt & 0x0080)
    {
        // 256-color

        // preload shit as needed
        if ((xoff & 0x7) || mosaic)
        {
            curtile = *(u16*)&bgvram[(tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3)) & bgvrammask];

            if (extpal) curpal = CurUnit->GetBGExtPal(extpalslot, curtile>>12);
            else        curpal = pal;

            pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 6)
                         + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 3);
        }

        if (mosaic) lastxpos = xoff;

        for (int i = 0; i < 256; i++)
        {
            u32 xpos;
            if (mosaic) xpos = xoff - CurBGXMosaicTable[i];
            else        xpos = xoff;

            if ((!mosaic && (!(xpos & 0x7))) ||
                (mosaic && ((xpos >> 3) != (lastxpos >> 3))))
            {
                // load a new tile
                curtile = *(u16*)&bgvram[(tilemapaddr + ((xpos & 0xF8) >> 2) + ((xpos & widexmask) << 3)) & bgvrammask];

                if (extpal) curpal = CurUnit->GetBGExtPal(extpalslot, curtile>>12);
                else        curpal = pal;

                pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 6)
                             + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 3);

                if (mosaic) lastxpos = xpos;
            }

            // draw pixel
            if (WindowMask[i] & (1<<bgnum))
            {
                u32 tilexoff = (curtile & 0x0400) ? (7-(xpos&0x7)) : (xpos&0x7);
                color = bgvram[(pixelsaddr + tilexoff) & bgvrammask];

                if (color)
                    DrawPixel(&BGOBJLine[i], curpal[color], 0x01000000<<bgnum);
            }

            xoff++;
        }
    }
    else
    {
        // 16-color

        // preload shit as needed
        if ((xoff & 0x7) || mosaic)
        {
            curtile = *(u16*)&bgvram[((tilemapaddr + ((xoff & 0xF8) >> 2) + ((xoff & widexmask) << 3))) & bgvrammask];
            curpal = pal + ((curtile & 0xF000) >> 8);
            pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 5)
                         + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 2);
        }

        if (mosaic) lastxpos = xoff;

        for (int i = 0; i < 256; i++)
        {
            u32 xpos;
            if (mosaic) xpos = xoff - CurBGXMosaicTable[i];
            else        xpos = xoff;

            if ((!mosaic && (!(xpos & 0x7))) ||
                (mosaic && ((xpos >> 3) != (lastxpos >> 3))))
            {
                // load a new tile
                curtile = *(u16*)&bgvram[(tilemapaddr + ((xpos & 0xF8) >> 2) + ((xpos & widexmask) << 3)) & bgvrammask];
                curpal = pal + ((curtile & 0xF000) >> 8);
                pixelsaddr = tilesetaddr + ((curtile & 0x03FF) << 5)
                             + (((curtile & 0x0800) ? (7-(yoff&0x7)) : (yoff&0x7)) << 2);

                if (mosaic) lastxpos = xpos;
            }

            // draw pixel
            if (WindowMask[i] & (1<<bgnum))
            {
                u32 tilexoff = (curtile & 0x0400) ? (7-(xpos&0x7)) : (xpos&0x7);
                if (tilexoff & 0x1)
                {
                    color = bgvram[(pixelsaddr + (tilexoff >> 1)) & bgvrammask] >> 4;
                }
                else
                {
                    color = bgvram[(pixelsaddr + (tilexoff >> 1)) & bgvrammask] & 0x0F;
                }

                if (color)
                    DrawPixel(&BGOBJLine[i], curpal[color], 0x01000000<<bgnum);
            }

            xoff++;
        }
    }
}

template<bool mosaic>
void GLRenderer::DrawBG_Affine(u32 line, u32 bgnum)
{
    u16 bgcnt = CurUnit->BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;

    u32 coordmask;
    u32 yshift;
    switch (bgcnt & 0xC000)
    {
        case 0x0000: coordmask = 0x07800; yshift = 7; break;
        case 0x4000: coordmask = 0x0F800; yshift = 8; break;
        case 0x8000: coordmask = 0x1F800; yshift = 9; break;
        case 0xC000: coordmask = 0x3F800; yshift = 10; break;
    }

    u32 overflowmask;
    if (bgcnt & 0x2000) overflowmask = 0;
    else                overflowmask = ~(coordmask | 0x7FF);

    s16 rotA = CurUnit->BGRotA[bgnum-2];
    s16 rotB = CurUnit->BGRotB[bgnum-2];
    s16 rotC = CurUnit->BGRotC[bgnum-2];
    s16 rotD = CurUnit->BGRotD[bgnum-2];

    s32 rotX = CurUnit->BGXRefInternal[bgnum-2];
    s32 rotY = CurUnit->BGYRefInternal[bgnum-2];

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        rotX -= (CurUnit->BGMosaicY * rotB);
        rotY -= (CurUnit->BGMosaicY * rotD);
    }

    u8* bgvram;
    u32 bgvrammask;
    CurUnit->GetBGVRAM(bgvram, bgvrammask);

    if (CurUnit->Num)
    {
        tilesetaddr = ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU.Palette[0x400];
    }
    else
    {
        tilesetaddr = ((CurUnit->DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
        tilemapaddr = ((CurUnit->DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

        pal = (u16*)&GPU.Palette[0];
    }

    u16 curtile;
    u8 color;

    yshift -= 3;

    for (int i = 0; i < 256; i++)
    {
        if (WindowMask[i] & (1<<bgnum))
        {
            s32 finalX, finalY;
            if (mosaic)
            {
                int im = CurBGXMosaicTable[i];
                finalX = rotX - (im * rotA);
                finalY = rotY - (im * rotC);
            }
            else
            {
                finalX = rotX;
                finalY = rotY;
            }

            if ((!((finalX|finalY) & overflowmask)))
            {
                curtile = bgvram[(tilemapaddr + ((((finalY & coordmask) >> 11) << yshift) + ((finalX & coordmask) >> 11))) & bgvrammask];

                // draw pixel
                u32 tilexoff = (finalX >> 8) & 0x7;
                u32 tileyoff = (finalY >> 8) & 0x7;

                color = bgvram[(tilesetaddr + (curtile << 6) + (tileyoff << 3) + tilexoff) & bgvrammask];

                if (color)
                    DrawPixel(&BGOBJLine[i], pal[color], 0x01000000<<bgnum);
            }
        }

        rotX += rotA;
        rotY += rotC;
    }

    CurUnit->BGXRefInternal[bgnum-2] += rotB;
    CurUnit->BGYRefInternal[bgnum-2] += rotD;
}

template<bool mosaic>
void GLRenderer::DrawBG_Extended(u32 line, u32 bgnum)
{
    u16 bgcnt = CurUnit->BGCnt[bgnum];

    u32 tilesetaddr, tilemapaddr;
    u16* pal;
    u32 extpal;

    u8* bgvram;
    u32 bgvrammask;
    CurUnit->GetBGVRAM(bgvram, bgvrammask);

    extpal = (CurUnit->DispCnt & 0x40000000);

    s16 rotA = CurUnit->BGRotA[bgnum-2];
    s16 rotB = CurUnit->BGRotB[bgnum-2];
    s16 rotC = CurUnit->BGRotC[bgnum-2];
    s16 rotD = CurUnit->BGRotD[bgnum-2];

    s32 rotX = CurUnit->BGXRefInternal[bgnum-2];
    s32 rotY = CurUnit->BGYRefInternal[bgnum-2];

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        rotX -= (CurUnit->BGMosaicY * rotB);
        rotY -= (CurUnit->BGMosaicY * rotD);
    }

    if (bgcnt & 0x0080)
    {
        // bitmap modes

        u32 xmask, ymask;
        u32 yshift;
        switch (bgcnt & 0xC000)
        {
            case 0x0000: xmask = 0x07FFF; ymask = 0x07FFF; yshift = 7; break;
            case 0x4000: xmask = 0x0FFFF; ymask = 0x0FFFF; yshift = 8; break;
            case 0x8000: xmask = 0x1FFFF; ymask = 0x0FFFF; yshift = 9; break;
            case 0xC000: xmask = 0x1FFFF; ymask = 0x1FFFF; yshift = 9; break;
        }

        u32 ofxmask, ofymask;
        if (bgcnt & 0x2000)
        {
            ofxmask = 0;
            ofymask = 0;
        }
        else
        {
            ofxmask = ~xmask;
            ofymask = ~ymask;
        }

        if (CurUnit->Num) tilemapaddr = ((bgcnt & 0x1F00) << 6);
        else              tilemapaddr = ((bgcnt & 0x1F00) << 6);

        if (bgcnt & 0x0004)
        {
            // direct color bitmap

            u16 color;

            for (int i = 0; i < 256; i++)
            {
                if (WindowMask[i] & (1<<bgnum))
                {
                    s32 finalX, finalY;
                    if (mosaic)
                    {
                        int im = CurBGXMosaicTable[i];
                        finalX = rotX - (im * rotA);
                        finalY = rotY - (im * rotC);
                    }
                    else
                    {
                        finalX = rotX;
                        finalY = rotY;
                    }

                    if (!(finalX & ofxmask) && !(finalY & ofymask))
                    {
                        color = *(u16*)&bgvram[(tilemapaddr + (((((finalY & ymask) >> 8) << yshift) + ((finalX & xmask) >> 8)) << 1)) & bgvrammask];

                        if (color & 0x8000)
                            DrawPixel(&BGOBJLine[i], color, 0x01000000<<bgnum);
                    }
                }

                rotX += rotA;
                rotY += rotC;
            }
        }
        else
        {
            // 256-color bitmap

            if (CurUnit->Num) pal = (u16*)&GPU.Palette[0x400];
            else              pal = (u16*)&GPU.Palette[0];

            u8 color;

            for (int i = 0; i < 256; i++)
            {
                if (WindowMask[i] & (1<<bgnum))
                {
                    s32 finalX, finalY;
                    if (mosaic)
                    {
                        int im = CurBGXMosaicTable[i];
                        finalX = rotX - (im * rotA);
                        finalY = rotY - (im * rotC);
                    }
                    else
                    {
                        finalX = rotX;
                        finalY = rotY;
                    }

                    if (!(finalX & ofxmask) && !(finalY & ofymask))
                    {
                        color = bgvram[(tilemapaddr + (((finalY & ymask) >> 8) << yshift) + ((finalX & xmask) >> 8)) & bgvrammask];

                        if (color)
                            DrawPixel(&BGOBJLine[i], pal[color], 0x01000000<<bgnum);
                    }
                }

                rotX += rotA;
                rotY += rotC;
            }
        }
    }
    else
    {
        // mixed affine/text mode

        u32 coordmask;
        u32 yshift;
        switch (bgcnt & 0xC000)
        {
            case 0x0000: coordmask = 0x07800; yshift = 7; break;
            case 0x4000: coordmask = 0x0F800; yshift = 8; break;
            case 0x8000: coordmask = 0x1F800; yshift = 9; break;
            case 0xC000: coordmask = 0x3F800; yshift = 10; break;
        }

        u32 overflowmask;
        if (bgcnt & 0x2000) overflowmask = 0;
        else                overflowmask = ~(coordmask | 0x7FF);

        if (CurUnit->Num)
        {
            tilesetaddr = ((bgcnt & 0x003C) << 12);
            tilemapaddr = ((bgcnt & 0x1F00) << 3);

            pal = (u16*)&GPU.Palette[0x400];
        }
        else
        {
            tilesetaddr = ((CurUnit->DispCnt & 0x07000000) >> 8) + ((bgcnt & 0x003C) << 12);
            tilemapaddr = ((CurUnit->DispCnt & 0x38000000) >> 11) + ((bgcnt & 0x1F00) << 3);

            pal = (u16*)&GPU.Palette[0];
        }

        u16 curtile;
        u16* curpal;
        u8 color;

        yshift -= 3;

        for (int i = 0; i < 256; i++)
        {
            if (WindowMask[i] & (1<<bgnum))
            {
                s32 finalX, finalY;
                if (mosaic)
                {
                    int im = CurBGXMosaicTable[i];
                    finalX = rotX - (im * rotA);
                    finalY = rotY - (im * rotC);
                }
                else
                {
                    finalX = rotX;
                    finalY = rotY;
                }

                if ((!((finalX|finalY) & overflowmask)))
                {
                    curtile = *(u16*)&bgvram[(tilemapaddr + (((((finalY & coordmask) >> 11) << yshift) + ((finalX & coordmask) >> 11)) << 1)) & bgvrammask];

                    if (extpal) curpal = CurUnit->GetBGExtPal(bgnum, curtile>>12);
                    else        curpal = pal;

                    // draw pixel
                    u32 tilexoff = (finalX >> 8) & 0x7;
                    u32 tileyoff = (finalY >> 8) & 0x7;

                    if (curtile & 0x0400) tilexoff = 7-tilexoff;
                    if (curtile & 0x0800) tileyoff = 7-tileyoff;

                    color = bgvram[(tilesetaddr + ((curtile & 0x03FF) << 6) + (tileyoff << 3) + tilexoff) & bgvrammask];

                    if (color)
                        DrawPixel(&BGOBJLine[i], curpal[color], 0x01000000<<bgnum);
                }
            }

            rotX += rotA;
            rotY += rotC;
        }
    }

    CurUnit->BGXRefInternal[bgnum-2] += rotB;
    CurUnit->BGYRefInternal[bgnum-2] += rotD;
}

template<bool mosaic>
void GLRenderer::DrawBG_Large(u32 line) // BG is always BG2
{
    u16 bgcnt = CurUnit->BGCnt[2];

    u16* pal;

    // large BG sizes:
    // 0: 512x1024
    // 1: 1024x512
    // 2: 512x256
    // 3: 512x512
    u32 xmask, ymask;
    u32 yshift;
    switch (bgcnt & 0xC000)
    {
        case 0x0000: xmask = 0x1FFFF; ymask = 0x3FFFF; yshift = 9; break;
        case 0x4000: xmask = 0x3FFFF; ymask = 0x1FFFF; yshift = 10; break;
        case 0x8000: xmask = 0x1FFFF; ymask = 0x0FFFF; yshift = 9; break;
        case 0xC000: xmask = 0x1FFFF; ymask = 0x1FFFF; yshift = 9; break;
    }

    u32 ofxmask, ofymask;
    if (bgcnt & 0x2000)
    {
        ofxmask = 0;
        ofymask = 0;
    }
    else
    {
        ofxmask = ~xmask;
        ofymask = ~ymask;
    }

    s16 rotA = CurUnit->BGRotA[0];
    s16 rotB = CurUnit->BGRotB[0];
    s16 rotC = CurUnit->BGRotC[0];
    s16 rotD = CurUnit->BGRotD[0];

    s32 rotX = CurUnit->BGXRefInternal[0];
    s32 rotY = CurUnit->BGYRefInternal[0];

    if (bgcnt & 0x0040)
    {
        // vertical mosaic
        rotX -= (CurUnit->BGMosaicY * rotB);
        rotY -= (CurUnit->BGMosaicY * rotD);
    }

    u8* bgvram;
    u32 bgvrammask;
    CurUnit->GetBGVRAM(bgvram, bgvrammask);

    // 256-color bitmap

    if (CurUnit->Num) pal = (u16*)&GPU.Palette[0x400];
    else     pal = (u16*)&GPU.Palette[0];

    u8 color;

    for (int i = 0; i < 256; i++)
    {
        if (WindowMask[i] & (1<<2))
        {
            s32 finalX, finalY;
            if (mosaic)
            {
                int im = CurBGXMosaicTable[i];
                finalX = rotX - (im * rotA);
                finalY = rotY - (im * rotC);
            }
            else
            {
                finalX = rotX;
                finalY = rotY;
            }

            if (!(finalX & ofxmask) && !(finalY & ofymask))
            {
                color = bgvram[((((finalY & ymask) >> 8) << yshift) + ((finalX & xmask) >> 8)) & bgvrammask];

                if (color)
                    DrawPixel(&BGOBJLine[i], pal[color], 0x01000000<<2);
            }
        }

        rotX += rotA;
        rotY += rotC;
    }

    CurUnit->BGXRefInternal[0] += rotB;
    CurUnit->BGYRefInternal[0] += rotD;
}

// OBJ line buffer:
// * bit0-15: color (bit15=1: direct color, bit15=0: palette index, bit12=0 to indicate extpal)
// * bit16-17: BG-relative priority
// * bit18: non-transparent sprite pixel exists here
// * bit19: X mosaic should be applied here
// * bit24-31: compositor flags

void GLRenderer::ApplySpriteMosaicX()
{
    // apply X mosaic if needed
    // X mosaic for sprites is applied after all sprites are rendered

    if (CurUnit->OBJMosaicSize[0] == 0) return;

    u32* objLine = OBJLine[CurUnit->Num];

    u8* curOBJXMosaicTable = MosaicTable[CurUnit->OBJMosaicSize[0]].data();

    u32 lastcolor = objLine[0];

    for (u32 i = 1; i < 256; i++)
    {
        u32 currentcolor = objLine[i];

        if (!(lastcolor & currentcolor & 0x100000) || curOBJXMosaicTable[i] == 0)
            lastcolor = currentcolor;
        else
            objLine[i] = lastcolor;
    }
}

void GLRenderer::InterleaveSprites(u32 prio)
{
    u32* objLine = OBJLine[CurUnit->Num];
    u16* pal = (u16*)&GPU.Palette[CurUnit->Num ? 0x600 : 0x200];

    if (CurUnit->DispCnt & 0x80000000)
    {
        u16* extpal = CurUnit->GetOBJExtPal();

        for (u32 i = 0; i < 256; i++)
        {
            if ((objLine[i] & 0x70000) != prio) continue;
            if (!(WindowMask[i] & 0x10))        continue;

            u16 color;
            u32 pixel = objLine[i];

            if (pixel & 0x8000)
                color = pixel & 0x7FFF;
            else if (pixel & 0x1000)
                color = pal[pixel & 0xFF];
            else
                color = extpal[pixel & 0xFFF];

            DrawPixel(&BGOBJLine[i], color, pixel & 0xFF000000);
        }
    }
    else
    {
        // optimized no-extpal version

        for (u32 i = 0; i < 256; i++)
        {
            if ((objLine[i] & 0x70000) != prio) continue;
            if (!(WindowMask[i] & 0x10))        continue;

            u16 color;
            u32 pixel = objLine[i];

            if (pixel & 0x8000)
                color = pixel & 0x7FFF;
            else
                color = pal[pixel & 0xFF];

            DrawPixel(&BGOBJLine[i], color, pixel & 0xFF000000);
        }
    }
}

#define DoDrawSprite(type, ...) \
if (iswin) \
{ \
    DrawSprite_##type<true>(__VA_ARGS__); \
} \
else \
{ \
    DrawSprite_##type<false>(__VA_ARGS__); \
}

void GLRenderer::DrawSprites(u32 line, Unit* unit)
{
    CurUnit = unit;

    if (line == 0)
    {
        // reset those counters here
        // TODO: find out when those are supposed to be reset
        // it would make sense to reset them at the end of VBlank
        // however, sprites are rendered one scanline in advance
        // so they need to be reset a bit earlier

        CurUnit->OBJMosaicY = 0;
        CurUnit->OBJMosaicYCount = 0;
    }

    if (CurUnit->Num == 0)
    {
        auto objDirty = GPU.VRAMDirty_AOBJ.DeriveState(GPU.VRAMMap_AOBJ, GPU);
        GPU.MakeVRAMFlat_AOBJCoherent(objDirty);
    }
    else
    {
        auto objDirty = GPU.VRAMDirty_BOBJ.DeriveState(GPU.VRAMMap_BOBJ, GPU);
        GPU.MakeVRAMFlat_BOBJCoherent(objDirty);
    }

    NumSprites[CurUnit->Num] = 0;
    memset(OBJLine[CurUnit->Num], 0, 256*4);
    memset(OBJWindow[CurUnit->Num], 0, 256);
    if (!(CurUnit->DispCnt & 0x1000)) return;

    u16* oam = (u16*)&GPU.OAM[CurUnit->Num ? 0x400 : 0];

    const s32 spritewidth[16] =
            {
                    8, 16, 8, 8,
                    16, 32, 8, 8,
                    32, 32, 16, 8,
                    64, 64, 32, 8
            };
    const s32 spriteheight[16] =
            {
                    8, 8, 16, 8,
                    16, 8, 32, 8,
                    32, 16, 32, 8,
                    64, 32, 64, 8
            };

    for (int bgnum = 0x0C00; bgnum >= 0x0000; bgnum -= 0x0400)
    {
        for (int sprnum = 127; sprnum >= 0; sprnum--)
        {
            u16* attrib = &oam[sprnum*4];

            if ((attrib[2] & 0x0C00) != bgnum)
                continue;

            bool iswin = (((attrib[0] >> 10) & 0x3) == 2);

            u32 sprline;
            if ((attrib[0] & 0x1000) && !iswin)
            {
                // apply Y mosaic
                sprline = CurUnit->OBJMosaicY;
            }
            else
                sprline = line;

            if (attrib[0] & 0x0100)
            {
                u32 sizeparam = (attrib[0] >> 14) | ((attrib[1] & 0xC000) >> 12);
                s32 width = spritewidth[sizeparam];
                s32 height = spriteheight[sizeparam];
                s32 boundwidth = width;
                s32 boundheight = height;

                if (attrib[0] & 0x0200)
                {
                    boundwidth <<= 1;
                    boundheight <<= 1;
                }

                u32 ypos = attrib[0] & 0xFF;
                if (((line - ypos) & 0xFF) >= (u32)boundheight)
                    continue;
                ypos = (sprline - ypos) & 0xFF;

                s32 xpos = (s32)(attrib[1] << 23) >> 23;
                if (xpos <= -boundwidth)
                    continue;

                u32 rotparamgroup = (attrib[1] >> 9) & 0x1F;

                DoDrawSprite(Rotscale, sprnum, boundwidth, boundheight, width, height, xpos, ypos);

                NumSprites[CurUnit->Num]++;
            }
            else
            {
                if (attrib[0] & 0x0200)
                    continue;

                u32 sizeparam = (attrib[0] >> 14) | ((attrib[1] & 0xC000) >> 12);
                s32 width = spritewidth[sizeparam];
                s32 height = spriteheight[sizeparam];

                u32 ypos = attrib[0] & 0xFF;
                if (((line - ypos) & 0xFF) >= (u32)height)
                    continue;
                ypos = (sprline - ypos) & 0xFF;

                s32 xpos = (s32)(attrib[1] << 23) >> 23;
                if (xpos <= -width)
                    continue;

                DoDrawSprite(Normal, sprnum, width, height, xpos, ypos);

                NumSprites[CurUnit->Num]++;
            }
        }
    }
}

template<bool window>
void GLRenderer::DrawSprite_Rotscale(u32 num, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, s32 ypos)
{
    u16* oam = (u16*)&GPU.OAM[CurUnit->Num ? 0x400 : 0];
    u16* attrib = &oam[num * 4];
    u16* rotparams = &oam[(((attrib[1] >> 9) & 0x1F) * 16) + 3];

    u32 pixelattr = ((attrib[2] & 0x0C00) << 6) | 0xC0000;
    u32 tilenum = attrib[2] & 0x03FF;
    u32 spritemode = window ? 0 : ((attrib[0] >> 10) & 0x3);

    u32 ytilefactor;

    u8* objvram;
    u32 objvrammask;
    CurUnit->GetOBJVRAM(objvram, objvrammask);

    u32* objLine = OBJLine[CurUnit->Num];
    u8* objWindow = OBJWindow[CurUnit->Num];

    s32 centerX = boundwidth >> 1;
    s32 centerY = boundheight >> 1;

    if ((attrib[0] & 0x1000) && !window)
    {
        // apply Y mosaic
        pixelattr |= 0x100000;
    }

    u32 xoff;
    if (xpos >= 0)
    {
        xoff = 0;
        if ((xpos+boundwidth) > 256)
            boundwidth = 256-xpos;
    }
    else
    {
        xoff = -xpos;
        xpos = 0;
    }

    s16 rotA = (s16)rotparams[0];
    s16 rotB = (s16)rotparams[4];
    s16 rotC = (s16)rotparams[8];
    s16 rotD = (s16)rotparams[12];

    s32 rotX = ((xoff-centerX) * rotA) + ((ypos-centerY) * rotB) + (width << 7);
    s32 rotY = ((xoff-centerX) * rotC) + ((ypos-centerY) * rotD) + (height << 7);

    width <<= 8;
    height <<= 8;

    u16 color = 0; // transparent in all cases

    if (spritemode == 3)
    {
        u32 alpha = attrib[2] >> 12;
        if (!alpha) return;
        alpha++;

        pixelattr |= (0xC0000000 | (alpha << 24));

        u32 pixelsaddr;
        if (CurUnit->DispCnt & 0x40)
        {
            if (CurUnit->DispCnt & 0x20)
            {
                // 'reserved'
                // draws nothing

                return;
            }
            else
            {
                pixelsaddr = tilenum << (7 + ((CurUnit->DispCnt >> 22) & 0x1));
                ytilefactor = ((width >> 8) * 2);
            }
        }
        else
        {
            if (CurUnit->DispCnt & 0x20)
            {
                pixelsaddr = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                ytilefactor = (256 * 2);
            }
            else
            {
                pixelsaddr = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                ytilefactor = (128 * 2);
            }
        }

        for (; xoff < boundwidth;)
        {
            if ((u32)rotX < width && (u32)rotY < height)
            {
                color = *(u16*)&objvram[(pixelsaddr + ((rotY >> 8) * ytilefactor) + ((rotX >> 8) << 1)) & objvrammask];

                if (color & 0x8000)
                {
                    if (window) objWindow[xpos] = 1;
                    else        objLine[xpos] = color | pixelattr;
                }
                else if (!window)
                {
                    if (objLine[xpos] == 0)
                        objLine[xpos] = pixelattr & 0x180000;
                }
            }

            rotX += rotA;
            rotY += rotC;
            xoff++;
            xpos++;
        }
    }
    else
    {
        u32 pixelsaddr = tilenum;
        if (CurUnit->DispCnt & 0x10)
        {
            pixelsaddr <<= ((CurUnit->DispCnt >> 20) & 0x3);
            ytilefactor = (width >> 11) << ((attrib[0] & 0x2000) ? 1:0);
        }
        else
        {
            ytilefactor = 0x20;
        }

        if (spritemode == 1) pixelattr |= 0x80000000;
        else                 pixelattr |= 0x10000000;

        ytilefactor <<= 5;
        pixelsaddr <<= 5;

        if (attrib[0] & 0x2000)
        {
            // 256-color

            if (!window)
            {
                if (!(CurUnit->DispCnt & 0x80000000))
                    pixelattr |= 0x1000;
                else
                    pixelattr |= ((attrib[2] & 0xF000) >> 4);
            }

            for (; xoff < boundwidth;)
            {
                if ((u32)rotX < width && (u32)rotY < height)
                {
                    color = objvram[(pixelsaddr + ((rotY>>11)*ytilefactor) + ((rotY&0x700)>>5) + ((rotX>>11)*64) + ((rotX&0x700)>>8)) & objvrammask];

                    if (color)
                    {
                        if (window) objWindow[xpos] = 1;
                        else        objLine[xpos] = color | pixelattr;
                    }
                    else if (!window)
                    {
                        if (objLine[xpos] == 0)
                            objLine[xpos] = pixelattr & 0x180000;
                    }
                }

                rotX += rotA;
                rotY += rotC;
                xoff++;
                xpos++;
            }
        }
        else
        {
            // 16-color
            if (!window)
            {
                pixelattr |= 0x1000;
                pixelattr |= ((attrib[2] & 0xF000) >> 8);
            }

            for (; xoff < boundwidth;)
            {
                if ((u32)rotX < width && (u32)rotY < height)
                {
                    color = objvram[(pixelsaddr + ((rotY>>11)*ytilefactor) + ((rotY&0x700)>>6) + ((rotX>>11)*32) + ((rotX&0x700)>>9)) & objvrammask];
                    if (rotX & 0x100)
                        color >>= 4;
                    else
                        color &= 0x0F;

                    if (color)
                    {
                        if (window) objWindow[xpos] = 1;
                        else        objLine[xpos] = color | pixelattr;
                    }
                    else if (!window)
                    {
                        if (objLine[xpos] == 0)
                            objLine[xpos] = pixelattr & 0x180000;
                    }
                }

                rotX += rotA;
                rotY += rotC;
                xoff++;
                xpos++;
            }
        }
    }
}

template<bool window>
void GLRenderer::DrawSprite_Normal(u32 num, u32 width, u32 height, s32 xpos, s32 ypos)
{
    u16* oam = (u16*)&GPU.OAM[CurUnit->Num ? 0x400 : 0];
    u16* attrib = &oam[num * 4];

    u32 pixelattr = ((attrib[2] & 0x0C00) << 6) | 0xC0000;
    u32 tilenum = attrib[2] & 0x03FF;
    u32 spritemode = window ? 0 : ((attrib[0] >> 10) & 0x3);

    u32 wmask = width - 8; // really ((width - 1) & ~0x7)

    if ((attrib[0] & 0x1000) && !window)
    {
        // apply Y mosaic
        pixelattr |= 0x100000;
    }

    u8* objvram;
    u32 objvrammask;
    CurUnit->GetOBJVRAM(objvram, objvrammask);

    u32* objLine = OBJLine[CurUnit->Num];
    u8* objWindow = OBJWindow[CurUnit->Num];

    // yflip
    if (attrib[1] & 0x2000)
        ypos = height-1 - ypos;

    u32 xoff;
    u32 xend = width;
    if (xpos >= 0)
    {
        xoff = 0;
        if ((xpos+xend) > 256)
            xend = 256-xpos;
    }
    else
    {
        xoff = -xpos;
        xpos = 0;
    }

    u16 color = 0; // transparent in all cases

    if (spritemode == 3)
    {
        // bitmap sprite

        u32 alpha = attrib[2] >> 12;
        if (!alpha) return;
        alpha++;

        pixelattr |= (0xC0000000 | (alpha << 24));

        u32 pixelsaddr = tilenum;
        if (CurUnit->DispCnt & 0x40)
        {
            if (CurUnit->DispCnt & 0x20)
            {
                // 'reserved'
                // draws nothing

                return;
            }
            else
            {
                pixelsaddr <<= (7 + ((CurUnit->DispCnt >> 22) & 0x1));
                pixelsaddr += (ypos * width * 2);
            }
        }
        else
        {
            if (CurUnit->DispCnt & 0x20)
            {
                pixelsaddr = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                pixelsaddr += (ypos * 256 * 2);
            }
            else
            {
                pixelsaddr = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                pixelsaddr += (ypos * 128 * 2);
            }
        }

        s32 pixelstride;

        if (attrib[1] & 0x1000) // xflip
        {
            pixelsaddr += ((width-1) << 1);
            pixelsaddr -= (xoff << 1);
            pixelstride = -2;
        }
        else
        {
            pixelsaddr += (xoff << 1);
            pixelstride = 2;
        }

        for (; xoff < xend;)
        {
            color = *(u16*)&objvram[pixelsaddr & objvrammask];

            pixelsaddr += pixelstride;

            if (color & 0x8000)
            {
                if (window) objWindow[xpos] = 1;
                else        objLine[xpos] = color | pixelattr;
            }
            else if (!window)
            {
                if (objLine[xpos] == 0)
                    objLine[xpos] = pixelattr & 0x180000;
            }

            xoff++;
            xpos++;
        }
    }
    else
    {
        u32 pixelsaddr = tilenum;
        if (CurUnit->DispCnt & 0x10)
        {
            pixelsaddr <<= ((CurUnit->DispCnt >> 20) & 0x3);
            pixelsaddr += ((ypos >> 3) * (width >> 3)) << ((attrib[0] & 0x2000) ? 1:0);
        }
        else
        {
            pixelsaddr += ((ypos >> 3) * 0x20);
        }

        if (spritemode == 1) pixelattr |= 0x80000000;
        else                 pixelattr |= 0x10000000;

        if (attrib[0] & 0x2000)
        {
            // 256-color
            pixelsaddr <<= 5;
            pixelsaddr += ((ypos & 0x7) << 3);
            s32 pixelstride;

            if (!window)
            {
                if (!(CurUnit->DispCnt & 0x80000000))
                    pixelattr |= 0x1000;
                else
                    pixelattr |= ((attrib[2] & 0xF000) >> 4);
            }

            if (attrib[1] & 0x1000) // xflip
            {
                pixelsaddr += (((width-1) & wmask) << 3);
                pixelsaddr += ((width-1) & 0x7);
                pixelsaddr -= ((xoff & wmask) << 3);
                pixelsaddr -= (xoff & 0x7);
                pixelstride = -1;
            }
            else
            {
                pixelsaddr += ((xoff & wmask) << 3);
                pixelsaddr += (xoff & 0x7);
                pixelstride = 1;
            }

            for (; xoff < xend;)
            {
                color = objvram[pixelsaddr & objvrammask];

                pixelsaddr += pixelstride;

                if (color)
                {
                    if (window) objWindow[xpos] = 1;
                    else        objLine[xpos] = color | pixelattr;
                }
                else if (!window)
                {
                    if (objLine[xpos] == 0)
                        objLine[xpos] = pixelattr & 0x180000;
                }

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr += (56 * pixelstride);
            }
        }
        else
        {
            // 16-color
            pixelsaddr <<= 5;
            pixelsaddr += ((ypos & 0x7) << 2);
            s32 pixelstride;

            if (!window)
            {
                pixelattr |= 0x1000;
                pixelattr |= ((attrib[2] & 0xF000) >> 8);
            }

            // TODO: optimize VRAM access!!
            // TODO: do xflip better? the 'two pixels per byte' thing makes it a bit shitty

            if (attrib[1] & 0x1000) // xflip
            {
                pixelsaddr += (((width-1) & wmask) << 2);
                pixelsaddr += (((width-1) & 0x7) >> 1);
                pixelsaddr -= ((xoff & wmask) << 2);
                pixelsaddr -= ((xoff & 0x7) >> 1);
                pixelstride = -1;
            }
            else
            {
                pixelsaddr += ((xoff & wmask) << 2);
                pixelsaddr += ((xoff & 0x7) >> 1);
                pixelstride = 1;
            }

            for (; xoff < xend;)
            {
                if (attrib[1] & 0x1000)
                {
                    if (xoff & 0x1) { color = objvram[pixelsaddr & objvrammask] & 0x0F; pixelsaddr--; }
                    else              color = objvram[pixelsaddr & objvrammask] >> 4;
                }
                else
                {
                    if (xoff & 0x1) { color = objvram[pixelsaddr & objvrammask] >> 4; pixelsaddr++; }
                    else              color = objvram[pixelsaddr & objvrammask] & 0x0F;
                }

                if (color)
                {
                    if (window) objWindow[xpos] = 1;
                    else        objLine[xpos] = color | pixelattr;
                }
                else if (!window)
                {
                    if (objLine[xpos] == 0)
                        objLine[xpos] = pixelattr & 0x180000;
                }

                xoff++;
                xpos++;
                if (!(xoff & 0x7)) pixelsaddr += ((attrib[1] & 0x1000) ? -28 : 28);
            }
        }
    }
}

}
