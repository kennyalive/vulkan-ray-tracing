#include <algorithm>
#include <string>
#include <vector>

#include "device_initialization.h"
#include "vulkan_utilities.h"

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

static uint32_t select_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; i++) {
        VkBool32 presentation_supported;
        auto result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &presentation_supported);
        check_vk_result(result, "vkGetPhysicalDeviceSurfaceSupportKHR");

        if (presentation_supported && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            return i;
    }
    error("failed to find queue family");
    return -1;
}

VkInstance CreateInstance()
{
    // check instance extensions availability
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    check_vk_result(result, "vkEnumerateInstanceExtensionProperties");

    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
    check_vk_result(result, "vkEnumerateInstanceExtensionProperties");

    for (auto extensionName : instanceExtensionNames)
    {
        if (!IsExtensionAvailable(extensionProperties, extensionName))
            error(std::string("required instance extension is not available: ") + extensionName);
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
    check_vk_result(result, "vkCreateInstance");
    return instance;
}

VkPhysicalDevice SelectPhysicalDevice(VkInstance instance)
{
    uint32_t physicalDeviceCount;
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    check_vk_result(result, "vkEnumeratePhysicalDevices");

    if (physicalDeviceCount == 0)
        error("no physical device found");

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    check_vk_result(result, "vkEnumeratePhysicalDevices");

    return physicalDevices[0]; // just get the first one
}

Device_Info CreateDevice(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensionCount, nullptr);
    check_vk_result(result, "vkEnumerateDeviceExtensionProperties");

    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensionCount, extensionProperties.data());
    check_vk_result(result, "vkEnumerateDeviceExtensionProperties");

    for (auto extensionName : deviceExtensionNames)
    {
        if (!IsExtensionAvailable(extensionProperties, extensionName))
            error(std::string("required device extension is not available: ") + extensionName);
    }

    const float queuePriority = 1.0;
    VkDeviceQueueCreateInfo queue_desc;
    queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_desc.pNext = nullptr;
    queue_desc.flags = 0;
    queue_desc.queueFamilyIndex = select_queue_family(physical_device, surface);
    queue_desc.queueCount = 1;
    queue_desc.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo device_desc;
    device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_desc.pNext = nullptr;
    device_desc.flags = 0;
    device_desc.queueCreateInfoCount = 1;
    device_desc.pQueueCreateInfos = &queue_desc;
    device_desc.enabledLayerCount = 0;
    device_desc.ppEnabledLayerNames = nullptr;
    device_desc.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size());
    device_desc.ppEnabledExtensionNames = deviceExtensionNames.data();
    device_desc.pEnabledFeatures = nullptr;

    Device_Info deviceInfo;
    result = vkCreateDevice(physical_device, &device_desc, nullptr, &deviceInfo.device);
    check_vk_result(result, "vkCreateDevice");
    deviceInfo.queue_family_index = queue_desc.queueFamilyIndex;
    vkGetDeviceQueue(deviceInfo.device, queue_desc.queueFamilyIndex, 0, &deviceInfo.queue);
    return deviceInfo;
}
