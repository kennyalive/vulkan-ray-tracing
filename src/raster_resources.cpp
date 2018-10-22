#include "matrix.h"
#include "raster_resources.h"
#include "utils.h"

namespace {
struct Uniform_Buffer {
    Matrix4x4   mvp;
};
}

void Rasterization_Resources::create(VkImageView texture_view, VkSampler sampler) {
    uniform_buffer = vk_create_host_visible_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &mapped_uniform_buffer, "raster_uniform_buffer");

    //
    // Descriptor set layouts.
    //
    {
        VkDescriptorSetLayoutBinding bindings[3] = {};
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[2].binding         = 2;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        create_info.bindingCount   = (uint32_t)std::size(bindings);
        create_info.pBindings      = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &create_info, nullptr, &descriptor_set_layout));
        vk_set_debug_name(descriptor_set_layout, "raster_descriptor_set_layout");
    }

    // Pipeline layout.
    {
        VkPipelineLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        create_info.setLayoutCount = 1;
        create_info.pSetLayouts = &descriptor_set_layout;

        VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &pipeline_layout));
        vk_set_debug_name(pipeline_layout, "raster_pipeline_layout");
    }

    // Render pass.
    {
        VkAttachmentDescription attachments[2] = {};
        attachments[0].format           = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        attachments[1].format           = vk.depth_info.format;
        attachments[1].samples          = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[1].finalLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_ref;
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref;
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &color_attachment_ref;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        VkRenderPassCreateInfo create_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        create_info.attachmentCount = (uint32_t)std::size(attachments);
        create_info.pAttachments = attachments;
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(vk.device, &create_info, nullptr, &render_pass));
        vk_set_debug_name(render_pass, "color_depth_render_pass");
    }

    // Pipeline.
    {
        VkShaderModule vertex_shader = vk_load_spirv("spirv/model.vert.spv");
        VkShaderModule fragment_shader = vk_load_spirv("spirv/model.frag.spv");

        pipeline = vk_create_graphics_pipeline(get_default_graphics_pipeline_state(),
            pipeline_layout, render_pass, vertex_shader, fragment_shader);

        vkDestroyShaderModule(vk.device, vertex_shader, nullptr);
        vkDestroyShaderModule(vk.device, fragment_shader, nullptr);
    }

    //
    // Descriptor sets.
    //
    {
        VkDescriptorSetAllocateInfo desc { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        desc.descriptorPool     = vk.descriptor_pool;
        desc.descriptorSetCount = 1;
        desc.pSetLayouts        = &descriptor_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(vk.device, &desc, &descriptor_set));

        Descriptor_Writes(descriptor_set)
            .uniform_buffer (0, uniform_buffer.handle, 0, sizeof(Uniform_Buffer))
            .sampled_image  (1, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .sampler        (2, sampler);
    }
}

void Rasterization_Resources::destroy() {
    uniform_buffer.destroy();
    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroyRenderPass(vk.device, render_pass, nullptr);
    *this = Rasterization_Resources{};
}

void Rasterization_Resources::create_framebuffer(VkImageView output_image_view) {
    VkImageView attachments[] = {output_image_view, vk.depth_info.image_view};

    VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    create_info.renderPass      = render_pass;
    create_info.attachmentCount = (uint32_t)std::size(attachments);
    create_info.pAttachments    = attachments;
    create_info.width           = vk.surface_size.width;
    create_info.height          = vk.surface_size.height;
    create_info.layers          = 1;

    VK_CHECK(vkCreateFramebuffer(vk.device, &create_info, nullptr, &framebuffer));
    vk_set_debug_name(framebuffer, "color_depth_framebuffer");
}

void Rasterization_Resources::destroy_framebuffer() {
    vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
    framebuffer = VK_NULL_HANDLE;
}

void Rasterization_Resources::update(const Matrix3x4& model_transform, const Matrix3x4& view_transform) {
    float aspect_ratio = (float)vk.surface_size.width / (float)vk.surface_size.height;
    Matrix4x4 proj = perspective_transform_opengl_z01(radians(45.0f), aspect_ratio, 0.1f, 50.0f);
    Matrix4x4 mvp = proj * view_transform * model_transform;
    static_cast<Uniform_Buffer*>(mapped_uniform_buffer)->mvp = mvp;
}
