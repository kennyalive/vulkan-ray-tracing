#include <iostream>
#include <string>
#include <vector>
#include "vulkan/vulkan.h"

void Error(const std::string& message)
{
  std::cout << message << std::endl;
  exit(1);
}

void CheckResult(VkResult result, const std::string& message)
{
  if (result != VK_SUCCESS)
    Error(message);
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

  // select physical device
  uint32_t physicalDevicesCount;
  result = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, nullptr);
  CheckResult(result, "failed to get physical devices count");

  std::vector<VkPhysicalDevice> allPhysicalDevices(physicalDevicesCount);
  result = vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, allPhysicalDevices.data());
  allPhysicalDevices.resize(physicalDevicesCount);
  CheckResult(result, "failed to enumerate physical devices");

  if (physicalDevicesCount == 0)
    Error("no physical devices found");

  VkPhysicalDevice physicalDevice = allPhysicalDevices[0]; // get the first one

  // determine index of the queue family that supports graphics operations
  uint32_t queueFamiliesCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);

  std::vector<VkQueueFamilyProperties> allQueueFamilies(queueFamiliesCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, allQueueFamilies.data());
  allQueueFamilies.resize(queueFamiliesCount);

  uint32_t queueFamilyIndex;
  for (queueFamilyIndex = 0; queueFamilyIndex < queueFamiliesCount; queueFamilyIndex++)
  {
    if ((allQueueFamilies[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
      break;
  }
  if (queueFamilyIndex == queueFamiliesCount)
    Error("failed to choose queue family index");

  // create device
  const float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueCreateInfo;
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.pNext = nullptr;
  queueCreateInfo.flags = 0;
  queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  VkDeviceCreateInfo deviceCreateInfo;
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.pNext = nullptr;
  deviceCreateInfo.flags = 0;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  deviceCreateInfo.enabledLayerCount = 0;
  deviceCreateInfo.ppEnabledLayerNames = nullptr;
  deviceCreateInfo.enabledExtensionCount = 0;
  deviceCreateInfo.ppEnabledExtensionNames = nullptr;
  deviceCreateInfo.pEnabledFeatures = nullptr;

  VkDevice device;
  result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
  CheckResult(result, "failed to create device");
  std::cout << "Device created\n";

  // get queue
  VkQueue queue;
  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

  // destroy device
  result = vkDeviceWaitIdle(device);
  CheckResult(result, "failed waiting for device idle state");

  vkDestroyDevice(device, nullptr);
  device = VK_NULL_HANDLE;
  std::cout << "Device destroyed\n";

  // destroy vulkan instance
  vkDestroyInstance(instance, nullptr);
  instance = VK_NULL_HANDLE;
  std::cout << "Vulkan instance destroyed\n";
  return 0;
}
