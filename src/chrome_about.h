#pragma once
#include <string>
#include <string_view>

inline constexpr std::string_view kChromeAboutMarker = "vivaldi-plus-chrome-entry";

bool PatchChromeAbout(std::string_view pack, std::string_view settings_url,
    std::string_view version, bool enabled, std::string& output, std::string& error);
