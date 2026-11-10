#ifndef VIVALDI_PLUS_WEB_SETTINGS_H_
#define VIVALDI_PLUS_WEB_SETTINGS_H_

#include <string>

using WebSettingsReady = void (*)(const std::wstring&);
void InitializeWebSettings(WebSettingsReady on_ready = nullptr);
std::wstring GetWebSettingsUrl();

#endif
