#define STDOUT_INITIALIZATION_FAILED_EXIT_CODE 2
#define MIN_S64 -9223372036854775808LL
#define QUOTE(macro) #macro
#define DOUBLE_QUOTE(macro) QUOTE(macro)
#define countof(array) (sizeof(array) / sizeof((array)[0]))

#define MIN(left, right) ((left) < (right) ? (left) : (right))
#define MAX(left, right) ((left) > (right) ? (left) : (right))

typedef signed char        s8;
typedef short              s16;
typedef int                s32;
typedef long long          s64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

HANDLE g_stdout = NULL;

struct String
{
    u64 size;
    char* data;
};

String make_string(u64 size, char* data)
{
    String result;
    result.size = size;
    result.data = data;
    return result;
}

void push(char c, String* string) { string->data[string->size++] = c; }

void push(char* c_string, String* string)
{
    for (auto i = 0; c_string[i] != '\0'; i++)
    { push(c_string[i], string); }
}

struct Vector { int x, y; };

Vector make_vector(int x, int y)
{
    Vector result;
    result.x = x;
    result.y = y;
    return result;
}

Vector rotate(int degrees, Vector point, Vector dimensions)
{
    while (degrees != 0)
    {
        point = make_vector(dimensions.y - point.y - 1, point.x);
        dimensions = make_vector(dimensions.y, dimensions.x);
        degrees -= 90;
    }
    return point;
}

int c_string_length(char* string)
{
    int result = 0;
    while (string[result] != '\0') { result++; }
    return result;
}

void print(char* message, int length)
{
    if (g_stdout == NULL)
    {
        g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (g_stdout == NULL || g_stdout == INVALID_HANDLE_VALUE)
        {
            ExitProcess(STDOUT_INITIALIZATION_FAILED_EXIT_CODE);
        }
    }
    WriteFile(g_stdout, message, length, NULL, NULL);
}

void print(String string) { print(string.data, string.size); }

void print(char* message) { print(message, c_string_length(message)); }

void set_memory(char value, u64 size, void* data)
{
    for (u64 i = 0; i < size; i++)
    { ((char*)data)[i] = value; }
}

void copy_memory(u64 size, void* from, void* to)
{
    for (u64 i = 0; i < size; i++) { ((char*)to)[i] = ((char*)from)[i]; }
}

void assert(bool condition)
{
    if (!condition)
    {
        print("Assertion failed\n");
        ExitProcess(3);
    }
}

void uint_to_string(u64 value, String* result)
{
    auto start = result->size;
    auto digits = 0;
    do
    {
        push((value % 10) + '0', result);
        value /= 10;
        digits++;
    }
    while (value != 0);

    for (auto i = 0; i < digits / 2; i++)
    {
        auto temp = result->data[start + i];
        result->data[start + i] = result->data[start + digits - i - 1];
        result->data[start + digits - i - 1] = temp;
    }
}

void print(u64 value)
{
    char buffer[20];
    auto string = make_string(0, buffer);
    uint_to_string(value, &string);
    print(string);
}

void int_to_string(s64 value, String* result)
{
    if (value == MIN_S64)
    {
        push(DOUBLE_QUOTE(MIN_S64), result);
        return;
    }
    if (value < 0)
    {
        push('-', result);
        value = -value;
    }
    uint_to_string((u64)value, result);
}

struct ParsedInt
{
    bool success;
    int value;
};

ParsedInt string_to_int(String source)
{
    ParsedInt result;
    if (false)
    {
        fail:
        result.success = false;
        return result;
    }
    if (source.size == 0) { goto fail; }
    auto negative = false;
    int i = 0;
    if (source.data[0] == '-')
    {
        negative = true;
        i++;
        if (source.size == 1) { goto fail; }
    }
    int value = 0;
    while (i < source.size)
    {
        if (source.data[i] < '0' || source.data[i] > '9') { goto fail; }
        value = value * 10 + source.data[i] - '0';
        i++;
    }
    result.success = true;
    result.value = value;
    if (negative) { result.value = -result.value; }
    return result;
}

void print(s64 value)
{
    char buffer[20];
    auto string = make_string(0, buffer);
    int_to_string(value, &string);
    print(string);
}

void float_to_string(float value, String* result)
{
    if (value < 0)
    {
        result->data[result->size++] = '-';
        value = -value;
    }
    int_to_string((s64)value, result);
    value = value - (int)value;
    auto fraction = (s64)(value * 100000);
    if (fraction == 0) { return; }
    push('.', result);
    while (fraction % 10 == 0) { fraction /= 10; }
    int_to_string(fraction, result);
}

void print(float value)
{
    char buffer[20];
    auto string = make_string(0, buffer);
    float_to_string(value, &string);
    print(string);
}

void panic_sdl(char* function_name)
{
    print(function_name);
    print(": ");
    print((char*)SDL_GetError());
    print("\n");
    ExitProcess(1);
}
