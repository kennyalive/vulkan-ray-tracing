#include "std.h"
#include "lib.h"

#define VMA_IMPLEMENTATION
#include "vk.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw/glfw3.h"
#include "glfw/glfw3native.h"

#include "vulkan/vk_enum_string_helper.h"
const char* get_VkResult_string(VkResult result)
{
    return string_VkResult(result);
}

static const VkDescriptorPoolSize descriptor_pool_sizes[] = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             16},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             16},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_SAMPLER,                    16},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 16},
};

constexpr uint32_t max_descriptor_sets = 64;
constexpr uint32_t max_timestamp_queries = 64;

//
// Vk_Instance is a container that stores common Vulkan resources like vulkan instance,
// device, command pool, swapchain, etc.
//
Vk_Instance vk;

#ifdef _WIN32
VkSurfaceKHR platform_create_surface(VkInstance instance, GLFWwindow* window) {
    VkWin32SurfaceCreateInfoKHR desc{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    desc.hinstance = ::GetModuleHandle(nullptr);
    desc.hwnd = glfwGetWin32Window(window);
    VkSurfaceKHR surface;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &desc, nullptr, &surface));
    return surface;
}
#else
#error platform_create_surface() is not implemented on this platform
#endif

static void create_instance(bool enable_validation_layers) {
    const char* instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    uint32_t count = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> extension_properties(count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, extension_properties.data()));

    for (auto name : instance_extensions) {
        bool supported = false;
        for (const auto& property : extension_properties) {
            if (!strcmp(property.extensionName, name)) {
                supported = true;
                break;
            }
        }
        if (!supported)
            error("Required instance extension is not available: " + std::string(name));
    }

    VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_create_info { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = sizeof(instance_extensions)/sizeof(instance_extensions[0]);
    instance_create_info.ppEnabledExtensionNames = instance_extensions;

    if (enable_validation_layers) {
        static const char* layer_names[] = {
            "VK_LAYER_KHRONOS_validation"
        };
		instance_create_info.enabledLayerCount = (uint32_t)std::size(layer_names);
		instance_create_info.ppEnabledLayerNames = layer_names;
    }    

    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &vk.instance));
}

static void create_device(GLFWwindow* window) {
    // select physical device
    {
        uint32_t count;
        VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &count, nullptr));

        if (count == 0)
            error("There are no Vulkan physical devices available");

        std::vector<VkPhysicalDevice> physical_devices(count);
        VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &count, physical_devices.data()));

        for (const auto& physical_device : physical_devices)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(physical_device, &props);

            // Check for device-level Vulkan 1.3 compatibility.
            if (VK_VERSION_MAJOR(props.apiVersion) == 1 && VK_VERSION_MINOR(props.apiVersion) >= 3)
            {
                vk.physical_device = physical_device;
                vk.timestamp_period_ms = (double)props.limits.timestampPeriod * 1e-6;
                break;
            }
        }

        if (vk.physical_device == nullptr)
            error("Failed to find physical device that supports requested Vulkan API version");
    }

    vk.surface = platform_create_surface(vk.instance, window);

    // select queue family
    {
        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(vk.physical_device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(vk.physical_device, &queue_family_count, queue_families.data());

        // select queue family with presentation and graphics support
        vk.queue_family_index = -1;
        for (uint32_t i = 0; i < queue_family_count; i++) {
            VkBool32 presentation_supported;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vk.physical_device, i, vk.surface, &presentation_supported));

            if (presentation_supported && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                vk.queue_family_index = i;
                break;
            }
        }
        if (vk.queue_family_index == -1)
            error("Vulkan: failed to find queue family");
    }

    // create VkDevice
    {
        std::vector<const char*> device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, // nullDescriptor feature
        };

        uint32_t count = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vk.physical_device, nullptr, &count, nullptr));
        std::vector<VkExtensionProperties> extension_properties(count);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vk.physical_device, nullptr, &count, extension_properties.data()));

        auto is_extension_supported = [&extension_properties](const char* extension_name) {
            for (const auto& property : extension_properties) {
                if (!strcmp(property.extensionName, extension_name)) {
                    return true;
                }
            }
            return false;
        };
        for (auto required_extension : device_extensions) {
            if (!is_extension_supported(required_extension))
                error("Vulkan: required device extension is not available: " + std::string(required_extension));
        }

        // Check for ray tracing support.
        {
            VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };

            VkPhysicalDeviceFeatures2 features2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
            features2.pNext = &acceleration_structure_features;
            acceleration_structure_features.pNext = &ray_tracing_pipeline_features;

            vkGetPhysicalDeviceFeatures2(vk.physical_device, &features2);
            bool raytracing_supported = acceleration_structure_features.accelerationStructure && ray_tracing_pipeline_features.rayTracingPipeline;
            if (!raytracing_supported)
                error("This vulkan implementation does not support raytracing");

            assert(is_extension_supported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME));
            assert(is_extension_supported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME));
            assert(is_extension_supported(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME));

            device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

            assert(is_extension_supported(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME));
            device_extensions.push_back(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
        }

        const float priority = 1.0;
        VkDeviceQueueCreateInfo queue_create_info { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queue_create_info.queueFamilyIndex = vk.queue_family_index;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &priority;

        VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
        buffer_device_address_features.bufferDeviceAddress = VK_TRUE;

        VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
        dynamic_rendering_features.dynamicRendering = VK_TRUE;

        VkPhysicalDeviceSynchronization2Features synchronization2_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
        synchronization2_features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
        descriptor_indexing_features.runtimeDescriptorArray = VK_TRUE;

        VkPhysicalDeviceMaintenance4Features maintenance4_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES };
        maintenance4_features.maintenance4 = VK_TRUE;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        acceleration_structure_features.accelerationStructure = VK_TRUE;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;

        VkPhysicalDeviceRobustness2FeaturesEXT robustness2_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT };
        robustness2_features.nullDescriptor = VK_TRUE;

        VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR ray_tracing_position_fetch_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR };
        ray_tracing_position_fetch_features.rayTracingPositionFetch = VK_TRUE;

        buffer_device_address_features.pNext = &dynamic_rendering_features;
        dynamic_rendering_features.pNext = &synchronization2_features;
        synchronization2_features.pNext = &descriptor_indexing_features;
        descriptor_indexing_features.pNext = &maintenance4_features;
        maintenance4_features.pNext = &acceleration_structure_features;
        acceleration_structure_features.pNext = &ray_tracing_pipeline_features;
        ray_tracing_pipeline_features.pNext = &robustness2_features;
        robustness2_features.pNext = &ray_tracing_position_fetch_features;

        VkPhysicalDeviceFeatures2 features2 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        features2.pNext = &buffer_device_address_features;

        VkDeviceCreateInfo device_create_info { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        device_create_info.pNext = &features2;
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.enabledExtensionCount = (uint32_t)device_extensions.size();
        device_create_info.ppEnabledExtensionNames = device_extensions.data();

        VK_CHECK(vkCreateDevice(vk.physical_device, &device_create_info, nullptr, &vk.device));
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT          message_severity,
    VkDebugUtilsMessageTypeFlagsEXT                 message_type,
    const VkDebugUtilsMessengerCallbackDataEXT*     callback_data,
    void*                                           user_data)
{
#ifdef _WIN32
    printf("%s\n", callback_data->pMessage);
    OutputDebugStringA(callback_data->pMessage);
    OutputDebugStringA("\n");
    DebugBreak();
#endif
    return VK_FALSE;
}

void Vk_Image::destroy() {
    vmaDestroyImage(vk.allocator, handle, allocation);
    vkDestroyImageView(vk.device, view, nullptr);
    *this = Vk_Image{};
}

void Vk_Buffer::destroy() {
    vmaDestroyBuffer(vk.allocator, handle, allocation);
    *this = Vk_Buffer{};
}

void vk_initialize(GLFWwindow* window, bool enable_validation_layers) {
    VK_CHECK(volkInitialize());
    uint32_t instance_version = volkGetInstanceVersion();

    // Require version 1.1 or higher of instance-level functionality.
    // "As long as the instance supports at least Vulkan 1.1, an application can use different 
    // versions of Vulkan with an instance than it does with a device or physical device."
    bool instance_version_higher_than_or_equal_to_1_1 =
        VK_VERSION_MAJOR(instance_version) > 1 || VK_VERSION_MINOR(instance_version) >= 1;
    if (!instance_version_higher_than_or_equal_to_1_1)
        error("The supported instance version is Vulkan 1.1 or higher, but Vulkan 1.0 loader is detected");

    create_instance(enable_validation_layers);
    volkLoadInstance(vk.instance);

    // Create debug messenger as early as possible (even before VkDevice is created).
    {
        VkDebugUtilsMessengerCreateInfoEXT desc{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

        desc.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        desc.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        desc.pfnUserCallback = &debug_utils_messenger_callback;
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(vk.instance, &desc, nullptr, &vk.debug_utils_messenger));
    }

    create_device(window);
    volkLoadDevice(vk.device);

    vkGetDeviceQueue(vk.device, vk.queue_family_index, 0, &vk.queue);

    // Initialize Vulkan memory allocator.
    {
        VmaVulkanFunctions alloc_funcs{};
        alloc_funcs.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        alloc_funcs.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        alloc_funcs.vkAllocateMemory = vkAllocateMemory;
        alloc_funcs.vkFreeMemory = vkFreeMemory;
        alloc_funcs.vkMapMemory = vkMapMemory;
        alloc_funcs.vkUnmapMemory = vkUnmapMemory;
        alloc_funcs.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        alloc_funcs.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        alloc_funcs.vkBindBufferMemory = vkBindBufferMemory;
        alloc_funcs.vkBindImageMemory = vkBindImageMemory;
        alloc_funcs.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        alloc_funcs.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        alloc_funcs.vkCreateBuffer = vkCreateBuffer;
        alloc_funcs.vkDestroyBuffer = vkDestroyBuffer;
        alloc_funcs.vkCreateImage = vkCreateImage;
        alloc_funcs.vkDestroyImage = vkDestroyImage;
        alloc_funcs.vkCmdCopyBuffer = vkCmdCopyBuffer;
        alloc_funcs.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
        alloc_funcs.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
        alloc_funcs.vkBindBufferMemory2KHR = vkBindBufferMemory2;
        alloc_funcs.vkBindImageMemory2KHR = vkBindImageMemory2;
        alloc_funcs.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
        alloc_funcs.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
        alloc_funcs.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

        VmaAllocatorCreateInfo allocator_info{};
        allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocator_info.physicalDevice = vk.physical_device;
        allocator_info.device = vk.device;
        allocator_info.instance = vk.instance;
        allocator_info.pVulkanFunctions = &alloc_funcs;
        allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
        VK_CHECK(vmaCreateAllocator(&allocator_info, &vk.allocator));
    }

    // Sync primitives.
    {
        VkSemaphoreCreateInfo desc { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.image_acquired_semaphore[0]));
        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.image_acquired_semaphore[1]));
        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.rendering_finished_semaphore[0]));
        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.rendering_finished_semaphore[1]));

        VkFenceCreateInfo fence_desc { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(vk.device, &fence_desc, nullptr, &vk.frame_fence[0]));
        VK_CHECK(vkCreateFence(vk.device, &fence_desc, nullptr, &vk.frame_fence[1]));
    }

    // Command pool.
    {
        VkCommandPoolCreateInfo desc { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        desc.queueFamilyIndex = vk.queue_family_index;
        VK_CHECK(vkCreateCommandPool(vk.device, &desc, nullptr, &vk.command_pools[0]));
        VK_CHECK(vkCreateCommandPool(vk.device, &desc, nullptr, &vk.command_pools[1]));
    }

    // Command buffer.
    {
        VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        alloc_info.commandPool = vk.command_pools[0];
        VK_CHECK(vkAllocateCommandBuffers(vk.device, &alloc_info, &vk.command_buffers[0]));
        alloc_info.commandPool = vk.command_pools[1];
        VK_CHECK(vkAllocateCommandBuffers(vk.device, &alloc_info, &vk.command_buffers[1]));
    }

    // Descriptor pool.
    {
        std::vector<VkDescriptorPoolSize> pool_sizes;
        for (size_t i = 0; i < std::size(descriptor_pool_sizes); i++) {
            pool_sizes.push_back(descriptor_pool_sizes[i]);
        }
        VkDescriptorPoolCreateInfo desc{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        desc.maxSets = max_descriptor_sets;
        desc.poolSizeCount = (uint32_t)pool_sizes.size();
        desc.pPoolSizes = pool_sizes.data();

        VK_CHECK(vkCreateDescriptorPool(vk.device, &desc, nullptr, &vk.descriptor_pool));
        vk_set_debug_name(vk.descriptor_pool, "descriptor_pool");
    }

    // Select surface format.
    {
        uint32_t format_count;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, nullptr));
        assert(format_count > 0);

        std::vector<VkSurfaceFormatKHR> candidates(format_count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, candidates.data()));

        // don't support special case described in the spec
        assert(!(candidates.size() == 1 && candidates[0].format == VK_FORMAT_UNDEFINED));

        // use non-srgb formats for swapchain images, so we can render to swapchain from compute,
        // also it means we should do srgb encoding manually.
        VkFormat supported_formats[] = {
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM
        };

        [&candidates, &supported_formats]() {
            for (VkFormat format : supported_formats) {
                for (VkSurfaceFormatKHR surface_format : candidates) {
                    if (surface_format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                        continue;
                    if (surface_format.format == format) {
                        vk.surface_format = surface_format;
                        return;
                    }
                }
            }
            error("Failed to find supported surface format");
        } ();
    }

    vk_create_swapchain(true);

    // Query pool.
    {
        VkQueryPoolCreateInfo create_info { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        create_info.queryCount = max_timestamp_queries;
        VK_CHECK(vkCreateQueryPool(vk.device, &create_info, nullptr, &vk.timestamp_query_pools[0]));
        VK_CHECK(vkCreateQueryPool(vk.device, &create_info, nullptr, &vk.timestamp_query_pools[1]));
    }
}

void vk_shutdown() {
    vkDeviceWaitIdle(vk.device);

    if (vk.staging_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vk.allocator, vk.staging_buffer, vk.staging_buffer_allocation);
    }

    vkDestroyCommandPool(vk.device, vk.command_pools[0], nullptr);
    vkDestroyCommandPool(vk.device, vk.command_pools[1], nullptr);
    vkDestroyDescriptorPool(vk.device, vk.descriptor_pool, nullptr);
    vkDestroySemaphore(vk.device, vk.image_acquired_semaphore[0], nullptr);
    vkDestroySemaphore(vk.device, vk.image_acquired_semaphore[1], nullptr);
    vkDestroySemaphore(vk.device, vk.rendering_finished_semaphore[0], nullptr);
    vkDestroySemaphore(vk.device, vk.rendering_finished_semaphore[1], nullptr);
    vkDestroyFence(vk.device, vk.frame_fence[0], nullptr);
    vkDestroyFence(vk.device, vk.frame_fence[1], nullptr);
    vkDestroyQueryPool(vk.device, vk.timestamp_query_pools[0], nullptr);
    vkDestroyQueryPool(vk.device, vk.timestamp_query_pools[1], nullptr);
    vk_destroy_swapchain();
    vmaDestroyAllocator(vk.allocator);
    vkDestroyDevice(vk.device, nullptr);
    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.debug_utils_messenger, nullptr);
    vkDestroyInstance(vk.instance, nullptr);
}

void vk_create_swapchain(bool vsync) {
    assert(vk.swapchain_info.handle == VK_NULL_HANDLE);

    VkSurfaceCapabilitiesKHR surface_caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical_device, vk.surface, &surface_caps));

    vk.surface_size = surface_caps.currentExtent;

    // don't expect special value described in spec on Win32
    assert(vk.surface_size.width != 0xffffffff && vk.surface_size.height != 0xffffffff);

    // we should not try to create a swapchain when window is minimized
    assert(vk.surface_size.width != 0 && vk.surface_size.height != 0);

    // VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
    if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        error("vk_create_swapchain: VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain");

    // determine present mode and swapchain image count
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t min_image_count = std::max(2u, surface_caps.minImageCount);

    if (!vsync) {
        uint32_t present_mode_count;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical_device, vk.surface, &present_mode_count, nullptr));
        std::vector<VkPresentModeKHR> present_modes(present_mode_count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical_device, vk.surface, &present_mode_count, present_modes.data()));

        for (auto pm : present_modes) {
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                min_image_count = std::max(3u, surface_caps.minImageCount);
                break; // mailbox is preferred mode
            }
            if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                min_image_count = std::max(2u, surface_caps.minImageCount);
            }
        }
    }
    if (surface_caps.maxImageCount > 0) {
        min_image_count = std::min(min_image_count, surface_caps.maxImageCount);
    }

    // create swap chain
    VkSwapchainCreateInfoKHR desc { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    desc.surface            = vk.surface;
    desc.minImageCount      = min_image_count;
    desc.imageFormat        = vk.surface_format.format;
    desc.imageColorSpace    = vk.surface_format.colorSpace;
    desc.imageExtent        = vk.surface_size;
    desc.imageArrayLayers   = 1;
    desc.imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    desc.imageSharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    desc.preTransform       = surface_caps.currentTransform;
    desc.compositeAlpha     = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    desc.presentMode        = present_mode;
    desc.clipped            = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(vk.device, &desc, nullptr, &vk.swapchain_info.handle));

    // retrieve swapchain images
    uint32_t image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(vk.device, vk.swapchain_info.handle, &image_count, nullptr));
    vk.swapchain_info.images.resize(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(vk.device, vk.swapchain_info.handle, &image_count, vk.swapchain_info.images.data()));

    vk.swapchain_info.image_views.resize(image_count);

    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo desc { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        desc.image          = vk.swapchain_info.images[i];
        desc.viewType       = VK_IMAGE_VIEW_TYPE_2D;
        desc.format         = vk.surface_format.format;

        desc.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.subresourceRange.baseMipLevel   = 0;
        desc.subresourceRange.levelCount     = 1;
        desc.subresourceRange.baseArrayLayer = 0;
        desc.subresourceRange.layerCount     = 1;

        VK_CHECK(vkCreateImageView(vk.device, &desc, nullptr, &vk.swapchain_info.image_views[i]));
    }
}

void vk_destroy_swapchain() {
    for (auto image_view : vk.swapchain_info.image_views) {
        vkDestroyImageView(vk.device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(vk.device, vk.swapchain_info.handle, nullptr);
    vk.swapchain_info = Swapchain_Info{};
}

void vk_ensure_staging_buffer_allocation(VkDeviceSize size) {
    if (vk.staging_buffer_size >= size)
        return;

    if (vk.staging_buffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(vk.allocator, vk.staging_buffer, vk.staging_buffer_allocation);

    VkBufferCreateInfo buffer_create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_create_info.size = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo alloc_create_info{};
    alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_create_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // to avoid manual flush/invalidation

    VmaAllocationInfo alloc_info;
    VK_CHECK(vmaCreateBuffer(vk.allocator, &buffer_create_info, &alloc_create_info, &vk.staging_buffer, &vk.staging_buffer_allocation, &alloc_info));

    vk.staging_buffer_ptr = (uint8_t*)alloc_info.pMappedData;
    vk.staging_buffer_size = size;
}

Vk_Buffer vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data, const char* name)
{
    return vk_create_buffer_with_alignment(size, usage, 1, data, name);
}

Vk_Buffer vk_create_buffer_with_alignment(VkDeviceSize size, VkBufferUsageFlags usage, uint32_t min_alignment,
    const void* data, const char* name)
{
    VkBufferCreateInfo buffer_create_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_create_info.size = size;
    buffer_create_info.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo alloc_create_info{};
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    Vk_Buffer buffer;
    VK_CHECK(vmaCreateBufferWithAlignment(vk.allocator, &buffer_create_info, &alloc_create_info, min_alignment,
        &buffer.handle, &buffer.allocation, nullptr));
    vk_set_debug_name(buffer.handle, name);

    VkBufferDeviceAddressInfo buffer_address_info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    buffer_address_info.buffer = buffer.handle;
    buffer.device_address = vkGetBufferDeviceAddress(vk.device, &buffer_address_info);

    if (data != nullptr) {
        vk_ensure_staging_buffer_allocation(size);
        memcpy(vk.staging_buffer_ptr, data, size);
        vk_execute(vk.command_pools[0], vk.queue, [size, &buffer](VkCommandBuffer command_buffer) {
            VkBufferCopy region{};
            region.size = size;
            vkCmdCopyBuffer(command_buffer, vk.staging_buffer, buffer.handle, 1, &region);
        });
    }
    return buffer;
}

Vk_Buffer vk_create_mapped_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** buffer_ptr, const char* name) {
    VkBufferCreateInfo buffer_create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_create_info.size = size;
    buffer_create_info.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo alloc_create_info{};
    alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_create_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // to avoid manual flush/invalidation

    VmaAllocationInfo alloc_info;
    Vk_Buffer buffer;
    VK_CHECK(vmaCreateBuffer(vk.allocator, &buffer_create_info, &alloc_create_info, &buffer.handle, &buffer.allocation, &alloc_info));
    vk_set_debug_name(buffer.handle, name);

    VkBufferDeviceAddressInfo buffer_address_info { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    buffer_address_info.buffer = buffer.handle;
    buffer.device_address = vkGetBufferDeviceAddress(vk.device, &buffer_address_info);

    if (buffer_ptr)
        *buffer_ptr = alloc_info.pMappedData;
    return buffer;
}

Vk_Image vk_create_texture(int width, int height, VkFormat format, bool generate_mipmaps, const uint8_t* pixels, int bytes_per_pixel, const char* name) {
    Vk_Image image;

    uint32_t mip_levels = 1;
    if (generate_mipmaps) {
        mip_levels = 0;
        for (int k = std::max(width, height); k > 0; k >>= 1)
            mip_levels++;
    }

    // create image
    {
        VkImageCreateInfo image_create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        image_create_info.imageType      = VK_IMAGE_TYPE_2D;
        image_create_info.format         = format;
        image_create_info.extent.width   = width;
        image_create_info.extent.height  = height;
        image_create_info.extent.depth   = 1;
        image_create_info.mipLevels      = mip_levels;
        image_create_info.arrayLayers    = 1;
        image_create_info.samples        = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling         = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage          = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (generate_mipmaps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
        image_create_info.sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;

        VK_CHECK(vmaCreateImage(vk.allocator, &image_create_info, &alloc_create_info, &image.handle, &image.allocation, nullptr));
        vk_set_debug_name(image.handle, name);
    }

    // create image view
    {
        VkImageSubresourceRange subresource_range;
        subresource_range.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel      = 0;
        subresource_range.levelCount        = VK_REMAINING_MIP_LEVELS;
        subresource_range.baseArrayLayer    = 0;
        subresource_range.layerCount        = 1;

        VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        create_info.image               = image.handle;
        create_info.viewType            = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format              = format;
        create_info.subresourceRange    = subresource_range;

        VK_CHECK(vkCreateImageView(vk.device, &create_info, nullptr, &image.view));
        vk_set_debug_name(image.view, (name + std::string(" (ImageView)")).c_str());
    }

    // upload image data
    {
        int buffer_size = width * height * bytes_per_pixel;
        vk_ensure_staging_buffer_allocation(buffer_size);
        memcpy(vk.staging_buffer_ptr, pixels, buffer_size);

        VkBufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = VkOffset3D{ 0, 0, 0 };
        region.imageExtent = VkExtent3D{ (uint32_t)width, (uint32_t)height, 1 };

        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.levelCount = 1;
        subresource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vk_execute(vk.command_pools[0], vk.queue,
            [&image, &region, &subresource_range, width, height, mip_levels](VkCommandBuffer command_buffer) {

            subresource_range.baseMipLevel = 0;

            vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            vkCmdCopyBufferToImage(command_buffer, vk.staging_buffer, image.handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            if (mip_levels == 1) {
                vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                return;
            }

            VkImageBlit blit{};
            blit.srcSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.baseArrayLayer  = 0;
            blit.srcSubresource.layerCount      = 1;
            blit.dstSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.baseArrayLayer  = 0;
            blit.dstSubresource.layerCount      = 1;

            int32_t w = (int32_t)width;
            int32_t h = (int32_t)height;

            for (uint32_t i = 1; i < mip_levels; i++) {
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcOffsets[1] = VkOffset3D { w, h, 1 };

                w = std::max(w >> 1, 1);
                h = std::max(h >> 1, 1);

                blit.dstSubresource.mipLevel = i;
                blit.dstOffsets[1] = VkOffset3D { w, h, 1 };

                subresource_range.baseMipLevel = i-1;
                vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                subresource_range.baseMipLevel = i;
                vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                vkCmdBlitImage(command_buffer,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit, VK_FILTER_LINEAR);

                subresource_range.baseMipLevel = i-1;
                vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            subresource_range.baseMipLevel = mip_levels - 1;
            vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
    }

    return image;
}

Vk_Image vk_load_texture(const std::string& texture_file) {
    int w, h;
    int component_count;

    auto rgba_pixels = stbi_load(texture_file.c_str(), &w, &h, &component_count,STBI_rgb_alpha);
    if (rgba_pixels == nullptr)
        error("failed to load image file: " + texture_file);

    Vk_Image texture = vk_create_texture(w, h, VK_FORMAT_R8G8B8A8_SRGB, true, rgba_pixels, 4, texture_file.c_str());
    stbi_image_free(rgba_pixels);
    return texture;
}

VkShaderModule vk_load_spirv(const std::string& spirv_file) {
    std::vector<uint8_t> bytes = read_binary_file(spirv_file);

    if (bytes.size() % 4 != 0) {
        error("Vulkan: SPIR-V binary buffer size is not multiple of 4");
    }

    VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO } ;
    create_info.codeSize = bytes.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(vk.device, &create_info, nullptr, &shader_module));
    return shader_module;
}

Vk_Image vk_create_image(int width, int height, VkFormat format, VkImageUsageFlags usage_flags, const char* name) {
    Vk_Image image;

    // create image
    {
        VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        create_info.imageType      = VK_IMAGE_TYPE_2D;
        create_info.format         = format;
        create_info.extent.width   = width;
        create_info.extent.height  = height;
        create_info.extent.depth   = 1;
        create_info.mipLevels      = 1;
        create_info.arrayLayers    = 1;
        create_info.samples        = VK_SAMPLE_COUNT_1_BIT;
        create_info.tiling         = VK_IMAGE_TILING_OPTIMAL;
        create_info.usage          = usage_flags;
        create_info.sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
        create_info.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;

        VK_CHECK(vmaCreateImage(vk.allocator, &create_info, &alloc_create_info, &image.handle, &image.allocation, nullptr));
        vk_set_debug_name(image.handle, name);
    }
    // create image view
    {
        VkImageAspectFlagBits aspect_flags = (usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        create_info.image                           = image.handle;
        create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format                          = format;
        create_info.subresourceRange.aspectMask     = aspect_flags;
        create_info.subresourceRange.baseMipLevel   = 0;
        create_info.subresourceRange.levelCount     = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount     = 1;

        VK_CHECK(vkCreateImageView(vk.device, &create_info, nullptr, &image.view));
        vk_set_debug_name(image.view, (name + std::string(" (ImageView)")).c_str());
    }
    return image;
}

Vk_Graphics_Pipeline_State get_default_graphics_pipeline_state() {
    Vk_Graphics_Pipeline_State state;

    // VkVertexInputBindingDescription
    state.vertex_binding_count = 0;

    // VkVertexInputAttributeDescription
    state.vertex_attribute_count = 0;

    // VkPipelineInputAssemblyStateCreateInfo
    auto& input_assembly_state = state.input_assembly_state;
    input_assembly_state = VkPipelineInputAssemblyStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_assembly_state.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    // VkPipelineViewportStateCreateInfo
    auto& viewport_state = state.viewport_state;
    viewport_state = VkPipelineViewportStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount                = 1;
    viewport_state.pViewports                   = nullptr;
    viewport_state.scissorCount                 = 1;
    viewport_state.pScissors                    = nullptr;

    // VkPipelineRasterizationStateCreateInfo
    auto& rasterization_state = state.rasterization_state;
    rasterization_state = VkPipelineRasterizationStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterization_state.depthClampEnable        = VK_FALSE;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterization_state.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterization_state.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.depthBiasEnable         = VK_FALSE;
    rasterization_state.depthBiasConstantFactor = 0.0f;
    rasterization_state.depthBiasClamp          = 0.0f;
    rasterization_state.depthBiasSlopeFactor    = 0.0f;
    rasterization_state.lineWidth               = 1.0f;

    // VkPipelineMultisampleStateCreateInfo
    auto& multisample_state = state.multisample_state;
    multisample_state = VkPipelineMultisampleStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisample_state.rasterizationSamples      = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable       = VK_FALSE;
    multisample_state.minSampleShading          = 1.0f;
    multisample_state.pSampleMask               = nullptr;
    multisample_state.alphaToCoverageEnable     = VK_FALSE;
    multisample_state.alphaToOneEnable          = VK_FALSE;

    // VkPipelineDepthStencilStateCreateInfo
    auto& depth_stencil_state = state.depth_stencil_state;
    depth_stencil_state = VkPipelineDepthStencilStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depth_stencil_state.depthTestEnable         = VK_TRUE;
    depth_stencil_state.depthWriteEnable        = VK_TRUE;
    depth_stencil_state.depthCompareOp          = VK_COMPARE_OP_LESS;
    depth_stencil_state.depthBoundsTestEnable   = VK_FALSE;
    depth_stencil_state.stencilTestEnable       = VK_FALSE;

    // VkPipelineColorBlendAttachmentState
    auto& attachment_blend_state = state.attachment_blend_state[0];
    attachment_blend_state = VkPipelineColorBlendAttachmentState{};
    attachment_blend_state.blendEnable          = VK_FALSE;
    attachment_blend_state.colorWriteMask       = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    state.attachment_blend_state_count          = 1;

    // VkPipelineDynamicStateCreateInfo
    state.dynamic_state[0]                      = VK_DYNAMIC_STATE_VIEWPORT;
    state.dynamic_state[1]                      = VK_DYNAMIC_STATE_SCISSOR;
    state.dynamic_state_count                   = 2;

    // VkPipelineRenderingCreateInfo
    state.color_attachment_formats[0]           = VK_FORMAT_UNDEFINED;
    state.color_attachment_count                = 0;
    state.depth_attachment_format               = VK_FORMAT_UNDEFINED;

    return state;
}

VkPipeline vk_create_graphics_pipeline(
    const Vk_Graphics_Pipeline_State&   state,
    VkPipelineLayout                    pipeline_layout,
    VkShaderModule                      vertex_shader,
    VkShaderModule                      fragment_shader)
{
    auto get_shader_stage_create_info = [](VkShaderStageFlagBits stage, VkShaderModule shader_module) {
        VkPipelineShaderStageCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        create_info.stage  = stage;
        create_info.module = shader_module;
        create_info.pName  = "main";
        return create_info;
    };

    VkPipelineShaderStageCreateInfo shader_stages_state[2] {
        get_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader),
        get_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader)
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_state{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input_state.vertexBindingDescriptionCount    = state.vertex_binding_count;
    vertex_input_state.pVertexBindingDescriptions       = state.vertex_bindings;
    vertex_input_state.vertexAttributeDescriptionCount  = state.vertex_attribute_count;
    vertex_input_state.pVertexAttributeDescriptions     = state.vertex_attributes;

    VkPipelineColorBlendStateCreateInfo blend_state{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blend_state.logicOpEnable                           = VK_FALSE;
    blend_state.logicOp                                 = VK_LOGIC_OP_COPY;
    blend_state.attachmentCount                         = state.attachment_blend_state_count;
    blend_state.pAttachments                            = state.attachment_blend_state;

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state_create_info.dynamicStateCount         = state.dynamic_state_count;
    dynamic_state_create_info.pDynamicStates            = state.dynamic_state;

    VkPipelineRenderingCreateInfo rendering_create_info{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rendering_create_info.colorAttachmentCount          = state.color_attachment_count;
    rendering_create_info.pColorAttachmentFormats       = state.color_attachment_formats;
    rendering_create_info.depthAttachmentFormat         = state.depth_attachment_format;

    VkGraphicsPipelineCreateInfo create_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    create_info.pNext                                   = &rendering_create_info;
    create_info.stageCount                              = (uint32_t)std::size(shader_stages_state);
    create_info.pStages                                 = shader_stages_state;
    create_info.pVertexInputState                       = &vertex_input_state;
    create_info.pInputAssemblyState                     = &state.input_assembly_state;
    create_info.pViewportState                          = &state.viewport_state;
    create_info.pRasterizationState                     = &state.rasterization_state;
    create_info.pMultisampleState                       = &state.multisample_state;
    create_info.pDepthStencilState                      = &state.depth_stencil_state;
    create_info.pColorBlendState                        = &blend_state;
    create_info.pDynamicState                           = &dynamic_state_create_info;
    create_info.layout                                  = pipeline_layout;
    create_info.subpass                                 = 0;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));
    return pipeline;
}

void vk_begin_frame() {
    VK_CHECK(vkWaitForFences(vk.device, 1, &vk.frame_fence[vk.frame_index], VK_FALSE, std::numeric_limits<uint64_t>::max()));
    VK_CHECK(vkResetFences(vk.device, 1, &vk.frame_fence[vk.frame_index]));
    vkResetCommandPool(vk.device, vk.command_pools[vk.frame_index], 0);
    vk.command_buffer = vk.command_buffers[vk.frame_index];
    vk.timestamp_query_pool = vk.timestamp_query_pools[vk.frame_index];

    START_TIMER
    VK_CHECK(vkAcquireNextImageKHR(vk.device, vk.swapchain_info.handle, UINT64_MAX, vk.image_acquired_semaphore[vk.frame_index], VK_NULL_HANDLE, &vk.swapchain_image_index));
    STOP_TIMER("vkAcquireNextImageKHR")

    VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(vk.command_buffer, &begin_info));
}

void vk_end_frame() {
    VK_CHECK(vkEndCommandBuffer(vk.command_buffer));

    const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.waitSemaphoreCount   = 1;
    submit_info.pWaitSemaphores      = &vk.image_acquired_semaphore[vk.frame_index];
    submit_info.pWaitDstStageMask    = &wait_dst_stage_mask;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &vk.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores    = &vk.rendering_finished_semaphore[vk.frame_index];

    START_TIMER
    VK_CHECK(vkQueueSubmit(vk.queue, 1, &submit_info, vk.frame_fence[vk.frame_index]));
    STOP_TIMER("vkQueueSubmit")

    VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &vk.rendering_finished_semaphore[vk.frame_index];
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &vk.swapchain_info.handle;
    present_info.pImageIndices      = &vk.swapchain_image_index;

    START_TIMER
    VK_CHECK(vkQueuePresentKHR(vk.queue, &present_info));
    STOP_TIMER("vkQueuePresentKHR")

    vk.frame_index = 1 - vk.frame_index;
}

void vk_execute(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder) {

    VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc_info.commandPool          = command_pool;
    alloc_info.level                = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount   = 1;

    VkCommandBuffer command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
    recorder(command_buffer);
    VK_CHECK(vkEndCommandBuffer(command_buffer));

    VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount  = 1;
    submit_info.pCommandBuffers     = &command_buffer;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
    vkFreeCommandBuffers(vk.device, command_pool, 1, &command_buffer);
}

void vk_cmd_image_barrier(VkCommandBuffer command_buffer, VkImage image,
    VkPipelineStageFlags src_stage_mask, VkAccessFlags src_access_mask, VkImageLayout old_layout,
    VkPipelineStageFlags dst_stage_mask, VkAccessFlags dst_access_mask, VkImageLayout new_layout)
{
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0,
        0, nullptr, 0, nullptr, 1, &barrier);
}

void vk_cmd_image_barrier_for_subresource(
    VkCommandBuffer command_buffer, VkImage image, const VkImageSubresourceRange& subresource_range,
    VkPipelineStageFlags src_stage_mask, VkAccessFlags src_access_mask, VkImageLayout old_layout,
    VkPipelineStageFlags dst_stage_mask, VkAccessFlags dst_access_mask, VkImageLayout new_layout)
{
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcAccessMask = src_access_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0,
        0, nullptr, 0, nullptr, 1, &barrier);
}

uint32_t vk_allocate_timestamp_queries(uint32_t count) {
    assert(count > 0);
    assert(vk.timestamp_query_count + count <= max_timestamp_queries);
    uint32_t first_query = vk.timestamp_query_count;
    vk.timestamp_query_count += count;
    return first_query;
}

void set_debug_name_impl(VkObjectType object_type, uint64_t object_handle, const char* name) {
    if (name) {
        VkDebugUtilsObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        name_info.objectType = object_type;
        name_info.objectHandle = object_handle;
        name_info.pObjectName = name;
        VK_CHECK(vkSetDebugUtilsObjectNameEXT(vk.device, &name_info));
    }
}
