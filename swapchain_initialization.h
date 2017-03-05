#pragma once

#include "vulkan_definitions.h"
#include <vector>

VkSurfaceKHR create_surface(VkInstance instance, HWND hwnd);

struct Swapchain_Info {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat surface_format = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> images;
};

Swapchain_Info create_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface);
