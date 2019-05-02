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
PFNGLDISABLEVERTEXATTRIBARRAYPROC   glDisableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC        glVertexAttribPointer;
PFNGLVERTEXATTRIBIPOINTERPROC       glVertexAttribIPointer;

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
PFNGLUNIFORM1UIPROC             glUniform1ui;
PFNGLUNIFORM4UIPROC             glUniform4ui;

PFNGLACTIVETEXTUREPROC          glActiveTexture;
PFNGLBINDIMAGETEXTUREPROC       glBindImageTexture;

PFNGLGETSTRINGIPROC             glGetStringi;


#define kShaderHeader "#version 430"


const char* kClearVS = kShaderHeader R"(

layout(location=0) in vec2 vPosition;

layout(location=1) uniform uint uDepth;

void main()
{
    float fdepth = (float(uDepth) / 8388608.0) - 1.0;
    gl_Position = vec4(vPosition, fdepth, 1.0);
}
)";

const char* kClearFS = kShaderHeader R"(

layout(location=0) uniform uvec4 uColor;

out vec4 oColor;

void main()
{
    oColor = vec4(uColor).bgra / 31.0;
}
)";


const char* kRenderVSCommon = R"(

layout(location=0) in uvec4 vPosition;
layout(location=1) in uvec4 vColor;
layout(location=2) in ivec2 vTexcoord;
layout(location=3) in uvec3 vPolygonAttr;

smooth out vec4 fColor;
smooth out vec2 fTexcoord;
flat out uvec3 fPolygonAttr;
)";

const char* kRenderFSCommon = R"(

layout(binding=0) uniform usampler2D TexMem;
layout(binding=1) uniform sampler2D TexPalMem;

smooth in vec4 fColor;
smooth in vec2 fTexcoord;
flat in uvec3 fPolygonAttr;

out vec4 oColor;

vec4 TextureLookup()
{
    uint attr = fPolygonAttr.y;
    uint paladdr = fPolygonAttr.z;

    float alpha0;
    if ((attr & (1<<29)) != 0) alpha0 = 0.0;
    else                       alpha0 = 1.0;

    int tw = 8 << int((attr >> 20) & 0x7);
    int th = 8 << int((attr >> 23) & 0x7);

    int vramaddr = int(attr & 0xFFFF) << 3;
    ivec2 st = ivec2(fTexcoord);

    if ((attr & (1<<16)) != 0)
    {
        if ((attr & (1<<18)) != 0)
        {
            if ((st.x & tw) != 0)
                st.x = (tw-1) - (st.x & (tw-1));
            else
                st.x = (st.x & (tw-1));
        }
        else
            st.x &= (tw-1);
    }
    else
        st.x = clamp(st.x, 0, tw-1);

    if ((attr & (1<<17)) != 0)
    {
        if ((attr & (1<<19)) != 0)
        {
            if ((st.y & th) != 0)
                st.y = (th-1) - (st.y & (th-1));
            else
                st.y = (st.y & (th-1));
        }
        else
            st.y &= (th-1);
    }
    else
        st.y = clamp(st.y, 0, th-1);

    uint type = (attr >> 26) & 0x7;
    if (type == 1)
    {
        vramaddr += ((st.y * tw) + st.x);
        uvec4 pixel = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);

        pixel.a = (pixel.r & 0xE0);
        pixel.a = (pixel.a >> 3) + (pixel.a >> 6);
        pixel.r &= 0x1F;

        paladdr = (paladdr << 3) + pixel.r;
        vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);

        return vec4(color.rgb, float(pixel.a)/31.0);
    }
    else if (type == 2)
    {
        vramaddr += ((st.y * tw) + st.x) >> 2;
        uvec4 pixel = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);
        pixel.r >>= (2 * (st.x & 3));
        pixel.r &= 0x03;

        paladdr = (paladdr << 2) + pixel.r;
        vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);

        return vec4(color.rgb, max(step(1,pixel.r),alpha0));
    }
    else if (type == 3)
    {
        vramaddr += ((st.y * tw) + st.x) >> 1;
        uvec4 pixel = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);
        if ((st.x & 1) != 0) pixel.r >>= 4;
        else                 pixel.r &= 0x0F;

        paladdr = (paladdr << 3) + pixel.r;
        vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);

        return vec4(color.rgb, max(step(1,pixel.r),alpha0));
    }
    else if (type == 4)
    {
        vramaddr += ((st.y * tw) + st.x);
        uvec4 pixel = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);

        paladdr = (paladdr << 3) + pixel.r;
        vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);

        return vec4(color.rgb, max(step(1,pixel.r),alpha0));
    }
    else if (type == 5)
    {
        vramaddr += ((st.y & 0x3FC) * (tw>>2)) + (st.x & 0x3FC) + (st.y & 0x3);
        uvec4 p = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);
        uint val = (p.r >> (2 * (st.x & 0x3))) & 0x3;

        int slot1addr = 0x20000 + ((vramaddr & 0x1FFFC) >> 1);
        if (vramaddr >= 0x40000) slot1addr += 0x10000;

        uint palinfo;
        p = texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0);
        palinfo = p.r;
        slot1addr++;
        p = texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0);
        palinfo |= (p.r << 8);

        paladdr = (paladdr << 3) + ((palinfo & 0x3FFF) << 1);
        palinfo >>= 14;

        if (val == 0)
        {
            vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
            return vec4(color.rgb, 1.0);
        }
        else if (val == 1)
        {
            paladdr++;
            vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
            return vec4(color.rgb, 1.0);
        }
        else if (val == 2)
        {
            if (palinfo == 1)
            {
                vec4 color0 = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                paladdr++;
                vec4 color1 = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                return vec4((color0.rgb + color1.rgb) / 2.0, 1.0);
            }
            else if (palinfo == 3)
            {
                vec4 color0 = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                paladdr++;
                vec4 color1 = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                return vec4((color0.rgb*5.0 + color1.rgb*3.0) / 8.0, 1.0);
            }
            else
            {
                paladdr += 2;
                vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                return vec4(color.rgb, 1.0);
            }
        }
        else
        {
            if (palinfo == 2)
            {
                paladdr += 3;
                vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                return vec4(color.rgb, 1.0);
            }
            else if (palinfo == 3)
            {
                vec4 color0 = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                paladdr++;
                vec4 color1 = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);
                return vec4((color0.rgb*3.0 + color1.rgb*5.0) / 8.0, 1.0);
            }
            else
            {
                return vec4(0.0);
            }
        }
    }
    else if (type == 6)
    {
        vramaddr += ((st.y * tw) + st.x);
        uvec4 pixel = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);

        pixel.a = (pixel.r & 0xF8) >> 3;
        pixel.r &= 0x07;

        paladdr = (paladdr << 3) + pixel.r;
        vec4 color = texelFetch(TexPalMem, ivec2(paladdr&0x3FF, paladdr>>10), 0);

        return vec4(color.rgb, float(pixel.a)/31.0);
    }
    else
    {
        vramaddr += ((st.y * tw) + st.x) << 1;
        uvec4 pixelL = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);
        vramaddr++;
        uvec4 pixelH = texelFetch(TexMem, ivec2(vramaddr&0x3FF, vramaddr>>10), 0);

        vec4 color;
        color.r = float(pixelL.r & 0x1F) / 31.0;
        color.g = float((pixelL.r >> 5) | ((pixelH.r & 0x03) << 3)) / 31.0;
        color.b = float((pixelH.r & 0x7C) >> 2) / 31.0;
        color.a = float(pixelH.r >> 7);

        return color;
    }
}

vec4 FinalColor()
{
    vec4 col;
    vec4 vcol = fColor;

    // TODO: also check DISPCNT
    if (((fPolygonAttr.y >> 26) & 0x7) == 0)
    {
        // no texture
        col = vcol;
    }
    else
    {
        vec4 tcol = TextureLookup();

        col = vcol * tcol;
    }

    return col.bgra;
}
)";


const char* kRenderVS_Z = R"(

void main()
{
    uint attr = vPolygonAttr.x;
    uint zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.x = ((float(vPosition.x) * 2.0) / 256.0) - 1.0;
    fpos.y = ((float(vPosition.y) * 2.0) / 192.0) - 1.0;
    fpos.z = (float(vPosition.z << zshift) / 8388608.0) - 1.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xyz *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
    fPolygonAttr = vPolygonAttr;

    gl_Position = fpos;
}
)";

const char* kRenderVS_W = R"(

smooth out float fZ;

void main()
{
    uint attr = vPolygonAttr.x;
    uint zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.x = ((float(vPosition.x) * 2.0) / 256.0) - 1.0;
    fpos.y = ((float(vPosition.y) * 2.0) / 192.0) - 1.0;
    fZ = float(vPosition.z << zshift) / 16777216.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xy *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
    fPolygonAttr = vPolygonAttr;

    gl_Position = fpos;
}
)";


const char* kRenderFS_ZO = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oColor = col;
}
)";

const char* kRenderFS_WO = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oColor = col;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZT = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    oColor = col;
}
)";

const char* kRenderFS_WT = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    oColor = col;
    gl_FragDepth = fZ;
}
)";


enum
{
    RenderFlag_WBuffer = 0x01,
    RenderFlag_Trans   = 0x02,
};


GLuint ClearShaderPlain[3];

GLuint RenderShader[16][3];

typedef struct
{
    Polygon* PolyData;

    u16* Indices;
    u32 RenderKey;

} RendererPolygon;

RendererPolygon PolygonList[2048];

GLuint ClearVertexBufferID, ClearVertexArrayID;

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

GLuint FramebufferTex[3];
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
    LOADPROC(GLDISABLEVERTEXATTRIBARRAY, glDisableVertexAttribArray);
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
    LOADPROC(GLUNIFORM1UI, glUniform1ui);
    LOADPROC(GLUNIFORM4UI, glUniform4ui);

    LOADPROC(GLACTIVETEXTURE, glActiveTexture);
    LOADPROC(GLBINDIMAGETEXTURE, glBindImageTexture);

    LOADPROC(GLGETSTRINGI, glGetStringi);

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
        printf("shader source:\n--\n%s\n--\n", vs);
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
        printf("shader source:\n--\n%s\n--\n", fs);
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

    bool ret = BuildShaderProgram(vsbuf, fsbuf, RenderShader[flags], shadername);

    delete[] vsbuf;
    delete[] fsbuf;

    return ret;
}

void UseRenderShader(u32 flags)
{
    glUseProgram(RenderShader[flags][2]);
}

bool Init()
{
    if (!InitGLExtensions()) return false;

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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    // TODO: make configurable (hires, etc)
    glViewport(0, 0, 256, 192);
    glDepthRange(0, 1);
    glClearDepth(1.0);


    if (!BuildShaderProgram(kClearVS, kClearFS, ClearShaderPlain, "ClearShader"))
        return false;

    if (!BuildRenderShader(0,
                           kRenderVS_Z, kRenderFS_ZO)) return false;
    if (!BuildRenderShader(RenderFlag_WBuffer,
                           kRenderVS_W, kRenderFS_WO)) return false;
    if (!BuildRenderShader(RenderFlag_Trans,
                           kRenderVS_Z, kRenderFS_ZT)) return false;
    if (!BuildRenderShader(RenderFlag_Trans | RenderFlag_WBuffer,
                           kRenderVS_W, kRenderFS_WT)) return false;


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


    glGenFramebuffers(1, &FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);

    glGenTextures(1, &FramebufferTex[0]);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FramebufferTex[0], 0);

    /*glGenTextures(1, &FramebufferTex[1]);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 256, 192, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL); // welp
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, FramebufferTex[1], 0);

    glGenTextures(1, &FramebufferTex[2]);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX, 256, 192, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, FramebufferTex[2], 0);*/

    glGenTextures(1, &FramebufferTex[1]);
    glBindTexture(GL_TEXTURE_2D, FramebufferTex[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, 256, 192, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL); printf("%04X\n", glGetError());
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, FramebufferTex[1], 0);printf("%04X\n", glGetError());

    glGenBuffers(1, &PixelbufferID);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, PixelbufferID);
    //glBufferData(GL_PIXEL_PACK_BUFFER, 256*48*4, NULL, GL_DYNAMIC_READ);
    glBufferData(GL_PIXEL_PACK_BUFFER, 256*192*4, NULL, GL_DYNAMIC_READ);

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
    // TODO CLEAN UP SHIT!!!!
}

void Reset()
{
    //
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
            rp->RenderKey |= (polygon->Attr >> 10) & 0x2; // bit11 - depth write
            rp->RenderKey |= (polygon->Attr & 0x3F000000) >> 16; // polygon ID
        }
        else
        {
            if ((polygon->Attr & 0x001F0000) == 0)
                rp->RenderKey |= 0x2;
        }
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

            // TODO hires-upgraded positions?
            *vptr++ = vtx->FinalPosition[0] | (vtx->FinalPosition[1] << 16);
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


void VCount144()
{
}

void RenderFrame()
{
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

    glDisable(GL_BLEND);

    // clear buffers
    // TODO: clear bitmap
    // TODO: check whether 'clear polygon ID' affects translucent polyID
    // (for example when alpha is 1..30)
    {
        glUseProgram(ClearShaderPlain[2]);
        glDepthFunc(GL_ALWAYS);
        glDepthMask(GL_TRUE);

        u32 r = RenderClearAttr1 & 0x1F;
        u32 g = (RenderClearAttr1 >> 5) & 0x1F;
        u32 b = (RenderClearAttr1 >> 10) & 0x1F;
        u32 a = (RenderClearAttr1 >> 16) & 0x1F;
        u32 z = ((RenderClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;

        if (a == 0)
        {
            glStencilFunc(GL_ALWAYS, 0x80, 0xFF);
            glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        }
        else
        {
            glStencilFunc(GL_ALWAYS, 0, 0xFF);
            glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        }

        /*if (r) r = r*2 + 1;
        if (g) g = g*2 + 1;
        if (b) b = b*2 + 1;*/

        glUniform4ui(0, r, g, b, a);
        glUniform1ui(1, z);

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

            SetupPolygon(&PolygonList[npolys], RenderPolygonRAM[i]);
            if (firsttrans < 0 && RenderPolygonRAM[i]->Translucent)
                firsttrans = npolys;

            npolys++;
        }

        BuildPolygons(&PolygonList[0], npolys);
        glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, NumVertices*7*4, VertexBuffer);

        // pass 1: opaque pixels

        UseRenderShader(flags);

        // zorp
        glDepthFunc(GL_LESS);

        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);

        glBindVertexArray(VertexArrayID);
        glDrawElements(GL_TRIANGLES, NumTriangles*3, GL_UNSIGNED_SHORT, IndexBuffer);

        // pass 2: if needed, render translucent pixels that are against background pixels

        glEnable(GL_BLEND);
        UseRenderShader(flags | RenderFlag_Trans);

        u16* iptr;
        u32 curkey;

        /*if ((RenderClearAttr1 & 0x001F0000) == 0)
        {
            glStencilFunc(GL_EQUAL, 0x80, 0x80);
            glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);

            //
        }*/

        // pass 3: translucent pixels

        if (firsttrans > -1)
        {
            iptr = PolygonList[firsttrans].Indices;
            curkey = 0xFFFFFFFF;

            for (int i = firsttrans; i < npolys; i++)
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

                    // zorp
                    glDepthFunc(GL_LESS);

                    u32 polyattr = rp->PolyData->Attr;
                    u32 polyid = (polyattr >> 24) & 0x3F;

                    glStencilFunc(GL_NOTEQUAL, 0x40|polyid, 0x7F);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

                    if (polyattr & (1<<11)) glDepthMask(GL_TRUE);
                    else                    glDepthMask(GL_FALSE);
                }
            }

            {
                u16* endptr = &IndexBuffer[NumTriangles*3];
                u32 num = (u32)(endptr - iptr);
                if (num) glDrawElements(GL_TRIANGLES, num, GL_UNSIGNED_SHORT, iptr);
            }
        }
    }


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

    u32* ptr = (u32*)&Framebuffer[256*4 * line];
    for (int i = 0; i < 256; i++)
    {
        u32 rgb = *ptr & 0x00FCFCFC;
        u32 a = *ptr & 0xF8000000;

        *ptr++ = (rgb >> 2) | (a >> 3);
    }
}

u32* GetLine(int line)
{
    return (u32*)&Framebuffer[256*4 * line];
}

}
}
