#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include "volk.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL_syswm.h"

#include "common.h"

#define VK_CHECK(function_call) { \
    VkResult result = function_call; \
    if (result < 0) \
        error("Vulkan: error code " + std::to_string(result) + " returned by " + #function_call); \
}

#include <functional>
#include <string>
#include <vector>

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
Vk_Image vk_create_texture(int width, int height, VkFormat format, bool generate_mipmaps, const uint8_t* pixels, int bytes_per_pixel, const char*  name);
Vk_Image vk_create_render_target(int width, int height, VkFormat format, const char* name);
VkPipeline vk_find_pipeline(const Vk_Pipeline_Def& def);

//
// Rendering setup.
//
void vk_begin_frame();
void vk_end_frame();

void vk_record_and_run_commands(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder);

// Vk_Instance contains vulkan resources that do not depend on applicaton logic.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
struct Vk_Instance {
    SDL_SysWMinfo           system_window_info;
    HMODULE                 vulkan_library;
    int                     surface_width;
    int                     surface_height;

    VkInstance              instance;
    VkPhysicalDevice        physical_device;
    uint32_t                queue_family_index;
    VkDevice                device;
    VkQueue                 queue;

    VmaAllocator            allocator;

    VkSurfaceKHR            surface;
    VkSurfaceFormatKHR      surface_format;
    VkSwapchainKHR          swapchain;
    std::vector<VkImage>    swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    uint32_t                swapchain_image_index = -1; // current swapchain image

    VkCommandPool           command_pool;
    VkCommandBuffer         command_buffer;

    VkSemaphore             image_acquired;
    VkSemaphore             rendering_finished;
    VkFence                 rendering_finished_fence;

    VkImage                 depth_image;
    VmaAllocation           depth_image_allocation;
    VkFormat                depth_image_format;
    VkImageView             depth_image_view;

    std::vector<Vk_Pipeline_Def> pipeline_defs;
    std::vector<VkPipeline> pipelines;

    // Host visible memory used to copy image data to device local memory.
    VkBuffer                staging_buffer;
    VmaAllocation           staging_buffer_allocation;
    VkDeviceSize            staging_buffer_size;
    uint8_t*                staging_buffer_ptr; // pointer to mapped staging buffer

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debug_utils_messenger = nullptr;
#endif
};

extern Vk_Instance vk;
