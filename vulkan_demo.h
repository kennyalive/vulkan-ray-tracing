#pragma once

#include <memory>
#include <vector>
#include "allocator.h"
#include "resource_manager.h"
#include "vulkan_definitions.h"

class Vulkan_Demo
{
public:
    Vulkan_Demo(uint32_t window_width, uint32_t window_height);

    void CreateResources(HWND windowHandle);
    void CleanupResources();
    void run_frame();

private:
    void create_command_pool();

    void create_uniform_buffer();
    void create_texture();
    void create_texture_sampler();
    void create_depth_buffer_resources();

    void create_render_pass();
    void create_framebuffers();
    void create_descriptor_set_layout();
    void create_pipeline();
    void create_descriptor_pool();
    void create_descriptor_set();

    void upload_geometry();
    void create_command_buffers();
    void record_primary_command_buffers();
    void record_render_scene_command_buffer();

    void update_uniform_buffer();

private:
    const uint32_t window_width = 0;
    const uint32_t window_height = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queue_family_index = 0;
    VkQueue queue = VK_NULL_HANDLE;
    Device_Memory_Allocator allocator;
    Resource_Manager resource_manager;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_image_format = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkSemaphore image_acquired = VK_NULL_HANDLE;
    VkSemaphore rendering_finished = VK_NULL_HANDLE;

    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkRenderPass render_pass = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    uint32_t model_indices_count = 0;

    VkBuffer uniform_staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory uniform_staging_buffer_memory = VK_NULL_HANDLE; // owned by the allocator
    VkBuffer uniform_buffer = VK_NULL_HANDLE;

    VkImage texture_image = VK_NULL_HANDLE;
    VkImageView texture_image_view = VK_NULL_HANDLE;
    VkSampler texture_image_sampler = VK_NULL_HANDLE;

    VkImage depth_image = VK_NULL_HANDLE;
    VkImageView depth_image_view = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> framebuffers;

    std::vector<VkCommandBuffer> command_buffers;
    VkCommandBuffer render_scene_cmdbuf;
};
