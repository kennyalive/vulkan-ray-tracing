#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

#include "common.h"
#include "vulkan_demo.h"

const int windowWidth = 640;
const int windowHeight = 480;

Vulkan_Demo demo(windowWidth, windowHeight);

void RunMainLoop()
{
    SDL_Event event;
    bool running = true;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = false;
        }
        if (running)
        {
            demo.RunFrame();
            SDL_Delay(1);
        }
    }
}

int main()
{
    // create SDL window
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        Error("SDL_Init error");

    SDL_Window* window = SDL_CreateWindow("Vulkan app", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        windowWidth, windowHeight, SDL_WINDOW_SHOWN);
    if (window == nullptr)
    {
        SDL_Quit();
        Error("failed to create SDL window");
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version)
    if (SDL_GetWindowWMInfo(window, &wmInfo) == SDL_FALSE)
    {
        SDL_Quit();
        Error("failed to get platform specific window information");
    }

    demo.CreateResources(wmInfo.info.win.window);
    RunMainLoop();
    demo.CleanupResources();
    SDL_Quit();
    return 0;
}
