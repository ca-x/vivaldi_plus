#include "lifecycle.h"
#include "lifecycle_commands.h"
#include <windows.h>
#include <detours.h>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <new>
#include <vector>

std::wstring GetAppDir();
namespace {
constexpr DWORD kLockTimeout = 4500;
struct Participant { DWORD pid; FILETIME created; };
struct SharedState { Participant owners[64]; };
struct Context {
    LifecycleOptions options;
    std::wstring appDir;
    HANDLE mutex = nullptr;
    HANDLE mapping = nullptr;
    SharedState* shared = nullptr;
    Participant self{};
    std::atomic<bool> joined{false};
    std::atomic<bool> joining{false};
    std::atomic<bool> exiting{false};
};
std::atomic<bool> initialized{false};
std::atomic<Context*> context{nullptr};
auto originalExitProcess = &ExitProcess;

bool Lock(HANDLE mutex) {
    DWORD status = WaitForSingleObject(mutex, kLockTimeout);
    if (status == WAIT_OBJECT_0 || status == WAIT_ABANDONED) return true;
    OutputDebugStringW(L"[vivaldi++] Lifecycle mutex wait failed or timed out.\n");
    return false;
}
bool CreationTime(HANDLE process, FILETIME& created) {
    FILETIME exited{}, kernel{}, user{};
    return GetProcessTimes(process, &created, &exited, &kernel, &user) != FALSE;
}
bool IsAlive(const Participant& owner) {
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, owner.pid);
    if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
    FILETIME created{};
    const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT &&
        CreationTime(process, created) && CompareFileTime(&created, &owner.created) == 0;
    CloseHandle(process);
    return alive;
}
size_t Clean(SharedState& state) {
    size_t count = 0;
    for (auto& owner : state.owners) {
        if (owner.pid && !IsAlive(owner)) owner = {};
        if (owner.pid) ++count;
    }
    return count;
}
bool OwnsProfileWindow() {
    HWND window = nullptr;
    while ((window = FindWindowExW(HWND_MESSAGE, window, L"Chrome_MessageWindow", nullptr))) {
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (pid != GetCurrentProcessId()) continue;
        int length = GetWindowTextLengthW(window);
        if (length <= 0 || length > 32767) continue;
        std::vector<wchar_t> title(static_cast<size_t>(length) + 1);
        if (!GetWindowTextW(window, title.data(), static_cast<int>(title.size()))) continue;
        DWORD attributes = GetFileAttributesW(title.data());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) return true;
    }
    return false;
}
std::wstring ObjectName(const std::wstring& directory) {
    uint64_t hash = 14695981039346656037ULL;
    std::wstring lowercase = directory;
    CharLowerBuffW(lowercase.data(), static_cast<DWORD>(lowercase.size()));
    for (wchar_t ch : lowercase) {
        hash ^= static_cast<uint16_t>(ch);
        hash *= 1099511628211ULL;
    }
    wchar_t name[80]{};
    swprintf_s(name, L"Local\\VivaldiPlusLifecycle-v1-%016llx", static_cast<unsigned long long>(hash));
    return name;
}
void CloseState(Context& state) {
    if (state.shared) UnmapViewOfFile(state.shared);
    if (state.mapping) CloseHandle(state.mapping);
    if (state.mutex) CloseHandle(state.mutex);
    state.shared = nullptr;
    state.mapping = state.mutex = nullptr;
}
void Join(Context& state) {
    const auto name = ObjectName(state.appDir);
    state.mutex = CreateMutexW(nullptr, FALSE, (name + L"-mutex").c_str());
    if (!state.mutex) {
        OutputDebugStringW(L"[vivaldi++] Lifecycle mutex creation failed.\n");
        return;
    }
    if (!Lock(state.mutex)) { CloseState(state); return; }
    state.mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                     sizeof(SharedState), (name + L"-state").c_str());
    if (state.mapping) state.shared = static_cast<SharedState*>(MapViewOfFile(state.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState)));
    if (!state.shared) OutputDebugStringW(L"[vivaldi++] Lifecycle shared state creation failed.\n");
    if (state.shared && !state.exiting.load()) {
        const size_t count = Clean(*state.shared);
        for (auto& owner : state.shared->owners) {
            if (owner.pid) continue;
            owner = state.self;
            state.joined.store(true);
            if (count == 0) {
                RunLifecycleCommand(state.options.startup, state.appDir, false);
            }
            break;
        }
    }
    ReleaseMutex(state.mutex);
    if (!state.joined.load()) CloseState(state);
}
DWORD WINAPI WaitForOwner(void* parameter) {
    auto& state = *static_cast<Context*>(parameter);
    while (!state.exiting.load()) {
        if (OwnsProfileWindow()) {
            state.joining.store(true);
            if (!state.exiting.load()) Join(state);
            state.joining.store(false);
            return 0;
        }
        Sleep(100);
    }
    return 0;
}
void WINAPI LifecycleExitProcess(UINT code) {
    Context* state = context;
    if (state && !state->exiting.exchange(true)) {
        const ULONGLONG deadline = GetTickCount64() + kLockTimeout;
        while (state->joining.load() && GetTickCount64() < deadline) Sleep(1);
        if (state->joining.load() || !state->joined.load() || !Lock(state->mutex)) {
            originalExitProcess(code);
            return;
        }
        for (auto& owner : state->shared->owners) {
            if (owner.pid == state->self.pid && CompareFileTime(&owner.created, &state->self.created) == 0) owner = {};
        }
        if (Clean(*state->shared) == 0) {
            RunLifecycleCommand(state->options.exit, state->appDir, true);
        }
        ReleaseMutex(state->mutex);
    }
    originalExitProcess(code);
}
} // namespace

void InitializeLifecycle(const LifecycleOptions& options) {
    if (!options.enabled || (options.startup.empty() && options.exit.empty())) return;
    if (initialized.exchange(true)) return;
    std::unique_ptr<Context> owned(new (std::nothrow) Context);
    if (!owned) {
        OutputDebugStringW(L"[vivaldi++] Lifecycle context allocation failed.\n");
        return;
    }
    auto* state = owned.get();
    state->options = options;
    state->appDir = GetAppDir();
    state->self.pid = GetCurrentProcessId();
    if (state->appDir.empty() || !CreationTime(GetCurrentProcess(), state->self.created)) {
        OutputDebugStringW(L"[vivaldi++] Lifecycle process identification failed.\n");
        return;
    }
    context = state;
    if (DetourTransactionBegin() != NO_ERROR) {
        context = nullptr;
        OutputDebugStringW(L"[vivaldi++] Lifecycle hook transaction could not begin.\n");
        return;
    }
    LONG result = DetourUpdateThread(GetCurrentThread());
    if (result == NO_ERROR) result = DetourAttach(reinterpret_cast<PVOID*>(&originalExitProcess), reinterpret_cast<PVOID>(LifecycleExitProcess));
    if (result != NO_ERROR) {
        DetourTransactionAbort();
        context = nullptr;
        OutputDebugStringW(L"[vivaldi++] Lifecycle exit hook attachment failed.\n");
        return;
    }
    if (DetourTransactionCommit() != NO_ERROR) {
        context = nullptr;
        OutputDebugStringW(L"[vivaldi++] Lifecycle exit hook transaction failed.\n");
        return;
    }
    owned.release();
    // The process owns this context until ExitProcess; no DLL-detach waits.
    HANDLE thread = CreateThread(nullptr, 0, WaitForOwner, state, 0, nullptr);
    if (thread) CloseHandle(thread);
    else OutputDebugStringW(L"[vivaldi++] Lifecycle owner thread creation failed.\n");
}

