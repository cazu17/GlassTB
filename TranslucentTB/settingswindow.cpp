#include "mainappwindow.hpp"
#include <array>
#include <format>
#include <utility>

#include "application.hpp"
#include "compat/backend.hpp"
#include "settings/appearanceedit.hpp"
#include <filesystem>
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
