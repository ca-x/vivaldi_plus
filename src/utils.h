#include <map>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <functional>

#include "FastSearch.h"

std::wstring Format(const wchar_t *format, va_list args)
{
    std::vector<wchar_t> buffer;

    size_t length = _vscwprintf(format, args);

    buffer.resize((length + 1) * sizeof(wchar_t));

    _vsnwprintf_s(&buffer[0], length + 1, length, format, args);

    return std::wstring(&buffer[0]);
}

std::wstring Format(const wchar_t *format, ...)
{
    va_list args;

    va_start(args, format);
    auto str = Format(format, args);
    va_end(args);

    return str;
}

void DebugLog(const wchar_t *format, ...)
{
    va_list args;

    va_start(args, format);
    auto str = Format(format, args);
    va_end(args);

    str = Format(L"[vivaldi++]%s\n", str.c_str());

    OutputDebugStringW(str.c_str());
}

// 搜索内存
uint8_t *memmem(uint8_t *src, int n, const uint8_t *sub, int m)
{
    return (uint8_t *)FastSearch(src, n, sub, m);
}

uint8_t *SearchModuleRaw(HMODULE module, const uint8_t *sub, int m)
{
    uint8_t *buffer = (uint8_t *)module;

    PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)(buffer + ((PIMAGE_DOS_HEADER)buffer)->e_lfanew);
    PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)((char *)nt_header + sizeof(DWORD) +
                                                            sizeof(IMAGE_FILE_HEADER) + nt_header->FileHeader.SizeOfOptionalHeader);

    for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
    {
        if (strcmp((const char *)section[i].Name, ".text") == 0)
        {
            return memmem(buffer + section[i].PointerToRawData, section[i].SizeOfRawData, sub, m);
            break;
        }
    }
    return NULL;
}

uint8_t *SearchModuleRaw2(HMODULE module, const uint8_t *sub, int m)
{
    uint8_t *buffer = (uint8_t *)module;

    PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)(buffer + ((PIMAGE_DOS_HEADER)buffer)->e_lfanew);
    PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)((char *)nt_header + sizeof(DWORD) +
                                                            sizeof(IMAGE_FILE_HEADER) + nt_header->FileHeader.SizeOfOptionalHeader);

    for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
    {
        if (strcmp((const char *)section[i].Name, ".rdata") == 0)
        {
            return memmem(buffer + section[i].PointerToRawData, section[i].SizeOfRawData, sub, m);
            break;
        }
    }
    return NULL;
}
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

// 获得程序所在文件夹
std::wstring GetAppDir()
{
    wchar_t path[MAX_PATH];
    ::GetModuleFileName(NULL, path, MAX_PATH);
    ::PathRemoveFileSpec(path);
    return path;
}

bool isEndWith(const wchar_t *s, const wchar_t *sub)
{
    if (!s || !sub)
        return false;
    size_t len1 = wcslen(s);
    size_t len2 = wcslen(sub);
    if (len2 > len1)
        return false;
    return !_memicmp(s + len1 - len2, sub, len2 * sizeof(wchar_t));
}

// 获得指定路径的绝对路径，如 "data/../Cache"
std::wstring GetAbsolutePath(const std::wstring &path)
{
    wchar_t buffer[MAX_PATH];
    ::GetFullPathNameW(path.c_str(), MAX_PATH, buffer, NULL);
    return buffer;
}

// 展开环境路径比如 %windir%
std::wstring ExpandEnvironmentPath(const std::wstring &path)
{
    std::vector<wchar_t> buffer(MAX_PATH);
    size_t expandedLength = ::ExpandEnvironmentStrings(path.c_str(), &buffer[0], (DWORD)buffer.size());
    if (expandedLength > buffer.size())
    {
        buffer.resize(expandedLength);
        expandedLength = ::ExpandEnvironmentStrings(path.c_str(), &buffer[0], (DWORD)buffer.size());
    }
    return std::wstring(&buffer[0], 0, expandedLength);
}
// 替换字符串
void ReplaceStringInPlace(std::wstring &subject, const std::wstring &search, const std::wstring &replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::wstring::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

// 压缩HTML
std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
    return s;
}
std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
}

std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

std::vector<std::string> split(const std::string &text, char sep)
{
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos)
    {
        std::string temp = text.substr(start, end - start);
        tokens.push_back(temp);
        start = end + 1;
    }
    std::string temp = text.substr(start);
    tokens.push_back(temp);
    return tokens;
}

void compression_html(std::string &html)
{
    auto lines = split(html, '\n');
    html.clear();
    for (auto &line : lines)
    {
        html += "\n";
        html += trim(line);
    }
}

// 替换字符串
bool ReplaceStringInPlace(std::string &subject, const std::string &search, const std::string &replace)
{
    bool find = false;
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
        find = true;
    }
    return find;
}
// bool WriteMemory(PBYTE BaseAddress, PBYTE Buffer, DWORD nSize)
//{
//     DWORD ProtectFlag = 0;
//     if (VirtualProtectEx(GetCurrentProcess(), BaseAddress, nSize, PAGE_EXECUTE_READWRITE, &ProtectFlag))
//     {
//         memcpy(BaseAddress, Buffer, nSize);
//         FlushInstructionCache(GetCurrentProcess(), BaseAddress, nSize);
//         VirtualProtectEx(GetCurrentProcess(), BaseAddress, nSize, ProtectFlag, &ProtectFlag);
//         return true;
//     }
//     return false;
// }
