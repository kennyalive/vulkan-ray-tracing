#define VMA_IMPLEMENTATION
#include "vk.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "glfw/glfw3.h"

#include "vulkan/vk_enum_string_helper.h"
const char* vk_result_to_string(VkResult result) { return string_VkResult(result); }

#include <format>
#include <fstream>

constexpr uint32_t max_timestamp_queries = 64;

//
// Vk_Instance is a container that stores common Vulkan resources like vulkan instance,
// device, command pool, swapchain, etc.
//
Vk_Instance vk;

static void create_instance(const std::span<const char*>& instance_extensions)
{
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
        if (!supported) {
            vk.error("Required instance extension is not available: " + std::string(name));
        }
    }

    VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_create_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = uint32_t(instance_extensions.size());
    instance_create_info.ppEnabledExtensionNames = instance_extensions.data();

    VK_CHECK(vkCreateInstance(&instance_create_info, nullptr, &vk.instance));
}

static void create_device(const Vk_Init_Params& params, GLFWwindow* window)
{
    // select physical device
    {
        uint32_t gpu_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &gpu_count, nullptr));
        if (gpu_count == 0) {
            vk.error("There are no Vulkan physical devices available");
        }
        if (params.physical_device_index >= (int)gpu_count) {
            vk.error(std::format("Requested physical device index (zero-based) is {}, but physical device count is {}",
                params.physical_device_index, gpu_count));
        }
        std::vector<VkPhysicalDevice> physical_devices(gpu_count);
        VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &gpu_count, physical_devices.data()));

        int selected_gpu = -1;
        VkPhysicalDeviceProperties gpu_properties{};

        if (params.physical_device_index != -1) {
            vkGetPhysicalDeviceProperties(physical_devices[params.physical_device_index], &gpu_properties);
            if (VK_VERSION_MAJOR(gpu_properties.apiVersion) != 1 || VK_VERSION_MINOR(gpu_properties.apiVersion) < 3) {
                vk.error(std::format("Physical device %d reports unsupported Vulkan version: {}.{}. Vulkan 1.3 or higher is required",
                    params.physical_device_index, VK_VERSION_MAJOR(gpu_properties.apiVersion), VK_VERSION_MINOR(gpu_properties.apiVersion)));
            }
            selected_gpu = params.physical_device_index;
        }
        else {
            for (const auto& physical_device : physical_devices) {
                vkGetPhysicalDeviceProperties(physical_device, &gpu_properties);
                if (VK_VERSION_MAJOR(gpu_properties.apiVersion) == 1 && VK_VERSION_MINOR(gpu_properties.apiVersion) >= 3) {
                    selected_gpu = int(&physical_device - physical_devices.data());
                    break;
                }
            }
        }
        if (selected_gpu == -1) {
            vk.error("Failed to select physical device that supports Vulkan 1.3 or higher");
        }
        vk.physical_device = physical_devices[selected_gpu];
        vk.timestamp_period_ms = (double)gpu_properties.limits.timestampPeriod * 1e-6;
    }

    VK_CHECK(glfwCreateWindowSurface(vk.instance, window, nullptr, &vk.surface));
    vk.surface_usage_flags = params.surface_usage_flags;

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
        if (vk.queue_family_index == uint32_t(-1)) {
            vk.error("Vulkan: failed to find queue family");
        }
    }

    // create VkDevice
    {
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
        for (auto required_extension : params.device_extensions) {
            if (!is_extension_supported(required_extension)) {
                vk.error("Vulkan: required device extension is not available: " + std::string(required_extension));
            }
        }

        const float priority = 1.0;
        VkDeviceQueueCreateInfo queue_create_info { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queue_create_info.queueFamilyIndex = vk.queue_family_index;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &priority;

        VkDeviceCreateInfo device_create_info { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        device_create_info.pNext = params.device_create_info_pnext;
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.enabledExtensionCount = (uint32_t)params.device_extensions.size();
        device_create_info.ppEnabledExtensionNames = params.device_extensions.data();

        VK_CHECK(vkCreateDevice(vk.physical_device, &device_create_info, nullptr, &vk.device));
    }
}

void Vk_Image::destroy()
{
    vmaDestroyImage(vk.allocator, handle, allocation);
    vkDestroyImageView(vk.device, view, nullptr);
    *this = Vk_Image{};
}

void Vk_Buffer::destroy()
{
    vmaDestroyBuffer(vk.allocator, handle, allocation);
    *this = Vk_Buffer{};
}

void vk_initialize(GLFWwindow* window, const Vk_Init_Params& init_params)
{
    vk.error = init_params.error_reporter;
    VK_CHECK(volkInitialize());
    uint32_t instance_version = volkGetInstanceVersion();

    // Require version 1.1 or higher of instance-level functionality.
    // "As long as the instance supports at least Vulkan 1.1, an application can use different 
    // versions of Vulkan with an instance than it does with a device or physical device."
    bool instance_version_higher_than_or_equal_to_1_1 =
        VK_VERSION_MAJOR(instance_version) > 1 || VK_VERSION_MINOR(instance_version) >= 1;
    if (!instance_version_higher_than_or_equal_to_1_1) {
        vk.error("The supported instance version is Vulkan 1.1 or higher, but Vulkan 1.0 loader is detected");
    }
    create_instance(init_params.instance_extensions);
    volkLoadInstance(vk.instance);
    create_device(init_params, window);
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

    // Imgui descriptor pool.
    {
        const VkDescriptorPoolSize pool_size_info = {
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1
        };
        VkDescriptorPoolCreateInfo desc{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        desc.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        desc.maxSets = 1;
        desc.poolSizeCount = 1;
        desc.pPoolSizes = &pool_size_info;
        VK_CHECK(vkCreateDescriptorPool(vk.device, &desc, nullptr, &vk.imgui_descriptor_pool));
        vk_set_debug_name(vk.imgui_descriptor_pool, "imgui_descriptor_pool");
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

        [&candidates, &init_params]() {
            for (VkFormat format : init_params.supported_surface_formats) {
                for (VkSurfaceFormatKHR surface_format : candidates) {
                    if (surface_format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                        continue;
                    if (surface_format.format == format) {
                        vk.surface_format = surface_format;
                        return;
                    }
                }
            }
            vk.error("Failed to find supported surface format");
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

void vk_shutdown()
{
    vkDeviceWaitIdle(vk.device);

    if (vk.staging_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vk.allocator, vk.staging_buffer, vk.staging_buffer_allocation);
    }

    vkDestroyCommandPool(vk.device, vk.command_pools[0], nullptr);
    vkDestroyCommandPool(vk.device, vk.command_pools[1], nullptr);
    vkDestroySemaphore(vk.device, vk.image_acquired_semaphore[0], nullptr);
    vkDestroySemaphore(vk.device, vk.image_acquired_semaphore[1], nullptr);
    vkDestroySemaphore(vk.device, vk.rendering_finished_semaphore[0], nullptr);
    vkDestroySemaphore(vk.device, vk.rendering_finished_semaphore[1], nullptr);
    vkDestroyFence(vk.device, vk.frame_fence[0], nullptr);
    vkDestroyFence(vk.device, vk.frame_fence[1], nullptr);
    vkDestroyQueryPool(vk.device, vk.timestamp_query_pools[0], nullptr);
    vkDestroyQueryPool(vk.device, vk.timestamp_query_pools[1], nullptr);
    vkDestroyDescriptorPool(vk.device, vk.imgui_descriptor_pool, nullptr);
    vk_destroy_swapchain();
    vmaDestroyAllocator(vk.allocator);
    vkDestroyDevice(vk.device, nullptr);
    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.debug_utils_messenger, nullptr);
    vkDestroyInstance(vk.instance, nullptr);
}

void vk_create_swapchain(bool vsync)
{
    assert(vk.swapchain_info.handle == VK_NULL_HANDLE);

    VkSurfaceCapabilitiesKHR surface_caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical_device, vk.surface, &surface_caps));

    vk.surface_size = surface_caps.currentExtent;

    // don't expect special value described in spec on Win32
    assert(vk.surface_size.width != 0xffffffff && vk.surface_size.height != 0xffffffff);

    // we should not try to create a swapchain when window is minimized
    assert(vk.surface_size.width != 0 && vk.surface_size.height != 0);

    // VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
    if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
        vk.error("vk_create_swapchain: VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain");
    }

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
    desc.imageUsage         = vk.surface_usage_flags;
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

void vk_destroy_swapchain()
{
    for (auto image_view : vk.swapchain_info.image_views) {
        vkDestroyImageView(vk.device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(vk.device, vk.swapchain_info.handle, nullptr);
    vk.swapchain_info = Swapchain_Info{};
}

void vk_ensure_staging_buffer_allocation(VkDeviceSize size)
{
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

Vk_Buffer vk_create_mapped_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** buffer_ptr, const char* name)
{
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

Vk_Image vk_create_image(int width, int height, VkFormat format, VkImageUsageFlags usage_flags, const char* name)
{
    Vk_Image image;
    // create image
    {
        VkImageCreateInfo create_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        create_info.imageType = VK_IMAGE_TYPE_2D;
        create_info.format = format;
        create_info.extent.width = width;
        create_info.extent.height = height;
        create_info.extent.depth = 1;
        create_info.mipLevels = 1;
        create_info.arrayLayers = 1;
        create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        create_info.usage = usage_flags;
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

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

        VkImageViewCreateInfo create_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        create_info.image = image.handle;
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = format;
        create_info.subresourceRange.aspectMask = aspect_flags;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(vk.device, &create_info, nullptr, &image.view));
        vk_set_debug_name(image.view, (name + std::string(" (ImageView)")).c_str());
    }
    return image;
}

Vk_Image vk_create_texture(int width, int height, VkFormat format, bool generate_mipmaps, const uint8_t* pixels, int bytes_per_pixel, const char* name)
{
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
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            vkCmdCopyBufferToImage(command_buffer, vk.staging_buffer, image.handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            if (mip_levels == 1) {
                vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                subresource_range.baseMipLevel = i;
                vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                    VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                vkCmdBlitImage(command_buffer,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit, VK_FILTER_LINEAR);

                subresource_range.baseMipLevel = i-1;
                vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            subresource_range.baseMipLevel = mip_levels - 1;
            vk_cmd_image_barrier_for_subresource(command_buffer, image.handle, subresource_range,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
    }

    return image;
}

Vk_Image vk_load_texture(const std::string& texture_file)
{
    int w, h;
    int component_count;

    auto rgba_pixels = stbi_load(texture_file.c_str(), &w, &h, &component_count,STBI_rgb_alpha);
    if (rgba_pixels == nullptr) {
        vk.error("failed to load image file: " + texture_file);
    }

    Vk_Image texture = vk_create_texture(w, h, VK_FORMAT_R8G8B8A8_SRGB, true, rgba_pixels, 4, texture_file.c_str());
    stbi_image_free(rgba_pixels);
    return texture;
}

static std::vector<uint8_t> read_binary_file(const std::string& file_name)
{
    std::ifstream file(file_name, std::ios_base::in | std::ios_base::binary);
    if (!file) {
        vk.error("failed to open file: " + file_name);
    }
    // get file size
    file.seekg(0, std::ios_base::end);
    std::streampos file_size = file.tellg();
    file.seekg(0, std::ios_base::beg);

    if (file_size == std::streampos(-1) || !file) {
        vk.error("failed to read file stats: " + file_name);
    }
    // read file content
    std::vector<uint8_t> file_content(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(file_content.data()), file_size);
    if (!file) {
        vk.error("failed to read file content: " + file_name);
    }
    return file_content;
}

VkShaderModule vk_load_spirv(const std::string& spirv_file)
{
    std::vector<uint8_t> bytes = read_binary_file(spirv_file);

    if (bytes.size() % 4 != 0) {
        vk.error("Vulkan: SPIR-V binary buffer size is not multiple of 4");
    }

    VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO } ;
    create_info.codeSize = bytes.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(vk.device, &create_info, nullptr, &shader_module));
    return shader_module;
}

VkPipelineLayout vk_create_pipeline_layout(std::initializer_list<VkDescriptorSetLayout> set_layouts,
    std::initializer_list<VkPushConstantRange> push_constant_ranges, const char* name)
{
    VkPipelineLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    create_info.setLayoutCount = (uint32_t)set_layouts.size();
    create_info.pSetLayouts = set_layouts.begin();
    create_info.pushConstantRangeCount = (uint32_t)push_constant_ranges.size();
    create_info.pPushConstantRanges = push_constant_ranges.begin();

    VkPipelineLayout pipeline_layout{};
    VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &pipeline_layout));
    vk_set_debug_name(pipeline_layout, name);
    return pipeline_layout;
}

Vk_Graphics_Pipeline_State get_default_graphics_pipeline_state()
{
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

VkPipeline vk_create_graphics_pipeline(const Vk_Graphics_Pipeline_State& state,
    VkShaderModule vertex_shader, VkShaderModule fragment_shader,
    VkPipelineLayout pipeline_layout, const char* name)
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
    create_info.flags                                   = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
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

    VkPipeline pipeline{};
    VK_CHECK(vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));
    vk_set_debug_name(pipeline, name);
    return pipeline;
}

VkPipeline vk_create_compute_pipeline(VkShaderModule compute_shader, VkPipelineLayout pipeline_layout, const char* name)
{
    VkPipelineShaderStageCreateInfo compute_stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    compute_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_stage.module = compute_shader;
    compute_stage.pName = "main";

    VkComputePipelineCreateInfo create_info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    create_info.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    create_info.stage = compute_stage;
    create_info.layout = pipeline_layout;

    VkPipeline pipeline{};
    VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));
    vk_set_debug_name(pipeline, name);
    return pipeline;
}

void vk_begin_frame()
{
    VK_CHECK(vkWaitForFences(vk.device, 1, &vk.frame_fence[vk.frame_index], VK_FALSE, std::numeric_limits<uint64_t>::max()));
    VK_CHECK(vkResetFences(vk.device, 1, &vk.frame_fence[vk.frame_index]));
    vkResetCommandPool(vk.device, vk.command_pools[vk.frame_index], 0);
    vk.command_buffer = vk.command_buffers[vk.frame_index];
    vk.timestamp_query_pool = vk.timestamp_query_pools[vk.frame_index];

    VK_CHECK(vkAcquireNextImageKHR(vk.device, vk.swapchain_info.handle, UINT64_MAX, vk.image_acquired_semaphore[vk.frame_index], VK_NULL_HANDLE, &vk.swapchain_image_index));

    VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(vk.command_buffer, &begin_info));
}

void vk_end_frame()
{
    VK_CHECK(vkEndCommandBuffer(vk.command_buffer));

    VkSemaphoreSubmitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    wait_info.semaphore = vk.image_acquired_semaphore[vk.frame_index];
    wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBufferSubmitInfo cmd_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cmd_info.commandBuffer = vk.command_buffer;

    VkSemaphoreSubmitInfo signal_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    signal_info.semaphore = vk.rendering_finished_semaphore[vk.frame_index];
    signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submit_info.waitSemaphoreInfoCount = 1;
    submit_info.pWaitSemaphoreInfos = &wait_info;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &cmd_info;
    submit_info.signalSemaphoreInfoCount = 1;
    submit_info.pSignalSemaphoreInfos = &signal_info;

    VK_CHECK(vkQueueSubmit2(vk.queue, 1, &submit_info, vk.frame_fence[vk.frame_index]));

    VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &vk.rendering_finished_semaphore[vk.frame_index];
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &vk.swapchain_info.handle;
    present_info.pImageIndices      = &vk.swapchain_image_index;

    VK_CHECK(vkQueuePresentKHR(vk.queue, &present_info));

    vk.frame_index = 1 - vk.frame_index;
}

void vk_execute(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder)
{
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

    VkCommandBufferSubmitInfo cmd_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cmd_info.commandBuffer = command_buffer;

    VkSubmitInfo2 submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &cmd_info;

    VK_CHECK(vkQueueSubmit2(queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
    vkFreeCommandBuffers(vk.device, command_pool, 1, &command_buffer);
}

void vk_cmd_image_barrier(VkCommandBuffer command_buffer, VkImage image,
    VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkImageLayout old_layout,
    VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkImageLayout new_layout)
{
    VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dep_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(command_buffer, &dep_info);
}

void vk_cmd_image_barrier_for_subresource(
    VkCommandBuffer command_buffer, VkImage image, const VkImageSubresourceRange& subresource_range,
    VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkImageLayout old_layout,
    VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkImageLayout new_layout)
{
    VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.image = image;
    barrier.subresourceRange = subresource_range;

    VkDependencyInfo dep_info{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(command_buffer, &dep_info);
}

uint32_t vk_allocate_timestamp_queries(uint32_t count)
{
    assert(count > 0);
    assert(vk.timestamp_query_count + count <= max_timestamp_queries);
    uint32_t first_query = vk.timestamp_query_count;
    vk.timestamp_query_count += count;
    return first_query;
}

void set_debug_name_impl(VkObjectType object_type, uint64_t object_handle, const char* name)
{
    if (name) {
        VkDebugUtilsObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        name_info.objectType = object_type;
        name_info.objectHandle = object_handle;
        name_info.pObjectName = name;
        VK_CHECK(vkSetDebugUtilsObjectNameEXT(vk.device, &name_info));
    }
}

//*****************************************************************************
//
// Misc Vulkan utilities section
//
//*****************************************************************************
Vk_Shader_Module::Vk_Shader_Module(const std::string& spirv_file)
{
    handle = vk_load_spirv(spirv_file);
}

Vk_Shader_Module::~Vk_Shader_Module()
{
    vkDestroyShaderModule(vk.device, handle, nullptr);
}

static VkDescriptorSetLayoutBinding get_set_layout_binding(uint32_t binding, uint32_t count,
    VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags)
{
    VkDescriptorSetLayoutBinding entry{};
    entry.binding = binding;
    entry.descriptorType = descriptor_type;
    entry.descriptorCount = count;
    entry.stageFlags = stage_flags;
    return entry;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::sampled_image(uint32_t binding, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage_flags);
    return *this;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::sampled_image_array(uint32_t binding, uint32_t array_size, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, array_size, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage_flags);
    return *this;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::storage_image(uint32_t binding, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage_flags);
    return *this;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::sampler(uint32_t binding, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, 1, VK_DESCRIPTOR_TYPE_SAMPLER, stage_flags);
    return *this;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::uniform_buffer(uint32_t binding, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage_flags);
    return *this;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::storage_buffer(uint32_t binding, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage_flags);
    return *this;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::storage_buffer_array(uint32_t binding, uint32_t array_size, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, array_size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage_flags);
    return *this;
}

Vk_Descriptor_Set_Layout& Vk_Descriptor_Set_Layout::accelerator(uint32_t binding, VkShaderStageFlags stage_flags)
{
    assert(binding_count < max_bindings);
    bindings[binding_count++] = get_set_layout_binding(binding, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stage_flags);
    return *this;
}

VkDescriptorSetLayout Vk_Descriptor_Set_Layout::create(const char* name)
{
    VkDescriptorSetLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    create_info.bindingCount = binding_count;
    create_info.pBindings = bindings;

    VkDescriptorSetLayout set_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &create_info, nullptr, &set_layout));
    vk_set_debug_name(set_layout, name);
    return set_layout;
}

void Vk_GPU_Time_Interval::begin()
{
    vkCmdWriteTimestamp(vk.command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pool, start_query[vk.frame_index]);
}

void Vk_GPU_Time_Interval::end()
{
    vkCmdWriteTimestamp(vk.command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pool, start_query[vk.frame_index] + 1);
}

Vk_GPU_Time_Interval* Vk_GPU_Time_Keeper::allocate_time_interval()
{
    assert(time_interval_count < max_time_intervals);
    Vk_GPU_Time_Interval* time_interval = &time_intervals[time_interval_count++];

    time_interval->start_query[0] = time_interval->start_query[1] = vk_allocate_timestamp_queries(2);
    time_interval->length_ms = 0.f;
    return time_interval;
}

void Vk_GPU_Time_Keeper::initialize_time_intervals()
{
    vk_execute(vk.command_pools[0], vk.queue, [this](VkCommandBuffer command_buffer) {
        vkCmdResetQueryPool(command_buffer, vk.timestamp_query_pools[0], 0, 2 * time_interval_count);
        vkCmdResetQueryPool(command_buffer, vk.timestamp_query_pools[1], 0, 2 * time_interval_count);
        for (uint32_t i = 0; i < time_interval_count; i++) {
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pools[0], time_intervals[i].start_query[0]);
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pools[0], time_intervals[i].start_query[0] + 1);
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pools[1], time_intervals[i].start_query[1]);
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk.timestamp_query_pools[1], time_intervals[i].start_query[1] + 1);
        }
        });
}

void Vk_GPU_Time_Keeper::next_frame()
{
    uint64_t query_results[2/*query_result + availability*/ * 2/*start + end*/ * max_time_intervals];
    const uint32_t query_count = 2 * time_interval_count;
    VkResult result = vkGetQueryPoolResults(vk.device, vk.timestamp_query_pool, 0, query_count,
        query_count * 2 * sizeof(uint64_t), query_results, 2 * sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    VK_CHECK_RESULT(result);
    assert(result != VK_NOT_READY);

    const float influence = 0.25f;

    for (uint32_t i = 0; i < time_interval_count; i++) {
        assert(query_results[4 * i + 2] >= query_results[4 * i]);
        time_intervals[i].length_ms = (1.f - influence) * time_intervals[i].length_ms + influence * float(double(query_results[4 * i + 2] - query_results[4 * i]) * vk.timestamp_period_ms);
    }

    vkCmdResetQueryPool(vk.command_buffer, vk.timestamp_query_pool, 0, query_count);
}

void vk_begin_gpu_marker_scope(VkCommandBuffer command_buffer, const char* name)
{
    VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = name;
    vkCmdBeginDebugUtilsLabelEXT(command_buffer, &label);
}

void vk_end_gpu_marker_scope(VkCommandBuffer command_buffer)
{
    vkCmdEndDebugUtilsLabelEXT(command_buffer);
}

void vk_write_gpu_marker(VkCommandBuffer command_buffer, const char* name)
{
    VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = name;
    vkCmdInsertDebugUtilsLabelEXT(command_buffer, &label);
}
