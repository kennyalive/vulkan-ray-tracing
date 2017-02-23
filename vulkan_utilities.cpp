#include "common.h"
#include "vulkan_utilities.h"

Device_Memory_Allocator::Device_Memory_Allocator(VkPhysicalDevice physical_device, VkDevice device)
    : physical_device(physical_device)
    , device(device) {}

Device_Memory_Allocator::~Device_Memory_Allocator() {
    for (const auto& chunk : device_local_chunks)
        vkFreeMemory(device, chunk, nullptr);

    if (staging_chunk != VK_NULL_HANDLE)
        vkFreeMemory(device, staging_chunk, nullptr);
}

VkDeviceMemory Device_Memory_Allocator::allocate_staging_memory(VkImage image) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device, image, &memory_requirements);

    uint32_t memory_type_index = find_memory_type_with_properties(physical_device, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (staging_chunk_size < memory_requirements.size || staging_memory_type_index != memory_type_index) {
        if (staging_chunk != VK_NULL_HANDLE) {
            vkFreeMemory(device, staging_chunk, nullptr);
        }
        staging_chunk = VK_NULL_HANDLE;
        staging_chunk_size = 0;
        staging_memory_type_index = -1;

        VkMemoryAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.pNext = nullptr;
        alloc_info.allocationSize = memory_requirements.size;
        alloc_info.memoryTypeIndex = memory_type_index;

        VkResult result = vkAllocateMemory(device, &alloc_info, nullptr, &staging_chunk);
        check_vk_result(result, "vkAllocateMemory");
        staging_chunk_size = memory_requirements.size;
        staging_memory_type_index = memory_type_index;
    }
    return staging_chunk;
}

VkDeviceMemory Device_Memory_Allocator::allocate_device_local_memory(VkImage image) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device, image, &memory_requirements);

    uint32_t memory_type_index = find_memory_type_with_properties(physical_device, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = memory_type_index;

    VkDeviceMemory chunk;
    VkResult result = vkAllocateMemory(device, &alloc_info, nullptr, &chunk);
    check_vk_result(result, "vkAllocateMemory");
    device_local_chunks.push_back(chunk);
    return chunk;
}

uint32_t find_memory_type_with_properties(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((memory_type_bits & (1 << i)) != 0 &&
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    error("failed to find matching memory type with requested properties");
    return -1;
}

VkImage create_staging_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator,
    const uint8_t* pixels, int bytes_per_pixel) {

    VkImageCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.imageType = VK_IMAGE_TYPE_2D;
    create_info.format = format;
    create_info.extent.width = image_width;
    create_info.extent.height = image_height;
    create_info.extent.depth = 1;
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.tiling = VK_IMAGE_TILING_LINEAR;
    create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
    create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    VkImage image;
    VkResult result = vkCreateImage(device, &create_info, nullptr, &image);
    check_vk_result(result, "vkCreateImage");

    VkDeviceMemory memory = allocator.allocate_staging_memory(image);
    result = vkBindImageMemory(device, image, memory, 0);
    check_vk_result(result, "vkBindImageMemory");

    VkImageSubresource staging_image_subresource;
    staging_image_subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    staging_image_subresource.mipLevel = 0;
    staging_image_subresource.arrayLayer = 0;

    VkSubresourceLayout staging_image_layout;
    vkGetImageSubresourceLayout(device, image, &staging_image_subresource, &staging_image_layout);

    void* data;
    result = vkMapMemory(device, memory, 0, staging_image_layout.size, 0, &data);
    check_vk_result(result, "vkMapMemory");

    const int bytes_per_row = image_width * bytes_per_pixel;
    if (staging_image_layout.rowPitch == bytes_per_row) {
        memcpy(data, pixels, bytes_per_row * image_height);
    } else {
        auto bytes = static_cast<uint8_t*>(data);
        for (int i = 0; i < image_height; i++) {
            memcpy(&bytes[i * staging_image_layout.rowPitch], &pixels[i * bytes_per_row], bytes_per_row);
        }
    }
    vkUnmapMemory(device, memory);
    return image;
}

VkImage create_device_local_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator) {
    VkImageCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.imageType = VK_IMAGE_TYPE_2D;
    create_info.format = format;
    create_info.extent.width = image_width;
    create_info.extent.height = image_height;
    create_info.extent.depth = 1;
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    VkResult result = vkCreateImage(device, &create_info, nullptr, &image);
    check_vk_result(result, "vkCreateImage");

    VkDeviceMemory memory = allocator.allocate_device_local_memory(image);
    result = vkBindImageMemory(device, image, memory, 0);
    check_vk_result(result, "vkBindImageMemory");
    return image;
}
