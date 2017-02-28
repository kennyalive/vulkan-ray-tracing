#pragma once

#include "vulkan_definitions.h"
#include <vector>

class Device_Memory_Allocator {
public:
    Device_Memory_Allocator(VkPhysicalDevice physical_device, VkDevice device);
    ~Device_Memory_Allocator();

    // We have single memory chunk that is used as staging memory. It is assumed that previous operations
    // with staging memory are finished, so we can reuse it. If chunk has not enough size we reallocate it.
    VkDeviceMemory allocate_staging_memory(VkImage image);
    VkDeviceMemory allocate_staging_memory(VkBuffer buffer);

    // NOTE: in this implementation I do device memory allocation for each allocation request.
    // TODO: sub-allocate from larger chunks and return chunk handle plus offset withing corresponding chunk.
    VkDeviceMemory allocate_device_local_memory(VkImage image);
    VkDeviceMemory allocate_device_local_memory(VkBuffer buffer);

private:
    VkDeviceMemory allocate_staging_memory(const VkMemoryRequirements& memory_requirements);
    VkDeviceMemory allocate_device_local_memory(const VkMemoryRequirements& memory_requirements);

private:
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    std::vector<VkDeviceMemory> device_local_chunks;

    VkDeviceMemory staging_chunk = VK_NULL_HANDLE;
    VkDeviceSize staging_chunk_size = 0;
    uint32_t staging_memory_type_index = -1;
};

void record_and_run_commands(VkDevice device, VkCommandPool command_pool, VkQueue queue,
    std::function<void(VkCommandBuffer)> recorder);

VkImage create_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator);
VkImage create_staging_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator, const uint8_t* pixels, int bytes_per_pixel);

VkBuffer create_buffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, Device_Memory_Allocator& allocator);
VkBuffer create_staging_buffer(VkDevice device, VkDeviceSize size, Device_Memory_Allocator& allocator, const void* data);
