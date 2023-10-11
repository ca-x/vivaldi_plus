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

    wcscpy(userDataBuffer, expandedPath.c_str());

    return std::wstring(userDataBuffer);
}

std::wstring GetDiskCacheDir(std::wstring customPath = L"")
{
    if (!customPath.empty() && PathFileExists(customPath.c_str()))
    {
        return customPath;
    }

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

    wcscpy(cacheDirBuffer, expandedPath.c_str());

    return std::wstring(cacheDirBuffer);
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

    std::wstring customCacheDir;

    int insert_pos = 0;
    for (int i = 0; i < argc; i++)
    {
        if (wcscmp(argv[i], L"--") == 0)
        {
            break;
        }
        if (wcscmp(argv[i], L"--single-argument") == 0)
        {
            break;
        }
        if (wcsncmp(argv[i], L"--disk-cache-dir=", 16) == 0)
        {
            customCacheDir = argv[i] + 16;
        }
        insert_pos = i;
    }
    for (int i = 0; i < argc; i++)
    {
        // 保留原来参数
        if (i)
            args.push_back(argv[i]);

        // 追加参数
        if (i == insert_pos)
        {
            args.push_back(L"--gopher");

            args.push_back(L"--force-local-ntp");
            args.push_back(L"--disable-background-networking");

            args.push_back(L"--disable-features=RendererCodeIntegrity,FlashDeprecationWarning");

            if (!customCacheDir.empty())
            {
                auto diskcache = GetDiskCacheDir(customCacheDir);

                wchar_t temp[MAX_PATH];
                wsprintf(temp, L"--disk-cache-dir=%s", diskcache.c_str());
                args.push_back(temp);
            }
            {
                auto userdata = GetUserDataDir();

                wchar_t temp[MAX_PATH];
                wsprintf(temp, L"--user-data-dir=%s", userdata.c_str());
                args.push_back(temp);
            }
        }
    }
    LocalFree(argv);

    return JoinArgsString(args, L" ");
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
