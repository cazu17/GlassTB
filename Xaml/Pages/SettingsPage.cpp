#include "pch.h"

#include "SettingsPage.h"
#if __has_include("Pages/SettingsPage.g.cpp")
#include "Pages/SettingsPage.g.cpp"
#endif

#include <algorithm>
#include <cmath>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include "appinfo.hpp"
#include "util/color.hpp"

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
	namespace {
		settings::AppearanceValues ReadAppearance(const txmp::TaskbarAppearance &appearance)
		{
			const auto optional = appearance.try_as<txmp::OptionalTaskbarAppearance>();
			return {
				static_cast<std::int32_t>(appearance.Accent()), Util::Color(appearance.Color()).ToRGBA(),
				appearance.ShowPeek(), appearance.ShowLine(), appearance.BlurRadius(),
				!optional || optional.Enabled()
			};
		}

		wux::Visibility VisibleWhen(bool value) noexcept
		{
			return value ? wux::Visibility::Visible : wux::Visibility::Collapsed;
		}
	}

	void SettingsPage::InitializeComponent()
	{
		ComponentConnectorT::InitializeComponent();
		m_Resources = wam::Resources::ResourceLoader::GetForUIContext(UIContext());
		Title(APP_NAME);
		Navigation().SelectedItem(HomeItem());
		State().SelectedIndex(0);
		ConditionKind().SelectedIndex(0);
		ActionKind().SelectedIndex(0);
	}

	hstring SettingsPage::Text(std::wstring_view key) const
	{
		std::wstring name = L"/TranslucentTB.Xaml/Resources/SettingsPage_";
		name += key;
		return m_Resources.GetString(name);
	}

	bool SettingsPage::Close()
	{
		if (m_ResetDialogOpen || m_DeleteDialogOpen)
		{
			return false;
		}

		m_Closing = true;
		if (m_MetricsTimer)
		{
			m_MetricsTimer.Stop();
		}
		if (m_AdaptiveTimer)
		{
			m_AdaptiveTimer.Stop();
		}
		return base_type::Close();
	}

	void SettingsPage::SetDiagnostics(const hstring &windowsVersion, const hstring &nativeArchitecture, const hstring &processArchitecture,
		const hstring &taskbarType, const hstring &backend, std::uint32_t monitorCount, bool blurAccentState,
		bool xamlTaskbarService, bool canPeek, bool canShowLine)
	{
		m_HasTaskbar = monitorCount != 0;
		m_CanBlur = m_HasTaskbar && (blurAccentState || xamlTaskbarService);
		m_CanChangeRadius = m_HasTaskbar && xamlTaskbarService;
		m_CanPeek = canPeek;
		m_CanShowLine = canShowLine;

		std::wstring summary;
		const auto append = [&](std::wstring_view key, std::wstring_view value)
		{
			if (!summary.empty())
			{
				summary += L'\n';
			}
			summary += Text(key);
			summary += L": ";
			summary += value;
		};

		append(L"Engine", Text(m_HasTaskbar ? L"EngineActive" : L"EngineWaiting"));
		append(L"Backend", backend);
		append(L"Windows", windowsVersion.empty() ? Text(L"Unknown") : windowsVersion);
		append(L"NativeArchitecture", nativeArchitecture);
		append(L"ProcessArchitecture", processArchitecture);
		append(L"TaskbarType", taskbarType);
		append(L"AccentBlur", Text(blurAccentState ? L"Yes" : L"No"));
		append(L"XamlService", Text(xamlTaskbarService ? L"Yes" : L"No"));
		append(L"MonitorCount", std::to_wstring(monitorCount));
		DiagnosticSummary().Text(summary);
		CopyButton().IsEnabled(true);
		RefreshControls();
		UpdatePreview();
	}

	void SettingsPage::CopyClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		try
		{
			wam::DataTransfer::DataPackage data;
			data.SetText(DiagnosticSummary().Text());
			wam::DataTransfer::Clipboard::SetContent(data);
			wam::DataTransfer::Clipboard::Flush();
			CopyStatus().Text(Text(L"Copied"));
		}
		catch (const hresult_error &)
		{
			CopyStatus().Text(Text(L"CopyFailed"));
		}
	}

	void SettingsPage::SetAppearance(txmp::TaskbarState state, const txmp::TaskbarAppearance &saved, const txmp::TaskbarAppearance &defaults)
	{
		auto &appearance = m_Appearances.at(static_cast<std::size_t>(state));
		const auto fields = settings::ChangedFields(appearance.saved, appearance.draft);
		appearance.saved = ReadAppearance(saved);
		appearance.defaults = ReadAppearance(defaults);
		appearance.draft = settings::MergeAppearance(appearance.saved, appearance.draft, fields);
	}

	void SettingsPage::UpdateEditor(std::uint32_t ruleCount, bool canSave, bool hasBattery)
	{
		m_Ready = true;
		m_CanSave = canSave;
		m_HasBattery = hasBattery;
		RuleSummary().Text(winrt::format(L"{} {}. {}", Text(L"Rules"), ruleCount, Text(L"RulesReadOnly")));
		LoadControls();
		UpdatePreview();
	}

	void SettingsPage::NavigationChanged(const muxc::NavigationView &, const muxc::NavigationViewSelectionChangedEventArgs &args)
	{
		const auto selected = args.SelectedItem();
		const bool appearance = selected == AppearanceItem();
		const bool profiles = selected == ProfilesItem();
		const bool automation = selected == AutomationItem();
		const bool adaptive = selected == AdaptiveItem();
		const bool hotkeys = selected == HotkeysItem();
		const bool performance = selected == PerformanceItem();
		const bool displays = selected == DisplaysItem();
		HomeSection().Visibility(VisibleWhen(!appearance && !profiles && !automation && !adaptive && !hotkeys && !performance && !displays));
		DisplaysSection().Visibility(VisibleWhen(displays));
		PerformanceSection().Visibility(VisibleWhen(performance));

		// Metrics are only sampled while the section is visible (every 2 s).
		if (performance)
		{
			if (!m_MetricsTimer)
			{
				m_MetricsTimer = wux::DispatcherTimer();
				m_MetricsTimer.Interval(std::chrono::seconds(2));
				m_MetricsTimer.Tick([weak = get_weak()](const IInspectable &, const IInspectable &)
				{
					if (const auto self = weak.get())
					{
						self->m_MetricsRequested();
					}
				});
			}
			m_MetricsRequested();
			m_MetricsTimer.Start();
		}
		else if (m_MetricsTimer)
		{
			m_MetricsTimer.Stop();
		}
		HotkeysSection().Visibility(VisibleWhen(hotkeys));
		AdaptiveSection().Visibility(VisibleWhen(adaptive));
		AppearanceSection().Visibility(VisibleWhen(appearance));
		ProfilesSection().Visibility(VisibleWhen(profiles));
		AutomationSection().Visibility(VisibleWhen(automation));
		UpdatePreview();
	}

	void SettingsPage::StateChanged(const IInspectable &, const wuxc::SelectionChangedEventArgs &)
	{
		const int state = State().SelectedIndex();
		if (state >= 0)
		{
			m_State = static_cast<std::size_t>(state);
			if (m_Ready)
			{
				LoadControls();
				UpdatePreview();
			}
		}
	}

	bool SettingsPage::CanEdit() const noexcept
	{
		return m_Ready && !m_Updating && !m_Applying && !m_Closing;
	}

	void SettingsPage::LoadControls()
	{
		m_Updating = true;
		const auto &draft = m_Appearances.at(m_State).draft;
		EnabledOption().IsChecked(box_value(draft.enabled).as<wf::IReference<bool>>());
		Material().SelectedIndex(draft.accent);
		Picker().Color(Util::Color::FromRGBA(draft.color));
		Peek().IsChecked(box_value(draft.showPeek).as<wf::IReference<bool>>());
		Line().IsChecked(box_value(draft.showLine).as<wf::IReference<bool>>());
		Radius().Value(draft.blurRadius);
		m_Updating = false;
		RefreshControls();
	}

	void SettingsPage::RefreshControls()
	{
		if (!m_Ready)
		{
			return;
		}

		const auto &draft = m_Appearances.at(m_State).draft;
		const bool batteryUnavailable = m_State == static_cast<std::size_t>(txmp::TaskbarState::BatterySaver) && !m_HasBattery;
		const bool canEditAppearance = !batteryUnavailable;
		const bool canChangeColor = canEditAppearance && draft.accent != static_cast<std::int32_t>(txmp::AccentState::Normal);
		const bool canChangeRadius = canEditAppearance && m_CanChangeRadius &&
			draft.accent == static_cast<std::int32_t>(txmp::AccentState::Blur);
		const bool hasChanges = std::ranges::any_of(m_Appearances, [](const auto &appearance)
		{
			return settings::ChangedFields(appearance.saved, appearance.draft) != 0;
		});

		Editor().IsEnabled(!m_Applying);
		EnabledOption().Visibility(VisibleWhen(m_State != static_cast<std::size_t>(txmp::TaskbarState::Desktop)));
		EnabledOption().IsEnabled(!batteryUnavailable);
		BatteryHint().Visibility(VisibleWhen(batteryUnavailable));
		Material().IsEnabled(canEditAppearance);
		BlurItem().IsEnabled(m_CanBlur);
		BlurHint().Visibility(VisibleWhen(!m_CanBlur));
		Picker().IsEnabled(canChangeColor);
		ColorHint().Visibility(VisibleWhen(!canChangeColor && canEditAppearance));
		Radius().IsEnabled(canChangeRadius);
		RadiusValue().Text(winrt::format(L"{:.1f}", draft.blurRadius));
		RadiusHint().Visibility(VisibleWhen(!canChangeRadius));
		Peek().IsEnabled(canEditAppearance && m_CanPeek);
		Line().IsEnabled(canEditAppearance && m_CanShowLine);
		PeekHint().Visibility(VisibleWhen(!m_CanPeek));
		LineHint().Visibility(VisibleWhen(!m_CanShowLine));
		PreviewHint().Visibility(VisibleWhen(m_HasTaskbar));
		SavingHint().Visibility(VisibleWhen(!m_CanSave));
		ApplyButton().IsEnabled(hasChanges && m_CanSave && !m_Applying);
		CancelButton().IsEnabled(hasChanges && !m_Applying);
		ResetButton().IsEnabled(!m_Applying);
	}

	void SettingsPage::UpdatePreview()
	{
		if (!m_Ready || m_Closing)
		{
			return;
		}

		const auto &appearance = m_Appearances.at(m_State);
		const auto &draft = appearance.draft;
		const bool shouldPreview = AppearanceSection().Visibility() == wux::Visibility::Visible &&
			m_HasTaskbar && draft.enabled && settings::ChangedFields(appearance.saved, draft) != 0 &&
			(m_CanBlur || draft.accent != static_cast<std::int32_t>(txmp::AccentState::Blur));

		if (shouldPreview)
		{
			m_PreviewActive = true;
			m_PreviewChanged(txmp::TaskbarAppearance(static_cast<txmp::AccentState>(draft.accent),
				Util::Color::FromRGBA(draft.color), draft.showPeek, draft.showLine, draft.blurRadius));
		}
		else if (m_PreviewActive)
		{
			m_PreviewActive = false;
			m_PreviewChanged(txmp::TaskbarAppearance(nullptr));
		}
	}

	void SettingsPage::AppearanceEdited()
	{
		EditStatus().Text(L"");
		RefreshControls();
		UpdatePreview();
	}

	void SettingsPage::MaterialChanged(const IInspectable &, const wuxc::SelectionChangedEventArgs &)
	{
		if (CanEdit() && Material().SelectedIndex() >= 0)
		{
			m_Appearances.at(m_State).draft.accent = Material().SelectedIndex();
			AppearanceEdited();
		}
	}

	void SettingsPage::OptionsChanged(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (CanEdit())
		{
			auto &draft = m_Appearances.at(m_State).draft;
			draft.enabled = EnabledOption().IsChecked().Value();
			draft.showPeek = Peek().IsChecked().Value();
			draft.showLine = Line().IsChecked().Value();
			AppearanceEdited();
		}
	}

	void SettingsPage::RadiusChanged(const IInspectable &, const wuxc::Primitives::RangeBaseValueChangedEventArgs &args)
	{
		if (CanEdit())
		{
			m_Appearances.at(m_State).draft.blurRadius = static_cast<float>(args.NewValue());
			AppearanceEdited();
		}
	}

	void SettingsPage::ColorChanged(const muxc::ColorPicker &, const muxc::ColorChangedEventArgs &args)
	{
		if (CanEdit())
		{
			m_Appearances.at(m_State).draft.color = Util::Color(args.NewColor()).ToRGBA();
			AppearanceEdited();
		}
	}

	void SettingsPage::ApplyClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		std::vector<SettingsEdit> edits;
		for (std::size_t i = 0; i < m_Appearances.size(); ++i)
		{
			const auto &appearance = m_Appearances.at(i);
			const auto &draft = appearance.draft;
			const auto fields = settings::ChangedFields(appearance.saved, draft);
			if (fields)
			{
				edits.push_back({ static_cast<txmp::TaskbarState>(i), fields, draft.accent, draft.color,
					draft.showPeek, draft.showLine, draft.blurRadius, draft.enabled });
			}
		}
		if (!edits.empty())
		{
			m_Applying = true;
			RefreshControls();
			m_ApplyRequested(single_threaded_vector(std::move(edits)).GetView());
		}
	}

	void SettingsPage::FinishApply(bool saved)
	{
		m_Applying = false;
		if (saved)
		{
			for (auto &appearance : m_Appearances)
			{
				appearance.draft = appearance.saved;
			}
		}
		EditStatus().Text(Text(saved ? L"Applied" : L"SaveFailed"));
	}

	void SettingsPage::CancelClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		for (auto &appearance : m_Appearances)
		{
			appearance.draft = appearance.saved;
		}
		LoadControls();
		UpdatePreview();
		EditStatus().Text(Text(L"Cancelled"));
	}

	fire_and_forget SettingsPage::ResetClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		const auto self = get_strong();
		m_ResetDialogOpen = true;
		ResetDialog().XamlRoot(XamlRoot());
		const auto result = co_await ResetDialog().ShowAsync();
		m_ResetDialogOpen = false;

		if (result == wuxc::ContentDialogResult::Primary)
		{
			for (auto &appearance : m_Appearances)
			{
				appearance.draft = appearance.defaults;
			}
			LoadControls();
			UpdatePreview();
			EditStatus().Text(Text(L"RestoredDraft"));
		}
	}

	void SettingsPage::SetProfiles(const wfc::IVectorView<ProfileInfo> &profiles, const hstring &active, bool canSave)
	{
		const auto previous = SelectedProfile();
		m_Profiles.assign(begin(profiles), end(profiles));
		m_ActiveProfile = active;
		m_CanSaveProfiles = canSave;

		m_Updating = true;
		const auto items = ProfileList().Items();
		items.Clear();
		int selectedIndex = -1;
		for (std::size_t i = 0; i < m_Profiles.size(); ++i)
		{
			const auto &profile = m_Profiles[i];
			std::wstring label { profile.Name };
			if (profile.BuiltIn)
			{
				label += L" \u00B7 ";
				label += Text(L"ProfileBuiltIn");
			}
			if (profile.Name == m_ActiveProfile)
			{
				label += L" \u00B7 ";
				label += Text(L"ProfileActive");
			}
			items.Append(box_value(hstring { label }));

			if (previous && previous->Name == profile.Name)
			{
				selectedIndex = static_cast<int>(i);
			}
		}
		ProfileList().SelectedIndex(selectedIndex);
		m_Updating = false;

		ActiveProfileText().Text(m_ActiveProfile.empty() ? Text(L"ProfileNoneActive") : winrt::format(L"{}: {}", Text(L"ProfileActiveLabel"), m_ActiveProfile));
		SavingHintProfiles();
		RefreshProfileButtons();
		UpdateActionProfiles();
	}

	void SettingsPage::SavingHintProfiles()
	{
		if (!m_CanSaveProfiles)
		{
			ProfileStatus().Text(Text(L"ProfilesSavingDisabled"));
		}
	}

	std::optional<ProfileInfo> SettingsPage::SelectedProfile()
	{
		const int index = ProfileList().SelectedIndex();
		if (index >= 0 && static_cast<std::size_t>(index) < m_Profiles.size())
		{
			return m_Profiles[static_cast<std::size_t>(index)];
		}
		return std::nullopt;
	}

	void SettingsPage::RefreshProfileButtons()
	{
		const auto selected = SelectedProfile();
		const bool idle = !m_ProfileBusy && m_CanSaveProfiles;
		ActivateProfileButton().IsEnabled(idle && selected.has_value());
		DuplicateProfileButton().IsEnabled(idle && selected.has_value());
		ExportProfileButton().IsEnabled(!m_ProfileBusy && selected.has_value());
		DeleteProfileButton().IsEnabled(idle && selected.has_value() && !selected->BuiltIn);
		ImportProfileButton().IsEnabled(idle);

		const std::wstring_view name = NewProfileName().Text();
		SaveProfileButton().IsEnabled(idle && !name.empty() && name.front() != L' ' && name.back() != L' ');
	}

	void SettingsPage::RequestProfileAction(std::wstring_view action, const hstring &name)
	{
		m_ProfileBusy = true;
		ProfileStatus().Text(Text(L"ProfileWorking"));
		RefreshProfileButtons();
		m_ProfileActionRequested(hstring { action }, name);
	}

	void SettingsPage::FinishProfileAction(bool success, const hstring &messageKey, const hstring &detail)
	{
		m_ProfileBusy = false;
		std::wstring message { Text(std::wstring(L"Profile") + std::wstring(messageKey)) };
		if (!detail.empty())
		{
			message += L" ";
			message += detail;
		}
		ProfileStatus().Text(message);
		if (success && messageKey == L"Saved")
		{
			NewProfileName().Text(L"");
		}
		RefreshProfileButtons();
	}

	void SettingsPage::ProfileSelectionChanged(const IInspectable &, const wuxc::SelectionChangedEventArgs &)
	{
		if (!m_Updating)
		{
			RefreshProfileButtons();
		}
	}

	void SettingsPage::NewProfileNameChanged(const IInspectable &, const wuxc::TextChangedEventArgs &)
	{
		RefreshProfileButtons();
	}

	void SettingsPage::ProfileButtonClicked(const IInspectable &sender, const wux::RoutedEventArgs &)
	{
		const auto action = unbox_value<hstring>(sender.as<wux::FrameworkElement>().Tag());
		if (action == L"save")
		{
			RequestProfileAction(action, NewProfileName().Text());
		}
		else if (action == L"import")
		{
			RequestProfileAction(action, { });
		}
		else if (const auto selected = SelectedProfile())
		{
			RequestProfileAction(action, selected->Name);
		}
	}

	fire_and_forget SettingsPage::DeleteProfileClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		const auto self = get_strong();
		const auto selected = SelectedProfile();
		if (!selected || selected->BuiltIn)
		{
			co_return;
		}

		m_DeleteDialogOpen = true;
		DeleteProfileDialog().XamlRoot(XamlRoot());
		DeleteProfileDialog().Content(box_value(winrt::format(L"{} {}", Text(L"DeleteProfileQuestion"), selected->Name)));
		const auto result = co_await DeleteProfileDialog().ShowAsync();
		m_DeleteDialogOpen = false;

		if (result == wuxc::ContentDialogResult::Primary)
		{
			RequestProfileAction(L"delete", selected->Name);
		}
	}

	hstring SettingsPage::SelectedTag(const wuxc::ComboBox &box)
	{
		if (const auto item = box.SelectedItem().try_as<wux::FrameworkElement>())
		{
			if (const auto tag = item.Tag())
			{
				return unbox_value<hstring>(tag);
			}
		}
		return { };
	}

	void SettingsPage::SetAutomation(const wfc::IVectorView<RuleInfo> &rules, const hstring &activeRule, const hstring &loadError, bool paused)
	{
		const int previous = RulesList().SelectedIndex();
		m_Rules.assign(begin(rules), end(rules));
		m_RulesEditable = loadError.empty();

		m_Updating = true;
		PauseAutomation().IsOn(paused);
		const auto items = RulesList().Items();
		items.Clear();
		for (const auto &rule : m_Rules)
		{
			items.Append(box_value(winrt::format(L"{}  [{} {}{}]\n{}", rule.Name, Text(L"RulePriorityShort"), rule.Priority,
				rule.Enabled ? L"" : std::wstring(L", ") + std::wstring(Text(L"RuleDisabled")), rule.Summary)));
		}
		if (previous >= 0 && static_cast<std::size_t>(previous) < m_Rules.size())
		{
			RulesList().SelectedIndex(previous);
		}
		m_Updating = false;

		std::wstring status;
		if (!loadError.empty())
		{
			status = winrt::format(L"{} {}", Text(L"RulesLoadError"), loadError);
		}
		else if (paused)
		{
			status = Text(L"AutomationPaused");
		}
		else if (!activeRule.empty())
		{
			status = winrt::format(L"{}: {}", Text(L"RuleActive"), activeRule);
		}
		else
		{
			status = winrt::format(L"{} ({})", Text(L"NoRuleActive"), m_Rules.size());
		}
		AutomationStatusText().Text(status);
		RefreshRuleButtons();
		RefreshRuleForm();
	}

	void SettingsPage::FinishAutomationAction(bool success, const hstring &messageKey, const hstring &detail)
	{
		m_AutomationBusy = false;
		std::wstring message { Text(std::wstring(L"Automation") + std::wstring(messageKey)) };
		if (!detail.empty())
		{
			message += L" ";
			message += detail;
		}
		AutomationStatus().Text(message);
		if (success && messageKey == L"Created")
		{
			RuleName().Text(L"");
			ConditionValue().Text(L"");
		}
		RefreshRuleButtons();
		RefreshRuleForm();
	}

	void SettingsPage::RefreshRuleButtons()
	{
		const int index = RulesList().SelectedIndex();
		const bool selected = index >= 0 && static_cast<std::size_t>(index) < m_Rules.size();
		const bool enabled = selected && m_RulesEditable && !m_AutomationBusy;
		ToggleRuleButton().IsEnabled(enabled);
		RaiseRuleButton().IsEnabled(enabled);
		LowerRuleButton().IsEnabled(enabled);
		DeleteRuleButton().IsEnabled(enabled);
		if (selected)
		{
			ToggleRuleButton().Content(box_value(Text(m_Rules[static_cast<std::size_t>(index)].Enabled ? L"DisableRule" : L"EnableRule")));
		}
	}

	void SettingsPage::RefreshRuleForm()
	{
		const auto condition = SelectedTag(ConditionKind());
		const bool needsValue = condition == L"foreground_app" || condition == L"app_running" || condition == L"battery_below" ||
			condition == L"time_range" || condition == L"weekdays" || condition == L"monitors_at_least";
		ConditionValue().Visibility(VisibleWhen(needsValue));
		ConditionHint().Text(needsValue ? Text(std::wstring(L"Hint_") + std::wstring(condition)) : hstring { });
		ConditionHint().Visibility(VisibleWhen(needsValue));

		const bool profileAction = SelectedTag(ActionKind()) == L"activate_profile";
		ActionProfile().Visibility(VisibleWhen(profileAction));

		const bool ready = m_RulesEditable && !m_AutomationBusy && !RuleName().Text().empty() &&
			(!needsValue || !ConditionValue().Text().empty()) && (!profileAction || ActionProfile().SelectedItem());
		CreateRuleButton().IsEnabled(ready);
		EditRulesButton().IsEnabled(!m_AutomationBusy);
		ReloadRulesButton().IsEnabled(!m_AutomationBusy);
	}

	void SettingsPage::UpdateActionProfiles()
	{
		const auto current = ActionProfile().SelectedItem();
		const hstring previous = current ? unbox_value_or<hstring>(current, { }) : hstring { };
		const auto items = ActionProfile().Items();
		items.Clear();
		int selected = m_Profiles.empty() ? -1 : 0;
		for (std::size_t i = 0; i < m_Profiles.size(); ++i)
		{
			items.Append(box_value(m_Profiles[i].Name));
			if (m_Profiles[i].Name == previous)
			{
				selected = static_cast<int>(i);
			}
		}
		ActionProfile().SelectedIndex(selected);
		RefreshRuleForm();
	}

	void SettingsPage::PauseAutomationToggled(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (!m_Updating)
		{
			m_AutomationBusy = true;
			RefreshRuleButtons();
			m_AutomationActionRequested(L"pause", -1, PauseAutomation().IsOn() ? 1 : 0);
		}
	}

	void SettingsPage::AutomationButtonClicked(const IInspectable &sender, const wux::RoutedEventArgs &)
	{
		m_AutomationActionRequested(unbox_value<hstring>(sender.as<wux::FrameworkElement>().Tag()), -1, 0);
	}

	void SettingsPage::RuleSelectionChanged(const IInspectable &, const wuxc::SelectionChangedEventArgs &)
	{
		if (!m_Updating)
		{
			RefreshRuleButtons();
		}
	}

	void SettingsPage::RuleButtonClicked(const IInspectable &sender, const wux::RoutedEventArgs &)
	{
		const int index = RulesList().SelectedIndex();
		if (index < 0 || static_cast<std::size_t>(index) >= m_Rules.size())
		{
			return;
		}

		const auto tag = unbox_value<hstring>(sender.as<wux::FrameworkElement>().Tag());
		m_AutomationBusy = true;
		RefreshRuleButtons();
		if (tag == L"raise")
		{
			m_AutomationActionRequested(L"priority", index, 10);
		}
		else if (tag == L"lower")
		{
			m_AutomationActionRequested(L"priority", index, -10);
		}
		else
		{
			m_AutomationActionRequested(tag, index, 0);
		}
	}

	void SettingsPage::RuleFormChanged(const IInspectable &, const wuxc::TextChangedEventArgs &)
	{
		RefreshRuleForm();
	}

	void SettingsPage::ConditionKindChanged(const IInspectable &, const wuxc::SelectionChangedEventArgs &)
	{
		if (m_Resources)
		{
			RefreshRuleForm();
		}
	}

	void SettingsPage::ActionKindChanged(const IInspectable &, const wuxc::SelectionChangedEventArgs &)
	{
		if (m_Resources)
		{
			RefreshRuleForm();
		}
	}

	void SettingsPage::CreateRuleClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		const auto priority = RulePriority().Value();
		NewRuleSpec spec {
			RuleName().Text(),
			std::isnan(priority) ? 0 : static_cast<std::int32_t>(std::clamp(priority, -1000.0, 1000.0)),
			SelectedTag(ConditionKind()),
			ConditionValue().Text(),
			unbox_value_or<bool>(NegateCondition().IsChecked(), false),
			SelectedTag(ActionKind()),
			ActionProfile().SelectedItem() ? unbox_value<hstring>(ActionProfile().SelectedItem()) : hstring { }
		};

		m_AutomationBusy = true;
		RefreshRuleForm();
		AutomationStatus().Text(Text(L"AutomationWorking"));
		m_CreateRuleRequested(spec);
	}

	void SettingsPage::SetAdaptiveColor(bool enabled, bool useSecondary, std::int32_t intensity, std::int32_t saturation, std::int32_t contrast, bool active, std::uint32_t color, const hstring &source, bool canSave)
	{
		m_Updating = true;
		AdaptiveEnabled().IsOn(enabled);
		AdaptiveSecondary().IsChecked(useSecondary);
		AdaptiveIntensity().Value(intensity);
		AdaptiveSaturation().Value(saturation);
		AdaptiveContrast().Value(contrast);
		m_Updating = false;

		for (const wux::Controls::Control control : { AdaptiveEnabled().as<wux::Controls::Control>(), AdaptiveSecondary().as<wux::Controls::Control>(),
			AdaptiveIntensity().as<wux::Controls::Control>(), AdaptiveSaturation().as<wux::Controls::Control>(),
			AdaptiveContrast().as<wux::Controls::Control>(), AdaptiveExclude().as<wux::Controls::Control>() })
		{
			control.IsEnabled(canSave);
		}

		if (active)
		{
			const auto c = Util::Color::FromRGBA(color);
			AdaptiveSwatch().Background(wux::Media::SolidColorBrush(winrt::Windows::UI::Color { c.A, c.R, c.G, c.B }));
			AdaptiveSwatch().Visibility(wux::Visibility::Visible);
			AdaptiveStatus().Text(winrt::format(L"{} {} ({})", Text(L"AdaptiveActive"), c.ToString(), source));
		}
		else
		{
			AdaptiveSwatch().Visibility(wux::Visibility::Collapsed);
			AdaptiveStatus().Text(!canSave ? Text(L"AdaptiveCannotSave") : enabled ? winrt::format(L"{} {}", Text(L"AdaptiveUnavailable"), source) : Text(L"AdaptiveOff"));
		}
	}

	void SettingsPage::SendAdaptive(bool excludeCurrent)
	{
		m_AdaptiveColorChanged(AdaptiveEnabled().IsOn(), unbox_value_or<bool>(AdaptiveSecondary().IsChecked(), false),
			static_cast<std::int32_t>(AdaptiveIntensity().Value()), static_cast<std::int32_t>(AdaptiveSaturation().Value()),
			static_cast<std::int32_t>(AdaptiveContrast().Value()), excludeCurrent);
	}

	void SettingsPage::AdaptiveToggled(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (!m_Updating)
		{
			SendAdaptive(false);
		}
	}

	void SettingsPage::AdaptiveOptionClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		if (!m_Updating)
		{
			SendAdaptive(false);
		}
	}

	void SettingsPage::AdaptiveSliderChanged(const IInspectable &, const wuxc::Primitives::RangeBaseValueChangedEventArgs &)
	{
		if (m_Updating)
		{
			return;
		}

		// Sliders fire continuously while dragging: send (and save) only after 400 ms of inactivity.
		if (!m_AdaptiveTimer)
		{
			m_AdaptiveTimer = wux::DispatcherTimer();
			m_AdaptiveTimer.Interval(std::chrono::milliseconds(400));
			m_AdaptiveTimer.Tick([weak = get_weak()](const IInspectable &, const IInspectable &)
			{
				if (const auto self = weak.get())
				{
					self->m_AdaptiveTimer.Stop();
					self->SendAdaptive(false);
				}
			});
		}
		m_AdaptiveTimer.Stop();
		m_AdaptiveTimer.Start();
	}

	void SettingsPage::AdaptiveExcludeClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		SendAdaptive(true);
	}

	void SettingsPage::SetHotkeys(const wfc::IVectorView<hstring> &bindings, const hstring &messageKey, const hstring &detail)
	{
		const std::array<wuxc::TextBox, 4> boxes { HotkeyOpenSettings(), HotkeyToggleEffects(), HotkeyNextProfile(), HotkeyToggleAutomation() };
		for (std::uint32_t i = 0; i < boxes.size() && i < bindings.Size(); ++i)
		{
			boxes[i].Text(bindings.GetAt(i));
		}
		if (!messageKey.empty())
		{
			HotkeysStatus().Text(winrt::format(L"{} {}", Text(std::wstring(L"Hotkeys") + std::wstring(messageKey)), detail));
		}
	}

	void SettingsPage::SaveHotkeysClicked(const IInspectable &, const wux::RoutedEventArgs &)
	{
		std::vector<hstring> bindings { HotkeyOpenSettings().Text(), HotkeyToggleEffects().Text(), HotkeyNextProfile().Text(), HotkeyToggleAutomation().Text() };
		m_HotkeysChanged(single_threaded_vector(std::move(bindings)).GetView());
	}

	void SettingsPage::SetMetrics(double cpuPercent, std::uint64_t privateBytes, std::uint64_t workingSet, std::uint32_t handles, std::uint32_t threads, std::uint32_t gdiObjects, std::uint32_t userObjects, std::uint64_t taskbarRefreshes, std::uint64_t ruleEvaluations, bool effectsPaused, bool safeMode)
	{
		const auto mb = [](std::uint64_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); };
		MetricsText().Text(winrt::format(
			L"{}: {:.2f} %\n{}: {:.1f} MB\n{}: {:.1f} MB\n{}: {}\n{}: {}\n{}: {} / {}\n{}: {}\n{}: {}\n{}: {}",
			Text(L"MetricCpu"), cpuPercent,
			Text(L"MetricPrivate"), mb(privateBytes),
			Text(L"MetricWorkingSet"), mb(workingSet),
			Text(L"MetricHandles"), handles,
			Text(L"MetricThreads"), threads,
			Text(L"MetricGui"), gdiObjects, userObjects,
			Text(L"MetricRefreshes"), taskbarRefreshes,
			Text(L"MetricEvaluations"), ruleEvaluations,
			Text(L"MetricEffects"), Text(safeMode ? L"MetricSafeMode" : effectsPaused ? L"MetricPaused" : L"MetricActive")));
	}

	void SettingsPage::SetDisplays(const wfc::IVectorView<DisplayInfo> &displays, bool canSave)
	{
		m_Updating = true;
		const auto children = DisplaysPanel().Children();
		children.Clear();
		for (const auto &display : displays)
		{
			wuxc::StackPanel row;
			row.Spacing(4);

			wuxc::TextBlock label;
			label.Text(display.Description);
			label.TextWrapping(wux::TextWrapping::Wrap);
			row.Children().Append(label);

			wuxc::ComboBox box;
			box.MinWidth(280);
			box.Tag(box_value(display.Device));
			box.IsEnabled(canSave);
			wux::Automation::AutomationProperties::SetName(box, display.Description);
			box.Items().Append(box_value(Text(L"DisplayGlobal")));
			int selected = 0;
			for (std::size_t i = 0; i < m_Profiles.size(); ++i)
			{
				box.Items().Append(box_value(m_Profiles[i].Name));
				if (m_Profiles[i].Name == display.Profile)
				{
					selected = static_cast<int>(i) + 1;
				}
			}
			box.SelectedIndex(selected);
			box.SelectionChanged([weak = get_weak()](const IInspectable &sender, const wuxc::SelectionChangedEventArgs &)
			{
				const auto self = weak.get();
				if (!self || self->m_Updating)
				{
					return;
				}
				const auto combo = sender.as<wuxc::ComboBox>();
				const int index = combo.SelectedIndex();
				const hstring profile = index > 0 && static_cast<std::size_t>(index - 1) < self->m_Profiles.size() ? self->m_Profiles[static_cast<std::size_t>(index - 1)].Name : hstring { };
				self->m_MonitorProfileChanged(unbox_value<hstring>(combo.Tag()), profile);
			});
			row.Children().Append(box);
			children.Append(row);
		}
		DisplaysStatus().Text(displays.Size() == 0 ? Text(L"DisplaysNone") : canSave ? hstring { } : Text(L"DisplaysCannotSave"));
		m_Updating = false;
	}
}
