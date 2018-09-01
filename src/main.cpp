#include "demo.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

const int window_width = 720;
const int window_height = 720;

static SDL_Window* the_window = nullptr;
static bool minimized = false;
static bool restore_failed = false;
static bool toogle_fullscreen = false;

static bool process_events(Vk_Demo& demo) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        // Quit event.
        if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            return false;

        // Fullscreen (Alt-Enter or F11).
        if (event.type == SDL_KEYDOWN &&
                (event.key.keysym.sym == SDLK_RETURN && (SDL_GetModState() & KMOD_LALT) ||
                    event.key.keysym.sym == SDLK_F11)) 
        {
            toogle_fullscreen = true;
        }

        // Minimization and resize events.
        if (event.type == SDL_WINDOWEVENT) 
        {
            if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                minimized = true;
            } else if (event.window.event == SDL_WINDOWEVENT_RESTORED && minimized) {
                minimized = false;
            } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                demo.release_resolution_dependent_resources();

                vk.surface_width = event.window.data1;
                vk.surface_height = event.window.data2;

                restore_failed = !demo.restore_resolution_dependent_resources();
            }
        }
    }
    return true;
}

static void check_fullscreen_toggle(Vk_Demo& demo) {
    if (toogle_fullscreen) {
        demo.run_frame(true);

        if (SDL_GetWindowFlags(the_window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
            SDL_SetWindowFullscreen(the_window, 0);
        else
            SDL_SetWindowFullscreen(the_window, SDL_WINDOW_FULLSCREEN_DESKTOP);

        process_events(demo);
        toogle_fullscreen = false;
    }
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        error("SDL_Init error");

    struct On_Exit {~On_Exit() { SDL_Quit(); }} exit_action;

    the_window = SDL_CreateWindow("Vulkan demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (the_window == nullptr)
        error("failed to create SDL window");

    uint32_t the_window_id = SDL_GetWindowID(the_window);

    SDL_SysWMinfo window_sys_info;
    SDL_VERSION(&window_sys_info.version)
    if (SDL_GetWindowWMInfo(the_window, &window_sys_info) == SDL_FALSE)
        error("failed to get platform specific window information");

    Vk_Demo demo(window_width, window_height, window_sys_info);

    while (true) {
        if (!process_events(demo))
            return 0;

        check_fullscreen_toggle(demo);

        if (!minimized) {
            if (!restore_failed)
                demo.run_frame();
            else
                restore_failed = !demo.restore_resolution_dependent_resources();
        }

        SDL_Delay(1);
    }
}
