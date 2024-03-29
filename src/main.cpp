#include <windows.h>
#include "lib/SDL2/SDL.h"
#include "lib/SDL2/SDL_ttf.h"

#include "helpers.cpp"
#include "rendering.cpp"
#include "high_score.cpp"

#define SCREEN_WIDTH 500
#define SCREEN_HEIGHT 500
#define BOARD_WIDTH 8
#define BOARD_HEIGHT 20
#define FALLING_SHAPE_PERIOD_MS 500
#define QUICK_FALL_PERIOD_MS 50
#define STARTING_BOARD_COLOR_PERIOD 200
#define MINIMUM_BOARD_COLOR_PERIOD 1
#define CELL_MAP_PITCH 20

// TODO:
// [ ] power-ups: mirror, fill cell, invert board

struct CellMap
{
    int width, height;
    bool data[CELL_MAP_PITCH * CELL_MAP_PITCH];
};

CellMap make_cell_map(int width, int height)
{
    CellMap result;
    result.width = width;
    result.height = height;
    set_memory(0, sizeof(result.data), result.data);
    return result;
}

bool get_cell(int x, int y, CellMap cell_map)
{
    if (x < 0 || x >= cell_map.width || y < 0 || y >= cell_map.height) { return false; }
    return cell_map.data[y * CELL_MAP_PITCH + x];
}

void set_cell(int x, int y, bool value, CellMap* cell_map) { cell_map->data[y * CELL_MAP_PITCH + x] = value; }

void rotate(CellMap* source)
{
    auto rotated_cell_map = make_cell_map(source->height, source->width);
    for (auto y = 0; y < source->height; y++)
    {
        for (auto x = 0; x < source->width; x++)
        {
            set_cell(source->height - y - 1, x, get_cell(x, y, *source), &rotated_cell_map);
        }
    }
    *source = rotated_cell_map;
}

struct FallingShape
{
    CellMap cell_map;
    int x, y;
};

struct PlayerInput
{
    bool left;
    bool right;
    bool down;
    bool up;
    bool r;
    bool enter;
    bool escape;
    bool one;
    bool two;
    bool three;
};

struct FallingShapeSavedState
{
    int x, y;
    CellMap cell_map;
};

enum GameMode
{
    GameModePlaying,
    GameModeLost,
    GameModePause,
};

struct GameState
{
    u32 time;
    GameMode mode;
    CellMap board;
    FallingShape falling_shape;
    int falling_shape_saved_states_size;
    FallingShapeSavedState falling_shape_saved_states[4];
    s32 shape_fall_timer;
    bool quick_fall_mode;
    int score;
    int high_score;
    Pixel board_color;
    s32 board_color_timer;
    s32 board_color_going_negative;
    s32 mirror_power_ups;
    s32 fill_cell_power_ups;
    s32 invert_board_power_ups;

    PlayerInput input;
    struct
    {
        TTF_Font* font16;
        TTF_Font* font32;
    } resources;
};

GameState g_game_state;

CellMap SQUARE_SHAPE_CELL_MAP;
CellMap T_SHAPE_CELL_MAP;
CellMap PIPE_SHAPE_CELL_MAP;
CellMap L_SHAPE_CELL_MAP;
CellMap SNAKE_SHAPE_CELL_MAP;

CellMap* ALL_SHAPES[] = { &SQUARE_SHAPE_CELL_MAP, &T_SHAPE_CELL_MAP, &PIPE_SHAPE_CELL_MAP, &L_SHAPE_CELL_MAP, &SNAKE_SHAPE_CELL_MAP };

void initialize_shape_cell_maps()
{
    SQUARE_SHAPE_CELL_MAP = make_cell_map(2, 2);
    bool square_shape_cells[] =
    {
        1, 1,
        1, 1,
    };
    for (auto y = 0; y < SQUARE_SHAPE_CELL_MAP.height; y++)
    {
        for (auto x = 0; x < SQUARE_SHAPE_CELL_MAP.width; x++)
        {
            set_cell(x, y, square_shape_cells[y * SQUARE_SHAPE_CELL_MAP.width + x], &SQUARE_SHAPE_CELL_MAP);
        }
    }

    T_SHAPE_CELL_MAP = make_cell_map(3, 2);
    bool t_shape_cells[] =
    {
        1, 1, 1,
        0, 1, 0,
    };
    for (auto y = 0; y < T_SHAPE_CELL_MAP.height; y++)
    {
        for (auto x = 0; x < T_SHAPE_CELL_MAP.width; x++)
        {
            set_cell(x, y, t_shape_cells[y * T_SHAPE_CELL_MAP.width + x], &T_SHAPE_CELL_MAP);
        }
    }

    PIPE_SHAPE_CELL_MAP = make_cell_map(1, 3);
    bool pipe_shape_cells[] =
    {
        1,
        1,
        1,
    };
    for (auto y = 0; y < PIPE_SHAPE_CELL_MAP.height; y++)
    {
        for (auto x = 0; x < PIPE_SHAPE_CELL_MAP.width; x++)
        {
            set_cell(x, y, pipe_shape_cells[y * PIPE_SHAPE_CELL_MAP.width + x], &PIPE_SHAPE_CELL_MAP);
        }
    }

    L_SHAPE_CELL_MAP = make_cell_map(2, 3);
    bool l_shape_cells[] =
    {
        1, 0,
        1, 0,
        1, 1,
    };
    for (auto y = 0; y < L_SHAPE_CELL_MAP.height; y++)
    {
        for (auto x = 0; x < L_SHAPE_CELL_MAP.width; x++)
        {
            set_cell(x, y, l_shape_cells[y * L_SHAPE_CELL_MAP.width + x], &L_SHAPE_CELL_MAP);
        }
    }

    SNAKE_SHAPE_CELL_MAP = make_cell_map(2, 3);
    bool snake_shape_cells[] =
    {
        1, 0,
        1, 1,
        0, 1,
    };
    for (auto y = 0; y < SNAKE_SHAPE_CELL_MAP.height; y++)
    {
        for (auto x = 0; x < SNAKE_SHAPE_CELL_MAP.width; x++)
        {
            set_cell(x, y, snake_shape_cells[y * SNAKE_SHAPE_CELL_MAP.width + x], &SNAKE_SHAPE_CELL_MAP);
        }
    }
}

void save_falling_shape_state()
{
    assert(g_game_state.falling_shape_saved_states_size != countof(g_game_state.falling_shape_saved_states));
    auto i = g_game_state.falling_shape_saved_states_size;
    g_game_state.falling_shape_saved_states[i].x = g_game_state.falling_shape.x;
    g_game_state.falling_shape_saved_states[i].y = g_game_state.falling_shape.y;
    g_game_state.falling_shape_saved_states[i].cell_map = g_game_state.falling_shape.cell_map;
    g_game_state.falling_shape_saved_states_size++;
}

void discard_falling_shape_state()
{
    assert(g_game_state.falling_shape_saved_states_size != 0);
    g_game_state.falling_shape_saved_states_size--;
}

void restore_falling_shape_state()
{
    assert(g_game_state.falling_shape_saved_states_size != 0);
    auto i = g_game_state.falling_shape_saved_states_size - 1;
    g_game_state.falling_shape.x = g_game_state.falling_shape_saved_states[i].x;
    g_game_state.falling_shape.y = g_game_state.falling_shape_saved_states[i].y;
    g_game_state.falling_shape.cell_map = g_game_state.falling_shape_saved_states[i].cell_map;
    g_game_state.falling_shape_saved_states_size--;
}

void draw_game_screen(Bitmap bitmap)
{
    auto line_width = 1;
    auto min_side_padding = (int)(bitmap.width * .2);
    auto min_top_bottom_padding = bitmap.height / 10;
    auto maybe_cell_width = (int)((bitmap.width - min_side_padding * 2) / BOARD_WIDTH);
    auto maybe_cell_height = (int)((bitmap.height - min_top_bottom_padding * 2) / BOARD_HEIGHT);
    auto cell_size = MIN(maybe_cell_width, maybe_cell_height);
    auto board_width = cell_size * BOARD_WIDTH;
    auto board_height = cell_size * BOARD_HEIGHT;
    auto side_padding = (bitmap.width - board_width) / 2;
    auto top_bottom_padding = (bitmap.height - board_height) / 2;

    // horizontal lines
    for (auto i = 0; i <= BOARD_HEIGHT; i++)
    {
        draw_horizontal_line(
            side_padding,
            bitmap.width - side_padding,
            top_bottom_padding + i * cell_size - (i == BOARD_HEIGHT ? line_width : 0),
            LIGHT_PURPLE,
            bitmap
        );
    }

    // vertical lines
    for (auto i = 0; i <= BOARD_WIDTH; i++)
    {
        draw_vertical_line(
            side_padding + i * cell_size - (i == BOARD_WIDTH ? line_width : 0),
            top_bottom_padding,
            bitmap.height - top_bottom_padding,
            LIGHT_PURPLE,
            bitmap
        );
    }

    // fill cells
    auto cell_padding = cell_size / 20;
    for (auto y = 0; y < BOARD_HEIGHT; y++)
    {
        for (auto x = 0; x < BOARD_WIDTH; x++)
        {
            if (get_cell(x, y, g_game_state.board))
            {
                draw_rectangle(
                    side_padding + x * cell_size + cell_padding,
                    top_bottom_padding + y * cell_size + cell_padding,
                    cell_size - cell_padding * 2,
                    cell_size - cell_padding * 2,
                    g_game_state.board_color,
                    bitmap
                );
            }
        }
    }

    // falling shape
    if (g_game_state.mode != GameModeLost)
    {
        auto cell_map = g_game_state.falling_shape.cell_map;
        for (auto map_y = 0; map_y < cell_map.height; map_y++)
        {
            for (auto map_x = 0; map_x < cell_map.width; map_x++)
            {
                if (get_cell(map_x, map_y, cell_map))
                {
                    auto x = g_game_state.falling_shape.x + map_x;
                    auto y = g_game_state.falling_shape.y + map_y;
                    draw_rectangle(
                        side_padding + x * cell_size + cell_padding,
                        top_bottom_padding + y * cell_size + cell_padding,
                        cell_size - cell_padding,
                        cell_size - cell_padding,
                        0x00ff00,
                        bitmap
                    );
                }
            }
        }
    }

    // score
    char buffer_data[40];
    auto buffer = make_string(0, buffer_data);
    push("Score: ", &buffer);
    int_to_string(g_game_state.score, &buffer);
    push('\0', &buffer);
    auto score_text_dimensions = draw_text(
        side_padding + board_width + 10,
        top_bottom_padding + 5,
        WHITE,
        g_game_state.resources.font16,
        buffer_data,
        bitmap
    );

    // high score
    buffer.size = 0;
    push("High Score: ", &buffer);
    int_to_string(g_game_state.high_score, &buffer);
    push('\0', &buffer);
    draw_text(
        side_padding + board_width + 10,
        top_bottom_padding + 5 + score_text_dimensions.y + 5,
        WHITE,
        g_game_state.resources.font16,
        buffer_data,
        bitmap
    );

    // power ups
    {
        char power_up_text_data[256];
        auto power_up_text = make_string(0, power_up_text_data);
        auto power_up_color = WHITE;
        auto y = top_bottom_padding;

        push("Mirror shape: ", &power_up_text);
        int_to_string(g_game_state.mirror_power_ups, &power_up_text);
        push('\0', &power_up_text);
        auto mirror_power_up_text_surface = text_to_surface(power_up_color, g_game_state.resources.font16, power_up_text.data);
        draw_text(
            side_padding - mirror_power_up_text_surface->w - 5,
            y,
            power_up_color,
            g_game_state.resources.font16,
            power_up_text.data,
            bitmap
        );
        y += mirror_power_up_text_surface->h + 5;
        power_up_text.size = 0;
        SDL_FreeSurface(mirror_power_up_text_surface);

        push("Fill cell: ", &power_up_text);
        int_to_string(g_game_state.fill_cell_power_ups, &power_up_text);
        push('\0', &power_up_text);
        auto fill_cell_power_up_text_surface = text_to_surface(power_up_color, g_game_state.resources.font16, power_up_text.data);
        draw_text(
            side_padding - fill_cell_power_up_text_surface->w - 5,
            y,
            power_up_color,
            g_game_state.resources.font16,
            power_up_text.data,
            bitmap
        );
        y += fill_cell_power_up_text_surface->h + 5;
        power_up_text.size = 0;
        SDL_FreeSurface(fill_cell_power_up_text_surface);

        push("Invert board: ", &power_up_text);
        int_to_string(g_game_state.invert_board_power_ups, &power_up_text);
        push('\0', &power_up_text);
        auto invert_board_power_up_text_surface = text_to_surface(power_up_color, g_game_state.resources.font16, power_up_text.data);
        draw_text(
            side_padding - invert_board_power_up_text_surface->w - 5,
            y,
            power_up_color,
            g_game_state.resources.font16,
            power_up_text.data,
            bitmap
        );
        y += invert_board_power_up_text_surface->h + 5;
        power_up_text.size = 0;
        SDL_FreeSurface(invert_board_power_up_text_surface);
    }
}

void cement_falling_shape()
{
    auto cell_map = g_game_state.falling_shape.cell_map;

    for (auto y = 0; y < cell_map.height; y++)
    {
        for (auto x = 0; x < cell_map.width; x++)
        {
            if (get_cell(x, y, cell_map))
            {
                set_cell(x + g_game_state.falling_shape.x, y + g_game_state.falling_shape.y, true, &g_game_state.board);
            }
        }
    }
}

void generate_new_falling_shape()
{
    g_game_state.falling_shape.cell_map = *ALL_SHAPES[get_random_number_in_range(0, countof(ALL_SHAPES))];
    g_game_state.falling_shape.x = 3;
    g_game_state.falling_shape.y = 0;
    g_game_state.shape_fall_timer = 0;
    g_game_state.quick_fall_mode = false;
}

void check_for_game_over()
{
    for (auto x = 0; x < g_game_state.board.width; x++)
    {
        if (get_cell(x, 0, g_game_state.board))
        {
            g_game_state.mode = GameModeLost;
            break;
        }
    }
}

void shift_everything_down(int until_y)
{
    for (auto y = until_y - 1; y >= 0; y--)
    {
        for (auto x = 0; x < g_game_state.board.width; x++)
        { set_cell(x, y + 1, get_cell(x, y, g_game_state.board), &g_game_state.board); }
    }
}

void clear_solid_rows()
{
    // starting from 1, because if the top row is filled, it's game over
    for (auto y = 1; y < g_game_state.board.height; y++)
    {
        auto gaps = false;
        for (auto x = 0; x < g_game_state.board.width; x++)
        {
            if (!get_cell(x, y, g_game_state.board))
            {
                gaps = true;
                break;
            }
        }
        if (!gaps)
        {
            for (auto x = 0; x < g_game_state.board.width; x++)
            { set_cell(x, y, false, &g_game_state.board); }
            shift_everything_down(y);
            g_game_state.score++;
            if (g_game_state.score > g_game_state.high_score)
            {
                g_game_state.high_score = g_game_state.score;
                save_high_score(g_game_state.score);
            }
            if (g_game_state.score % 5 == 0)
            {
                switch (get_random_number_in_range(0, 3))
                {
                    case 0: g_game_state.mirror_power_ups++; break;
                    case 1: g_game_state.fill_cell_power_ups++; break;
                    case 2: g_game_state.invert_board_power_ups++; break;
                }
            }
        }
    }
}

bool does_falling_shape_conflict_with_board()
{
    if (g_game_state.falling_shape.x < 0 || g_game_state.falling_shape.x + g_game_state.falling_shape.cell_map.width > g_game_state.board.width
        || g_game_state.falling_shape.y + g_game_state.falling_shape.cell_map.height > g_game_state.board.height)
    { return true; }
    for (auto shape_y = 0; shape_y < g_game_state.falling_shape.cell_map.height; shape_y++)
    {
        for (auto shape_x = 0; shape_x < g_game_state.falling_shape.cell_map.width; shape_x++)
        {
            if (get_cell(shape_x, shape_y, g_game_state.falling_shape.cell_map)
                && get_cell(g_game_state.falling_shape.x + shape_x, g_game_state.falling_shape.y + shape_y, g_game_state.board))
            { return true; }
        }
    }
    return false;
}

void generate_initial_board_layout()
{
    auto hole1 = get_random_number_in_range(0, BOARD_WIDTH);
    auto hole2 = get_random_number_in_range(0, BOARD_WIDTH);
    for (auto y = 0; y < BOARD_HEIGHT; y++)
    {
        for (auto x = 0; x < BOARD_WIDTH; x++)
        {
            if (y < BOARD_HEIGHT - 2) { set_cell(x, y, false, &g_game_state.board); }
            else if (y == BOARD_HEIGHT - 2)
            { set_cell(x, y, x != hole1, &g_game_state.board); }
            else { set_cell(x, y, x != hole2, &g_game_state.board); }
        }
    }

}

int main(int, char**)
{
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
        set_memory(0, sizeof(PlayerInput), &g_game_state.input);
        SDL_Event event;
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
                    g_game_state.input.left |= sym == SDLK_LEFT;
                    g_game_state.input.right |= sym == SDLK_RIGHT;
                    g_game_state.input.down |= sym == SDLK_DOWN;
                    g_game_state.input.up |= sym == SDLK_UP;
                    g_game_state.input.r |= sym == SDLK_r;
                    g_game_state.input.enter |= sym == SDLK_RETURN;
                    g_game_state.input.escape |= sym == SDLK_ESCAPE;
                    g_game_state.input.one |= sym == SDLK_1;
                    g_game_state.input.two |= sym == SDLK_2;
                    g_game_state.input.three |= sym == SDLK_3;
                    break;
                }
            }
        }
        if (quit) { break; }

        auto screen_surface = SDL_GetWindowSurface(window);
        if (screen_surface == NULL) { panic_sdl("SDL_GetWindowSurface"); }
        auto screen = make_bitmap(screen_surface->w, screen_surface->h, (Pixel*)screen_surface->pixels);

        // state
        {
            if (g_game_state.mode == GameModePlaying)
            {
                if (g_game_state.input.escape) { g_game_state.mode = GameModePause; }
                if (g_game_state.input.r)
                {
                    save_falling_shape_state();
                    rotate(&g_game_state.falling_shape.cell_map);
                    if (does_falling_shape_conflict_with_board())
                    {
                        g_game_state.falling_shape.x -= MAX(0, g_game_state.falling_shape.x + g_game_state.falling_shape.cell_map.width - g_game_state.board.width);
                        if (does_falling_shape_conflict_with_board())
                        { restore_falling_shape_state(); }
                        else { discard_falling_shape_state(); }
                    }
                    else { discard_falling_shape_state(); }
                }
                if (g_game_state.input.left || g_game_state.input.right)
                {
                    save_falling_shape_state();
                    g_game_state.falling_shape.x += g_game_state.input.left ? -1 : 1;
                    if (does_falling_shape_conflict_with_board())
                    { restore_falling_shape_state(); }
                    else { discard_falling_shape_state(); }
                }
                // double checks here to prevent timer reset
                if (g_game_state.input.down && !g_game_state.quick_fall_mode) { g_game_state.quick_fall_mode = true; g_game_state.shape_fall_timer = 0; }
                if (g_game_state.input.up && g_game_state.quick_fall_mode) { g_game_state.quick_fall_mode = false; g_game_state.shape_fall_timer = 0; }
            }
            else if (g_game_state.mode == GameModeLost)
            {
                if (g_game_state.input.enter)
                {
                    g_game_state.mode = GameModePlaying;
                    generate_new_falling_shape();
                    g_game_state.board_color_timer = 0;
                    g_game_state.board_color = PURPLE;
                    g_game_state.board_color_going_negative = false;
                    g_game_state.score = 0;
                    generate_initial_board_layout();
                }
            }
            else
            {
                if (g_game_state.input.escape) { g_game_state.mode = GameModePlaying; }
            }

            if (g_game_state.mode == GameModePlaying)
            {
                g_game_state.shape_fall_timer += dt;
                auto falling_shape_period = g_game_state.quick_fall_mode ? QUICK_FALL_PERIOD_MS : FALLING_SHAPE_PERIOD_MS;
                if (g_game_state.shape_fall_timer >= falling_shape_period)
                {
                    g_game_state.shape_fall_timer -= falling_shape_period;
                    save_falling_shape_state();
                    g_game_state.falling_shape.y++;
                    if (does_falling_shape_conflict_with_board())
                    {
                        restore_falling_shape_state();
                        cement_falling_shape();
                        clear_solid_rows();
                        generate_new_falling_shape();
                        check_for_game_over();
                    }
                    else { discard_falling_shape_state(); }
                }
            }

            if (g_game_state.score != 0)
            {
                g_game_state.board_color_timer += dt;
                auto period = MAX(MINIMUM_BOARD_COLOR_PERIOD, STARTING_BOARD_COLOR_PERIOD - g_game_state.score * 10);
                if (g_game_state.board_color_timer >= period)
                {
                    while (g_game_state.board_color_timer > 0)
                    {
                        g_game_state.board_color_timer -= period;
                        if ((g_game_state.board_color & 0xff) == 0xff) { g_game_state.board_color_going_negative = true; }
                        else if ((g_game_state.board_color & 0xff) == 0) { g_game_state.board_color_going_negative = false; }
                        auto channel = ((g_game_state.board_color & 0xff) + (g_game_state.board_color_going_negative ? -1 : 1)) % 0x100;
                        g_game_state.board_color = 0xff0000 | ((0xff - channel) << 8) | channel;
                    }
                }
            }
        }

        // rendering
        {
            clear_bitmap(BLACK, screen);

            draw_game_screen(screen);

            if (g_game_state.mode == GameModeLost)
            {
                auto game_over_text_surface = text_to_surface(RED, g_game_state.resources.font32, "GAME OVER");
                auto game_over_text_dimensions = draw_text_with_shade(
                    (screen.width - game_over_text_surface->w) / 2,
                    (screen.height - game_over_text_surface->h) / 2,
                    RED,
                    2,
                    BLACK,
                    g_game_state.resources.font32,
                    "GAME OVER",
                    screen
                );
                SDL_FreeSurface(game_over_text_surface);

                auto restart_text_surface = text_to_surface(WHITE, g_game_state.resources.font16, "(press ENTER to restart)");
                draw_text_with_shade(
                    (screen.width - restart_text_surface->w) / 2,
                    (screen.height + game_over_text_dimensions.y) / 2,
                    WHITE,
                    2,
                    BLACK,
                    g_game_state.resources.font16,
                    "(press ENTER to restart)",
                    screen
                );
                SDL_FreeSurface(restart_text_surface);
            }
            if (g_game_state.mode == GameModePause)
            {
                auto paused_text_surface = text_to_surface(WHITE, g_game_state.resources.font32, "PAUSED");
                draw_text_with_shade(
                    (screen.width - paused_text_surface->w) / 2,
                    (screen.height - paused_text_surface->h) / 2,
                    WHITE,
                    2,
                    BLACK,
                    g_game_state.resources.font32,
                    "PAUSED",
                    screen
                );
                SDL_FreeSurface(paused_text_surface);
            }

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
