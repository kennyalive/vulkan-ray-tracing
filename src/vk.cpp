#include "vk.h"
#include "common.h"
#include "debug.h"
#include "geometry.h"
#include "resource_manager.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <vector>

//
// Vk_Instance is a container that stores common Vulkan resources like vulkan instance,
// device, command pool, swapchain, etc.
//
Vk_Instance vk;

static void create_swapchain(bool vsync) {
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
        error("create_swapchain: VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain");

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
    desc.imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

static void destroy_swapchain() {
    for (auto image_view : vk.swapchain_info.image_views) {
        vkDestroyImageView(vk.device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(vk.device, vk.swapchain_info.handle, nullptr);
    vk.swapchain_info = Swapchain_Info{};
}

static void create_instance() {
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
            error("Vulkan: required instance extension is not available: " + std::string(name));
    }

    VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo desc { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    desc.pApplicationInfo        = &app_info;
    desc.enabledExtensionCount   = sizeof(instance_extensions)/sizeof(instance_extensions[0]);
    desc.ppEnabledExtensionNames = instance_extensions;

    if (vk.create_info.enable_validation_layers) {
        static const char* layer_names[] = {
            "VK_LAYER_LUNARG_standard_validation"
        };
        desc.enabledLayerCount = array_length(layer_names);
        desc.ppEnabledLayerNames = layer_names;
    }    

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

    VkWin32SurfaceCreateInfoKHR desc { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    desc.hinstance  = ::GetModuleHandle(nullptr);
    desc.hwnd       = vk.create_info.windowing_system_info.info.win.window;
    VK_CHECK(vkCreateWin32SurfaceKHR(vk.instance, &desc, nullptr, &vk.surface));

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
        VkDeviceQueueCreateInfo queue_desc { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queue_desc.queueFamilyIndex = vk.queue_family_index;
        queue_desc.queueCount       = 1;
        queue_desc.pQueuePriorities = &priority;

        VkDeviceCreateInfo device_desc { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        device_desc.queueCreateInfoCount    = 1;
        device_desc.pQueueCreateInfos       = &queue_desc;
        device_desc.enabledExtensionCount   = sizeof(device_extensions)/sizeof(device_extensions[0]);
        device_desc.ppEnabledExtensionNames = device_extensions;

        VK_CHECK(vkCreateDevice(vk.physical_device, &device_desc, nullptr, &vk.device));
    }
}

static void record_image_layout_transition(
    VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags image_aspect_flags,
    VkAccessFlags src_access_flags, VkImageLayout old_layout,
    VkAccessFlags dst_access_flags, VkImageLayout new_layout,
    uint32_t mip_level = VK_REMAINING_MIP_LEVELS) {

    VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcAccessMask       = src_access_flags;
    barrier.dstAccessMask       = dst_access_flags;
    barrier.oldLayout           = old_layout;
    barrier.newLayout           = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;

    barrier.subresourceRange.aspectMask     = image_aspect_flags;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

    if (mip_level == VK_REMAINING_MIP_LEVELS) {
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    } else {
        barrier.subresourceRange.baseMipLevel   = mip_level;
        barrier.subresourceRange.levelCount     = 1;
    }

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
        0, nullptr, 0, nullptr, 1, &barrier);
}

static void create_depth_buffer() {
    // choose depth image format
    {
        VkFormat candidates[2] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
        for (auto format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(vk.physical_device, format, &props);
            if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
                vk.depth_info.format = format;
                break;
            }
        }
        if (vk.depth_info.format == VK_FORMAT_UNDEFINED)
            error("failed to choose depth attachment format");
    }

    // create depth image
    {
        VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        create_info.imageType       = VK_IMAGE_TYPE_2D;
        create_info.format          = vk.depth_info.format;
        create_info.extent.width    = vk.surface_size.width;
        create_info.extent.height   = vk.surface_size.height;
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

        VK_CHECK(vmaCreateImage(vk.allocator, &create_info, &alloc_create_info, &vk.depth_info.image, &vk.depth_info.allocation, nullptr));
    }

    // create depth image view
    {
        VkImageViewCreateInfo desc { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        desc.image      = vk.depth_info.image;
        desc.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        desc.format     = vk.depth_info.format;

        desc.subresourceRange.aspectMask        = VK_IMAGE_ASPECT_DEPTH_BIT;
        desc.subresourceRange.baseMipLevel      = 0;
        desc.subresourceRange.levelCount        = 1;
        desc.subresourceRange.baseArrayLayer    = 0;
        desc.subresourceRange.layerCount        = 1;

        VK_CHECK(vkCreateImageView(vk.device, &desc, nullptr, &vk.depth_info.image_view));
    }

    VkImageAspectFlags image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    vk_record_and_run_commands(vk.command_pool, vk.queue, [&image_aspect_flags](VkCommandBuffer command_buffer) {
        record_image_layout_transition(command_buffer, vk.depth_info.image, image_aspect_flags, 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    });
}

static void destroy_depth_buffer() {
    vmaDestroyImage(vk.allocator, vk.depth_info.image, vk.depth_info.allocation);
    vkDestroyImageView(vk.device, vk.depth_info.image_view, nullptr);
    vk.depth_info = Depth_Buffer_Info{};
}

void vk_initialize(const Vk_Create_Info& create_info) {
    vk.create_info = create_info;

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
    vk_create_debug_utils_messenger();

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
    // Sync primitives.
    //
    {
        VkSemaphoreCreateInfo desc { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.image_acquired));
        VK_CHECK(vkCreateSemaphore(vk.device, &desc, nullptr, &vk.rendering_finished));

        VkFenceCreateInfo fence_desc { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(vk.device, &fence_desc, nullptr, &vk.rendering_finished_fence));
    }

    //
    // Command pool.
    //
    {
        VkCommandPoolCreateInfo desc { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        desc.queueFamilyIndex = vk.queue_family_index;
        VK_CHECK(vkCreateCommandPool(vk.device, &desc, nullptr, &vk.command_pool));
    }

    //
    // Command buffer.
    //
    {
        VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        alloc_info.commandPool        = vk.command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(vk.device, &alloc_info, &vk.command_buffer));
    }

    //
    // Select surface format.
    //
    {
        uint32_t format_count;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, nullptr));
        assert(format_count > 0);

        std::vector<VkSurfaceFormatKHR> candidates(format_count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical_device, vk.surface, &format_count, candidates.data()));

        // don't support special case described in the spec
        assert(!(candidates.size() == 1 && candidates[0].format == VK_FORMAT_UNDEFINED));

        VkFormat supported_srgb_formats[] = {
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_A8B8G8R8_SRGB_PACK32
        };

        [&candidates, &supported_srgb_formats]() {
            for (VkFormat srgb_format : supported_srgb_formats) {
                for (VkSurfaceFormatKHR surface_format : candidates) {
                    if (surface_format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                        continue;
                    if (surface_format.format == srgb_format) {
                        vk.surface_format = surface_format;
                        return;
                    }
                }
            }
            error("Failed to find supported surface format");
        } ();
    }

    create_swapchain(true);
    create_depth_buffer();
}

void vk_shutdown() {
    vkDeviceWaitIdle(vk.device);

    if (vk.staging_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vk.allocator, vk.staging_buffer, vk.staging_buffer_allocation);
    }

    for (auto pipeline : vk.pipelines) {
        vkDestroyPipeline(vk.device, pipeline, nullptr);
    }
    vk.pipeline_defs.clear();
    vk.pipelines.clear();

    vkDestroyCommandPool(vk.device, vk.command_pool, nullptr);
    vkDestroySemaphore(vk.device, vk.image_acquired, nullptr);
    vkDestroySemaphore(vk.device, vk.rendering_finished, nullptr);
    vkDestroyFence(vk.device, vk.rendering_finished_fence, nullptr);
    destroy_swapchain();
    destroy_depth_buffer();
    vmaDestroyAllocator(vk.allocator);
    vkDestroyDevice(vk.device, nullptr);
    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    vk_destroy_debug_utils_messenger();
    vkDestroyInstance(vk.instance, nullptr);
}

void vk_release_resolution_dependent_resources() {
    destroy_swapchain();
    destroy_depth_buffer();
}

void vk_restore_resolution_dependent_resources(bool vsync) {
    create_swapchain(vsync);
    create_depth_buffer();
}

void vk_ensure_staging_buffer_allocation(VkDeviceSize size) {
    if (vk.staging_buffer_size >= size)
        return;

    if (vk.staging_buffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(vk.allocator, vk.staging_buffer, vk.staging_buffer_allocation);

    VkBufferCreateInfo buffer_desc { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer_desc.size        = size;
    buffer_desc.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_create_info{};
    alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VmaAllocationInfo alloc_info;
    VK_CHECK(vmaCreateBuffer(vk.allocator, &buffer_desc, &alloc_create_info, &vk.staging_buffer, &vk.staging_buffer_allocation, &alloc_info));

    vk.staging_buffer_ptr = (uint8_t*)alloc_info.pMappedData;
    vk.staging_buffer_size = size;
}

VkPipeline create_pipeline(const Vk_Pipeline_Def&);

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
        desc.usage          = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (generate_mipmaps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
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

        vk_record_and_run_commands(vk.command_pool, vk.queue,
            [&image, &region, generate_mipmaps, width, height, mip_levels](VkCommandBuffer command_buffer) {

            record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                0, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0 /* first mip level */);

            vkCmdCopyBufferToImage(command_buffer, vk.staging_buffer, image.handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            if (!generate_mipmaps) {
                record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    0 /* first mip level */);
            }

            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            int32_t w = (int32_t)width;
            int32_t h = (int32_t)height;

            for (uint32_t i = 1; i < mip_levels; i++) {
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcOffsets[1] = VkOffset3D { w, h, 1 };

                w = std::max(w >> 1, 1);
                h = std::max(h >> 1, 1);

                blit.dstSubresource.mipLevel = i;
                blit.dstOffsets[1] = VkOffset3D { w, h, 1 };

                record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    i-1);

                record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                    0, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    i);

                vkCmdBlitImage(command_buffer,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit, VK_FILTER_LINEAR);

                record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    i-1);
            }

            if (generate_mipmaps) {
                 record_image_layout_transition(command_buffer, image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        mip_levels - 1);
            }
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
        VkPipelineShaderStageCreateInfo desc { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        desc.stage  = stage;
        desc.module = shader_module;
        desc.pName  = entry;
        return desc;
    };

    VkPipelineShaderStageCreateInfo shader_stages_state[2] {
        get_shader_stage_desc(VK_SHADER_STAGE_VERTEX_BIT, def.vs_module, "main_vs"),
        get_shader_stage_desc(VK_SHADER_STAGE_FRAGMENT_BIT, def.fs_module, "main_fs")
    };

    //
    // Vertex input.
    //
    auto bindings = Vertex::get_bindings();
    auto attribs = Vertex::get_attributes();

    VkPipelineVertexInputStateCreateInfo vertex_input_state { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input_state.vertexBindingDescriptionCount    = static_cast<uint32_t>(bindings.size());
    vertex_input_state.pVertexBindingDescriptions       = bindings.data();
    vertex_input_state.vertexAttributeDescriptionCount  = static_cast<uint32_t>(attribs.size());
    vertex_input_state.pVertexAttributeDescriptions     = attribs.data();

    //
    // Primitive assembly.
    //
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    //
    // Viewport.
    //
    VkPipelineViewportStateCreateInfo viewport_state { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount    = 1;
    viewport_state.pViewports       = nullptr;
    viewport_state.scissorCount     = 1;
    viewport_state.pScissors        = nullptr;

    //
    // Rasterization.
    //
    VkPipelineRasterizationStateCreateInfo rasterization_state { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
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

    //
    // Multisampling.
    //
    VkPipelineMultisampleStateCreateInfo multisample_state { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisample_state.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable   = VK_FALSE;
    multisample_state.minSampleShading      = 1.0f;
    multisample_state.pSampleMask           = nullptr;
    multisample_state.alphaToCoverageEnable = VK_FALSE;
    multisample_state.alphaToOneEnable      = VK_FALSE;

    //
    // Depth/stencil.
    //
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depth_stencil_state.depthTestEnable         = VK_TRUE;
    depth_stencil_state.depthWriteEnable        = VK_TRUE;
    depth_stencil_state.depthCompareOp          = VK_COMPARE_OP_LESS;
    depth_stencil_state.depthBoundsTestEnable   = VK_FALSE;
    depth_stencil_state.stencilTestEnable       = VK_FALSE;

    //
    // Blending.
    //
    VkPipelineColorBlendAttachmentState attachment_blend_state = {};
    attachment_blend_state.blendEnable = VK_FALSE;
    attachment_blend_state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend_state { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blend_state.logicOpEnable   = VK_FALSE;
    blend_state.logicOp         = VK_LOGIC_OP_COPY;
    blend_state.attachmentCount = 1;
    blend_state.pAttachments    = &attachment_blend_state;

    //
    // Dynamic state.
    //
    VkDynamicState dynamic_state[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state_info.dynamicStateCount = 2;
    dynamic_state_info.pDynamicStates    = dynamic_state;

    //
    // Finally create graphics pipeline.
    //
    VkGraphicsPipelineCreateInfo desc { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    desc.stageCount             = array_length(shader_stages_state);
    desc.pStages                = shader_stages_state;
    desc.pVertexInputState      = &vertex_input_state;
    desc.pInputAssemblyState    = &input_assembly_state;
    desc.pTessellationState     = nullptr;
    desc.pViewportState         = &viewport_state;
    desc.pRasterizationState    = &rasterization_state;
    desc.pMultisampleState      = &multisample_state;
    desc.pDepthStencilState     = &depth_stencil_state;
    desc.pColorBlendState       = &blend_state;
    desc.pDynamicState          = &dynamic_state_info;
    desc.layout                 = def.pipeline_layout;
    desc.renderPass             = def.render_pass;
    desc.subpass                = 0;

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
    VK_CHECK(vkAcquireNextImageKHR(vk.device, vk.swapchain_info.handle, UINT64_MAX, vk.image_acquired, VK_NULL_HANDLE, &vk.swapchain_image_index));
    STOP_TIMER("vkAcquireNextImageKHR")

    VK_CHECK(vkWaitForFences(vk.device, 1, &vk.rendering_finished_fence, VK_FALSE, std::numeric_limits<uint64_t>::max()));
    VK_CHECK(vkResetFences(vk.device, 1, &vk.rendering_finished_fence));

    VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(vk.command_buffer, &begin_info));
}

void vk_end_frame() {
    VK_CHECK(vkEndCommandBuffer(vk.command_buffer));

    const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.waitSemaphoreCount   = 1;
    submit_info.pWaitSemaphores      = &vk.image_acquired;
    submit_info.pWaitDstStageMask    = &wait_dst_stage_mask;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &vk.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores    = &vk.rendering_finished;

    START_TIMER
    VK_CHECK(vkQueueSubmit(vk.queue, 1, &submit_info, vk.rendering_finished_fence));
    STOP_TIMER("vkQueueSubmit")

    VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &vk.rendering_finished;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &vk.swapchain_info.handle;
    present_info.pImageIndices      = &vk.swapchain_image_index;

    START_TIMER
    VK_CHECK(vkQueuePresentKHR(vk.queue, &present_info));
    STOP_TIMER("vkQueuePresentKHR")
}

void vk_record_and_run_commands(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder) {

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
