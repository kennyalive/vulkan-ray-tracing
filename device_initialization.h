#pragma once

#include "vulkan_definitions.h"

struct Device_Info {
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queue_family_index = 0;
    VkQueue queue = VK_NULL_HANDLE;
};

VkInstance create_instance();
VkPhysicalDevice select_physical_device(VkInstance instance);
Device_Info create_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
