#pragma once

#include "matrix.h"
#include "vk.h"

#include "sdl/SDL_syswm.h"
#include <vector>

struct Uniform_Buffer;

struct Rasterization_Resources {
    VkDescriptorSetLayout       descriptor_set_layout;
    VkPipelineLayout            pipeline_layout;
    VkPipeline                  pipeline;
    VkDescriptorSet             descriptor_set;

    VkRenderPass                render_pass;
    VkFramebuffer               framebuffer;

    Vk_Buffer                   uniform_buffer;
    Uniform_Buffer*             mapped_uniform_buffer;

    void create(
        VkImageView texture_view,
        VkSampler sample,
        VkImageView output_image_view
    );

    void destroy();
    void create_framebuffer(VkImageView output_image_view);
    void destroy_framebuffer();
    void update(const Matrix3x4& model_transform, const Matrix3x4& view_transform);
};

// Use this definition while waiting for update to official headers.
struct VkInstanceNVX {
    Matrix3x4   transform;
    uint32_t    instance_id : 24;
    uint32_t    instance_mask : 8;
    uint32_t    instance_contribution_to_hit_group_index : 24;
    uint32_t    flags : 8;
    uint64_t    acceleration_structure_handle;
};

struct Raytracing_Resources {
    uint32_t                    shader_header_size;

    VkAccelerationStructureNVX  bottom_level_accel;
    VmaAllocation               bottom_level_accel_allocation;
    uint64_t                    bottom_level_accel_handle;

    VkAccelerationStructureNVX  top_level_accel;
    VmaAllocation               top_level_accel_allocation;

    VkBuffer                    scratch_buffer;
    VmaAllocation               scratch_buffer_allocation;

    VkBuffer                    instance_buffer;
    VmaAllocation               instance_buffer_allocation;
    VkInstanceNVX*              mapped_instance_buffer;

    VkDescriptorSetLayout       descriptor_set_layout;
    VkPipelineLayout            pipeline_layout;
    VkPipeline                  pipeline;
    VkDescriptorSet             descriptor_set;

    Vk_Buffer                   shader_binding_table;

    void create(
        const VkGeometryTrianglesNVX& model_triangles,
        VkImageView texture_view,
        VkSampler sampler,
        VkImageView output_image_view
    );

    void destroy();
    void update_output_image_descriptor(VkImageView output_image_view);
    void update_instance(const Matrix3x4& model_transform);

private:
    void create_acceleration_structure(const VkGeometryTrianglesNVX& triangles);
};

struct Copy_To_Swapchain {
    VkDescriptorSetLayout           set_layout;
    VkPipelineLayout                pipeline_layout;
    VkPipeline                      pipeline;
    std::vector<VkDescriptorSet>    sets; // per swapchain image

    void create(VkImageView output_image_view);
    void destroy();
    void update_resolution_dependent_descriptors(VkImageView output_image_view);
};

class Vk_Demo {
public:
    void initialize(Vk_Create_Info vk_create_info, SDL_Window* sdl_window);
    void shutdown();

    void run_frame();

    void release_resolution_dependent_resources();
    void restore_resolution_dependent_resources();

private:
    void create_ui_framebuffer();
    void destroy_ui_framebuffer();
    void create_output_image();

    void setup_imgui();
    void release_imgui();
    void do_imgui();

private:
    SDL_Window*                 sdl_window;

    bool                        show_ui                 = true;
    bool                        vsync                   = true;
    bool                        animate                 = false;
    bool                        raytracing              = false;

    VkRenderPass                ui_render_pass;
    VkFramebuffer               ui_framebuffer;
    Vk_Image                    output_image;
    Copy_To_Swapchain           copy_to_swapchain;

    Vk_Buffer                   vertex_buffer;
    Vk_Buffer                   index_buffer;
    uint32_t                    model_vertex_count;
    uint32_t                    model_index_count;

    Matrix3x4                   model_transform;
    Matrix3x4                   view_transform;

    Vk_Image                    texture;
    VkSampler                   sampler;

    Rasterization_Resources     raster;
    Raytracing_Resources        rt;
};
