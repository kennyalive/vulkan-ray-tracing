#include "swapchain_initialization.h"
#include "vulkan_utilities.h"
#include <algorithm>
#include <cassert>

#define SDL_MAIN_HANDLED
#include "sdl/SDL_syswm.h"

VkSurfaceKHR create_surface(VkInstance instance, const SDL_SysWMinfo& window_sys_info) {
    VkWin32SurfaceCreateInfoKHR desc;
    desc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.hinstance = ::GetModuleHandle(nullptr);
    desc.hwnd = window_sys_info.info.win.window;

    VkSurfaceKHR surface;
    VkResult result = vkCreateWin32SurfaceKHR(instance, &desc, nullptr, &surface);
    check_vk_result(result, "vkCreateWin32SurfaceKHR");
    return surface;
}

static VkSurfaceFormatKHR select_surface_format(const std::vector<VkSurfaceFormatKHR>& format_candidates) {
    // special case that means we can choose any format
    if (format_candidates.size() == 1 && format_candidates[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR surface_format;
        surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
        surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        return surface_format;
    }
    return format_candidates[0];
}

Swapchain_Info create_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR surface_caps;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps);
    check_vk_result(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    VkExtent2D image_extent = surface_caps.currentExtent;
    if (image_extent.width == 0xffffffff && image_extent.height == 0xffffffff) {
        image_extent.width = std::min(surface_caps.maxImageExtent.width, std::max(surface_caps.minImageExtent.width, 640u));
        image_extent.height = std::min(surface_caps.maxImageExtent.height, std::max(surface_caps.minImageExtent.height, 480u));
    }

    // transfer destination usage is required by image clear operations
    if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        error("VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain");

    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // determine surface format
    uint32_t format_count;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    check_vk_result(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    assert(format_count > 0);

    std::vector<VkSurfaceFormatKHR> format_candidates(format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, format_candidates.data());
    check_vk_result(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    VkSurfaceFormatKHR surface_format = select_surface_format(format_candidates);

    // determine present mode and swapchain image count
    uint32_t present_mode_count;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
    check_vk_result(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    assert(present_mode_count > 0);

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data());
    check_vk_result(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");

    VkPresentModeKHR present_mode;
    uint32_t image_count;

    auto it = std::find(present_modes.cbegin(), present_modes.cend(), VK_PRESENT_MODE_MAILBOX_KHR);
    if (it != present_modes.cend()) {
        present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        image_count = std::max(3u, surface_caps.minImageCount);
        if (surface_caps.maxImageCount > 0) {
            image_count = std::min(image_count, surface_caps.maxImageCount);
        }
    } else {
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
        image_count = surface_caps.minImageCount;
    }

    // create swap chain
    VkSwapchainCreateInfoKHR desc;
    desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.surface = surface;
    desc.minImageCount = image_count;
    desc.imageFormat = surface_format.format;
    desc.imageColorSpace = surface_format.colorSpace;
    desc.imageExtent = image_extent;
    desc.imageArrayLayers = 1;
    desc.imageUsage = image_usage;
    desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = nullptr;
    desc.preTransform = surface_caps.currentTransform;
    desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    desc.presentMode = present_mode;
    desc.clipped = VK_TRUE;
    desc.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(device, &desc, nullptr, &swapchain);
    check_vk_result(result, "vkCreateSwapchainKHR");

    Swapchain_Info info;
    info.swapchain = swapchain;
    info.surface_format= surface_format.format;
    uint32_t swapchain_image_count; // swapchain can create more images than requested
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
    check_vk_result(result, "vkGetSwapchainImagesKHR (get count)");
    info.images.resize(image_count);
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, info.images.data());
    check_vk_result(result, "vkGetSwapchainImagesKHR (get images)");
    return info;
}
