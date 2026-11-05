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

#include "GPU_Vulkan.h"

#include "GPU3D_Vulkan.h"
#include "NDS.h"

namespace melonDS
{

VulkanRenderer::VulkanRenderer(melonDS::NDS& nds)
    : SoftRenderer(nds)
{
    Rend3D = std::make_unique<VulkanRenderer3D>(GPU.GPU3D, Context);
}

VulkanRenderer::~VulkanRenderer()
{
    Rend3D.reset();
}

bool VulkanRenderer::Init()
{
    return Context.Init() && Rend3D->Init();
}

void VulkanRenderer::PreSavestate()
{
}

void VulkanRenderer::PostSavestate()
{
    Rend3D->Reset();
}

void VulkanRenderer::SetRenderSettings(RendererSettings& settings)
{
    static_cast<VulkanRenderer3D*>(Rend3D.get())->SetRenderSettings(settings.ScaleFactor);
}

}
