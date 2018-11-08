#pragma once

#include "matrix.h"
#include "vk.h"

struct Rt_Uniform_Buffer;

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

    Vk_Buffer                   uniform_buffer;
    Rt_Uniform_Buffer*          mapped_uniform_buffer;

    void create(const VkGeometryTrianglesNVX& model_triangles, VkImageView texture_view, VkSampler sampler);
    void destroy();
    void update_output_image_descriptor(VkImageView output_image_view);
    void update(const Matrix3x4& model_transform, const Matrix3x4& camera_to_world_transform);

private:
    void create_acceleration_structure(const VkGeometryTrianglesNVX& triangles);
    void create_pipeline(const VkGeometryTrianglesNVX& model_triangles, VkImageView texture_view, VkSampler sampler);
};
