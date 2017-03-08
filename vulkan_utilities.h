#pragma once

#include "vulkan_definitions.h"

#include <functional>
#include <string>
#include <vector>

void check_vk_result(VkResult result, const std::string& function_name);
void error(const std::string& message);

struct Defer_Action {
    Defer_Action(std::function<void()> action)
        : action(action) {}
    ~Defer_Action() {
        action();
    }
    std::function<void()> action;
};

struct Shader_Module {
    Shader_Module(VkDevice device, const std::string& spriv_file_name);
    ~Shader_Module();
    VkDevice device;
    VkShaderModule handle;
};

class Device_Memory_Allocator;

// Command buffers
void record_and_run_commands(VkDevice device, VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder);

void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image, VkFormat format,
    VkAccessFlags src_access_flags, VkImageLayout old_layout, VkAccessFlags dst_access_flags, VkImageLayout new_layout);

// Images
VkImage create_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator);
VkImage create_staging_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator, const uint8_t* pixels, int bytes_per_pixel);

VkImage create_depth_attachment_image(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator);

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);

// Buffers
VkBuffer create_buffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, Device_Memory_Allocator& allocator);
VkBuffer create_staging_buffer(VkDevice device, VkDeviceSize size, Device_Memory_Allocator& allocator, const void* data);
VkBuffer create_permanent_staging_buffer(VkDevice device, VkDeviceSize size, Device_Memory_Allocator& allocator, VkDeviceMemory& memory);
