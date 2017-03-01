#pragma once

#include "vulkan_definitions.h"
#include <vector>

class Shared_Staging_Memory {
public:
    Shared_Staging_Memory(VkPhysicalDevice physical_device, VkDevice device);
    ~Shared_Staging_Memory();

    void ensure_allocation_for_object(VkImage image);
    void ensure_allocation_for_object(VkBuffer buffer);
    VkDeviceMemory get_handle() const;

private:
    void ensure_allocation(const VkMemoryRequirements& memory_requirements);

private:
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkDeviceMemory handle = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    uint32_t memory_type_index = -1;
};

// NOTE: in this implementation I do memory allocation for each allocation request.
// TODO: sub-allocate from larger chunks and return chunk handle plus offset withing corresponding chunk.
class Device_Memory_Allocator {
public:
    Device_Memory_Allocator(VkPhysicalDevice physical_device, VkDevice device);
    ~Device_Memory_Allocator();

    VkDeviceMemory allocate_memory(VkImage image);
    VkDeviceMemory allocate_memory(VkBuffer buffer);
    VkDeviceMemory allocate_staging_memory(VkImage image);
    VkDeviceMemory allocate_staging_memory(VkBuffer buffer);

    Shared_Staging_Memory& get_shared_staging_memory();

private:
    VkDeviceMemory allocate_memory(const VkMemoryRequirements& memory_requirements, VkMemoryPropertyFlags properties);

private:
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    std::vector<VkDeviceMemory> chunks;
    Shared_Staging_Memory shared_staging_memory;
};

void record_and_run_commands(VkDevice device, VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder);

VkImage create_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator);
VkImage create_staging_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator, const uint8_t* pixels, int bytes_per_pixel);

VkBuffer create_buffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, Device_Memory_Allocator& allocator);
VkBuffer create_staging_buffer(VkDevice device, VkDeviceSize size, Device_Memory_Allocator& allocator, const void* data);
VkBuffer create_permanent_staging_buffer(VkDevice device, VkDeviceSize size, Device_Memory_Allocator& allocator, VkDeviceMemory& memory);
