#include <windows.h>
#include <cwchar>
#include <cstdarg>

void win_printf(HANDLE consoleHandle, const wchar_t* format, ...) {
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, sizeof(buffer)/sizeof(wchar_t), format, args);
    va_end(args);
    WriteConsoleW(consoleHandle, buffer, wcslen(buffer), NULL, NULL);
}

void win_scanf(HANDLE consoleHandle, const wchar_t* format, ...) {
    wchar_t buffer[1024];
    DWORD charsRead;
    ReadConsoleW(consoleHandle, buffer, sizeof(buffer)/sizeof(wchar_t) - 1, &charsRead, NULL);
    buffer[charsRead] = L'\0';
    
    va_list args;
    va_start(args, format);
    vswscanf(buffer, format, args);
    va_end(args);
}
