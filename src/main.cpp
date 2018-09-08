#include "demo.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

#include "imgui/imgui.h"
#include "imgui/impl/imgui_impl_sdl.h"

static SDL_Window* the_window   = nullptr;

static bool toogle_fullscreen   = false;
static bool handle_resize       = false;

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

    the_window = SDL_CreateWindow("Vulkan demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 720, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (the_window == nullptr)
        error("failed to create SDL window");

    SDL_SysWMinfo window_sys_info;
    SDL_VERSION(&window_sys_info.version)

    if (SDL_GetWindowWMInfo(the_window, &window_sys_info) == SDL_FALSE)
        error("failed to get platform specific window information");

    bool enable_validation_layers = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--validation") == 0)
            enable_validation_layers = true;
    }

    Vk_Demo demo(window_sys_info, the_window, enable_validation_layers);

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
