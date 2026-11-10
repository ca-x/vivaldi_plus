#include "about_integration.h"
#include "about_patch.h"
#include "utils.h"
#include "version.h"
#include <bcrypt.h>
#include <array>

namespace {
bool IntegrationDisabled() {
    return GetFileAttributesW((GetAppDir() + L"\\vivaldi-plus-about-disabled").c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring LatestBundle() {
    const auto app = GetAppDir();
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
        const auto path = app + L"\\" + entry.cFileName + L"\\resources\\vivaldi\\bundle.js";
        const auto attributes = GetFileAttributesW(path.c_str());
        if (valid && version > latest && attributes != INVALID_FILE_ATTRIBUTES && !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            latest = version; result = path;
        }
    } while (FindNextFileW(find, &entry));
    FindClose(find);
    return result;
}

bool ReadBundle(const std::wstring& path, std::string& bytes, BY_HANDLE_FILE_INFORMATION& info) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool ok = GetFileInformationByHandle(file, &info) && info.nFileSizeHigh == 0 && info.nFileSizeLow <= 64 * 1024 * 1024;
    if (ok) {
        bytes.assign(info.nFileSizeLow, '\0');
        DWORD read = 0;
        ok = ReadFile(file, bytes.data(), info.nFileSizeLow, &read, nullptr) && read == info.nFileSizeLow;
    }
    CloseHandle(file);
    return ok;
}

bool SameFile(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber && a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow &&
        a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow && CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) == 0;
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
}

bool CanIntegrateAboutPage() {
    if (IntegrationDisabled()) return false;
    const auto bundle = LatestBundle();
    if (bundle.empty()) return false;
    std::string source, candidate, error;
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!ReadBundle(bundle, source, info) || !PatchAboutUi(source,
        "http://127.0.0.1:16877/settings.html", RELEASE_VER_STR, candidate, error)) return false;
    if (source.find("/* vivaldi-plus-ui:start */") != std::string::npos) return true;
    if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) return false;
    HANDLE file = CreateFileW(bundle.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
    return true;
}

void InitializeAboutPage(const std::wstring& settings_url) {
    if (settings_url.empty() || IntegrationDisabled()) return;
    const auto bundle = LatestBundle();
    if (bundle.empty()) return;
    std::string source, candidate, error;
    BY_HANDLE_FILE_INFORMATION initial = {};
    if (!ReadBundle(bundle, source, initial)) {
        DebugLog(L"About-page bundle unavailable; integration skipped"); return;
    }
    if (!PatchAboutUi(source, Utf8(settings_url), RELEASE_VER_STR, candidate, error)) {
        DebugLog(L"About-page UI unsupported; integration skipped: %hs", error.c_str()); return;
    }
    if (candidate == source) return; // Repeated startup does not touch any files.
    if (initial.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        DebugLog(L"About-page bundle read-only; integration skipped"); return;
    }
    const auto temp = TempPath(bundle);
    if (temp.empty() || !WriteCandidate(temp, candidate)) {
        DebugLog(L"About-page candidate could not be staged; integration skipped"); return;
    }
    std::string current;
    BY_HANDLE_FILE_INFORMATION latest = {};
    if (!ReadBundle(bundle, current, latest) || current != source || !SameFile(initial, latest)) {
        DeleteFileW(temp.c_str()); DebugLog(L"About-page UI changed during integration; modification skipped"); return;
    }
    if (!ReplaceFileW(bundle.c_str(), temp.c_str(), nullptr, 0, nullptr, nullptr)) {
        const DWORD code = GetLastError();
        DeleteFileW(temp.c_str()); DebugLog(L"About-page UI replacement skipped (%lu)", code);
    }
}
