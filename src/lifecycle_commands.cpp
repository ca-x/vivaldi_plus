#include "lifecycle_commands.h"
#include <windows.h>
#include <shellapi.h>
#include <vector>

namespace {
bool AbsoluteDirectory(const std::wstring& path) {
    return (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) ||
           (path.size() >= 3 && path[0] == L'\\' && path[1] == L'\\');
}
std::wstring FindExecutable(const std::wstring& executable, const std::wstring& appDir) {
    std::vector<std::wstring> directories{appDir};
    wchar_t system[32768]{};
    UINT length = GetSystemDirectoryW(system, 32768);
    if (length && length < 32768) directories.emplace_back(system, length);
    length = GetWindowsDirectoryW(system, 32768);
    if (length && length < 32768) directories.emplace_back(system, length);
    DWORD size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (size && size <= 32767) {
        std::vector<wchar_t> environment(size);
        DWORD actual = GetEnvironmentVariableW(L"PATH", environment.data(), size);
        if (actual && actual < size) {
            std::wstring path(environment.data(), actual);
            size_t start = 0;
            while (start <= path.size()) {
                size_t end = path.find(L';', start);
                std::wstring directory = path.substr(start, end == std::wstring::npos ? end : end - start);
                if (directory.size() >= 2 && directory.front() == L'"' && directory.back() == L'"')
                    directory = directory.substr(1, directory.size() - 2);
                if (AbsoluteDirectory(directory)) directories.push_back(directory);
                if (end == std::wstring::npos) break;
                start = end + 1;
            }
        }
    }
    std::vector<wchar_t> found(32768);
    for (const auto& directory : directories) {
        if (!AbsoluteDirectory(directory)) continue;
        // A non-null explicit path suppresses SearchPath's inherited-CWD search.
        DWORD result = SearchPathW(directory.c_str(), executable.c_str(), L".exe",
                                   static_cast<DWORD>(found.size()), found.data(), nullptr);
        if (result && result < found.size()) return std::wstring(found.data(), result);
    }
    return {};
}
} // namespace

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
        executable = FindExecutable(executable, appDir);
        if (executable.empty()) {
            OutputDebugStringW(L"[vivaldi++] Lifecycle executable could not be resolved.\n");
            return;
        }
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, appDir.c_str(), &startup, &process)) {
        OutputDebugStringW(L"[vivaldi++] Lifecycle command could not be started.\n");
        return;
    }
    CloseHandle(process.hThread);
    if (wait) WaitForSingleObject(process.hProcess, 3000);
    CloseHandle(process.hProcess);
}
