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

#ifndef GPU_VULKAN_H
#define GPU_VULKAN_H

#include "GPU_Soft.h"
#include "VulkanSupport.h"

namespace melonDS
{

class VulkanRenderer : public SoftRenderer
{
public:
    explicit VulkanRenderer(melonDS::NDS& nds);
    ~VulkanRenderer() override;

    bool Init() override;
    void PreSavestate() override;
    void PostSavestate() override;
    void SetRenderSettings(RendererSettings& settings) override;

private:
    Vulkan::Context Context;
};

}

#endif // GPU_VULKAN_H
