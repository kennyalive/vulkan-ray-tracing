#pragma once

#include "vk.h"
#include <vector>

class Resource_Manager {
public:
    void initialize(VkDevice device, VmaAllocator allocator);
    void release_resources();

    VkSemaphore             create_semaphore            ();
    VkCommandPool           create_command_pool         (const VkCommandPoolCreateInfo& desc);
    VkDescriptorPool        create_descriptor_pool      (const VkDescriptorPoolCreateInfo& desc);
    VkBuffer                create_buffer               (const VkBufferCreateInfo& desc, bool host_visible = false, void** mapped_data = nullptr);
    VkImage                 create_image                (const VkImageCreateInfo& desc);
    VkImageView             create_image_view           (const VkImageViewCreateInfo& desc);
    VkSampler               create_sampler              (const VkSamplerCreateInfo& desc);
    VkRenderPass            create_render_pass          (const VkRenderPassCreateInfo& desc);
    VkFramebuffer           create_framebuffer          (const VkFramebufferCreateInfo& desc);
    VkDescriptorSetLayout   create_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& desc);
    VkPipelineLayout        create_pipeline_layout      (const VkPipelineLayoutCreateInfo& desc);
    VkPipeline              create_graphics_pipeline    (const VkGraphicsPipelineCreateInfo& desc);
    VkShaderModule          create_shader_module        (const VkShaderModuleCreateInfo& desc);

private:
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;

    struct Buffer_Info {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    struct Image_Info {
        VkImage image;
        VmaAllocation allocation;
    };

    std::vector<VkSemaphore>            semaphores;
    std::vector<VkCommandPool>          command_pools;
    std::vector<VkDescriptorPool>       descriptor_pools;
    std::vector<Buffer_Info>            buffers;
    std::vector<Image_Info>             images;
    std::vector<VkImageView>            image_views;
    std::vector<VkSampler>              samplers;
    std::vector<VkDescriptorSetLayout>  descriptor_set_layouts;
    std::vector<VkRenderPass>           render_passes;
    std::vector<VkFramebuffer>          framebuffers;
    std::vector<VkPipelineLayout>       pipeline_layouts;
    std::vector<VkPipeline>             graphics_pipelines;
    std::vector<VkShaderModule>         shader_modules;
};

Resource_Manager* get_resource_manager();
