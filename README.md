# 🌋 vulkan-ray-tracing 🖖

This is a basic Vulkan ray tracing demo based on __VK_KHR_ray_tracing_pipeline__ + __VK_KHR_acceleration_structure__ extensions. It shows how to setup a ray tracing pipeline and also provides an example of how to use ray differentials for texture filtering.

Prerequisites:
* CMake
* Vulkan SDK  (VULKAN_SDK environment variable should be set)

Build steps: 
1. `cmake -S . -B build`
2. `cmake --build build`

Supported platforms: Windows, Linux.

In order to enable Vulkan validation layers specify ```--validation-layers``` command line argument.

![demo](https://user-images.githubusercontent.com/4964024/48605463-26722a00-e97d-11e8-9548-65de42d50c21.png)
