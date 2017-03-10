#pragma once

#include "vulkan_definitions.h"
#include <vector>

struct SDL_SysWMinfo;

struct Swapchain_Info {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat surface_format = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> images;
};

VkSurfaceKHR create_surface(VkInstance instance, const SDL_SysWMinfo& window_sys_info);
Swapchain_Info create_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface);
