#include "vk.h"
#include "common.h"
#include "geometry.h"
#include "resource_manager.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <vector>

Vk_Instance vk;

static VkSwapchainKHR create_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format) {
    VkSurfaceCapabilitiesKHR surface_caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));

    VkExtent2D image_extent = surface_caps.currentExtent;
    if (image_extent.width == 0xffffffff && image_extent.height == 0xffffffff) {
        image_extent.width = std::min(surface_caps.maxImageExtent.width, std::max(surface_caps.minImageExtent.width, 640u));
        image_extent.height = std::min(surface_caps.maxImageExtent.height, std::max(surface_caps.minImageExtent.height, 480u));
    }

    // VK_IMAGE_USAGE_TRANSFER_DST_BIT is required by image clear operations.
    if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
        error("create_swapchain: VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain");

    // VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required in order to take screenshots.
    if ((surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0)
        error("create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain");

    // determine present mode and swapchain image count
    uint32_t present_mode_count;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data()));

    bool mailbox_supported = false;
    bool immediate_supported = false;
    for (auto pm : present_modes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
            mailbox_supported = true;
        else if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR)
            immediate_supported = true;
    }

    VkPresentModeKHR present_mode;
    uint32_t image_count;
    if (mailbox_supported) {
        present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        image_count = std::max(3u, surface_caps.minImageCount);
        if (surface_caps.maxImageCount > 0) {
            image_count = std::min(image_count, surface_caps.maxImageCount);
        }
    } else if (immediate_supported) {
        present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        image_count = surface_caps.minImageCount;
    } else {
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
        image_count = surface_caps.minImageCount;
    }

    // create swap chain
    VkSwapchainCreateInfoKHR desc;
    desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.surface = surface;
    desc.minImageCount = image_count;
    desc.imageFormat = surface_format.format;
    desc.imageColorSpace = surface_format.colorSpace;
    desc.imageExtent = image_extent;
    desc.imageArrayLayers = 1;
    desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = nullptr;
    desc.preTransform = surface_caps.currentTransform;
    desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    desc.presentMode = present_mode;
    desc.clipped = VK_TRUE;
    desc.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain;
    VK_CHECK(vkCreateSwapchainKHR(device, &desc, nullptr, &swapchain));
    return swapchain;
}

void vk_record_and_run_commands(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder) {

    VkCommandBufferAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(vk.device, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
    recorder(command_buffer);
    VK_CHECK(vkEndCommandBuffer(command_buffer));

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = nullptr;
    submit_info.pWaitDstStageMask = nullptr;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = nullptr;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
    vkFreeCommandBuffers(vk.device, command_pool, 1, &command_buffer);
}

static void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
    VkAccessFlags src_access_flags, VkImageLayout old_layout, VkAccessFlags dst_access_flags, VkImageLayout new_layout) {

    VkImageMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = src_access_flags;
    barrier.dstAccessMask = dst_access_flags;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = image_aspect_flags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
        0, nullptr, 0, nullptr, 1, &barrier);
}

void vk_ensure_staging_buffer_allocation(VkDeviceSize size) {
    if (vk.staging_buffer_size >= size)
        return;

    if (vk.staging_buffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(vk.allocator, vk.staging_buffer, vk.staging_buffer_allocation);

    VkBufferCreateInfo buffer_desc { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_desc.size = size;
    buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_create_info{};
    alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VmaAllocationInfo alloc_info;
    VK_CHECK(vmaCreateBuffer(vk.allocator, &buffer_desc, &alloc_create_info, &vk.staging_buffer, &vk.staging_buffer_allocation, &alloc_info));

    vk.staging_buffer_ptr = (uint8_t*)alloc_info.pMappedData;
    vk.staging_buffer_size = size;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT          message_severity,
    VkDebugUtilsMessageTypeFlagsEXT                 message_type,
    const VkDebugUtilsMessengerCallbackDataEXT*     callback_data,
    void*                                           user_data)
{
#ifdef _WIN32
    OutputDebugStringA(callback_data->pMessage);
    OutputDebugStringA("\n");
    DebugBreak();
#endif
    return VK_FALSE;
}

static void create_instance() {
    const char* instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#ifndef NDEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
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
            error("Vulkan: required instance extension is not available: " + std::string(name));
    }

    VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo desc { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    desc.pApplicationInfo = &app_info;
    desc.enabledExtensionCount = sizeof(instance_extensions)/sizeof(instance_extensions[0]);
    desc.ppEnabledExtensionNames = instance_extensions;

#ifndef NDEBUG
    const char* validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
    desc.enabledLayerCount = 1;
    desc.ppEnabledLayerNames = &validation_layer_name;
#endif

    VK_CHECK(vkCreateInstance(&desc, nullptr, &vk.instance));
}

static void create_device() {
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

            // Check for Vulkan 1.1 ccompatibility.
            if (VK_VERSION_MAJOR(props.apiVersion) == 1 && VK_VERSION_MINOR(props.apiVersion) >= 1)
            {
                vk.physical_device = physical_device;
                break;
            }
        }

        if (vk.physical_device == nullptr)
            error("Failed to find physical device that supports requested Vulkan API version");
    }

    VkWin32SurfaceCreateInfoKHR desc;
    desc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.hinstance = ::GetModuleHandle(nullptr);
    desc.hwnd = vk.system_window_info.info.win.window;
    VK_CHECK(vkCreateWin32SurfaceKHR(vk.instance, &desc, nullptr, &vk.surface));


    // select surface format
    {
        uint32_t format_count;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, nullptr));
        assert(format_count > 0);

        std::vector<VkSurfaceFormatKHR> candidates(format_count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, candidates.data()));

        
        if (candidates.size() == 1 && candidates[0].format == VK_FORMAT_UNDEFINED) { // special case that means we can choose any format
            vk.surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
            vk.surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        } else {
            vk.surface_format = candidates[0];
        }
    }

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
        const char* device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        uint32_t count = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vk.physical_device, nullptr, &count, nullptr));
        std::vector<VkExtensionProperties> extension_properties(count);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vk.physical_device, nullptr, &count, extension_properties.data()));

        for (auto name : device_extensions) {
            bool supported = false;
            for (const auto& property : extension_properties) {
                if (!strcmp(property.extensionName, name)) {
                    supported = true;
                    break;
                }
            }
            if (!supported)
                error("Vulkan: required device extension is not available: " + std::string(name));
        }

        const float priority = 1.0;
        VkDeviceQueueCreateInfo queue_desc;
        queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_desc.pNext = nullptr;
        queue_desc.flags = 0;
        queue_desc.queueFamilyIndex = vk.queue_family_index;
        queue_desc.queueCount = 1;
        queue_desc.pQueuePriorities = &priority;

        VkPhysicalDeviceFeatures features;
        memset(&features, 0, sizeof(features));
        features.shaderClipDistance = VK_TRUE;
        features.fillModeNonSolid = VK_TRUE;

        VkDeviceCreateInfo device_desc;
        device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_desc.pNext = nullptr;
        device_desc.flags = 0;
        device_desc.queueCreateInfoCount = 1;
        device_desc.pQueueCreateInfos = &queue_desc;
        device_desc.enabledLayerCount = 0;
        device_desc.ppEnabledLayerNames = nullptr;
        device_desc.enabledExtensionCount = sizeof(device_extensions)/sizeof(device_extensions[0]);
        device_desc.ppEnabledExtensionNames = device_extensions;
        device_desc.pEnabledFeatures = &features;
        VK_CHECK(vkCreateDevice(vk.physical_device, &device_desc, nullptr, &vk.device));
    }
}

VkPipeline create_pipeline(const Vk_Pipeline_Def&);

void vk_initialize(const SDL_SysWMinfo& window_info) {
    vk.system_window_info = window_info;

    VK_CHECK(volkInitialize());

    uint32_t instance_version = volkGetInstanceVersion();

    // Check the highest Vulkan instance version supported by the loader.
    bool loader_supports_version_higher_than_or_equal_to_1_1 =
        VK_VERSION_MAJOR(instance_version) > 1 || VK_VERSION_MINOR(instance_version) >= 1;

    if (!loader_supports_version_higher_than_or_equal_to_1_1)
        error("Vulkan loader does not support Vulkan API version 1.1");

    // If Vulkan loader reports it supports Vulkan version that is > X it does not guarantee that X is supported.
    // Only when we successfully create VkInstance by setting VkApplicationInfo::apiVersion to X
    // we will know that X is supported.
    create_instance();
    volkLoadInstance(vk.instance);

    // Create debug messenger as early as possible (even before VkDevice is created).
#ifndef NDEBUG
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
#endif

    create_device();
    volkLoadDevice(vk.device);

    vkGetDeviceQueue(vk.device, vk.queue_family_index, 0, &vk.queue);

    VmaVulkanFunctions alloc_funcs{};
    alloc_funcs.vkGetPhysicalDeviceProperties       = vkGetPhysicalDeviceProperties;
    alloc_funcs.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    alloc_funcs.vkAllocateMemory                    = vkAllocateMemory;
    alloc_funcs.vkFreeMemory                        = vkFreeMemory;
    alloc_funcs.vkMapMemory                         = vkMapMemory;
    alloc_funcs.vkUnmapMemory                       = vkUnmapMemory;
    alloc_funcs.vkBindBufferMemory                  = vkBindBufferMemory;
    alloc_funcs.vkBindImageMemory                   = vkBindImageMemory;
    alloc_funcs.vkGetBufferMemoryRequirements       = vkGetBufferMemoryRequirements;
    alloc_funcs.vkGetImageMemoryRequirements        = vkGetImageMemoryRequirements;
    alloc_funcs.vkCreateBuffer                      = vkCreateBuffer;
    alloc_funcs.vkDestroyBuffer                     = vkDestroyBuffer;
    alloc_funcs.vkCreateImage                       = vkCreateImage;
    alloc_funcs.vkDestroyImage                      = vkDestroyImage;
    alloc_funcs.vkGetBufferMemoryRequirements2KHR   = vkGetBufferMemoryRequirements2KHR;
    alloc_funcs.vkGetImageMemoryRequirements2KHR    = vkGetImageMemoryRequirements2KHR;

    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = vk.physical_device;
    allocator_info.device = vk.device;
    allocator_info.pVulkanFunctions = &alloc_funcs;
    VK_CHECK(vmaCreateAllocator(&allocator_info, &vk.allocator));

    //
    // Swapchain.
    //
    {
        vk.swapchain = create_swapchain(vk.physical_device, vk.device, vk.surface, vk.surface_format);

        uint32_t image_count;
        VK_CHECK(vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &image_count, nullptr));
        vk.swapchain_images.resize(image_count);
        VK_CHECK(vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &image_count, vk.swapchain_images.data()));

        vk.swapchain_image_views.resize(image_count);
        for (uint32_t i = 0; i < image_count; i++) {
            VkImageViewCreateInfo desc;
            desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            desc.pNext = nullptr;
            desc.flags = 0;
            desc.image = vk.swapchain_images[i];
            desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
            desc.format = vk.surface_format.format;
            desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            desc.subresourceRange.baseMipLevel = 0;
            desc.subresourceRange.levelCount = 1;
            desc.subresourceRange.baseArrayLayer = 0;
            desc.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(vk.device, &desc, nullptr, &vk.swapchain_image_views[i]));
        }
    }

    //
    // Sync primitives.
    //
    {
        VkSemaphoreCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;

        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.image_acquired));
        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.rendering_finished));

        VkFenceCreateInfo fence_desc;
        fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_desc.pNext = nullptr;
        fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(vk.device, &fence_desc, nullptr, &vk.rendering_finished_fence));
    }

    //
    // Command pool.
    //
    {
        VkCommandPoolCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        desc.queueFamilyIndex = vk.queue_family_index;

        VK_CHECK(vkCreateCommandPool(vk.device, &desc, nullptr, &vk.command_pool));
    }

    //
    // Command buffer.
    //
    {
        VkCommandBufferAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.pNext = nullptr;
        alloc_info.commandPool = vk.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(vk.device, &alloc_info, &vk.command_buffer));
    }

    //
    // Depth attachment image.
    // 
    {
        // choose depth image format
        {
            std::array<VkFormat, 2> candidates = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
            for (auto format : candidates) {
                VkFormatProperties props;
                vkGetPhysicalDeviceFormatProperties(vk.physical_device, format, &props);
                if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
                    vk.depth_image_format = format;
                    break;
                }
            }
            if (vk.depth_image_format == VK_FORMAT_UNDEFINED)
                error("failed to choose depth attachment format");
        }

        // create depth image
        {
            VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            create_info.imageType       = VK_IMAGE_TYPE_2D;
            create_info.format          = vk.depth_image_format;
            create_info.extent.width    = vk.surface_width;
            create_info.extent.height   = vk.surface_height;
            create_info.extent.depth    = 1;
            create_info.mipLevels       = 1;
            create_info.arrayLayers     = 1;
            create_info.samples         = VK_SAMPLE_COUNT_1_BIT;
            create_info.tiling          = VK_IMAGE_TILING_OPTIMAL;
            create_info.usage           = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            create_info.sharingMode     = VK_SHARING_MODE_EXCLUSIVE;
            create_info.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo alloc_create_info{};
            alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VK_CHECK(vmaCreateImage(vk.allocator, &create_info, &alloc_create_info, &vk.depth_image, &vk.depth_image_allocation, nullptr));
        }

        // create depth image view
        {
            VkImageViewCreateInfo desc { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            desc.image      = vk.depth_image;
            desc.viewType   = VK_IMAGE_VIEW_TYPE_2D;
            desc.format     = vk.depth_image_format;

            desc.subresourceRange.aspectMask        = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.subresourceRange.baseMipLevel      = 0;
            desc.subresourceRange.levelCount        = 1;
            desc.subresourceRange.baseArrayLayer    = 0;
            desc.subresourceRange.layerCount        = 1;

            VK_CHECK(vkCreateImageView(vk.device, &desc, nullptr, &vk.depth_image_view));
        }

        VkImageAspectFlags image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        vk_record_and_run_commands(vk.command_pool, vk.queue, [&image_aspect_flags](VkCommandBuffer command_buffer) {
            record_image_layout_transition(command_buffer, vk.depth_image, image_aspect_flags, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        });
    }
}

void vk_shutdown() {
    vkDeviceWaitIdle(vk.device);

    if (vk.staging_buffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(vk.allocator, vk.staging_buffer, vk.staging_buffer_allocation);

    for (auto pipeline : vk.pipelines) {
        vkDestroyPipeline(vk.device, pipeline, nullptr);
    }
    vk.pipeline_defs.clear();
    vk.pipelines.clear();

    vmaDestroyImage(vk.allocator, vk.depth_image, vk.depth_image_allocation);
    vkDestroyImageView(vk.device, vk.depth_image_view, nullptr);

    vkDestroyCommandPool(vk.device, vk.command_pool, nullptr);

    for (auto image_view : vk.swapchain_image_views) {
        vkDestroyImageView(vk.device, image_view, nullptr);
    }
    vk.swapchain_images.clear();
    vkDestroySwapchainKHR(vk.device, vk.swapchain, nullptr);

    vkDestroySemaphore(vk.device, vk.image_acquired, nullptr);
    vkDestroySemaphore(vk.device, vk.rendering_finished, nullptr);
    vkDestroyFence(vk.device, vk.rendering_finished_fence, nullptr);

    vmaDestroyAllocator(vk.allocator);
    
    vkDestroyDevice(vk.device, nullptr);
    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);

#ifndef NDEBUG
    vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.debug_utils_messenger, nullptr);
#endif

    vkDestroyInstance(vk.instance, nullptr);
}

void vk_record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
        VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
        VkAccessFlags src_access, VkAccessFlags dst_access) {

    VkBufferMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cb, src_stages, dst_stages, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

VkBuffer vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, const char* name) {
    VkBufferCreateInfo desc { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    desc.size           = size;
    desc.usage          = usage;
    desc.sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
    return get_resource_manager()->create_buffer(desc, false, nullptr, name);
}

VkBuffer vk_create_host_visible_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** buffer_ptr, const char* name) {
    VkBufferCreateInfo desc { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    desc.size           = size;
    desc.usage          = usage;
    desc.sharingMode    = VK_SHARING_MODE_EXCLUSIVE;

    return get_resource_manager()->create_buffer(desc, true, buffer_ptr, name);
}

Vk_Image vk_create_texture(int width, int height, VkFormat format, int mip_levels, const uint8_t* pixels, int bytes_per_pixel, const char* name) {
    Vk_Image image;

    // create image
    {
        VkImageCreateInfo desc { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        desc.imageType      = VK_IMAGE_TYPE_2D;
        desc.format         = format;
        desc.extent.width   = width;
        desc.extent.height  = height;
        desc.extent.depth   = 1;
        desc.mipLevels      = mip_levels;
        desc.arrayLayers    = 1;
        desc.samples        = VK_SAMPLE_COUNT_1_BIT;
        desc.tiling         = VK_IMAGE_TILING_OPTIMAL;
        desc.usage          = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
        desc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

        image.handle = get_resource_manager()->create_image(desc, name);
    }

    // create image view
    {
        VkImageViewCreateInfo desc { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        desc.image      = image.handle;
        desc.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        desc.format     = format;
        desc.subresourceRange.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.subresourceRange.baseMipLevel      = 0;
        desc.subresourceRange.levelCount        = VK_REMAINING_MIP_LEVELS;
        desc.subresourceRange.baseArrayLayer    = 0;
        desc.subresourceRange.layerCount        = 1;

        image.view = get_resource_manager()->create_image_view(desc, (name + std::string(" ImageView")).c_str());
    }

    // upload image data
    {
        VkBufferImageCopy regions[16];
        int num_regions = 0;

        int buffer_size = 0;

        while (true) {
            VkBufferImageCopy region;
            region.bufferOffset = buffer_size;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = num_regions;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = VkOffset3D{ 0, 0, 0 };
            region.imageExtent = VkExtent3D{ (uint32_t)width, (uint32_t)height, 1 };

            regions[num_regions] = region;
            num_regions++;

            buffer_size += width * height * bytes_per_pixel;

            if (mip_levels == 1 || (width == 1 && height == 1))
                break;

            width >>= 1;
            if (width < 1) width = 1;

            height >>= 1;
            if (height < 1) height = 1;
        }

        vk_ensure_staging_buffer_allocation(buffer_size);
        memcpy(vk.staging_buffer_ptr, pixels, buffer_size);

        vk_record_and_run_commands(vk.command_pool, vk.queue,
            [&image, &num_regions, &regions](VkCommandBuffer command_buffer) {

            vk_record_buffer_memory_barrier(command_buffer, vk.staging_buffer,
                VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

            record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                0, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            vkCmdCopyBufferToImage(command_buffer, vk.staging_buffer, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions);

            record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
    }

    return image;
}

Vk_Image vk_create_render_target(int width, int height, VkFormat format, const char* name) {
    Vk_Image image;

    // create image
    {
        VkImageCreateInfo desc { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        desc.imageType      = VK_IMAGE_TYPE_2D;
        desc.format         = format;
        desc.extent.width   = width;
        desc.extent.height  = height;
        desc.extent.depth   = 1;
        desc.mipLevels      = 1;
        desc.arrayLayers    = 1;
        desc.samples        = VK_SAMPLE_COUNT_1_BIT;
        desc.tiling         = VK_IMAGE_TILING_OPTIMAL;
        desc.usage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
        desc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

        image.handle = get_resource_manager()->create_image(desc, name);
    }
    // create image view
    {
        VkImageViewCreateInfo desc { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        desc.image      = image.handle;
        desc.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        desc.format     = format;
        desc.subresourceRange.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.subresourceRange.baseMipLevel      = 0;
        desc.subresourceRange.levelCount        = 1;
        desc.subresourceRange.baseArrayLayer    = 0;
        desc.subresourceRange.layerCount        = 1;

        image.view = get_resource_manager()->create_image_view(desc, (name + std::string(" ImageView")).c_str());
    }
    return image;
}

static VkPipeline create_pipeline(const Vk_Pipeline_Def& def) {
    //
    // Shader stages.
    //
    auto get_shader_stage_desc = [](VkShaderStageFlagBits stage, VkShaderModule shader_module, const char* entry) {
        VkPipelineShaderStageCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.stage = stage;
        desc.module = shader_module;
        desc.pName = entry;
        desc.pSpecializationInfo = nullptr;
        return desc;
    };

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages_state {
        get_shader_stage_desc(VK_SHADER_STAGE_VERTEX_BIT, def.vs_module, "main_vs"),
        get_shader_stage_desc(VK_SHADER_STAGE_FRAGMENT_BIT, def.fs_module, "main_fs")
    };

    //
    // Vertex input.
    //
    auto bindings = Vertex::get_bindings();
    auto attribs = Vertex::get_attributes();

    VkPipelineVertexInputStateCreateInfo vertex_input_state;
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.pNext = nullptr;
    vertex_input_state.flags = 0;
    vertex_input_state.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertex_input_state.pVertexBindingDescriptions = bindings.data();
    vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size());
    vertex_input_state.pVertexAttributeDescriptions = attribs.data();

    //
    // Primitive assembly.
    //
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.pNext = nullptr;
    input_assembly_state.flags = 0;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    //
    // Viewport.
    //
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(vk.surface_width);
    viewport.height = static_cast<float>(vk.surface_height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = static_cast<uint32_t>(vk.surface_width);
    scissor.extent.height = static_cast<uint32_t>(vk.surface_height);

    VkPipelineViewportStateCreateInfo viewport_state;
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    //
    // Rasterization.
    //
    VkPipelineRasterizationStateCreateInfo rasterization_state;
    rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.pNext = nullptr;
    rasterization_state.flags = 0;
    rasterization_state.depthClampEnable = VK_FALSE;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.depthBiasEnable = VK_FALSE;
    rasterization_state.depthBiasConstantFactor = 0.0f;
    rasterization_state.depthBiasClamp = 0.0f;
    rasterization_state.depthBiasSlopeFactor = 0.0f;
    rasterization_state.lineWidth = 1.0f;

    //
    // Multisampling.
    //
    VkPipelineMultisampleStateCreateInfo multisample_state;
    multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.pNext = nullptr;
    multisample_state.flags = 0;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable = VK_FALSE;
    multisample_state.minSampleShading = 1.0f;
    multisample_state.pSampleMask = nullptr;
    multisample_state.alphaToCoverageEnable = VK_FALSE;
    multisample_state.alphaToOneEnable = VK_FALSE;

    //
    // Depth/stencil.
    //
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.pNext = nullptr;
    depth_stencil_state.flags = 0;
    depth_stencil_state.depthTestEnable = VK_TRUE;
    depth_stencil_state.depthWriteEnable = VK_TRUE;
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state.stencilTestEnable = VK_FALSE;
    depth_stencil_state.minDepthBounds = 0.0;
    depth_stencil_state.maxDepthBounds = 0.0;
    depth_stencil_state.front = {};
    depth_stencil_state.back = {};

    //
    // Blending.
    //
    VkPipelineColorBlendAttachmentState attachment_blend_state = {};
    attachment_blend_state.blendEnable = VK_FALSE;
    attachment_blend_state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend_state;
    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.pNext = nullptr;
    blend_state.flags = 0;
    blend_state.logicOpEnable = VK_FALSE;
    blend_state.logicOp = VK_LOGIC_OP_COPY;
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &attachment_blend_state;
    blend_state.blendConstants[0] = 0.0f;
    blend_state.blendConstants[1] = 0.0f;
    blend_state.blendConstants[2] = 0.0f;
    blend_state.blendConstants[3] = 0.0f;

    //
    // Finally create graphics pipeline.
    //
    VkGraphicsPipelineCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.stageCount = static_cast<uint32_t>(shader_stages_state.size());
    desc.pStages = shader_stages_state.data();
    desc.pVertexInputState = &vertex_input_state;
    desc.pInputAssemblyState = &input_assembly_state;
    desc.pTessellationState = nullptr;
    desc.pViewportState = &viewport_state;
    desc.pRasterizationState = &rasterization_state;
    desc.pMultisampleState = &multisample_state;
    desc.pDepthStencilState = &depth_stencil_state;
    desc.pColorBlendState = &blend_state;
    desc.pDynamicState = nullptr;
    desc.layout = def.pipeline_layout;
    desc.renderPass = def.render_pass;
    desc.subpass = 0;
    desc.basePipelineHandle = VK_NULL_HANDLE;
    desc.basePipelineIndex = -1;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &desc, nullptr, &pipeline));
    return pipeline;
}

VkPipeline vk_find_pipeline(const Vk_Pipeline_Def& def) {
    for (size_t i = 0; i < vk.pipeline_defs.size(); i++) {
        if (vk.pipeline_defs[i] == def)
            return vk.pipelines[i];
    }

    VkPipeline pipeline = create_pipeline(def);
    vk.pipeline_defs.push_back(def);
    vk.pipelines.push_back(pipeline);
    return pipeline;
}

void vk_begin_frame() {
	START_TIMER
    VK_CHECK(vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.image_acquired, VK_NULL_HANDLE, &vk.swapchain_image_index));
	STOP_TIMER("vkAcquireNextImageKHR")

    VK_CHECK(vkWaitForFences(vk.device, 1, &vk.rendering_finished_fence, VK_FALSE, std::numeric_limits<uint64_t>::max()));
    VK_CHECK(vkResetFences(vk.device, 1, &vk.rendering_finished_fence));

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(vk.command_buffer, &begin_info));
}

void vk_end_frame() {
    VK_CHECK(vkEndCommandBuffer(vk.command_buffer));

    VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &vk.image_acquired;
    submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &vk.rendering_finished;
	START_TIMER
    VK_CHECK(vkQueueSubmit(vk.queue, 1, &submit_info, vk.rendering_finished_fence));
	STOP_TIMER("vkQueueSubmit")

    VkPresentInfoKHR present_info;
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &vk.rendering_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vk.swapchain;
    present_info.pImageIndices = &vk.swapchain_image_index;
    present_info.pResults = nullptr;
	START_TIMER
    VK_CHECK(vkQueuePresentKHR(vk.queue, &present_info));
	STOP_TIMER("vkQueuePresentKHR")
}
