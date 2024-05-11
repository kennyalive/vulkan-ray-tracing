#pragma once

#include "acceleration_structure.h"
#include "lib.h"

struct GPU_Mesh;

struct Raytrace_Scene {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties;
    Vk_Intersection_Accelerator accelerator;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    Vk_Buffer descriptor_buffer;
    void* mapped_descriptor_buffer_ptr = nullptr;
    Vk_Buffer shader_binding_table;
    Vk_Buffer uniform_buffer;
    void* mapped_uniform_buffer;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{};

    void create(const GPU_Mesh& gpu_mesh, VkImageView texture_view, VkSampler sampler);
    void destroy();
    void update_output_image_descriptor(VkImageView output_image_view);
    void update(const Matrix3x4& model_transform, const Matrix3x4& camera_to_world_transform);
    void dispatch(bool spp4, bool show_texture_lod);

private:
    void create_pipeline(const GPU_Mesh& gpu_mesh, VkImageView texture_view, VkSampler sampler);
};
