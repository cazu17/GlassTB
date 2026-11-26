#include "configmanager.hpp"
#include <cerrno>
#include <cstdio>
#include <rapidjson/encodedstream.h>
#include <rapidjson/error/error.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <share.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <wil/resource.h>
#include "winrt.hpp"
#include <winrt/Windows.ApplicationModel.Resources.Core.h>

#include "constants.hpp"
#include "../../ProgramLog/error/errno.hpp"
#include "../../ProgramLog/error/rapidjson.hpp"
#include "../../ProgramLog/error/std.hpp"
#include "../../ProgramLog/error/win32.hpp"
#include "../../ProgramLog/error/winrt.hpp"
#include "../../ProgramLog/log.hpp"
#include "config/backups.hpp"
#include "config/rapidjsonhelper.hpp"
#include "win32.hpp"
#include "../localization.hpp"
#include "../resources/ids.h"

std::filesystem::path ConfigManager::DetermineDataFolder()
{
	wil::unique_cotaskmem_string localAppData;
	const HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, localAppData.put());
	HresultVerify(hr, spdlog::level::critical, L"Failed to determine the local application data folder!");

	std::filesystem::path folder = localAppData.get();
	folder /= L"GlassTB";
	return folder;
}

std::filesystem::path ConfigManager::DetermineLegacyConfigPath(const std::optional<std::filesystem::path> &storageFolder)
{
	// Location used by TranslucentTB and by GlassTB before schema version 1.
	std::filesystem::path path;
	if (storageFolder)
	{
		path = *storageFolder / L"RoamingState";
	}
	else
	{
		const auto [loc, hr] = win32::GetExeLocation();
		HresultVerify(hr, spdlog::level::critical, L"Failed to determine executable location!");
		path = loc.parent_path();
	}

	path /= LEGACY_CONFIG_FILE;
	return path;
}

std::optional<std::filesystem::path> ConfigManager::FindTranslucentTBConfig()
{
	// TranslucentTB from the Microsoft Store keeps its settings in its package RoamingState.
	// We do not hardcode the package family name: any installed package folder whose
	// name contains "TranslucentTB" and has a RoamingState\settings.json qualifies.
	wil::unique_cotaskmem_string localAppData;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, localAppData.put())))
	{
		return std::nullopt;
	}

	std::error_code errc;
	const std::filesystem::path packages = std::filesystem::path(localAppData.get()) / L"Packages";
	for (std::filesystem::directory_iterator it(packages, std::filesystem::directory_options::skip_permission_denied, errc), end; !errc && it != end; it.increment(errc))
	{
		const std::wstring name = it->path().filename().native();
		if (name.find(L"TranslucentTB") == std::wstring::npos)
		{
			continue;
		}

		auto candidate = it->path() / L"RoamingState" / LEGACY_CONFIG_FILE;
		if (std::filesystem::is_regular_file(candidate, errc))
		{
			return candidate;
		}
	}

	return std::nullopt;
}

void ConfigManager::BackupFile(const std::filesystem::path &source, std::wstring_view prefix)
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	const backups::Timestamp ts { now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond };

	std::error_code errc;
	const auto folder = DetermineDataFolder() / L"backups";
	std::filesystem::create_directories(folder, errc);
	if (errc)
	{
		StdErrorCodeHandle(errc, spdlog::level::warn, L"Failed to create backups folder");
		return;
	}

	std::filesystem::copy_file(source, folder / backups::TimestampedName(prefix, ts), std::filesystem::copy_options::overwrite_existing, errc);
	if (errc)
	{
		StdErrorCodeHandle(errc, spdlog::level::warn, L"Failed to back up configuration file");
		return;
	}

	std::vector<std::wstring> names;
	for (std::filesystem::directory_iterator it(folder, errc), end; !errc && it != end; it.increment(errc))
	{
		names.push_back(it->path().filename().native());
	}

	for (const auto &name : backups::SelectForDeletion(names, prefix, BACKUPS_TO_KEEP))
	{
		std::filesystem::remove(folder / name, errc);
	}
}

std::filesystem::path ConfigManager::PrepareConfigPath(const std::optional<std::filesystem::path> &storageFolder)
{
	// Runs before the folder watcher exists, so copying files here cannot trigger a reload.
	const auto folder = DetermineDataFolder();
	std::error_code errc;
	std::filesystem::create_directories(folder, errc);
	StdErrorCodeVerify(errc, spdlog::level::critical, L"Failed to create the GlassTB data folder!");

	const auto path = folder / CONFIG_FILE;
	if (std::filesystem::exists(path, errc))
	{
		return path;
	}

	std::optional<std::filesystem::path> source;
	std::wstring_view backupPrefix;
	if (const auto legacy = DetermineLegacyConfigPath(storageFolder); std::filesystem::is_regular_file(legacy, errc))
	{
		source = legacy;
		backupPrefix = L"legacy-settings";
	}
	else if (const auto ttb = FindTranslucentTBConfig())
	{
		source = ttb;
		backupPrefix = L"translucenttb-import";
	}

	if (source)
	{
		BackupFile(*source, backupPrefix);
		std::filesystem::copy_file(*source, path, errc);
		if (errc)
		{
			StdErrorCodeHandle(errc, spdlog::level::warn, L"Failed to migrate the previous configuration file");
		}
		else
		{
			MessagePrint(spdlog::level::info, std::format(L"Migrated configuration from {}", source->native()));
		}
	}

	return path;
}

void ConfigManager::WatcherCallback(void *context, DWORD, std::wstring_view fileName)
{
	if (fileName.empty() || win32::IsSameFilename(fileName, CONFIG_FILE))
	{
		const auto that = static_cast<ConfigManager *>(context);

		if (!that->ScheduleReload())
		{
			that->Reload();
		}
	}
}

void ConfigManager::TimerCallback(void *context, DWORD, DWORD)
{
	static_cast<ConfigManager *>(context)->Reload();
}

bool ConfigManager::TryOpenConfigAsJson() noexcept
{
	wil::unique_hkey key;
	const HRESULT hr = AssocQueryKey(ASSOCF_VERIFY | ASSOCF_INIT_IGNOREUNKNOWN, ASSOCKEY_SHELLEXECCLASS, L".json", L"open", key.put());
	if (SUCCEEDED(hr))
	{
		SHELLEXECUTEINFO info = {
			.cbSize = sizeof(info),
			.fMask = SEE_MASK_CLASSKEY | SEE_MASK_FLAG_NO_UI,
			.lpVerb = L"open",
			.lpFile = m_ConfigPath.c_str(),
			.nShow = SW_SHOW,
			.hkeyClass = key.get()
		};

		const bool success = ShellExecuteEx(&info);
		if (!success)
		{
			LastErrorHandle(spdlog::level::warn, L"Failed to launch JSON file editor");
		}

		return success;
	}
	else
	{
		if (hr != HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION))
		{
			HresultHandle(hr, spdlog::level::warn, L"Failed to query for .json file association");
		}

		return false;
	}
}

void ConfigManager::SaveToFile(FILE *f) const
{
	char buffer[1024];
	rj::FileWriteStream filestream(f, buffer, std::size(buffer));

	using OutputStream = rj::EncodedOutputStream<rj::UTF8<>, rj::FileWriteStream>;
	OutputStream out(filestream, true);

	rj::PrettyWriter<OutputStream, rj::UTF16LE<>> writer(out);
	writer.SetIndent(' ', 2);

	writer.StartObject();
	m_Config.Serialize(writer);
	writer.EndObject();

	writer.Flush();
}

ConfigManager::LoadResult ConfigManager::LoadFromFile(FILE *f)
{
	static constexpr std::wstring_view DESERIALIZE_FAILED = L"Failed to deserialize JSON document";

	char buffer[1024];
	rj::FileReadStream filestream(f, buffer, std::size(buffer));

	rj::AutoUTFInputStream<uint32_t, rj::FileReadStream> in(filestream);

	rj::GenericDocument<rj::UTF16LE<>> doc;
	if (const rj::ParseResult result = doc.ParseStream<rj::kParseCommentsFlag | rj::kParseTrailingCommasFlag, rj::AutoUTF<uint32_t>>(in))
	{
		// remove the schema key to avoid a false unknown key warning
		doc.RemoveMember(rjh::StringViewToValue(SCHEMA_KEY));

		try
		{
			// load the defaults before deserializing to not reuse previous settings
			// in case some keys are missing from the file
			m_Config = { };
			m_Config.Deserialize(doc, [](std::wstring_view unknownKey)
			{
				if (Error::ShouldLog<spdlog::level::info>())
				{
					MessagePrint(spdlog::level::info, std::format(L"Unknown key found in JSON: {}", unknownKey));
				}
			});

			// everything went fine, we can return!
			return LoadResult::Loaded;
		}
		HelperDeserializationErrorCatch(spdlog::level::err, DESERIALIZE_FAILED)
		StdSystemErrorCatch(spdlog::level::err, DESERIALIZE_FAILED);
	}
	else if (result.Code() == rj::kParseErrorDocumentEmpty)
	{
		m_Config = { };
		return LoadResult::Empty;
	}
	else
	{
		ParseErrorCodeHandle(result.Code(), spdlog::level::err, DESERIALIZE_FAILED);
	}

	// parsing failed, use defaults
	m_Config = { };
	return LoadResult::Invalid;
}

bool ConfigManager::Load(bool firstLoad)
{
	if (wil::unique_file file { _wfsopen(m_ConfigPath.c_str(), L"rbS", _SH_DENYNO) })
	{
		const LoadResult result = LoadFromFile(file.get());
		file.reset();

		if (result == LoadResult::Invalid)
		{
			// Keep a copy of the broken file so the user's edits are not lost when
			// GlassTB later saves the defaults over it.
			BackupFile(m_ConfigPath, L"config-invalid");
		}
		else if (result == LoadResult::Loaded)
		{
			m_NewerSchema = m_Config.LoadedSchemaVersion > Config::SCHEMA_VERSION;
			if (m_NewerSchema)
			{
				MessagePrint(spdlog::level::warn, std::format(L"Configuration schema version {} is newer than {}; the file will not be overwritten.", m_Config.LoadedSchemaVersion, Config::SCHEMA_VERSION));
			}
			else if (m_Config.LoadedSchemaVersion < Config::SCHEMA_VERSION)
			{
				// Version 0 (TranslucentTB format) has the same keys; the next save writes version 1.
				BackupFile(m_ConfigPath, std::format(L"config-v{}", m_Config.LoadedSchemaVersion));
			}
			else if (!std::exchange(m_SessionBackupDone, true))
			{
				BackupFile(m_ConfigPath, L"config");
			}
		}

		if (result == LoadResult::Loaded)
		{
			if (firstLoad)
			{
				if (!m_Config.Language.empty() && Localization::SetProcessLangOverride(m_Config.Language))
				{
					m_StartupLanguage = m_Config.Language;
				}
			}
			else if (m_StartupLanguage != m_Config.Language && !std::exchange(m_ShownChangeWarning, true))
			{
				// try using the new locale for that dialog box
				LCID newLang = LocaleNameToLCID(m_Config.Language.c_str(), LOCALE_ALLOW_NEUTRAL_NAMES);

				Localization::ShowLocalizedMessageBox(IDS_LANGUAGE_CHANGED, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND, wil::GetModuleInstanceHandle(), newLang ? LANGIDFROMLCID(newLang) : MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)).detach();
			}
		}

		// note: this demarks if the file exists, even if parsing failed.
		return true;
	}
	else
	{
		const errno_t err = errno;
		const bool fileExists = err != ENOENT;
		if (fileExists)
		{
			// if the file failed to open, but it exists, something went wrong
			ErrnoTHandle(err, spdlog::level::err, L"Failed to open configuration file");
		}

		// opening file failed, use defaults
		m_Config = { };
		return fileExists;
	}
}

void ConfigManager::Reload()
{
	Load();
	UpdateVerbosity();
	m_Callback(m_Context);
}

bool ConfigManager::ScheduleReload()
{
	if (m_ReloadTimer)
	{
		LARGE_INTEGER waitTime{};
		// 200 ms, relative to current time. arbitrarily chosen to get a balance
		// between feeling snappy and not triggering errors due to the various weird
		// ways editors save files.
		waitTime.QuadPart = -2000000;
		if (SetWaitableTimer(m_ReloadTimer.get(), &waitTime, 0, TimerCallback, this, false))
		{
			return true;
		}
		else
		{
			LastErrorHandle(spdlog::level::warn, L"Failed to set waitable timer");
		}
	}

	return false;
}

ConfigManager::ConfigManager(const std::optional<std::filesystem::path> &storageFolder, bool &fileExists, callback_t callback, void *context) :
	m_ConfigPath(PrepareConfigPath(storageFolder)),
	m_Watcher(m_ConfigPath.parent_path(), false, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, WatcherCallback, this),
	m_ReloadTimer(CreateWaitableTimer(nullptr, true, nullptr)),
	m_ShownChangeWarning(false),
	m_Callback(callback),
	m_Context(context)
{
	if (!m_ReloadTimer)
	{
		LastErrorHandle(spdlog::level::warn, L"Failed to create waitable timer");
	}

	fileExists = Load(true);
	UpdateVerbosity();
}

ConfigManager::~ConfigManager()
{
	if (m_ReloadTimer)
	{
		if (!CancelWaitableTimer(m_ReloadTimer.get()))
		{
			LastErrorHandle(spdlog::level::info, L"Failed to cancel reload timer");
		}
	}
}

void ConfigManager::UpgradeBlur()
{
	std::initializer_list<TaskbarAppearance*> appearances = {
		&m_Config.DesktopAppearance,
		&m_Config.VisibleWindowAppearance,
		&m_Config.MaximisedWindowAppearance,
		&m_Config.StartOpenedAppearance,
		&m_Config.SearchOpenedAppearance,
		&m_Config.TaskViewOpenedAppearance,
		&m_Config.BatterySaverAppearance
	};

	for (TaskbarAppearance* appearance : appearances)
	{
		if (appearance->Accent == ACCENT_ENABLE_BLURBEHIND)
		{
			appearance->Accent = ACCENT_ENABLE_ACRYLICBLURBEHIND;
		}
	}
}

void ConfigManager::UpdateVerbosity()
{
	if (const auto sink = Log::GetSink())
	{
		sink->set_level(m_Config.LogVerbosity);
	}
}

void ConfigManager::EditConfigFile()
{
	SaveConfig();

	if (!TryOpenConfigAsJson())
	{
		HresultVerify(win32::EditFile(m_ConfigPath), spdlog::level::err, L"Failed to open configuration file.");
	}
}

void ConfigManager::DeleteConfigFile()
{
	std::error_code errc;
	std::filesystem::remove(m_ConfigPath, errc);
	StdErrorCodeVerify(errc, spdlog::level::warn, L"Failed to delete config file");
}

bool ConfigManager::SaveConfig() const
{
	if (m_Config.DisableSaving || m_NewerSchema)
	{
		return false;
	}

	std::filesystem::path tempFile = m_ConfigPath;
	tempFile.replace_extension(L".tmp");

	wil::unique_file file;
	const errno_t err = _wfopen_s(file.put(), tempFile.c_str(), L"wbS");
	if (err != 0)
	{
		ErrnoTHandle(err, spdlog::level::err, L"Failed to save configuration!");
		return false;
	}

	SaveToFile(file.get());
	const bool writeFailed = ferror(file.get()) != 0;
	if (fclose(file.release()) != 0 || writeFailed)
	{
		MessagePrint(spdlog::level::err, L"Failed to write temporary configuration file");
		return false;
	}

	if (ReplaceFile(m_ConfigPath.c_str(), tempFile.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH | REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, nullptr, nullptr))
	{
		return true;
	}

	// A new installation may not have created the destination yet.
	if (const auto lastErr = GetLastError(); lastErr != ERROR_FILE_NOT_FOUND)
	{
		HresultHandle(HRESULT_FROM_WIN32(lastErr), spdlog::level::err, L"Failed to replace configuration file");
	}
	else if (MoveFileEx(tempFile.c_str(), m_ConfigPath.c_str(), MOVEFILE_WRITE_THROUGH))
	{
		return true;
	}
	else
	{
		LastErrorHandle(spdlog::level::err, L"Failed to move temporary configuration file");
	}
	return false;
}
