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
#include "GPU_OpenGL.h"
#include "GPU2D_OpenGL.h"
#include "GPU.h"
#include "GPU3D.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

#include "OpenGL_shaders/2DLayerPreVS.h"
#include "OpenGL_shaders/2DLayerPreFS.h"
#include "OpenGL_shaders/2DSpritePreVS.h"
#include "OpenGL_shaders/2DSpritePreFS.h"
#include "OpenGL_shaders/2DSpriteVS.h"
#include "OpenGL_shaders/2DSpriteFS.h"
#include "OpenGL_shaders/2DCompositorVS.h"
#include "OpenGL_shaders/2DCompositorFS.h"


int GLRenderer2D::ShaderCount = 0;
GLuint GLRenderer2D::LayerPreShader = 0;
GLint GLRenderer2D::LayerPreCurBGULoc = 0;
GLuint GLRenderer2D::SpritePreShader = 0;
GLuint GLRenderer2D::SpriteShader = 0;
GLint GLRenderer2D::SpriteRenderTransULoc = 0;
GLuint GLRenderer2D::CompositorShader = 0;
GLint GLRenderer2D::CompositorScaleULoc = 0;
GLuint GLRenderer2D::MosaicTex = 0;


GLRenderer2D::GLRenderer2D(melonDS::GPU2D& gpu2D, GLRenderer& parent)
    : Renderer2D(gpu2D), Parent(parent)
{
    ScaleFactor = 0;
}

#define glDefaultTexParams(target) \
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); \
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); \
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST); \
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

bool GLRenderer2D::Init()
{
    GLint uniloc;

    if (ShaderCount++ == 0)
    {
        // compile shaders

        if (!OpenGL::CompileVertexFragmentProgram(LayerPreShader,
                                                  k2DLayerPreVS, k2DLayerPreFS,
                                                  "2DLayerPreShader",
                                                  {{"vPosition", 0}},
                                                  {{"oColor", 0}}))
            return false;

        if (!OpenGL::CompileVertexFragmentProgram(SpritePreShader,
                                                  k2DSpritePreVS, k2DSpritePreFS,
                                                  "2DSpritePreShader",
                                                  {{"vPosition", 0}, {"vSpriteIndex", 1}},
                                                  {{"oColor", 0}}))
            return false;

        if (!OpenGL::CompileVertexFragmentProgram(SpriteShader,
                                                  k2DSpriteVS, k2DSpriteFS,
                                                  "2DSpriteShader",
                                                  {{"vPosition", 0}, {"vTexcoord", 1}, {"vSpriteIndex", 2}},
                                                  {{"oColor", 0}, {"oFlags", 1}}))
            return false;

        if (!OpenGL::CompileVertexFragmentProgram(CompositorShader,
                                                  k2DCompositorVS, k2DCompositorFS,
                                                  "2DCompositorShader",
                                                  {{"vPosition", 0}},
                                                  {{"oColor", 0}}))
            return false;

        // set up uniforms

        glUseProgram(LayerPreShader);

        uniloc = glGetUniformLocation(LayerPreShader, "VRAMTex");
        glUniform1i(uniloc, 0);
        uniloc = glGetUniformLocation(LayerPreShader, "PalTex");
        glUniform1i(uniloc, 1);

        uniloc = glGetUniformBlockIndex(LayerPreShader, "ubBGConfig");
        glUniformBlockBinding(LayerPreShader, uniloc, 20);

        LayerPreCurBGULoc = glGetUniformLocation(LayerPreShader, "uCurBG");


        glUseProgram(SpritePreShader);

        uniloc = glGetUniformLocation(SpritePreShader, "VRAMTex");
        glUniform1i(uniloc, 0);
        uniloc = glGetUniformLocation(SpritePreShader, "PalTex");
        glUniform1i(uniloc, 1);

        uniloc = glGetUniformBlockIndex(SpritePreShader, "ubSpriteConfig");
        glUniformBlockBinding(SpritePreShader, uniloc, 21);


        glUseProgram(SpriteShader);

        uniloc = glGetUniformLocation(SpriteShader, "SpriteTex");
        glUniform1i(uniloc, 0);
        uniloc = glGetUniformLocation(SpriteShader, "Capture128Tex");
        glUniform1i(uniloc, 1);
        uniloc = glGetUniformLocation(SpriteShader, "Capture256Tex");
        glUniform1i(uniloc, 2);

        uniloc = glGetUniformBlockIndex(SpriteShader, "ubSpriteConfig");
        glUniformBlockBinding(SpriteShader, uniloc, 21);
        uniloc = glGetUniformBlockIndex(SpriteShader, "ubSpriteScanlineConfig");
        glUniformBlockBinding(SpriteShader, uniloc, 24);

        SpriteRenderTransULoc = glGetUniformLocation(SpriteShader, "uRenderTransparent");


        glUseProgram(CompositorShader);

        uniloc = glGetUniformLocation(CompositorShader, "BGLayerTex[0]");
        glUniform1i(uniloc, 0);
        uniloc = glGetUniformLocation(CompositorShader, "BGLayerTex[1]");
        glUniform1i(uniloc, 1);
        uniloc = glGetUniformLocation(CompositorShader, "BGLayerTex[2]");
        glUniform1i(uniloc, 2);
        uniloc = glGetUniformLocation(CompositorShader, "BGLayerTex[3]");
        glUniform1i(uniloc, 3);
        uniloc = glGetUniformLocation(CompositorShader, "OBJLayerTex");
        glUniform1i(uniloc, 4);
        uniloc = glGetUniformLocation(CompositorShader, "Capture128Tex");
        glUniform1i(uniloc, 5);
        uniloc = glGetUniformLocation(CompositorShader, "Capture256Tex");
        glUniform1i(uniloc, 6);
        uniloc = glGetUniformLocation(CompositorShader, "MosaicTex");
        glUniform1i(uniloc, 7);

        uniloc = glGetUniformBlockIndex(CompositorShader, "ubBGConfig");
        glUniformBlockBinding(CompositorShader, uniloc, 20);
        uniloc = glGetUniformBlockIndex(CompositorShader, "ubScanlineConfig");
        glUniformBlockBinding(CompositorShader, uniloc, 22);
        uniloc = glGetUniformBlockIndex(CompositorShader, "ubCompositorConfig");
        glUniformBlockBinding(CompositorShader, uniloc, 23);

        CompositorScaleULoc = glGetUniformLocation(CompositorShader, "uScaleFactor");

        // generate mosaic lookup texture

        u8* mosaic_tex = new u8[256 * 16];
        for (int m = 0; m < 16; m++)
        {
            int mosx = 0;
            for (int x = 0; x < 256; x++)
            {
                mosaic_tex[(m * 256) + x] = mosx;

                if (mosx == m)
                    mosx = 0;
                else
                    mosx++;
            }
        }

        glGenTextures(1, &MosaicTex);
        glBindTexture(GL_TEXTURE_2D, MosaicTex);
        glDefaultTexParams(GL_TEXTURE_2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8I, 256, 16, 0, GL_RED_INTEGER, GL_BYTE, mosaic_tex);

        delete[] mosaic_tex;
    }

    // sprite prerender vertex data: 2x position, 1x sprite index
    int sprdatasize = (3 * 6) * 128;
    SpritePreVtxData = new u16[sprdatasize];

    glGenBuffers(1, &SpritePreVtxBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, SpritePreVtxBuffer);
    glBufferData(GL_ARRAY_BUFFER, sprdatasize * sizeof(u16), nullptr, GL_STREAM_DRAW);

    glGenVertexArrays(1, &SpritePreVtxArray);
    glBindVertexArray(SpritePreVtxArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribIPointer(0, 2, GL_SHORT, 3 * sizeof(u16), (void*)0);
    glEnableVertexAttribArray(1); // sprite index
    glVertexAttribIPointer(1, 1, GL_SHORT, 3 * sizeof(u16), (void*)(2 * sizeof(u16)));

    // sprite vertex data: 2x position, 2x texcoord, 1x index
    sprdatasize = (5 * 6) * 256;
    SpriteVtxData = new u16[sprdatasize];

    glGenBuffers(1, &SpriteVtxBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, SpriteVtxBuffer);
    glBufferData(GL_ARRAY_BUFFER, sprdatasize * sizeof(u16), nullptr, GL_STREAM_DRAW);

    glGenVertexArrays(1, &SpriteVtxArray);
    glBindVertexArray(SpriteVtxArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribIPointer(0, 2, GL_SHORT, 5 * sizeof(u16), (void*)0);
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribIPointer(1, 2, GL_SHORT, 5 * sizeof(u16), (void*)(2 * sizeof(u16)));
    glEnableVertexAttribArray(2); // sprite index
    glVertexAttribIPointer(2, 1, GL_SHORT, 5 * sizeof(u16), (void*)(4 * sizeof(u16)));

    // generate textures to hold raw BG and OBJ VRAM and palettes

    int bgheight = (GPU2D.Num == 0) ? 512 : 128;
    int objheight = (GPU2D.Num == 0) ? 256 : 128;

    glGenTextures(1, &VRAMTex_BG);
    glBindTexture(GL_TEXTURE_2D, VRAMTex_BG);
    glDefaultTexParams(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 1024, bgheight, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &VRAMTex_OBJ);
    glBindTexture(GL_TEXTURE_2D, VRAMTex_OBJ);
    glDefaultTexParams(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 1024, objheight, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &PalTex_BG);
    glBindTexture(GL_TEXTURE_2D, PalTex_BG);
    glDefaultTexParams(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 1+(4*16), 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);

    glGenTextures(1, &PalTex_OBJ);
    glBindTexture(GL_TEXTURE_2D, PalTex_OBJ);
    glDefaultTexParams(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 1+16, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);

    // generate texture to hold pre-rendered BG layers

    glGenTextures(22, AllBGLayerTex);
    glGenFramebuffers(22, AllBGLayerFB);

    const u16 bgsizes[8][3] = {
        {128, 128, 2},
        {256, 256, 4},
        {256, 512, 4},
        {512, 256, 4},
        {512, 512, 4},
        {512, 1024, 1},
        {1024, 512, 1},
        {1024, 1024, 2}
    };

    int l = 0;
    for (int j = 0; j < 8; j++)
    {
        const u16* sz = bgsizes[j];

        for (int k = 0; k < sz[2]; k++)
        {
            glBindTexture(GL_TEXTURE_2D, AllBGLayerTex[l]);
            glDefaultTexParams(GL_TEXTURE_2D);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz[0], sz[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            glBindFramebuffer(GL_FRAMEBUFFER, AllBGLayerFB[l]);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, AllBGLayerTex[l], 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);

            l++;
        }
    }

    // generate texture to hold pre-rendered sprites

    glGenTextures(1, &SpriteTex);
    glBindTexture(GL_TEXTURE_2D, SpriteTex);
    glDefaultTexParams(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &SpriteFB);
    glBindFramebuffer(GL_FRAMEBUFFER, SpriteFB);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, SpriteTex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    // generate texture to hold final (upscaled) sprites

    glGenTextures(1, &OBJLayerTex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, OBJLayerTex);
    glDefaultTexParams(GL_TEXTURE_2D_ARRAY);

    glGenTextures(1, &OBJDepthTex);
    glBindTexture(GL_TEXTURE_2D, OBJDepthTex);
    glDefaultTexParams(GL_TEXTURE_2D);

    glGenFramebuffers(1, &OBJLayerFB);

    // generate texture for the compositor output

    glGenTextures(1, &OutputTex);
    glBindTexture(GL_TEXTURE_2D, OutputTex);
    glDefaultTexParams(GL_TEXTURE_2D);

    glGenFramebuffers(1, &OutputFB);

    Parent.OutputTex2D[GPU2D.Num] = OutputTex;

    // generate UBOs

    glGenBuffers(1, &LayerConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, LayerConfigUBO);
    static_assert((sizeof(sLayerConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sLayerConfig), nullptr, GL_STREAM_DRAW);

    glGenBuffers(1, &SpriteConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, SpriteConfigUBO);
    static_assert((sizeof(sSpriteConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sSpriteConfig), nullptr, GL_STREAM_DRAW);

    glGenBuffers(1, &ScanlineConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, ScanlineConfigUBO);
    static_assert((sizeof(sScanlineConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sScanlineConfig), nullptr, GL_STREAM_DRAW);

    glGenBuffers(1, &SpriteScanlineConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, SpriteScanlineConfigUBO);
    static_assert((sizeof(sSpriteScanlineConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sSpriteScanlineConfig), nullptr, GL_STREAM_DRAW);

    glGenBuffers(1, &CompositorConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, CompositorConfigUBO);
    static_assert((sizeof(sCompositorConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sCompositorConfig), nullptr, GL_STREAM_DRAW);

    return true;
}

GLRenderer2D::~GLRenderer2D()
{
    if (--ShaderCount == 0)
    {
        glDeleteProgram(LayerPreShader);
        glDeleteProgram(SpritePreShader);
        glDeleteProgram(SpriteShader);
        glDeleteProgram(CompositorShader);

        glDeleteTextures(1, &MosaicTex);
    }

    glDeleteBuffers(1, &SpritePreVtxBuffer);
    glDeleteVertexArrays(1, &SpritePreVtxArray);

    glDeleteBuffers(1, &SpriteVtxBuffer);
    glDeleteVertexArrays(1, &SpriteVtxArray);

    glDeleteBuffers(1, &LayerConfigUBO);
    glDeleteBuffers(1, &SpriteConfigUBO);

    glDeleteTextures(1, &VRAMTex_BG);
    glDeleteTextures(1, &VRAMTex_OBJ);
    glDeleteTextures(1, &PalTex_BG);
    glDeleteTextures(1, &PalTex_OBJ);

    glDeleteTextures(22, AllBGLayerTex);
    glDeleteFramebuffers(22, AllBGLayerFB);

    glDeleteTextures(1, &SpriteTex);
    glDeleteFramebuffers(1, &SpriteFB);

    glDeleteTextures(1, &OBJLayerTex);
    glDeleteTextures(1, &OBJDepthTex);
    glDeleteFramebuffers(1, &OBJLayerFB);

    glDeleteTextures(1, &OutputTex);
    glDeleteFramebuffers(1, &OutputFB);

    glDeleteBuffers(1, &ScanlineConfigUBO);
    glDeleteBuffers(1, &SpriteScanlineConfigUBO);
    glDeleteBuffers(1, &CompositorConfigUBO);
}

void GLRenderer2D::Reset()
{
    memset(BGLayerFB, 0, sizeof(BGLayerFB));
    memset(BGLayerTex, 0, sizeof(BGLayerTex));

    memset(&LayerConfig, 0, sizeof(LayerConfig));
    memset(&SpriteConfig, 0, sizeof(SpriteConfig));
    memset(&ScanlineConfig, 0, sizeof(ScanlineConfig));
    memset(&SpriteScanlineConfig, 0, sizeof(SpriteScanlineConfig));
    memset(&CompositorConfig, 0, sizeof(CompositorConfig));

    int bgheight = (GPU2D.Num == 0) ? 512 : 128;
    int objheight = (GPU2D.Num == 0) ? 256 : 128;
    LayerConfig.uVRAMMask = bgheight - 1;
    SpriteConfig.uVRAMMask = objheight - 1;

    LastLine = 0;

    UnitEnabled = false;

    DispCnt = 0;
    LayerEnable = 0;
    OBJEnable = 0;
    ForcedBlank = 0;
    memset(BGCnt, 0, sizeof(BGCnt));
    BlendCnt = 0;
    EVA = 0; EVB = 0; EVY = 0;

    memset(BGVRAMRange, 0xFF, sizeof(BGVRAMRange));

    LayerConfigDirty = true;

    LastSpriteLine = 0;
    memset(OAM, 0, sizeof(OAM));
    NumSprites = 0;
    SpriteUseMosaic = false;

    SpriteDispCnt = 0;
    SpriteConfigDirty = true;
    SpriteDirty = true;

    memset(TempPalBuffer, 0, sizeof(TempPalBuffer));
}

void GLRenderer2D::PostSavestate()
{
    Reset();
}


void GLRenderer2D::SetScaleFactor(int scale)
{
    if (scale == ScaleFactor)
        return;

    ScaleFactor = scale;
    ScreenW = 256 * scale;
    ScreenH = 192 * scale;

    const GLenum fbassign2[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glUseProgram(CompositorShader);
    glUniform1i(CompositorScaleULoc, ScaleFactor);

    glBindTexture(GL_TEXTURE_2D_ARRAY, OBJLayerTex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, ScreenW, ScreenH, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, OBJDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, ScreenW, ScreenH, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, OBJLayerFB);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, OBJLayerTex, 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, OBJLayerTex, 0, 1);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, OBJDepthTex, 0);
    glDrawBuffers(2, fbassign2);

    glBindTexture(GL_TEXTURE_2D, OutputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, OutputFB);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, OutputTex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
}


bool GLRenderer2D::IsScreenOn()
{
    if (!GPU.ScreensEnabled) return false;
    if (!GPU2D.Enabled) return false;
    if (GPU2D.ForcedBlank) return false;

    u16 masterbright = GPU2D.Num ? GPU.MasterBrightnessB : GPU.MasterBrightnessA;
    u16 brightmode = masterbright >> 14;
    u16 brightness = masterbright & 0x1F;
    if ((brightmode == 1 || brightmode == 2) && brightness >= 16)
        return false;

    u16 layers = GPU2D.LayerEnable | 0x20;
    u16 bldeffect = (GPU2D.BlendCnt >> 6) & 0x3;
    u16 bldlayers = GPU2D.BlendCnt & layers & 0x3F;
    if ((bldeffect == 2 || bldeffect == 3) && bldlayers == layers && GPU2D.EVY >= 16 &&
        !(GPU2D.DispCnt & 0xE000))
        return false;

    u32 dispmode = (GPU2D.DispCnt >> 16) & 0x3;
    if (dispmode != 1)
    {
        if (GPU2D.Num) return false;
        if (!GPU.CaptureEnable) return false;
    }

    return true;
}


void GLRenderer2D::UpdateAndRender(int line)
{
    u32 palmask = 1 << (GPU2D.Num * 2);

    // check if any 'critical' registers were modified

    u32 dispcnt_diff;
    u8 layer_diff;
    u16 bgcnt_diff[4];

    dispcnt_diff = GPU2D.DispCnt ^ DispCnt;
    layer_diff = GPU2D.LayerEnable ^ LayerEnable;
    for (int layer = 0; layer < 4; layer++)
        bgcnt_diff[layer] = GPU2D.BGCnt[layer] ^ BGCnt[layer];

    u8 layer_pre_dirty = 0;
    bool comp_dirty = false;
    bool screenon = IsScreenOn();

    if (dispcnt_diff & 0x8)
        layer_pre_dirty |= 0x1;
    if (dispcnt_diff & 0x7)
        layer_pre_dirty |= 0xC;
    if (dispcnt_diff & 0x7F000000)
        layer_pre_dirty |= 0xF;

    if (dispcnt_diff & 0x0000E008)
        comp_dirty = true;
    else if (layer_diff & 0x1F)
        comp_dirty = true;
    else if (UnitEnabled != GPU2D.Enabled)
        comp_dirty = true;
    else if (ForcedBlank != GPU2D.ForcedBlank)
        comp_dirty = true;

    for (int layer = 0; layer < 4; layer++)
    {
        u16 mask = 0xDFBC;
        if (layer < 2) mask |= (1 << 13);
        if (bgcnt_diff[layer] & mask)
            layer_pre_dirty |= (1 << layer);
        if (bgcnt_diff[layer] & (~mask))
            comp_dirty = true;
    }

    if ((GPU2D.BlendCnt != BlendCnt) ||
        (GPU2D.EVA != EVA) ||
        (GPU2D.EVB != EVB) ||
        (GPU2D.EVY != EVY))
        comp_dirty = true;

    // check if VRAM was modified, and flatten it as needed

    static_assert(VRAMDirtyGranularity == 512);
    NonStupidBitField<1024> bgDirty;
    NonStupidBitField<64> bgExtPalDirty;
    NonStupidBitField<16> objExtPalDirty;

    if (screenon)
    {
        if (GPU2D.Num == 0)
        {
            bgDirty = GPU.VRAMDirty_ABG.DeriveState(GPU.VRAMMap_ABG, GPU);
            GPU.MakeVRAMFlat_ABGCoherent(bgDirty);

            bgExtPalDirty = GPU.VRAMDirty_ABGExtPal.DeriveState(GPU.VRAMMap_ABGExtPal, GPU);
            GPU.MakeVRAMFlat_ABGExtPalCoherent(bgExtPalDirty);
            objExtPalDirty = GPU.VRAMDirty_AOBJExtPal.DeriveState(&GPU.VRAMMap_AOBJExtPal, GPU);
            GPU.MakeVRAMFlat_AOBJExtPalCoherent(objExtPalDirty);
        }
        else
        {
            auto _bgDirty = GPU.VRAMDirty_BBG.DeriveState(GPU.VRAMMap_BBG, GPU);
            GPU.MakeVRAMFlat_BBGCoherent(_bgDirty);
            for (int i = 0; i < 1024; i += 256)
                memcpy(&bgDirty.Data[i>>6], _bgDirty.Data, 256>>3);

            bgExtPalDirty = GPU.VRAMDirty_BBGExtPal.DeriveState(GPU.VRAMMap_BBGExtPal, GPU);
            GPU.MakeVRAMFlat_BBGExtPalCoherent(bgExtPalDirty);
            objExtPalDirty = GPU.VRAMDirty_BOBJExtPal.DeriveState(&GPU.VRAMMap_BOBJExtPal, GPU);
            GPU.MakeVRAMFlat_BOBJExtPalCoherent(objExtPalDirty);
        }
    }

    // for each layer, check if the VRAM and palettes involved are dirty

    for (int layer = 0; layer < 4; layer++)
    {
        const u32* rangeinfo = BGVRAMRange[layer];

        // to consider: only check the tileset range that is actually used
        // (would require parsing the tilemap)
        for (int r = 0; r < 4; r+=2)
        {
            if (rangeinfo[r] == 0xFFFFFFFF)
                continue;

            bool dirty = false;
            u32 rstart = (rangeinfo[r] >> 9) & 0x3FF;
            u32 rcount = (rangeinfo[r+1] >> 9);
            if ((rstart + rcount) > 1024)
            {
                dirty = bgDirty.CheckRange(rstart, 1024-rstart) ||
                        bgDirty.CheckRange(0, rcount-(1024-rstart));
            }
            else
                dirty = bgDirty.CheckRange(rstart, rcount);

            if (dirty)
                layer_pre_dirty |= (1 << layer);
        }

        auto& cfg = LayerConfig.uBGConfig[layer];
        if ((cfg.Type == 1 || cfg.Type == 3) && (cfg.PalOffset > 0))
        {
            u32 pal = cfg.PalOffset - 1;
            if (bgExtPalDirty.CheckRange(pal, pal + 16))
                layer_pre_dirty |= (1 << layer);
        }
        else if (cfg.Type <= 4)
        {
            if (GPU.PaletteDirty & palmask)
                layer_pre_dirty |= (1 << layer);
        }
    }

    if (layer_pre_dirty)
        comp_dirty = true;

    if (Parent.NeedPartialRender)
        comp_dirty = true;

    // if needed, render sprites

    if ((comp_dirty || SpriteDirty) && (line > 0))
    {
        DoRenderSprites(line);
    }

    // if needed, composite the previous screen section

    if (comp_dirty && (line > 0))
    {
        RenderScreen(LastLine, line);
        LastLine = line;
    }

    // update registers

    UnitEnabled = GPU2D.Enabled;
    DispCnt = GPU2D.DispCnt;
    LayerEnable = GPU2D.LayerEnable;
    OBJEnable = GPU2D.OBJEnable;
    ForcedBlank = GPU2D.ForcedBlank;
    for (int layer = 0; layer < 4; layer++)
        BGCnt[layer] = GPU2D.BGCnt[layer];
    BlendCnt = GPU2D.BlendCnt;
    EVA = GPU2D.EVA;
    EVB = GPU2D.EVB;
    EVY = GPU2D.EVY;

    if (layer_pre_dirty || LayerConfigDirty)
        UpdateLayerConfig();

    UpdateScanlineConfig(line);

    // update VRAM and palettes

    int dirtybits = GPU2D.Num ? 256 : 1024;
    if (bgDirty.CheckRange(0, dirtybits))
    {
        // TODO: only do it for active layers?
        // this would require keeping track of the dirty state for areas not included in any layer

        u8 *vram;
        u32 vrammask;
        GPU2D.GetBGVRAM(vram, vrammask);

        glBindTexture(GL_TEXTURE_2D, VRAMTex_BG);

        int texlen = dirtybits >> 6;
        for (int i = 0; i < texlen; )
        {
            if (!bgDirty.Data[i])
            {
                i++;
                continue;
            }

            int start = i * 32;
            for (;;)
            {
                i++;
                if (i >= texlen) break;
                if (!bgDirty.Data[i]) break;
            }
            int end = i * 32;

            glTexSubImage2D(GL_TEXTURE_2D, 0,
                            0, start,
                            1024, end - start,
                            GL_RED_INTEGER, GL_UNSIGNED_BYTE,
                            &vram[start * 1024]);
        }
    }

    if ((GPU.PaletteDirty & palmask) || bgExtPalDirty.CheckRange(0, 64))
    {
        memcpy(&TempPalBuffer[0], &GPU.Palette[GPU2D.Num ? 0x400 : 0], 256*2);
        for (int s = 0; s < 4; s++)
        {
            for (int p = 0; p < 16; p++)
            {
                u16 *pal = GPU2D.GetBGExtPal(s, p);
                memcpy(&TempPalBuffer[(1 + ((s*16)+p)) * 256], pal, 256*2);
            }
        }

        glBindTexture(GL_TEXTURE_2D, PalTex_BG);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+(4*16), GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV,
                        TempPalBuffer);
    }

    GPU.PaletteDirty &= ~palmask;

    if (layer_pre_dirty)
    {
        // pre-render BG layers with the new settings

        glUseProgram(LayerPreShader);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_BLEND);
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_FALSE);

        glBindBufferBase(GL_UNIFORM_BUFFER, 20, LayerConfigUBO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, VRAMTex_BG);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, PalTex_BG);

        for (int layer = 0; layer < 4; layer++)
        {
            if (!(layer_pre_dirty & (1 << layer)))
                continue;

            PrerenderLayer(layer);
        }
    }

    if (SpriteDirty)
    {
        // OAM and VRAM have already been updated prior
        // palette needs to be updated here though

        // TODO make this only do it over the required subsection?
        NumSprites = 0;
        SpriteUseMosaic = false;
        UpdateOAM(0, 192);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, VRAMTex_OBJ);

        memcpy(&TempPalBuffer[0], &GPU.Palette[GPU2D.Num ? 0x600 : 0x200], 256*2);
        {
            u16* pal = GPU2D.GetOBJExtPal();
            memcpy(&TempPalBuffer[256], pal, 256*16*2);
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, PalTex_OBJ);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+16, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, TempPalBuffer);

        PrerenderSprites();

        LastSpriteLine = line;
    }

    LayerConfigDirty = false;
    SpriteDirty = false;
}


void GLRenderer2D::DrawScanline(u32 line)
{
    UpdateAndRender(line);
}

void GLRenderer2D::VBlank()
{
    DoRenderSprites(192);
    RenderScreen(LastLine, 192);

    LastSpriteLine = 0;
    LastLine = 0;
}

void GLRenderer2D::VBlankEnd()
{
}


void GLRenderer2D::UpdateScanlineConfig(int line)
{
    auto& cfg = ScanlineConfig.uScanline[line];

    // update BG layer coordinates
    // Y coordinates are adjusted to account for vertical mosaic
    // horizontal mosaic will be done during compositing

    u32 bgmode = DispCnt & 0x7;
    bool xmosaic = (GPU2D.BGMosaicSize[0] > 0);

    if (DispCnt & (1<<3))
    {
        // 3D layer
        int xpos = GPU.GPU3D.GetRenderXPos() & 0x1FF;
        cfg.BGOffset[0][0] = xpos - ((xpos & 0x100) << 1);
        cfg.BGOffset[0][1] = line;
        cfg.BGMosaicEnable[0] = false;
    }
    else
    {
        // text layer
        cfg.BGOffset[0][0] = GPU2D.BGXPos[0];
        if (GPU2D.BGCnt[0] & (1<<6))
        {
            cfg.BGOffset[0][1] = GPU2D.BGYPos[0] + GPU2D.BGMosaicLine;
            cfg.BGMosaicEnable[0] = xmosaic;
        }
        else
        {
            cfg.BGOffset[0][1] = GPU2D.BGYPos[0] + line;
            cfg.BGMosaicEnable[0] = false;
        }
    }

    // always a text layer
    cfg.BGOffset[1][0] = GPU2D.BGXPos[1];
    if (GPU2D.BGCnt[1] & (1<<6))
    {
        cfg.BGOffset[1][1] = GPU2D.BGYPos[1] + GPU2D.BGMosaicLine;
        cfg.BGMosaicEnable[1] = xmosaic;
    }
    else
    {
        cfg.BGOffset[1][1] = GPU2D.BGYPos[1] + line;
        cfg.BGMosaicEnable[1] = false;
    }

    if ((bgmode == 2) || (bgmode >= 4 && bgmode <= 6))
    {
        // rotscale layer
        cfg.BGOffset[2][0] = GPU2D.BGXRefInternal[0];
        cfg.BGOffset[2][1] = GPU2D.BGYRefInternal[0];
        cfg.BGRotscale[0][0] = GPU2D.BGRotA[0];
        cfg.BGRotscale[0][1] = GPU2D.BGRotB[0];
        cfg.BGRotscale[0][2] = GPU2D.BGRotC[0];
        cfg.BGRotscale[0][3] = GPU2D.BGRotD[0];
    }
    else
    {
        // text layer
        cfg.BGOffset[2][0] = GPU2D.BGXPos[2];
        if (GPU2D.BGCnt[2] & (1<<6))
            cfg.BGOffset[2][1] = GPU2D.BGYPos[2] + GPU2D.BGMosaicLine;
        else
            cfg.BGOffset[2][1] = GPU2D.BGYPos[2] + line;
    }

    if (GPU2D.BGCnt[2] & (1<<6))
        cfg.BGMosaicEnable[2] = xmosaic;
    else
        cfg.BGMosaicEnable[2] = false;

    if (bgmode >= 1 && bgmode <= 5)
    {
        // rotscale layer
        cfg.BGOffset[3][0] = GPU2D.BGXRefInternal[1];
        cfg.BGOffset[3][1] = GPU2D.BGYRefInternal[1];
        cfg.BGRotscale[1][0] = GPU2D.BGRotA[1];
        cfg.BGRotscale[1][1] = GPU2D.BGRotB[1];
        cfg.BGRotscale[1][2] = GPU2D.BGRotC[1];
        cfg.BGRotscale[1][3] = GPU2D.BGRotD[1];
    }
    else
    {
        // text layer
        cfg.BGOffset[3][0] = GPU2D.BGXPos[3];
        if (GPU2D.BGCnt[3] & (1<<6))
            cfg.BGOffset[3][1] = GPU2D.BGYPos[3] + GPU2D.BGMosaicLine;
        else
            cfg.BGOffset[3][1] = GPU2D.BGYPos[3] + line;
    }

    if (GPU2D.BGCnt[3] & (1<<6))
        cfg.BGMosaicEnable[3] = xmosaic;
    else
        cfg.BGMosaicEnable[3] = false;

    u16* pal = (u16*)&GPU.Palette[GPU2D.Num ? 0x400 : 0];
    cfg.BackColor = pal[0];

    // mosaic

    cfg.MosaicSize[0] = GPU2D.BGMosaicSize[0];
    cfg.MosaicSize[1] = GPU2D.BGMosaicSize[1];
    cfg.MosaicSize[2] = GPU2D.OBJMosaicSize[0];
    cfg.MosaicSize[3] = GPU2D.OBJMosaicSize[1];

    // windows

    //cfg.WinRegs = GPU2D.WinCnt[2] | (GPU2D.WinCnt[3] << 8) | (GPU2D.WinCnt[1] << 16) | (GPU2D.WinCnt[0] << 24);
    if (GPU2D.DispCnt & 0xE000)
        cfg.WinRegs = GPU2D.WinCnt[2];
    else
        cfg.WinRegs = 0xFF;

    if (GPU2D.DispCnt & (1<<15))
        cfg.WinRegs |= (GPU2D.WinCnt[3] << 8);
    else
        cfg.WinRegs |= 0xFF00;

    if (GPU2D.DispCnt & (1<<14))
        cfg.WinRegs |= (GPU2D.WinCnt[1] << 16);
    else
        cfg.WinRegs |= 0xFF0000;

    if (GPU2D.DispCnt & (1<<13))
        cfg.WinRegs |= (GPU2D.WinCnt[0] << 24);
    else
        cfg.WinRegs |= 0xFF000000;

    cfg.WinMask = 0;

    if ((GPU2D.DispCnt & (1<<13)) && (GPU2D.Win0Active & 0x1))
    {
        int x0 = GPU2D.Win0Coords[0];
        int x1 = GPU2D.Win0Coords[1];

        if (x0 <= x1)
        {
            cfg.WinPos[0] = x0;
            cfg.WinPos[1] = x1;
            if (GPU2D.Win0Active == 0x3)
                cfg.WinMask |= (1<<0);
            cfg.WinMask |= (1<<1);
            GPU2D.Win0Active &= ~0x2;
        }
        else
        {
            cfg.WinPos[0] = x1;
            cfg.WinPos[1] = x0;
            if (GPU2D.Win0Active == 0x3)
                cfg.WinMask |= (1<<0);
            cfg.WinMask |= (1<<2);
            GPU2D.Win0Active |= 0x2;
        }
    }
    else
    {
        cfg.WinPos[0] = 256;
        cfg.WinPos[1] = 256;
    }

    if ((GPU2D.DispCnt & (1<<14)) && (GPU2D.Win1Active & 0x1))
    {
        int x0 = GPU2D.Win1Coords[0];
        int x1 = GPU2D.Win1Coords[1];

        if (x0 <= x1)
        {
            cfg.WinPos[2] = x0;
            cfg.WinPos[3] = x1;
            if (GPU2D.Win1Active == 0x3)
                cfg.WinMask |= (1<<3);
            cfg.WinMask |= (1<<4);
            GPU2D.Win1Active &= ~0x2;
        }
        else
        {
            cfg.WinPos[2] = x1;
            cfg.WinPos[3] = x0;
            if (GPU2D.Win1Active == 0x3)
                cfg.WinMask |= (1<<3);
            cfg.WinMask |= (1<<5);
            GPU2D.Win1Active |= 0x2;
        }
    }
    else
    {
        cfg.WinPos[2] = 256;
        cfg.WinPos[3] = 256;
    }
};

void GLRenderer2D::UpdateLayerConfig()
{
    // determine which parts of VRAM were used for captures
    int capturemask = GPU2D.Num ? 0x7 : 0x1F;
    int captureinfo[32];
    GPU2D.GetCaptureInfo_BG(captureinfo);

    u32 tilebase, mapbase;
    if (!GPU2D.Num)
    {
        tilebase = ((GPU2D.DispCnt >> 24) & 0x7) << 16;
        mapbase = ((GPU2D.DispCnt >> 27) & 0x7) << 16;
    }
    else
    {
        tilebase = 0;
        mapbase = 0;
    }

    int layertype[4] = {1, 1, 0, 0};
    switch (GPU2D.DispCnt & 0x7)
    {
        case 0: layertype[2] = 1; layertype[3] = 1; break;
        case 1: layertype[2] = 1; layertype[3] = 2; break;
        case 2: layertype[2] = 2; layertype[3] = 2; break;
        case 3: layertype[2] = 1; layertype[3] = 3; break;
        case 4: layertype[2] = 2; layertype[3] = 3; break;
        case 5: layertype[2] = 3; layertype[3] = 3; break;
        case 6: layertype[0] = 0; layertype[1] = 0;
                layertype[2] = 4; layertype[3] = 0; break;
        case 7: layertype[2] = 0; layertype[3] = 0; break;
    }

    for (int layer = 0; layer < 4; layer++)
    {
        int type = layertype[layer];
        if (!type)
            continue;

        u16 bgcnt = GPU2D.BGCnt[layer];
        auto& cfg = LayerConfig.uBGConfig[layer];

        cfg.TileOffset = tilebase + (((bgcnt >> 2) & 0xF) << 14);
        cfg.MapOffset = mapbase + (((bgcnt >> 8) & 0x1F) << 11);
        cfg.PalOffset = 0;

        BGVRAMRange[layer][0] = cfg.TileOffset;
        BGVRAMRange[layer][2] = cfg.MapOffset;

        if ((layer == 0) && (GPU2D.DispCnt & (1<<3)))
        {
            // 3D layer

            cfg.Size[0] = 256; cfg.Size[1] = 192;
            cfg.Type = 6;
            cfg.Clamp = 1;

            BGVRAMRange[layer][0] = 0xFFFFFFFF;
            BGVRAMRange[layer][1] = 0xFFFFFFFF;
            BGVRAMRange[layer][2] = 0xFFFFFFFF;
            BGVRAMRange[layer][3] = 0xFFFFFFFF;
        }
        else if (type == 1)
        {
            // text layer

            u32 tilesz, mapsz;
            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x800; break;
                case 1: cfg.Size[0] = 512; cfg.Size[1] = 256; mapsz = 0x1000; break;
                case 2: cfg.Size[0] = 256; cfg.Size[1] = 512; mapsz = 0x1000; break;
                case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x2000; break;
            }

            if (bgcnt & (1<<7))
            {
                // 256-color
                cfg.Type = 1;
                if (DispCnt & (1<<30))
                {
                    // extended palette
                    int paloff = layer;
                    if ((layer < 2) && (bgcnt & (1<<13)))
                        paloff += 2;
                    cfg.PalOffset = 1 + (16 * paloff);
                }

                tilesz = 0x10000;
            }
            else
            {
                // 16-color
                cfg.Type = 0;

                tilesz = 0x8000;
            }

            cfg.Clamp = 0;

            int n = BGBaseIndex[0][bgcnt >> 14] + layer;
            BGLayerTex[layer] = AllBGLayerTex[n];
            BGLayerFB[layer] = AllBGLayerFB[n];

            BGVRAMRange[layer][1] = tilesz;
            BGVRAMRange[layer][3] = mapsz;
        }
        else if (type == 2)
        {
            // affine layer

            u32 mapsz;
            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; mapsz = 0x100; break;
                case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x400; break;
                case 2: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x1000; break;
                case 3: cfg.Size[0] = 1024; cfg.Size[1] = 1024; mapsz = 0x4000; break;
            }

            cfg.Type = 2;
            cfg.Clamp = !(bgcnt & (1<<13));

            int n = BGBaseIndex[1][bgcnt >> 14] + layer - 2;
            BGLayerTex[layer] = AllBGLayerTex[n];
            BGLayerFB[layer] = AllBGLayerFB[n];

            BGVRAMRange[layer][1] = 0x4000;
            BGVRAMRange[layer][3] = mapsz;
        }
        else if (type == 3)
        {
            // extended layer

            if (bgcnt & (1<<7))
            {
                // bitmap modes

                u32 mapsz;
                switch (bgcnt >> 14)
                {
                    case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; mapsz = 0x4000; break;
                    case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x10000; break;
                    case 2: cfg.Size[0] = 512; cfg.Size[1] = 256; mapsz = 0x20000; break;
                    case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x40000; break;
                }

                u32 tileoffset = 0;
                u32 mapoffset = ((bgcnt >> 8) & 0x1F) << 14;

                BGVRAMRange[layer][0] = 0xFFFFFFFF;
                BGVRAMRange[layer][1] = 0xFFFFFFFF;
                BGVRAMRange[layer][2] = mapoffset;
                BGVRAMRange[layer][3] = mapsz;

                if (bgcnt & (1<<2))
                {
                    mapsz <<= 1;

                    int capblock = -1;
                    if ((cfg.Size[0] == 128) || (cfg.Size[0] == 256))
                    {
                        // if this is a direct color bitmap, and the width is 128 or 256
                        // then it might be a display capture
                        u32 startaddr = mapoffset;
                        u32 endaddr = startaddr + mapsz;

                        startaddr >>= 14;
                        endaddr = (endaddr + 0x3FFF) >> 14;

                        for (u32 b = startaddr; b < endaddr; b++)
                        {
                            int blk = captureinfo[b & capturemask];
                            if (blk == -1) continue;

                            capblock = blk;
                        }
                    }

                    if (capblock != -1)
                    {
                        if (cfg.Size[0] == 128)
                        {
                            cfg.Type = 7;
                            tileoffset = capblock;
                            mapoffset = (mapoffset >> 8) & 0x7F;
                        }
                        else
                        {
                            cfg.Type = 8;
                            tileoffset = capblock >> 2;
                            mapoffset = (mapoffset >> 9) & 0xFF;
                        }
                    }
                    else
                        cfg.Type = 5;
                }
                else
                    cfg.Type = 4;

                cfg.TileOffset = tileoffset;
                cfg.MapOffset = mapoffset;

                int n = BGBaseIndex[2][bgcnt >> 14] + layer - 2;
                BGLayerTex[layer] = AllBGLayerTex[n];
                BGLayerFB[layer] = AllBGLayerFB[n];
            }
            else
            {
                // rotscale w/ tiles

                u32 mapsz;
                switch (bgcnt >> 14)
                {
                    case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; mapsz = 0x200; break;
                    case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x800; break;
                    case 2: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x2000; break;
                    case 3: cfg.Size[0] = 1024; cfg.Size[1] = 1024; mapsz = 0x8000; break;
                }

                // this layer type is always 256-color
                cfg.Type = 3;
                if (DispCnt & (1<<30))
                {
                    // extended palette
                    int paloff = layer;
                    if ((layer < 2) && (bgcnt & (1<<13)))
                        paloff += 2;
                    cfg.PalOffset = 1 + (16 * paloff);
                }

                int n = BGBaseIndex[1][bgcnt >> 14] + layer - 2;
                BGLayerTex[layer] = AllBGLayerTex[n];
                BGLayerFB[layer] = AllBGLayerFB[n];

                BGVRAMRange[layer][1] = 0x10000;
                BGVRAMRange[layer][3] = mapsz;
            }

            cfg.Clamp = !(bgcnt & (1<<13));
        }
        else //if (type == 4)
        {
            // large layer

            u32 mapsz;
            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 512; cfg.Size[1] = 1024; mapsz = 0x80000; break;
                case 1: cfg.Size[0] = 1024; cfg.Size[1] = 512; mapsz = 0x80000; break;
                case 2: cfg.Size[0] = 512; cfg.Size[1] = 256; mapsz = 0x20000; break;
                case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x40000; break;
            }

            cfg.Type = 4;
            cfg.TileOffset = 0;
            cfg.MapOffset = 0;
            cfg.Clamp = !(bgcnt & (1<<13));

            int n = BGBaseIndex[3][bgcnt >> 14];
            BGLayerTex[layer] = AllBGLayerTex[n];
            BGLayerFB[layer] = AllBGLayerFB[n];

            BGVRAMRange[layer][0] = 0xFFFFFFFF;
            BGVRAMRange[layer][1] = 0xFFFFFFFF;
            BGVRAMRange[layer][3] = mapsz;
        }
    }

    glBindBuffer(GL_UNIFORM_BUFFER, LayerConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LayerConfig), &LayerConfig);
}

void GLRenderer2D::UpdateOAM(int ystart, int yend)
{
    auto& cfg = SpriteConfig;
    u16* oam = OAM;

    // determine which parts of VRAM were used for captures
    int capturemask = GPU2D.Num ? 0x7 : 0xF;
    int captureinfo[16];
    GPU2D.GetCaptureInfo_OBJ(captureinfo);

    for (int i = 0; i < 32; i++)
    {
        s16* rotscale = (s16*)&oam[(i * 16) + 3];
        auto& rotdst = cfg.uRotscale[i];

        rotdst[0] = rotscale[0];
        rotdst[1] = rotscale[4];
        rotdst[2] = rotscale[8];
        rotdst[3] = rotscale[12];
    }

    const u8 spritewidth[16] =
    {
        8, 16, 8, 8,
        16, 32, 8, 8,
        32, 32, 16, 8,
        64, 64, 32, 8
    };
    const u8 spriteheight[16] =
    {
        8, 8, 16, 8,
        16, 8, 32, 8,
        32, 16, 32, 8,
        64, 32, 64, 8
    };

    for (int sprnum = 0; sprnum < 128; sprnum++)
    {
        u16* attrib = &oam[sprnum * 4];

        u32 sprtype = (attrib[0] >> 8) & 0x3;
        if (sprtype == 2) // sprite disabled
            continue;

        // note on sprite position:
        // X > 255 is interpreted as negative (-256..-1)
        // Y > 127 is interpreted as both positive (128..255) and negative (-128..-1)

        s32 xpos = (s32)(attrib[1] << 23) >> 23;
        s32 ypos = (s32)(attrib[0] << 24) >> 24;

        u32 sizeparam = (attrib[0] >> 14) | ((attrib[1] & 0xC000) >> 12);
        s32 width = spritewidth[sizeparam];
        s32 height = spriteheight[sizeparam];
        s32 boundwidth = width;
        s32 boundheight = height;

        if (sprtype == 3)
        {
            // double-size rotscale sprite
            boundwidth <<= 1;
            boundheight <<= 1;
        }

        if (xpos <= -boundwidth)
            continue;

        bool yc0 = ((ypos + boundheight) > ystart) && (ypos < yend);
        bool yc1 = (((ypos&0xFF) + boundheight) > ystart) && ((ypos&0xFF) < yend);
        if (!(yc0 || yc1))
            continue;

        u32 sprmode = (attrib[0] >> 10) & 0x3;
        if (sprmode == 3)
        {
            if ((GPU2D.DispCnt & 0x60) == 0x60)
                continue;
            if ((attrib[2] >> 12) == 0)
                continue;
        }

        if (NumSprites >= 128)
        {
            Log(LogLevel::Error, "GPU2D_OpenGL: SPRITE BUFFER IS FULL!!!!!\n");
            break;
        }

        // add this sprite to the OAM array

        auto& sprcfg = cfg.uOAM[NumSprites];

        sprcfg.Position[0] = (u32)xpos;
        sprcfg.Position[1] = (u32)ypos;
        sprcfg.Size[0] = width;
        sprcfg.Size[1] = height;
        sprcfg.BoundSize[0] = boundwidth;
        sprcfg.BoundSize[1] = boundheight;

        if (sprtype & 1)
        {
            sprcfg.Flip[0] = 0;
            sprcfg.Flip[1] = 0;
            sprcfg.Rotscale = (attrib[1] >> 9) & 0x1F;
        }
        else
        {
            sprcfg.Flip[0] = !!(attrib[1] & (1<<12));
            sprcfg.Flip[1] = !!(attrib[1] & (1<<13));
            sprcfg.Rotscale = (u32)-1;
        }

        sprcfg.OBJMode = sprmode;
        sprcfg.Mosaic = !!(attrib[0] & (1<<12)) && (sprmode != 2);
        sprcfg.BGPrio = (attrib[2] >> 10) & 0x3;

        u32 tilenum = attrib[2] & 0x3FF;

        if (sprmode == 3)
        {
            // bitmap sprite

            sprcfg.Type = 2;

            if (GPU2D.DispCnt & (1<<6))
            {
                // 1D mapping
                sprcfg.TileOffset = tilenum << (7 + ((GPU2D.DispCnt >> 22) & 0x1));
                sprcfg.TileStride = width * 2;
            }
            else
            {
                bool is256 = !!(GPU2D.DispCnt & (1<<5));
                int capblock = -1;

                u32 tileoffset, tilestride;
                if (is256)
                {
                    // 2D mapping, 256 pixels
                    tileoffset = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                    tilestride = 256 * 2;
                }
                else
                {
                    // 2D mapping, 128 pixels
                    tileoffset = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                    tilestride = 128 * 2;
                }

                // if this is a direct color bitmap, and the width is 128 or 256
                // then it might be a display capture
                u32 startaddr = tileoffset;
                u32 endaddr = startaddr + (height * tilestride);

                startaddr >>= 14;
                endaddr = (endaddr + 0x3FFF) >> 14;

                for (u32 b = startaddr; b < endaddr; b++)
                {
                    int blk = captureinfo[b & capturemask];
                    if (blk == -1) continue;

                    capblock = blk;
                }

                if (capblock != -1)
                {
                    if (!is256)
                    {
                        sprcfg.Type = 3;
                        tilestride = capblock;
                        tileoffset &= 0x7FFF;
                    }
                    else
                    {
                        sprcfg.Type = 4;
                        tilestride = capblock >> 2;
                        tileoffset &= 0x1FFFF;
                    }
                }

                sprcfg.TileOffset = tileoffset;
                sprcfg.TileStride = tilestride;
            }

            sprcfg.PalOffset = 1 + (attrib[2] >> 12); // alpha
        }
        else
        {
            if (GPU2D.DispCnt & (1<<4))
            {
                // 1D mapping
                sprcfg.TileOffset = tilenum << (5 + ((GPU2D.DispCnt >> 20) & 0x3));
                sprcfg.TileStride = (width >> 3) * 32;
                if (attrib[0] & (1<<13))
                    sprcfg.TileStride <<= 1;
            }
            else
            {
                // 2D mapping
                sprcfg.TileOffset = tilenum << 5;
                sprcfg.TileStride = 32 * 32;
            }

            if (attrib[0] & (1<<13))
            {
                // 256-color sprite
                sprcfg.Type = 1;
                if (GPU2D.DispCnt & (1<<31))
                    sprcfg.PalOffset = 1 + (attrib[2] >> 12);
                else
                    sprcfg.PalOffset = 0;
            }
            else
            {
                // 16-color sprite
                sprcfg.Type = 0;
                sprcfg.PalOffset = (attrib[2] >> 12) << 4;
            }
        }

        NumSprites++;

        if (sprcfg.Mosaic && (GPU2D.OBJMosaicSize[0] > 0))
            SpriteUseMosaic = true;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, SpriteConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    offsetof(sSpriteConfig, uOAM) + (NumSprites * sizeof(cfg.uOAM[0])),
                    &cfg);
}

void GLRenderer2D::UpdateCompositorConfig()
{
    // compositor info buffer
    for (int i = 0; i < 4; i++)
        CompositorConfig.uBGPrio[i] = -1;

    for (int layer = 0; layer < 4; layer++)
    {
        if (!(LayerEnable & (1 << layer)))
            continue;

        int prio = BGCnt[layer] & 0x3;
        CompositorConfig.uBGPrio[layer] = prio;
    }

    CompositorConfig.uEnableOBJ = !!(LayerEnable & (1<<4));

    CompositorConfig.uEnable3D = !!(DispCnt & (1<<3));

    CompositorConfig.uBlendCnt = BlendCnt;
    CompositorConfig.uBlendEffect = (BlendCnt >> 6) & 0x3;
    CompositorConfig.uBlendCoef[0] = EVA;
    CompositorConfig.uBlendCoef[1] = EVB;
    CompositorConfig.uBlendCoef[2] = EVY;

    glBindBuffer(GL_UNIFORM_BUFFER, CompositorConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CompositorConfig), &CompositorConfig);
}


void GLRenderer2D::PrerenderSprites()
{
    u16* vtxbuf = SpritePreVtxData;
    int vtxnum = 0;

    for (int i = 0; i < NumSprites; i++)
    {
        auto& sprite = SpriteConfig.uOAM[i];
        if (sprite.Type >= 3)
            continue;

        *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
        *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
        *vtxbuf++ = 1; *vtxbuf++ = 1; *vtxbuf++ = i;
        *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
        *vtxbuf++ = 0; *vtxbuf++ = 0; *vtxbuf++ = i;
        *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
        vtxnum += 6;
    }

    if (vtxnum == 0) return;

    glUseProgram(SpritePreShader);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);

    glBindBufferBase(GL_UNIFORM_BUFFER, 21, SpriteConfigUBO);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, SpriteFB);
    glViewport(0, 0, 1024, 512);

    glBindBuffer(GL_ARRAY_BUFFER, SpritePreVtxBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vtxnum * 3 * sizeof(u16), SpritePreVtxData);

    glBindVertexArray(SpritePreVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, vtxnum);
}

void GLRenderer2D::PrerenderLayer(int layer)
{
    auto& cfg = LayerConfig.uBGConfig[layer];

    if (cfg.Type >= 6)
        return;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, BGLayerFB[layer]);

    glUniform1i(LayerPreCurBGULoc, layer);

    // set layer size
    glViewport(0, 0, cfg.Size[0], cfg.Size[1]);

    glBindBuffer(GL_ARRAY_BUFFER, Parent.RectVtxBuffer);
    glBindVertexArray(Parent.RectVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);
}


void GLRenderer2D::DoRenderSprites(int line)
{
    int ystart = LastSpriteLine;
    int yend = line;

    glUseProgram(SpriteShader);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);

    glBindBufferBase(GL_UNIFORM_BUFFER, 21, SpriteConfigUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 24, SpriteScanlineConfigUBO);

    glBindBuffer(GL_UNIFORM_BUFFER, SpriteScanlineConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    ystart * sizeof(s32),
                    (yend - ystart) * sizeof(s32),
                    &SpriteScanlineConfig.uMosaicLine[ystart]);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OBJLayerFB);
    glViewport(0, 0, ScreenW, ScreenH);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, SpriteTex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Parent.CaptureOutput128Tex);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Parent.CaptureOutput256Tex);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, ystart * ScaleFactor, ScreenW, (yend-ystart) * ScaleFactor);

    // NOTE
    // this requires two passes for mosaic emulation, because mosaic flags get set for
    // transparent pixels too, and priority is only checked against opaque pixels

    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

    if (SpriteUseMosaic)
    {
        glUniform1i(SpriteRenderTransULoc, 1);
        glColorMaski(1, GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);

        RenderSprites(false, ystart, yend);
    }

    glUniform1i(SpriteRenderTransULoc, 0);
    glColorMaski(1, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);

    RenderSprites(true, ystart, yend);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);

    RenderSprites(false, ystart, yend);

    glDisable(GL_SCISSOR_TEST);
}

void GLRenderer2D::RenderSprites(bool window, int ystart, int yend)
{
    if (window)
    {
        if (!(GPU2D.DispCnt & (1<<15)))
            return;
    }

    u16* vtxbuf = SpriteVtxData;
    int vtxnum = 0;

    for (int i = 0; i < NumSprites; i++)
    {
        auto& sprite = SpriteConfig.uOAM[i];

        bool iswin = (sprite.OBJMode == 2);
        if (iswin != window)
            continue;

        s32 xpos = sprite.Position[0];
        s32 ypos = sprite.Position[1];
        s32 boundwidth = sprite.BoundSize[0];
        s32 boundheight = sprite.BoundSize[1];

        bool yc0 = ((ypos + boundheight) > ystart) && (ypos < yend);
        bool yc1 = (((ypos&0xFF) + boundheight) > ystart) && ((ypos&0xFF) < yend);

        if (yc0)
        {
            s32 x0 = xpos, x1 = xpos + boundwidth;
            s32 y0 = ypos, y1 = ypos + boundheight;

            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y1; *vtxbuf++ = 1; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y0; *vtxbuf++ = 0; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            vtxnum += 6;
        }

        if (yc1)
        {
            ypos &= 0xFF;
            s32 x0 = xpos, x1 = xpos + boundwidth;
            s32 y0 = ypos, y1 = ypos + boundheight;

            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y1; *vtxbuf++ = 1; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y0; *vtxbuf++ = 0; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            vtxnum += 6;
        }
    }

    if (vtxnum == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, SpriteVtxBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vtxnum * 5 * sizeof(u16), SpriteVtxData);

    glBindVertexArray(SpriteVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, vtxnum);
}

void GLRenderer2D::RenderScreen(int ystart, int yend)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OutputFB);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);

    glViewport(0, 0, ScreenW, ScreenH);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, ystart * ScaleFactor, ScreenW, (yend-ystart) * ScaleFactor);

    if (ForcedBlank || !UnitEnabled)
    {
        if (!UnitEnabled)
        {
            if (GPU2D.Num)
                glClearColor(1, 1, 1, 1);
            else
                glClearColor(0, 0, 0, 1);
        }
        else
            glClearColor(1, 1, 1, 1);

        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_SCISSOR_TEST);
        return;
    }

    glUseProgram(CompositorShader);

    glBindBufferBase(GL_UNIFORM_BUFFER, 20, LayerConfigUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 22, ScanlineConfigUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 23, CompositorConfigUBO);

    glBindBuffer(GL_UNIFORM_BUFFER, ScanlineConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    ystart * sizeof(sScanlineConfig::sScanline),
                    (yend - ystart) * sizeof(sScanlineConfig::sScanline),
                    &ScanlineConfig.uScanline[ystart]);

    UpdateCompositorConfig();

    for (int i = 0; i < 4; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);

        if ((i == 0) && (DispCnt & (1<<3)))
            glBindTexture(GL_TEXTURE_2D, Parent.OutputTex3D);
        else
            glBindTexture(GL_TEXTURE_2D, BGLayerTex[i]);

        GLint wrapmode = LayerConfig.uBGConfig[i].Clamp ? GL_CLAMP_TO_BORDER : GL_REPEAT;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapmode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapmode);
    }

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D_ARRAY, OBJLayerTex);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Parent.CaptureOutput128Tex);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Parent.CaptureOutput256Tex);

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, MosaicTex);

    glBindBuffer(GL_ARRAY_BUFFER, Parent.RectVtxBuffer);
    glBindVertexArray(Parent.RectVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);

    glDisable(GL_SCISSOR_TEST);
}

void GLRenderer2D::DrawSprites(u32 line)
{
    u32 oammask = 1 << GPU2D.Num;
    bool dirty = false;
    bool screenon = IsScreenOn();

    SpriteScanlineConfig.uMosaicLine[line] = GPU2D.OBJMosaicLine;

    u32 dispcnt_diff = GPU2D.DispCnt ^ SpriteDispCnt;
    SpriteDispCnt = GPU2D.DispCnt; // TODO CHECKME might not be right to do it here
    if (dispcnt_diff & 0x80F000F0)
        dirty = true;

    static_assert(VRAMDirtyGranularity == 512);
    NonStupidBitField<512> objDirty;

    if (screenon)
    {
        if (GPU2D.Num == 0)
        {
            objDirty = GPU.VRAMDirty_AOBJ.DeriveState(GPU.VRAMMap_AOBJ, GPU);
            GPU.MakeVRAMFlat_AOBJCoherent(objDirty);
        }
        else
        {
            auto _objDirty = GPU.VRAMDirty_BOBJ.DeriveState(GPU.VRAMMap_BOBJ, GPU);
            GPU.MakeVRAMFlat_BOBJCoherent(_objDirty);
            memcpy(objDirty.Data, _objDirty.Data, 256>>3);
        }
    }

    u8* vram; u32 vrammask;
    GPU2D.GetOBJVRAM(vram, vrammask);

    glBindTexture(GL_TEXTURE_2D, VRAMTex_OBJ);

    int texlen = (GPU2D.Num ? 256 : 512) >> 6;
    for (int i = 0; i < texlen; )
    {
        if (!objDirty.Data[i])
        {
            i++;
            continue;
        }

        int start = i * 32;
        for (;;)
        {
            i++;
            if (i >= texlen) break;
            if (!objDirty.Data[i]) break;
        }
        int end = i * 32;

        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        0, start,
                        1024, end - start,
                        GL_RED_INTEGER, GL_UNSIGNED_BYTE,
                        &vram[start * 1024]);
        dirty = true;
    }

    if ((GPU.OAMDirty & oammask) || SpriteConfigDirty)
    {
        memcpy(OAM, &GPU.OAM[GPU2D.Num ? 0x400 : 0], 0x400);
        GPU.OAMDirty &= ~oammask;
        SpriteConfigDirty = false;
        dirty = true;
    }

    // DrawScanline() for the next scanline will be called after this
    // so it will be able to do the actual sprite rendering
    if (dirty)
        SpriteDirty = true;
}

}
