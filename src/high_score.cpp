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
