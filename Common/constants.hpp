#pragma once
#include "arch.h"
#include <cstdint>
#include <guiddef.h>

#include "util/null_terminated_string_view.hpp"

#pragma region App

// Mutex name for app uniqueness
static constexpr Util::null_terminated_wstring_view MUTEX_GUID = L"04278CBF-81E7-4BB5-8FBA-4E6C4B39DC7F";

// Event used to signal when the TAP is ready
static constexpr Util::null_terminated_wstring_view TAP_READY_EVENT = L"GTBTAP_Ready";

// Current version of the API used for IPC with the TAP
static constexpr std::uint32_t TAP_API_VERSION = 4;

// Tray icon GUID
static constexpr GUID TRAY_GUID = { 0xb4d437b4, 0x7dbe, 0x47d4, { 0x8c, 0xd0, 0xf3, 0xa9, 0xcc, 0x10, 0xa9, 0x80 } };

// GUID of payload injected in Explorer
static constexpr GUID EXPLORER_PAYLOAD = { 0xfe663e7a, 0xd525, 0x4539, { 0xad, 0x94, 0xa4, 0xf9, 0xb4, 0x50, 0x89, 0xb7 } };

#pragma endregion

#pragma region Messages

// Message sent by explorer when the taskbar is created
static constexpr Util::null_terminated_wstring_view WM_TASKBARCREATED = L"TaskbarCreated";

// Sent by the hook to the worker when the taskbar is trying to change its composition attribute
static constexpr Util::null_terminated_wstring_view WM_GTBHOOKREQUESTREFRESH = L"GTBHook_RequestAttributeRefresh";

// Sent by the hook to the worker when Task View closes/opens
static constexpr Util::null_terminated_wstring_view WM_GTBHOOKTASKVIEWVISIBILITYCHANGE = L"GTBHook_TaskViewVisibilityChange";

// Sent by the worker to the hook to get the current Task View status
static constexpr Util::null_terminated_wstring_view WM_GTBHOOKISTASKVIEWOPENED = L"GTBHook_IsTaskViewOpened";

// Sent by LauncherVisibilitySink when the start menu opens/closes
static constexpr Util::null_terminated_wstring_view WM_GTBSTARTVISIBILITYCHANGE = L"GTB_StartVisibilityChange";

// Sent by TaskbarAttributeWorker to itself to switch back to main thread
static constexpr Util::null_terminated_wstring_view WM_GTBSEARCHVISIBILITYCHANGE = L"GTB_SearchVisibilityChange";

// Sent by TaskbarAttributeWorker to itself to switch back to main thread
static constexpr Util::null_terminated_wstring_view WM_GTBFINDINSTARTVISIBILITYCHANGE = L"GTB_FindInStartVisibilityChange";

// Sent to the worker to force the taskbar to toggle to normal and back to the expected appearance
static constexpr Util::null_terminated_wstring_view WM_GTBFORCEREFRESHTASKBAR = L"GTB_ForceRefreshTaskbar";

// Sent by another instance of GlassTB to signal that it was started while this instance is running.
static constexpr Util::null_terminated_wstring_view WM_GTBNEWINSTANCESTARTED = L"GTB_NewInstanceStarted";

// Sent to the worker to apply taskbar color preview.
static constexpr Util::null_terminated_wstring_view WM_GTBAPPLYCOLORPREVIEW = L"GTB_ApplyColorPreview";

// Hotkey IDs
static constexpr int RESET_STATE_GLOBAL_HOTKEY_ID = 2;

#pragma endregion

#pragma region Window classes

// Window class used by our tray icon
static constexpr Util::null_terminated_wstring_view TRAY_WINDOW = L"GTB_TrayWindow";

// Window class and title used by our attribute worker
static constexpr Util::null_terminated_wstring_view GTB_WORKERWINDOW = L"GTB_WorkerWindow";

// Window class and title used by the Task View monitor
static constexpr Util::null_terminated_wstring_view GTBHOOK_TASKVIEWMONITOR = L"GTBHook_TaskViewMonitor";

// Window class for taskbar on primary monitor
static constexpr Util::null_terminated_wstring_view TASKBAR = L"Shell_TrayWnd";

// Window class for taskbars on other monitors
static constexpr Util::null_terminated_wstring_view SECONDARY_TASKBAR = L"Shell_SecondaryTrayWnd";

// Window class used by UWP
static constexpr Util::null_terminated_wstring_view CORE_WINDOW = L"Windows.UI.Core.CoreWindow";

#pragma endregion

#pragma region Other

// UTF-8 Byte Order Mark
static constexpr Util::null_terminated_string_view UTF8_BOM = "\xEF\xBB\xBF";

// Serialization Keys
static constexpr std::wstring_view CLASS_KEY = L"window_class";
static constexpr std::wstring_view TITLE_KEY = L"window_title";
static constexpr std::wstring_view FILE_KEY = L"process_name";

#pragma endregion
