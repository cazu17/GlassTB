#include "pch.h"

#include "SettingsPage.h"
#if __has_include("Pages/SettingsPage.g.cpp")
#include "Pages/SettingsPage.g.cpp"
#endif

#include <algorithm>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
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
		HomeSection().Visibility(VisibleWhen(!appearance && !profiles));
		AppearanceSection().Visibility(VisibleWhen(appearance));
		ProfilesSection().Visibility(VisibleWhen(profiles));
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
}
