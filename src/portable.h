#include "config.h"  // For Config::Instance()

std::wstring QuoteSpaceIfNeeded(const std::wstring &str)
{
    if (str.find(L' ') == std::wstring::npos)
        return std::move(str);

    std::wstring escaped(L"\"");
    for (auto c : str)
    {
        if (c == L'"')
            escaped += L'"';
        escaped += c;
    }
    escaped += L'"';
    return std::move(escaped);
}

std::wstring JoinArgsString(std::vector<std::wstring> lines, const std::wstring &delimiter)
{
    std::wstring text;
    bool first = true;
    for (auto &line : lines)
    {
        if (!first)
            text += delimiter;
        else
            first = false;
        text += QuoteSpaceIfNeeded(line);
    }
    return text;
}

bool IsExistsPortable()
{
    std::wstring path = GetAppDir() + L"\\portable";
    if (PathFileExists(path.data()))
    {
        return true;
    }
    return false;
}

bool IsNeedPortable()
{
    return true;
    //    static bool need_portable = IsExistsPortable();
    //    return need_portable;
}

bool IsCustomIniExist()
{
    std::wstring path = GetAppDir() + L"\\config.ini";
    if (PathFileExists(path.data()))
    {
        return true;
    }
    return false;
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

    TCHAR userDataBuffer[MAX_PATH];
    ::GetPrivateProfileStringW(L"dir_setting", L"data", L"", userDataBuffer, MAX_PATH, configFilePath.c_str());

    std::wstring expandedPath = ExpandEnvironmentPath(userDataBuffer);

    // Expand %app%
    ReplaceStringInPlace(expandedPath, L"%app%", GetAppDir());

    std::wstring dataDir;
    dataDir = GetAbsolutePath(expandedPath);

    wcscpy(userDataBuffer, dataDir.c_str());

    return std::wstring(userDataBuffer);
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

    TCHAR cacheDirBuffer[MAX_PATH];
    ::GetPrivateProfileStringW(L"dir_setting", L"cache", L"", cacheDirBuffer, MAX_PATH, configFilePath.c_str());

    std::wstring expandedPath = ExpandEnvironmentPath(cacheDirBuffer);

    // Expand %app%
    ReplaceStringInPlace(expandedPath, L"%app%", GetAppDir());

    std::wstring cacheDir;
    cacheDir = GetAbsolutePath(expandedPath);
    wcscpy(cacheDirBuffer, cacheDir.c_str());

    return std::wstring(cacheDirBuffer);
}

// Merge all --disable-features flags into a single one
// Chrome expects only one --disable-features flag, so we need to combine them
struct ProcessedArgs
{
    std::vector<std::wstring> final_args;
    bool has_user_data_dir = false;
    bool has_disk_cache_dir = false;
};

ProcessedArgs ProcessAndMergeArgs(const std::vector<std::wstring>& args)
{
    ProcessedArgs result;
    result.final_args.reserve(args.size() + 4);

    std::wstring combined_features;
    const std::wstring disable_features_prefix = L"--disable-features=";

    for (const auto& arg : args)
    {
        if (arg.find(disable_features_prefix) == 0)
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
            if (arg.find(L"--user-data-dir=") == 0)
            {
                result.has_user_data_dir = true;
            }
            if (arg.find(L"--disk-cache-dir=") == 0)
            {
                result.has_disk_cache_dir = true;
            }
            result.final_args.push_back(arg);
        }
    }

    // Add features to disable
    // Priority: User-specified in config.ini > Default compatibility features
    std::wstring features_to_add = Config::Instance().GetDisableFeatures();

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

// 构造新命令行
// Parses the command line param into individual arguments and inserts
// additional arguments for portable mode.
//
// param: The command line passed to the application.
//
// Returns: The modified command line with additional args.
std::wstring GetCommand(LPWSTR param)
{
    std::vector<std::wstring> args;

    int argc;
    LPWSTR *argv = CommandLineToArgvW(param, &argc);

    // Check if --gopher flag already exists (browser is already running in portable mode)
    bool has_gopher = false;
    for (int i = 0; i < argc; i++)
    {
        if (wcscmp(argv[i], L"--gopher") == 0)
        {
            has_gopher = true;
            break;
        }
    }

    // If already in portable mode, just return the original params unchanged
    // This happens when the browser is already running and receives a new URL
    if (has_gopher)
    {
        // Skip argv[0] (executable path) and reconstruct the command line
        std::vector<std::wstring> original_args;
        for (int i = 1; i < argc; i++)
        {
            original_args.push_back(argv[i]);
        }
        LocalFree(argv);
        return JoinArgsString(original_args, L" ");
    }

    // Collect all original arguments (skip argv[0] which is the executable path)
    for (int i = 1; i < argc; i++)
    {
        args.push_back(argv[i]);
    }

    LocalFree(argv);

    // Process and merge --disable-features flags
    ProcessedArgs processed = ProcessAndMergeArgs(args);

    // Build final argument list
    std::vector<std::wstring> final_args;

    // Add marker flag to indicate portable mode is active
    final_args.push_back(L"--gopher");

    // Add merged disable-features
    for (const auto& arg : processed.final_args)
    {
        final_args.push_back(arg);
    }

    // Add custom directories if not already specified by user
    if (!processed.has_disk_cache_dir)
    {
        auto diskcache = GetDiskCacheDir();
        if (!diskcache.empty())
        {
            wchar_t temp[MAX_PATH];
            wsprintf(temp, L"--disk-cache-dir=%s", diskcache.c_str());
            final_args.push_back(temp);
        }
    }

    if (!processed.has_user_data_dir)
    {
        auto userdata = GetUserDataDir();
        if (!userdata.empty())
        {
            wchar_t temp[MAX_PATH];
            wsprintf(temp, L"--user-data-dir=%s", userdata.c_str());
            final_args.push_back(temp);
        }
    }

    return JoinArgsString(final_args, L" ");
}

void Portable(LPWSTR param)
{
    wchar_t path[MAX_PATH];
    ::GetModuleFileName(NULL, path, MAX_PATH);

    std::wstring args = GetCommand(param);

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
}
