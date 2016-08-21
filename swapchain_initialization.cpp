#include <algorithm>
#include <cassert>
#include "common.h"
#include "swapchain_initialization.h"

VkSurfaceFormatKHR SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
{
    assert(!surfaceFormats.empty());

    // special case that means we can choose any format
    if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        VkSurfaceFormatKHR surfaceFormat;
        surfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
        surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        return surfaceFormat;
    }

    for (const auto& surfaceFormat : surfaceFormats)
    {
        if (surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
            return surfaceFormat;
    }
    return surfaceFormats[0];
}

SwapchainInfo CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface)
{
    // get surface capabilities
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    CheckVkResult(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0)
        imageCount = std::min(imageCount,  surfaceCapabilities.maxImageCount);

    VkExtent2D imageExtent = surfaceCapabilities.currentExtent;
    if (surfaceCapabilities.currentExtent.width == -1)
    {
        imageExtent.width = std::min(
            surfaceCapabilities.maxImageExtent.width, 
            std::max(
                surfaceCapabilities.minImageExtent.width, 
                640u));

        imageExtent.height = std::min(
            surfaceCapabilities.maxImageExtent.height,
            std::max(
                surfaceCapabilities.minImageExtent.height,
                480u));
    }

    if ((surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        Error("VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swap chain");
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkSurfaceTransformFlagBitsKHR preTransform;
    if ((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0)
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        preTransform = surfaceCapabilities.currentTransform;

    // determine surface format
    uint32_t surfaceFormatsCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, nullptr);
    CheckVkResult(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");

    if (surfaceFormatsCount == 0)
        Error("zero number of surface formats count");

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, surfaceFormats.data());
    CheckVkResult(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");

    VkSurfaceFormatKHR surfaceFormat = SelectSurfaceFormat(surfaceFormats);

    // determine present mode
    uint32_t presentModesCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, nullptr);
    CheckVkResult(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");

    if (presentModesCount == 0)
        Error("zero number of present modes count");

    std::vector<VkPresentModeKHR> presentModes(presentModesCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, presentModes.data());
    CheckVkResult(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto currentPresentMode : presentModes)
    {
        if (currentPresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = currentPresentMode;
            break;
        }
    }

    // create swap chain
    VkSwapchainCreateInfoKHR swapChainCreateInfo;
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.pNext = nullptr;
    swapChainCreateInfo.flags = 0;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = imageExtent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = imageUsage;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.queueFamilyIndexCount = 0;
    swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    swapChainCreateInfo.preTransform = preTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain);
    CheckVkResult(result, "vkCreateSwapchainKHR");

    SwapchainInfo info;
    info.swapchain = swapchain;
    info.imageFormat = surfaceFormat.format;

    // get swapchain images
    uint32_t imagesCount;
    result = vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, nullptr);
    CheckVkResult(result, "vkGetSwapchainImagesKHR");

    info.images.resize(imagesCount);
    result = vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, info.images.data());
    CheckVkResult(result, "vkGetSwapchainImagesKHR");

    // create views for swapchain images
    info.imageViews.resize(imagesCount);
    for (uint32_t i = 0; i < imagesCount; i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = info.images[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = info.imageFormat;
        imageViewCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        imageViewCreateInfo.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            1,
            0,
            1
        };
        result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &info.imageViews[i]);
        CheckVkResult(result, "vkCreateImageView");
    }

    return info;
}
