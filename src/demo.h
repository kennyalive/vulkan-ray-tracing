#pragma once

#include "vk.h"
#include "sdl/SDL_syswm.h"
#include <vector>

struct Uniform_Buffer;

struct Demo_Create_Info {
    Vk_Create_Info  vk_create_info;
    SDL_Window*     window;
};

struct Copy_To_Swapchain {
    VkDescriptorSetLayout           set_layout;
    VkPipelineLayout                pipeline_layout;
    VkPipeline                      pipeline;
    std::vector<VkDescriptorSet>    sets; // per swapchain image

    void create_resources(VkDescriptorSetLayout global_descriptor_set_layout, VkDescriptorPool descriptor_pool, const Vk_Image& output_image);
    void destroy_resources(VkDescriptorPool descriptor_pool);
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
    void create_acceleration_structures();

    void create_render_passes();

    void create_framebuffers();
    void destroy_framebuffers();

    void create_global_descriptor_set();

    void create_descriptor_sets();
    void create_pipeline_layouts();
    void create_shader_modules();
    void create_pipelines();

    void create_raytracing_pipeline();
    void update_raytracing_output_image_descriptor();
    void create_shader_binding_table();

    void create_output_image();

    void setup_imgui();
    void release_imgui();
    void do_imgui();

    void update_uniform_buffer();

private:
    const Demo_Create_Info      create_info;
    bool                        show_ui                 = true;
    bool                        vsync                   = true;
    bool                        animate                 = false;
    bool                        raytracing              = false;

    VkSampler                   sampler                 = VK_NULL_HANDLE;
    Vk_Image                    texture;

    VkBuffer                    uniform_buffer          = VK_NULL_HANDLE;
    Uniform_Buffer*             mapped_uniform_buffer   = nullptr;

    VkBuffer                    vertex_buffer           = VK_NULL_HANDLE;
    VkBuffer                    index_buffer            = VK_NULL_HANDLE;
    uint32_t                    model_vertex_count      = 0;
    uint32_t                    model_index_count       = 0;

    VkRenderPass                color_depth_render_pass = VK_NULL_HANDLE;
    VkFramebuffer               color_depth_framebuffer = VK_NULL_HANDLE;

    VkRenderPass                color_render_pass       = VK_NULL_HANDLE;
    VkFramebuffer               color_framebuffer       = VK_NULL_HANDLE;

    VkDescriptorSetLayout       global_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSet             global_descriptor_set        = VK_NULL_HANDLE;

    VkDescriptorPool            descriptor_pool         = VK_NULL_HANDLE;
    VkDescriptorSetLayout       descriptor_set_layout   = VK_NULL_HANDLE;
    VkDescriptorSet             descriptor_set          = VK_NULL_HANDLE;

    VkShaderModule              model_vs                = VK_NULL_HANDLE;
    VkShaderModule              model_fs                = VK_NULL_HANDLE;

    VkPipelineLayout            pipeline_layout         = VK_NULL_HANDLE;
    VkPipeline                  pipeline                = VK_NULL_HANDLE;

    //
    // Raytracing resources.
    //
    uint32_t                    shader_header_size                  = 0;

    VkAccelerationStructureNVX  bottom_level_accel                  = VK_NULL_HANDLE;
    VmaAllocation               bottom_level_accel_allocation       = VK_NULL_HANDLE;

    VkAccelerationStructureNVX  top_level_accel                     = VK_NULL_HANDLE;
    VmaAllocation               top_level_accel_allocation          = VK_NULL_HANDLE;

    VkDescriptorSetLayout       raytracing_descriptor_set_layout    = VK_NULL_HANDLE;
    VkPipelineLayout            raytracing_pipeline_layout          = VK_NULL_HANDLE;
    VkPipeline                  raytracing_pipeline                 = VK_NULL_HANDLE;
    VkDescriptorSet             raytracing_descriptor_set           = VK_NULL_HANDLE;

    VkBuffer                    shader_binding_table                = VK_NULL_HANDLE;

    //
    // Output image.
    //
    Vk_Image output_image = {};
    Copy_To_Swapchain copy_to_swapchain = {};
};
