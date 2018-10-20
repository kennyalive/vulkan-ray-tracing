#include "common.h"
#include "demo.h"
#include "matrix.h"
#include "vk.h"
#include "utils.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/impl/imgui_impl_vulkan.h"
#include "imgui/impl/imgui_impl_sdl.h"

#include "sdl/SDL_scancode.h"

#include <chrono>

static const VkDescriptorPoolSize descriptor_pool_sizes[] = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             16},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_SAMPLER,                    16},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, 16},
};

constexpr uint32_t max_descriptor_sets = 64;

void Vk_Demo::initialize(Vk_Create_Info vk_create_info, SDL_Window* sdl_window) {
    this->sdl_window = sdl_window;

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


        if (vk.raytracing_supported) {
            printf("\n");
            printf("VkPhysicalDeviceRaytracingPropertiesNVX:\n");
            printf("  shaderHeaderSize = %u\n", raytracing_properties.shaderHeaderSize);
            printf("  maxRecursionDepth = %u\n", raytracing_properties.maxRecursionDepth);
            printf("  maxGeometryCount = %u\n", raytracing_properties.maxGeometryCount);
        }
    }

    // Geometry buffers.
    {
        Model model = load_obj_model(get_resource_path("iron-man/model.obj"));
        model_vertex_count = static_cast<uint32_t>(model.vertices.size());
        model_index_count = static_cast<uint32_t>(model.indices.size());
        {
            const VkDeviceSize size = model.vertices.size() * sizeof(model.vertices[0]);
            vertex_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "vertex_buffer");
            vk_ensure_staging_buffer_allocation(size);
            memcpy(vk.staging_buffer_ptr, model.vertices.data(), size);

            vk_execute(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
                VkBufferCopy region;
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = size;
                vkCmdCopyBuffer(command_buffer, vk.staging_buffer, vertex_buffer.handle, 1, &region);
            });
        }
        {
            const VkDeviceSize size = model.indices.size() * sizeof(model.indices[0]);
            index_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "index_buffer");
            vk_ensure_staging_buffer_allocation(size);
            memcpy(vk.staging_buffer_ptr, model.indices.data(), size);

            vk_execute(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
                VkBufferCopy region;
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = size;
                vkCmdCopyBuffer(command_buffer, vk.staging_buffer, index_buffer.handle, 1, &region);
            });
        }
    }

    // Texture.
    {
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
    }

    // UI render pass.
    {
        VkAttachmentDescription attachments[1] = {};
        attachments[0].format           = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[0].finalLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
    model_triangles.indexType     = VK_INDEX_TYPE_UINT16;

    raster.create(texture.view, sampler, output_image.view);

    if (vk.raytracing_supported)
        rt.create(model_triangles, texture.view, sampler, output_image.view);

    copy_to_swapchain.create(output_image.view);
    setup_imgui();
    last_frame_time = Clock::now();
}

void Vk_Demo::shutdown() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    release_imgui();

    vertex_buffer.destroy();
    index_buffer.destroy();

    texture.destroy();
    vkDestroySampler(vk.device, sampler, nullptr);

    output_image.destroy();
    destroy_ui_framebuffer();

    vkDestroyRenderPass(vk.device, ui_render_pass, nullptr);
    vkDestroyFramebuffer(vk.device, ui_framebuffer, nullptr);

    raster.destroy();

    if (vk.raytracing_supported)
        rt.destroy();

    copy_to_swapchain.destroy();;

    vk_shutdown();
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

    if (vk.raytracing_supported)
        rt.update_output_image_descriptor(output_image.view);

    copy_to_swapchain.update_resolution_dependent_descriptors(output_image.view);
    last_frame_time = Clock::now();
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
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "output_image");

    if (raytracing) {
        vk_execute(vk.command_pool, vk.queue, [this](VkCommandBuffer command_buffer) {
            vk_cmd_image_barrier(command_buffer, output_image.handle,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                0,                                  0,
                VK_IMAGE_LAYOUT_UNDEFINED,          VK_IMAGE_LAYOUT_GENERAL);
        });
    }
}

void Vk_Demo::setup_imgui() {
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(sdl_window);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance          = vk.instance;
    init_info.PhysicalDevice    = vk.physical_device;
    init_info.Device            = vk.device;
    init_info.QueueFamily       = vk.queue_family_index;
    init_info.Queue             = vk.queue;
    init_info.DescriptorPool    = vk.descriptor_pool;

    ImGui_ImplVulkan_Init(&init_info, ui_render_pass);
    ImGui::StyleColorsDark();

    vk_execute(vk.command_pool, vk.queue, [](VkCommandBuffer cb) {
        ImGui_ImplVulkan_CreateFontsTexture(cb);
    });
    ImGui_ImplVulkan_InvalidateFontUploadObjects();
}

void Vk_Demo::release_imgui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void Vk_Demo::run_frame() {
    // Update time.
    Time current_time = Clock::now();
    if (animate) {
        double time_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time).count() / 1e6;
        sim_time += time_delta;
    }
    last_frame_time = current_time;

    // Update resources.
    model_transform = rotate_y(Matrix3x4::identity, (float)sim_time * radians(30.0f));
    view_transform = look_at_transform(Vector(0, 0.5, 3.0), Vector(0), Vector(0, 1, 0));
    raster.update(model_transform, view_transform);

    if (vk.raytracing_supported)
        rt.update_instance(model_transform);

    bool old_vsync = vsync;
    bool old_raytracing = raytracing;

    do_imgui();

    vk_begin_frame();

    if (raytracing) {
        if (old_raytracing == false) {
            vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,                                  VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,          VK_IMAGE_LAYOUT_GENERAL);
        }
        draw_raytraced_image();
    }
    else
       draw_rasterized_image();

    draw_imgui();
    copy_output_image_to_swapchain();
    vk_end_frame();

    if (vsync != old_vsync) {
        release_resolution_dependent_resources();
        restore_resolution_dependent_resources();
    }
}

void Vk_Demo::draw_rasterized_image() {
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
    const VkDeviceSize zero_offset = 0;
    vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &vertex_buffer.handle, &zero_offset);
    vkCmdBindIndexBuffer(vk.command_buffer, index_buffer.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster.pipeline_layout, 0, 1, &raster.descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster.pipeline);
    vkCmdDrawIndexed(vk.command_buffer, model_index_count, 1, 0, 0, 0);
    vkCmdEndRenderPass(vk.command_buffer);
}

void Vk_Demo::draw_raytraced_image() {
    vkCmdBuildAccelerationStructureNVX(vk.command_buffer,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
        1, rt.instance_buffer, 0,
        0, nullptr,
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NVX,
        VK_TRUE, rt.top_level_accel, VK_NULL_HANDLE,
        rt.scratch_buffer, 0);

    VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;

    vkCmdPipelineBarrier(vk.command_buffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    const VkBuffer sbt = rt.shader_binding_table.handle;
    const uint32_t sbt_slot_size = rt.shader_header_size;

    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, rt.pipeline_layout, 0, 1, &rt.descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, rt.pipeline);

    vkCmdTraceRaysNVX(vk.command_buffer,
        sbt, 0, // raygen shader
        sbt, 1 * sbt_slot_size, sbt_slot_size, // miss shader
        sbt, 2 * sbt_slot_size, sbt_slot_size, // chit shader
        vk.surface_size.width, vk.surface_size.height);
}

void Vk_Demo::draw_imgui() {
    ImGui::Render();

    if (raytracing) {
        vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
            VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX,   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,                                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    VkRenderPassBeginInfo render_pass_begin_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_begin_info.renderPass           = ui_render_pass;
    render_pass_begin_info.framebuffer          = ui_framebuffer;
    render_pass_begin_info.renderArea.extent    = vk.surface_size;

    vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.command_buffer);
    vkCmdEndRenderPass(vk.command_buffer);

    if (raytracing) {
        vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,           VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,       VK_IMAGE_LAYOUT_GENERAL);
    } else {
        vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,           VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void Vk_Demo::copy_output_image_to_swapchain() {
    const uint32_t group_size_x = 32; // according to shader
    const uint32_t group_size_y = 32;

    uint32_t group_count_x = (vk.surface_size.width + group_size_x - 1) / group_size_x;
    uint32_t group_count_y = (vk.surface_size.height + group_size_y - 1) / group_size_y;

    if (raytracing) {
        vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
            VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX,   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,             VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,                                  VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,          VK_IMAGE_LAYOUT_GENERAL);

    uint32_t push_constants[] = { vk.surface_size.width, vk.surface_size.height };

    vkCmdPushConstants(vk.command_buffer, copy_to_swapchain.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(push_constants), push_constants);

    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy_to_swapchain.pipeline_layout,
        0, 1, &copy_to_swapchain.sets[vk.swapchain_image_index], 0, nullptr);

    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy_to_swapchain.pipeline);
    vkCmdDispatch(vk.command_buffer, group_count_x, group_count_y, 1);

    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,             0,
        VK_IMAGE_LAYOUT_GENERAL,                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    if (raytracing) {
        vk_cmd_image_barrier(vk.command_buffer, output_image.handle,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX,
            VK_ACCESS_SHADER_READ_BIT,              VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,              VK_IMAGE_LAYOUT_GENERAL);
    }
}

void Vk_Demo::do_imgui() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(sdl_window);
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

            if (!vk.raytracing_supported) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            ImGui::Checkbox("Raytracing", &raytracing);
            if (!vk.raytracing_supported) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }

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
