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

void HandleTestTypeUISwitch(LPWSTR param)
{
    // Implement the necessary logic to ensure that the DLL works correctly,
    // the portability is not broken, and the installed extensions do not disappear
    // when the "--test-type-ui" switch is used.
}

void VivaldiPlusCommand(LPWSTR param)
{
    if (wcsstr(param, L"--test-type-ui"))
    {
        HandleTestTypeUISwitch(param);
    }
    else if (!wcsstr(param, L"--gopher"))
    {
        Portable(param);
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
        VivaldiPlusCommand(param);
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

void HandleTestTypeUISwitchInDllMain(LPWSTR param)
{
    // Implement the necessary logic to ensure that the DLL works correctly,
    // the portability is not broken, and the installed extensions do not disappear
    // when the "--test-type-ui" switch is used.
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
            InstallLoader();
        }
        else
        {
            DebugLog(L"MH_Initialize failed:%d", status);
        }

        LPWSTR param = GetCommandLineW();
        if (wcsstr(param, L"--test-type-ui"))
        {
            HandleTestTypeUISwitchInDllMain(param);
        }
    }
    return TRUE;
}
