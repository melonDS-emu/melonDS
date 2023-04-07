/*
    Copyright 2016-2022 melonDS team

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

#include "OpenGLSupport.h"

using Platform::Log;
using Platform::LogLevel;

namespace OpenGL
{

#define checkGLError() if (glGetError() != GL_NO_ERROR) printf("error %d\n", __LINE__)

bool CompilerShader(GLuint& id, const char* source, const char* name, const char* type)
{
    int len;
    int res;

    if (!glCreateShader)
    {
        Log(LogLevel::Error, "OpenGL: Cannot build shader program, OpenGL hasn't been loaded\n");
        return false;
    }

    len = strlen(source);
    glShaderSource(id, 1, &source, &len);
    checkGLError();
    glCompileShader(id);
    checkGLError();

    glGetShaderiv(id, GL_COMPILE_STATUS, &res);
    checkGLError();
    if (res != GL_TRUE)
    {
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &res);
        if (res < 1) res = 1024;
        char* log = new char[res+1];
        glGetShaderInfoLog(id, res+1, NULL, log);
        Log(LogLevel::Error, "OpenGL: failed to compile %s shader %s: %s\n", type, name, log);
        Log(LogLevel::Debug, "shader source:\n--\n%s\n--\n", source);
        delete[] log;

        return false;
    }

    return true;
}

bool LinkProgram(GLuint& result, GLuint* ids, int numIds)
{
    int res;

    if (!glLinkProgram)
    {
        Log(LogLevel::Error, "OpenGL: Cannot link shader program, OpenGL hasn't been loaded\n");
        return false;
    }

    for (int i = 0; i < numIds; i++)
    {
        glAttachShader(result, ids[i]);
        checkGLError();
    }

    glLinkProgram(result);

    for (int i = 0; i < numIds; i++)
        glDetachShader(result, ids[i]);

    glGetProgramiv(result, GL_LINK_STATUS, &res);
    if (res != GL_TRUE)
    {
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &res);
        if (res < 1) res = 1024;
        char* log = new char[res+1];
        glGetProgramInfoLog(result, res+1, NULL, log);
        Log(LogLevel::Error, "OpenGL: failed to link shader program: %s\n", log);
        delete[] log;

        return false;
    }

    return true;
}

bool CompileComputeProgram(GLuint& result, const char* source, const char* name)
{
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    bool linkingSucess = false;
    if (glDeleteProgram)
    { // If OpenGL isn't loaded, then there's no shader program to delete
        goto error;
    }

    result = glCreateProgram();

    printf("compiling %s", name);
    if (!CompilerShader(shader, source, name, "compute"))
        goto error;

    linkingSucess = LinkProgram(result, &shader, 1);

error:
    glDeleteShader(shader);

    if (!linkingSucess)
        glDeleteProgram(result);

    return linkingSucess;
}

bool CompileVertexFragmentProgram(GLuint& result,
    const char* vs, const char* fs,
    const char* name,
    const std::initializer_list<AttributeTarget>& vertexInAttrs,
    const std::initializer_list<AttributeTarget>& fragmentOutAttrs)
{
    GLuint shaders[2] =
    {
        glCreateShader(GL_VERTEX_SHADER),
        glCreateShader(GL_FRAGMENT_SHADER)
    };
    result = glCreateProgram();

    bool linkingSucess = false;

    if (!CompilerShader(shaders[0], vs, name, "vertex"))
        goto error;

    if (!CompilerShader(shaders[1], fs, name, "fragment"))
        goto error;


    for (const AttributeTarget& target : vertexInAttrs)
    {
        glBindAttribLocation(result, target.Location, target.Name);
    }
    for (const AttributeTarget& target : fragmentOutAttrs)
    {
        glBindFragDataLocation(result, target.Location, target.Name);
    }

    linkingSucess = LinkProgram(result, shaders, 2);

error:
    glDeleteShader(shaders[1]);
    glDeleteShader(shaders[0]);

    if (!linkingSucess)
        glDeleteProgram(result);

    return linkingSucess;
}

}
