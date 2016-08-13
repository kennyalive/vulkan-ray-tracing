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
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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
} // namespace

VulkanApp::VulkanApp(int windowWidth, int windowHeight)
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
    graphicsQueue = deviceInfo.graphicsQueue;
    presentationQueue = deviceInfo.presentationQueue;
    
    SwapchainInfo swapchainInfo = CreateSwapchain(physicalDevice, device, surface);
    swapchain = swapchainInfo.swapchain;
    swapchainImageFormat = swapchainInfo.imageFormat;
    swapchainImages = swapchainInfo.images;

    renderPass = CreateRenderPass(device, swapchainInfo.imageFormat);

    CreateFramebuffers();
    CreateSemaphores();
    CreateCommandBuffers(deviceInfo.presentationQueueFamilyIndex);
}

void VulkanApp::CleanupResources()
{
    VkResult result = vkDeviceWaitIdle(device);
    CheckVkResult(result, "vkDeviceWaitIdle");

    vkDestroyCommandPool(device, presentQueueCommandPool, nullptr);
    presentQueueCommandPool = VK_NULL_HANDLE;

    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    imageAvailableSemaphore = VK_NULL_HANDLE;

    vkDestroySemaphore(device, renderingFinishedSemaphore, nullptr);
    renderingFinishedSemaphore = VK_NULL_HANDLE;

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

void VulkanApp::CreateCommandBuffers(uint32_t queueFamilyIndex)
{
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkResult result = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &presentQueueCommandPool);
    CheckVkResult(result, "vkCreateCommandPool");

    uint32_t imagesCount = static_cast<uint32_t>(swapchainImages.size());

    presentQueueCommandBuffers.resize(imagesCount);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = presentQueueCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = imagesCount;

    result = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, presentQueueCommandBuffers.data());
    CheckVkResult(result, "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    VkClearColorValue clearColor = {1.0f, 0.8f, 0.4f, 0.0f};

    VkImageSubresourceRange imageSubresourceRange;
    imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageSubresourceRange.baseMipLevel = 0;
    imageSubresourceRange.levelCount = 1;
    imageSubresourceRange.baseArrayLayer = 0;
    imageSubresourceRange.layerCount = 1;

    for (uint32_t i = 0; i < imagesCount; ++i)
    {
        VkImageMemoryBarrier barrierFromPresentToClear;
        barrierFromPresentToClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierFromPresentToClear.pNext = nullptr;
        barrierFromPresentToClear.srcAccessMask = 0;
        barrierFromPresentToClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierFromPresentToClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierFromPresentToClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierFromPresentToClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierFromPresentToClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierFromPresentToClear.image = swapchainImages[i];
        barrierFromPresentToClear.subresourceRange = imageSubresourceRange;

        VkImageMemoryBarrier barrierFromClearToPresent;
        barrierFromClearToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierFromClearToPresent.pNext = nullptr;
        barrierFromClearToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierFromClearToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrierFromClearToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierFromClearToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierFromClearToPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierFromClearToPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierFromClearToPresent.image = swapchainImages[i];
        barrierFromClearToPresent.subresourceRange = imageSubresourceRange;

        vkBeginCommandBuffer(presentQueueCommandBuffers[i], &commandBufferBeginInfo);
        vkCmdPipelineBarrier(
            presentQueueCommandBuffers[i],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrierFromPresentToClear
            );

        vkCmdClearColorImage(presentQueueCommandBuffers[i], swapchainImages[i],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &imageSubresourceRange);

        vkCmdPipelineBarrier(
            presentQueueCommandBuffers[i],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrierFromClearToPresent);

        result = vkEndCommandBuffer(presentQueueCommandBuffers[i]);
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
    submitInfo.pCommandBuffers = &presentQueueCommandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderingFinishedSemaphore;

    result = vkQueueSubmit(presentationQueue, 1, &submitInfo, VK_NULL_HANDLE);
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
