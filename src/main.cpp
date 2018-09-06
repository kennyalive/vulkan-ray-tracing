#include "demo.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

static SDL_Window* the_window   = nullptr;

static bool toogle_fullscreen   = false;
static bool toggle_vsync        = false;
static bool handle_resize       = false;

static bool process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        bool alt_pressed = (SDL_GetModState() & KMOD_LALT) != 0;

        // Quit event.
        if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            return false;

        // Fullscreen (Alt-Enter or F11).
        if (event.type == SDL_KEYDOWN &&
            (event.key.keysym.sym == SDLK_F11 || event.key.keysym.sym == SDLK_RETURN && alt_pressed))
        {
            toogle_fullscreen = true;
        }

        // Vsync.
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F10)
        {
            toggle_vsync = true;
        }

        // Resize event
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
        {
            handle_resize = true;
        }
    }
    return true;
}

int main() {
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

    Vk_Demo demo(window_sys_info);

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

        if (toggle_vsync) {
            demo.toggle_vsync();
            toggle_vsync = false;
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
