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

#ifndef OPENGLSUPPORT_H
#define OPENGLSUPPORT_H

#include <stdio.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include "Platform.h"


// here, have some macro magic
// we at the melonDS company really love macro magic
// also, suggestion to the fine folks who write the OpenGL headers:
// pls make the type names follow the same capitalization as their
// matching function names, so this is more convenient to deal with

#define DECLPROC(type, name)  \
    PFN##type##PROC name ;

#define DECLPROC_EXT(type, name)  \
    extern PFN##type##PROC name ;

#define LOADPROC(type, name)  \
    name = (PFN##type##PROC)Platform::GL_GetProcAddress(#name); \
    if (!name) { printf("OpenGL: " #name " not found\n"); return false; }


// if you need more OpenGL functions, add them to the macronator here
// TODO: handle conditionally loading certain functions for different GL versions

#ifndef __WIN32__

#define DO_PROCLIST_1_3(func)

#else

#define DO_PROCLIST_1_3(func) \
    func(GLACTIVETEXTURE, glActiveTexture); \
    func(GLBLENDCOLOR, glBlendColor); \

#endif


#define DO_PROCLIST(func) \
    DO_PROCLIST_1_3(func) \
    \
    func(GLGENFRAMEBUFFERS, glGenFramebuffers); \
    func(GLDELETEFRAMEBUFFERS, glDeleteFramebuffers); \
    func(GLBINDFRAMEBUFFER, glBindFramebuffer); \
    func(GLFRAMEBUFFERTEXTURE, glFramebufferTexture); \
    func(GLBLITFRAMEBUFFER, glBlitFramebuffer); \
    func(GLCHECKFRAMEBUFFERSTATUS, glCheckFramebufferStatus); \
     \
    func(GLGENBUFFERS, glGenBuffers); \
    func(GLDELETEBUFFERS, glDeleteBuffers); \
    func(GLBINDBUFFER, glBindBuffer); \
    func(GLMAPBUFFER, glMapBuffer); \
    func(GLMAPBUFFERRANGE, glMapBufferRange); \
    func(GLUNMAPBUFFER, glUnmapBuffer); \
    func(GLBUFFERDATA, glBufferData); \
    func(GLBUFFERSUBDATA, glBufferSubData); \
    func(GLBINDBUFFERBASE, glBindBufferBase); \
     \
    func(GLGENVERTEXARRAYS, glGenVertexArrays); \
    func(GLDELETEVERTEXARRAYS, glDeleteVertexArrays); \
    func(GLBINDVERTEXARRAY, glBindVertexArray); \
    func(GLENABLEVERTEXATTRIBARRAY, glEnableVertexAttribArray); \
    func(GLDISABLEVERTEXATTRIBARRAY, glDisableVertexAttribArray); \
    func(GLVERTEXATTRIBPOINTER, glVertexAttribPointer); \
    func(GLVERTEXATTRIBIPOINTER, glVertexAttribIPointer); \
    func(GLBINDATTRIBLOCATION, glBindAttribLocation); \
    func(GLBINDFRAGDATALOCATION, glBindFragDataLocation); \
     \
    func(GLCREATESHADER, glCreateShader); \
    func(GLSHADERSOURCE, glShaderSource); \
    func(GLCOMPILESHADER, glCompileShader); \
    func(GLCREATEPROGRAM, glCreateProgram); \
    func(GLATTACHSHADER, glAttachShader); \
    func(GLLINKPROGRAM, glLinkProgram); \
    func(GLUSEPROGRAM, glUseProgram); \
    func(GLGETSHADERIV, glGetShaderiv); \
    func(GLGETSHADERINFOLOG, glGetShaderInfoLog); \
    func(GLGETPROGRAMIV, glGetProgramiv); \
    func(GLGETPROGRAMINFOLOG, glGetProgramInfoLog); \
    func(GLDELETESHADER, glDeleteShader); \
    func(GLDELETEPROGRAM, glDeleteProgram); \
     \
    func(GLUNIFORM1I, glUniform1i); \
    func(GLUNIFORM1UI, glUniform1ui); \
    func(GLUNIFORM2I, glUniform2i); \
    func(GLUNIFORM4UI, glUniform4ui); \
    func(GLUNIFORMBLOCKBINDING, glUniformBlockBinding); \
    func(GLGETUNIFORMLOCATION, glGetUniformLocation); \
    func(GLGETUNIFORMBLOCKINDEX, glGetUniformBlockIndex); \
     \
    func(GLBINDIMAGETEXTURE, glBindImageTexture); \
     \
    func(GLDRAWBUFFERS, glDrawBuffers); \
     \
    func(GLBLENDFUNCSEPARATE, glBlendFuncSeparate); \
    func(GLBLENDEQUATIONSEPARATE, glBlendEquationSeparate); \
     \
    func(GLCOLORMASKI, glColorMaski); \
     \
    func(GLGETSTRINGI, glGetStringi); \


DO_PROCLIST(DECLPROC_EXT);


bool OpenGL_Init();

bool OpenGL_BuildShaderProgram(const char* vs, const char* fs, GLuint* ids, const char* name);
bool OpenGL_LinkShaderProgram(GLuint* ids);
void OpenGL_DeleteShaderProgram(GLuint* ids);
void OpenGL_UseShaderProgram(GLuint* ids);

#endif // OPENGLSUPPORT_H
