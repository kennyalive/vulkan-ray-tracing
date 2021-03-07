#pragma once

#include "vk.h"

struct Matrix3x4;
struct GPU_Mesh;

struct Draw_Mesh {
    VkDescriptorSetLayout       descriptor_set_layout;
    VkPipelineLayout            pipeline_layout;
    VkPipeline                  pipeline;
    VkDescriptorSet             descriptor_set;

    VkRenderPass                render_pass;
    VkFramebuffer               framebuffer;

    Vk_Buffer                   uniform_buffer;
    void*                       mapped_uniform_buffer;

    void create(VkImageView texture_view, VkSampler sample);
    void destroy();
    void create_framebuffer(VkImageView output_image_view);
    void destroy_framebuffer();
    void update(const Matrix3x4& model_transform, const Matrix3x4& view_transform);
    void dispatch(const GPU_Mesh& mesh, bool show_texture_lod);
};
