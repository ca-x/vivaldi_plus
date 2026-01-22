#include <windows.h>
#include <psapi.h>

#include "detours.h"
#include "version.h"

#include "hijack.h"
#include "utils.h"
#include "patch.h"
#include "portable.h"
#include "appid.h"
#include "green.h"
#include "hotkey.h"

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

    // Initialize boss key hotkey (if configured in config.ini)
    bosskey::Initialize();
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
    // Load system DLL safely (moved from DllMain to avoid loader lock deadlock)
    static bool dll_loaded = false;
    if (!dll_loaded)
    {
        LoadSysDll(hInstance);
        dll_loaded = true;
    }

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
    ExeMain = reinterpret_cast<Startup>(entry);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<LPVOID*>(&ExeMain), reinterpret_cast<void*>(Loader));
    LONG status = DetourTransactionCommit();
    if (status != NO_ERROR)
    {
        DebugLog(L"DetourAttach InstallLoader failed: %d", status);
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
        {
            DisableThreadLibraryCalls(hModule);
            hInstance = hModule;

            // Install our loader hook
            // NOTE: LoadSysDll() is now called in Loader() to avoid DllMain loader lock deadlock
            // Calling LoadLibrary in DllMain can cause deadlocks with antivirus software
            InstallLoader();
        }
        break;

    case DLL_PROCESS_DETACH:
        // No cleanup needed for Detours
        break;
    }

    return TRUE;
}
