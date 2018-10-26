#include "utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <cassert>
#include <unordered_map>

namespace std {
template<> struct hash<Vertex> {
    size_t operator()(Vertex const& v) const {
        size_t hash = 0;
        hash_combine(hash, v.pos);
        hash_combine(hash, v.tex_coord);
        //hash_combine(hash, v.normal);
        return hash;
    }
};
}

Vk_Image load_texture(const std::string& texture_file) {
    int w, h;
    int component_count;

    std::string abs_path = get_resource_path(texture_file);

    auto rgba_pixels = stbi_load(abs_path.c_str(), &w, &h, &component_count,STBI_rgb_alpha);
    if (rgba_pixels == nullptr)
        error("failed to load image file: " + abs_path);

    Vk_Image texture = vk_create_texture(w, h, VK_FORMAT_R8G8B8A8_SRGB, true, rgba_pixels, 4, texture_file.c_str());
    stbi_image_free(rgba_pixels);
    return texture;
};

Model load_obj_model(const std::string& path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str()))
        error("failed to load obj model: " + path);

    Vector model_min(std::numeric_limits<float>::infinity());
    Vector model_max(-std::numeric_limits<float>::infinity());

    Model model;
    std::unordered_map<Vertex, std::size_t> unique_vertices;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex;
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            vertex.tex_coord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            /*vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2],
            };*/

            if (unique_vertices.count(vertex) == 0) {
                unique_vertices[vertex] = model.vertices.size();
                model.vertices.push_back(vertex);

                // update model bounds
                model_min.x = std::min(model_min.x, vertex.pos.x);
                model_min.y = std::min(model_min.y, vertex.pos.y);
                model_min.z = std::min(model_min.z, vertex.pos.z);
                model_max.x = std::max(model_max.x, vertex.pos.x);
                model_max.y = std::max(model_max.y, vertex.pos.y);
                model_max.z = std::max(model_max.z, vertex.pos.z);
            }
            model.indices.push_back((uint32_t)unique_vertices[vertex]);
        }
    }

    // center the model
    Vector center = (model_min + model_max) * 0.5f;
    for (auto& v : model.vertices) {
        v.pos -= center;
    }
    return model;
}

//
// Descriptor_Writes
//
Descriptor_Writes& Descriptor_Writes::sampled_image(uint32_t binding, VkImageView image_view, VkImageLayout layout) {
    assert(write_count < max_writes);
    VkDescriptorImageInfo& image = resource_infos[write_count].image;
    image               = VkDescriptorImageInfo{};
    image.imageView     = image_view;
    image.imageLayout   = layout;

    VkWriteDescriptorSet& write = descriptor_writes[write_count++];
    write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet             = descriptor_set;
    write.dstBinding         = binding;
    write.descriptorCount    = 1;
    write.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo         = &image;
    return *this;
}

Descriptor_Writes& Descriptor_Writes::storage_image(uint32_t binding, VkImageView image_view) {
    assert(write_count < max_writes);
    VkDescriptorImageInfo& image = resource_infos[write_count].image;
    image               = VkDescriptorImageInfo{};
    image.imageView     = image_view;
    image.imageLayout   = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet& write = descriptor_writes[write_count++];
    write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet             = descriptor_set;
    write.dstBinding         = binding;
    write.descriptorCount    = 1;
    write.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo         = &image;
    return *this;
}

Descriptor_Writes& Descriptor_Writes::sampler(uint32_t binding, VkSampler sampler) {
    assert(write_count < max_writes);
    VkDescriptorImageInfo& image = resource_infos[write_count].image;
    image           = VkDescriptorImageInfo{};
    image.sampler   = sampler;

    VkWriteDescriptorSet& write = descriptor_writes[write_count++];
    write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet             = descriptor_set;
    write.dstBinding         = binding;
    write.descriptorCount    = 1;
    write.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.pImageInfo         = &image;
    return *this;
}

Descriptor_Writes& Descriptor_Writes::uniform_buffer(uint32_t binding, VkBuffer buffer_handle, VkDeviceSize offset, VkDeviceSize range) {
    assert(write_count < max_writes);
    VkDescriptorBufferInfo& buffer = resource_infos[write_count].buffer;
    buffer.buffer   = buffer_handle;
    buffer.offset   = offset;
    buffer.range    = range;

    VkWriteDescriptorSet& write = descriptor_writes[write_count++];
    write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet             = descriptor_set;
    write.dstBinding         = binding;
    write.descriptorCount    = 1;
    write.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo        = &buffer;
    return *this;
}

Descriptor_Writes& Descriptor_Writes::storage_buffer(uint32_t binding, VkBuffer buffer_handle, VkDeviceSize offset, VkDeviceSize range) {
    assert(write_count < max_writes);
    VkDescriptorBufferInfo& buffer = resource_infos[write_count].buffer;
    buffer.buffer   = buffer_handle;
    buffer.offset   = offset;
    buffer.range    = range;

    VkWriteDescriptorSet& write = descriptor_writes[write_count++];
    write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet             = descriptor_set;
    write.dstBinding         = binding;
    write.descriptorCount    = 1;
    write.descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo        = &buffer;
    return *this;
}

Descriptor_Writes& Descriptor_Writes::accelerator(uint32_t binding, VkAccelerationStructureNVX acceleration_structure) {
    assert(write_count < max_writes);
    Accel_Info& accel_info = resource_infos[write_count].accel_info;
    accel_info.handle = acceleration_structure;

    VkDescriptorAccelerationStructureInfoNVX& accel = accel_info.accel;
    accel = VkDescriptorAccelerationStructureInfoNVX { VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX };
    accel.accelerationStructureCount = 1;
    accel.pAccelerationStructures = &accel_info.handle;

    VkWriteDescriptorSet& write = descriptor_writes[write_count++];
    write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.pNext              = &accel;
    write.dstSet             = descriptor_set;
    write.dstBinding         = binding;
    write.descriptorCount    = 1;
    write.descriptorType     = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    return *this;
}

void Descriptor_Writes::commit() {
    assert(descriptor_set != VK_NULL_HANDLE);
    if (write_count > 0) {
        vkUpdateDescriptorSets(vk.device, write_count, descriptor_writes, 0, nullptr);
        write_count = 0;
    }
}

//
// Descriptor_Set_Layout
//
static VkDescriptorSetLayoutBinding get_set_layout_binding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags) {
    VkDescriptorSetLayoutBinding entry{};
    entry.binding           = binding;
    entry.descriptorType    = descriptor_type;
    entry.descriptorCount   = 1;
    entry.stageFlags        = stage_flags;
    return entry;
}

Descriptor_Set_Layout& Descriptor_Set_Layout::sampled_image(uint32_t binding, VkShaderStageFlags stage_flags) {
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage_flags);
    return *this;
}

Descriptor_Set_Layout& Descriptor_Set_Layout::storage_image(uint32_t binding, VkShaderStageFlags stage_flags) {
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage_flags);
    return *this;
}

Descriptor_Set_Layout& Descriptor_Set_Layout::sampler(uint32_t binding, VkShaderStageFlags stage_flags) {
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, VK_DESCRIPTOR_TYPE_SAMPLER, stage_flags);
    return *this;
}

Descriptor_Set_Layout& Descriptor_Set_Layout::uniform_buffer(uint32_t binding, VkShaderStageFlags stage_flags) {
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage_flags);
    return *this;
}

Descriptor_Set_Layout& Descriptor_Set_Layout::storage_buffer(uint32_t binding, VkShaderStageFlags stage_flags) {
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage_flags);
    return *this;
}

Descriptor_Set_Layout& Descriptor_Set_Layout::accelerator(uint32_t binding, VkShaderStageFlags stage_flags) {
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, stage_flags);
    return *this;
}

VkDescriptorSetLayout Descriptor_Set_Layout::create(const char* name) {
    VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    create_info.bindingCount    = binding_count;
    create_info.pBindings       = bindings;

    VkDescriptorSetLayout set_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &create_info, nullptr, &set_layout));
    vk_set_debug_name(set_layout, name);
    return set_layout;
}

void GPU_Time_Interval::begin() {
    vkCmdWriteTimestamp(vk.command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pool, start_query);
}
void GPU_Time_Interval::end() {
    vkCmdWriteTimestamp(vk.command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pool, start_query + 1);
}

GPU_Time_Interval* GPU_Time_Keeper::allocate_time_interval() {
    assert(time_interval_count < max_time_intervals);
    GPU_Time_Interval* time_interval = &time_intervals[time_interval_count++];

    time_interval->start_query = vk_allocate_timestamp_queries(2);
    time_interval->length_ms = 0.f;
    return time_interval;
}

void GPU_Time_Keeper::initialize_time_intervals() {
    vk_execute(vk.command_pool, vk.queue, [this](VkCommandBuffer command_buffer) {
        vkCmdResetQueryPool(command_buffer, vk.timestamp_query_pool, 0, 2 * time_interval_count);
        for (uint32_t i = 0; i < time_interval_count; i++) {
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pool, time_intervals[i].start_query);
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pool, time_intervals[i].start_query + 1);
        }
    });
}

void GPU_Time_Keeper::next_frame() {
    uint64_t timestamps[2*max_time_intervals];
    const uint32_t query_count = 2 * time_interval_count;
    VkResult result = vkGetQueryPoolResults(vk.device, vk.timestamp_query_pool, 0, query_count, query_count * sizeof(uint64_t), timestamps, 0, VK_QUERY_RESULT_64_BIT);
    VK_CHECK_RESULT(result);
    assert(result != VK_NOT_READY);

    const float influence = 0.25f;

    for (uint32_t i = 0; i < time_interval_count; i++) {
        assert(timestamps[2*i + 1] >= timestamps[2*i]);
        time_intervals[i].length_ms = (1.f-influence) * time_intervals[i].length_ms + influence * float(double(timestamps[2*i + 1] - timestamps[2*i]) * vk.timestamp_period_ms);
    }

    vkCmdResetQueryPool(vk.command_buffer, vk.timestamp_query_pool, 0, query_count);
}
