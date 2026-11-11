#include <windows.h>
#include <string>
#include <cwchar>
#include <cstdlib>
#include <iostream>
#include <atomic>
#include <cstring>
#include "../src/lifecycle.h"
std::wstring appDir;
std::wstring GetAppDir() { return appDir; }
std::atomic<bool> holdRegistration{false};
void LifecycleTestBeforeJoin() {
    const ULONGLONG deadline = GetTickCount64() + 10000;
    while (holdRegistration.load() && GetTickCount64() < deadline) Sleep(10);
}
void LifecycleTestAfterJoin() { std::cout << "registered" << std::endl; }
int wmain(int argc, wchar_t** argv) {
    if (argc != 8) return 2;
    appDir = argv[1];
    holdRegistration.store(wcscmp(argv[7], L"hold") == 0);
    LifecycleOptions options{wcscmp(argv[3], L"on") == 0, argv[4], argv[5]};
    InitializeLifecycle(options);
    InitializeLifecycle(options);
    if (wcscmp(argv[2], L"none") != 0) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"Chrome_MessageWindow";
        RegisterClassW(&wc);
        CreateWindowExW(0, wc.lpszClassName, argv[2], 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    }
    std::cout << "ready" << std::endl;
    const ULONGLONG deadline = GetTickCount64() + wcstoul(argv[6], nullptr, 10);
    bool running = true;
    while (running && GetTickCount64() < deadline) {
        DWORD available = 0;
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr) && available) {
            char text[128]{}; DWORD length = 0;
            if (ReadFile(input, text, sizeof(text) - 1, &length, nullptr)) {
                if (strstr(text, "join")) holdRegistration.store(false);
                if (strstr(text, "exit")) running = false;
            }
        }
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    ExitProcess(0);
}
