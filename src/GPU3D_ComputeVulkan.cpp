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

#include "GPU_OpenGL.h"

#include <assert.h>
#include <string.h>
#include <algorithm>

#include "Utils.h"
#include "Platform.h"

#include "GPU3D_ComputeVulkan.h"
#include "GPU3D_ComputeVulkan_shaders.h"

namespace melonDS
{

using Platform::Log;
using Platform::LogLevel;

// sentinels matching the GL compute renderer's (GLuint)-1/-2 capture markers
static VulkanTexArray* const kCaptureTex128 = (VulkanTexArray*)(uintptr_t)-1;
static VulkanTexArray* const kCaptureTex256 = (VulkanTexArray*)(uintptr_t)-2;

ComputeRenderer3D_Vulkan::ComputeRenderer3D_Vulkan(melonDS::GPU3D& gpu3D, GLRenderer* parent)
    : Renderer3D(gpu3D), Parent(parent), Texcache(gpu3D.GPU, TexcacheVulkanLoader(&Ctx))
{
    ScaleFactor = 0;
    HiresCoordinates = false;
}

bool ComputeRenderer3D_Vulkan::CompileShader(VkPipeline& pipeline, const std::string& source, const std::initializer_list<const char*>& defines)
{
    std::string shaderName;
    std::string shaderSource;
    shaderSource += "#version 460\n";
    for (const char* define : defines)
    {
        shaderSource += "#define ";
        shaderSource += define;
        shaderSource += '\n';
        shaderName += define;
        shaderName += ',';
    }
    shaderSource += "#define ScreenWidth ";
    shaderSource += std::to_string(ScreenWidth);
    shaderSource += "\n#define ScreenHeight ";
    shaderSource += std::to_string(ScreenHeight);
    shaderSource += "\n#define MaxWorkTiles ";
    shaderSource += std::to_string(MaxWorkTiles);
    shaderSource += "\n#define TileSize ";
    shaderSource += std::to_string(TileSize);
    shaderSource += "\nconst int CoarseTileCountY = ";
    shaderSource += std::to_string(CoarseTileCountY) + ";";
    shaderSource += "\n#define CoarseTileArea ";
    shaderSource += std::to_string(CoarseTileArea);
    shaderSource += "\n#define ClearCoarseBinMaskLocalSize ";
    shaderSource += std::to_string(ClearCoarseBinMaskLocalSize);
    shaderSource += "\n";

    shaderSource += ComputeRendererShadersVulkan::Common;
    shaderSource += source;

    VkShaderModule module = VK_NULL_HANDLE;
    if (!Ctx.CompileComputeShader(module, shaderSource, shaderName.c_str()))
        return false;

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = module;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = PipelineLayout;

    VkResult res = VK::vkCreateComputePipelines(Ctx.Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    VK::vkDestroyShaderModule(Ctx.Device, module, nullptr);

    if (res != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: failed to create pipeline %s (%d)\n", shaderName.c_str(), res);
        pipeline = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void ComputeRenderer3D_Vulkan::ShaderCompileStep(int& current, int& count)
{
    current = ShaderStepIdx;
    ShaderStepIdx++;
    count = 33;
    switch (current)
    {
    case 0:
        CompileShader(ShaderInterpXSpans[0], ComputeRendererShadersVulkan::InterpSpans, {"InterpSpans", "ZBuffer"});
        return;
    case 1:
        CompileShader(ShaderInterpXSpans[1], ComputeRendererShadersVulkan::InterpSpans, {"InterpSpans", "WBuffer"});
        return;
    case 2:
        CompileShader(ShaderBinCombined, ComputeRendererShadersVulkan::BinCombined, {"BinCombined"});
        return;
    case 3:
        CompileShader(ShaderDepthBlend[0], ComputeRendererShadersVulkan::DepthBlend, {"DepthBlend", "ZBuffer"});
        return;
    case 4:
        CompileShader(ShaderDepthBlend[1], ComputeRendererShadersVulkan::DepthBlend, {"DepthBlend", "WBuffer"});
        return;
    case 5:
        CompileShader(ShaderRasteriseNoTexture[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "NoTexture"});
        return;
    case 6:
        CompileShader(ShaderRasteriseNoTexture[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "NoTexture"});
        return;
    case 7:
        CompileShader(ShaderRasteriseNoTextureToon[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "NoTexture", "Toon"});
        return;
    case 8:
        CompileShader(ShaderRasteriseNoTextureToon[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "NoTexture", "Toon"});
        return;
    case 9:
        CompileShader(ShaderRasteriseNoTextureHighlight[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "NoTexture", "Highlight"});
        return;
    case 10:
        CompileShader(ShaderRasteriseNoTextureHighlight[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "NoTexture", "Highlight"});
        return;
    case 11:
        CompileShader(ShaderRasteriseUseTextureDecal[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Decal"});
        return;
    case 12:
        CompileShader(ShaderRasteriseUseTextureDecal[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Decal"});
        return;
    case 13:
        CompileShader(ShaderRasteriseUseTextureModulate[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Modulate"});
        return;
    case 14:
        CompileShader(ShaderRasteriseUseTextureModulate[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Modulate"});
        return;
    case 15:
        CompileShader(ShaderRasteriseUseTextureToon[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Toon"});
        return;
    case 16:
        CompileShader(ShaderRasteriseUseTextureToon[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Toon"});
        return;
    case 17:
        CompileShader(ShaderRasteriseUseTextureHighlight[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "UseTexture", "Highlight"});
        return;
    case 18:
        CompileShader(ShaderRasteriseUseTextureHighlight[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "UseTexture", "Highlight"});
        return;
    case 19:
        CompileShader(ShaderRasteriseShadowMask[0], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "ZBuffer", "ShadowMask"});
        return;
    case 20:
        CompileShader(ShaderRasteriseShadowMask[1], ComputeRendererShadersVulkan::Rasterise, {"Rasterise", "WBuffer", "ShadowMask"});
        return;
    case 21:
        CompileShader(ShaderClearCoarseBinMask, ComputeRendererShadersVulkan::ClearCoarseBinMask, {"ClearCoarseBinMask"});
        return;
    case 22:
        CompileShader(ShaderClearIndirectWorkCount, ComputeRendererShadersVulkan::ClearIndirectWorkCount, {"ClearIndirectWorkCount"});
        return;
    case 23:
        CompileShader(ShaderCalculateWorkListOffset, ComputeRendererShadersVulkan::CalcOffsets, {"CalculateWorkOffsets"});
        return;
    case 24:
        CompileShader(ShaderSortWork, ComputeRendererShadersVulkan::SortWork, {"SortWork"});
        return;
    case 25:
        CompileShader(ShaderFinalPass[0], ComputeRendererShadersVulkan::FinalPass, {"FinalPass"});
        return;
    case 26:
        CompileShader(ShaderFinalPass[1], ComputeRendererShadersVulkan::FinalPass, {"FinalPass", "EdgeMarking"});
        return;
    case 27:
        CompileShader(ShaderFinalPass[2], ComputeRendererShadersVulkan::FinalPass, {"FinalPass", "Fog"});
        return;
    case 28:
        CompileShader(ShaderFinalPass[3], ComputeRendererShadersVulkan::FinalPass, {"FinalPass", "EdgeMarking", "Fog"});
        return;
    case 29:
        CompileShader(ShaderFinalPass[4], ComputeRendererShadersVulkan::FinalPass, {"FinalPass", "AntiAliasing"});
        return;
    case 30:
        CompileShader(ShaderFinalPass[5], ComputeRendererShadersVulkan::FinalPass, {"FinalPass", "AntiAliasing", "EdgeMarking"});
        return;
    case 31:
        CompileShader(ShaderFinalPass[6], ComputeRendererShadersVulkan::FinalPass, {"FinalPass", "AntiAliasing", "Fog"});
        return;
    case 32:
        CompileShader(ShaderFinalPass[7], ComputeRendererShadersVulkan::FinalPass, {"FinalPass", "AntiAliasing", "EdgeMarking", "Fog"});
        return;
    default:
        return;
    }
}

bool ComputeRenderer3D_Vulkan::Init()
{
    if (!Ctx.Init())
    {
        Log(LogLevel::Error, "Vulkan compute renderer: no usable Vulkan implementation, falling back\n");
        return false;
    }

    // fixed-size buffers
    if (!Ctx.CreateBuffer(YSpanSetupMemory, sizeof(SpanSetupY)*MaxYSpanSetups,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true))
        return false;
    if (!Ctx.CreateBuffer(RenderPolygonMemory, sizeof(RenderPolygon)*2048,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true))
        return false;
    if (!Ctx.CreateBuffer(MetaUniformMemory, sizeof(MetaUniform),
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true))
        return false;

    // samplers, one per DS wrap mode combination
    for (u32 j = 0; j < 3; j++)
    {
        for (u32 i = 0; i < 3; i++)
        {
            const VkSamplerAddressMode translateWrapMode[3] =
            {
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
            };
            VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = translateWrapMode[i];
            samplerInfo.addressModeV = translateWrapMode[j];
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            if (VK::vkCreateSampler(Ctx.Device, &samplerInfo, nullptr, &Samplers[i+j*3]) != VK_SUCCESS)
                return false;
        }
    }

    {
        VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        if (VK::vkCreateSampler(Ctx.Device, &samplerInfo, nullptr, &ClearBitmapSampler) != VK_SUCCESS)
            return false;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (VK::vkCreateSampler(Ctx.Device, &samplerInfo, nullptr, &CaptureSampler) != VK_SUCCESS)
            return false;
    }

    // clear bitmap textures
    for (int i = 0; i < 2; i++)
    {
        if (!Ctx.CreateImage(ClearBitmapImg[i], VK_FORMAT_R32_UINT, 256, 256, 1,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false))
            return false;
    }

    // dummy textures so every descriptor is always valid
    if (!Ctx.CreateImage(DummyTexture, VK_FORMAT_R8G8B8A8_UINT, 1, 1, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true))
        return false;
    if (!Ctx.CreateImage(DummyCapture, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true))
        return false;

    const u32 zeroPixel = 0;
    Ctx.UploadImageLayer(DummyTexture, &zeroPixel, 1, 1, 0, 4);
    Ctx.UploadImageLayer(DummyCapture, &zeroPixel, 1, 1, 0, 4);
    {
        // move the clear bitmap images into a shader-readable state before first use
        u32* zeroes = new u32[256*256]();
        Ctx.UploadImageLayer(ClearBitmapImg[0], zeroes, 256, 256, 0, 4);
        Ctx.UploadImageLayer(ClearBitmapImg[1], zeroes, 256, 256, 0, 4);
        delete[] zeroes;
    }

    // descriptor set layouts
    {
        VkDescriptorSetLayoutBinding bindings[16] = {};
        auto setBinding = [&](u32 idx, VkDescriptorType type)
        {
            bindings[idx].binding = idx;
            bindings[idx].descriptorType = type;
            bindings[idx].descriptorCount = 1;
            bindings[idx].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        };
        setBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        for (u32 i = 1; i <= 10; i++)
            setBinding(i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        setBinding(11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        setBinding(12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        setBinding(13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        setBinding(14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        setBinding(15, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 16;
        layoutInfo.pBindings = bindings;
        if (VK::vkCreateDescriptorSetLayout(Ctx.Device, &layoutInfo, nullptr, &SetLayoutStatic) != VK_SUCCESS)
            return false;

        VkDescriptorSetLayoutBinding texBinding = {};
        texBinding.binding = 0;
        texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBinding.descriptorCount = 1;
        texBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &texBinding;
        if (VK::vkCreateDescriptorSetLayout(Ctx.Device, &layoutInfo, nullptr, &SetLayoutTexture) != VK_SUCCESS)
            return false;
    }

    // pipeline layout (shared by all pipelines)
    {
        VkDescriptorSetLayout setLayouts[2] = {SetLayoutStatic, SetLayoutTexture};

        VkPushConstantRange pushRange = {};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(RasterPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 2;
        layoutInfo.pSetLayouts = setLayouts;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (VK::vkCreatePipelineLayout(Ctx.Device, &layoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS)
            return false;
    }

    // descriptor pools
    {
        VkDescriptorPoolSize staticSizes[] =
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };
        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 4;
        poolInfo.pPoolSizes = staticSizes;
        if (VK::vkCreateDescriptorPool(Ctx.Device, &poolInfo, nullptr, &DescPoolStatic) != VK_SUCCESS)
            return false;

        VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = DescPoolStatic;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &SetLayoutStatic;
        if (VK::vkAllocateDescriptorSets(Ctx.Device, &allocInfo, &SetStatic) != VK_SUCCESS)
            return false;

        VkDescriptorPoolSize frameSizes[] =
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
        };
        poolInfo.maxSets = 512;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = frameSizes;
        if (VK::vkCreateDescriptorPool(Ctx.Device, &poolInfo, nullptr, &DescPoolFrame) != VK_SUCCESS)
            return false;
    }

    // command buffer + fence
    {
        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = Ctx.CmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        if (VK::vkAllocateCommandBuffers(Ctx.Device, &allocInfo, &FrameCmd) != VK_SUCCESS)
            return false;

        VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (VK::vkCreateFence(Ctx.Device, &fenceInfo, nullptr, &FrameFence) != VK_SUCCESS)
            return false;
    }

    ClearBitmap[0] = new u32[256*256];
    ClearBitmap[1] = new u32[256*256];

    return true;
}

ComputeRenderer3D_Vulkan::~ComputeRenderer3D_Vulkan()
{
    if (Ctx.Valid)
    {
        VK::vkDeviceWaitIdle(Ctx.Device);

        Texcache.Reset();

        DeleteShaders();
        DestroyScaleDependentResources();

        Ctx.DestroyBuffer(YSpanSetupMemory);
        Ctx.DestroyBuffer(RenderPolygonMemory);
        Ctx.DestroyBuffer(MetaUniformMemory);

        for (int i = 0; i < 2; i++)
            Ctx.DestroyImage(ClearBitmapImg[i]);
        Ctx.DestroyImage(DummyTexture);
        Ctx.DestroyImage(DummyCapture);

        for (int i = 0; i < 9; i++)
            if (Samplers[i]) VK::vkDestroySampler(Ctx.Device, Samplers[i], nullptr);
        if (ClearBitmapSampler) VK::vkDestroySampler(Ctx.Device, ClearBitmapSampler, nullptr);
        if (CaptureSampler) VK::vkDestroySampler(Ctx.Device, CaptureSampler, nullptr);

        if (FrameFence) VK::vkDestroyFence(Ctx.Device, FrameFence, nullptr);
        if (DescPoolFrame) VK::vkDestroyDescriptorPool(Ctx.Device, DescPoolFrame, nullptr);
        if (DescPoolStatic) VK::vkDestroyDescriptorPool(Ctx.Device, DescPoolStatic, nullptr);
        if (PipelineLayout) VK::vkDestroyPipelineLayout(Ctx.Device, PipelineLayout, nullptr);
        if (SetLayoutStatic) VK::vkDestroyDescriptorSetLayout(Ctx.Device, SetLayoutStatic, nullptr);
        if (SetLayoutTexture) VK::vkDestroyDescriptorSetLayout(Ctx.Device, SetLayoutTexture, nullptr);

        Ctx.Deinit();
    }

    if (OutputGLTex)
        glDeleteTextures(1, &OutputGLTex);

    delete[] ClearBitmap[0];
    delete[] ClearBitmap[1];
}

void ComputeRenderer3D_Vulkan::DeleteShaders()
{
    VkPipeline* allPipelines[] =
    {
        &ShaderInterpXSpans[0],
        &ShaderInterpXSpans[1],
        &ShaderBinCombined,
        &ShaderDepthBlend[0],
        &ShaderDepthBlend[1],
        &ShaderRasteriseNoTexture[0],
        &ShaderRasteriseNoTexture[1],
        &ShaderRasteriseNoTextureToon[0],
        &ShaderRasteriseNoTextureToon[1],
        &ShaderRasteriseNoTextureHighlight[0],
        &ShaderRasteriseNoTextureHighlight[1],
        &ShaderRasteriseUseTextureDecal[0],
        &ShaderRasteriseUseTextureDecal[1],
        &ShaderRasteriseUseTextureModulate[0],
        &ShaderRasteriseUseTextureModulate[1],
        &ShaderRasteriseUseTextureToon[0],
        &ShaderRasteriseUseTextureToon[1],
        &ShaderRasteriseUseTextureHighlight[0],
        &ShaderRasteriseUseTextureHighlight[1],
        &ShaderRasteriseShadowMask[0],
        &ShaderRasteriseShadowMask[1],
        &ShaderClearCoarseBinMask,
        &ShaderClearIndirectWorkCount,
        &ShaderCalculateWorkListOffset,
        &ShaderSortWork,
        &ShaderFinalPass[0],
        &ShaderFinalPass[1],
        &ShaderFinalPass[2],
        &ShaderFinalPass[3],
        &ShaderFinalPass[4],
        &ShaderFinalPass[5],
        &ShaderFinalPass[6],
        &ShaderFinalPass[7],
    };
    for (VkPipeline* pipeline : allPipelines)
    {
        if (*pipeline)
        {
            VK::vkDestroyPipeline(Ctx.Device, *pipeline, nullptr);
            *pipeline = VK_NULL_HANDLE;
        }
    }
}

void ComputeRenderer3D_Vulkan::DestroyScaleDependentResources()
{
    Ctx.DestroyBuffer(YSpanIndicesMemory);
    Ctx.DestroyBuffer(XSpanSetupMemory);
    Ctx.DestroyBuffer(BinResultMemory);
    Ctx.DestroyBuffer(WorkDescMemory);
    Ctx.DestroyBuffer(FinalTileMemory);
    for (int i = 0; i < tilememoryLayer_Num; i++)
        Ctx.DestroyBuffer(TileMemory[i]);
    Ctx.DestroyBuffer(ReadbackBuffer);
    Ctx.DestroyImage(FramebufferImg);
}

void ComputeRenderer3D_Vulkan::Reset()
{
    if (Ctx.Valid)
        VK::vkDeviceWaitIdle(Ctx.Device);
    Texcache.Reset();
    ClearBitmapDirty = 0x3;
}

void ComputeRenderer3D_Vulkan::UpdateStaticDescriptorSet()
{
    VkDescriptorBufferInfo bufferInfos[11] = {};
    auto bufInfo = [&](u32 slot, VK::Context::Buffer& buf) -> VkDescriptorBufferInfo*
    {
        bufferInfos[slot] = {buf.Buf, 0, VK_WHOLE_SIZE};
        return &bufferInfos[slot];
    };

    VkDescriptorImageInfo imageInfos[5] = {};

    VkWriteDescriptorSet writes[16] = {};
    auto write = [&](u32 binding, VkDescriptorType type, VkDescriptorBufferInfo* buf, VkDescriptorImageInfo* img)
    {
        VkWriteDescriptorSet& w = writes[binding];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = SetStatic;
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = type;
        w.pBufferInfo = buf;
        w.pImageInfo = img;
    };

    write(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufInfo(0, MetaUniformMemory), nullptr);
    write(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(1, RenderPolygonMemory), nullptr);
    write(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(2, XSpanSetupMemory), nullptr);
    write(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(3, YSpanSetupMemory), nullptr);
    write(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(4, BinResultMemory), nullptr);
    write(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(5, WorkDescMemory), nullptr);
    write(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(6, TileMemory[tilememoryLayer_Color]), nullptr);
    write(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(7, TileMemory[tilememoryLayer_Depth]), nullptr);
    write(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(8, TileMemory[tilememoryLayer_Attr]), nullptr);
    write(9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(9, FinalTileMemory), nullptr);
    write(10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufInfo(10, YSpanIndicesMemory), nullptr);

    imageInfos[0] = {VK_NULL_HANDLE, FramebufferImg.View, VK_IMAGE_LAYOUT_GENERAL};
    write(11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &imageInfos[0]);

    imageInfos[1] = {ClearBitmapSampler, ClearBitmapImg[0].View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    write(12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &imageInfos[1]);
    imageInfos[2] = {ClearBitmapSampler, ClearBitmapImg[1].View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    write(13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &imageInfos[2]);

    imageInfos[3] = {CaptureSampler, DummyCapture.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    write(14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &imageInfos[3]);
    imageInfos[4] = {CaptureSampler, DummyCapture.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    write(15, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &imageInfos[4]);

    VK::vkUpdateDescriptorSets(Ctx.Device, 16, writes, 0, nullptr);
}

VkDescriptorSet ComputeRenderer3D_Vulkan::GetTextureDescriptorSet(VkImageView view, VkSampler sampler)
{
    u64 key = (u64)(uintptr_t)view * 31 + (u64)(uintptr_t)sampler;
    auto it = FrameTextureSets.find(key);
    if (it != FrameTextureSets.end())
        return it->second;

    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = DescPoolFrame;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &SetLayoutTexture;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (VK::vkAllocateDescriptorSets(Ctx.Device, &allocInfo, &set) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    VkDescriptorImageInfo imageInfo = {sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeSet.dstSet = set;
    writeSet.dstBinding = 0;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeSet.pImageInfo = &imageInfo;
    VK::vkUpdateDescriptorSets(Ctx.Device, 1, &writeSet, 0, nullptr);

    FrameTextureSets.emplace(key, set);
    return set;
}

void ComputeRenderer3D_Vulkan::ComputeToComputeBarrier(bool indirect)
{
    VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (indirect)
    {
        barrier.dstAccessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        dstStages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }
    VK::vkCmdPipelineBarrier(FrameCmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, dstStages,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void ComputeRenderer3D_Vulkan::SetRenderSettings(int scale, bool highResolutionCoordinates)
{
    if (!Ctx.Valid)
        return;

    u8 TileScale;

    VK::vkDeviceWaitIdle(Ctx.Device);

    if (ScaleFactor != -1)
    {
        DeleteShaders();
        DestroyScaleDependentResources();
    }

    ShaderStepIdx = 0;

    ScaleFactor = scale;
    ScreenWidth = 256 * ScaleFactor;
    ScreenHeight = 192 * ScaleFactor;

    //Starting at 4.5x we want to double TileSize every time scale doubles
    TileScale = 2 * ScaleFactor / 9;
    TileScale = GetMSBit(TileScale);
    TileScale <<= 1;
    TileScale += TileScale == 0;

    TileSize = std::min(8 * TileScale, 32);
    CoarseTileCountY = TileSize < 32 ? 4 : 6;
    ClearCoarseBinMaskLocalSize = TileSize < 32 ? 64 : 48;
    CoarseTileArea = CoarseTileCountX * CoarseTileCountY;
    CoarseTileW = CoarseTileCountX * TileSize;
    CoarseTileH = CoarseTileCountY * TileSize;

    TilesPerLine = ScreenWidth/TileSize;
    TileLines = ScreenHeight/TileSize;

    HiresCoordinates = highResolutionCoordinates;

    MaxWorkTiles = TilesPerLine*TileLines*16;

    for (int i = 0; i < tilememoryLayer_Num; i++)
        Ctx.CreateBuffer(TileMemory[i], (VkDeviceSize)4*TileSize*TileSize*MaxWorkTiles,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

    Ctx.CreateBuffer(FinalTileMemory, (VkDeviceSize)4*3*2*ScreenWidth*ScreenHeight,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

    VkDeviceSize binResultSize = sizeof(BinResultHeader)
        + (VkDeviceSize)TilesPerLine*TileLines*CoarseBinStride*4 // BinnedMaskCoarse
        + (VkDeviceSize)TilesPerLine*TileLines*BinStride*4 // BinnedMask
        + (VkDeviceSize)TilesPerLine*TileLines*BinStride*4; // WorkOffsets
    Ctx.CreateBuffer(BinResultMemory, binResultSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, false);

    Ctx.CreateBuffer(WorkDescMemory, (VkDeviceSize)MaxWorkTiles*2*4*2,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

    // eh those are pretty bad guesses
    // though real hw shouldn't be eable to render all 2048 polygons on every line either
    int maxYSpanIndices = 64*2048 * ScaleFactor;
    YSpanIndices.resize(maxYSpanIndices);

    Ctx.CreateBuffer(YSpanIndicesMemory, (VkDeviceSize)maxYSpanIndices*2*4,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    Ctx.CreateBuffer(XSpanSetupMemory, (VkDeviceSize)sizeof(SpanSetupX)*maxYSpanIndices,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false);

    Ctx.CreateImage(FramebufferImg, VK_FORMAT_R8G8B8A8_UNORM, ScreenWidth, ScreenHeight, 1,
                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    Ctx.CreateBuffer(ReadbackBuffer, (VkDeviceSize)ScreenWidth*ScreenHeight*4,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);

    // storage images live in GENERAL layout
    {
        VkCommandBuffer cmd = Ctx.BeginOneShot();
        Ctx.TransitionImage(cmd, FramebufferImg, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        Ctx.EndOneShot(cmd);
    }

    UpdateStaticDescriptorSet();

    if (VulkanNativeOutput)
    {
        // the all-Vulkan parent samples FramebufferImg directly; leave it in
        // SHADER_READ_ONLY_OPTIMAL so the first frame's barrier is a no-op
        VkCommandBuffer cmd = Ctx.BeginOneShot();
        Ctx.TransitionImage(cmd, FramebufferImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        Ctx.EndOneShot(cmd);
        return;
    }

    // (re)create the GL texture the compositor reads the 3D layer from
    if (OutputGLTex)
        glDeleteTextures(1, &OutputGLTex);
    glGenTextures(1, &OutputGLTex);
    glBindTexture(GL_TEXTURE_2D, OutputGLTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ScreenWidth, ScreenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    if (Parent)
        Parent->OutputTex3D = OutputGLTex;
}

void ComputeRenderer3D_Vulkan::SetupAttrs(SpanSetupY* span, Polygon* poly, int from, int to)
{
    span->Z0 = poly->FinalZ[from];
    span->W0 = poly->FinalW[from];
    span->Z1 = poly->FinalZ[to];
    span->W1 = poly->FinalW[to];
    span->ColorR0 = poly->Vertices[from]->FinalColor[0];
    span->ColorG0 = poly->Vertices[from]->FinalColor[1];
    span->ColorB0 = poly->Vertices[from]->FinalColor[2];
    span->ColorR1 = poly->Vertices[to]->FinalColor[0];
    span->ColorG1 = poly->Vertices[to]->FinalColor[1];
    span->ColorB1 = poly->Vertices[to]->FinalColor[2];
    span->TexcoordU0 = poly->Vertices[from]->TexCoords[0];
    span->TexcoordV0 = poly->Vertices[from]->TexCoords[1];
    span->TexcoordU1 = poly->Vertices[to]->TexCoords[0];
    span->TexcoordV1 = poly->Vertices[to]->TexCoords[1];
}

void ComputeRenderer3D_Vulkan::SetupYSpanDummy(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int vertex, int side, s32 positions[10][2])
{
    s32 x0 = positions[vertex][0];
    if (side)
    {
        span->DxInitial = -0x40000;
        x0--;
    }
    else
    {
        span->DxInitial = 0;
    }

    span->X0 = span->X1 = x0;
    span->XMin = x0;
    span->XMax = x0;
    span->Y0 = span->Y1 = positions[vertex][1];

    if (span->XMin < rp->XMin)
    {
        rp->XMin = span->XMin;
        rp->XMinY = span->Y0;
    }
    if (span->XMax > rp->XMax)
    {
        rp->XMax = span->XMax;
        rp->XMaxY = span->Y0;
    }

    span->Increment = 0;

    span->I0 = span->I1 = span->IRecip = 0;
    span->Linear = true;

    span->XCovIncr = 0;

    span->IsDummy = true;

    SetupAttrs(span, poly, vertex, vertex);
}

void ComputeRenderer3D_Vulkan::SetupYSpan(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int from, int to, int side, s32 positions[10][2])
{
    span->X0 = positions[from][0];
    span->X1 = positions[to][0];
    span->Y0 = positions[from][1];
    span->Y1 = positions[to][1];

    SetupAttrs(span, poly, from, to);

    s32 minXY, maxXY;
    bool negative = false;
    if (span->X1 > span->X0)
    {
        span->XMin = span->X0;
        span->XMax = span->X1-1;

        minXY = span->Y0;
        maxXY = span->Y1;
    }
    else if (span->X1 < span->X0)
    {
        span->XMin = span->X1;
        span->XMax = span->X0-1;
        negative = true;

        minXY = span->Y1;
        maxXY = span->Y0;
    }
    else
    {
        span->XMin = span->X0;
        if (side) span->XMin--;
        span->XMax = span->XMin;

        // doesn't matter for completely vertical slope
        minXY = span->Y0;
        maxXY = span->Y0;
    }

    if (span->XMin < rp->XMin)
    {
        rp->XMin = span->XMin;
        rp->XMinY = minXY;
    }
    if (span->XMax > rp->XMax)
    {
        rp->XMax = span->XMax;
        rp->XMaxY = maxXY;
    }

    span->IsDummy = false;

    s32 xlen = span->XMax+1 - span->XMin;
    s32 ylen = span->Y1 - span->Y0;

    // slope increment has a 18-bit fractional part
    // note: for some reason, x/y isn't calculated directly,
    // instead, 1/y is calculated and then multiplied by x
    // TODO: this is still not perfect (see for example x=169 y=33)
    if (ylen == 0)
    {
        span->Increment = 0;
    }
    else if (ylen == xlen)
    {
        span->Increment = 0x40000;
    }
    else
    {
        s32 yrecip = (1<<18) / ylen;
        span->Increment = (span->X1-span->X0) * yrecip;
        if (span->Increment < 0) span->Increment = -span->Increment;
    }

    bool xMajor = (span->Increment > 0x40000);

    if (side)
    {
        // right

        if (xMajor)
            span->DxInitial = negative ? (0x20000 + 0x40000) : (span->Increment - 0x20000);
        else if (span->Increment != 0)
            span->DxInitial = negative ? 0x40000 : 0;
        else
            span->DxInitial = -0x40000;
    }
    else
    {
        // left

        if (xMajor)
            span->DxInitial = negative ? ((span->Increment - 0x20000) + 0x40000) : 0x20000;
        else if (span->Increment != 0)
            span->DxInitial = negative ? 0x40000 : 0;
        else
            span->DxInitial = 0;
    }

    if (xMajor)
    {
        if (side)
        {
            span->I0 = span->X0 - 1;
            span->I1 = span->X1 - 1;
        }
        else
        {
            span->I0 = span->X0;
            span->I1 = span->X1;
        }

        // used for calculating AA coverage
        span->XCovIncr = (ylen << 10) / xlen;
    }
    else
    {
        span->I0 = span->Y0;
        span->I1 = span->Y1;
    }

    if (span->I0 != span->I1)
        span->IRecip = (1<<30) / (span->I1 - span->I0);
    else
        span->IRecip = 0;

    span->Linear = (span->W0 == span->W1) && !(span->W0 & 0x7E) && !(span->W1 & 0x7E);

    if ((span->W0 & 0x1) && !(span->W1 & 0x1))
    {
        span->W0n = (span->W0 - 1) >> 1;
        span->W0d = (span->W0 + 1) >> 1;
        span->W1d = span->W1 >> 1;
    }
    else
    {
        span->W0n = span->W0 >> 1;
        span->W0d = span->W0 >> 1;
        span->W1d = span->W1 >> 1;
    }
}

struct VulkanRenderVariant
{
    VulkanTexArray* Texture;
    u32 Sampler;
    u16 Width, Height;
    u8 BlendMode;
    int CaptureYOffset;

    bool operator==(const VulkanRenderVariant& other)
    {
        return Texture == other.Texture && Sampler == other.Sampler && BlendMode == other.BlendMode &&
               CaptureYOffset == other.CaptureYOffset;
    }
};

void ComputeRenderer3D_Vulkan::RenderFrame()
{
    assert(!NeedsShaderCompile());

    u8 clrBitmapDirty;
    if (!Texcache.Update(clrBitmapDirty) && GPU3D.RenderFrameIdentical)
    {
        return;
    }

    // figure out which chunks of texture memory contain display captures
    int captureinfo[16];
    GPU.GetCaptureInfo_Texture(captureinfo);

    // if we're using a clear bitmap, set that up
    ClearBitmapDirty |= clrBitmapDirty;
    if (GPU3D.RenderDispCnt & (1<<14))
    {
        if (ClearBitmapDirty & (1<<0))
        {
            u16* vram = (u16*)&GPU.VRAMFlat_Texture[0x40000];
            for (int i = 0; i < 256*256; i++)
            {
                u16 color = vram[i];
                u32 r = (color << 1) & 0x3E; if (r) r++;
                u32 g = (color >> 4) & 0x3E; if (g) g++;
                u32 b = (color >> 9) & 0x3E; if (b) b++;
                u32 a = (color & 0x8000) ? 31 : 0;

                ClearBitmap[0][i] = r | (g << 8) | (b << 16) | (a << 24);
            }

            Ctx.UploadImageLayer(ClearBitmapImg[0], ClearBitmap[0], 256, 256, 0, 4);
        }

        if (ClearBitmapDirty & (1<<1))
        {
            u16* vram = (u16*)&GPU.VRAMFlat_Texture[0x60000];
            for (int i = 0; i < 256*256; i++)
            {
                u16 val = vram[i];
                u32 depth = ((val & 0x7FFF) * 0x200) + 0x1FF;
                u32 fog = (val & 0x8000) << 9;

                ClearBitmap[1][i] = depth | fog;
            }

            Ctx.UploadImageLayer(ClearBitmapImg[1], ClearBitmap[1], 256, 256, 0, 4);
        }

        ClearBitmapDirty = 0;
    }

    int numYSpans = 0;
    int numSetupIndices = 0;

    u32 numVariants = 0, prevVariant, prevTexLayer;
    VulkanRenderVariant variants[MaxVariants];
    u32 capLastVariant[16] = {0};

    bool enableTextureMaps = GPU3D.RenderDispCnt & (1<<0);

    for (int i = 0; i < GPU3D.RenderNumPolygons; i++)
    {
        Polygon* polygon = GPU3D.RenderPolygonRAM[i];

        u32 nverts = polygon->NumVertices;
        u32 vtop = polygon->VTop, vbot = polygon->VBottom;

        u32 curVL = vtop, curVR = vtop;
        u32 nextVL, nextVR;

        RenderPolygons[i].FirstXSpan = numSetupIndices;
        RenderPolygons[i].Attr = polygon->Attr;

        bool foundVariant = false;
        if (i > 0)
        {
            // if the whole texture attribute matches
            // the texture layer will also match
            Polygon* prevPolygon = GPU3D.RenderPolygonRAM[i - 1];
            foundVariant = prevPolygon->TexParam == polygon->TexParam
                && prevPolygon->TexPalette == polygon->TexPalette
                && (prevPolygon->Attr & 0x30) == (polygon->Attr & 0x30)
                && prevPolygon->IsShadowMask == polygon->IsShadowMask;
        }

        if (!foundVariant)
        {
            VulkanRenderVariant variant;
            variant.BlendMode = polygon->IsShadowMask ? 4 : ((polygon->Attr >> 4) & 0x3);
            variant.Texture = nullptr;
            variant.Sampler = 0;
            variant.CaptureYOffset = -1;
            u32* textureLastVariant = nullptr;
            // we always need to look up the texture to get the layer of the array texture
            u32 textype = (polygon->TexParam >> 26) & 0x7;
            if (enableTextureMaps && textype)
            {
                u32 texaddr = polygon->TexParam & 0xFFFF;
                u32 texwidth = TextureWidth(polygon->TexParam);
                u32 texheight = TextureHeight(polygon->TexParam);
                int capblock = -1;
                if ((textype == 7) && ((texwidth == 128) || (texwidth == 256)))
                {
                    // if this is a direct color texture, and the width is 128 or 256
                    // then it might be a display capture
                    u32 startaddr = texaddr << 3;
                    u32 endaddr = startaddr + (texheight * texwidth * 2);

                    startaddr >>= 15;
                    endaddr = (endaddr + 0x7FFF) >> 15;

                    for (u32 b = startaddr; b < endaddr; b++)
                    {
                        int blk = captureinfo[b];
                        if (blk == -1) continue;

                        capblock = blk;
                    }
                }

                if (capblock != -1)
                {
                    // TODO: display captures as textures aren't wired up to the
                    // GL compositor's capture output yet; a transparent dummy
                    // texture is sampled instead
                    if (texwidth == 128)
                    {
                        variant.Texture = kCaptureTex128;
                        variant.CaptureYOffset = (int)((texaddr >> 5) & 0x7F);
                        prevTexLayer = capblock;
                    }
                    else
                    {
                        variant.Texture = kCaptureTex256;
                        variant.CaptureYOffset = (int)((texaddr >> 6) & 0xFF);
                        prevTexLayer = capblock >> 2;
                    }

                    textureLastVariant = &capLastVariant[capblock];
                }
                else
                {
                    Texcache.GetTexture(polygon->TexParam, polygon->TexPalette, variant.Texture, prevTexLayer, textureLastVariant);
                    variant.CaptureYOffset = -1;
                }

                bool wrapS = (polygon->TexParam >> 16) & 1;
                bool wrapT = (polygon->TexParam >> 17) & 1;
                bool mirrorS = (polygon->TexParam >> 18) & 1;
                bool mirrorT = (polygon->TexParam >> 19) & 1;
                variant.Sampler = (wrapS ? (mirrorS ? 2 : 1) : 0) + (wrapT ? (mirrorT ? 2 : 1) : 0) * 3;

                if (*textureLastVariant < numVariants && variants[*textureLastVariant] == variant)
                {
                    foundVariant = true;
                    prevVariant = *textureLastVariant;
                }
            }

            if (!foundVariant)
            {
                for (int j = numVariants - 1; j >= 0; j--)
                {
                    if (variants[j] == variant)
                    {
                        foundVariant = true;
                        prevVariant = j;
                        goto foundVariant;
                    }
                }

                prevVariant = numVariants;
                variants[numVariants] = variant;
                variants[numVariants].Width = TextureWidth(polygon->TexParam);
                variants[numVariants].Height = TextureHeight(polygon->TexParam);
                numVariants++;
                assert(numVariants <= MaxVariants);
            foundVariant:;

                if (textureLastVariant)
                    *textureLastVariant = prevVariant;
            }
        }
        RenderPolygons[i].Variant = prevVariant;
        RenderPolygons[i].TextureLayer = (float)prevTexLayer;

        if (polygon->FacingView)
        {
            nextVL = curVL + 1;
            if (nextVL >= nverts) nextVL = 0;
            nextVR = curVR - 1;
            if ((s32)nextVR < 0) nextVR = nverts - 1;
        }
        else
        {
            nextVL = curVL - 1;
            if ((s32)nextVL < 0) nextVL = nverts - 1;
            nextVR = curVR + 1;
            if (nextVR >= nverts) nextVR = 0;
        }

        s32 scaledPositions[10][2];
        s32 ytop = ScreenHeight, ybot = 0;
        for (int j = 0; j < polygon->NumVertices; j++)
        {
            if (HiresCoordinates)
            {
                scaledPositions[j][0] = (polygon->Vertices[j]->HiresPosition[0] * ScaleFactor) >> 4;
                scaledPositions[j][1] = (polygon->Vertices[j]->HiresPosition[1] * ScaleFactor) >> 4;
            }
            else
            {
                scaledPositions[j][0] = polygon->Vertices[j]->FinalPosition[0] * ScaleFactor;
                scaledPositions[j][1] = polygon->Vertices[j]->FinalPosition[1] * ScaleFactor;
            }
            ytop = std::min(scaledPositions[j][1], ytop);
            ybot = std::max(scaledPositions[j][1], ybot);
        }
        RenderPolygons[i].YTop = ytop;
        RenderPolygons[i].YBot = ybot;
        RenderPolygons[i].XMin = ScreenWidth;
        RenderPolygons[i].XMax = 0;

        if (ybot == ytop)
        {
            vtop = 0; vbot = 0;

            RenderPolygons[i].YBot++;

            int j = 1;
            if (scaledPositions[j][0] < scaledPositions[vtop][0]) vtop = j;
            if (scaledPositions[j][0] > scaledPositions[vbot][0]) vbot = j;

            j = nverts - 1;
            if (scaledPositions[j][0] < scaledPositions[vtop][0]) vtop = j;
            if (scaledPositions[j][0] > scaledPositions[vbot][0]) vbot = j;

            assert(numYSpans < MaxYSpanSetups);
            u32 curSpanL = numYSpans;
            SetupYSpanDummy(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, vtop, 0, scaledPositions);
            assert(numYSpans < MaxYSpanSetups);
            u32 curSpanR = numYSpans;
            SetupYSpanDummy(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, vbot, 1, scaledPositions);

            YSpanIndices[numSetupIndices].PolyIdx = i;
            YSpanIndices[numSetupIndices].SpanIdxL = curSpanL;
            YSpanIndices[numSetupIndices].SpanIdxR = curSpanR;
            YSpanIndices[numSetupIndices].Y = ytop;
            numSetupIndices++;
        }
        else
        {
            u32 curSpanL = numYSpans;
            assert(numYSpans < MaxYSpanSetups);
            SetupYSpan(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, curVL, nextVL, 0, scaledPositions);
            u32 curSpanR = numYSpans;
            assert(numYSpans < MaxYSpanSetups);
            SetupYSpan(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, curVR, nextVR, 1, scaledPositions);

            for (u32 y = ytop; y < ybot; y++)
            {
                if (y >= scaledPositions[nextVL][1] && curVL != polygon->VBottom)
                {
                    while (y >= scaledPositions[nextVL][1] && curVL != polygon->VBottom)
                    {
                        curVL = nextVL;
                        if (polygon->FacingView)
                        {
                            nextVL = curVL + 1;
                            if (nextVL >= nverts)
                                nextVL = 0;
                        }
                        else
                        {
                            nextVL = curVL - 1;
                            if ((s32)nextVL < 0)
                                nextVL = nverts - 1;
                        }
                    }


                    assert(numYSpans < MaxYSpanSetups);
                    curSpanL = numYSpans;
                    SetupYSpan(&RenderPolygons[i], &YSpanSetups[numYSpans++], polygon, curVL, nextVL, 0, scaledPositions);
                }
                if (y >= scaledPositions[nextVR][1] && curVR != polygon->VBottom)
                {
                    while (y >= scaledPositions[nextVR][1] && curVR != polygon->VBottom)
                    {
                        curVR = nextVR;
                        if (polygon->FacingView)
                        {
                            nextVR = curVR - 1;
                            if ((s32)nextVR < 0)
                                nextVR = nverts - 1;
                        }
                        else
                        {
                            nextVR = curVR + 1;
                            if (nextVR >= nverts)
                                nextVR = 0;
                        }
                    }

                    assert(numYSpans < MaxYSpanSetups);
                    curSpanR = numYSpans;
                    SetupYSpan(&RenderPolygons[i] ,&YSpanSetups[numYSpans++], polygon, curVR, nextVR, 1, scaledPositions);
                }

                YSpanIndices[numSetupIndices].PolyIdx = i;
                YSpanIndices[numSetupIndices].SpanIdxL = curSpanL;
                YSpanIndices[numSetupIndices].SpanIdxR = curSpanR;
                YSpanIndices[numSetupIndices].Y = y;
                numSetupIndices++;
            }
        }
    }

    // upload geometry data
    if (numYSpans > 0)
    {
        memcpy(YSpanSetupMemory.Map, YSpanSetups, sizeof(SpanSetupY)*numYSpans);
        memcpy(YSpanIndicesMemory.Map, YSpanIndices.data(), (size_t)numSetupIndices*sizeof(SetupIndices));
        memcpy(RenderPolygonMemory.Map, RenderPolygons, GPU3D.RenderNumPolygons*sizeof(RenderPolygon));
    }

    // build the meta uniform
    MetaUniform meta;
    meta.DispCnt = GPU3D.RenderDispCnt;
    meta.NumPolygons = GPU3D.RenderNumPolygons;
    meta.NumVariants = numVariants;
    meta.AlphaRef = GPU3D.RenderAlphaRef;
    {
        u32 r = (GPU3D.RenderClearAttr1 << 1) & 0x3E; if (r) r++;
        u32 g = (GPU3D.RenderClearAttr1 >> 4) & 0x3E; if (g) g++;
        u32 b = (GPU3D.RenderClearAttr1 >> 9) & 0x3E; if (b) b++;
        u32 a = (GPU3D.RenderClearAttr1 >> 16) & 0x1F;
        meta.ClearColor = r | (g << 8) | (b << 16) | (a << 24);
        meta.ClearDepth = ((GPU3D.RenderClearAttr2 & 0x7FFF) * 0x200) + 0x1FF;
        meta.ClearAttr = GPU3D.RenderClearAttr1 & 0x3F008000;

        u8 xoff = (GPU3D.RenderClearAttr2 >> 16) & 0xFF;
        u8 yoff = (GPU3D.RenderClearAttr2 >> 24) & 0xFF;
        meta.ClearBitmapOffset[0] = (float)xoff / 256.0f;
        meta.ClearBitmapOffset[1] = (float)yoff / 256.0f;
    }
    for (u32 i = 0; i < 32; i++)
    {
        u32 color = GPU3D.RenderToonTable[i];
        u32 r = (color << 1) & 0x3E;
        u32 g = (color >> 4) & 0x3E;
        u32 b = (color >> 9) & 0x3E;
        if (r) r++;
        if (g) g++;
        if (b) b++;

        meta.ToonTable[i*4+0] = r | (g << 8) | (b << 16);
    }
    for (u32 i = 0; i < 34; i++)
    {
        meta.ToonTable[i*4+1] = GPU3D.RenderFogDensityTable[i];
    }
    for (u32 i = 0; i < 8; i++)
    {
        u32 color = GPU3D.RenderEdgeTable[i];
        u32 r = (color << 1) & 0x3E;
        u32 g = (color >> 4) & 0x3E;
        u32 b = (color >> 9) & 0x3E;
        if (r) r++;
        if (g) g++;
        if (b) b++;

        meta.ToonTable[i*4+2] = r | (g << 8) | (b << 16);
    }
    meta.FogOffset = GPU3D.RenderFogOffset;
    meta.FogShift = GPU3D.RenderFogShift;
    {
        u32 fogR = (GPU3D.RenderFogColor << 1) & 0x3E; if (fogR) fogR++;
        u32 fogG = (GPU3D.RenderFogColor >> 4) & 0x3E; if (fogG) fogG++;
        u32 fogB = (GPU3D.RenderFogColor >> 9) & 0x3E; if (fogB) fogB++;
        u32 fogA = (GPU3D.RenderFogColor >> 16) & 0x1F;
        meta.FogColor = fogR | (fogG << 8) | (fogB << 16) | (fogA << 24);
    }
    memcpy(MetaUniformMemory.Map, &meta, sizeof(MetaUniform));

    // fresh descriptor sets for this frame's textures
    VK::vkResetDescriptorPool(Ctx.Device, DescPoolFrame, 0);
    FrameTextureSets.clear();
    VkDescriptorSet dummyTextureSet = GetTextureDescriptorSet(DummyTexture.View, Samplers[0]);

    // record the frame
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK::vkBeginCommandBuffer(FrameCmd, &beginInfo);

    if (VulkanNativeOutput)
    {
        // the storage-image final pass needs GENERAL; the 2D compositor left
        // it in SHADER_READ_ONLY after sampling last frame's output
        Ctx.TransitionImage(FrameCmd, FramebufferImg, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
    }

    VK::vkCmdBindDescriptorSets(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout,
        0, 1, &SetStatic, 0, nullptr);
    VK::vkCmdBindDescriptorSets(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout,
        1, 1, &dummyTextureSet, 0, nullptr);

    VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderClearCoarseBinMask);
    VK::vkCmdDispatch(FrameCmd, TilesPerLine*TileLines/ClearCoarseBinMaskLocalSize, 1, 1);

    bool wbuffer = false;
    if (numYSpans > 0)
    {
        wbuffer = GPU3D.RenderPolygonRAM[0]->WBuffer;

        VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderClearIndirectWorkCount);
        VK::vkCmdDispatch(FrameCmd, (numVariants+31)/32, 1, 1);

        // calculate x-spans
        VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderInterpXSpans[wbuffer]);
        VK::vkCmdDispatch(FrameCmd, (numSetupIndices + 31) / 32, 1, 1);
        ComputeToComputeBarrier(false);

        // bin polygons
        VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderBinCombined);
        VK::vkCmdDispatch(FrameCmd, ((GPU3D.RenderNumPolygons + 31) / 32), ScreenWidth/CoarseTileW, ScreenHeight/CoarseTileH);
        ComputeToComputeBarrier(false);

        // calculate list offsets
        VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderCalculateWorkListOffset);
        VK::vkCmdDispatch(FrameCmd, (numVariants + 31) / 32, 1, 1);
        ComputeToComputeBarrier(true);

        // sort shader work
        VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderSortWork);
        VK::vkCmdDispatchIndirect(FrameCmd, BinResultMemory.Buf, offsetof(BinResultHeader, SortWorkWorkCount));
        ComputeToComputeBarrier(true);

        // rasterise
        {
            bool highLightMode = GPU3D.RenderDispCnt & (1<<1);

            VkPipeline shadersNoTexture[] =
            {
                ShaderRasteriseNoTexture[wbuffer],
                ShaderRasteriseNoTexture[wbuffer],
                highLightMode
                    ? ShaderRasteriseNoTextureHighlight[wbuffer]
                    : ShaderRasteriseNoTextureToon[wbuffer],
                ShaderRasteriseNoTexture[wbuffer],
                ShaderRasteriseShadowMask[wbuffer]
            };
            VkPipeline shadersUseTexture[] =
            {
                ShaderRasteriseUseTextureModulate[wbuffer],
                ShaderRasteriseUseTextureDecal[wbuffer],
                highLightMode
                    ? ShaderRasteriseUseTextureHighlight[wbuffer]
                    : ShaderRasteriseUseTextureToon[wbuffer],
                ShaderRasteriseUseTextureDecal[wbuffer],
                ShaderRasteriseShadowMask[wbuffer]
            };

            VkPipeline prevShader = VK_NULL_HANDLE;
            VkDescriptorSet prevTexSet = dummyTextureSet;
            for (u32 i = 0; i < numVariants; i++)
            {
                VkPipeline shader;
                VkDescriptorSet texSet = dummyTextureSet;

                RasterPushConstants pc;
                pc.CurVariant = i;
                pc.TexIsCapture = 0;
                pc.InvTextureSize[0] = 1.f / variants[i].Width;
                pc.InvTextureSize[1] = 1.f / variants[i].Height;
                pc.CaptureYOffset = 0.f;

                if (variants[i].Texture == nullptr)
                {
                    shader = shadersNoTexture[variants[i].BlendMode];
                }
                else
                {
                    shader = shadersUseTexture[variants[i].BlendMode];

                    if (variants[i].CaptureYOffset != -1)
                    {
                        pc.TexIsCapture = variants[i].Width == 128 ? 1 : 2;
                        pc.CaptureYOffset = (float)variants[i].CaptureYOffset / (float)variants[i].Height;
                    }
                    else if (variants[i].Texture != kCaptureTex128 && variants[i].Texture != kCaptureTex256)
                    {
                        texSet = GetTextureDescriptorSet(variants[i].Texture->Image.View, Samplers[variants[i].Sampler]);
                        if (texSet == VK_NULL_HANDLE)
                            texSet = dummyTextureSet;
                    }
                }
                assert(shader != VK_NULL_HANDLE);
                if (shader != prevShader)
                {
                    VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, shader);
                    prevShader = shader;
                }
                if (texSet != prevTexSet)
                {
                    VK::vkCmdBindDescriptorSets(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout,
                        1, 1, &texSet, 0, nullptr);
                    prevTexSet = texSet;
                }

                VK::vkCmdPushConstants(FrameCmd, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                    0, sizeof(RasterPushConstants), &pc);
                VK::vkCmdDispatchIndirect(FrameCmd, BinResultMemory.Buf,
                    offsetof(BinResultHeader, VariantWorkCount) + i*4*4);
            }
        }
    }
    ComputeToComputeBarrier(false);

    // compose final image
    VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderDepthBlend[wbuffer]);
    VK::vkCmdDispatch(FrameCmd, ScreenWidth/TileSize, ScreenHeight/TileSize, 1);
    ComputeToComputeBarrier(false);

    u32 finalPassShader = 0;
    if (GPU3D.RenderDispCnt & (1<<4))
        finalPassShader |= 0x4;
    if (GPU3D.RenderDispCnt & (1<<7))
        finalPassShader |= 0x2;
    if (GPU3D.RenderDispCnt & (1<<5))
        finalPassShader |= 0x1;

    VK::vkCmdBindPipeline(FrameCmd, VK_PIPELINE_BIND_POINT_COMPUTE, ShaderFinalPass[finalPassShader]);
    VK::vkCmdDispatch(FrameCmd, ScreenWidth/32, ScreenHeight, 1);

    if (VulkanNativeOutput)
    {
        // leave the output sampleable by the 2D compositor; the parent
        // fence-waits on the same queue before recording its 2D work
        Ctx.TransitionImage(FrameCmd, FramebufferImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

        VK::vkEndCommandBuffer(FrameCmd);

        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &FrameCmd;
        VK::vkQueueSubmit(Ctx.Queue, 1, &submitInfo, FrameFence);

        VK::vkWaitForFences(Ctx.Device, 1, &FrameFence, VK_TRUE, UINT64_MAX);
        VK::vkResetFences(Ctx.Device, 1, &FrameFence);
        return;
    }

    // read the frame back for the GL compositor
    Ctx.TransitionImage(FrameCmd, FramebufferImg, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    VkBufferImageCopy region = {};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {(u32)ScreenWidth, (u32)ScreenHeight, 1};
    VK::vkCmdCopyImageToBuffer(FrameCmd, FramebufferImg.Img, VK_IMAGE_LAYOUT_GENERAL,
        ReadbackBuffer.Buf, 1, &region);

    VkMemoryBarrier hostBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    VK::vkCmdPipelineBarrier(FrameCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
        0, 1, &hostBarrier, 0, nullptr, 0, nullptr);

    VK::vkEndCommandBuffer(FrameCmd);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &FrameCmd;
    VK::vkQueueSubmit(Ctx.Queue, 1, &submitInfo, FrameFence);

    VK::vkWaitForFences(Ctx.Device, 1, &FrameFence, VK_TRUE, UINT64_MAX);
    VK::vkResetFences(Ctx.Device, 1, &FrameFence);

    // hand the finished frame to the GL compositor
    glBindTexture(GL_TEXTURE_2D, OutputGLTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ScreenWidth, ScreenHeight,
        GL_RGBA, GL_UNSIGNED_BYTE, ReadbackBuffer.Map);
}

void ComputeRenderer3D_Vulkan::RestartFrame()
{
}

u32* ComputeRenderer3D_Vulkan::GetLine(int line)
{
    return nullptr;
}

}
