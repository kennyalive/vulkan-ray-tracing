#include "vulkan_demo.h"
#include "vulkan_utilities.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

const int window_width = 640;
const int window_height = 480;

Vulkan_Demo demo(window_width, window_height);

static void main_loop()
{
    SDL_Event event;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
        }
        if (running) {
            demo.run_frame();
            SDL_Delay(1);
        }
    }
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        error("SDL_Init error");

    SDL_Window* window = SDL_CreateWindow("Vulkan app", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        SDL_Quit();
        error("failed to create SDL window");
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version)
    if (SDL_GetWindowWMInfo(window, &wmInfo) == SDL_FALSE) {
        SDL_Quit();
        error("failed to get platform specific window information");
    }

    demo.initialize(wmInfo.info.win.window);
    main_loop();
    demo.release_resources();
    SDL_Quit();
    return 0;
}
