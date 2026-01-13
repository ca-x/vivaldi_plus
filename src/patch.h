#ifndef VIVALDI_PLUS_PATCH_H_
#define VIVALDI_PLUS_PATCH_H_

#include <windows.h>
#include <atomic>
#include "detours.h"

// NT API definitions
typedef LONG NTSTATUS, *PNTSTATUS;

#ifndef NT_SUCCESS
#define NT_SUCCESS(x) ((x) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0)
#endif

typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef NTSTATUS(WINAPI *pLdrLoadDll)(IN PWCHAR PathToFile OPTIONAL, IN ULONG Flags OPTIONAL,
                                      IN PUNICODE_STRING ModuleFileName, OUT PHANDLE ModuleHandle);

namespace patch
{

// Original LdrLoadDll function pointer
static pLdrLoadDll RawLdrLoadDll = nullptr;

// Thread-safe flag to track if vivaldi.dll has been patched
static std::atomic<bool> vivaldi_patched{false};

// Patch "OutdatedUpgradeBubble.Show" to disable update nag
// This prevents the annoying "Browser is outdated" message
void PatchOutdatedWarning(HMODULE module)
{
    if (!module)
        return;

    // Binary signature for the outdated upgrade check
#ifdef _WIN64
    // x64 pattern: mov [rsp+F0h], rcx; cmp byte ptr [...]
    BYTE search[] = {0x48, 0x89, 0x8C, 0x24, 0xF0, 0x00, 0x00, 0x00, 0x80, 0x3D};
#else
    // x86 pattern: mov [ebp-10h], eax; mov [ebp-11h], bl; cmp byte ptr [...]
    BYTE search[] = {0x31, 0xE8, 0x89, 0x45, 0xF0, 0x88, 0x5D, 0xEF, 0x80, 0x3D};
#endif

    uint8_t *match = SearchModuleRaw(module, search, sizeof(search));
    if (match)
    {
        // Check if the je/jz instruction is present at expected offset
        if (*(match + 0xF) == 0x74)  // je/jz opcode
        {
            // NOP out the conditional jump to always skip the update check
            BYTE patch[] = {0x90, 0x90};  // nop; nop
            if (!WriteMemory(match + 0xF, patch, sizeof(patch)))
            {
                DebugLog(L"Failed to write memory for Outdated patch at %p", match);
            }
        }
    }
    else
    {
        // Pattern not found - likely a different Vivaldi version
        // This is not critical, just log it for debugging
        DebugLog(L"OutdatedWarning pattern not found in module %p (version mismatch?)", module);
    }
}

// Patch "enable-automation" detection (DevTools warning)
// TODO: Implement this if needed to hide automation indicators
void PatchDevToolsWarning(HMODULE module)
{
    if (!module)
        return;

    // Pattern for "enable-automation" check
    // Currently not implemented - add pattern matching if needed
    // This would remove the "Chrome is being controlled by automated test software" warning
}

// Hook for LdrLoadDll to intercept vivaldi.dll loading
NTSTATUS WINAPI HookedLdrLoadDll(IN PWCHAR PathToFile OPTIONAL,
                                 IN ULONG Flags OPTIONAL,
                                 IN PUNICODE_STRING ModuleFileName,
                                 OUT PHANDLE ModuleHandle)
{
    // Call original LdrLoadDll
    NTSTATUS ntstatus = RawLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);

    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    // Check if this is vivaldi.dll being loaded
    if (ModuleFileName && ModuleFileName->Buffer)
    {
        // Use case-insensitive search for vivaldi.dll
        if (wcsstr(ModuleFileName->Buffer, L"vivaldi.dll") != nullptr ||
            wcsstr(ModuleFileName->Buffer, L"VIVALDI.DLL") != nullptr)
        {
            // Use atomic exchange to ensure we only patch once
            bool expected = false;
            if (vivaldi_patched.compare_exchange_strong(expected, true))
            {
                HMODULE hVivaldi = (HMODULE)*ModuleHandle;

                // Apply patches
                PatchOutdatedWarning(hVivaldi);
                PatchDevToolsWarning(hVivaldi);
            }
        }
    }

    return ntstatus;
}

// Install LdrLoadDll hook to intercept vivaldi.dll loading
void InstallPatches()
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
    {
        DebugLog(L"Failed to get ntdll.dll handle");
        return;
    }

    PBYTE LdrLoadDll = (PBYTE)GetProcAddress(ntdll, "LdrLoadDll");
    if (!LdrLoadDll)
    {
        DebugLog(L"Failed to get LdrLoadDll address");
        return;
    }

    // Initialize function pointer with original API address
    RawLdrLoadDll = reinterpret_cast<pLdrLoadDll>(LdrLoadDll);

    // Create hook using Detours
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<LPVOID*>(&RawLdrLoadDll), reinterpret_cast<void*>(HookedLdrLoadDll));
    LONG status = DetourTransactionCommit();
    if (status != NO_ERROR)
    {
        DebugLog(L"DetourAttach LdrLoadDll failed: %d", status);
        return;
    }
}

} // namespace patch

// Public interface
inline void MakePatch()
{
    patch::InstallPatches();
}

#endif // VIVALDI_PLUS_PATCH_H_

