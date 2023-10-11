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
    static bool need_portable = IsExistsPortable();
    return need_portable;
}

std::wstring GetUserDataDir()
{
    TCHAR user_data_path[MAX_PATH];

    std::wstring iniFilePath = GetAppDir() + L"\\config.ini";

    if (!PathFileExists(iniFilePath.c_str()))
    {
        return GetAppDir() + L"\\..\\Data";
    }

    TCHAR userDataPath[MAX_PATH];
    ::GetPrivateProfileStringW(L"dir_setting", L"data", L"", userDataPath, MAX_PATH, iniFilePath.c_str());

    std::wstring finalPath = ExpandEnvironmentPath(userDataPath);

    // 扩展%app%
    ReplaceStringInPlace(finalPath, L"%app%", GetAppDir());

    wcscpy(user_data_path, finalPath.c_str());

    return std::wstring(userDataPath);
}

std::wstring GetDiskCacheDir()
{

    std::wstring iniFilePath = GetAppDir() + L"\\config.ini";

    if (!PathFileExists(iniFilePath.c_str()))
    {
        return GetAppDir() + L"\\..\\Cache";
    }

    TCHAR cacheDirPath[MAX_PATH];
    ::GetPrivateProfileStringW(L"dir_setting", L"cache", L"", cacheDirPath, MAX_PATH, iniFilePath.c_str());

    std::wstring finalPath = ExpandEnvironmentPath(cacheDirPath);

    // 扩展%app%
    ReplaceStringInPlace(finalPath, L"%app%", GetAppDir());

    wcscpy(cacheDirPath, finalPath.c_str());

    return std::wstring(cacheDirPath);
}

// 构造新命令行
std::wstring GetCommand(LPWSTR param)
{
    std::vector<std::wstring> args;

    int argc;
    LPWSTR *argv = CommandLineToArgvW(param, &argc);

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

            // args.push_back(L"--force-local-ntp");
            // args.push_back(L"--disable-background-networking");

            args.push_back(L"--disable-features=RendererCodeIntegrity,FlashDeprecationWarning");

            // if (IsNeedPortable())
            {
                auto diskcache = GetDiskCacheDir();

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
