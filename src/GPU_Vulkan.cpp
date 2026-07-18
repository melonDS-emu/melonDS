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

#include <string.h>
#include <algorithm>

#include "NDS.h"
#include "GPU_Vulkan.h"
#include "GPU_Vulkan_shaders.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


VulkanRenderer::VulkanRenderer(melonDS::NDS& nds)
    : Renderer(nds.GPU)
{
    AuxInputBuffer[0] = new u16[256 * 256];
    AuxInputBuffer[1] = new u16[256 * 192];
    memset(AuxInputBuffer[0], 0, 256 * 256 * sizeof(u16));
    memset(AuxInputBuffer[1], 0, 256 * 192 * sizeof(u16));
    memset(AuxInputDirty, 1, sizeof(AuxInputDirty));

    // the 3D renderer owns the shared VK::Context; it must be constructed
    // (and its context initialised, in Init()) before the 2D units, which
    // borrow the same context/queue via a reference. SetVulkanNativeOutput
    // must be set before SetRenderSettings() ever runs (GPU3D_ComputeVulkan.h);
    // nullptr parent is safe because that path is only reached when
    // VulkanNativeOutput is false, which it never is here.
    auto rend3d = std::make_unique<ComputeRenderer3D_Vulkan>(GPU.GPU3D, nullptr);
    rend3d->SetVulkanNativeOutput(true);
    Ctx = &rend3d->GetContext();
    Rend3D = std::move(rend3d);

    Rend2D_A = std::make_unique<VulkanRenderer2D>(GPU.GPU2D_A, *this, *Ctx);
    Rend2D_B = std::make_unique<VulkanRenderer2D>(GPU.GPU2D_B, *this, *Ctx);

    ScaleFactor = 0;
}

bool VulkanRenderer::Init()
{
    // NOTE: deviates from GLRenderer::Init()'s ordering (parent's own
    // resources, then 2D units, then 3D last). Rend3D->Init() is what
    // actually creates the VkDevice (Ctx->Init()), so it must run first;
    // everything else here and the 2D units' Init() depend on a valid Ctx.
    if (!Rend3D->Init())
        return false;
    if (!Ctx->Valid)
        return false;

    // ---- compile the parent's own shaders ----

    std::string fpVS = "#version 460\n" + GPUShadersVulkan::FinalPassVS;
    std::string fpFS = "#version 460\n" + GPUShadersVulkan::FinalPassFS;
    std::string capVS = "#version 460\n" + GPUShadersVulkan::CaptureVS;
    std::string capFS = "#version 460\n" + GPUShadersVulkan::CaptureFS;
    std::string capDownVS = "#version 460\n" + GPUShadersVulkan::CaptureDownscaleVS;
    std::string capDownFS = "#version 460\n" + GPUShadersVulkan::CaptureDownscaleFS;

    VkShaderModule fpVSMod = VK_NULL_HANDLE, fpFSMod = VK_NULL_HANDLE;
    VkShaderModule capVSMod = VK_NULL_HANDLE, capFSMod = VK_NULL_HANDLE;
    VkShaderModule capDownVSMod = VK_NULL_HANDLE, capDownFSMod = VK_NULL_HANDLE;

    bool ok = true;
    ok = ok && Ctx->CompileShader(fpVSMod, VK::Context::ShaderStage::Vertex, fpVS, "FinalPassVS");
    ok = ok && Ctx->CompileShader(fpFSMod, VK::Context::ShaderStage::Fragment, fpFS, "FinalPassFS");
    ok = ok && Ctx->CompileShader(capVSMod, VK::Context::ShaderStage::Vertex, capVS, "CaptureVS");
    ok = ok && Ctx->CompileShader(capFSMod, VK::Context::ShaderStage::Fragment, capFS, "CaptureFS");
    ok = ok && Ctx->CompileShader(capDownVSMod, VK::Context::ShaderStage::Vertex, capDownVS, "CaptureDownscaleVS");
    ok = ok && Ctx->CompileShader(capDownFSMod, VK::Context::ShaderStage::Fragment, capDownFS, "CaptureDownscaleFS");

    if (!ok)
    {
        VkShaderModule mods[6] = {fpVSMod, fpFSMod, capVSMod, capFSMod, capDownVSMod, capDownFSMod};
        for (VkShaderModule m : mods)
            if (m) VK::vkDestroyShaderModule(Ctx->Device, m, nullptr);
        return false;
    }

    // ---- FinalPass: descriptor set layout, pipeline layout, render pass, pipeline ----
    {
        VkDescriptorSetLayoutBinding bindings[4] = {};
        auto setBinding = [&](u32 idx, VkDescriptorType type, VkShaderStageFlags stages)
        {
            bindings[idx].binding = idx;
            bindings[idx].descriptorType = type;
            bindings[idx].descriptorCount = 1;
            bindings[idx].stageFlags = stages;
        };
        setBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        setBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        setBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        setBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 4;
        layoutInfo.pBindings = bindings;
        if (VK::vkCreateDescriptorSetLayout(Ctx->Device, &layoutInfo, nullptr, &FPSetLayout) != VK_SUCCESS)
            return false;

        VkPipelineLayoutCreateInfo plInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plInfo.setLayoutCount = 1;
        plInfo.pSetLayouts = &FPSetLayout;
        if (VK::vkCreatePipelineLayout(Ctx->Device, &plInfo, nullptr, &FPPipelineLayout) != VK_SUCCESS)
            return false;
    }

    if (!Ctx->CreateRenderPass(FPRenderPass, VK_FORMAT_R8G8B8A8_UNORM, false, 2, false))
        return false;

    {
        VK::Context::GraphicsPipelineConfig cfg = {};
        cfg.VertexShader = fpVSMod;
        cfg.FragmentShader = fpFSMod;
        cfg.Layout = FPPipelineLayout;
        cfg.RenderPass = FPRenderPass;
        cfg.VertexStride = 2 * sizeof(float);
        cfg.VertexAttributes = { {0, 0, VK_FORMAT_R32G32_SFLOAT, 0} };
        cfg.ColorAttachmentCount = 2;
        if (!Ctx->CreateGraphicsPipeline(FPPipeline, cfg))
            return false;
    }

    // ---- Capture: descriptor set layout, pipeline layout, render pass, pipeline ----
    {
        VkDescriptorSetLayoutBinding bindings[3] = {};
        auto setBinding = [&](u32 idx, VkDescriptorType type, VkShaderStageFlags stages)
        {
            bindings[idx].binding = idx;
            bindings[idx].descriptorType = type;
            bindings[idx].descriptorCount = 1;
            bindings[idx].stageFlags = stages;
        };
        setBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        setBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        setBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 3;
        layoutInfo.pBindings = bindings;
        if (VK::vkCreateDescriptorSetLayout(Ctx->Device, &layoutInfo, nullptr, &CaptureSetLayout) != VK_SUCCESS)
            return false;

        VkPipelineLayoutCreateInfo plInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plInfo.setLayoutCount = 1;
        plInfo.pSetLayouts = &CaptureSetLayout;
        if (VK::vkCreatePipelineLayout(Ctx->Device, &plInfo, nullptr, &CapturePipelineLayout) != VK_SUCCESS)
            return false;
    }

    if (!Ctx->CreateRenderPass(CaptureRenderPass, VK_FORMAT_R8G8B8A8_UNORM, false, 1, false))
        return false;

    {
        VK::Context::GraphicsPipelineConfig cfg = {};
        cfg.VertexShader = capVSMod;
        cfg.FragmentShader = capFSMod;
        cfg.Layout = CapturePipelineLayout;
        cfg.RenderPass = CaptureRenderPass;
        cfg.VertexStride = 4 * sizeof(s16);
        cfg.VertexAttributes = {
            {0, 0, VK_FORMAT_R16G16_SINT, 0},
            {1, 0, VK_FORMAT_R16G16_SINT, 2 * (u32)sizeof(s16)},
        };
        cfg.ColorAttachmentCount = 1;
        if (!Ctx->CreateGraphicsPipeline(CapturePipeline, cfg))
            return false;
    }

    // ---- CaptureDownscale: descriptor set layout, pipeline layout, render pass, pipeline ----
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        if (VK::vkCreateDescriptorSetLayout(Ctx->Device, &layoutInfo, nullptr, &CapDownSetLayout) != VK_SUCCESS)
            return false;

        VkPushConstantRange pushRange = {};
        pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(s32);

        VkPipelineLayoutCreateInfo plInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plInfo.setLayoutCount = 1;
        plInfo.pSetLayouts = &CapDownSetLayout;
        plInfo.pushConstantRangeCount = 1;
        plInfo.pPushConstantRanges = &pushRange;
        if (VK::vkCreatePipelineLayout(Ctx->Device, &plInfo, nullptr, &CapDownPipelineLayout) != VK_SUCCESS)
            return false;
    }

    // CaptureSync is a self-contained one-shot target each time it's used
    // (see SyncVRAMCapture), so a clearing render pass is always safely
    // reenterable regardless of the image's actual previous layout
    if (!Ctx->CreateRenderPass(CapDownRenderPass, VK_FORMAT_R8G8B8A8_UNORM, true, 1, false))
        return false;

    {
        VK::Context::GraphicsPipelineConfig cfg = {};
        cfg.VertexShader = capDownVSMod;
        cfg.FragmentShader = capDownFSMod;
        cfg.Layout = CapDownPipelineLayout;
        cfg.RenderPass = CapDownRenderPass;
        cfg.VertexStride = 2 * sizeof(float);
        cfg.VertexAttributes = { {0, 0, VK_FORMAT_R32G32_SFLOAT, 0} };
        cfg.ColorAttachmentCount = 1;
        if (!Ctx->CreateGraphicsPipeline(CapDownPipeline, cfg))
            return false;
    }

    VkShaderModule allMods[6] = {fpVSMod, fpFSMod, capVSMod, capFSMod, capDownVSMod, capDownFSMod};
    for (VkShaderModule m : allMods)
        if (m) VK::vkDestroyShaderModule(Ctx->Device, m, nullptr);

    // ---- samplers ----

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
        return VK::vkCreateSampler(Ctx->Device, &samplerInfo, nullptr, &out) == VK_SUCCESS;
    };
    if (!createSampler(SamplerNearestClamp, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)) return false;
    if (!createSampler(SamplerNearestRepeat, VK_SAMPLER_ADDRESS_MODE_REPEAT)) return false;
    if (!createSampler(SamplerNearestBorder, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)) return false;

    // ---- vertex buffers ----

    // FinalPass fullscreen quad: position only, NDC (-1..1) directly
    // (GL: FPVertexArrayID; FinalPassVS derives fTexcoord from vPosition)
    {
        const float verts[6][2] = {
            {-1, 1}, {1, -1}, {1, 1},
            {-1, 1}, {-1, -1}, {1, -1},
        };
        if (!Ctx->CreateBuffer(FPVertexBuffer, sizeof(verts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true))
            return false;
        memcpy(FPVertexBuffer.Map, verts, sizeof(verts));
    }

    // shared fullscreen unit rect [0,1] (GL: RectVtxBuffer), used by CaptureDownscale
    {
        const float rectverts[6][2] = {
            {0, 1}, {1, 0}, {1, 1},
            {0, 1}, {0, 0}, {1, 0},
        };
        if (!Ctx->CreateBuffer(RectVtxBuffer, sizeof(rectverts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true))
            return false;
        memcpy(RectVtxBuffer.Map, rectverts, sizeof(rectverts));
    }

    if (!Ctx->CreateBuffer(CaptureVertexRing.Buf, 256 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true))
        return false;
    CaptureVertexRing.Host.resize(CaptureVertexRing.Buf.Size);
    if (!Ctx->CreateBuffer(AuxStagingRing.Buf, 512 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
        return false;
    AuxStagingRing.Host.resize(AuxStagingRing.Buf.Size);

    // ---- config rings ----

    static_assert((sizeof(sFinalPassConfig) & 15) == 0);
    static_assert((sizeof(sCaptureConfig) & 15) == 0);

    if (!InitConfigRing(FPConfigRing, sizeof(sFinalPassConfig), 256))
        return false;
    if (!InitConfigRing(CaptureConfigRing, sizeof(sCaptureConfig), 256))
        return false;

    // ---- AuxInput texture (VRAM-display / mainmem DISP FIFO); fixed size ----

    if (!Ctx->CreateImage(AuxInputImg, VK_FORMAT_A1R5G5B5_UNORM_PACK16, 256, 256, 2,
                          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true))
        return false;
    {
        // r<->b swizzle recovers the DS channel order for this raw-uploaded
        // packed format (see VulkanRenderer2D::CreatePalView for the same trick)
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = AuxInputImg.Img;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.format = AuxInputImg.Format;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G,
                               VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_A};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2};
        if (VK::vkCreateImageView(Ctx->Device, &viewInfo, nullptr, &AuxInputView) != VK_SUCCESS)
            return false;
    }
    {
        VkCommandBuffer cmd = Ctx->BeginOneShot();
        Ctx->TransitionImage(cmd, AuxInputImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        Ctx->EndOneShot(cmd);
    }

    // ---- CaptureSync (1x IR capture readback target); fixed size ----

    if (!Ctx->CreateImage(CaptureSyncImg, VK_FORMAT_R8G8B8A8_UNORM, 256, 256, 1,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false))
        return false;
    if (!Ctx->CreateFramebuffer(CaptureSyncFB, CapDownRenderPass, CaptureSyncImg, 0))
        return false;
    if (!Ctx->CreateBuffer(CaptureSyncReadback, 256 * 256 * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true))
        return false;

    // ---- descriptor pools ----

    {
        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6},
        };
        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 2;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = sizes;
        if (VK::vkCreateDescriptorPool(Ctx->Device, &poolInfo, nullptr, &FPDescPool) != VK_SUCCESS)
            return false;

        VkDescriptorSetLayout layouts[2] = {FPSetLayout, FPSetLayout};
        VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = FPDescPool;
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = layouts;
        if (VK::vkAllocateDescriptorSets(Ctx->Device, &allocInfo, FPDescSet) != VK_SUCCESS)
            return false;
    }

    {
        const u32 maxSets = 32;
        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * maxSets},
        };
        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = maxSets;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = sizes;
        if (VK::vkCreateDescriptorPool(Ctx->Device, &poolInfo, nullptr, &CaptureDescPool) != VK_SUCCESS)
            return false;
    }

    {
        VkDescriptorPoolSize sizes[] = { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2} };
        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 2;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = sizes;
        if (VK::vkCreateDescriptorPool(Ctx->Device, &poolInfo, nullptr, &CapDownDescPool) != VK_SUCCESS)
            return false;

        VkDescriptorSetLayout layouts[2] = {CapDownSetLayout, CapDownSetLayout};
        VkDescriptorSet sets[2] = {};
        VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = CapDownDescPool;
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = layouts;
        if (VK::vkAllocateDescriptorSets(Ctx->Device, &allocInfo, sets) != VK_SUCCESS)
            return false;
        CapDown128Set = sets[0];
        CapDown256Set = sets[1];
    }

    // ---- command buffer + fence (own submission, separate from Rend3D's) ----

    {
        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = Ctx->CmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 2;
        if (VK::vkAllocateCommandBuffers(Ctx->Device, &allocInfo, FrameCmd) != VK_SUCCESS)
            return false;

        VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        for (int i = 0; i < 2; i++)
            if (VK::vkCreateFence(Ctx->Device, &fenceInfo, nullptr, &FrameFence[i]) != VK_SUCCESS)
                return false;
    }

    // ---- GL interop presentation textures (mirrors GLRenderer::FPOutputTex) ----

    glGenTextures(2, FPOutputTex);
    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, FPOutputTex[i]);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // ---- 2D units ----

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    if (!rend2dA->InitShaders()) return false;
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    if (!rend2dB->InitShaders(*rend2dA)) return false;

    if (!Rend2D_A->Init()) return false;
    if (!Rend2D_B->Init()) return false;

    return true;
}

VulkanRenderer::~VulkanRenderer()
{
    if (Ctx && Ctx->Valid)
        VK::vkDeviceWaitIdle(Ctx->Device);

    // Rend3D owns Ctx, while both 2D renderers borrow it. The base class
    // declares Rend3D after the 2D renderers, so its implicit destruction
    // order would invalidate their context references. Destroy the borrower
    // that shares A's resources first, followed by their owner.
    Rend2D_B.reset();
    Rend2D_A.reset();

    if (Ctx && Ctx->Valid)
    {
        DestroyScaleDependentResources();

        for (int i = 0; i < 2; i++)
            if (FrameFence[i]) VK::vkDestroyFence(Ctx->Device, FrameFence[i], nullptr);
        if (FrameCmd[0]) VK::vkFreeCommandBuffers(Ctx->Device, Ctx->CmdPool, 2, FrameCmd);

        if (FPDescPool) VK::vkDestroyDescriptorPool(Ctx->Device, FPDescPool, nullptr);
        if (CaptureDescPool) VK::vkDestroyDescriptorPool(Ctx->Device, CaptureDescPool, nullptr);
        if (CapDownDescPool) VK::vkDestroyDescriptorPool(Ctx->Device, CapDownDescPool, nullptr);

        if (FPPipeline) VK::vkDestroyPipeline(Ctx->Device, FPPipeline, nullptr);
        if (CapturePipeline) VK::vkDestroyPipeline(Ctx->Device, CapturePipeline, nullptr);
        if (CapDownPipeline) VK::vkDestroyPipeline(Ctx->Device, CapDownPipeline, nullptr);

        if (FPRenderPass) VK::vkDestroyRenderPass(Ctx->Device, FPRenderPass, nullptr);
        if (CaptureRenderPass) VK::vkDestroyRenderPass(Ctx->Device, CaptureRenderPass, nullptr);
        if (CapDownRenderPass) VK::vkDestroyRenderPass(Ctx->Device, CapDownRenderPass, nullptr);

        if (FPPipelineLayout) VK::vkDestroyPipelineLayout(Ctx->Device, FPPipelineLayout, nullptr);
        if (CapturePipelineLayout) VK::vkDestroyPipelineLayout(Ctx->Device, CapturePipelineLayout, nullptr);
        if (CapDownPipelineLayout) VK::vkDestroyPipelineLayout(Ctx->Device, CapDownPipelineLayout, nullptr);

        if (FPSetLayout) VK::vkDestroyDescriptorSetLayout(Ctx->Device, FPSetLayout, nullptr);
        if (CaptureSetLayout) VK::vkDestroyDescriptorSetLayout(Ctx->Device, CaptureSetLayout, nullptr);
        if (CapDownSetLayout) VK::vkDestroyDescriptorSetLayout(Ctx->Device, CapDownSetLayout, nullptr);

        if (SamplerNearestClamp) VK::vkDestroySampler(Ctx->Device, SamplerNearestClamp, nullptr);
        if (SamplerNearestRepeat) VK::vkDestroySampler(Ctx->Device, SamplerNearestRepeat, nullptr);
        if (SamplerNearestBorder) VK::vkDestroySampler(Ctx->Device, SamplerNearestBorder, nullptr);

        if (AuxInputView) VK::vkDestroyImageView(Ctx->Device, AuxInputView, nullptr);
        Ctx->DestroyImage(AuxInputImg);

        if (CaptureSyncFB) VK::vkDestroyFramebuffer(Ctx->Device, CaptureSyncFB, nullptr);
        Ctx->DestroyImage(CaptureSyncImg);
        Ctx->DestroyBuffer(CaptureSyncReadback);

        Ctx->DestroyBuffer(FPVertexBuffer);
        Ctx->DestroyBuffer(RectVtxBuffer);
        Ctx->DestroyBuffer(CaptureVertexRing.Buf);
        Ctx->DestroyBuffer(AuxStagingRing.Buf);
        Ctx->DestroyBuffer(FPConfigRing.Buf);
        Ctx->DestroyBuffer(CaptureConfigRing.Buf);
    }

    for (GLuint tex : FPOutputTex)
        if (tex) glDeleteTextures(1, &tex);

    delete[] AuxInputBuffer[0];
    delete[] AuxInputBuffer[1];

    Rend3D.reset();
    Ctx = nullptr;
}

void VulkanRenderer::DestroyScaleDependentResources()
{
    if (FPFramebuffer) { VK::vkDestroyFramebuffer(Ctx->Device, FPFramebuffer, nullptr); FPFramebuffer = VK_NULL_HANDLE; }
    for (int i = 0; i < 2; i++)
    {
        if (FPOutputView[i]) { VK::vkDestroyImageView(Ctx->Device, FPOutputView[i], nullptr); FPOutputView[i] = VK_NULL_HANDLE; }
    }
    Ctx->DestroyImage(FPOutputImg);
    Ctx->DestroyBuffer(FPReadbackBuffer[0]);
    Ctx->DestroyBuffer(FPReadbackBuffer[1]);

    for (int i = 0; i < 4; i++)
    {
        if (CaptureOutput256FB[i]) { VK::vkDestroyFramebuffer(Ctx->Device, CaptureOutput256FB[i], nullptr); CaptureOutput256FB[i] = VK_NULL_HANDLE; }
    }
    for (int i = 0; i < 16; i++)
    {
        if (CaptureOutput128FB[i]) { VK::vkDestroyFramebuffer(Ctx->Device, CaptureOutput128FB[i], nullptr); CaptureOutput128FB[i] = VK_NULL_HANDLE; }
    }
    Ctx->DestroyImage(CaptureOutput256Img);
    Ctx->DestroyImage(CaptureOutput128Img);
    Ctx->DestroyImage(CaptureVRAMImg);
}

void VulkanRenderer::Reset()
{
    memset(&FinalPassConfig, 0, sizeof(FinalPassConfig));
    memset(&CaptureConfig, 0, sizeof(CaptureConfig));

    AuxUsageMask = 0;
    memset(AuxInputBuffer[0], 0, 256 * 256 * sizeof(u16));
    memset(AuxInputBuffer[1], 0, 256 * 192 * sizeof(u16));
    memset(AuxInputDirty, 1, sizeof(AuxInputDirty));
    FrameReady = false;
    FrameDirty = false;

    DispCntA = 0;
    DispCntB = 0;
    MasterBrightnessA = 0;
    MasterBrightnessB = 0;
    CaptureCnt = 0;

    NeedPartialRender = false;
    LastLine = 0;
    LastCapLine = 0;
    Aux0VRAMCap = -1;

    // drain any pipelined-but-unwaited frames before discarding state
    for (int i = 0; i < 2; i++)
    {
        if (SlotPending[i])
        {
            VK::vkWaitForFences(Ctx->Device, 1, &FrameFence[i], VK_TRUE, UINT64_MAX);
            VK::vkResetFences(Ctx->Device, 1, &FrameFence[i]);
            SlotPending[i] = false;
        }
    }
    HavePrevFrame = false;
    FrameSlot = 0;

    if (FrameStarted)
    {
        // nothing has been submitted yet; safe to discard
        VK::vkResetCommandBuffer(FrameCmd[FrameSlot], 0);
        FrameStarted = false;
        CurCmd = VK_NULL_HANDLE;
    }

    Rend2D_A->Reset();
    Rend2D_B->Reset();
    Rend3D->Reset();
}

void VulkanRenderer::Stop()
{
    // TODO clear buffers
    // TODO: do we even need this anymore?
}

void VulkanRenderer::PostSavestate()
{
    Reset();

    auto* rend2D = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    rend2D->PostSavestate();
    rend2D = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2D->PostSavestate();
}


void VulkanRenderer::SetRenderSettings(RendererSettings& settings)
{
    // NOTE: order deviates from GLRenderer::SetRenderSettings (which
    // resizes the parent first, then the 2D units, then the 3D renderer
    // last). Here, both 2D units' SetSharedResources() needs Rend3D's
    // freshly-resized output view, and this renderer's own SetScaleFactor()
    // needs both 2D units' freshly-resized output views to (re)build the
    // FinalPass/Capture descriptor sets -- so 3D must resize first, then
    // the 2D units, then this renderer last.
    EnableDither = settings.Dither;
    EnableTexFilter = settings.TexFilter;

    auto* rend3d = dynamic_cast<ComputeRenderer3D_Vulkan*>(Rend3D.get());
    rend3d->SetTextureFilter(settings.TexFilter);
    rend3d->SetRenderSettings(settings.ScaleFactor, settings.HiresCoordinates);

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    rend2dA->SetScaleFactor(settings.ScaleFactor);
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2dB->SetScaleFactor(settings.ScaleFactor);

    SetScaleFactor(settings.ScaleFactor);
}

void VulkanRenderer::SetScaleFactor(int scale)
{
    if (scale == ScaleFactor)
        return;

    VK::vkDeviceWaitIdle(Ctx->Device);

    // deferred frames' fences are now signalled but unwaited; drop them so the
    // recreated pipeline starts clean
    for (int i = 0; i < 2; i++)
    {
        if (SlotPending[i])
        {
            VK::vkResetFences(Ctx->Device, 1, &FrameFence[i]);
            SlotPending[i] = false;
        }
    }
    FrameStarted = false;
    FrameReady = false;
    FrameDirty = true;
    HavePrevFrame = false;
    FrameSlot = 0;

    DestroyScaleDependentResources();

    ScaleFactor = scale;
    ScreenW = 256 * scale;
    ScreenH = 192 * scale;

    // ---- FinalPass MRT output: layer0=top, layer1=bottom ----

    Ctx->CreateImage(FPOutputImg, VK_FORMAT_R8G8B8A8_UNORM, ScreenW, ScreenH, 2,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, true);
    Ctx->CreateLayerView(FPOutputView[0], FPOutputImg, 0);
    Ctx->CreateLayerView(FPOutputView[1], FPOutputImg, 1);
    Ctx->CreateFramebufferMulti(FPFramebuffer, FPRenderPass, {FPOutputView[0], FPOutputView[1]},
                                (u32)ScreenW, (u32)ScreenH);

    for (int i = 0; i < 2; i++)
        if (!Ctx->CreateBuffer(FPReadbackBuffer[i], (VkDeviceSize)ScreenW * ScreenH * 2 * 4,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT, true))
            Log(LogLevel::Error, "GPU_Vulkan: failed to create FinalPass readback buffer\n");
    HavePrevFrame = false;

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, FPOutputTex[i]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, ScreenW, ScreenH, 2, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    // ---- Capture destination images ----

    Ctx->CreateImage(CaptureOutput256Img, VK_FORMAT_R8G8B8A8_UNORM, 256 * scale, 256 * scale, 4,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT, true);
    for (int i = 0; i < 4; i++)
        Ctx->CreateFramebuffer(CaptureOutput256FB[i], CaptureRenderPass, CaptureOutput256Img, i);

    Ctx->CreateImage(CaptureOutput128Img, VK_FORMAT_R8G8B8A8_UNORM, 128 * scale, 128 * scale, 16,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, true);
    for (int i = 0; i < 16; i++)
        Ctx->CreateFramebuffer(CaptureOutput128FB[i], CaptureRenderPass, CaptureOutput128Img, i);

    Ctx->CreateImage(CaptureVRAMImg, VK_FORMAT_R8G8B8A8_UNORM, 256 * scale, 256 * scale, 1,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);

    // ---- initial layouts ----
    {
        VkCommandBuffer cmd = Ctx->BeginOneShot();

        Ctx->TransitionImage(cmd, FPOutputImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

        Ctx->TransitionImage(cmd, CaptureOutput256Img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        Ctx->TransitionImage(cmd, CaptureOutput128Img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        Ctx->TransitionImage(cmd, CaptureVRAMImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

        Ctx->EndOneShot(cmd);
    }

    // ---- push the freshly (re)created views to the 2D units, and refresh
    // this renderer's own descriptor sets that reference them ----

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    auto* rend3dVk = dynamic_cast<ComputeRenderer3D_Vulkan*>(Rend3D.get());

    VulkanRenderer2D::SharedResources shared;
    shared.OutputTex3D = rend3dVk->GetOutputImage().View;
    shared.Capture128 = CaptureOutput128Img.View;
    shared.Capture256 = CaptureOutput256Img.View;
    rend2dA->SetSharedResources(shared);
    rend2dB->SetSharedResources(shared);

    // also feed the capture output to the 3D rasteriser, so polygons that
    // use a display capture as their texture sample the real thing (closes
    // the capture feedback loop, previously a transparent dummy in Vulkan)
    rend3dVk->SetCaptureImages(CaptureOutput128Img.View, CaptureOutput256Img.View);

    {
        VkDescriptorBufferInfo bufInfo = {FPConfigRing.Buf.Buf, 0, sizeof(sFinalPassConfig)};
        VkDescriptorImageInfo mainInputs[2] = {
            {SamplerNearestClamp, rend2dA->GetOutput().View,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {SamplerNearestClamp, rend2dB->GetOutput().View,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        };
        VkDescriptorImageInfo auxInputs[2] = {
            {SamplerNearestRepeat, AuxInputView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {SamplerNearestRepeat, CaptureOutput256Img.View,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        };

        for (u32 variant = 0; variant < 2; variant++)
        {
            VkWriteDescriptorSet writes[4] = {};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = FPDescSet[variant];
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            writes[0].pBufferInfo = &bufInfo;
            for (u32 i = 0; i < 2; i++)
            {
                writes[1 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[1 + i].dstSet = FPDescSet[variant];
                writes[1 + i].dstBinding = 1 + i;
                writes[1 + i].descriptorCount = 1;
                writes[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[1 + i].pImageInfo = &mainInputs[i];
            }
            writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet = FPDescSet[variant];
            writes[3].dstBinding = 3;
            writes[3].descriptorCount = 1;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[3].pImageInfo = &auxInputs[variant];
            VK::vkUpdateDescriptorSets(Ctx->Device, 4, writes, 0, nullptr);
        }
    }

    {
        VkDescriptorImageInfo imgInfo128 = {SamplerNearestRepeat, CaptureOutput128Img.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo imgInfo256 = {SamplerNearestRepeat, CaptureOutput256Img.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = CapDown128Set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &imgInfo128;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = CapDown256Set;
        writes[1].dstBinding = 0;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &imgInfo256;

        VK::vkUpdateDescriptorSets(Ctx->Device, 2, writes, 0, nullptr);
    }

    InvalidateCaptureDescCache();
}


// ---- per-frame command buffer plumbing --------------------------------

void VulkanRenderer::EnsureFrameStarted()
{
    if (FrameStarted)
        return;

    int s = FrameSlot;

    // reclaim this slot if a frame from 2 frames ago is still marked pending
    // (normally already waited at that frame's present; this is the safety net)
    if (SlotPending[s])
    {
        VK::vkWaitForFences(Ctx->Device, 1, &FrameFence[s], VK_TRUE, UINT64_MAX);
        VK::vkResetFences(Ctx->Device, 1, &FrameFence[s]);
        SlotPending[s] = false;
    }

    VK::vkResetCommandBuffer(FrameCmd[s], 0);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK::vkBeginCommandBuffer(FrameCmd[s], &beginInfo);

    CurCmd = FrameCmd[s];
    FrameStarted = true;
}

void VulkanRenderer::SubmitAndWaitFrame()
{
    if (!FrameStarted)
        return;

    PrepareMappedBuffersForSubmit();

    int s = FrameSlot;
    VK::vkEndCommandBuffer(FrameCmd[s]);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &FrameCmd[s];
    VK::vkQueueSubmit(Ctx->Queue, 1, &submitInfo, FrameFence[s]);

    VK::vkWaitForFences(Ctx->Device, 1, &FrameFence[s], VK_TRUE, UINT64_MAX);
    VK::vkResetFences(Ctx->Device, 1, &FrameFence[s]);

    SlotPending[s] = false;
    FrameStarted = false;
    CurCmd = VK_NULL_HANDLE;
}

// Submit without blocking; the fence is reclaimed at the next VBlank present
// (or the safety net in EnsureFrameStarted) so CPU frame N+1 overlaps GPU N.
void VulkanRenderer::SubmitFramePipelined()
{
    if (!FrameStarted)
        return;

    PrepareMappedBuffersForSubmit();

    int s = FrameSlot;
    VK::vkEndCommandBuffer(FrameCmd[s]);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &FrameCmd[s];
    VK::vkQueueSubmit(Ctx->Queue, 1, &submitInfo, FrameFence[s]);

    SlotPending[s] = true;
    FrameStarted = false;
    CurCmd = VK_NULL_HANDLE;
}

void VulkanRenderer::PrepareMappedBuffersForSubmit()
{
    // Command buffers are double-buffered, but the mapped upload/config
    // buffers are shared. Let the CPU build the next frame in private mirrors,
    // then publish those mirrors only after the previous GPU consumer exits.
    const int prev = FrameSlot ^ 1;
    if (SlotPending[prev])
    {
        VK::vkWaitForFences(Ctx->Device, 1, &FrameFence[prev], VK_TRUE, UINT64_MAX);
        VK::vkResetFences(Ctx->Device, 1, &FrameFence[prev]);
        SlotPending[prev] = false;
    }

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2dA->FlushMappedBuffers();
    rend2dB->FlushMappedBuffers();
    FlushMappedBuffers();
}


// ---- Vulkan plumbing helpers -------------------------------------------

u32 VulkanRenderer::RingAlloc(sRingBuffer& ring, u32 size)
{
    size = (size + 255) & ~255u;
    if (size > ring.Buf.Size || ring.Offset > ring.Buf.Size - size)
    {
        if (!ring.Overflowed)
        {
            Log(LogLevel::Error,
                "GPU_Vulkan: transient buffer exhausted (used=%u, request=%u, capacity=%llu)\n",
                ring.Offset, size, (unsigned long long)ring.Buf.Size);
            ring.Overflowed = true;
        }
        return ~0u;
    }

    u32 offset = ring.Offset;
    ring.Offset += size;
    return offset;
}

bool VulkanRenderer::InitConfigRing(sConfigRing& ring, u32 size, u32 slots)
{
    u32 uboAlign = (u32)Ctx->Props.limits.minUniformBufferOffsetAlignment;
    if (uboAlign < 16) uboAlign = 16;

    ring.Stride = (size + uboAlign - 1) & ~(uboAlign - 1);
    ring.Slots = slots;
    ring.Next = 0;
    ring.CurOffset = 0;
    if (!Ctx->CreateBuffer(ring.Buf, (VkDeviceSize)ring.Stride * slots,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true))
        return false;
    ring.Host.resize(ring.Buf.Size);
    memset(ring.Host.data(), 0, ring.Host.size());
    memset(ring.Buf.Map, 0, ring.Buf.Size);
    return true;
}

void VulkanRenderer::PushConfig(sConfigRing& ring, const void* data, u32 size)
{
    if (ring.Next >= ring.Slots)
    {
        if (!ring.Overflowed)
        {
            Log(LogLevel::Error,
                "GPU_Vulkan: per-band config buffer exhausted (%u slots)\n",
                ring.Slots);
            ring.Overflowed = true;
        }
        return;
    }

    ring.CurOffset = ring.Next * ring.Stride;
    memcpy(ring.Host.data() + ring.CurOffset, data, size);
    ring.Next++;
}

void VulkanRenderer::FlushMappedBuffers()
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

    flushRing(AuxStagingRing);
    flushRing(CaptureVertexRing);
    flushConfig(FPConfigRing);
    flushConfig(CaptureConfigRing);
}

void VulkanRenderer::BeginColorTarget(VK::Context::Image& img)
{
    Ctx->TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
}

void VulkanRenderer::EndColorTarget(VK::Context::Image& img)
{
    Ctx->TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

void VulkanRenderer::BeginTexUpload(VK::Context::Image& img)
{
    Ctx->TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
}

void VulkanRenderer::EndTexUpload(VK::Context::Image& img)
{
    Ctx->TransitionImage(CurCmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

bool VulkanRenderer::UploadTexRows(VK::Context::Image& img, const void* data,
                                   u32 rowStart, u32 rowCount, u32 bytesPerRow, u32 layer)
{
    u32 size = rowCount * bytesPerRow;
    u32 offset = RingAlloc(AuxStagingRing, size);
    if (offset == ~0u)
        return false;
    memcpy(AuxStagingRing.Host.data() + offset, data, size);

    VkBufferImageCopy region = {};
    region.bufferOffset = offset;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
    region.imageOffset = {0, (s32)rowStart, 0};
    region.imageExtent = {img.Width, rowCount, 1};
    VK::vkCmdCopyBufferToImage(CurCmd, AuxStagingRing.Buf.Buf, img.Img,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return true;
}

void VulkanRenderer::InvalidateCaptureDescCache()
{
    if (CaptureDescPool != VK_NULL_HANDLE)
        VK::vkResetDescriptorPool(Ctx->Device, CaptureDescPool, 0);
    CaptureDescCache.clear();
}

VkDescriptorSet VulkanRenderer::GetCaptureDescriptorSet(VkImageView viewA, VkImageView viewB)
{
    std::array<uintptr_t, 2> key = {(uintptr_t)viewA, (uintptr_t)viewB};
    auto it = CaptureDescCache.find(key);
    if (it != CaptureDescCache.end())
        return it->second;

    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = CaptureDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &CaptureSetLayout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (VK::vkAllocateDescriptorSets(Ctx->Device, &allocInfo, &set) != VK_SUCCESS)
    {
        Log(LogLevel::Error, "GPU_Vulkan: capture descriptor pool exhausted\n");
        return VK_NULL_HANDLE;
    }

    VkDescriptorBufferInfo bufInfo = {CaptureConfigRing.Buf.Buf, 0, sizeof(sCaptureConfig)};
    VkDescriptorImageInfo imgInfoA = {SamplerNearestBorder, viewA, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo imgInfoB = {SamplerNearestRepeat, viewB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet writes[3] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].pBufferInfo = &bufInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &imgInfoA;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &imgInfoB;

    VK::vkUpdateDescriptorSets(Ctx->Device, 3, writes, 0, nullptr);

    CaptureDescCache.emplace(key, set);
    return set;
}


// ---- mirrors of GLRenderer's per-frame driver logic --------------------

void VulkanRenderer::DrawScanline(u32 line)
{
    if (!Ctx->Valid)
        return;

    FrameDirty = true;

    EnsureFrameStarted();

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2dA->SetCommandBuffer(FrameCmd[FrameSlot]);
    rend2dB->SetCommandBuffer(FrameCmd[FrameSlot]);

    u32 dispcnt_a_diff = DispCntA ^ GPU.GPU2D_A.DispCnt;
    u32 dispcnt_b_diff = DispCntB ^ GPU.GPU2D_B.DispCnt;
    u32 capturecnt_diff = CaptureCnt ^ GPU.CaptureCnt;

    bool need_render = false;
    bool need_capture = false;

    if (dispcnt_a_diff & 0xF0000)
        need_render = true;
    else if (dispcnt_b_diff & 0x10000)
        need_render = true;
    else if (MasterBrightnessA != GPU.MasterBrightnessA ||
             MasterBrightnessB != GPU.MasterBrightnessB)
        need_render = true;

    if (GPU.CaptureEnable && (capturecnt_diff & 0x7FFFFFFF))
    {
        need_render = true;
        need_capture = true;
    }

    NeedPartialRender = need_render;
    rend2dA->SetNeedPartialRender(NeedPartialRender);
    rend2dB->SetNeedPartialRender(NeedPartialRender);
    rend2dA->DrawScanline(line);
    rend2dB->DrawScanline(line);

    if (need_render && (line > 0))
    {
        RenderScreen(LastLine, line);
        LastLine = line;
    }

    if (need_capture && (line > 0))
    {
        DoCapture(LastCapLine, line);
        LastCapLine = line;
    }

    DispCntA = GPU.GPU2D_A.DispCnt;
    DispCntB = GPU.GPU2D_B.DispCnt;
    MasterBrightnessA = GPU.MasterBrightnessA;
    MasterBrightnessB = GPU.MasterBrightnessB;
    CaptureCnt = GPU.CaptureCnt;

    FinalPassConfig.uScreenSwap[line] = GPU.ScreenSwap;
    FinalPassConfig.uVCount[line] = GPU.VCount;
    CaptureConfig.uVCount[line] = GPU.VCount;

    u32 dispcnt = GPU.GPU2D_A.DispCnt;
    u32 dispmode = (dispcnt >> 16) & 0x3;
    u32 capcnt = GPU.CaptureCnt;
    u32 capsel = (capcnt >> 29) & 0x3;
    u32 capA = (capcnt >> 24) & 0x1;
    u32 capB = (capcnt >> 25) & 0x1;
    bool checkcap = GPU.CaptureEnable && (capsel != 0);

    if (GPU.CaptureEnable && (capsel != 1))
    {
        if (capA == 0)
            CaptureConfig.uSrcAOffset[line] = 0;
        else
        {
            int xpos = GPU.GPU3D.GetRenderXPos() & 0x1FF;
            xpos -= ((xpos & 0x100) << 1);
            CaptureConfig.uSrcAOffset[line] = (float)xpos / 256.f;
        }
    }

    if ((dispmode == 2) || (checkcap && (capB == 0)))
    {
        AuxUsageMask |= (1<<0);

        u32 vrambank = (dispcnt >> 18) & 0x3;
        u32 vramoffset = GPU.VCount * 256;
        u32 outoffset = line * 256;
        if (dispmode != 2)
        {
            u32 yoff = ((capcnt >> 26) & 0x3) << 14;
            vramoffset += yoff;
            outoffset += yoff;
        }

        vramoffset &= 0xFFFF;
        outoffset &= 0xFFFF;

        u16* adst = &AuxInputBuffer[0][outoffset];

        if (GPU.VRAMMap_LCDC & (1<<vrambank))
        {
            u16* vram = (u16*)GPU.VRAM[vrambank];

            for (int i = 0; i < 256; i++)
            {
                adst[i] = vram[vramoffset];
                vramoffset++;
            }
        }
        else
        {
            for (int i = 0; i < 256; i++)
                adst[i] = 0;
        }

        AuxInputDirty[0][outoffset >> 8] = true;
    }

    if ((dispmode == 3) || (checkcap && (capB == 1)))
    {
        AuxUsageMask |= (1<<1);

        u16* adst = &AuxInputBuffer[1][line * 256];
        for (int i = 0; i < 256; i++)
            adst[i] = GPU.DispFIFOBuffer[i];
        AuxInputDirty[1][line] = true;
    }
}

void VulkanRenderer::DrawSprites(u32 line)
{
    if (!Ctx->Valid)
        return;

    // this can run standalone (VCount 262 pre-renders the next frame's
    // sprite line 0 after this frame's VBlank() has already submitted and
    // ended the command buffer), so a fresh one may need to be opened here
    EnsureFrameStarted();

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2dA->SetCommandBuffer(FrameCmd[FrameSlot]);
    rend2dB->SetCommandBuffer(FrameCmd[FrameSlot]);
    rend2dA->DrawSprites(line);
    rend2dB->DrawSprites(line);
}


void VulkanRenderer::RenderScreen(int ystart, int yend)
{
    if (ystart >= yend)
        return;

    EnsureFrameStarted();

    int vramcap = -1;
    if (AuxUsageMask & (1<<0))
    {
        u32 vrambank = (DispCntA >> 18) & 0x3;
        if (GPU.VRAMMap_LCDC & (1<<vrambank))
            vramcap = GPU.GetCaptureBlock_LCDC(vrambank << 17);
    }
    Aux0VRAMCap = vramcap;

    // Transfers and pipeline barriers are not valid inside a render pass.
    // Publish only auxiliary rows changed since the preceding band.
    FlushAuxInput(vramcap);

    // Each band uses LOAD to preserve rows written by earlier bands. The
    // layout stays unchanged, but those color writes still need an explicit
    // memory dependency before the next render pass loads the attachment.
    Ctx->TransitionImage(CurCmd, FPOutputImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VkRenderPassBeginInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpInfo.renderPass = FPRenderPass;
    rpInfo.framebuffer = FPFramebuffer;
    rpInfo.renderArea = {{0, 0}, {(u32)ScreenW, (u32)ScreenH}};
    VK::vkCmdBeginRenderPass(CurCmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {0.f, 0.f, (float)ScreenW, (float)ScreenH, 0.f, 1.f};
    VK::vkCmdSetViewport(CurCmd, 0, 1, &viewport);
    VkRect2D scissor = {{0, ystart * ScaleFactor}, {(u32)ScreenW, (u32)((yend - ystart) * ScaleFactor)}};
    VK::vkCmdSetScissor(CurCmd, 0, 1, &scissor);

    if (!GPU.ScreensEnabled)
    {
        VkClearAttachment clearAtt[2] = {};
        for (int i = 0; i < 2; i++)
        {
            clearAtt[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clearAtt[i].colorAttachment = i;
            clearAtt[i].clearValue.color = {{0.f, 0.f, 0.f, 1.f}};
        }
        VkClearRect rect = {};
        rect.rect = scissor;
        rect.baseArrayLayer = 0;
        rect.layerCount = 1;
        VK::vkCmdClearAttachments(CurCmd, 2, clearAtt, 1, &rect);
    }
    else
    {
        FinalPassConfig.uScaleFactor = ScaleFactor;
        FinalPassConfig.uDither = EnableDither ? 1 : 0;
        FinalPassConfig.uDispModeA = (DispCntA >> 16) & 0x3;
        FinalPassConfig.uDispModeB = (DispCntB >> 16) & 0x1;
        FinalPassConfig.uBrightModeA = (MasterBrightnessA >> 14) & 0x3;
        FinalPassConfig.uBrightModeB = (MasterBrightnessB >> 14) & 0x3;
        FinalPassConfig.uBrightFactorA = std::min(MasterBrightnessA & 0x1F, 16);
        FinalPassConfig.uBrightFactorB = std::min(MasterBrightnessB & 0x1F, 16);
        FinalPassConfig.uAuxUseVCount = 0;

        u32 modeA = (DispCntA >> 16) & 0x3;
        VkDescriptorSet fpDescSet = FPDescSet[0];
        if ((modeA == 2) && (vramcap != -1))
        {
            FinalPassConfig.uAuxLayer = vramcap >> 2;
            FinalPassConfig.uAuxColorFactor = 63.75f;
            FinalPassConfig.uAuxUseVCount = 1;
            fpDescSet = FPDescSet[1];
        }
        else if (modeA >= 2)
        {
            FinalPassConfig.uAuxLayer = (modeA - 2);
            FinalPassConfig.uAuxColorFactor = 62.f;
        }

        PushConfig(FPConfigRing, &FinalPassConfig, sizeof(FinalPassConfig));

        VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, FPPipeline);
        u32 dynOffset = FPConfigRing.CurOffset;
        VK::vkCmdBindDescriptorSets(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, FPPipelineLayout,
                                    0, 1, &fpDescSet, 1, &dynOffset);

        VkDeviceSize bindOffset = 0;
        VK::vkCmdBindVertexBuffers(CurCmd, 0, 1, &FPVertexBuffer.Buf, &bindOffset);
        VK::vkCmdDraw(CurCmd, 2 * 3, 1, 0, 0);
    }

    VK::vkCmdEndRenderPass(CurCmd);
}

void VulkanRenderer::FlushAuxInput(int vramcap)
{
    bool uploading = false;

    for (int layer = 0; layer < 2; layer++)
    {
        if (!(AuxUsageMask & (1 << layer)))
            continue;
        if (layer == 0 && vramcap != -1)
            continue;

        const int rows = layer == 0 ? 256 : 192;
        for (int start = 0; start < rows;)
        {
            while (start < rows && !AuxInputDirty[layer][start])
                start++;
            if (start >= rows)
                break;

            int end = start + 1;
            while (end < rows && AuxInputDirty[layer][end])
                end++;

            if (!uploading)
            {
                BeginTexUpload(AuxInputImg);
                uploading = true;
            }

            if (UploadTexRows(AuxInputImg, &AuxInputBuffer[layer][start * 256],
                              start, end - start, 256 * sizeof(u16), layer))
            {
                memset(&AuxInputDirty[layer][start], 0,
                       (end - start) * sizeof(AuxInputDirty[layer][0]));
            }
            start = end;
        }
    }

    if (uploading)
        EndTexUpload(AuxInputImg);
}

void VulkanRenderer::VBlank(u32 endLine)
{
    if (!Ctx->Valid || !FrameDirty)
        return;

    endLine = std::min(endLine, 192u);
    EnsureFrameStarted();

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2dA->SetCommandBuffer(FrameCmd[FrameSlot]);
    rend2dB->SetCommandBuffer(FrameCmd[FrameSlot]);
    rend2dA->Flush(endLine);
    rend2dB->Flush(endLine);

    if (LastLine < (int)endLine)
        RenderScreen(LastLine, endLine);

    if (GPU.CaptureEnable && LastCapLine < (int)endLine)
        DoCapture(LastCapLine, endLine);

    LastLine = endLine;
    if (GPU.CaptureEnable)
        LastCapLine = endLine;
    FrameDirty = false;
}

void VulkanRenderer::FinishFrame(u32 endLine)
{
    if (!Ctx->Valid || FrameReady)
        return;

    endLine = std::min(endLine, 192u);
    EnsureFrameStarted();

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    auto* rend2dB = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2dA->SetCommandBuffer(FrameCmd[FrameSlot]);
    rend2dB->SetCommandBuffer(FrameCmd[FrameSlot]);
    rend2dA->FinishFrame(endLine);
    rend2dB->FinishFrame(endLine);

    if (FrameDirty)
    {
        if (LastLine < (int)endLine)
            RenderScreen(LastLine, endLine);
        if (GPU.CaptureEnable && LastCapLine < (int)endLine)
            DoCapture(LastCapLine, endLine);
    }

    LastLine = 0;
    LastCapLine = 0;
    FrameDirty = false;

    // read back the finished FinalPass output and hand it to the GL
    // compositor as the BACK buffer (mirrors GLRenderer::RenderScreen
    // writing into FPOutputFB[BackBuffer]; the front/back flip happens in
    // Renderer::SwapBuffers(), called by GPU.cpp at frame end)
    int backbuf = BackBuffer;

    // copy this frame's FinalPass output into this slot's readback buffer
    int cur = FrameSlot;
    if (FPReadbackBuffer[cur].Buf != VK_NULL_HANDLE)
    {
        Ctx->TransitionImage(FrameCmd[cur], FPOutputImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        VkBufferImageCopy region = {};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 2};
        region.imageExtent = {(u32)ScreenW, (u32)ScreenH, 1};
        VK::vkCmdCopyImageToBuffer(FrameCmd[cur], FPOutputImg.Img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   FPReadbackBuffer[cur].Buf, 1, &region);

        VkMemoryBarrier hostBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        VK::vkCmdPipelineBarrier(FrameCmd[cur], VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hostBarrier, 0, nullptr, 0, nullptr);

        Ctx->TransitionImage(FrameCmd[cur], FPOutputImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }

    auto uploadToGL = [&](VK::Context::Buffer& rb)
    {
        if (!rb.Map)
            return false;

        glBindTexture(GL_TEXTURE_2D_ARRAY, FPOutputTex[backbuf]);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, ScreenW, ScreenH, 2,
                        GL_RGBA, GL_UNSIGNED_BYTE, rb.Map);
        return true;
    };

    if (!HavePrevFrame)
    {
        // first frame: no completed frame to show yet, so present this one
        // synchronously to avoid flashing an uninitialised back buffer
        SubmitAndWaitFrame();
        FrameReady = uploadToGL(FPReadbackBuffer[cur]);
        HavePrevFrame = true;
    }
    else
    {
        // steady state: hand this frame to the GPU without blocking, then
        // present the PREVIOUS slot (finished during this frame's emulation)
        // with 1 frame of latency. Waiting it here is where CPU/GPU overlap.
        SubmitFramePipelined();
        int prev = cur ^ 1;
        if (SlotPending[prev])
        {
            VK::vkWaitForFences(Ctx->Device, 1, &FrameFence[prev], VK_TRUE, UINT64_MAX);
            VK::vkResetFences(Ctx->Device, 1, &FrameFence[prev]);
            SlotPending[prev] = false;
        }
        FrameReady = uploadToGL(FPReadbackBuffer[prev]);
    }

    FrameSlot ^= 1;
}

void VulkanRenderer::VBlankEnd()
{
    AuxUsageMask = 0;
    FrameDirty = true;
}

void VulkanRenderer::SwapBuffers()
{
    if (!FrameReady)
        return;

    Renderer::SwapBuffers();
    FrameReady = false;
}


void VulkanRenderer::DoCapture(int ystart, int yend)
{
    u32 dispcnt = DispCntA;
    u32 capcnt = CaptureCnt;
    u32 dispmode = (dispcnt >> 16) & 0x3;
    u32 srcA = (capcnt >> 24) & 0x1;
    u32 srcB = (capcnt >> 25) & 0x1;
    u32 srcBblock = (dispcnt >> 18) & 0x3;
    u32 srcBoffset = (dispmode == 2) ? 0 : ((capcnt >> 26) & 0x3);
    u32 dstblock = (capcnt >> 16) & 0x3;
    u32 dstoffset = (capcnt >> 18) & 0x3;
    u32 capsize = (capcnt >> 20) & 0x3;
    u32 dstmode = (capcnt >> 29) & 0x3;
    u32 eva = std::min(capcnt & 0x1F, 16u);
    u32 evb = std::min((capcnt >> 8) & 0x1F, 16u);

    // determine the region we're going to capture to

    int dstwidth, dstheight;

    if (capsize == 0)
    {
        dstwidth = 128;
        dstheight = 128;
    }
    else
    {
        dstwidth = 256;
        dstheight = 64 * capsize;
    }

    EnsureFrameStarted();

    FlushAuxInput(Aux0VRAMCap);

    auto* rend3dVk = dynamic_cast<ComputeRenderer3D_Vulkan*>(Rend3D.get());

    auto* rend2dA = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    VkImageView viewA = srcA ? rend3dVk->GetOutputImage().View : rend2dA->GetOutput().View;

    bool useSrcB = (dstmode == 1) || (dstmode >= 2 && evb > 0);

    VkImageView viewB = AuxInputView;
    u32 layerB = srcB;
    CaptureConfig.uSrcBColorFactor = 248.f;

    const bool useTrackedSrcB = useSrcB && !srcB && (Aux0VRAMCap != -1);
    if (useTrackedSrcB)
    {
        // hi-res VRAM
        if (capsize != 0)
        {
            // CaptureOutput256Img contains all four banks. Rendering any one
            // bank transitions the whole image to attachment layout, so a
            // different bank cannot remain bound as shader-read input. A
            // full scratch snapshot also handles non-linear VCOUNT rows and
            // same-bank read-before-write feedback without mixed layouts.
            if (dstblock == srcBblock && dstoffset != srcBoffset)
                Log(LogLevel::Error, "GPU_Vulkan: MISMATCHED VRAM OFFSETS ON SAME BANK!!! bank=%d src=%d dst=%d\n",
                    dstblock, srcBoffset, dstoffset);

            Ctx->TransitionImage(CurCmd, CaptureOutput256Img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            Ctx->TransitionImage(CurCmd, CaptureVRAMImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

            VkImageCopy region = {};
            region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, srcBblock, 1};
            region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.extent = {(u32)(256 * ScaleFactor), (u32)(256 * ScaleFactor), 1};

            VK::vkCmdCopyImage(CurCmd, CaptureOutput256Img.Img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               CaptureVRAMImg.Img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            Ctx->TransitionImage(CurCmd, CaptureOutput256Img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
            Ctx->TransitionImage(CurCmd, CaptureVRAMImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

            viewB = CaptureVRAMImg.View;
            layerB = 0;
        }
        else
        {
            // A 128-wide capture writes a separate image, so the tracked
            // 256-wide source can stay bound directly.
            viewB = CaptureOutput256Img.View;
            layerB = srcBblock;
        }

        CaptureConfig.uSrcBColorFactor = 255.f;
    }

    VkDescriptorSet set = GetCaptureDescriptorSet(viewA, viewB);
    if (set == VK_NULL_HANDLE)
        return;

    VK::Context::Image& dstImg = (capsize == 0) ? CaptureOutput128Img : CaptureOutput256Img;
    VkFramebuffer dstFB;
    u32 fbSize;
    if (capsize == 0)
    {
        dstFB = CaptureOutput128FB[(dstblock << 2) | dstoffset];
        fbSize = (u32)(128 * ScaleFactor);
    }
    else
    {
        dstFB = CaptureOutput256FB[dstblock];
        fbSize = (u32)(256 * ScaleFactor);
    }

    CaptureConfig.uInvCaptureSize[0] = 1.f / (float)dstwidth;
    CaptureConfig.uInvCaptureSize[1] = 1.f / (float)dstheight;

    CaptureConfig.uSrcALayer = srcA;

    if (srcB == 0)
        CaptureConfig.uSrcBOffset = 64 * srcBoffset;
    else
        CaptureConfig.uSrcBOffset = 0;

    CaptureConfig.uSrcBLayer = layerB;
    CaptureConfig.uSrcBUseVCount = useTrackedSrcB;

    CaptureConfig.uDstMode = dstmode;
    CaptureConfig.uBlendFactors[0] = eva;
    CaptureConfig.uBlendFactors[1] = evb;

    PushConfig(CaptureConfigRing, &CaptureConfig, sizeof(CaptureConfig));

    s16 vtxbuf[192 * 6 * 4];
    s16* vptr = vtxbuf;
    int numvtx = 0;

    // VCOUNT can be rewritten during active display. Capture writes use
    // the emulated row, while 2D/FIFO inputs remain scheduled snapshots;
    // the fragment shader remaps only direct-3D and tracked VRAM inputs.
    for (int line = ystart; line < yend; line++)
    {
        const int vcount = CaptureConfig.uVCount[line];
        if (vcount >= dstheight)
            continue;

        const int y0 = capsize == 0 ? vcount : ((dstoffset * 64 + vcount) & 0xFF);
        const int y1 = y0 + 1;
        const int t0 = line;
        const int t1 = line + 1;

        *vptr++ = 0;        *vptr++ = y1; *vptr++ = 0;         *vptr++ = t1;
        *vptr++ = dstwidth; *vptr++ = y0; *vptr++ = dstwidth;  *vptr++ = t0;
        *vptr++ = dstwidth; *vptr++ = y1; *vptr++ = dstwidth;  *vptr++ = t1;
        *vptr++ = 0;        *vptr++ = y1; *vptr++ = 0;         *vptr++ = t1;
        *vptr++ = 0;        *vptr++ = y0; *vptr++ = 0;         *vptr++ = t0;
        *vptr++ = dstwidth; *vptr++ = y0; *vptr++ = dstwidth;  *vptr++ = t0;
        numvtx += 6;
    }

    if (numvtx == 0)
        return;

    u32 vtxbytes = (u32)numvtx * 4 * sizeof(s16);
    u32 vtxoffset = RingAlloc(CaptureVertexRing, vtxbytes);
    if (vtxoffset == ~0u)
        return;
    memcpy(CaptureVertexRing.Host.data() + vtxoffset, vtxbuf, vtxbytes);

    BeginColorTarget(dstImg);

    VkRenderPassBeginInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpInfo.renderPass = CaptureRenderPass;
    rpInfo.framebuffer = dstFB;
    rpInfo.renderArea = {{0, 0}, {fbSize, fbSize}};
    VK::vkCmdBeginRenderPass(CurCmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {0.f, 0.f, (float)fbSize, (float)fbSize, 0.f, 1.f};
    VK::vkCmdSetViewport(CurCmd, 0, 1, &viewport);
    VkRect2D scissor = {{0, 0}, {fbSize, fbSize}};
    VK::vkCmdSetScissor(CurCmd, 0, 1, &scissor);

    VK::vkCmdBindPipeline(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, CapturePipeline);
    u32 dynOffset = CaptureConfigRing.CurOffset;
    VK::vkCmdBindDescriptorSets(CurCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, CapturePipelineLayout,
                               0, 1, &set, 1, &dynOffset);

    VkDeviceSize bindOffset = vtxoffset;
    VK::vkCmdBindVertexBuffers(CurCmd, 0, 1, &CaptureVertexRing.Buf.Buf, &bindOffset);
    VK::vkCmdDraw(CurCmd, numvtx, 1, 0, 0);

    VK::vkCmdEndRenderPass(CurCmd);
    EndColorTarget(dstImg);
}


void VulkanRenderer::AllocCapture(u32 bank, u32 start, u32 len)
{
    auto* rend2D = dynamic_cast<VulkanRenderer2D*>(Rend2D_A.get());
    rend2D->LayerConfigDirty = true;
    rend2D->SpriteConfigDirty = true;
    rend2D = dynamic_cast<VulkanRenderer2D*>(Rend2D_B.get());
    rend2D->LayerConfigDirty = true;
    rend2D->SpriteConfigDirty = true;
}

void VulkanRenderer::DownscaleCapture(VkCommandBuffer cmd, int width, int height, int layer)
{
    // downscale a hi-res capture buffer to 1x IR, ready for CPU readback;
    // like the GL renderer, this uses a shader pass (not a blit) so color
    // components get accurately averaged rather than point-sampled
    VkRenderPassBeginInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpInfo.renderPass = CapDownRenderPass;
    rpInfo.framebuffer = CaptureSyncFB;
    rpInfo.renderArea = {{0, 0}, {256, 256}};
    VK::vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {0.f, 0.f, (float)width, (float)height, 0.f, 1.f};
    VK::vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {{0, 0}, {(u32)width, (u32)height}};
    VK::vkCmdSetScissor(cmd, 0, 1, &scissor);

    VK::vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, CapDownPipeline);
    VkDescriptorSet set = (width == 128) ? CapDown128Set : CapDown256Set;
    VK::vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, CapDownPipelineLayout,
                               0, 1, &set, 0, nullptr);

    s32 layerVal = layer;
    VK::vkCmdPushConstants(cmd, CapDownPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(s32), &layerVal);

    VkDeviceSize bindOffset = 0;
    VK::vkCmdBindVertexBuffers(cmd, 0, 1, &RectVtxBuffer.Buf, &bindOffset);
    VK::vkCmdDraw(cmd, 2 * 3, 1, 0, 0);

    VK::vkCmdEndRenderPass(cmd);

    // the render pass's finalLayout is COLOR_ATTACHMENT_OPTIMAL; correct
    // our bookkeeping to match before the manual transition below (mirrors
    // the OBJDepthImg.Layout correction in VulkanRenderer2D::SetScaleFactor)
    CaptureSyncImg.Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    Ctx->TransitionImage(cmd, CaptureSyncImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
}

void VulkanRenderer::SyncVRAMCapture(u32 bank, u32 start, u32 len, bool complete)
{
    if (!complete)
        Log(LogLevel::Error, "GPU_Vulkan: !!! READING VRAM AS IT IS BEING CAPTURED TO\n");

    u8* vram = GPU.VRAM[bank];

    // this can run mid-frame: SyncVRAMCaptureBlock is called from GPU.cpp
    // while frame emulation is in progress, not just at frame boundaries.
    // Flush whatever capture work is already recorded so the downscale
    // below observes it, mirroring the implicit GPU sync glReadPixels
    // performs in GLRenderer::SyncVRAMCapture. The next DrawScanline/
    // DrawSprites call will lazily reopen a fresh command buffer for the
    // remainder of the frame.
    if (FrameStarted)
        SubmitAndWaitFrame();

    VkCommandBuffer cmd = Ctx->BeginOneShot();
    CurCmd = cmd;

    if (len == 0) // 128x128
    {
        DownscaleCapture(cmd, 128, 128, (int)((bank << 2) | start));

        VkBufferImageCopy region = {};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {128, 128, 1};
        VK::vkCmdCopyImageToBuffer(cmd, CaptureSyncImg.Img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   CaptureSyncReadback.Buf, 1, &region);
    }
    else
    {
        DownscaleCapture(cmd, 256, 256, (int)bank);

        u32 pos = start;
        for (u32 i = 0; i < len;)
        {
            u32 end = pos + len;
            if (end > 4)
                end = 4;

            VkBufferImageCopy region = {};
            region.bufferOffset = (VkDeviceSize)(pos * 64) * 256 * 4;
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageOffset = {0, (s32)(pos * 64), 0};
            region.imageExtent = {256, (end - pos) * 64, 1};
            VK::vkCmdCopyImageToBuffer(cmd, CaptureSyncImg.Img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       CaptureSyncReadback.Buf, 1, &region);

            i += (end - pos);
            pos += (end - pos);
            pos &= 3;
        }
    }

    VkMemoryBarrier hostBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    VK::vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hostBarrier, 0, nullptr, 0, nullptr);

    // return CaptureSyncImg to a rest layout; the next DownscaleCapture
    // call always uses a clearing render pass so any layout is fine here,
    // but keeping the bookkeeping consistent costs nothing
    Ctx->TransitionImage(cmd, CaptureSyncImg, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    Ctx->EndOneShot(cmd); // submits + fence-waits

    CurCmd = VK_NULL_HANDLE;

    // unpack RGBA8 (already snapped to 5-bit granularity by the downscale
    // shader: oColor.rgb = (col.rgb>>3)/31, oColor.a = col.a>0?1:0) into the
    // DS's native RGBA5551 VRAM representation, matching what
    // glReadPixels(..., GL_UNSIGNED_SHORT_1_5_5_5_REV, ...) did for GL:
    // bit15=A, bits14-10=B, bits9-5=G, bits4-0=R
    auto unpackRegion = [&](u32 bufByteOffset, u32 rowCount, u32 strideWidth, u32 vramByteOffset)
    {
        const u8* src = (const u8*)CaptureSyncReadback.Map + bufByteOffset;
        u16* dst = (u16*)&vram[vramByteOffset];
        u32 count = rowCount * strideWidth;
        for (u32 i = 0; i < count; i++)
        {
            u8 r = src[i*4+0], g = src[i*4+1], b = src[i*4+2], a = src[i*4+3];
            dst[i] = (u16)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | (a ? (1 << 15) : 0));
        }
    };

    if (len == 0)
    {
        unpackRegion(0, 128, 128, start * 64 * 512);

        for (u32 j = start * 64; j < (start+1) * 64; j++)
            GPU.VRAMDirty[bank][j] = true;
    }
    else
    {
        u32 pos = start;
        for (u32 i = 0; i < len;)
        {
            u32 end = pos + len;
            if (end > 4)
                end = 4;

            unpackRegion(pos * 64 * 256 * 4, (end - pos) * 64, 256, pos * 64 * 512);

            for (u32 j = pos * 64; j < end * 64; j++)
                GPU.VRAMDirty[bank][j] = true;

            i += (end - pos);
            pos += (end - pos);
            pos &= 3;
        }
    }
}


bool VulkanRenderer::GetFramebuffers(void** top, void** bottom)
{
    // since we use an array texture, we only need one of the pointer fields
    int frontbuf = BackBuffer ^ 1;
    *top = &FPOutputTex[frontbuf];
    *bottom = nullptr;
    return false;
}


bool VulkanRenderer::NeedsShaderCompile()
{
    return Rend3D->NeedsShaderCompile();
}

void VulkanRenderer::ShaderCompileStep(int& current, int& count)
{
    return Rend3D->ShaderCompileStep(current, count);
}

}
