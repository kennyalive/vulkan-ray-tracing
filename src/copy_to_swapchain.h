#pragma once

#include "vk.h"

struct Copy_To_Swapchain {
    VkDescriptorSetLayout           set_layout;
    VkPipelineLayout                pipeline_layout;
    VkPipeline                      pipeline;
    VkSampler                       point_sampler;
    std::vector<VkDescriptorSet>    sets; // per swapchain image

    void create();
    void destroy();
    void update_resolution_dependent_descriptors(VkImageView output_image_view);
};
