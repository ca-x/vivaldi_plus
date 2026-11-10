#pragma once
#include <string>
#include <string_view>

bool PatchChromeAbout(std::string_view pack, std::string_view settings_url,
    std::string_view version, bool enabled, std::string& output, std::string& error);
