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

#include "GPU3D_TexcacheVulkan.h"

namespace melonDS
{

VulkanTexArray* TexcacheVulkanLoader::GenerateTexture(u32 width, u32 height, u32 layers) const
{
    VulkanTexArray* tex = new VulkanTexArray();
    if (!Ctx->CreateImage(tex->Image, VK_FORMAT_R8G8B8A8_UINT, width, height, layers,
                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true))
    {
        delete tex;
        return nullptr;
    }
    return tex;
}

void TexcacheVulkanLoader::UploadTexture(VulkanTexArray* handle, u32 width, u32 height, u32 layer, void* data) const
{
    if (!handle)
        return;
    Ctx->UploadImageLayer(handle->Image, data, width, height, layer, 4);
}

void TexcacheVulkanLoader::DeleteTexture(VulkanTexArray* handle) const
{
    if (!handle)
        return;
    // the renderer waits for the frame fence before the texture cache
    // runs invalidation, so the image is no longer in use here
    Ctx->DestroyImage(handle->Image);
    delete handle;
}

}
