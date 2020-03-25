#include "acceleration_structure.h"
#include "matrix.h"
#include "mesh.h"
#include "vk_utils.h"

#include <algorithm>
#include <cassert>

void Vk_Intersection_Accelerator::destroy() {
    for (VkAccelerationStructureKHR accel : bottom_level_accels) {
        vkDestroyAccelerationStructureKHR(vk.device, accel, nullptr);
    }
    vkDestroyAccelerationStructureKHR(vk.device, top_level_accel, nullptr);
    vmaFreeMemory(vk.allocator, allocation);
    instance_buffer.destroy();
    scratch_buffer.destroy();
    *this = Vk_Intersection_Accelerator{};
}

static VmaAllocation allocate_acceleration_structures_memory(const Vk_Intersection_Accelerator& accelerator) {
    // Get memory requirements for each acceleration structure.
    auto get_memory_reqs = [](VkAccelerationStructureKHR accel) {
        VkAccelerationStructureMemoryRequirementsInfoKHR reqs_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR };
        reqs_info.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;
        reqs_info.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
        reqs_info.accelerationStructure = accel;
        VkMemoryRequirements2 reqs_holder{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        vkGetAccelerationStructureMemoryRequirementsKHR(vk.device, &reqs_info, &reqs_holder);
        return reqs_holder.memoryRequirements;
    };

    VkMemoryRequirements top_level_memory_reqs = get_memory_reqs(accelerator.top_level_accel);
    std::vector<VkMemoryRequirements> bottom_level_memory_reqs(accelerator.bottom_level_accels.size());
    for (int i = 0; i < (int)accelerator.bottom_level_accels.size(); i++) {
        bottom_level_memory_reqs[i] = get_memory_reqs(accelerator.bottom_level_accels[i]);
    }

    // Compute alignment and memory type bits.
    VkDeviceSize alignment = top_level_memory_reqs.alignment;
    uint32_t memory_type_bits = top_level_memory_reqs.memoryTypeBits;
    for (const VkMemoryRequirements& reqs : bottom_level_memory_reqs) {
        alignment = std::max(alignment, reqs.alignment);
        memory_type_bits &= reqs.memoryTypeBits;
    }
    assert(memory_type_bits != 0); // not guaranteed by spec

    // Compute required amount of memory and offsets.
    std::vector<VkDeviceSize> bottom_level_offsets(accelerator.bottom_level_accels.size());
    VkDeviceSize size = top_level_memory_reqs.size;
    for (int i = 0; i < (int)accelerator.bottom_level_accels.size(); i++) {
        const VkDeviceSize offset = round_up(size, alignment);
        bottom_level_offsets[i] = offset;
        size = offset + bottom_level_memory_reqs[i].size;
    }

    // Allocate memory.
    VmaAllocationCreateInfo alloc_create_info{};
    alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkMemoryRequirements memory_reqs;
    memory_reqs.size = size;
    memory_reqs.alignment = alignment;
    memory_reqs.memoryTypeBits = memory_type_bits;

    VmaAllocation allocation;
    VmaAllocationInfo alloc_info;
    VK_CHECK(vmaAllocateMemory(vk.allocator, &memory_reqs, &alloc_create_info, &allocation, &alloc_info));

    // Attach memory to acceleration structures.
    std::vector<VkBindAccelerationStructureMemoryInfoKHR> bind_infos(1 + accelerator.bottom_level_accels.size());
    bind_infos[0] = VkBindAccelerationStructureMemoryInfoKHR{ VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR };
    bind_infos[0].accelerationStructure = accelerator.top_level_accel;
    bind_infos[0].memory = alloc_info.deviceMemory;
    bind_infos[0].memoryOffset = alloc_info.offset + 0;

    for (int i = 0; i < (int)accelerator.bottom_level_accels.size(); i++) {
        bind_infos[i+1] = VkBindAccelerationStructureMemoryInfoKHR{ VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR };
        bind_infos[i+1].accelerationStructure = accelerator.bottom_level_accels[i];
        bind_infos[i+1].memory = alloc_info.deviceMemory;
        bind_infos[i+1].memoryOffset = alloc_info.offset + bottom_level_offsets[i];
    }
    VK_CHECK(vkBindAccelerationStructureMemoryKHR(vk.device, (uint32_t)bind_infos.size(), bind_infos.data()));
    return allocation;
}

Vk_Intersection_Accelerator create_intersection_accelerator(const std::vector<GPU_Mesh>& gpu_meshes, bool keep_scratch_buffer) {
    Vk_Intersection_Accelerator accelerator;

    // Create bottom level acceleration structures.
    accelerator.bottom_level_accels.resize(gpu_meshes.size());
    for (int i = 0; i < (int)gpu_meshes.size(); i++) {
        VkAccelerationStructureCreateGeometryTypeInfoKHR geometry_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR };
        geometry_info.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry_info.maxPrimitiveCount = gpu_meshes[i].index_count / 3;
        geometry_info.indexType = VK_INDEX_TYPE_UINT32;
        geometry_info.maxVertexCount = gpu_meshes[i].vertex_count;
        geometry_info.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

        VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        create_info.maxGeometryCount = 1;
        create_info.pGeometryInfos = &geometry_info;

        VK_CHECK(vkCreateAccelerationStructureKHR(vk.device, &create_info, nullptr, &accelerator.bottom_level_accels[i]));
        vk_set_debug_name(accelerator.bottom_level_accels[i], "bottom_level_accel");
    }

    // Create top level acceleration structure.
    {
        VkAccelerationStructureCreateGeometryTypeInfoKHR geometry_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR };
        geometry_info.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry_info.maxPrimitiveCount = (uint32_t)gpu_meshes.size();

        VkAccelerationStructureCreateInfoKHR create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.maxGeometryCount = 1;
        create_info.pGeometryInfos = &geometry_info;

        VK_CHECK(vkCreateAccelerationStructureKHR(vk.device, &create_info, nullptr, &accelerator.top_level_accel));
        vk_set_debug_name(accelerator.top_level_accel, "top_level_accel");
    }

    // Allocate memory and bind acceleration structures.
    // Single allocation is made, acceleration structures are bound at different offsets.
    accelerator.allocation = allocate_acceleration_structures_memory(accelerator);

    // Create instance buffer.
    {
        accelerator.bottom_level_accel_device_addresses.resize(gpu_meshes.size());
        for (int i = 0; i < (int)gpu_meshes.size(); i++) {
            VkAccelerationStructureDeviceAddressInfoKHR device_address_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
            device_address_info.accelerationStructure = accelerator.bottom_level_accels[i];
            accelerator.bottom_level_accel_device_addresses[i] = vkGetAccelerationStructureDeviceAddressKHR(vk.device, &device_address_info);
        }

        std::vector<VkAccelerationStructureInstanceKHR> instances(gpu_meshes.size());
        for (int i = 0; i < (int)instances.size(); i++) {
            memcpy(&instances[i].transform.matrix[0][0], &Matrix3x4::identity.a[0][0], 12 * sizeof(float));
            instances[i].instanceCustomIndex = i;
            instances[i].mask = 0xff;
            instances[i].instanceShaderBindingTableRecordOffset = 0;
            instances[i].flags = 0;
            instances[i].accelerationStructureReference = accelerator.bottom_level_accel_device_addresses[i];
        }
        VkDeviceSize instance_buffer_size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
        accelerator.instance_buffer = vk_create_mapped_buffer(instance_buffer_size, VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR, &(void*&)accelerator.mapped_instance_buffer, "instance_buffer");
    }

    // Create scratch buffert.
    {
        auto get_scratch_size = [](VkAccelerationStructureKHR accel) {
            VkAccelerationStructureMemoryRequirementsInfoKHR reqs_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR };
            reqs_info.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
            reqs_info.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
            reqs_info.accelerationStructure = accel;
            VkMemoryRequirements2 reqs_holder{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
            vkGetAccelerationStructureMemoryRequirementsKHR(vk.device, &reqs_info, &reqs_holder);
            // According to the spec, the alignment and memoryTypeBits returned
            // from vkGetAccelerationStructureMemoryRequirementsKHR for scratch
            // allocation should be ignored.
            return reqs_holder.memoryRequirements.size;            
        };

        {
            VkDeviceSize scratch_size = get_scratch_size(accelerator.top_level_accel);
            for (VkAccelerationStructureKHR accel : accelerator.bottom_level_accels) {
                scratch_size = std::max(scratch_size, get_scratch_size(accel));
            }
            accelerator.scratch_buffer = vk_create_buffer(scratch_size, VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR);
        }
    }

    // Build acceleration structures.
    Timestamp t;

    vk_execute(vk.command_pools[0], vk.queue,
        [&gpu_meshes, &accelerator](VkCommandBuffer command_buffer)
    {
        for (auto [i, accel] : enumerate(accelerator.bottom_level_accels)) {
            VkAccelerationStructureGeometryKHR geometry { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
            const VkAccelerationStructureGeometryKHR* p_geometry[1] = { &geometry };
            {
                geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
                auto& triangles = geometry.geometry.triangles;
                triangles = VkAccelerationStructureGeometryTrianglesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
                triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                triangles.vertexData.deviceAddress = gpu_meshes[i].vertex_buffer.device_address;
                triangles.vertexStride = sizeof(Vertex);
                triangles.indexType = VK_INDEX_TYPE_UINT32;
                triangles.indexData.deviceAddress = gpu_meshes[i].index_buffer.device_address;
            }

            VkAccelerationStructureBuildGeometryInfoKHR geometry_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
            {
                geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                geometry_info.update = VK_FALSE;
                geometry_info.dstAccelerationStructure = accelerator.bottom_level_accels[i];
                geometry_info.geometryArrayOfPointers = VK_TRUE;
                geometry_info.geometryCount = 1;
                geometry_info.ppGeometries = p_geometry;
                geometry_info.scratchData.deviceAddress = accelerator.scratch_buffer.device_address;
            }

            VkAccelerationStructureBuildOffsetInfoKHR offset_info{};
            const VkAccelerationStructureBuildOffsetInfoKHR* p_offset_info[1] = { &offset_info };
            offset_info.primitiveCount = gpu_meshes[i].index_count / 3;
            offset_info.primitiveOffset = 0;

            vkCmdBuildAccelerationStructureKHR(command_buffer, 1, &geometry_info, p_offset_info);

            // We need a barrier when building multiple bottom level AS because
            // signle scratch space is used for all builds.
            VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            vkCmdPipelineBarrier(command_buffer,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0, 1, &barrier, 0, nullptr, 0, nullptr);
        }

        VkAccelerationStructureGeometryKHR geometry { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        const VkAccelerationStructureGeometryKHR* p_geometry[1] = { &geometry };
        {
            geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            geometry.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
            geometry.geometry.instances.arrayOfPointers = VK_FALSE;
            geometry.geometry.instances.data.deviceAddress = accelerator.instance_buffer.device_address;
        }

        VkAccelerationStructureBuildGeometryInfoKHR geometry_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        {
            geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            geometry_info.flags = 0;
            geometry_info.update = VK_FALSE;
            geometry_info.dstAccelerationStructure = accelerator.top_level_accel;
            geometry_info.geometryArrayOfPointers = VK_TRUE;
            geometry_info.geometryCount = 1;
            geometry_info.ppGeometries = p_geometry;
            geometry_info.scratchData.deviceAddress = accelerator.scratch_buffer.device_address;
        }

        VkAccelerationStructureBuildOffsetInfoKHR offset_info{};
        const VkAccelerationStructureBuildOffsetInfoKHR* p_offset_info[1] = { &offset_info };
        offset_info.primitiveCount = 1;
        offset_info.primitiveOffset = 0;

        vkCmdBuildAccelerationStructureKHR(command_buffer, 1, &geometry_info, p_offset_info);

        VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    });

    if (!keep_scratch_buffer)
        accelerator.scratch_buffer.destroy();

    printf("\nAcceleration structures build time = %lld microseconds\n", elapsed_microseconds(t));
    return accelerator;
}
