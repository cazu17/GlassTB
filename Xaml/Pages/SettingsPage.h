#pragma once
#include "../event.h"
#include "../factory.h"
#include "../../Common/settings/appearanceedit.hpp"
#include "winrt.hpp"
#include <array>
#include <optional>
#include <vector>

#include "FramelessPage.h"
#include "Pages/SettingsPage.g.h"

namespace winrt::TranslucentTB::Xaml::Pages::implementation
{
	struct SettingsPage : wux::Markup::ComponentConnectorT<SettingsPageT<SettingsPage>>
	{
		SettingsPage() = default;
		void InitializeComponent();
		bool Close() override;

		void SetDiagnostics(const hstring &windowsVersion, const hstring &nativeArchitecture, const hstring &processArchitecture,
			const hstring &taskbarType, const hstring &backend, std::uint32_t monitorCount, bool blurAccentState,
			bool xamlTaskbarService, bool canPeek, bool canShowLine);
		void SetAppearance(txmp::TaskbarState state, const txmp::TaskbarAppearance &saved, const txmp::TaskbarAppearance &defaults);
		void UpdateEditor(std::uint32_t ruleCount, bool canSave, bool hasBattery);
		void FinishApply(bool saved);
		void SetProfiles(const wfc::IVectorView<ProfileInfo> &profiles, const hstring &active, bool canSave);
		void FinishProfileAction(bool success, const hstring &messageKey, const hstring &detail);
		void SetAutomation(const wfc::IVectorView<RuleInfo> &rules, const hstring &activeRule, const hstring &loadError, bool paused);
		void FinishAutomationAction(bool success, const hstring &messageKey, const hstring &detail);
		void SetAdaptiveColor(bool enabled, bool useSecondary, std::int32_t intensity, std::int32_t saturation, std::int32_t contrast, bool active, std::uint32_t color, const hstring &source, bool canSave);

		DECL_EVENT(SettingsPreviewChangedDelegate, PreviewChanged, m_PreviewChanged);
		DECL_EVENT(SettingsApplyRequestedDelegate, ApplyRequested, m_ApplyRequested);
		DECL_EVENT(ProfileActionRequestedDelegate, ProfileActionRequested, m_ProfileActionRequested);
		DECL_EVENT(AutomationActionRequestedDelegate, AutomationActionRequested, m_AutomationActionRequested);
		DECL_EVENT(CreateRuleRequestedDelegate, CreateRuleRequested, m_CreateRuleRequested);
		DECL_EVENT(AdaptiveColorChangedDelegate, AdaptiveColorChanged, m_AdaptiveColorChanged);
		DECL_EVENT(HotkeysChangedDelegate, HotkeysChanged, m_HotkeysChanged);
		DECL_EVENT(MetricsRequestedDelegate, MetricsRequested, m_MetricsRequested);
		DECL_EVENT(MonitorProfileChangedDelegate, MonitorProfileChanged, m_MonitorProfileChanged);
		void SetDisplays(const wfc::IVectorView<DisplayInfo> &displays, bool canSave);
		void SetMetrics(double cpuPercent, std::uint64_t privateBytes, std::uint64_t workingSet, std::uint32_t handles, std::uint32_t threads, std::uint32_t gdiObjects, std::uint32_t userObjects, std::uint64_t taskbarRefreshes, std::uint64_t ruleEvaluations, bool effectsPaused, bool safeMode);
		void SetHotkeys(const wfc::IVectorView<hstring> &bindings, const hstring &messageKey, const hstring &detail);
		void SaveHotkeysClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);

		void NavigationChanged(const muxc::NavigationView &sender, const muxc::NavigationViewSelectionChangedEventArgs &args);
		void StateChanged(const IInspectable &sender, const wuxc::SelectionChangedEventArgs &args);
		void MaterialChanged(const IInspectable &sender, const wuxc::SelectionChangedEventArgs &args);
		void OptionsChanged(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void RadiusChanged(const IInspectable &sender, const wuxc::Primitives::RangeBaseValueChangedEventArgs &args);
		void ColorChanged(const muxc::ColorPicker &sender, const muxc::ColorChangedEventArgs &args);
		void CopyClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void ApplyClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void CancelClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		fire_and_forget ResetClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void ProfileSelectionChanged(const IInspectable &sender, const wuxc::SelectionChangedEventArgs &args);
		void ProfileButtonClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		fire_and_forget DeleteProfileClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void NewProfileNameChanged(const IInspectable &sender, const wuxc::TextChangedEventArgs &args);
		void PauseAutomationToggled(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void AutomationButtonClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void RuleSelectionChanged(const IInspectable &sender, const wuxc::SelectionChangedEventArgs &args);
		void RuleButtonClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void RuleFormChanged(const IInspectable &sender, const wuxc::TextChangedEventArgs &args);
		void ConditionKindChanged(const IInspectable &sender, const wuxc::SelectionChangedEventArgs &args);
		void ActionKindChanged(const IInspectable &sender, const wuxc::SelectionChangedEventArgs &args);
		void CreateRuleClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void AdaptiveToggled(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void AdaptiveOptionClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);
		void AdaptiveSliderChanged(const IInspectable &sender, const wuxc::Primitives::RangeBaseValueChangedEventArgs &args);
		void AdaptiveExcludeClicked(const IInspectable &sender, const wux::RoutedEventArgs &args);

	private:
		struct Appearance {
			settings::AppearanceValues saved, draft, defaults;
		};

		std::array<Appearance, 7> m_Appearances{};
		wam::Resources::ResourceLoader m_Resources{ nullptr };
		std::size_t m_State = 0;
		bool m_Ready = false;
		bool m_Updating = false;
		bool m_Closing = false;
		bool m_Applying = false;
		bool m_ResetDialogOpen = false;
		bool m_PreviewActive = false;
		bool m_CanSave = false;
		bool m_HasBattery = true;
		bool m_HasTaskbar = false;
		bool m_CanBlur = false;
		bool m_CanChangeRadius = false;
		bool m_CanPeek = false;
		bool m_CanShowLine = false;
		std::vector<ProfileInfo> m_Profiles;
		hstring m_ActiveProfile;
		bool m_ProfileBusy = false;
		bool m_CanSaveProfiles = false;
		bool m_DeleteDialogOpen = false;
		std::vector<RuleInfo> m_Rules;
		bool m_RulesEditable = false;
		bool m_AutomationBusy = false;
		wux::DispatcherTimer m_AdaptiveTimer{ nullptr };
		wux::DispatcherTimer m_MetricsTimer{ nullptr };
		void SendAdaptive(bool excludeCurrent);

		static hstring SelectedTag(const wuxc::ComboBox &box);
		void RefreshRuleButtons();
		void RefreshRuleForm();
		void UpdateActionProfiles();

		std::optional<ProfileInfo> SelectedProfile();
		void RefreshProfileButtons();
		void SavingHintProfiles();
		void RequestProfileAction(std::wstring_view action, const hstring &name);

		hstring Text(std::wstring_view key) const;
		bool CanEdit() const noexcept;
		void LoadControls();
		void RefreshControls();
		void UpdatePreview();
		void AppearanceEdited();
	};
}

FACTORY(winrt::TranslucentTB::Xaml::Pages, SettingsPage);
