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

#pragma once

#include <memory>
#include <optional>
#include "OpenGLSupport.h"
#include "GPU2D.h"

namespace melonDS
{
class GLRenderer;

// sprite prerender vertex data: 2x position, 1x sprite index
constexpr int SpritePreVtxDataSize = (3 * 6) * 128;

// sprite vertex data: 2x position, 2x texcoord, 1x index
constexpr int SpriteVtxDataSize = (5 * 6) * 256;

class GLRenderer2D : public Renderer2D
{
public:
    GLRenderer2D(melonDS::GPU2D& gpu2D, GLRenderer& parent);
    ~GLRenderer2D() override = default;
    bool Init() override;
    void Reset() override;

    // OpenGL resources that are shared between the two 2D renderers.
    struct SharedResources
    {
        OpenGL::ProgramHandle LayerPreShader;
        GLint LayerPreCurBGULoc;
        OpenGL::ProgramHandle SpritePreShader;
        OpenGL::ProgramHandle SpriteShader;
        GLint SpriteRenderTransULoc;
        OpenGL::ProgramHandle CompositorShader;
        GLint CompositorScaleULoc;
        OpenGL::TextureHandle MosaicTex;
    };

    bool InitShaders(const SharedResources& shared);

    void PostSavestate();

    void SetScaleFactor(int scale);

    void DrawScanline(u32 line) override;
    void DrawSprites(u32 line) override;
    void VBlank() override;
    void VBlankEnd() override;

private:
    friend class GLRenderer;
    GLRenderer& Parent;

    int ScaleFactor;
    int ScreenW, ScreenH;

    // Non-owning reference to the shared LayerPreShader
    GLuint LayerPreShader;
    GLint LayerPreCurBGULoc;

    OpenGL::BufferHandle ScanlineConfigUBO;
    OpenGL::BufferHandle SpriteScanlineConfigUBO;

    // Non-owning reference to the shared SpritePreShader
    GLuint SpritePreShader;
    OpenGL::BufferHandle SpritePreVtxBuffer;
    OpenGL::VertexArrayHandle SpritePreVtxArray;
    std::array<u16, SpritePreVtxDataSize> SpritePreVtxData {};

    // Non-owning reference to the shared SpriteShader
    GLuint SpriteShader;
    GLint SpriteRenderTransULoc;
    OpenGL::BufferHandle SpriteVtxBuffer;
    OpenGL::VertexArrayHandle SpriteVtxArray;
    std::array<u16, SpriteVtxDataSize> SpriteVtxData {};

    // Non-owning reference to the shared CompositorShader
    GLuint CompositorShader;
    OpenGL::BufferHandle CompositorConfigUBO;
    GLint CompositorScaleULoc;

    // base index for a BG layer within the BG texture arrays
    // based on BG type and size
    const u8 BGBaseIndex[4][4] = {
        {2, 10, 6, 14},     // text mode
        {0, 4, 16, 20},     // rotscale
        {0, 4, 12, 16},     // bitmap
        {18, 19, 12, 16},   // large bitmap
    };

    OpenGL::BufferHandle LayerConfigUBO;
    OpenGL::BufferHandle SpriteConfigUBO;

    OpenGL::TextureHandle VRAMTex_BG;
    OpenGL::TextureHandle VRAMTex_OBJ;
    OpenGL::TextureHandle PalTex_BG;
    OpenGL::TextureHandle PalTex_OBJ;

    // Non-owning reference to the shared MosaicTex
    GLuint MosaicTex;

    OpenGL::FramebufferHandles<22> AllBGLayerFB;
    OpenGL::TextureHandles<22> AllBGLayerTex;

    OpenGL::FramebufferHandles<4> BGLayerFB;
    OpenGL::TextureHandles<4> BGLayerTex;

    OpenGL::FramebufferHandle SpriteFB;
    OpenGL::TextureHandle SpriteTex;

    OpenGL::FramebufferHandle OBJLayerFB;
    OpenGL::TextureHandle OBJLayerTex;
    OpenGL::TextureHandle OBJDepthTex;

    OpenGL::FramebufferHandle OutputFB;
    OpenGL::TextureHandle OutputTex;

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
    bool SpriteUseMosaic;

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
            u32 BGMosaicEnable[4];
            s32 MosaicSize[4];
        } uScanline[192];
    } ScanlineConfig;

    struct sSpriteScanlineConfig
    {
        s32 uMosaicLine[192];
    } SpriteScanlineConfig;

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

    bool UnitEnabled;

    u32 DispCnt;
    u8 LayerEnable;
    u8 OBJEnable;
    u8 ForcedBlank;
    u16 BGCnt[4];
    u16 BlendCnt;
    u8 EVA, EVB, EVY;

    u32 BGVRAMRange[4][4];

    bool LayerConfigDirty;

    int LastSpriteLine;
    u16 OAM[512];

    u32 SpriteDispCnt;
    bool SpriteConfigDirty;
    bool SpriteDirty;

    u16 TempPalBuffer[256 * (1 + (4*16))];

    bool IsScreenOn();

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
};

}
