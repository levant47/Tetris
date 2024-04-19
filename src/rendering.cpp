struct Bitmap
{
    u64 width, height;
    Pixel* data;
};

Bitmap make_bitmap(u64 width, u64 height, Pixel* data)
{
    Bitmap result;
    result.width = width;
    result.height = height;
    result.data = data;
    return result;
}

void set_pixel(int x, int y, Pixel color, Bitmap bitmap)
{
    assert(0 <= x && x < bitmap.width && 0 <= y && y < bitmap.height);
    bitmap.data[y * bitmap.width + x] = color;
}

void clear_bitmap(Pixel color, Bitmap bitmap)
{
    for (u64 y = 0; y < bitmap.height; y++)
    {
        for (u64 x = 0; x < bitmap.width; x++)
        {
            set_pixel(x, y, color, bitmap);
        }
    }
}

void draw_horizontal_line(int x0, int x1, int y, Pixel color, Bitmap bitmap)
{
    auto start = MIN(x0, x1);
    auto end = MAX(x0, x1);
    for (int x = start; x < end; x++)
    { set_pixel(x, y, color, bitmap); }
}

void draw_vertical_line(int x, int y0, int y1, Pixel color, Bitmap bitmap)
{
    auto start = MIN(y0, y1);
    auto end = MAX(y0, y1);
    for (int y = start; y < end; y++)
    { set_pixel(x, y, color, bitmap); }
}

void draw_rectangle(int x0, int y0, int width, int height, Pixel color, Bitmap bitmap)
{
    for (auto y = y0; y < y0 + height; y++)
    {
        for (auto x = x0; x < x0 + width; x++)
        {
            set_pixel(x, y, color, bitmap);
        }
    }
}

SDL_Surface* text_to_surface(Pixel color, TTF_Font* font, char* text)
{
    SDL_Color sdl_color;
    set_memory(0xff, sizeof(sdl_color), &sdl_color);
    return TTF_RenderText_Solid(font, text, sdl_color);
}

Vector draw_text(int x0, int y0, Pixel color, TTF_Font* font, char* text, Bitmap bitmap)
{
    auto text_surface = text_to_surface(color, font, text);
    if (!text_surface) { panic_sdl("TTF_RenderText_Solid"); }
    for (auto y = 0; y < text_surface->h; y++)
    {
        for (auto x = 0; x < text_surface->w; x++)
        {
            auto text_pixel = ((u8*)text_surface->pixels)[y * text_surface->pitch + x];
            if (text_pixel == 1)
            {
                set_pixel(x + x0, y + y0, color, bitmap);
            }
        }
    }
    auto result = make_vector(text_surface->w, text_surface->h);
    SDL_FreeSurface(text_surface);
    return result;
}

Vector draw_text_with_shade(int x0, int y0, Pixel color, int shade, Pixel shade_color, TTF_Font* font, char* text, Bitmap bitmap)
{
    draw_text(x0 - shade, y0 - shade, shade_color, font, text, bitmap);
    draw_text(x0 + shade, y0 + shade, shade_color, font, text, bitmap);
    auto dimensions = draw_text(x0, y0, color, font, text, bitmap);
    dimensions.x += shade;
    dimensions.y += shade;
    return dimensions;
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
        int_to_string(g_game_state.power_ups.mirror, &power_up_text);
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
        int_to_string(g_game_state.power_ups.fill_cell, &power_up_text);
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
        int_to_string(g_game_state.power_ups.invert_board, &power_up_text);
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

        push("Bomb: ", &power_up_text);
        int_to_string(g_game_state.power_ups.bomb, &power_up_text);
        push('\0', &power_up_text);
        auto bomb_power_up_text_surface = text_to_surface(power_up_color, g_game_state.resources.font16, power_up_text.data);
        draw_text(
            side_padding - bomb_power_up_text_surface->w - 5,
            y,
            power_up_color,
            g_game_state.resources.font16,
            power_up_text.data,
            bitmap
        );
        y += bomb_power_up_text_surface->h + 5;
        power_up_text.size = 0;
        SDL_FreeSurface(bomb_power_up_text_surface);
    }
}

void draw_game(Bitmap bitmap)
{
    clear_bitmap(BLACK, bitmap);

    draw_game_screen(bitmap);

    if (g_game_state.mode == GameModeLost)
    {
        auto game_over_text_surface = text_to_surface(RED, g_game_state.resources.font32, "GAME OVER");
        auto game_over_text_dimensions = draw_text_with_shade(
            (bitmap.width - game_over_text_surface->w) / 2,
            (bitmap.height - game_over_text_surface->h) / 2,
            RED,
            2,
            BLACK,
            g_game_state.resources.font32,
            "GAME OVER",
            bitmap
        );
        SDL_FreeSurface(game_over_text_surface);

        auto restart_text_surface = text_to_surface(WHITE, g_game_state.resources.font16, "(press ENTER to restart)");
        draw_text_with_shade(
            (bitmap.width - restart_text_surface->w) / 2,
            (bitmap.height + game_over_text_dimensions.y) / 2,
            WHITE,
            2,
            BLACK,
            g_game_state.resources.font16,
            "(press ENTER to restart)",
            bitmap
        );
        SDL_FreeSurface(restart_text_surface);
    }
    if (g_game_state.mode == GameModePause)
    {
        auto paused_text_surface = text_to_surface(WHITE, g_game_state.resources.font32, "PAUSED");
        draw_text_with_shade(
            (bitmap.width - paused_text_surface->w) / 2,
            (bitmap.height - paused_text_surface->h) / 2,
            WHITE,
            2,
            BLACK,
            g_game_state.resources.font32,
            "PAUSED",
            bitmap
        );
        SDL_FreeSurface(paused_text_surface);
    }
}
