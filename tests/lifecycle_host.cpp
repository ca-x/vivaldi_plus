#include <windows.h>
#include <string>
#include <cwchar>
#include <cstdlib>
#include <iostream>
#include "../src/lifecycle.h"
std::wstring appDir;
std::wstring GetAppDir() { return appDir; }
int wmain(int argc, wchar_t** argv) {
    if (argc != 7) return 2;
    appDir = argv[1];
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
    while (GetTickCount64() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    ExitProcess(0);
}


