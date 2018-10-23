#pragma once

#include "copy_to_swapchain.h"
#include "matrix.h"
#include "raster_resources.h"
#include "rt_resources.h"
#include "vk.h"

#include "sdl/SDL_syswm.h"
#include <vector>

class Vk_Demo {
public:
    void initialize(Vk_Create_Info vk_create_info, SDL_Window* sdl_window);
    void shutdown();

    void release_resolution_dependent_resources();
    void restore_resolution_dependent_resources();
    bool vsync_enabled() const { return vsync; }

    void run_frame();

private:
    void setup_imgui();
    void release_imgui();
    void do_imgui();

    void draw_rasterized_image();
    void draw_raytraced_image();
    void draw_imgui();
    void copy_output_image_to_swapchain();

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

    Vector                      camera_pos = Vector(0, 0.5, 3.0);
    Matrix3x4                   model_transform;
    Matrix3x4                   view_transform;

    Rasterization_Resources     raster;
    Raytracing_Resources        rt;
};
