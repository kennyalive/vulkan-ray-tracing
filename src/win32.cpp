#include "common.h"
#include "platform.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw/glfw3.h"
#include "glfw/glfw3native.h"

namespace platform
{
VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow* window) {
    VkWin32SurfaceCreateInfoKHR desc{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    desc.hinstance = ::GetModuleHandle(nullptr);
    desc.hwnd = glfwGetWin32Window(window);
    VkSurfaceKHR surface;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &desc, nullptr, &surface));
    return surface;
}

void sleep(int milliseconds) {
    ::Sleep(milliseconds);
}

} // namespace platform
