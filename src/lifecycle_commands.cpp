#include "lifecycle_commands.h"
#include <windows.h>
#include <shellapi.h>
#include <vector>

void RunLifecycleCommand(const std::wstring& command, const std::wstring& appDir, bool wait) {
    if (command.empty()) return;
    std::wstring expanded = command;
    size_t position = 0;
    while ((position = expanded.find(L"%app%", position)) != std::wstring::npos) {
        expanded.replace(position, 5, appDir);
        position += appDir.size();
    }
    DWORD size = ExpandEnvironmentStringsW(expanded.c_str(), nullptr, 0);
    if (!size || size > 32767) return;
    std::vector<wchar_t> buffer(size);
    if (ExpandEnvironmentStringsW(expanded.c_str(), buffer.data(), size) != size) return;
    int count = 0;
    auto args = CommandLineToArgvW(buffer.data(), &count);
    if (!args) return;
    std::wstring executable = count ? args[0] : L"";
    LocalFree(args);
    if (executable.empty()) return;
    if (executable.find_first_of(L"\\/") != std::wstring::npos &&
        !(executable.size() > 1 && executable[1] == L':') &&
        executable.front() != L'\\' && executable.front() != L'/') {
        executable = appDir + L"\\" + executable;
    }
    if (executable.find_first_of(L"\\/:") == std::wstring::npos) {
        std::wstring local = appDir + L"\\" + executable;
        if (GetFileAttributesW(local.c_str()) != INVALID_FILE_ATTRIBUTES) {
            executable = local;
        } else {
            std::vector<wchar_t> path(32768);
            DWORD length = SearchPathW(nullptr, executable.c_str(), L".exe", static_cast<DWORD>(path.size()), path.data(), nullptr);
            if (!length || length >= path.size()) return;
            executable.assign(path.data(), length);
        }
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, appDir.c_str(), &startup, &process)) {
        OutputDebugStringW(L"[vivaldi++] Lifecycle command could not be started.\n");
        return;
    }
    CloseHandle(process.hThread);
    if (wait) WaitForSingleObject(process.hProcess, 3000);
    CloseHandle(process.hProcess);
}

