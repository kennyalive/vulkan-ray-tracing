#pragma once

#include <vector>
#include "vulkan_definitions.h"

struct Device_Info {
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queue_family_index = 0;
    VkQueue queue = VK_NULL_HANDLE;
};

VkInstance CreateInstance();

VkPhysicalDevice SelectPhysicalDevice(VkInstance instance);

Device_Info CreateDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
