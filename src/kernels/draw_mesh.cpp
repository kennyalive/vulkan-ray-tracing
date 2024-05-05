#include "draw_mesh.h"
#include "gpu_mesh.h"
#include "lib.h"
#include "linear_algebra.h"
#include "triangle_mesh.h"

namespace {
struct Uniform_Buffer {
    Matrix4x4 model_view_proj;
};
}

void Draw_Mesh::create(VkFormat color_attachment_format, VkFormat depth_attachment_format, VkImageView texture_view, VkSampler sampler) {
    uniform_buffer = vk_create_mapped_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &mapped_uniform_buffer, "raster_uniform_buffer");

    descriptor_set_layout = Vk_Descriptor_Set_Layout()
        .uniform_buffer (0, VK_SHADER_STAGE_VERTEX_BIT)
        .sampled_image (1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .sampler (2, VK_SHADER_STAGE_FRAGMENT_BIT)
        .create ("raster_set_layout");

    pipeline_layout = vk_create_pipeline_layout(
        { descriptor_set_layout },
        { VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4} },
        "raster_pipeline_layout");

    // pipeline
    {
        Vk_Shader_Module vertex_shader(get_resource_path("spirv/raster_mesh.vert.spv"));
        Vk_Shader_Module fragment_shader(get_resource_path("spirv/raster_mesh.frag.spv"));

        Vk_Graphics_Pipeline_State state = get_default_graphics_pipeline_state();

        // VkVertexInputBindingDescription
        state.vertex_bindings[0].binding = 0;
        state.vertex_bindings[0].stride = sizeof(Vertex);
        state.vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        state.vertex_binding_count = 1;

        // VkVertexInputAttributeDescription
        state.vertex_attributes[0].location = 0; // vertex
        state.vertex_attributes[0].binding = 0;
        state.vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        state.vertex_attributes[0].offset = 0;

        state.vertex_attributes[1].location = 1; // uv
        state.vertex_attributes[1].binding = 0;
        state.vertex_attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        state.vertex_attributes[1].offset = 12;

        state.vertex_attribute_count = 2;

        state.color_attachment_formats[0] = color_attachment_format;
        state.color_attachment_count = 1;
        state.depth_attachment_format = depth_attachment_format;

        pipeline = vk_create_graphics_pipeline(state, vertex_shader.handle, fragment_shader.handle,
            pipeline_layout, "draw_mesh_pipeline");
    }

    // Descriptor buffer.
    {
        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
        VkPhysicalDeviceProperties2 physical_device_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        physical_device_properties.pNext = &descriptor_buffer_properties;
        vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

        VkDeviceSize layout_size_in_bytes = 0;
        vkGetDescriptorSetLayoutSizeEXT(vk.device, descriptor_set_layout, &layout_size_in_bytes);
        std::vector<uint8_t> descriptor_data(layout_size_in_bytes);

        // Get descriptor 0 (uniform buffer)
        {
            VkDescriptorAddressInfoEXT address_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
            address_info.address = uniform_buffer.device_address;
            address_info.range = sizeof(Matrix4x4);

            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_info.data.pUniformBuffer = &address_info;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, descriptor_set_layout, 0, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.uniformBufferDescriptorSize,
                descriptor_data.data() + offset);
        }
        // Get descriptor 1 (sampled image)
        {
            VkDescriptorImageInfo image_info;
            image_info.imageView = texture_view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descriptor_info.data.pSampledImage = &image_info;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, descriptor_set_layout, 1, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.sampledImageDescriptorSize,
                descriptor_data.data() + offset);
        }
        // Get descriptor 2 (sampler)
        {
            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptor_info.data.pSampler = &sampler;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, descriptor_set_layout, 2, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.samplerDescriptorSize,
                descriptor_data.data() + offset);
        }
        VkBufferUsageFlags usage =
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        descriptor_buffer = vk_create_buffer_with_alignment(layout_size_in_bytes, usage,
            (uint32_t)descriptor_buffer_properties.descriptorBufferOffsetAlignment,
            descriptor_data.data(), "draw_mesh_descriptor_buffer");
    }
}

void Draw_Mesh::destroy() {
    descriptor_buffer.destroy();
    uniform_buffer.destroy();
    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    *this = Draw_Mesh{};
}

void Draw_Mesh::update(const Matrix3x4& object_to_camera_transform) {
    float aspect_ratio = (float)vk.surface_size.width / (float)vk.surface_size.height;
    Matrix4x4 projection_transform = perspective_transform_opengl_z01(radians(45.0f), aspect_ratio, 0.1f, 50.0f);
    Matrix4x4 transform = projection_transform * object_to_camera_transform;
    memcpy(mapped_uniform_buffer, &transform, sizeof(transform));
}

void Draw_Mesh::dispatch(const GPU_Mesh& mesh, bool show_texture_lod) {
    const VkDeviceSize zero_offset = 0;
    vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &mesh.vertex_buffer.handle, &zero_offset);
    vkCmdBindIndexBuffer(vk.command_buffer, mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT };
    descriptor_buffer_binding_info.address = descriptor_buffer.device_address;
    descriptor_buffer_binding_info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    vkCmdBindDescriptorBuffersEXT(vk.command_buffer, 1, &descriptor_buffer_binding_info);

    const uint32_t buffer_index = 0;
    const VkDeviceSize set_offset = 0;
    vkCmdSetDescriptorBufferOffsetsEXT(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &buffer_index, &set_offset);

    uint32_t show_texture_lod_uint = show_texture_lod;
    vkCmdPushConstants(vk.command_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &show_texture_lod_uint);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDrawIndexed(vk.command_buffer, mesh.index_count, 1, 0, 0, 0);
}
