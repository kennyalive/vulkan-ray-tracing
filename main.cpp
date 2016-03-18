#include <iostream>
#include <string>
#include "vulkan/vulkan.h"

void CheckResult(VkResult result, const std::string& message)
{
  if (result != VK_SUCCESS)
  {
    std::cout << message << std::endl;
    exit(1);
  }
}

int main()
{
  // create vulkan instance
  VkInstanceCreateInfo instanceCreateInfo;
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = nullptr;
  instanceCreateInfo.flags = 0;
  instanceCreateInfo.pApplicationInfo = nullptr;
  instanceCreateInfo.enabledLayerCount = 0;
  instanceCreateInfo.ppEnabledLayerNames = nullptr;
  instanceCreateInfo.enabledExtensionCount = 0;
  instanceCreateInfo.ppEnabledExtensionNames = 0;

  VkInstance instance;
  VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
  CheckResult(result, "failed to create vulkan instance");

  std::cout << "Vulkan instance created\n";

  // enumerate physical devices
  uint32_t physicalDevicesCount;
  result = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, nullptr);
  CheckResult(result, "failed to enumerate physical devices");

  VkPhysicalDevice physicalDevice;

  // destroy vulkan instance
  vkDestroyInstance(instance, nullptr);
  return 0;
}
