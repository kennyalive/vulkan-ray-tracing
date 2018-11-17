#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif
#include "volk/volk.h"
#include "vk_enum_string_helper.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

#include "sdl/SDL_syswm.h"

#include <functional>
#include <string>
#include <vector>

#define VK_CHECK_RESULT(result) if (result < 0) error(std::string("Error: ") + string_VkResult(result));
#define VK_CHECK(function_call) { VkResult result = function_call;  VK_CHECK_RESULT(result); }

struct Vk_Create_Info {
    SDL_SysWMinfo               windowing_system_info;

    bool                        enable_validation_layers;
    bool                        use_debug_names;
};

struct Vk_Image {
    VkImage         handle;
    VkImageView     view;
    VmaAllocation   allocation;
    void destroy();
};

struct Vk_Buffer {
    VkBuffer        handle;
    VmaAllocation   allocation;
    void destroy();
};

struct Vk_Graphics_Pipeline_State {
    VkVertexInputBindingDescription         vertex_bindings[8];
    uint32_t                                vertex_binding_count;
    VkVertexInputAttributeDescription       vertex_attributes[16];
    uint32_t                                vertex_attribute_count;
    VkPipelineInputAssemblyStateCreateInfo  input_assembly_state;
    VkPipelineViewportStateCreateInfo       viewport_state;
    VkPipelineRasterizationStateCreateInfo  rasterization_state;
    VkPipelineMultisampleStateCreateInfo    multisample_state;
    VkPipelineDepthStencilStateCreateInfo   depth_stencil_state;
    VkPipelineColorBlendAttachmentState     attachment_blend_state[4];
    UINT32                                  attachment_blend_state_count;
    VkDynamicState                          dynamic_state[8];
    uint32_t                                dynamic_state_count;
};

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize(const Vk_Create_Info& create_info);

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown();

void vk_release_resolution_dependent_resources();
void vk_restore_resolution_dependent_resources(bool vsync);

void vk_ensure_staging_buffer_allocation(VkDeviceSize size);
Vk_Buffer vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, const char* name);
Vk_Buffer vk_create_host_visible_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** buffer_ptr, const char* name);
Vk_Image vk_create_texture(int width, int height, VkFormat format, bool generate_mipmaps, const uint8_t* pixels, int bytes_per_pixel, const char*  name);
Vk_Image vk_create_image(int width, int height, VkFormat format, VkImageCreateFlags usage_flags, const char* name);
Vk_Image vk_load_texture(const std::string& texture_file);
VkShaderModule vk_load_spirv(const std::string& spirv_file);

Vk_Graphics_Pipeline_State get_default_graphics_pipeline_state();

VkPipeline vk_create_graphics_pipeline(
    const Vk_Graphics_Pipeline_State&   state,
    VkPipelineLayout                    pipeline_layout,
    VkRenderPass                        render_pass,
    VkShaderModule                      vertex_shader,
    VkShaderModule                      fragment_shader
);


void vk_begin_frame();
void vk_end_frame();

void vk_execute(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder);

// Barrier for all subresources of non-depth image.
void vk_cmd_image_barrier(
    VkCommandBuffer command_buffer, VkImage image,
    VkPipelineStageFlags    src_stage_mask,     VkPipelineStageFlags    dst_stage_mask,
    VkAccessFlags           src_access_mask,    VkAccessFlags           dst_access_mask,
    VkImageLayout           old_layout,         VkImageLayout           new_layout
);

// General image barrier.
void vk_cmd_image_barrier_for_subresource(
    VkCommandBuffer command_buffer, VkImage image, const VkImageSubresourceRange& subresource_range,
    VkPipelineStageFlags    src_stage_mask,     VkPipelineStageFlags    dst_stage_mask,
    VkAccessFlags           src_access_mask,    VkAccessFlags           dst_access_mask,
    VkImageLayout           old_layout,         VkImageLayout           new_layout
);

uint32_t vk_allocate_timestamp_queries(uint32_t count);

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
    else IF_TYPE_THEN_ENUM(VkAccelerationStructureNVX,  VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NVX   )
    else static_assert(false, "Unknown Vulkan object type");
#undef IF_TYPE_THEN_ENUM

    VK_CHECK(vkSetDebugUtilsObjectNameEXT(vk.device, &name_info));
}

struct Swapchain_Info {
    VkSwapchainKHR           handle;
    std::vector<VkImage>     images;
    std::vector<VkImageView> image_views;
};

struct Depth_Buffer_Info {
    VkImage                 image;
    VkImageView             image_view;
    VmaAllocation           allocation;
    VkFormat                format;
};

// Vk_Instance contains vulkan resources that do not depend on applicaton logic.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
struct Vk_Instance {
    Vk_Create_Info                  create_info;

    VkInstance                      instance;
    VkPhysicalDevice                physical_device;
    uint32_t                        queue_family_index;
    VkDevice                        device;
    VkQueue                         queue;
    double                          timestamp_period_ms;
    bool                            raytracing_supported;

    VmaAllocator                    allocator;

    VkSurfaceKHR                    surface;
    VkSurfaceFormatKHR              surface_format;
    VkExtent2D                      surface_size;
    Swapchain_Info                  swapchain_info;

    uint32_t                        swapchain_image_index = -1; // current swapchain image

    VkCommandPool                   command_pool;
    VkCommandBuffer                 command_buffer;

    VkDescriptorPool                descriptor_pool;

    VkSemaphore                     image_acquired;
    VkSemaphore                     rendering_finished;
    VkFence                         rendering_finished_fence;

    VkQueryPool                     timestamp_query_pool;
    uint32_t                        timestamp_query_count;

    // Host visible memory used to copy image data to device local memory.
    VkBuffer                        staging_buffer;
    VmaAllocation                   staging_buffer_allocation;
    VkDeviceSize                    staging_buffer_size;
    uint8_t*                        staging_buffer_ptr; // pointer to mapped staging buffer

    Depth_Buffer_Info               depth_info;
    VkDebugUtilsMessengerEXT        debug_utils_messenger;
};

extern Vk_Instance vk;
