#include <windows.h>
#include <string>

bool g_rightClickCloseTab = true;

void LoadConfig() {
    char buffer[4] = {0};
    GetPrivateProfileString("Settings", "rightClickCloseTab", "1", buffer, 4, ".\\config.ini");
    g_rightClickCloseTab = std::string(buffer) == "1";
}
