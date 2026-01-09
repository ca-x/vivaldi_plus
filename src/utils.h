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

#include "fastsearch.h"

// String formatting utilities
std::wstring Format(const wchar_t *format, va_list args);
std::wstring Format(const wchar_t *format, ...);

// Debug logging to OutputDebugString
void DebugLog(const wchar_t *format, ...);

// Memory search wrapper
uint8_t *memmem(uint8_t *src, int n, const uint8_t *sub, int m);

// Search for byte pattern in PE module's .text section
uint8_t *SearchModuleRaw(HMODULE module, const uint8_t *sub, int m);

// Search for byte pattern in PE module's .rdata section
uint8_t *SearchModuleRaw2(HMODULE module, const uint8_t *sub, int m);

// Get application directory path
std::wstring GetAppDir();

// Check if string ends with suffix (case-insensitive)
bool isEndWith(const wchar_t *s, const wchar_t *sub);

// Get absolute path from relative path
std::wstring GetAbsolutePath(std::wstring_view path);

// Expand environment variables in path (e.g., %WINDIR%)
std::wstring ExpandEnvironmentPath(std::wstring_view path);

// Replace all occurrences of 'search' with 'replace' in string (wide char version)
void ReplaceStringInPlace(std::wstring &subject, std::wstring_view search, std::wstring_view replace);

// Replace all occurrences of 'search' with 'replace' in string (narrow char version)
bool ReplaceStringInPlace(std::string &subject, std::string_view search, std::string_view replace);

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
std::vector<std::string> split(const std::string &text, char sep);

// Compress HTML by removing extra whitespace
void compression_html(std::string &html);

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
