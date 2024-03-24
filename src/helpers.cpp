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

s64 absolute(s64 value) { return value >= 0 ? value : -value; }

s64 modulo(s64 dividend, s64 divisor) { return absolute(dividend % divisor); } 

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

#define panic(message) panic_implementation(message, __FILE__, __LINE__)

void panic_implementation(char* message, char* filename, int line);

bool g_stdout_initialization_failed;

void print(char* message, int length)
{
    if (g_stdout == NULL)
    {
        g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (g_stdout == NULL || g_stdout == INVALID_HANDLE_VALUE)
        {
            g_stdout_initialization_failed = true;
            panic("Stdout initialization failed");
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

#define assert(condition) assert_implementation(condition, __FILE__, __LINE__)

void assert_implementation(bool condition, char* filename, int line)
{
    if (!condition)
    {
        panic_implementation("Assertion failed", filename, line);
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

void panic_implementation(char* message, char* filename, int line)
{
    char buffer_data[256];
    auto buffer = make_string(0, buffer_data);
    push("[", &buffer);
    push(filename, &buffer);
    push(":", &buffer);
    int_to_string(line, &buffer);
    push("] ", &buffer);
    push(message, &buffer);
    if (!g_stdout_initialization_failed)
    {
        auto original_size = buffer.size;
        push("\n", &buffer);
        print(buffer.data, buffer.size);
        buffer.size = original_size;
    }
    push('\0', &buffer);
    MessageBoxA(NULL, buffer.data, "Error", MB_OK);
    ExitProcess(1);
}

#define panic_sdl(function_name) panic_sdl_implementation(function_name, __FILE__, __LINE__)

void panic_sdl_implementation(char* function_name, char* filename, int line)
{
    char buffer[256];
    auto message = make_string(0, buffer);
    push(function_name, &message);
    push(": ", &message);
    push((char*)SDL_GetError(), &message);
    push("\n", &message);
    push('\0', &message);
    panic_implementation(message.data, filename, line);
}

struct SystemTime
{
    u32 year;
    u32 month;
    u32 day;
    u32 hour;
    u32 minute;
    u32 second;
    u32 milliseconds;
};

SystemTime get_system_time()
{
    SystemTime result;
    SYSTEMTIME windows_system_time;
    GetSystemTime(&windows_system_time);
    result.year = windows_system_time.wYear;
    result.month = windows_system_time.wMonth;
    result.day = windows_system_time.wDay;
    result.hour = windows_system_time.wHour;
    result.minute = windows_system_time.wMinute;
    result.second = windows_system_time.wSecond;
    result.milliseconds = windows_system_time.wMilliseconds;
    return result;
}

s64 previous_random = 1;

s32 get_random_number() { return previous_random = previous_random * 1103515243 + 12345; }

void seed_random_number_generator(s32 seed) { previous_random = seed; }

s32 get_random_number_in_range(s32 min, s32 max) { return modulo(get_random_number(), max - min) + min; }
