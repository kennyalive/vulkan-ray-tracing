#pragma once

#include "gpu_mesh.h"
#include "linear_algebra.h"
#include "vk_utils.h"

#include "kernels/copy_to_swapchain.h"
#include "kernels/draw_mesh.h"
#include "kernels/raytrace_scene.h"

struct GLFWwindow;

class Vk_Demo {
public:
    void initialize(GLFWwindow* glfw_window, bool enable_validation_layers);
    void shutdown();

    void release_resolution_dependent_resources();
    void restore_resolution_dependent_resources();
    bool vsync_enabled() const { return vsync; }
    void run_frame();

private:
    void draw_frame();
    void draw_rasterized_image();
    void draw_raytraced_image();
    void draw_imgui();
    void copy_output_image_to_swapchain();
    void do_imgui();

private:
    using Clock = std::chrono::high_resolution_clock;
    using Time  = std::chrono::time_point<Clock>;

    bool show_ui = true;
    bool vsync = true;
    bool animate = false;
    bool raytracing_active = true;
    bool show_texture_lod = false;
    bool spp4 = false;

    Time last_frame_time;
    double sim_time;
    Vector3 camera_pos = Vector3(0, 0.5, 3.0);

    GPU_Time_Keeper time_keeper;
    struct {
        GPU_Time_Interval* frame;
        GPU_Time_Interval* draw;
        GPU_Time_Interval* ui;
        GPU_Time_Interval* compute_copy;
    } gpu_times;

    VkRenderPass ui_render_pass;
    VkFramebuffer ui_framebuffer;
    Vk_Image depth_buffer_image;
    Vk_Image output_image;
    GPU_Mesh gpu_mesh;
    Vk_Image texture;
    VkSampler sampler;
    Copy_To_Swapchain copy_to_swapchain;
    Draw_Mesh draw_mesh;
    Raytrace_Scene raytrace_scene;
};
