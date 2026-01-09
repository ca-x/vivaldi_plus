#ifndef VIVALDI_PLUS_UTILS_H_
#define VIVALDI_PLUS_UTILS_H_

// Prevent common Windows macro conflicts
#ifndef NOMINMAX
#define NOMINMAX  // Prevent min/max macros
#endif

#include <windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <cwctype>
#include <algorithm>
#include <ranges>

#include "FastSearch.h"

// String formatting utilities
inline std::wstring Format(const wchar_t *format, va_list args)
{
    if (!format)
        return L"";

    // Calculate required buffer size
    int length = _vscwprintf(format, args);
    if (length < 0)
        return L"";

    // Allocate buffer (length + 1 for null terminator)
    std::vector<wchar_t> buffer(length + 1);

    // Format string into buffer
    _vsnwprintf_s(&buffer[0], buffer.size(), length, format, args);

    return std::wstring(&buffer[0]);
}

inline std::wstring Format(const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    std::wstring str = Format(format, args);
    va_end(args);
    return str;
}

// Debug logging to OutputDebugString
inline void DebugLog(const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    std::wstring str = Format(format, args);
    va_end(args);

    std::wstring output = Format(L"[vivaldi++]%s\n", str.c_str());
    OutputDebugStringW(output.c_str());
}

// Memory search wrapper
inline uint8_t *memmem(uint8_t *src, int n, const uint8_t *sub, int m)
{
    return (uint8_t *)FastSearch(src, n, sub, m);
}

// Search for byte pattern in PE module's .text section
inline uint8_t *SearchModuleRaw(HMODULE module, const uint8_t *sub, int m)
{
    if (!module || !sub || m <= 0)
        return nullptr;

    uint8_t *buffer = (uint8_t *)module;

    // Verify DOS header
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)buffer;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;

    // Verify NT header
    PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)(buffer + dos_header->e_lfanew);
    if (nt_header->Signature != IMAGE_NT_SIGNATURE)
        return nullptr;

    // Get section headers
    PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)((char *)nt_header + sizeof(DWORD) +
                                                             sizeof(IMAGE_FILE_HEADER) + nt_header->FileHeader.SizeOfOptionalHeader);

    // Search .text section
    for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
    {
        if (strcmp((const char *)section[i].Name, ".text") == 0)
        {
            return memmem(buffer + section[i].VirtualAddress, section[i].Misc.VirtualSize, sub, m);
        }
    }
    return nullptr;
}

// Search for byte pattern in PE module's .rdata section
inline uint8_t *SearchModuleRaw2(HMODULE module, const uint8_t *sub, int m)
{
    if (!module || !sub || m <= 0)
        return nullptr;

    uint8_t *buffer = (uint8_t *)module;

    // Verify DOS header
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)buffer;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;

    // Verify NT header
    PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)(buffer + dos_header->e_lfanew);
    if (nt_header->Signature != IMAGE_NT_SIGNATURE)
        return nullptr;

    // Get section headers
    PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)((char *)nt_header + sizeof(DWORD) +
                                                             sizeof(IMAGE_FILE_HEADER) + nt_header->FileHeader.SizeOfOptionalHeader);

    // Search .rdata section
    for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
    {
        if (strcmp((const char *)section[i].Name, ".rdata") == 0)
        {
            return memmem(buffer + section[i].VirtualAddress, section[i].Misc.VirtualSize, sub, m);
        }
    }
    return nullptr;
}

// Get application directory path
inline std::wstring GetAppDir()
{
    wchar_t path[MAX_PATH];
    if (!::GetModuleFileNameW(nullptr, path, MAX_PATH))
        return L"";

    ::PathRemoveFileSpecW(path);
    return path;
}

// Check if string ends with suffix (case-insensitive)
inline bool isEndWith(const wchar_t *s, const wchar_t *sub)
{
    if (!s || !sub)
        return false;

    size_t len1 = wcslen(s);
    size_t len2 = wcslen(sub);
    if (len2 > len1)
        return false;

    return _wcsicmp(s + len1 - len2, sub) == 0;
}

// Get absolute path from relative path
inline std::wstring GetAbsolutePath(std::wstring_view path)
{
    if (path.empty())
        return L"";

    wchar_t buffer[MAX_PATH];
    if (!::GetFullPathNameW(path.data(), MAX_PATH, buffer, nullptr))
        return std::wstring(path);  // Return original on failure

    return buffer;
}

// Expand environment variables in path (e.g., %WINDIR%)
inline std::wstring ExpandEnvironmentPath(std::wstring_view path)
{
    if (path.empty())
        return L"";

    // First try with reasonable buffer
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD result = ::ExpandEnvironmentStringsW(path.data(), &buffer[0], (DWORD)buffer.size());

    if (result == 0)
        return std::wstring(path);  // Error, return original

    if (result > buffer.size())
    {
        // Buffer too small, resize and try again
        buffer.resize(result);
        result = ::ExpandEnvironmentStringsW(path.data(), &buffer[0], (DWORD)buffer.size());
        if (result == 0)
            return std::wstring(path);
    }

    // result includes null terminator, so subtract 1
    return std::wstring(&buffer[0], result - 1);
}

// Replace all occurrences of 'search' with 'replace' in string (wide char version)
inline void ReplaceStringInPlace(std::wstring &subject, std::wstring_view search, std::wstring_view replace)
{
    if (search.empty())
        return;

    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::wstring::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

// Replace all occurrences of 'search' with 'replace' in string (narrow char version)
inline bool ReplaceStringInPlace(std::string &subject, std::string_view search, std::string_view replace)
{
    if (search.empty())
        return false;

    bool found = false;
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
        found = true;
    }
    return found;
}

// String trimming utilities
inline std::string &ltrim(std::string &s)
{
    auto it = std::ranges::find_if(s, [](unsigned char ch) {
        return !std::isspace(ch);
    });
    s.erase(s.begin(), it);
    return s;
}

inline std::string &rtrim(std::string &s)
{
    auto it = std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    s.erase(it.base(), s.end());
    return s;
}

inline std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

// Split string by delimiter
std::vector<std::string> split(const std::string &text, char sep)
{
    std::vector<std::string> tokens;
    size_t start = 0, end = 0;

    while ((end = text.find(sep, start)) != std::string::npos)
    {
        tokens.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(text.substr(start));

    return tokens;
}

// Compress HTML by removing extra whitespace
inline void compression_html(std::string &html)
{
    auto lines = split(html, '\n');
    html.clear();

    for (auto &line : lines)
    {
        trim(line);
        if (!line.empty())
        {
            html += line;
            html += '\n';
        }
    }
}

// ====================================================================================
// Hotkey parsing utilities
// ====================================================================================

#include <optional>

namespace hotkey_impl {

// MOD_NOREPEAT value for RegisterHotKey
// Windows 7+ defines this in winuser.h, but we define it here for compatibility
constexpr UINT kModNoRepeat = 0x4000;

// Modifier keys mapping
constexpr std::pair<std::wstring_view, UINT> kModifierKeys[] = {
    {L"shift", MOD_SHIFT},
    {L"ctrl", MOD_CONTROL},
    {L"control", MOD_CONTROL},  // alias
    {L"alt", MOD_ALT},
    {L"win", MOD_WIN},
};

// Special virtual keys mapping
constexpr std::pair<std::wstring_view, UINT> kSpecialKeys[] = {
    // Arrow keys
    {L"left", VK_LEFT},
    {L"right", VK_RIGHT},
    {L"up", VK_UP},
    {L"down", VK_DOWN},
    {L"←", VK_LEFT},
    {L"→", VK_RIGHT},
    {L"↑", VK_UP},
    {L"↓", VK_DOWN},
    // Control keys
    {L"esc", VK_ESCAPE},
    {L"escape", VK_ESCAPE},  // alias
    {L"tab", VK_TAB},
    {L"backspace", VK_BACK},
    {L"enter", VK_RETURN},
    {L"return", VK_RETURN},  // alias
    {L"space", VK_SPACE},
    // System keys
    {L"prtsc", VK_SNAPSHOT},
    {L"printscreen", VK_SNAPSHOT},  // alias
    {L"scroll", VK_SCROLL},
    {L"pause", VK_PAUSE},
    // Navigation keys
    {L"insert", VK_INSERT},
    {L"delete", VK_DELETE},
    {L"del", VK_DELETE},  // alias
    {L"home", VK_HOME},
    {L"end", VK_END},
    {L"pageup", VK_PRIOR},
    {L"pgup", VK_PRIOR},  // alias
    {L"pagedown", VK_NEXT},
    {L"pgdn", VK_NEXT},  // alias
};

constexpr bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (::towlower(a[i]) != ::towlower(b[i]))  // Use global towlower
      return false;
  }
  return true;
}

template <size_t N>
constexpr std::optional<UINT> FindInKeyMap(
    std::wstring_view key,
    const std::pair<std::wstring_view, UINT> (&map)[N]) {
  for (const auto& [name, code] : map) {
    if (EqualsIgnoreCase(key, name))
      return code;
  }
  return std::nullopt;
}

// Parse function key (F1-F24)
inline std::optional<UINT> ParseFunctionKey(std::wstring_view key) {
  if (key.size() < 2 || (key[0] != L'F' && key[0] != L'f'))
    return std::nullopt;

  auto num_part = key.substr(1);
  if (num_part.empty() || !std::ranges::all_of(num_part, ::iswdigit))
    return std::nullopt;

  int fx = 0;
  for (wchar_t c : num_part) {
    fx = fx * 10 + (c - L'0');
  }

  if (fx >= 1 && fx <= 24)
    return VK_F1 + fx - 1;
  return std::nullopt;
}

// Parse single character key (A-Z, 0-9, symbols)
inline std::optional<UINT> ParseCharacterKey(std::wstring_view key) {
  if (key.size() != 1)
    return std::nullopt;

  wchar_t ch = key[0];
  if (::iswalnum(ch))
    return static_cast<UINT>(::towupper(ch));

  // For other characters, use VkKeyScan
  SHORT scan = ::VkKeyScanW(ch);
  if (scan != -1)
    return LOBYTE(scan);

  return std::nullopt;
}

}  // namespace hotkey_impl

// Parse hotkey string like "Ctrl+Alt+B" into Windows RegisterHotKey format
// Returns MAKELPARAM(modifiers, virtual_key)
inline UINT ParseHotkeys(std::wstring_view keys, bool no_repeat = true) {
  UINT modifiers = 0;
  UINT virtual_key = 0;

  for (const auto& part : std::views::split(keys, L'+')) {
    std::wstring_view key(part.begin(), part.end());
    if (key.empty())
      continue;
    if (auto mod = hotkey_impl::FindInKeyMap(key, hotkey_impl::kModifierKeys)) {
      modifiers |= *mod;
      continue;
    }
    if (auto vk = hotkey_impl::FindInKeyMap(key, hotkey_impl::kSpecialKeys)) {
      virtual_key = *vk;
      continue;
    }
    if (auto vk = hotkey_impl::ParseFunctionKey(key)) {
      virtual_key = *vk;
      continue;
    }
    if (auto vk = hotkey_impl::ParseCharacterKey(key))
      virtual_key = *vk;
  }

  if (no_repeat)
    modifiers |= hotkey_impl::kModNoRepeat;

  return MAKELPARAM(modifiers, virtual_key);
}

#endif // VIVALDI_PLUS_UTILS_H_
