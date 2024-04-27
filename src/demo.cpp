#include "std.h"
#include "demo.h"
#include "triangle_mesh.h"

#include "glfw/glfw3.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"

static VkFormat get_depth_image_format() {
    VkFormat candidates[2] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
    for (auto format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vk.physical_device, format, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }
    error("failed to select depth attachment format");
    return VK_FORMAT_UNDEFINED;
}

void Vk_Demo::initialize(GLFWwindow* window, bool enable_validation_layers) {
    vk_initialize(window, enable_validation_layers);

    // Device properties.
    {
		auto& rt_properties = raytrace_scene.properties;
		rt_properties = VkPhysicalDeviceRayTracingPipelinePropertiesKHR{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
		};
		VkPhysicalDeviceProperties2 physical_device_properties{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			&rt_properties
		};
		vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

        printf("Device: %s\n", physical_device_properties.properties.deviceName);
        printf("Vulkan API version: %d.%d.%d\n",
            VK_VERSION_MAJOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_MINOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_PATCH(physical_device_properties.properties.apiVersion)
        );

        printf("\n");
        printf("VkPhysicalDeviceRayTracingPropertiesKHR:\n");
        printf("  shaderGroupHandleSize = %u\n", rt_properties.shaderGroupHandleSize);
        printf("  maxRayRecursionDepth = %u\n", rt_properties.maxRayRecursionDepth);
        printf("  maxShaderGroupStride = %u\n", rt_properties.maxShaderGroupStride);
        printf("  shaderGroupBaseAlignment = %u\n", rt_properties.shaderGroupBaseAlignment);
        printf("  maxRayDispatchInvocationCount = %u\n", rt_properties.maxRayDispatchInvocationCount);
        printf("  shaderGroupHandleAlignment = %u\n", rt_properties.shaderGroupHandleAlignment);
        printf("  maxRayHitAttributeSize = %u\n", rt_properties.maxRayHitAttributeSize);
    }

    // Geometry buffers.
    {
        Triangle_Mesh mesh = load_obj_model((get_data_directory() / "model/mesh.obj").string(), 1.25f);
        {
            VkDeviceSize size = mesh.vertices.size() * sizeof(mesh.vertices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            gpu_mesh.vertex_buffer = vk_create_buffer(size, usage, mesh.vertices.data(), "vertex_buffer");
            gpu_mesh.vertex_count = uint32_t(mesh.vertices.size());
        }
        {
            VkDeviceSize size = mesh.indices.size() * sizeof(mesh.indices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            gpu_mesh.index_buffer = vk_create_buffer(size, usage, mesh.indices.data(), "index_buffer");
            gpu_mesh.index_count = uint32_t(mesh.indices.size());
        }
    }

    // Texture.
    {
        texture = vk_load_texture((get_data_directory() / "model/diffuse.jpg").string());

        VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        create_info.magFilter = VK_FILTER_LINEAR;
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.mipLodBias = 0.0f;
        create_info.anisotropyEnable = VK_FALSE;
        create_info.maxAnisotropy = 1;
        create_info.minLod = 0.0f;
        create_info.maxLod = 12.0f;

        VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &sampler));
        vk_set_debug_name(sampler, "diffuse_texture_sampler");
    }

    // UI render pass.
    {
        VkAttachmentDescription attachments[1] = {};
        attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_ref;
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkRenderPassCreateInfo create_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        create_info.attachmentCount = (uint32_t)std::size(attachments);
        create_info.pAttachments = attachments;
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(vk.device, &create_info, nullptr, &ui_render_pass));
        vk_set_debug_name(ui_render_pass, "ui_render_pass");
    }

    // ImGui setup.
    {
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance = vk.instance;
        init_info.PhysicalDevice = vk.physical_device;
        init_info.Device = vk.device;
        init_info.QueueFamily = vk.queue_family_index;
        init_info.Queue = vk.queue;
        init_info.DescriptorPool = vk.descriptor_pool;
        init_info.RenderPass = ui_render_pass;
		init_info.MinImageCount = 2;
		init_info.ImageCount = (uint32_t)vk.swapchain_info.images.size();
        ImGui_ImplVulkan_Init(&init_info);
        ImGui::StyleColorsDark();
        ImGui_ImplVulkan_CreateFontsTexture();
    }

    draw_mesh.create(get_depth_image_format(), texture.view, sampler);
    raytrace_scene.create(gpu_mesh, texture.view, sampler);
    copy_to_swapchain.create();
    restore_resolution_dependent_resources();

    gpu_times.frame = time_keeper.allocate_time_interval();
    gpu_times.draw = time_keeper.allocate_time_interval();
    gpu_times.ui = time_keeper.allocate_time_interval();
    gpu_times.compute_copy = time_keeper.allocate_time_interval();
    time_keeper.initialize_time_intervals();
}

void Vk_Demo::shutdown() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    gpu_mesh.destroy();
    texture.destroy();
    copy_to_swapchain.destroy();
    vkDestroySampler(vk.device, sampler, nullptr);
    vkDestroyRenderPass(vk.device, ui_render_pass, nullptr);
    release_resolution_dependent_resources();
    draw_mesh.destroy();
    raytrace_scene.destroy();
    
    vk_shutdown();
}

void Vk_Demo::release_resolution_dependent_resources() {
    vkDestroyFramebuffer(vk.device, ui_framebuffer, nullptr);
    ui_framebuffer = VK_NULL_HANDLE;
    output_image.destroy();
    depth_buffer_image.destroy();
}

void Vk_Demo::restore_resolution_dependent_resources() {
    // create depth buffer
    {
        VkFormat depth_format = get_depth_image_format();
        depth_buffer_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, depth_format,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth_buffer");

        vk_execute(vk.command_pools[0], vk.queue, [this](VkCommandBuffer command_buffer) {
            VkImageSubresourceRange subresource_range{};
            subresource_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            subresource_range.levelCount = 1;
            subresource_range.layerCount = 1;

            vk_cmd_image_barrier_for_subresource(command_buffer, depth_buffer_image.handle, subresource_range,
                VK_PIPELINE_STAGE_NONE, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_NONE, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            });
    }

    // output image
    {
        output_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "output_image");

        raytrace_scene.update_output_image_descriptor(output_image.view);
        copy_to_swapchain.update_resolution_dependent_descriptors(output_image.view);
    }

    // imgui framebuffer
    {
        VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        create_info.renderPass = ui_render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &output_image.view;
        create_info.width = vk.surface_size.width;
        create_info.height = vk.surface_size.height;
        create_info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(vk.device, &create_info, nullptr, &ui_framebuffer));
        vk_set_debug_name(ui_framebuffer, "imgui_framebuffer");
    }

    last_frame_time = Clock::now();
}

void Vk_Demo::run_frame() {
    Time current_time = Clock::now();
    if (animate) {
        double time_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time).count() / 1e6;
        sim_time += time_delta;
    }
    last_frame_time = current_time;

    Matrix3x4 model_transform = rotate_y(Matrix3x4::identity, (float)sim_time * radians(20.0f));
    Matrix3x4 view_transform = look_at_transform(camera_pos, Vector3(0), Vector3(0, 1, 0));
    draw_mesh.update(model_transform, view_transform);

    Matrix3x4 camera_to_world_transform;
    camera_to_world_transform.set_column(0, Vector3(view_transform.get_row(0)));
    camera_to_world_transform.set_column(1, Vector3(view_transform.get_row(1)));
    camera_to_world_transform.set_column(2, Vector3(view_transform.get_row(2)));
    camera_to_world_transform.set_column(3, camera_pos);
    
    raytrace_scene.update(model_transform, camera_to_world_transform);

    bool old_raytracing = raytracing_active;
    do_imgui();
    draw_frame();
}

void Vk_Demo::draw_frame() {
    vk_begin_frame();
    time_keeper.next_frame();
    gpu_times.frame->begin();

    if (raytracing_active)
        draw_raytraced_image();
    else
        draw_rasterized_image();

    draw_imgui();
    copy_output_image_to_swapchain();

    gpu_times.frame->end();
    vk_end_frame();
}

void Vk_Demo::draw_rasterized_image() {
    GPU_TIME_SCOPE(gpu_times.draw);

    vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkViewport viewport{};
    viewport.width = static_cast<float>(vk.surface_size.width);
    viewport.height = static_cast<float>(vk.surface_size.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vk.command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = vk.surface_size;
    vkCmdSetScissor(vk.command_buffer, 0, 1, &scissor);

    VkRenderingAttachmentInfo color_attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    color_attachment.imageView = output_image.view;
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = { srgb_encode(0.32f), srgb_encode(0.32f), srgb_encode(0.4f), 0.0f };

    VkRenderingAttachmentInfo depth_attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    depth_attachment.imageView = depth_buffer_image.view;
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.clearValue.depthStencil = { 1.f, 0 };

    VkRenderingInfo rendering_info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    rendering_info.renderArea.extent = vk.surface_size;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = &depth_attachment;

    vkCmdBeginRendering(vk.command_buffer, &rendering_info);
    draw_mesh.dispatch(gpu_mesh, show_texture_lod);
    vkCmdEndRendering(vk.command_buffer);
}

void Vk_Demo::draw_raytraced_image() {
    GPU_TIME_SCOPE(gpu_times.draw);

    vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    raytrace_scene.dispatch(spp4, show_texture_lod);
}

void Vk_Demo::draw_imgui() {
    GPU_TIME_SCOPE(gpu_times.ui);

    ImGui::Render();

    if (raytracing_active) {
        vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    VkRenderPassBeginInfo render_pass_begin_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_begin_info.renderPass = ui_render_pass;
    render_pass_begin_info.framebuffer = ui_framebuffer;
    render_pass_begin_info.renderArea.extent = vk.surface_size;

    vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.command_buffer);
    vkCmdEndRenderPass(vk.command_buffer);
}

void Vk_Demo::copy_output_image_to_swapchain() {
    GPU_TIME_SCOPE(gpu_times.compute_copy);

    vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,  VK_IMAGE_LAYOUT_GENERAL);

    copy_to_swapchain.dispatch();

    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_NONE, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void Vk_Demo::do_imgui() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
            show_ui = !show_ui;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_W) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            camera_pos.z -= 0.2f;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            camera_pos.z += 0.2f;
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
            ImGui::Text("Frame time         : %.2f ms", gpu_times.frame->length_ms);
            ImGui::Text("Draw time          : %.2f ms", gpu_times.draw->length_ms);
            ImGui::Text("UI time            : %.2f ms", gpu_times.ui->length_ms);
            ImGui::Text("Compute copy time  : %.2f ms", gpu_times.compute_copy->length_ms);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Vertical sync", &vsync);
            ImGui::Checkbox("Animate", &animate);
            ImGui::Checkbox("Show texture lod", &show_texture_lod);

            ImGui::Checkbox("Raytracing", &raytracing_active);
            ImGui::Checkbox("4 rays per pixel", &spp4);

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
