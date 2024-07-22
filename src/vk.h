#pragma once

#ifdef _WIN32
#define NOMINMAX
#endif

#include "volk/volk.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vma/vk_mem_alloc.h"

#include <functional>
#include <span>
#include <string>
#include <vector>

const char* vk_result_to_string(VkResult result);
#define VK_CHECK_RESULT(result) if (result < 0 && vk.error) vk.error(std::string("Error: ") + vk_result_to_string(result));
#define VK_CHECK(function_call) { VkResult result = function_call;  VK_CHECK_RESULT(result); }

using Vk_Error_Func = void (*)(const std::string& error_message);

struct Vk_Init_Params {
    Vk_Error_Func error_reporter = nullptr;
    int physical_device_index = -1;
    bool vsync = false;
    std::span<const char*> instance_extensions;
    std::span<const char*> device_extensions;
    const VkBaseInStructure* device_create_info_pnext = nullptr;
    std::span<VkFormat> supported_surface_formats;
    VkImageUsageFlags surface_usage_flags = 0;
};

struct Vk_Image {
    VkImage handle = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    void destroy();
};

struct Vk_Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceAddress device_address = 0;
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
    uint32_t                                attachment_blend_state_count;
    VkDynamicState                          dynamic_state[8];
    uint32_t                                dynamic_state_count;
    VkFormat                                color_attachment_formats[4];
    uint32_t                                color_attachment_count;
    VkFormat                                depth_attachment_format;
};

struct GLFWwindow;

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize(GLFWwindow* window, const Vk_Init_Params& init_params);

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown();

// vk_initialize/vk_shutdown call these functions. Should be called manually to recreate swapchain if needed.
void vk_create_swapchain(bool vsync);
void vk_destroy_swapchain();

void vk_ensure_staging_buffer_allocation(VkDeviceSize size);

// Buffers
Vk_Buffer vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
    const void* data = nullptr, const char* name = nullptr);
Vk_Buffer vk_create_buffer_with_alignment(VkDeviceSize size, VkBufferUsageFlags usage, uint32_t min_alignment,
    const void* data = nullptr, const char* name = nullptr);
Vk_Buffer vk_create_mapped_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
    void** buffer_ptr, const char* name = nullptr);

// Images
Vk_Image vk_create_image(int width, int height, VkFormat format, VkImageUsageFlags usage_flags, const char* name);
Vk_Image vk_create_texture(int width, int height, VkFormat format, bool generate_mipmaps, const uint8_t* pixels, int bytes_per_pixel, const char*  name);
Vk_Image vk_load_texture(const std::string& texture_file);

VkShaderModule vk_load_spirv(const std::string& spirv_file);

VkPipelineLayout vk_create_pipeline_layout(
    std::initializer_list<VkDescriptorSetLayout> set_layouts,
    std::initializer_list<VkPushConstantRange> push_constant_ranges,
    const char* name);

Vk_Graphics_Pipeline_State get_default_graphics_pipeline_state();

VkPipeline vk_create_graphics_pipeline(const Vk_Graphics_Pipeline_State& state,
    VkShaderModule vertex_shader, VkShaderModule fragment_shader,
    VkPipelineLayout pipeline_layout, const char* name);

VkPipeline vk_create_compute_pipeline(VkShaderModule compute_shader,
    VkPipelineLayout pipeline_layout, const char* name);

void vk_begin_frame();
void vk_end_frame();

void vk_execute(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder);

// Barrier for all subresources of non-depth image.
void vk_cmd_image_barrier(VkCommandBuffer command_buffer, VkImage image,
    VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkImageLayout old_layout,
    VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkImageLayout new_layout);

// General image barrier.
void vk_cmd_image_barrier_for_subresource(VkCommandBuffer command_buffer, VkImage image, const VkImageSubresourceRange& subresource_range,
    VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkImageLayout old_layout,
    VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkImageLayout new_layout);


uint32_t vk_allocate_timestamp_queries(uint32_t count);

// Workaround for static_assert(false). It should be used like this: static_assert(dependent_false_v<T>)
template<typename>
inline constexpr bool dependent_false_v = false;

template <typename Vk_Object_Type>
void vk_set_debug_name(Vk_Object_Type object, const char* name)
{
    VkObjectType object_type;

#define IF_TYPE_THEN_ENUM(vk_type, vk_object_type_enum) \
    if constexpr (std::is_same<Vk_Object_Type, vk_type>::value) object_type = vk_object_type_enum;

    IF_TYPE_THEN_ENUM(VkInstance,                       VK_OBJECT_TYPE_INSTANCE                     )
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
    else IF_TYPE_THEN_ENUM(VkAccelerationStructureKHR,  VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR   )
    else IF_TYPE_THEN_ENUM(VkDebugUtilsMessengerEXT,    VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT    )
    else static_assert(dependent_false_v<Vk_Object_Type>, "Unknown Vulkan object type");
#undef IF_TYPE_THEN_ENUM

    void set_debug_name_impl(VkObjectType object_type, uint64_t object_handle, const char* name);
    set_debug_name_impl(object_type, (uint64_t)object, name);
}

struct Swapchain_Info {
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
};

// Vk_Instance contains vulkan resources that do not depend on applicaton logic.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
struct Vk_Instance {
    Vk_Error_Func                   error = nullptr;
    VkInstance                      instance;
    VkPhysicalDevice                physical_device;
    uint32_t                        queue_family_index;
    VkDevice                        device;
    VkQueue                         queue;
    double                          timestamp_period_ms;

    VmaAllocator                    allocator;

    VkSurfaceKHR                    surface;
    VkImageUsageFlags               surface_usage_flags;
    VkSurfaceFormatKHR              surface_format;
    VkExtent2D                      surface_size;
    Swapchain_Info                  swapchain_info;

    uint32_t                        swapchain_image_index = -1; // current swapchain image

    VkCommandPool                   command_pools[2];
    VkCommandBuffer                 command_buffers[2];
    VkCommandBuffer                 command_buffer; // command_buffers[frame_index]
    int                             frame_index;

    VkSemaphore                     image_acquired_semaphore[2];
    VkSemaphore                     rendering_finished_semaphore[2];
    VkFence                         frame_fence[2];

    VkQueryPool                     timestamp_query_pools[2];
    VkQueryPool                     timestamp_query_pool; // timestamp_query_pool[frame_index]
    uint32_t                        timestamp_query_count;

    // Host visible memory used to copy image data to device local memory.
    VkBuffer                        staging_buffer;
    VmaAllocation                   staging_buffer_allocation;
    VkDeviceSize                    staging_buffer_size;
    uint8_t*                        staging_buffer_ptr; // pointer to mapped staging buffer

    VkDebugUtilsMessengerEXT        debug_utils_messenger;

    VkDescriptorPool                imgui_descriptor_pool;
};

extern Vk_Instance vk;

//*****************************************************************************
//
// Misc Vulkan utilities section
//
//*****************************************************************************
struct Vk_PNexer {
    VkBaseOutStructure* tail = nullptr;
    template <typename TVkStruct>
    Vk_PNexer(TVkStruct& vk_struct)
    {
        tail = reinterpret_cast<VkBaseOutStructure*>(&vk_struct);
    }
    template <typename TVkStruct>
    void next(TVkStruct& vk_struct)
    {
        tail->pNext = reinterpret_cast<VkBaseOutStructure*>(&vk_struct);
        tail = tail->pNext;
    }
};

struct Vk_Shader_Module {
    Vk_Shader_Module(const std::string& spirv_file);
    ~Vk_Shader_Module();
    VkShaderModule handle;
};

struct Vk_Descriptor_Set_Layout {
    static constexpr uint32_t max_bindings = 32;
    VkDescriptorSetLayoutBinding bindings[max_bindings];
    uint32_t binding_count = 0;

    Vk_Descriptor_Set_Layout& sampled_image(uint32_t binding, VkShaderStageFlags stage_flags);
    Vk_Descriptor_Set_Layout& sampled_image_array(uint32_t binding, uint32_t array_size, VkShaderStageFlags stage_flags);
    Vk_Descriptor_Set_Layout& storage_image(uint32_t binding, VkShaderStageFlags stage_flags);
    Vk_Descriptor_Set_Layout& sampler(uint32_t binding, VkShaderStageFlags stage_flags);
    Vk_Descriptor_Set_Layout& uniform_buffer(uint32_t binding, VkShaderStageFlags stage_flags);
    Vk_Descriptor_Set_Layout& storage_buffer(uint32_t binding, VkShaderStageFlags stage_flags);
    Vk_Descriptor_Set_Layout& storage_buffer_array(uint32_t binding, uint32_t array_size, VkShaderStageFlags stage_flags);
    Vk_Descriptor_Set_Layout& accelerator(uint32_t binding, VkShaderStageFlags stage_flags);
    VkDescriptorSetLayout create(const char* name);
};

//
// GPU time queries.
//
struct Vk_GPU_Time_Interval {
    uint32_t start_query[2]; // end query == (start_query[frame_index] + 1)
    float length_ms;

    void begin();
    void end();
};

struct Vk_GPU_Time_Keeper {
    static constexpr uint32_t max_time_intervals = 128;

    Vk_GPU_Time_Interval time_intervals[max_time_intervals];
    uint32_t time_interval_count;

    Vk_GPU_Time_Interval* allocate_time_interval();
    void initialize_time_intervals();
    void next_frame();
};

struct Vk_GPU_Time_Scope {
    Vk_GPU_Time_Scope(Vk_GPU_Time_Interval* time_interval) {
        this->time_interval = time_interval;
        time_interval->begin();
    }
    ~Vk_GPU_Time_Scope() {
        time_interval->end();
    }

private:
    Vk_GPU_Time_Interval* time_interval;
};

#define VK_GPU_TIME_SCOPE(time_interval) Vk_GPU_Time_Scope gpu_time_scope##__LINE__(time_interval)

//
// GPU debug markers.
//
void vk_begin_gpu_marker_scope(VkCommandBuffer command_buffer, const char* name);
void vk_end_gpu_marker_scope(VkCommandBuffer command_buffer);
void vk_write_gpu_marker(VkCommandBuffer command_buffer, const char* name);

struct Vk_GPU_Marker_Scope {
    Vk_GPU_Marker_Scope(VkCommandBuffer command_buffer, const char* name) {
        this->command_buffer = command_buffer;
        vk_begin_gpu_marker_scope(command_buffer, name);
    }
    ~Vk_GPU_Marker_Scope() {
        vk_end_gpu_marker_scope(command_buffer);
    }

private:
    VkCommandBuffer command_buffer;
};

#define VK_GPU_MARKER_SCOPE(command_buffer, name) GPU_Marker_Scope gpu_marker_scope##__LINE__(command_buffer, name)
