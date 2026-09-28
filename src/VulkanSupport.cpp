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

#include <cstring>
#include <vector>

#include "Platform.h"

namespace melonDS::Vulkan
{

using Platform::Log;
using Platform::LogLevel;

namespace
{

bool HasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
    for (const VkExtensionProperties& extension : extensions)
    {
        if (std::strcmp(extension.extensionName, name) == 0)
            return true;
    }
    return false;
}

std::vector<VkExtensionProperties> EnumerateInstanceExtensions()
{
    u32 count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS)
        return {};

    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS)
        return {};
    extensions.resize(count);
    return extensions;
}

std::vector<VkExtensionProperties> EnumerateDeviceExtensions(VkPhysicalDevice device)
{
    u32 count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS)
        return {};

    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) != VK_SUCCESS)
        return {};
    extensions.resize(count);
    return extensions;
}

}

const char* ResultName(VkResult result) noexcept
{
    switch (result)
    {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    default: return "VK_ERROR_UNKNOWN";
    }
}

Context::~Context()
{
    Reset();
}

bool Context::Init()
{
    Reset();

    u32 loaderVersion = VK_API_VERSION_1_0;
    const auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerateInstanceVersion)
        enumerateInstanceVersion(&loaderVersion);

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "melonDS";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
    applicationInfo.pEngineName = "melonDS";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 1, 0);
    applicationInfo.apiVersion = loaderVersion < VK_API_VERSION_1_1 ? loaderVersion : VK_API_VERSION_1_1;

    const std::vector<VkExtensionProperties> instanceExtensions = EnumerateInstanceExtensions();
    std::vector<const char*> enabledInstanceExtensions;
    VkInstanceCreateFlags instanceFlags = 0;
    if (HasExtension(instanceExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        enabledInstanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        instanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.flags = instanceFlags;
    instanceInfo.pApplicationInfo = &applicationInfo;
    instanceInfo.enabledExtensionCount = static_cast<u32>(enabledInstanceExtensions.size());
    instanceInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &Instance);
    if (result != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: failed to create instance: %s (%d)\n",
            ResultName(result), result);
        Reset();
        return false;
    }

    u32 deviceCount = 0;
    result = vkEnumeratePhysicalDevices(Instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0)
    {
        Log(LogLevel::Error, "Vulkan: no physical device available: %s (%d)\n",
            ResultName(result), result);
        Reset();
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(Instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: failed to enumerate physical devices: %s (%d)\n",
            ResultName(result), result);
        Reset();
        return false;
    }

    for (VkPhysicalDevice device : devices)
    {
        u32 familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

        for (u32 family = 0; family < familyCount; family++)
        {
            const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if ((families[family].queueFlags & required) == required)
            {
                PhysicalDevice = device;
                QueueFamily = family;
                break;
            }
        }
        if (PhysicalDevice != VK_NULL_HANDLE)
            break;
    }

    if (PhysicalDevice == VK_NULL_HANDLE)
    {
        Log(LogLevel::Error, "Vulkan: no graphics and compute queue is available\n");
        Reset();
        return false;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = QueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    const std::vector<VkExtensionProperties> deviceExtensions = EnumerateDeviceExtensions(PhysicalDevice);
    std::vector<const char*> enabledDeviceExtensions;
    static constexpr const char* PortabilitySubsetExtension = "VK_KHR_portability_subset";
    if (HasExtension(deviceExtensions, PortabilitySubsetExtension))
        enabledDeviceExtensions.push_back(PortabilitySubsetExtension);

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<u32>(enabledDeviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();

    result = vkCreateDevice(PhysicalDevice, &deviceInfo, nullptr, &Device);
    if (result != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: failed to create device: %s (%d)\n",
            ResultName(result), result);
        Reset();
        return false;
    }
    vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = QueueFamily;
    result = vkCreateCommandPool(Device, &commandPoolInfo, nullptr, &CommandPool);
    if (result != VK_SUCCESS)
    {
        Log(LogLevel::Error, "Vulkan: failed to create command pool: %s (%d)\n",
            ResultName(result), result);
        Reset();
        return false;
    }

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(PhysicalDevice, &properties);
    Log(LogLevel::Info, "Vulkan: using %s (API %u.%u.%u)\n",
        properties.deviceName,
        VK_API_VERSION_MAJOR(properties.apiVersion),
        VK_API_VERSION_MINOR(properties.apiVersion),
        VK_API_VERSION_PATCH(properties.apiVersion));
    return true;
}

void Context::Reset()
{
    if (Device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(Device);
        if (CommandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(Device, CommandPool, nullptr);
        vkDestroyDevice(Device, nullptr);
    }
    if (Instance != VK_NULL_HANDLE)
        vkDestroyInstance(Instance, nullptr);

    Instance = VK_NULL_HANDLE;
    PhysicalDevice = VK_NULL_HANDLE;
    Device = VK_NULL_HANDLE;
    Queue = VK_NULL_HANDLE;
    CommandPool = VK_NULL_HANDLE;
    QueueFamily = 0;
}

bool Context::FindMemoryType(u32 typeMask, VkMemoryPropertyFlags properties,
                             u32& result) const noexcept
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &memoryProperties);
    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((typeMask & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            result = i;
            return true;
        }
    }
    return false;
}

}
