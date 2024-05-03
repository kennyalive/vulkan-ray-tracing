#include "draw_mesh.h"
#include "gpu_mesh.h"
#include "linear_algebra.h"
#include "triangle_mesh.h"
#include "vk_utils.h"

namespace {
struct Uniform_Buffer {
    Matrix4x4 model_view_proj;
};
}

void Draw_Mesh::create(VkFormat color_attachment_format, VkFormat depth_attachment_format, VkImageView texture_view, VkSampler sampler) {
    uniform_buffer = vk_create_mapped_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &mapped_uniform_buffer, "raster_uniform_buffer");

    descriptor_set_layout = Descriptor_Set_Layout()
        .uniform_buffer (0, VK_SHADER_STAGE_VERTEX_BIT)
        .sampled_image (1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .sampler (2, VK_SHADER_STAGE_FRAGMENT_BIT)
        .create ("raster_set_layout");

    pipeline_layout = create_pipeline_layout(
        { descriptor_set_layout },
        { VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4} },
        "raster_pipeline_layout");

    // pipeline
    {
        Shader_Module vertex_shader("spirv/raster_mesh.vert.spv");
        Shader_Module fragment_shader("spirv/raster_mesh.frag.spv");

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

        pipeline = vk_create_graphics_pipeline(state, pipeline_layout, vertex_shader.handle, fragment_shader.handle);
    }

    descriptor_set = allocate_descriptor_set(descriptor_set_layout);
    Descriptor_Writes(descriptor_set)
        .uniform_buffer(0, uniform_buffer.handle, 0, sizeof(Uniform_Buffer))
        .sampled_image(1, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .sampler(2, sampler);
}

void Draw_Mesh::destroy() {
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

    uint32_t show_texture_lod_uint = show_texture_lod;
    vkCmdPushConstants(vk.command_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &show_texture_lod_uint);

    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDrawIndexed(vk.command_buffer, mesh.index_count, 1, 0, 0, 0);
}
