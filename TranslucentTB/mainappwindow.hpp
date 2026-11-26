#pragma once
#include "arch.h"
#include "tray/traycontextmenu.hpp"
#include <cstddef>
#include <spdlog/common.h>
#include <tuple>
#include <vector>
#include <windef.h>
#include "winrt.hpp"
#include "undefgetcurrenttime.h"
#include <winrt/TranslucentTB.Xaml.Models.Primitives.h>
#include <winrt/TranslucentTB.Xaml.Pages.h>
#include <winrt/Windows.System.h>
#include "redefgetcurrenttime.h"

#include "config/config.hpp"
#include "dynamicloader.hpp"
#include "managers/startupmanager.hpp"
#include "util/thread_independent_mutex.hpp"
#include "uwp/basexamlpagehost.hpp"

class Application;

class MainAppWindow final : public TrayContextMenu<winrt::TranslucentTB::Xaml::Pages::TrayFlyoutPage> {
private:
	using page_t = winrt::TranslucentTB::Xaml::Pages::TrayFlyoutPage;
	using settings_page_t = winrt::TranslucentTB::Xaml::Pages::SettingsPage;
	using settings_edit_t = winrt::TranslucentTB::Xaml::Pages::SettingsEdit;

	Application &m_App;
	bool m_HideIconOverride;

	Util::thread_independent_mutex m_PickerMutex;
	std::array<BaseXamlPageHost*, 7> m_ColorPickers{};

	page_t::TaskbarSettingsChanged_revoker m_TaskbarSettingsChangedRevoker;
	page_t::ColorRequested_revoker m_ColorRequestedRevoker;
	page_t::OpenSettingsRequested_revoker m_OpenSettingsRequestedRevoker;

	Window m_SettingsWindow{ nullptr };
	winrt::weak_ref<settings_page_t> m_SettingsPage;
	winrt::Windows::System::DispatcherQueue m_SettingsDispatcher{ nullptr };
	bool m_SettingsOpening = false;

	page_t::OpenLogFileRequested_revoker m_OpenLogFileRequestedRevoker;
	page_t::LogLevelChanged_revoker m_LogLevelChangedRevoker;
	page_t::DumpDynamicStateRequested_revoker m_DumpDynamicStateRequestedRevoker;
	page_t::EditSettingsRequested_revoker m_EditSettingsRequestedRevoker;
	page_t::ResetSettingsRequested_revoker m_ResetSettingsRequestedRevoker;
	page_t::DisableSavingSettingsChanged_revoker m_DisableSavingSettingsChangedRevoker;
	page_t::ResetDynamicStateRequested_revoker m_ResetDynamicStateRequestedRevoker;
	page_t::CompactThunkHeapRequested_revoker m_CompactThunkHeapRequestedRevoker;

	page_t::StartupStateChanged_revoker m_StartupStateChangedRevoker;
	page_t::TipsAndTricksRequested_revoker m_TipsAndTricksRequestedRevoker;
	page_t::AboutRequested_revoker m_AboutRequestedRevoker;
	page_t::ExitRequested_revoker m_ExitRequestedRevoker;

	std::optional<UINT> m_NewInstanceMessage;

	LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	void RefreshMenu() override;
	void RegisterMenuHandlers();

	void TaskbarSettingsChanged(const txmp::TaskbarState &state, const txmp::TaskbarAppearance &appearance);
	void ColorRequested(const txmp::TaskbarState &state);
	void OpenSettings();
	void ApplySettings(const std::vector<settings_edit_t> &edits);
	void PublishSettings(std::optional<bool> applyResult = {});
	void PublishDiagnostics();

	void OpenLogFileRequested();
	void LogLevelChanged(const txmp::LogLevel &level);
	void DumpDynamicStateRequested();
	void EditSettingsRequested();
	void ResetSettingsRequested();
	void DisableSavingSettingsChanged(bool disabled);
	void ResetDynamicStateRequested();
	static void CompactThunkHeapRequested();

	winrt::fire_and_forget StartupStateChanged();
	static void TipsAndTricksRequested();
	void AboutRequested();
	void Exit();

	TaskbarAppearance &GetConfigForState(const txmp::TaskbarState &state);
	void UpdateTrayVisibility(bool visible);

public:
	MainAppWindow(Application &app, bool hideIconOverride, bool hasPackageIdentity, HINSTANCE hInstance, DynamicLoader &loader);
	~MainAppWindow();

	void ConfigurationChanged();
	void RemoveHideTrayIconOverride();

	static void PostNewInstanceNotification();
};
