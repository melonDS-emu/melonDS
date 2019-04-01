/*
    Copyright 2016-2019 Arisotura

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

#include <GL/gl.h>
#include <GL/glext.h>

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GPU.h"
#include "Platform.h"

namespace GPU3D
{
namespace GLRenderer43
{

PFNGLGENFRAMEBUFFERSPROC        glGenFramebuffers;
PFNGLDELETEFRAMEBUFFERSPROC     glDeleteFramebuffers;
PFNGLBINDFRAMEBUFFERPROC        glBindFramebuffer;
PFNGLFRAMEBUFFERTEXTUREPROC     glFramebufferTexture;

PFNGLGENBUFFERSPROC             glGenBuffers;
PFNGLDELETEBUFFERSPROC          glDeleteBuffers;
PFNGLBINDBUFFERPROC             glBindBuffer;
PFNGLMAPBUFFERPROC              glMapBuffer;
PFNGLMAPBUFFERRANGEPROC         glMapBufferRange;
PFNGLUNMAPBUFFERPROC            glUnmapBuffer;
PFNGLBUFFERDATAPROC             glBufferData;


GLuint FramebufferID, PixelbufferID;
u8 Framebuffer[256*192*4];
u8 CurLine[256*4];


bool InitGLExtensions()
{
#define LOADPROC(type, name)  \
    name = (PFN##type##PROC)Platform::GL_GetProcAddress(#name); \
    if (!name) { printf("OpenGL: " #name " not found\n"); return false; }

    LOADPROC(GLGENFRAMEBUFFERS, glGenFramebuffers);
    LOADPROC(GLDELETEFRAMEBUFFERS, glDeleteFramebuffers);
    LOADPROC(GLBINDFRAMEBUFFER, glBindFramebuffer);
    LOADPROC(GLFRAMEBUFFERTEXTURE, glFramebufferTexture);

    LOADPROC(GLGENBUFFERS, glGenBuffers);
    LOADPROC(GLDELETEBUFFERS, glDeleteBuffers);
    LOADPROC(GLBINDBUFFER, glBindBuffer);
    LOADPROC(GLMAPBUFFER, glMapBuffer);
    LOADPROC(GLMAPBUFFERRANGE, glMapBufferRange);
    LOADPROC(GLUNMAPBUFFER, glUnmapBuffer);
    LOADPROC(GLBUFFERDATA, glBufferData);

#undef LOADPROC
    return true;
}

bool Init()
{
    if (!InitGLExtensions()) return false;

    u8* test_tex = new u8[256*192*4];
    u8* ptr = test_tex;
    for (int y = 0; y < 192; y++)
    {
        for (int x = 0; x < 256; x++)
        {
            if ((x & 0x10) ^ (y & 0x10))
            {
                *ptr++ = 0x3F;
                *ptr++ = 0x00;
                *ptr++ = 0;
                *ptr++ = 0x1F;
            }
            else
            {
                *ptr++ = 0x3F;
                *ptr++ = y>>2;
                *ptr++ = 0;
                *ptr++ = 0x1F;
            }
        }
    }

    glGenFramebuffers(1, &FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);

    GLuint frametex;
    glGenTextures(1, &frametex);
    glBindTexture(GL_TEXTURE_2D, frametex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, test_tex);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, frametex, 0);

    glGenBuffers(1, &PixelbufferID);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
    glBufferData(GL_PIXEL_PACK_BUFFER, 256*48*4, NULL, GL_DYNAMIC_READ);

    return true;
}

void DeInit()
{
    //
}

void Reset()
{
    //
}


void VCount144()
{
}

void RenderFrame()
{
    // render shit here

    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    //glReadPixels(0, 0, 256, 48, GL_RGBA, GL_UNSIGNED_BYTE, Framebuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
    glReadPixels(0, 0, 256, 48, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
}

void RequestLine(int line)
{
    if (line == 0)
    {
        u8* data = (u8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (data) memcpy(&Framebuffer[4*256*0], data, 4*256*48);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

        glReadPixels(0, 48, 256, 48, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }
    else if (line == 48)
    {
        u8* data = (u8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (data) memcpy(&Framebuffer[4*256*48], data, 4*256*48);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

        glReadPixels(0, 96, 256, 48, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }
    else if (line == 96)
    {
        u8* data = (u8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (data) memcpy(&Framebuffer[4*256*96], data, 4*256*48);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

        glReadPixels(0, 144, 256, 48, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }
    else if (line == 144)
    {
        u8* data = (u8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (data) memcpy(&Framebuffer[4*256*144], data, 4*256*48);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
}

u32* GetLine(int line)
{
    return (u32*)&Framebuffer[256*4 * line];
}

}
}
