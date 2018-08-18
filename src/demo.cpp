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

struct Uniform_Buffer_Object {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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

    uniform_buffer = vk_create_host_visible_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer_Object)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniform_buffer_ptr);

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
        auto texture = vk_create_texture(w, h, VK_FORMAT_R8G8B8A8_UNORM, 1, rgba_pixels, 4);
        stbi_image_free(rgba_pixels);
        return texture;
    };

    texture = load_texture(texture_path);

    // create sampler
    VkSamplerCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.magFilter = VK_FILTER_LINEAR;
    desc.minFilter = VK_FILTER_LINEAR;
    desc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.mipLodBias = 0.0f;
    desc.anisotropyEnable = VK_FALSE;
    desc.maxAnisotropy = 1;
    desc.compareEnable = VK_FALSE;
    desc.compareOp = VK_COMPARE_OP_ALWAYS;
    desc.minLod = 0.0f;
    desc.maxLod = 0.0f;
    desc.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    desc.unnormalizedCoordinates = VK_FALSE;
    sampler = get_resource_manager()->create_sampler(desc);
}

void Vk_Demo::upload_geometry() {
    Model model = load_obj_model(model_path);
    model_index_count = static_cast<uint32_t>(model.indices.size());

    {
        const VkDeviceSize size = model.vertices.size() * sizeof(model.vertices[0]);
        vertex_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
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
        index_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
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
    VkAttachmentDescription attachments[2];
    attachments[0].flags = 0;
    attachments[0].format =  vk.surface_format.format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].flags = 0;
    attachments[1].format = vk.depth_image_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref;
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkRenderPassCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    desc.pAttachments = attachments;
    desc.subpassCount = 1;
    desc.pSubpasses = &subpass;
    desc.dependencyCount = 0;
    desc.pDependencies = nullptr;

    return get_resource_manager()->create_render_pass(desc);
}

void Vk_Demo::create_render_passes() {
    render_pass = create_render_pass();

    //
    // Create framebuffers to use with the renderpasses.
    //
    VkFramebufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.width = vk.surface_width;
    desc.height = vk.surface_height;
    desc.layers = 1;

    std::array<VkImageView, 2> attachments {VK_NULL_HANDLE, vk.depth_image_view};
    desc.attachmentCount = static_cast<uint32_t>(attachments.size());
    desc.pAttachments = attachments.data();
    desc.renderPass = render_pass;

    swapchain_framebuffers.resize(vk.swapchain_images.size());
    for (size_t i = 0; i < vk.swapchain_images.size(); i++) {
        attachments[0] = vk.swapchain_image_views[i]; // set color attachment
        swapchain_framebuffers[i] = get_resource_manager()->create_framebuffer(desc);
    }
}

void Vk_Demo::create_descriptor_sets() {
    constexpr int buffer_count = 1;
    constexpr int image_count = 1;

    //
    // Descriptor pool.
    //
    {
        std::array<VkDescriptorPoolSize, 2> pool_sizes;
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = buffer_count;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = image_count;

        VkDescriptorPoolCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.maxSets = buffer_count + image_count;
        desc.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        desc.pPoolSizes = pool_sizes.data();

        descriptor_pool = get_resource_manager()->create_descriptor_pool(desc);
    }

    //
    // Set layouts.
    //

    // buffer set layout
    {
        VkDescriptorSetLayoutBinding descriptor_binding;
        descriptor_binding.binding = 0;
        descriptor_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_binding.descriptorCount = 1;
        descriptor_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        descriptor_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.bindingCount = 1;
        desc.pBindings = &descriptor_binding;
        buffer_set_layout = get_resource_manager()->create_descriptor_set_layout(desc);
    }

    // image set layout
    {
        VkDescriptorSetLayoutBinding descriptor_binding;
        descriptor_binding.binding = 0;
        descriptor_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_binding.descriptorCount = 1;
        descriptor_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        descriptor_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.bindingCount = 1;
        desc.pBindings = &descriptor_binding;
        image_set_layout = get_resource_manager()->create_descriptor_set_layout(desc);
    }

    //
    // Descriptor sets.
    //

    // buffer set
    {
        VkDescriptorSetAllocateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc.pNext = nullptr;
        desc.descriptorPool = descriptor_pool;
        desc.descriptorSetCount = 1;
        desc.pSetLayouts = &buffer_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(vk.device, &desc, &buffer_set));

        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = uniform_buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(Uniform_Buffer_Object);

        VkWriteDescriptorSet descriptor_write;
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.pNext = nullptr;
        descriptor_write.dstSet = buffer_set;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorCount = 1;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.pImageInfo = nullptr;
        descriptor_write.pBufferInfo = &buffer_info;
        descriptor_write.pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(vk.device, 1, &descriptor_write, 0, nullptr);
    }

    // image sets
    {
        std::array<VkDescriptorSetLayout, image_count> set_layouts;
        set_layouts.fill(image_set_layout);

        VkDescriptorSetAllocateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc.pNext = nullptr;
        desc.descriptorPool = descriptor_pool;
        desc.descriptorSetCount = image_count;
        desc.pSetLayouts = set_layouts.data();

        std::array<VkDescriptorSet, image_count> sets;
        VK_CHECK(vkAllocateDescriptorSets(vk.device, &desc, sets.data()));

        auto update_set = [](VkDescriptorSet set, VkImageView image_view, VkSampler sampler) {
            VkDescriptorImageInfo image_info;
            image_info.sampler = sampler;
            image_info.imageView = image_view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet descriptor_write;
            descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet = set;
            descriptor_write.dstBinding = 0;
            descriptor_write.dstArrayElement = 0;
            descriptor_write.descriptorCount = 1;
            descriptor_write.pNext = nullptr;
            descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_write.pImageInfo = &image_info;
            descriptor_write.pBufferInfo = nullptr;
            descriptor_write.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(vk.device, 1, &descriptor_write, 0, nullptr);
        };

        texture_set = sets[0];
        update_set(texture_set, texture.view, sampler);
    }
}

void Vk_Demo::create_pipeline_layouts() {
    VkPipelineLayoutCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.setLayoutCount = 1;
    desc.pushConstantRangeCount = 0;
    desc.pPushConstantRanges = nullptr;

    {
        std::array<VkDescriptorSetLayout, 2> set_layouts {buffer_set_layout, image_set_layout};
        desc.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
        desc.pSetLayouts = set_layouts.data();
        pipeline_layout = get_resource_manager()->create_pipeline_layout(desc);
    }
}

void Vk_Demo::create_shader_modules() {
    auto create_shader_module = [](uint8_t* bytes, long long count) {
        if (count % 4 != 0) {
            error("Vulkan: SPIR-V binary buffer size is not multiple of 4");
        }
        VkShaderModuleCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.codeSize = count;
        desc.pCode = reinterpret_cast<const uint32_t*>(bytes);

        return get_resource_manager()->create_shader_module(desc);
    };

    extern unsigned char model_vert_spv[];
    extern long long model_vert_spv_size;
    model_vs = create_shader_module(model_vert_spv, model_vert_spv_size);

    extern unsigned char model_frag_spv[];
    extern long long model_frag_spv_size;
    model_fs = create_shader_module(model_frag_spv, model_frag_spv_size);
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

    Uniform_Buffer_Object ubo;
    ubo.model = glm::rotate(glm::mat4(), time * glm::radians(30.0f), glm::vec3(0, 1, 0));
    ubo.view = glm::lookAt(glm::vec3(0, 0.5, 3.0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    // Vulkan clip space has inverted Y and half Z.
    const glm::mat4 clip(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f);

    ubo.proj = clip * glm::perspective(glm::radians(45.0f), vk.surface_width / (float)vk.surface_height, 0.1f, 50.0f);
    memcpy(uniform_buffer_ptr, &ubo, sizeof(ubo));
}

void Vk_Demo::run_frame() {
    update_uniform_buffer();
    vk_begin_frame();

    // Ensure that vertex shader has access to updated data.
    vk_record_buffer_memory_barrier(vk.command_buffer, uniform_buffer,
        VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT);

    // Prepare render pass instance.
    VkClearValue clear_values[2];
    clear_values[0].color = {0.6f, 0.6f, 0.66f, 0.0f};
    clear_values[1].depthStencil.depth = 1.0;
    clear_values[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo render_pass_begin_info;
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.pNext = nullptr;
    render_pass_begin_info.renderArea.offset = { 0, 0 };
    render_pass_begin_info.renderArea.extent = { (uint32_t)vk.surface_width, (uint32_t)vk.surface_height };
    render_pass_begin_info.clearValueCount = 2;
    render_pass_begin_info.pClearValues = clear_values;

    // Render scene
    {
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer = swapchain_framebuffers[vk.swapchain_image_index];
        vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // model
        {
            const VkDeviceSize offset = 0;
            std::array<VkDescriptorSet, 2> sets = {buffer_set, texture_set};
            vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &vertex_buffer, &offset);
            vkCmdBindIndexBuffer(vk.command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, (uint32_t)sets.size(), sets.data(), 0, nullptr);
            vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdDrawIndexed(vk.command_buffer, model_index_count, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(vk.command_buffer);
    }

    vk_end_frame();
}
