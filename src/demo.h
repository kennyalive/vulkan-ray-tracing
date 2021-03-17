#pragma once

#include "matrix.h"
#include "vk_utils.h"
#include "vk.h"

#include "kernels/copy_to_swapchain.h"
#include "kernels/draw_mesh.h"
#include "kernels/raytrace_scene.h"

#include <vector>

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
    void create_depth_buffer();
    void destroy_depth_buffer();
    void draw_frame();
    void draw_rasterized_image();
    void draw_raytraced_image();
    void draw_imgui();
    void copy_output_image_to_swapchain();
    void do_imgui();

private:
    struct UI_Result {
        bool raytracing_toggled;
    };

    using Clock = std::chrono::high_resolution_clock;
    using Time  = std::chrono::time_point<Clock>;

    bool                        show_ui                 = true;
    bool                        vsync                   = true;
    bool                        animate                 = false;
    bool                        raytracing_active       = false;
    bool                        show_texture_lod        = false;
    bool                        spp4                    = false;

    Time                        last_frame_time;
    double                      sim_time;

    UI_Result                   ui_result;

    struct Depth_Buffer_Info {
        VkImage image;
        VkImageView image_view;
        VmaAllocation allocation;
    };
    Depth_Buffer_Info depth_info;

    VkRenderPass                render_pass;
    VkFramebuffer               framebuffer;
    VkRenderPass                ui_render_pass;
    VkFramebuffer               ui_framebuffer;
    Vk_Image                    output_image;
    GPU_Mesh                    gpu_mesh;
    Vk_Image                    texture;
    VkSampler                   sampler;

    Vector3                     camera_pos = Vector3(0, 0.5, 3.0);
    Matrix3x4                   model_transform;
    Matrix3x4                   view_transform;

    Copy_To_Swapchain copy_to_swapchain;
    Draw_Mesh draw_mesh;
    Raytrace_Scene raytrace_scene;

    GPU_Time_Keeper             time_keeper;
    struct {
        GPU_Time_Interval*      frame;
        GPU_Time_Interval*      draw;
        GPU_Time_Interval*      ui;
        GPU_Time_Interval*      compute_copy;
    } gpu_times;
};
