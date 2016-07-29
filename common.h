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

inline void CheckVkResult(VkResult result, const std::string& functionName)
{
    if (result < 0)
    {
        throw VulkanException(functionName + 
            " has returned error code with value " + std::to_string(result));
    }
}

inline void Error(const std::string& message)
{
    throw VulkanException(message);
}
