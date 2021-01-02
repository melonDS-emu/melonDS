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
GLuint Comp3DXPosLoc[1];

GLuint CompVertexBufferID;
GLuint CompVertexArrayID;

struct CompVertex
{
    float Position[2];
    float Texcoord[2];
};
CompVertex CompVertices[2 * 3*2];

GLuint CompScreenInputTex;
GLuint CompScreenOutputTex;
GLuint CompScreenOutputFB;


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
        glBindAttribLocation(CompShader[i][2], 1, "vTexcoord");
        glBindFragDataLocation(CompShader[i][2], 0, "oColor");

        if (!OpenGL::LinkShaderProgram(CompShader[i]))
            return false;

        CompScaleLoc[i] = glGetUniformLocation(CompShader[i][2], "u3DScale");
        Comp3DXPosLoc[i] = glGetUniformLocation(CompShader[i][2], "u3DXPos");

        glUseProgram(CompShader[i][2]);
        uni_id = glGetUniformLocation(CompShader[i][2], "ScreenTex");
        glUniform1i(uni_id, 0);
        uni_id = glGetUniformLocation(CompShader[i][2], "_3DTex");
        glUniform1i(uni_id, 1);
    }

    // all this mess is to prevent bleeding
#define SETVERTEX(i, x, y, offset) \
    CompVertices[i].Position[0] = x; \
    CompVertices[i].Position[1] = y + offset; \
    CompVertices[i].Texcoord[0] = (x + 1.f) * (256.f / 2.f); \
    CompVertices[i].Texcoord[1] = (y + 1.f) * (384.f / 2.f)

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

    glGenBuffers(1, &CompVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, CompVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CompVertices), CompVertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &CompVertexArrayID);
    glBindVertexArray(CompVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(CompVertex), (void*)(offsetof(CompVertex, Position)));
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(CompVertex), (void*)(offsetof(CompVertex, Texcoord)));

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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void DeInit()
{
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
}


void SetRenderSettings(RenderSettings& settings)
{
    int scale = settings.GL_ScaleFactor;

    Scale = scale;
    ScreenW = 256 * scale;
    ScreenH = (384+2) * scale;

    glBindTexture(GL_TEXTURE_2D, CompScreenOutputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    // fill the padding
    u8 zeroPixels[ScreenW*2*scale*4];
    memset(zeroPixels, 0, sizeof(zeroPixels));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192*scale, ScreenW, 2*scale, GL_RGBA, GL_UNSIGNED_BYTE, zeroPixels);

    GLenum fbassign[] = {GL_COLOR_ATTACHMENT0};
    glBindFramebuffer(GL_FRAMEBUFFER, CompScreenOutputFB);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, CompScreenOutputTex, 0);
    glDrawBuffers(1, fbassign);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    // TODO: support setting this midframe, if ever needed
    glUniform1i(Comp3DXPosLoc[0], ((int)GPU3D::RenderXPos << 23) >> 23);

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

    glBindBuffer(GL_ARRAY_BUFFER, CompVertexBufferID);
    glBindVertexArray(CompVertexArrayID);
    glDrawArrays(GL_TRIANGLES, 0, 4*3);

    glFlush();
}

void BindOutputTexture()
{
    glBindTexture(GL_TEXTURE_2D, CompScreenOutputTex);
}

}
}
