#include "demo.h"

#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

#include "imgui/imgui.h"
#include "imgui/impl/imgui_impl_sdl.h"

static SDL_Window* the_window   = nullptr;

static bool toogle_fullscreen   = false;
static bool handle_resize       = false;

static void parse_command_line(int argc, char** argv, Demo_Create_Info& demo_create_info) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--validation-layers") == 0) {
            demo_create_info.vk_create_info.enable_validation_layers = true;
        }
        if (strcmp(argv[i], "--debug-names") == 0) {
            demo_create_info.vk_create_info.use_debug_names = true;
        }
    }
}

static bool process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
            return false;

        if (event.type == SDL_KEYDOWN) {
            SDL_Scancode scancode = event.key.keysym.scancode;

            if (scancode == SDL_SCANCODE_ESCAPE)
                return false;

            if (scancode == SDL_SCANCODE_F11 || scancode == SDL_SCANCODE_RETURN && (SDL_GetModState() & KMOD_LALT) != 0)
                toogle_fullscreen = true;
        }

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
            handle_resize = true;
    }
    return true;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        error("SDL_Init error");

    struct On_Exit {~On_Exit() { SDL_Quit(); }} exit_action;

    // Create window.
    the_window = SDL_CreateWindow("Vulkan demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 720, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (the_window == nullptr)
        error("failed to create SDL window");

    SDL_SysWMinfo windowing_system_info;
    SDL_VERSION(&windowing_system_info.version);
    if (SDL_GetWindowWMInfo(the_window, &windowing_system_info) == SDL_FALSE)
        error("failed to get platform specific window information");

    // Initialize demo.
    Demo_Create_Info demo_info{};
    demo_info.vk_create_info.windowing_system_info = windowing_system_info;
    demo_info.window = the_window;
    parse_command_line(argc, argv, demo_info);
    Vk_Demo demo(demo_info);

    // Run main loop.
    while (process_events()) {
        if (toogle_fullscreen) {
            demo.run_frame(true); // draw only background during fullscreen toggle to prevent image stretching

            if (SDL_GetWindowFlags(the_window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
                SDL_SetWindowFullscreen(the_window, 0);
            else
                SDL_SetWindowFullscreen(the_window, SDL_WINDOW_FULLSCREEN_DESKTOP);

            process_events();
            toogle_fullscreen = false;
        }

        if (handle_resize) {
            if (vk.swapchain_info.handle != VK_NULL_HANDLE) {
                demo.release_resolution_dependent_resources();
            }
            handle_resize = false;
        }

        if ((SDL_GetWindowFlags(the_window) & SDL_WINDOW_MINIMIZED) == 0) {
            if (vk.swapchain_info.handle == VK_NULL_HANDLE) {
                demo.restore_resolution_dependent_resources();
            }
            demo.run_frame();
        }
        SDL_Delay(1);
    }
    return 0;
}
