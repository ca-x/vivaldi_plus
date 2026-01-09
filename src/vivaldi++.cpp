#include <windows.h>
#include <psapi.h>

#include "MinHook.h"
#include "version.h"

#include "hijack.h"
#include "utils.h"
#include "patch.h"
#include "portable.h"
#include "appid.h"
#include "green.h"

// Global module instance
HMODULE hInstance = nullptr;

// Function pointer to original program entry point
typedef int (*Startup)();
static Startup ExeMain = nullptr;

// Apply Vivaldi Plus enhancements
void VivaldiPlus()
{
    // Set custom AppUserModelID for Windows taskbar
    SetAppId();

    // Apply portable mode registry patches
    MakeGreen();
}

// Handle command line and decide whether to restart in portable mode
void VivaldiPlusCommand(LPWSTR param)
{
    if (!param)
        return;

    // Check if already running in portable mode (--gopher flag present)
    if (!wcsstr(param, L"--gopher"))
    {
        // Not in portable mode yet, restart with portable parameters
        Portable(param);
    }
    else
    {
        // Already in portable mode, apply enhancements
        VivaldiPlus();
    }
}

// Main loader function called instead of original entry point
int Loader()
{
    // Install binary patches for Vivaldi (disable update nag, etc.)
    MakePatch();

    // Only process main browser process, not child processes
    LPWSTR param = GetCommandLineW();
    if (param && !wcsstr(param, L"-type="))
    {
        // Main process: handle portable mode
        VivaldiPlusCommand(param);
    }

    // Jump to original program entry point
    return ExeMain ? ExeMain() : 0;
}

// Install hook on program entry point
void InstallLoader()
{
    // Get current process module information
    HMODULE hModule = GetModuleHandleW(nullptr);
    if (!hModule)
    {
        DebugLog(L"GetModuleHandle failed: %d", GetLastError());
        return;
    }

    MODULEINFO mi = {0};
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &mi, sizeof(MODULEINFO)))
    {
        DebugLog(L"GetModuleInformation failed: %d", GetLastError());
        return;
    }

    PBYTE entry = (PBYTE)mi.EntryPoint;
    if (!entry)
    {
        DebugLog(L"Entry point is null");
        return;
    }

    // Hook the entry point to redirect to our Loader function
    MH_STATUS status = MH_CreateHook(entry, Loader, (LPVOID *)&ExeMain);
    if (status != MH_OK)
    {
        DebugLog(L"MH_CreateHook InstallLoader failed: %d", status);
        return;
    }

    status = MH_EnableHook(entry);
    if (status != MH_OK)
    {
        DebugLog(L"MH_EnableHook InstallLoader failed: %d", status);
        return;
    }
}

// Exported marker function for detection
// This function is referenced in portable.h to verify DLL is loaded
extern "C" __declspec(dllexport) void gopher()
{
    // Empty marker function - used only for DLL identification
}

// DLL entry point
extern "C" BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID pv)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        hInstance = hModule;

        // Restore original version.dll functionality by loading system DLL
        LoadSysDll(hModule);

        // Initialize MinHook library
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK)
        {
            DebugLog(L"MH_Initialize failed: %d", status);
            return TRUE;  // Return TRUE to allow process to continue
        }

        // Install our loader hook
        InstallLoader();
        break;

    case DLL_PROCESS_DETACH:
        // Cleanup MinHook
        MH_Uninitialize();
        break;
    }

    return TRUE;
}
