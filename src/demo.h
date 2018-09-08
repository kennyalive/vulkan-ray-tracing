#pragma once

#include "vk.h"
#include "sdl/SDL_syswm.h"
#include <vector>

struct Demo_Create_Info {
    Vk_Create_Info  vk_create_info;
    SDL_Window*     window;
};

class Vk_Demo {
public:
    Vk_Demo(const Demo_Create_Info& create_info);
    ~Vk_Demo();

    void run_frame(bool draw_only_background = false);

    void release_resolution_dependent_resources();
    void restore_resolution_dependent_resources();

private:
    void upload_textures();
    void upload_geometry();

    void create_render_passes();

    void create_framebuffers();
    void destroy_framebuffers();

    void create_descriptor_sets();
    void create_pipeline_layouts();
    void create_shader_modules();
    void create_pipelines();

    void setup_imgui();
    void release_imgui();
    void do_imgui();

    void update_uniform_buffer();

private:
    const Demo_Create_Info      create_info;
    bool                        show_ui                 = true;
    bool                        vsync                   = true;

    VkSampler                   sampler                 = VK_NULL_HANDLE;
    Vk_Image                    texture;

    VkBuffer                    uniform_buffer          = VK_NULL_HANDLE;
    void*                       uniform_buffer_ptr      = nullptr;

    VkBuffer                    vertex_buffer           = VK_NULL_HANDLE;
    VkBuffer                    index_buffer            = VK_NULL_HANDLE;
    uint32_t                    model_index_count       = 0;

    VkRenderPass                render_pass             = VK_NULL_HANDLE;
    std::vector<VkFramebuffer>  swapchain_framebuffers;

    VkDescriptorPool            descriptor_pool         = VK_NULL_HANDLE;
    VkDescriptorSetLayout       descriptor_set_layout   = VK_NULL_HANDLE;
    VkDescriptorSet             descriptor_set          = VK_NULL_HANDLE;

    VkShaderModule              model_vs                = VK_NULL_HANDLE;
    VkShaderModule              model_fs                = VK_NULL_HANDLE;

    VkPipelineLayout            pipeline_layout         = VK_NULL_HANDLE;
    VkPipeline                  pipeline                = VK_NULL_HANDLE;
};
