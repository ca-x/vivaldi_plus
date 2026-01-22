#include "hotkey.h"

#include <windows.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.h"
#include "utils.h"

namespace bosskey {

namespace {

using HotkeyAction = void (*)();

// Delayed unmute retry configuration
constexpr UINT UNMUTE_RETRY_DELAY_MS = 500;

// PID cache to avoid frequent process enumeration
constexpr ULONGLONG PID_CACHE_DURATION_MS = 5000;  // 5 seconds

// Lazy-initialized state variables (only created when bosskey is actually used)
struct BossKeyState {
    std::atomic<bool> is_hide{false};
    std::vector<HWND> hwnd_list;
    std::unordered_map<std::wstring, bool> original_mute_states;
    std::atomic<bool> has_unmuted_sessions{false};
    std::mutex audio_state_mutex;
    HANDLE unmute_retry_timer{nullptr};
    std::mutex timer_mutex;
};

// Get singleton state instance (lazy initialization)
BossKeyState& GetState() {
    static BossKeyState state;
    return state;
}

class ProcessCache {
public:
    std::vector<DWORD> GetPids() {
        ULONGLONG current_time = GetTickCount64();

        // Use lock to ensure thread-safe access
        std::lock_guard<std::mutex> lock(cache_mutex_);

        // Update cache if expired or empty
        if (cached_pids_.empty() ||
            (current_time - last_update_time_) > PID_CACHE_DURATION_MS) {
            cached_pids_ = GetAppPidsInternal();
            last_update_time_ = current_time;
        }

        return cached_pids_;
    }

    // Force refresh cache (call when process state might have changed)
    void InvalidateCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cached_pids_.clear();
        last_update_time_ = 0;
    }

private:
    std::vector<DWORD> GetAppPidsInternal();

    std::vector<DWORD> cached_pids_;
    ULONGLONG last_update_time_ = 0;
    std::mutex cache_mutex_;
};

// Get PID cache instance (lazy initialization)
ProcessCache& GetProcessCache() {
    static ProcessCache cache;
    return cache;
}

// Search for Vivaldi browser windows
BOOL CALLBACK SearchVivaldiWindow(HWND hwnd, LPARAM lparam) {
  if (IsWindowVisible(hwnd)) {
    wchar_t class_name[256];
    GetClassNameW(hwnd, class_name, 255);

    // Vivaldi uses the same window class as Chrome
    if (wcscmp(class_name, L"Chrome_WidgetWin_1") == 0) {
      DWORD pid;
      GetWindowThreadProcessId(hwnd, &pid);
      if (pid == GetCurrentProcessId()) {
        ShowWindow(hwnd, SW_HIDE);
        GetState().hwnd_list.emplace_back(hwnd);
      }
    }
  }
  return true;
}

// Internal implementation for getting all PIDs of current application
std::vector<DWORD> ProcessCache::GetAppPidsInternal() {
  std::vector<DWORD> pids;
  wchar_t current_exe_path[MAX_PATH];
  GetModuleFileNameW(nullptr, current_exe_path, MAX_PATH);

  wchar_t* exe_name = wcsrchr(current_exe_path, L'\\');
  if (exe_name) {
    ++exe_name;
  } else {
    exe_name = current_exe_path;
  }

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

// Get all PIDs of current application (using cache)
std::vector<DWORD> GetAppPids() {
  return GetProcessCache().GetPids();
}
// Collect all active audio devices (default + all active devices)
std::vector<IMMDevice*> CollectAudioDevices(IMMDeviceEnumerator* enumerator) {
  std::vector<IMMDevice*> devices;
  std::unordered_set<std::wstring> seen_device_ids;

  if (!enumerator) {
    return devices;
  }

  auto add_device = [&](IMMDevice* device) {
    if (!device) return;

    LPWSTR device_id = nullptr;
    if (SUCCEEDED(device->GetId(&device_id)) && device_id) {
      if (seen_device_ids.insert(device_id).second) {
        devices.emplace_back(device);
      } else {
        device->Release();
      }
      CoTaskMemFree(device_id);
    } else {
      device->Release();
    }
  };

  // Add default devices for all roles
  const ERole roles[] = {eConsole, eMultimedia, eCommunications};
  for (auto role : roles) {
    IMMDevice* device = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, role, &device))) {
      add_device(device);
    }
  }

  // Add all active devices
  IMMDeviceCollection* collection = nullptr;
  if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
    UINT count = 0;
    if (SUCCEEDED(collection->GetCount(&count))) {
      for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (SUCCEEDED(collection->Item(i, &device))) {
          add_device(device);
        }
      }
    }
    collection->Release();
  }

  return devices;
}

// Mute or unmute process audio sessions
// Returns true if any session was found
bool MuteProcess(const std::vector<DWORD>& pids,
                 bool set_mute,
                 bool save_mute_state = false) {
  bool found_any_session = false;

  // Convert PIDs to unordered_set for O(1) lookup
  std::unordered_set<DWORD> pid_set(pids.begin(), pids.end());

  HRESULT hr = CoInitialize(nullptr);
  const bool should_uninit = (hr == S_OK || hr == S_FALSE);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    return false;
  }

  IMMDeviceEnumerator* enumerator = nullptr;
  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&enumerator))) || !enumerator) {
    if (should_uninit) CoUninitialize();
    return false;
  }

  // Collect all audio devices
  auto devices = CollectAudioDevices(enumerator);
  enumerator->Release();

  // Process each device
  for (auto* device : devices) {
    IAudioSessionManager2* manager = nullptr;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                nullptr, (void**)&manager)) || !manager) {
      device->Release();
      continue;
    }

    IAudioSessionEnumerator* session_enumerator = nullptr;
    if (SUCCEEDED(manager->GetSessionEnumerator(&session_enumerator))) {
      int session_count = 0;
      session_enumerator->GetCount(&session_count);

      for (int i = 0; i < session_count; ++i) {
        IAudioSessionControl* session = nullptr;
        if (FAILED(session_enumerator->GetSession(i, &session)) || !session) {
          continue;
        }

        IAudioSessionControl2* session2 = nullptr;
        if (SUCCEEDED(session->QueryInterface(__uuidof(IAudioSessionControl2),
                                              (void**)&session2))) {
          DWORD session_pid = 0;
          session2->GetProcessId(&session_pid);

          // Check if this session belongs to our process (O(1) lookup with unordered_set)
          bool is_our_process = pid_set.find(session_pid) != pid_set.end();

          if (is_our_process) {
            found_any_session = true;

            // Get unique session instance ID (GUID)
            LPWSTR session_id = nullptr;
            if (SUCCEEDED(session2->GetSessionInstanceIdentifier(&session_id)) && session_id) {
              ISimpleAudioVolume* volume = nullptr;
              if (SUCCEEDED(session2->QueryInterface(__uuidof(ISimpleAudioVolume),
                                                     (void**)&volume))) {
                BOOL is_muted = FALSE;
                volume->GetMute(&is_muted);

                if (save_mute_state) {
                  // Save the current mute state for this specific session (thread-safe)
                  auto& state = GetState();
                  std::lock_guard<std::mutex> lock(state.audio_state_mutex);
                  state.original_mute_states[session_id] = (is_muted == TRUE);
                  if (is_muted == FALSE) {
                    state.has_unmuted_sessions.store(true, std::memory_order_release);
                  }
                }

                if (set_mute) {
                  // Mute all sessions
                  volume->SetMute(TRUE, nullptr);
                } else {
                  // Restore original mute state for this session
                  auto& state = GetState();
                  std::lock_guard<std::mutex> lock(state.audio_state_mutex);
                  auto it = state.original_mute_states.find(session_id);
                  if (it != state.original_mute_states.end()) {
                    // Restore to the original state (muted or unmuted)
                    volume->SetMute(it->second ? TRUE : FALSE, nullptr);
                  } else {
                    // Session not found in saved states (might be newly created)
                    // Unmute if we had any unmuted sessions originally
                    if (state.has_unmuted_sessions.load(std::memory_order_acquire)) {
                      volume->SetMute(FALSE, nullptr);
                    }
                  }
                }
                volume->Release();
              }
              CoTaskMemFree(session_id);
            }
          }
          session2->Release();
        }
        session->Release();
      }
      session_enumerator->Release();
    }
    manager->Release();
    device->Release();
  }

  if (should_uninit) {
    CoUninitialize();
  }

  return found_any_session;
}

// Delayed unmute retry handler (runs in timer thread)
VOID CALLBACK HandleUnmuteRetry(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
  auto& state = GetState();

  // Stop and cleanup timer
  {
    std::lock_guard<std::mutex> lock(state.timer_mutex);
    if (state.unmute_retry_timer) {
      DeleteTimerQueueTimer(nullptr, state.unmute_retry_timer, nullptr);
      state.unmute_retry_timer = nullptr;
    }
  }

  if (state.is_hide.load(std::memory_order_acquire)) {
    // Still hidden, don't retry
    return;
  }

  // Run unmute in a separate thread to avoid blocking timer thread
  std::thread([=]() {
    auto vivaldi_pids = GetAppPids();
    MuteProcess(vivaldi_pids, false, false);

    // Clean up saved states after retry (thread-safe)
    auto& state = GetState();
    {
      std::lock_guard<std::mutex> lock(state.audio_state_mutex);
      state.original_mute_states.clear();
    }
    state.has_unmuted_sessions.store(false, std::memory_order_release);
  }).detach();
}

// Toggle hide/show windows and mute/unmute audio
void HideAndShow() {
  // Get PIDs from cache (fast: ~0.1ms if cached, ~15ms on first call)
  auto vivaldi_pids = GetAppPids();

  auto& state = GetState();
  bool current_hide_state = state.is_hide.load(std::memory_order_acquire);

  if (!current_hide_state) {
    // ===== HIDE MODE =====
    // 1. Stop any pending retry timer
    {
      std::lock_guard<std::mutex> lock(state.timer_mutex);
      if (state.unmute_retry_timer) {
        DeleteTimerQueueTimer(nullptr, state.unmute_retry_timer, INVALID_HANDLE_VALUE);
        state.unmute_retry_timer = nullptr;
      }
    }

    // 2. Clear saved audio states (thread-safe)
    {
      std::lock_guard<std::mutex> lock(state.audio_state_mutex);
      state.original_mute_states.clear();
    }
    state.has_unmuted_sessions.store(false, std::memory_order_release);

    // 3. Hide windows immediately (this must be synchronous for user experience)
    EnumWindows(SearchVivaldiWindow, 0);

    // 4. Update hide state before async audio processing
    state.is_hide.store(true, std::memory_order_release);

    // 5. Mute audio asynchronously (don't block window hiding)
    std::thread([vivaldi_pids]() {
      MuteProcess(vivaldi_pids, true, true);
    }).detach();

  } else {
    // ===== SHOW MODE =====
    // 1. Update hide state first
    state.is_hide.store(false, std::memory_order_release);

    // 2. Restore windows immediately (synchronous for smooth UX)
    for (auto r_iter = state.hwnd_list.rbegin(); r_iter != state.hwnd_list.rend(); ++r_iter) {
      ShowWindow(*r_iter, SW_SHOW);
      SetWindowPos(*r_iter, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      SetForegroundWindow(*r_iter);
      SetWindowPos(*r_iter, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      SetActiveWindow(*r_iter);
    }
    state.hwnd_list.clear();

    // 3. Unmute audio asynchronously (don't block window showing)
    std::thread([vivaldi_pids]() {
      bool found_sessions = MuteProcess(vivaldi_pids, false);

      auto& state = GetState();
      // If we found sessions and had unmuted ones, set up a retry timer
      // to handle late-loading audio sessions
      if (found_sessions && state.has_unmuted_sessions.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(state.timer_mutex);
        CreateTimerQueueTimer(
          &state.unmute_retry_timer,
          nullptr,
          HandleUnmuteRetry,
          nullptr,
          UNMUTE_RETRY_DELAY_MS,
          0,  // One-shot timer
          WT_EXECUTEINTIMERTHREAD
        );
      } else {
        // No need to retry, clean up now
        {
          std::lock_guard<std::mutex> lock(state.audio_state_mutex);
          state.original_mute_states.clear();
        }
        state.has_unmuted_sessions.store(false, std::memory_order_release);
      }
    }).detach();
  }
}

// Handle hotkey message
void OnHotkey(HotkeyAction action) {
  action();
}

// Register hotkey and start message loop in a separate thread
void RegisterHotkeyThread(std::wstring_view keys, HotkeyAction action) {
  if (keys.empty()) {
    return;
  }

  UINT flag = ParseHotkeys(keys);

  std::thread th([flag, action]() {
    RegisterHotKey(nullptr, 0, LOWORD(flag), HIWORD(flag));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
      if (msg.message == WM_HOTKEY) {
        OnHotkey(action);
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  });

  th.detach();
}

}  // anonymous namespace

// Public interface
void Initialize() {
  // Early return if boss key is not configured
  // This ensures zero performance impact when feature is disabled
  const auto& boss_key = GetConfig().GetBossKey();
  if (boss_key.empty()) {
    return;  // No bosskey configured, no initialization needed
  }

  // Only register hotkey if bosskey is actually configured
  RegisterHotkeyThread(boss_key, HideAndShow);
}

}  // namespace bosskey
