#pragma once
#include <string>
#include <string_view>

// On failure, output is untouched and error explains the unsupported input.
bool PatchAboutUi(std::string_view source, std::string_view settings_url,
    std::string_view version, std::string& output, std::string& error);
