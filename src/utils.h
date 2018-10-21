#pragma once

#include "vector.h"
#include "vk.h"

#include <array>
#include <vector>

Vk_Image load_texture(const std::string& texture_file);

struct Descriptor_Writes {
    static constexpr uint32_t max_writes = 32;

    struct Accel_Info {
        VkDescriptorAccelerationStructureInfoNVX    accel;
        VkAccelerationStructureNVX                  handle; // referenced by accel
    };

    union Resource_Info {
        VkDescriptorImageInfo   image;
        VkDescriptorBufferInfo  buffer;
        Accel_Info              accel_info;
    };

    VkDescriptorSet         descriptor_set;
    VkWriteDescriptorSet    descriptor_writes[max_writes];
    Resource_Info           resource_infos[max_writes];
    uint32_t                write_count;

    Descriptor_Writes(VkDescriptorSet set) {
        descriptor_set = set;
        write_count = 0;
    }
    ~Descriptor_Writes() {
        commit();
    }

    Descriptor_Writes& sampled_image(uint32_t binding, VkImageView image_view, VkImageLayout layout);
    Descriptor_Writes& storage_image(uint32_t binding, VkImageView image_view);
    Descriptor_Writes& sampler(uint32_t binding, VkSampler sampler);
    Descriptor_Writes& uniform_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
    Descriptor_Writes& storage_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
    Descriptor_Writes& acceleration_structure(uint32_t binding, VkAccelerationStructureNVX acceleration_structure);
    void commit();
};

struct Vertex {
    Vector pos;
    float pad;
    Vector2 tex_coord;
    Vector2 pad2;

    bool operator==(const Vertex& other) const {
        return pos == other.pos && tex_coord == other.tex_coord;
    }

    static std::array<VkVertexInputBindingDescription, 1> get_bindings() {
        VkVertexInputBindingDescription binding_desc;
        binding_desc.binding = 0;
        binding_desc.stride = sizeof(Vertex);
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return {binding_desc};
    }

    static std::array<VkVertexInputAttributeDescription, 2> get_attributes() {
        VkVertexInputAttributeDescription position_attrib;
        position_attrib.location = 0;
        position_attrib.binding = 0;
        position_attrib.format = VK_FORMAT_R32G32B32_SFLOAT;
        position_attrib.offset = 0;

        VkVertexInputAttributeDescription tex_coord_attrib;
        tex_coord_attrib.location = 1;
        tex_coord_attrib.binding = 0;
        tex_coord_attrib.format = VK_FORMAT_R32G32_SFLOAT;
        tex_coord_attrib.offset = 16;

        return {position_attrib, tex_coord_attrib};
    }
};

struct Model {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

Model load_obj_model(const std::string& path);
