#include "about_integration.h"
#include "about_patch.h"
#include "chrome_about.h"
#include "utils.h"
#include "version.h"
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>

namespace {
constexpr DWORD kChunkSize = 32 * 1024;
constexpr DWORD kMaxBundleSize = 64 * 1024 * 1024;

struct ReadFileHandle {
    HANDLE handle;
    explicit ReadFileHandle(const std::wstring& path)
        : handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
              nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr)) {}
    ~ReadFileHandle() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    ReadFileHandle(const ReadFileHandle&) = delete;
    ReadFileHandle& operator=(const ReadFileHandle&) = delete;
};

bool IntegrationDisabled() {
    return GetFileAttributesW((GetAppDir() + L"\\vivaldi-plus-about-disabled").c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring LatestBundle() {
    const auto app = GetAppDir();
    const bool chrome = GetFileAttributesW((app + L"\\Chrome.exe").c_str()) != INVALID_FILE_ATTRIBUTES;
    WIN32_FIND_DATAW entry = {};
    HANDLE find = FindFirstFileW((app + L"\\*").c_str(), &entry);
    if (find == INVALID_HANDLE_VALUE) return {};
    std::array<unsigned long, 4> latest = {};
    std::wstring result;
    do {
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        std::array<unsigned long, 4> version = {};
        const wchar_t* part = entry.cFileName;
        bool valid = true;
        for (size_t i = 0; i < version.size(); ++i) {
            if (*part < L'0' || *part > L'9') { valid = false; break; }
            unsigned long long number = 0;
            while (*part >= L'0' && *part <= L'9') {
                number = number * 10 + static_cast<unsigned>(*part++ - L'0');
                if (number > MAXDWORD) { valid = false; break; }
            }
            if (!valid || (i < 3 ? *part != L'.' : *part != L'\0')) { valid = false; break; }
            version[i] = static_cast<unsigned long>(number);
            if (i < 3) ++part;
        }
        if (!valid || version <= latest) continue;
        auto path = app + L"\\" + entry.cFileName + L"\\resources\\vivaldi\\bundle.js";
        auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES && chrome) {
            path = app + L"\\" + entry.cFileName + L"\\resources.pak";
            attributes = GetFileAttributesW(path.c_str());
        }
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            latest = version; result = path;
        }
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    return result;
}

bool ReadExact(HANDLE file, char* buffer, DWORD size) {
    DWORD read = 0;
    return ReadFile(file, buffer, size, &read, nullptr) && read == size;
}

bool ScanMarker(HANDLE file, DWORD remaining, std::string_view marker, bool& found) {
    constexpr auto overlap = (std::max)(kVivaldiAboutMarkerPrefix.size(), kChromeAboutMarker.size()) - 1;
    std::array<char, kChunkSize + overlap> buffer;
    size_t carry = 0;
    while (remaining != 0) {
        const DWORD size = (std::min)(remaining, kChunkSize);
        if (!ReadExact(file, buffer.data() + carry, size)) return false;
        const std::string_view window(buffer.data(), carry + size);
        if (window.find(marker) != std::string_view::npos) {
            found = true;
            return true;
        }
        carry = (std::min)(window.size(), marker.size() - 1);
        std::memmove(buffer.data(), window.data() + window.size() - carry, carry);
        remaining -= size;
    }
    found = false;
    return true;
}

bool ReadBundle(const std::wstring& path, std::string& bytes, BY_HANDLE_FILE_INFORMATION& info,
    std::string_view required_marker = {}) {
    const ReadFileHandle file(path);
    if (file.handle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(file.handle, &info) ||
        info.nFileSizeHigh != 0 || info.nFileSizeLow > kMaxBundleSize) return false;
    if (!required_marker.empty()) {
        bool found = false;
        if (!ScanMarker(file.handle, info.nFileSizeLow, required_marker, found)) return false;
        if (!found) { bytes.clear(); return true; }
        if (!SetFilePointerEx(file.handle, LARGE_INTEGER{}, nullptr, FILE_BEGIN)) return false;
    }
    bytes.resize(info.nFileSizeLow);
    return ReadExact(file.handle, bytes.data(), info.nFileSizeLow);
}

bool SameFile(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber && a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow &&
        a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow && CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) == 0;
}

bool MatchesBundle(const std::wstring& path, std::string_view source, const BY_HANDLE_FILE_INFORMATION& initial) {
    const ReadFileHandle file(path);
    BY_HANDLE_FILE_INFORMATION current = {};
    if (file.handle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(file.handle, &current) ||
        !SameFile(initial, current)) return false;
    std::array<char, kChunkSize> buffer;
    while (!source.empty()) {
        const auto size = static_cast<DWORD>((std::min)(source.size(), buffer.size()));
        if (!ReadExact(file.handle, buffer.data(), size) || std::memcmp(source.data(), buffer.data(), size) != 0)
            return false;
        source.remove_prefix(size);
    }
    return GetFileInformationByHandle(file.handle, &current) && SameFile(initial, current);
}

std::string Utf8(std::wstring_view text) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring TempPath(const std::wstring& bundle) {
    unsigned char random[12];
    if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return {};
    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring result = bundle + L".vivaldi-plus-tmp-";
    for (const auto byte : random) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}

bool WriteCandidate(const std::wstring& path, std::string_view bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) DeleteFileW(path.c_str());
    return ok;
}

bool MakeCandidate(const std::wstring& path, std::string_view source, std::string_view url,
    bool enabled, std::string& candidate, std::string& error) {
    if (std::wstring_view(path).ends_with(L".pak")) return PatchChromeAbout(source, url, RELEASE_VER_STR, enabled, candidate, error);
    return enabled ? PatchAboutUi(source, url, RELEASE_VER_STR, candidate, error) : RemoveAboutUi(source, candidate, error);
}
}

bool CanIntegrateAboutPage() {
    if (IntegrationDisabled()) return false;
    const auto bundle = LatestBundle();
    if (bundle.empty()) return false;
    std::string source, candidate, error;
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!ReadBundle(bundle, source, info) || !MakeCandidate(bundle, source,
        "http://127.0.0.1:16877/settings.html", true, candidate, error)) return false;
    if (candidate == source || source.find("/* vivaldi-plus-ui:start */") != std::string::npos ||
        source.find(kChromeAboutMarker) != std::string::npos) return true;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) return false;
    HANDLE file = CreateFileW(bundle.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    return true;
}

static void UpdateAboutPage(const std::wstring& settings_url, bool enabled) {
    if (enabled && (settings_url.empty() || IntegrationDisabled())) return;
    const auto bundle = LatestBundle();
    if (bundle.empty()) return;
    std::string source;
    BY_HANDLE_FILE_INFORMATION initial = {};
    const auto marker = std::wstring_view(bundle).ends_with(L".pak") ? kChromeAboutMarker : kVivaldiAboutMarkerPrefix;
    if (!ReadBundle(bundle, source, initial, enabled ? std::string_view{} : marker)) {
        DebugLog(L"About-page bundle unavailable; integration skipped"); return;
    }
    if (!enabled && source.empty()) return;
    std::wstring temp;
    {
        std::string candidate, error;
        if (!MakeCandidate(bundle, source, Utf8(settings_url), enabled, candidate, error)) {
            DebugLog(L"About-page UI unsupported; integration skipped: %hs", error.c_str()); return;
        }
        if (candidate == source) return; // Repeated startup does not touch any files.
        if (initial.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
            DebugLog(L"About-page bundle read-only; integration skipped"); return;
        }
        temp = TempPath(bundle);
        if (temp.empty() || !WriteCandidate(temp, candidate)) {
            DebugLog(L"About-page candidate could not be staged; integration skipped"); return;
        }
    }
    if (!MatchesBundle(bundle, source, initial)) {
        DeleteFileW(temp.c_str()); DebugLog(L"About-page UI changed during integration; modification skipped"); return;
    }
    if (!ReplaceFileW(bundle.c_str(), temp.c_str(), nullptr, 0, nullptr, nullptr)) {
        const DWORD code = GetLastError();
        DeleteFileW(temp.c_str()); DebugLog(L"About-page UI replacement skipped (%lu)", code);
    }
}

void InitializeAboutPage(const std::wstring& settings_url) { UpdateAboutPage(settings_url, true); }
void RemoveAboutPageEntry() { UpdateAboutPage({}, false); }
