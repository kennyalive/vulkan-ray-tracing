#pragma once

#include "vk.h"

struct GPU_Mesh;

struct Vk_Intersection_Accelerator {
    std::vector<VkAccelerationStructureKHR> bottom_level_accels;
    std::vector<VkDeviceAddress> bottom_level_accel_device_addresses;
    VkAccelerationStructureKHR top_level_accel = VK_NULL_HANDLE;

    // allocation shared by bottom level and top level acceleration structures
    VmaAllocation allocation = VK_NULL_HANDLE;

    Vk_Buffer instance_buffer; // array of VkAccelerationStructureInstanceKHR
    VkAccelerationStructureInstanceKHR* mapped_instance_buffer = nullptr;

    Vk_Buffer scratch_buffer;

    void destroy();
};

Vk_Intersection_Accelerator create_intersection_accelerator(const std::vector<GPU_Mesh>& gpu_meshes, bool keep_scratch_buffer);
