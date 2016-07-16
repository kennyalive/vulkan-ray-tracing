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

struct VulkanState
{
    VkSwapchainKHR oldSwapchain;

    VulkanState()
        : oldSwapchain(VK_NULL_HANDLE)
    {}
};

VulkanState vulkanState;

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

VkSwapchainKHR CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSwapchainKHR oldSwapchain)
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
    swapChainCreateInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain);
    CheckResult(result, "failed to create swap chain");

    if (oldSwapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

    return swapchain;
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

    VkInstance instance = CreateInstance();
    std::cout << "Vulkan instance created\n";

    VkSurfaceKHR surface = CreateSurface(instance, wmInfo.info.win.window);

    VkPhysicalDevice physicalDevice = SelectPhysicalDevice(instance);

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

    if (!SelectQueueFamilies(physicalDevice, surface, graphicsQueueFamilyIndex, presentQueueFamilyIndex))
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

    VkDevice device;
    result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    CheckResult(result, "failed to create device");
    std::cout << "Device created\n";

    VkSwapchainKHR swapchain = CreateSwapchain(physicalDevice, device, surface, VK_NULL_HANDLE);

    // get queues
    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

    VkQueue presentQueue;
    vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);

    // destroy swapchain
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    // destroy device
    result = vkDeviceWaitIdle(device);
    CheckResult(result, "failed waiting for device idle state");

    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
    std::cout << "Device destroyed\n";

    // destroy vulkan instance
    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;
    std::cout << "Vulkan instance destroyed\n";

    // run event loop
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
            SDL_Delay(1);
        }
    }

    SDL_Quit();
    return 0;
}
