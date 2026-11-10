#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <string>
#include "../src/web_settings.h"

HMODULE hInstance = nullptr;
static std::wstring app_directory;
std::wstring GetAppDir() { return app_directory; }
void DebugLog(const wchar_t*, ...) {}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) return 2;
    app_directory = argv[1];
    hInstance = GetModuleHandleW(nullptr);
    InitializeWebSettings();
    const auto url = GetWebSettingsUrl();
    if (url.empty()) return 3;
    std::wprintf(L"%ls\n", url.c_str());
    std::fflush(stdout);
    Sleep(INFINITE);
    return 0;
}
