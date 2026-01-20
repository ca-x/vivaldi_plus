#ifndef VIVALDI_PLUS_PORTABLE_H_
#define VIVALDI_PLUS_PORTABLE_H_

#include "config.h"  // For config global reference
#include <string_view>

namespace {

inline bool StartsWith(const std::wstring& str, const std::wstring& prefix)
{
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool IsWhitespace(wchar_t ch)
{
    switch (ch)
    {
    case L' ':
    case L'\t':
    case L'\n':
    case L'\r':
        return true;
    default:
        return false;
    }
}

// This function ensures the found switch is a whole "word" by checking for
// whitespace or string boundaries before and after it. This prevents incorrect
// partial matches (e.g., finding "--foo" within "--foobar").
size_t FindStandaloneSwitch(std::wstring_view command_line, std::wstring_view flag)
{
    auto pos = command_line.find(flag);
    while (pos != std::wstring_view::npos)
    {
        const bool at_start = pos == 0 || IsWhitespace(command_line[pos - 1]);
        const auto after = pos + flag.size();
        const bool at_end = after >= command_line.size() || IsWhitespace(command_line[after]);
        if (at_start && at_end)
        {
            return pos;
        }
        pos = command_line.find(flag, pos + flag.size());
    }
    return std::wstring_view::npos;
}

void TrimTrailingWhitespace(std::wstring &text)
{
    while (!text.empty() && IsWhitespace(text.back()))
    {
        text.pop_back();
    }
}

std::wstring QuoteSpaceIfNeeded(std::wstring_view str)
{
    if (str.find(L' ') == std::wstring_view::npos)
        return std::wstring(str);

    std::wstring escaped(L"\"");
    size_t backslash_count = 0;

    for (auto c : str)
    {
        if (c == L'\\')
        {
            backslash_count++;
        }
        else if (c == L'"')
        {
            // Escape preceding backslashes (if any)
            escaped.append(backslash_count, L'\\');
            // Escape the quote itself
            escaped += L'\\';
            backslash_count = 0;
        }
        else
        {
            backslash_count = 0;
        }
        escaped += c;
    }

    // Escape trailing backslashes (because they precede the closing quote)
    escaped.append(backslash_count, L'\\');
    escaped += L'"';
    return escaped;
}

std::wstring JoinArgsString(const std::vector<std::wstring> &lines, const std::wstring &delimiter)
{
    std::wstring text;
    bool first = true;
    for (const auto &line : lines)
    {
        if (!first)
            text += delimiter;
        else
            first = false;
        text += QuoteSpaceIfNeeded(line);
    }
    return text;
}

// Split command line to extract `--single-argument` suffix if present.
std::pair<std::wstring, std::wstring> SplitSingleArgumentSwitch(const std::wstring &command_line)
{
    constexpr std::wstring_view kSingleArgument = L"--single-argument";
    const auto single_argument_pos = FindStandaloneSwitch(command_line, kSingleArgument);

    if (single_argument_pos == std::wstring::npos)
    {
        return {command_line, L""};
    }

    std::wstring prefix = command_line.substr(0, single_argument_pos);
    std::wstring suffix = command_line.substr(single_argument_pos);
    TrimTrailingWhitespace(prefix);

    return {std::move(prefix), std::move(suffix)};
}

// Parse command line string into argument vector, skipping the executable name.
std::vector<std::wstring> ParseCommandLineArgs(const std::wstring &command_line, size_t reserve_extra = 15)
{
    std::vector<std::wstring> args;
    if (command_line.empty())
    {
        args.reserve(reserve_extra);
        return args;
    }

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(command_line.c_str(), &argc);
    if (!argv)
    {
        args.reserve(reserve_extra);
        return args;
    }

    const size_t original_arg_count = argc > 0 ? static_cast<size_t>(argc - 1) : 0;
    args.reserve(original_arg_count + reserve_extra);
    for (int i = 1; i < argc; ++i)
    {
        args.push_back(argv[i]);
    }
    LocalFree(argv);

    return args;
}

// Separate arguments before and after the `--` sentinel.
std::pair<std::vector<std::wstring>, std::vector<std::wstring>> SeparateSentinelArgs(std::vector<std::wstring> args)
{
    constexpr std::wstring_view kSentinel = L"--";
    size_t sentinel_index = args.size();
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == kSentinel)
        {
            sentinel_index = i;
            break;
        }
    }

    if (sentinel_index == args.size())
    {
        return {std::move(args), {}};
    }

    std::vector<std::wstring> trailing_args(args.begin() + sentinel_index, args.end());
    args.erase(args.begin() + sentinel_index, args.end());

    return {std::move(args), std::move(trailing_args)};
}

}  // namespace

bool IsExistsPortable()
{
    std::wstring path = GetAppDir() + L"\\portable";
    return PathFileExists(path.c_str()) != FALSE;
}

bool IsNeedPortable()
{
    static bool need_portable = IsExistsPortable();
    return need_portable;
}

bool IsCustomIniExist()
{
    std::wstring path = GetAppDir() + L"\\config.ini";
    return PathFileExists(path.c_str()) != FALSE;
}

// GetUserDataDir retrieves the user data directory path from the config file.
// It first tries to read the data dir from the "data" key in the "dir_setting" section.
// If that fails, it falls back to a default relative to the app dir.
// It expands any environment variables in the path.
std::wstring GetUserDataDir()
{
    std::wstring configFilePath = GetAppDir() + L"\\config.ini";
    if (!PathFileExists(configFilePath.c_str()))
    {
        return GetAppDir() + L"\\..\\Data";
    }

    TCHAR userDataBuffer[MAX_PATH] = {0};
    ::GetPrivateProfileStringW(L"dir_setting", L"data", L"", userDataBuffer, MAX_PATH, configFilePath.c_str());

    // If no value in config, return default
    if (wcslen(userDataBuffer) == 0)
    {
        return GetAppDir() + L"\\..\\Data";
    }

    std::wstring expandedPath = ExpandEnvironmentPath(userDataBuffer);

    // Expand %app%
    ReplaceStringInPlace(expandedPath, L"%app%", GetAppDir());

    return GetAbsolutePath(expandedPath);
}

// GetDiskCacheDir retrieves the disk cache directory path from the config file.
// It first tries to read the cache dir from the "cache" key in the "dir_setting" section.
// If that fails, it falls back to a default relative to the app dir.
// It expands any environment variables in the path.
std::wstring GetDiskCacheDir()
{
    std::wstring configFilePath = GetAppDir() + L"\\config.ini";

    if (!PathFileExists(configFilePath.c_str()))
    {
        return GetAppDir() + L"\\..\\Cache";
    }

    TCHAR cacheDirBuffer[MAX_PATH] = {0};
    ::GetPrivateProfileStringW(L"dir_setting", L"cache", L"", cacheDirBuffer, MAX_PATH, configFilePath.c_str());

    // If no value in config, return default
    if (wcslen(cacheDirBuffer) == 0)
    {
        return GetAppDir() + L"\\..\\Cache";
    }

    std::wstring expandedPath = ExpandEnvironmentPath(cacheDirBuffer);

    // Expand %app%
    ReplaceStringInPlace(expandedPath, L"%app%", GetAppDir());

    return GetAbsolutePath(expandedPath);
}

// Merge all --disable-features flags into a single one
// Chrome expects only one --disable-features flag, so we need to combine them
struct ProcessedArgs
{
    std::vector<std::wstring> final_args;
    bool has_user_data_dir = false;
    bool has_disk_cache_dir = false;
};

ProcessedArgs ProcessAndMergeArgs(const std::vector<std::wstring> &args)
{
    ProcessedArgs result;
    result.final_args.reserve(args.size() + 4);

    std::wstring combined_features;
    const std::wstring disable_features_prefix = L"--disable-features=";
    const std::wstring user_data_dir_prefix = L"--user-data-dir=";
    const std::wstring disk_cache_dir_prefix = L"--disk-cache-dir=";

    for (const auto &arg : args)
    {
        if (StartsWith(arg, disable_features_prefix))
        {
            // Extract feature names from existing --disable-features flags
            if (!combined_features.empty())
            {
                combined_features += L",";
            }
            combined_features += arg.substr(disable_features_prefix.length());
        }
        else
        {
            // Check if user already specified data/cache dirs
            if (StartsWith(arg, user_data_dir_prefix))
            {
                result.has_user_data_dir = true;
            }
            else if (StartsWith(arg, disk_cache_dir_prefix))
            {
                result.has_disk_cache_dir = true;
            }
            result.final_args.push_back(arg);
        }
    }

    // Add features to disable
    // Priority: User-specified in config.ini > Default compatibility features
    std::wstring features_to_add = config.GetDisableFeatures();

    if (!features_to_add.empty())
    {
        if (!combined_features.empty())
        {
            combined_features += L",";
        }
        combined_features += features_to_add;
    }

    // Rebuild the final argument list with a single merged --disable-features flag
    if (!combined_features.empty())
    {
        result.final_args.push_back(disable_features_prefix + combined_features);
    }

    return result;
}

// Inject additional arguments based on config settings.
void InjectConfigPaths(std::vector<std::wstring> &args, bool has_user_data_dir, bool has_disk_cache_dir)
{
    if (!has_user_data_dir)
    {
        auto userdata = GetUserDataDir();
        if (!userdata.empty())
        {
            args.push_back(L"--user-data-dir=" + userdata);
        }
    }
    if (!has_disk_cache_dir)
    {
        auto diskcache = GetDiskCacheDir();
        if (!diskcache.empty())
        {
            args.push_back(L"--disk-cache-dir=" + diskcache);
        }
    }
}

// Reassemble final command line from arguments and suffix.
std::wstring ReassembleCommandLine(const std::vector<std::wstring> &args, const std::wstring &suffix)
{
    std::wstring result = JoinArgsString(args, L" ");
    if (!suffix.empty())
    {
        if (!result.empty())
        {
            result.reserve(result.size() + suffix.size() + 1);
            result += L' ';
        }
        result += suffix;
    }
    return result;
}

// 构造新命令行
// Parses the command line param into individual arguments and inserts
// additional arguments for portable mode.
//
// The `--single-argument` switch is a special case used by the Windows Shell
// for file associations. Standard parsers like `CommandLineToArgvW` can
// incorrectly split the argument that follows it (typically a file path with
// spaces). To handle this, we split the command line here. The part before
// the switch will be parsed and modified, while the switch and its entire
// argument will be appended verbatim at the end.
//
// param: The command line passed to the application.
//
// Returns: The modified command line with additional args.
std::wstring GetCommand(LPWSTR param)
{
    if (!param)
    {
        return L"";
    }

    // Split off --single-argument if present
    std::wstring command_line(param);
    auto [prefix, suffix] = SplitSingleArgumentSwitch(command_line);

    // Parse the command line arguments (skipping executable name)
    std::vector<std::wstring> args = ParseCommandLineArgs(prefix);

    // Separate arguments before and after the `--` sentinel
    auto [main_args, trailing_args] = SeparateSentinelArgs(std::move(args));

    // Add marker flag to indicate portable mode is active
    main_args.insert(main_args.begin(), L"--gopher");

    // Process and merge --disable-features flags
    ProcessedArgs processed = ProcessAndMergeArgs(main_args);

    // Inject custom directories if not already specified by user
    InjectConfigPaths(processed.final_args, processed.has_user_data_dir, processed.has_disk_cache_dir);

    // Append trailing arguments (after `--` sentinel)
    processed.final_args.insert(processed.final_args.end(), trailing_args.begin(), trailing_args.end());

    // Reassemble the final command line
    return ReassembleCommandLine(processed.final_args, suffix);
}

void Portable(LPWSTR param)
{
    wchar_t path[MAX_PATH];
    if (!::GetModuleFileName(nullptr, path, MAX_PATH))
    {
        if (config.IsDebugLogEnabled())
        {
            DebugLog(L"GetModuleFileName failed: %d", GetLastError());
        }
        return;
    }

    std::wstring args = GetCommand(param);

    if (config.IsDebugLogEnabled())
    {
        DebugLog(L"Portable mode: path=%s, args=%s", path, args.c_str());
    }

    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"open";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    sei.lpParameters = args.c_str();

    if (ShellExecuteEx(&sei))
    {
        ExitProcess(0);
    }
    else
    {
        if (config.IsDebugLogEnabled())
        {
            DebugLog(L"ShellExecuteEx failed: %d", GetLastError());
        }
    }
}

#endif  // VIVALDI_PLUS_PORTABLE_H_
