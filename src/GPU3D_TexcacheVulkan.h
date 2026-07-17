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

#ifndef GPU3D_TEXCACHEVULKAN
#define GPU3D_TEXCACHEVULKAN

#include "GPU3D_Texcache.h"
#include "VulkanSupport.h"

namespace melonDS
{

template <typename, typename>
class Texcache;

// one array texture allocated by the texture cache
struct VulkanTexArray
{
    VK::Context::Image Image;
};

class TexcacheVulkanLoader
{
public:
    explicit TexcacheVulkanLoader(VK::Context* ctx) : Ctx(ctx) {}

    VulkanTexArray* GenerateTexture(u32 width, u32 height, u32 layers) const;
    void UploadTexture(VulkanTexArray* handle, u32 width, u32 height, u32 layer, void* data) const;
    void DeleteTexture(VulkanTexArray* handle) const;

private:
    VK::Context* Ctx;
};

using TexcacheVulkan = Texcache<TexcacheVulkanLoader, VulkanTexArray*>;

}

#endif
