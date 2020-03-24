#include "mesh.h"
#include "rt_resources.h"
#include "vk_utils.h"

#include <algorithm>
#include <cassert>

struct Rt_Uniform_Buffer {
    Matrix3x4 camera_to_world;
};

void Raytracing_Resources::create(const GPU_Mesh& gpu_mesh, VkImageView texture_view, VkSampler sampler) {
    uniform_buffer = vk_create_mapped_buffer(static_cast<VkDeviceSize>(sizeof(Rt_Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &(void*&)mapped_uniform_buffer, "rt_uniform_buffer");

    accelerator = create_intersection_accelerator({gpu_mesh}, true);
    create_pipeline(gpu_mesh, texture_view, sampler);

    // Shader binding table.
    {
        uint32_t miss_offset = round_up(properties.shaderGroupHandleSize /* raygen slot*/, properties.shaderGroupBaseAlignment);
        uint32_t hit_offset = round_up(miss_offset + properties.shaderGroupHandleSize /* miss slot */, properties.shaderGroupBaseAlignment);

        uint32_t sbt_buffer_size = hit_offset + properties.shaderGroupHandleSize;

        void* mapped_memory;
        shader_binding_table = vk_create_mapped_buffer(sbt_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &mapped_memory, "shader_binding_table");

        // raygen slot
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, pipeline, 0, 1, properties.shaderGroupHandleSize, mapped_memory));

        // miss slot
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, pipeline, 1, 1, properties.shaderGroupHandleSize, (uint8_t*)mapped_memory + miss_offset));

        // hit slot
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, pipeline, 2, 1, properties.shaderGroupHandleSize, (uint8_t*)mapped_memory + hit_offset));
    }
}

void Raytracing_Resources::destroy() {
    uniform_buffer.destroy();
    shader_binding_table.destroy();
    accelerator.destroy();

    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
}

void Raytracing_Resources::update_output_image_descriptor(VkImageView output_image_view) {
    Descriptor_Writes(descriptor_set).storage_image(0, output_image_view);
}

void Raytracing_Resources::update(const Matrix3x4& model_transform, const Matrix3x4& camera_to_world_transform) {
    assert(accelerator.bottom_level_accels.size() == 1);

    VkAccelerationStructureInstanceKHR& instance = *accelerator.mapped_instance_buffer;
    memcpy(&instance.transform.matrix[0][0], &model_transform.a[0][0], 12 * sizeof(float));
    instance.instanceCustomIndex                        = 0;
    instance.mask                                       = 0xff;
    instance.instanceShaderBindingTableRecordOffset     = 0;
    instance.flags                                      = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference             = accelerator.bottom_level_accel_device_addresses[0];

    Rt_Uniform_Buffer& uniform_buffer = *mapped_uniform_buffer;
    uniform_buffer.camera_to_world = camera_to_world_transform;
}

void Raytracing_Resources::create_pipeline(const GPU_Mesh& gpu_mesh, VkImageView texture_view, VkSampler sampler) {
    descriptor_set_layout = Descriptor_Set_Layout()
        .storage_image  (0, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .accelerator    (1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .uniform_buffer (2, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .storage_buffer (3, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .storage_buffer (4, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .sampled_image  (5, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .sampler        (6, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .create         ("rt_set_layout");

    // pipeline layout
    {
        VkPushConstantRange push_constant_ranges[2]; // show_texture_lods value
        push_constant_ranges[0].stageFlags  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        push_constant_ranges[0].offset      = 0;
        push_constant_ranges[0].size        = 4;
        push_constant_ranges[1].stageFlags  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
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
        stage_infos[0].stage    = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        stage_infos[0].module   = rgen_shader;
        stage_infos[0].pName    = "main";

        stage_infos[1].sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[1].stage    = VK_SHADER_STAGE_MISS_BIT_KHR;
        stage_infos[1].module   = miss_shader;
        stage_infos[1].pName    = "main";

        stage_infos[2].sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[2].stage    = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        stage_infos[2].module   = chit_shader;
        stage_infos[2].pName    = "main";

        VkRayTracingShaderGroupCreateInfoKHR shader_groups[3];

        {
            auto& group = shader_groups[0];
            group = VkRayTracingShaderGroupCreateInfoKHR { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
            group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            group.generalShader = 0;
            group.closestHitShader = VK_SHADER_UNUSED_KHR;
            group.anyHitShader = VK_SHADER_UNUSED_KHR;
            group.intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        {
            auto& group = shader_groups[1];
            group = VkRayTracingShaderGroupCreateInfoKHR { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
            group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            group.generalShader = 1;
            group.closestHitShader = VK_SHADER_UNUSED_KHR;
            group.anyHitShader = VK_SHADER_UNUSED_KHR;
            group.intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        {
            auto& group = shader_groups[2];
            group = VkRayTracingShaderGroupCreateInfoKHR { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
            group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            group.generalShader = VK_SHADER_UNUSED_KHR;
            group.closestHitShader = 2;
            group.anyHitShader = VK_SHADER_UNUSED_KHR;
            group.intersectionShader = VK_SHADER_UNUSED_KHR;
        }

        VkRayTracingPipelineCreateInfoKHR create_info { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
        create_info.stageCount          = (uint32_t)std::size(stage_infos);
        create_info.pStages             = stage_infos;
        create_info.groupCount          = (uint32_t)std::size(shader_groups);
        create_info.pGroups             = shader_groups;
        create_info.maxRecursionDepth   = 1;
        create_info.layout              = pipeline_layout;
        VK_CHECK(vkCreateRayTracingPipelinesKHR(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));

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
            .accelerator(1, accelerator.top_level_accel)
            .uniform_buffer(2, uniform_buffer.handle, 0, sizeof(Rt_Uniform_Buffer))

            .storage_buffer(3,
                gpu_mesh.index_buffer.handle,
                0,
                gpu_mesh.index_count * 4 /*VK_INDEX_TYPE_UINT32*/)

            .storage_buffer(4,
                gpu_mesh.vertex_buffer.handle,
                0, /* assume that position is the first vertex attribute */
                gpu_mesh.vertex_count * sizeof(Vertex))

            .sampled_image(5, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .sampler(6, sampler);
    }
}
