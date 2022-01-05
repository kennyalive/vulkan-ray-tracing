#include "copy_to_swapchain.h"
#include "vk_utils.h"

void Copy_To_Swapchain::create() {

    set_layout = Descriptor_Set_Layout()
        .sampler(0, VK_SHADER_STAGE_COMPUTE_BIT)
        .sampled_image(1, VK_SHADER_STAGE_COMPUTE_BIT)
        .storage_image(2, VK_SHADER_STAGE_COMPUTE_BIT)
        .create("copy_to_swapchain_set_layout");

    // pipeline layout
    {
        VkPushConstantRange range;
        range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        range.offset = 0;
        range.size = 8; // uint32 width + uint32 height

        VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        create_info.setLayoutCount = 1;
        create_info.pSetLayouts = &set_layout;
        create_info.pushConstantRangeCount = 1;
        create_info.pPushConstantRanges = &range;
        VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &pipeline_layout));
    }

    // pipeline
    {
        Shader_Module copy_shader("spirv/copy_to_swapchain.comp.spv");

        VkPipelineShaderStageCreateInfo compute_stage { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        compute_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_stage.module = copy_shader.handle;
        compute_stage.pName = "main";

        VkComputePipelineCreateInfo create_info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        create_info.stage = compute_stage;
        create_info.layout = pipeline_layout;
        VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));
    }

    // point sampler
    {
        VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &point_sampler));
        vk_set_debug_name(point_sampler, "point_sampler");
    }
}

void Copy_To_Swapchain::destroy() {
    vkDestroyDescriptorSetLayout(vk.device, set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroySampler(vk.device, point_sampler, nullptr);
    sets.clear();
}

void Copy_To_Swapchain::update_resolution_dependent_descriptors(VkImageView output_image_view) {
    if (sets.size() < vk.swapchain_info.images.size())
    {
        size_t n = vk.swapchain_info.images.size() - sets.size();
        for (size_t i = 0; i < n; i++)
        {
            VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            alloc_info.descriptorPool = vk.descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &set_layout;

            VkDescriptorSet set;
            VK_CHECK(vkAllocateDescriptorSets(vk.device, &alloc_info, &set));
            sets.push_back(set);

            Descriptor_Writes(set).sampler(0, point_sampler);
        }
    }

    for (size_t i = 0; i < vk.swapchain_info.images.size(); i++) {
        Descriptor_Writes(sets[i])
            .sampled_image(1, output_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .storage_image(2, vk.swapchain_info.image_views[i]);
    }
}

void Copy_To_Swapchain::dispatch() {
    const uint32_t group_size_x = 32; // according to shader
    const uint32_t group_size_y = 32;

    uint32_t group_count_x = (vk.surface_size.width + group_size_x - 1) / group_size_x;
    uint32_t group_count_y = (vk.surface_size.height + group_size_y - 1) / group_size_y;

    uint32_t push_constants[] = { vk.surface_size.width, vk.surface_size.height };

    vkCmdPushConstants(vk.command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);
    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &sets[vk.swapchain_image_index], 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdDispatch(vk.command_buffer, group_count_x, group_count_y, 1);
}
