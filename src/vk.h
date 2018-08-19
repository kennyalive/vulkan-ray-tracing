#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include "volk.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL_syswm.h"

#define VK_CHECK(function_call) { \
    VkResult result = function_call; \
    if (result < 0) \
        error("Vulkan: error code " + std::to_string(result) + " returned by " + #function_call); \
}

#include "debug.h"

#include <functional>
#include <string>
#include <vector>

void error(const std::string& message);
void set_window_title(const std::string&);

struct Vk_Pipeline_Def {
    VkShaderModule vs_module = VK_NULL_HANDLE;
    VkShaderModule fs_module = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    bool operator==(const Vk_Pipeline_Def& other) const {
        return vs_module == other.vs_module &&
               fs_module == other.fs_module &&
               render_pass == other.render_pass &&
               pipeline_layout == other.pipeline_layout;

    }
};

struct Vk_Image {
    VkImage handle = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize(const SDL_SysWMinfo& window_info);

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown();

//
// Resources allocation.
//
void vk_ensure_staging_buffer_allocation(VkDeviceSize size);
VkBuffer vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, const char* name);
VkBuffer vk_create_host_visible_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** buffer_ptr, const char* name);
Vk_Image vk_create_texture(int width, int height, VkFormat format, int mip_levels, const uint8_t* pixels, int bytes_per_pixel, const char*  name);
Vk_Image vk_create_render_target(int width, int height, VkFormat format, const char* name);
VkPipeline vk_find_pipeline(const Vk_Pipeline_Def& def);

//
// Rendering setup.
//
void vk_begin_frame();
void vk_end_frame();

void vk_record_and_run_commands(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder);

void vk_record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
                                     VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
                                     VkAccessFlags src_access, VkAccessFlags dst_access);

// Vk_Instance contains vulkan resources that do not depend on applicaton logic.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
struct Vk_Instance {
    SDL_SysWMinfo system_window_info;
    HMODULE vulkan_library = NULL;
    int surface_width = 0;
    int surface_height = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surface_format = {};

    uint32_t queue_family_index = 0;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;

    VmaAllocator allocator;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    uint32_t swapchain_image_index = -1; // current swapchain image

    VkSemaphore image_acquired = VK_NULL_HANDLE;
    VkSemaphore rendering_finished = VK_NULL_HANDLE;
    VkFence rendering_finished_fence = VK_NULL_HANDLE;

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    VkImage depth_image = VK_NULL_HANDLE;
    VmaAllocation depth_image_allocation = VK_NULL_HANDLE;
    VkFormat depth_image_format = VK_FORMAT_UNDEFINED;
    VkImageView depth_image_view = VK_NULL_HANDLE;

    std::vector<Vk_Pipeline_Def> pipeline_defs;
    std::vector<VkPipeline> pipelines;

    // Host visible memory used to copy image data to device local memory.
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_buffer_allocation = VK_NULL_HANDLE;
    VkDeviceSize staging_buffer_size = 0;
    uint8_t* staging_buffer_ptr = nullptr; // pointer to mapped staging buffer

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_utils_messenger = nullptr;
#endif
};

extern Vk_Instance vk;
