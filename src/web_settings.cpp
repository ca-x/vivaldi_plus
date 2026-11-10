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
std::wstring configPath;
std::atomic<bool> ready{false};
std::atomic<bool> initialized{false};
struct SharedService {
    volatile LONG published;
    unsigned short port;
    unsigned char token[32];
};
std::atomic<SharedService*> sharedService{nullptr};
HANDLE installationMutex = nullptr;
HANDLE serviceMapping = nullptr;
HANDLE initialAttempt = nullptr;
WebSettingsReady onServiceReady = nullptr;

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

bool ReadConfigAt(const std::wstring& path, Configuration& config, bool decodeText = true)
{
    File file;
    file.handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file.handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    config.exists = true;
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file.handle, &size) || size.QuadPart < 0 || size.QuadPart > web_http::kMaxBody) return false;
    config.bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!config.bytes.empty() && (!ReadFile(file.handle, config.bytes.data(), static_cast<DWORD>(config.bytes.size()), &read, nullptr) || read != config.bytes.size())) return false;
    return !decodeText || Decode(config.bytes, config.text);
}

bool ReadConfig(Configuration& config)
{
    return ReadConfigAt(configPath, config);
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
    if (!success) { DeleteFileW(temp); return Error(500, "Could not write temporary configuration; original file retained"); }
    if (previous.exists) {
        const std::wstring displaced = std::wstring(temp) + L".previous";
        const std::wstring rollback = std::wstring(temp) + L".rollback";
        // ReplaceFile captures the actual file present at replacement time, closing
        // the gap between our last revision check and the filesystem operation.
        if (GetFileAttributesW(displaced.c_str()) != INVALID_FILE_ATTRIBUTES || GetFileAttributesW(rollback.c_str()) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileW(temp);
            return Error(500, "Configuration recovery filename is already in use");
        }
        if (!ReplaceFileW(configPath.c_str(), temp, displaced.c_str(), 0, nullptr, nullptr)) {
            DeleteFileW(temp);
            return Error(500, "Could not replace configuration; any recovery backup was retained");
        }
        Configuration captured;
        if (!ReadConfigAt(displaced, captured, false) || captured.Revision() != previous.Revision()) {
            Configuration target;
            if (ReadConfigAt(configPath, target, false) && target.exists && target.bytes == request.body) {
                // Roll back only while our own candidate is still current. A second
                // backup preserves an editor change racing with this rollback too.
                if (ReplaceFileW(configPath.c_str(), displaced.c_str(), rollback.c_str(), 0, nullptr, nullptr)) {
                    Configuration restored;
                    if (ReadConfigAt(rollback, restored, false) && restored.exists && restored.bytes == request.body) DeleteFileW(rollback.c_str());
                }
            }
            if (GetFileAttributesW(displaced.c_str()) != INVALID_FILE_ATTRIBUTES || GetFileAttributesW(rollback.c_str()) != INVALID_FILE_ATTRIBUTES) {
                const auto message = "Configuration changed during saving; reload before saving. Recovery files retained beside config.ini with prefix " + Utf8(temp);
                return Error(409, message.c_str());
            }
            return Error(409, "Configuration changed during saving; external changes restored. Reload before saving");
        }
        DeleteFileW(displaced.c_str());
    } else if (!MoveFileExW(temp, configPath.c_str(), MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp);
        Configuration target;
        if (ReadConfigAt(configPath, target, false) && target.exists) return Error(409, "Configuration was created externally; reload before saving");
        return Error(500, "Could not create configuration");
    }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, configPath.c_str());
    Configuration saved;
    if (!ReadConfig(saved)) return Error(500, "Saved configuration could not be read");
    if (!saved.exists || saved.bytes != request.body) return Error(409, "Configuration changed after saving; reload before saving");
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
        {"/settings.js", IDR_WEB_SETTINGS_JS, "text/javascript; charset=utf-8"},
        {"/logo.png", IDR_WEB_LOGO_PNG, "image/png"},
        {"/logo.css", IDR_WEB_LOGO_CSS, "text/css; charset=utf-8"},
        {"/logo.js", IDR_WEB_LOGO_JS, "text/javascript; charset=utf-8"}
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

bool BecomeOwner()
{
    auto state = sharedService.load(std::memory_order_acquire);
    const bool published = InterlockedCompareExchange(&state->published, 0, 0) == 1;
    unsigned char random[32] = {};
    unsigned short port = published ? state->port : 0;
    if (published) std::memcpy(random, state->token, sizeof(random));
    else if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
    constexpr char hex[] = "0123456789abcdef";
    token.clear();
    for (unsigned char c : random) { token += hex[c >> 4]; token += hex[c & 15]; }
    // An abandoned owner may still have sockets being torn down by the kernel.
    // A published port is never replaced with a different one during takeover.
    bool bound = false;
    for (int attempt = 0; attempt < (published ? 40 : 1); attempt++) {
        if (web_http::Start(Handle, port)) { bound = true; break; }
        Sleep(50);
    }
    if (!bound) return false;
    authority = "127.0.0.1:" + std::to_string(port);
    origin = "http://" + authority;
    if (!published) {
        state->port = port;
        std::memcpy(state->token, random, sizeof(random));
        InterlockedExchange(&state->published, 1);
    }
    ready.store(true, std::memory_order_release);
    if (onServiceReady) onServiceReady(GetWebSettingsUrl());
    return true;
}

DWORD WINAPI GuardService(void*)
{
    bool startup = true;
    for (;;) {
        const DWORD result = WaitForSingleObject(installationMutex, startup ? 0 : INFINITE);
        if (result == WAIT_TIMEOUT) {
            SetEvent(initialAttempt);
            startup = false;
            continue;
        }
        if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
            SetEvent(initialAttempt);
            DebugLog(L"Web settings service lease could not be acquired");
            return 0;
        }
        const bool owner = BecomeOwner();
        SetEvent(initialAttempt);
        startup = false;
        if (owner) {
            // Mutex ownership belongs to this thread, which must outlive the HTTP
            // worker. Process exit abandons the lease for another browser instance.
            Sleep(INFINITE);
            return 0;
        }
        ReleaseMutex(installationMutex);
        DebugLog(L"Web settings could not bind its shared loopback port; retrying");
        Sleep(1000);
    }
}

} // namespace

void InitializeWebSettings(WebSettingsReady on_ready)
{
    if (initialized.exchange(true, std::memory_order_acq_rel)) return;
    const auto app = GetAppDir();
    auto canonicalApp = app;
    if (!canonicalApp.empty()) CharLowerBuffW(canonicalApp.data(), static_cast<DWORD>(canonicalApp.size()));
    const auto utf8 = Utf8(canonicalApp);
    const auto hash = Hex(Hash(utf8));
    const auto name = L"Local\\VivaldiPlusSettings-" + std::wstring(hash.begin(), hash.end());
    installationMutex = CreateMutexW(nullptr, FALSE, name.c_str());
    if (!installationMutex) return;
    const auto mappingName = name + L"-State-v1";
    serviceMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedService), mappingName.c_str());
    auto state = serviceMapping ? static_cast<SharedService*>(MapViewOfFile(serviceMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedService))) : nullptr;
    initialAttempt = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!state || !initialAttempt) {
        if (state) UnmapViewOfFile(state);
        if (serviceMapping) CloseHandle(serviceMapping);
        if (initialAttempt) CloseHandle(initialAttempt);
        CloseHandle(installationMutex);
        installationMutex = serviceMapping = initialAttempt = nullptr;
        return;
    }
    configPath = app + L"\\config.ini";
    onServiceReady = on_ready;
    sharedService.store(state, std::memory_order_release);
    HANDLE thread = CreateThread(nullptr, 0, GuardService, nullptr, 0, nullptr);
    if (!thread) {
        // Keep the published view mapped for any concurrent URL getter. It can
        // still describe a service owned by another process of this installation.
        CloseHandle(initialAttempt);
        CloseHandle(installationMutex);
        installationMutex = initialAttempt = nullptr;
        return;
    }
    CloseHandle(thread);
    // The first owner finishes its about-page callback before browser entry. A
    // standby process signals immediately and only waits for a future takeover.
    WaitForSingleObject(initialAttempt, INFINITE);
}

std::wstring GetWebSettingsUrl()
{
    auto state = sharedService.load(std::memory_order_acquire);
    if (!state || InterlockedCompareExchange(&state->published, 0, 0) != 1) return {};
    return L"http://127.0.0.1:" + std::to_wstring(state->port) + L"/settings.html";
}
