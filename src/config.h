#ifndef VIVALDI_PLUS_CONFIG_H_
#define VIVALDI_PLUS_CONFIG_H_

#include <string>
#include <windows.h>
#include <shlwapi.h>

// Configuration manager for vivaldi_plus
// Reads settings from config.ini in the application directory
class Config
{
private:
    std::wstring config_path_;
    bool win32k_enabled_;
    std::wstring command_line_;
    std::wstring disable_features_;
    bool has_custom_disable_features_;

    Config()
    {
        // Initialize with default values
        win32k_enabled_ = false;  // Default: do not force enable win32k (safer)
        has_custom_disable_features_ = false;
        LoadConfig();
    }

    void LoadConfig()
    {
        config_path_ = GetAppDir() + L"\\config.ini";

        if (!PathFileExistsW(config_path_.c_str()))
        {
            return;  // Use defaults if config doesn't exist
        }

        // Read win32k setting from [general] section
        // 0 = disabled (default, safer, better for video streaming)
        // 1 = enabled (only use if Chrome crashes at startup)
        win32k_enabled_ = (GetPrivateProfileIntW(L"general", L"win32k", 0, config_path_.c_str()) != 0);

        // Read additional command line arguments
        wchar_t buffer[4096];
        GetPrivateProfileStringW(L"general", L"command_line", L"", buffer, 4096, config_path_.c_str());
        command_line_ = buffer;

        // Read custom disable_features setting
        // If user specifies this, it will be used instead of defaults
        // If empty or not specified, use default: WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading
        wchar_t features_buffer[4096];
        DWORD result = GetPrivateProfileStringW(L"general", L"disable_features", L"", features_buffer, 4096, config_path_.c_str());

        if (result > 0 && features_buffer[0] != L'\0')
        {
            disable_features_ = features_buffer;
            has_custom_disable_features_ = true;
        }
        else
        {
            // Default features to disable for compatibility
            disable_features_ = L"WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading";
            has_custom_disable_features_ = false;
        }
    }

public:
    // Singleton instance
    static Config& Instance()
    {
        static Config instance;
        return instance;
    }

    // Returns true if win32k system calls should be forcibly enabled
    // Default is false (safer, better for video streaming)
    bool IsWin32KEnabled() const
    {
        return win32k_enabled_;
    }

    // Returns additional command line arguments from config
    const std::wstring& GetCommandLine() const
    {
        return command_line_;
    }

    // Returns features to disable (for --disable-features flag)
    // Either user-specified or default: WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading
    const std::wstring& GetDisableFeatures() const
    {
        return disable_features_;
    }

    // Returns true if user has customized disable_features in config.ini
    // Returns false if using default values
    bool HasCustomDisableFeatures() const
    {
        return has_custom_disable_features_;
    }

    // Delete copy constructor and assignment operator
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};

#endif  // VIVALDI_PLUS_CONFIG_H_
