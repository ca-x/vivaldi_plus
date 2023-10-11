//
// Created by czyt on 2023/10/12.
//

// This file is used to control the features of the browser.
// whether to open the new tab page when clicking the right mouse button.
bool IsRightClickToCloseTab()
{
    std::wstring configFilePath = GetAppDir() + L"\\config.ini";
    if (!PathFileExists(configFilePath.c_str()))
    {
        return false;
    }
    // Read the config file.
    RightClickCloseTab = GetPrivateProfileInt("features", L"right_click_close_tab", 1, configFilePath) == 1;
}