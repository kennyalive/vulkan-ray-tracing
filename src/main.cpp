#include "demo.h"

#include "sdl/SDL.h"
#include "sdl/SDL_syswm.h"

#include "imgui/imgui.h"
#include "imgui/impl/imgui_impl_sdl.h"

static bool toogle_fullscreen   = false;
static bool handle_resize       = false;

static bool parse_command_line(int argc, char** argv, Demo_Create_Info& demo_create_info) {
    bool found_unknown_option = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--validation-layers") == 0) {
            demo_create_info.vk_create_info.enable_validation_layers = true;
        }
        else if (strcmp(argv[i], "--debug-names") == 0) {
            demo_create_info.vk_create_info.use_debug_names = true;
        }
        else if (strcmp(argv[i], "--data-dir") == 0) {
            if (i == argc-1) {
                printf("--data-dir value is missing\n");
            } else {
                extern std::string g_data_dir;
                g_data_dir = argv[i+1];
                i++;
            }
        }
        else if (strcmp(argv[i], "--help") == 0) {
            printf("%-25s Path to the data directory. Default is ./data.\n", "--data-dir");
            printf("%-25s Enables Vulkan validation layers.\n", "--validation-layers");
            printf("%-25s Allows to assign debug names to Vulkan objects.\n", "--debug-names");
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
    Demo_Create_Info demo_info{};
    if (!parse_command_line(argc, argv, demo_info))
        return 0;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        error("SDL_Init error");

    struct On_Exit {~On_Exit() { SDL_Quit(); }} exit_action;

    // Create window.
    SDL_Window* the_window = SDL_CreateWindow("Vulkan demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 720, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (the_window == nullptr)
        error("failed to create SDL window");

    demo_info.window = the_window;

    SDL_VERSION(&demo_info.vk_create_info.windowing_system_info.version);
    if (SDL_GetWindowWMInfo(the_window, &demo_info.vk_create_info.windowing_system_info) == SDL_FALSE)
        error("failed to get platform specific window information");

    // Initialize demo.
    Vk_Demo demo;
    demo.initialize(demo_info);

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

    demo.shutdown();
    return 0;
}
