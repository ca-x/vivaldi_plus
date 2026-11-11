#include <windows.h>
#include <string>
#include <cwchar>
#include <cstdlib>
int wmain(int argc, wchar_t** argv) {
    if (argc < 3) return 2;
    wchar_t no_console[4]{};
    if (GetEnvironmentVariableW(L"LIFECYCLE_EXPECT_NO_CONSOLE", no_console, 4) && GetConsoleWindow()) return 5;
    if (wcscmp(argv[1], L"/d") == 0) return 7;
    wchar_t expected[32768]{}, actual[32768]{};
    if (GetEnvironmentVariableW(L"LIFECYCLE_EXPECT_CWD", expected, 32768)) {
        if (!GetCurrentDirectoryW(32768, actual) || _wcsicmp(expected, actual)) return 4;
    }
    auto append = [&](const wchar_t* text) {
        HANDLE file = CreateFileW(argv[1], FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        std::string line;
        for (const wchar_t* p = text; *p; ++p) line += static_cast<char>(*p);
        line += '\n';
        DWORD written;
        bool ok = WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) != FALSE;
        CloseHandle(file);
        return ok;
    };
    if (!append(argv[2])) return 3;
    if (argc > 3) Sleep(wcstoul(argv[3], nullptr, 10));
    if (argc > 4 && !append(argv[4])) return 3;
    return 0;
}
