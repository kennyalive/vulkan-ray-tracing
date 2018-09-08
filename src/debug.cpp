#include "vk.h"

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

void vk_create_debug_utils_messenger() {
    VkDebugUtilsMessengerCreateInfoEXT desc{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

    desc.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    desc.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    desc.pfnUserCallback = &debug_utils_messenger_callback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(vk.instance, &desc, nullptr, &vk.debug_utils_messenger));
}

void vk_destroy_debug_utils_messenger() {
    vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.debug_utils_messenger, nullptr);
}
