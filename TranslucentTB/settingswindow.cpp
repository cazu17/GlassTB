#include "mainappwindow.hpp"
#include <array>
#include <format>
#include <utility>

#include "application.hpp"
#include "compat/backend.hpp"
#include "settings/appearanceedit.hpp"

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

	const bool saved = manager.SaveConfig();
	if (saved)
	{
		m_App.GetWorker().SetAppearancePreview(std::nullopt);
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
