# vulkan-base

* Simple vulkan application with all dependencies included.
* The application uses __volk__ library to initialize Vulkan entry points and __Vulkan Memory Allocator (VMA)__ library for memory management.
* __Dear ImGui__ immediate mode UI library is integrated.
* Command line options:
  - _--validation-layers_ - enables VK_LAYER_LUNARG_standard_validation layer. VulkanSDK should be installed in order to use this option.
  - _--debug-names_ - enables debug naming of vulkan objects.

![vulkan-demo](https://user-images.githubusercontent.com/4964024/28161150-168fe846-67cb-11e7-973f-c844e3c398b3.jpg)
