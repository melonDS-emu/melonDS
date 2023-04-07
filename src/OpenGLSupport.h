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

#ifndef OPENGLSUPPORT_H
#define OPENGLSUPPORT_H

#include <stdio.h>
#include <string.h>

#include "Platform.h"
#include "PlatformOGL.h"


namespace OpenGL
{

struct AttributeTarget
{
    const char* Name;
    u32 Location;
};

bool CompileVertexFragmentProgram(GLuint& result,
    const char* vs, const char* fs,
    const char* name,
    const std::initializer_list<AttributeTarget>& vertexInAttrs,
    const std::initializer_list<AttributeTarget>& fragmentOutAttrs);

bool CompileComputeProgram(GLuint& result, const char* source, const char* name);

}

#endif // OPENGLSUPPORT_H
