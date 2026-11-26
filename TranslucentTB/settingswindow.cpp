#include "mainappwindow.hpp"
#include <array>
#include <format>
#include <utility>

#include "application.hpp"
#include "compat/backend.hpp"
#include "input/hotkey.hpp"
#include "settings/appearanceedit.hpp"
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <optional>
#include <ShObjIdl.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>

#include "../ProgramLog/error/win32.hpp"

namespace {
	constexpr std::array States{
		txmp::TaskbarState::Desktop, txmp::TaskbarState::VisibleWindow, txmp::TaskbarState::MaximisedWindow,
		txmp::TaskbarState::StartOpened, txmp::TaskbarState::SearchOpened, txmp::TaskbarState::TaskViewOpened,
		txmp::TaskbarState::BatterySaver
	};

	settings::AppearanceValues ReadAppearance(txmp::TaskbarState state, const TaskbarAppearance &appearance)
	{
		return {
			static_cast<std::int32_t>(appearance.Accent), appearance.Color.ToRGBA(), appearance.ShowPeek,
			appearance.ShowLine, appearance.BlurRadius,
			state == txmp::TaskbarState::Desktop || static_cast<const OptionalTaskbarAppearance &>(appearance).Enabled
		};
	}

	txmp::TaskbarAppearance MakeModel(txmp::TaskbarState state, const settings::AppearanceValues &appearance)
	{
		const auto accent = static_cast<txmp::AccentState>(appearance.accent);
		const auto color = Util::Color::FromRGBA(appearance.color);
		if (state == txmp::TaskbarState::Desktop)
		{
			return { accent, color, appearance.showPeek, appearance.showLine, appearance.blurRadius };
		}
		return txmp::OptionalTaskbarAppearance(appearance.enabled, accent, color, appearance.showPeek,
			appearance.showLine, appearance.blurRadius);
	}

	std::size_t CountRules(const RuledTaskbarAppearance &appearance)
	{
		return appearance.ClassRules.size() + appearance.TitleRules.size() + appearance.FileRules.size();
	}
}

void MainAppWindow::OpenSettings()
{
	if (m_SettingsWindow)
	{
		SetForegroundWindow(m_SettingsWindow);
		return;
	}
	if (m_SettingsOpening)
	{
		return;
	}

	m_SettingsOpening = true;
	m_App.CreateXamlWindow<settings_page_t>(xaml_startup_position::center,
		[this](const settings_page_t &content, BaseXamlPageHost *host)
		{
			content.Closed([this]
			{
				m_App.DispatchToMainThread([this]
				{
					m_SettingsDispatcher = nullptr;
					m_SettingsPage = {};
					m_SettingsWindow = nullptr;
					m_SettingsOpening = false;
					m_App.GetWorker().SetAppearancePreview(std::nullopt);
				});
			});

			content.PreviewChanged([this](const txmp::TaskbarAppearance &appearance)
			{
				const auto value = appearance ? std::optional<TaskbarAppearance>{ TaskbarAppearance(appearance) } : std::nullopt;
				m_App.DispatchToMainThread([this, value]
				{
					if (m_SettingsWindow)
					{
						m_App.GetWorker().SetAppearancePreview(value);
					}
				});
			});

			content.ProfileActionRequested([this](const winrt::hstring &action, const winrt::hstring &name)
			{
				m_App.DispatchToMainThread([this, action = std::wstring(action), name = std::wstring(name)]
				{
					if (m_SettingsWindow)
					{
						HandleProfileAction(action, name);
					}
				});
			});

			content.AutomationActionRequested([this](const winrt::hstring &action, std::int32_t index, std::int32_t value)
			{
				m_App.DispatchToMainThread([this, action = std::wstring(action), index, value]
				{
					if (m_SettingsWindow)
					{
						HandleAutomationAction(action, index, value);
					}
				});
			});

			content.CreateRuleRequested([this](const winrt::TranslucentTB::Xaml::Pages::NewRuleSpec &spec)
			{
				m_App.DispatchToMainThread([this, spec]
				{
					if (m_SettingsWindow)
					{
						HandleCreateRule(spec);
					}
				});
			});

			content.AdaptiveColorChanged([this](bool enabled, bool secondary, std::int32_t intensity, std::int32_t saturation, std::int32_t contrast, bool excludeCurrent)
			{
				m_App.DispatchToMainThread([=, this]
				{
					if (m_SettingsWindow)
					{
						HandleAdaptiveColor(enabled, secondary, intensity, saturation, contrast, excludeCurrent);
					}
				});
			});

			content.HotkeysChanged([this](const winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> &bindings)
			{
				std::vector<std::wstring> values;
				for (const auto &b : bindings)
				{
					values.emplace_back(b);
				}
				m_App.DispatchToMainThread([this, values = std::move(values)]
				{
					if (m_SettingsWindow)
					{
						HandleHotkeys(values);
					}
				});
			});

			content.MetricsRequested([this]
			{
				m_App.DispatchToMainThread([this]
				{
					if (m_SettingsWindow)
					{
						PublishMetrics();
					}
				});
			});

			content.MonitorProfileChanged([this](const winrt::hstring &device, const winrt::hstring &profile)
			{
				m_App.DispatchToMainThread([this, device = std::wstring(device), profile = std::wstring(profile)]
				{
					if (m_SettingsWindow)
					{
						auto &manager = m_App.GetConfigManager();
						auto &map = manager.GetConfig().MonitorProfiles;
						const auto previous = map;
						if (profile.empty())
						{
							map.erase(device);
						}
						else
						{
							map.insert_or_assign(device, profile);
						}
						if (!manager.SaveConfig())
						{
							map = previous;
						}
						PublishDisplays();
					}
				});
			});

			content.ApplyRequested([this](const winrt::Windows::Foundation::Collections::IVectorView<settings_edit_t> &edits)
			{
				std::vector<settings_edit_t> snapshot;
				snapshot.reserve(edits.Size());
				for (const auto &edit : edits)
				{
					snapshot.push_back(edit);
				}
				m_App.DispatchToMainThread([this, snapshot = std::move(snapshot)]
				{
					if (m_SettingsWindow)
					{
						ApplySettings(snapshot);
					}
				});
			});

			m_App.DispatchToMainThread([this, hwnd = host->handle(), weak = winrt::make_weak(content),
				dispatcher = winrt::Windows::System::DispatcherQueue::GetForCurrentThread()]
			{
				m_SettingsWindow = hwnd;
				m_SettingsPage = weak;
				m_SettingsDispatcher = dispatcher;
				m_SettingsOpening = false;
				PublishSettings();
				PublishDiagnostics();
				PublishProfiles();
				PublishAutomation();
				PublishAdaptiveColor();
				PublishHotkeys({}, {});
				PublishDisplays();
			});
		});
}

void MainAppWindow::ApplySettings(const std::vector<settings_edit_t> &edits)
{
	auto &manager = m_App.GetConfigManager();
	auto &config = manager.GetConfig();
	if (config.DisableSaving)
	{
		PublishSettings(false);
		return;
	}

	const auto original = config;
	for (const auto &edit : edits)
	{
		auto &appearance = GetConfigForState(edit.State);
		const auto merged = settings::MergeAppearance(ReadAppearance(edit.State, appearance),
			{ edit.Accent, edit.Color, edit.ShowPeek, edit.ShowLine, edit.BlurRadius, edit.Enabled }, edit.Fields);
		appearance = TaskbarAppearance(static_cast<ACCENT_STATE>(merged.accent), Util::Color::FromRGBA(merged.color),
			merged.showPeek, merged.showLine, merged.blurRadius);
		if (edit.State != txmp::TaskbarState::Desktop)
		{
			static_cast<OptionalTaskbarAppearance &>(appearance).Enabled = merged.enabled;
		}
	}

	config.ActiveProfile.clear();
	const bool saved = manager.SaveConfig();
	if (saved)
	{
		m_App.GetWorker().SetAppearancePreview(std::nullopt);
		PublishProfiles();
	}
	else
	{
		config = original;
	}

	PublishSettings(saved);
}

void MainAppWindow::PublishSettings(std::optional<bool> applyResult)
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	const auto &config = m_App.GetConfigManager().GetConfig();
	const Config defaults;
	const std::array<const TaskbarAppearance *, States.size()> defaultAppearances{
		&defaults.DesktopAppearance, &defaults.VisibleWindowAppearance, &defaults.MaximisedWindowAppearance,
		&defaults.StartOpenedAppearance, &defaults.SearchOpenedAppearance, &defaults.TaskViewOpenedAppearance,
		&defaults.BatterySaverAppearance
	};

	std::array<settings::AppearanceValues, States.size()> saved{}, reset{};
	for (std::size_t i = 0; i < States.size(); ++i)
	{
		saved[i] = ReadAppearance(States[i], GetConfigForState(States[i]));
		reset[i] = ReadAppearance(States[i], *defaultAppearances[i]);
	}

	const auto ruleCount = static_cast<std::uint32_t>(CountRules(config.VisibleWindowAppearance) + CountRules(config.MaximisedWindowAppearance));
	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, saved, reset, ruleCount,
		canSave = !config.DisableSaving, hasBattery = page().SystemHasBattery(), applyResult]
	{
		if (const auto settingsPage = weak.get())
		{
			if (applyResult)
			{
				settingsPage.FinishApply(*applyResult);
			}
			for (std::size_t i = 0; i < States.size(); ++i)
			{
				settingsPage.SetAppearance(States[i], MakeModel(States[i], saved[i]), MakeModel(States[i], reset[i]));
			}
			settingsPage.UpdateEditor(ruleCount, canSave, hasBattery);
		}
	});
}

void MainAppWindow::PublishDiagnostics()
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	auto &worker = m_App.GetWorker();
	const auto system = worker.GetSystemInfo();
	const auto capabilities = worker.GetCapabilities();
	const auto type = worker.GetType();
	const auto backend = compat::SelectBackend(type, capabilities);
	const auto count = static_cast<std::uint32_t>(worker.GetTaskbarMonitorCount());
	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, system, capabilities, type, backend, count,
		canPeek = worker.CanShowPeek(), canShowLine = worker.CanShowTaskbarLine()]
	{
		if (const auto settingsPage = weak.get())
		{
			const auto &v = system.version;
			const auto version = v ? std::format(L"{}.{}.{}.{}", v->major, v->minor, v->build, v->revision) : std::wstring{};
			settingsPage.SetDiagnostics(version, system.nativeArchitecture, system.processArchitecture, compat::ToString(type),
				compat::ToString(backend), count, capabilities.blurAccentState, capabilities.xamlTaskbarService, canPeek, canShowLine);
		}
	});
}

namespace {
	// Shows a modal Open/Save dialog for JSON files owned by the settings window.
	std::optional<std::filesystem::path> PickJsonFile(HWND owner, bool save, std::wstring_view suggestedName)
	{
		try
		{
			const auto dialog = save
				? wil::CoCreateInstance<IFileDialog>(CLSID_FileSaveDialog)
				: wil::CoCreateInstance<IFileDialog>(CLSID_FileOpenDialog);

			static constexpr COMDLG_FILTERSPEC filters[] = { { L"JSON", L"*.json" } };
			THROW_IF_FAILED(dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters));
			THROW_IF_FAILED(dialog->SetDefaultExtension(L"json"));

			FILEOPENDIALOGOPTIONS options = 0;
			THROW_IF_FAILED(dialog->GetOptions(&options));
			THROW_IF_FAILED(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST)));
			if (save && !suggestedName.empty())
			{
				THROW_IF_FAILED(dialog->SetFileName(std::wstring(suggestedName).c_str()));
			}

			const HRESULT shown = dialog->Show(owner);
			if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED))
			{
				return std::nullopt;
			}
			THROW_IF_FAILED(shown);

			wil::com_ptr<IShellItem> item;
			THROW_IF_FAILED(dialog->GetResult(item.put()));
			wil::unique_cotaskmem_string path;
			THROW_IF_FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put()));
			return std::filesystem::path(path.get());
		}
		catch (const wil::ResultException &ex)
		{
			HresultHandle(ex.GetErrorCode(), spdlog::level::warn, L"File dialog failed");
			return std::nullopt;
		}
	}
}

void MainAppWindow::HandleProfileAction(const std::wstring &action, const std::wstring &name)
{
	auto &profiles = m_App.GetProfileManager();
	auto &manager = m_App.GetConfigManager();
	auto &config = manager.GetConfig();

	bool success = false;
	std::wstring key = L"Failed";
	std::wstring detail;

	if (action == L"activate")
	{
		if (const auto entry = profiles.Find(name))
		{
			const auto original = config;
			entry->profile.ApplyTo(config);
			if (manager.SaveConfig())
			{
				success = true;
				key = L"Activated";
				detail = entry->profile.Name;
				m_App.GetWorker().ConfigurationChanged();
			}
			else
			{
				config = original;
				key = L"SaveFailed";
			}
		}
		else
		{
			key = L"NotFound";
		}
	}
	else if (action == L"save")
	{
		if (!Profile::IsValidName(name))
		{
			key = L"InvalidName";
		}
		else if (profiles.Save(Profile::FromConfig(config, name), detail))
		{
			success = true;
			key = L"Saved";
			detail = name;
		}
	}
	else if (action == L"duplicate")
	{
		if (const auto entry = profiles.Find(name))
		{
			auto copy = entry->profile;
			copy.Name = profiles.UniqueName(entry->profile.Name);
			if (profiles.Save(copy, detail))
			{
				success = true;
				key = L"Duplicated";
				detail = copy.Name;
			}
		}
		else
		{
			key = L"NotFound";
		}
	}
	else if (action == L"delete")
	{
		if (profiles.Delete(name, detail))
		{
			success = true;
			key = L"Deleted";
			detail = name;
			if (config.ActiveProfile == name)
			{
				config.ActiveProfile.clear();
				manager.SaveConfig();
			}
		}
	}
	else if (action == L"import")
	{
		if (const auto file = PickJsonFile(m_SettingsWindow, false, { }))
		{
			if (const auto imported = profiles.Import(*file, detail))
			{
				success = true;
				key = L"Imported";
				detail = imported->Name;
			}
		}
		else
		{
			key = L"Cancelled";
		}
	}
	else if (action == L"export")
	{
		if (const auto file = PickJsonFile(m_SettingsWindow, true, Profile::FileStem(name) + L".json"))
		{
			if (profiles.Export(name, *file, detail))
			{
				success = true;
				key = L"Exported";
				detail = file->native();
			}
		}
		else
		{
			key = L"Cancelled";
		}
	}

	if (!success && key == L"Failed" && !detail.empty())
	{
		MessagePrint(spdlog::level::warn, std::format(L"Profile action {} failed: {}", action, detail));
	}

	PublishSettings();
	PublishTrayProfiles();
	PublishProfiles(ProfileActionResult { success, std::move(key), std::move(detail) });
	PublishDisplays();
}

void MainAppWindow::PublishTrayProfiles()
{
	std::vector<winrt::hstring> names;
	for (const auto &entry : m_App.GetProfileManager().List())
	{
		names.emplace_back(entry.profile.Name);
	}

	const auto &manager = m_App.GetConfigManager();
	const auto &config = manager.GetConfig();
	page().SetProfiles(winrt::single_threaded_vector(std::move(names)).GetView(), winrt::hstring(config.ActiveProfile),
		!config.DisableSaving && !manager.IsReadOnlyNewerSchema());
}

void MainAppWindow::PublishProfiles(std::optional<ProfileActionResult> result)
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	std::vector<winrt::TranslucentTB::Xaml::Pages::ProfileInfo> infos;
	for (const auto &entry : m_App.GetProfileManager().List())
	{
		infos.push_back({ winrt::hstring(entry.profile.Name), entry.builtIn });
	}

	const auto &config = m_App.GetConfigManager().GetConfig();
	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, infos = std::move(infos), active = winrt::hstring(config.ActiveProfile),
		canSave = !config.DisableSaving && !m_App.GetConfigManager().IsReadOnlyNewerSchema(), result = std::move(result)]() mutable
	{
		if (const auto settingsPage = weak.get())
		{
			settingsPage.SetProfiles(winrt::single_threaded_vector(std::move(infos)).GetView(), active, canSave);
			if (result)
			{
				settingsPage.FinishProfileAction(result->success, winrt::hstring(result->key), winrt::hstring(result->detail));
			}
		}
	});
}

namespace {
	std::wstring SummarizeCondition(const rules::Condition &c)
	{
		using rules::ConditionType;
		switch (c.type)
		{
		case ConditionType::All:
		case ConditionType::Any:
		{
			std::wstring out = L"(";
			for (std::size_t i = 0; i < c.children.size(); ++i)
			{
				out += (i ? (c.type == ConditionType::All ? L" AND " : L" OR ") : L"") + SummarizeCondition(c.children[i]);
			}
			return out + L")";
		}
		case ConditionType::Not:
			return L"NOT " + (c.children.empty() ? std::wstring { } : SummarizeCondition(c.children.front()));
		default:
		{
			const auto json = rules::detail::ConditionToJson(c);
			return json;
		}
		}
	}

	std::wstring SummarizeRule(const rules::Rule &rule)
	{
		std::wstring out = SummarizeCondition(rule.condition) + L"  \u2192  ";
		for (std::size_t i = 0; i < rule.actions.size(); ++i)
		{
			out += (i ? L", " : L"") + rules::detail::ActionToJson(rule.actions[i]);
		}
		return out;
	}

	// Escapes a user-supplied string for embedding in a JSON document.
	std::wstring JsonString(std::wstring_view text)
	{
		return L"\"" + rules::detail::Escape(text) + L"\"";
	}

	std::vector<std::wstring> SplitList(std::wstring_view text)
	{
		std::vector<std::wstring> items;
		std::size_t start = 0;
		while (start <= text.size())
		{
			const auto end = text.find(L',', start);
			auto item = text.substr(start, end == std::wstring_view::npos ? std::wstring_view::npos : end - start);
			while (!item.empty() && iswspace(item.front())) item.remove_prefix(1);
			while (!item.empty() && iswspace(item.back())) item.remove_suffix(1);
			if (!item.empty())
			{
				items.emplace_back(item);
			}
			if (end == std::wstring_view::npos)
			{
				break;
			}
			start = end + 1;
		}
		return items;
	}

	// Builds the JSON of a single condition from the simple editor's fields. Validation is
	// left to rules::FromJson so the editor and rules.json share exactly the same rules.
	std::wstring BuildConditionJson(std::wstring_view type, std::wstring_view value)
	{
		std::wstring json = std::format(L"{{ \"type\": {}", JsonString(type));
		if (type == L"foreground_app" || type == L"app_running")
		{
			json += L", \"apps\": [";
			const auto apps = SplitList(value);
			for (std::size_t i = 0; i < apps.size(); ++i)
			{
				json += (i ? L", " : L"") + JsonString(apps[i]);
			}
			json += L"]";
		}
		else if (type == L"time_range")
		{
			const auto dash = value.find(L'-');
			const auto from = SplitList(value.substr(0, dash));
			const auto to = dash == std::wstring_view::npos ? std::vector<std::wstring> { } : SplitList(value.substr(dash + 1));
			json += std::format(L", \"from\": {}, \"to\": {}", JsonString(from.empty() ? L"" : from[0]), JsonString(to.empty() ? L"" : to[0]));
		}
		else if (type == L"weekdays")
		{
			json += L", \"days\": [";
			const auto days = SplitList(value);
			for (std::size_t i = 0; i < days.size(); ++i)
			{
				const bool numeric = !days[i].empty() && std::ranges::all_of(days[i], [](wchar_t c) { return iswdigit(c) != 0; }) && days[i].size() <= 2;
				json += (i ? L", " : L"") + (numeric ? days[i] : JsonString(days[i]));
			}
			json += L"]";
		}
		else if (type == L"battery_below" || type == L"monitors_at_least")
		{
			const std::wstring trimmed = SplitList(value).empty() ? std::wstring { } : SplitList(value)[0];
			const bool numeric = !trimmed.empty() && trimmed.size() <= 4 && std::ranges::all_of(trimmed, [](wchar_t c) { return iswdigit(c) != 0; });
			json += std::format(L", \"{}\": {}", type == L"battery_below" ? L"percent" : L"count", numeric ? trimmed : JsonString(trimmed));
		}
		json += L" }";
		return json;
	}
}

void MainAppWindow::HandleAutomationAction(const std::wstring &action, std::int32_t index, std::int32_t value)
{
	auto &engine = m_App.GetRuleEngine();
	bool success = true;
	std::wstring key = L"Done";
	std::wstring detail;

	if (action == L"edit")
	{
		engine.OpenRulesFile();
		key = L"Opened";
	}
	else if (action == L"reload")
	{
		engine.Reload();
		key = L"Reloaded";
	}
	else if (action == L"pause")
	{
		engine.SetPaused(value != 0);
		key = value ? L"PausedNow" : L"Resumed";
	}
	else if (index >= 0 && static_cast<std::size_t>(index) < engine.Rules().size())
	{
		auto list = engine.Rules();
		auto &rule = list[static_cast<std::size_t>(index)];
		detail = rule.name;
		if (action == L"toggle")
		{
			rule.enabled = !rule.enabled;
			key = rule.enabled ? L"Enabled" : L"Disabled";
		}
		else if (action == L"priority")
		{
			rule.priority = std::clamp(rule.priority + value, -1000, 1000);
			key = L"PriorityChanged";
		}
		else if (action == L"delete")
		{
			list.erase(list.begin() + index);
			key = L"Deleted";
		}

		std::wstring error;
		if (!engine.SaveRules(list, error))
		{
			success = false;
			key = L"SaveFailed";
			detail = error;
		}
	}
	else
	{
		success = false;
		key = L"Failed";
	}

	PublishAutomation(ActionResult { success, std::move(key), std::move(detail) });
}

void MainAppWindow::HandleCreateRule(const winrt::TranslucentTB::Xaml::Pages::NewRuleSpec &spec)
{
	auto &engine = m_App.GetRuleEngine();

	std::wstring condition = BuildConditionJson(spec.Condition, spec.ConditionValue);
	if (spec.Negate)
	{
		condition = L"{ \"type\": \"not\", \"conditions\": [ " + condition + L" ] }";
	}

	std::wstring actionJson = std::format(L"{{ \"type\": {}", JsonString(spec.Action));
	if (spec.Action == L"activate_profile")
	{
		actionJson += L", \"profile\": " + JsonString(spec.ActionValue);
	}
	actionJson += L" }";

	const std::wstring document = std::format(L"{{ \"rules\": [ {{ \"name\": {}, \"priority\": {}, \"condition\": {}, \"actions\": [ {} ] }} ] }}",
		JsonString(spec.Name), spec.Priority, condition, actionJson);

	std::wstring error;
	const auto parsed = rules::FromJson(document, error);
	if (!parsed || parsed->size() != 1)
	{
		PublishAutomation(ActionResult { false, L"Invalid", error });
		return;
	}

	auto list = engine.Rules();
	list.push_back(parsed->front());
	if (!engine.SaveRules(list, error))
	{
		PublishAutomation(ActionResult { false, L"SaveFailed", error });
		return;
	}

	PublishAutomation(ActionResult { true, L"Created", std::wstring(spec.Name) });
}

void MainAppWindow::PublishAutomation(std::optional<ActionResult> result)
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	const auto &engine = m_App.GetRuleEngine();
	const auto status = engine.GetStatus();
	std::vector<winrt::TranslucentTB::Xaml::Pages::RuleInfo> infos;
	for (const auto &rule : engine.Rules())
	{
		infos.push_back({ winrt::hstring(rule.name), rule.enabled, rule.priority, winrt::hstring(SummarizeRule(rule)) });
	}

	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, infos = std::move(infos), active = winrt::hstring(status.activeRule),
		error = winrt::hstring(status.loadError), paused = status.paused, result = std::move(result)]() mutable
	{
		if (const auto settingsPage = weak.get())
		{
			settingsPage.SetAutomation(winrt::single_threaded_vector(std::move(infos)).GetView(), active, error, paused);
			if (result)
			{
				settingsPage.FinishAutomationAction(result->success, winrt::hstring(result->key), winrt::hstring(result->detail));
			}
		}
	});
}

void MainAppWindow::HandleAdaptiveColor(bool enabled, bool secondary, std::int32_t intensity, std::int32_t saturation, std::int32_t contrast, bool excludeCurrent)
{
	auto &manager = m_App.GetConfigManager();
	auto &settings = manager.GetConfig().AdaptiveColor;
	settings.Enabled = enabled;
	settings.UseSecondary = secondary;
	settings.Intensity = static_cast<std::uint8_t>(std::clamp(intensity, 0, 255));
	settings.Saturation = static_cast<std::uint8_t>(std::clamp(saturation, 0, 200));
	settings.Contrast = static_cast<std::uint8_t>(std::clamp(contrast, 0, 100));

	if (excludeCurrent)
	{
		wchar_t path[MAX_PATH] = { };
		if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0) && path[0])
		{
			std::wstring name = std::filesystem::path(path).filename().native();
			for (auto &c : name)
			{
				c = static_cast<wchar_t>(towlower(c));
			}
			if (std::ranges::find(settings.Excluded, name) == settings.Excluded.end())
			{
				settings.Excluded.push_back(std::move(name));
			}
		}
	}

	// Saving reloads the file and calls RuleEngine::BaseConfigurationChanged; if saving is
	// disabled the change still applies in memory for this session.
	if (!manager.SaveConfig())
	{
		m_App.GetRuleEngine().BaseConfigurationChanged();
	}
	PublishAdaptiveColor();
}

void MainAppWindow::PublishAdaptiveColor()
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	const auto &manager = m_App.GetConfigManager();
	const auto &settings = manager.GetConfig().AdaptiveColor;
	const auto status = m_App.GetRuleEngine().GetStatus();
	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, enabled = settings.Enabled, secondary = settings.UseSecondary,
		intensity = static_cast<std::int32_t>(settings.Intensity), saturation = static_cast<std::int32_t>(settings.Saturation),
		contrast = static_cast<std::int32_t>(settings.Contrast), active = status.adaptiveActive, color = status.adaptiveColor.ToRGBA(),
		source = winrt::hstring(status.adaptiveSource), canSave = !manager.IsReadOnlyNewerSchema()]
	{
		if (const auto settingsPage = weak.get())
		{
			settingsPage.SetAdaptiveColor(enabled, secondary, intensity, saturation, contrast, active, color, source, canSave);
		}
	});
}

void MainAppWindow::HandleHotkeys(const std::vector<std::wstring> &values)
{
	auto &manager = m_App.GetConfigManager();
	auto &bindings = manager.GetConfig().Hotkeys.Bindings;
	for (std::size_t i = 0; i < bindings.size() && i < values.size(); ++i)
	{
		if (!values[i].empty() && !hotkey::Parse(values[i]))
		{
			PublishHotkeys(L"Invalid", values[i]);
			return;
		}
	}

	const auto previous = bindings;
	for (std::size_t i = 0; i < bindings.size() && i < values.size(); ++i)
	{
		bindings[i] = values[i];
	}

	if (!manager.SaveConfig())
	{
		bindings = previous;
		PublishHotkeys(L"SaveFailed", {});
		return;
	}

	RegisterUserHotkeys();
	PublishHotkeys(L"Saved", {});
}

void MainAppWindow::PublishHotkeys(std::wstring key, std::wstring detail)
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	std::vector<winrt::hstring> values;
	for (const auto &b : m_App.GetConfigManager().GetConfig().Hotkeys.Bindings)
	{
		values.emplace_back(b);
	}
	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, values = std::move(values), key = winrt::hstring(key), detail = winrt::hstring(detail)]() mutable
	{
		if (const auto settingsPage = weak.get())
		{
			settingsPage.SetHotkeys(winrt::single_threaded_vector(std::move(values)).GetView(), key, detail);
		}
	});
}

void MainAppWindow::PublishMetrics()
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	const auto sample = m_Metrics.Take();
	auto &worker = m_App.GetWorker();
	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, sample, refreshes = worker.RefreshCount(),
		evaluations = m_App.GetRuleEngine().GetStatus().evaluations, paused = worker.EffectsSuspended(), safe = worker.SafeMode()]
	{
		if (const auto settingsPage = weak.get())
		{
			settingsPage.SetMetrics(sample.cpuPercent, sample.privateBytes, sample.workingSet, sample.handles, sample.threads,
				sample.gdiObjects, sample.userObjects, refreshes, evaluations, paused, safe);
		}
	});
}

void MainAppWindow::PublishDisplays()
{
	if (!m_SettingsDispatcher)
	{
		return;
	}

	struct Context {
		const Config *config;
		std::vector<winrt::TranslucentTB::Xaml::Pages::DisplayInfo> displays;
	} context { &m_App.GetConfigManager().GetConfig(), { } };

	EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL
	{
		auto &ctx = *reinterpret_cast<Context *>(data);
		MONITORINFOEXW info = { };
		info.cbSize = sizeof(info);
		if (GetMonitorInfoW(monitor, &info))
		{
			const std::wstring device = info.szDevice;
			const auto &rc = info.rcMonitor;
			const auto it = ctx.config->MonitorProfiles.find(device);
			ctx.displays.push_back({
				winrt::hstring(device),
				winrt::hstring(std::format(L"{}  ·  {}×{}{}", device, rc.right - rc.left, rc.bottom - rc.top, (info.dwFlags & MONITORINFOF_PRIMARY) ? L"  ·  ★" : L"")),
				it != ctx.config->MonitorProfiles.end() ? winrt::hstring(it->second) : winrt::hstring { }
			});
		}
		return TRUE;
	}, reinterpret_cast<LPARAM>(&context));

	m_SettingsDispatcher.TryEnqueue([weak = m_SettingsPage, displays = std::move(context.displays),
		canSave = !context.config->DisableSaving && !m_App.GetConfigManager().IsReadOnlyNewerSchema()]() mutable
	{
		if (const auto settingsPage = weak.get())
		{
			settingsPage.SetDisplays(winrt::single_threaded_vector(std::move(displays)).GetView(), canSave);
		}
	});
}
