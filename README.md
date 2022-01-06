# 🌋 vulkan-raytracing 🖖

* "Simple" Vulkan raytracing demo based on __VK_KHR_ray_tracing_pipeline__ + __VK_KHR_acceleration_structure__ extensions.
* Highlights: setup of ray tracing pipeline, example of how to use ray differentials for texture filtering.

Prerequisites:
* Microsoft Visual Studio  
<sub>VS 2017 solution is provided. Fow never versions of VS you might need to confirm toolset upgrade request when opening the solution for the first time. The project was tested in VS 2017/VS 2019/VS 2022.</sub>
* Vulkan SDK  
<sub>Should be installed locally (installation also sets VULKAN_SDK environment variable which is accessed by custom build step). We use shader compiler from Vulkan SDK distribution to compile the shaders.</sub>

Build steps: 

1. Open solution in Visual Studio IDE.
2. Press F7 (Build Solution). That's all. All dependencies are included.

![demo](https://user-images.githubusercontent.com/4964024/48605463-26722a00-e97d-11e8-9548-65de42d50c21.png)

_"About 128KB of source code is a reasonable upper limit for the complexity of explicit graphics API initialization problems now envisioned."_
