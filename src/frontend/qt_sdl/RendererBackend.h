/*
    Copyright 2016-2026 melonDS team

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

#ifndef RENDERERBACKEND_H
#define RENDERERBACKEND_H

#include <memory>

namespace melonDS
{
class NDS;
class Renderer;
}

enum
{
    renderer3D_Software = 0,
#ifdef OGLRENDERER_ENABLED
    renderer3D_OpenGL,
    renderer3D_OpenGLCompute,
#endif
    renderer3D_Max,
};

namespace RendererBackend
{

std::unique_ptr<melonDS::Renderer> Create(melonDS::NDS& nds, int renderer);
bool RequiresOpenGL(int renderer);

}

#endif // RENDERERBACKEND_H
