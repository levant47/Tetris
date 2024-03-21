typedef u32 Pixel;

#define BLACK 0
#define WHITE 0xffffff
#define RED 0xff0000
#define LIGHT_PURPLE 0x770077

struct Bitmap
{
    u64 width, height;
    Pixel* data;
};

struct Color { u8 r, g, b; };

Color pixel_to_color(Pixel pixel)
{
    Color result;
    result.r = (pixel >> 16) & 0xff;
    result.g = (pixel >> 8) & 0xff;
    result.b = pixel & 0xff;
    return result;
}

Pixel color_to_pixel(Color color) { return (color.r << 16) | (color.g << 8) | color.b; }

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
