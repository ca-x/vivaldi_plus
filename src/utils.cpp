#include "utils.h"

// String formatting utilities
std::wstring Format(const wchar_t *format, va_list args)
{
    if (!format)
        return L"";

    // Copy va_list for length calculation (va_list can only be used once)
    va_list args_copy;
    va_copy(args_copy, args);
    int length = _vscwprintf(format, args_copy);
    va_end(args_copy);

    if (length < 0)
        return L"";

    // Allocate buffer (length + 1 for null terminator)
    std::vector<wchar_t> buffer(length + 1);

    // Format string into buffer using original args
    _vsnwprintf_s(&buffer[0], buffer.size(), length, format, args);

    return std::wstring(&buffer[0]);
}

std::wstring Format(const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    std::wstring str = Format(format, args);
    va_end(args);
    return str;
}

// Debug logging to OutputDebugString
void DebugLog(const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    std::wstring str = Format(format, args);
    va_end(args);

    std::wstring output = Format(L"[vivaldi++]%s\n", str.c_str());
    OutputDebugStringW(output.c_str());
}

// Memory search wrapper
uint8_t *memmem(uint8_t *src, int n, const uint8_t *sub, int m)
{
    return (uint8_t *)FastSearch(src, n, sub, m);
}

// Search for byte pattern in PE module's .text section
uint8_t *SearchModuleRaw(HMODULE module, const uint8_t *sub, int m)
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
uint8_t *SearchModuleRaw2(HMODULE module, const uint8_t *sub, int m)
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

// Get application directory path (cached for performance)
std::wstring GetAppDir()
{
    static const std::wstring cached_path = []() {
        wchar_t path[MAX_PATH];
        if (::GetModuleFileNameW(nullptr, path, MAX_PATH))
        {
            ::PathRemoveFileSpecW(path);
            return std::wstring(path);
        }
        return std::wstring();
    }();
    return cached_path;
}

// Check if string ends with suffix (case-insensitive)
bool isEndWith(const wchar_t *s, const wchar_t *sub)
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
std::wstring GetAbsolutePath(std::wstring_view path)
{
    if (path.empty())
        return L"";

    wchar_t buffer[MAX_PATH];
    if (!::GetFullPathNameW(path.data(), MAX_PATH, buffer, nullptr))
        return std::wstring(path);  // Return original on failure

    return buffer;
}

// Expand environment variables in path (e.g., %WINDIR%)
std::wstring ExpandEnvironmentPath(std::wstring_view path)
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
void ReplaceStringInPlace(std::wstring &subject, std::wstring_view search, std::wstring_view replace)
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
bool ReplaceStringInPlace(std::string &subject, std::string_view search, std::string_view replace)
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
void compression_html(std::string &html)
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
