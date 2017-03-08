#pragma once

#include "vulkan_utilities.h"
#include <vector>

class Resource_Manager {
public:
    void initialize(VkDevice device) {
        this->device = device;
    }

    void release_resources() {
        for (auto command_pool : command_pools) {
            vkDestroyCommandPool(device, command_pool, nullptr);
        }
        command_pools.clear();

        for (auto semaphore : semaphores) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
        semaphores.clear();

        for (auto render_pass : render_passes) {
            vkDestroyRenderPass(device, render_pass, nullptr);
        }
        render_passes.clear();
    }

    VkCommandPool create_command_pool(const VkCommandPoolCreateInfo& desc) {
        VkCommandPool command_pool;
        VkResult result = vkCreateCommandPool(device, &desc, nullptr, &command_pool);
        check_vk_result(result, "vkCreateCommandPool");
        command_pools.push_back(command_pool);
        return command_pool;
    }

    VkSemaphore create_semaphore() {
        VkSemaphoreCreateInfo desc;
        desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        desc.pNext = nullptr;
        desc.flags = 0;

        VkSemaphore semaphore;
        VkResult result = vkCreateSemaphore(device, &desc, nullptr, &semaphore);
        check_vk_result(result, "vkCreateSemaphore");
        semaphores.push_back(semaphore);
        return semaphore;
    }

    VkRenderPass create_render_pass(const VkRenderPassCreateInfo& desc) {
        VkRenderPass render_pass;
        VkResult result = vkCreateRenderPass(device, &desc, nullptr, &render_pass);
        check_vk_result(result, "vkCreateRenderPass");
        render_passes.push_back(render_pass);
        return render_pass;
    }

private:
    VkDevice device = VK_NULL_HANDLE;
    std::vector<VkCommandPool> command_pools;
    std::vector<VkSemaphore> semaphores;
    std::vector<VkRenderPass> render_passes;
};
