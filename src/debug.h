#pragma once

#include "vk.h"

void vk_create_debug_utils_messenger();
void vk_destroy_debug_utils_messenger();


template <typename Vk_Object_Type>
void vk_set_debug_name(Vk_Object_Type object, const char* name) {
    if (!vk.create_info.use_debug_names)
        return;

    VkDebugUtilsObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    /*char buf[128];
    snprintf(buf, sizeof(buf), "%s 0x%llx", name, (uint64_t)object);*/
    name_info.objectHandle = (uint64_t)object;
    name_info.pObjectName = name;

#define IF_TYPE_THEN_ENUM(vk_type, vk_object_type_enum) \
    if constexpr (std::is_same<Vk_Object_Type, vk_type>::value) name_info.objectType = vk_object_type_enum;

         IF_TYPE_THEN_ENUM(VkInstance,                  VK_OBJECT_TYPE_INSTANCE                     )
    else IF_TYPE_THEN_ENUM(VkPhysicalDevice,            VK_OBJECT_TYPE_PHYSICAL_DEVICE              )
    else IF_TYPE_THEN_ENUM(VkDevice,                    VK_OBJECT_TYPE_DEVICE                       )
    else IF_TYPE_THEN_ENUM(VkQueue,                     VK_OBJECT_TYPE_QUEUE                        )
    else IF_TYPE_THEN_ENUM(VkSemaphore,                 VK_OBJECT_TYPE_SEMAPHORE                    )
    else IF_TYPE_THEN_ENUM(VkCommandBuffer,             VK_OBJECT_TYPE_COMMAND_BUFFER               )
    else IF_TYPE_THEN_ENUM(VkFence,                     VK_OBJECT_TYPE_FENCE                        )
    else IF_TYPE_THEN_ENUM(VkDeviceMemory,              VK_OBJECT_TYPE_DEVICE_MEMORY                )
    else IF_TYPE_THEN_ENUM(VkBuffer,                    VK_OBJECT_TYPE_BUFFER                       )
    else IF_TYPE_THEN_ENUM(VkImage,                     VK_OBJECT_TYPE_IMAGE                        )
    else IF_TYPE_THEN_ENUM(VkEvent,                     VK_OBJECT_TYPE_EVENT                        )
    else IF_TYPE_THEN_ENUM(VkQueryPool,                 VK_OBJECT_TYPE_QUERY_POOL                   )
    else IF_TYPE_THEN_ENUM(VkBufferView,                VK_OBJECT_TYPE_BUFFER_VIEW                  )
    else IF_TYPE_THEN_ENUM(VkImageView,                 VK_OBJECT_TYPE_IMAGE_VIEW                   )
    else IF_TYPE_THEN_ENUM(VkShaderModule,              VK_OBJECT_TYPE_SHADER_MODULE                )
    else IF_TYPE_THEN_ENUM(VkPipelineCache,             VK_OBJECT_TYPE_PIPELINE_CACHE               )
    else IF_TYPE_THEN_ENUM(VkPipelineLayout,            VK_OBJECT_TYPE_PIPELINE_LAYOUT              )
    else IF_TYPE_THEN_ENUM(VkRenderPass,                VK_OBJECT_TYPE_RENDER_PASS                  )
    else IF_TYPE_THEN_ENUM(VkPipeline,                  VK_OBJECT_TYPE_PIPELINE                     )
    else IF_TYPE_THEN_ENUM(VkDescriptorSetLayout,       VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT        )
    else IF_TYPE_THEN_ENUM(VkSampler,                   VK_OBJECT_TYPE_SAMPLER                      )
    else IF_TYPE_THEN_ENUM(VkDescriptorPool,            VK_OBJECT_TYPE_DESCRIPTOR_POOL              )
    else IF_TYPE_THEN_ENUM(VkDescriptorSet,             VK_OBJECT_TYPE_DESCRIPTOR_SET               )
    else IF_TYPE_THEN_ENUM(VkFramebuffer,               VK_OBJECT_TYPE_FRAMEBUFFER                  )
    else IF_TYPE_THEN_ENUM(VkCommandPool,               VK_OBJECT_TYPE_COMMAND_POOL                 )
    else IF_TYPE_THEN_ENUM(VkDescriptorUpdateTemplate,  VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE   )
    else IF_TYPE_THEN_ENUM(VkSurfaceKHR,                VK_OBJECT_TYPE_SURFACE_KHR                  )
    else IF_TYPE_THEN_ENUM(VkSwapchainKHR,              VK_OBJECT_TYPE_SWAPCHAIN_KHR                )
    else IF_TYPE_THEN_ENUM(VkDebugUtilsMessengerEXT,    VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT    )
    else static_assert(false, "Unknown Vulkan object type");
#undef IF_TYPE_THEN_ENUM

    VK_CHECK(vkSetDebugUtilsObjectNameEXT(vk.device, &name_info));
}
