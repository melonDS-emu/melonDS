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

#include "VulkanSupport.h"

#include <string.h>

#include "Platform.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace melonDS
{

namespace VK
{

using Platform::Log;
using Platform::LogLevel;

static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

#define VK_DEFINE_FUNC(name) PFN_##name name = nullptr;
VK_FOREACH_GLOBAL_FUNC(VK_DEFINE_FUNC)
VK_FOREACH_INSTANCE_FUNC(VK_DEFINE_FUNC)
VK_FOREACH_DEVICE_FUNC(VK_DEFINE_FUNC)
#undef VK_DEFINE_FUNC

static void* OpenLibrary()
{
#ifdef _WIN32
    return (void*)LoadLibraryA("vulkan-1.dll");
#elif defined(__APPLE__)
    const char* candidates[] =
    {
        // the copy shipped inside the app bundle wins, so releases behave
        // the same regardless of what is installed on the system
        "@executable_path/../Frameworks/libvulkan.1.dylib",
        "@executable_path/../Frameworks/libMoltenVK.dylib",
        "libvulkan.dylib",
        "libvulkan.1.dylib",
        "libMoltenVK.dylib",
        "/opt/homebrew/lib/libvulkan.1.dylib",
        "/usr/local/lib/libvulkan.1.dylib",
        "/opt/homebrew/lib/libMoltenVK.dylib",
        "/usr/local/lib/libMoltenVK.dylib",
    };
    for (const char* name : candidates)
    {
        if (void* lib = dlopen(name, RTLD_NOW | RTLD_LOCAL))
            return lib;
    }
    return nullptr;
#else
    if (void* lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL))
        return lib;
    return dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#endif
}

static void* GetLibSymbol(void* lib, const char* name)
{
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}

bool IsRuntimeAvailable()
{
    static int available = -1;
    if (available == -1)
    {
        void* lib = OpenLibrary();
        available = (lib && GetLibSymbol(lib, "vkGetInstanceProcAddr")) ? 1 : 0;
        if (lib)
        {
#ifdef _WIN32
            FreeLibrary((HMODULE)lib);
#else
            dlclose(lib);
#endif
        }
    }
    return available == 1;
}

bool Context::LoadLibrary()
{
    if (LibVulkan)
        return true;

    LibVulkan = OpenLibrary();
    if (!LibVulkan)
    {
        Log(LogLevel::Info, "Vulkan: no Vulkan library found\n");
        return false;
    }

    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetLibSymbol(LibVulkan, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr)
    {
        Log(LogLevel::Error, "Vulkan: library has no vkGetInstanceProcAddr\n");
        return false;
    }

#define VK_LOAD_GLOBAL(name) name = (PFN_##name)vkGetInstanceProcAddr(nullptr, #name);
    VK_FOREACH_GLOBAL_FUNC(VK_LOAD_GLOBAL)
#undef VK_LOAD_GLOBAL

    return vkCreateInstance != nullptr;
}

Context::~Context()
{
    Deinit();
}

bool Context::Init()
{
    if (Valid)
        return true;

    if (!LoadLibrary())
        return false;

    // instance

    u32 numInstExts = 0;
    std::vector<VkExtensionProperties> instExts;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &numInstExts, nullptr) == VK_SUCCESS && numInstExts)
    {
        instExts.resize(numInstExts);
        vkEnumerateInstanceExtensionProperties(nullptr, &numInstExts, instExts.data());
    }

    auto hasInstExt = [&](const char* name)
    {
        for (auto& ext : instExts)
            if (!strcmp(ext.extensionName, name)) return true;
        return false;
    };

    std::vector<const char*> instExtNames;
    VkInstanceCreateFlags instFlags = 0;
    if (hasInstExt("VK_KHR_portability_enumeration"))
    {
        // required to enumerate MoltenVK through the Khronos loader
        instExtNames.push_back("VK_KHR_portability_enumeration");
        instFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    if (hasInstExt("VK_KHR_get_physical_device_properties2"))
        instExtNames.push_back("VK_KHR_get_physical_device_properties2");

    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "melonDS";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "melonDS";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instInfo.flags = instFlags;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = (u32)instExtNames.size();
    instInfo.ppEnabledExtensionNames = instExtNames.data();

    VkResult res = vkCreateInstance(&instInfo, nullptr, &Instance);
    if (res != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: vkCreateInstance failed (%d)\n", res);
        return false;
    }

#define VK_LOAD_INSTANCE(name) name = (PFN_##name)vkGetInstanceProcAddr(Instance, #name);
    VK_FOREACH_INSTANCE_FUNC(VK_LOAD_INSTANCE)
#undef VK_LOAD_INSTANCE

    // physical device

    u32 numPhysDevs = 0;
    vkEnumeratePhysicalDevices(Instance, &numPhysDevs, nullptr);
    if (numPhysDevs == 0)
    {
        Log(LogLevel::Error, "Vulkan: no physical devices\n");
        Deinit();
        return false;
    }
    std::vector<VkPhysicalDevice> physDevs(numPhysDevs);
    vkEnumeratePhysicalDevices(Instance, &numPhysDevs, physDevs.data());

    // prefer a discrete GPU, then integrated, then whatever comes first
    PhysDev = physDevs[0];
    int bestScore = -1;
    for (VkPhysicalDevice dev : physDevs)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 2;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 1;
        if (score > bestScore)
        {
            bestScore = score;
            PhysDev = dev;
        }
    }

    vkGetPhysicalDeviceProperties(PhysDev, &Props);
    vkGetPhysicalDeviceMemoryProperties(PhysDev, &MemProps);

    Log(LogLevel::Info, "Vulkan: using device %s (Vulkan %d.%d)\n",
        Props.deviceName,
        VK_VERSION_MAJOR(Props.apiVersion), VK_VERSION_MINOR(Props.apiVersion));

    // queue family with compute + transfer
    u32 numQueueFamilies = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysDev, &numQueueFamilies, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(numQueueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysDev, &numQueueFamilies, queueFamilies.data());

    bool foundQueue = false;
    for (u32 i = 0; i < numQueueFamilies; i++)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            QueueFamily = i;
            foundQueue = true;
            break;
        }
    }
    if (!foundQueue)
    {
        Log(LogLevel::Error, "Vulkan: no compute queue\n");
        Deinit();
        return false;
    }

    // device

    u32 numDevExts = 0;
    std::vector<VkExtensionProperties> devExts;
    vkEnumerateDeviceExtensionProperties(PhysDev, nullptr, &numDevExts, nullptr);
    if (numDevExts)
    {
        devExts.resize(numDevExts);
        vkEnumerateDeviceExtensionProperties(PhysDev, nullptr, &numDevExts, devExts.data());
    }
    auto hasDevExt = [&](const char* name)
    {
        for (auto& ext : devExts)
            if (!strcmp(ext.extensionName, name)) return true;
        return false;
    };

    std::vector<const char*> devExtNames;
    if (hasDevExt("VK_KHR_portability_subset"))
        devExtNames.push_back("VK_KHR_portability_subset");

    VkPhysicalDeviceFeatures supported = {};
    vkGetPhysicalDeviceFeatures(PhysDev, &supported);
    VkPhysicalDeviceFeatures enabled = {};
    // matches the robustness the GL renderer gets from the GL spec:
    // out-of-bounds SSBO accesses in pathological scenes must not crash
    enabled.robustBufferAccess = supported.robustBufferAccess;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = QueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo devInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &queueInfo;
    devInfo.enabledExtensionCount = (u32)devExtNames.size();
    devInfo.ppEnabledExtensionNames = devExtNames.data();
    devInfo.pEnabledFeatures = &enabled;

    res = vkCreateDevice(PhysDev, &devInfo, nullptr, &Device);
    if (res != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: vkCreateDevice failed (%d)\n", res);
        Deinit();
        return false;
    }

#define VK_LOAD_DEVICE(name) name = (PFN_##name)vkGetDeviceProcAddr(Device, #name);
    VK_FOREACH_DEVICE_FUNC(VK_LOAD_DEVICE)
#undef VK_LOAD_DEVICE

    vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);

    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = QueueFamily;
    if (vkCreateCommandPool(Device, &poolInfo, nullptr, &CmdPool) != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: failed to create command pool\n");
        Deinit();
        return false;
    }

    if (!glslang::InitializeProcess())
    {
        Log(LogLevel::Error, "Vulkan: failed to initialise glslang\n");
        Deinit();
        return false;
    }
    OwnsGlslang = true;

    Valid = true;
    return true;
}

void Context::Deinit()
{
    if (Device)
    {
        vkDeviceWaitIdle(Device);
        if (CmdPool) vkDestroyCommandPool(Device, CmdPool, nullptr);
        vkDestroyDevice(Device, nullptr);
    }
    if (Instance)
        vkDestroyInstance(Instance, nullptr);

    if (OwnsGlslang)
    {
        glslang::FinalizeProcess();
        OwnsGlslang = false;
    }

    CmdPool = VK_NULL_HANDLE;
    Device = VK_NULL_HANDLE;
    Instance = VK_NULL_HANDLE;
    PhysDev = VK_NULL_HANDLE;
    Queue = VK_NULL_HANDLE;
    Valid = false;
}

u32 Context::FindMemoryType(u32 typeBits, VkMemoryPropertyFlags wanted)
{
    for (u32 i = 0; i < MemProps.memoryTypeCount; i++)
    {
        if ((typeBits & (1u << i)) &&
            (MemProps.memoryTypes[i].propertyFlags & wanted) == wanted)
            return i;
    }
    return UINT32_MAX;
}

bool Context::CreateBuffer(Buffer& buf, VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible)
{
    VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(Device, &bufInfo, nullptr, &buf.Buf) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(Device, buf.Buf, &memReq);

    u32 memType;
    if (hostVisible)
    {
        memType = FindMemoryType(memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    else
    {
        memType = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == UINT32_MAX)
            memType = FindMemoryType(memReq.memoryTypeBits, 0);
    }
    if (memType == UINT32_MAX)
    {
        vkDestroyBuffer(Device, buf.Buf, nullptr);
        buf.Buf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memType;
    if (vkAllocateMemory(Device, &allocInfo, nullptr, &buf.Mem) != VK_SUCCESS)
    {
        vkDestroyBuffer(Device, buf.Buf, nullptr);
        buf.Buf = VK_NULL_HANDLE;
        return false;
    }
    vkBindBufferMemory(Device, buf.Buf, buf.Mem, 0);

    buf.Size = size;
    buf.Map = nullptr;
    if (hostVisible)
    {
        if (vkMapMemory(Device, buf.Mem, 0, VK_WHOLE_SIZE, 0, &buf.Map) != VK_SUCCESS)
        {
            DestroyBuffer(buf);
            return false;
        }
    }

    return true;
}

void Context::DestroyBuffer(Buffer& buf)
{
    if (buf.Map)
        vkUnmapMemory(Device, buf.Mem);
    if (buf.Buf)
        vkDestroyBuffer(Device, buf.Buf, nullptr);
    if (buf.Mem)
        vkFreeMemory(Device, buf.Mem, nullptr);
    buf = {};
}

bool Context::CreateImage(Image& img, VkFormat format, u32 width, u32 height, u32 layers,
                          VkImageUsageFlags usage, bool array2D)
{
    VkImageCreateInfo imgInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent = {width, height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = layers;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = usage;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(Device, &imgInfo, nullptr, &img.Img) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(Device, img.Img, &memReq);

    u32 memType = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == UINT32_MAX)
        memType = FindMemoryType(memReq.memoryTypeBits, 0);
    if (memType == UINT32_MAX)
    {
        vkDestroyImage(Device, img.Img, nullptr);
        img.Img = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memType;
    if (vkAllocateMemory(Device, &allocInfo, nullptr, &img.Mem) != VK_SUCCESS)
    {
        vkDestroyImage(Device, img.Img, nullptr);
        img.Img = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(Device, img.Img, img.Mem, 0);

    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = img.Img;
    viewInfo.viewType = array2D ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
    if (vkCreateImageView(Device, &viewInfo, nullptr, &img.View) != VK_SUCCESS)
    {
        DestroyImage(img);
        return false;
    }

    img.Format = format;
    img.Width = width;
    img.Height = height;
    img.Layers = layers;
    img.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    return true;
}

void Context::DestroyImage(Image& img)
{
    if (img.View)
        vkDestroyImageView(Device, img.View, nullptr);
    if (img.Img)
        vkDestroyImage(Device, img.Img, nullptr);
    if (img.Mem)
        vkFreeMemory(Device, img.Mem, nullptr);
    img = {};
}

void Context::TransitionImage(VkCommandBuffer cmd, Image& img, VkImageLayout newLayout,
                              VkPipelineStageFlags srcStage, VkAccessFlags srcAccess,
                              VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = img.Layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img.Img;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, img.Layers};

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    img.Layout = newLayout;
}

VkCommandBuffer Context::BeginOneShot()
{
    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = CmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(Device, &allocInfo, &cmd) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void Context::EndOneShot(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(Queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(Queue);

    vkFreeCommandBuffers(Device, CmdPool, 1, &cmd);
}

void Context::UploadImageLayer(Image& img, const void* data, u32 width, u32 height, u32 layer, u32 bytesPerPixel)
{
    VkDeviceSize size = (VkDeviceSize)width * height * bytesPerPixel;

    Buffer staging;
    if (!CreateBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
        return;
    memcpy(staging.Map, data, size);

    VkCommandBuffer cmd = BeginOneShot();
    if (cmd == VK_NULL_HANDLE)
    {
        DestroyBuffer(staging);
        return;
    }

    // the whole image (all layers) moves through TRANSFER_DST; individual layer
    // uploads keep the image in SHADER_READ_ONLY between them
    TransitionImage(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region = {};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, staging.Buf, img.Img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    TransitionImage(cmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

    EndOneShot(cmd);
    DestroyBuffer(staging);
}

bool Context::CompileComputeShader(VkShaderModule& out, const std::string& source, const char* name)
{
    glslang::TShader shader(EShLangCompute);

    const char* sources[] = {source.c_str()};
    shader.setStrings(sources, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, EShLangCompute, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

    if (!shader.parse(GetDefaultResources(), 110, false, EShMsgDefault))
    {
        Log(LogLevel::Error, "Vulkan: shader %s failed to compile:\n%s\n", name, shader.getInfoLog());
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault))
    {
        Log(LogLevel::Error, "Vulkan: shader %s failed to link:\n%s\n", name, program.getInfoLog());
        return false;
    }

    std::vector<u32> spirv;
    glslang::SpvOptions options;
    options.disableOptimizer = false;
    glslang::GlslangToSpv(*program.getIntermediate(EShLangCompute), spirv, &options);

    if (spirv.empty())
    {
        Log(LogLevel::Error, "Vulkan: shader %s produced no SPIR-V\n", name);
        return false;
    }

    VkShaderModuleCreateInfo moduleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = spirv.size() * sizeof(u32);
    moduleInfo.pCode = spirv.data();
    if (vkCreateShaderModule(Device, &moduleInfo, nullptr, &out) != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: failed to create shader module %s\n", name);
        return false;
    }

    return true;
}

}

}
