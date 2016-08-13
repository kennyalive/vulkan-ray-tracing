#pragma once

#include <vector>
#include "vulkan_definitions.h"

class VulkanApp
{
public:
    VulkanApp(int windowWidth, int windowHeight);

    void CreateResources(HWND windowHandle);
    void CleanupResources();
    void RunFrame();

private:
    void CreateFramebuffers();
    void CreateSemaphores();
    void CreateCommandBuffers(uint32_t queueFamilyIndex);

private:
    int windowWidth = 0;
    int windowHeight = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentationQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;

    VkRenderPass renderPass = VK_NULL_HANDLE;

    std::vector<VkImageView> framebufferImageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderingFinishedSemaphore = VK_NULL_HANDLE;

    VkCommandPool presentQueueCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> presentQueueCommandBuffers;
};
