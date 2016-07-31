#pragma once

#include <vector>
#include "vulkan_definitions.h"

struct SwapchainInfo
{
    VkSwapchainKHR handle;
    VkFormat imageFormat;
    std::vector<VkImage> images;

    SwapchainInfo()
        : handle(VK_NULL_HANDLE)
        , imageFormat(VK_FORMAT_UNDEFINED)
    {}
};

SwapchainInfo CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface);
