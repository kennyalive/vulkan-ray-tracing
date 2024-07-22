#include "demo.h"
#include "glfw/glfw3.h"
#include <cassert>
#include <cstring>

static bool parse_command_line(int argc, char** argv) {
    bool found_unknown_option = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data-dir") == 0) {
            if (i == argc - 1) {
                printf("--data-dir value is missing\n");
            }
            else {
                extern std::string g_data_dir;
                g_data_dir = argv[i + 1];
                i++;
            }
        }
        else if (strcmp(argv[i], "--help") == 0) {
            printf("%-25s Path to the data directory. Default is ./data.\n", "--data-dir");
            printf("%-25s Shows this information.\n", "--help");
            return false;
        }
        else
            found_unknown_option = true;
    }
    if (found_unknown_option)
        printf("Use --help to list all options.\n");
    return true;
}

static int window_width = 720;
static int window_height = 720;

static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_F11 || (key == GLFW_KEY_ENTER && mods == GLFW_MOD_ALT)) {
            static int last_window_xpos, last_window_ypos;
            static int last_window_width, last_window_height;

            VK_CHECK(vkDeviceWaitIdle(vk.device));
            GLFWmonitor* monitor = glfwGetWindowMonitor(window);
            if (monitor == nullptr) {
                glfwGetWindowPos(window, &last_window_xpos, &last_window_ypos);
                last_window_width = window_width;
                last_window_height = window_height;

                monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, nullptr, last_window_xpos, last_window_ypos, last_window_width, last_window_height, 0);
            }
        }
    }
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW error: %s\n", description);
}

int main(int argc, char** argv) {
    if (!parse_command_line(argc, argv)) {
        return 0;
    }
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        error("glfwInit failed");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* glfw_window = glfwCreateWindow(window_width, window_height, "Vulkan demo", nullptr, nullptr);
    assert(glfw_window != nullptr);
    glfwSetKeyCallback(glfw_window, glfw_key_callback);

    Vk_Demo demo{};
    demo.initialize(glfw_window);

    bool prev_vsync = demo.vsync_enabled();

    bool window_active = true;

    while (!glfwWindowShouldClose(glfw_window)) {
        if (window_active)
            demo.run_frame();

        glfwPollEvents();

        int width, height;
        glfwGetWindowSize(glfw_window, &width, &height);

        bool recreate_swapchain = false;
        if (prev_vsync != demo.vsync_enabled()) {
            prev_vsync = demo.vsync_enabled();
            recreate_swapchain = true;
        } else if (width != window_width || height != window_height) {
            window_width = width;
            window_height = height;
            recreate_swapchain = true;
        }

        window_active = (width != 0 && height != 0);

        if (!window_active)
            continue; 

        if (recreate_swapchain) {
            VK_CHECK(vkDeviceWaitIdle(vk.device));
            demo.release_resolution_dependent_resources();
            vk_destroy_swapchain();
            vk_create_swapchain(demo.vsync_enabled());
            demo.restore_resolution_dependent_resources();
            recreate_swapchain = false;
        }
    }

    demo.shutdown();
    glfwTerminate();
    return 0;
}
