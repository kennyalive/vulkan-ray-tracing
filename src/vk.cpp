#include "vk.h"
#include "geometry.h"
#include "resource_manager.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <vector>

const int DEVICE_LOCAL_CHUNK_SIZE = 16 * 1024 * 1024;
const int HOST_VISIBLE_CHUNK_SIZE = 8 * 1024 * 1024;

Vk_Instance vk;

//
// Vulkan API functions used by the renderer.
//
PFN_vkGetInstanceProcAddr                       vkGetInstanceProcAddr;

PFN_vkCreateInstance                            vkCreateInstance;
PFN_vkEnumerateInstanceExtensionProperties      vkEnumerateInstanceExtensionProperties;

PFN_vkCreateDevice                              vkCreateDevice;
PFN_vkDestroyInstance                           vkDestroyInstance;
PFN_vkEnumerateDeviceExtensionProperties        vkEnumerateDeviceExtensionProperties;
PFN_vkEnumeratePhysicalDevices                  vkEnumeratePhysicalDevices;
PFN_vkGetDeviceProcAddr                         vkGetDeviceProcAddr;
PFN_vkGetPhysicalDeviceFeatures                 vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFormatProperties         vkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceMemoryProperties         vkGetPhysicalDeviceMemoryProperties;
PFN_vkGetPhysicalDeviceProperties               vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties    vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkCreateWin32SurfaceKHR                     vkCreateWin32SurfaceKHR;
PFN_vkDestroySurfaceKHR                         vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR   vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR        vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR   vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR        vkGetPhysicalDeviceSurfaceSupportKHR;

PFN_vkAllocateCommandBuffers                    vkAllocateCommandBuffers;
PFN_vkAllocateDescriptorSets                    vkAllocateDescriptorSets;
PFN_vkAllocateMemory                            vkAllocateMemory;
PFN_vkBeginCommandBuffer                        vkBeginCommandBuffer;
PFN_vkBindBufferMemory                          vkBindBufferMemory;
PFN_vkBindImageMemory                           vkBindImageMemory;
PFN_vkCmdBeginRenderPass                        vkCmdBeginRenderPass;
PFN_vkCmdBindDescriptorSets                     vkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer                        vkCmdBindIndexBuffer;
PFN_vkCmdBindPipeline                           vkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers                      vkCmdBindVertexBuffers;
PFN_vkCmdBlitImage                              vkCmdBlitImage;
PFN_vkCmdClearAttachments                       vkCmdClearAttachments;
PFN_vkCmdCopyBufferToImage                      vkCmdCopyBufferToImage;
PFN_vkCmdCopyImage                              vkCmdCopyImage;
PFN_vkCmdCopyBuffer                             vkCmdCopyBuffer;
PFN_vkCmdDraw                                   vkCmdDraw;
PFN_vkCmdDrawIndexed                            vkCmdDrawIndexed;
PFN_vkCmdEndRenderPass                          vkCmdEndRenderPass;
PFN_vkCmdPipelineBarrier                        vkCmdPipelineBarrier;
PFN_vkCmdPushConstants                          vkCmdPushConstants;
PFN_vkCmdSetDepthBias                           vkCmdSetDepthBias;
PFN_vkCmdSetScissor                             vkCmdSetScissor;
PFN_vkCmdSetViewport                            vkCmdSetViewport;
PFN_vkCreateBuffer                              vkCreateBuffer;
PFN_vkCreateCommandPool                         vkCreateCommandPool;
PFN_vkCreateDescriptorPool                      vkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout                 vkCreateDescriptorSetLayout;
PFN_vkCreateFence                               vkCreateFence;
PFN_vkCreateFramebuffer                         vkCreateFramebuffer;
PFN_vkCreateGraphicsPipelines                   vkCreateGraphicsPipelines;
PFN_vkCreateImage                               vkCreateImage;
PFN_vkCreateImageView                           vkCreateImageView;
PFN_vkCreatePipelineLayout                      vkCreatePipelineLayout;
PFN_vkCreateRenderPass                          vkCreateRenderPass;
PFN_vkCreateSampler                             vkCreateSampler;
PFN_vkCreateSemaphore                           vkCreateSemaphore;
PFN_vkCreateShaderModule                        vkCreateShaderModule;
PFN_vkDestroyBuffer                             vkDestroyBuffer;
PFN_vkDestroyCommandPool                        vkDestroyCommandPool;
PFN_vkDestroyDescriptorPool                     vkDestroyDescriptorPool;
PFN_vkDestroyDescriptorSetLayout                vkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice                             vkDestroyDevice;
PFN_vkDestroyFence                              vkDestroyFence;
PFN_vkDestroyFramebuffer                        vkDestroyFramebuffer;
PFN_vkDestroyImage                              vkDestroyImage;
PFN_vkDestroyImageView                          vkDestroyImageView;
PFN_vkDestroyPipeline                           vkDestroyPipeline;
PFN_vkDestroyPipelineLayout                     vkDestroyPipelineLayout;
PFN_vkDestroyRenderPass                         vkDestroyRenderPass;
PFN_vkDestroySampler                            vkDestroySampler;
PFN_vkDestroySemaphore                          vkDestroySemaphore;
PFN_vkDestroyShaderModule                       vkDestroyShaderModule;
PFN_vkDeviceWaitIdle                            vkDeviceWaitIdle;
PFN_vkEndCommandBuffer                          vkEndCommandBuffer;
PFN_vkFreeCommandBuffers                        vkFreeCommandBuffers;
PFN_vkFreeDescriptorSets                        vkFreeDescriptorSets;
PFN_vkFreeMemory                                vkFreeMemory;
PFN_vkGetBufferMemoryRequirements               vkGetBufferMemoryRequirements;
PFN_vkGetDeviceQueue                            vkGetDeviceQueue;
PFN_vkGetImageMemoryRequirements                vkGetImageMemoryRequirements;
PFN_vkGetImageSubresourceLayout                 vkGetImageSubresourceLayout;
PFN_vkMapMemory                                 vkMapMemory;
PFN_vkQueueSubmit                               vkQueueSubmit;
PFN_vkQueueWaitIdle                             vkQueueWaitIdle;
PFN_vkResetDescriptorPool                       vkResetDescriptorPool;
PFN_vkResetFences                               vkResetFences;
PFN_vkUnmapMemory                               vkUnmapMemory;
PFN_vkUpdateDescriptorSets                      vkUpdateDescriptorSets;
PFN_vkWaitForFences                             vkWaitForFences;
PFN_vkAcquireNextImageKHR                       vkAcquireNextImageKHR;
PFN_vkCreateSwapchainKHR                        vkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR                       vkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR                     vkGetSwapchainImagesKHR;
PFN_vkQueuePresentKHR                           vkQueuePresentKHR;
////////////////////////////////////////////////////////////////////////////

static uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((memory_type_bits & (1 << i)) != 0 &&
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    error("Vulkan: failed to find matching memory type with requested properties");
    return -1;
}

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

struct Allocation {
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    void* data = nullptr;
};

static Allocation allocate_memory(const VkMemoryRequirements& memory_requirements, bool host_visible) {
    Vk_Instance::Chunk* chunk = nullptr;

    auto& chunks = host_visible ? vk.host_visible_chunks : vk.device_local_chunks;
    VkDeviceSize chunk_size = host_visible ? HOST_VISIBLE_CHUNK_SIZE : DEVICE_LOCAL_CHUNK_SIZE;

    // Try to find an existing chunk of sufficient capacity.
    const auto mask = ~(memory_requirements.alignment - 1);
    for (size_t i = 0; i < chunks.size(); i++) {
        if (((1 << chunks[i].memory_type_index) & memory_requirements.memoryTypeBits) == 0)
            continue;

        // ensure that memory region has proper alignment
        VkDeviceSize offset = (chunks[i].used + memory_requirements.alignment - 1) & mask;

        if (offset + memory_requirements.size <= chunk_size) {
            chunks[i].used = offset + memory_requirements.size;
            chunk = &chunks[i];
            break;
        }
    }

    // Allocate a new chunk in case we couldn't find suitable existing chunk.
    if (chunk == nullptr) {
        VkMemoryAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.pNext = nullptr;
        alloc_info.allocationSize = std::max(chunk_size, memory_requirements.size);
        alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits,
            host_visible
                ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory memory;
        VK_CHECK(vkAllocateMemory(vk.device, &alloc_info, nullptr, &memory));

        chunks.push_back(Vk_Instance::Chunk());
        chunk = &chunks.back();
        chunk->memory = memory;
        chunk->used = memory_requirements.size;
        chunk->memory_type_index = alloc_info.memoryTypeIndex;

        if (host_visible) {
            VK_CHECK(vkMapMemory(vk.device, chunk->memory, 0, VK_WHOLE_SIZE, 0, &chunk->data));
        }
    }

    Allocation alloc;
    alloc.memory = chunk->memory;
    alloc.offset = chunk->used - memory_requirements.size;
    alloc.data = static_cast<uint8_t*>(chunk->data) + alloc.offset;
    return alloc;
}

void vk_ensure_staging_buffer_allocation(VkDeviceSize size) {
    if (vk.staging_buffer_size >= size)
        return;

    if (vk.staging_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(vk.device, vk.staging_buffer, nullptr);

    if (vk.staging_buffer_memory != VK_NULL_HANDLE)
        vkFreeMemory(vk.device, vk.staging_buffer_memory, nullptr);

    vk.staging_buffer_size = size;

    VkBufferCreateInfo buffer_desc;
    buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_desc.pNext = nullptr;
    buffer_desc.flags = 0;
    buffer_desc.size = size;
    buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_desc.queueFamilyIndexCount = 0;
    buffer_desc.pQueueFamilyIndices = nullptr;
    VK_CHECK(vkCreateBuffer(vk.device, &buffer_desc, nullptr, &vk.staging_buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(vk.device, vk.staging_buffer, &memory_requirements);

    uint32_t memory_type = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = memory_type;
    VK_CHECK(vkAllocateMemory(vk.device, &alloc_info, nullptr, &vk.staging_buffer_memory));
    VK_CHECK(vkBindBufferMemory(vk.device, vk.staging_buffer, vk.staging_buffer_memory, 0));

    void* data;
    VK_CHECK(vkMapMemory(vk.device, vk.staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
    vk.staging_buffer_ptr = (byte*)data;
}

static void create_instance() {
    const char* instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
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

    VkInstanceCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.pApplicationInfo = nullptr;
    desc.enabledLayerCount = 0;
    desc.ppEnabledLayerNames = nullptr;
    desc.enabledExtensionCount = sizeof(instance_extensions)/sizeof(instance_extensions[0]);
    desc.ppEnabledExtensionNames = instance_extensions;

    VK_CHECK(vkCreateInstance(&desc, nullptr, &vk.instance));
}

static void create_device() {
    // select physical device
    {
        uint32_t count;
        VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &count, nullptr));

        if (count == 0)
            error("Vulkan: no physical device found");

        std::vector<VkPhysicalDevice> physical_devices(count);
        VK_CHECK(vkEnumeratePhysicalDevices(vk.instance, &count, physical_devices.data()));
        vk.physical_device = physical_devices[0];
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

#define INIT_INSTANCE_FUNCTION(func) func = (PFN_ ## func)vkGetInstanceProcAddr(vk.instance, #func);
#define INIT_DEVICE_FUNCTION(func) func = (PFN_ ## func)vkGetDeviceProcAddr(vk.device, #func);

static void init_vulkan_library() {
    // Win32 Vulkan specific code
    vk.vulkan_library = LoadLibrary(L"vulkan-1.dll");
    if (vk.vulkan_library == NULL)
        error("Could not load vulkan dll");
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vk.vulkan_library, "vkGetInstanceProcAddr");

    //
    // Get functions that do not depend on VkInstance (vk.instance == nullptr at this point).
    //
    INIT_INSTANCE_FUNCTION(vkCreateInstance)
    INIT_INSTANCE_FUNCTION(vkEnumerateInstanceExtensionProperties)

    //
    // Get instance level functions.
    //
    create_instance();
    INIT_INSTANCE_FUNCTION(vkCreateDevice)
    INIT_INSTANCE_FUNCTION(vkDestroyInstance)
    INIT_INSTANCE_FUNCTION(vkEnumerateDeviceExtensionProperties)
    INIT_INSTANCE_FUNCTION(vkEnumeratePhysicalDevices)
    INIT_INSTANCE_FUNCTION(vkGetDeviceProcAddr)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceFormatProperties)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)
    INIT_INSTANCE_FUNCTION(vkCreateWin32SurfaceKHR)
    INIT_INSTANCE_FUNCTION(vkDestroySurfaceKHR)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR)
    INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)

    //
    // Get device level functions.
    //
    create_device();
    INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
    INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
    INIT_DEVICE_FUNCTION(vkAllocateMemory)
    INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
    INIT_DEVICE_FUNCTION(vkBindBufferMemory)
    INIT_DEVICE_FUNCTION(vkBindImageMemory)
    INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
    INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
    INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
    INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
    INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
    INIT_DEVICE_FUNCTION(vkCmdBlitImage)
    INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
    INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
    INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
    INIT_DEVICE_FUNCTION(vkCmdCopyImage)
    INIT_DEVICE_FUNCTION(vkCmdDraw)
    INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
    INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
    INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
    INIT_DEVICE_FUNCTION(vkCmdPushConstants)
    INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
    INIT_DEVICE_FUNCTION(vkCmdSetScissor)
    INIT_DEVICE_FUNCTION(vkCmdSetViewport)
    INIT_DEVICE_FUNCTION(vkCreateBuffer)
    INIT_DEVICE_FUNCTION(vkCreateCommandPool)
    INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
    INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
    INIT_DEVICE_FUNCTION(vkCreateFence)
    INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
    INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
    INIT_DEVICE_FUNCTION(vkCreateImage)
    INIT_DEVICE_FUNCTION(vkCreateImageView)
    INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
    INIT_DEVICE_FUNCTION(vkCreateRenderPass)
    INIT_DEVICE_FUNCTION(vkCreateSampler)
    INIT_DEVICE_FUNCTION(vkCreateSemaphore)
    INIT_DEVICE_FUNCTION(vkCreateShaderModule)
    INIT_DEVICE_FUNCTION(vkDestroyBuffer)
    INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
    INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
    INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
    INIT_DEVICE_FUNCTION(vkDestroyDevice)
    INIT_DEVICE_FUNCTION(vkDestroyFence)
    INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
    INIT_DEVICE_FUNCTION(vkDestroyImage)
    INIT_DEVICE_FUNCTION(vkDestroyImageView)
    INIT_DEVICE_FUNCTION(vkDestroyPipeline)
    INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
    INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
    INIT_DEVICE_FUNCTION(vkDestroySampler)
    INIT_DEVICE_FUNCTION(vkDestroySemaphore)
    INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
    INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
    INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
    INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
    INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
    INIT_DEVICE_FUNCTION(vkFreeMemory)
    INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
    INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
    INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
    INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
    INIT_DEVICE_FUNCTION(vkMapMemory)
    INIT_DEVICE_FUNCTION(vkQueueSubmit)
    INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
    INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
    INIT_DEVICE_FUNCTION(vkResetFences)
    INIT_DEVICE_FUNCTION(vkUnmapMemory)
    INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
    INIT_DEVICE_FUNCTION(vkWaitForFences)
    INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
    INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
    INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
    INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
    INIT_DEVICE_FUNCTION(vkQueuePresentKHR)
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION

static void deinit_vulkan_library() {
    vkCreateInstance                            = nullptr;
    vkEnumerateInstanceExtensionProperties		= nullptr;

    vkCreateDevice								= nullptr;
    vkDestroyInstance							= nullptr;
    vkEnumerateDeviceExtensionProperties		= nullptr;
    vkEnumeratePhysicalDevices					= nullptr;
    vkGetDeviceProcAddr							= nullptr;
    vkGetPhysicalDeviceFeatures					= nullptr;
    vkGetPhysicalDeviceFormatProperties			= nullptr;
    vkGetPhysicalDeviceMemoryProperties			= nullptr;
    vkGetPhysicalDeviceProperties				= nullptr;
    vkGetPhysicalDeviceQueueFamilyProperties	= nullptr;
    vkCreateWin32SurfaceKHR						= nullptr;
    vkDestroySurfaceKHR							= nullptr;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR	= nullptr;
    vkGetPhysicalDeviceSurfaceFormatsKHR		= nullptr;
    vkGetPhysicalDeviceSurfacePresentModesKHR	= nullptr;
    vkGetPhysicalDeviceSurfaceSupportKHR		= nullptr;

    vkAllocateCommandBuffers					= nullptr;
    vkAllocateDescriptorSets					= nullptr;
    vkAllocateMemory							= nullptr;
    vkBeginCommandBuffer						= nullptr;
    vkBindBufferMemory							= nullptr;
    vkBindImageMemory							= nullptr;
    vkCmdBeginRenderPass						= nullptr;
    vkCmdBindDescriptorSets						= nullptr;
    vkCmdBindIndexBuffer						= nullptr;
    vkCmdBindPipeline							= nullptr;
    vkCmdBindVertexBuffers						= nullptr;
    vkCmdBlitImage								= nullptr;
    vkCmdClearAttachments						= nullptr;
    vkCmdCopyBufferToImage						= nullptr;
    vkCmdCopyImage								= nullptr;
    vkCmdDraw									= nullptr;
    vkCmdDrawIndexed							= nullptr;
    vkCmdEndRenderPass							= nullptr;
    vkCmdPipelineBarrier						= nullptr;
    vkCmdPushConstants							= nullptr;
    vkCmdSetDepthBias							= nullptr;
    vkCmdSetScissor								= nullptr;
    vkCmdSetViewport							= nullptr;
    vkCreateBuffer								= nullptr;
    vkCreateCommandPool							= nullptr;
    vkCreateDescriptorPool						= nullptr;
    vkCreateDescriptorSetLayout					= nullptr;
    vkCreateFence								= nullptr;
    vkCreateFramebuffer							= nullptr;
    vkCreateGraphicsPipelines					= nullptr;
    vkCreateImage								= nullptr;
    vkCreateImageView							= nullptr;
    vkCreatePipelineLayout						= nullptr;
    vkCreateRenderPass							= nullptr;
    vkCreateSampler								= nullptr;
    vkCreateSemaphore							= nullptr;
    vkCreateShaderModule						= nullptr;
    vkDestroyBuffer								= nullptr;
    vkDestroyCommandPool						= nullptr;
    vkDestroyDescriptorPool						= nullptr;
    vkDestroyDescriptorSetLayout				= nullptr;
    vkDestroyDevice								= nullptr;
    vkDestroyFence								= nullptr;
    vkDestroyFramebuffer						= nullptr;
    vkDestroyImage								= nullptr;
    vkDestroyImageView							= nullptr;
    vkDestroyPipeline							= nullptr;
    vkDestroyPipelineLayout						= nullptr;
    vkDestroyRenderPass							= nullptr;
    vkDestroySampler							= nullptr;
    vkDestroySemaphore							= nullptr;
    vkDestroyShaderModule						= nullptr;
    vkDeviceWaitIdle							= nullptr;
    vkEndCommandBuffer							= nullptr;
    vkFreeCommandBuffers						= nullptr;
    vkFreeDescriptorSets						= nullptr;
    vkFreeMemory								= nullptr;
    vkGetBufferMemoryRequirements				= nullptr;
    vkGetDeviceQueue							= nullptr;
    vkGetImageMemoryRequirements				= nullptr;
    vkGetImageSubresourceLayout					= nullptr;
    vkMapMemory									= nullptr;
    vkQueueSubmit								= nullptr;
    vkQueueWaitIdle								= nullptr;
    vkResetDescriptorPool						= nullptr;
    vkResetFences								= nullptr;
    vkUnmapMemory                               = nullptr;
    vkUpdateDescriptorSets						= nullptr;
    vkWaitForFences								= nullptr;
    vkAcquireNextImageKHR						= nullptr;
    vkCreateSwapchainKHR						= nullptr;
    vkDestroySwapchainKHR						= nullptr;
    vkGetSwapchainImagesKHR						= nullptr;
    vkQueuePresentKHR							= nullptr;
}

VkPipeline create_pipeline(const Vk_Pipeline_Def&);

void vk_initialize(const SDL_SysWMinfo& window_info) {
    vk.system_window_info = window_info;

    init_vulkan_library();

    vkGetDeviceQueue(vk.device, vk.queue_family_index, 0, &vk.queue);

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
            VkImageCreateInfo desc;
            desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            desc.pNext = nullptr;
            desc.flags = 0;
            desc.imageType = VK_IMAGE_TYPE_2D;
            desc.format = vk.depth_image_format;
            desc.extent.width = vk.surface_width;
            desc.extent.height = vk.surface_height;
            desc.extent.depth = 1;
            desc.mipLevels = 1;
            desc.arrayLayers = 1;
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.tiling = VK_IMAGE_TILING_OPTIMAL;
            desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            desc.queueFamilyIndexCount = 0;
            desc.pQueueFamilyIndices = nullptr;
            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VK_CHECK(vkCreateImage(vk.device, &desc, nullptr, &vk.depth_image));
        }

        // allocate depth image memory
        {
            VkMemoryRequirements memory_requirements;
            vkGetImageMemoryRequirements(vk.device, vk.depth_image, &memory_requirements);

            VkMemoryAllocateInfo alloc_info;
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.pNext = nullptr;
            alloc_info.allocationSize = memory_requirements.size;
            alloc_info.memoryTypeIndex = find_memory_type(vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            VK_CHECK(vkAllocateMemory(vk.device, &alloc_info, nullptr, &vk.depth_image_memory));
            VK_CHECK(vkBindImageMemory(vk.device, vk.depth_image, vk.depth_image_memory, 0));
        }

        // create depth image view
        {
            VkImageViewCreateInfo desc;
            desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            desc.pNext = nullptr;
            desc.flags = 0;
            desc.image = vk.depth_image;
            desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
            desc.format = vk.depth_image_format;
            desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            desc.subresourceRange.baseMipLevel = 0;
            desc.subresourceRange.levelCount = 1;
            desc.subresourceRange.baseArrayLayer = 0;
            desc.subresourceRange.layerCount = 1;
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
        vkDestroyBuffer(vk.device, vk.staging_buffer, nullptr);

    if (vk.staging_buffer_memory != VK_NULL_HANDLE)
        vkFreeMemory(vk.device, vk.staging_buffer_memory, nullptr);

    for (auto pipeline : vk.pipelines) {
        vkDestroyPipeline(vk.device, pipeline, nullptr);
    }
    vk.pipeline_defs.clear();
    vk.pipelines.clear();

    for (const auto& chunk : vk.device_local_chunks) {
        vkFreeMemory(vk.device, chunk.memory, nullptr);
    }
    vk.device_local_chunks.clear();

    for (const auto& chunk : vk.host_visible_chunks) {
        vkFreeMemory(vk.device, chunk.memory, nullptr);
    }
    vk.host_visible_chunks.clear();

    vkDestroyImage(vk.device, vk.depth_image, nullptr);
    vkFreeMemory(vk.device, vk.depth_image_memory, nullptr);
    vkDestroyImageView(vk.device, vk.depth_image_view, nullptr);

    vkDestroyCommandPool(vk.device, vk.command_pool, nullptr);

    for (auto image_view : vk.swapchain_image_views) {
        vkDestroyImageView(vk.device, image_view, nullptr);
    }
    vk.swapchain_images.clear();

    vkDestroySemaphore(vk.device, vk.image_acquired, nullptr);
    vkDestroySemaphore(vk.device, vk.rendering_finished, nullptr);
    vkDestroyFence(vk.device, vk.rendering_finished_fence, nullptr);

    vkDestroySwapchainKHR(vk.device, vk.swapchain, nullptr);
    vkDestroyDevice(vk.device, nullptr);
    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    vkDestroyInstance(vk.instance, nullptr);

    deinit_vulkan_library();
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

VkBuffer vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.size = size;
    desc.usage = usage;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = nullptr;

    VkBuffer buffer = get_resource_manager()->create_buffer(desc);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(vk.device, buffer, &memory_requirements);
    auto alloc = allocate_memory(memory_requirements, false);
    VK_CHECK(vkBindBufferMemory(vk.device, buffer, alloc.memory, alloc.offset));

    return buffer;
}

VkBuffer vk_create_host_visible_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** buffer_ptr) {
    VkBufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.size = size;
    desc.usage = usage;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = nullptr;

    VkBuffer buffer = get_resource_manager()->create_buffer(desc);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(vk.device, buffer, &memory_requirements);
    auto alloc = allocate_memory(memory_requirements, true);
    VK_CHECK(vkBindBufferMemory(vk.device, buffer, alloc.memory, alloc.offset));
    *buffer_ptr = alloc.data;

    return buffer;
}


Vk_Image vk_create_texture(int width, int height, VkFormat format, int mip_levels, const uint8_t* pixels, int bytes_per_pixel) {
    Vk_Image image;

    // create image
    {
        VkImageCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.imageType = VK_IMAGE_TYPE_2D;
        desc.format = format;
        desc.extent.width = width;
        desc.extent.height = height;
        desc.extent.depth = 1;
        desc.mipLevels = mip_levels;
        desc.arrayLayers = 1;
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        desc.tiling = VK_IMAGE_TILING_OPTIMAL;
        desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        desc.queueFamilyIndexCount = 0;
        desc.pQueueFamilyIndices = nullptr;
        desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        image.handle = get_resource_manager()->create_image(desc);

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(vk.device, image.handle, &memory_requirements);
        auto alloc = allocate_memory(memory_requirements, false);
        VK_CHECK(vkBindImageMemory(vk.device, image.handle, alloc.memory, alloc.offset));
    }

    // create image view
    {
        VkImageViewCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.image = image.handle;
        desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
        desc.format = format;
        desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.subresourceRange.baseMipLevel = 0;
        desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        desc.subresourceRange.baseArrayLayer = 0;
        desc.subresourceRange.layerCount = 1;

        image.view = get_resource_manager()->create_image_view(desc);
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

Vk_Image vk_create_render_target(int width, int height, VkFormat format) {
    Vk_Image image;

    // create image
    {
        VkImageCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.imageType = VK_IMAGE_TYPE_2D;
        desc.format = format;
        desc.extent.width = width;
        desc.extent.height = height;
        desc.extent.depth = 1;
        desc.mipLevels = 1;
        desc.arrayLayers = 1;
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        desc.tiling = VK_IMAGE_TILING_OPTIMAL;
        desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        desc.queueFamilyIndexCount = 0;
        desc.pQueueFamilyIndices = nullptr;
        desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        image.handle = get_resource_manager()->create_image(desc);

        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(vk.device, image.handle, &memory_requirements);
        auto alloc = allocate_memory(memory_requirements, false);
        VK_CHECK(vkBindImageMemory(vk.device, image.handle, alloc.memory, alloc.offset));
    }
    // create image view
    {
        VkImageViewCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;
        desc.image = image.handle;
        desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
        desc.format = format;
        desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.subresourceRange.baseMipLevel = 0;
        desc.subresourceRange.levelCount = 1;
        desc.subresourceRange.baseArrayLayer = 0;
        desc.subresourceRange.layerCount = 1;

        image.view = get_resource_manager()->create_image_view(desc);
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
        get_shader_stage_desc(VK_SHADER_STAGE_VERTEX_BIT, def.vs_module, "main"),
        get_shader_stage_desc(VK_SHADER_STAGE_FRAGMENT_BIT, def.fs_module, "main")
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
    VK_CHECK(vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.image_acquired, VK_NULL_HANDLE, &vk.swapchain_image_index));

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
    VK_CHECK(vkQueueSubmit(vk.queue, 1, &submit_info, vk.rendering_finished_fence));

    VkPresentInfoKHR present_info;
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &vk.rendering_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vk.swapchain;
    present_info.pImageIndices = &vk.swapchain_image_index;
    present_info.pResults = nullptr;
    VK_CHECK(vkQueuePresentKHR(vk.queue, &present_info));
}
