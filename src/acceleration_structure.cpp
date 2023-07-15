#include "std.h"
#include "acceleration_structure.h"
#include "gpu_mesh.h"
#include "lib.h"
#include "triangle_mesh.h"

static BLAS_Info create_BLAS(const GPU_Mesh& mesh, uint32_t scratch_alignment) {
    VkAccelerationStructureGeometryKHR geometry { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

    auto& trianglesData = geometry.geometry.triangles;
    trianglesData = VkAccelerationStructureGeometryTrianglesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
    trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    trianglesData.vertexData.deviceAddress = mesh.vertex_buffer.device_address;
    trianglesData.vertexStride = sizeof(Vertex);
    trianglesData.maxVertex = mesh.vertex_count - 1;
    trianglesData.indexType = VK_INDEX_TYPE_UINT32;
    trianglesData.indexData.deviceAddress = mesh.index_buffer.device_address;

    VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR;
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.geometryCount = 1;
    build_info.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR build_sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    uint32_t triangle_count = mesh.index_count / 3;
    vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &triangle_count, &build_sizes);

    // Create buffer to hold acceleration structure data.
    BLAS_Info blas;
    blas.buffer = vk_create_buffer(build_sizes.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, nullptr, "blas_buffer");

    // Create acceleration structure.
    VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    create_info.buffer = blas.buffer.handle;
    create_info.offset = 0;
    create_info.size = build_sizes.accelerationStructureSize;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(vkCreateAccelerationStructureKHR(vk.device, &create_info, nullptr, &blas.acceleration_structure));
    vk_set_debug_name(blas.acceleration_structure, "blas");

    // Get acceleration structure address.
    VkAccelerationStructureDeviceAddressInfoKHR device_address_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    device_address_info.accelerationStructure = blas.acceleration_structure;
    blas.device_address = vkGetAccelerationStructureDeviceAddressKHR(vk.device, &device_address_info);

    // Build acceleration structure.
    Vk_Buffer scratch_buffer = vk_create_buffer_with_alignment(build_sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, scratch_alignment);
    build_info.dstAccelerationStructure = blas.acceleration_structure;
    build_info.scratchData.deviceAddress = scratch_buffer.device_address;

    VkAccelerationStructureBuildRangeInfoKHR build_range_info{};
    build_range_info.primitiveCount = mesh.index_count / 3;
    const VkAccelerationStructureBuildRangeInfoKHR* p_build_range_infos[1] = { &build_range_info };

    vk_execute(vk.command_pools[0], vk.queue, [&build_info, p_build_range_infos](VkCommandBuffer command_buffer)
    {
        vkCmdBuildAccelerationStructuresKHR(command_buffer, 1, &build_info, p_build_range_infos);
    });
    scratch_buffer.destroy();
    return blas;
}

static TLAS_Info create_TLAS(uint32_t instance_count, VkDeviceAddress instances_device_address, uint32_t scratch_alignment) {
    VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instances_device_address;

    VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.geometryCount = 1;
    build_info.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR build_sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build_info, &instance_count, &build_sizes);

    // Create buffer to hold acceleration structure data.
    TLAS_Info tlas;
    tlas.buffer = vk_create_buffer(build_sizes.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, nullptr, "tlas_buffer");

    // Create acceleration structure.
    VkAccelerationStructureCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    create_info.buffer = tlas.buffer.handle;
    create_info.offset = 0;
    create_info.size = build_sizes.accelerationStructureSize;
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(vkCreateAccelerationStructureKHR(vk.device, &create_info, nullptr, &tlas.aceleration_structure));
    vk_set_debug_name(tlas.aceleration_structure, "tlas");

    // Build acceleration structure.
    tlas.scratch_buffer = vk_create_buffer_with_alignment(build_sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, scratch_alignment);
    build_info.dstAccelerationStructure = tlas.aceleration_structure;
    build_info.scratchData.deviceAddress = tlas.scratch_buffer.device_address;

    VkAccelerationStructureBuildRangeInfoKHR build_range_info{};
    build_range_info.primitiveCount = instance_count;
    const VkAccelerationStructureBuildRangeInfoKHR* p_build_range_infos[1] = { &build_range_info };

    vk_execute(vk.command_pools[0], vk.queue, [&build_info, p_build_range_infos](VkCommandBuffer command_buffer)
    {
        vkCmdBuildAccelerationStructuresKHR(command_buffer, 1, &build_info, p_build_range_infos);
    });
    return tlas;
}

Vk_Intersection_Accelerator create_intersection_accelerator(const std::vector<GPU_Mesh>& gpu_meshes) {
    Timestamp t;
    Vk_Intersection_Accelerator accelerator;

    auto accel_properties = VkPhysicalDeviceAccelerationStructurePropertiesKHR{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 physical_device_properties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        &accel_properties
    };
    vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);
    const uint32_t scratch_alignment = accel_properties.minAccelerationStructureScratchOffsetAlignment;

    // Create BLASes.
    accelerator.bottom_level_accels.resize(gpu_meshes.size());
    for (int i = 0; i < (int)gpu_meshes.size(); i++) {
        accelerator.bottom_level_accels[i] = create_BLAS(gpu_meshes[i], scratch_alignment);
    }
    // Create instance buffer.
    {
        std::vector<VkAccelerationStructureInstanceKHR> instances(gpu_meshes.size());
        for (int i = 0; i < (int)instances.size(); i++) {
            memcpy(&instances[i].transform.matrix[0][0], &Matrix3x4::identity.a[0][0], 12 * sizeof(float));
            instances[i].instanceCustomIndex = i;
            instances[i].mask = 0xff;
            instances[i].instanceShaderBindingTableRecordOffset = 0;
            instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instances[i].accelerationStructureReference = accelerator.bottom_level_accels[i].device_address;
        }
        VkDeviceSize instance_buffer_size = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
        accelerator.instance_buffer = vk_create_mapped_buffer(instance_buffer_size,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            &(void*&)accelerator.mapped_instance_buffer, "instance_buffer");
    }
    // Create TLAS.
    accelerator.top_level_accel = create_TLAS((uint32_t)gpu_meshes.size(), accelerator.instance_buffer.device_address, scratch_alignment);

    printf("\nAcceleration structures build time = %lld microseconds\n", elapsed_nanoseconds(t) / 1000);
    return accelerator;
}

void Vk_Intersection_Accelerator::rebuild_top_level_accel(VkCommandBuffer command_buffer) {
    VkAccelerationStructureGeometryKHR geometry { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instance_buffer.device_address;

    VkAccelerationStructureBuildGeometryInfoKHR build_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.dstAccelerationStructure = top_level_accel.aceleration_structure;
    build_info.geometryCount = 1;
    build_info.pGeometries = &geometry;
    build_info.scratchData.deviceAddress = top_level_accel.scratch_buffer.device_address;

    VkAccelerationStructureBuildRangeInfoKHR build_range_info{};
    build_range_info.primitiveCount = (uint32_t)bottom_level_accels.size();
    const VkAccelerationStructureBuildRangeInfoKHR* p_build_range_info[1] = { &build_range_info };

    vkCmdBuildAccelerationStructuresKHR(command_buffer, 1, &build_info, p_build_range_info);

    VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR /*| VK_ACCESS_SHADER_READ_BIT*/;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void Vk_Intersection_Accelerator::destroy() {
    for (auto& blas : bottom_level_accels) {
        vkDestroyAccelerationStructureKHR(vk.device, blas.acceleration_structure, nullptr);
        blas.buffer.destroy();
    }
    vkDestroyAccelerationStructureKHR(vk.device, top_level_accel.aceleration_structure, nullptr);
    top_level_accel.buffer.destroy();
    top_level_accel.scratch_buffer.destroy();
    instance_buffer.destroy();
    *this = Vk_Intersection_Accelerator{};
}
