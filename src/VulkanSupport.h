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

#ifndef VULKANSUPPORT_H
#define VULKANSUPPORT_H

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "types.h"

namespace melonDS
{

namespace VK
{

// cheap probe for whether a Vulkan implementation can be loaded at all,
// without creating an instance (used to grey out the renderer in settings)
bool IsRuntimeAvailable();

// The set of Vulkan entry points used by the compute renderer.
// Loaded dynamically so melonDS keeps working on systems without
// a Vulkan implementation (the renderer just reports itself
// unavailable).

#define VK_FOREACH_GLOBAL_FUNC(x) \
    x(vkCreateInstance) \
    x(vkEnumerateInstanceExtensionProperties)

#define VK_FOREACH_INSTANCE_FUNC(x) \
    x(vkDestroyInstance) \
    x(vkEnumeratePhysicalDevices) \
    x(vkGetPhysicalDeviceProperties) \
    x(vkGetPhysicalDeviceFeatures) \
    x(vkGetPhysicalDeviceQueueFamilyProperties) \
    x(vkGetPhysicalDeviceMemoryProperties) \
    x(vkGetPhysicalDeviceFormatProperties) \
    x(vkEnumerateDeviceExtensionProperties) \
    x(vkCreateDevice) \
    x(vkGetDeviceProcAddr)

#define VK_FOREACH_DEVICE_FUNC(x) \
    x(vkDestroyDevice) \
    x(vkGetDeviceQueue) \
    x(vkCreateCommandPool) \
    x(vkDestroyCommandPool) \
    x(vkAllocateCommandBuffers) \
    x(vkFreeCommandBuffers) \
    x(vkResetCommandBuffer) \
    x(vkBeginCommandBuffer) \
    x(vkEndCommandBuffer) \
    x(vkQueueSubmit) \
    x(vkQueueWaitIdle) \
    x(vkDeviceWaitIdle) \
    x(vkCreateFence) \
    x(vkDestroyFence) \
    x(vkWaitForFences) \
    x(vkResetFences) \
    x(vkCreateBuffer) \
    x(vkDestroyBuffer) \
    x(vkGetBufferMemoryRequirements) \
    x(vkAllocateMemory) \
    x(vkFreeMemory) \
    x(vkBindBufferMemory) \
    x(vkMapMemory) \
    x(vkUnmapMemory) \
    x(vkCreateImage) \
    x(vkDestroyImage) \
    x(vkGetImageMemoryRequirements) \
    x(vkBindImageMemory) \
    x(vkCreateImageView) \
    x(vkDestroyImageView) \
    x(vkCreateSampler) \
    x(vkDestroySampler) \
    x(vkCreateShaderModule) \
    x(vkDestroyShaderModule) \
    x(vkCreateDescriptorSetLayout) \
    x(vkDestroyDescriptorSetLayout) \
    x(vkCreateDescriptorPool) \
    x(vkDestroyDescriptorPool) \
    x(vkResetDescriptorPool) \
    x(vkAllocateDescriptorSets) \
    x(vkUpdateDescriptorSets) \
    x(vkCreatePipelineLayout) \
    x(vkDestroyPipelineLayout) \
    x(vkCreateComputePipelines) \
    x(vkDestroyPipeline) \
    x(vkCmdBindPipeline) \
    x(vkCmdBindDescriptorSets) \
    x(vkCmdPushConstants) \
    x(vkCmdDispatch) \
    x(vkCmdDispatchIndirect) \
    x(vkCmdPipelineBarrier) \
    x(vkCmdCopyBuffer) \
    x(vkCmdCopyBufferToImage) \
    x(vkCmdCopyImageToBuffer) \
    x(vkCmdFillBuffer)

#define VK_DECLARE_FUNC(name) extern PFN_##name name;
VK_FOREACH_GLOBAL_FUNC(VK_DECLARE_FUNC)
VK_FOREACH_INSTANCE_FUNC(VK_DECLARE_FUNC)
VK_FOREACH_DEVICE_FUNC(VK_DECLARE_FUNC)
#undef VK_DECLARE_FUNC

class Context
{
public:
    Context() = default;
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    // Loads the Vulkan library, creates instance + device.
    // Returns false (and leaves Valid == false) if no usable
    // Vulkan implementation is present.
    bool Init();
    void Deinit();

    bool Valid = false;

    VkInstance Instance = VK_NULL_HANDLE;
    VkPhysicalDevice PhysDev = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;
    u32 QueueFamily = 0;
    VkQueue Queue = VK_NULL_HANDLE;
    VkCommandPool CmdPool = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties Props = {};
    VkPhysicalDeviceMemoryProperties MemProps = {};

    struct Buffer
    {
        VkBuffer Buf = VK_NULL_HANDLE;
        VkDeviceMemory Mem = VK_NULL_HANDLE;
        void* Map = nullptr;
        VkDeviceSize Size = 0;
    };

    struct Image
    {
        VkImage Img = VK_NULL_HANDLE;
        VkDeviceMemory Mem = VK_NULL_HANDLE;
        VkImageView View = VK_NULL_HANDLE;
        VkFormat Format = VK_FORMAT_UNDEFINED;
        u32 Width = 0, Height = 0, Layers = 0;
        VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    bool CreateBuffer(Buffer& buf, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible);
    void DestroyBuffer(Buffer& buf);

    bool CreateImage(Image& img, VkFormat format, u32 width, u32 height, u32 layers,
                     VkImageUsageFlags usage, bool array2D);
    void DestroyImage(Image& img);

    // records a layout transition into cmd
    void TransitionImage(VkCommandBuffer cmd, Image& img, VkImageLayout newLayout,
                         VkPipelineStageFlags srcStage, VkAccessFlags srcAccess,
                         VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);

    // uploads data into one layer of an image and leaves it in SHADER_READ_ONLY_OPTIMAL
    // (synchronous; intended for cache-miss texture uploads)
    void UploadImageLayer(Image& img, const void* data, u32 width, u32 height, u32 layer, u32 bytesPerPixel);

    VkCommandBuffer BeginOneShot();
    void EndOneShot(VkCommandBuffer cmd); // submits and waits

    // compiles Vulkan-flavoured compute GLSL to SPIR-V and wraps it in a shader module
    bool CompileComputeShader(VkShaderModule& out, const std::string& source, const char* name);

    u32 FindMemoryType(u32 typeBits, VkMemoryPropertyFlags wanted);

private:
    void* LibVulkan = nullptr;
    bool LoadLibrary();
    bool OwnsGlslang = false;
};

}

}

#endif // VULKANSUPPORT_H
