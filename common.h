#pragma once

#include <functional>
#include <exception>
#include <string>
#include "vulkan_definitions.h"

class Scope_Exit_Action {
public:
    Scope_Exit_Action(std::function<void()> action)
        : action(action) {}

    ~Scope_Exit_Action() {
        action();
    }

private:
    std::function<void()> action;
};

class VulkanException : public std::exception
{
public:
    VulkanException(const std::string& errorMessage)
    : errorMessage(errorMessage)
    {}

    const char* what() const override
    {
        return errorMessage.c_str();
    }

private:
    std::string errorMessage;
};

class ShaderModule
{
public:
    ShaderModule(VkDevice device, const std::string& spirvFileName);
    ~ShaderModule();
    VkShaderModule GetHandle() const;

private:
    VkDevice device;
    VkShaderModule shaderModule;
};

class PipelineLayout
{
public:
    PipelineLayout(VkDevice device, VkPipelineLayoutCreateInfo createInfo);
    ~PipelineLayout();
    VkPipelineLayout GetHandle() const;

private:
    VkDevice device;
    VkPipelineLayout pipelineLayout;
};

void CheckVkResult(VkResult result, const std::string& functionName);
inline void check_vk_result(VkResult result, const std::string& functionName) { CheckVkResult(result, functionName); }

void Error(const std::string& message);
inline void error(const std::string& message) { Error(message); }

