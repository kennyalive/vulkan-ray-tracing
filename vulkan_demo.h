#pragma once

#include <array>
#include <memory>
#include <vector>
#include "vulkan_utilities.h"

class Vulkan_Demo
{
public:
    Vulkan_Demo(uint32_t windowWidth, uint32_t windowHeight);

    void CreateResources(HWND windowHandle);
    void CleanupResources();
    void RunFrame();

private:
    void CreateFrameResources();

    void create_descriptor_set_layout();
    void CreatePipeline();

    void create_vertex_buffer();
    void create_index_buffer();
    void create_uniform_buffer();
    void create_descriptor_pool();
    void create_descriptor_set();
    void create_texture();
    void create_texture_view();
    void create_texture_sampler();

    void RecordCommandBuffer();

    void update_uniform_buffer();

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

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;

    VkBuffer uniform_staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory uniform_staging_buffer_memory = VK_NULL_HANDLE; // owned by the allocator
    VkBuffer uniform_buffer = VK_NULL_HANDLE;

    VkImage texture_image = VK_NULL_HANDLE;
    VkImageView texture_image_view = VK_NULL_HANDLE;
    VkSampler texture_image_sampler = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::array<FrameRenderResources, 3> frameResources;

    int frameResourcesIndex = 0;
    uint32_t swapchainImageIndex = 0;

    std::unique_ptr<Device_Memory_Allocator> allocator;
};
