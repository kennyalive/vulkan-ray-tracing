#include "common.h"
#include "demo.h"
#include "geometry.h"
#include "resource_manager.h"
#include "vk.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "glm/gtc/matrix_transform.hpp"

#include <array>
#include <chrono>

const std::string model_path = "../../data/model.obj";
const std::string texture_path = "../../data/model.jpg";

struct Uniform_Buffer {
    glm::mat4 mvp;
};

Vk_Demo::Vk_Demo(int window_width, int window_height, const SDL_SysWMinfo& window_sys_info) {
    vk.surface_width = window_width;
    vk.surface_height = window_height;
    vk_initialize(window_sys_info);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(vk.physical_device, &props);
    uint32_t major = VK_VERSION_MAJOR(props.apiVersion);
    uint32_t minor = VK_VERSION_MINOR(props.apiVersion);
    uint32_t patch = VK_VERSION_PATCH(props.apiVersion);
    printf("Vulkan API version: %d.%d.%d\n", major, minor, patch);

    get_resource_manager()->initialize(vk.device, vk.allocator);

    upload_textures();
    upload_geometry();

    uniform_buffer = vk_create_host_visible_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniform_buffer_ptr, "uniform buffer to store matrices");

    create_render_passes();
    create_descriptor_sets();
    create_pipeline_layouts();
    create_shader_modules();
    create_pipelines();
}

void Vk_Demo::upload_textures() {
    auto load_texture = [](const std::string& path) {
        int w, h;
        int component_count;

        auto rgba_pixels = stbi_load(path.c_str(), &w, &h, &component_count,STBI_rgb_alpha);
        if (rgba_pixels == nullptr)
            error("failed to load image file: " + path);
        Vk_Image texture = vk_create_texture(w, h, VK_FORMAT_R8G8B8A8_SRGB, true, rgba_pixels, 4, path.c_str());
        stbi_image_free(rgba_pixels);
        return texture;
    };

    texture = load_texture(texture_path);

    // create sampler
    VkSamplerCreateInfo desc { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    desc.magFilter           = VK_FILTER_LINEAR;
    desc.minFilter           = VK_FILTER_LINEAR;
    desc.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    desc.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.mipLodBias          = 0.0f;
    desc.anisotropyEnable    = VK_FALSE;
    desc.maxAnisotropy       = 1;
    desc.minLod              = 0.0f;
    desc.maxLod              = 12.0f;

    sampler = get_resource_manager()->create_sampler(desc, "sampler for diffuse texture");
}

void Vk_Demo::upload_geometry() {
    Model model = load_obj_model(model_path);
    model_index_count = static_cast<uint32_t>(model.indices.size());

    {
        const VkDeviceSize size = model.vertices.size() * sizeof(model.vertices[0]);
        vertex_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "vertex buffer");
        vk_ensure_staging_buffer_allocation(size);
        memcpy(vk.staging_buffer_ptr, model.vertices.data(), size);

        vk_record_and_run_commands(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
            VkBufferCopy region;
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = size;
            vkCmdCopyBuffer(command_buffer, vk.staging_buffer, vertex_buffer, 1, &region);
        });
    }
    {
        const VkDeviceSize size = model.indices.size() * sizeof(model.indices[0]);
        index_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "index buffer");
        vk_ensure_staging_buffer_allocation(size);
        memcpy(vk.staging_buffer_ptr, model.indices.data(), size);

        vk_record_and_run_commands(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
            VkBufferCopy region;
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = size;
            vkCmdCopyBuffer(command_buffer, vk.staging_buffer, index_buffer, 1, &region);
        });
    }
}

Vk_Demo::~Vk_Demo() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    get_resource_manager()->release_resources();
    vk_shutdown();
}

static VkRenderPass create_render_pass() {
    VkAttachmentDescription attachments[2] = {};
    attachments[0].format           = vk.surface_format.format;
    attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format           = vk.depth_image_format;
    attachments[1].samples          = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref;
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkRenderPassCreateInfo desc{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    desc.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    desc.pAttachments    = attachments;
    desc.subpassCount    = 1;
    desc.pSubpasses      = &subpass;

    return get_resource_manager()->create_render_pass(desc, "main render pass");
}

void Vk_Demo::create_render_passes() {
    render_pass = create_render_pass();

    //
    // Create framebuffers to use with the renderpasses.
    //
    VkFramebufferCreateInfo desc{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    desc.width  = vk.surface_width;
    desc.height = vk.surface_height;
    desc.layers = 1;

    std::array<VkImageView, 2> attachments {VK_NULL_HANDLE, vk.depth_image_view};
    desc.attachmentCount = static_cast<uint32_t>(attachments.size());
    desc.pAttachments    = attachments.data();
    desc.renderPass      = render_pass;

    swapchain_framebuffers.resize(vk.swapchain_images.size());
    for (size_t i = 0; i < vk.swapchain_images.size(); i++) {
        attachments[0] = vk.swapchain_image_views[i]; // set color attachment
        swapchain_framebuffers[i] = get_resource_manager()->create_framebuffer(desc, ("framebuffer for swapchain image " + std::to_string(i)).c_str());
    }
}

void Vk_Demo::create_descriptor_sets() {
    //
    // Descriptor pool.
    //
    {
        std::array<VkDescriptorPoolSize, 3> pool_sizes;
        pool_sizes[0].type              = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount   = 16;
        pool_sizes[1].type              = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        pool_sizes[1].descriptorCount   = 16;
        pool_sizes[2].type              = VK_DESCRIPTOR_TYPE_SAMPLER;
        pool_sizes[2].descriptorCount   = 16;

        VkDescriptorPoolCreateInfo desc{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        desc.maxSets        = 32;
        desc.poolSizeCount  = static_cast<uint32_t>(pool_sizes.size());
        desc.pPoolSizes     = pool_sizes.data();

        descriptor_pool = get_resource_manager()->create_descriptor_pool(desc, "global descriptor pool");
    }

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

        VkDescriptorSetLayoutCreateInfo desc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        desc.bindingCount   = sizeof(bindings) / sizeof(bindings[0]);
        desc.pBindings      = bindings;
        descriptor_set_layout = get_resource_manager()->create_descriptor_set_layout(desc, "Main descriptor set layout");
    }

    //
    // Descriptor sets.
    //
    {
        VkDescriptorSetAllocateInfo desc { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        desc.descriptorPool     = descriptor_pool;
        desc.descriptorSetCount = 1;
        desc.pSetLayouts        = &descriptor_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(vk.device, &desc, &descriptor_set));

        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer  = uniform_buffer;
        buffer_info.offset  = 0;
        buffer_info.range   = sizeof(Uniform_Buffer);

        VkDescriptorImageInfo image_info = {};
        image_info.imageView    = texture.view;
        image_info.imageLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo sampler_info = {};
        sampler_info.sampler = sampler;

        VkWriteDescriptorSet descriptor_writes[3] = {};
        descriptor_writes[0].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet             = descriptor_set;
        descriptor_writes[0].dstBinding         = 0;
        descriptor_writes[0].descriptorCount    = 1;
        descriptor_writes[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].pBufferInfo        = &buffer_info;

        descriptor_writes[1].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet             = descriptor_set;
        descriptor_writes[1].dstBinding         = 1;
        descriptor_writes[1].descriptorCount    = 1;
        descriptor_writes[1].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptor_writes[1].pImageInfo         = &image_info;

        descriptor_writes[2].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[2].dstSet             = descriptor_set;
        descriptor_writes[2].dstBinding         = 2;
        descriptor_writes[2].descriptorCount    = 1;
        descriptor_writes[2].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptor_writes[2].pImageInfo         = &sampler_info;

        vkUpdateDescriptorSets(vk.device, 3, descriptor_writes, 0, nullptr);
    }
}

void Vk_Demo::create_pipeline_layouts() {
    VkPipelineLayoutCreateInfo desc{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    desc.setLayoutCount = 1;
    desc.pSetLayouts = &descriptor_set_layout;
    pipeline_layout = get_resource_manager()->create_pipeline_layout(desc, "Main pipeline layout");
}

void Vk_Demo::create_shader_modules() {
    auto create_shader_module = [](const char* file_name, const char* debug_name) {
        std::vector<uint8_t> bytes = read_binary_file(file_name);

        if (bytes.size() % 4 != 0) {
            error("Vulkan: SPIR-V binary buffer size is not multiple of 4");
        }
        VkShaderModuleCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.codeSize = bytes.size();
        desc.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

        return get_resource_manager()->create_shader_module(desc, debug_name);
    };
    model_vs = create_shader_module("spirv/model.vb", "vertex shader");
    model_fs = create_shader_module("spirv/model.fb", "fragment shader");
}

void Vk_Demo::create_pipelines() {
    Vk_Pipeline_Def def;
    def.vs_module = model_vs;
    def.fs_module = model_fs;
    def.render_pass = render_pass;
    def.pipeline_layout = pipeline_layout;
    pipeline = vk_find_pipeline(def);
}

void Vk_Demo::update_uniform_buffer() {
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() / 1000.f;

    Uniform_Buffer ubo;
    glm::mat4x4 model = glm::rotate(glm::mat4(), time * glm::radians(30.0f), glm::vec3(0, 1, 0));
    glm::mat4x4 view = glm::lookAt(glm::vec3(0, 0.5, 3.0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    // Vulkan clip space has inverted Y and half Z.
    const glm::mat4 clip(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f);

    glm::mat4x4 proj = clip * glm::perspective(glm::radians(45.0f), vk.surface_width / (float)vk.surface_height, 0.1f, 50.0f);

    ubo.mvp = proj * view * model;
    memcpy(uniform_buffer_ptr, &ubo, sizeof(ubo));
}

void Vk_Demo::run_frame() {
    update_uniform_buffer();
    vk_begin_frame();

    // Prepare render pass instance.
    VkClearValue clear_values[2];
    clear_values[0].color = {0.32f, 0.32f, 0.4f, 0.0f};
    clear_values[1].depthStencil.depth = 1.0;
    clear_values[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo render_pass_begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_begin_info.renderPass        = render_pass;
    render_pass_begin_info.framebuffer       = swapchain_framebuffers[vk.swapchain_image_index];
    render_pass_begin_info.renderArea.offset = { 0, 0 };
    render_pass_begin_info.renderArea.extent = { (uint32_t)vk.surface_width, (uint32_t)vk.surface_height };
    render_pass_begin_info.clearValueCount   = 2;
    render_pass_begin_info.pClearValues      = clear_values;

    vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // Draw model.
    const VkDeviceSize zero_offset = 0;
    vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &vertex_buffer, &zero_offset);
    vkCmdBindIndexBuffer(vk.command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDrawIndexed(vk.command_buffer, model_index_count, 1, 0, 0, 0);

    vkCmdEndRenderPass(vk.command_buffer);
    vk_end_frame();
}
