#pragma once

#include <vector>
#include "vulkan_definitions.h"

struct SwapchainInfo
{
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> images;
};

SwapchainInfo CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface);
