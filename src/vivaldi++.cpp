#include <windows.h>
#include <stdio.h>
#include <psapi.h>

HMODULE hInstance;

#define MAGIC_CODE 0x1603ABD9

#include "MinHook.h"
#include "version.h"

#include "hijack.h"
#include "utils.h"
#include "patch.h"
#include "featuresFlag.h"
#include "TabBookmark.h"
#include "portable.h"
#include "PakPatch.h"
#include "appid.h"
#include "green.h"

typedef int (*Startup)();
Startup ExeMain = NULL;

void VivaldiPlus()
{
    // 快捷方式
    SetAppId();

    // 读取特性标志
    ParseFeatureFlags();

    // 便携化补丁
    MakeGreen();

    // 标签页，书签，地址栏增强
    TabBookmark();

    // 给pak文件打补丁
    PakPatch();
}

void VivaldiPlusCommand(LPWSTR param)
{
    if (!wcsstr(param, L"--gopher"))
    {
        if (!wcsstr(param, L"--test-type-ui"))
        {
            // Handle "--test-type-ui" switch here
            // ...
        }
        else
        {
            Portable(param);
        }
    }
    else
    {
        VivaldiPlus();
    }
}

int Loader()
{
    // 硬补丁
    MakePatch();

    // 只关注主界面
    LPWSTR param = GetCommandLineW();
    // DebugLog(L"param %s", param);
    if (!wcsstr(param, L"-type="))
    {
        if (!wcsstr(param, L"--test-type-ui"))
        {
            // Handle "--test-type-ui" switch here
            // ...
        }
        else
        {
            VivaldiPlusCommand(param);
        }
    }

    // 返回到主程序
    return ExeMain();
}

void InstallLoader()
{
    // 获取程序入口点
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &mi, sizeof(MODULEINFO));
    PBYTE entry = (PBYTE)mi.EntryPoint;

    // 入口点跳转到Loader
    MH_STATUS status = MH_CreateHook(entry, Loader, (LPVOID *)&ExeMain);
}
    if (status == MH_OK)
    {
        MH_EnableHook(entry);
    }
    else
    {
        DebugLog(L"MH_CreateHook InstallLoader failed:%d", status);
    }
}
#define EXTERNC extern "C"

//
EXTERNC __declspec(dllexport) void gopher()
{
}

EXTERNC BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID pv)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        hInstance = hModule;

        // 保持系统dll原有功能
        LoadSysDll(hModule);

        // 初始化HOOK库成功以后安装加载器
        MH_STATUS status = MH_Initialize();
        if (status == MH_OK)
        {
            // Check for "--test-type-ui" switch before installing loader
            LPWSTR param = GetCommandLineW();
            if (!wcsstr(param, L"--test-type-ui"))
            {
                InstallLoader();
            }
            else
            {
                // Handle "--test-type-ui" switch here
                // ...
            }
        }
        else
        {
            DebugLog(L"MH_Initialize failed:%d", status);
        }
    }
    return TRUE;
}
