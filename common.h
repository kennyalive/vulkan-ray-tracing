#pragma once

#include <exception>
#include <string>
#include "vulkan_definitions.h"

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
void Error(const std::string& message);
