#include "common.h"
#include "device_initialization.h"
#include "swapchain_initialization.h"
#include "vulkan_demo.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <array>
#include <chrono>

namespace {
struct Uniform_Buffer_Object {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};

const std::vector<Vertex> vertices {
    {{-0.7f, -0.7f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.7f, -0.7f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.7f,  0.7f}, {0.0f, 0.0f, 1.0f}},
    {{-0.7f,  0.7f}, {0.3f, 0.3f, 0.3f}}
};

const std::vector<uint32_t> indices {0, 1, 2, 0, 2, 3};

VkSurfaceKHR CreateSurface(VkInstance instance, HWND hwnd)
{
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.hinstance = ::GetModuleHandle(nullptr);
    surfaceCreateInfo.hwnd = hwnd;

    VkSurfaceKHR surface;
    VkResult result = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
    CheckVkResult(result, "vkCreateWin32SurfaceKHR");
    return surface;
}

VkRenderPass CreateRenderPass(VkDevice device, VkFormat attachmentImageFormat)
{
    VkAttachmentDescription attachmentDescription;
    attachmentDescription.flags = 0;
    attachmentDescription.format = attachmentImageFormat;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachmentReference;
    attachmentReference.attachment = 0;
    attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &attachmentReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassCreateInfo.pDependencies = dependencies.data();

    VkRenderPass renderPass;
    VkResult result = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
    CheckVkResult(result, "vkCreateRenderPass");
    return renderPass;
}

VkPipelineShaderStageCreateInfo GetPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
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
} // namespace

Vulkan_Demo::Vulkan_Demo(uint32_t windowWidth, uint32_t windowHeight)
: windowWidth(windowWidth)
, windowHeight(windowHeight)
{
}

void Vulkan_Demo::CreateResources(HWND windowHandle)
{
    instance = CreateInstance();
    physicalDevice = SelectPhysicalDevice(instance);

    surface = CreateSurface(instance, windowHandle);

    DeviceInfo deviceInfo = CreateDevice(physicalDevice, surface);
    device = deviceInfo.device;
    graphicsQueueFamilyIndex = deviceInfo.graphicsQueueFamilyIndex;
    graphicsQueue = deviceInfo.graphicsQueue;
    presentationQueueFamilyIndex = deviceInfo.presentationQueueFamilyIndex;
    presentationQueue = deviceInfo.presentationQueue;

    allocator = std::make_unique<Device_Memory_Allocator>(physicalDevice, device);

    SwapchainInfo swapchainInfo = CreateSwapchain(physicalDevice, device, surface);
    swapchain = swapchainInfo.swapchain;
    swapchainImageFormat = swapchainInfo.imageFormat;
    swapchainImages = swapchainInfo.images;
    swapchainImageViews = swapchainInfo.imageViews;

    renderPass = CreateRenderPass(device, swapchainInfo.imageFormat);

    create_descriptor_set_layout();
    CreatePipeline();
    CreateFrameResources();

    create_vertex_buffer();
    create_index_buffer();
    create_uniform_buffer();
    create_descriptor_pool();
    create_descriptor_set();
    create_texture();
    create_texture_view();
    create_texture_sampler();
}

void Vulkan_Demo::CleanupResources() 
{
    auto destroy_buffer = [this](VkBuffer& buffer) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    };

    VkResult result = vkDeviceWaitIdle(device);
    CheckVkResult(result, "vkDeviceWaitIdle");

    vkDestroyCommandPool(device, commandPool, nullptr);
    commandPool = VK_NULL_HANDLE;

    for (auto& resources : frameResources)
    {
        vkDestroyFramebuffer(device, resources.framebuffer, nullptr);
        resources.framebuffer = VK_NULL_HANDLE;

        vkDestroySemaphore(device, resources.imageAvailableSemaphore, nullptr);
        resources.imageAvailableSemaphore = VK_NULL_HANDLE;

        vkDestroySemaphore(device, resources.renderingFinishedSemaphore, nullptr);
        resources.renderingFinishedSemaphore = VK_NULL_HANDLE;

        vkDestroyFence(device, resources.fence, nullptr);
        resources.fence = VK_NULL_HANDLE;
    }

    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
    descriptor_set_layout = VK_NULL_HANDLE;

    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    pipeline_layout = VK_NULL_HANDLE;

    vkDestroyPipeline(device, pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;

    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    descriptor_pool = VK_NULL_HANDLE;

    destroy_buffer(vertex_buffer);
    destroy_buffer(index_buffer);
    destroy_buffer(uniform_staging_buffer);
    destroy_buffer(uniform_buffer);

    vkDestroySampler(device, texture_image_sampler, nullptr);
    texture_image_sampler = VK_NULL_HANDLE;

    vkDestroyImageView(device, texture_image_view, nullptr);
    texture_image_view = VK_NULL_HANDLE;

    vkDestroyImage(device, texture_image, nullptr);
    texture_image = VK_NULL_HANDLE;

    for (auto imageView : swapchainImageViews)
        vkDestroyImageView(device, imageView, nullptr);
    swapchainImageViews.clear();

    vkDestroyRenderPass(device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
    swapchainImageFormat = VK_FORMAT_UNDEFINED;
    swapchainImages.clear();

    allocator.reset();

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;

    vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;

    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;
}

void Vulkan_Demo::create_descriptor_set_layout() {
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

    VkResult result = vkCreateDescriptorSetLayout(device, &desc, nullptr, &descriptor_set_layout);
    check_vk_result(result, "vkCreateDescriptorSetLayout");
}

void Vulkan_Demo::CreatePipeline()
{
    ShaderModule vertexShaderModule(device, "shaders/vert.spv");
    ShaderModule fragmentShaderModule(device, "shaders/frag.spv");

    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos
    {
        GetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
            vertexShaderModule.GetHandle(), "main"),
        GetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
            fragmentShaderModule.GetHandle(), "main")
    };

    VkVertexInputBindingDescription vertexBindingDescription;
    vertexBindingDescription.binding = 0;
    vertexBindingDescription.stride = sizeof(Vertex);
    vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescriptions;
    vertexAttributeDescriptions[0].location = 0;
    vertexAttributeDescriptions[0].binding = vertexBindingDescription.binding;
    vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributeDescriptions[0].offset = offsetof(struct Vertex, pos);

    vertexAttributeDescriptions[1].location = 1;
    vertexAttributeDescriptions[1].binding = vertexBindingDescription.binding;
    vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributeDescriptions[1].offset = offsetof(struct Vertex, color);

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
    viewport.width = static_cast<float>(windowWidth);
    viewport.height = static_cast<float>(windowHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = windowWidth;
    scissor.extent.height = windowHeight;

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

    VkPipelineLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = nullptr;
    layoutCreateInfo.flags = 0;
    layoutCreateInfo.setLayoutCount = 1;
    layoutCreateInfo.pSetLayouts = &descriptor_set_layout;
    layoutCreateInfo.pushConstantRangeCount = 0;
    layoutCreateInfo.pPushConstantRanges = nullptr;

    VkResult result = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipeline_layout);
    check_vk_result(result, "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStageCreateInfos.size());
    pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pTessellationState = nullptr;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &resterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.layout = pipeline_layout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
    CheckVkResult(result, "vkCreateGraphicsPipelines");
}

void Vulkan_Demo::create_vertex_buffer() {
    const VkDeviceSize size = vertices.size() * sizeof(Vertex);
    vertex_buffer = create_buffer(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, *allocator);

    VkBuffer staging_buffer = create_staging_buffer(device, size, *allocator, vertices.data());
    Scope_Exit_Action destroy_staging_buffer([&staging_buffer, this]() {
        vkDestroyBuffer(device, staging_buffer, nullptr);
    });

    record_and_run_commands(device, commandPool, graphicsQueue, [&staging_buffer, &size, this](VkCommandBuffer command_buffer) {
        VkBufferCopy region;
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = size;
        vkCmdCopyBuffer(command_buffer, staging_buffer, vertex_buffer, 1, &region);
        // NOTE: we do not need memory barrier here since record_and_run_commands performs queue wait operation
    });
}

void Vulkan_Demo::create_index_buffer() {
    const VkDeviceSize size = indices.size() * sizeof(indices[0]);
    index_buffer = create_buffer(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, *allocator);

    VkBuffer staging_buffer = create_staging_buffer(device, size, *allocator, indices.data());
    Scope_Exit_Action destroy_staging_buffer([&staging_buffer, this]() {
        vkDestroyBuffer(device, staging_buffer, nullptr);
    });

    record_and_run_commands(device, commandPool, graphicsQueue, [&staging_buffer, &size, this](VkCommandBuffer command_buffer) {
        VkBufferCopy region;
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = size;
        vkCmdCopyBuffer(command_buffer, staging_buffer, index_buffer, 1, &region);
        // NOTE: we do not need memory barrier here since record_and_run_commands performs queue wait operation
    });
}

void Vulkan_Demo::create_uniform_buffer() {
    auto size = static_cast<VkDeviceSize>(sizeof(Uniform_Buffer_Object));
    uniform_staging_buffer = create_permanent_staging_buffer(device, size, *allocator, uniform_staging_buffer_memory);
    uniform_buffer = create_buffer(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, *allocator);
}

void Vulkan_Demo::create_descriptor_pool() {
    VkDescriptorPoolSize pool_size;
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.maxSets = 1;
    desc.poolSizeCount = 1;
    desc.pPoolSizes = &pool_size;

    VkResult result = vkCreateDescriptorPool(device, &desc, nullptr, &descriptor_pool);
    check_vk_result(result, "vkCreateDescriptorPool");
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

    VkWriteDescriptorSet descriptor_write;
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.pNext = nullptr;
    descriptor_write.dstSet = descriptor_set;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write.pImageInfo = nullptr;
    descriptor_write.pBufferInfo = &buffer_info;
    descriptor_write.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
}

void Vulkan_Demo::create_texture() {
    int image_width, image_height, image_component_count;

    auto rgba_pixels = stbi_load("images/statue.jpg", &image_width, &image_height, &image_component_count, STBI_rgb_alpha);
    if (rgba_pixels == nullptr) {
        error("failed to load image file");
    }

    VkImage staging_image = create_staging_texture(device, image_width, image_height, VK_FORMAT_R8G8B8A8_UNORM, *allocator, rgba_pixels, 4);

    Scope_Exit_Action destroy_staging_image_action([this, &staging_image]() {
        vkDestroyImage(device, staging_image, nullptr);
    });
    stbi_image_free(rgba_pixels);

    texture_image = ::create_texture(device, image_width, image_height, VK_FORMAT_R8G8B8A8_UNORM, *allocator);

    record_and_run_commands(device, commandPool, graphicsQueue,
        [&staging_image, &image_width, &image_height, this](VkCommandBuffer command_buffer) {

        // peform image layout transitions
        VkImageMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.image = staging_image;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = texture_image;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

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

        // perform image layout transition
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.image = texture_image;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    });
}

void Vulkan_Demo::create_texture_view() {
    VkImageViewCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.image = texture_image;
    desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.subresourceRange.baseMipLevel = 0;
    desc.subresourceRange.levelCount = 1;
    desc.subresourceRange.baseArrayLayer = 0;
    desc.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(device, &desc, nullptr, &texture_image_view);
    check_vk_result(result, "vkCreateImageView");
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

    VkResult result = vkCreateSampler(device, &desc, nullptr, &texture_image_sampler);
    check_vk_result(result, "vkCreateSampler");
}

void Vulkan_Demo::CreateFrameResources()
{
    // Allocate command buffers.
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;

    commandPoolCreateInfo.flags = 
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    commandPoolCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

    VkResult result = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
    CheckVkResult(result, "vkCreateCommandPool");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(frameResources.size());

    std::vector<VkCommandBuffer> commandBuffers(frameResources.size());
    result = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data());
    CheckVkResult(result, "vkAllocateCommandBuffers");

    for (size_t i = 0; i < frameResources.size(); i++)
        frameResources[i].commandBuffer = commandBuffers[i];

    // Create semaphores.
    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    for (size_t i = 0; i < frameResources.size(); i++)
    {
        VkResult result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr,
            &frameResources[i].imageAvailableSemaphore);
        CheckVkResult(result, "vkCreateSemaphore");

        result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr,
            &frameResources[i].renderingFinishedSemaphore);
        CheckVkResult(result, "vkCreateSemaphore");
    }

    // Create fences.
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < frameResources.size(); i++)
    {
        result = vkCreateFence(device, &fenceCreateInfo, nullptr, &frameResources[i].fence);
        CheckVkResult(result, "vkCreateFence");
    }
}

void Vulkan_Demo::RecordCommandBuffer()
{
    // Recreate framebuffer for current swapchain image.
    auto& currentFrameResources = frameResources[frameResourcesIndex];

    if (currentFrameResources.framebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(device, currentFrameResources.framebuffer, nullptr);
        currentFrameResources.framebuffer = VK_NULL_HANDLE;
    }

    VkFramebufferCreateInfo framebufferCreateInfo;
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.pNext = nullptr;
    framebufferCreateInfo.flags = 0;
    framebufferCreateInfo.renderPass = renderPass;
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.pAttachments = &swapchainImageViews[swapchainImageIndex];
    framebufferCreateInfo.width = windowWidth;
    framebufferCreateInfo.height = windowHeight;
    framebufferCreateInfo.layers = 1;

    VkResult result = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &currentFrameResources.framebuffer);
    CheckVkResult(result, "vkCreateFramebuffer");

    // Record command buffer for current frame.
    VkImageSubresourceRange imageSubresourceRange;
    imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageSubresourceRange.baseMipLevel = 0;
    imageSubresourceRange.levelCount = 1;
    imageSubresourceRange.baseArrayLayer = 0;
    imageSubresourceRange.layerCount = 1;

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    result = vkBeginCommandBuffer(currentFrameResources.commandBuffer, &commandBufferBeginInfo);
    CheckVkResult(result, "vkBeginCommandBuffer");

    if (presentationQueue != graphicsQueue)
    {
        VkImageMemoryBarrier barrierFromPresentToDraw;
        barrierFromPresentToDraw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierFromPresentToDraw.pNext = nullptr;
        barrierFromPresentToDraw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrierFromPresentToDraw.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrierFromPresentToDraw.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierFromPresentToDraw.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierFromPresentToDraw.srcQueueFamilyIndex = presentationQueueFamilyIndex;
        barrierFromPresentToDraw.dstQueueFamilyIndex = graphicsQueueFamilyIndex;
        barrierFromPresentToDraw.image = swapchainImages[swapchainImageIndex];
        barrierFromPresentToDraw.subresourceRange = imageSubresourceRange;

        vkCmdPipelineBarrier(
            currentFrameResources.commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrierFromPresentToDraw
            );
    }

    VkClearValue clearValue;
    clearValue.color = {1.0f, 0.8f, 0.4f, 0.0f};

    VkRenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = currentFrameResources.framebuffer;
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = {windowWidth, windowHeight};
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(currentFrameResources.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(currentFrameResources.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentFrameResources.commandBuffer, 0, 1, &vertex_buffer, &offset);
    vkCmdBindIndexBuffer(currentFrameResources.commandBuffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(currentFrameResources.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdDrawIndexed(currentFrameResources.commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    vkCmdEndRenderPass(currentFrameResources.commandBuffer);

    if (presentationQueue != graphicsQueue)
    {
        VkImageMemoryBarrier barrierFromDrawToPresent;
        barrierFromDrawToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierFromDrawToPresent.pNext = nullptr;
        barrierFromDrawToPresent.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrierFromDrawToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrierFromDrawToPresent.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierFromDrawToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierFromDrawToPresent.srcQueueFamilyIndex = graphicsQueueFamilyIndex;
        barrierFromDrawToPresent.dstQueueFamilyIndex = presentationQueueFamilyIndex;
        barrierFromDrawToPresent.image = swapchainImages[swapchainImageIndex];
        barrierFromDrawToPresent.subresourceRange = imageSubresourceRange;

        vkCmdPipelineBarrier(
            currentFrameResources.commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrierFromDrawToPresent
            );
    }

    result = vkEndCommandBuffer(currentFrameResources.commandBuffer);
    CheckVkResult(result, "vkEndCommandBuffer");
}

void Vulkan_Demo::update_uniform_buffer() {
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() / 1000.f;

    Uniform_Buffer_Object ubo;
    ubo.model = glm::rotate(glm::mat4(), time * glm::radians(90.0f), glm::vec3(0, 0, 1));
    ubo.view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    // Vulkan clip space has inverted Y and half Z.
    const glm::mat4 clip(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f);

    ubo.proj = clip * glm::perspective(glm::radians(45.0f), windowWidth / (float)windowHeight, 0.1f, 10.0f);

    void* data;
    VkResult result = vkMapMemory(device, uniform_staging_buffer_memory, 0, sizeof(ubo), 0, &data);
    check_vk_result(result, "vkMapMemory");
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device, uniform_staging_buffer_memory);

    // TODO: this commands is slow since it waits until queue finishes specified operation
    record_and_run_commands(device, commandPool, graphicsQueue, [this](VkCommandBuffer command_buffer) {
        VkBufferCopy region;
        region.srcOffset = 0;
        region.dstOffset = 0;
        region.size = sizeof(Uniform_Buffer_Object);
        vkCmdCopyBuffer(command_buffer, uniform_staging_buffer, uniform_buffer, 1, &region);
    });
}

void Vulkan_Demo::RunFrame()
{
    update_uniform_buffer();

    const auto& currentFrameResources = frameResources[frameResourcesIndex];

    VkResult result = vkWaitForFences(device, 1, &currentFrameResources.fence, VK_FALSE, 1000000000);
    CheckVkResult(result, "vkWaitForFences");

    result = vkResetFences(device, 1, &currentFrameResources.fence);
    CheckVkResult(result, "vkResetFences");

    result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, currentFrameResources.imageAvailableSemaphore,
        VK_NULL_HANDLE, &swapchainImageIndex);
    CheckVkResult(result, "vkAcquireNextImageKHR");

    RecordCommandBuffer();

    VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &currentFrameResources.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &currentFrameResources.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &currentFrameResources.renderingFinishedSemaphore;

    result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, currentFrameResources.fence);
    CheckVkResult(result, "vkQueueSubmit");

    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &currentFrameResources.renderingFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &swapchainImageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(presentationQueue, &presentInfo);
    CheckVkResult(result, "vkQueuePresentKHR");

    frameResourcesIndex = (frameResourcesIndex + 1) % frameResources.size();
}
