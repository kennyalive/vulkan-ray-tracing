set(THIRD_PARTY_SOURCE
    # glfw
    glfw/context.c
    glfw/egl_context.c
    glfw/glx_context.c
    glfw/init.c
    glfw/input.c
    glfw/linux_joystick.c
    glfw/monitor.c
    glfw/null_init.c
    glfw/null_joystick.c
    glfw/null_monitor.c
    glfw/null_window.c
    glfw/osmesa_context.c
    glfw/platform.c
    glfw/posix_module.c
    glfw/posix_poll.c
    glfw/posix_thread.c
    glfw/posix_time.c
    glfw/vulkan.c
    glfw/wgl_context.c
    glfw/win32_init.c
    glfw/win32_joystick.c
    glfw/win32_module.c
    glfw/win32_monitor.c
    glfw/win32_thread.c
    glfw/win32_time.c
    glfw/win32_window.c
    glfw/window.c
    glfw/x11_init.c
    glfw/x11_monitor.c
    glfw/x11_window.c
    glfw/xkb_unicode.c
    # imgui
    imgui/imconfig.h
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_glfw.h
    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_impl_vulkan.h
    # volk
    volk/volk.c
    volk/volk.h
)
source_group(TREE "${CMAKE_SOURCE_DIR}/third-party" FILES ${THIRD_PARTY_SOURCE})

add_library(third-party ${THIRD_PARTY_SOURCE})
target_compile_features(third-party PRIVATE cxx_std_17)
target_include_directories(third-party PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(third-party PUBLIC _GLFW_WIN32 VK_USE_PLATFORM_WIN32_KHR IMGUI_IMPL_VULKAN_USE_VOLK)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(third-party PUBLIC _GLFW_X11 VK_USE_PLATFORM_XCB_KHR IMGUI_IMPL_VULKAN_USE_VOLK)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(third-party PRIVATE
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-missing-field-initializers
    )
    target_compile_options(third-party INTERFACE
        -Wno-unused-variable
        -Wno-parentheses
        -Wno-shift-negative-value
        -Wno-implicit-fallthrough
    )
endif()
