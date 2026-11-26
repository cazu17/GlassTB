#pragma once
#include "../event.h"
#include "../factory.h"
#include "../../Common/settings/appearanceedit.hpp"
#include "winrt.hpp"
#include <array>

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

		DECL_EVENT(SettingsPreviewChangedDelegate, PreviewChanged, m_PreviewChanged);
		DECL_EVENT(SettingsApplyRequestedDelegate, ApplyRequested, m_ApplyRequested);

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

		hstring Text(std::wstring_view key) const;
		bool CanEdit() const noexcept;
		void LoadControls();
		void RefreshControls();
		void UpdatePreview();
		void AppearanceEdited();
	};
}

FACTORY(winrt::TranslucentTB::Xaml::Pages, SettingsPage);
