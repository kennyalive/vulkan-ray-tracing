#pragma once

#include <vector>
#include "vulkan_definitions.h"

struct SwapchainInfo
{
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
};

SwapchainInfo CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface);
