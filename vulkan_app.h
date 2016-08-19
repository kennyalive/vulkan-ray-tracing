#pragma once

#include <vector>
#include "vulkan_definitions.h"

class VulkanApp
{
public:
    VulkanApp(uint32_t windowWidth, uint32_t windowHeight);

    void CreateResources(HWND windowHandle);
    void CleanupResources();
    void RunFrame();

private:
    void CreateFramebuffers();
    void CreatePipeline();
    void CreateSemaphores();
    void CreateCommandBuffers();

private:
    uint32_t windowWidth = 0;
    uint32_t windowHeight = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkDevice device = VK_NULL_HANDLE;

    uint32_t graphicsQueueFamilyIndex = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    uint32_t presentationQueueFamilyIndex = 0;
    VkQueue presentationQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;

    VkRenderPass renderPass = VK_NULL_HANDLE;

    std::vector<VkImageView> framebufferImageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderingFinishedSemaphore = VK_NULL_HANDLE;

    VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> graphicsCommandBuffers;
};
