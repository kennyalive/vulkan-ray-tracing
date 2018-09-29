#include "common.h"
#include "demo.h"
#include "debug.h"
#include "geometry.h"
#include "matrix.h"
#include "resource_manager.h"
#include "vk.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/impl/imgui_impl_vulkan.h"
#include "imgui/impl/imgui_impl_sdl.h"

#include "sdl/SDL_scancode.h"

#include <chrono>

struct Uniform_Buffer {
    Matrix4x4 mvp;
};

Vk_Demo::Vk_Demo(const Demo_Create_Info& create_info)
    : create_info(create_info)
{
    vk_initialize(create_info.vk_create_info);
    {
        VkPhysicalDeviceRaytracingPropertiesNVX raytracing_properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAYTRACING_PROPERTIES_NVX };

        VkPhysicalDeviceProperties2 physical_device_properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        physical_device_properties.pNext = &raytracing_properties;

        vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

        printf("Device: %s\n", physical_device_properties.properties.deviceName);
        printf("Vulkan API version: %d.%d.%d\n",
            VK_VERSION_MAJOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_MINOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_PATCH(physical_device_properties.properties.apiVersion)
        );

        printf("\n");
        printf("VkPhysicalDeviceRaytracingPropertiesNVX:\n");
        printf("  shaderHeaderSize = %u\n", raytracing_properties.shaderHeaderSize);
        printf("  maxRecursionDepth = %u\n", raytracing_properties.maxRecursionDepth);
        printf("  maxGeometryCount = %u\n", raytracing_properties.maxGeometryCount);
    }

    get_resource_manager()->initialize(vk.device, vk.allocator);

    upload_textures();
    upload_geometry();
    create_acceleration_structure();

    uniform_buffer = vk_create_host_visible_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniform_buffer_ptr, "uniform buffer to store matrices");

    create_render_passes();
    create_framebuffers();
    create_descriptor_sets();
    create_pipeline_layouts();
    create_shader_modules();
    create_pipelines();

    setup_imgui();
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

    texture = load_texture(get_resource_path("iron-man/model.jpg"));

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
    Model model = load_obj_model(get_resource_path("iron-man/model.obj"));
    model_vertex_count = static_cast<uint32_t>(model.vertices.size());
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

void Vk_Demo::create_acceleration_structure() {
    VkGeometryTrianglesNVX triangles { VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX };
    triangles.vertexData    = vertex_buffer;
    triangles.vertexOffset  = 0;
    triangles.vertexCount   = model_vertex_count;
    triangles.vertexStride  = sizeof(Vertex);
    triangles.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.indexData     = index_buffer;
    triangles.indexOffset   = 0;
    triangles.indexCount    = model_index_count;
    triangles.indexType     = VK_INDEX_TYPE_UINT32;

    VkGeometryNVX geometry { VK_STRUCTURE_TYPE_GEOMETRY_NVX };
    geometry.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureCreateInfoNVX create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
    create_info.type            = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX;
    create_info.geometryCount   = 1;
    create_info.pGeometries     = &geometry;
    VK_CHECK(vkCreateAccelerationStructureNVX(vk.device, &create_info, nullptr, &acceleration_structure));

    VkAccelerationStructureMemoryRequirementsInfoNVX memory_requirements_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
    memory_requirements_info.accelerationStructure = acceleration_structure;
    VkMemoryRequirements2 memory_requirements { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
    vkGetAccelerationStructureMemoryRequirementsNVX(vk.device, &memory_requirements_info, &memory_requirements);
    VmaAllocationCreateInfo alloc_create_info{};
    alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VmaAllocationInfo alloc_info;
    VK_CHECK(vmaAllocateMemory(vk.allocator, &memory_requirements.memoryRequirements, &alloc_create_info, &acceleration_structure_allocation, &alloc_info));

    VkBindAccelerationStructureMemoryInfoNVX bind_info { VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NVX };
    bind_info.accelerationStructure = acceleration_structure;
    bind_info.memory                = alloc_info.deviceMemory;
    bind_info.memoryOffset          = alloc_info.offset;
    VK_CHECK(vkBindAccelerationStructureMemoryNVX(vk.device, 1, &bind_info));
}

Vk_Demo::~Vk_Demo() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    release_imgui();
    destroy_framebuffers();
    vkDestroyAccelerationStructureNVX(vk.device, acceleration_structure, nullptr);
    vmaFreeMemory(vk.allocator, acceleration_structure_allocation);
    get_resource_manager()->release_resources();
    vk_shutdown();
}

void Vk_Demo::create_render_passes() {
    VkAttachmentDescription attachments[2] = {};
    attachments[0].format           = vk.surface_format.format;
    attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

    VkRenderPassCreateInfo desc{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    desc.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    desc.pAttachments    = attachments;
    desc.subpassCount    = 1;
    desc.pSubpasses      = &subpass;

    render_pass = get_resource_manager()->create_render_pass(desc, "Main render pass");
}

void Vk_Demo::create_framebuffers() {
    VkFramebufferCreateInfo desc{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    desc.width  = vk.surface_size.width;
    desc.height = vk.surface_size.height;
    desc.layers = 1;

    VkImageView attachments[] = {VK_NULL_HANDLE, vk.depth_info.image_view};
    desc.attachmentCount = array_length(attachments);
    desc.pAttachments    = attachments;
    desc.renderPass      = render_pass;

    swapchain_framebuffers.resize(vk.swapchain_info.images.size());
    for (size_t i = 0; i < vk.swapchain_info.images.size(); i++) {
        attachments[0] = vk.swapchain_info.image_views[i]; // set color attachment

        VK_CHECK(vkCreateFramebuffer(vk.device, &desc, nullptr, &swapchain_framebuffers[i]));
        vk_set_debug_name(swapchain_framebuffers[i], ("Framebuffer for swapchain image " + std::to_string(i)).c_str());
    }
}

void Vk_Demo::destroy_framebuffers() {
    for (VkFramebuffer framebuffer : swapchain_framebuffers) {
        vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
    }
    swapchain_framebuffers.clear();
}

void Vk_Demo::create_descriptor_sets() {
    //
    // Descriptor pool.
    //
    {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 16}
        };

        VkDescriptorPoolCreateInfo desc{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        desc.maxSets        = 32;
        desc.poolSizeCount  = array_length(pool_sizes);
        desc.pPoolSizes     = pool_sizes;

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
    auto create_shader_module = [](const std::string& file_name, const char* debug_name) {
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
    model_vs = create_shader_module(get_resource_path("spirv/model.vb"), "vertex shader");
    model_fs = create_shader_module(get_resource_path("spirv/model.fb"), "fragment shader");
}

void Vk_Demo::create_pipelines() {
    Vk_Pipeline_Def def;
    def.vs_module = model_vs;
    def.fs_module = model_fs;
    def.render_pass = render_pass;
    def.pipeline_layout = pipeline_layout;
    pipeline = vk_find_pipeline(def);
}

void Vk_Demo::setup_imgui() {
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(create_info.window);

    // Setup Vulkan binding
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance          = vk.instance;
    init_info.PhysicalDevice    = vk.physical_device;
    init_info.Device            = vk.device;
    init_info.QueueFamily       = vk.queue_family_index;
    init_info.Queue             = vk.queue;
    init_info.DescriptorPool    = descriptor_pool;

    ImGui_ImplVulkan_Init(&init_info, render_pass);
    ImGui::StyleColorsDark();

    vk_record_and_run_commands(vk.command_pool, vk.queue, [](VkCommandBuffer cb) {
        ImGui_ImplVulkan_CreateFontsTexture(cb);
    });
    ImGui_ImplVulkan_InvalidateFontUploadObjects();
}

void Vk_Demo::release_imgui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void Vk_Demo::do_imgui() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(create_info.window);
    ImGui::NewFrame();

    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(SDL_SCANCODE_F10)) {
            show_ui = !show_ui;
        }
    }

    if (show_ui) {
        const float DISTANCE = 10.0f;
        static int corner = 0;

        ImVec2 window_pos = ImVec2((corner & 1) ? ImGui::GetIO().DisplaySize.x - DISTANCE : DISTANCE,
                                   (corner & 2) ? ImGui::GetIO().DisplaySize.y - DISTANCE : DISTANCE);

        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

        if (corner != -1)
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.8f);

        if (ImGui::Begin("UI", &show_ui, 
            (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Vertical sync", &vsync);
            ImGui::Checkbox("Animate", &animate);

            if (ImGui::BeginPopupContextWindow()) {
                if (ImGui::MenuItem("Custom",       NULL, corner == -1)) corner = -1;
                if (ImGui::MenuItem("Top-left",     NULL, corner == 0)) corner = 0;
                if (ImGui::MenuItem("Top-right",    NULL, corner == 1)) corner = 1;
                if (ImGui::MenuItem("Bottom-left",  NULL, corner == 2)) corner = 2;
                if (ImGui::MenuItem("Bottom-right", NULL, corner == 3)) corner = 3;
                if (ImGui::MenuItem("Close")) show_ui = false;
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.command_buffer);
}

using Clock = std::chrono::high_resolution_clock;
using Time = std::chrono::time_point<Clock>;
static Time prev_time = Clock::now();

void Vk_Demo::update_uniform_buffer() {
    // Update simulation time.
    static double time = 0.0;
    Time current_time = Clock::now();        
    if (animate) {
        double time_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - prev_time).count() / 1e6;
        time += time_delta;
    }
    prev_time = current_time;

    // Update mvp matrix.
    Uniform_Buffer ubo;
    Matrix3x4 model = rotate_y(Matrix3x4::identity, (float)time * radians(30.0f));
    Matrix3x4 view = look_at_transform(Vector(0, 0.5, 3.0), Vector(0), Vector(0, 1, 0));
    float aspect_ratio = (float)vk.surface_size.width / (float)vk.surface_size.height;
    Matrix4x4 proj = perspective_transform_opengl_z01(radians(45.0f), aspect_ratio, 0.1f, 50.0f);

    ubo.mvp = proj * view * model;
    memcpy(uniform_buffer_ptr, &ubo, sizeof(ubo));
}

void Vk_Demo::run_frame(bool draw_only_background) {
    update_uniform_buffer();
    vk_begin_frame();

    // Set viewport and scisor rect.
    VkViewport viewport{};
    viewport.width = static_cast<float>(vk.surface_size.width);
    viewport.height = static_cast<float>(vk.surface_size.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = vk.surface_size;

    vkCmdSetViewport(vk.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(vk.command_buffer, 0, 1, &scissor);

    // Prepare render pass instance.
    VkClearValue clear_values[2];
    clear_values[0].color = {0.32f, 0.32f, 0.4f, 0.0f};
    clear_values[1].depthStencil.depth = 1.0;
    clear_values[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo render_pass_begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_begin_info.renderPass        = render_pass;
    render_pass_begin_info.framebuffer       = swapchain_framebuffers[vk.swapchain_image_index];
    render_pass_begin_info.renderArea.extent = vk.surface_size;
    render_pass_begin_info.clearValueCount   = 2;
    render_pass_begin_info.pClearValues      = clear_values;

    vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // Draw model.
    if (!draw_only_background) {
        const VkDeviceSize zero_offset = 0;
        vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &vertex_buffer, &zero_offset);
        vkCmdBindIndexBuffer(vk.command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
        vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDrawIndexed(vk.command_buffer, model_index_count, 1, 0, 0, 0);
    }

    bool old_vsync = vsync;
    do_imgui();

    vkCmdEndRenderPass(vk.command_buffer);
    vk_end_frame();

    if (vsync != old_vsync) {
        release_resolution_dependent_resources();
        restore_resolution_dependent_resources();
    }
}

void Vk_Demo::release_resolution_dependent_resources() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    destroy_framebuffers();
    vk_release_resolution_dependent_resources();
}

void Vk_Demo::restore_resolution_dependent_resources() {
    vk_restore_resolution_dependent_resources(vsync);
    create_framebuffers();
    prev_time = Clock::now();
}
