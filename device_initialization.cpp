#include <algorithm>
#include <string>
#include <vector>

#include "common.h"
#include "device_initialization.h"

namespace
{
    const std::vector<const char*> instanceExtensionNames =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    const std::vector<const char*> deviceExtensionNames =
    {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& extensionProperties, const char* extensionName)
    {
        auto it = std::find_if(extensionProperties.cbegin(), extensionProperties.cend(),
            [&extensionName](const auto& extensionProperty)
        {
            return strcmp(extensionName, extensionProperty.extensionName) == 0;
        });
        return it != extensionProperties.cend();
    }
}

VkInstance CreateInstance()
{
    // check instance extensions availability
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    CheckVkResult(result, "vkEnumerateInstanceExtensionProperties");

    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
    CheckVkResult(result, "vkEnumerateInstanceExtensionProperties");

    for (auto extensionName : instanceExtensionNames)
    {
        if (!IsExtensionAvailable(extensionProperties, extensionName))
            Error(std::string("required instance extension is not available: ") + extensionName);
    }

    // create vulkan instance
    VkInstanceCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = nullptr;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensionNames.size());
    createInfo.ppEnabledExtensionNames = instanceExtensionNames.data();

    VkInstance instance;
    result = vkCreateInstance(&createInfo, nullptr, &instance);
    CheckVkResult(result, "vkCreateInstance");
    return instance;
}

VkPhysicalDevice SelectPhysicalDevice(VkInstance instance)
{
    uint32_t physicalDeviceCount;
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    CheckVkResult(result, "vkEnumeratePhysicalDevices");

    if (physicalDeviceCount == 0)
        Error("no physical device found");

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    CheckVkResult(result, "vkEnumeratePhysicalDevices");

    return physicalDevices[0]; // just get the first one
}

bool SelectQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    uint32_t& graphicsQueueFamilyIndex, uint32_t& presentQueueFamilyIndex)
{
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t selectedGraphicsQueueFamilyIndex;
    bool graphicsFound = false;

    uint32_t selectedPresentationQueueFamilyIndex;
    bool presentationFound = false;

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        if (queueFamilies[i].queueCount == 0)
            continue;

        // check graphics operations support
        bool graphicsSupported = (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

        // check presentation support
        VkBool32 presentationSupported;
        auto result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentationSupported);
        CheckVkResult(result, "vkGetPhysicalDeviceSurfaceSupportKHR");

        if (graphicsSupported)
        {
            selectedGraphicsQueueFamilyIndex = i;
            graphicsFound = true;
        }
        if (presentationSupported == VK_TRUE)
        {
            selectedPresentationQueueFamilyIndex = i;
            presentationFound = true;
        }

        // check if we found a preferred queue that supports both present and graphics operations
        if (selectedGraphicsQueueFamilyIndex == i && 
            selectedPresentationQueueFamilyIndex == i)
            break;
    }

    if (!graphicsFound || !presentationFound)
        return false;

    graphicsQueueFamilyIndex = selectedGraphicsQueueFamilyIndex;
    presentQueueFamilyIndex = selectedPresentationQueueFamilyIndex;
    return true;
}

VkDevice CreateDevice(VkPhysicalDevice physicalDevice, const std::vector<QueueInfo>& queueInfos)
{
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    CheckVkResult(result, "vkEnumerateDeviceExtensionProperties");

    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensionProperties.data());
    CheckVkResult(result, "vkEnumerateDeviceExtensionProperties");

    for (auto extensionName : deviceExtensionNames)
    {
        if (!IsExtensionAvailable(extensionProperties, extensionName))
            Error(std::string("required device extension is not available: ") + extensionName);
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float queuePriority = 1.0;
    for (const auto& queueInfo : queueInfos)
    {
        VkDeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = nullptr;
        queueCreateInfo.flags = 0;
        queueCreateInfo.queueFamilyIndex = queueInfo.queueFamilyIndex;
        queueCreateInfo.queueCount = queueInfo.queueCount;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size());
    createInfo.ppEnabledExtensionNames = deviceExtensionNames.data();
    createInfo.pEnabledFeatures = nullptr;

    VkDevice device;
    result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    CheckVkResult(result, "vkCreateDevice");
    return device;
}
