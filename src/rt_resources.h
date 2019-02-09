#pragma once

#include "matrix.h"
#include "vk.h"

struct Rt_Uniform_Buffer;

struct VkGeometryInstanceNV {
    Matrix3x4      transform;
    uint32_t       instanceCustomIndex : 24;
    uint32_t       mask : 8;
    uint32_t       instanceOffset : 24;
    uint32_t       flags : 8;
    uint64_t       accelerationStructureHandle;
};

struct Raytracing_Resources {
    VkPhysicalDeviceRayTracingPropertiesNV properties;

    VkAccelerationStructureNV   bottom_level_accel;
    VmaAllocation               bottom_level_accel_allocation;
    uint64_t                    bottom_level_accel_handle;

    VkAccelerationStructureNV   top_level_accel;
    VmaAllocation               top_level_accel_allocation;

    VkBuffer                    scratch_buffer;
    VmaAllocation               scratch_buffer_allocation;

    VkBuffer                    instance_buffer;
    VmaAllocation               instance_buffer_allocation;
    VkGeometryInstanceNV*       mapped_instance_buffer;

    VkDescriptorSetLayout       descriptor_set_layout;
    VkPipelineLayout            pipeline_layout;
    VkPipeline                  pipeline;
    VkDescriptorSet             descriptor_set;

    Vk_Buffer                   shader_binding_table;

    Vk_Buffer                   uniform_buffer;
    Rt_Uniform_Buffer*          mapped_uniform_buffer;

    void create(const VkGeometryTrianglesNV& model_triangles, VkImageView texture_view, VkSampler sampler);
    void destroy();
    void update_output_image_descriptor(VkImageView output_image_view);
    void update(const Matrix3x4& model_transform, const Matrix3x4& camera_to_world_transform);

private:
    void create_acceleration_structure(const VkGeometryTrianglesNV& triangles);
    void create_pipeline(const VkGeometryTrianglesNV& model_triangles, VkImageView texture_view, VkSampler sampler);
};
