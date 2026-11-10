#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <string>

BOOL WINAPI InterleavedReplace(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID);
#define ReplaceFileW InterleavedReplace
#include "../src/web_settings.cpp"
#undef ReplaceFileW

HMODULE hInstance = nullptr;
namespace {
std::wstring testDirectory;
std::string externalChange;
bool injectBeforeReplace = false;
bool injectAfterReplace = false;

void Require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

void WriteBytes(const std::wstring& path, const std::string& bytes)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(file != INVALID_HANDLE_VALUE, "fixture file creation failed");
    DWORD written = 0;
    Require(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size(), "fixture write failed");
    CloseHandle(file);
}

std::string Ini(const wchar_t* text)
{
    std::string bytes("\xff\xfe", 2);
    bytes.append(reinterpret_cast<const char*>(text), std::wcslen(text) * sizeof(wchar_t));
    return bytes;
}

web_http::Request SaveRequest(const std::string& bytes)
{
    Configuration config;
    Require(ReadConfig(config), "fixture config read failed");
    web_http::Request request;
    request.method = "PUT";
    request.path = "/api/config";
    request.body = bytes;
    request.headers = {{"origin", origin}, {"content-type", "application/octet-stream"}, {"x-vivaldi-plus-revision", config.Revision()}};
    return request;
}
}

std::wstring GetAppDir() { return testDirectory; }
void DebugLog(const wchar_t*, ...) {}
namespace web_http { bool Start(Handler, unsigned short&) { return false; } }

BOOL WINAPI InterleavedReplace(LPCWSTR target, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved)
{
    if (injectBeforeReplace) { injectBeforeReplace = false; WriteBytes(target, externalChange); }
    BOOL result = ::ReplaceFileW(target, replacement, backup, flags, exclude, reserved);
    if (result && injectAfterReplace) { injectAfterReplace = false; WriteBytes(target, externalChange); }
    return result;
}

int main()
{
    wchar_t tempRoot[MAX_PATH] = {}, name[MAX_PATH] = {};
    Require(GetTempPathW(MAX_PATH, tempRoot) != 0 && GetTempFileNameW(tempRoot, L"vct", 0, name) != 0, "temporary directory setup failed");
    Require(DeleteFileW(name) && CreateDirectoryW(name, nullptr), "temporary directory creation failed");
    testDirectory = name;
    configPath = testDirectory + L"\\config.ini";
    origin = "http://127.0.0.1:16877";
    const auto original = Ini(L"[general]\r\ncommand_line=original\r\n");
    const auto candidate = Ini(L"[general]\r\ncommand_line=candidate\r\n");
    externalChange = Ini(L"[general]\r\ncommand_line=external\r\n");

    WriteBytes(configPath, original);
    auto request = SaveRequest(candidate);
    injectBeforeReplace = true;
    auto response = WriteConfig(request);
    Configuration current;
    Require(response.status == 409 && ReadConfig(current) && current.bytes == externalChange, "external change between last revision check and replacement was lost");

    WriteBytes(configPath, original);
    request = SaveRequest(candidate);
    injectAfterReplace = true;
    response = WriteConfig(request);
    current = {};
    Require(response.status == 409 && ReadConfig(current) && current.bytes == externalChange, "external change after replacement was not reported as a conflict");

    WriteBytes(configPath, original);
    response = WriteConfig(SaveRequest(candidate));
    current = {};
    Require(response.status == 200 && ReadConfig(current) && current.bytes == candidate, "normal atomic config save failed");
    Require(DeleteFileW(configPath.c_str()) && RemoveDirectoryW(testDirectory.c_str()), "test fixtures or recovery files were left behind");
    std::cout << "Configuration atomic replacement and external-edit race tests passed\n";
    return 0;
}
