#pragma once

#include "vk.h"

struct Copy_To_Swapchain {
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkSampler point_sampler;
    Vk_Buffer descriptor_buffer; // contains descriptors per swapchain image

    VkDeviceSize layout_size_in_bytes = 0;

    void create();
    void destroy();
    void update_resolution_dependent_descriptors(VkImageView output_image_view);
    void dispatch();
};
