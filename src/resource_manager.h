#pragma once

#include "vk.h"
#include <vector>

class Resource_Manager {
public:
    void initialize(VkDevice device, VmaAllocator allocator);
    void release_resources();

    VkSemaphore             create_semaphore            (const char* name);
    VkCommandPool           create_command_pool         (const VkCommandPoolCreateInfo& desc, const char* name);
    VkDescriptorPool        create_descriptor_pool      (const VkDescriptorPoolCreateInfo& desc, const char* name);
    VkBuffer                create_buffer               (const VkBufferCreateInfo& desc, bool host_visible, void** mapped_data, const char* name);
    VkImage                 create_image                (const VkImageCreateInfo& desc, const char* name);
    VkImageView             create_image_view           (const VkImageViewCreateInfo& desc, const char* name);
    VkSampler               create_sampler              (const VkSamplerCreateInfo& desc, const char* name);
    VkRenderPass            create_render_pass          (const VkRenderPassCreateInfo& desc, const char* name);
    VkFramebuffer           create_framebuffer          (const VkFramebufferCreateInfo& desc, const char* name);
    VkDescriptorSetLayout   create_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& desc, const char* name);
    VkPipelineLayout        create_pipeline_layout      (const VkPipelineLayoutCreateInfo& desc, const char* name);
    VkPipeline              create_graphics_pipeline    (const VkGraphicsPipelineCreateInfo& desc, const char* name);
    VkShaderModule          create_shader_module        (const VkShaderModuleCreateInfo& desc, const char* name);

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
