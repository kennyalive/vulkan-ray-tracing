#include "demo.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

const int window_width = 720;
const int window_height = 720;

static SDL_Window* the_window = nullptr;
static bool minimized = false;

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
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                return 0;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.windowID == the_window_id) {
                if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    demo.release_resolution_dependent_resources();
                    minimized = true;
                }
                if ((event.window.event == SDL_WINDOWEVENT_RESTORED || event.window.event == SDL_WINDOWEVENT_MAXIMIZED) && minimized) {
                    demo.restore_resolution_dependent_resources();
                    minimized = false;
                }
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    demo.on_resize(event.window.data1, event.window.data2);
                }
            }
        }

        if (!minimized) {
            demo.run_frame();
        }
        SDL_Delay(1);
    }
}
