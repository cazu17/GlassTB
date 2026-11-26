#pragma once
#include <cstdint>

namespace settings {
enum AppearanceField : std::uint32_t {
	AccentField = 1 << 0,
	ColorField = 1 << 1,
	PeekField = 1 << 2,
	LineField = 1 << 3,
	RadiusField = 1 << 4,
	EnabledField = 1 << 5,
	AllAppearanceFields = AccentField | ColorField | PeekField | LineField | RadiusField | EnabledField
};

struct AppearanceValues {
	std::int32_t accent = 0;
	std::uint32_t color = 0;
	bool showPeek = true;
	bool showLine = true;
	float blurRadius = 9.0f;
	bool enabled = true;
};

constexpr std::uint32_t ChangedFields(const AppearanceValues &saved, const AppearanceValues &draft) noexcept
{
	return (saved.accent != draft.accent ? AccentField : 0u)
		| (saved.color != draft.color ? ColorField : 0u)
		| (saved.showPeek != draft.showPeek ? PeekField : 0u)
		| (saved.showLine != draft.showLine ? LineField : 0u)
		| (saved.blurRadius != draft.blurRadius ? RadiusField : 0u)
		| (saved.enabled != draft.enabled ? EnabledField : 0u);
}

constexpr AppearanceValues MergeAppearance(const AppearanceValues &saved, const AppearanceValues &draft, std::uint32_t fields) noexcept
{
	return {
		fields & AccentField ? draft.accent : saved.accent,
		fields & ColorField ? draft.color : saved.color,
		fields & PeekField ? draft.showPeek : saved.showPeek,
		fields & LineField ? draft.showLine : saved.showLine,
		fields & RadiusField ? draft.blurRadius : saved.blurRadius,
		fields & EnabledField ? draft.enabled : saved.enabled
	};
}
}
