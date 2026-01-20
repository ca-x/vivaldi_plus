#ifndef VIVALDI_PLUS_GREEN_H_
#define VIVALDI_PLUS_GREEN_H_

#include <lmaccess.h>
#include "detours.h"
#include "config.h"  // For Config::Instance()

BOOL WINAPI FakeGetComputerName(
    _Out_ LPTSTR lpBuffer,
    _Inout_ LPDWORD lpnSize)
{
    return 0;
}

BOOL WINAPI FakeGetVolumeInformation(
    _In_opt_ LPCTSTR lpRootPathName,
    _Out_opt_ LPTSTR lpVolumeNameBuffer,
    _In_ DWORD nVolumeNameSize,
    _Out_opt_ LPDWORD lpVolumeSerialNumber,
    _Out_opt_ LPDWORD lpMaximumComponentLength,
    _Out_opt_ LPDWORD lpFileSystemFlags,
    _Out_opt_ LPTSTR lpFileSystemNameBuffer,
    _In_ DWORD nFileSystemNameSize)
{
    return 0;
}

BOOL WINAPI MyCryptProtectData(
    _In_ DATA_BLOB *pDataIn,
    _In_opt_ LPCWSTR szDataDescr,
    _In_opt_ DATA_BLOB *pOptionalEntropy,
    _Reserved_ PVOID pvReserved,
    _In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
    _In_ DWORD dwFlags,
    _Out_ DATA_BLOB *pDataOut)
{
    pDataOut->cbData = pDataIn->cbData;
    pDataOut->pbData = (BYTE *)LocalAlloc(LMEM_FIXED, pDataOut->cbData);
    memcpy(pDataOut->pbData, pDataIn->pbData, pDataOut->cbData);
    return true;
}

typedef BOOL(WINAPI *pCryptProtectData)(
    _In_ DATA_BLOB *pDataIn,
    _In_opt_ LPCWSTR szDataDescr,
    _In_opt_ DATA_BLOB *pOptionalEntropy,
    _Reserved_ PVOID pvReserved,
    _In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
    _In_ DWORD dwFlags,
    _Out_ DATA_BLOB *pDataOut);

pCryptProtectData RawCryptProtectData = nullptr;

typedef BOOL(WINAPI *pCryptUnprotectData)(
    _In_ DATA_BLOB *pDataIn,
    _Out_opt_ LPWSTR *ppszDataDescr,
    _In_opt_ DATA_BLOB *pOptionalEntropy,
    _Reserved_ PVOID pvReserved,
    _In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
    _In_ DWORD dwFlags,
    _Out_ DATA_BLOB *pDataOut);

pCryptUnprotectData RawCryptUnprotectData = nullptr;

BOOL WINAPI MyCryptUnprotectData(
    _In_ DATA_BLOB *pDataIn,
    _Out_opt_ LPWSTR *ppszDataDescr,
    _In_opt_ DATA_BLOB *pOptionalEntropy,
    _Reserved_ PVOID pvReserved,
    _In_opt_ CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
    _In_ DWORD dwFlags,
    _Out_ DATA_BLOB *pDataOut)
{
    if (RawCryptUnprotectData(pDataIn, ppszDataDescr, pOptionalEntropy, pvReserved, pPromptStruct, dwFlags, pDataOut))
    {
        return true;
    }

    pDataOut->cbData = pDataIn->cbData;
    pDataOut->pbData = (BYTE *)LocalAlloc(LMEM_FIXED, pDataOut->cbData);
    memcpy(pDataOut->pbData, pDataIn->pbData, pDataOut->cbData);
    return true;
}

typedef BOOL(WINAPI *pLogonUserW)(
    LPCWSTR lpszUsername,
    LPCWSTR lpszDomain,
    LPCWSTR lpszPassword,
    DWORD dwLogonType,
    DWORD dwLogonProvider,
    PHANDLE phToken);

pLogonUserW RawLogonUserW = nullptr;

BOOL WINAPI MyLogonUserW(
    LPCWSTR lpszUsername,
    LPCWSTR lpszDomain,
    LPCWSTR lpszPassword,
    DWORD dwLogonType,
    DWORD dwLogonProvider,
    PHANDLE phToken)
{
    BOOL ret = RawLogonUserW(lpszUsername, lpszDomain, lpszPassword, dwLogonType, dwLogonProvider, phToken);

    SetLastError(ERROR_ACCOUNT_RESTRICTION);
    return ret;
}

typedef BOOL(WINAPI *pIsOS)(DWORD dwOS);

pIsOS RawIsOS = nullptr;

BOOL WINAPI MyIsOS(
    DWORD dwOS)
{
    DWORD ret = RawIsOS(dwOS);
    if (dwOS == OS_DOMAINMEMBER)
    {
        return false;
    }

    return ret;
}

typedef NET_API_STATUS(WINAPI *pNetUserGetInfo)(
    LPCWSTR servername,
    LPCWSTR username,
    DWORD level,
    LPBYTE *bufptr);

pNetUserGetInfo RawNetUserGetInfo = nullptr;

NET_API_STATUS WINAPI MyNetUserGetInfo(
    LPCWSTR servername,
    LPCWSTR username,
    DWORD level,
    LPBYTE *bufptr)
{
    // DebugLog(L"MyNetUserGetInfo %s", username);

    NET_API_STATUS ret = RawNetUserGetInfo(servername, username, level, bufptr);
    if (level == 1 && ret == 0)
    {
        LPUSER_INFO_1 user_info = (LPUSER_INFO_1)*bufptr;
        // DebugLog(L"user_info %d %s", user_info->usri1_password_age, user_info->usri1_name);
        user_info->usri1_password_age = 0;
        // DebugLog(L"user_info %d", user_info->usri1_password_age);

        // DebugLog(L"User account name: %s\n", user_info->usri1_name);
        // DebugLog(L"Password: %s\n", user_info->usri1_password);
        // DebugLog(L"Password age (seconds): %d\n", user_info->usri1_password_age);
        // DebugLog(L"Privilege level: %d\n", user_info->usri1_priv);
        // DebugLog(L"Home directory: %s\n", user_info->usri1_home_dir);
        // DebugLog(L"User comment: %s\n", user_info->usri1_comment);
        // DebugLog(L"Flags (in hex): %x\n", user_info->usri1_flags);
        // DebugLog(L"Script path: %s\n", user_info->usri1_script_path);
    }

    return ret;
}

#define PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON (0x00000001ui64 << 44)
#ifndef PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON
#define PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON (0x00000001ui64 << 28)
#endif

typedef BOOL(WINAPI *pUpdateProcThreadAttribute)(
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
    DWORD dwFlags,
    DWORD_PTR Attribute,
    PVOID lpValue,
    SIZE_T cbSize,
    PVOID lpPreviousValue,
    PSIZE_T lpReturnSize);

pUpdateProcThreadAttribute RawUpdateProcThreadAttribute = nullptr;

BOOL WINAPI MyUpdateProcThreadAttribute(
    __inout LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
    __in DWORD dwFlags,
    __in DWORD_PTR Attribute,
    __in_bcount_opt(cbSize) PVOID lpValue,
    __in SIZE_T cbSize,
    __out_bcount_opt(cbSize) PVOID lpPreviousValue,
    __in_opt PSIZE_T lpReturnSize)
{
    if (Attribute == PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY && cbSize >= sizeof(DWORD64))
    {
        // https://source.chromium.org/chromium/chromium/src/+/main:sandbox/win/src/process_mitigations.cc;l=362;drc=4c2fec5f6699ffeefd93137d2bf8c03504c6664c
        PDWORD64 policy_value_1 = &((PDWORD64)lpValue)[0];

        // Always remove the block on non-Microsoft binaries (required for portable mode)
        *policy_value_1 &= ~PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON;

        // Conditionally handle win32k system call policy based on config
        // Default (win32k=0): Keep Chrome's win32k lockdown for security and proper GPU acceleration
        // Optional (win32k=1): Force enable win32k only if Chrome crashes at startup
        //
        // IMPORTANT: Forcing win32k enabled can break:
        // - Hardware video decoding (causes Twitch/YouTube to use software decode)
        // - GPU acceleration in renderer processes
        // - Media Source Extensions (MSE) performance
        // Only enable if absolutely necessary for compatibility
        if (Config::Instance().IsWin32KEnabled())
        {
            *policy_value_1 &= ~PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON;
        }
    }
    return RawUpdateProcThreadAttribute(lpAttributeList, dwFlags, Attribute, lpValue, cbSize, lpPreviousValue, lpReturnSize);
}

void MakeGreen()
{
    // Initialize function pointers with original API addresses
    auto RawGetComputerNameW = GetComputerNameW;
    auto RawGetVolumeInformationW = GetVolumeInformationW;
    RawUpdateProcThreadAttribute = UpdateProcThreadAttribute;
    RawCryptProtectData = CryptProtectData;
    RawCryptUnprotectData = CryptUnprotectData;
    RawLogonUserW = LogonUserW;
    RawIsOS = IsOS;
    RawNetUserGetInfo = NetUserGetInfo;

    // Begin Detours transaction to install all hooks atomically
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // Attach hooks for portable mode support
    DetourAttach(reinterpret_cast<LPVOID*>(&RawGetComputerNameW), reinterpret_cast<void*>(FakeGetComputerName));
    DetourAttach(reinterpret_cast<LPVOID*>(&RawGetVolumeInformationW), reinterpret_cast<void*>(FakeGetVolumeInformation));
    DetourAttach(reinterpret_cast<LPVOID*>(&RawUpdateProcThreadAttribute), reinterpret_cast<void*>(MyUpdateProcThreadAttribute));
    DetourAttach(reinterpret_cast<LPVOID*>(&RawCryptProtectData), reinterpret_cast<void*>(MyCryptProtectData));
    DetourAttach(reinterpret_cast<LPVOID*>(&RawCryptUnprotectData), reinterpret_cast<void*>(MyCryptUnprotectData));
    DetourAttach(reinterpret_cast<LPVOID*>(&RawLogonUserW), reinterpret_cast<void*>(MyLogonUserW));
    DetourAttach(reinterpret_cast<LPVOID*>(&RawIsOS), reinterpret_cast<void*>(MyIsOS));
    DetourAttach(reinterpret_cast<LPVOID*>(&RawNetUserGetInfo), reinterpret_cast<void*>(MyNetUserGetInfo));

    // Commit all hooks in one transaction
    LONG status = DetourTransactionCommit();
    if (status != NO_ERROR)
    {
        DebugLog(L"MakeGreen DetourTransactionCommit failed: %d", status);
    }
}

#endif  // VIVALDI_PLUS_GREEN_H_
