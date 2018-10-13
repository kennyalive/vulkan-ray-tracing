#include "common.h"
#include "demo.h"
#include "debug.h"
#include "geometry.h"
#include "matrix.h"
#include "vk.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/impl/imgui_impl_vulkan.h"
#include "imgui/impl/imgui_impl_sdl.h"

#include "sdl/SDL_scancode.h"

#include <chrono>

struct Uniform_Buffer {
    Matrix4x4   mvp;
    uint32_t    viewport_size[2];
    uint32_t    pad[2];
};

static VkShaderModule load_spirv(const std::string& spirv_file) {
    std::vector<uint8_t> bytes = read_binary_file(spirv_file);

    if (bytes.size() % 4 != 0) {
        error("Vulkan: SPIR-V binary buffer size is not multiple of 4");
    }

    VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO } ;
    create_info.codeSize = bytes.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(vk.device, &create_info, nullptr, &shader_module));
    return shader_module;
}

static Vk_Image load_texture(const std::string& texture_file) {
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

static const VkDescriptorPoolSize descriptor_pool_sizes[] = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             16},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_SAMPLER,                    16},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, 16},
};

constexpr uint32_t max_descriptor_sets = 64;

void Vk_Demo::initialize(const Demo_Create_Info& create_info) {
    this->create_info = create_info;
    Vk_Create_Info vk_create_info = create_info.vk_create_info;

    vk_create_info.descriptor_pool_sizes        = descriptor_pool_sizes;
    vk_create_info.descriptor_pool_size_count   = (uint32_t)std::size(descriptor_pool_sizes);
    vk_create_info.max_descriptor_sets          = max_descriptor_sets;

    vk_initialize(vk_create_info);

    // Device properties.
    {
        VkPhysicalDeviceRaytracingPropertiesNVX raytracing_properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAYTRACING_PROPERTIES_NVX };
        VkPhysicalDeviceProperties2 physical_device_properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        physical_device_properties.pNext = &raytracing_properties;
        vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

        rt.shader_header_size = raytracing_properties.shaderHeaderSize;

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

    // Geometry buffers.
    {
        Model model = load_obj_model(get_resource_path("iron-man/model.obj"));
        model_vertex_count = static_cast<uint32_t>(model.vertices.size());
        model_index_count = static_cast<uint32_t>(model.indices.size());
        {
            const VkDeviceSize size = model.vertices.size() * sizeof(model.vertices[0]);
            vertex_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "vertex_buffer");
            vk_ensure_staging_buffer_allocation(size);
            memcpy(vk.staging_buffer_ptr, model.vertices.data(), size);

            vk_record_and_run_commands(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
                VkBufferCopy region;
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = size;
                vkCmdCopyBuffer(command_buffer, vk.staging_buffer, vertex_buffer.handle, 1, &region);
            });
        }
        {
            const VkDeviceSize size = model.indices.size() * sizeof(model.indices[0]);
            index_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "index_buffer");
            vk_ensure_staging_buffer_allocation(size);
            memcpy(vk.staging_buffer_ptr, model.indices.data(), size);

            vk_record_and_run_commands(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
                VkBufferCopy region;
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = size;
                vkCmdCopyBuffer(command_buffer, vk.staging_buffer, index_buffer.handle, 1, &region);
            });
        }
    }

    // color render pass
    {
        VkAttachmentDescription attachments[1] = {};
        attachments[0].format           = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout    = VK_IMAGE_LAYOUT_GENERAL;
        attachments[0].finalLayout      = VK_IMAGE_LAYOUT_GENERAL;

        VkAttachmentReference color_attachment_ref;
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &color_attachment_ref;

        VkRenderPassCreateInfo create_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        create_info.attachmentCount = (uint32_t)std::size(attachments);
        create_info.pAttachments = attachments;
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(vk.device, &create_info, nullptr, &ui_render_pass));
        vk_set_debug_name(ui_render_pass, "ui_render_pass");
    }

    create_output_image();
    create_ui_framebuffer();

    VkGeometryTrianglesNVX model_triangles { VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX };
    model_triangles.vertexData    = vertex_buffer.handle;
    model_triangles.vertexOffset  = 0;
    model_triangles.vertexCount   = model_vertex_count;
    model_triangles.vertexStride  = sizeof(Vertex);
    model_triangles.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
    model_triangles.indexData     = index_buffer.handle;
    model_triangles.indexOffset   = 0;
    model_triangles.indexCount    = model_index_count;
    model_triangles.indexType     = VK_INDEX_TYPE_UINT32;

    raster.create(output_image.view);
    rt.create(output_image.view, model_triangles);
    copy_to_swapchain.create(output_image.view);

    setup_imgui();
}

void Vk_Demo::shutdown() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    release_imgui();

    vertex_buffer.destroy();
    index_buffer.destroy();

    raster.destroy();
    rt.destroy();
    copy_to_swapchain.destroy();;

    output_image.destroy();
    destroy_ui_framebuffer();

    vkDestroyRenderPass(vk.device, ui_render_pass, nullptr);
    vkDestroyFramebuffer(vk.device, ui_framebuffer, nullptr);

    vk_shutdown();
}

void Vk_Demo::create_ui_framebuffer() {
    VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    create_info.renderPass      = ui_render_pass;
    create_info.attachmentCount = 1;
    create_info.pAttachments    = &output_image.view;
    create_info.width           = vk.surface_size.width;
    create_info.height          = vk.surface_size.height;
    create_info.layers          = 1;

    VK_CHECK(vkCreateFramebuffer(vk.device, &create_info, nullptr, &ui_framebuffer));
}

void Vk_Demo::destroy_ui_framebuffer() {
    vkDestroyFramebuffer(vk.device, ui_framebuffer, nullptr);
    ui_framebuffer = VK_NULL_HANDLE;
}

void Vk_Demo::create_output_image() {
    output_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, "output image");

    vk_record_and_run_commands(vk.command_pool, vk.queue, [this](VkCommandBuffer command_buffer) {
        vk_record_image_layout_transition(command_buffer, output_image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    });
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
    init_info.DescriptorPool    = vk.descriptor_pool;

    ImGui_ImplVulkan_Init(&init_info, ui_render_pass);
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
        ImGui::SetNextWindowBgAlpha(0.3f);

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
            ImGui::Checkbox("Raytracing", &raytracing);

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
}

using Clock = std::chrono::high_resolution_clock;
using Time = std::chrono::time_point<Clock>;
static Time prev_time = Clock::now();

void Vk_Demo::run_frame(bool draw_only_background) {
    static double time = 0.0;
    Time current_time = Clock::now();        
    if (animate) {
        double time_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - prev_time).count() / 1e6;
        time += time_delta;
    }
    prev_time = current_time;

    model_transform = rotate_y(Matrix3x4::identity, (float)time * radians(30.0f));
    view_transform = look_at_transform(Vector(0, 0.5, 3.0), Vector(0), Vector(0, 1, 0));

    raster.update(model_transform, view_transform);

    do_imgui();

    vk_begin_frame();

    bool old_vsync = vsync;

    if (raytracing) {
        const VkBuffer sbt = rt.shader_binding_table.handle;
        const uint32_t sbt_slot_size = rt.shader_header_size;

        vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, rt.pipeline_layout, 0, 1, &rt.descriptor_set, 0, nullptr);
        vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, rt.pipeline);
        vkCmdTraceRaysNVX(vk.command_buffer,
            sbt, 0, // raygen shader
            sbt, 1 * sbt_slot_size, sbt_slot_size, // miss shaders
            sbt, 2 * sbt_slot_size, sbt_slot_size, // hit groups are not used yet
            vk.surface_size.width, vk.surface_size.height);
    } else {
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

        VkClearValue clear_values[2];
        clear_values[0].color = {srgb_encode(0.32f), srgb_encode(0.32f), srgb_encode(0.4f), 0.0f};
        clear_values[1].depthStencil.depth = 1.0;
        clear_values[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo render_pass_begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        render_pass_begin_info.renderPass        = raster.render_pass;
        render_pass_begin_info.framebuffer       = raster.framebuffer;
        render_pass_begin_info.renderArea.extent = vk.surface_size;
        render_pass_begin_info.clearValueCount   = (uint32_t)std::size(clear_values);
        render_pass_begin_info.pClearValues      = clear_values;

        vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // Draw model.
        if (!draw_only_background) {
            const VkDeviceSize zero_offset = 0;
            vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &vertex_buffer.handle, &zero_offset);
            vkCmdBindIndexBuffer(vk.command_buffer, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster.pipeline_layout, 0, 1, &raster.descriptor_set, 0, nullptr);
            vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster.pipeline);
            vkCmdDrawIndexed(vk.command_buffer, model_index_count, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(vk.command_buffer);
    }

    // Draw ImGui.
    {
        ImGui::Render();
        
        VkRenderPassBeginInfo render_pass_begin_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        render_pass_begin_info.renderPass           = ui_render_pass;
        render_pass_begin_info.framebuffer          = ui_framebuffer;
        render_pass_begin_info.renderArea.extent    = vk.surface_size;

        vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.command_buffer);
        vkCmdEndRenderPass(vk.command_buffer);
    }

    // Copy output image to swapchain.
    {
        const uint32_t group_size_x = 32; // according to shader
        const uint32_t group_size_y = 32;

        uint32_t group_count_x = (vk.surface_size.width + group_size_x - 1) / group_size_x;
        uint32_t group_count_y = (vk.surface_size.height + group_size_y - 1) / group_size_y;

        vk_record_image_layout_transition(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index], VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        uint32_t push_constants[] = { vk.surface_size.width, vk.surface_size.height };

        vkCmdPushConstants(vk.command_buffer, copy_to_swapchain.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(push_constants), push_constants);

        vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy_to_swapchain.pipeline_layout,
            0, 1, &copy_to_swapchain.sets[vk.swapchain_image_index], 0, nullptr);

        vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy_to_swapchain.pipeline);
        vkCmdDispatch(vk.command_buffer, group_count_x, group_count_y, 1);

        vk_record_image_layout_transition(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index], VK_IMAGE_ASPECT_COLOR_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    vk_end_frame();

    if (vsync != old_vsync) {
        release_resolution_dependent_resources();
        restore_resolution_dependent_resources();
    }
}

void Vk_Demo::release_resolution_dependent_resources() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    destroy_ui_framebuffer();
    raster.destroy_framebuffer();
    output_image.destroy();
    vk_release_resolution_dependent_resources();
}

void Vk_Demo::restore_resolution_dependent_resources() {
    vk_restore_resolution_dependent_resources(vsync);
    create_output_image();
    create_ui_framebuffer();

    raster.create_framebuffer(output_image.view);
    rt.update_resolution_dependent_descriptor(output_image.view);
    copy_to_swapchain.update_resolution_dependent_descriptors(output_image.view);
    prev_time = Clock::now();
}

void Rasterization_Resources::create(VkImageView output_image_view) {
    void* mapped_memory;
    uniform_buffer = vk_create_host_visible_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &mapped_memory, "uniform buffer to store matrices");
    mapped_uniform_buffer = static_cast<Uniform_Buffer*>(mapped_memory);

    texture = load_texture("iron-man/model.jpg");

    VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    create_info.magFilter           = VK_FILTER_LINEAR;
    create_info.minFilter           = VK_FILTER_LINEAR;
    create_info.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    create_info.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    create_info.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    create_info.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    create_info.mipLodBias          = 0.0f;
    create_info.anisotropyEnable    = VK_FALSE;
    create_info.maxAnisotropy       = 1;
    create_info.minLod              = 0.0f;
    create_info.maxLod              = 12.0f;

    VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &sampler));
    vk_set_debug_name(sampler, "diffuse_texture_sampler");

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
        attachments[0].finalLayout      = VK_IMAGE_LAYOUT_GENERAL;

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

        create_framebuffer(output_image_view);
    }

    // Pipeline.
    {
        VkShaderModule vertex_shader = load_spirv("spirv/model.vert.spv");
        VkShaderModule fragment_shader = load_spirv("spirv/model.frag.spv");

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

        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer  = uniform_buffer.handle;
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

void Rasterization_Resources::destroy() {
    texture.destroy();
    uniform_buffer.destroy();
    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroySampler(vk.device, sampler, nullptr);
    vkDestroyRenderPass(vk.device, render_pass, nullptr);
    vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
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
    mapped_uniform_buffer->mvp = mvp;
}

static void create_raytracing_acceleration_structure(const VkGeometryTrianglesNVX& triangles, Raytracing_Resources& rt) {
    VkGeometryNVX geometry { VK_STRUCTURE_TYPE_GEOMETRY_NVX };
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
    geometry.geometry.triangles = triangles;
    geometry.geometry.aabbs = VkGeometryAABBNVX { VK_STRUCTURE_TYPE_GEOMETRY_AABB_NVX };

    // Create acceleration structures and allocate/bind memory.
    {
        auto allocate_acceleration_structure_memory = [](VkAccelerationStructureNVX acceleration_structutre, VmaAllocation* allocation) {
            VkAccelerationStructureMemoryRequirementsInfoNVX accel_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
            accel_info.accelerationStructure = acceleration_structutre;

            VkMemoryRequirements2 reqs_holder { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
            vkGetAccelerationStructureMemoryRequirementsNVX(vk.device, &accel_info, &reqs_holder);

            VmaAllocationCreateInfo alloc_create_info{};
            alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VmaAllocationInfo alloc_info;
            VK_CHECK(vmaAllocateMemory(vk.allocator, &reqs_holder.memoryRequirements, &alloc_create_info, allocation, &alloc_info));

            VkBindAccelerationStructureMemoryInfoNVX bind_info { VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NVX };
            bind_info.accelerationStructure = acceleration_structutre;
            bind_info.memory = alloc_info.deviceMemory;
            bind_info.memoryOffset = alloc_info.offset;
            VK_CHECK(vkBindAccelerationStructureMemoryNVX(vk.device, 1, &bind_info));
        };

        // Bottom level.
        {
            VkAccelerationStructureCreateInfoNVX create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX;
            create_info.geometryCount = 1;
            create_info.pGeometries = &geometry;

            VK_CHECK(vkCreateAccelerationStructureNVX(vk.device, &create_info, nullptr, &rt.bottom_level_accel));
            allocate_acceleration_structure_memory(rt.bottom_level_accel, &rt.bottom_level_accel_allocation);
            vk_set_debug_name(rt.bottom_level_accel, "bottom_level_accel");
        }

        // Top level.
        {
            VkAccelerationStructureCreateInfoNVX create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX;
            create_info.instanceCount = 1;

            VK_CHECK(vkCreateAccelerationStructureNVX(vk.device, &create_info, nullptr, &rt.top_level_accel));
            allocate_acceleration_structure_memory(rt.top_level_accel, &rt.top_level_accel_allocation);
            vk_set_debug_name(rt.top_level_accel, "top_level_accel");
        }
    }

    // Create instance buffer
    VkBuffer instance_buffer = VK_NULL_HANDLE;
    VmaAllocation instance_buffer_allocation = VK_NULL_HANDLE;
    {
        uint64_t bottom_level_accel_handle;
        VK_CHECK(vkGetAccelerationStructureHandleNVX(vk.device, rt.bottom_level_accel, sizeof(uint64_t), &bottom_level_accel_handle));

        struct Instance {
            Matrix3x4   transform;
            uint32_t    instance_id : 24;
            uint32_t    instance_mask : 8;
            uint32_t    instance_contribution_to_hit_group_index : 24;
            uint32_t    flags : 8;
            uint64_t    acceleration_structure_handle;
        } instance{};

        instance.transform = Matrix3x4::identity;
        instance.instance_mask = 0xff;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
        instance.acceleration_structure_handle = bottom_level_accel_handle;

        VkBufferCreateInfo buffer_create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_create_info.size = sizeof(Instance);
        buffer_create_info.usage = VK_BUFFER_USAGE_RAYTRACING_BIT_NVX;

        VmaAllocationCreateInfo alloc_create_info {};
        alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VmaAllocationInfo alloc_info;
        VK_CHECK(vmaCreateBuffer(vk.allocator, &buffer_create_info, &alloc_create_info, &instance_buffer, &instance_buffer_allocation, &alloc_info));
        memcpy(alloc_info.pMappedData, &instance, sizeof(Instance));
    }

    // Create scratch buffert required to build acceleration structures.
    VkBuffer scratch_buffer = VK_NULL_HANDLE;
    VmaAllocation scratch_buffer_allocation = VK_NULL_HANDLE;
    {
        VkMemoryRequirements scratch_memory_requirements;
        {
            VkAccelerationStructureMemoryRequirementsInfoNVX accel_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
            VkMemoryRequirements2 reqs_holder { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };

            accel_info.accelerationStructure = rt.bottom_level_accel;
            vkGetAccelerationStructureScratchMemoryRequirementsNVX(vk.device, &accel_info, &reqs_holder);
            VkMemoryRequirements reqs_a = reqs_holder.memoryRequirements;

            accel_info.accelerationStructure = rt.top_level_accel;
            vkGetAccelerationStructureScratchMemoryRequirementsNVX(vk.device, &accel_info, &reqs_holder);
            VkMemoryRequirements reqs_b = reqs_holder.memoryRequirements;

            // Right now the spec does not provide additional guarantees related to scratch
            // buffer allocations, so the following asserts could fail.
            // Probably some things will be clarified in the future revisions.
            assert(reqs_a.alignment == reqs_b.alignment);
            assert(reqs_a.memoryTypeBits == reqs_b.memoryTypeBits);

            scratch_memory_requirements.size = std::max(reqs_a.size, reqs_b.size);
            scratch_memory_requirements.alignment = reqs_a.alignment;
            scratch_memory_requirements.memoryTypeBits = reqs_a.memoryTypeBits;
        }

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // NOTE: do not use vmaCreateBuffer since it does not allow to specify 'alignment'
        // returned from vkGetAccelerationStructureScratchMemoryRequirementsNVX.
        VmaAllocationInfo alloc_info;
        VK_CHECK(vmaAllocateMemory(vk.allocator, &scratch_memory_requirements, &alloc_create_info, &scratch_buffer_allocation, &alloc_info));

        VkBufferCreateInfo buffer_create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_create_info.size = scratch_memory_requirements.size;
        buffer_create_info.usage = VK_BUFFER_USAGE_RAYTRACING_BIT_NVX;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(vk.device, &buffer_create_info, nullptr, &scratch_buffer);

        vkGetBufferMemoryRequirements(vk.device, scratch_buffer, &VkMemoryRequirements()); // shut up validation layers

                                                                                           // NOTE: do we really need vkGetAccelerationStructureScratchMemoryRequirementsNVX function in the API?
                                                                                           // It should be possible to use vkGetBufferMemoryRequirements2 without introducing new API function
                                                                                           // and without modifying standard way to query memory requirements for the resource.
        VK_CHECK(vkBindBufferMemory(vk.device, scratch_buffer, alloc_info.deviceMemory, alloc_info.offset));
    }

    // Build acceleration structures.
    Timestamp t;

    vk_record_and_run_commands(vk.command_pool, vk.queue,
        [&rt, instance_buffer, scratch_buffer, &geometry](VkCommandBuffer command_buffer)
    {
        VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;

        vkCmdBuildAccelerationStructureNVX(command_buffer,
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
            0, VK_NULL_HANDLE, 0,
            1, &geometry,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX, VK_FALSE, rt.bottom_level_accel, VK_NULL_HANDLE,
            scratch_buffer, 0);

        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX,
            0, 1, &barrier, 0, nullptr, 0, nullptr);

        vkCmdBuildAccelerationStructureNVX(command_buffer,
            VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
            1, instance_buffer, 0,
            0, nullptr,
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX, VK_FALSE, rt.top_level_accel, VK_NULL_HANDLE,
            scratch_buffer, 0);
    });
    printf("\nAcceleration structures build time = %lld microseconds\n", elapsed_microseconds(t));

    vmaDestroyBuffer(vk.allocator, instance_buffer, instance_buffer_allocation);
    vkDestroyBuffer(vk.device, scratch_buffer, nullptr);
    vmaFreeMemory(vk.allocator, scratch_buffer_allocation);
}

static void create_raytracing_pipeline(VkImageView output_image_view, Raytracing_Resources& rt) {
    // Descriptor set layout.
    {
        VkDescriptorSetLayoutBinding layout_bindings[2] {};
        layout_bindings[0].binding          = 0;
        layout_bindings[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layout_bindings[0].descriptorCount  = 1;
        layout_bindings[0].stageFlags       = VK_SHADER_STAGE_RAYGEN_BIT_NVX;

        layout_bindings[1].binding          = 1;
        layout_bindings[1].descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
        layout_bindings[1].descriptorCount  = 1;
        layout_bindings[1].stageFlags       = VK_SHADER_STAGE_RAYGEN_BIT_NVX;

        VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        create_info.bindingCount    = (uint32_t)std::size(layout_bindings);
        create_info.pBindings       = layout_bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &create_info, nullptr, &rt.descriptor_set_layout));
    }

    // Pipeline layout.
    {
        VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        create_info.setLayoutCount  = 1;
        create_info.pSetLayouts     = &rt.descriptor_set_layout;
        VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &rt.pipeline_layout));
    }

    // Pipeline.
    {
        VkShaderModule rgen_shader = load_spirv("spirv/simple.rgen.spv");
        VkShaderModule miss_shader = load_spirv("spirv/simple.miss.spv");

        VkPipelineShaderStageCreateInfo stage_infos[2] {};
        stage_infos[0].sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[0].stage    = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
        stage_infos[0].module   = rgen_shader;
        stage_infos[0].pName    = "main";

        stage_infos[1].sType    = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[1].stage    = VK_SHADER_STAGE_MISS_BIT_NVX;
        stage_infos[1].module   = miss_shader;
        stage_infos[1].pName    = "main";

        uint32_t group_numbers[4] = { 0, 1 }; // [raygen] [miss shader]

        VkRaytracingPipelineCreateInfoNVX create_info { VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX };
        create_info.stageCount          = (uint32_t)std::size(stage_infos);
        create_info.pStages             = stage_infos;
        create_info.pGroupNumbers       = group_numbers;
        create_info.maxRecursionDepth   = 1;
        create_info.layout              = rt.pipeline_layout;
        VK_CHECK(vkCreateRaytracingPipelinesNVX(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &rt.pipeline));

        vkDestroyShaderModule(vk.device, rgen_shader, nullptr);
        vkDestroyShaderModule(vk.device, miss_shader, nullptr);
    }

    // Descriptor set.
    {
        VkDescriptorSetAllocateInfo desc { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        desc.descriptorPool     = vk.descriptor_pool;
        desc.descriptorSetCount = 1;
        desc.pSetLayouts        = &rt.descriptor_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(vk.device, &desc, &rt.descriptor_set));

        rt.update_resolution_dependent_descriptor(output_image_view);

        // Update acceleration structure slot.
        VkDescriptorAccelerationStructureInfoNVX accel_info { VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX };
        accel_info.accelerationStructureCount = 1;
        accel_info.pAccelerationStructures = &rt.top_level_accel;

        VkWriteDescriptorSet descriptor_write = {};
        descriptor_write.sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.pNext              = &accel_info;
        descriptor_write.dstSet             = rt.descriptor_set;
        descriptor_write.dstBinding         = 1;
        descriptor_write.descriptorCount    = 1;
        descriptor_write.descriptorType     = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;;

        vkUpdateDescriptorSets(vk.device, 1, &descriptor_write, 0, nullptr);
    }
}

void Raytracing_Resources::create(VkImageView output_image_view, const VkGeometryTrianglesNVX& model_triangles) {
    create_raytracing_acceleration_structure(model_triangles, *this);
    create_raytracing_pipeline(output_image_view, *this);

    constexpr uint32_t group_count = 2;
    VkDeviceSize sbt_size = group_count * shader_header_size;

    void* mapped_memory;
    shader_binding_table = vk_create_host_visible_buffer(sbt_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &mapped_memory, "shader_binding_table");
    VK_CHECK(vkGetRaytracingShaderHandlesNVX(vk.device, pipeline, 0, group_count, sbt_size, mapped_memory));
}

void Raytracing_Resources::destroy() {
    shader_binding_table.destroy();

    vkDestroyAccelerationStructureNVX(vk.device, bottom_level_accel, nullptr);
    vmaFreeMemory(vk.allocator, bottom_level_accel_allocation);

    vkDestroyAccelerationStructureNVX(vk.device, top_level_accel, nullptr);
    vmaFreeMemory(vk.allocator, top_level_accel_allocation);

    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
}

void Raytracing_Resources::update_resolution_dependent_descriptor(VkImageView output_image_view) {
    VkDescriptorImageInfo image_info = {};
    image_info.imageView    = output_image_view;
    image_info.imageLayout  = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet descriptor_writes[1] = {};
    descriptor_writes[0].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[0].dstSet             = descriptor_set;
    descriptor_writes[0].dstBinding         = 0;
    descriptor_writes[0].descriptorCount    = 1;
    descriptor_writes[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_writes[0].pImageInfo         = &image_info;

    vkUpdateDescriptorSets(vk.device, (uint32_t)std::size(descriptor_writes), descriptor_writes, 0, nullptr);
}

void Copy_To_Swapchain::create(VkImageView output_image_view) {
    // Descriptor set layout.
    {
        VkDescriptorSetLayoutBinding layout_bindings[2] {};
        layout_bindings[0].binding          = 0;
        layout_bindings[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layout_bindings[0].descriptorCount  = 1;
        layout_bindings[0].stageFlags       = VK_SHADER_STAGE_COMPUTE_BIT;

        layout_bindings[1].binding          = 1;
        layout_bindings[1].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layout_bindings[1].descriptorCount  = 1;
        layout_bindings[1].stageFlags       = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        create_info.bindingCount = (uint32_t)std::size(layout_bindings);
        create_info.pBindings = layout_bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &create_info, nullptr, &set_layout));
    }

    // Pipeline layout.
    {
        VkPushConstantRange range;
        range.stageFlags    = VK_SHADER_STAGE_COMPUTE_BIT;
        range.offset        = 0;
        range.size          = 8; // uint32 width + uint32 height

        VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        create_info.setLayoutCount          = 1;
        create_info.pSetLayouts             = &set_layout;
        create_info.pushConstantRangeCount  = 1;
        create_info.pPushConstantRanges     = &range;

        VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &pipeline_layout));
    }

    // Pipeline.
    {
        VkShaderModule copy_shader = load_spirv("spirv/copy_to_swapchain.comp.spv");

        VkPipelineShaderStageCreateInfo compute_stage { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        compute_stage.stage    = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_stage.module   = copy_shader;
        compute_stage.pName    = "main";

        VkComputePipelineCreateInfo create_info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        create_info.stage = compute_stage;
        create_info.layout = pipeline_layout;
        VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));

        vkDestroyShaderModule(vk.device, copy_shader, nullptr);
    }

    update_resolution_dependent_descriptors(output_image_view);
}

void Copy_To_Swapchain::destroy() {
    vkDestroyDescriptorSetLayout(vk.device, set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    sets.clear();
}

void Copy_To_Swapchain::update_resolution_dependent_descriptors(VkImageView output_image_view) {
    if (sets.size() < vk.swapchain_info.images.size())
    {
        size_t n = vk.swapchain_info.images.size() - sets.size();
        for (size_t i = 0; i < n; i++)
        {
            VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            alloc_info.descriptorPool     = vk.descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts        = &set_layout;

            VkDescriptorSet set;
            VK_CHECK(vkAllocateDescriptorSets(vk.device, &alloc_info, &set));
            sets.push_back(set);
        }
    }

    VkDescriptorImageInfo src_image_info{};
    src_image_info.imageView    = output_image_view;
    src_image_info.imageLayout  = VK_IMAGE_LAYOUT_GENERAL;

    for (size_t i = 0; i < vk.swapchain_info.images.size(); i++) {
        VkDescriptorImageInfo swapchain_image_info{};
        swapchain_image_info.imageView      = vk.swapchain_info.image_views[i];
        swapchain_image_info.imageLayout    = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet descriptor_writes[2] = {};
        descriptor_writes[0].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet             = sets[i];
        descriptor_writes[0].dstBinding         = 0;
        descriptor_writes[0].descriptorCount    = 1;
        descriptor_writes[0].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_writes[0].pImageInfo         = &src_image_info;

        descriptor_writes[1].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet             = sets[i];
        descriptor_writes[1].dstBinding         = 1;
        descriptor_writes[1].descriptorCount    = 1;
        descriptor_writes[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_writes[1].pImageInfo         = &swapchain_image_info;

        vkUpdateDescriptorSets(vk.device, (uint32_t)std::size(descriptor_writes), descriptor_writes, 0, nullptr);
    }
}
