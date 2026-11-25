#include "hotkey.h"

#include <windows.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <tlhelp32.h>
#include <wrl/client.h>

#include <chrono>
#include <condition_variable>
#include <iterator>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.h"
#include "utils.h"

using Microsoft::WRL::ComPtr;

namespace bosskey {

namespace {

constexpr auto UNMUTE_RETRY_DELAY = std::chrono::milliseconds(500);
constexpr ULONGLONG PID_CACHE_DURATION_MS = 5000;

struct MuteSnapshot {
  bool muted;
  bool restored = false;
};

struct AudioState {
  std::unordered_map<std::wstring, MuteSnapshot> original_mute_states;
  bool had_unmuted_sessions = false;
};

struct ComApartment {
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  ~ComApartment() {
    if (SUCCEEDED(result)) CoUninitialize();
  }
};

class ProcessCache {
public:
  const std::vector<DWORD>& GetPids() {
    const ULONGLONG current_time = GetTickCount64();
    if (cached_pids_.empty() || current_time - last_update_time_ > PID_CACHE_DURATION_MS) {
      cached_pids_ = GetAppPidsInternal();
      last_update_time_ = current_time;
    }
    return cached_pids_;
  }

private:
  static std::vector<DWORD> GetAppPidsInternal();
  std::vector<DWORD> cached_pids_;
  ULONGLONG last_update_time_ = 0;
};

BOOL CALLBACK HideBrowserWindow(HWND hwnd, LPARAM parameter) {
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;

  wchar_t class_name[32];
  if (GetClassNameW(hwnd, class_name, static_cast<int>(std::size(class_name))) &&
      wcscmp(class_name, L"Chrome_WidgetWin_1") == 0) {
    ShowWindow(hwnd, SW_HIDE);
    reinterpret_cast<std::vector<HWND>*>(parameter)->push_back(hwnd);
  }
  return TRUE;
}

// Internal implementation for getting all PIDs of current application
std::vector<DWORD> ProcessCache::GetAppPidsInternal() {
  std::vector<DWORD> pids;
  std::vector<wchar_t> current_exe_path(32768);
  const auto length = GetModuleFileNameW(nullptr, current_exe_path.data(), static_cast<DWORD>(current_exe_path.size()));
  if (!length || length >= current_exe_path.size()) return pids;
  const wchar_t* separator = wcsrchr(current_exe_path.data(), L'\\');
  const wchar_t* exe_name = separator ? separator + 1 : current_exe_path.data();

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return pids;
  }

  PROCESSENTRY32W pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32W);

  if (Process32FirstW(snapshot, &pe32)) {
    do {
      if (_wcsicmp(pe32.szExeFile, exe_name) == 0) {
        pids.emplace_back(pe32.th32ProcessID);
      }
    } while (Process32NextW(snapshot, &pe32));
  }

  CloseHandle(snapshot);
  return pids;
}

// Collect all active audio devices (default + all active devices)
std::vector<ComPtr<IMMDevice>> CollectAudioDevices(IMMDeviceEnumerator* enumerator) {
  std::vector<ComPtr<IMMDevice>> devices;
  std::unordered_set<std::wstring> seen_device_ids;

  if (!enumerator) {
    return devices;
  }

  auto add_device = [&](const ComPtr<IMMDevice>& device) {
    if (!device) return;

    LPWSTR device_id = nullptr;
    if (SUCCEEDED(device->GetId(&device_id)) && device_id) {
      if (seen_device_ids.insert(device_id).second) {
        devices.emplace_back(device);
      }
      CoTaskMemFree(device_id);
    }
  };

  // Add default devices for all roles
  const ERole roles[] = {eConsole, eMultimedia, eCommunications};
  for (auto role : roles) {
    ComPtr<IMMDevice> device;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, role, &device))) {
      add_device(device);
    }
  }

  // Add all active devices
  ComPtr<IMMDeviceCollection> collection;
  if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
    UINT count = 0;
    if (SUCCEEDED(collection->GetCount(&count))) {
      for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(i, &device))) {
          add_device(device);
        }
      }
    }
  }

  return devices;
}

void MuteProcess(const std::vector<DWORD>& pids, bool hide, AudioState& state) {
  if (pids.empty()) return;
  ComApartment apartment;
  if (FAILED(apartment.result)) return;

  ComPtr<IMMDeviceEnumerator> enumerator;
  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&enumerator))) || !enumerator) {
    return;
  }

  const std::unordered_set<DWORD> pid_set(pids.begin(), pids.end());
  for (const auto& device : CollectAudioDevices(enumerator.Get())) {
    ComPtr<IAudioSessionManager2> manager;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                nullptr, IID_PPV_ARGS_Helper(&manager))) || !manager) {
      continue;
    }

    ComPtr<IAudioSessionEnumerator> sessions;
    int count = 0;
    if (FAILED(manager->GetSessionEnumerator(&sessions)) || !sessions ||
        FAILED(sessions->GetCount(&count))) continue;

    for (int i = 0; i < count; ++i) {
      ComPtr<IAudioSessionControl> session;
      ComPtr<IAudioSessionControl2> session2;
      DWORD pid = 0;
      if (FAILED(sessions->GetSession(i, &session)) || !session ||
          FAILED(session.As(&session2)) || FAILED(session2->GetProcessId(&pid)) ||
          !pid_set.contains(pid)) continue;

      ComPtr<ISimpleAudioVolume> volume;
      LPWSTR session_id = nullptr;
      if (FAILED(session2.As(&volume)) ||
          FAILED(session2->GetSessionInstanceIdentifier(&session_id)) || !session_id) continue;

      if (hide) {
        BOOL was_muted = FALSE;
        if (SUCCEEDED(volume->GetMute(&was_muted))) {
          const auto [snapshot, inserted] = state.original_mute_states.try_emplace(session_id, MuteSnapshot{was_muted != FALSE});
          if (!inserted && snapshot->second.restored) snapshot->second = {was_muted != FALSE};
          if (!was_muted) state.had_unmuted_sessions = true;
          volume->SetMute(TRUE, nullptr);
        }
      } else {
        const auto original = state.original_mute_states.find(session_id);
        if (original != state.original_mute_states.end()) {
          original->second.restored = SUCCEEDED(volume->SetMute(original->second.muted ? TRUE : FALSE, nullptr));
        } else if (state.had_unmuted_sessions) {
          volume->SetMute(FALSE, nullptr);
        }
      }
      CoTaskMemFree(session_id);
    }
  }
}

class AudioWorker {
public:
  void SetHidden(bool hidden) {
    if (!thread_) {
      if (!hidden) return;
      thread_ = CreateThread(nullptr, 0, Entry, this, 0, nullptr);
      if (!thread_) return;
    }
    {
      std::lock_guard lock(mutex_);
      desired_hidden_ = hidden;
    }
    changed_.notify_one();
  }

  ~AudioWorker() {
    if (!thread_) return;
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
    }
    changed_.notify_one();
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
  }

private:
  static DWORD WINAPI Entry(void* worker) {
    static_cast<AudioWorker*>(worker)->Run();
    return 0;
  }

  void Run() {
    AudioState state;
    ProcessCache pids;
    bool hidden = false;
    bool retry_pending = false;
    std::chrono::steady_clock::time_point retry_at;
    std::unique_lock lock(mutex_);
    const auto changed = [&] { return stopping_ || desired_hidden_ != hidden; };

    while (!stopping_) {
      bool retry = false;
      if (!changed()) {
        if (!retry_pending) {
          changed_.wait(lock, changed);
          continue;
        }
        if (changed_.wait_until(lock, retry_at, changed)) continue;
        retry = true;
      }

      hidden = desired_hidden_;
      lock.unlock();

      MuteProcess(pids.GetPids(), hidden, state);
      retry_pending = !hidden && !retry && state.had_unmuted_sessions;
      if (retry_pending) {
        retry_at = std::chrono::steady_clock::now() + UNMUTE_RETRY_DELAY;
      } else if (!hidden) {
        state = {};
      }

      lock.lock();
    }
  }

  HANDLE thread_ = nullptr;
  std::mutex mutex_;
  std::condition_variable changed_;
  bool desired_hidden_ = false;
  bool stopping_ = false;
};

void RestoreBrowserWindows(std::vector<HWND>& windows) {
  for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
    const HWND window = *it;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != GetCurrentProcessId()) continue;
    ShowWindow(window, SW_SHOW);
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(window);
    SetWindowPos(window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetActiveWindow(window);
  }
  windows.clear();
}

DWORD WINAPI HotkeyThread(void* parameter) {
  const auto flag = static_cast<UINT>(reinterpret_cast<UINT_PTR>(parameter));
  if (!RegisterHotKey(nullptr, 0, LOWORD(flag), HIWORD(flag))) return 0;

  AudioWorker audio;
  std::vector<HWND> windows;
  bool hidden = false;
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (msg.message == WM_HOTKEY) {
      hidden = !hidden;
      if (hidden) {
        EnumWindows(HideBrowserWindow, reinterpret_cast<LPARAM>(&windows));
      } else {
        RestoreBrowserWindows(windows);
      }
      audio.SetHidden(hidden);
    }
  }
  UnregisterHotKey(nullptr, 0);
  return 0;
}

}  // namespace

void Initialize() {
  const auto& keys = GetConfig().GetBossKey();
  if (keys.empty()) return;
  const UINT flag = ParseHotkeys(keys);
  if (!HIWORD(flag)) return;
  HANDLE thread = CreateThread(nullptr, 0, HotkeyThread, reinterpret_cast<void*>(static_cast<UINT_PTR>(flag)), 0, nullptr);
  if (thread) CloseHandle(thread);
}

}  // namespace bosskey
