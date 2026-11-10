#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <atomic>
#include <string>
#include "../src/web_settings.h"

HMODULE hInstance = nullptr;
static std::wstring app_directory;
static std::atomic<unsigned> readinessCalls{0};
static std::atomic<bool> announced{false};
static bool service_test = false;
static bool blocked_ready = false;
std::wstring GetAppDir() { return app_directory; }
void DebugLog(const wchar_t*, ...) {}

static void Ready(const std::wstring& url)
{
    const auto count = readinessCalls.fetch_add(1, std::memory_order_relaxed);
    if (blocked_ready && count == 0) {
        std::wprintf(L"callback-start\ncallback-url=%ls\n", url.c_str());
        std::fflush(stdout);
        char byte = 0;
        DWORD read = 0;
        ReadFile(GetStdHandle(STD_INPUT_HANDLE), &byte, 1, &read, nullptr);
    }
    if (service_test && announced.load(std::memory_order_acquire)) {
        std::wprintf(L"service-ready\n");
        std::fflush(stdout);
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2 || argc > 3) return 2;
    if (argc == 3) {
        const auto mode = std::wstring(argv[2]);
        if (mode != L"--service-test" && mode != L"--blocked-ready") return 2;
        service_test = true;
        blocked_ready = mode == L"--blocked-ready";
    }
    app_directory = argv[1];
    hInstance = GetModuleHandleW(nullptr);
    if (service_test) { std::wprintf(L"initializing\n"); std::fflush(stdout); }
    InitializeWebSettings(Ready);
    InitializeWebSettings(Ready);
    auto url = GetWebSettingsUrl();
    for (unsigned attempt = 0; url.empty() && attempt < 200; attempt++) {
        Sleep(25);
        url = GetWebSettingsUrl();
    }
    if (url.empty()) return 3;
    std::wprintf(L"%ls\n", url.c_str());
    if (service_test) std::wprintf(L"ready-count=%u\n", readinessCalls.load(std::memory_order_relaxed));
    std::fflush(stdout);
    announced.store(true, std::memory_order_release);
    Sleep(INFINITE);
    return 0;
}
