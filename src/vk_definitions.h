#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"
