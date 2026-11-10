#include "about_integration.h"
#include "utils.h"
#include "version.h"
#include <array>

extern HMODULE hInstance;

namespace {
std::wstring LatestResourcePath()
{
    const auto app = GetAppDir();
    WIN32_FIND_DATAW data = {};
    HANDLE find = FindFirstFileW((app + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return {};
    std::array<unsigned long, 4> latest = {};
    std::wstring result;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        std::array<unsigned long, 4> version = {};
        wchar_t* end = nullptr;
        const wchar_t* part = data.cFileName;
        bool valid = true;
        for (size_t i = 0; i < version.size(); ++i) {
            if (!iswdigit(*part)) { valid = false; break; }
            version[i] = wcstoul(part, &end, 10);
            if ((i < 3 && *end != L'.') || (i == 3 && *end != L'\0')) { valid = false; break; }
            part = end + (i < 3 ? 1 : 0);
        }
        const auto path = app + L"\\" + data.cFileName + L"\\resources.pak";
        if (valid && version > latest && PathFileExistsW(path.c_str())) {
            latest = version;
            result = path;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return result;
}

bool ExtractTool(WORD id, const std::wstring& path)
{
    HRSRC resource = FindResourceW(hInstance, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!resource) return false;
    const DWORD size = SizeofResource(hInstance, resource);
    const void* bytes = LockResource(LoadResource(hInstance, resource));
    if (!bytes || !size) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes, size, &written, nullptr) && written == size;
    CloseHandle(file);
    return ok;
}

std::wstring ResourceStamp(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA attr = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr)) return {};
    return std::to_wstring(attr.ftLastWriteTime.dwHighDateTime) + L":" +
        std::to_wstring(attr.ftLastWriteTime.dwLowDateTime) + L":" + std::to_wstring(attr.nFileSizeLow);
}
}

void InitializeAboutPage(const std::wstring& settings_url)
{
    if (settings_url.empty()) return;
    if (PathFileExistsW((GetAppDir() + L"\\vivaldi-plus-about-disabled").c_str())) return;
    const auto pak = LatestResourcePath();
    if (pak.empty()) return;
    const auto tools = GetAppDir() + L"\\vivaldi-plus-tools";
    if (!CreateDirectoryW(tools.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return;
    const auto state = tools + L"\\integration.ini";
    const std::wstring version = L"" RELEASE_VER_STR;
    // A resource update, a changed endpoint, or a new DLL version invalidates the attempt.
    auto fingerprint = ResourceStamp(pak) + L"|" + settings_url + L"|" + version;
    wchar_t previous[1024] = {};
    GetPrivateProfileStringW(L"about", L"attempt", L"", previous, 1024, state.c_str());
    if (fingerprint == previous) return;
    const auto script = tools + L"\\Install-AboutPage.ps1";
    if (!ExtractTool(206, script) || !ExtractTool(207, tools + L"\\AboutPage.cs") ||
        !ExtractTool(208, tools + L"\\Install-VivaldiUI.ps1")) {
        DebugLog(L"Could not extract about-page integration tools");
        return;
    }
    wchar_t system[MAX_PATH] = {};
    if (!GetSystemDirectoryW(system, MAX_PATH)) return;
    const std::wstring powershell = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring command = L"\"" + powershell + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" +
        script + L"\" -AppDir \"" + GetAppDir() + L"\" -SettingsUrl \"" + settings_url + L"\" -Version \"" + version + L"\" -Startup";
    SECURITY_ATTRIBUTES security = {sizeof(security), nullptr, TRUE};
    HANDLE log = CreateFileW((tools + L"\\integration.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return;
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = log;
    startup.hStdError = log;
    PROCESS_INFORMATION process = {};
    if (CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, tools.c_str(), &startup, &process)) {
        if (WaitForSingleObject(process.hProcess, 20000) == WAIT_TIMEOUT) {
            TerminateProcess(process.hProcess, ERROR_TIMEOUT);
            WaitForSingleObject(process.hProcess, 1000);
            DebugLog(L"About-page integration timed out; see integration.log");
        }
        DWORD exit_code = 1;
        GetExitCodeProcess(process.hProcess, &exit_code);
        if (exit_code != 0) DebugLog(L"About-page integration failed (%lu); see integration.log", exit_code);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        fingerprint = ResourceStamp(pak) + L"|" + settings_url + L"|" + version;
        WritePrivateProfileStringW(L"about", L"attempt", fingerprint.c_str(), state.c_str());
    }
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    CloseHandle(log);
}
