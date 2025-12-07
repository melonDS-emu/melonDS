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

    GLuint RectVtxBuffer;
    GLuint RectVtxArray;

    GLuint LayerPreShader;
    GLint LayerPreCurBGULoc;
    GLuint LayerConfigUBO;

    GLuint LayerShader;
    GLint LayerScaleULoc;
    GLint LayerCurBGULoc;
    GLuint ScanlineConfigUBO;

    GLuint SpritePreShader;
    GLuint SpriteConfigUBO;
    GLuint SpritePreVtxBuffer;
    GLuint SpritePreVtxArray;
    u16* SpritePreVtxData;

    GLuint SpriteShader;
    GLuint SpriteVtxBuffer;
    GLuint SpriteVtxArray;
    u16* SpriteVtxData;

    GLuint CompositorShader;
    GLuint CompositorConfigUBO;
    GLint CompositorScaleULoc;

    struct sUnitState
    {
        GLuint VRAMTex_BG;
        GLuint VRAMTex_OBJ;
        GLuint PalTex_BG;
        GLuint PalTex_OBJ;

        GLuint BGLayerFB[4];
        GLuint BGLayerTex;

        GLuint SpriteFB;
        GLuint SpriteTex;

        GLuint FinalLayerFB[5];
        GLuint FinalLayerTex;

        GLuint OutputFB;
        GLuint OutputTex;

        // std140 compliant config struct for the layer shader
        struct sLayerConfig
        {
            u32 uVRAMMask;
            u32 __pad0[3];
            struct sBGConfig
            {
                u32 Size[2];
                u32 Type;
                u32 PalOffset;
                u32 TileOffset;
                u32 MapOffset;
                u32 Clamp;
                u32 __pad0[1];
            } uBGConfig[4];
        } LayerConfig;
        //GLuint LayerConfigUBO;

        struct sSpriteConfig
        {
            u32 uVRAMMask;
            u32 __pad0[3];
            s32 uRotscale[32][4];
            struct sOAM
            {
                s32 Position[2];
                s32 Flip[2];
                s32 Size[2];
                s32 BoundSize[2];
                u32 OBJMode;
                u32 Type;
                u32 PalOffset;
                u32 TileOffset;
                u32 TileStride;
                u32 Rotscale;
                u32 BGPrio;
                u32 Mosaic;
            } uOAM[128];
        } SpriteConfig;
        int NumSprites;

        struct sScanlineConfig
        {
            struct sScanline
            {
                s32 BGOffset[4][4];     // really [4][2]
                s32 BGRotscale[2][4];
                u32 BackColor;          // 96
                u32 WinRegs;            // 100
                u32 WinMask;            // 104
                u32 __pad0[1];
                s32 WinPos[4];
            } uScanline[192];
        } ScanlineConfig;

        struct sCompositorConfig
        {
            u32 uBGPrio[4];
            u32 uEnableOBJ;
            u32 uEnable3D;
            u32 uBlendCnt;
            u32 uBlendEffect;
            u32 uBlendCoef[4];
        } CompositorConfig;

        int LastSpriteLine;

        u16 OAM[512];

    } UnitState[2];

    struct sFinalPassConfig
    {
        u32 uScreenSwap[192];
        u32 uScaleFactor;
        u32 uAuxScaleFactor;
        u32 uDispModeA;
        u32 uDispModeB;
        u32 uBrightModeA;
        u32 uBrightModeB;
        u32 uBrightFactorA;
        u32 uBrightFactorB;
    } FinalPassConfig;

    u16 TempPalBuffer[256 * (1 + (4*16))];

    GLuint _3DLayerTex;

    GLuint FPShaderID = 0;
    /*GLint FPScaleULoc = 0;
    GLint FPCaptureRegULoc = 0;
    GLint FPCaptureMaskULoc = 0;
    GLint FPCaptureTexLoc[16] {};*/
    GLuint FPConfigUBO;

    GLuint FPVertexBufferID = 0;
    GLuint FPVertexArrayID = 0;

    //GLuint LineAttribTex = 0;               // per-scanline attribute texture
    //GLuint BGOBJTex = 0;                    // prerender of BG/OBJ layers
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

    void UpdateScanlineConfig(Unit* unit, int line);
    void UpdateLayerConfig(Unit* unit);
    void UpdateOAM(Unit* unit, int ystart, int yend);
    void UpdateCompositorConfig(Unit* unit);

    void PrerenderSprites(Unit* unit);
    void PrerenderLayer(Unit* unit, int layer);

    void RenderSprites(Unit* unit, bool window, int ystart, int yend);
    void RenderLayer(Unit* unit, int layer, int ystart, int yend);

    void RenderScreen(Unit* unit, int ystart, int yend);

    void DoCapture(u32 line, u32 width);
};

}
}
