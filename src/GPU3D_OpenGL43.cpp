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

PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture;


GLuint FramebufferID;
u8 Framebuffer[256*192*4];
u8 CurLine[256*4];


bool InitGLExtensions()
{
#define LOADPROC(type, name)  \
    name = (PFN##type##PROC)Platform::GL_GetProcAddress(#name); \
    if (!name) return false;

    LOADPROC(GLGENFRAMEBUFFERS, glGenFramebuffers);
    LOADPROC(GLBINDFRAMEBUFFER, glBindFramebuffer);
    LOADPROC(GLFRAMEBUFFERTEXTURE, glFramebufferTexture);

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
                *ptr++ = 0x00;
                *ptr++ = 0x00;
                *ptr++ = 0x3F;
                *ptr++ = 0x1F;
            }
            else
            {
                *ptr++ = 0;
                *ptr++ = y>>2;
                *ptr++ = 0x3F;
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
    //
}

void RequestLine(int line)
{
    //
}

u32* GetLine(int line)
{
    if (line == 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 256, 192, GL_RGBA, GL_UNSIGNED_BYTE, Framebuffer);
    }

    return (u32*)&Framebuffer[256*4 * line];
}

}
}
