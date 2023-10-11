//
// Created by czyt on 2023/10/12.
//

// This file is used to control the features of the browser.
// whether to open the new tab page when clicking the right mouse button.

bool EnableRightClickCloseTab = false;

void ParseFeatureFlags()
{
    std::wstring configFilePath = GetAppDir() + L"\\config.ini";

    if (PathFileExists(configFilePath.c_str()))
    {
        const wchar_t *lpFileName = configFilePath.c_str();

        // Read the config file.
        EnableRightClickCloseTab = GetPrivateProfileIntW(L"features", L"right_click_close_tab", 1, lpFileName) == 1;
    }
}
