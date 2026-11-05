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

#include "GPU_Soft.h"
#ifdef OGLRENDERER_ENABLED
#include "GPU_OpenGL.h"
#endif
#ifdef VULKANRENDERER_ENABLED
#include "GPU_Vulkan.h"
#endif

#include "RendererBackend.h"

namespace RendererBackend
{

std::unique_ptr<melonDS::Renderer> Create(melonDS::NDS& nds, int renderer)
{
    switch (renderer)
    {
    case renderer3D_Software:
        return std::make_unique<melonDS::SoftRenderer>(nds);

#ifdef OGLRENDERER_ENABLED
    case renderer3D_OpenGL:
        return std::make_unique<melonDS::GLRenderer>(nds, false);

    case renderer3D_OpenGLCompute:
        return std::make_unique<melonDS::GLRenderer>(nds, true);
#endif

#ifdef VULKANRENDERER_ENABLED
    case renderer3D_Vulkan:
        return std::make_unique<melonDS::VulkanRenderer>(nds);
#endif

    default:
        return nullptr;
    }
}

bool RequiresOpenGL(int renderer)
{
#ifdef OGLRENDERER_ENABLED
    return renderer == renderer3D_OpenGL || renderer == renderer3D_OpenGLCompute;
#else
    return false;
#endif
}

}
