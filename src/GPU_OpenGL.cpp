/*
    Copyright 2016-2020 Arisotura

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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GPU.h"
#include "Config.h"
#include "OpenGLSupport.h"
#include "GPU_OpenGL_shaders.h"

namespace GPU
{
namespace GLCompositor
{

using namespace OpenGL;

int Scale;
int ScreenH, ScreenW;

GLuint CompShader[1][3];
GLuint CompScaleLoc[1];

GLuint CompVertexBufferID;
GLuint CompVertexArrayID;
float CompVertices[2 * 3*2 * 2]; // position

GLuint CompScreenInputTex;
GLuint CompScreenOutputTex;
GLuint CompScreenOutputFB;

GLuint CaptureTex[4][2];
GLuint CaptureFB[4][2];
int CaptureSet[4];
u32 CaptureCnt;


bool Init()
{
    if (!OpenGL::BuildShaderProgram(kCompositorVS, kCompositorFS_Nearest, CompShader[0], "CompositorShader"))
    //if (!OpenGL::BuildShaderProgram(kCompositorVS, kCompositorFS_Linear, CompShader[0], "CompositorShader"))
    //if (!OpenGL::BuildShaderProgram(kCompositorVS_xBRZ, kCompositorFS_xBRZ, CompShader[0], "CompositorShader"))
        return false;

    for (int i = 0; i < 1; i++)
    {
        GLint uni_id;

        glBindAttribLocation(CompShader[i][2], 0, "vPosition");
        glBindFragDataLocation(CompShader[i][2], 0, "oColor");

        if (!OpenGL::LinkShaderProgram(CompShader[i]))
            return false;

        CompScaleLoc[i] = glGetUniformLocation(CompShader[i][2], "u3DScale");

        glUseProgram(CompShader[i][2]);
        uni_id = glGetUniformLocation(CompShader[i][2], "ScreenTex");
        glUniform1i(uni_id, 0);
        uni_id = glGetUniformLocation(CompShader[i][2], "_3DTex");
        glUniform1i(uni_id, 1);
        uni_id = glGetUniformLocation(CompShader[i][2], "CaptureTex");
        glUniform1i(uni_id, 2);
    }

#define SETVERTEX(i, x, y) \
    CompVertices[2*(i) + 0] = x; \
    CompVertices[2*(i) + 1] = y;

    // top screen
    SETVERTEX(0, -1, 1);
    SETVERTEX(1, 1, 0);
    SETVERTEX(2, 1, 1);
    SETVERTEX(3, -1, 1);
    SETVERTEX(4, -1, 0);
    SETVERTEX(5, 1, 0);

    // bottom screen
    SETVERTEX(6, -1, 0);
    SETVERTEX(7, 1, -1);
    SETVERTEX(8, 1, 0);
    SETVERTEX(9, -1, 0);
    SETVERTEX(10, -1, -1);
    SETVERTEX(11, 1, -1);

#undef SETVERTEX

    glGenBuffers(1, &CompVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, CompVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CompVertices), CompVertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &CompVertexArrayID);
    glBindVertexArray(CompVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*4, (void*)(0));

    glGenFramebuffers(1, &CompScreenOutputFB);

    glGenTextures(1, &CompScreenInputTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, CompScreenInputTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 256*3 + 1, 192*2, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);

    glGenTextures(1, &CompScreenOutputTex);
    glBindTexture(GL_TEXTURE_2D, CompScreenOutputTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(8, &CaptureFB[0][0]);
    glGenTextures(8, &CaptureTex[0][0]);
    for (int i = 0; i < 8; i++)
    {
        glBindTexture(GL_TEXTURE_2D, CaptureTex[i>>1][i&0x1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void DeInit()
{
    glDeleteFramebuffers(8, &CaptureFB[0][0]);
    glDeleteTextures(8, &CaptureTex[0][0]);

    glDeleteFramebuffers(1, &CompScreenOutputFB);
    glDeleteTextures(1, &CompScreenInputTex);
    glDeleteTextures(1, &CompScreenOutputTex);

    glDeleteVertexArrays(1, &CompVertexArrayID);
    glDeleteBuffers(1, &CompVertexBufferID);

    for (int i = 0; i < 1; i++)
        OpenGL::DeleteShaderProgram(CompShader[i]);
}

void Reset()
{
    for (int i = 0; i < 4; i++)
        CaptureSet[i] = 0;

    CaptureCnt = 0;
}


void SetRenderSettings(RenderSettings& settings)
{
    int scale = settings.GL_ScaleFactor;

    Scale = scale;
    ScreenW = 256 * scale;
    ScreenH = 384 * scale;

    glBindTexture(GL_TEXTURE_2D, CompScreenOutputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    GLenum fbassign[] = {GL_COLOR_ATTACHMENT0};
    glBindFramebuffer(GL_FRAMEBUFFER, CompScreenOutputFB);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CompScreenOutputTex, 0);
    glDrawBuffers(1, fbassign);

    for (int i = 0; i < 8; i++)
    {
        glBindTexture(GL_TEXTURE_2D, CaptureTex[i>>1][i&0x1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenW, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glBindFramebuffer(GL_FRAMEBUFFER, CaptureFB[i>>1][i&0x1]);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CaptureTex[i>>1][i&0x1], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void DoCapture(u32 capcnt)
{
    u32 dstvram = (capcnt >> 16) & 0x3;

    // TODO: blank out destination buffer?
    if (!(GPU::VRAMMap_LCDC & (1<<dstvram)))
        return;

    if ((capcnt & 0xEF000000) != 0x80000000)
    {
        printf("GLCompositor: capture type %08X no good, can't accelerate\n", capcnt);
        return;
    }

    CaptureCnt = capcnt;
}

void RenderFrame()
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CompScreenOutputFB);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(0, 0, ScreenW, ScreenH);

    // TODO: select more shaders (filtering, etc)
    OpenGL::UseShaderProgram(CompShader[0]);
    glUniform1ui(CompScaleLoc[0], Scale);

    int frontbuf = GPU::FrontBuffer;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, CompScreenInputTex);

    if (GPU::Framebuffer[frontbuf][0] && GPU::Framebuffer[frontbuf][1])
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256*3 + 1, 192, GL_RGBA_INTEGER,
                        GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256*3 + 1, 192, GL_RGBA_INTEGER,
                        GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][1]);
    }

    glActiveTexture(GL_TEXTURE1);
    GPU3D::GLRenderer::SetupAccelFrame();

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, CaptureTex[3][CaptureSet[3]]);

    glBindBuffer(GL_ARRAY_BUFFER, CompVertexBufferID);
    glBindVertexArray(CompVertexArrayID);
    glDrawArrays(GL_TRIANGLES, 0, 4*3);

    // take care of capture as needed
    if (CaptureCnt & (1<<31))
    {
        u32 dstvram = (CaptureCnt >> 16) & 0x3;

        CaptureSet[dstvram] ^= 1;
        int cset = CaptureSet[dstvram];

        u32 capwidth, capheight;
        switch ((CaptureCnt >> 20) & 0x3)
        {
        case 0: capwidth = 128; capheight = 128; break;
        case 1: capwidth = 256; capheight = 64;  break;
        case 2: capwidth = 256; capheight = 128; break;
        case 3: capwidth = 256; capheight = 192; break;
        }

        u32 yoffset = ((CaptureCnt >> 18) & 0x3) * 64;

        capwidth *= Scale;
        capheight *= Scale;
        yoffset *= Scale;

        // TODO: other sources!!
        glBindFramebuffer(GL_READ_FRAMEBUFFER, CompScreenOutputFB);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CaptureFB[dstvram][cset]);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        // TODO: offset and shit!
        glBlitFramebuffer(0, 192*Scale, capwidth, 192*Scale+capheight, 0, 0, capwidth, capheight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        CaptureCnt = 0;
    }

    glFlush();
}

void BindOutputTexture()
{
    glBindTexture(GL_TEXTURE_2D, CompScreenOutputTex);
}

}
}
