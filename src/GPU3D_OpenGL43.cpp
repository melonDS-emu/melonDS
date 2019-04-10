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
PFNGLBUFFERSUBDATAPROC          glBufferSubData;

PFNGLGENVERTEXARRAYSPROC            glGenVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC         glDeleteVertexArrays;
PFNGLBINDVERTEXARRAYPROC            glBindVertexArray;
PFNGLENABLEVERTEXATTRIBARRAYPROC    glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC        glVertexAttribPointer;
PFNGLVERTEXATTRIBIPOINTERPROC        glVertexAttribIPointer;

PFNGLCREATESHADERPROC           glCreateShader;
PFNGLSHADERSOURCEPROC           glShaderSource;
PFNGLCOMPILESHADERPROC          glCompileShader;
PFNGLCREATEPROGRAMPROC          glCreateProgram;
PFNGLATTACHSHADERPROC           glAttachShader;
PFNGLLINKPROGRAMPROC            glLinkProgram;
PFNGLUSEPROGRAMPROC             glUseProgram;
PFNGLGETSHADERIVPROC            glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC       glGetShaderInfoLog;
PFNGLGETPROGRAMIVPROC           glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC      glGetProgramInfoLog;
PFNGLDELETESHADERPROC           glDeleteShader;
PFNGLDELETEPROGRAMPROC          glDeleteProgram;


const char* kRenderVS = R"(#version 400

layout(location=0) in uvec4 vPosition;

void main()
{
    // burp
    vec4 fpos;
    fpos.x = ((float(vPosition.x) / 256.0) * 2.0) - 1.0;
    fpos.y = ((float(vPosition.y) / 192.0) * 2.0) - 1.0;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
}
)";

const char* kRenderFS = R"(#version 400

out vec4 oColor;

void main()
{
    oColor = vec4(0, 63.0/255.0, 63.0/255.0, 31.0/255.0);
}
)";


GLuint RenderShader[3];

// vertex buffer
// * XYZW: 4x16bit
// * RGBA: 4x8bit
// * ST: 2x16bit
// * polygon data: 3x32bit (polygon/texture attributes)
//
// polygon attributes:
// * bit4-7, 11, 14-15, 24-29: POLYGON_ATTR
// * bit16-20: Z shift
// * bit8: front-facing (?)
// * bit9: W-buffering

GLuint VertexBufferID;
u32 VertexBuffer[10240 * 7];
u32 NumVertices;

GLuint VertexArrayID;
u16 IndexBuffer[2048 * 10];
u32 NumTriangles;

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
    LOADPROC(GLBUFFERSUBDATA, glBufferSubData);

    LOADPROC(GLGENVERTEXARRAYS, glGenVertexArrays);
    LOADPROC(GLDELETEVERTEXARRAYS, glDeleteVertexArrays);
    LOADPROC(GLBINDVERTEXARRAY, glBindVertexArray);
    LOADPROC(GLENABLEVERTEXATTRIBARRAY, glEnableVertexAttribArray);
    LOADPROC(GLVERTEXATTRIBPOINTER, glVertexAttribPointer);
    LOADPROC(GLVERTEXATTRIBIPOINTER, glVertexAttribIPointer);

    LOADPROC(GLCREATESHADER, glCreateShader);
    LOADPROC(GLSHADERSOURCE, glShaderSource);
    LOADPROC(GLCOMPILESHADER, glCompileShader);
    LOADPROC(GLCREATEPROGRAM, glCreateProgram);
    LOADPROC(GLATTACHSHADER, glAttachShader);
    LOADPROC(GLLINKPROGRAM, glLinkProgram);
    LOADPROC(GLUSEPROGRAM, glUseProgram);
    LOADPROC(GLGETSHADERIV, glGetShaderiv);
    LOADPROC(GLGETSHADERINFOLOG, glGetShaderInfoLog);
    LOADPROC(GLGETPROGRAMIV, glGetProgramiv);
    LOADPROC(GLGETPROGRAMINFOLOG, glGetProgramInfoLog);
    LOADPROC(GLDELETESHADER, glDeleteShader);
    LOADPROC(GLDELETEPROGRAM, glDeleteProgram);

#undef LOADPROC
    return true;
}

bool BuildShaderProgram(const char* vs, const char* fs, GLuint* ids, const char* name)
{
    int len;
    int res;

    ids[0] = glCreateShader(GL_VERTEX_SHADER);
    len = strlen(vs);
    glShaderSource(ids[0], 1, &vs, &len);
    glCompileShader(ids[0]);

    glGetShaderiv(ids[0], GL_COMPILE_STATUS, &res);
    if (res != GL_TRUE)
    {
        glGetShaderiv(ids[0], GL_INFO_LOG_LENGTH, &res);
        if (res < 1) res = 1024;
        char* log = new char[res+1];
        glGetShaderInfoLog(ids[0], res+1, NULL, log);
        printf("OpenGL: failed to compile vertex shader %s: %s\n", name, log);
        delete[] log;

        glDeleteShader(ids[0]);

        return false;
    }

    ids[1] = glCreateShader(GL_FRAGMENT_SHADER);
    len = strlen(fs);
    glShaderSource(ids[1], 1, &fs, &len);
    glCompileShader(ids[1]);

    glGetShaderiv(ids[1], GL_COMPILE_STATUS, &res);
    if (res != GL_TRUE)
    {
        glGetShaderiv(ids[1], GL_INFO_LOG_LENGTH, &res);
        if (res < 1) res = 1024;
        char* log = new char[res+1];
        glGetShaderInfoLog(ids[1], res+1, NULL, log);
        printf("OpenGL: failed to compile fragment shader %s: %s\n", name, log);
        delete[] log;

        glDeleteShader(ids[0]);
        glDeleteShader(ids[1]);

        return false;
    }

    ids[2] = glCreateProgram();
    glAttachShader(ids[2], ids[0]);
    glAttachShader(ids[2], ids[1]);
    glLinkProgram(ids[2]);

    glGetProgramiv(ids[2], GL_LINK_STATUS, &res);
    if (res != GL_TRUE)
    {
        glGetProgramiv(ids[2], GL_INFO_LOG_LENGTH, &res);
        if (res < 1) res = 1024;
        char* log = new char[res+1];
        glGetProgramInfoLog(ids[2], res+1, NULL, log);
        printf("OpenGL: failed to link program %s: %s\n", name, log);
        delete[] log;

        glDeleteShader(ids[0]);
        glDeleteShader(ids[1]);
        glDeleteProgram(ids[2]);

        return false;
    }

    return true;
}

bool Init()
{
    if (!InitGLExtensions()) return false;

    const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString(GL_VERSION); // version as a string
    printf("OpenGL: renderer: %s\n", renderer);
    printf("OpenGL: version: %s\n", version);


    // TODO: make configurable (hires, etc)
    glViewport(0, 0, 256, 192);
    glDepthRange(0, 1);


    if (!BuildShaderProgram(kRenderVS, kRenderFS, RenderShader, "RenderShader"))
        return false;


    glGenBuffers(1, &VertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexBuffer), NULL, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribIPointer(0, 4, GL_UNSIGNED_SHORT, 7*4, (void*)(0));
    glEnableVertexAttribArray(1); // color
    glVertexAttribIPointer(1, 4, GL_UNSIGNED_BYTE, 7*4, (void*)(2*4));
    glEnableVertexAttribArray(2); // texcoords
    glVertexAttribIPointer(2, 2, GL_UNSIGNED_SHORT, 7*4, (void*)(3*4));
    glEnableVertexAttribArray(3); // attrib
    glVertexAttribIPointer(3, 3, GL_UNSIGNED_INT, 7*4, (void*)(4*4));


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
    //glBufferData(GL_PIXEL_PACK_BUFFER, 256*48*4, NULL, GL_DYNAMIC_READ);
    glBufferData(GL_PIXEL_PACK_BUFFER, 256*192*4, NULL, GL_DYNAMIC_READ);

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


void BuildPolygons(Polygon** polygons, int npolys)
{
    u32* vptr = &VertexBuffer[0];
    u32 vidx = 0;

    u16* iptr = &IndexBuffer[0];
    u32 numtriangles = 0;

    for (int i = 0; i < npolys; i++)
    {
        Polygon* poly = polygons[i];
        if (poly->Degenerate) continue;

        u32 vidx_first = vidx;

        u32 polyattr = poly->Attr;

        u32 alpha = (polyattr >> 16) & 0x1F;

        u32 vtxattr = polyattr & 0x1F00C8F0;
        if (poly->FacingView) vtxattr |= (1<<8);
        if (poly->WBuffer)    vtxattr |= (1<<9);

        // assemble vertices
        for (int j = 0; j < poly->NumVertices; j++)
        {
            Vertex* vtx = poly->Vertices[j];

            u32 z = poly->FinalZ[j];
            u32 w = poly->FinalW[j];

            // Z should always fit within 16 bits, so it's okay to do this
            u32 zshift = 0;
            while (z > 0xFFFF) { z >>= 1; zshift++; }

            // TODO hires-upgraded positions?
            *vptr++ = vtx->FinalPosition[0] | (vtx->FinalPosition[1] << 16);
            *vptr++ = z | (w << 16);

            *vptr++ =  (vtx->FinalColor[0] >> 1) |
                      ((vtx->FinalColor[1] >> 1) << 8) |
                      ((vtx->FinalColor[2] >> 1) << 16) |
                      (alpha << 24);

            *vptr++ = vtx->TexCoords[0] | (vtx->TexCoords[1] << 16);

            *vptr++ = vtxattr | (zshift << 16);
            *vptr++ = poly->TexParam;
            *vptr++ = poly->TexPalette;

            if (j >= 2)
            {
                // build a triangle
                *iptr++ = vidx_first;
                *iptr++ = vidx - 1;
                *iptr++ = vidx;
                numtriangles++;
            }

            vidx++;
        }
    }

    NumTriangles = numtriangles;
    NumVertices = vidx;
}


void VCount144()
{
}

void RenderFrame()
{
    // TODO: proper clear color!!
    glClearColor(0, 0, 0, 31.0/255.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // render shit here
    glUseProgram(RenderShader[2]);

    BuildPolygons(&RenderPolygonRAM[0], RenderNumPolygons);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
    glBufferSubData(GL_ARRAY_BUFFER, 0, NumVertices*7*4, VertexBuffer);

    glBindVertexArray(VertexArrayID);
    glDrawElements(GL_TRIANGLES, NumTriangles, GL_UNSIGNED_SHORT, IndexBuffer);


    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    //glReadPixels(0, 0, 256, 48, GL_RGBA, GL_UNSIGNED_BYTE, Framebuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
    //glReadPixels(0, 0, 256, 48, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
}

void RequestLine(int line)
{
    if (line == 0)
    {
        u8* data = (u8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        //if (data) memcpy(&Framebuffer[4*256*0], data, 4*256*48);
        if (data) memcpy(&Framebuffer[4*256*0], data, 4*256*192);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

        //glReadPixels(0, 48, 256, 48, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }
    /*else if (line == 48)
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
    }*/
}

u32* GetLine(int line)
{
    return (u32*)&Framebuffer[256*4 * line];
}

}
}
