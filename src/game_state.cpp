#define BOARD_WIDTH 8
#define BOARD_HEIGHT 20
#define FALLING_SHAPE_PERIOD_MS 500
#define QUICK_FALL_PERIOD_MS 50
#define MINIMUM_BOARD_COLOR_PERIOD 1
#define CELL_MAP_PITCH 20

char* HIGH_SCORE_FILE_NAME = "game_data.txt";

int load_high_score()
{
    auto file_handle = CreateFileA(
        HIGH_SCORE_FILE_NAME,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    assert(file_handle != INVALID_HANDLE_VALUE);
    char buffer[20];
    DWORD bytes_read;
    ReadFile(file_handle, buffer, countof(buffer), &bytes_read, NULL);
    CloseHandle(file_handle);
    auto parsed = string_to_int(make_string((u64)bytes_read, buffer));
    if (!parsed.success) { return 0; }
    return parsed.value;
}

void save_high_score(int value)
{
    auto file_handle = CreateFileA(
        HIGH_SCORE_FILE_NAME,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    char buffer_data[20];
    auto buffer = make_string(0, buffer_data);
    int_to_string(value, &buffer);
    WriteFile(file_handle, buffer.data, buffer.size, NULL, NULL);
    CloseHandle(file_handle);
}

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

void mirror(CellMap* cell_map)
{
    for (auto y = 0; y < cell_map->height; y++)
    {
        for (auto x = 0; x < cell_map->width / 2; x++)
        {
            auto temp = get_cell(x, y, *cell_map);
            set_cell(x, y, get_cell(cell_map->width - x - 1, y, *cell_map), cell_map);
            set_cell(cell_map->width - x - 1, y, temp, cell_map);
        }
    }
}

struct FallingShape
{
    CellMap cell_map;
    int x, y;
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

struct GameInput
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
    bool four;
};

int g_starting_board_color_period;

struct GameState
{
    u32 time;
    GameMode mode;
    CellMap board;
    FallingShape falling_shape;
    int falling_shape_saved_states_size;
    FallingShapeSavedState falling_shape_saved_states[4];
    bool quick_fall_mode;
    int score;
    int high_score;
    Pixel board_color;
    bool board_color_going_negative;
    struct
    {
        s32 shape_fall;
        s32 board_color;
    } timers;
    struct
    {
        s32 mirror;
        s32 fill_cell;
        s32 invert_board;
        s32 bomb;
    } power_ups;
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

#define INITIALIZE_INDIVIDUAL_SHAPE_CELL_MAP(cell_map, cell_map_width, cell_map_height, ...) \
{ \
    bool data[] = { __VA_ARGS__ }; \
    cell_map = make_cell_map(cell_map_width, cell_map_height); \
    for (auto y = 0; y < cell_map.height; y++) \
    { \
        for (auto x = 0; x < cell_map.width; x++) \
        { \
            set_cell(x, y, data[y * cell_map_width + x], &cell_map); \
        } \
    } \
}

void initialize_shape_cell_maps()
{
    INITIALIZE_INDIVIDUAL_SHAPE_CELL_MAP(SQUARE_SHAPE_CELL_MAP, 2, 2,
        1, 1,
        1, 1,
    )
    INITIALIZE_INDIVIDUAL_SHAPE_CELL_MAP(T_SHAPE_CELL_MAP, 3, 2,
        1, 1, 1,
        0, 1, 0,
    )
    INITIALIZE_INDIVIDUAL_SHAPE_CELL_MAP(PIPE_SHAPE_CELL_MAP, 1, 3,
        1,
        1,
        1,
    )
    INITIALIZE_INDIVIDUAL_SHAPE_CELL_MAP(L_SHAPE_CELL_MAP, 2, 3,
        1, 0,
        1, 0,
        1, 1,
    )
    INITIALIZE_INDIVIDUAL_SHAPE_CELL_MAP(SNAKE_SHAPE_CELL_MAP, 2, 3,
        1, 0,
        1, 1,
        0, 1,
    )
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
    g_game_state.timers.shape_fall = 0;
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

            switch (g_game_state.score % 3)
            {
                case 1: g_game_state.power_ups.mirror = MIN(10, g_game_state.power_ups.mirror + 1); break;
                case 2: g_game_state.power_ups.fill_cell = MIN(10, g_game_state.power_ups.fill_cell + 1); break;
                case 0: g_game_state.power_ups.bomb = MIN(10, g_game_state.power_ups.bomb + 1); break;
            }

            if (g_game_state.score % 5 == 0) { g_game_state.power_ups.invert_board++; }
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


void process_input(float dt, GameInput input)
{
    // initialize g_starting_board_color_period
    if (g_starting_board_color_period == 0) { g_starting_board_color_period = MAX(200, g_game_state.high_score * 10); }

    if (g_game_state.mode == GameModePlaying)
    {
        if (input.escape) { g_game_state.mode = GameModePause; }
        if (input.r)
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
        if (input.one)
        {
            if (g_game_state.power_ups.mirror != 0)
            {
                save_falling_shape_state();
                mirror(&g_game_state.falling_shape.cell_map);
                if (!does_falling_shape_conflict_with_board())
                {
                    g_game_state.power_ups.mirror--;
                    discard_falling_shape_state();
                }
                else { restore_falling_shape_state(); }
            }
        }
        if (input.two)
        {
            if (g_game_state.power_ups.fill_cell != 0)
            {
                g_game_state.falling_shape.cell_map.width = 1;
                g_game_state.falling_shape.cell_map.height = 1;
                g_game_state.falling_shape.cell_map.data[0] = 1;
                g_game_state.power_ups.fill_cell--;
            }
        }
        if (input.three)
        {
            if (g_game_state.power_ups.invert_board != 0)
            {
                auto y0 = 0;
                for (auto y = 0; y < BOARD_HEIGHT; y++)
                {
                    auto all_cells_blank = true;
                    for (auto x = 0; x < BOARD_WIDTH; x++)
                    {
                        if (get_cell(x, y, g_game_state.board))
                        {
                            all_cells_blank = false;
                            break;
                        }
                    }
                    if (!all_cells_blank)
                    {
                        y0 = y;
                        break;
                    }
                }

                for (auto y = y0; y < y0 + (BOARD_HEIGHT - y0) / 2; y++)
                {
                    for (auto x = 0; x < BOARD_WIDTH; x++)
                    {
                        auto temp = get_cell(x, y, g_game_state.board);
                        set_cell(x, y, get_cell(x, BOARD_HEIGHT - (y - y0) - 1, g_game_state.board), &g_game_state.board);
                        set_cell(x, BOARD_HEIGHT - (y - y0) - 1, temp, &g_game_state.board);
                    }
                }
                g_game_state.power_ups.invert_board--;
            }
        }
        if (input.four)
        {
            if (g_game_state.power_ups.bomb != 0)
            {
                for (auto y = g_game_state.falling_shape.y - 1; y < g_game_state.falling_shape.y + g_game_state.falling_shape.cell_map.height + 1; y++)
                {
                    for (auto x = g_game_state.falling_shape.x - 1; x < g_game_state.falling_shape.x + g_game_state.falling_shape.cell_map.width + 1; x++)
                    { set_cell(x, y, false, &g_game_state.board); }
                }
                generate_new_falling_shape();
                g_game_state.power_ups.bomb--;
            }
        }
        if (input.left || input.right)
        {
            save_falling_shape_state();
            g_game_state.falling_shape.x += input.left ? -1 : 1;
            if (does_falling_shape_conflict_with_board())
            { restore_falling_shape_state(); }
            else { discard_falling_shape_state(); }
        }
        // double checks here to prevent timer reset
        if (input.down && !g_game_state.quick_fall_mode) { g_game_state.quick_fall_mode = true; g_game_state.timers.shape_fall = 0; }
        if (input.up && g_game_state.quick_fall_mode) { g_game_state.quick_fall_mode = false; g_game_state.timers.shape_fall = 0; }
    }
    else if (g_game_state.mode == GameModeLost)
    {
        if (input.enter)
        {
            g_game_state.mode = GameModePlaying;
            generate_new_falling_shape();
            g_game_state.timers.board_color = 0;
            g_game_state.board_color = PURPLE;
            g_game_state.board_color_going_negative = false;
            g_game_state.score = 0;
            generate_initial_board_layout();
        }
    }
    else
    {
        if (input.escape) { g_game_state.mode = GameModePlaying; }
    }

    if (g_game_state.mode == GameModePlaying)
    {
        g_game_state.timers.shape_fall += dt;
        auto falling_shape_period = g_game_state.quick_fall_mode ? QUICK_FALL_PERIOD_MS : FALLING_SHAPE_PERIOD_MS;
        if (g_game_state.timers.shape_fall >= falling_shape_period)
        {
            g_game_state.timers.shape_fall -= falling_shape_period;
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
        g_game_state.timers.board_color += dt;
        auto period = MAX(MINIMUM_BOARD_COLOR_PERIOD, g_starting_board_color_period - g_game_state.score * 10);
        if (g_game_state.timers.board_color >= period)
        {
            while (g_game_state.timers.board_color > 0)
            {
                g_game_state.timers.board_color -= period;
                if ((g_game_state.board_color & 0xff) == 0xff) { g_game_state.board_color_going_negative = true; }
                else if ((g_game_state.board_color & 0xff) == 0) { g_game_state.board_color_going_negative = false; }
                auto channel = ((g_game_state.board_color & 0xff) + (g_game_state.board_color_going_negative ? -1 : 1)) % 0x100;
                g_game_state.board_color = 0xff0000 | ((0xff - channel) << 8) | channel;
            }
        }
    }
}
