#include "common.h"
#include "vulkan_utilities.h"

static uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties) {
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

Shared_Staging_Memory::Shared_Staging_Memory(VkPhysicalDevice physical_device, VkDevice device)
    : physical_device(physical_device)
    , device(device) {}

Shared_Staging_Memory::~Shared_Staging_Memory() {
    if (handle != VK_NULL_HANDLE) {
        vkFreeMemory(device, handle, nullptr);
    }
}

void Shared_Staging_Memory::ensure_allocation_for_object(VkImage image) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device, image, &memory_requirements);
    ensure_allocation(memory_requirements);
}

void Shared_Staging_Memory::ensure_allocation_for_object(VkBuffer buffer) {
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
    ensure_allocation(memory_requirements);
}

VkDeviceMemory Shared_Staging_Memory::get_handle() const {
    return handle;
}

void Shared_Staging_Memory::ensure_allocation(const VkMemoryRequirements& memory_requirements) {
    uint32_t required_memory_type_index = find_memory_type(physical_device, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (size < memory_requirements.size || memory_type_index != required_memory_type_index) {
        if (handle != VK_NULL_HANDLE) {
            vkFreeMemory(device, handle, nullptr);
        }
        handle = VK_NULL_HANDLE;
        size = 0;
        memory_type_index = -1;

        VkMemoryAllocateInfo alloc_info;
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.pNext = nullptr;
        alloc_info.allocationSize = memory_requirements.size;
        alloc_info.memoryTypeIndex = required_memory_type_index;

        VkResult result = vkAllocateMemory(device, &alloc_info, nullptr, &handle);
        check_vk_result(result, "vkAllocateMemory");
        size = memory_requirements.size;
        memory_type_index = required_memory_type_index;
    }
}

Device_Memory_Allocator::Device_Memory_Allocator(VkPhysicalDevice physical_device, VkDevice device)
    : physical_device(physical_device)
    , device(device)
    , shared_staging_memory(physical_device, device) {}

Device_Memory_Allocator::~Device_Memory_Allocator() {
    for (auto chunk : chunks)
        vkFreeMemory(device, chunk, nullptr);
}

VkDeviceMemory Device_Memory_Allocator::allocate_memory(VkImage image) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device, image, &memory_requirements);
    return allocate_memory(memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

VkDeviceMemory Device_Memory_Allocator::allocate_memory(VkBuffer buffer) {
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
    return allocate_memory(memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

VkDeviceMemory Device_Memory_Allocator::allocate_staging_memory(VkImage image) {
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device, image, &memory_requirements);
    return allocate_memory(memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

VkDeviceMemory Device_Memory_Allocator::allocate_staging_memory(VkBuffer buffer) {
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
    return allocate_memory(memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

Shared_Staging_Memory& Device_Memory_Allocator::get_shared_staging_memory() {
    return shared_staging_memory;
}

VkDeviceMemory Device_Memory_Allocator::allocate_memory(const VkMemoryRequirements& memory_requirements, VkMemoryPropertyFlags properties) {
    VkMemoryAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, memory_requirements.memoryTypeBits, properties);

    VkDeviceMemory chunk;
    VkResult result = vkAllocateMemory(device, &alloc_info, nullptr, &chunk);
    check_vk_result(result, "vkAllocateMemory");
    chunks.push_back(chunk);
    return chunk;
}

void record_and_run_commands(VkDevice device, VkCommandPool command_pool, VkQueue queue,
    std::function<void(VkCommandBuffer)> recorder) {

    VkCommandBufferAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    VkResult result = vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);
    check_vk_result(result, "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;

    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_vk_result(result, "vkBeginCommandBuffer");
    recorder(command_buffer);
    result = vkEndCommandBuffer(command_buffer);
    check_vk_result(result, "vkEndCommandBuffer");

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

    result = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    check_vk_result(result, "vkQueueSubmit");
    result = vkQueueWaitIdle(queue);
    check_vk_result(result, "vkQueueWaitIdle");
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

static bool has_depth_component(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

static bool has_stencil_component(VkFormat format) {
    switch (format) {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

void record_image_layout_transition(VkCommandBuffer command_buffer, VkImage image, VkFormat format,
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

    bool depth = has_depth_component(format);
    bool stencil = has_stencil_component(format);
    if (depth && stencil)
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else if (depth)
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if (stencil)
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    else
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

VkImage create_texture(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator) {
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

    VkDeviceMemory memory = allocator.allocate_memory(image);
    result = vkBindImageMemory(device, image, memory, 0);
    check_vk_result(result, "vkBindImageMemory");
    return image;
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

    allocator.get_shared_staging_memory().ensure_allocation_for_object(image);
    VkDeviceMemory memory = allocator.get_shared_staging_memory().get_handle();
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

VkImage create_depth_attachment_image(VkDevice device, int image_width, int image_height, VkFormat format, Device_Memory_Allocator& allocator) {
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
    create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    VkResult result = vkCreateImage(device, &create_info, nullptr, &image);
    check_vk_result(result, "vkCreateImage");

    VkDeviceMemory memory = allocator.allocate_memory(image);
    result = vkBindImageMemory(device, image, memory, 0);
    check_vk_result(result, "vkBindImageMemory");
    return image;
}

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.image = image;
    desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
    desc.format = format;
    desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    desc.subresourceRange.aspectMask = aspect_flags;
    desc.subresourceRange.baseMipLevel = 0;
    desc.subresourceRange.levelCount = 1;
    desc.subresourceRange.baseArrayLayer = 0;
    desc.subresourceRange.layerCount = 1;

    VkImageView image_view;
    VkResult result = vkCreateImageView(device, &desc, nullptr, &image_view);
    check_vk_result(result, "vkCreateImageView");
    return image_view;
}

VkBuffer create_buffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, Device_Memory_Allocator& allocator) {
    VkBufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.size = size;
    desc.usage = usage;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = nullptr;

    VkBuffer buffer;
    VkResult result = vkCreateBuffer(device, &desc, nullptr, &buffer);
    check_vk_result(result, "vkCreateBuffer");

    VkDeviceMemory memory = allocator.allocate_memory(buffer);
    result = vkBindBufferMemory(device, buffer, memory, 0);
    check_vk_result(result, "vkBindBufferMemory");
    return buffer;
}

VkBuffer create_staging_buffer(VkDevice device, VkDeviceSize size, Device_Memory_Allocator& allocator, const void* data) {
    VkBufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.size = size;
    desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = nullptr;

    VkBuffer buffer;
    VkResult result = vkCreateBuffer(device, &desc, nullptr, &buffer);
    check_vk_result(result, "vkCreateBuffer");

    allocator.get_shared_staging_memory().ensure_allocation_for_object(buffer);
    VkDeviceMemory memory = allocator.get_shared_staging_memory().get_handle();
    result = vkBindBufferMemory(device, buffer, memory, 0);
    check_vk_result(result, "vkBindBufferMemory");

    void* buffer_data;
    result = vkMapMemory(device, memory, 0, size, 0, &buffer_data);
    check_vk_result(result, "vkMapMemory");
    memcpy(buffer_data, data, size);
    vkUnmapMemory(device, memory);
    return buffer;
}

VkBuffer create_permanent_staging_buffer(VkDevice device, VkDeviceSize size, Device_Memory_Allocator& allocator, VkDeviceMemory& memory) {
    VkBufferCreateInfo desc;
    desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.pNext = nullptr;
    desc.flags = 0;
    desc.size = size;
    desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    desc.queueFamilyIndexCount = 0;
    desc.pQueueFamilyIndices = nullptr;

    VkBuffer buffer;
    VkResult result = vkCreateBuffer(device, &desc, nullptr, &buffer);
    check_vk_result(result, "vkCreateBuffer");

    memory = allocator.allocate_staging_memory(buffer);
    result = vkBindBufferMemory(device, buffer, memory, 0);
    check_vk_result(result, "vkBindBufferMemory");
    return buffer;
}
