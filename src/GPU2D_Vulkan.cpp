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

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include "GPU_Vulkan.h"
#include "GPU2D_Vulkan.h"
#include "GPU2D_Soft.h"
#include "GPU2D_Vulkan_shaders.h"
#include "GPU.h"
#include "GPU3D.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;



VulkanRenderer2D::VulkanRenderer2D(melonDS::GPU2D& gpu2D, VulkanRenderer& parent,
                                   VK::Context& ctx)
    : Renderer2D(gpu2D), Parent(parent), Ctx(ctx)
{
    ScaleFactor = 0;
    ScreenW = 0;
    ScreenH = 0;
    SoftFallback = std::make_unique<SoftRenderer2D>(gpu2D, SoftOutput);
}

bool VulkanRenderer2D::CompilePipelineShader(VkShaderModule& out, VK::Context::ShaderStage stage,
                                             const std::string& source, const char* name)
{
    std::string full = "#version 460\n" + source;
    return Ctx.CompileShader(out, stage, full, name);
}

bool VulkanRenderer2D::InitShaders()
{
    OwnsShared = true;

    // descriptor set layout, matching the binding map documented at the
    // top of GPU2D_Vulkan_shaders.h; the configs that get re-uploaded
    // mid-frame are dynamic-offset UBOs so each recorded draw keeps the
    // version it was recorded with
    {
        VkDescriptorSetLayoutBinding bindings[16] = {};
        auto setBinding = [&](u32 idx, VkDescriptorType type)
        {
            bindings[idx].binding = idx;
            bindings[idx].descriptorType = type;
            bindings[idx].descriptorCount = 1;
            bindings[idx].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        };
        setBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);   // ubBGConfig
        setBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);           // ubScanlineConfig
        setBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);   // ubCompositorConfig
        setBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);   // ubSpriteConfig
        setBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);           // ubSpriteScanlineConfig
        for (u32 i = 5; i < 16; i++)                                // textures
            setBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 16;
        layoutInfo.pBindings = bindings;
        if (VK::vkCreateDescriptorSetLayout(Ctx.Device, &layoutInfo, nullptr, &SetLayout) != VK_SUCCESS)
            return false;
    }

    // pipeline layout: one 12-byte push constant block
    // ({uCurBG, uRenderTransparent, uScaleFactor}) shared by all pipelines
    {
        static_assert(sizeof(sPush2D) == 12);

        VkPushConstantRange pushRange = {};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(sPush2D);

        VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &SetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (VK::vkCreatePipelineLayout(Ctx.Device, &layoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS)
            return false;
    }

    // render passes

    if (!Ctx.CreateRenderPass(RPColorClear, VK_FORMAT_R8G8B8A8_UNORM, true))
        return false;
    if (!Ctx.CreateRenderPass(RPColorLoad, VK_FORMAT_R8G8B8A8_UNORM, false))
        return false;
    if (!Ctx.CreateRenderPass(RPObjLoad, VK_FORMAT_R8G8B8A8_UNORM, false, 2, true))
        return false;

    // compile shaders

    VkShaderModule layerPreVS = VK_NULL_HANDLE, layerPreFS = VK_NULL_HANDLE;
    VkShaderModule spritePreVS = VK_NULL_HANDLE, spritePreFS = VK_NULL_HANDLE;
    VkShaderModule spriteVS = VK_NULL_HANDLE, spriteFS = VK_NULL_HANDLE;
    VkShaderModule compVS = VK_NULL_HANDLE, compFS = VK_NULL_HANDLE;

    bool ok = true;
    ok = ok && CompilePipelineShader(layerPreVS, VK::Context::ShaderStage::Vertex,
                                     GPU2DShadersVulkan::LayerPreVS, "2DLayerPreVS");
    ok = ok && CompilePipelineShader(layerPreFS, VK::Context::ShaderStage::Fragment,
                                     GPU2DShadersVulkan::LayerPreFS, "2DLayerPreFS");
    ok = ok && CompilePipelineShader(spritePreVS, VK::Context::ShaderStage::Vertex,
                                     GPU2DShadersVulkan::SpritePreVS, "2DSpritePreVS");
    ok = ok && CompilePipelineShader(spritePreFS, VK::Context::ShaderStage::Fragment,
                                     GPU2DShadersVulkan::SpritePreFS, "2DSpritePreFS");
    ok = ok && CompilePipelineShader(spriteVS, VK::Context::ShaderStage::Vertex,
                                     GPU2DShadersVulkan::SpriteVS, "2DSpriteVS");
    ok = ok && CompilePipelineShader(spriteFS, VK::Context::ShaderStage::Fragment,
                                     GPU2DShadersVulkan::SpriteFS, "2DSpriteFS");
    ok = ok && CompilePipelineShader(compVS, VK::Context::ShaderStage::Vertex,
                                     GPU2DShadersVulkan::CompositorVS, "2DCompositorVS");
    ok = ok && CompilePipelineShader(compFS, VK::Context::ShaderStage::Fragment,
                                     GPU2DShadersVulkan::CompositorFS, "2DCompositorFS");

    // build the pipelines

    const VkColorComponentFlags maskAll =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    if (ok)
    {
        // BG layer prerender: fullscreen rect scaled to the layer size
        VK::Context::GraphicsPipelineConfig cfg = {};
        cfg.VertexShader = layerPreVS;
        cfg.FragmentShader = layerPreFS;
        cfg.Layout = PipelineLayout;
        cfg.RenderPass = RPColorClear;
        cfg.VertexStride = 2 * sizeof(float);
        cfg.VertexAttributes = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        };
        ok = Ctx.CreateGraphicsPipeline(LayerPrePipeline, cfg);
    }

    if (ok)
    {
        // sprite prerender into the 1024x512 atlas
        // (the shaders declare integer attributes, hence SINT formats;
        // GL used glVertexAttribIPointer with GL_SHORT)
        VK::Context::GraphicsPipelineConfig cfg = {};
        cfg.VertexShader = spritePreVS;
        cfg.FragmentShader = spritePreFS;
        cfg.Layout = PipelineLayout;
        cfg.RenderPass = RPColorLoad;
        cfg.VertexStride = 3 * sizeof(u16);
        cfg.VertexAttributes = {
            {0, 0, VK_FORMAT_R16G16_SINT, 0},                       // position
            {1, 0, VK_FORMAT_R16_SINT, 2 * sizeof(u16)},            // sprite index
        };
        ok = Ctx.CreateGraphicsPipeline(SpritePrePipeline, cfg);
    }

    if (ok)
    {
        // sprite compositing into the OBJ layer array; the per-pass
        // color-mask/depth-func changes of the GL renderer become three
        // pipeline permutations (see DoRenderSprites)
        auto spriteCfg = [&](bool depth, VkColorComponentFlags mask0, VkColorComponentFlags mask1)
        {
            VK::Context::GraphicsPipelineConfig cfg = {};
            cfg.VertexShader = spriteVS;
            cfg.FragmentShader = spriteFS;
            cfg.Layout = PipelineLayout;
            cfg.RenderPass = RPObjLoad;
            cfg.VertexStride = 5 * sizeof(u16);
            cfg.VertexAttributes = {
                {0, 0, VK_FORMAT_R16G16_SINT, 0},                   // position
                {1, 0, VK_FORMAT_R16G16_SINT, 2 * sizeof(u16)},     // texcoord
                {2, 0, VK_FORMAT_R16_SINT, 4 * sizeof(u16)},        // sprite index
            };
            cfg.ColorAttachmentCount = 2;
            cfg.ColorWriteMasks = {mask0, mask1};
            cfg.DepthTest = depth;
            cfg.DepthWrite = depth;
            cfg.DepthCompare = VK_COMPARE_OP_LESS;                  // GL_LESS
            return cfg;
        };

        // transparent-flag pass (GL: colorMask0 off, colorMask1 = _G_A)
        ok = ok && Ctx.CreateGraphicsPipeline(SpriteTransPipeline,
            spriteCfg(false, 0, VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT));
        // OBJ window pass (GL: colorMask0 off, colorMask1 = __B_)
        ok = ok && Ctx.CreateGraphicsPipeline(SpriteWindowPipeline,
            spriteCfg(false, 0, VK_COLOR_COMPONENT_B_BIT));
        // opaque pass (GL: colorMask0 all, colorMask1 = RG_A, depth GL_LESS)
        ok = ok && Ctx.CreateGraphicsPipeline(SpriteOpaquePipeline,
            spriteCfg(true, maskAll,
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT));
    }

    if (ok)
    {
        // compositor
        VK::Context::GraphicsPipelineConfig cfg = {};
        cfg.VertexShader = compVS;
        cfg.FragmentShader = compFS;
        cfg.Layout = PipelineLayout;
        cfg.RenderPass = RPColorLoad;
        cfg.VertexStride = 2 * sizeof(float);
        cfg.VertexAttributes = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        };
        ok = Ctx.CreateGraphicsPipeline(CompositorPipeline, cfg);
    }

    VkShaderModule modules[8] = {layerPreVS, layerPreFS, spritePreVS, spritePreFS,
                                 spriteVS, spriteFS, compVS, compFS};
    for (VkShaderModule mod : modules)
        if (mod) VK::vkDestroyShaderModule(Ctx.Device, mod, nullptr);

    if (!ok)
        return false;

    // samplers: nearest filtering, one per wrap mode the 2D renderer uses
    // (the GL renderer mutates the wrap mode of the bound BG textures per
    // draw; here it becomes a sampler choice baked into the descriptor set)

    auto createSampler = [&](VkSampler& out, VkSamplerAddressMode mode) -> bool
    {
        VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = mode;
        samplerInfo.addressModeV = mode;
        samplerInfo.addressModeW = mode;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        return VK::vkCreateSampler(Ctx.Device, &samplerInfo, nullptr, &out) == VK_SUCCESS;
    };

    if (!createSampler(SamplerNearestClamp, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE))
        return false;
    if (!createSampler(SamplerNearestRepeat, VK_SAMPLER_ADDRESS_MODE_REPEAT))
        return false;
    if (!createSampler(SamplerNearestBorder, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER))
        return false;

    // generate mosaic lookup texture

    u8* mosaic_tex = new u8[256 * 16];
    for (int m = 0; m < 16; m++)
    {
        int mosx = 0;
        for (int x = 0; x < 256; x++)
        {
            mosaic_tex[(m * 256) + x] = mosx;

            if (mosx == m)
                mosx = 0;
            else
                mosx++;
        }
    }

    if (!Ctx.CreateImage(MosaicImg, VK_FORMAT_R8_SINT, 256, 16, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
    {
        delete[] mosaic_tex;
        return false;
    }
    Ctx.UploadImageLayer(MosaicImg, mosaic_tex, 256, 16, 0, 1);

    delete[] mosaic_tex;
    return true;
}

bool VulkanRenderer2D::InitShaders(VulkanRenderer2D& other)
{
    OwnsShared = false;

    SetLayout = other.SetLayout;
    PipelineLayout = other.PipelineLayout;
    RPColorClear = other.RPColorClear;
    RPColorLoad = other.RPColorLoad;
    RPObjLoad = other.RPObjLoad;

    LayerPrePipeline = other.LayerPrePipeline;
    SpritePrePipeline = other.SpritePrePipeline;
    SpriteTransPipeline = other.SpriteTransPipeline;
    SpriteWindowPipeline = other.SpriteWindowPipeline;
    SpriteOpaquePipeline = other.SpriteOpaquePipeline;
    CompositorPipeline = other.CompositorPipeline;

    SamplerNearestClamp = other.SamplerNearestClamp;
    SamplerNearestRepeat = other.SamplerNearestRepeat;
    SamplerNearestBorder = other.SamplerNearestBorder;

    MosaicImg = other.MosaicImg;

    return true;
}

void VulkanRenderer2D::DeleteShaders()
{
    VkPipeline pipelines[6] = {LayerPrePipeline, SpritePrePipeline,
                               SpriteTransPipeline, SpriteWindowPipeline,
                               SpriteOpaquePipeline, CompositorPipeline};
    for (VkPipeline pipeline : pipelines)
        if (pipeline) VK::vkDestroyPipeline(Ctx.Device, pipeline, nullptr);

    LayerPrePipeline = VK_NULL_HANDLE;
    SpritePrePipeline = VK_NULL_HANDLE;
    SpriteTransPipeline = VK_NULL_HANDLE;
    SpriteWindowPipeline = VK_NULL_HANDLE;
    SpriteOpaquePipeline = VK_NULL_HANDLE;
    CompositorPipeline = VK_NULL_HANDLE;

    if (SamplerNearestClamp) VK::vkDestroySampler(Ctx.Device, SamplerNearestClamp, nullptr);
    if (SamplerNearestRepeat) VK::vkDestroySampler(Ctx.Device, SamplerNearestRepeat, nullptr);
    if (SamplerNearestBorder) VK::vkDestroySampler(Ctx.Device, SamplerNearestBorder, nullptr);
    SamplerNearestClamp = VK_NULL_HANDLE;
    SamplerNearestRepeat = VK_NULL_HANDLE;
    SamplerNearestBorder = VK_NULL_HANDLE;

    if (RPColorClear) VK::vkDestroyRenderPass(Ctx.Device, RPColorClear, nullptr);
    if (RPColorLoad) VK::vkDestroyRenderPass(Ctx.Device, RPColorLoad, nullptr);
    if (RPObjLoad) VK::vkDestroyRenderPass(Ctx.Device, RPObjLoad, nullptr);
    RPColorClear = VK_NULL_HANDLE;
    RPColorLoad = VK_NULL_HANDLE;
    RPObjLoad = VK_NULL_HANDLE;

    if (PipelineLayout) VK::vkDestroyPipelineLayout(Ctx.Device, PipelineLayout, nullptr);
    if (SetLayout) VK::vkDestroyDescriptorSetLayout(Ctx.Device, SetLayout, nullptr);
    PipelineLayout = VK_NULL_HANDLE;
    SetLayout = VK_NULL_HANDLE;

    Ctx.DestroyImage(MosaicImg);
}

bool VulkanRenderer2D::CreatePalView(const VK::Context::Image& img, VkImageView& out)
{
    // the DS palette format is RGBA5551 with red in the low bits, uploaded
    // by the GL renderer as GL_UNSIGNED_SHORT_1_5_5_5_REV. the only 16-bit
    // packed Vulkan format with mandatory sampled-image support is
    // A1R5G5B5 (A = bit 15, B = bits 4-0), which stores the DS blue in its
    // red channel and vice versa; so the palette data is uploaded raw and
    // this r<->b swizzled view hands GetBGPalEntry/GetOBJPalEntry the
    // channel order they expect
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = img.Img;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = img.Format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G,
                           VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_A};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return VK::vkCreateImageView(Ctx.Device, &viewInfo, nullptr, &out) == VK_SUCCESS;
}

bool VulkanRenderer2D::Init()
{
    // sprite prerender vertex data: 2x position, 1x sprite index
    int sprdatasize = (3 * 6) * 128;
    SpritePreVtxData = new u16[sprdatasize];

    // sprite vertex data: 2x position, 2x texcoord, 1x index
    sprdatasize = (5 * 6) * 256;
    SpriteVtxData = new u16[sprdatasize];

    // per-frame transient buffers: sprite vertex data (one slice per draw)
    // and staging memory for the mid-frame VRAM/palette uploads

    if (!Ctx.CreateBuffer(VertexRing.Buf, 8 * 1024 * 1024,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true))
        return false;
    VertexRing.Host.resize(VertexRing.Buf.Size);
    StagingPages.emplace_back();
    if (!Ctx.CreateBuffer(StagingPages.back().Buf, 8 * 1024 * 1024,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
        return false;
    StagingPages.back().Host.resize(StagingPages.back().Buf.Size);

    // fullscreen unit rect (the GL renderer borrows the parent's)

    const float rectvertices[2 * 2 * 3] = {
        0, 1,   1, 0,   1, 1,
        0, 1,   0, 0,   1, 0
    };
    if (!Ctx.CreateBuffer(RectVtxBuffer, sizeof(rectvertices),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true))
        return false;
    memcpy(RectVtxBuffer.Map, rectvertices, sizeof(rectvertices));

    // generate textures to hold raw BG and OBJ VRAM and palettes

    int bgheight = (GPU2D.Num == 0) ? 512 : 128;
    int objheight = (GPU2D.Num == 0) ? 256 : 128;

    if (!Ctx.CreateImage(VRAMTexBG, VK_FORMAT_R8_UINT, 1024, bgheight, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
        return false;

    if (!Ctx.CreateImage(VRAMTexOBJ, VK_FORMAT_R8_UINT, 1024, objheight, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
        return false;

    if (!Ctx.CreateImage(PalTexBG, VK_FORMAT_A1R5G5B5_UNORM_PACK16, 256, 1 + (4*16), 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
        return false;
    if (!CreatePalView(PalTexBG, PalViewBG))
        return false;

    if (!Ctx.CreateImage(PalTexOBJ, VK_FORMAT_A1R5G5B5_UNORM_PACK16, 256, 1 + 16, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
        return false;
    if (!CreatePalView(PalTexOBJ, PalViewOBJ))
        return false;

    // generate textures to hold pre-rendered BG layers

    const u16 bgsizes[8][3] = {
        {128, 128, 2},
        {256, 256, 4},
        {256, 512, 4},
        {512, 256, 4},
        {512, 512, 4},
        {512, 1024, 1},
        {1024, 512, 1},
        {1024, 1024, 2}
    };

    int l = 0;
    for (int j = 0; j < 8; j++)
    {
        const u16* sz = bgsizes[j];

        for (int k = 0; k < sz[2]; k++)
        {
            if (!Ctx.CreateImage(AllBGLayerImg[l], VK_FORMAT_R8G8B8A8_UNORM, sz[0], sz[1], 1,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false))
                return false;
            if (!Ctx.CreateFramebuffer(AllBGLayerFB[l], RPColorClear, AllBGLayerImg[l], 0))
                return false;

            l++;
        }
    }

    // generate texture to hold pre-rendered sprites

    if (!Ctx.CreateImage(SpriteImg, VK_FORMAT_R8G8B8A8_UNORM, 1024, 512, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false))
        return false;
    if (!Ctx.CreateFramebuffer(SpriteFB, RPColorLoad, SpriteImg, 0))
        return false;

    // the final (upscaled) sprite layer and the compositor output are
    // scale-dependent and get created in SetScaleFactor()

    // dummy textures standing in for unset shared resources and
    // not-yet-assigned BG layers, so descriptors are always valid

    if (!Ctx.CreateImage(DummyTex, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
        return false;
    if (!Ctx.CreateImage(DummyTexArray, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true))
        return false;

    const u32 zeroPixel = 0;
    Ctx.UploadImageLayer(DummyTex, &zeroPixel, 1, 1, 0, 4);
    Ctx.UploadImageLayer(DummyTexArray, &zeroPixel, 1, 1, 0, 4);

    // generate UBOs

    static_assert((sizeof(sLayerConfig) & 15) == 0);
    static_assert((sizeof(sSpriteConfig) & 15) == 0);
    static_assert((sizeof(sScanlineConfig) & 15) == 0);
    static_assert((sizeof(sSpriteScanlineConfig) & 15) == 0);
    static_assert((sizeof(sCompositorConfig) & 15) == 0);

    u32 uboAlign = (u32)Ctx.Props.limits.minUniformBufferOffsetAlignment;
    if (uboAlign < 16) uboAlign = 16;

    auto initRing = [&](sConfigRing& ring, u32 size, u32 slots) -> bool
    {
        ring.Stride = (size + uboAlign - 1) & ~(uboAlign - 1);
        ring.Slots = slots;
        ring.Next = 0;
        ring.CurOffset = 0;
        if (!Ctx.CreateBuffer(ring.Buf, (VkDeviceSize)ring.Stride * slots,
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true))
            return false;
        // make the first slice defined for draws recorded before the
        // first config upload
        ring.Host.resize(ring.Buf.Size);
        memset(ring.Host.data(), 0, ring.Host.size());
        memset(ring.Buf.Map, 0, ring.Buf.Size);
        return true;
    };

    if (!initRing(LayerConfigRing, sizeof(sLayerConfig), 256))
        return false;
    if (!initRing(SpriteConfigRing, sizeof(sSpriteConfig), 256))
        return false;
    if (!initRing(CompositorConfigRing, sizeof(sCompositorConfig), 256))
        return false;

    // the scanline configs are only ever written in band-disjoint ranges
    // within a frame, so plain host-visible buffers suffice
    if (!Ctx.CreateBuffer(ScanlineConfigUBO, sizeof(sScanlineConfig),
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true))
        return false;
    ScanlineConfigHost.resize(ScanlineConfigUBO.Size);
    if (!Ctx.CreateBuffer(SpriteScanlineConfigUBO, sizeof(sSpriteScanlineConfig),
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true))
        return false;
    SpriteScanlineConfigHost.resize(SpriteScanlineConfigUBO.Size);

    // descriptor pool; sets are cached per combination of variable
    // bindings, see GetDescriptorSet()

    {
        const u32 maxSets = 256;
        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3 * maxSets},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * maxSets},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 11 * maxSets},
        };
        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = maxSets;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = sizes;
        if (VK::vkCreateDescriptorPool(Ctx.Device, &poolInfo, nullptr, &DescPool) != VK_SUCCESS)
            return false;
    }

    // move every sampled image into a defined layout; the sampled-and-
    // rendered images sit in SHADER_READ_ONLY_OPTIMAL between passes

    {
        VkCommandBuffer cmd = Ctx.BeginOneShot();
        auto toSampled = [&](VK::Context::Image& img)
        {
            Ctx.TransitionImage(cmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        };
        toSampled(VRAMTexBG);
        toSampled(VRAMTexOBJ);
        toSampled(PalTexBG);
        toSampled(PalTexOBJ);
        for (int i = 0; i < 22; i++)
            toSampled(AllBGLayerImg[i]);
        toSampled(SpriteImg);
        Ctx.EndOneShot(cmd);
    }

    return true;
}

VulkanRenderer2D::~VulkanRenderer2D()
{
    if (Ctx.Valid)
    {
        VK::vkDeviceWaitIdle(Ctx.Device);

        DestroyScaleDependentResources();

        if (DescPool) VK::vkDestroyDescriptorPool(Ctx.Device, DescPool, nullptr);

        if (SpriteFB) VK::vkDestroyFramebuffer(Ctx.Device, SpriteFB, nullptr);
        for (int i = 0; i < 22; i++)
            if (AllBGLayerFB[i]) VK::vkDestroyFramebuffer(Ctx.Device, AllBGLayerFB[i], nullptr);

        if (PalViewBG) VK::vkDestroyImageView(Ctx.Device, PalViewBG, nullptr);
        if (PalViewOBJ) VK::vkDestroyImageView(Ctx.Device, PalViewOBJ, nullptr);

        Ctx.DestroyImage(VRAMTexBG);
        Ctx.DestroyImage(VRAMTexOBJ);
        Ctx.DestroyImage(PalTexBG);
        Ctx.DestroyImage(PalTexOBJ);
        for (int i = 0; i < 22; i++)
            Ctx.DestroyImage(AllBGLayerImg[i]);
        Ctx.DestroyImage(SpriteImg);
        Ctx.DestroyImage(DummyTex);
        Ctx.DestroyImage(DummyTexArray);

        Ctx.DestroyBuffer(VertexRing.Buf);
        for (sRingBuffer& page : StagingPages)
            Ctx.DestroyBuffer(page.Buf);
        Ctx.DestroyBuffer(RectVtxBuffer);
        Ctx.DestroyBuffer(LayerConfigRing.Buf);
        Ctx.DestroyBuffer(SpriteConfigRing.Buf);
        Ctx.DestroyBuffer(CompositorConfigRing.Buf);
        Ctx.DestroyBuffer(ScanlineConfigUBO);
        Ctx.DestroyBuffer(SpriteScanlineConfigUBO);

        if (OwnsShared)
            DeleteShaders();
    }

    delete[] SpritePreVtxData;
    delete[] SpriteVtxData;
}

void VulkanRenderer2D::DestroyScaleDependentResources()
{
    if (OBJLayerFB)
    {
        VK::vkDestroyFramebuffer(Ctx.Device, OBJLayerFB, nullptr);
        OBJLayerFB = VK_NULL_HANDLE;
    }
    if (OutputFB)
    {
        VK::vkDestroyFramebuffer(Ctx.Device, OutputFB, nullptr);
        OutputFB = VK_NULL_HANDLE;
    }
    for (int i = 0; i < 2; i++)
    {
        if (OBJLayerView[i])
        {
            VK::vkDestroyImageView(Ctx.Device, OBJLayerView[i], nullptr);
            OBJLayerView[i] = VK_NULL_HANDLE;
        }
    }
    Ctx.DestroyImage(OBJLayerImg);
    Ctx.DestroyImage(OBJDepthImg);
    Ctx.DestroyImage(OutputImg);
}

void VulkanRenderer2D::InvalidateDescriptorCache()
{
    if (DescPool != VK_NULL_HANDLE)
        VK::vkResetDescriptorPool(Ctx.Device, DescPool, 0);
    DescCache.clear();
}

void VulkanRenderer2D::Reset()
{
    SoftFallback->Reset();
    memset(SoftFrameBuffer, 0, sizeof(SoftFrameBuffer));
    SoftFlushedLine = 0;
    UseSoftware2D = false;
    CompositeBands = 0;
    SawVCountMismatch = false;

    memset(BGLayerFB, 0, sizeof(BGLayerFB));
    for (int i = 0; i < 4; i++)
        BGLayerImg[i] = nullptr;

    memset(&LayerConfig, 0, sizeof(LayerConfig));
    memset(&SpriteConfig, 0, sizeof(SpriteConfig));
    memset(&ScanlineConfig, 0, sizeof(ScanlineConfig));
    memset(&SpriteScanlineConfig, 0, sizeof(SpriteScanlineConfig));
    memset(&CompositorConfig, 0, sizeof(CompositorConfig));

    int bgheight = (GPU2D.Num == 0) ? 512 : 128;
    int objheight = (GPU2D.Num == 0) ? 256 : 128;
    LayerConfig.uVRAMMask = bgheight - 1;
    SpriteConfig.uVRAMMask = objheight - 1;

    LastLine = 0;

    UnitEnabled = false;

    DispCnt = 0;
    LayerEnable = 0;
    OBJEnable = 0;
    ForcedBlank = 0;
    memset(BGCnt, 0, sizeof(BGCnt));
    BlendCnt = 0;
    EVA = 0; EVB = 0; EVY = 0;

    memset(BGVRAMRange, 0xFF, sizeof(BGVRAMRange));

    LayerConfigDirty = true;

    LastSpriteLine = 0;
    memset(OAM, 0, sizeof(OAM));
    NumSprites = 0;
    SpriteUseMosaic = false;

    SpriteDispCnt = 0;
    SpriteConfigDirty = true;
    SpriteDirty = true;

    memset(TempPalBuffer, 0, sizeof(TempPalBuffer));
}

void VulkanRenderer2D::PostSavestate()
{
    Reset();
}


void VulkanRenderer2D::SetScaleFactor(int scale)
{
    if (scale == ScaleFactor)
        return;

    if (!Ctx.Valid)
        return;

    VK::vkDeviceWaitIdle(Ctx.Device);

    DestroyScaleDependentResources();

    ScaleFactor = scale;
    ScreenW = 256 * scale;
    ScreenH = 192 * scale;
    SoftUploadBuffer.resize(ScreenW * ScreenH);

    // final (upscaled) sprite layer: a 2-layer array (color + flags)
    // rendered as two color attachments, plus a D16 depth buffer
    // (GL: OBJLayerTex + OBJDepthTex + OBJLayerFB with two draw buffers)

    Ctx.CreateImage(OBJLayerImg, VK_FORMAT_R8G8B8A8_UNORM, ScreenW, ScreenH, 2,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, true);
    Ctx.CreateLayerView(OBJLayerView[0], OBJLayerImg, 0);
    Ctx.CreateLayerView(OBJLayerView[1], OBJLayerImg, 1);

    Ctx.CreateImage(OBJDepthImg, VK_FORMAT_D16_UNORM, ScreenW, ScreenH, 1,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false);

    Ctx.CreateFramebufferMulti(OBJLayerFB, RPObjLoad,
                               {OBJLayerView[0], OBJLayerView[1], OBJDepthImg.View},
                               ScreenW, ScreenH);

    // compositor output

    Ctx.CreateImage(OutputImg, VK_FORMAT_R8G8B8A8_UNORM, ScreenW, ScreenH, 1,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false);
    Ctx.CreateFramebuffer(OutputFB, RPColorLoad, OutputImg, 0);

    // initial layouts: color targets sit in SHADER_READ_ONLY_OPTIMAL
    // between passes, the depth buffer permanently in
    // DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    {
        VkCommandBuffer cmd = Ctx.BeginOneShot();

        Ctx.TransitionImage(cmd, OBJLayerImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        Ctx.TransitionImage(cmd, OutputImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

        // manual barrier: Context::TransitionImage assumes the color aspect
        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = OBJDepthImg.Img;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        VK::vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        OBJDepthImg.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        Ctx.EndOneShot(cmd);
    }

    // the OBJ layer view baked into the cached descriptor sets changed
    InvalidateDescriptorCache();
}

void VulkanRenderer2D::SetSharedResources(const SharedResources& shared)
{
    Shared = shared;
    InvalidateDescriptorCache();
}


// ---- recording helpers ----------------------------------------------------

u32 VulkanRenderer2D::RingAlloc(sRingBuffer& ring, u32 size)
{
    // 256-byte alignment satisfies both vertex binding and
    // vkCmdCopyBufferToImage offset requirements
    size = (size + 255) & ~255u;
    if (size > ring.Buf.Size || ring.Offset > ring.Buf.Size - size)
    {
        if (!ring.Overflowed)
        {
            Log(LogLevel::Error,
                "GPU2D_Vulkan: transient buffer exhausted (used=%u, request=%u, capacity=%llu)\n",
                ring.Offset, size, (unsigned long long)ring.Buf.Size);
            ring.Overflowed = true;
        }
        return ~0u;
    }

    u32 offset = ring.Offset;
    ring.Offset += size;
    return offset;
}

bool VulkanRenderer2D::StagingAlloc(u32 size, sRingBuffer*& page, u32& offset)
{
    const u32 alignedSize = (size + 255) & ~255u;
    for (sRingBuffer& candidate : StagingPages)
    {
        if (alignedSize <= candidate.Buf.Size &&
            candidate.Offset <= candidate.Buf.Size - alignedSize)
        {
            page = &candidate;
            offset = candidate.Offset;
            candidate.Offset += alignedSize;
            return true;
        }
    }

    const u32 pageSize = std::max(8u * 1024 * 1024, alignedSize);
    StagingPages.emplace_back();
    sRingBuffer& newPage = StagingPages.back();
    if (!Ctx.CreateBuffer(newPage.Buf, pageSize,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
    {
        StagingPages.pop_back();
        Log(LogLevel::Error,
            "GPU2D_Vulkan: failed to allocate a %u-byte staging page\n",
            pageSize);
        return false;
    }

    newPage.Host.resize(newPage.Buf.Size);
    newPage.Offset = alignedSize;
    page = &newPage;
    offset = 0;
    return true;
}

void VulkanRenderer2D::RetryStagingUploads()
{
    if (GPU2D.Num == 0)
    {
        GPU.VRAMDirty_ABG.Reset();
        GPU.VRAMDirty_AOBJ.Reset();
        GPU.VRAMDirty_ABGExtPal.Reset();
        GPU.VRAMDirty_AOBJExtPal.Reset();
    }
    else
    {
        GPU.VRAMDirty_BBG.Reset();
        GPU.VRAMDirty_BOBJ.Reset();
        GPU.VRAMDirty_BBGExtPal.Reset();
        GPU.VRAMDirty_BOBJExtPal.Reset();
    }

    GPU.PaletteDirty |= 0x3 << (GPU2D.Num * 2);
    GPU.OAMDirty |= 1 << GPU2D.Num;
    LayerConfigDirty = true;
    SpriteConfigDirty = true;
    SpriteDirty = true;
}

void VulkanRenderer2D::PushConfig(sConfigRing& ring, const void* data, u32 size)
{
    // GL updates these configs with glBufferSubData mid-frame and relies
    // on the driver to keep already-issued draws consistent; here each
    // upload takes a fresh ring slice, bound through a dynamic offset
    if (ring.Next >= ring.Slots)
    {
        if (!ring.Overflowed)
        {
            Log(LogLevel::Error,
                "GPU2D_Vulkan: per-band config buffer exhausted (%u slots)\n",
                ring.Slots);
            ring.Overflowed = true;
        }
        return;
    }

    ring.CurOffset = ring.Next * ring.Stride;
    memcpy(ring.Host.data() + ring.CurOffset, data, size);
    ring.Next++;
}

void VulkanRenderer2D::FlushMappedBuffers()
{
    auto flushRing = [](sRingBuffer& ring)
    {
        if (ring.Offset)
            memcpy(ring.Buf.Map, ring.Host.data(), ring.Offset);
        ring.Offset = 0;
        ring.Overflowed = false;
    };
    auto flushConfig = [](sConfigRing& ring)
    {
        memcpy(ring.Buf.Map, ring.Host.data(), ring.Host.size());
        ring.Next = 0;
        ring.CurOffset = 0;
        ring.Overflowed = false;
    };

    flushRing(VertexRing);
    for (sRingBuffer& page : StagingPages)
        flushRing(page);
    flushConfig(LayerConfigRing);
    flushConfig(SpriteConfigRing);
    flushConfig(CompositorConfigRing);
    memcpy(ScanlineConfigUBO.Map, ScanlineConfigHost.data(), ScanlineConfigHost.size());
    memcpy(SpriteScanlineConfigUBO.Map, SpriteScanlineConfigHost.data(),
           SpriteScanlineConfigHost.size());
}

VkDescriptorSet VulkanRenderer2D::GetDescriptorSet(VkImageView vram, VkImageView pal,
                                                   const VkImageView* bgViews, const VkSampler* bgSamplers)
{
    std::array<uintptr_t, 10> key = {
        (uintptr_t)vram, (uintptr_t)pal,
        (uintptr_t)bgViews[0], (uintptr_t)bgSamplers[0],
        (uintptr_t)bgViews[1], (uintptr_t)bgSamplers[1],
        (uintptr_t)bgViews[2], (uintptr_t)bgSamplers[2],
        (uintptr_t)bgViews[3], (uintptr_t)bgSamplers[3],
    };
    auto it = DescCache.find(key);
    if (it != DescCache.end())
        return it->second;

    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = DescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &SetLayout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (VK::vkAllocateDescriptorSets(Ctx.Device, &allocInfo, &set) != VK_SUCCESS)
    {
        Log(LogLevel::Error, "GPU2D_Vulkan: descriptor pool exhausted\n");
        return VK_NULL_HANDLE;
    }

    VkImageView objView = OBJLayerImg.View ? OBJLayerImg.View : DummyTexArray.View;
    VkImageView cap128 = Shared.Capture128 ? Shared.Capture128 : DummyTexArray.View;
    VkImageView cap256 = Shared.Capture256 ? Shared.Capture256 : DummyTexArray.View;

    VkDescriptorBufferInfo bufInfos[5] = {};
    bufInfos[0] = {LayerConfigRing.Buf.Buf, 0, sizeof(sLayerConfig)};
    bufInfos[1] = {ScanlineConfigUBO.Buf, 0, sizeof(sScanlineConfig)};
    bufInfos[2] = {CompositorConfigRing.Buf.Buf, 0, sizeof(sCompositorConfig)};
    bufInfos[3] = {SpriteConfigRing.Buf.Buf, 0, sizeof(sSpriteConfig)};
    bufInfos[4] = {SpriteScanlineConfigUBO.Buf, 0, sizeof(sSpriteScanlineConfig)};

    const VkImageLayout ro = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo imgInfos[11] = {};
    imgInfos[0] = {SamplerNearestClamp, vram, ro};              // 5: VRAMTex
    imgInfos[1] = {SamplerNearestClamp, pal, ro};               // 6: PalTex
    imgInfos[2] = {SamplerNearestClamp, SpriteImg.View, ro};    // 7: SpriteTex
    for (int i = 0; i < 4; i++)                                 // 8-11: BGLayerTex0-3
        imgInfos[3 + i] = {bgSamplers[i], bgViews[i], ro};
    imgInfos[7] = {SamplerNearestClamp, objView, ro};           // 12: OBJLayerTex
    imgInfos[8] = {SamplerNearestClamp, cap128, ro};            // 13: Capture128Tex
    imgInfos[9] = {SamplerNearestClamp, cap256, ro};            // 14: Capture256Tex
    imgInfos[10] = {SamplerNearestClamp, MosaicImg.View, ro};   // 15: MosaicTex

    const VkDescriptorType uboTypes[5] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    };

    VkWriteDescriptorSet writes[16] = {};
    for (u32 i = 0; i < 16; i++)
    {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        if (i < 5)
        {
            writes[i].descriptorType = uboTypes[i];
            writes[i].pBufferInfo = &bufInfos[i];
        }
        else
        {
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].pImageInfo = &imgInfos[i - 5];
        }
    }
    VK::vkUpdateDescriptorSets(Ctx.Device, 16, writes, 0, nullptr);

    DescCache.emplace(key, set);
    return set;
}

VkDescriptorSet VulkanRenderer2D::GetBGPassDescriptorSet()
{
    // the layer prerender pass only samples VRAMTex/PalTex; the BG slots
    // are filled with dummies to keep the cache key stable
    VkImageView bgViews[4] = {DummyTex.View, DummyTex.View, DummyTex.View, DummyTex.View};
    VkSampler bgSamplers[4] = {SamplerNearestClamp, SamplerNearestClamp,
                               SamplerNearestClamp, SamplerNearestClamp};
    return GetDescriptorSet(VRAMTexBG.View, PalViewBG, bgViews, bgSamplers);
}

VkDescriptorSet VulkanRenderer2D::GetOBJPassDescriptorSet()
{
    VkImageView bgViews[4] = {DummyTex.View, DummyTex.View, DummyTex.View, DummyTex.View};
    VkSampler bgSamplers[4] = {SamplerNearestClamp, SamplerNearestClamp,
                               SamplerNearestClamp, SamplerNearestClamp};
    return GetDescriptorSet(VRAMTexOBJ.View, PalViewOBJ, bgViews, bgSamplers);
}

void VulkanRenderer2D::BindDescriptorSet(VkDescriptorSet set)
{
    // dynamic offsets in binding order: 0 (BGConfig), 2 (CompositorConfig),
    // 3 (SpriteConfig)
    u32 offsets[3] = {LayerConfigRing.CurOffset, CompositorConfigRing.CurOffset,
                      SpriteConfigRing.CurOffset};
    VK::vkCmdBindDescriptorSets(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout,
                                0, 1, &set, 3, offsets);
}

void VulkanRenderer2D::PushConstants(int curBG, int renderTransparent)
{
    sPush2D push;
    push.uCurBG = curBG;
    push.uRenderTransparent = renderTransparent;
    push.uScaleFactor = ScaleFactor;
    VK::vkCmdPushConstants(CurCmd, PipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(push), &push);
}

void VulkanRenderer2D::SetViewportScissor(u32 w, u32 h, u32 scissorY, u32 scissorH)
{
    VkViewport viewport = {0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f};
    VK::vkCmdSetViewport(CurCmd, 0, 1, &viewport);

    VkRect2D scissor = {{0, (s32)scissorY}, {w, scissorH}};
    VK::vkCmdSetScissor(CurCmd, 0, 1, &scissor);
}

void VulkanRenderer2D::BeginPass(VkRenderPass pass, VkFramebuffer fb, u32 w, u32 h)
{
    VkClearValue clears[3] = {};

    VkRenderPassBeginInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    info.renderPass = pass;
    info.framebuffer = fb;
    info.renderArea = {{0, 0}, {w, h}};
    info.clearValueCount = 3;
    info.pClearValues = clears;
    VK::vkCmdBeginRenderPass(CurCmd, &info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer2D::BeginColorTarget(VK::Context::Image& img)
{
    Ctx.TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
}

void VulkanRenderer2D::EndColorTarget(VK::Context::Image& img)
{
    Ctx.TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

void VulkanRenderer2D::BeginTexUpload(VK::Context::Image& img)
{
    Ctx.TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
}

void VulkanRenderer2D::EndTexUpload(VK::Context::Image& img)
{
    Ctx.TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

void VulkanRenderer2D::DepthTargetBarrier()
{
    // write-after-write ordering for the OBJ depth buffer between bands
    // (Context::TransitionImage can't be used, it assumes the color aspect)
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = OBJDepthImg.Img;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    VK::vkCmdPipelineBarrier(CurCmd,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool VulkanRenderer2D::UploadTexRows(VK::Context::Image& img, const void* data,
                                     u32 rowStart, u32 rowCount, u32 bytesPerRow)
{
    // the image must already be in TRANSFER_DST_OPTIMAL (BeginTexUpload).
    // each upload snapshots the data into a fresh staging slice so draws
    // recorded earlier keep sampling the pre-upload contents
    u32 size = rowCount * bytesPerRow;
    sRingBuffer* page;
    u32 offset;
    if (!StagingAlloc(size, page, offset))
        return false;
    memcpy(page->Host.data() + offset, data, size);

    VkBufferImageCopy region = {};
    region.bufferOffset = offset;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, (s32)rowStart, 0};
    region.imageExtent = {img.Width, rowCount, 1};
    VK::vkCmdCopyBufferToImage(CurCmd, page->Buf.Buf, img.Img,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return true;
}

bool VulkanRenderer2D::UploadTexRectR8(VK::Context::Image& img, const void* data,
                                       u32 x, u32 rowStart, u32 width, u32 rowCount)
{
    u32 size = width * rowCount;
    sRingBuffer* page;
    u32 offset;
    if (!StagingAlloc(size, page, offset))
        return false;
    memcpy(page->Host.data() + offset, data, size);

    VkBufferImageCopy region = {};
    region.bufferOffset = offset;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {(s32)x, (s32)rowStart, 0};
    region.imageExtent = {width, rowCount, 1};
    VK::vkCmdCopyBufferToImage(CurCmd, page->Buf.Buf, img.Img,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return true;
}


// ---- mirrored GLRenderer2D logic -------------------------------------------

bool VulkanRenderer2D::IsScreenOn()
{
    if (!GPU.ScreensEnabled) return false;
    if (!GPU2D.Enabled) return false;
    if (GPU2D.ForcedBlank) return false;

    u16 masterbright = GPU2D.Num ? GPU.MasterBrightnessB : GPU.MasterBrightnessA;
    u16 brightmode = masterbright >> 14;
    u16 brightness = masterbright & 0x1F;
    if ((brightmode == 1 || brightmode == 2) && brightness >= 16)
        return false;

    u16 layers = GPU2D.LayerEnable | 0x20;
    u16 bldeffect = (GPU2D.BlendCnt >> 6) & 0x3;
    u16 bldlayers = GPU2D.BlendCnt & layers & 0x3F;
    if ((bldeffect == 2 || bldeffect == 3) && bldlayers == layers && GPU2D.EVY >= 16 &&
        !(GPU2D.DispCnt & 0xE000))
        return false;

    u32 dispmode = (GPU2D.DispCnt >> 16) & 0x3;
    if (dispmode != 1)
    {
        if (GPU2D.Num) return false;
        if (!GPU.CaptureEnable) return false;
    }

    return true;
}

void VulkanRenderer2D::UpdateAndRender(int line)
{
    u32 palmask = 1 << (GPU2D.Num * 2);

    // check if any 'critical' registers were modified

    u32 dispcnt_diff;
    u8 layer_diff;
    u16 bgcnt_diff[4];

    dispcnt_diff = GPU2D.DispCnt ^ DispCnt;
    layer_diff = GPU2D.LayerEnable ^ LayerEnable;
    for (int layer = 0; layer < 4; layer++)
        bgcnt_diff[layer] = GPU2D.BGCnt[layer] ^ BGCnt[layer];

    u8 layer_pre_dirty = 0;
    bool comp_dirty = false;
    bool screenon = IsScreenOn();

    if (dispcnt_diff & 0x8)
        layer_pre_dirty |= 0x1;
    if (dispcnt_diff & 0x7)
        layer_pre_dirty |= 0xC;
    if (dispcnt_diff & 0x7F000000)
        layer_pre_dirty |= 0xF;

    if (dispcnt_diff & 0x0000E008)
        comp_dirty = true;
    else if (layer_diff & 0x1F)
        comp_dirty = true;
    else if (UnitEnabled != GPU2D.Enabled)
        comp_dirty = true;
    else if (ForcedBlank != GPU2D.ForcedBlank)
        comp_dirty = true;

    for (int layer = 0; layer < 4; layer++)
    {
        u16 mask = 0xDFBC;
        if (layer < 2) mask |= (1 << 13);
        if (bgcnt_diff[layer] & mask)
            layer_pre_dirty |= (1 << layer);
        if (bgcnt_diff[layer] & (~mask))
            comp_dirty = true;
    }

    if ((GPU2D.BlendCnt != BlendCnt) ||
        (GPU2D.EVA != EVA) ||
        (GPU2D.EVB != EVB) ||
        (GPU2D.EVY != EVY))
        comp_dirty = true;

    // check if VRAM was modified, and flatten it as needed

    static_assert(VRAMDirtyGranularity == 512);
    NonStupidBitField<1024> bgDirty;
    NonStupidBitField<64> bgExtPalDirty;
    NonStupidBitField<16> objExtPalDirty;

    if (screenon)
    {
        if (GPU2D.Num == 0)
        {
            bgDirty = GPU.VRAMDirty_ABG.DeriveState(GPU.VRAMMap_ABG, GPU);
            GPU.MakeVRAMFlat_ABGCoherent(bgDirty);

            bgExtPalDirty = GPU.VRAMDirty_ABGExtPal.DeriveState(GPU.VRAMMap_ABGExtPal, GPU);
            GPU.MakeVRAMFlat_ABGExtPalCoherent(bgExtPalDirty);
            objExtPalDirty = GPU.VRAMDirty_AOBJExtPal.DeriveState(&GPU.VRAMMap_AOBJExtPal, GPU);
            GPU.MakeVRAMFlat_AOBJExtPalCoherent(objExtPalDirty);
        }
        else
        {
            auto _bgDirty = GPU.VRAMDirty_BBG.DeriveState(GPU.VRAMMap_BBG, GPU);
            GPU.MakeVRAMFlat_BBGCoherent(_bgDirty);
            for (int i = 0; i < 1024; i += 256)
                memcpy(&bgDirty.Data[i>>6], _bgDirty.Data, 256>>3);

            bgExtPalDirty = GPU.VRAMDirty_BBGExtPal.DeriveState(GPU.VRAMMap_BBGExtPal, GPU);
            GPU.MakeVRAMFlat_BBGExtPalCoherent(bgExtPalDirty);
            objExtPalDirty = GPU.VRAMDirty_BOBJExtPal.DeriveState(&GPU.VRAMMap_BOBJExtPal, GPU);
            GPU.MakeVRAMFlat_BOBJExtPalCoherent(objExtPalDirty);
        }
    }

    // for each layer, check if the VRAM and palettes involved are dirty

    for (int layer = 0; layer < 4; layer++)
    {
        const u32* rangeinfo = BGVRAMRange[layer];

        // to consider: only check the tileset range that is actually used
        // (would require parsing the tilemap)
        for (int r = 0; r < 4; r+=2)
        {
            if (rangeinfo[r] == 0xFFFFFFFF)
                continue;

            bool dirty = false;
            u32 rstart = (rangeinfo[r] >> 9) & 0x3FF;
            u32 rcount = (rangeinfo[r+1] >> 9);
            if ((rstart + rcount) > 1024)
            {
                dirty = bgDirty.CheckRange(rstart, 1024-rstart) ||
                        bgDirty.CheckRange(0, rcount-(1024-rstart));
            }
            else
                dirty = bgDirty.CheckRange(rstart, rcount);

            if (dirty)
                layer_pre_dirty |= (1 << layer);
        }

        auto& cfg = LayerConfig.uBGConfig[layer];
        if ((cfg.Type == 1 || cfg.Type == 3) && (cfg.PalOffset > 0))
        {
            u32 pal = cfg.PalOffset - 1;
            if (bgExtPalDirty.CheckRange(pal, 16))
                layer_pre_dirty |= (1 << layer);
        }
        else if (cfg.Type <= 4)
        {
            if (GPU.PaletteDirty & palmask)
                layer_pre_dirty |= (1 << layer);
        }
    }

    // Keep disabled BGs lazy. Their source texture remains coherent, and a
    // newly-enabled BG is rebuilt before it can reach the compositor.
    const u8 enabledBGs = GPU2D.LayerEnable & 0xF;
    const u8 newlyEnabledBGs = enabledBGs & ~LayerEnable;
    layer_pre_dirty = (layer_pre_dirty & enabledBGs) | newlyEnabledBGs;

    if (layer_pre_dirty)
        comp_dirty = true;

    if (NeedPartialRender)
        comp_dirty = true;

    // if needed, render sprites

    if ((comp_dirty || SpriteDirty) && (line > 0))
    {
        DoRenderSprites(line);
    }

    // if needed, composite the previous screen section

    if (comp_dirty && (line > 0))
    {
        CompositeBands++;
        RenderScreen(LastLine, line);
        LastLine = line;
    }

    // update registers

    UnitEnabled = GPU2D.Enabled;
    DispCnt = GPU2D.DispCnt;
    LayerEnable = GPU2D.LayerEnable;
    OBJEnable = GPU2D.OBJEnable;
    ForcedBlank = GPU2D.ForcedBlank;
    for (int layer = 0; layer < 4; layer++)
        BGCnt[layer] = GPU2D.BGCnt[layer];
    BlendCnt = GPU2D.BlendCnt;
    EVA = GPU2D.EVA;
    EVB = GPU2D.EVB;
    EVY = GPU2D.EVY;

    if (layer_pre_dirty || LayerConfigDirty)
        UpdateLayerConfig();

    UpdateScanlineConfig(line);

    // update VRAM and palettes
    // (staging copies get recorded before the draws that sample them)

    bool stagingUploadFailed = false;
    int dirtybits = GPU2D.Num ? 256 : 1024;
    if (bgDirty.CheckRange(0, dirtybits))
    {
        // TODO: only do it for active layers?
        // this would require keeping track of the dirty state for areas not included in any layer

        u8 *vram;
        u32 vrammask;
        GPU2D.GetBGVRAM(vram, vrammask);

        bool uploading = false;

        for (int i = 0; i < dirtybits; )
        {
            if (!bgDirty[i])
            {
                i++;
                continue;
            }

            if (!uploading)
            {
                BeginTexUpload(VRAMTexBG);
                uploading = true;
            }

            if (i & 1)
            {
                if (!UploadTexRectR8(VRAMTexBG, &vram[i * 512],
                                     512, i >> 1, 512, 1))
                    stagingUploadFailed = true;
                i++;
                continue;
            }

            int start = i;
            while ((i + 1) < dirtybits && bgDirty[i] && bgDirty[i + 1])
                i += 2;

            if (i > start)
            {
                if (!UploadTexRectR8(VRAMTexBG, &vram[start * 512],
                                     0, start >> 1, 1024, (i - start) >> 1))
                    stagingUploadFailed = true;
                continue;
            }

            if (!UploadTexRectR8(VRAMTexBG, &vram[i * 512],
                                 0, i >> 1, 512, 1))
                stagingUploadFailed = true;
            i++;
        }

        if (uploading)
            EndTexUpload(VRAMTexBG);
    }

    if ((GPU.PaletteDirty & palmask) || bgExtPalDirty.CheckRange(0, 64))
    {
        memcpy(&TempPalBuffer[0], &GPU.Palette[GPU2D.Num ? 0x400 : 0], 256*2);
        for (int s = 0; s < 4; s++)
        {
            for (int p = 0; p < 16; p++)
            {
                u16 *pal = GPU2D.GetBGExtPal(s, p);
                memcpy(&TempPalBuffer[(1 + ((s*16)+p)) * 256], pal, 256*2);
            }
        }

        BeginTexUpload(PalTexBG);
        if (!UploadTexRows(PalTexBG, TempPalBuffer, 0, 1+(4*16), 256*2))
            stagingUploadFailed = true;
        EndTexUpload(PalTexBG);
    }

    GPU.PaletteDirty &= ~palmask;

    if (layer_pre_dirty)
    {
        // pre-render BG layers with the new settings

        for (int layer = 0; layer < 4; layer++)
        {
            if (!(layer_pre_dirty & (1 << layer)))
                continue;

            PrerenderLayer(layer);
        }
    }

    if (SpriteDirty)
    {
        // OAM and VRAM have already been updated prior
        // palette needs to be updated here though

        // TODO make this only do it over the required subsection?
        NumSprites = 0;
        SpriteUseMosaic = false;
        UpdateOAM(0, 192);

        memcpy(&TempPalBuffer[0], &GPU.Palette[GPU2D.Num ? 0x600 : 0x200], 256*2);
        {
            u16* pal = GPU2D.GetOBJExtPal();
            memcpy(&TempPalBuffer[256], pal, 256*16*2);
        }

        BeginTexUpload(PalTexOBJ);
        if (!UploadTexRows(PalTexOBJ, TempPalBuffer, 0, 1+16, 256*2))
            stagingUploadFailed = true;
        EndTexUpload(PalTexOBJ);

        PrerenderSprites();

        LastSpriteLine = line;
    }

    LayerConfigDirty = false;
    SpriteDirty = false;
    if (stagingUploadFailed)
        RetryStagingUploads();
}


void VulkanRenderer2D::DrawSoftwareLine(u32 line)
{
    // The scheduled output line and emulated VCOUNT can differ when software
    // writes VCOUNT. Match SoftRenderer: render state for VCOUNT, but store
    // the result in the scheduled output line.
    const u32 drawLine = GPU.VCount;
    if (drawLine >= 192)
    {
        for (u32& color : SoftOutput)
            color = 0xFF3F3F3F;
    }
    else
    {
        if (GPU2D.Num == 0 && (GPU2D.DispCnt & (1 << 3)))
            SoftFallback->SetOutput3D(Parent.GetLine3D(drawLine));

        SoftFallback->DrawScanline(drawLine);
    }
    memcpy(&SoftFrameBuffer[line * 256], SoftOutput, sizeof(SoftOutput));

    if (NeedPartialRender)
        FlushSoftwareLines(line + 1);
}

void VulkanRenderer2D::FlushSoftwareLines(u32 endLine)
{
    endLine = std::min(endLine, 192u);
    if (SoftFlushedLine >= endLine)
        return;

    u32* dst = SoftUploadBuffer.data();
    for (u32 line = SoftFlushedLine; line < endLine; line++)
    {
        u32* row = dst;
        for (int x = 0; x < 256; x++)
        {
            u32 color = SoftFrameBuffer[(line * 256) + x];
            color = ((color & 0x0000003F) << 2) |
                    ((color & 0x00003F00) << 2) |
                    ((color & 0x003F0000) << 2) |
                    0xFF000000;
            for (int sx = 0; sx < ScaleFactor; sx++)
                *row++ = color;
        }

        const size_t rowBytes = ScreenW * sizeof(u32);
        for (int sy = 0; sy < ScaleFactor; sy++)
        {
            if (sy > 0)
                memcpy(dst, dst - ScreenW, rowBytes);
            dst += ScreenW;
        }
    }

    const size_t rowBytes = ScreenW * sizeof(u32);
    const u32 uploadRows = (endLine - SoftFlushedLine) * ScaleFactor;

    BeginTexUpload(OutputImg);
    const bool uploaded = UploadTexRows(OutputImg, SoftUploadBuffer.data(),
                                        SoftFlushedLine * ScaleFactor,
                                        uploadRows, rowBytes);
    EndTexUpload(OutputImg);
    if (uploaded)
        SoftFlushedLine = endLine;
}

void VulkanRenderer2D::DrawScanline(u32 line)
{
    if (CurCmd == VK_NULL_HANDLE)
        return;

    SawVCountMismatch |= line != GPU.VCount;

    if (UseSoftware2D)
        DrawSoftwareLine(line);
    else
        UpdateAndRender(line);
}

void VulkanRenderer2D::Flush(u32 endLine)
{
    if (CurCmd == VK_NULL_HANDLE)
        return;

    endLine = std::min(endLine, 192u);
    if (UseSoftware2D)
        FlushSoftwareLines(endLine);
    else if (endLine > (u32)LastLine)
    {
        DoRenderSprites(endLine);
        RenderScreen(LastLine, endLine);
        LastLine = endLine;
    }
}

void VulkanRenderer2D::FinishFrame(u32 endLine)
{
    Flush(endLine);

    if (UseSoftware2D)
        SoftFlushedLine = 0;
    else if (CurCmd != VK_NULL_HANDLE)
    {

        // The accelerated 2D design pre-renders whole BGs whenever their
        // source VRAM changes. A workload that streams BG data on most
        // scanlines can therefore turn one frame into hundreds of full GPU
        // passes. At modest scale factors, the scanline-accurate software
        // compositor is both faster and exact; 3D is read back once per frame.
        if (ScaleFactor <= 4 && (CompositeBands > 64 || SawVCountMismatch))
        {
            SoftFallback->Reset();
            UseSoftware2D = true;
        }
    }

    LastSpriteLine = 0;
    LastLine = 0;

    CompositeBands = 0;
    SawVCountMismatch = false;

    // Transient allocation offsets are reset when the parent publishes the
    // CPU mirrors immediately before submitting this frame.
}

void VulkanRenderer2D::VBlank()
{
    FinishFrame(192);
}

void VulkanRenderer2D::VBlankEnd()
{
}


void VulkanRenderer2D::UpdateScanlineConfig(int line)
{
    auto& cfg = ScanlineConfig.uScanline[line];

    // update BG layer coordinates
    // Y coordinates are adjusted to account for vertical mosaic
    // horizontal mosaic will be done during compositing

    u32 bgmode = DispCnt & 0x7;
    bool xmosaic = (GPU2D.BGMosaicSize[0] > 0);

    if (DispCnt & (1<<3))
    {
        // 3D layer
        int xpos = GPU.GPU3D.GetRenderXPos() & 0x1FF;
        cfg.BGOffset[0][0] = xpos - ((xpos & 0x100) << 1);
        cfg.BGOffset[0][1] = line;
        cfg.BGMosaicEnable[0] = false;
    }
    else
    {
        // text layer
        cfg.BGOffset[0][0] = GPU2D.BGXPos[0];
        if (GPU2D.BGCnt[0] & (1<<6))
        {
            cfg.BGOffset[0][1] = GPU2D.BGYPos[0] + GPU2D.BGMosaicLine;
            cfg.BGMosaicEnable[0] = xmosaic;
        }
        else
        {
            cfg.BGOffset[0][1] = GPU2D.BGYPos[0] + line;
            cfg.BGMosaicEnable[0] = false;
        }
    }

    // always a text layer
    cfg.BGOffset[1][0] = GPU2D.BGXPos[1];
    if (GPU2D.BGCnt[1] & (1<<6))
    {
        cfg.BGOffset[1][1] = GPU2D.BGYPos[1] + GPU2D.BGMosaicLine;
        cfg.BGMosaicEnable[1] = xmosaic;
    }
    else
    {
        cfg.BGOffset[1][1] = GPU2D.BGYPos[1] + line;
        cfg.BGMosaicEnable[1] = false;
    }

    if ((bgmode == 2) || (bgmode >= 4 && bgmode <= 6))
    {
        // rotscale layer
        cfg.BGOffset[2][0] = GPU2D.BGXRefInternal[0];
        cfg.BGOffset[2][1] = GPU2D.BGYRefInternal[0];
        cfg.BGRotscale[0][0] = GPU2D.BGRotA[0];
        cfg.BGRotscale[0][1] = GPU2D.BGRotB[0];
        cfg.BGRotscale[0][2] = GPU2D.BGRotC[0];
        cfg.BGRotscale[0][3] = GPU2D.BGRotD[0];
    }
    else
    {
        // text layer
        cfg.BGOffset[2][0] = GPU2D.BGXPos[2];
        if (GPU2D.BGCnt[2] & (1<<6))
            cfg.BGOffset[2][1] = GPU2D.BGYPos[2] + GPU2D.BGMosaicLine;
        else
            cfg.BGOffset[2][1] = GPU2D.BGYPos[2] + line;
    }

    if (GPU2D.BGCnt[2] & (1<<6))
        cfg.BGMosaicEnable[2] = xmosaic;
    else
        cfg.BGMosaicEnable[2] = false;

    if (bgmode >= 1 && bgmode <= 5)
    {
        // rotscale layer
        cfg.BGOffset[3][0] = GPU2D.BGXRefInternal[1];
        cfg.BGOffset[3][1] = GPU2D.BGYRefInternal[1];
        cfg.BGRotscale[1][0] = GPU2D.BGRotA[1];
        cfg.BGRotscale[1][1] = GPU2D.BGRotB[1];
        cfg.BGRotscale[1][2] = GPU2D.BGRotC[1];
        cfg.BGRotscale[1][3] = GPU2D.BGRotD[1];
    }
    else
    {
        // text layer
        cfg.BGOffset[3][0] = GPU2D.BGXPos[3];
        if (GPU2D.BGCnt[3] & (1<<6))
            cfg.BGOffset[3][1] = GPU2D.BGYPos[3] + GPU2D.BGMosaicLine;
        else
            cfg.BGOffset[3][1] = GPU2D.BGYPos[3] + line;
    }

    if (GPU2D.BGCnt[3] & (1<<6))
        cfg.BGMosaicEnable[3] = xmosaic;
    else
        cfg.BGMosaicEnable[3] = false;

    u16* pal = (u16*)&GPU.Palette[GPU2D.Num ? 0x400 : 0];
    cfg.BackColor = pal[0];

    // mosaic

    cfg.MosaicSize[0] = GPU2D.BGMosaicSize[0];
    cfg.MosaicSize[1] = GPU2D.BGMosaicSize[1];
    cfg.MosaicSize[2] = GPU2D.OBJMosaicSize[0];
    cfg.MosaicSize[3] = GPU2D.OBJMosaicSize[1];

    // windows

    //cfg.WinRegs = GPU2D.WinCnt[2] | (GPU2D.WinCnt[3] << 8) | (GPU2D.WinCnt[1] << 16) | (GPU2D.WinCnt[0] << 24);
    if (GPU2D.DispCnt & 0xE000)
        cfg.WinRegs = GPU2D.WinCnt[2];
    else
        cfg.WinRegs = 0xFF;

    if (GPU2D.DispCnt & (1<<15))
        cfg.WinRegs |= (GPU2D.WinCnt[3] << 8);
    else
        cfg.WinRegs |= 0xFF00;

    if (GPU2D.DispCnt & (1<<14))
        cfg.WinRegs |= (GPU2D.WinCnt[1] << 16);
    else
        cfg.WinRegs |= 0xFF0000;

    if (GPU2D.DispCnt & (1<<13))
        cfg.WinRegs |= (GPU2D.WinCnt[0] << 24);
    else
        cfg.WinRegs |= 0xFF000000;

    cfg.WinMask = 0;

    if ((GPU2D.DispCnt & (1<<13)) && (GPU2D.Win0Active & 0x1))
    {
        int x0 = GPU2D.Win0Coords[0];
        int x1 = GPU2D.Win0Coords[1];

        if (x0 <= x1)
        {
            cfg.WinPos[0] = x0;
            cfg.WinPos[1] = x1;
            if (GPU2D.Win0Active == 0x3)
                cfg.WinMask |= (1<<0);
            cfg.WinMask |= (1<<1);
            GPU2D.Win0Active &= ~0x2;
        }
        else
        {
            cfg.WinPos[0] = x1;
            cfg.WinPos[1] = x0;
            if (GPU2D.Win0Active == 0x3)
                cfg.WinMask |= (1<<0);
            cfg.WinMask |= (1<<2);
            GPU2D.Win0Active |= 0x2;
        }
    }
    else
    {
        cfg.WinPos[0] = 256;
        cfg.WinPos[1] = 256;
    }

    if ((GPU2D.DispCnt & (1<<14)) && (GPU2D.Win1Active & 0x1))
    {
        int x0 = GPU2D.Win1Coords[0];
        int x1 = GPU2D.Win1Coords[1];

        if (x0 <= x1)
        {
            cfg.WinPos[2] = x0;
            cfg.WinPos[3] = x1;
            if (GPU2D.Win1Active == 0x3)
                cfg.WinMask |= (1<<3);
            cfg.WinMask |= (1<<4);
            GPU2D.Win1Active &= ~0x2;
        }
        else
        {
            cfg.WinPos[2] = x1;
            cfg.WinPos[3] = x0;
            if (GPU2D.Win1Active == 0x3)
                cfg.WinMask |= (1<<3);
            cfg.WinMask |= (1<<5);
            GPU2D.Win1Active |= 0x2;
        }
    }
    else
    {
        cfg.WinPos[2] = 256;
        cfg.WinPos[3] = 256;
    }
};

void VulkanRenderer2D::UpdateLayerConfig()
{
    // determine which parts of VRAM were used for captures
    int capturemask = GPU2D.Num ? 0x7 : 0x1F;
    int captureinfo[32];
    GPU2D.GetCaptureInfo_BG(captureinfo);

    u32 tilebase, mapbase;
    if (!GPU2D.Num)
    {
        tilebase = ((GPU2D.DispCnt >> 24) & 0x7) << 16;
        mapbase = ((GPU2D.DispCnt >> 27) & 0x7) << 16;
    }
    else
    {
        tilebase = 0;
        mapbase = 0;
    }

    int layertype[4] = {1, 1, 0, 0};
    switch (GPU2D.DispCnt & 0x7)
    {
        case 0: layertype[2] = 1; layertype[3] = 1; break;
        case 1: layertype[2] = 1; layertype[3] = 2; break;
        case 2: layertype[2] = 2; layertype[3] = 2; break;
        case 3: layertype[2] = 1; layertype[3] = 3; break;
        case 4: layertype[2] = 2; layertype[3] = 3; break;
        case 5: layertype[2] = 3; layertype[3] = 3; break;
        case 6: layertype[0] = 0; layertype[1] = 0;
                layertype[2] = 4; layertype[3] = 0; break;
        case 7: layertype[2] = 0; layertype[3] = 0; break;
    }

    for (int layer = 0; layer < 4; layer++)
    {
        int type = layertype[layer];
        if (!type)
            continue;

        u16 bgcnt = GPU2D.BGCnt[layer];
        auto& cfg = LayerConfig.uBGConfig[layer];

        cfg.TileOffset = tilebase + (((bgcnt >> 2) & 0xF) << 14);
        cfg.MapOffset = mapbase + (((bgcnt >> 8) & 0x1F) << 11);
        cfg.PalOffset = 0;

        BGVRAMRange[layer][0] = cfg.TileOffset;
        BGVRAMRange[layer][2] = cfg.MapOffset;

        if ((layer == 0) && (GPU2D.DispCnt & (1<<3)))
        {
            // 3D layer

            cfg.Size[0] = 256; cfg.Size[1] = 192;
            cfg.Type = 6;
            cfg.Clamp = 1;

            BGVRAMRange[layer][0] = 0xFFFFFFFF;
            BGVRAMRange[layer][1] = 0xFFFFFFFF;
            BGVRAMRange[layer][2] = 0xFFFFFFFF;
            BGVRAMRange[layer][3] = 0xFFFFFFFF;
        }
        else if (type == 1)
        {
            // text layer

            u32 tilesz, mapsz;
            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x800; break;
                case 1: cfg.Size[0] = 512; cfg.Size[1] = 256; mapsz = 0x1000; break;
                case 2: cfg.Size[0] = 256; cfg.Size[1] = 512; mapsz = 0x1000; break;
                case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x2000; break;
            }

            if (bgcnt & (1<<7))
            {
                // 256-color
                cfg.Type = 1;
                if (DispCnt & (1<<30))
                {
                    // extended palette
                    int paloff = layer;
                    if ((layer < 2) && (bgcnt & (1<<13)))
                        paloff += 2;
                    cfg.PalOffset = 1 + (16 * paloff);
                }

                tilesz = 0x10000;
            }
            else
            {
                // 16-color
                cfg.Type = 0;

                tilesz = 0x8000;
            }

            cfg.Clamp = 0;

            int n = BGBaseIndex[0][bgcnt >> 14] + layer;
            BGLayerImg[layer] = &AllBGLayerImg[n];
            BGLayerFB[layer] = AllBGLayerFB[n];

            BGVRAMRange[layer][1] = tilesz;
            BGVRAMRange[layer][3] = mapsz;
        }
        else if (type == 2)
        {
            // affine layer

            u32 mapsz;
            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; mapsz = 0x100; break;
                case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x400; break;
                case 2: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x1000; break;
                case 3: cfg.Size[0] = 1024; cfg.Size[1] = 1024; mapsz = 0x4000; break;
            }

            cfg.Type = 2;
            cfg.Clamp = !(bgcnt & (1<<13));

            int n = BGBaseIndex[1][bgcnt >> 14] + layer - 2;
            BGLayerImg[layer] = &AllBGLayerImg[n];
            BGLayerFB[layer] = AllBGLayerFB[n];

            BGVRAMRange[layer][1] = 0x4000;
            BGVRAMRange[layer][3] = mapsz;
        }
        else if (type == 3)
        {
            // extended layer

            if (bgcnt & (1<<7))
            {
                // bitmap modes

                u32 mapsz;
                switch (bgcnt >> 14)
                {
                    case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; mapsz = 0x4000; break;
                    case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x10000; break;
                    case 2: cfg.Size[0] = 512; cfg.Size[1] = 256; mapsz = 0x20000; break;
                    case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x40000; break;
                }

                u32 tileoffset = 0;
                u32 mapoffset = ((bgcnt >> 8) & 0x1F) << 14;

                BGVRAMRange[layer][0] = 0xFFFFFFFF;
                BGVRAMRange[layer][1] = 0xFFFFFFFF;
                BGVRAMRange[layer][2] = mapoffset;
                BGVRAMRange[layer][3] = mapsz;

                if (bgcnt & (1<<2))
                {
                    mapsz <<= 1;

                    int capblock = -1;
                    if ((cfg.Size[0] == 128) || (cfg.Size[0] == 256))
                    {
                        // if this is a direct color bitmap, and the width is 128 or 256
                        // then it might be a display capture
                        u32 startaddr = mapoffset;
                        u32 endaddr = startaddr + mapsz;

                        startaddr >>= 14;
                        endaddr = (endaddr + 0x3FFF) >> 14;

                        for (u32 b = startaddr; b < endaddr; b++)
                        {
                            int blk = captureinfo[b & capturemask];
                            if (blk == -1) continue;

                            capblock = blk;
                        }
                    }

                    if (capblock != -1)
                    {
                        if (cfg.Size[0] == 128)
                        {
                            cfg.Type = 7;
                            tileoffset = capblock;
                            mapoffset = (mapoffset >> 8) & 0x7F;
                        }
                        else
                        {
                            cfg.Type = 8;
                            tileoffset = capblock >> 2;
                            mapoffset = (mapoffset >> 9) & 0xFF;
                        }
                    }
                    else
                        cfg.Type = 5;
                }
                else
                    cfg.Type = 4;

                cfg.TileOffset = tileoffset;
                cfg.MapOffset = mapoffset;

                int n = BGBaseIndex[2][bgcnt >> 14] + layer - 2;
                BGLayerImg[layer] = &AllBGLayerImg[n];
                BGLayerFB[layer] = AllBGLayerFB[n];
            }
            else
            {
                // rotscale w/ tiles

                u32 mapsz;
                switch (bgcnt >> 14)
                {
                    case 0: cfg.Size[0] = 128; cfg.Size[1] = 128; mapsz = 0x200; break;
                    case 1: cfg.Size[0] = 256; cfg.Size[1] = 256; mapsz = 0x800; break;
                    case 2: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x2000; break;
                    case 3: cfg.Size[0] = 1024; cfg.Size[1] = 1024; mapsz = 0x8000; break;
                }

                // this layer type is always 256-color
                cfg.Type = 3;
                if (DispCnt & (1<<30))
                {
                    // extended palette
                    int paloff = layer;
                    if ((layer < 2) && (bgcnt & (1<<13)))
                        paloff += 2;
                    cfg.PalOffset = 1 + (16 * paloff);
                }

                int n = BGBaseIndex[1][bgcnt >> 14] + layer - 2;
                BGLayerImg[layer] = &AllBGLayerImg[n];
                BGLayerFB[layer] = AllBGLayerFB[n];

                BGVRAMRange[layer][1] = 0x10000;
                BGVRAMRange[layer][3] = mapsz;
            }

            cfg.Clamp = !(bgcnt & (1<<13));
        }
        else //if (type == 4)
        {
            // large layer

            u32 mapsz;
            switch (bgcnt >> 14)
            {
                case 0: cfg.Size[0] = 512; cfg.Size[1] = 1024; mapsz = 0x80000; break;
                case 1: cfg.Size[0] = 1024; cfg.Size[1] = 512; mapsz = 0x80000; break;
                case 2: cfg.Size[0] = 512; cfg.Size[1] = 256; mapsz = 0x20000; break;
                case 3: cfg.Size[0] = 512; cfg.Size[1] = 512; mapsz = 0x40000; break;
            }

            cfg.Type = 4;
            cfg.TileOffset = 0;
            cfg.MapOffset = 0;
            cfg.Clamp = !(bgcnt & (1<<13));

            int n = BGBaseIndex[3][bgcnt >> 14];
            BGLayerImg[layer] = &AllBGLayerImg[n];
            BGLayerFB[layer] = AllBGLayerFB[n];

            BGVRAMRange[layer][0] = 0xFFFFFFFF;
            BGVRAMRange[layer][1] = 0xFFFFFFFF;
            BGVRAMRange[layer][3] = mapsz;
        }
    }

    PushConfig(LayerConfigRing, &LayerConfig, sizeof(LayerConfig));
}

void VulkanRenderer2D::UpdateOAM(int ystart, int yend)
{
    auto& cfg = SpriteConfig;
    u16* oam = OAM;

    // determine which parts of VRAM were used for captures
    int capturemask = GPU2D.Num ? 0x7 : 0xF;
    int captureinfo[16];
    GPU2D.GetCaptureInfo_OBJ(captureinfo);

    for (int i = 0; i < 32; i++)
    {
        s16* rotscale = (s16*)&oam[(i * 16) + 3];
        auto& rotdst = cfg.uRotscale[i];

        rotdst[0] = rotscale[0];
        rotdst[1] = rotscale[4];
        rotdst[2] = rotscale[8];
        rotdst[3] = rotscale[12];
    }

    const u8 spritewidth[16] =
    {
        8, 16, 8, 8,
        16, 32, 8, 8,
        32, 32, 16, 8,
        64, 64, 32, 8
    };
    const u8 spriteheight[16] =
    {
        8, 8, 16, 8,
        16, 8, 32, 8,
        32, 16, 32, 8,
        64, 32, 64, 8
    };

    for (int sprnum = 0; sprnum < 128; sprnum++)
    {
        u16* attrib = &oam[sprnum * 4];

        u32 sprtype = (attrib[0] >> 8) & 0x3;
        if (sprtype == 2) // sprite disabled
            continue;

        // note on sprite position:
        // X > 255 is interpreted as negative (-256..-1)
        // Y > 127 is interpreted as both positive (128..255) and negative (-128..-1)

        s32 xpos = (s32)(attrib[1] << 23) >> 23;
        s32 ypos = (s32)(attrib[0] << 24) >> 24;

        u32 sizeparam = (attrib[0] >> 14) | ((attrib[1] & 0xC000) >> 12);
        s32 width = spritewidth[sizeparam];
        s32 height = spriteheight[sizeparam];
        s32 boundwidth = width;
        s32 boundheight = height;

        if (sprtype == 3)
        {
            // double-size rotscale sprite
            boundwidth <<= 1;
            boundheight <<= 1;
        }

        if (xpos <= -boundwidth)
            continue;

        bool yc0 = ((ypos + boundheight) > ystart) && (ypos < yend);
        bool yc1 = (((ypos&0xFF) + boundheight) > ystart) && ((ypos&0xFF) < yend);
        if (!(yc0 || yc1))
            continue;

        u32 sprmode = (attrib[0] >> 10) & 0x3;
        if (sprmode == 3)
        {
            if ((GPU2D.DispCnt & 0x60) == 0x60)
                continue;
            if ((attrib[2] >> 12) == 0)
                continue;
        }

        if (NumSprites >= 128)
        {
            Log(LogLevel::Error, "GPU2D_Vulkan: SPRITE BUFFER IS FULL!!!!!\n");
            break;
        }

        // add this sprite to the OAM array

        auto& sprcfg = cfg.uOAM[NumSprites];

        sprcfg.Position[0] = (u32)xpos;
        sprcfg.Position[1] = (u32)ypos;
        sprcfg.Size[0] = width;
        sprcfg.Size[1] = height;
        sprcfg.BoundSize[0] = boundwidth;
        sprcfg.BoundSize[1] = boundheight;

        if (sprtype & 1)
        {
            sprcfg.Flip[0] = 0;
            sprcfg.Flip[1] = 0;
            sprcfg.Rotscale = (attrib[1] >> 9) & 0x1F;
        }
        else
        {
            sprcfg.Flip[0] = !!(attrib[1] & (1<<12));
            sprcfg.Flip[1] = !!(attrib[1] & (1<<13));
            sprcfg.Rotscale = (u32)-1;
        }

        sprcfg.OBJMode = sprmode;
        sprcfg.Mosaic = !!(attrib[0] & (1<<12)) && (sprmode != 2);
        sprcfg.BGPrio = (attrib[2] >> 10) & 0x3;

        u32 tilenum = attrib[2] & 0x3FF;

        if (sprmode == 3)
        {
            // bitmap sprite

            sprcfg.Type = 2;

            if (GPU2D.DispCnt & (1<<6))
            {
                // 1D mapping
                sprcfg.TileOffset = tilenum << (7 + ((GPU2D.DispCnt >> 22) & 0x1));
                sprcfg.TileStride = width * 2;
            }
            else
            {
                bool is256 = !!(GPU2D.DispCnt & (1<<5));
                int capblock = -1;

                u32 tileoffset, tilestride;
                if (is256)
                {
                    // 2D mapping, 256 pixels
                    tileoffset = ((tilenum & 0x01F) << 4) + ((tilenum & 0x3E0) << 7);
                    tilestride = 256 * 2;
                }
                else
                {
                    // 2D mapping, 128 pixels
                    tileoffset = ((tilenum & 0x00F) << 4) + ((tilenum & 0x3F0) << 7);
                    tilestride = 128 * 2;
                }

                // if this is a direct color bitmap, and the width is 128 or 256
                // then it might be a display capture
                u32 startaddr = tileoffset;
                u32 endaddr = startaddr + (height * tilestride);

                startaddr >>= 14;
                endaddr = (endaddr + 0x3FFF) >> 14;

                for (u32 b = startaddr; b < endaddr; b++)
                {
                    int blk = captureinfo[b & capturemask];
                    if (blk == -1) continue;

                    capblock = blk;
                }

                if (capblock != -1)
                {
                    if (!is256)
                    {
                        sprcfg.Type = 3;
                        tilestride = capblock;
                        tileoffset &= 0x7FFF;
                    }
                    else
                    {
                        sprcfg.Type = 4;
                        tilestride = capblock >> 2;
                        tileoffset &= 0x1FFFF;
                    }
                }

                sprcfg.TileOffset = tileoffset;
                sprcfg.TileStride = tilestride;
            }

            sprcfg.PalOffset = 1 + (attrib[2] >> 12); // alpha
        }
        else
        {
            if (GPU2D.DispCnt & (1<<4))
            {
                // 1D mapping
                sprcfg.TileOffset = tilenum << (5 + ((GPU2D.DispCnt >> 20) & 0x3));
                sprcfg.TileStride = (width >> 3) * 32;
                if (attrib[0] & (1<<13))
                    sprcfg.TileStride <<= 1;
            }
            else
            {
                // 2D mapping
                sprcfg.TileOffset = tilenum << 5;
                sprcfg.TileStride = 32 * 32;
            }

            if (attrib[0] & (1<<13))
            {
                // 256-color sprite
                sprcfg.Type = 1;
                if (GPU2D.DispCnt & (1<<31))
                    sprcfg.PalOffset = 1 + (attrib[2] >> 12);
                else
                    sprcfg.PalOffset = 0;
            }
            else
            {
                // 16-color sprite
                sprcfg.Type = 0;
                sprcfg.PalOffset = (attrib[2] >> 12) << 4;
            }
        }

        NumSprites++;

        if (sprcfg.Mosaic && (GPU2D.OBJMosaicSize[0] > 0))
            SpriteUseMosaic = true;
    }

    PushConfig(SpriteConfigRing, &cfg,
               offsetof(sSpriteConfig, uOAM) + (NumSprites * sizeof(cfg.uOAM[0])));
}

void VulkanRenderer2D::UpdateCompositorConfig()
{
    // compositor info buffer
    for (int i = 0; i < 4; i++)
        CompositorConfig.uBGPrio[i] = -1;

    for (int layer = 0; layer < 4; layer++)
    {
        if (!(LayerEnable & (1 << layer)))
            continue;

        int prio = BGCnt[layer] & 0x3;
        CompositorConfig.uBGPrio[layer] = prio;
    }

    CompositorConfig.uEnableOBJ = !!(LayerEnable & (1<<4));

    CompositorConfig.uEnable3D = !!(DispCnt & (1<<3));

    CompositorConfig.uBlendCnt = BlendCnt;
    CompositorConfig.uBlendEffect = (BlendCnt >> 6) & 0x3;
    CompositorConfig.uBlendCoef[0] = EVA;
    CompositorConfig.uBlendCoef[1] = EVB;
    CompositorConfig.uBlendCoef[2] = EVY;

    PushConfig(CompositorConfigRing, &CompositorConfig, sizeof(CompositorConfig));
}


void VulkanRenderer2D::PrerenderSprites()
{
    u16* vtxbuf = SpritePreVtxData;
    int vtxnum = 0;

    for (int i = 0; i < NumSprites; i++)
    {
        auto& sprite = SpriteConfig.uOAM[i];
        if (sprite.Type >= 3)
            continue;

        *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
        *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
        *vtxbuf++ = 1; *vtxbuf++ = 1; *vtxbuf++ = i;
        *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
        *vtxbuf++ = 0; *vtxbuf++ = 0; *vtxbuf++ = i;
        *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
        vtxnum += 6;
    }

    if (vtxnum == 0) return;

    VkDescriptorSet set = GetOBJPassDescriptorSet();
    if (set == VK_NULL_HANDLE) return;

    u32 vtxoffset = RingAlloc(VertexRing, vtxnum * 3 * sizeof(u16));
    if (vtxoffset == ~0u)
        return;
    memcpy(VertexRing.Host.data() + vtxoffset, SpritePreVtxData,
           vtxnum * 3 * sizeof(u16));

    BeginColorTarget(SpriteImg);
    BeginPass(RPColorLoad, SpriteFB, 1024, 512);
    SetViewportScissor(1024, 512, 0, 512);

    VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, SpritePrePipeline);
    BindDescriptorSet(set);
    PushConstants(0, 0);

    VkDeviceSize bindOffset = vtxoffset;
    VK::vkCmdBindVertexBuffers(CurCmd, 0, 1, &VertexRing.Buf.Buf, &bindOffset);
    VK::vkCmdDraw(CurCmd, vtxnum, 1, 0, 0);

    VK::vkCmdEndRenderPass(CurCmd);
    EndColorTarget(SpriteImg);
}

void VulkanRenderer2D::PrerenderLayer(int layer)
{
    auto& cfg = LayerConfig.uBGConfig[layer];

    if (cfg.Type >= 6)
        return;

    if (!BGLayerImg[layer] || (BGLayerFB[layer] == VK_NULL_HANDLE))
        return;

    VkDescriptorSet set = GetBGPassDescriptorSet();
    if (set == VK_NULL_HANDLE) return;

    BeginColorTarget(*BGLayerImg[layer]);
    BeginPass(RPColorClear, BGLayerFB[layer], cfg.Size[0], cfg.Size[1]);
    SetViewportScissor(cfg.Size[0], cfg.Size[1], 0, cfg.Size[1]);

    VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, LayerPrePipeline);
    BindDescriptorSet(set);
    PushConstants(layer, 0);

    VkDeviceSize bindOffset = 0;
    VK::vkCmdBindVertexBuffers(CurCmd, 0, 1, &RectVtxBuffer.Buf, &bindOffset);
    VK::vkCmdDraw(CurCmd, 2*3, 1, 0, 0);

    VK::vkCmdEndRenderPass(CurCmd);
    EndColorTarget(*BGLayerImg[layer]);
}


void VulkanRenderer2D::DoRenderSprites(int line)
{
    int ystart = LastSpriteLine;
    int yend = line;

    if (ystart >= yend)
        return;

    if (OBJLayerFB == VK_NULL_HANDLE)
        return;

    memcpy(SpriteScanlineConfigHost.data() + (ystart * sizeof(s32)),
           &SpriteScanlineConfig.uMosaicLine[ystart],
           (yend - ystart) * sizeof(s32));

    VkDescriptorSet set = GetOBJPassDescriptorSet();
    if (set == VK_NULL_HANDLE) return;

    BeginColorTarget(OBJLayerImg);
    DepthTargetBarrier();

    BeginPass(RPObjLoad, OBJLayerFB, ScreenW, ScreenH);
    SetViewportScissor(ScreenW, ScreenH, ystart * ScaleFactor, (yend - ystart) * ScaleFactor);

    // clear color/flags/depth for this scanline band only
    // (GL: scissored glClear)
    {
        VkClearAttachment clears[3] = {};
        clears[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clears[0].colorAttachment = 0;
        clears[1].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clears[1].colorAttachment = 1;
        clears[2].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clears[2].clearValue.depthStencil = {1.0f, 0};

        VkClearRect rect = {};
        rect.rect = {{0, (s32)(ystart * ScaleFactor)},
                     {(u32)ScreenW, (u32)((yend - ystart) * ScaleFactor)}};
        rect.baseArrayLayer = 0;
        rect.layerCount = 1;
        VK::vkCmdClearAttachments(CurCmd, 3, clears, 1, &rect);
    }

    BindDescriptorSet(set);

    // NOTE
    // this requires two passes for mosaic emulation, because mosaic flags get set for
    // transparent pixels too, and priority is only checked against opaque pixels

    if (SpriteUseMosaic)
    {
        VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, SpriteTransPipeline);
        PushConstants(0, 1);

        RenderSprites(false, ystart, yend);
    }

    VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, SpriteWindowPipeline);
    PushConstants(0, 0);

    RenderSprites(true, ystart, yend);

    VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, SpriteOpaquePipeline);

    RenderSprites(false, ystart, yend);

    VK::vkCmdEndRenderPass(CurCmd);
    EndColorTarget(OBJLayerImg);
}

void VulkanRenderer2D::RenderSprites(bool window, int ystart, int yend)
{
    if (window)
    {
        if (!(GPU2D.DispCnt & (1<<15)))
            return;
    }

    u16* vtxbuf = SpriteVtxData;
    int vtxnum = 0;

    for (int i = 0; i < NumSprites; i++)
    {
        auto& sprite = SpriteConfig.uOAM[i];

        bool iswin = (sprite.OBJMode == 2);
        if (iswin != window)
            continue;

        s32 xpos = sprite.Position[0];
        s32 ypos = sprite.Position[1];
        s32 boundwidth = sprite.BoundSize[0];
        s32 boundheight = sprite.BoundSize[1];

        bool yc0 = ((ypos + boundheight) > ystart) && (ypos < yend);
        bool yc1 = (((ypos&0xFF) + boundheight) > ystart) && ((ypos&0xFF) < yend);

        if (yc0)
        {
            s32 x0 = xpos, x1 = xpos + boundwidth;
            s32 y0 = ypos, y1 = ypos + boundheight;

            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y1; *vtxbuf++ = 1; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y0; *vtxbuf++ = 0; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            vtxnum += 6;
        }

        if (yc1)
        {
            ypos &= 0xFF;
            s32 x0 = xpos, x1 = xpos + boundwidth;
            s32 y0 = ypos, y1 = ypos + boundheight;

            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y1; *vtxbuf++ = 1; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y1; *vtxbuf++ = 0; *vtxbuf++ = 1; *vtxbuf++ = i;
            *vtxbuf++ = x0; *vtxbuf++ = y0; *vtxbuf++ = 0; *vtxbuf++ = 0; *vtxbuf++ = i;
            *vtxbuf++ = x1; *vtxbuf++ = y0; *vtxbuf++ = 1; *vtxbuf++ = 0; *vtxbuf++ = i;
            vtxnum += 6;
        }
    }

    if (vtxnum == 0) return;

    u32 vtxoffset = RingAlloc(VertexRing, vtxnum * 5 * sizeof(u16));
    if (vtxoffset == ~0u)
        return;
    memcpy(VertexRing.Host.data() + vtxoffset, SpriteVtxData,
           vtxnum * 5 * sizeof(u16));

    VkDeviceSize bindOffset = vtxoffset;
    VK::vkCmdBindVertexBuffers(CurCmd, 0, 1, &VertexRing.Buf.Buf, &bindOffset);
    VK::vkCmdDraw(CurCmd, vtxnum, 1, 0, 0);
}

void VulkanRenderer2D::RenderScreen(int ystart, int yend)
{
    if (ystart >= yend)
        return;

    if (OutputFB == VK_NULL_HANDLE)
        return;

    if (ForcedBlank || !UnitEnabled)
    {
        float lum;
        if (!UnitEnabled)
            lum = GPU2D.Num ? 1.0f : 0.0f;
        else
            lum = 1.0f;

        BeginColorTarget(OutputImg);
        BeginPass(RPColorLoad, OutputFB, ScreenW, ScreenH);

        VkClearAttachment clear = {};
        clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear.colorAttachment = 0;
        clear.clearValue.color = {{lum, lum, lum, 1.0f}};

        VkClearRect rect = {};
        rect.rect = {{0, (s32)(ystart * ScaleFactor)},
                     {(u32)ScreenW, (u32)((yend - ystart) * ScaleFactor)}};
        rect.baseArrayLayer = 0;
        rect.layerCount = 1;
        VK::vkCmdClearAttachments(CurCmd, 1, &clear, 1, &rect);

        VK::vkCmdEndRenderPass(CurCmd);
        EndColorTarget(OutputImg);
        return;
    }

    // Save the per-scanline config for this band. It is published only after
    // the prior frame's fence completes.
    memcpy(ScanlineConfigHost.data() + (ystart * sizeof(sScanlineConfig::sScanline)),
           &ScanlineConfig.uScanline[ystart],
           (yend - ystart) * sizeof(sScanlineConfig::sScanline));

    UpdateCompositorConfig();

    // BG layer inputs: the 3D output replaces the BG0 slot when DISPCNT
    // bit3 is set; the per-layer wrap mode (GL: CLAMP_TO_BORDER vs REPEAT
    // texparam mutation) selects the sampler baked into the descriptor set
    VkImageView bgViews[4];
    VkSampler bgSamplers[4];
    for (int i = 0; i < 4; i++)
    {
        if ((i == 0) && (DispCnt & (1<<3)))
            bgViews[i] = Shared.OutputTex3D ? Shared.OutputTex3D : DummyTex.View;
        else
            bgViews[i] = BGLayerImg[i] ? BGLayerImg[i]->View : DummyTex.View;

        bgSamplers[i] = LayerConfig.uBGConfig[i].Clamp ? SamplerNearestBorder : SamplerNearestRepeat;
    }

    VkDescriptorSet set = GetDescriptorSet(VRAMTexBG.View, PalViewBG, bgViews, bgSamplers);
    if (set == VK_NULL_HANDLE) return;

    BeginColorTarget(OutputImg);
    BeginPass(RPColorLoad, OutputFB, ScreenW, ScreenH);
    SetViewportScissor(ScreenW, ScreenH, ystart * ScaleFactor, (yend - ystart) * ScaleFactor);

    VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, CompositorPipeline);
    BindDescriptorSet(set);
    PushConstants(0, 0);

    VkDeviceSize bindOffset = 0;
    VK::vkCmdBindVertexBuffers(CurCmd, 0, 1, &RectVtxBuffer.Buf, &bindOffset);
    VK::vkCmdDraw(CurCmd, 2*3, 1, 0, 0);

    VK::vkCmdEndRenderPass(CurCmd);
    EndColorTarget(OutputImg);
}

void VulkanRenderer2D::DrawSprites(u32 line)
{
    if (CurCmd == VK_NULL_HANDLE)
        return;

    if (UseSoftware2D)
    {
        SoftFallback->DrawSprites(line);
        return;
    }

    u32 oammask = 1 << GPU2D.Num;
    bool dirty = false;
    bool stagingUploadFailed = false;
    bool screenon = IsScreenOn();

    SpriteScanlineConfig.uMosaicLine[line] = GPU2D.OBJMosaicLine;

    u32 dispcnt_diff = GPU2D.DispCnt ^ SpriteDispCnt;
    SpriteDispCnt = GPU2D.DispCnt; // TODO CHECKME might not be right to do it here
    if (dispcnt_diff & 0x80F000F0)
        dirty = true;

    static_assert(VRAMDirtyGranularity == 512);
    NonStupidBitField<512> objDirty;

    if (screenon)
    {
        if (GPU2D.Num == 0)
        {
            objDirty = GPU.VRAMDirty_AOBJ.DeriveState(GPU.VRAMMap_AOBJ, GPU);
            GPU.MakeVRAMFlat_AOBJCoherent(objDirty);
        }
        else
        {
            auto _objDirty = GPU.VRAMDirty_BOBJ.DeriveState(GPU.VRAMMap_BOBJ, GPU);
            GPU.MakeVRAMFlat_BOBJCoherent(_objDirty);
            memcpy(objDirty.Data, _objDirty.Data, 256>>3);
        }
    }

    u8* vram; u32 vrammask;
    GPU2D.GetOBJVRAM(vram, vrammask);

    bool uploading = false;

    int dirtybits = GPU2D.Num ? 256 : 512;
    for (int i = 0; i < dirtybits; )
    {
        if (!objDirty[i])
        {
            i++;
            continue;
        }

        if (!uploading)
        {
            BeginTexUpload(VRAMTexOBJ);
            uploading = true;
        }

        if (i & 1)
        {
            if (!UploadTexRectR8(VRAMTexOBJ, &vram[i * 512],
                                 512, i >> 1, 512, 1))
                stagingUploadFailed = true;
            i++;
            dirty = true;
            continue;
        }

        int start = i;
        while ((i + 1) < dirtybits && objDirty[i] && objDirty[i + 1])
            i += 2;

        if (i > start)
        {
            if (!UploadTexRectR8(VRAMTexOBJ, &vram[start * 512],
                                 0, start >> 1, 1024, (i - start) >> 1))
                stagingUploadFailed = true;
        }
        else
        {
            if (!UploadTexRectR8(VRAMTexOBJ, &vram[i * 512],
                                 0, i >> 1, 512, 1))
                stagingUploadFailed = true;
            i++;
        }
        dirty = true;
    }

    if (uploading)
        EndTexUpload(VRAMTexOBJ);

    if ((GPU.OAMDirty & oammask) || SpriteConfigDirty)
    {
        memcpy(OAM, &GPU.OAM[GPU2D.Num ? 0x400 : 0], 0x400);
        GPU.OAMDirty &= ~oammask;
        SpriteConfigDirty = false;
        dirty = true;
    }

    // DrawScanline() for the next scanline will be called after this
    // so it will be able to do the actual sprite rendering
    if (dirty)
        SpriteDirty = true;
    if (stagingUploadFailed)
        RetryStagingUploads();
}

}
