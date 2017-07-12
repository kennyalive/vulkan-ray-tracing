#pragma once

#include "vk_definitions.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL_syswm.h"

#include <functional>
#include <string>
#include <vector>

void error(const std::string& message);
void set_window_title(const std::string&);

#define VK_CHECK(function_call) { \
    VkResult result = function_call; \
    if (result < 0) \
        error("Vulkan: error code " + std::to_string(result) + " returned by " + #function_call); \
}

struct Vk_Pipeline_Def {
    VkShaderModule vs_module = VK_NULL_HANDLE;
    VkShaderModule fs_module = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    bool operator==(const Vk_Pipeline_Def& other) const {
        return vs_module == other.vs_module &&
               fs_module == other.fs_module &&
               render_pass == other.render_pass &&
               pipeline_layout == other.pipeline_layout;

    }
};

struct Vk_Image {
    VkImage handle = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize(const SDL_SysWMinfo& window_info);

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown();

//
// Resources allocation.
//
void vk_ensure_staging_buffer_allocation(VkDeviceSize size);
VkBuffer vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage);
VkBuffer vk_create_host_visible_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** buffer_ptr);
Vk_Image vk_create_texture(int width, int height, VkFormat format, int mip_levels, const uint8_t* pixels, int bytes_per_pixel);
Vk_Image vk_create_render_target(int width, int height, VkFormat format);
VkPipeline vk_find_pipeline(const Vk_Pipeline_Def& def);

//
// Rendering setup.
//
void vk_begin_frame();
void vk_end_frame();

void vk_record_and_run_commands(VkCommandPool command_pool, VkQueue queue, std::function<void(VkCommandBuffer)> recorder);

void vk_record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
                                     VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
                                     VkAccessFlags src_access, VkAccessFlags dst_access);

// Vk_Instance contains vulkan resources that do not depend on applicaton logic.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
struct Vk_Instance {
    SDL_SysWMinfo system_window_info;
    HMODULE vulkan_library = NULL;
    int surface_width = 0;
    int surface_height = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surface_format = {};

    uint32_t queue_family_index = 0;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    uint32_t swapchain_image_index = -1; // current swapchain image

    VkSemaphore image_acquired = VK_NULL_HANDLE;
    VkSemaphore rendering_finished = VK_NULL_HANDLE;
    VkFence rendering_finished_fence = VK_NULL_HANDLE;

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    VkImage depth_image = VK_NULL_HANDLE;
    VkDeviceMemory depth_image_memory = VK_NULL_HANDLE;
    VkFormat depth_image_format = VK_FORMAT_UNDEFINED;
    VkImageView depth_image_view = VK_NULL_HANDLE;

    std::vector<Vk_Pipeline_Def> pipeline_defs;
    std::vector<VkPipeline> pipelines;

    // Host visible memory used to copy image data to device local memory.
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
    VkDeviceSize staging_buffer_size = 0;
    uint8_t* staging_buffer_ptr = nullptr; // pointer to mapped staging buffer

    //
    // Memory allocation.
    //
    struct Chunk {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize used = 0;
        uint32_t memory_type_index = -1;
        void* data = nullptr; // only for host visible memory
    };
    std::vector<Chunk> device_local_chunks;
    std::vector<Chunk> host_visible_chunks;
};

extern Vk_Instance vk;

extern PFN_vkGetInstanceProcAddr                        vkGetInstanceProcAddr;

extern PFN_vkCreateInstance                             vkCreateInstance;
extern PFN_vkEnumerateInstanceExtensionProperties       vkEnumerateInstanceExtensionProperties;

extern PFN_vkCreateDevice                               vkCreateDevice;
extern PFN_vkDestroyInstance                            vkDestroyInstance;
extern PFN_vkEnumerateDeviceExtensionProperties         vkEnumerateDeviceExtensionProperties;
extern PFN_vkEnumeratePhysicalDevices                   vkEnumeratePhysicalDevices;
extern PFN_vkGetDeviceProcAddr                          vkGetDeviceProcAddr;
extern PFN_vkGetPhysicalDeviceFeatures                  vkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceFormatProperties          vkGetPhysicalDeviceFormatProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties          vkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetPhysicalDeviceProperties                vkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties     vkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkCreateWin32SurfaceKHR                      vkCreateWin32SurfaceKHR;
extern PFN_vkDestroySurfaceKHR                          vkDestroySurfaceKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR    vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR         vkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR    vkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR         vkGetPhysicalDeviceSurfaceSupportKHR;

extern PFN_vkAllocateCommandBuffers                     vkAllocateCommandBuffers;
extern PFN_vkAllocateDescriptorSets                     vkAllocateDescriptorSets;
extern PFN_vkAllocateMemory                             vkAllocateMemory;
extern PFN_vkBeginCommandBuffer                         vkBeginCommandBuffer;
extern PFN_vkBindBufferMemory                           vkBindBufferMemory;
extern PFN_vkBindImageMemory                            vkBindImageMemory;
extern PFN_vkCmdBeginRenderPass                         vkCmdBeginRenderPass;
extern PFN_vkCmdBindDescriptorSets                      vkCmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer                         vkCmdBindIndexBuffer;
extern PFN_vkCmdBindPipeline                            vkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers                       vkCmdBindVertexBuffers;
extern PFN_vkCmdBlitImage                               vkCmdBlitImage;
extern PFN_vkCmdClearAttachments                        vkCmdClearAttachments;
extern PFN_vkCmdCopyBufferToImage                       vkCmdCopyBufferToImage;
extern PFN_vkCmdCopyImage                               vkCmdCopyImage;
extern PFN_vkCmdCopyBuffer                              vkCmdCopyBuffer;
extern PFN_vkCmdDraw                                    vkCmdDraw;
extern PFN_vkCmdDrawIndexed                             vkCmdDrawIndexed;
extern PFN_vkCmdEndRenderPass                           vkCmdEndRenderPass;
extern PFN_vkCmdPipelineBarrier                         vkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants                           vkCmdPushConstants;
extern PFN_vkCmdSetDepthBias                            vkCmdSetDepthBias;
extern PFN_vkCmdSetScissor                              vkCmdSetScissor;
extern PFN_vkCmdSetViewport                             vkCmdSetViewport;
extern PFN_vkCreateBuffer                               vkCreateBuffer;
extern PFN_vkCreateCommandPool                          vkCreateCommandPool;
extern PFN_vkCreateDescriptorPool                       vkCreateDescriptorPool;
extern PFN_vkCreateDescriptorSetLayout                  vkCreateDescriptorSetLayout;
extern PFN_vkCreateFence                                vkCreateFence;
extern PFN_vkCreateFramebuffer                          vkCreateFramebuffer;
extern PFN_vkCreateGraphicsPipelines                    vkCreateGraphicsPipelines;
extern PFN_vkCreateImage                                vkCreateImage;
extern PFN_vkCreateImageView                            vkCreateImageView;
extern PFN_vkCreatePipelineLayout                       vkCreatePipelineLayout;
extern PFN_vkCreateRenderPass                           vkCreateRenderPass;
extern PFN_vkCreateSampler                              vkCreateSampler;
extern PFN_vkCreateSemaphore                            vkCreateSemaphore;
extern PFN_vkCreateShaderModule                         vkCreateShaderModule;
extern PFN_vkDestroyBuffer                              vkDestroyBuffer;
extern PFN_vkDestroyCommandPool                         vkDestroyCommandPool;
extern PFN_vkDestroyDescriptorPool                      vkDestroyDescriptorPool;
extern PFN_vkDestroyDescriptorSetLayout                 vkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDevice                              vkDestroyDevice;
extern PFN_vkDestroyFence                               vkDestroyFence;
extern PFN_vkDestroyFramebuffer                         vkDestroyFramebuffer;
extern PFN_vkDestroyImage                               vkDestroyImage;
extern PFN_vkDestroyImageView                           vkDestroyImageView;
extern PFN_vkDestroyPipeline                            vkDestroyPipeline;
extern PFN_vkDestroyPipelineLayout                      vkDestroyPipelineLayout;
extern PFN_vkDestroyRenderPass                          vkDestroyRenderPass;
extern PFN_vkDestroySampler                             vkDestroySampler;
extern PFN_vkDestroySemaphore                           vkDestroySemaphore;
extern PFN_vkDestroyShaderModule                        vkDestroyShaderModule;
extern PFN_vkDeviceWaitIdle                             vkDeviceWaitIdle;
extern PFN_vkEndCommandBuffer                           vkEndCommandBuffer;
extern PFN_vkFreeCommandBuffers                         vkFreeCommandBuffers;
extern PFN_vkFreeDescriptorSets                         vkFreeDescriptorSets;
extern PFN_vkFreeMemory                                 vkFreeMemory;
extern PFN_vkGetBufferMemoryRequirements                vkGetBufferMemoryRequirements;
extern PFN_vkGetDeviceQueue                             vkGetDeviceQueue;
extern PFN_vkGetImageMemoryRequirements                 vkGetImageMemoryRequirements;
extern PFN_vkGetImageSubresourceLayout                  vkGetImageSubresourceLayout;
extern PFN_vkMapMemory                                  vkMapMemory;
extern PFN_vkQueueSubmit                                vkQueueSubmit;
extern PFN_vkQueueWaitIdle                              vkQueueWaitIdle;
extern PFN_vkResetDescriptorPool                        vkResetDescriptorPool;
extern PFN_vkResetFences                                vkResetFences;
extern PFN_vkUnmapMemory                                vkUnmapMemory;
extern PFN_vkUpdateDescriptorSets                       vkUpdateDescriptorSets;
extern PFN_vkWaitForFences                              vkWaitForFences;
extern PFN_vkAcquireNextImageKHR                        vkAcquireNextImageKHR;
extern PFN_vkCreateSwapchainKHR                         vkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR                        vkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR                      vkGetSwapchainImagesKHR;
extern PFN_vkQueuePresentKHR                            vkQueuePresentKHR;
