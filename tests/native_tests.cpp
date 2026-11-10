#include <windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "../src/portable.h"

static std::wstring test_directory;
std::wstring GetAppDir() { return test_directory; }
void DebugLog(const wchar_t*, ...) {}
std::wstring GetAbsolutePath(std::wstring_view path) {
    return std::filesystem::absolute(std::filesystem::path(path)).wstring();
}
std::wstring ExpandEnvironmentPath(std::wstring_view path) {
    std::wstring input(path);
    DWORD size = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    std::wstring output(size, L'\0');
    ExpandEnvironmentStringsW(input.c_str(), output.data(), size);
    if (!output.empty()) output.pop_back();
    return output;
}
void ReplaceStringInPlace(std::wstring& text, std::wstring_view from, std::wstring_view to) {
    size_t at = 0;
    while ((at = text.find(from, at)) != std::wstring::npos) {
        text.replace(at, from.size(), to); at += to.size();
    }
}
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::vector<std::wstring> Build(const wchar_t* command) {
    std::wstring input(command);
    return ParseCommandLineArgs(L"vivaldi.exe " + GetCommand(input.data()));
}
bool Contains(const std::vector<std::wstring>& args, const std::wstring& value) {
    return std::find(args.begin(), args.end(), value) != args.end();
}

int wmain(int argc, wchar_t** argv) {
    wchar_t temp[MAX_PATH];
    Check(GetTempPathW(MAX_PATH, temp) != 0, "GetTempPathW failed");
    test_directory = std::wstring(temp) + L"vivaldi-plus-native-" + std::to_wstring(GetCurrentProcessId());
    std::filesystem::create_directory(test_directory);
    const auto cleanup = [] { std::filesystem::remove_all(test_directory); };
    try {
        const std::wstring scenario = argc > 1 ? argv[1] : L"missing";
        const std::wstring ini = test_directory + L"\\config.ini";
        if (scenario == L"empty") {
            HANDLE file = CreateFileW(ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "Could not create empty INI");
            CloseHandle(file);
        } else if (scenario == L"custom") {
            Check(WritePrivateProfileStringW(L"general", L"disable_features", L"CustomFeature", ini.c_str()), "Write custom INI failed");
            Check(WritePrivateProfileStringW(L"general", L"command_line", L"--force-dark-mode --user-data-dir=\"C:\\Config Data\" --gopher --type=renderer --single-argument ignored", ini.c_str()), "Write command INI failed");
        }
        const auto& config = GetConfig();
        Check(config.GetDisableFeatures() == (scenario == L"custom" ? L"CustomFeature" : L"WinSboxNoFakeGdiInit,WebUIInProcessResourceLoading"), "Missing and empty INI must have the same default compatibility features");
        Check(config.HasCustomDisableFeatures() == (scenario == L"custom"), "Custom feature indicator is wrong");

        const std::vector<std::wstring> values = {L"", L"plain", L"with space", L"with\ttab", L"a\"b", L"C:\\with space\\", L"a\\\"b", L"尾部\\", L"line\nbreak"};
        const auto roundtrip = ParseCommandLineArgs(L"vivaldi.exe " + JoinArgsString(values, L" "));
        Check(roundtrip == values, "Reassembled command arguments must round-trip through CommandLineToArgvW");
        Check(!HasCommandLineSwitch(L"vivaldi.exe https://example.com/--gopher", L"--gopher"), "URL text must not enable portable marker");
        Check(!HasCommandLineSwitch(L"vivaldi.exe \"name --gopher file\"", L"--gopher"), "Quoted file text must not enable portable marker");
        Check(!HasCommandLineSwitch(L"vivaldi.exe --gopher-extra", L"--gopher"), "Partial marker must not match");
        Check(HasCommandLineSwitch(L"vivaldi.exe \"--gopher\"", L"--gopher"), "Quoted marker token must match");
        Check(!HasCommandLineSwitch(L"vivaldi.exe -- --gopher", L"--gopher"), "Marker after sentinel is data");
        Check(!HasCommandLineSwitch(L"vivaldi.exe --single-argument C:\\a --gopher", L"--gopher"), "Marker in single-argument suffix is data");
        Check(!HasCommandLineSwitch(L"vivaldi.exe https://example.com/?-type=renderer", L"--type", true), "URL type text must not classify browser as child");
        Check(HasCommandLineSwitch(L"vivaldi.exe --type=renderer", L"--type", true), "Child process type token must match");
        Check(!HasCommandLineSwitch(L"vivaldi.exe -- --type=renderer", L"--type", true), "Type after sentinel is data");

        const auto [prefix, suffix] = SplitSingleArgumentSwitch(L"vivaldi.exe \"file --single-argument name\" --single-argument C:\\real file");
        Check(suffix == L"--single-argument C:\\real file", "Only a complete single-argument token splits raw suffix");
        Check(SplitSingleArgumentSwitch(L"vivaldi.exe -- --single-argument data").second.empty(), "Single-argument after sentinel must remain data");
        auto built = Build(L"vivaldi.exe --user-data-dir=\"C:\\Explicit Data\" --disk-cache-dir=\"C:\\Explicit Cache\" --disable-features=Existing https://example.com");
        Check(Contains(built, L"--gopher"), "Restart must include portable marker");
        Check(Contains(built, L"--user-data-dir=C:\\Explicit Data"), "Explicit user data path must survive");
        Check(Contains(built, L"--disk-cache-dir=C:\\Explicit Cache"), "Explicit cache path must survive");
        Check(Contains(built, L"--disable-features=Existing," + config.GetDisableFeatures()), "Existing and configured feature flags must merge");
        Check(Contains(built, L"https://example.com"), "URL argument must survive restart");
        const auto with_sentinel = Build(L"vivaldi.exe -- --gopher --disable-features=Literal");
        const auto sentinel_at = std::find(with_sentinel.begin(), with_sentinel.end(), L"--");
        Check(sentinel_at != with_sentinel.end() && std::vector<std::wstring>(sentinel_at, with_sentinel.end()) ==
            std::vector<std::wstring>({L"--", L"--gopher", L"--disable-features=Literal"}), "Arguments after sentinel must stay untouched");
        Check(GetCommand(nullptr).empty(), "Null command line must stay empty");
        if (scenario == L"custom") {
            Check(Contains(built, L"--force-dark-mode"), "Configured command_line flags must actually be injected");
            Check(!Contains(built, L"--type=renderer"), "Configured flags must not turn the browser into a child process");
            Check(!Contains(built, L"ignored"), "Configured single-argument suffix must not become restart arguments");
            Check(std::count(built.begin(), built.end(), L"--gopher") == 1, "Configuration must not duplicate marker");
            Check(std::find(built.begin(), built.end(), L"--user-data-dir=C:\\Config Data") < std::find(built.begin(), built.end(), L"--user-data-dir=C:\\Explicit Data"), "Explicit command-line path must have precedence over INI flags");
        }
        std::wstring raw = L"vivaldi.exe --single-argument C:\\real file --gopher";
        const auto output = GetCommand(raw.data());
        Check(output.ends_with(L"--single-argument C:\\real file --gopher"), "Single-argument suffix must be kept verbatim");
        cleanup();
        std::wcout << L"PASS native scenario: " << scenario << L'\n';
        return 0;
    } catch (const std::exception& error) {
        cleanup();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
