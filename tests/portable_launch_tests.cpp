#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

static std::wstring app_directory;
static int module_failure = 0;
static int launch_calls = 0;
static std::vector<HANDLE> launch_handles;
static bool launch_contract = false;

static DWORD WINAPI TestModuleFileName(HMODULE module, LPWSTR path, DWORD size) {
    if (module_failure == 1) { SetLastError(ERROR_ACCESS_DENIED); return 0; }
    if (module_failure == 2) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return size; }
    return GetModuleFileNameW(module, path, size);
}
static BOOL WINAPI TestCreateProcess(LPCWSTR app, LPWSTR command, LPSECURITY_ATTRIBUTES process,
    LPSECURITY_ATTRIBUTES thread, BOOL inherit, DWORD flags, LPVOID environment, LPCWSTR cwd,
    LPSTARTUPINFOW startup, LPPROCESS_INFORMATION info) {
    ++launch_calls;
    wchar_t actual[32768];
    GetModuleFileNameW(nullptr, actual, 32768);
    launch_contract = app && std::wstring(app) == actual && command && command[0] == L'"' &&
        !inherit && cwd && cwd == app_directory && startup &&
        (startup->dwFlags & STARTF_USESHOWWINDOW) && startup->wShowWindow == SW_SHOWNORMAL;
    const BOOL result = CreateProcessW(app, command, process, thread, inherit, flags, environment, cwd, startup, info);
    if (result) launch_handles = {info->hProcess, info->hThread};
    return result;
}
static BOOL WINAPI TestCloseHandle(HANDLE handle) {
    const BOOL result = CloseHandle(handle);
    if (result) launch_handles.erase(std::remove(launch_handles.begin(), launch_handles.end(), handle), launch_handles.end());
    return result;
}
[[noreturn]] static void WINAPI TestExitProcess(UINT code) {
    ExitProcess(launch_calls == 1 && launch_contract && launch_handles.empty() ? code : 81);
}
#define GetModuleFileNameW TestModuleFileName
#define CreateProcessW TestCreateProcess
#define CloseHandle TestCloseHandle
#define ExitProcess TestExitProcess
#include "../src/portable.h"
#undef GetModuleFileNameW
#undef CreateProcessW
#undef CloseHandle
#undef ExitProcess

std::wstring GetAppDir() { return app_directory; }
void DebugLog(const wchar_t*, ...) {}
std::wstring GetAbsolutePath(std::wstring_view path) { return std::filesystem::absolute(path).wstring(); }
std::wstring ExpandEnvironmentPath(std::wstring_view path) {
    std::wstring source(path);
    std::wstring result(ExpandEnvironmentStringsW(source.c_str(), nullptr, 0), L'\0');
    ExpandEnvironmentStringsW(source.c_str(), result.data(), static_cast<DWORD>(result.size()));
    result.pop_back();
    return result;
}
void ReplaceStringInPlace(std::wstring& value, std::wstring_view from, std::wstring_view to) {
    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::wstring::npos) {
        value.replace(pos, from.size(), to); pos += to.size();
    }
}
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static std::wstring Executable() {
    wchar_t path[32768];
    Check(GetModuleFileNameW(nullptr, path, 32768) != 0, "module path");
    return path;
}
static void WriteString(std::ofstream& file, const std::wstring& value) {
    const DWORD length = static_cast<DWORD>(value.size());
    file.write(reinterpret_cast<const char*>(&length), sizeof(length));
    file.write(reinterpret_cast<const char*>(value.data()), length * sizeof(wchar_t));
}
static std::wstring ReadString(std::ifstream& file) {
    DWORD length = 0;
    file.read(reinterpret_cast<char*>(&length), sizeof(length));
    Check(file.good() && length < 32768, "report string length");
    std::wstring value(length, L'\0');
    file.read(reinterpret_cast<char*>(value.data()), length * sizeof(wchar_t));
    Check(file.good(), "report contents");
    return value;
}
static void Record(const std::filesystem::path& report, int argc, wchar_t** argv) {
    wchar_t cwd[32768];
    Check(GetCurrentDirectoryW(32768, cwd) != 0, "child cwd");
    std::ofstream file(report.wstring() + L".writing", std::ios::binary);
    WriteString(file, std::to_wstring(GetCurrentProcessId()));
    WriteString(file, cwd);
    for (int i = 0; i < argc; ++i) WriteString(file, argv[i]);
    file.close();
    std::filesystem::rename(report.wstring() + L".writing", report);
}
static DWORD Run(const std::wstring& executable, const std::wstring& arguments, const std::wstring& cwd) {
    std::wstring command = L"\"" + executable + L"\" " + arguments;
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION info{};
    Check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
        nullptr, cwd.c_str(), &startup, &info) != FALSE, "start regression host");
    CloseHandle(info.hThread);
    const DWORD waited = WaitForSingleObject(info.hProcess, 15000);
    if (waited != WAIT_OBJECT_0) TerminateProcess(info.hProcess, 82);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    Check(waited == WAIT_OBJECT_0, "regression host timeout");
    return code;
}
static void RoundTrip(const std::wstring& executable, const std::filesystem::path& root, bool single) {
    const auto report = root / (single ? L"single.bin" : L"roundtrip.bin");
    const std::vector<std::wstring> values = {L"--test-relaunch", L"--report=" + report.wstring(),
        L"--user-data-dir=C:\\Explicit Profile\\", L"--disk-cache-dir=C:\\Explicit Cache\\",
        L"", L"Unicode \u6d4b\u8bd5", L"literal\"quote", L"slashes\\\"quote", L"C:\\tail space\\"};
    std::wstring args = JoinArgsString(values, L" ");
    if (single) args += L" --single-argument C:\\raw unquoted\\file.txt";
    else args += L" -- --single-argument sentinel data";
    std::wstring original = L"\"" + executable + L"\" " + args;
    const auto expected = ParseCommandLineArgs(L"host.exe " + GetCommand(original.data()));
    Check(Run(executable, args, root.wstring()) == 0, "portable launch contract or handle closure");
    for (int i = 0; i < 300 && !std::filesystem::exists(report); ++i) Sleep(50);
    Check(std::filesystem::exists(report), "relaunched child did not report");
    std::ifstream file(report, std::ios::binary);
    const DWORD child_id = std::stoul(ReadString(file));
    Check(ReadString(file) == std::filesystem::path(executable).parent_path().wstring(), "child cwd must be executable directory");
    Check(ReadString(file) == executable, "child argv[0]");
    std::vector<std::wstring> actual;
    while (file.peek() != std::char_traits<char>::eof()) actual.push_back(ReadString(file));
    Check(actual == expected, "native child argv must preserve GetCommand result");
    Check(std::find(actual.begin(), actual.end(), values[2]) != actual.end(), "explicit profile changed");
    Check(std::find(actual.begin(), actual.end(), values[3]) != actual.end(), "explicit cache changed");
    HANDLE child = OpenProcess(SYNCHRONIZE, FALSE, child_id);
    if (child) {
        const DWORD waited = WaitForSingleObject(child, 15000);
        CloseHandle(child);
        Check(waited == WAIT_OBJECT_0, "reporting child timeout");
    } else Check(GetLastError() == ERROR_INVALID_PARAMETER, "open reporting child");
}
int wmain(int argc, wchar_t** argv) {
    std::filesystem::path cleanup;
    try {
        app_directory = std::filesystem::path(Executable()).parent_path().wstring();
        if (argc > 1 && std::wstring_view(argv[1]) == L"--failure") {
            std::wstring command = GetCommandLineW();
            for (module_failure = 1; module_failure <= 2; ++module_failure) {
                launch_calls = 0;
                Portable(command.data());
                Check(launch_calls == 0, "module failure/truncation must not launch");
            }
            module_failure = 0;
            app_directory += L"\\directory-that-does-not-exist";
            Portable(command.data());
            Check(launch_calls == 1 && launch_handles.empty(), "native CreateProcess failure must return");
            return 0;
        }
        bool marker = false;
        std::wstring report;
        for (int i = 1; i < argc; ++i) {
            if (std::wstring_view(argv[i]) == L"--gopher") marker = true;
            if (std::wstring_view(argv[i]).starts_with(L"--report=")) report = argv[i] + 9;
        }
        if (!report.empty()) {
            if (marker) Record(report, argc, argv);
            else { std::wstring command = GetCommandLineW(); Portable(command.data()); return 83; }
            return 0;
        }
        wchar_t temp[32768];
        Check(GetTempPathW(32768, temp) != 0, "temp directory");
        cleanup = std::filesystem::path(temp) / (L"portable launch \u6d4b\u8bd5 " + std::to_wstring(GetCurrentProcessId()));
        std::filesystem::create_directories(cleanup / L"app directory");
        const auto executable = cleanup / L"app directory" / L"launch host \u6d4b\u8bd5.exe";
        std::filesystem::copy_file(Executable(), executable);
        RoundTrip(executable.wstring(), cleanup, false);
        RoundTrip(executable.wstring(), cleanup, true);
        Check(Run(executable.wstring(), L"--failure", cleanup.wstring()) == 0, "failure paths must retain original process");
        std::filesystem::remove_all(cleanup);
        std::cout << "Portable launch regression tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (!cleanup.empty()) { std::error_code ignored; std::filesystem::remove_all(cleanup, ignored); }
        return 1;
    }
}
