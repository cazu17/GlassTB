#include "ruleengine.hpp"
#include <algorithm>
#include <cwctype>
#include <iterator>
#include <wil/result.h>
#include <format>
#include <fstream>
#include <ShellAPI.h>
#include <ShlObj.h>
#include <tlhelp32.h>
#include <wil/com.h>
#include <wincodec.h>

#include "../managers/configmanager.hpp"
#include "../managers/profilemanager.hpp"
#include "../taskbar/taskbarattributeworker.hpp"
#include "../../ProgramLog/error/win32.hpp"
#include "../../ProgramLog/log.hpp"
#include "win32.hpp"
#include "wilx.hpp"

RuleEngine *RuleEngine::s_Instance = nullptr;

namespace {
	std::wstring LowerFileName(const std::filesystem::path &path)
	{
		std::wstring name = path.filename().native();
		for (auto &c : name)
		{
			c = static_cast<wchar_t>(towlower(c));
		}
		return name;
	}

	bool IsDarkTheme()
	{
		DWORD value = 1;
		DWORD size = sizeof(value);
		if (RegGetValue(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", L"AppsUseLightTheme",
			RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
		{
			return value == 0;
		}
		return false;
	}

	std::optional<std::wstring> ReadUtf8(const std::filesystem::path &path, std::wstring &error)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			return std::nullopt;
		}
		std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (bytes.size() > 1024 * 1024)
		{
			error = L"rules.json is larger than 1 MiB";
			return std::wstring { };
		}
		std::string_view view = bytes;
		if (view.starts_with("\xEF\xBB\xBF"))
		{
			view.remove_prefix(3);
		}
		if (view.empty())
		{
			return std::wstring { };
		}
		const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, view.data(), static_cast<int>(view.size()), nullptr, 0);
		if (length <= 0)
		{
			error = L"rules.json is not valid UTF-8";
			return std::wstring { };
		}
		std::wstring text(static_cast<std::size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, view.data(), static_cast<int>(view.size()), text.data(), length);
		return text;
	}
}

RuleEngine::RuleEngine(HINSTANCE hInstance, ConfigManager &config, ProfileManager &profiles, TaskbarAttributeWorker &worker) :
	MessageWindow(L"GTB_RuleEngine", L"GTB_RuleEngine", hInstance, WS_POPUP, WS_EX_NOREDIRECTIONBITMAP),
	m_Config(config),
	m_Profiles(profiles),
	m_Worker(worker),
	m_RulesPath(config.GetDataFolder() / L"rules.json"),
	m_Watcher(config.GetDataFolder(), false, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, WatcherCallback, this)
{
	s_Instance = this;

	m_ForegroundHook.reset(SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, OnWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
	if (!m_ForegroundHook)
	{
		MessagePrint(spdlog::level::warn, L"Failed to hook foreground changes; foreground rules will only update on other events");
	}

	m_AcDcHook.reset(RegisterPowerSettingNotification(m_WindowHandle, &GUID_ACDC_POWER_SOURCE, DEVICE_NOTIFY_WINDOW_HANDLE));
	m_BatteryHook.reset(RegisterPowerSettingNotification(m_WindowHandle, &GUID_BATTERY_PERCENTAGE_REMAINING, DEVICE_NOTIFY_WINDOW_HANDLE));
	m_SaverHook.reset(RegisterPowerSettingNotification(m_WindowHandle, &GUID_POWER_SAVING_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE));

	Reload();
}

RuleEngine::~RuleEngine()
{
	KillTimer(m_WindowHandle, DEBOUNCE_TIMER);
	KillTimer(m_WindowHandle, CLOCK_TIMER);
	KillTimer(m_WindowHandle, PROCESS_TIMER);
	s_Instance = nullptr;
}

void CALLBACK RuleEngine::OnWinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD)
{
	if (s_Instance && !s_Instance->m_Rules.empty())
	{
		s_Instance->ScheduleEvaluation();
	}
}

void RuleEngine::WatcherCallback(void *context, DWORD, std::wstring_view fileName)
{
	if (fileName.empty() || win32::IsSameFilename(fileName, L"rules.json"))
	{
		static_cast<RuleEngine *>(context)->Reload();
	}
}

LRESULT RuleEngine::MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_TIMER:
		if (wParam == DEBOUNCE_TIMER)
		{
			KillTimer(m_WindowHandle, DEBOUNCE_TIMER);
			Evaluate(false);
		}
		else if (wParam == CLOCK_TIMER)
		{
			Evaluate(false);
			ArmClockTimer();
		}
		else if (wParam == PROCESS_TIMER)
		{
			Evaluate(false);
		}
		return 0;

	case WM_POWERBROADCAST:
		if (wParam == PBT_POWERSETTINGCHANGE || wParam == PBT_APMPOWERSTATUSCHANGE || wParam == PBT_APMRESUMEAUTOMATIC)
		{
			ScheduleEvaluation();
		}
		return TRUE;

	case WM_SETTINGCHANGE:
		if (wParam == SPI_SETDESKWALLPAPER)
		{
			m_WallpaperLoaded = false;
			if (AdaptiveActive() || m_Config.GetConfig().AdaptiveColor.Enabled)
			{
				Apply(m_ActiveIndex, true);
			}
		}
		else if (lParam && std::wstring_view(reinterpret_cast<const wchar_t *>(lParam)) == L"ImmersiveColorSet")
		{
			ScheduleEvaluation();
			if (m_Config.GetConfig().AdaptiveColor.Enabled)
			{
				Apply(m_ActiveIndex, true);
			}
		}
		break;

	case WM_DISPLAYCHANGE:
		ScheduleEvaluation();
		break;
	}

	return MessageWindow::MessageHandler(uMsg, wParam, lParam);
}

void RuleEngine::ScheduleEvaluation()
{
	// Debounce: bursts of events (e.g. alt-tab) collapse into a single evaluation.
	SetTimer(m_WindowHandle, DEBOUNCE_TIMER, DEBOUNCE_MS, nullptr);
}

void RuleEngine::ArmClockTimer()
{
	KillTimer(m_WindowHandle, CLOCK_TIMER);
	if (m_NeedsClock && !m_Paused)
	{
		SYSTEMTIME now;
		GetLocalTime(&now);
		const UINT untilNextMinute = (60 - now.wSecond) * 1000 - now.wMilliseconds + 50;
		SetTimer(m_WindowHandle, CLOCK_TIMER, untilNextMinute, nullptr);
	}
}

void RuleEngine::Reload()
{
	std::wstring error;
	std::vector<rules::Rule> loaded;
	if (const auto text = ReadUtf8(m_RulesPath, error))
	{
		if (error.empty() && !text->empty())
		{
			if (auto parsed = rules::FromJson(*text, error))
			{
				loaded = std::move(*parsed);
			}
		}
	}

	if (!error.empty())
	{
		// Keep the previous valid rules so a typo while editing does not drop the user's automation.
		MessagePrint(spdlog::level::warn, std::format(L"rules.json was not loaded: {}", error));
		m_LoadError = std::move(error);
	}
	else
	{
		m_LoadError.clear();
		m_Rules = std::move(loaded);
	}

	m_NeedsClock = std::ranges::any_of(m_Rules, [](const rules::Rule &r) { return r.enabled && r.condition.NeedsClock(); });
	m_NeedsProcesses = std::ranges::any_of(m_Rules, [](const rules::Rule &r) { return r.enabled && r.condition.NeedsRunningApps(); });

	ArmClockTimer();
	KillTimer(m_WindowHandle, PROCESS_TIMER);
	if (m_NeedsProcesses && !m_Paused)
	{
		SetTimer(m_WindowHandle, PROCESS_TIMER, PROCESS_POLL_MS, nullptr);
	}

	Evaluate(true);
	NotifyStatus();
}

void RuleEngine::OpenRulesFile()
{
	std::error_code errc;
	if (!std::filesystem::exists(m_RulesPath, errc))
	{
		const std::wstring_view text = rules::TEMPLATE;
		const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		std::string bytes(static_cast<std::size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), bytes.data(), length, nullptr, nullptr);
		std::ofstream(m_RulesPath, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}

	HresultVerify(win32::EditFile(m_RulesPath), spdlog::level::err, L"Failed to open rules file.");
}

void RuleEngine::BaseConfigurationChanged()
{
	const bool adaptive = m_Config.GetConfig().AdaptiveColor.Enabled;
	if (m_HasOverride || adaptive || adaptive != m_AdaptiveWasEnabled)
	{
		Apply(m_ActiveIndex, true);
	}
}

bool RuleEngine::AdaptiveActive() const
{
	return m_Config.GetConfig().AdaptiveColor.Enabled && !m_Paused;
}

void RuleEngine::LoadWallpaperColors()
{
	m_WallpaperLoaded = true;
	m_Wallpaper.reset();
	m_WallpaperSource.clear();

	wchar_t path[MAX_PATH] = { };
	if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0))
	{
		LastErrorHandle(spdlog::level::warn, L"Failed to read the wallpaper path");
		return;
	}

	if (path[0] == L'\0')
	{
		// Solid color desktop: use the background color itself.
		const COLORREF color = GetSysColor(COLOR_BACKGROUND);
		const Util::Color solid(GetRValue(color), GetGValue(color), GetBValue(color));
		m_Wallpaper = palette::Result { solid, solid };
		m_WallpaperSource = L"solid color";
		return;
	}

	const std::filesystem::path file = path;
	const auto excluded = m_Config.GetConfig().AdaptiveColor.Excluded;
	const std::wstring name = LowerFileName(file);
	if (std::ranges::find(excluded, name) != excluded.end())
	{
		m_WallpaperSource = name + L" (excluded)";
		return;
	}

	try
	{
		// Decode once and downscale to 64x64: bounded cost, no screen capture.
		const auto factory = wil::CoCreateInstance<IWICImagingFactory>(CLSID_WICImagingFactory);
		wil::com_ptr<IWICBitmapDecoder> decoder;
		THROW_IF_FAILED(factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.put()));
		wil::com_ptr<IWICBitmapFrameDecode> frame;
		THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));
		wil::com_ptr<IWICBitmapScaler> scaler;
		THROW_IF_FAILED(factory->CreateBitmapScaler(scaler.put()));
		THROW_IF_FAILED(scaler->Initialize(frame.get(), 64, 64, WICBitmapInterpolationModeFant));
		wil::com_ptr<IWICFormatConverter> converter;
		THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));
		THROW_IF_FAILED(converter->Initialize(scaler.get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom));

		std::vector<std::uint8_t> bgra(64 * 64 * 4);
		THROW_IF_FAILED(converter->CopyPixels(nullptr, 64 * 4, static_cast<UINT>(bgra.size()), bgra.data()));

		std::vector<Util::Color> pixels;
		pixels.reserve(64 * 64);
		for (std::size_t i = 0; i + 3 < bgra.size(); i += 4)
		{
			pixels.emplace_back(bgra[i + 2], bgra[i + 1], bgra[i], bgra[i + 3]);
		}

		m_Wallpaper = palette::Extract(pixels);
		m_WallpaperSource = name;
	}
	catch (const wil::ResultException &ex)
	{
		HresultHandle(ex.GetErrorCode(), spdlog::level::warn, L"Failed to read wallpaper colors");
	}
}

void RuleEngine::SetPaused(bool paused)
{
	if (m_Paused != paused)
	{
		m_Paused = paused;
		Reload();
	}
}

rules::SystemSnapshot RuleEngine::Snapshot() const
{
	rules::SystemSnapshot s;

	if (const Window foreground = Window::ForegroundWindow())
	{
		if (const auto file = foreground.file())
		{
			s.foregroundExe = LowerFileName(*file);
		}
		s.foregroundMaximized = foreground.maximised();
	}

	QUERY_USER_NOTIFICATION_STATE state;
	if (SUCCEEDED(SHQueryUserNotificationState(&state)))
	{
		s.fullscreen = state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_BUSY || state == QUNS_PRESENTATION_MODE;
	}

	SYSTEMTIME now;
	GetLocalTime(&now);
	s.minutesOfDay = now.wHour * 60 + now.wMinute;
	s.weekday = now.wDayOfWeek;

	SYSTEM_POWER_STATUS power;
	if (GetSystemPowerStatus(&power))
	{
		s.onBattery = power.ACLineStatus == 0;
		s.batterySaver = power.SystemStatusFlag == 1;
		s.batteryPercent = (power.BatteryFlag & 128) || power.BatteryLifePercent > 100 ? -1 : power.BatteryLifePercent;
	}

	s.darkTheme = IsDarkTheme();
	s.monitorCount = GetSystemMetrics(SM_CMONITORS);

	if (m_NeedsProcesses)
	{
		if (const wil::unique_tool_help_snapshot snapshot { CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) })
		{
			PROCESSENTRY32W entry = { .dwSize = sizeof(entry) };
			for (BOOL ok = Process32FirstW(snapshot.get(), &entry); ok; ok = Process32NextW(snapshot.get(), &entry))
			{
				s.runningExes.push_back(LowerFileName(entry.szExeFile));
			}
		}
	}

	return s;
}

void RuleEngine::Evaluate(bool force)
{
	const auto index = m_Paused || m_Rules.empty() ? std::nullopt : rules::Resolve(m_Rules, Snapshot());
	// Deduplication: only touch the taskbar when the winning rule changes.
	if (force || index != m_ActiveIndex)
	{
		Apply(index, force);
	}
}

void RuleEngine::Apply(std::optional<std::size_t> index, bool force)
{
	const bool changed = index != m_ActiveIndex;
	m_ActiveIndex = index;

	// Layer 1: adaptive color replaces the tint color of every state (materials are kept).
	const auto &base = m_Config.GetConfig();
	const bool adaptive = AdaptiveActive();
	m_AdaptiveWasEnabled = base.AdaptiveColor.Enabled;
	std::optional<Config> adapted;
	if (adaptive)
	{
		if (!m_WallpaperLoaded)
		{
			LoadWallpaperColors();
		}
		if (m_Wallpaper)
		{
			const auto &settings = base.AdaptiveColor;
			const palette::Tuning tuning { settings.Saturation / 100.0f, settings.Contrast / 100.0f, settings.Intensity };
			const bool dark = IsDarkTheme();
			const auto tint = palette::ForTheme(settings.UseSecondary ? m_Wallpaper->secondary : m_Wallpaper->dominant, dark, tuning);
			adapted = base;
			for (TaskbarAppearance *a : { static_cast<TaskbarAppearance *>(&adapted->DesktopAppearance), static_cast<TaskbarAppearance *>(&adapted->VisibleWindowAppearance),
				static_cast<TaskbarAppearance *>(&adapted->MaximisedWindowAppearance), static_cast<TaskbarAppearance *>(&adapted->StartOpenedAppearance),
				static_cast<TaskbarAppearance *>(&adapted->SearchOpenedAppearance), static_cast<TaskbarAppearance *>(&adapted->TaskViewOpenedAppearance),
				static_cast<TaskbarAppearance *>(&adapted->BatterySaverAppearance) })
			{
				a->Color = tint;
			}
			m_AdaptiveTint = tint;
		}
	}

	if (index)
	{
		const auto &rule = m_Rules[*index];
		// Layer 2: the winning rule is applied on top of the (possibly adapted) configuration.
		auto effective = rules::BuildEffectiveConfig(adapted ? *adapted : base, rule, [this](std::wstring_view name) -> std::optional<Profile>
		{
			if (auto entry = m_Profiles.Find(name))
			{
				return std::move(entry->profile);
			}
			MessagePrint(spdlog::level::warn, std::format(L"Rule action refers to unknown profile {}", name));
			return std::nullopt;
		});
		m_Worker.SetRuleOverride(std::move(effective));
		m_HasOverride = true;
		if (changed)
		{
			MessagePrint(spdlog::level::info, std::format(L"Automation rule active: {}", rule.name));
		}
	}
	else if (adapted)
	{
		m_Worker.SetRuleOverride(std::move(adapted));
		m_HasOverride = true;
	}
	else if (m_HasOverride || force)
	{
		if (m_HasOverride)
		{
			m_Worker.SetRuleOverride(std::nullopt);
			MessagePrint(spdlog::level::info, L"No automation rule or adaptive color active");
		}
		m_HasOverride = false;
	}

	if (changed || force)
	{
		NotifyStatus();
	}
}

RuleEngine::Status RuleEngine::GetStatus() const
{
	Status status;
	status.ruleCount = m_Rules.size();
	status.enabledCount = static_cast<std::size_t>(std::ranges::count_if(m_Rules, &rules::Rule::enabled));
	status.loadError = m_LoadError;
	status.paused = m_Paused;
	status.adaptiveActive = AdaptiveActive() && m_Wallpaper.has_value();
	status.adaptiveColor = m_AdaptiveTint;
	status.adaptiveSource = m_WallpaperSource;
	if (m_ActiveIndex && *m_ActiveIndex < m_Rules.size())
	{
		status.activeRule = m_Rules[*m_ActiveIndex].name;
	}
	return status;
}

void RuleEngine::SetStatusChanged(std::function<void()> callback)
{
	m_StatusChanged = std::move(callback);
}

void RuleEngine::NotifyStatus()
{
	if (m_StatusChanged)
	{
		m_StatusChanged();
	}
}

bool RuleEngine::SaveRules(const std::vector<rules::Rule> &list, std::wstring &error)
{
	const auto json = rules::ToJson(list);
	if (!rules::FromJson(json, error))
	{
		return false;
	}

	const int length = WideCharToMultiByte(CP_UTF8, 0, json.data(), static_cast<int>(json.size()), nullptr, 0, nullptr, nullptr);
	std::string bytes(static_cast<std::size_t>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, json.data(), static_cast<int>(json.size()), bytes.data(), length, nullptr, nullptr);

	auto temp = m_RulesPath;
	temp += L".tmp";
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		out.close();
		if (out.fail())
		{
			error = L"Cannot write rules.json";
			return false;
		}
	}

	if (!MoveFileEx(temp.c_str(), m_RulesPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		error = std::format(L"Cannot replace rules.json (error {})", GetLastError());
		return false;
	}

	Reload();
	return true;
}
