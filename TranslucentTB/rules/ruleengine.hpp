#pragma once
#include "arch.h"
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <wil/resource.h>

#include "color/palette.hpp"
#include "rules/rules.hpp"
#include "../folderwatcher.hpp"
#include "../windows/messagewindow.hpp"

class ConfigManager;
class ProfileManager;
class TaskbarAttributeWorker;

// Evaluates automation rules (rules.json) against live system state and applies
// the winning rule as an in-memory override on the taskbar worker.
//
// It is event driven: foreground changes, power/battery notifications, theme and
// display changes trigger a debounced evaluation. A one-minute timer runs only if a
// rule uses time conditions, and the process list is polled (every 15 s) only if a
// rule uses "app_running". When nothing changes, nothing is applied.
class RuleEngine final : public MessageWindow {
public:
	struct Status {
		std::size_t ruleCount = 0;
		std::size_t enabledCount = 0;
		std::wstring activeRule;
		std::wstring loadError;
		bool paused = false;
		bool adaptiveActive = false;
		Util::Color adaptiveColor;
		std::wstring adaptiveSource;
		std::uint64_t evaluations = 0;
	};

	RuleEngine(HINSTANCE hInstance, ConfigManager &config, ProfileManager &profiles, TaskbarAttributeWorker &worker);
	~RuleEngine();

	// Call when config.json changed so the override is rebuilt from the new base.
	void BaseConfigurationChanged();
	void SetPaused(bool paused);
	void Reload();
	void OpenRulesFile();
	Status GetStatus() const;
	void SetStatusChanged(std::function<void()> callback);
	std::filesystem::path RulesPath() const { return m_RulesPath; }
	const std::vector<rules::Rule> &Rules() const noexcept { return m_Rules; }

	// Validates by round-tripping through the parser, writes rules.json atomically and reloads.
	bool SaveRules(const std::vector<rules::Rule> &list, std::wstring &error);

private:
	static constexpr UINT_PTR DEBOUNCE_TIMER = 1;
	static constexpr UINT_PTR CLOCK_TIMER = 2;
	static constexpr UINT_PTR PROCESS_TIMER = 3;
	static constexpr UINT DEBOUNCE_MS = 300;
	static constexpr UINT PROCESS_POLL_MS = 15000;

	static RuleEngine *s_Instance;
	static void CALLBACK OnWinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
	static void WatcherCallback(void *context, DWORD, std::wstring_view fileName);

	ConfigManager &m_Config;
	ProfileManager &m_Profiles;
	TaskbarAttributeWorker &m_Worker;
	std::filesystem::path m_RulesPath;
	FolderWatcher m_Watcher;

	std::vector<rules::Rule> m_Rules;
	std::wstring m_LoadError;
	std::optional<std::size_t> m_ActiveIndex;
	bool m_HasOverride = false;
	bool m_Paused = false;
	bool m_NeedsClock = false;
	bool m_NeedsProcesses = false;
	std::function<void()> m_StatusChanged;

	// Adaptive color (wallpaper). Recomputed only when the wallpaper or the theme changes.
	std::optional<palette::Result> m_Wallpaper;
	std::wstring m_WallpaperSource;
	bool m_WallpaperLoaded = false;
	bool m_AdaptiveWasEnabled = false;
	std::uint64_t m_Evaluations = 0;
	Util::Color m_AdaptiveTint;
	void LoadWallpaperColors();
	bool AdaptiveActive() const;

	wil::unique_hwineventhook m_ForegroundHook;
	wil::unique_hpowernotify m_AcDcHook, m_BatteryHook, m_SaverHook;

	LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	void ScheduleEvaluation();
	void Evaluate(bool force);
	void ArmClockTimer();
	void Apply(std::optional<std::size_t> index, bool force);
	void NotifyStatus();
	rules::SystemSnapshot Snapshot() const;
};
