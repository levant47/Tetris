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

struct CellMap
{
    int width, height;
    int pitch;
    bool* data;
    int rotation;
};

bool get_cell(int x, int y, CellMap cell_map)
{
    if (x < 0 || x >= cell_map.width || y < 0 || y >= cell_map.height) { return false; }
    auto coordinates = rotate((360 - cell_map.rotation) % 360, make_vector(x, y), make_vector(cell_map.width, cell_map.height));
    return cell_map.data[coordinates.y * cell_map.pitch + coordinates.x];
}

void set_cell(int x, int y, bool value, CellMap board) { board.data[y * board.width + x] = value; }

void rotate(CellMap* cell_map)
{
    cell_map->rotation = (cell_map->rotation + 90) % 360;
    auto temp = cell_map->width;
    cell_map->width = cell_map->height;
    cell_map->height = temp;
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
};

struct FallingShapeSavedState
{
    int x, y;
    int cell_map_width, cell_map_height;
    int cell_map_rotation;
};

struct GameState
{
    u32 time;
    bool game_over;
    CellMap board;
    FallingShape falling_shape;
    int falling_shape_saved_states_size;
    FallingShapeSavedState falling_shape_saved_states[4];
    u32 shape_fall_timer;
    bool quick_fall_mode;
    int score;
    int high_score;

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

CellMap* ALL_SHAPES[] = { &SQUARE_SHAPE_CELL_MAP, &T_SHAPE_CELL_MAP, &PIPE_SHAPE_CELL_MAP };

void initialize_shape_cell_maps()
{
    SQUARE_SHAPE_CELL_MAP.width = 2;
    SQUARE_SHAPE_CELL_MAP.height = 2;
    SQUARE_SHAPE_CELL_MAP.pitch = 2;
    auto square_shape_cell_map_size_in_bytes = SQUARE_SHAPE_CELL_MAP.width * SQUARE_SHAPE_CELL_MAP.height;
    SQUARE_SHAPE_CELL_MAP.data = (bool*)malloc(square_shape_cell_map_size_in_bytes);
    bool square_shape_cells[] =
    {
        1, 1,
        1, 1,
    };
    copy_memory(square_shape_cell_map_size_in_bytes, square_shape_cells, SQUARE_SHAPE_CELL_MAP.data);

    T_SHAPE_CELL_MAP.width = 3;
    T_SHAPE_CELL_MAP.height = 2;
    T_SHAPE_CELL_MAP.pitch = 3;
    auto t_shape_cell_map_size_in_bytes = T_SHAPE_CELL_MAP.width * T_SHAPE_CELL_MAP.height;
    T_SHAPE_CELL_MAP.data = (bool*)malloc(t_shape_cell_map_size_in_bytes);
    bool t_shape_cells[] =
    {
        1, 1, 1,
        0, 1, 0,
    };
    copy_memory(t_shape_cell_map_size_in_bytes, t_shape_cells, T_SHAPE_CELL_MAP.data);

    PIPE_SHAPE_CELL_MAP.width = 1;
    PIPE_SHAPE_CELL_MAP.height = 3;
    PIPE_SHAPE_CELL_MAP.pitch = 1;
    auto pipe_shape_cell_map_size_in_bytes = PIPE_SHAPE_CELL_MAP.width * PIPE_SHAPE_CELL_MAP.height;
    PIPE_SHAPE_CELL_MAP.data = (bool*)malloc(pipe_shape_cell_map_size_in_bytes);
    bool pipe_shape_cells[] =
    {
        1,
        1,
        1,
    };
    copy_memory(pipe_shape_cell_map_size_in_bytes, pipe_shape_cells, PIPE_SHAPE_CELL_MAP.data);
}

void save_falling_shape_state()
{
    assert(g_game_state.falling_shape_saved_states_size != countof(g_game_state.falling_shape_saved_states));
    auto i = g_game_state.falling_shape_saved_states_size;
    g_game_state.falling_shape_saved_states[i].x = g_game_state.falling_shape.x;
    g_game_state.falling_shape_saved_states[i].y = g_game_state.falling_shape.y;
    g_game_state.falling_shape_saved_states[i].cell_map_width = g_game_state.falling_shape.cell_map.width;
    g_game_state.falling_shape_saved_states[i].cell_map_height = g_game_state.falling_shape.cell_map.height;
    g_game_state.falling_shape_saved_states[i].cell_map_rotation = g_game_state.falling_shape.cell_map.rotation;
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
    g_game_state.falling_shape.cell_map.width = g_game_state.falling_shape_saved_states[i].cell_map_width;
    g_game_state.falling_shape.cell_map.height = g_game_state.falling_shape_saved_states[i].cell_map_height;
    g_game_state.falling_shape.cell_map.rotation = g_game_state.falling_shape_saved_states[i].cell_map_rotation;
    discard_falling_shape_state();
}

void draw_board(Bitmap bitmap)
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
            bitmap.width - top_bottom_padding,
            LIGHT_PURPLE,
            bitmap
        );
    }

    // fill cells
    auto cell_padding = cell_size / 10;
    for (auto y = 0; y < BOARD_HEIGHT; y++)
    {
        for (auto x = 0; x < BOARD_WIDTH; x++)
        {
            if (get_cell(x, y, g_game_state.board))
            {
                draw_rectangle(
                    side_padding + x * cell_size + cell_padding,
                    top_bottom_padding + y * cell_size + cell_padding,
                    cell_size - cell_padding,
                    cell_size - cell_padding,
                    0xff00ff,
                    bitmap
                );
            }
        }
    }

    // falling shape
    if (!g_game_state.game_over)
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
}

bool did_falling_shape_hit_the_ground()
{
    auto cell_map = g_game_state.falling_shape.cell_map;

    if (g_game_state.falling_shape.y + cell_map.height == g_game_state.board.height) { return true; }

    for (auto y = 0; y < cell_map.height; y++)
    {
        for (auto x = 0; x < cell_map.width; x++)
        {
            if (get_cell(x, y, cell_map)
                && get_cell(x + g_game_state.falling_shape.x, y + g_game_state.falling_shape.y + 1, g_game_state.board))
            {
                return true;
            }
        }
    }
    return false;
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
                set_cell(x + g_game_state.falling_shape.x, y + g_game_state.falling_shape.y, true, g_game_state.board);
            }
        }
    }
}

void generate_new_falling_shape()
{
    g_game_state.falling_shape.cell_map = *ALL_SHAPES[g_game_state.time % countof(ALL_SHAPES)];
    g_game_state.falling_shape.x = 3;
    g_game_state.falling_shape.y = 0;
    g_game_state.shape_fall_timer = g_game_state.time;
    g_game_state.quick_fall_mode = false;
}

void check_for_game_over()
{
    for (auto x = 0; x < g_game_state.board.width; x++)
    {
        if (get_cell(x, 0, g_game_state.board))
        {
            g_game_state.game_over = true;
            break;
        }
    }
}

void shift_everything_down(int until_y)
{
    for (auto y = until_y - 1; y >= 0; y--)
    {
        for (auto x = 0; x < g_game_state.board.width; x++)
        { set_cell(x, y + 1, get_cell(x, y, g_game_state.board), g_game_state.board); }
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
            { set_cell(x, y, false, g_game_state.board); }
            shift_everything_down(y);
            g_game_state.score++;
            if (g_game_state.score > g_game_state.high_score)
            {
                g_game_state.high_score = g_game_state.score;
                save_high_score(g_game_state.score);
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

int main(int, char**)
{
    auto sdl_init_result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (sdl_init_result < 0) { panic_sdl("SDL_Init"); }

    auto ttf_init_result = TTF_Init();
    if (ttf_init_result < 0) { panic_sdl("TTF_Init"); }

    auto window = SDL_CreateWindow("Tetris", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, /* flags: */ 0);
    if (!window) { panic_sdl("SDL_CreateWindow"); }

    auto screen_surface = SDL_GetWindowSurface(window);
    if (screen_surface == NULL) { panic_sdl("SDL_GetWindowSurface"); }

    auto screen = make_bitmap(screen_surface->w, screen_surface->h, (Pixel*)screen_surface->pixels);

    g_game_state.resources.font16 = TTF_OpenFont("res/Sans.ttf", 16);
    if (g_game_state.resources.font16 == NULL) { panic_sdl("TTF_OpenFont"); }
    g_game_state.resources.font32 = TTF_OpenFont("res/Sans.ttf", 32);
    if (g_game_state.resources.font32 == NULL) { panic_sdl("TTF_OpenFont"); }

    initialize_shape_cell_maps();

    g_game_state.board.width = BOARD_WIDTH;
    g_game_state.board.height = BOARD_HEIGHT;
    g_game_state.board.pitch = BOARD_WIDTH;
    bool board_data[BOARD_WIDTH * BOARD_HEIGHT];
    set_memory(0, sizeof(board_data), board_data);
    g_game_state.board.data = board_data;
    g_game_state.score = 0;
    g_game_state.high_score = load_high_score();
    g_game_state.time = SDL_GetTicks();
    generate_new_falling_shape();

    // DEBUG
    for (auto y = BOARD_HEIGHT - 2; y < BOARD_HEIGHT; y++)
    {
        for (auto x = 0; x < BOARD_WIDTH; x++)
        {
            if (x != (y % BOARD_WIDTH)) // make a hole in each line
            {
                board_data[y * BOARD_WIDTH + x] = true;
            }
        }
    }

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
                case SDL_KEYUP:
                {
                    auto sym = event.key.keysym.sym;
                    g_game_state.input.left |= sym == SDLK_LEFT;
                    g_game_state.input.right |= sym == SDLK_RIGHT;
                    g_game_state.input.down |= sym == SDLK_DOWN;
                    g_game_state.input.up |= sym == SDLK_UP;
                    g_game_state.input.r |= sym == SDLK_r;
                    break;
                }
            }
        }
        if (quit) { break; }

        // state
        {
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
            if (g_game_state.input.down) { g_game_state.quick_fall_mode = true; }
            if (g_game_state.input.up) { g_game_state.quick_fall_mode = false; }

            auto falling_shape_period = g_game_state.quick_fall_mode ? QUICK_FALL_PERIOD_MS : FALLING_SHAPE_PERIOD_MS;
            if (!g_game_state.game_over && g_game_state.time - g_game_state.shape_fall_timer >= falling_shape_period)
            {
                g_game_state.shape_fall_timer = g_game_state.time;
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

        // rendering
        {
            clear_bitmap(BLACK, screen);

            draw_board(screen);

            if (g_game_state.game_over)
            {
                draw_text(0, 0, RED, g_game_state.resources.font32, "GAME OVER", screen);
            }

            SDL_UpdateWindowSurface(window);
        }

        auto frame_end = (int)SDL_GetTicks();
        SDL_Delay(MAX(0, 16 - (frame_end - frame_start)));
        auto dt = ((int)SDL_GetTicks() - frame_start);
        auto fps = 1000.0f / (float)dt;
        print(fps);
        print("\n");
    }

    return 0;
}
