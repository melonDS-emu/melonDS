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
class GLRenderer;

class GLRenderer2D : public Renderer2D
{
public:
    //static std::unique_ptr<GLRenderer> New(melonDS::GPU& gpu) noexcept;
    GLRenderer2D(melonDS::GPU2D& gpu2D, GLRenderer& parent);
    ~GLRenderer2D() override;
    bool Init() override;
    void Reset() override;

    void SetScaleFactor(int scale);

    void DrawScanline(u32 line) override;
    void DrawSprites(u32 line) override;
    void VBlank() override;
    void VBlankEnd() override;

private:
    GLRenderer& Parent;

    int ScaleFactor;
    int ScreenW, ScreenH;

    static int ShaderCount;

    GLuint RectVtxBuffer;
    GLuint RectVtxArray;

    static GLuint LayerPreShader;
    static GLint LayerPreCurBGULoc;

    GLuint ScanlineConfigUBO;

    static GLuint SpritePreShader;
    GLuint SpritePreVtxBuffer;
    GLuint SpritePreVtxArray;
    u16* SpritePreVtxData;

    static GLuint SpriteShader;
    GLuint SpriteVtxBuffer;
    GLuint SpriteVtxArray;
    u16* SpriteVtxData;

    static GLuint CompositorShader;
    GLuint CompositorConfigUBO;
    static GLint CompositorScaleULoc;

    // base index for a BG layer within the BG texture arrays
    // based on BG type and size
    const u8 BGBaseIndex[4][4] = {
        {2, 10, 6, 14},     // text mode
        {0, 4, 16, 20},     // rotscale
        {0, 4, 12, 16},     // bitmap
        {18, 19, 12, 16},   // large bitmap
    };

    GLuint LayerConfigUBO;
    GLuint SpriteConfigUBO;

    GLuint VRAMTex_BG;
    GLuint VRAMTex_OBJ;
    GLuint PalTex_BG;
    GLuint PalTex_OBJ;

    GLuint AllBGLayerFB[22];
    GLuint AllBGLayerTex[22];

    GLuint BGLayerFB[4];
    GLuint BGLayerTex[4];

    GLuint SpriteFB;
    GLuint SpriteTex;

    GLuint OBJLayerFB;
    GLuint OBJLayerTex;

    GLuint OutputFB;
    GLuint OutputTex;

    // std140 compliant config struct for the layer shader
    struct sLayerConfig
    {
        u32 uVRAMMask;
        u32 __pad0[3];
        u32 uCaptureMask[32];
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
        u32 uScaleFactor;
        u32 uVRAMMask;
        u32 __pad0[2];
        u32 uCaptureMask[32];
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

    int LastLine;

    u32 DispCnt;
    u16 BGCnt[4];
    u16 BlendCnt;
    u8 EVA, EVB, EVY;

    u32 BGVRAMRange[4][4];

    int LastSpriteLine;
    u16 OAM[512];

    u32 SpriteDispCnt;
    bool SpriteDirty;

    /*struct sFinalPassConfig
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
    } FinalPassConfig;*/

    u16 TempPalBuffer[256 * (1 + (4*16))];

    //GLuint _3DLayerTex;

    //GLuint FPShaderID = 0;
    /*GLint FPScaleULoc = 0;
    GLint FPCaptureRegULoc = 0;
    GLint FPCaptureMaskULoc = 0;
    GLint FPCaptureTexLoc[16] {};*/
    //GLuint FPConfigUBO;

    //GLuint FPVertexBufferID = 0;
    //GLuint FPVertexArrayID = 0;

    //GLuint LineAttribTex = 0;               // per-scanline attribute texture
    //GLuint BGOBJTex = 0;                    // prerender of BG/OBJ layers
    //GLuint AuxInputTex = 0;                 // aux input (VRAM and mainmem FIFO)

    // texture/fb for display capture VRAM input
    /*GLuint CaptureVRAMTex;
    GLuint CaptureVRAMFB;

    GLuint FPOutputTex[2];               // final output
    GLuint FPOutputFB[2];

    struct sCaptureConfig
    {
        u32 uCaptureSize[2];
        u32 uScaleFactor;
        u32 uSrcAOffset;
        u32 uSrcBLayer;
        u32 uSrcBOffset;
        u32 uDstOffset;
        u32 uDstMode;
        u32 uBlendFactors[2];
        u32 __pad0[2];
    } CaptureConfig;

    GLuint CaptureShader;
    GLuint CaptureConfigUBO;

    GLuint CaptureVtxBuffer;
    GLuint CaptureVtxArray;

    GLuint CaptureOutput256FB[4];
    GLuint CaptureOutput256Tex;
    GLuint CaptureOutput128FB[16];
    GLuint CaptureOutput128Tex;

    GLuint CaptureSyncFB;
    GLuint CaptureSyncTex;

    u16* AuxInputBuffer[2];

    int BackBuffer;

    u8 AuxUsageMask;*/

    /*u8* CurBGXMosaicTable;
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
    }();*/

    void UpdateAndRender(int line);

    void UpdateScanlineConfig(int line);
    void UpdateLayerConfig();
    void UpdateOAM(int ystart, int yend);
    void UpdateCompositorConfig();

    void PrerenderSprites();
    void PrerenderLayer(int layer);

    void DoRenderSprites(int line);
    void RenderSprites(bool window, int ystart, int yend);

    void RenderScreen(int ystart, int yend);

    void DoCapture(int vramcap);
};

}
