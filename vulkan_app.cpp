#include "common.h"
#include "device_initialization.h"
#include "swapchain_initialization.h"
#include "vulkan_app.h"

namespace 
{
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

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = nullptr;

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

VulkanApp::VulkanApp(uint32_t windowWidth, uint32_t windowHeight)
: windowWidth(windowWidth)
, windowHeight(windowHeight)
{
}

void VulkanApp::CreateResources(HWND windowHandle)
{
    instance = CreateInstance();
    VkPhysicalDevice physicalDevice = SelectPhysicalDevice(instance);

    surface = CreateSurface(instance, windowHandle);

    DeviceInfo deviceInfo = CreateDevice(physicalDevice, surface);
    device = deviceInfo.device;
    graphicsQueueFamilyIndex = deviceInfo.graphicsQueueFamilyIndex;
    graphicsQueue = deviceInfo.graphicsQueue;
    presentationQueueFamilyIndex = deviceInfo.presentationQueueFamilyIndex;
    presentationQueue = deviceInfo.presentationQueue;
    
    SwapchainInfo swapchainInfo = CreateSwapchain(physicalDevice, device, surface);
    swapchain = swapchainInfo.swapchain;
    swapchainImageFormat = swapchainInfo.imageFormat;
    swapchainImages = swapchainInfo.images;

    renderPass = CreateRenderPass(device, swapchainInfo.imageFormat);

    CreateFramebuffers();
    CreatePipeline();
    CreateSemaphores();
    CreateCommandBuffers();
}

void VulkanApp::CleanupResources()
{
    VkResult result = vkDeviceWaitIdle(device);
    CheckVkResult(result, "vkDeviceWaitIdle");

    vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
    graphicsCommandPool = VK_NULL_HANDLE;

    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    imageAvailableSemaphore = VK_NULL_HANDLE;

    vkDestroySemaphore(device, renderingFinishedSemaphore, nullptr);
    renderingFinishedSemaphore = VK_NULL_HANDLE;

    vkDestroyPipeline(device, pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;

    for (size_t i = 0; i < framebuffers.size(); i++)
        vkDestroyFramebuffer(device, framebuffers[i], nullptr);
    framebuffers.clear();

    for (auto imageView : framebufferImageViews)
        vkDestroyImageView(device, imageView, nullptr);
    framebufferImageViews.clear();

    vkDestroyRenderPass(device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
    swapchainImageFormat = VK_FORMAT_UNDEFINED;
    swapchainImages.clear();

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;

    vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;

    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;
}

void VulkanApp::CreateFramebuffers()
{
    VkResult result;
    uint32_t imagesCount = static_cast<uint32_t>(swapchainImages.size());

    // create views for swapchain images
    framebufferImageViews.resize(imagesCount);
    for (uint32_t i = 0; i < imagesCount; i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = swapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = swapchainImageFormat;
        imageViewCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        imageViewCreateInfo.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            1,
            0,
            1
        };
        result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &framebufferImageViews[i]);
        CheckVkResult(result, "vkCreateImageView");
    }

    framebuffers.resize(imagesCount);
    for (uint32_t i = 0; i < imagesCount; i++)
    {
        VkFramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = nullptr;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = &framebufferImageViews[i];
        framebufferCreateInfo.width = windowWidth;
        framebufferCreateInfo.height = windowHeight;
        framebufferCreateInfo.layers = 1;

        result = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffers[i]);
        CheckVkResult(result, "vkCreateFramebuffer");
    }
}

void VulkanApp::CreatePipeline()
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

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.pNext = nullptr;
    vertexInputStateCreateInfo.flags = 0;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

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
    layoutCreateInfo.setLayoutCount = 0;
    layoutCreateInfo.pSetLayouts = nullptr;
    layoutCreateInfo.pushConstantRangeCount = 0;
    layoutCreateInfo.pPushConstantRanges = nullptr;

    PipelineLayout pipelineLayout(device, layoutCreateInfo);

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
    pipelineCreateInfo.layout = pipelineLayout.GetHandle();
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
    CheckVkResult(result, "vkCreateGraphicsPipelines");
}

void VulkanApp::CreateSemaphores()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    VkResult result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore);
    CheckVkResult(result, "vkCreateSemaphore");

    result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderingFinishedSemaphore);
    CheckVkResult(result, "vkCreateSemaphore");
}

void VulkanApp::CreateCommandBuffers()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

    VkResult result = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &graphicsCommandPool);
    CheckVkResult(result, "vkCreateCommandPool");

    uint32_t imagesCount = static_cast<uint32_t>(swapchainImages.size());

    graphicsCommandBuffers.resize(imagesCount);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = graphicsCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = imagesCount;

    result = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, graphicsCommandBuffers.data());
    CheckVkResult(result, "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    VkImageSubresourceRange imageSubresourceRange;
    imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageSubresourceRange.baseMipLevel = 0;
    imageSubresourceRange.levelCount = 1;
    imageSubresourceRange.baseArrayLayer = 0;
    imageSubresourceRange.layerCount = 1;

    VkClearValue clearValue;
    clearValue.color = {1.0f, 0.8f, 0.4f, 0.0f};

    for (uint32_t i = 0; i < imagesCount; ++i)
    {
        result = vkBeginCommandBuffer(graphicsCommandBuffers[i], &commandBufferBeginInfo);
        CheckVkResult(result, "vkBeginCommandBuffer");

        if (presentationQueue != graphicsQueue)
        {
            VkImageMemoryBarrier barrierFromPresentToDraw;
            barrierFromPresentToDraw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromPresentToDraw.pNext = nullptr;
            barrierFromPresentToDraw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromPresentToDraw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromPresentToDraw.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromPresentToDraw.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromPresentToDraw.srcQueueFamilyIndex = presentationQueueFamilyIndex;
            barrierFromPresentToDraw.dstQueueFamilyIndex = graphicsQueueFamilyIndex;
            barrierFromPresentToDraw.image = swapchainImages[i];
            barrierFromPresentToDraw.subresourceRange = imageSubresourceRange;

            vkCmdPipelineBarrier(
                graphicsCommandBuffers[i],
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

        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = framebuffers[i];
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = {windowWidth, windowHeight};
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(graphicsCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(graphicsCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(graphicsCommandBuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(graphicsCommandBuffers[i]);

        if (presentationQueue != graphicsQueue)
        {
            VkImageMemoryBarrier barrierFromDrawToPresent;
            barrierFromDrawToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromDrawToPresent.pNext = nullptr;
            barrierFromDrawToPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromDrawToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromDrawToPresent.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.srcQueueFamilyIndex = graphicsQueueFamilyIndex;
            barrierFromDrawToPresent.dstQueueFamilyIndex = presentationQueueFamilyIndex;
            barrierFromDrawToPresent.image = swapchainImages[i];
            barrierFromDrawToPresent.subresourceRange = imageSubresourceRange;

            vkCmdPipelineBarrier(
                graphicsCommandBuffers[i],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrierFromDrawToPresent
                );
        }

        result = vkEndCommandBuffer(graphicsCommandBuffers[i]);
        CheckVkResult(result, "vkEndCommandBuffer");
    }
}

void VulkanApp::RunFrame()
{
    uint32_t imageIndex;

    VkResult result = vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,
        imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex);

    CheckVkResult(result, "vkAcquireNextImageKHR");

    VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &graphicsCommandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderingFinishedSemaphore;

    result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    CheckVkResult(result, "vkQueueSubmit");

    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderingFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(presentationQueue, &presentInfo);
    CheckVkResult(result, "vkQueuePresentKHR");
}
