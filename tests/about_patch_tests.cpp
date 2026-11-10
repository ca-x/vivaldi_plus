#include "../src/about_patch.h"
#include <cstdlib>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iterator>
#include "../src/about_integration.h"
#endif

#ifdef _WIN32
static std::wstring fake_app;
std::wstring GetAppDir() { return fake_app; }
void DebugLog(const wchar_t*, ...) {}
#endif

namespace {
void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
const std::string fixture =
    "function render(){const re=url=>globalThis.opened=url;"
    "const label=(0,I.Z)(\"title\",\"Version\");let Ue;"
    "Ue=(0,Yi.jsxs)(\"div\",{className:\"card-content\",children:[systemCard]});"
    "const card=(0,Yi.jsxs)(\"div\",{className:\"card system-build-card\",children:[header,Ue]});"
    "const report={onClick:()=>re(\"https://vivaldi.com/bugreport/\")};return card}";
const std::string url = "http://127.0.0.1:16877/settings.html";

#ifdef _WIN32
void Write(const std::filesystem::path& file, std::string_view bytes) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Check(stream.good(), "Fixture write failed");
}
std::string Read(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
std::string StripRow(std::string source) {
    const auto at = source.find("/* vivaldi-plus-ui:start */");
    const auto end = source.find("/* vivaldi-plus-ui:end */");
    Check(at != std::string::npos && end != std::string::npos, "Integration row not found");
    source.erase(at, end + std::string("/* vivaldi-plus-ui:end */").size() - at);
    return source;
}
void IntegrationTests() {
    wchar_t temp[MAX_PATH];
    Check(GetTempPathW(MAX_PATH, temp) != 0, "GetTempPath failed");
    fake_app = std::wstring(temp) + L"vivaldi-plus-about-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const auto directory = std::filesystem::path(fake_app) / L"8.2.1.0" / L"resources" / L"vivaldi";
    const auto bundle = directory / L"bundle.js";
    Check(!CanIntegrateAboutPage(), "Missing UI bundle must not start the settings service");
    std::filesystem::create_directories(directory);
    Write(bundle, fixture);
    const auto initial_time = std::filesystem::last_write_time(bundle);
    Check(CanIntegrateAboutPage(), "Fresh supported UI must pass the read-only preflight");
    Check(Read(bundle) == fixture && std::filesystem::last_write_time(bundle) == initial_time,
        "Preflight must not modify the bundle");
    Check(SetFileAttributesW(bundle.c_str(), FILE_ATTRIBUTE_READONLY), "Could not protect fresh fixture");
    Check(!CanIntegrateAboutPage(), "Fresh read-only UI must not start the settings service");
    Check(SetFileAttributesW(bundle.c_str(), FILE_ATTRIBUTE_NORMAL), "Could not unprotect fresh fixture");
    InitializeAboutPage(L"http://127.0.0.1:16877/settings.html");
    const auto installed = Read(bundle);
    Check(CanIntegrateAboutPage(), "An already integrated UI still needs the settings service");
    Check(installed != fixture && StripRow(installed) == fixture, "Installer must preserve every original byte outside the inserted row");
    std::filesystem::last_write_time(bundle, std::filesystem::file_time_type::clock::now() - std::chrono::hours(24));
    const auto timestamp = std::filesystem::last_write_time(bundle);
    InitializeAboutPage(L"http://127.0.0.1:16877/settings.html");
    Check(Read(bundle) == installed && std::filesystem::last_write_time(bundle) == timestamp, "Second startup must not rewrite the bundle");
    Check(std::distance(std::filesystem::directory_iterator(directory), std::filesystem::directory_iterator()) == 1, "Installer must not leave backups, metadata or temporary files");
    const auto old_backup = directory / L"bundle.js.vivaldi-plus-original";
    const auto old_receipt = directory / L"bundle.js.vivaldi-plus.json";
    Write(old_backup, "user-retained backup"); Write(old_receipt, "user-retained metadata");
    InitializeAboutPage(L"http://127.0.0.1:16877/settings.html");
    Check(Read(old_backup) == "user-retained backup" && Read(old_receipt) == "user-retained metadata", "Existing backups/metadata must be left alone");
    std::filesystem::remove(old_backup); std::filesystem::remove(old_receipt);
    InitializeAboutPage(L"http://127.0.0.1:16887/settings.html");
    auto upgraded = Read(bundle);
    const auto marker = upgraded.find("/* vivaldi-plus-ui:start */");
    Check(StripRow(upgraded) == fixture && upgraded.find("/* vivaldi-plus-ui:start */", marker + 1) == std::string::npos, "Port upgrade must replace one row, never duplicate it");
    Check(upgraded.find("16877") == std::string::npos && upgraded.find("16887") != std::string::npos, "Port update failed");
    Check(SetFileAttributesW(bundle.c_str(), FILE_ATTRIBUTE_READONLY), "Could not set read-only fixture");
    Check(CanIntegrateAboutPage(), "A readable integrated UI keeps its existing settings entry even when read-only");
    InitializeAboutPage(L"http://127.0.0.1:16878/settings.html");
    Check(Read(bundle) == upgraded, "Read-only UI must be skipped");
    Check(SetFileAttributesW(bundle.c_str(), FILE_ATTRIBUTE_NORMAL), "Could not clear fixture read-only bit");
    Write(std::filesystem::path(fake_app) / L"vivaldi-plus-about-disabled", "");
    Check(!CanIntegrateAboutPage(), "Disabled integration must not start the settings service");
    InitializeAboutPage(L"http://127.0.0.1:16878/settings.html");
    Check(Read(bundle) == upgraded, "Existing disabled switch must be respected");
    std::filesystem::remove(std::filesystem::path(fake_app) / L"vivaldi-plus-about-disabled");
    Write(bundle, "unsupported UI");
    Check(!CanIntegrateAboutPage(), "Unsupported UI must not start the settings service");
    const auto unsupported_time = std::filesystem::last_write_time(bundle);
    InitializeAboutPage(L"http://127.0.0.1:16877/settings.html");
    Check(Read(bundle) == "unsupported UI" && std::filesystem::last_write_time(bundle) == unsupported_time, "Unsupported bundle must stay unchanged");
    Check(std::distance(std::filesystem::directory_iterator(directory), std::filesystem::directory_iterator()) == 1, "Skipped modification must not leave extra files");
    Check(!std::filesystem::exists(std::filesystem::path(fake_app) / L"vivaldi-plus-integration.log"), "Installer must not create log files");
    std::filesystem::remove_all(fake_app);
    std::cout << "PASS native about-page installer idempotence/skip tests\n";
}
#endif
}

int main(int argc, char** argv) {
    std::string output, error;
    Check(PatchAboutUi(fixture, url, "1.6.0.2", output, error), error.c_str());
    Check(output.find("className:\"card-content\",children:[/* vivaldi-plus-ui:start */") != std::string::npos, "Row must be inside the native system/build card");
    Check(output.find("id:\"vivaldi-plus-about\",className:\"info-row\"") != std::string::npos, "Native info-row missing");
    Check(output.find("children:(0,I.Z)(\"Settings\")") != std::string::npos, "Settings must use the native zh/en translation function");
    Check(output.find("e.preventDefault();re(\"http://127.0.0.1:16877/settings.html?version=1.6.0.2\")") != std::string::npos, "Settings must open via the existing Vivaldi new-tab callback");
    Check(output.find("Settings / ") == std::string::npos, "Do not mix languages in the link label");
    std::string repeated, upgraded;
    Check(PatchAboutUi(output, url, "1.6.0.2", repeated, error) && repeated == output, "Reinstall must be idempotent");
    Check(PatchAboutUi(output, "http://127.0.0.1:16887/settings.html", "1.6.0.3", upgraded, error), "Version/port upgrade must succeed");
    Check(upgraded.find("16877") == std::string::npos && upgraded.find("1.6.0.2") == std::string::npos, "Upgrade must replace the old row completely");
    const std::string unsafe = "\"\\</script>\n\xe2\x80\xa8";
    Check(PatchAboutUi(fixture, url, unsafe, repeated, error), "Version string escaping failed");
    Check(repeated.find("</script>") == std::string::npos && repeated.find("\\u2028") != std::string::npos, "JS string contents must be safely escaped");
    for (const auto* invalid : {"https://127.0.0.1:16877/settings.html", "http://localhost:16877/settings.html", "http://127.0.0.1:16876/settings.html", "http://127.0.0.1:16888/settings.html", "http://127.0.0.1:16877/settings.html/other", "http://127.0.0.1:16877/settings.html#fragment", "http://127.0.0.1:16877@evil/settings.html"}) {
        Check(!PatchAboutUi(fixture, invalid, "1", repeated, error) && !error.empty(), "Invalid endpoint was accepted");
    }
    for (const auto& invalid : {std::string("unsupported"), fixture + fixture, fixture + "onClick:()=>re(\"https://vivaldi.com/bugreport/\")", std::string("/* vivaldi-plus-ui:start */") + fixture}) {
        repeated = "unchanged";
        Check(!PatchAboutUi(invalid, url, "1", repeated, error) && repeated == "unchanged", "Unsupported/ambiguous bundle must not be modified");
    }
    auto renamed = fixture;
    for (size_t at = 0; (at = renamed.find("Yi", at)) != std::string::npos; at += 3) renamed.replace(at, 2, "$x9");
    Check(PatchAboutUi(renamed, url, "1", repeated, error) && repeated.find("(0,$x9.jsx)") != std::string::npos, "Runtime identifiers must be discovered, including dollar signs");
    if (argc > 1) {
        Check(PatchAboutUi(fixture, url, "1.6.0.2", output, error), "Fixture patch failed");
        std::cout << output;
    } else {
        std::cout << "PASS pure C++ about-page patch tests\n";
#ifdef _WIN32
        IntegrationTests();
#endif
    }
    return 0;
}
