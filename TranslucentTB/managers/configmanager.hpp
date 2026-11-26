#pragma once
#include "arch.h"
#include <filesystem>
#include <optional>
#include <string_view>
#include <synchapi.h>
#include <type_traits>
#include <wil/resource.h>

#include "config/config.hpp"
#include "../folderwatcher.hpp"

class ConfigManager {
	// GlassTB stores its data in %LOCALAPPDATA%\GlassTB (see docs/glasstb/persistencia.md).
	static constexpr std::wstring_view CONFIG_FILE = L"config.json";
	static constexpr std::wstring_view LEGACY_CONFIG_FILE = L"settings.json";
	static constexpr std::wstring_view SCHEMA_KEY = L"$schema";
	static constexpr std::size_t BACKUPS_TO_KEEP = 10;

	using callback_t = std::add_pointer_t<void(void *)>;

	static std::filesystem::path DetermineDataFolder();
	static std::filesystem::path DetermineLegacyConfigPath(const std::optional<std::filesystem::path> &storageFolder);
	static std::filesystem::path PrepareConfigPath(const std::optional<std::filesystem::path> &storageFolder);
	static std::optional<std::filesystem::path> FindTranslucentTBConfig();
	static void BackupFile(const std::filesystem::path &source, std::wstring_view prefix);
	static void WatcherCallback(void *context, DWORD, std::wstring_view fileName);
	static void APIENTRY TimerCallback(void *context, DWORD timerLow, DWORD timerHigh);

	std::filesystem::path m_ConfigPath;
	Config m_Config;
	FolderWatcher m_Watcher;

	wil::unique_handle m_ReloadTimer;

	std::wstring m_StartupLanguage;
	bool m_ShownChangeWarning;
	bool m_NewerSchema = false;
	bool m_SessionBackupDone = false;

	callback_t m_Callback;
	void *m_Context;

	bool TryOpenConfigAsJson() noexcept;
	void SaveToFile(FILE *f) const;
	enum class LoadResult { Loaded, Empty, Invalid };
	LoadResult LoadFromFile(FILE *f);
	bool Load(bool firstLoad = false);
	void Reload();
	bool ScheduleReload();

public:
	ConfigManager(const std::optional<std::filesystem::path> &storageFolder, bool &fileExists, callback_t callback, void *context);
	~ConfigManager();

	void UpgradeBlur();
	void UpdateVerbosity();
	void EditConfigFile();
	void DeleteConfigFile();
	bool SaveConfig() const;

	std::filesystem::path GetDataFolder() const
	{
		return m_ConfigPath.parent_path();
	}

	// True when the file was written by a newer GlassTB: it is never overwritten.
	bool IsReadOnlyNewerSchema() const noexcept
	{
		return m_NewerSchema;
	}

	constexpr Config &GetConfig() noexcept
	{
		return m_Config;
	}

	constexpr const Config &GetConfig() const noexcept
	{
		return m_Config;
	}
};
