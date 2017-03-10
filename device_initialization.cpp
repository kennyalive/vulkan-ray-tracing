#include "device_initialization.h"
#include "vulkan_utilities.h"

#include <algorithm>
#include <string>
#include <vector>

const std::vector<const char*> instance_extensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME
};

const std::vector<const char*> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static bool is_extension_available(const std::vector<VkExtensionProperties>& properties, const char* extension_name) {
    for (const auto& property : properties) {
        if (strcmp(property.extensionName, extension_name) == 0)
            return true;
    }
    return false;
}

static uint32_t select_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    // select queue family with presentation and graphics support
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

VkInstance create_instance() {
    uint32_t count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    check_vk_result(result, "vkEnumerateInstanceExtensionProperties");

    std::vector<VkExtensionProperties> extension_properties(count);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &count, extension_properties.data());
    check_vk_result(result, "vkEnumerateInstanceExtensionProperties");

    for (auto name : instance_extensions) {
        if (!is_extension_available(extension_properties, name))
            error(std::string("required instance extension is not available: ") + name);
    }

    VkInstanceCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.pApplicationInfo = nullptr;
    desc.enabledLayerCount = 0;
    desc.ppEnabledLayerNames = nullptr;
    desc.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
    desc.ppEnabledExtensionNames = instance_extensions.data();

    VkInstance instance;
    result = vkCreateInstance(&desc, nullptr, &instance);
    check_vk_result(result, "vkCreateInstance");
    return instance;
}

VkPhysicalDevice select_physical_device(VkInstance instance) {
    uint32_t count;
    VkResult result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    check_vk_result(result, "vkEnumeratePhysicalDevices");

    if (count == 0)
        error("no physical device found");

    std::vector<VkPhysicalDevice> physical_devices(count);
    result = vkEnumeratePhysicalDevices(instance, &count, physical_devices.data());
    check_vk_result(result, "vkEnumeratePhysicalDevices");
    return physical_devices[0]; // just get the first one
}

Device_Info create_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
    uint32_t count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, nullptr);
    check_vk_result(result, "vkEnumerateDeviceExtensionProperties");

    std::vector<VkExtensionProperties> extension_properties(count);
    result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, extension_properties.data());
    check_vk_result(result, "vkEnumerateDeviceExtensionProperties");

    for (auto name : device_extensions) {
        if (!is_extension_available(extension_properties, name))
            error(std::string("required device extension is not available: ") + name);
    }

    const float priority = 1.0;
    VkDeviceQueueCreateInfo queue_desc;
    queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_desc.pNext = nullptr;
    queue_desc.flags = 0;
    queue_desc.queueFamilyIndex = select_queue_family(physical_device, surface);
    queue_desc.queueCount = 1;
    queue_desc.pQueuePriorities = &priority;

    VkDeviceCreateInfo device_desc;
    device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_desc.pNext = nullptr;
    device_desc.flags = 0;
    device_desc.queueCreateInfoCount = 1;
    device_desc.pQueueCreateInfos = &queue_desc;
    device_desc.enabledLayerCount = 0;
    device_desc.ppEnabledLayerNames = nullptr;
    device_desc.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    device_desc.ppEnabledExtensionNames = device_extensions.data();
    device_desc.pEnabledFeatures = nullptr;

    Device_Info device_info;
    result = vkCreateDevice(physical_device, &device_desc, nullptr, &device_info.device);
    check_vk_result(result, "vkCreateDevice");
    device_info.queue_family_index = queue_desc.queueFamilyIndex;
    vkGetDeviceQueue(device_info.device, queue_desc.queueFamilyIndex, 0, &device_info.queue);
    return device_info;
}
