#include "copy_to_swapchain.h"
#include "lib.h"

void Copy_To_Swapchain::create()
{
    set_layout = Vk_Descriptor_Set_Layout()
        .sampler(0, VK_SHADER_STAGE_COMPUTE_BIT)
        .sampled_image(1, VK_SHADER_STAGE_COMPUTE_BIT)
        .storage_image(2, VK_SHADER_STAGE_COMPUTE_BIT)
        .create("copy_to_swapchain_set_layout");

    pipeline_layout = vk_create_pipeline_layout(
        { set_layout },
        { VkPushConstantRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, 8 /*uint32 width + uint32 height*/} },
        "copy_to_swapchain_pipeline_layout");

    Vk_Shader_Module compute_shader(get_resource_path("spirv/copy_to_swapchain.comp.spv"));

    pipeline = vk_create_compute_pipeline(compute_shader.handle, pipeline_layout, "copy_to_swapchain_pipeline");

    // point sampler
    {
        VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &point_sampler));
        vk_set_debug_name(point_sampler, "point_sampler");
    }
}

void Copy_To_Swapchain::destroy() {
    descriptor_buffer.destroy();
    vkDestroyDescriptorSetLayout(vk.device, set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroySampler(vk.device, point_sampler, nullptr);
}

void Copy_To_Swapchain::update_resolution_dependent_descriptors(VkImageView output_image_view) {
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
    VkPhysicalDeviceProperties2 physical_device_properties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    physical_device_properties.pNext = &descriptor_buffer_properties;
    vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

    layout_size_in_bytes = 0;
    vkGetDescriptorSetLayoutSizeEXT(vk.device, set_layout, &layout_size_in_bytes);
    std::vector<uint8_t> descriptor_data(layout_size_in_bytes * vk.swapchain_info.images.size());

    for (size_t i = 0; i < vk.swapchain_info.images.size(); i++) {
        // Descriptor 0 (sampler)
        {
            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptor_info.data.pSampler = &point_sampler;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, set_layout, 0, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.samplerDescriptorSize,
                descriptor_data.data() + i * layout_size_in_bytes +  offset);
        }
        // Descriptor 1 (sampled image)
        {
            VkDescriptorImageInfo image_info;
            image_info.imageView = output_image_view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descriptor_info.data.pSampledImage = &image_info;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, set_layout, 1, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.sampledImageDescriptorSize,
                descriptor_data.data() + i * layout_size_in_bytes + offset);
        }
        // Descriptor 2 (storage image)
        {
            VkDescriptorImageInfo image_info;
            image_info.imageView = vk.swapchain_info.image_views[i];
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptor_info.data.pStorageImage = &image_info;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, set_layout, 2, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.storageImageDescriptorSize,
                descriptor_data.data() + i * layout_size_in_bytes + offset);
        }
    }

    descriptor_buffer.destroy();

    VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
        VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    descriptor_buffer = vk_create_buffer_with_alignment(
        layout_size_in_bytes * vk.swapchain_info.images.size(),
        usage,
        (uint32_t)descriptor_buffer_properties.descriptorBufferOffsetAlignment,
        descriptor_data.data(), "copy_to_swapchain_descriptor_buffer");
}

void Copy_To_Swapchain::dispatch() {
    const uint32_t group_size_x = 32; // according to shader
    const uint32_t group_size_y = 32;

    uint32_t group_count_x = (vk.surface_size.width + group_size_x - 1) / group_size_x;
    uint32_t group_count_y = (vk.surface_size.height + group_size_y - 1) / group_size_y;

    uint32_t push_constants[] = { vk.surface_size.width, vk.surface_size.height };

    VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT };
    descriptor_buffer_binding_info.address = descriptor_buffer.device_address;
    descriptor_buffer_binding_info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    vkCmdBindDescriptorBuffersEXT(vk.command_buffer, 1, &descriptor_buffer_binding_info);

    const uint32_t buffer_index = 0;
    const VkDeviceSize set_offset = layout_size_in_bytes * vk.swapchain_image_index;
    vkCmdSetDescriptorBufferOffsetsEXT(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &buffer_index, &set_offset);

    vkCmdPushConstants(vk.command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), push_constants);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdDispatch(vk.command_buffer, group_count_x, group_count_y, 1);
}
