#include "allocator.h"
#include "resource_manager.h"
#include "device_initialization.h"
#include "swapchain_initialization.h"
#include "vulkan_demo.h"
#include "vulkan_utilities.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtx/hash.hpp>

#include <array>
#include <chrono>
#include <functional>
#include <unordered_map>

const std::string model_path = "data/chalet.obj";
const std::string texture_path = "data/chalet.jpg";

//const std::string model_path = "data/teapot.obj";
//const std::string texture_path = "data/teapot.jpg";

struct Uniform_Buffer_Object {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex_coord;

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
    }
};

struct Model {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.tex_coord) << 1);
        }
    };
}

static Model load_model() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, model_path.c_str()))
        error("failed to load obj model: " + model_path);

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
                1.0 - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            vertex.color = {1.0f, 1.0f, 1.0f};

            if (unique_vertices.count(vertex) == 0) {
                unique_vertices[vertex] = model.vertices.size();
                model.vertices.push_back(vertex);
            }
            model.indices.push_back((uint32_t)unique_vertices[vertex]);
        }
    }
    return model;
}

static VkPipelineShaderStageCreateInfo GetPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
    VkShaderModule shaderModule, const char* entryPoint)
{
    VkPipelineShaderStageCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.stage = stage;
    createInfo.module = shaderModule;
    createInfo.pName = entryPoint;
    createInfo.pSpecializationInfo = nullptr;
    return createInfo;
}

static VkFormat find_format_with_features(VkPhysicalDevice physical_device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
            return format;
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
            return format;
    }
    error("failed to find format with requested features");
    return VK_FORMAT_UNDEFINED; // never get here
}

static VkFormat find_depth_format(VkPhysicalDevice physical_device) {
    return find_format_with_features(physical_device, {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

Vulkan_Demo::Vulkan_Demo(uint32_t window_width, uint32_t window_height)
: window_width(window_width)
, window_height(window_height) {}

void Vulkan_Demo::initialize(HWND windowHandle) {
    instance = CreateInstance();
    physical_device = SelectPhysicalDevice(instance);

    surface = create_surface(instance, windowHandle);

    Device_Info deviceInfo = CreateDevice(physical_device, surface);
    device = deviceInfo.device;
    queue_family_index = deviceInfo.queue_family_index;
    queue = deviceInfo.queue;

    get_allocator()->initialize(physical_device, device);
    get_resource_manager()->initialize(device);

    Swapchain_Info swapchain_info = create_swapchain(physical_device, device, surface);
    swapchain = swapchain_info.swapchain;
    swapchain_image_format = swapchain_info.surface_format;
    swapchainImages = swapchain_info.images;

    for (auto image : swapchainImages) {
        auto image_view = create_image_view(device, image, swapchain_info.surface_format, VK_IMAGE_ASPECT_COLOR_BIT);
        swapchainImageViews.push_back(image_view);
    }

    image_acquired = get_resource_manager()->create_semaphore();
    rendering_finished = get_resource_manager()->create_semaphore();

    create_command_pool();
    create_descriptor_pool();

    create_uniform_buffer();
    create_texture();
    create_texture_sampler();
    create_depth_buffer_resources();

    create_descriptor_set_layout();
    create_descriptor_set();
    create_render_pass();
    create_framebuffers();
    create_pipeline_layout();
    create_pipeline();

    upload_geometry();
    create_command_buffers();
    record_render_scene_command_buffer();
    record_primary_command_buffers();
}

void Vulkan_Demo::release_resources() {
    VkResult result = vkDeviceWaitIdle(device);
    check_vk_result(result, "vkDeviceWaitIdle");

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
    swapchainImages.clear();
    swapchainImageViews.clear();
    image_acquired = VK_NULL_HANDLE;
    rendering_finished = VK_NULL_HANDLE;

    command_pool = VK_NULL_HANDLE;
    descriptor_pool = VK_NULL_HANDLE;

    uniform_staging_buffer = VK_NULL_HANDLE;
    uniform_buffer = VK_NULL_HANDLE;
    texture_image = VK_NULL_HANDLE;
    texture_image_view = VK_NULL_HANDLE;
    texture_image_sampler = VK_NULL_HANDLE;
    depth_image = VK_NULL_HANDLE;
    depth_image_view = VK_NULL_HANDLE;

    descriptor_set_layout = VK_NULL_HANDLE;
    descriptor_set = VK_NULL_HANDLE;
    render_pass = VK_NULL_HANDLE;
    framebuffers.clear();
    pipeline_layout = VK_NULL_HANDLE;
    pipeline = VK_NULL_HANDLE;

    vertex_buffer = VK_NULL_HANDLE;
    index_buffer = VK_NULL_HANDLE;
    model_indices_count = 0;

    get_resource_manager()->release_resources();
    get_allocator()->deallocate_all();

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
    vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;
}

void Vulkan_Demo::create_render_pass() {
    // configure attachments
    VkAttachmentDescription color_attachment;
    color_attachment.flags = 0;
    color_attachment.format = swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment;
    depth_attachment.flags = 0;
    depth_attachment.format = find_depth_format(physical_device);
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref;
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentDescription, 2> attachments {color_attachment, depth_attachment};

    // configure subpasses
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

    // create render pass
    VkRenderPassCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.attachmentCount = static_cast<uint32_t>(attachments.size());
    desc.pAttachments = attachments.data();
    desc.subpassCount = 1;
    desc.pSubpasses = &subpass;
    desc.dependencyCount = 0;
    desc.pDependencies = nullptr;

    render_pass = get_resource_manager()->create_render_pass(desc);
}

void Vulkan_Demo::create_descriptor_set_layout() {
    std::array<VkDescriptorSetLayoutBinding, 2> descriptor_bindings;
    descriptor_bindings[0].binding = 0;
    descriptor_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_bindings[0].descriptorCount = 1;
    descriptor_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    descriptor_bindings[0].pImmutableSamplers = nullptr;

    descriptor_bindings[1].binding = 1;
    descriptor_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_bindings[1].descriptorCount = 1;
    descriptor_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptor_bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.bindingCount = static_cast<uint32_t>(descriptor_bindings.size());
    desc.pBindings = descriptor_bindings.data();

    descriptor_set_layout = get_resource_manager()->create_descriptor_set_layout(desc);
}

void Vulkan_Demo::create_pipeline_layout() {
    VkPipelineLayoutCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.setLayoutCount = 1;
    desc.pSetLayouts = &descriptor_set_layout;
    desc.pushConstantRangeCount = 0;
    desc.pPushConstantRanges = nullptr;

    pipeline_layout = get_resource_manager()->create_pipeline_layout(desc);
}

void Vulkan_Demo::create_pipeline() {
    Shader_Module vertex_shader(device, "shaders/vert.spv");
    Shader_Module fragment_shader(device, "shaders/frag.spv");

    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos {
        GetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader.handle, "main"),
        GetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader.handle, "main")
    };

    VkVertexInputBindingDescription vertexBindingDescription;
    vertexBindingDescription.binding = 0;
    vertexBindingDescription.stride = sizeof(Vertex);
    vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> vertexAttributeDescriptions;
    vertexAttributeDescriptions[0].location = 0;
    vertexAttributeDescriptions[0].binding = vertexBindingDescription.binding;
    vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributeDescriptions[0].offset = offsetof(struct Vertex, pos);

    vertexAttributeDescriptions[1].location = 1;
    vertexAttributeDescriptions[1].binding = vertexBindingDescription.binding;
    vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributeDescriptions[1].offset = offsetof(struct Vertex, color);

    vertexAttributeDescriptions[2].location = 2;
    vertexAttributeDescriptions[2].binding = vertexBindingDescription.binding;
    vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributeDescriptions[2].offset = offsetof(struct Vertex, tex_coord);

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.pNext = nullptr;
    vertexInputStateCreateInfo.flags = 0;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.pNext = nullptr;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(window_width);
    viewport.height = static_cast<float>(window_height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = window_width;
    scissor.extent.height = window_height;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = nullptr;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo resterizationStateCreateInfo;
    resterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    resterizationStateCreateInfo.pNext = nullptr;
    resterizationStateCreateInfo.flags = 0;
    resterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    resterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    resterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    resterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    resterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    resterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    resterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    resterizationStateCreateInfo.depthBiasClamp = 0.0f;
    resterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    resterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = nullptr;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.minSampleShading = 1.0f;
    multisampleStateCreateInfo.pSampleMask = nullptr;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.pNext = nullptr;
    depth_stencil_state.flags = 0;
    depth_stencil_state.depthTestEnable = VK_TRUE;
    depth_stencil_state.depthWriteEnable = VK_TRUE;
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state.stencilTestEnable = VK_FALSE;
    depth_stencil_state.front = {};
    depth_stencil_state.back = {};
    depth_stencil_state.minDepthBounds = 0.0;
    depth_stencil_state.maxDepthBounds = 0.0;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = nullptr;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
    colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.stageCount = static_cast<uint32_t>(shaderStageCreateInfos.size());
    desc.pStages = shaderStageCreateInfos.data();
    desc.pVertexInputState = &vertexInputStateCreateInfo;
    desc.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    desc.pTessellationState = nullptr;
    desc.pViewportState = &viewportStateCreateInfo;
    desc.pRasterizationState = &resterizationStateCreateInfo;
    desc.pMultisampleState = &multisampleStateCreateInfo;
    desc.pDepthStencilState = &depth_stencil_state;
    desc.pColorBlendState = &colorBlendStateCreateInfo;
    desc.pDynamicState = nullptr;
    desc.layout = pipeline_layout;
    desc.renderPass = render_pass;
    desc.subpass = 0;
    desc.basePipelineHandle = VK_NULL_HANDLE;
    desc.basePipelineIndex = -1;

    pipeline = get_resource_manager()->create_graphics_pipeline(desc);
}

void Vulkan_Demo::create_command_pool() {
    VkCommandPoolCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.queueFamilyIndex = queue_family_index;
    command_pool = get_resource_manager()->create_command_pool(desc);
}

void Vulkan_Demo::create_uniform_buffer() {
    auto size = static_cast<VkDeviceSize>(sizeof(Uniform_Buffer_Object));
    uniform_staging_buffer = create_permanent_staging_buffer(device, size, uniform_staging_buffer_memory);
    uniform_buffer = create_buffer(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

void Vulkan_Demo::upload_geometry() {
    Model model = load_model();
    model_indices_count = static_cast<uint32_t>(model.indices.size());

    {
        const VkDeviceSize size = model.vertices.size() * sizeof(model.vertices[0]);
        vertex_buffer = create_buffer(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        VkBuffer staging_buffer = create_staging_buffer(device, size, model.vertices.data());
        Defer_Action destroy_staging_buffer([&staging_buffer, this]() { vkDestroyBuffer(device, staging_buffer, nullptr); });
        record_and_run_commands(device, command_pool, queue, [&staging_buffer, &size, this](VkCommandBuffer command_buffer) {
            VkBufferCopy region;
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = size;
            vkCmdCopyBuffer(command_buffer, staging_buffer, vertex_buffer, 1, &region);
        });
    }
    {
        const VkDeviceSize size = model.indices.size() * sizeof(model.indices[0]);
        index_buffer = create_buffer(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        VkBuffer staging_buffer = create_staging_buffer(device, size, model.indices.data());
        Defer_Action destroy_staging_buffer([&staging_buffer, this]() { vkDestroyBuffer(device, staging_buffer, nullptr); });
        record_and_run_commands(device, command_pool, queue, [&staging_buffer, &size, this](VkCommandBuffer command_buffer) {
            VkBufferCopy region;
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = size;
            vkCmdCopyBuffer(command_buffer, staging_buffer, index_buffer, 1, &region);
        });
    }
}

void Vulkan_Demo::create_descriptor_pool() {
    std::array<VkDescriptorPoolSize, 2> pool_sizes;
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.maxSets = 1;
    desc.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    desc.pPoolSizes = pool_sizes.data();

    descriptor_pool = get_resource_manager()->create_descriptor_pool(desc);
}

void Vulkan_Demo::create_descriptor_set() {
    VkDescriptorSetAllocateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    desc.pNext = nullptr;
    desc.descriptorPool = descriptor_pool;
    desc.descriptorSetCount = 1;
    desc.pSetLayouts = &descriptor_set_layout;

    VkResult result = vkAllocateDescriptorSets(device, &desc, &descriptor_set);
    check_vk_result(result, "vkAllocateDescriptorSets");

    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = uniform_buffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(Uniform_Buffer_Object);

    VkDescriptorImageInfo image_info;
    image_info.sampler = texture_image_sampler;
    image_info.imageView = texture_image_view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> descriptor_writes;
    descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[0].pNext = nullptr;
    descriptor_writes[0].dstSet = descriptor_set;
    descriptor_writes[0].dstBinding = 0;
    descriptor_writes[0].dstArrayElement = 0;
    descriptor_writes[0].descriptorCount = 1;
    descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_writes[0].pImageInfo = nullptr;
    descriptor_writes[0].pBufferInfo = &buffer_info;
    descriptor_writes[0].pTexelBufferView = nullptr;

    descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[1].dstSet = descriptor_set;
    descriptor_writes[1].dstBinding = 1;
    descriptor_writes[1].dstArrayElement = 0;
    descriptor_writes[1].descriptorCount = 1;
    descriptor_writes[1].pNext = nullptr;
    descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_writes[1].pImageInfo = &image_info;
    descriptor_writes[1].pBufferInfo = nullptr;
    descriptor_writes[1].pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(device, (uint32_t)descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
}

void Vulkan_Demo::create_texture() {
    int image_width, image_height, image_component_count;

    auto rgba_pixels = stbi_load(texture_path.c_str(), &image_width, &image_height, &image_component_count, STBI_rgb_alpha);
    if (rgba_pixels == nullptr) {
        error("failed to load image file");
    }

    VkImage staging_image = create_staging_texture(device, image_width, image_height, VK_FORMAT_R8G8B8A8_UNORM, rgba_pixels, 4);

    Defer_Action destroy_staging_image([this, &staging_image]() {
        vkDestroyImage(device, staging_image, nullptr);
    });
    stbi_image_free(rgba_pixels);

    texture_image = ::create_texture(device, image_width, image_height, VK_FORMAT_R8G8B8A8_UNORM);

    record_and_run_commands(device, command_pool, queue,
        [&staging_image, &image_width, &image_height, this](VkCommandBuffer command_buffer) {

        record_image_layout_transition(command_buffer, staging_image, VK_FORMAT_R8G8B8A8_UNORM,
            VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        record_image_layout_transition(command_buffer, texture_image, VK_FORMAT_R8G8B8A8_UNORM,
            0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // copy staging image's data to device local image
        VkImageSubresourceLayers subresource_layers;
        subresource_layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_layers.mipLevel = 0;
        subresource_layers.baseArrayLayer = 0;
        subresource_layers.layerCount = 1;

        VkImageCopy region;
        region.srcSubresource = subresource_layers;
        region.srcOffset = {0, 0, 0};
        region.dstSubresource = subresource_layers;
        region.dstOffset = {0, 0, 0};
        region.extent.width = image_width;
        region.extent.height = image_height;

        vkCmdCopyImage(command_buffer,
            staging_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

        record_image_layout_transition(command_buffer, texture_image, VK_FORMAT_R8G8B8A8_UNORM,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    texture_image_view = create_image_view(device, texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Vulkan_Demo::create_texture_sampler() {
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
    desc.anisotropyEnable = VK_TRUE;
    desc.maxAnisotropy = 16;
    desc.compareEnable = VK_FALSE;
    desc.compareOp = VK_COMPARE_OP_ALWAYS;
    desc.minLod = 0.0f;
    desc.maxLod = 0.0f;
    desc.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    desc.unnormalizedCoordinates = VK_FALSE;

    texture_image_sampler = get_resource_manager()->create_sampler(desc);
}

void Vulkan_Demo::create_depth_buffer_resources() {
    VkFormat depth_format = find_depth_format(physical_device);
    depth_image = create_depth_attachment_image(device, window_width, window_height, depth_format);
    depth_image_view = create_image_view(device, depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

    record_and_run_commands(device, command_pool, queue, [&depth_format, this](VkCommandBuffer command_buffer) {
        record_image_layout_transition(command_buffer, depth_image, depth_format, 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    });
}

void Vulkan_Demo::create_framebuffers() {
    std::array<VkImageView, 2> attachments = {VK_NULL_HANDLE, depth_image_view};

    VkFramebufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.renderPass = render_pass;
    desc.attachmentCount = static_cast<uint32_t>(attachments.size());
    desc.pAttachments = attachments.data();
    desc.width = window_width;
    desc.height = window_height;
    desc.layers = 1;

    framebuffers.resize(swapchainImages.size());
    for (std::size_t i = 0; i < framebuffers.size(); i++) {
        attachments[0] = swapchainImageViews[i]; // set color attachment
        framebuffers[i] = get_resource_manager()->create_framebuffer(desc);
    }
}

void Vulkan_Demo::create_command_buffers() {
    VkCommandBufferAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(swapchainImages.size());

    command_buffers.resize(swapchainImages.size());
    VkResult result = vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data());
    check_vk_result(result, "vkAllocateCommandBuffers");

    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    alloc_info.commandBufferCount = 1;
    result = vkAllocateCommandBuffers(device, &alloc_info, &render_scene_cmdbuf);
    check_vk_result(result, "vkAllocateCommandBuffers");
}

void Vulkan_Demo::record_primary_command_buffers() {
    VkImageSubresourceRange subresource_range;
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 1;

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    begin_info.pInheritanceInfo = nullptr;

    for (std::size_t i = 0; i < command_buffers.size(); i++) {
        VkCommandBuffer command_buffer = command_buffers[i];

        VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(result, "vkBeginCommandBuffer");

        std::array<VkClearValue, 2> clear_values;
        clear_values[0].color = {1.0f, 0.8f, 0.4f, 0.0f};
        clear_values[1].depthStencil = {1.0, 0};

        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = render_pass;
        renderPassBeginInfo.framebuffer = framebuffers[i];
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = {window_width, window_height};
        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clear_values.size());
        renderPassBeginInfo.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkCmdExecuteCommands(command_buffer, 1, &render_scene_cmdbuf);
        vkCmdEndRenderPass(command_buffer);

        result = vkEndCommandBuffer(command_buffer);
        check_vk_result(result, "vkEndCommandBuffer");
    }
}

void Vulkan_Demo::record_render_scene_command_buffer() {
    VkCommandBufferInheritanceInfo inheritance_info;
    inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance_info.pNext = nullptr;
    inheritance_info.renderPass = render_pass;
    inheritance_info.subpass = 0;
    inheritance_info.framebuffer = VK_NULL_HANDLE;
    inheritance_info.occlusionQueryEnable = VK_FALSE;
    inheritance_info.queryFlags = 0;
    inheritance_info.pipelineStatistics = 0;

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    begin_info.pInheritanceInfo = &inheritance_info;

    VkResult result = vkBeginCommandBuffer(render_scene_cmdbuf, &begin_info);
    check_vk_result(result, "vkBeginCommandBuffer");

    vkCmdBindPipeline(render_scene_cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(render_scene_cmdbuf, 0, 1, &vertex_buffer, &offset);
    vkCmdBindIndexBuffer(render_scene_cmdbuf, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(render_scene_cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdDrawIndexed(render_scene_cmdbuf, model_indices_count, 1, 0, 0, 0);

    result = vkEndCommandBuffer(render_scene_cmdbuf);
    check_vk_result(result, "vkEndCommandBuffer");
}

void Vulkan_Demo::update_uniform_buffer() {
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() / 1000.f;
    //time = 0.0;

    Uniform_Buffer_Object ubo;
    ubo.model = glm::rotate(glm::mat4(), time * glm::radians(30.0f), glm::vec3(0, 1, 0)) *
        glm::rotate(glm::mat4(), glm::radians(-90.0f), glm::vec3(1, 0, 0));

    ubo.view = glm::lookAt(glm::vec3(0.5, 1.4, 2.8), glm::vec3(0, 0.3, 0), glm::vec3(0, 1, 0));

    // Vulkan clip space has inverted Y and half Z.
    const glm::mat4 clip(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f);

    ubo.proj = clip * glm::perspective(glm::radians(45.0f), window_width / (float)window_height, 0.1f, 50.0f);

    void* data;
    VkResult result = vkMapMemory(device, uniform_staging_buffer_memory, 0, sizeof(ubo), 0, &data);
    check_vk_result(result, "vkMapMemory");
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device, uniform_staging_buffer_memory);

    // TODO: this commands is slow since it waits until queue finishes specified operation
    record_and_run_commands(device, command_pool, queue, [this](VkCommandBuffer command_buffer) {
        VkBufferCopy region;
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = sizeof(Uniform_Buffer_Object);
        vkCmdCopyBuffer(command_buffer, uniform_staging_buffer, uniform_buffer, 1, &region);
    });
}

void Vulkan_Demo::run_frame() {
    update_uniform_buffer();

    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_acquired, VK_NULL_HANDLE, &swapchain_image_index);
    check_vk_result(result, "vkAcquireNextImageKHR");

    VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_acquired;
    submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[swapchain_image_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &rendering_finished;

    result = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    check_vk_result(result, "vkQueueSubmit");

    VkPresentInfoKHR present_info;
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &rendering_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &swapchain_image_index;
    present_info.pResults = nullptr;

    result = vkQueuePresentKHR(queue, &present_info);
    check_vk_result(result, "vkQueuePresentKHR");
}
