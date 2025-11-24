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

#pragma once

#include <memory>
#include <optional>
#include "OpenGLSupport.h"
#include "GPU2D.h"

namespace melonDS
{
class GPU;

namespace GPU2D
{

class GLRenderer : public Renderer2D
{
public:
    static std::unique_ptr<GLRenderer> New(melonDS::GPU& gpu) noexcept;
    ~GLRenderer() override;

    void SetScaleFactor(int scale);

    void DrawScanline(u32 line, Unit* unit) override;
    void DrawSprites(u32 line, Unit* unit) override;
    void VBlank(Unit* unitA, Unit* unitB) override;
    void VBlankEnd(Unit* unitA, Unit* unitB) override;

    void AllocCapture(u32 bank, u32 start, u32 len) override;
    void SyncVRAMCapture(u32 bank, u32 start, u32 len, bool complete) override;

    bool GetFramebuffers(u32** top, u32** bottom) override;
    void SwapBuffers() override;

private:
    GLRenderer(melonDS::GPU& gpu);
    bool GLInit();
    melonDS::GPU& GPU;

    int ScaleFactor;
    int ScreenW, ScreenH;

    GLuint VRAM_ABG_Tex = 0;
    GLuint VRAM_AOBJ_Tex = 0;
    GLuint VRAM_BBG_Tex = 0;
    GLuint VRAM_BOBJ_Tex = 0;

    GLuint FPShaderID = 0;
    GLint FPScaleULoc = 0;
    GLint FPCaptureRegULoc = 0;
    GLint FPCaptureMaskULoc = 0;
    GLint FPCaptureTexLoc[16] {};

    GLuint FPVertexBufferID = 0;
    GLuint FPVertexArrayID = 0;

    GLuint LineAttribTex = 0;               // per-scanline attribute texture
    GLuint BGOBJTex = 0;                    // prerender of BG/OBJ layers
    GLuint AuxInputTex = 0;                 // aux input (VRAM and mainmem FIFO)

    // hi-res capture buffers
    // since the DS can read from and capture to the same VRAM bank (VRAM display + capture),
    // these need to be double-buffered
    struct sCaptureBuffer
    {
        GLuint Texture;
        u16 Width, Height;
        //int Length;
        bool Complete;
    } CaptureBuffers[16][2];
    int CaptureLastBuffer[16];
    int ActiveCapture;

    u16 CaptureUsageMask;

    //
    //std::array<GLuint, 2> FPOutputTex {};  // final output
    //std::array<GLuint, 2> FPOutputFB {};
    GLuint FPOutputTex[2][2];               // final output
    GLuint FPOutputFB[2];

    //GLuint test;

    u32* LineAttribBuffer;
    u32* BGOBJBuffer;
    u16* AuxInputBuffer[2];

    //u32* Framebuffer[2][2];
    int BackBuffer;

    u32* BGOBJLine;
    // REMOVEME
    //alignas(8) u32 BGOBJLine[256*3];
    u32* _3DLine;

    u8 AuxUsageMask;

    alignas(8) u8 WindowMask[256];

    alignas(8) u32 OBJLine[2][256];
    alignas(8) u8 OBJWindow[2][256];

    u32 NumSprites[2];

    u8* CurBGXMosaicTable;
    array2d<u8, 16, 256> MosaicTable = []() constexpr
    {
        array2d<u8, 16, 256> table {};
        // initialize mosaic table
        for (int m = 0; m < 16; m++)
        {
            for (int x = 0; x < 256; x++)
            {
                int offset = x % (m+1);
                table[m][x] = offset;
            }
        }

        return table;
    }();

    template<u32 bgmode> void DrawScanlineBGMode(u32 line);
    void DrawScanlineBGMode6(u32 line);
    void DrawScanlineBGMode7(u32 line);
    void DrawScanline_BGOBJ(u32 line);

    static void DrawPixel(u32* dst, u32 color, u32 flag);

    void DrawBG_3D();
    template<bool mosaic> void DrawBG_Text(u32 line, u32 bgnum);
    template<bool mosaic> void DrawBG_Affine(u32 line, u32 bgnum);
    template<bool mosaic> void DrawBG_Extended(u32 line, u32 bgnum);
    template<bool mosaic> void DrawBG_Large(u32 line);

    void ApplySpriteMosaicX();
    void InterleaveSprites(u32 prio);
    template<bool window> void DrawSprite_Rotscale(u32 num, u32 boundwidth, u32 boundheight, u32 width, u32 height, s32 xpos, s32 ypos);
    template<bool window> void DrawSprite_Normal(u32 num, u32 width, u32 height, s32 xpos, s32 ypos);

    void DoCapture(u32 line, u32 width);
};

}
}
