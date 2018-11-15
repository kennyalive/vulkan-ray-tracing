#include "mesh.h"
#include "rt_resources.h"
#include "utils.h"

#include <algorithm>
#include <cassert>

struct Rt_Uniform_Buffer {
    Matrix3x4 camera_to_world;
    Matrix3x4 model_transform;
};

void Raytracing_Resources::create(const VkGeometryTrianglesNVX& model_triangles, VkImageView texture_view, VkSampler sampler) {
    uniform_buffer = vk_create_host_visible_buffer(static_cast<VkDeviceSize>(sizeof(Rt_Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &(void*&)mapped_uniform_buffer, "rt_uniform_buffer");

    // Instance buffer.
    {
        VkBufferCreateInfo buffer_create_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_create_info.size = sizeof(VkInstanceNVX);
        buffer_create_info.usage = VK_BUFFER_USAGE_RAYTRACING_BIT_NVX;

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VmaAllocationInfo alloc_info;
        VK_CHECK(vmaCreateBuffer(vk.allocator, &buffer_create_info, &alloc_create_info, &instance_buffer, &instance_buffer_allocation, &alloc_info));
        mapped_instance_buffer = (VkInstanceNVX*)alloc_info.pMappedData;
    }

    create_acceleration_structure(model_triangles);
    create_pipeline(model_triangles, texture_view, sampler);

    // Shader binding table.
    {
        constexpr uint32_t group_count = 3;
        VkDeviceSize sbt_size = group_count * shader_header_size;
        void* mapped_memory;
        shader_binding_table = vk_create_host_visible_buffer(sbt_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &mapped_memory, "shader_binding_table");
        VK_CHECK(vkGetRaytracingShaderHandlesNVX(vk.device, pipeline, 0, group_count, sbt_size, mapped_memory));
    }
}

void Raytracing_Resources::destroy() {
    uniform_buffer.destroy();
    shader_binding_table.destroy();

    vkDestroyAccelerationStructureNVX(vk.device, bottom_level_accel, nullptr);
    vmaFreeMemory(vk.allocator, bottom_level_accel_allocation);

    vkDestroyAccelerationStructureNVX(vk.device, top_level_accel, nullptr);
    vmaFreeMemory(vk.allocator, top_level_accel_allocation);

    vmaDestroyBuffer(vk.allocator, scratch_buffer, scratch_buffer_allocation);
    vmaDestroyBuffer(vk.allocator, instance_buffer, instance_buffer_allocation);

    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
}

void Raytracing_Resources::update_output_image_descriptor(VkImageView output_image_view) {
    Descriptor_Writes(descriptor_set).storage_image(0, output_image_view);
}

void Raytracing_Resources::update(const Matrix3x4& model_transform, const Matrix3x4& camera_to_world_transform) {
    VkInstanceNVX& instance = *mapped_instance_buffer;
    instance.transform                                  = model_transform;
    instance.instance_id                                = 0;
    instance.instance_mask                              = 0xff;
    instance.instance_contribution_to_hit_group_index   = 0;
    instance.flags                                      = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
    instance.acceleration_structure_handle              = bottom_level_accel_handle;

    Rt_Uniform_Buffer& uniform_buffer = *mapped_uniform_buffer;
    uniform_buffer.camera_to_world = camera_to_world_transform;
    uniform_buffer.model_transform = model_transform;
}

void Raytracing_Resources::create_acceleration_structure(const VkGeometryTrianglesNVX& triangles) {
    VkGeometryNVX geometry { VK_STRUCTURE_TYPE_GEOMETRY_NVX };
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
    geometry.geometry.triangles = triangles;
    geometry.geometry.aabbs = VkGeometryAABBNVX { VK_STRUCTURE_TYPE_GEOMETRY_AABB_NVX };

    // Create acceleration structures and allocate/bind memory.
    {
        auto allocate_acceleration_structure_memory = [](VkAccelerationStructureNVX acceleration_structutre, VmaAllocation* allocation) {
            VkAccelerationStructureMemoryRequirementsInfoNVX accel_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
            accel_info.accelerationStructure = acceleration_structutre;

            VkMemoryRequirements2 reqs_holder { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
            vkGetAccelerationStructureMemoryRequirementsNVX(vk.device, &accel_info, &reqs_holder);

            VmaAllocationCreateInfo alloc_create_info{};
            alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VmaAllocationInfo alloc_info;
            VK_CHECK(vmaAllocateMemory(vk.allocator, &reqs_holder.memoryRequirements, &alloc_create_info, allocation, &alloc_info));

            VkBindAccelerationStructureMemoryInfoNVX bind_info { VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NVX };
            bind_info.accelerationStructure = acceleration_structutre;
            bind_info.memory = alloc_info.deviceMemory;
            bind_info.memoryOffset = alloc_info.offset;
            VK_CHECK(vkBindAccelerationStructureMemoryNVX(vk.device, 1, &bind_info));
        };

        // Bottom level.
        {
            VkAccelerationStructureCreateInfoNVX create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX;
            create_info.geometryCount = 1;
            create_info.pGeometries = &geometry;

            VK_CHECK(vkCreateAccelerationStructureNVX(vk.device, &create_info, nullptr, &bottom_level_accel));
            allocate_acceleration_structure_memory(bottom_level_accel, &bottom_level_accel_allocation);
            vk_set_debug_name(bottom_level_accel, "bottom_level_accel");

            VK_CHECK(vkGetAccelerationStructureHandleNVX(vk.device, bottom_level_accel, sizeof(uint64_t), &bottom_level_accel_handle));
            update(Matrix3x4::identity, Matrix3x4::identity);
        }

        // Top level.
        {
            VkAccelerationStructureCreateInfoNVX create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX;
            create_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NVX;
            create_info.instanceCount = 1;

            VK_CHECK(vkCreateAccelerationStructureNVX(vk.device, &create_info, nullptr, &top_level_accel));
            allocate_acceleration_structure_memory(top_level_accel, &top_level_accel_allocation);
            vk_set_debug_name(top_level_accel, "top_level_accel");
        }
    }

    // Create scratch buffert required to build/update acceleration structures.
    {
        VkMemoryRequirements scratch_memory_requirements;
        {
            VkAccelerationStructureMemoryRequirementsInfoNVX accel_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
            VkMemoryRequirements2 reqs_holder { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };

            accel_info.accelerationStructure = bottom_level_accel;
            vkGetAccelerationStructureScratchMemoryRequirementsNVX(vk.device, &accel_info, &reqs_holder);
            VkMemoryRequirements reqs_a = reqs_holder.memoryRequirements;

            accel_info.accelerationStructure = top_level_accel;
            vkGetAccelerationStructureScratchMemoryRequirementsNVX(vk.device, &accel_info, &reqs_holder);
            VkMemoryRequirements reqs_b = reqs_holder.memoryRequirements;

            // NOTE: do we really need vkGetAccelerationStructureScratchMemoryRequirementsNVX function in the API?
            // It should be possible to use vkGetBufferMemoryRequirements2 without introducing new API function
            // and without modifying standard way to query memory requirements for the resource.

            // Right now the spec does not provide additional guarantees related to scratch
            // buffer allocations, so the following asserts could fail.
            // Probably some things will be clarified in the future revisions.
            assert(reqs_a.alignment == reqs_b.alignment);
            assert(reqs_a.memoryTypeBits == reqs_b.memoryTypeBits);

            scratch_memory_requirements.size = std::max(reqs_a.size, reqs_b.size);
            scratch_memory_requirements.alignment = reqs_a.alignment;
            scratch_memory_requirements.memoryTypeBits = reqs_a.memoryTypeBits;
        }

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // NOTE: do not use vmaCreateBuffer since it does not allow to specify 'alignment'
        // returned from vkGetAccelerationStructureScratchMemoryRequirementsNVX.
        VK_CHECK(vmaAllocateMemory(vk.allocator, &scratch_memory_requirements, &alloc_create_info, &scratch_buffer_allocation, nullptr));

        VkBufferCreateInfo buffer_create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_create_info.size = scratch_memory_requirements.size;
        buffer_create_info.usage = VK_BUFFER_USAGE_RAYTRACING_BIT_NVX;

        VK_CHECK(vkCreateBuffer(vk.device, &buffer_create_info, nullptr, &scratch_buffer));
        vkGetBufferMemoryRequirements(vk.device, scratch_buffer, &VkMemoryRequirements()); // shut up validation layers
        VK_CHECK(vmaBindBufferMemory(vk.allocator, scratch_buffer_allocation, scratch_buffer));
    }

    // Build acceleration structures.
    Timestamp t;

    vk_execute(vk.command_pool, vk.queue,
        [this, &geometry](VkCommandBuffer command_buffer)
    {
        vkCmdBuildAccelerationStructureNVX(command_buffer,
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
            0, VK_NULL_HANDLE, 0,
            1, &geometry,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX, VK_FALSE, bottom_level_accel, VK_NULL_HANDLE,
            scratch_buffer, 0);

        VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX,
            0, 1, &barrier, 0, nullptr, 0, nullptr);

        vkCmdBuildAccelerationStructureNVX(command_buffer,
            VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
            1, instance_buffer, 0,
            0, nullptr,
            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NVX,
            VK_FALSE, top_level_accel, VK_NULL_HANDLE,
            scratch_buffer, 0);
    });
    printf("\nAcceleration structures build time = %lld microseconds\n", elapsed_microseconds(t));
}

void Raytracing_Resources::create_pipeline(const VkGeometryTrianglesNVX& model_triangles, VkImageView texture_view, VkSampler sampler) {

    descriptor_set_layout = Descriptor_Set_Layout()
        .storage_image  (0, VK_SHADER_STAGE_RAYGEN_BIT_NVX)
        .accelerator    (1, VK_SHADER_STAGE_RAYGEN_BIT_NVX)
        .uniform_buffer (2, VK_SHADER_STAGE_RAYGEN_BIT_NVX | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX)
        .storage_buffer (3, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX)
        .storage_buffer (4, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX)
        .sampled_image  (5, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX)
        .sampler        (6, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX)
        .create         ("rt_set_layout");

    // pipeline layout
    {
        VkPushConstantRange push_constant_ranges[2]; // show_texture_lods value
        push_constant_ranges[0].stageFlags  = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
        push_constant_ranges[0].offset      = 0;
        push_constant_ranges[0].size        = 4;
        push_constant_ranges[1].stageFlags  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;
        push_constant_ranges[1].offset      = 4;
        push_constant_ranges[1].size        = 4;

        VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        create_info.setLayoutCount          = 1;
        create_info.pSetLayouts             = &descriptor_set_layout;
        create_info.pushConstantRangeCount  = (uint32_t)std::size(push_constant_ranges);
        create_info.pPushConstantRanges     = push_constant_ranges;

        VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &pipeline_layout));
    }

    // pipeline
    {
        VkShaderModule rgen_shader = vk_load_spirv("spirv/rt_mesh.rgen.spv");
        VkShaderModule miss_shader = vk_load_spirv("spirv/rt_mesh.rmiss.spv");
        VkShaderModule chit_shader = vk_load_spirv("spirv/rt_mesh.rchit.spv");

        VkPipelineShaderStageCreateInfo stage_infos[3] {};
        stage_infos[0].sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[0].stage    = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
        stage_infos[0].module   = rgen_shader;
        stage_infos[0].pName    = "main";

        stage_infos[1].sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[1].stage    = VK_SHADER_STAGE_MISS_BIT_NVX;
        stage_infos[1].module   = miss_shader;
        stage_infos[1].pName    = "main";

        stage_infos[2].sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[2].stage    = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;
        stage_infos[2].module   = chit_shader;
        stage_infos[2].pName    = "main";

        uint32_t group_numbers[3] = { 0, 1, 2}; // [raygen] [miss] [chit]

        VkRaytracingPipelineCreateInfoNVX create_info { VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX };
        create_info.stageCount          = (uint32_t)std::size(stage_infos);
        create_info.pStages             = stage_infos;
        create_info.pGroupNumbers       = group_numbers;
        create_info.maxRecursionDepth   = 1;
        create_info.layout              = pipeline_layout;
        VK_CHECK(vkCreateRaytracingPipelinesNVX(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));

        vkDestroyShaderModule(vk.device, rgen_shader, nullptr);
        vkDestroyShaderModule(vk.device, miss_shader, nullptr);
        vkDestroyShaderModule(vk.device, chit_shader, nullptr);
    }

    // descriptor set
    {
        VkDescriptorSetAllocateInfo desc { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        desc.descriptorPool     = vk.descriptor_pool;
        desc.descriptorSetCount = 1;
        desc.pSetLayouts        = &descriptor_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(vk.device, &desc, &descriptor_set));

        Descriptor_Writes(descriptor_set)
            .accelerator(1, top_level_accel)
            .uniform_buffer(2, uniform_buffer.handle, 0, sizeof(Rt_Uniform_Buffer))
            .storage_buffer(3, model_triangles.indexData,
                               model_triangles.indexOffset,
                               model_triangles.indexCount * (model_triangles.indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4))
            .storage_buffer(4, model_triangles.vertexData,
                               model_triangles.vertexOffset, /* assume that position is the first vertex attribute */
                               model_triangles.vertexCount * sizeof(Vertex))
            .sampled_image(5, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .sampler(6, sampler);
    }
}
