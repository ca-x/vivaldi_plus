#ifndef VIVALDI_PLUS_HIJACK_H_
#define VIVALDI_PLUS_HIJACK_H_

//
// IMPORTANT: This header contains function definitions for DLL export forwarding.
// It MUST only be included in ONE translation unit (vivaldi++.cpp).
// DO NOT include this header in multiple .cpp files - it will cause ODR violations.
//
// This header uses inline assembly and export pragmas to forward version.dll exports.
// The implementation style is intentional for the DLL hijacking technique.
//

#include <windows.h>
#include <intrin.h>
#include <stdint.h>

namespace hijack
{

#define NOP_FUNC            \
    {                       \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        __nop();            \
        return __COUNTER__; \
    }
// 用 __COUNTER__ 来生成一点不一样的代码，避免被 VS 自动合并相同函数

#define EXPORT(api) int __cdecl api() NOP_FUNC

#pragma region 声明导出函数
// 声明导出函数
#pragma comment(linker, "/export:GetFileVersionInfoA=?GetFileVersionInfoA@hijack@@YAHXZ,@1")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=?GetFileVersionInfoByHandle@hijack@@YAHXZ,@2")
#pragma comment(linker, "/export:GetFileVersionInfoExA=?GetFileVersionInfoExA@hijack@@YAHXZ,@3")
#pragma comment(linker, "/export:GetFileVersionInfoExW=?GetFileVersionInfoExW@hijack@@YAHXZ,@4")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=?GetFileVersionInfoSizeA@hijack@@YAHXZ,@5")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=?GetFileVersionInfoSizeExA@hijack@@YAHXZ,@6")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=?GetFileVersionInfoSizeExW@hijack@@YAHXZ,@7")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=?GetFileVersionInfoSizeW@hijack@@YAHXZ,@8")
#pragma comment(linker, "/export:GetFileVersionInfoW=?GetFileVersionInfoW@hijack@@YAHXZ,@9")
#pragma comment(linker, "/export:VerFindFileA=?VerFindFileA@hijack@@YAHXZ,@10")
#pragma comment(linker, "/export:VerFindFileW=?VerFindFileW@hijack@@YAHXZ,@11")
#pragma comment(linker, "/export:VerInstallFileA=?VerInstallFileA@hijack@@YAHXZ,@12")
#pragma comment(linker, "/export:VerInstallFileW=?VerInstallFileW@hijack@@YAHXZ,@13")
#pragma comment(linker, "/export:VerLanguageNameA=?VerLanguageNameA@hijack@@YAHXZ,@14")
#pragma comment(linker, "/export:VerLanguageNameW=?VerLanguageNameW@hijack@@YAHXZ,@15")
#pragma comment(linker, "/export:VerQueryValueA=?VerQueryValueA@hijack@@YAHXZ,@16")
#pragma comment(linker, "/export:VerQueryValueW=?VerQueryValueW@hijack@@YAHXZ,@17")

EXPORT(GetFileVersionInfoA)
EXPORT(GetFileVersionInfoByHandle)
EXPORT(GetFileVersionInfoExA)
EXPORT(GetFileVersionInfoExW)
EXPORT(GetFileVersionInfoSizeA)
EXPORT(GetFileVersionInfoSizeExA)
EXPORT(GetFileVersionInfoSizeExW)
EXPORT(GetFileVersionInfoSizeW)
EXPORT(GetFileVersionInfoW)
EXPORT(VerFindFileA)
EXPORT(VerFindFileW)
EXPORT(VerInstallFileA)
EXPORT(VerInstallFileW)
EXPORT(VerLanguageNameA)
EXPORT(VerLanguageNameW)
EXPORT(VerQueryValueA)
EXPORT(VerQueryValueW)
} // namespace hijack
#pragma endregion

#pragma region 还原导出函数
#include "detours.h"

namespace {
inline void LoadVersion(HINSTANCE hModule)
{
    PBYTE pImageBase = (PBYTE)hModule;
    PIMAGE_DOS_HEADER pimDH = (PIMAGE_DOS_HEADER)pImageBase;
    if (pimDH->e_magic != IMAGE_DOS_SIGNATURE)
        return;

    PIMAGE_NT_HEADERS pimNH = (PIMAGE_NT_HEADERS)(pImageBase + pimDH->e_lfanew);
    if (pimNH->Signature != IMAGE_NT_SIGNATURE)
        return;

    PIMAGE_EXPORT_DIRECTORY pimExD = (PIMAGE_EXPORT_DIRECTORY)(pImageBase + pimNH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    // Get export table pointers
    DWORD *pName = (DWORD *)(pImageBase + pimExD->AddressOfNames);
    DWORD *pFunction = (DWORD *)(pImageBase + pimExD->AddressOfFunctions);
    WORD *pNameOrdinals = (WORD *)(pImageBase + pimExD->AddressOfNameOrdinals);

    // Load real system version.dll
    wchar_t szSysDirectory[MAX_PATH + 1];
    GetSystemDirectory(szSysDirectory, MAX_PATH);

    wchar_t szDLLPath[MAX_PATH + 1];
    lstrcpy(szDLLPath, szSysDirectory);
    lstrcat(szDLLPath, TEXT("\\version.dll"));

    HINSTANCE module = LoadLibrary(szDLLPath);
    if (!module)
        return;

    // Store function pointers in array (must persist until DetourTransactionCommit)
    static PVOID targets[32];  // version.dll has 17 exports, 32 is safe
    size_t count = 0;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    for (size_t i = 0; i < pimExD->NumberOfNames && count < 32; i++)
    {
        PBYTE Original = (PBYTE)GetProcAddress(module, (char *)(pImageBase + pName[i]));
        if (Original)
        {
            targets[count] = (PVOID)(pImageBase + pFunction[pNameOrdinals[i]]);
            DetourAttach(&targets[count], Original);
            count++;
        }
    }

    DetourTransactionCommit();
}
}  // namespace
#pragma endregion

inline void LoadSysDll(HINSTANCE hModule)
{
    LoadVersion(hModule);
}

#endif  // VIVALDI_PLUS_HIJACK_H_
