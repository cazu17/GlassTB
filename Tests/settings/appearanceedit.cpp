#include <gtest/gtest.h>

#include "../../Common/settings/appearanceedit.hpp"

using namespace settings;

TEST(SettingsAppearance, ChangeMaskTracksAllEditableFields)
{
	const AppearanceValues saved{};
	EXPECT_EQ(ChangedFields(saved, saved), 0u);
	EXPECT_EQ(ChangedFields(saved, { 3, 0xff00ffff, false, false, 18.0f, false }), AllAppearanceFields);
}

TEST(SettingsAppearance, ExternalChangesPreserveOnlyEditedFields)
{
	const AppearanceValues saved{ 2, 0xff0000ff, true, true, 9.0f, true };
	auto draft = saved;
	draft.color = 0x00ff00ff;
	auto external = saved;
	external.accent = 4;
	external.blurRadius = 12.0f;
	external.enabled = false;

	const auto result = MergeAppearance(external, draft, ChangedFields(saved, draft));
	EXPECT_EQ(result.color, draft.color);
	EXPECT_EQ(result.accent, external.accent);
	EXPECT_EQ(result.blurRadius, external.blurRadius);
	EXPECT_FALSE(result.enabled);
}

TEST(SettingsAppearance, ApplyUsesTheLatestSavedValueForUntouchedFields)
{
	const AppearanceValues latest{ 4, 0x12345678, false, false, 21.0f, false };
	const AppearanceValues edit{ 3, 0, true, true, 6.0f, true };
	const auto result = MergeAppearance(latest, edit, AccentField | RadiusField);

	EXPECT_EQ(result.accent, 3);
	EXPECT_EQ(result.blurRadius, 6.0f);
	EXPECT_EQ(result.color, latest.color);
	EXPECT_FALSE(result.showPeek);
	EXPECT_FALSE(result.showLine);
	EXPECT_FALSE(result.enabled);
}

TEST(SettingsAppearance, RevertingAFieldRemovesItFromTheEdit)
{
	const AppearanceValues saved{};
	auto draft = saved;
	draft.showLine = false;
	EXPECT_EQ(ChangedFields(saved, draft), LineField);
	draft.showLine = saved.showLine;
	EXPECT_EQ(ChangedFields(saved, draft), 0u);
}
