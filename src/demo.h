#pragma once

#include "matrix.h"
#include "vk.h"

#include "rasterization_resources.h"
#include "raytracing_resources.h"

#include "sdl/SDL_syswm.h"
#include <vector>

struct Copy_To_Swapchain {
    VkDescriptorSetLayout           set_layout;
    VkPipelineLayout                pipeline_layout;
    VkPipeline                      pipeline;
    VkSampler                       point_sampler;
    std::vector<VkDescriptorSet>    sets; // per swapchain image

    void create(VkImageView output_image_view);
    void destroy();
    void update_resolution_dependent_descriptors(VkImageView output_image_view);
};

class Vk_Demo {
public:
    void initialize(Vk_Create_Info vk_create_info, SDL_Window* sdl_window);
    void shutdown();

    void release_resolution_dependent_resources();
    void restore_resolution_dependent_resources();

    void run_frame();

private:
    void create_ui_framebuffer();
    void destroy_ui_framebuffer();
    void create_output_image();

    void setup_imgui();
    void release_imgui();
    void do_imgui();

    void draw_rasterized_image();
    void draw_raytraced_image();

private:
    using Clock = std::chrono::high_resolution_clock;
    using Time  = std::chrono::time_point<Clock>;

    SDL_Window*                 sdl_window;

    bool                        show_ui                 = true;
    bool                        vsync                   = true;
    bool                        animate                 = false;
    bool                        raytracing              = false;

    Time                        last_frame_time;
    double                      sim_time;

    VkRenderPass                ui_render_pass;
    VkFramebuffer               ui_framebuffer;
    Vk_Image                    output_image;
    Copy_To_Swapchain           copy_to_swapchain;

    Vk_Buffer                   vertex_buffer;
    Vk_Buffer                   index_buffer;
    uint32_t                    model_vertex_count;
    uint32_t                    model_index_count;
    Vk_Image                    texture;
    VkSampler                   sampler;

    Matrix3x4                   model_transform;
    Matrix3x4                   view_transform;

    Rasterization_Resources     raster;
    Raytracing_Resources        rt;
};
