#pragma once

#include "vk.h"
#include <vector>

struct SDL_SysWMinfo;

class Vk_Demo {
public:
    Vk_Demo(int window_width, int window_height, const SDL_SysWMinfo& window_sys_info);
    ~Vk_Demo();

    void run_frame();

private:
    void upload_textures();
    void upload_geometry();

    void create_render_passes();
    void create_descriptor_sets();
    void create_pipeline_layouts();
    void create_shader_modules();
    void create_pipelines();

    void update_uniform_buffer();

private:
    //
    // Textures.
    //
    VkSampler               sampler = VK_NULL_HANDLE;
    Vk_Image                texture;
   
    //
    // Buffers.
    //
    VkBuffer                uniform_buffer = VK_NULL_HANDLE;
    void*                   uniform_buffer_ptr = nullptr;

    VkBuffer                vertex_buffer = VK_NULL_HANDLE;
    VkBuffer                index_buffer = VK_NULL_HANDLE;
    uint32_t                model_index_count = 0;

    //
    // Render passes.
    //
    VkRenderPass            render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchain_framebuffers;

    //
    // Descriptor sets.
    //
    VkDescriptorPool        descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout   descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet         descriptor_set = VK_NULL_HANDLE;

    //
    // Pipelines.
    //
    VkPipelineLayout        pipeline_layout = VK_NULL_HANDLE;
    VkPipeline              pipeline = VK_NULL_HANDLE;

    //
    // Shader modules.
    //
    VkShaderModule          model_vs = VK_NULL_HANDLE;
    VkShaderModule          model_fs = VK_NULL_HANDLE;
};
