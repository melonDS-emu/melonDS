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
#include "GPU.h"
#include "GPU3D.h"

namespace melonDS::GPU2D
{

#include "OpenGL_shaders/LayerPreVS.h"
#include "OpenGL_shaders/LayerPreFS.h"
#include "OpenGL_shaders/LayerVS.h"
#include "OpenGL_shaders/LayerFS.h"
#include "OpenGL_shaders/SpritePreVS.h"
#include "OpenGL_shaders/SpritePreFS.h"
#include "OpenGL_shaders/SpriteVS.h"
#include "OpenGL_shaders/SpriteFS.h"
#include "OpenGL_shaders/CompositorVS.h"
#include "OpenGL_shaders/CompositorFS.h"
#include "OpenGL_shaders/FinalPassVS.h"
#include "OpenGL_shaders/FinalPassFS.h"
#include "OpenGL_shaders/CaptureVS.h"
#include "OpenGL_shaders/CaptureFS.h"

// NOTE
// for now, this is largely a reimplementation of the software 2D renderer
// in the future, the rendering may be refined to involve more hardware processing
// and include features such as filtering/upscaling for 2D layers

std::unique_ptr<GLRenderer> GLRenderer::New(melonDS::GPU& gpu) noexcept
{
    assert(glBindAttribLocation != nullptr);
    GLuint shaderid[7];

    // TODO move those to GLInit()
    if (!OpenGL::CompileVertexFragmentProgram(shaderid[0],
                                              kLayerPreVS, kLayerPreFS,
                                              "2DLayerPreShader",
                                              {{"vPosition", 0}},
                                              {{"oColor", 0}}))
        return nullptr;

    if (!OpenGL::CompileVertexFragmentProgram(shaderid[1],
                                              kLayerVS, kLayerFS,
                                              "2DLayerShader",
                                              {{"vPosition", 0}},
                                              {{"oColor", 0}, {"oCaptureColor", 1}}))
        return nullptr;

    if (!OpenGL::CompileVertexFragmentProgram(shaderid[2],
                                              kSpritePreVS, kSpritePreFS,
                                              "2DSpritePreShader",
                                              {{"vPosition", 0}, {"vSpriteIndex", 1}},
                                              {{"oColor", 0}}))
        return nullptr;

    if (!OpenGL::CompileVertexFragmentProgram(shaderid[3],
                                              kSpriteVS, kSpriteFS,
                                              "2DSpriteShader",
                                              {{"vPosition", 0}, {"vTexcoord", 1}, {"vSpriteIndex", 2}},
                                              {{"oColor", 0}, {"oFlags", 1}}))
        return nullptr;

    if (!OpenGL::CompileVertexFragmentProgram(shaderid[4],
                                              kCompositorVS, kCompositorFS,
                                              "2DCompositorShader",
                                              {{"vPosition", 0}},
                                              {{"oColor", 0}, {"oCaptureColor", 1}}))
        return nullptr;

    if (!OpenGL::CompileVertexFragmentProgram(shaderid[5],
                                              kFinalPassVS, kFinalPassFS,
                                              "2DFinalPassShader",
                                              {{"vPosition", 0}, {"vTexcoord", 1}},
                                              {{"oTopColor", 0}, {"oBottomColor", 1}}))
        return nullptr;

    if (!OpenGL::CompileVertexFragmentProgram(shaderid[6],
                                              kCaptureVS, kCaptureFS,
                                              "2DCaptureShader",
                                              {{"vPosition", 0}, {"vTexcoord", 1}},
                                              {{"oColor", 0}}))
        return nullptr;

    auto ret = std::unique_ptr<GLRenderer>(new GLRenderer(gpu));
    ret->LayerPreShader = shaderid[0];
    ret->LayerShader = shaderid[1];
    ret->SpritePreShader = shaderid[2];
    ret->SpriteShader = shaderid[3];
    ret->CompositorShader = shaderid[4];
    ret->FPShaderID = shaderid[5];
    ret->CaptureShader = shaderid[6];
    if (!ret->GLInit())
        return nullptr;
    return ret;
}

GLRenderer::GLRenderer(melonDS::GPU& gpu)
        : Renderer2D(), GPU(gpu)
{
    BackBuffer = 0;

    LineAttribBuffer = new u32[3 * 192 * 2];
    BGOBJBuffer = new u32[256 * 3 * 192 * 2];
    AuxInputBuffer[0] = new u16[256 * 192];
    AuxInputBuffer[1] = new u16[256 * 192];

    // TODO those need to get reset upon emu reset
    memset(CaptureBuffers, 0, sizeof(CaptureBuffers));
    memset(CaptureLastBuffer, 0, sizeof(CaptureLastBuffer));
    ActiveCapture = -1;
}

#define glDefaultTexParams(target) \
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); \
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); \
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST); \
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

bool GLRenderer::GLInit()
{
    GLint uniloc;

    float rectvertices[2*2*3] = {
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
    // TODO might change
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

    for (int i = 0; i < 2; i++)
    {
        auto& state = UnitState[i];
        memset(&state, 0, sizeof(state));

        // generate textures to hold raw BG and OBJ VRAM and palettes

        int bgheight = (i == 0) ? 512 : 128;
        int objheight = (i == 0) ? 256 : 128;

        state.LayerConfig.uVRAMMask = bgheight - 1;
        state.SpriteConfig.uVRAMMask = objheight - 1;

        glGenTextures(1, &state.VRAMTex_BG);
        glBindTexture(GL_TEXTURE_2D, state.VRAMTex_BG);
        glDefaultTexParams(GL_TEXTURE_2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 1024, bgheight, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

        glGenTextures(1, &state.VRAMTex_OBJ);
        glBindTexture(GL_TEXTURE_2D, state.VRAMTex_OBJ);
        glDefaultTexParams(GL_TEXTURE_2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 1024, objheight, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

        glGenTextures(1, &state.PalTex_BG);
        glBindTexture(GL_TEXTURE_2D, state.PalTex_BG);
        glDefaultTexParams(GL_TEXTURE_2D);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 1+(4*16), 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, 256, 1+(4*16), 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr);

        glGenTextures(1, &state.PalTex_OBJ);
        glBindTexture(GL_TEXTURE_2D, state.PalTex_OBJ);
        glDefaultTexParams(GL_TEXTURE_2D);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 1+16, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, 256, 1+16, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr);

        // generate texture to hold pre-rendered BG layers

        glGenTextures(1, &state.BGLayerTex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, state.BGLayerTex);
        glDefaultTexParams(GL_TEXTURE_2D_ARRAY);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 1024, 1024, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glGenFramebuffers(4, state.BGLayerFB);
        for (int j = 0; j < 4; j++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, state.BGLayerFB[j]);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.BGLayerTex, 0, j);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
        }

        // generate texture to hold pre-rendered sprites

        glGenTextures(1, &state.SpriteTex);
        glBindTexture(GL_TEXTURE_2D, state.SpriteTex);
        glDefaultTexParams(GL_TEXTURE_2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glGenFramebuffers(1, &state.SpriteFB);
        glBindFramebuffer(GL_FRAMEBUFFER, state.SpriteFB);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.SpriteTex, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        // generate texture to hold final (upscaled) layers (BG/OBJ)

        glGenTextures(1, &state.FinalLayerTex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, state.FinalLayerTex);
        glDefaultTexParams(GL_TEXTURE_2D_ARRAY);
        //glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 1024, 1024, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glGenFramebuffers(5, state.FinalLayerFB);

        // generate texture for the compositor output

        glGenTextures(1, &state.OutputTex);
        glBindTexture(GL_TEXTURE_2D, state.OutputTex);
        glDefaultTexParams(GL_TEXTURE_2D);

        glGenFramebuffers(1, &state.OutputFB);
    }

    // generate texture to hold display capture input (source A)
    glGenTextures(1, &CaptureInputTex);
    glBindTexture(GL_TEXTURE_2D, CaptureInputTex);
    glDefaultTexParams(GL_TEXTURE_2D);

    // generate buffers to hold display capture output

    glGenTextures(1, &CaptureOutput256Tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);
    glDefaultTexParams(GL_TEXTURE_2D_ARRAY);
    glGenFramebuffers(4, CaptureOutput256FB);

    glGenTextures(1, &CaptureOutput128Tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput128Tex);
    glDefaultTexParams(GL_TEXTURE_2D_ARRAY);
    glGenFramebuffers(16, CaptureOutput128FB);

    glGenTextures(1, &CaptureSyncTex);
    glBindTexture(GL_TEXTURE_2D, CaptureSyncTex);
    glDefaultTexParams(GL_TEXTURE_2D);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);

    glGenFramebuffers(1, &CaptureSyncFB);
    glBindFramebuffer(GL_FRAMEBUFFER, CaptureSyncFB);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CaptureSyncTex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);


    glGenBuffers(1, &LayerConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, LayerConfigUBO);
    static_assert((sizeof(sUnitState::sLayerConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sUnitState::sLayerConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 10, LayerConfigUBO);

    glGenBuffers(1, &SpriteConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, SpriteConfigUBO);
    static_assert((sizeof(sUnitState::sSpriteConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sUnitState::sSpriteConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 11, SpriteConfigUBO);

    glGenBuffers(1, &ScanlineConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, ScanlineConfigUBO);
    static_assert((sizeof(sUnitState::sScanlineConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sUnitState::sScanlineConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 12, ScanlineConfigUBO);

    glGenBuffers(1, &CompositorConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, CompositorConfigUBO);
    static_assert((sizeof(sUnitState::sCompositorConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sUnitState::sCompositorConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 13, CompositorConfigUBO);

    glGenBuffers(1, &FPConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, FPConfigUBO);
    static_assert((sizeof(sFinalPassConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sFinalPassConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 14, FPConfigUBO);

    glGenBuffers(1, &CaptureConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, CaptureConfigUBO);
    static_assert((sizeof(sCaptureConfig) & 15) == 0);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(sCaptureConfig), nullptr, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 15, CaptureConfigUBO);


    glUseProgram(LayerPreShader);

    uniloc = glGetUniformLocation(LayerPreShader, "VRAMTex");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(LayerPreShader, "PalTex");
    glUniform1i(uniloc, 1);

    uniloc = glGetUniformBlockIndex(LayerPreShader, "uConfig");
    glUniformBlockBinding(LayerPreShader, uniloc, 10);

    LayerPreCurBGULoc = glGetUniformLocation(LayerPreShader, "uCurBG");


    glUseProgram(LayerShader);

    uniloc = glGetUniformLocation(LayerShader, "BGLayerTex");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(LayerShader, "_3DLayerTex");
    glUniform1i(uniloc, 1);
    uniloc = glGetUniformLocation(LayerShader, "CaptureTex");
    glUniform1i(uniloc, 2);

    uniloc = glGetUniformBlockIndex(LayerShader, "uScanlineConfig");
    glUniformBlockBinding(LayerShader, uniloc, 12);
    uniloc = glGetUniformBlockIndex(LayerShader, "uConfig");
    glUniformBlockBinding(LayerShader, uniloc, 10);

    LayerScaleULoc = glGetUniformLocation(LayerShader, "uScaleFactor");
    LayerCurUnitULoc = glGetUniformLocation(LayerShader, "uCurUnit");
    LayerCurBGULoc = glGetUniformLocation(LayerShader, "uCurBG");


    glUseProgram(SpritePreShader);

    uniloc = glGetUniformLocation(SpritePreShader, "VRAMTex");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(SpritePreShader, "PalTex");
    glUniform1i(uniloc, 1);

    uniloc = glGetUniformBlockIndex(SpritePreShader, "uConfig");
    glUniformBlockBinding(SpritePreShader, uniloc, 11);


    glUseProgram(SpriteShader);

    uniloc = glGetUniformLocation(SpriteShader, "SpriteTex");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(SpriteShader, "Capture128Tex");
    glUniform1i(uniloc, 1);
    uniloc = glGetUniformLocation(SpriteShader, "Capture256Tex");
    glUniform1i(uniloc, 2);

    uniloc = glGetUniformBlockIndex(SpriteShader, "uConfig");
    glUniformBlockBinding(SpriteShader, uniloc, 11);


    glUseProgram(CompositorShader);

    uniloc = glGetUniformLocation(CompositorShader, "LayerTex");
    glUniform1i(uniloc, 0);

    uniloc = glGetUniformBlockIndex(CompositorShader, "uScanlineConfig");
    glUniformBlockBinding(CompositorShader, uniloc, 12);
    uniloc = glGetUniformBlockIndex(CompositorShader, "uCompositorConfig");
    glUniformBlockBinding(CompositorShader, uniloc, 13);

    CompositorScaleULoc = glGetUniformLocation(CompositorShader, "uScaleFactor");


    glUseProgram(FPShaderID);

    /*FPScaleULoc = glGetUniformLocation(FPShaderID, "u3DScale");
    FPCaptureRegULoc = glGetUniformLocation(FPShaderID, "uCaptureReg");
    FPCaptureMaskULoc = glGetUniformLocation(FPShaderID, "uCaptureVRAMMask");

    for (int i = 0; i < 16; i++)
    {
        char var[32];
        sprintf(var, "CaptureOutput256Tex[%d]", i);
        FPCaptureTexLoc[i] = glGetUniformLocation(FPShaderID, var);
    }*/

    uniloc = glGetUniformLocation(FPShaderID, "MainInputTexA");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(FPShaderID, "MainInputTexB");
    glUniform1i(uniloc, 1);
    uniloc = glGetUniformLocation(FPShaderID, "AuxInputTex");
    glUniform1i(uniloc, 2);

    uniloc = glGetUniformBlockIndex(FPShaderID, "uFinalPassConfig");
    glUniformBlockBinding(FPShaderID, uniloc, 14);


    glUseProgram(CaptureShader);

    uniloc = glGetUniformLocation(CaptureShader, "InputTexA");
    glUniform1i(uniloc, 0);
    uniloc = glGetUniformLocation(CaptureShader, "InputTexB");
    glUniform1i(uniloc, 1);

    uniloc = glGetUniformBlockIndex(CaptureShader, "uCaptureConfig");
    glUniformBlockBinding(CaptureShader, uniloc, 15);


    float vertices[12][4];
#define SETVERTEX(i, x, y) \
    vertices[i][0] = x; \
    vertices[i][1] = y; \
    vertices[i][2] = (x + 1.f) * (256.f / 2.f); \
    vertices[i][3] = (y + 1.f) * (192.f / 2.f); \

    SETVERTEX(0, -1, 1);
    SETVERTEX(1, 1, -1);
    SETVERTEX(2, 1, 1);
    SETVERTEX(3, -1, 1);
    SETVERTEX(4, -1, -1);
    SETVERTEX(5, 1, -1);

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

    //glGenFramebuffers(FPOutputFB.size(), &FPOutputFB[0]);
    glGenFramebuffers(2, &FPOutputFB[0]);

    /*glGenTextures(1, &LineAttribTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, LineAttribTex);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB32UI, 192*2, 0, GL_RGB_INTEGER, GL_UNSIGNED_INT, nullptr);

    glGenTextures(1, &BGOBJTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, BGOBJTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 256*3, 192*2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);*/

    glGenTextures(1, &AuxInputTex);
    glBindTexture(GL_TEXTURE_2D, AuxInputTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);
    /*glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D_ARRAY, AuxInputTex);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB5_A1, 256, 192, 2, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);
*/
    //glGenTextures(FPOutputTex.size(), &FPOutputTex[0]);
    for (int i = 0; i < 2; i++)
    {
        glGenTextures(2, FPOutputTex[i]);
        for (GLuint tex: FPOutputTex[i])
        {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

GLRenderer::~GLRenderer()
{
    // TODO delete capture textures!!
    // and a bunch of other shit I need to add here too

    //glDeleteFramebuffers(FPOutputFB.size(), &FPOutputFB[0]);
    glDeleteFramebuffers(2, &FPOutputFB[0]);
    //glDeleteTextures(1, &LineAttribTex);
    //glDeleteTextures(1, &BGOBJTex);
    //glDeleteTextures(FPOutputTex.size(), &FPOutputTex[0]);
    for (int i = 0; i < 2; i++)
        glDeleteTextures(2, FPOutputTex[i]);
    //glDeleteTextures(1, &test);

    glDeleteVertexArrays(1, &FPVertexArrayID);
    glDeleteBuffers(1, &FPVertexBufferID);

    glDeleteVertexArrays(1, &RectVtxArray);
    glDeleteBuffers(1, &RectVtxBuffer);

    glDeleteProgram(FPShaderID);
    glDeleteProgram(LayerPreShader);

    for (int i = 0; i < 2; i++)
    {
        auto& state = UnitState[i];

        glDeleteTextures(1, &state.VRAMTex_BG);
        glDeleteTextures(1, &state.VRAMTex_OBJ);
        glDeleteTextures(1, &state.PalTex_BG);
        glDeleteTextures(1, &state.PalTex_OBJ);
        glDeleteTextures(1, &state.BGLayerTex);
        //glDeleteBuffers(1, &state.LayerConfigUBO);
    }

    glDeleteBuffers(1, &LayerConfigUBO);

    delete[] LineAttribBuffer;
    delete[] BGOBJBuffer;
    delete[] AuxInputBuffer[0];
    delete[] AuxInputBuffer[1];
}


void GLRenderer::SetScaleFactor(int scale)
{
    if (scale == ScaleFactor)
        return;

    ScaleFactor = scale;
    ScreenW = 256 * scale;
    ScreenH = 192 * scale;

    const GLenum fbassign2[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glBindTexture(GL_TEXTURE_2D, CaptureInputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

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

    for (int u = 0; u < 2; u++)
    {
        auto& state = UnitState[u];

        state.SpriteConfig.uScaleFactor = ScaleFactor;

        glBindTexture(GL_TEXTURE_2D_ARRAY, state.FinalLayerTex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, ScreenW, ScreenH, 6, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        for (int i = 0; i < 4; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, state.FinalLayerFB[i]);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.FinalLayerTex, 0, i);

            if ((u == 0) && (i == 0))
            {
                glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, CaptureInputTex, 0);
                glDrawBuffers(2, fbassign2);
            }
            else
                glDrawBuffer(GL_COLOR_ATTACHMENT0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, state.FinalLayerFB[4]);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.FinalLayerTex, 0, 4);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, state.FinalLayerTex, 0, 5);
        glDrawBuffers(2, fbassign2);

        glBindTexture(GL_TEXTURE_2D, state.OutputTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, state.OutputFB);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.OutputTex, 0);

        if (u == 0)
        {
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, CaptureInputTex, 0);
            glDrawBuffers(2, fbassign2);
        }
        else
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }

    for (int i = 0; i < 2; i++)
    {
        /*glBindTexture(GL_TEXTURE_2D, FPOutputTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        // fill the padding
        u8* zeroPixels = (u8*) calloc(1, ScreenW*2*scale*4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192*scale, ScreenW, 2*scale, GL_RGBA, GL_UNSIGNED_BYTE, zeroPixels);*/
        for (GLuint tex : FPOutputTex[i])
        {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }

        //GLenum fbassign[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glBindFramebuffer(GL_FRAMEBUFFER, FPOutputFB[i]);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FPOutputTex[i][0], 0);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, FPOutputTex[i][1], 0);
        //glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, 0, 0);
        //glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, test, 0);
        //glDrawBuffers(3, fbassign);
        glDrawBuffers(2, fbassign2);
        //free(zeroPixels);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void GLRenderer::DrawScanline(u32 line, Unit* unit)
{
    CurUnit = unit;

    /*if (unit->Num == 1)
    {
        printf("line %d OAM %08X\n", line, GPU.OAMDirty);
        GPU.OAMDirty = 0;
    }*/

    int screen = CurUnit->ScreenPos;
    //int yoffset = (screen * 192) + line;
    int yoffset = (CurUnit->Num * 192) + line;

    u32* attrib = &LineAttribBuffer[3 * yoffset];
    u32* dst = &BGOBJBuffer[256 * 3 * yoffset];

    int lineout = line;
    line = GPU.VCount;

    // TODO detect if VRAM was modified and do a partial render
    bool vramdirty = false;

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

    bool forceblank = false;

    // scanlines that end up outside of the GPU drawing range
    // (as a result of writing to VCount) are filled white
    if (line > 192) forceblank = true;

    // GPU B can be completely disabled by POWCNT1
    // oddly that's not the case for GPU A
    if (CurUnit->Num && !CurUnit->Enabled) forceblank = true;

    // TODO move this logic to GPU!! (or atleast outside of the renderer)
    if (line == 0 && CurUnit->CaptureCnt & (1 << 31) && !forceblank)
        CurUnit->CaptureLatch = true;

    // TODO: do partial rendering if any critical registers/VRAM were modified
    UpdateScanlineConfig(unit, line);

    if (unit->Num == 0)
        FinalPassConfig.uScreenSwap[line] = unit->ScreenPos;

    if (forceblank)
    {
        // TODO signal blank differently.
        for (int i = 0; i < 256; i++)
            dst[i] = 0xFFFFFFFF;

        return;
    }

    /*attrib[0] = CurUnit->DispCnt;
    attrib[1] = CurUnit->BlendCnt | (CurUnit->EVA << 16) | (CurUnit->EVB << 24);

    u32 attr2 = (CurUnit->MasterBrightness & 0x1F) | ((CurUnit->MasterBrightness & 0xC000) >> 8) |
                (CurUnit->EVY << 8) | (CurUnit->ScreenPos << 31);
    if (!CurUnit->Num)
        attr2 |= (GPU.GPU3D.GetRenderXPos() << 16);
    attrib[2] = attr2;*/

    //u32 dispmode = CurUnit->DispCnt >> 16;
    //dispmode &= (CurUnit->Num ? 0x1 : 0x3);

    // always render regular graphics
    //BGOBJLine = &BGOBJBuffer[256 * 3 * yoffset];
    //DrawScanline_BGOBJ(line);
    CurUnit->UpdateMosaicCounters(line);

    /*for (int i = 0; i < 256; i++)
    {
        // add window mask for color effects
        // TODO

        //dst[i] = 0xFF3F003F | (line << 8);
    }*/

    // TODO is it also done if forceblank is set?
    unit->BGXRefInternal[0] += unit->BGRotB[0];
    unit->BGYRefInternal[0] += unit->BGRotD[0];
    unit->BGXRefInternal[1] += unit->BGRotB[1];
    unit->BGYRefInternal[1] += unit->BGRotD[1];

    if (CurUnit->Num == 0)
    {
        u32 dispmode = (CurUnit->DispCnt >> 16) & 0x3;
        u32 capcnt = CurUnit->CaptureCnt;
        u32 capsel = (capcnt >> 29) & 0x3;
        u32 capB = (capcnt >> 25) & 0x1;
        bool checkcap = CurUnit->CaptureLatch && (capsel != 0);

        if ((dispmode == 2) || (checkcap && (capB == 0)))
        {
            AuxUsageMask |= (1<<0);

            u32 vrambank = (CurUnit->DispCnt >> 18) & 0x3;
            if (GPU.VRAMMap_LCDC & (1<<vrambank))
            {
                u32 vramoffset = line * 256;
                if (dispmode != 2)
                    vramoffset += ((capcnt >> 26) & 0x3) << 14;

                int capblk = GPU.GetCaptureBlock_LCDC((vrambank << 17) | (vramoffset << 1));
                if (capblk != -1)
                {
                    // TODO inexact!
                    CaptureUsageMask |= (1 << capblk);
                }
                else
                {
                    u16* vram = (u16*)GPU.VRAM[vrambank];
                    u16* adst = &AuxInputBuffer[0][lineout * 256];

                    for (int i = 0; i < 256; i++)
                    {
                        adst[i] = vram[vramoffset & 0xFFFF];
                        vramoffset++;
                    }
                }
            }
            else
            {
                for (int i = 0; i < 256; i++)
                {
                    dst[i] = 0;
                }
            }
        }

        if ((dispmode == 3) || (checkcap && (capB == 1)))
        {
            AuxUsageMask |= (1<<1);

            u16* adst = &AuxInputBuffer[0][lineout * 256];
            for (int i = 0; i < 256; i++)
            {
                adst[i] = CurUnit->DispFIFOBuffer[i];
            }
        }
    }
}

void GLRenderer::VBlank(Unit* unitA, Unit* unitB)
{
    // TODO!!! do this more nicely!!!
    GLuint fart;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&fart);
    _3DLayerTex = fart;

    RenderScreen(unitA, 0, 192);
    RenderScreen(unitB, 0, 192);

    GPU.OAMDirty = 0;
    UnitState[0].LastSpriteLine = 0;
    UnitState[1].LastSpriteLine = 0;


    int backbuf = BackBuffer;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FPOutputFB[backbuf]);

    // TODO: unmap capture buffer when not capturing?

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (unitA->CaptureLatch)
        glColorMaski(2, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    else
        glColorMaski(2, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glViewport(0, 0, ScreenW, ScreenH);

    //glClear(GL_COLOR_BUFFER_BIT);

    // TODO: select more shaders (filtering, etc)
    glUseProgram(FPShaderID);
    //glUniform1ui(FPScaleULoc, ScaleFactor);
    // TODO: latch the register?
    // also it has bit31 cleared by now. sucks
    //glUniform1i(FPCaptureRegULoc, unitA->CaptureCnt | (unitA->CaptureLatch << 31));
    //glUniform1i(FPCaptureMaskULoc, CaptureUsageMask);

    FinalPassConfig.uScaleFactor = ScaleFactor;
    FinalPassConfig.uAuxScaleFactor = 1; // TODO adjust this for hi-res capture
    FinalPassConfig.uDispModeA = (unitA->DispCnt >> 16) & 0x3;
    FinalPassConfig.uDispModeB = (unitB->DispCnt >> 16) & 0x1;
    FinalPassConfig.uBrightModeA = (unitA->MasterBrightness >> 14) & 0x3;
    FinalPassConfig.uBrightModeB = (unitB->MasterBrightness >> 14) & 0x3;
    FinalPassConfig.uBrightFactorA = std::min(unitA->MasterBrightness & 0x1F, 16);
    FinalPassConfig.uBrightFactorB = std::min(unitB->MasterBrightness & 0x1F, 16);

    glBindBuffer(GL_UNIFORM_BUFFER, FPConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FinalPassConfig), &FinalPassConfig);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, UnitState[0].OutputTex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, UnitState[1].OutputTex);

    if (AuxUsageMask)
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, AuxInputTex);

        // TODO!!
        /*if (AuxUsageMask & (1<<0))
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 256, 192, 1, GL_RGBA,
                            GL_UNSIGNED_SHORT_1_5_5_5_REV, AuxInputBuffer[0]);
        if (AuxUsageMask & (1<<1))
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 256, 192, 1, GL_RGBA,
                            GL_UNSIGNED_SHORT_1_5_5_5_REV, AuxInputBuffer[1]);*/
    }

    // TODO!! do capture!!
    /*printf("SMOOSH=%04X\n", CaptureUsageMask);
    int tex = 4;
    for (int i = 0; i < 16; i++)
    {
        if (!(CaptureUsageMask & (1<<i)))
            continue;

        glActiveTexture(GL_TEXTURE0 + tex);
        glUniform1i(FPCaptureTexLoc[i], tex);

        int cur = CaptureLastBuffer[i];
        auto& capbuf = CaptureBuffers[i][cur];
        glBindTexture(GL_TEXTURE_2D, capbuf.Texture);
        tex++;
    }*/

    glBindBuffer(GL_ARRAY_BUFFER, FPVertexBufferID);
    glBindVertexArray(FPVertexArrayID);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);

    if (unitA->CaptureLatch)
    {
        DoCapture(unitA);
    }

    if (ActiveCapture != -1)
    {printf("we finished capturing to block %d\n", ActiveCapture);
        int block = ActiveCapture;
        int cur = CaptureLastBuffer[block] ^ 1;
        auto& capbuf = CaptureBuffers[block][cur];

        capbuf.Complete = true;
        CaptureLastBuffer[block] = cur;
        ActiveCapture = -1;
    }
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

    CaptureUsageMask = 0;
    AuxUsageMask = 0;
}


void GLRenderer::UpdateScanlineConfig(Unit* unit, int line)
{
    auto& state = UnitState[unit->Num];
    auto& cfg = state.ScanlineConfig.uScanline[line];

    u32 bgmode = unit->DispCnt & 0x7;

    if (unit->DispCnt & (1<<3))
    {
        // 3D layer
        cfg.BGOffset[0][0] = GPU.GPU3D.GetRenderXPos();
        cfg.BGOffset[0][1] = 0;
    }
    else
    {
        // text layer
        cfg.BGOffset[0][0] = unit->BGXPos[0];
        cfg.BGOffset[0][1] = unit->BGYPos[0];
    }

    // always a text layer
    cfg.BGOffset[1][0] = unit->BGXPos[1];
    cfg.BGOffset[1][1] = unit->BGYPos[1];

    if ((bgmode == 2) || (bgmode >= 4 && bgmode <= 6))
    {
        // rotscale layer
        cfg.BGOffset[2][0] = unit->BGXRefInternal[0];
        cfg.BGOffset[2][1] = unit->BGYRefInternal[0];
        cfg.BGRotscale[0][0] = unit->BGRotA[0];
        cfg.BGRotscale[0][1] = unit->BGRotB[0];
        cfg.BGRotscale[0][2] = unit->BGRotC[0];
        cfg.BGRotscale[0][3] = unit->BGRotD[0];
    }
    else
    {
        // text layer
        cfg.BGOffset[2][0] = unit->BGXPos[2];
        cfg.BGOffset[2][1] = unit->BGYPos[2];
    }

    if (bgmode >= 1 && bgmode <= 5)
    {
        // rotscale layer
        cfg.BGOffset[3][0] = unit->BGXRefInternal[1];
        cfg.BGOffset[3][1] = unit->BGYRefInternal[1];
        cfg.BGRotscale[1][0] = unit->BGRotA[1];
        cfg.BGRotscale[1][1] = unit->BGRotB[1];
        cfg.BGRotscale[1][2] = unit->BGRotC[1];
        cfg.BGRotscale[1][3] = unit->BGRotD[1];
    }
    else
    {
        // text layer
        cfg.BGOffset[3][0] = unit->BGXPos[3];
        cfg.BGOffset[3][1] = unit->BGYPos[3];
    }

    u16* pal = (u16*)&GPU.Palette[unit->Num ? 0x400 : 0];
    cfg.BackColor = pal[0];

    // windows

    //cfg.WinRegs = unit->WinCnt[2] | (unit->WinCnt[3] << 8) | (unit->WinCnt[1] << 16) | (unit->WinCnt[0] << 24);
    if (unit->DispCnt & 0xE000)
        cfg.WinRegs = unit->WinCnt[2];
    else
        cfg.WinRegs = 0xFF;

    if (unit->DispCnt & (1<<15))
        cfg.WinRegs |= (unit->WinCnt[3] << 8);
    else
        cfg.WinRegs |= 0xFF00;

    if (unit->DispCnt & (1<<14))
        cfg.WinRegs |= (unit->WinCnt[1] << 16);
    else
        cfg.WinRegs |= 0xFF0000;

    if (unit->DispCnt & (1<<13))
        cfg.WinRegs |= (unit->WinCnt[0] << 24);
    else
        cfg.WinRegs |= 0xFF000000;

    cfg.WinMask = 0;

    if ((unit->DispCnt & (1<<13)) && (unit->Win0Active & 0x1))
    {
        int x0 = unit->Win0Coords[0];
        int x1 = unit->Win0Coords[1];

        if (x0 <= x1)
        {
            cfg.WinPos[0] = x0;
            cfg.WinPos[1] = x1;
            if (unit->Win0Active == 0x3)
                cfg.WinMask |= (1<<0);
            cfg.WinMask |= (1<<1);
            unit->Win0Active &= ~0x2;
        }
        else
        {
            cfg.WinPos[0] = x1;
            cfg.WinPos[1] = x0;
            if (unit->Win0Active == 0x3)
                cfg.WinMask |= (1<<0);
            cfg.WinMask |= (1<<2);
            unit->Win0Active |= 0x2;
        }
    }
    else
    {
        cfg.WinPos[0] = 256;
        cfg.WinPos[1] = 256;
    }

    if ((unit->DispCnt & (1<<14)) && (unit->Win1Active & 0x1))
    {
        int x0 = unit->Win1Coords[0];
        int x1 = unit->Win1Coords[1];

        if (x0 <= x1)
        {
            cfg.WinPos[2] = x0;
            cfg.WinPos[3] = x1;
            if (unit->Win1Active == 0x3)
                cfg.WinMask |= (1<<3);
            cfg.WinMask |= (1<<4);
            unit->Win1Active &= ~0x2;
        }
        else
        {
            cfg.WinPos[2] = x1;
            cfg.WinPos[3] = x0;
            if (unit->Win1Active == 0x3)
                cfg.WinMask |= (1<<3);
            cfg.WinMask |= (1<<5);
            unit->Win1Active |= 0x2;
        }
    }
    else
    {
        cfg.WinPos[2] = 256;
        cfg.WinPos[3] = 256;
    }
};

void GLRenderer::UpdateLayerConfig(Unit* unit)
{
    auto& state = UnitState[unit->Num];
    u32 dispcnt = unit->DispCnt;

    // determine which parts of VRAM were used for captures
    // TODO make this more efficient
    for (u32 i = 0; i < (unit->Num ? 0x20000 : 0x80000); i += 0x4000)
    {
        int blk = unit->GetCaptureBlock_BG(i);
        state.LayerConfig.uCaptureMask[i >> 14] = blk;
    }

    u32 tilebase, mapbase;
    if (!unit->Num)
    {
        tilebase = ((dispcnt >> 24) & 0x7) << 16;
        mapbase = ((dispcnt >> 27) & 0x7) << 16;
    }
    else
    {
        tilebase = 0;
        mapbase = 0;
    }

    int layertype[4] = {1, 1, 0, 0};
    switch (dispcnt & 0x7)
    {
        case 0: layertype[2] = 1; layertype[3] = 1; break;
        case 1: layertype[2] = 1; layertype[3] = 2; break;
        case 2: layertype[2] = 2; layertype[3] = 2; break;
        case 3: layertype[2] = 1; layertype[3] = 3; break;
        case 4: layertype[2] = 2; layertype[3] = 3; break;
        case 5: layertype[2] = 3; layertype[3] = 3; break;
        case 6: layertype[2] = 4; layertype[3] = 0; break;
        case 7: layertype[2] = 0; layertype[3] = 0; break;
    }

    for (int layer = 0; layer < 4; layer++)
    {
        int type = layertype[layer];
        if (!type)
            continue;

        u16 bgcnt = unit->BGCnt[layer];
        auto& cfg = state.LayerConfig.uBGConfig[layer];

        cfg.TileOffset = tilebase + (((bgcnt >> 2) & 0xF) << 14);
        cfg.MapOffset = mapbase + (((bgcnt >> 8) & 0x1F) << 11);
        cfg.PalOffset = 0;

        if ((layer == 0) && (unit->DispCnt & (1<<3)))
        {
            // 3D layer

            cfg.Size[0] = 256; cfg.Size[1] = 192;
            cfg.Type = 6;
            cfg.Clamp = 1;
        }
        else if (type == 1)
        {
            // text layer

            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 256; cfg.Size[1] = 256; break;
                case 1: cfg.Size[0] = 512; cfg.Size[1] = 256; break;
                case 2: cfg.Size[0] = 256; cfg.Size[1] = 512; break;
                case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; break;
            }

            if (bgcnt & (1<<7))
            {
                // 256-color
                cfg.Type = 1;
                if (dispcnt & (1<<30))
                {
                    // extended palette
                    int paloff = layer;
                    if ((layer < 2) && (bgcnt & (1<<13)))
                        paloff += 2;
                    cfg.PalOffset = 1 + (16 * paloff);
                }
            }
            else
            {
                // 16-color
                cfg.Type = 0;
            }

            cfg.Clamp = 0;
        }
        else if (type == 2)
        {
            // affine layer

            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; break;
                case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; break;
                case 2: cfg.Size[0] = 512; cfg.Size[1] = 512; break;
                case 3: cfg.Size[0] = 1024; cfg.Size[1] = 1024; break;
            }

            cfg.Type = 2;
            cfg.Clamp = !(bgcnt & (1<<13));
        }
        else if (type == 3)
        {
            // extended layer

            if (bgcnt & (1<<7))
            {
                // bitmap modes

                switch (bgcnt >> 14)
                {
                    case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; break;
                    case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; break;
                    case 2: cfg.Size[0] = 512; cfg.Size[1] = 256; break;
                    case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; break;
                }

                if (bgcnt & (1<<2))
                    cfg.Type = 5;
                else
                    cfg.Type = 4;

                cfg.TileOffset = 0;
                cfg.MapOffset = ((bgcnt >> 8) & 0x1F) << 14;
            }
            else
            {
                // rotscale w/ tiles

                switch (bgcnt >> 14)
                {
                    case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; break;
                    case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; break;
                    case 2: cfg.Size[0] = 512; cfg.Size[1] = 512; break;
                    case 3: cfg.Size[0] = 1024; cfg.Size[1] = 1024; break;
                }

                // this layer type is always 256-color
                cfg.Type = 3;
                if (dispcnt & (1<<30))
                {
                    // extended palette
                    int paloff = layer;
                    if ((layer < 2) && (bgcnt & (1<<13)))
                        paloff += 2;
                    cfg.PalOffset = 1 + (16 * paloff);
                }
            }

            cfg.Clamp = !(bgcnt & (1<<13));
        }
        else //if (type == 4)
        {
            // large layer

            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 512; cfg.Size[1] = 1024; break;
                case 1: cfg.Size[0] = 1024; cfg.Size[1] = 512; break;
                case 2: cfg.Size[0] = 512; cfg.Size[1] = 256; break;
                case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; break;
            }

            cfg.Type = 4;
            cfg.TileOffset = 0;
            cfg.MapOffset = 0;
            cfg.Clamp = !(bgcnt & (1<<13));
        }
    }

    glBindBuffer(GL_UNIFORM_BUFFER, LayerConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(state.LayerConfig), &state.LayerConfig);
}

void GLRenderer::UpdateOAM(Unit* unit, int ystart, int yend)
{
    auto& state = UnitState[unit->Num];
    auto& cfg = state.SpriteConfig;
    //u16* oam = (u16*)&GPU.OAM[unit->Num ? 0x400 : 0];
    u16* oam = state.OAM;

    // determine which parts of VRAM were used for captures
    // TODO make this more efficient
    for (u32 i = 0; i < (unit->Num ? 0x20000 : 0x40000); i += 0x4000)
    {
        int blk = unit->GetCaptureBlock_OBJ(i);
        cfg.uCaptureMask[i >> 14] = blk;
    }

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

    for (int bgnum = 0x0C00; bgnum >= 0x0000; bgnum-=0x0400)
    {
        for (int sprnum = 127; sprnum >= 0; sprnum--)
        {
            u16* attrib = &oam[sprnum * 4];

            if ((attrib[2] & 0x0C00) != bgnum)
                continue;

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
                if ((unit->DispCnt & 0x60) == 0x60)
                    continue;
                if ((attrib[2] >> 12) == 0)
                    continue;
            }

            if (state.NumSprites >= 128)
            {
                printf("GPU2D_OpenGL: SPRITE BUFFER IS FULL!!!!!\n");
                break;
            }

            // add this sprite to the OAM array
            // TODO: if we do partial rendering, we need to check whether it was already in the array

            auto& sprcfg = cfg.uOAM[state.NumSprites];

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
            sprcfg.Mosaic = !!(attrib[0] & (1<<12));
            sprcfg.BGPrio = (attrib[2] >> 10) & 0x3;

            u32 tilenum = attrib[2] & 0x3FF;

            if (sprmode == 3)
            {
                // bitmap sprite

                if (unit->DispCnt & (1<<6))
                {
                    // 1D mapping
                    sprcfg.TileOffset = tilenum << (7 + ((CurUnit->DispCnt >> 22) & 0x1));
                    sprcfg.TileStride = width * 2;
                }
                else if (unit->DispCnt & (1<<5))
                {
                    // 2D mapping, 256 pixels
                    sprcfg.TileOffset = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                    sprcfg.TileStride = 256 * 2;
                }
                else
                {
                    // 2D mapping, 128 pixels
                    sprcfg.TileOffset = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                    sprcfg.TileStride = 128 * 2;
                }

                sprcfg.Type = 2;
                sprcfg.PalOffset = 1 + (attrib[2] >> 12); // alpha
            }
            else
            {
                if (unit->DispCnt & (1<<4))
                {
                    // 1D mapping
                    sprcfg.TileOffset = tilenum << (5 + ((unit->DispCnt >> 20) & 0x3));
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
                    if (unit->DispCnt & (1<<31))
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

            state.NumSprites++;
        }
    }

    glBindBuffer(GL_UNIFORM_BUFFER, SpriteConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    offsetof(sUnitState::sSpriteConfig, uOAM) + (state.NumSprites * sizeof(cfg.uOAM[0])),
                    &cfg);
}

void GLRenderer::UpdateCompositorConfig(Unit* unit)
{
    auto& state = UnitState[unit->Num];

    // compositor info buffer
    // TODO this could go with another buffer? optimize later
    for (int i = 0; i < 4; i++)
        state.CompositorConfig.uBGPrio[i] = -1;

    for (int layer = 0; layer < 4; layer++)
    {
        if (!(unit->DispCnt & (0x100 << layer)))
            continue;

        int prio = unit->BGCnt[layer] & 0x3;
        state.CompositorConfig.uBGPrio[layer] = prio;
    }

    // TODO should it account for the other bits?
    state.CompositorConfig.uEnableOBJ = !!(unit->DispCnt & (1<<12));

    state.CompositorConfig.uEnable3D = !!(unit->DispCnt & (1<<3));

    state.CompositorConfig.uBlendCnt = unit->BlendCnt;
    state.CompositorConfig.uBlendEffect = (unit->BlendCnt >> 6) & 0x3;
    state.CompositorConfig.uBlendCoef[0] = unit->EVA;
    state.CompositorConfig.uBlendCoef[1] = unit->EVB;
    state.CompositorConfig.uBlendCoef[2] = unit->EVY;

    glBindBuffer(GL_UNIFORM_BUFFER, CompositorConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(state.CompositorConfig), &state.CompositorConfig);
}


void GLRenderer::PrerenderSprites(Unit* unit)
{
    auto& state = UnitState[unit->Num];

    u16* vtxbuf = SpritePreVtxData;
    int vtxnum = 0;

    for (int i = 0; i < state.NumSprites; i++)
    {
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

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.SpriteFB);
    glViewport(0, 0, 1024, 512);

    // TODO upload VRAM shit

    glBindBuffer(GL_ARRAY_BUFFER, SpritePreVtxBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vtxnum * 3 * sizeof(u16), SpritePreVtxData);

    glBindVertexArray(SpritePreVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, vtxnum);
}

void GLRenderer::PrerenderLayer(Unit* unit, int layer)
{
    auto& state = UnitState[unit->Num];
    auto& cfg = state.LayerConfig.uBGConfig[layer];

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.BGLayerFB[layer]);

    glUniform1i(LayerPreCurBGULoc, layer);

    // set layer size
    glViewport(0, 0, cfg.Size[0], cfg.Size[1]);

    // TODO: set other layer parameters
    // maybe use uniform buffer/structure

    glBindBuffer(GL_ARRAY_BUFFER, RectVtxBuffer);
    glBindVertexArray(RectVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);
}


void GLRenderer::RenderSprites(Unit* unit, bool window, int ystart, int yend)
{
    auto& state = UnitState[unit->Num];

    glUseProgram(SpriteShader);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.FinalLayerFB[4]);
    glViewport(0, 0, ScreenW, ScreenH);

    // TODO enable scissor test when needed
    //glScissor(0, ystart * ScaleFactor, ScreenW, yend * ScaleFactor);

    if (window)
    {
        glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glColorMaski(1, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
    }
    else
    {
        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glColorMaski(1, GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    if (window)
    {
        if (!(unit->DispCnt & (1<<15)))
            return;
    }

    u16* vtxbuf = SpriteVtxData;
    int vtxnum = 0;

    for (int i = 0; i < state.NumSprites; i++)
    {
        auto& sprite = state.SpriteConfig.uOAM[i];

        bool iswin = (sprite.OBJMode == 2);
        if (iswin != window) continue;

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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.SpriteTex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput128Tex);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);

    glBindBuffer(GL_ARRAY_BUFFER, SpriteVtxBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vtxnum * 5 * sizeof(u16), SpriteVtxData);

    glBindVertexArray(SpriteVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, vtxnum);
}

void GLRenderer::RenderLayer(Unit* unit, int layer, int ystart, int yend)
{
    auto& state = UnitState[unit->Num];
    auto& cfg = state.LayerConfig.uBGConfig[layer];

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.FinalLayerFB[layer]);
    glViewport(0, 0, ScreenW, ScreenH);

    // TODO enable scissor test when needed
    //glScissor(0, ystart * ScaleFactor, ScreenW, yend * ScaleFactor);

    glUniform1i(LayerCurBGULoc, layer);

    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if ((unit->Num == 0) && (layer == 0) && unit->CaptureLatch && (unit->CaptureCnt & (1<<24)))
        glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    else
        glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, state.BGLayerTex);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _3DLayerTex);

    if (cfg.Type == 5)
    {
        glActiveTexture(GL_TEXTURE2);
        if (cfg.Size[0] == 128)
            glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput128Tex);
        else if (cfg.Size[0] == 256)
            glBindTexture(GL_TEXTURE_2D_ARRAY, CaptureOutput256Tex);
    }

    glBindBuffer(GL_ARRAY_BUFFER, RectVtxBuffer);
    glBindVertexArray(RectVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);
}

void GLRenderer::RenderScreen(Unit* unit, int ystart, int yend)
{
    auto& state = UnitState[unit->Num];

    glUseProgram(LayerPreShader);

    // update VRAM and palettes
    // TODO only update parts that are dirty

    u8* vram; u32 vrammask;
    unit->GetBGVRAM(vram, vrammask);
    u32 vramheight = (vrammask+1) >> 10;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.VRAMTex_BG);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, vramheight, GL_RED_INTEGER, GL_UNSIGNED_BYTE, vram);

    memcpy(&TempPalBuffer[0], &GPU.Palette[unit->Num ? 0x400 : 0], 256*2);
    for (int s = 0; s < 4; s++)
    {
        for (int p = 0; p < 16; p++)
        {
            u16* pal = unit->GetBGExtPal(s, p);
            memcpy(&TempPalBuffer[(1+((s*16)+p)) * 256], pal, 256*2);
        }
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, state.PalTex_BG);
    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+(4*16), GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, TempPalBuffer);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+(4*16), GL_RED_INTEGER, GL_UNSIGNED_SHORT, TempPalBuffer);

    UpdateLayerConfig(unit);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    PrerenderLayer(unit, 0);
    PrerenderLayer(unit, 1);
    PrerenderLayer(unit, 2);
    PrerenderLayer(unit, 3);

    if (unit->DispCnt & (1<<12))
    {
        // TODO only update parts that are dirty, too

        //u8* vram; u32 vrammask;
        unit->GetOBJVRAM(vram, vrammask);
        vramheight = (vrammask+1) >> 10;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state.VRAMTex_OBJ);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, vramheight, GL_RED_INTEGER, GL_UNSIGNED_BYTE, vram);

        memcpy(&TempPalBuffer[0], &GPU.Palette[unit->Num ? 0x600 : 0x200], 256*2);
        {
            u16* pal = unit->GetOBJExtPal();
            memcpy(&TempPalBuffer[256], pal, 256*16*2);
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, state.PalTex_OBJ);
        //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+16, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, TempPalBuffer);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+16, GL_RED_INTEGER, GL_UNSIGNED_SHORT, TempPalBuffer);

        int spr_ystart = state.LastSpriteLine;

        state.NumSprites = 0;
        UpdateOAM(unit, spr_ystart, yend);
        PrerenderSprites(unit);

        if (spr_ystart != 0)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, spr_ystart * ScaleFactor, ScreenW, (yend-spr_ystart) * ScaleFactor);
        }

        RenderSprites(unit, true, spr_ystart, yend);
        RenderSprites(unit, false, spr_ystart, yend);

        glDisable(GL_SCISSOR_TEST);
    }

    glUseProgram(LayerShader);

    // TODO not set this all the time!!
    glUniform1i(LayerScaleULoc, ScaleFactor);
    glUniform1i(LayerCurUnitULoc, unit->Num);

    // update scanline config buffer
    glBindBuffer(GL_UNIFORM_BUFFER, ScanlineConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(sUnitState::sScanlineConfig), &state.ScanlineConfig);

    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    RenderLayer(unit, 0, ystart, yend);
    RenderLayer(unit, 1, ystart, yend);
    RenderLayer(unit, 2, ystart, yend);
    RenderLayer(unit, 3, ystart, yend);


    glUseProgram(CompositorShader);

    // TODO not set this all the time?
    glUniform1i(CompositorScaleULoc, ScaleFactor);

    UpdateCompositorConfig(unit);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.OutputFB);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if ((unit->Num == 0) && unit->CaptureLatch && (!(unit->CaptureCnt & (1<<24))))
        glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    else
        glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glViewport(0, 0, ScreenW, ScreenH);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, state.FinalLayerTex);

    glBindBuffer(GL_ARRAY_BUFFER, RectVtxBuffer);
    glBindVertexArray(RectVtxArray);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);
}


void GLRenderer::AllocCapture(u32 bank, u32 start, u32 len)
{
    printf("GL: alloc capture in bank %d, start=%d len=%d\n", bank, start, len);
#if 0
    int block = (bank << 2) | start;
    int cur = CaptureLastBuffer[block] ^ 1;
    auto& capbuf = CaptureBuffers[block][cur];

    if (!capbuf.Texture)
    {
        printf("allocating new capture texture\n");
        glGenTextures(1, &capbuf.Texture);

        // TODO: textures should get reallocated if scale factor is changed?

        glBindTexture(GL_TEXTURE_2D, capbuf.Texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    if (len == 0)
    {
        capbuf.Width = 128;
        capbuf.Height = 128;
    }
    else
    {
        capbuf.Width = 256;
        capbuf.Height = 64 * len;
    }

    capbuf.Complete = false;
    ActiveCapture = block;

    // map it to the renderer, so it will get filled in
    glBindFramebuffer(GL_FRAMEBUFFER, FPOutputFB[BackBuffer]);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, capbuf.Texture, 0);
#endif
}

void GLRenderer::SyncVRAMCapture(u32 bank, u32 start, u32 len, bool complete)
{
    printf("SYNC VRAM CAPTURE: %d %d %d %d\n", bank, start, len, complete);

    if (!complete)
        printf("!!! READING VRAM AS IT IS BEING CAPTURED TO\n");

    u8* vram = GPU.VRAM[bank];

    if (len == 0) // 128x128
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureOutput128FB[(bank<<2) | start]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureSyncFB);
        glBlitFramebuffer(0, 0, 128 * ScaleFactor, 128 * ScaleFactor,
                          0, 0, 128, 128,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureSyncFB);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // TODO remove me

        glReadPixels(0, 0, 128, 128,
                     GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, &vram[start * 64 * 512]);

        for (u32 j = start * 64; j < (start+1) * 64; j++)
            GPU.VRAMDirty[bank][j] = true;
    }
    else
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureOutput256FB[bank]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureSyncFB);
        glBlitFramebuffer(0, 0, 256 * ScaleFactor, 256 * ScaleFactor,
                          0, 0, 256, 256,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, CaptureSyncFB);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // TODO remove me

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


bool GLRenderer::GetFramebuffers(u32** top, u32** bottom)
{
    int frontbuf = BackBuffer ^ 1;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, FPOutputTex[frontbuf][0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, FPOutputTex[frontbuf][1]);
    return false;
}

void GLRenderer::SwapBuffers()
{
    BackBuffer ^= 1;
}


void GLRenderer::DoCapture(Unit* unit)
{
    glUseProgram(CaptureShader);

    u32 capcnt = unit->CaptureCnt;
    u32 dstblock = (capcnt >> 16) & 0x3;
    u32 dstoffset = (capcnt >> 18) & 0x3;
    u32 capsize = (capcnt >> 20) & 0x3;

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

    if ((!(capcnt & (1<<25))) && (((unit->DispCnt >> 16) & 0x2) != 2))
        CaptureConfig.uSrcBOffset = 64 * ((capcnt >> 26) & 0x3);
    else
        CaptureConfig.uSrcBOffset = 0;

    CaptureConfig.uDstOffset = 64 * ((capcnt >> 18) & 0x3);
    CaptureConfig.uDstMode = (capcnt >> 29) & 0x3;
    CaptureConfig.uBlendFactors[0] = std::min(capcnt & 0x1F, 16u);
    CaptureConfig.uBlendFactors[1] = std::min((capcnt >> 8) & 0x1F, 16u);

    glBindBuffer(GL_UNIFORM_BUFFER, CaptureConfigUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CaptureConfig), &CaptureConfig);

    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, CaptureInputTex);

    glActiveTexture(GL_TEXTURE1);
    // TODO map something here!!

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


void GLRenderer::DrawSprites(u32 line, Unit* unit)
{
    auto& state = UnitState[unit->Num];
    u32 dirtymask = (1 << unit->Num);

    if (line == 0)
    {
        state.LastSpriteLine = 0;
        memcpy(state.OAM, &GPU.OAM[unit->Num ? 0x400 : 0], 0x400);
        GPU.OAMDirty &= ~dirtymask;
        return;
    }

    if (!(GPU.OAMDirty & dirtymask)) return;

    int ystart = state.LastSpriteLine;
    int yend = line;
    if (ystart == yend) return;

    if (unit->DispCnt & (1<<12))
    {
        // TODO only update parts that are dirty, too
        // TODO also check dirty flags up here

        u8* vram; u32 vrammask;
        unit->GetOBJVRAM(vram, vrammask);
        u32 vramheight = (vrammask+1) >> 10;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state.VRAMTex_OBJ);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, vramheight, GL_RED_INTEGER, GL_UNSIGNED_BYTE, vram);

        memcpy(&TempPalBuffer[0], &GPU.Palette[unit->Num ? 0x600 : 0x200], 256*2);
        {
            u16* pal = unit->GetOBJExtPal();
            memcpy(&TempPalBuffer[256], pal, 256*16*2);
        }

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, state.PalTex_OBJ);
        //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+16, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, TempPalBuffer);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1+16, GL_RED_INTEGER, GL_UNSIGNED_SHORT, TempPalBuffer);

        state.NumSprites = 0;
        UpdateOAM(unit, ystart, yend);
        PrerenderSprites(unit);

        glEnable(GL_SCISSOR_TEST);
        glScissor(0, ystart * ScaleFactor, ScreenW, (yend-ystart) * ScaleFactor);

        RenderSprites(unit, true, ystart, yend);
        RenderSprites(unit, false, ystart, yend);

        glDisable(GL_SCISSOR_TEST);
    }

    state.LastSpriteLine = yend;
    memcpy(state.OAM, &GPU.OAM[unit->Num ? 0x400 : 0], 0x400);
    GPU.OAMDirty &= ~dirtymask;
}

}
