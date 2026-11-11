#include "lifecycle_owners.h"
#include <vector>
#include <cwchar>

bool LifecycleCreationTime(HANDLE process, FILETIME& created) {
    FILETIME exited{}, kernel{}, user{};
    return GetProcessTimes(process, &created, &exited, &kernel, &user) != FALSE;
}
namespace {
bool IsAlive(const LifecycleParticipant& owner) {
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, owner.pid);
    if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
    FILETIME created{};
    const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT &&
        LifecycleCreationTime(process, created) && CompareFileTime(&created, &owner.created) == 0;
    CloseHandle(process);
    return alive;
}
bool ProfileWindow(HWND window) {
    int length = GetWindowTextLengthW(window);
    if (length <= 0 || length > 32767) return false;
    std::vector<wchar_t> title(static_cast<size_t>(length) + 1);
    if (!GetWindowTextW(window, title.data(), static_cast<int>(title.size()))) return false;
    DWORD attributes = GetFileAttributesW(title.data());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}
bool SameInstallation(HANDLE process, const std::wstring& appDir) {
    std::vector<wchar_t> image(32768);
    DWORD length = static_cast<DWORD>(image.size());
    if (!QueryFullProcessImageNameW(process, 0, image.data(), &length)) return false;
    std::wstring directory(image.data(), length);
    const size_t separator = directory.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return false;
    directory.resize(separator);
    return _wcsicmp(directory.c_str(), appDir.c_str()) == 0;
}
} // namespace
size_t CleanLifecycleOwners(LifecycleSharedState& state) {
    size_t count = 0;
    for (auto& owner : state.owners) {
        if (owner.pid && !IsAlive(owner)) owner = {};
        if (owner.pid) ++count;
    }
    return count;
}
bool HasLifecycleOwner(const LifecycleSharedState& state, const LifecycleParticipant& participant) {
    for (const auto& owner : state.owners) {
        if (owner.pid == participant.pid && CompareFileTime(&owner.created, &participant.created) == 0) return true;
    }
    return false;
}
bool AddLifecycleOwner(LifecycleSharedState& state, const LifecycleParticipant& participant) {
    if (HasLifecycleOwner(state, participant)) return true;
    for (auto& owner : state.owners) {
        if (!owner.pid) { owner = participant; return true; }
    }
    return false;
}
bool OwnsLifecycleProfileWindow() {
    HWND window = nullptr;
    while ((window = FindWindowExW(HWND_MESSAGE, window, L"Chrome_MessageWindow", nullptr))) {
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (pid == GetCurrentProcessId() && ProfileWindow(window)) return true;
    }
    return false;
}
void AdoptLifecycleWindowOwners(LifecycleSharedState& state, const std::wstring& appDir, DWORD exitingPid) {
    HWND window = nullptr;
    while ((window = FindWindowExW(HWND_MESSAGE, window, L"Chrome_MessageWindow", nullptr))) {
        LifecycleParticipant participant{};
        GetWindowThreadProcessId(window, &participant.pid);
        if (!participant.pid || participant.pid == exitingPid || !ProfileWindow(window)) continue;
        HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, participant.pid);
        if (!process) continue;
        if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT && SameInstallation(process, appDir) &&
            LifecycleCreationTime(process, participant.created)) AddLifecycleOwner(state, participant);
        CloseHandle(process);
    }
}
