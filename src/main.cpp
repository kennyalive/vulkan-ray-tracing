#include "demo.h"

#define SDL_MAIN_HANDLED
#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

const int window_width = 720;
const int window_height = 720;

static SDL_Window* the_window = nullptr;

void error(const std::string& message) {
    printf("%s\n", message.c_str());
    throw std::runtime_error(message);
}

void set_window_title(const std::string& title) {
    SDL_SetWindowTitle(the_window, title.c_str());
}

static int run_demo(const SDL_SysWMinfo& window_manager_info) {
    Vk_Demo demo(window_width, window_height, window_manager_info);

    while (true) {
        bool quit = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                quit = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
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

    struct On_Exit {~On_Exit() { SDL_Quit(); }} exit_action;

    the_window = SDL_CreateWindow("Vulkan demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN);
    if (the_window == nullptr)
        error("failed to create SDL window");

    SDL_SysWMinfo window_sys_info;
    SDL_VERSION(&window_sys_info.version)
    if (SDL_GetWindowWMInfo(the_window, &window_sys_info) == SDL_FALSE)
        error("failed to get platform specific window information");

    return run_demo(window_sys_info);
}
