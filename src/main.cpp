#include <windows.h>
#include "lib/SDL2/SDL.h"
#include "lib/SDL2/SDL_ttf.h"

#include "common.cpp"
#include "game_state.cpp"
#include "rendering.cpp"

#define SCREEN_WIDTH 500
#define SCREEN_HEIGHT 500

// TODO:
// [/] mirror
// [/] fill cell
// [/] invert board
// [/] bomb power up
// [/] refactor
// [ ] lower power up frequency
// [ ] visual hints for what buttons to press to activate what power-up
// [ ] animations for clearing a row and activating a power up
// [ ] textures for blocks?
// [ ] lagging?

int main(int, char**)
{
    seed_random_number_generator(get_system_time().milliseconds);

    auto sdl_init_result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (sdl_init_result < 0) { panic_sdl("SDL_Init"); }

    auto ttf_init_result = TTF_Init();
    if (ttf_init_result < 0) { panic_sdl("TTF_Init"); }

    auto window = SDL_CreateWindow(
        "Tetris",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        /* flags: */ SDL_WINDOW_RESIZABLE
    );
    if (!window) { panic_sdl("SDL_CreateWindow"); }

    g_game_state.resources.font16 = TTF_OpenFont("res/Sans.ttf", 16);
    if (g_game_state.resources.font16 == NULL) { panic_sdl("TTF_OpenFont"); }
    g_game_state.resources.font32 = TTF_OpenFont("res/Sans.ttf", 32);
    if (g_game_state.resources.font32 == NULL) { panic_sdl("TTF_OpenFont"); }

    initialize_shape_cell_maps();

    g_game_state.board = make_cell_map(BOARD_WIDTH,BOARD_HEIGHT);
    g_game_state.score = 0;
    g_game_state.high_score = load_high_score();
    g_game_state.board_color = PURPLE;
    g_game_state.board_color_going_negative = false;
    g_game_state.power_ups.mirror = 1;
    g_game_state.power_ups.fill_cell = 1;
    g_game_state.power_ups.invert_board = 1;
    g_game_state.power_ups.bomb = 1;
    g_game_state.time = SDL_GetTicks();
    generate_new_falling_shape();
    generate_initial_board_layout();

    float fps = 0;
    int dt = 0;
    while (true)
    {
        auto frame_start = (int)SDL_GetTicks();
        g_game_state.time = frame_start;

        // event processing
        auto quit = false;
        SDL_Event event;
        GameInput input;
        set_memory(0, sizeof(input), &input);
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_KEYDOWN:
                {
                    auto sym = event.key.keysym.sym;
                    input.left |= sym == SDLK_LEFT;
                    input.right |= sym == SDLK_RIGHT;
                    input.down |= sym == SDLK_DOWN;
                    input.up |= sym == SDLK_UP;
                    input.r |= sym == SDLK_r;
                    input.enter |= sym == SDLK_RETURN;
                    input.escape |= sym == SDLK_ESCAPE;
                    input.one |= sym == SDLK_1;
                    input.two |= sym == SDLK_2;
                    input.three |= sym == SDLK_3;
                    input.four |= sym == SDLK_4;
                    break;
                }
            }
        }
        if (quit) { break; }

        auto screen_surface = SDL_GetWindowSurface(window);
        if (screen_surface == NULL) { panic_sdl("SDL_GetWindowSurface"); }
        auto screen = make_bitmap(screen_surface->w, screen_surface->h, (Pixel*)screen_surface->pixels);

        process_input(dt, input);

        // rendering
        {
            draw_game(screen);

            char fps_buffer_data[20];
            auto fps_buffer = make_string(0, fps_buffer_data);
            float_to_string(fps, &fps_buffer);
            push('\0', &fps_buffer);
            draw_text(0, 0, RED, g_game_state.resources.font16, fps_buffer.data, screen);

            SDL_UpdateWindowSurface(window);
        }

        auto frame_end = (int)SDL_GetTicks();
        SDL_Delay(MAX(0, 16 - (frame_end - frame_start)));
        dt = ((int)SDL_GetTicks() - frame_start);
        fps = 1000.0f / (float)dt;
    }

    return 0;
}
