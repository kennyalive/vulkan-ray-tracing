#pragma once

#include <vector>
#include "vulkan_definitions.h"

struct DeviceInfo
{
    VkDevice device = VK_NULL_HANDLE;

    uint32_t graphicsQueueFamilyIndex = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    uint32_t presentationQueueFamilyIndex = 0;
    VkQueue presentationQueue = VK_NULL_HANDLE;
};

VkInstance CreateInstance();

VkPhysicalDevice SelectPhysicalDevice(VkInstance instance);

DeviceInfo CreateDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
