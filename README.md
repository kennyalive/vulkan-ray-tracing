# vulkan-base

* Simple vulkan application that renders textured model.
* VK_NVX_raytracing support. Ray differentials for texture filtering in raytracing mode.
* The application uses __volk__ library to initialize Vulkan entry points and __Vulkan Memory Allocator (VMA)__ library for memory management.
* __Dear ImGui__ immediate mode UI library is integrated.
* Command line options:
  - _--validation-layers_ - enables VK_LAYER_LUNARG_standard_validation layer. VulkanSDK should be installed in order to use this option.
  - _--debug-names_ - enables debug naming of vulkan objects.

Prerequisites: VulkanSDK is required to build the solution.

![demo](https://user-images.githubusercontent.com/4964024/48605463-26722a00-e97d-11e8-9548-65de42d50c21.png)
