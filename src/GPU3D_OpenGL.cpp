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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GPU.h"
#include "OpenGLSupport.h"
#include "GPU3D_OpenGL_shaders.h"

namespace GPU3D
{
namespace GLRenderer
{

// GL version requirements
// * texelFetch: 3.0 (GLSL 1.30)     (3.2/1.50 for MS)
// * UBO: 3.1


enum
{
    RenderFlag_WBuffer     = 0x01,
    RenderFlag_Trans       = 0x02,
    RenderFlag_ShadowMask  = 0x04,
};


GLuint ClearShaderPlain[3];

GLuint RenderShader[16][3];

struct
{
    float uScreenSize[2];
    u32 uDispCnt;
    u32 __pad0;
    float uToonColors[32][4];

} ShaderConfig;

GLuint ShaderConfigUBO;

typedef struct
{
    Polygon* PolyData;

    u16* Indices;
    u32 RenderKey;

} RendererPolygon;

RendererPolygon PolygonList[2048];
int NumFinalPolys, NumOpaqueFinalPolys;

GLuint ClearVertexBufferID, ClearVertexArrayID;
GLint ClearUniformLoc[4];

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
// * bit9: W-buffering (?)

GLuint VertexBufferID;
u32 VertexBuffer[10240 * 7];
u32 NumVertices;

GLuint VertexArrayID;
u16 IndexBuffer[2048 * 10];
u32 NumTriangles;

GLuint TexMemID;
GLuint TexPalMemID;

int ScaleFactor;
bool Accelerated;
int ScreenW, ScreenH;

GLuint FramebufferTex[8];
int FrontBuffer;
GLuint FramebufferID[4], PixelbufferID;
u32* Framebuffer = NULL;



bool BuildRenderShader(u32 flags, const char* vs, const char* fs)
{
    char shadername[32];
    sprintf(shadername, "RenderShader%02X", flags);

    int headerlen = strlen(kShaderHeader);

    int vslen = strlen(vs);
    int vsclen = strlen(kRenderVSCommon);
    char* vsbuf = new char[headerlen + vsclen + vslen + 1];
    strcpy(&vsbuf[0], kShaderHeader);
    strcpy(&vsbuf[headerlen], kRenderVSCommon);
    strcpy(&vsbuf[headerlen + vsclen], vs);

    int fslen = strlen(fs);
    int fsclen = strlen(kRenderFSCommon);
    char* fsbuf = new char[headerlen + fsclen + fslen + 1];
    strcpy(&fsbuf[0], kShaderHeader);
    strcpy(&fsbuf[headerlen], kRenderFSCommon);
    strcpy(&fsbuf[headerlen + fsclen], fs);

    bool ret = OpenGL_BuildShaderProgram(vsbuf, fsbuf, RenderShader[flags], shadername);

    delete[] vsbuf;
    delete[] fsbuf;

    if (!ret) return false;

    GLuint prog = RenderShader[flags][2];

    GLint uni_id = glGetUniformBlockIndex(prog, "uConfig");
    glUniformBlockBinding(prog, uni_id, 0);

    glBindAttribLocation(prog, 0, "vPosition");
    glBindAttribLocation(prog, 1, "vColor");
    glBindAttribLocation(prog, 2, "vTexcoord");
    glBindAttribLocation(prog, 3, "vPolygonAttr");
    glBindFragDataLocation(prog, 0, "oColor");
    glBindFragDataLocation(prog, 1, "oAttr");

    glUseProgram(prog);

    uni_id = glGetUniformLocation(prog, "TexMem");
    glUniform1i(uni_id, 0);
    uni_id = glGetUniformLocation(prog, "TexPalMem");
    glUniform1i(uni_id, 1);

    return true;
}

void UseRenderShader(u32 flags)
{
    glUseProgram(RenderShader[flags][2]);
}

void SetupDefaultTexParams(GLuint tex)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

bool Init()
{
    const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString(GL_VERSION); // version as a string
    printf("OpenGL: renderer: %s\n", renderer);
    printf("OpenGL: version: %s\n", version);

    int barg1, barg2;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &barg1);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &barg2);
    printf("max texture: %d\n", barg1);
    printf("max comb. texture: %d\n", barg2);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &barg1);
    printf("max tex size: %d\n", barg1);

    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &barg1);
    printf("max arraytex levels: %d\n", barg1);

    /*glGetIntegerv(GL_NUM_EXTENSIONS, &barg1);
    printf("extensions: %d\n", barg1);
    for (int i = 0; i < barg1; i++)
    {
        const GLubyte* ext = glGetStringi(GL_EXTENSIONS, i);
        printf("- %s\n", ext);
    }*/

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);


    glDepthRange(0, 1);
    glClearDepth(1.0);


    if (!OpenGL_BuildShaderProgram(kClearVS, kClearFS, ClearShaderPlain, "ClearShader"))
        return false;

    glBindAttribLocation(ClearShaderPlain[2], 0, "vPosition");
    glBindFragDataLocation(ClearShaderPlain[2], 0, "oColor");
    glBindFragDataLocation(ClearShaderPlain[2], 1, "oAttr");
    ClearUniformLoc[0] = glGetUniformLocation(ClearShaderPlain[2], "uColor");
    ClearUniformLoc[1] = glGetUniformLocation(ClearShaderPlain[2], "uDepth");
    ClearUniformLoc[2] = glGetUniformLocation(ClearShaderPlain[2], "uOpaquePolyId");
    ClearUniformLoc[3] = glGetUniformLocation(ClearShaderPlain[2], "uFogFlag");

    memset(RenderShader, 0, sizeof(RenderShader));

    if (!BuildRenderShader(0,
                           kRenderVS_Z, kRenderFS_ZO)) return false;
    if (!BuildRenderShader(RenderFlag_WBuffer,
                           kRenderVS_W, kRenderFS_WO)) return false;
    if (!BuildRenderShader(RenderFlag_Trans,
                           kRenderVS_Z, kRenderFS_ZT)) return false;
    if (!BuildRenderShader(RenderFlag_Trans | RenderFlag_WBuffer,
                           kRenderVS_W, kRenderFS_WT)) return false;
    if (!BuildRenderShader(RenderFlag_ShadowMask,
                           kRenderVS_Z, kRenderFS_ZSM)) return false;
    if (!BuildRenderShader(RenderFlag_ShadowMask | RenderFlag_WBuffer,
                           kRenderVS_W, kRenderFS_WSM)) return false;


    memset(&ShaderConfig, 0, sizeof(ShaderConfig));

    glGenBuffers(1, &ShaderConfigUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, ShaderConfigUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ShaderConfig), &ShaderConfig, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ShaderConfigUBO);


    float clearvtx[6*2] =
    {
        -1.0, -1.0,
        1.0, 1.0,
        -1.0, 1.0,

        -1.0, -1.0,
        1.0, -1.0,
        1.0, 1.0
    };

    glGenBuffers(1, &ClearVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, ClearVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(clearvtx), clearvtx, GL_STATIC_DRAW);

    glGenVertexArrays(1, &ClearVertexArrayID);
    glBindVertexArray(ClearVertexArrayID);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)(0));


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
    glVertexAttribIPointer(2, 2, GL_SHORT, 7*4, (void*)(3*4));
    glEnableVertexAttribArray(3); // attrib
    glVertexAttribIPointer(3, 3, GL_UNSIGNED_INT, 7*4, (void*)(4*4));


    glGenFramebuffers(4, &FramebufferID[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[0]);

    glGenTextures(8, &FramebufferTex[0]);
    FrontBuffer = 0;

    // color buffers
    SetupDefaultTexParams(FramebufferTex[0]);
    SetupDefaultTexParams(FramebufferTex[1]);

    // depth/stencil buffer
    SetupDefaultTexParams(FramebufferTex[4]);
    SetupDefaultTexParams(FramebufferTex[6]);

    // attribute buffer
    // R: opaque polyID (for edgemarking)
    // G: opaque polyID (for shadows, suppressed when rendering translucent polygons)
    // B: stencil flag
    SetupDefaultTexParams(FramebufferTex[5]);
    SetupDefaultTexParams(FramebufferTex[7]);

    // downscale framebuffer for antialiased mode
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[2]);
    SetupDefaultTexParams(FramebufferTex[2]);

    // downscale framebuffer for display capture (always 256x192)
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[3]);
    SetupDefaultTexParams(FramebufferTex[3]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FramebufferTex[3], 0);

    GLenum fbassign[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[0]);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FramebufferTex[0], 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, FramebufferTex[4], 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, FramebufferTex[5], 0);
    glDrawBuffers(2, fbassign);

    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[1]);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FramebufferTex[1], 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, FramebufferTex[6], 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, FramebufferTex[7], 0);
    glDrawBuffers(2, fbassign);

    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[0]);

    glEnable(GL_BLEND);
    glBlendFuncSeparatei(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquationSeparatei(0, GL_ADD, GL_MAX);
    glBlendFuncSeparatei(1, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);

    glGenBuffers(1, &PixelbufferID);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &TexMemID);
    glBindTexture(GL_TEXTURE_2D, TexMemID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 1024, 512, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);

    glActiveTexture(GL_TEXTURE1);
    glGenTextures(1, &TexPalMemID);
    glBindTexture(GL_TEXTURE_2D, TexPalMemID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1024, 48, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);

    return true;
}

void DeInit()
{
    glDeleteTextures(1, &TexMemID);
    glDeleteTextures(1, &TexPalMemID);

    glDeleteFramebuffers(4, &FramebufferID[0]);
    glDeleteTextures(8, &FramebufferTex[0]);

    glDeleteVertexArrays(1, &VertexArrayID);
    glDeleteBuffers(1, &VertexBufferID);
    glDeleteVertexArrays(1, &ClearVertexArrayID);
    glDeleteBuffers(1, &ClearVertexBufferID);

    glDeleteBuffers(1, &ShaderConfigUBO);

    for (int i = 0; i < 16; i++)
    {
        if (!RenderShader[i][2]) continue;
        OpenGL_DeleteShaderProgram(RenderShader[i]);
    }
}

void Reset()
{
    //
}

void SetDisplaySettings(int scale, bool accel)
{
    ScaleFactor = scale;
    Accelerated = accel;

    // TODO: antialiasing setting
    ScreenW = 256 << scale;
    ScreenH = 192 << scale;

    glBindTexture(GL_TEXTURE_2D, FramebufferTex[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW, ScreenH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenW/2, ScreenH/2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[4]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, ScreenW, ScreenH, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[5]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8UI, ScreenW, ScreenH, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[6]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, ScreenW, ScreenH, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[7]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8UI, ScreenW, ScreenH, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, NULL);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
    glBufferData(GL_PIXEL_PACK_BUFFER, 256*192*4, NULL, GL_DYNAMIC_READ);

    if (Framebuffer) delete[] Framebuffer;
    if (accel) Framebuffer = new u32[256*192];
    else       Framebuffer = new u32[ScreenW*ScreenH];
}

int GetScale()
{
    return ScaleFactor;
}


void SetupPolygon(RendererPolygon* rp, Polygon* polygon)
{
    rp->PolyData = polygon;

    // render key: depending on what we're drawing
    // opaque polygons:
    // - depthfunc
    // -- alpha=0
    // regular translucent polygons:
    // - depthfunc
    // -- depthwrite
    // --- polyID
    // shadow mask polygons:
    // - depthfunc?????
    // shadow polygons:
    // - depthfunc
    // -- depthwrite
    // --- polyID

    rp->RenderKey = (polygon->Attr >> 14) & 0x1; // bit14 - depth func
    if (!polygon->IsShadowMask)
    {
        if (polygon->Translucent)
        {
            if (polygon->IsShadow) rp->RenderKey |= 0x20000;
            else                   rp->RenderKey |= 0x10000;
            rp->RenderKey |= (polygon->Attr >> 10) & 0x2; // bit11 - depth write
            rp->RenderKey |= (polygon->Attr & 0x3F000000) >> 16; // polygon ID
        }
        else
        {
            if ((polygon->Attr & 0x001F0000) == 0)
                rp->RenderKey |= 0x2;
            rp->RenderKey |= (polygon->Attr & 0x3F000000) >> 16; // polygon ID
        }
    }
    else
    {
        rp->RenderKey |= 0x30000;
    }
}

void BuildPolygons(RendererPolygon* polygons, int npolys)
{
    u32* vptr = &VertexBuffer[0];
    u32 vidx = 0;

    u16* iptr = &IndexBuffer[0];
    u32 numtriangles = 0;

    for (int i = 0; i < npolys; i++)
    {
        RendererPolygon* rp = &polygons[i];
        Polygon* poly = rp->PolyData;

        rp->Indices = iptr;

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

            u32 x, y;
            if (ScaleFactor > 0)
            {
                x = vtx->HiresPosition[0] >> (4-ScaleFactor);
                y = vtx->HiresPosition[1] >> (4-ScaleFactor);
            }
            else
            {
                x = vtx->FinalPosition[0];
                y = vtx->FinalPosition[1];
            }

            *vptr++ = x | (y << 16);
            *vptr++ = z | (w << 16);

            *vptr++ =  (vtx->FinalColor[0] >> 1) |
                      ((vtx->FinalColor[1] >> 1) << 8) |
                      ((vtx->FinalColor[2] >> 1) << 16) |
                      (alpha << 24);

            *vptr++ = (u16)vtx->TexCoords[0] | ((u16)vtx->TexCoords[1] << 16);

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

void RenderSceneChunk(int y, int h)
{
    u32 flags = 0;
    if (RenderPolygonRAM[0]->WBuffer) flags |= RenderFlag_WBuffer;

    u16* iptr;
    u32 curkey;

    if (h != 192) glScissor(0, y<<ScaleFactor, 256<<ScaleFactor, h<<ScaleFactor);

    // pass 1: opaque pixels

    UseRenderShader(flags);

    glColorMaski(1, GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glBindVertexArray(VertexArrayID);

    iptr = PolygonList[0].Indices;
    curkey = 0xFFFFFFFF;

    for (int i = 0; i < NumFinalPolys; i++)
    {
        RendererPolygon* rp = &PolygonList[i];
        if (rp->RenderKey != curkey)
        {
            u16* endptr = rp->Indices;
            u32 num = (u32)(endptr - iptr);
            if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);

            iptr = rp->Indices;
            curkey = rp->RenderKey;

            // zorp
            glDepthFunc(GL_LESS);

            u32 polyattr = rp->PolyData->Attr;
            u32 polyid = (polyattr >> 24) & 0x3F;

            glStencilFunc(GL_ALWAYS, polyid, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);
        }
    }

    {
        u16* endptr = &IndexBuffer[NumTriangles*3];
        u32 num = (u32)(endptr - iptr);
        if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);
    }

    glEnable(GL_BLEND);
    UseRenderShader(flags | RenderFlag_Trans);

    if (NumOpaqueFinalPolys > -1)
    {
        glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        if (PolygonList[NumOpaqueFinalPolys].PolyData->IsShadow) printf("!! GLORG!!! %08X\n", PolygonList[NumOpaqueFinalPolys].PolyData->Attr);

         // pass 2: if needed, render translucent pixels that are against background pixels
        // when background alpha is zero, those need to be rendered with blending disabled

        if ((RenderClearAttr1 & 0x001F0000) == 0)
        {
            iptr = PolygonList[NumOpaqueFinalPolys].Indices;
            curkey = 0xFFFFFFFF;

            glDisable(GL_BLEND);

            for (int i = NumOpaqueFinalPolys; i < NumFinalPolys; i++)
            {
                RendererPolygon* rp = &PolygonList[i];
                if (rp->RenderKey != curkey)
                {
                    u16* endptr = rp->Indices;
                    u32 num = (u32)(endptr - iptr);
                    if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);

                    iptr = rp->Indices;
                    curkey = rp->RenderKey;

                    // configure new one

                    // shadows aren't likely to pass against the clear-plane, so
                    if (rp->PolyData->IsShadow) continue;

                    // zorp
                    glDepthFunc(GL_LESS);

                    u32 polyattr = rp->PolyData->Attr;
                    u32 polyid = (polyattr >> 24) & 0x3F;

                    glStencilFunc(GL_EQUAL, 0xFF, 0xFF);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
                    glStencilMask(~(0x40|polyid)); // heheh

                    if (polyattr & (1<<11)) glDepthMask(GL_TRUE);
                    else                    glDepthMask(GL_FALSE);
                }
            }

            {
                u16* endptr = &IndexBuffer[NumTriangles*3];
                u32 num = (u32)(endptr - iptr);
                if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);
            }

            glEnable(GL_BLEND);
            glStencilMask(0xFF);
        }

        // pass 3: translucent pixels

        iptr = PolygonList[NumOpaqueFinalPolys].Indices;
        curkey = 0xFFFFFFFF;

        for (int i = NumOpaqueFinalPolys; i < NumFinalPolys; i++)
        {
            RendererPolygon* rp = &PolygonList[i];
            if (rp->RenderKey != curkey)
            {
                u16* endptr = rp->Indices;
                u32 num = (u32)(endptr - iptr);
                if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);

                iptr = rp->Indices;
                curkey = rp->RenderKey;

                // configure new one

                if (rp->PolyData->IsShadowMask)
                {
                    // clear shadow bits in stencil buffer

                    glStencilMask(0x80);
                    glClear(GL_STENCIL_BUFFER_BIT);

                    // draw actual shadow mask

                    UseRenderShader(flags | RenderFlag_ShadowMask);

                    glDisable(GL_BLEND);
                    glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glDepthMask(GL_FALSE);

                    glDepthFunc(GL_LESS);
                    glStencilFunc(GL_ALWAYS, 0x80, 0x80);
                    glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);

                    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
                    glBindVertexArray(VertexArrayID);
                }
                else
                {
                    UseRenderShader(flags | RenderFlag_Trans);

                    u32 polyattr = rp->PolyData->Attr;
                    u32 polyid = (polyattr >> 24) & 0x3F;

                    // zorp
                    glDepthFunc(GL_LESS);

                    if (rp->PolyData->IsShadow)
                    {
                        glDisable(GL_BLEND);
                        glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                        glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                        glDepthMask(GL_FALSE);
                        glStencilFunc(GL_EQUAL, polyid, 0x3F);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
                        glStencilMask(0x80);

                        {
                            u16* _endptr = &IndexBuffer[NumTriangles*3];
                            for (int j = i; j < NumFinalPolys; j++)
                            {
                                RendererPolygon* rp = &PolygonList[j];
                                if (!rp->PolyData->IsShadow)
                                {
                                    _endptr = rp->Indices;
                                    break;
                                }

                                // berg.
                            }
                            u32 num = (u32)(_endptr - iptr);
                            if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);
                        }

                        glEnable(GL_BLEND);
                        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        glColorMaski(1, GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);

                        glStencilFunc(GL_EQUAL, 0xC0|polyid, 0x80);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        glStencilMask(0x7F);
                    }
                    else
                    {
                        glEnable(GL_BLEND);
                        glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        glColorMaski(1, GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);

                        glStencilFunc(GL_NOTEQUAL, 0x40|polyid, 0x7F);
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        glStencilMask(0x7F);
                    }

                    if (polyattr & (1<<11)) glDepthMask(GL_TRUE);
                    else                    glDepthMask(GL_FALSE);
                }
            }
        }

        {
            u16* endptr = &IndexBuffer[NumTriangles*3];
            u32 num = (u32)(endptr - iptr);
            if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);
        }
    }

    glFlush();
}


void RenderFrame()
{
    ShaderConfig.uScreenSize[0] = ScreenW;
    ShaderConfig.uScreenSize[1] = ScreenH;
    ShaderConfig.uDispCnt = RenderDispCnt;

    for (int i = 0; i < 32; i++)
    {
        u16 c = RenderToonTable[i];
        u32 r = c & 0x1F;
        u32 g = (c >> 5) & 0x1F;
        u32 b = (c >> 10) & 0x1F;

        ShaderConfig.uToonColors[i][0] = (float)r / 31.0;
        ShaderConfig.uToonColors[i][1] = (float)g / 31.0;
        ShaderConfig.uToonColors[i][2] = (float)b / 31.0;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, ShaderConfigUBO);
    void* unibuf = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
    if (unibuf) memcpy(unibuf, &ShaderConfig, sizeof(ShaderConfig));
    glUnmapBuffer(GL_UNIFORM_BUFFER);

    // SUCKY!!!!!!!!!!!!!!!!!!
    // TODO: detect when VRAM blocks are modified!
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TexMemID);
    for (int i = 0; i < 4; i++)
    {
        u32 mask = GPU::VRAMMap_Texture[i];
        u8* vram;
        if (!mask) continue;
        else if (mask & (1<<0)) vram = GPU::VRAM_A;
        else if (mask & (1<<1)) vram = GPU::VRAM_B;
        else if (mask & (1<<2)) vram = GPU::VRAM_C;
        else if (mask & (1<<3)) vram = GPU::VRAM_D;

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i*128, 1024, 128, GL_RED_INTEGER, GL_UNSIGNED_BYTE, vram);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, TexPalMemID);
    for (int i = 0; i < 6; i++)
    {
        // 6 x 16K chunks
        u32 mask = GPU::VRAMMap_TexPal[i];
        u8* vram;
        if (!mask) continue;
        else if (mask & (1<<4)) vram = &GPU::VRAM_E[(i&3)*0x4000];
        else if (mask & (1<<5)) vram = GPU::VRAM_F;
        else if (mask & (1<<6)) vram = GPU::VRAM_G;

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i*8, 1024, 8, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, vram);
    }

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);

    glViewport(0, 0, ScreenW, ScreenH);

    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[FrontBuffer]);
    FrontBuffer = FrontBuffer ? 0 : 1;

    glDisable(GL_BLEND);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glStencilMask(0xFF);

    // clear buffers
    // TODO: clear bitmap
    // TODO: check whether 'clear polygon ID' affects translucent polyID
    // (for example when alpha is 1..30)
    {
        glUseProgram(ClearShaderPlain[2]);
        glDepthFunc(GL_ALWAYS);

        u32 r = RenderClearAttr1 & 0x1F;
        u32 g = (RenderClearAttr1 >> 5) & 0x1F;
        u32 b = (RenderClearAttr1 >> 10) & 0x1F;
        u32 fog = (RenderClearAttr1 >> 15) & 0x1;
        u32 a = (RenderClearAttr1 >> 16) & 0x1F;
        u32 polyid = (RenderClearAttr1 >> 24) & 0x3F;
        u32 z = ((RenderClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;

        glStencilFunc(GL_ALWAYS, 0xFF, 0xFF);
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);

        /*if (r) r = r*2 + 1;
        if (g) g = g*2 + 1;
        if (b) b = b*2 + 1;*/

        glUniform4ui(ClearUniformLoc[0], r, g, b, a);
        glUniform1ui(ClearUniformLoc[1], z);
        glUniform1ui(ClearUniformLoc[2], polyid);
        glUniform1ui(ClearUniformLoc[3], fog);

        glBindBuffer(GL_ARRAY_BUFFER, ClearVertexBufferID);
        glBindVertexArray(ClearVertexArrayID);
        glDrawArrays(GL_TRIANGLES, 0, 2*3);
    }

    if (RenderNumPolygons)
    {
        // render shit here
        u32 flags = 0;
        if (RenderPolygonRAM[0]->WBuffer) flags |= RenderFlag_WBuffer;

        int npolys = 0;
        int firsttrans = -1;
        for (int i = 0; i < RenderNumPolygons; i++)
        {
            if (RenderPolygonRAM[i]->Degenerate) continue;

            // zog.
            //if (RenderPolygonRAM[i]->YBottom <= 96 || RenderPolygonRAM[i]->YTop >= 144) continue;

            SetupPolygon(&PolygonList[npolys], RenderPolygonRAM[i]);
            if (firsttrans < 0 && RenderPolygonRAM[i]->Translucent)
                firsttrans = npolys;

            npolys++;
        }
        NumFinalPolys = npolys;
        NumOpaqueFinalPolys = firsttrans;

        BuildPolygons(&PolygonList[0], npolys);
        glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, NumVertices*7*4, VertexBuffer);

        RenderSceneChunk(0, 192);
    }

    if (false)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferID[0]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferID[1]);
        glBlitFramebuffer(0, 0, ScreenW, ScreenH, 0, 0, ScreenW/2, ScreenH/2, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[1]);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[0]);
    }

    /*if (!Accelerated)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);

        glReadPixels(0, 0, 256<<ScaleFactor, 192<<ScaleFactor, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }*/
}

void PrepareCaptureFrame()
{
    // TODO: make sure this picks the right buffer when doing antialiasing
    int original_fb = FrontBuffer^1;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferID[original_fb]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferID[3]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, ScreenW, ScreenH, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferID[3]);
    glReadPixels(0, 0, 256, 192, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    //glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID[original_fb]);
    //glFlush();
}

u32* GetLine(int line)
{
    int stride = 256;// << (ScaleFactor*2);

    if (line == 0)
    {
        u8* data = (u8*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (data) memcpy(&Framebuffer[stride*0], data, 4*stride*192);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    u64* ptr = (u64*)&Framebuffer[stride * line];
    for (int i = 0; i < stride; i+=2)
    {
        u64 rgb = *ptr & 0x00FCFCFC00FCFCFC;
        u64 a = *ptr & 0xF8000000F8000000;

        *ptr++ = (rgb >> 2) | (a >> 3);
    }

    return &Framebuffer[stride * line];
}

void SetupAccelFrame()
{
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[FrontBuffer]);
}

}
}
