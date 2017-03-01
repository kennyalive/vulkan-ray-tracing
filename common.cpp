#include <fstream>
#include <vector>
#include "common.h"

namespace
{
std::vector<char> ReadBinaryFileContents(const std::string& fileName)
{
    std::ifstream stream(fileName, std::ios_base::binary | std::ios_base::ate);
    auto fileSize = stream.tellg();
    stream.seekg(0, std::ios_base::beg);

    std::vector<char> buffer(fileSize);
    stream.read(buffer.data(), fileSize);

    if (!stream)
        throw VulkanException("failed to read file contents: " + fileName);

    return buffer;
}
} // namespace

ShaderModule::ShaderModule(VkDevice device, const std::string& spirvFileName)
: device(device)
{
    auto data = ReadBinaryFileContents(spirvFileName);

    if (data.size() % 4 != 0)
        Error("SPIR-V binary file size is not multiple of 4: " + spirvFileName);

    VkShaderModuleCreateInfo createInfo;
    createInfo.sType =VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.codeSize = data.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(data.data());

    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
    CheckVkResult(result, "vkCreateShaderModule");
}

ShaderModule::~ShaderModule()
{
    vkDestroyShaderModule(device, shaderModule, nullptr);
}

VkShaderModule ShaderModule::GetHandle() const
{
    return shaderModule;
}

void CheckVkResult(VkResult result, const std::string& functionName)
{
    if (result < 0)
    {
        throw VulkanException(functionName + 
            " has returned error code with value " + std::to_string(result));
    }
}

void Error(const std::string& message)
{
    throw VulkanException(message);
}
