#ifndef VIVALDI_PLUS_APPID_H_
#define VIVALDI_PLUS_APPID_H_

#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include "detours.h"

typedef HRESULT(WINAPI *pPSStringFromPropertyKey)(
    REFPROPERTYKEY pkey,
    LPWSTR psz,
    UINT cch);
pPSStringFromPropertyKey RawPSStringFromPropertyKey = nullptr;

HRESULT WINAPI MyPSStringFromPropertyKey(
    REFPROPERTYKEY pkey,
    LPWSTR psz,
    UINT cch)
{
    HRESULT result = RawPSStringFromPropertyKey(pkey, psz, cch);
    if (SUCCEEDED(result))
    {
        if (pkey == PKEY_AppUserModel_ID)
        {
            // DebugLog(L"MyPSStringFromPropertyKey %s", psz);
            return -1;
        }
    }
    return result;
}

void SetAppId()
{
    // Initialize function pointer with original API address
    RawPSStringFromPropertyKey = PSStringFromPropertyKey;

    // Create hook using Detours
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<LPVOID*>(&RawPSStringFromPropertyKey),
                 reinterpret_cast<void*>(MyPSStringFromPropertyKey));
    LONG status = DetourTransactionCommit();
    if (status != NO_ERROR)
    {
        DebugLog(L"DetourAttach PSStringFromPropertyKey failed: %d", status);
    }
}

#endif  // VIVALDI_PLUS_APPID_H_
