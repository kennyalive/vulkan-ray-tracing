#pragma once

#include <array>
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
    void CreatePipeline();

    void CreateBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkMemoryPropertyFlagBits memoryProperty,
        VkBuffer& buffer, VkDeviceMemory& deviceMemory);

    void CreateStagingBuffer();
    void CreateVertexBuffer();
    
    void CreateFrameResources();
    void CopyVertexData();

    void RecordCommandBuffer();

private:
    struct FrameRenderResources
    {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderingFinishedSemaphore = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };

private:
    uint32_t windowWidth = 0;
    uint32_t windowHeight = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    VkDevice device = VK_NULL_HANDLE;

    uint32_t graphicsQueueFamilyIndex = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    uint32_t presentationQueueFamilyIndex = 0;
    VkQueue presentationQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkRenderPass renderPass = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::array<FrameRenderResources, 3> frameResources;

    int frameResourcesIndex = 0;
    uint32_t swapchainImageIndex = 0;
};
