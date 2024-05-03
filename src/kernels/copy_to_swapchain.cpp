#include "copy_to_swapchain.h"
#include "vk_utils.h"

void Copy_To_Swapchain::create() {

    set_layout = Descriptor_Set_Layout()
        .sampler(0, VK_SHADER_STAGE_COMPUTE_BIT)
        .sampled_image(1, VK_SHADER_STAGE_COMPUTE_BIT)
        .storage_image(2, VK_SHADER_STAGE_COMPUTE_BIT)
        .create("copy_to_swapchain_set_layout");

    pipeline_layout = create_pipeline_layout(
        { set_layout },
        { VkPushConstantRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, 8 /*uint32 width + uint32 height*/} },
        "copy_to_swapchain_pipeline_layout");

    pipeline = create_compute_pipeline("spirv/copy_to_swapchain.comp.spv", pipeline_layout,
        "copy_to_swapchain_pipeline");

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
            VkDescriptorSet set = allocate_descriptor_set(set_layout);
            sets.push_back(set);
            Descriptor_Writes(set).sampler(0, point_sampler);
        }
    }

    for (size_t i = 0; i < vk.swapchain_info.images.size(); i++) {
        Descriptor_Writes(sets[i])
            .sampled_image(1, output_image_view, VK_IMAGE_LAYOUT_GENERAL)
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
