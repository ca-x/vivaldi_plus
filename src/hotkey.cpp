#include "hotkey.h"

#include <windows.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.h"
#include "utils.h"

namespace bosskey {
namespace {

using HotkeyAction = void (*)();

// Static variables for internal use
bool is_hide = false;
std::vector<HWND> hwnd_list;
std::unordered_map<std::wstring, bool> original_mute_states;
bool has_unmuted_sessions = false;  // Track if we had any unmuted sessions

// Timer ID for delayed unmute retry
constexpr UINT_PTR UNMUTE_RETRY_TIMER_ID = 1;
constexpr UINT UNMUTE_RETRY_DELAY_MS = 500;

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
        hwnd_list.emplace_back(hwnd);
      }
    }
  }
  return true;
}

// Get all PIDs of current application
std::vector<DWORD> GetAppPids() {
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
  if (pids.empty()) return false;

  bool found_any_session = false;

  // Convert PIDs to unordered_set for O(1) lookup
  const std::unordered_set<DWORD> pid_set(pids.begin(), pids.end());

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
                  // Save the current mute state for this specific session
                  original_mute_states[session_id] = (is_muted == TRUE);
                  if (is_muted == FALSE) {
                    has_unmuted_sessions = true;
                  }
                }

                if (set_mute) {
                  // Mute all sessions
                  volume->SetMute(TRUE, nullptr);
                } else {
                  // Restore original mute state for this session
                  auto it = original_mute_states.find(session_id);
                  if (it != original_mute_states.end()) {
                    // Restore to the original state (muted or unmuted)
                    volume->SetMute(it->second ? TRUE : FALSE, nullptr);
                  } else {
                    // Session not found in saved states (might be newly created)
                    // Unmute if we had any unmuted sessions originally
                    if (has_unmuted_sessions) {
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

// Delayed unmute retry handler
void HandleUnmuteRetry() {
  KillTimer(nullptr, UNMUTE_RETRY_TIMER_ID);

  if (is_hide) {
    // Still hidden, don't retry
    return;
  }

  auto vivaldi_pids = GetAppPids();
  MuteProcess(vivaldi_pids, false, false);

  // Clean up saved states after retry
  original_mute_states.clear();
  has_unmuted_sessions = false;
}

// Toggle hide/show windows and mute/unmute audio
void HideAndShow() {
  auto vivaldi_pids = GetAppPids();

  if (!is_hide) {
    // Hide: clear saved states, enumerate and hide all windows, then mute
    KillTimer(nullptr, UNMUTE_RETRY_TIMER_ID);
    original_mute_states.clear();
    has_unmuted_sessions = false;

    EnumWindows(SearchVivaldiWindow, 0);
    MuteProcess(vivaldi_pids, true, true);
  } else {
    // Show: restore windows in reverse order, then unmute
    for (auto r_iter = hwnd_list.rbegin(); r_iter != hwnd_list.rend(); ++r_iter) {
      ShowWindow(*r_iter, SW_SHOW);
      SetWindowPos(*r_iter, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      SetForegroundWindow(*r_iter);
      SetWindowPos(*r_iter, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      SetActiveWindow(*r_iter);
    }
    hwnd_list.clear();

    bool found_sessions = MuteProcess(vivaldi_pids, false);

    // If we found sessions and had unmuted ones, set up a retry timer
    // to handle late-loading audio sessions
    if (found_sessions && has_unmuted_sessions) {
      SetTimer(nullptr, UNMUTE_RETRY_TIMER_ID, UNMUTE_RETRY_DELAY_MS, nullptr);
    } else {
      // No need to retry, clean up now
      original_mute_states.clear();
      has_unmuted_sessions = false;
    }
  }

  is_hide = !is_hide;
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

  try {
    std::thread th([flag, action]() {
      if (!RegisterHotKey(nullptr, 0, LOWORD(flag), HIWORD(flag))) {
        DebugLog(L"RegisterHotKey failed: %d", GetLastError());
        return;
      }

      MSG msg;
      while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_TIMER && msg.wParam == UNMUTE_RETRY_TIMER_ID) {
          HandleUnmuteRetry();
        } else if (msg.message == WM_HOTKEY) {
          OnHotkey(action);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      UnregisterHotKey(nullptr, 0);
    });

    th.detach();
  } catch (const std::system_error& e) {
    DebugLog(L"Failed to create hotkey thread: %S", e.what());
  }
}

}  // anonymous namespace

// Public interface
void Initialize() {
  const auto& boss_key = Config::Instance().GetBossKey();
  if (!boss_key.empty()) {
    RegisterHotkeyThread(boss_key, HideAndShow);
  }
}

}  // namespace bosskey
