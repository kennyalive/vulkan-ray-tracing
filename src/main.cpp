#include "demo.h"
#include "vulkan_utilities.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

const int window_width = 640;
const int window_height = 480;

static int run_demo(const SDL_SysWMinfo& window_manager_info) {
    Vulkan_Demo demo(window_width, window_height, window_manager_info);
    while (true) {
        bool quit = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                quit = true;
        }
        if (quit) {
            break;
        }
        demo.run_frame();
        SDL_Delay(1);
    }
    return 0;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        error("SDL_Init error");

    Defer_Action quit_on_exit([]() { SDL_Quit(); });

    SDL_Window* window = SDL_CreateWindow("Vulkan app", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN);
    if (window == nullptr)
        error("failed to create SDL window");

    SDL_SysWMinfo window_sys_info;
    SDL_VERSION(&window_sys_info.version)
    if (SDL_GetWindowWMInfo(window, &window_sys_info) == SDL_FALSE)
        error("failed to get platform specific window information");

    return run_demo(window_sys_info);
}
