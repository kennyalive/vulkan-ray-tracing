#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

#undef min
#undef max

void Error(const std::string& message)
{
    std::cout << message << std::endl;
    exit(1);
}

void CheckResult(VkResult result, const std::string& message)
{
    if (result != VK_SUCCESS)
        Error(message);
}

enum
{
    window_width = 640,
    window_height = 480,
};

struct SwapchainInfo
{
    VkSwapchainKHR handle;
    VkFormat imageFormat;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;

    SwapchainInfo()
        : handle(VK_NULL_HANDLE)
        , imageFormat(VK_FORMAT_UNDEFINED)
        {}
};

struct VulkanState
{
    VkInstance instance;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSurfaceKHR surface;
    SwapchainInfo swapchainInfo;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderingFinishedSemaphore;
    VkCommandPool presentQueueCommandPool;
    std::vector<VkCommandBuffer> presentQueueCommandBuffers;

    VulkanState()
        : instance(VK_NULL_HANDLE)
        , device(VK_NULL_HANDLE)
        , graphicsQueue(VK_NULL_HANDLE)
        , presentQueue(VK_NULL_HANDLE)
        , surface(VK_NULL_HANDLE)
        , imageAvailableSemaphore(VK_NULL_HANDLE)
        , renderingFinishedSemaphore(VK_NULL_HANDLE)
        , presentQueueCommandPool(VK_NULL_HANDLE)
    {}
};

VulkanState vulkanState;

VkRenderPass renderPass = VK_NULL_HANDLE;
std::vector<VkFramebuffer> framebuffers;

VkInstance CreateInstance()
{
    // check instance extensions availability
    uint32_t extensionsCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);
    CheckResult(result, "failed to get extensions count");

    std::vector<VkExtensionProperties> extensionProperties(extensionsCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, extensionProperties.data());
    CheckResult(result, "failed to enumerate available extensions");

    std::vector<const char*> extensionNames =
    {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    for (auto extensionName : extensionNames)
    {
        auto it = std::find_if(extensionProperties.cbegin(), extensionProperties.cend(),
            [&extensionName](const auto& extensionProperty)
        {
            return strcmp(extensionName, extensionProperty.extensionName) == 0;
        });
        if (it == extensionProperties.cend())
            Error(std::string("required instance extension is not supported: ") + extensionName);
    }

    // create vulkan instance
    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = nullptr;
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.ppEnabledLayerNames = nullptr;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
    instanceCreateInfo.ppEnabledExtensionNames = extensionNames.data();

    VkInstance instance;
    result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    CheckResult(result, "failed to create vulkan instance");
    return instance;
}

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
    CheckResult(result, "failed to create VkSurfaceKHR");
    return surface;
}

VkPhysicalDevice SelectPhysicalDevice(VkInstance instance)
{
    uint32_t physicalDevicesCount;
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, nullptr);
    CheckResult(result, "failed to get physical devices count");

    if (physicalDevicesCount == 0)
        Error("no physical devices found");

    std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, physicalDevices.data());
    CheckResult(result, "failed to enumerate physical devices");

    return physicalDevices[0]; // just get the first one
}

VkSurfaceFormatKHR SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
{
    assert(!surfaceFormats.empty());

    // special case that means we can choose any format
    if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        VkSurfaceFormatKHR surfaceFormat;
        surfaceFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
        surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        return surfaceFormat;
    }

    for (const auto& surfaceFormat : surfaceFormats)
    {
        if (surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
            return surfaceFormat;
    }
    return surfaceFormats[0];
}

SwapchainInfo CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface)
{
    // get surface capabilities
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    CheckResult(result, "failed to get surface capabilities");

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0)
        imageCount = std::min(imageCount,  surfaceCapabilities.maxImageCount);

    VkExtent2D imageExtent = surfaceCapabilities.currentExtent;
    if (surfaceCapabilities.currentExtent.width == -1)
    {
        imageExtent.width = std::min(
            surfaceCapabilities.maxImageExtent.width, 
            std::max(
                surfaceCapabilities.minImageExtent.width, 
                640u));

        imageExtent.height = std::min(
            surfaceCapabilities.maxImageExtent.height,
            std::max(
                surfaceCapabilities.minImageExtent.height,
                480u));
    }

    if ((surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        Error("VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swap chain");
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkSurfaceTransformFlagBitsKHR preTransform;
    if ((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0)
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        preTransform = surfaceCapabilities.currentTransform;

    // determine surface format
    uint32_t surfaceFormatsCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, nullptr);
    CheckResult(result, "faled to get surface formats count");

    if (surfaceFormatsCount == 0)
        Error("zero number of surface formats count");

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, surfaceFormats.data());
    CheckResult(result, "failed to get supported surface formats");

    VkSurfaceFormatKHR surfaceFormat = SelectSurfaceFormat(surfaceFormats);

    // determine present mode
    uint32_t presentModesCount;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, nullptr);
    CheckResult(result, "failed to get present mode count");

    if (presentModesCount == 0)
        Error("zero number of present modes count");

    std::vector<VkPresentModeKHR> presentModes(presentModesCount);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, presentModes.data());
    CheckResult(result, "failed to get supported present modes");

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto currentPresentMode : presentModes)
    {
        if (currentPresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = currentPresentMode;
            break;
        }
    }

    // create swap chain
    VkSwapchainCreateInfoKHR swapChainCreateInfo;
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.pNext = nullptr;
    swapChainCreateInfo.flags = 0;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = imageExtent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = imageUsage;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.queueFamilyIndexCount = 0;
    swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    swapChainCreateInfo.preTransform = preTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain);
    CheckResult(result, "failed to create swap chain");

    SwapchainInfo info;
    info.handle = swapchain;
    info.imageFormat = surfaceFormat.format;

    // get swapchain images
    uint32_t imagesCount;
    result = vkGetSwapchainImagesKHR(vulkanState.device, swapchain, &imagesCount, nullptr);
    CheckResult(result, "failed to get swapchain images count");

    info.images.resize(imagesCount);
    result = vkGetSwapchainImagesKHR(vulkanState.device, swapchain, &imagesCount, info.images.data());
    CheckResult(result, "failed to get swapchain images");

    // create views for swapchain images
    info.imageViews.resize(imagesCount);
    for (uint32_t i = 0; i < imagesCount; i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = info.images[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = info.imageFormat;
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
        result = vkCreateImageView(vulkanState.device, &imageViewCreateInfo, nullptr, &info.imageViews[i]);
        CheckResult(result, "failed to create image view");
    }
    return info;
}

bool SelectQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    uint32_t& graphicsQueueFamilyIndex, uint32_t& presentQueueFamilyIndex)
{
    uint32_t queueFamiliesCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamiliesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamilies.data());

    uint32_t selectedGraphicsQueueFamilyIndex;
    bool graphicsFound = false;

    uint32_t selectedPresentQueueFamilyIndex;
    bool presentFound = false;

    for (uint32_t i = 0; i < queueFamiliesCount; i++)
    {
        if (queueFamilies[i].queueCount == 0)
            continue;

        // check graphics operations support
        bool graphicsSupported = (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

        // check presentation support
        VkBool32 presentSupported;
        auto result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupported);
        CheckResult(result, "vkGetPhysicalDeviceSurfaceSupportKHR error");

        if (graphicsSupported)
        {
            selectedGraphicsQueueFamilyIndex = i;
            graphicsFound = true;
        }
        if (presentSupported == VK_TRUE)
        {
            selectedPresentQueueFamilyIndex = i;
            presentFound = true;
        }

        // check if we found a preferred queue that supports both present and graphics operations
        if (selectedGraphicsQueueFamilyIndex == i && 
            selectedPresentQueueFamilyIndex == i)
            break;
    }

    if (!graphicsFound || !presentFound)
        return false;

    graphicsQueueFamilyIndex = selectedGraphicsQueueFamilyIndex;
    presentQueueFamilyIndex = selectedPresentQueueFamilyIndex;
    return true;
}

VkRenderPass CreateRenderPass()
{
    VkAttachmentDescription attachmentDescription;
    attachmentDescription.flags = 0;
    attachmentDescription.format = vulkanState.swapchainInfo.imageFormat;
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
    VkResult result = vkCreateRenderPass(vulkanState.device, &renderPassCreateInfo, nullptr, &renderPass);
    CheckResult(result, "failed to create render pass");
    return renderPass;
}

void CleanupVulkanResources()
{
    VkResult result = vkDeviceWaitIdle(vulkanState.device);
    CheckResult(result, "vkDeviceWaitIdle failed");

    for (size_t i = 0; i < framebuffers.size(); i++)
    {
        vkDestroyFramebuffer(vulkanState.device, framebuffers[i], nullptr);
    }
    framebuffers.clear();

    auto& swapchainImageViews = vulkanState.swapchainInfo.imageViews;
    for (size_t i = 0; i < swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(vulkanState.device, swapchainImageViews[i], nullptr);
    }
    swapchainImageViews.clear();

    vkDestroyRenderPass(vulkanState.device, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;

    vkDestroySemaphore(vulkanState.device, vulkanState.imageAvailableSemaphore, nullptr);
    vulkanState.imageAvailableSemaphore = VK_NULL_HANDLE;

    vkDestroySemaphore(vulkanState.device, vulkanState.renderingFinishedSemaphore, nullptr);
    vulkanState.renderingFinishedSemaphore = VK_NULL_HANDLE;

    vkDestroyCommandPool(vulkanState.device, vulkanState.presentQueueCommandPool, nullptr);
    vulkanState.presentQueueCommandPool = VK_NULL_HANDLE;

    vkDestroySwapchainKHR(vulkanState.device, vulkanState.swapchainInfo.handle, nullptr);
    vulkanState.swapchainInfo = SwapchainInfo();

    vkDestroySurfaceKHR(vulkanState.instance, vulkanState.surface, nullptr);
    vulkanState.surface = VK_NULL_HANDLE;

    vkDestroyDevice(vulkanState.device, nullptr);
    vulkanState.device = VK_NULL_HANDLE;

    vkDestroyInstance(vulkanState.instance, nullptr);
    vulkanState.instance = VK_NULL_HANDLE;
}

void RunFrame()
{
    uint32_t imageIndex;

    VkResult result = vkAcquireNextImageKHR(
        vulkanState.device,
        vulkanState.swapchainInfo.handle,
        UINT64_MAX,
        vulkanState.imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex);

    CheckResult(result, "Failed to acquire presentation image");

    VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vulkanState.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vulkanState.presentQueueCommandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vulkanState.renderingFinishedSemaphore;

    result = vkQueueSubmit(vulkanState.presentQueue, 1, &submitInfo, VK_NULL_HANDLE);
    CheckResult(result, "failed to submit to presentation queue");

    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vulkanState.renderingFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vulkanState.swapchainInfo.handle;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(vulkanState.presentQueue, &presentInfo);
    CheckResult(result, "failed to present image");
}

void RunMainLoop()
{
    SDL_Event event;
    bool running = true;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = false;
        }
        if (running)
        {
            RunFrame();
            SDL_Delay(1);
        }
    }
}

int main()
{
    // create SDL window
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        Error("SDL_Init error");

    SDL_Window* window = SDL_CreateWindow("Vulkan app", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        window_width, window_height, SDL_WINDOW_SHOWN);
    if (window == nullptr)
    {
        SDL_Quit();
        Error("failed to create SDL window");
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version)
        if (SDL_GetWindowWMInfo(window, &wmInfo) == SDL_FALSE)
        {
            SDL_Quit();
            Error("failed to gt platform specific window information");
        }

    VkResult result;

    vulkanState.instance = CreateInstance();

    vulkanState.surface = CreateSurface(vulkanState.instance, wmInfo.info.win.window);

    VkPhysicalDevice physicalDevice = SelectPhysicalDevice(vulkanState.instance);

    // check device extensions availability
    uint32_t deviceExtensionsCount = 0;
    result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionsCount, nullptr);
    CheckResult(result, "failed to get device extensions count");

    std::vector<VkExtensionProperties> deviceExtensionProperties(deviceExtensionsCount);
    result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionsCount, deviceExtensionProperties.data());
    CheckResult(result, "failed to enumerate available device extensions");

    std::vector<const char*> deviceExtensionNames =
    {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    for (auto deviceExtensionName : deviceExtensionNames)
    {
        auto it = std::find_if(deviceExtensionProperties.cbegin(), deviceExtensionProperties.cend(),
            [&deviceExtensionName](const auto& extensionProperty)
        {
            return strcmp(deviceExtensionName, extensionProperty.extensionName) == 0;
        });
        if (it == deviceExtensionProperties.cend())
            Error(std::string("required device extension is not supported: ") + deviceExtensionName);
    }

    // determine indices of the queue families that support graphics and presentation operations
    uint32_t graphicsQueueFamilyIndex;
    uint32_t presentQueueFamilyIndex;

    if (!SelectQueueFamilies(physicalDevice, vulkanState.surface, graphicsQueueFamilyIndex, presentQueueFamilyIndex))
        Error("failed to select present and graphics queue families");

    // create device
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float queuePriority = 1.0;

    VkDeviceQueueCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    createInfo.queueCount = 1;
    createInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(createInfo);

    if (presentQueueFamilyIndex != graphicsQueueFamilyIndex)
    {
        createInfo = VkDeviceQueueCreateInfo();
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.queueFamilyIndex = presentQueueFamilyIndex;
        createInfo.queueCount = 1;
        createInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(createInfo);
    }

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;

    result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &vulkanState.device);
    CheckResult(result, "failed to create device");

    vulkanState.swapchainInfo = CreateSwapchain(physicalDevice, vulkanState.device, vulkanState.surface);

    uint32_t imagesCount = static_cast<uint32_t>(vulkanState.swapchainInfo.images.size());

    renderPass = CreateRenderPass();

    framebuffers.resize(imagesCount);
    for (uint32_t i = 0; i < imagesCount; i++)
    {
        VkFramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = nullptr;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = &vulkanState.swapchainInfo.imageViews[i];
        framebufferCreateInfo.width = window_width;
        framebufferCreateInfo.height = window_height;
        framebufferCreateInfo.layers = 1;

        result = vkCreateFramebuffer(vulkanState.device, &framebufferCreateInfo, nullptr, &framebuffers[i]);
        CheckResult(result, "failed to create framebuffer");
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    result = vkCreateSemaphore(vulkanState.device, &semaphoreCreateInfo, nullptr, &vulkanState.imageAvailableSemaphore);
    CheckResult(result, "failed to create semaphore");

    result = vkCreateSemaphore(vulkanState.device, &semaphoreCreateInfo, nullptr, &vulkanState.renderingFinishedSemaphore);
    CheckResult(result, "failed to create semaphore");

    // get queues
    vkGetDeviceQueue(vulkanState.device, graphicsQueueFamilyIndex, 0, &vulkanState.graphicsQueue);
    vkGetDeviceQueue(vulkanState.device, presentQueueFamilyIndex, 0, &vulkanState.presentQueue);

    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = presentQueueFamilyIndex;

    result = vkCreateCommandPool(vulkanState.device, &commandPoolCreateInfo, nullptr, &vulkanState.presentQueueCommandPool);
    CheckResult(result, "failed to create command pool");

    vulkanState.presentQueueCommandBuffers.resize(imagesCount);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = vulkanState.presentQueueCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = imagesCount;

    result = vkAllocateCommandBuffers(vulkanState.device, &commandBufferAllocateInfo, vulkanState.presentQueueCommandBuffers.data());
    CheckResult(result, "failed to allocate command buffers");

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
        barrierFromPresentToClear.image = vulkanState.swapchainInfo.images[i];
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
        barrierFromClearToPresent.image = vulkanState.swapchainInfo.images[i];
        barrierFromClearToPresent.subresourceRange = imageSubresourceRange;

        vkBeginCommandBuffer(vulkanState.presentQueueCommandBuffers[i], &commandBufferBeginInfo);
        vkCmdPipelineBarrier(
            vulkanState.presentQueueCommandBuffers[i],
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

        vkCmdClearColorImage(vulkanState.presentQueueCommandBuffers[i], vulkanState.swapchainInfo.images[i],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &imageSubresourceRange);

        vkCmdPipelineBarrier(
            vulkanState.presentQueueCommandBuffers[i],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrierFromClearToPresent);

        result = vkEndCommandBuffer(vulkanState.presentQueueCommandBuffers[i]);
        CheckResult(result, "failed to record command buffer");
    }

    RunMainLoop();
    CleanupVulkanResources();
    SDL_Quit();
    return 0;
}
