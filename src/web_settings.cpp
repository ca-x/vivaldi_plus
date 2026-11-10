#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include "web_settings.h"
#include "web_http.h"
#include "web_resources.h"

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

extern HMODULE hInstance;
std::wstring GetAppDir();
void DebugLog(const wchar_t* format, ...);

namespace {

std::string token, origin, authority;
std::wstring configPath, settingsUrl;
std::atomic<bool> ready{false};
HANDLE installationMutex = nullptr;

struct File {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};

uint64_t Hash(std::string_view bytes)
{
    uint64_t result = 14695981039346656037ull;
    for (unsigned char c : bytes) { result ^= c; result *= 1099511628211ull; }
    return result;
}

std::string Hex(uint64_t value)
{
    char buffer[17] = {};
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
    return buffer;
}

std::string Utf8(std::wstring_view value)
{
    if (value.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}

std::string Json(std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 0x20) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
        else result += static_cast<char>(c);
    }
    return result + '"';
}

bool Decode(const std::string& bytes, std::wstring& result, bool requireUtf16 = false)
{
    const bool utf16 = bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xff && static_cast<unsigned char>(bytes[1]) == 0xfe;
    if (requireUtf16 && !utf16) return false;
    if (utf16) {
        if (bytes.size() % 2) return false;
        result.resize((bytes.size() - 2) / 2);
        if (!result.empty()) std::memcpy(result.data(), bytes.data() + 2, bytes.size() - 2);
        for (size_t i = 0; i < result.size(); i++) {
            wchar_t c = result[i];
            if (c == 0 || (c >= 0xdc00 && c <= 0xdfff)) return false;
            if (c >= 0xd800 && c <= 0xdbff) {
                if (++i == result.size() || result[i] < 0xdc00 || result[i] > 0xdfff) return false;
            }
        }
        return true;
    }
    size_t skip = bytes.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    if (bytes.size() == skip) { result.clear(); return true; }
    if (bytes.find('\0') != std::string::npos) return false;
    UINT codepage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int count = MultiByteToWideChar(codepage, flags, bytes.data() + skip, static_cast<int>(bytes.size() - skip), nullptr, 0);
    if (count <= 0 && skip == 0) {
        codepage = CP_ACP; flags = 0;
        count = MultiByteToWideChar(codepage, flags, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    }
    if (count <= 0) return false;
    result.resize(count);
    return MultiByteToWideChar(codepage, flags, bytes.data() + skip, static_cast<int>(bytes.size() - skip), result.data(), count) == count;
}

struct Configuration {
    bool exists = false;
    std::string bytes;
    std::wstring text;
    std::string Revision() const { return exists ? Hex(Hash(bytes)) : "missing"; }
    std::string JsonBody() const {
        return "{\"text\":" + Json(Utf8(text)) + ",\"exists\":" + (exists ? "true" : "false")
            + ",\"revision\":" + Json(Revision()) + ",\"path\":" + Json(Utf8(configPath)) + "}";
    }
};

bool ReadConfig(Configuration& config)
{
    File file;
    file.handle = CreateFileW(configPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file.handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    config.exists = true;
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file.handle, &size) || size.QuadPart < 0 || size.QuadPart > web_http::kMaxBody) return false;
    config.bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!config.bytes.empty() && (!ReadFile(file.handle, config.bytes.data(), static_cast<DWORD>(config.bytes.size()), &read, nullptr) || read != config.bytes.size())) return false;
    return Decode(config.bytes, config.text);
}

web_http::Response Error(int status, const char* message)
{
    return {status, "application/json; charset=utf-8", "{\"error\":" + Json(message) + "}"};
}

web_http::Response WriteConfig(const web_http::Request& request)
{
    if (request.headers.find("origin") == request.headers.end() || request.headers.at("origin") != origin) return Error(403, "Origin rejected");
    if (request.headers.find("content-type") == request.headers.end() || request.headers.at("content-type") != "application/octet-stream") return Error(415, "Expected application/octet-stream");
    std::wstring text;
    if (!Decode(request.body, text, true)) return Error(400, "Expected valid UTF-16LE INI with BOM");
    Configuration previous;
    if (!ReadConfig(previous)) return Error(500, "Could not read configuration");
    auto revision = request.headers.find("x-vivaldi-plus-revision");
    if (revision == request.headers.end() || revision->second != previous.Revision()) return Error(409, "Configuration changed; reload before saving");
    wchar_t temp[MAX_PATH] = {};
    if (!GetTempFileNameW(GetAppDir().c_str(), L"vpp", 0, temp)) return Error(500, "Could not create temporary configuration");
    bool success = false;
    {
        File file;
        file.handle = CreateFileW(temp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD written = 0;
        success = file.handle != INVALID_HANDLE_VALUE && WriteFile(file.handle, request.body.data(), static_cast<DWORD>(request.body.size()), &written, nullptr)
            && written == request.body.size() && FlushFileBuffers(file.handle);
    }
    Configuration current;
    if (success && (!ReadConfig(current) || current.Revision() != previous.Revision())) {
        DeleteFileW(temp);
        return Error(409, "Configuration changed; reload before saving");
    }
    if (success) success = previous.exists ? ReplaceFileW(configPath.c_str(), temp, nullptr, 0, nullptr, nullptr) != FALSE
        : MoveFileExW(temp, configPath.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!success) { DeleteFileW(temp); return Error(500, "Could not save configuration; original file retained"); }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, configPath.c_str());
    Configuration saved;
    if (!ReadConfig(saved)) return Error(500, "Saved configuration could not be read");
    return {200, "application/json; charset=utf-8", saved.JsonBody()};
}

web_http::Response Handle(const web_http::Request& request)
{
    if (!ready.load(std::memory_order_acquire)) return Error(503, "Settings are initializing");
    if (request.headers.at("host") != authority) return Error(403, "Host rejected");
    if (request.path == "/api/config") {
        auto supplied = request.headers.find("x-vivaldi-plus-token");
        if (supplied == request.headers.end() || supplied->second != token) return Error(403, "Session rejected");
        if (request.method == "PUT") return WriteConfig(request);
        if (request.method != "GET" || !request.body.empty()) return Error(405, "Method rejected");
        Configuration config;
        if (!ReadConfig(config)) return Error(500, "Could not read configuration");
        return {200, "application/json; charset=utf-8", config.JsonBody()};
    }
    if (request.method != "GET" || !request.body.empty()) return Error(405, "Method rejected");
    const auto path = request.path.substr(0, request.path.find('?'));
    struct Asset { const char* path; int id; const char* type; };
    constexpr Asset assets[] = {
        {"/settings.html", IDR_WEB_SETTINGS_HTML, "text/html; charset=utf-8"},
        {"/settings.css", IDR_WEB_SETTINGS_CSS, "text/css; charset=utf-8"},
        {"/ini.js", IDR_WEB_INI_JS, "text/javascript; charset=utf-8"},
        {"/i18n.js", IDR_WEB_I18N_JS, "text/javascript; charset=utf-8"},
        {"/settings.js", IDR_WEB_SETTINGS_JS, "text/javascript; charset=utf-8"}
    };
    for (const auto& asset : assets) {
        if (path != asset.path) continue;
        HRSRC resource = FindResourceW(hInstance, MAKEINTRESOURCEW(asset.id), RT_RCDATA);
        HGLOBAL loaded = resource ? LoadResource(hInstance, resource) : nullptr;
        const auto data = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
        if (!data) return Error(500, "Embedded settings resource unavailable");
        std::string body(data, SizeofResource(hInstance, resource));
        if (asset.id == IDR_WEB_SETTINGS_HTML) {
            constexpr std::string_view marker = "__VIVALDI_PLUS_TOKEN__";
            const auto position = body.find(marker);
            if (position == std::string::npos) return Error(500, "Embedded settings session marker unavailable");
            body.replace(position, marker.size(), token);
        }
        return {200, asset.type, std::move(body)};
    }
    return Error(404, "Not found");
}

} // namespace

void InitializeWebSettings()
{
    if (installationMutex) return;
    const auto app = GetAppDir();
    auto canonicalApp = app;
    if (!canonicalApp.empty()) CharLowerBuffW(canonicalApp.data(), static_cast<DWORD>(canonicalApp.size()));
    const auto utf8 = Utf8(canonicalApp);
    const auto hash = Hex(Hash(utf8));
    const auto name = L"Local\\VivaldiPlusSettings-" + std::wstring(hash.begin(), hash.end());
    SetLastError(ERROR_SUCCESS);
    installationMutex = CreateMutexW(nullptr, FALSE, name.c_str());
    if (!installationMutex || GetLastError() == ERROR_ALREADY_EXISTS) return;
    unsigned char random[32] = {};
    if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) { DebugLog(L"Web settings random token generation failed"); return; }
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : random) { token += hex[c >> 4]; token += hex[c & 15]; }
    configPath = app + L"\\config.ini";
    unsigned short port = 0;
    if (!web_http::Start(Handle, port)) { DebugLog(L"Web settings could not bind a loopback port"); return; }
    authority = "127.0.0.1:" + std::to_string(port);
    origin = "http://" + authority;
    const auto url = origin + "/settings.html";
    settingsUrl.assign(url.begin(), url.end());
    ready.store(true, std::memory_order_release);
}

std::wstring GetWebSettingsUrl()
{
    return settingsUrl;
}
