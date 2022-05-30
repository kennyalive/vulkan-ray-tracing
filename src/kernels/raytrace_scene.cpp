#include "raytrace_scene.h"
#include "gpu_mesh.h"
#include "triangle_mesh.h"
#include "vk_utils.h"
#include <cassert>

namespace {
struct Uniform_Buffer {
    Matrix3x4 camera_to_world;
};
}

void Raytrace_Scene::create(const GPU_Mesh& gpu_mesh, VkImageView texture_view, VkSampler sampler) {
    uniform_buffer = vk_create_mapped_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &(void*&)mapped_uniform_buffer, "rt_uniform_buffer");

    accelerator = create_intersection_accelerator({gpu_mesh});
    create_pipeline(gpu_mesh, texture_view, sampler);

    // shader binding table
    {
        uint32_t miss_offset = round_up(properties.shaderGroupHandleSize /* raygen slot*/, properties.shaderGroupBaseAlignment);
        uint32_t hit_offset = round_up(miss_offset + properties.shaderGroupHandleSize /* miss slot */, properties.shaderGroupBaseAlignment);
        uint32_t sbt_buffer_size = hit_offset + properties.shaderGroupHandleSize;

        std::vector<uint8_t> data(sbt_buffer_size);
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, pipeline, 0, 1, properties.shaderGroupHandleSize, data.data() + 0)); // raygen slot
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, pipeline, 1, 1, properties.shaderGroupHandleSize, data.data() + miss_offset)); // miss slot
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, pipeline, 2, 1, properties.shaderGroupHandleSize, data.data() + hit_offset)); // hit slot
        shader_binding_table = vk_create_buffer(sbt_buffer_size, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT, data.data(), "shader_binding_table");
    }
}

void Raytrace_Scene::destroy() {
    uniform_buffer.destroy();
    shader_binding_table.destroy();
    accelerator.destroy();

    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
}

void Raytrace_Scene::update_output_image_descriptor(VkImageView output_image_view) {
    Descriptor_Writes(descriptor_set).storage_image(0, output_image_view);
}

void Raytrace_Scene::update(const Matrix3x4& model_transform, const Matrix3x4& camera_to_world_transform) {
    assert(accelerator.bottom_level_accels.size() == 1);

    VkAccelerationStructureInstanceKHR& instance = *accelerator.mapped_instance_buffer;
    memcpy(&instance.transform.matrix[0][0], &model_transform.a[0][0], 12 * sizeof(float));
    instance.instanceCustomIndex = 0;
    instance.mask = 0xff;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = accelerator.bottom_level_accels[0].device_address;

    memcpy(mapped_uniform_buffer, &camera_to_world_transform, sizeof(camera_to_world_transform));
}

void Raytrace_Scene::create_pipeline(const GPU_Mesh& gpu_mesh, VkImageView texture_view, VkSampler sampler) {
    descriptor_set_layout = Descriptor_Set_Layout()
        .storage_image (0, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .accelerator (1, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        .uniform_buffer (2, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .storage_buffer (3, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .storage_buffer (4, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .sampled_image (5, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .sampler (6, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        .create ("rt_set_layout");

    pipeline_layout = create_pipeline_layout(
        { descriptor_set_layout },
        { VkPushConstantRange{VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, 4}, 
          VkPushConstantRange{VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 4, 4} },
        "rt_pipeline_layout"
    );

    // pipeline
    {
        Shader_Module rgen_shader("spirv/rt_mesh.rgen.spv");
        Shader_Module miss_shader("spirv/rt_mesh.rmiss.spv");
        Shader_Module chit_shader("spirv/rt_mesh.rchit.spv");

        VkPipelineShaderStageCreateInfo stage_infos[3] {};
        stage_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        stage_infos[0].module = rgen_shader.handle;
        stage_infos[0].pName = "main";

        stage_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        stage_infos[1].module = miss_shader.handle;
        stage_infos[1].pName = "main";

        stage_infos[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        stage_infos[2].module = chit_shader.handle;
        stage_infos[2].pName = "main";

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
        create_info.flags = VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR |
                            VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR;
        create_info.stageCount = (uint32_t)std::size(stage_infos);
        create_info.pStages = stage_infos;
        create_info.groupCount = (uint32_t)std::size(shader_groups);
        create_info.pGroups = shader_groups;
        create_info.maxPipelineRayRecursionDepth = 1;
        create_info.layout = pipeline_layout;
        VK_CHECK(vkCreateRayTracingPipelinesKHR(vk.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));
    }

    descriptor_set = allocate_descriptor_set(descriptor_set_layout);
    Descriptor_Writes(descriptor_set)
        .accelerator(1, accelerator.top_level_accel.aceleration_structure)
        .uniform_buffer(2, uniform_buffer.handle, 0, sizeof(Uniform_Buffer))

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

void Raytrace_Scene::dispatch(bool spp4, bool show_texture_lod) {
    accelerator.rebuild_top_level_accel(vk.command_buffer);

    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);

    uint32_t push_constants[2] = { spp4, show_texture_lod };
    vkCmdPushConstants(vk.command_buffer, pipeline_layout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, 4, &push_constants[0]);
    vkCmdPushConstants(vk.command_buffer, pipeline_layout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 4, 4, &push_constants[1]);

    const VkBuffer sbt = shader_binding_table.handle;
    const uint32_t sbt_slot_size = properties.shaderGroupHandleSize;
    const uint32_t miss_offset = round_up(sbt_slot_size /* raygen slot*/, properties.shaderGroupBaseAlignment);
    const uint32_t hit_offset = round_up(miss_offset + sbt_slot_size /* miss slot */, properties.shaderGroupBaseAlignment);

    VkStridedDeviceAddressRegionKHR raygen_sbt{};
    raygen_sbt.deviceAddress = shader_binding_table.device_address + 0;
    raygen_sbt.stride = sbt_slot_size;
    raygen_sbt.size = sbt_slot_size;

    VkStridedDeviceAddressRegionKHR miss_sbt{};
    miss_sbt.deviceAddress = shader_binding_table.device_address + miss_offset;
    miss_sbt.stride = sbt_slot_size;
    miss_sbt.size = sbt_slot_size;

    VkStridedDeviceAddressRegionKHR chit_sbt{};
    chit_sbt.deviceAddress = shader_binding_table.device_address + hit_offset;
    chit_sbt.stride = sbt_slot_size;
    chit_sbt.size = sbt_slot_size;

    VkStridedDeviceAddressRegionKHR callable_sbt{};

    vkCmdTraceRaysKHR(vk.command_buffer, &raygen_sbt, &miss_sbt, &chit_sbt, &callable_sbt,
        vk.surface_size.width, vk.surface_size.height, 1);
}
