#pragma once

#include <vector>
#include "vulkan_definitions.h"

VkInstance CreateInstance();

VkPhysicalDevice SelectPhysicalDevice(VkInstance instance);

bool SelectQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    uint32_t& graphicsQueueFamilyIndex, uint32_t& presentationQueueFamilyIndex);

struct QueueInfo
{
    uint32_t queueFamilyIndex = -1;
    uint32_t queueCount = 0;

    QueueInfo(uint32_t queueFamilyIndex, uint32_t queueCount)
        : queueFamilyIndex(queueFamilyIndex)
        , queueCount(queueCount)
    {}
};

VkDevice CreateDevice(VkPhysicalDevice physicalDevice, const std::vector<QueueInfo>& queueInfos);
