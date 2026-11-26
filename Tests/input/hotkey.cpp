#include <gtest/gtest.h>

#include "input/hotkey.hpp"

using hotkey::Parse;

TEST(Hotkey, ParsesCommonShortcuts)
{
	EXPECT_EQ(Parse(L"Ctrl+Alt+T"), (hotkey::Hotkey { hotkey::MOD_CONTROL_ | hotkey::MOD_ALT_, L'T' }));
	EXPECT_EQ(Parse(L" win + shift + f9 "), (hotkey::Hotkey { hotkey::MOD_WIN_ | hotkey::MOD_SHIFT_, 0x78 }));
	EXPECT_EQ(Parse(L"Control+Alt+Home"), (hotkey::Hotkey { hotkey::MOD_CONTROL_ | hotkey::MOD_ALT_, 0x24 }));
	EXPECT_EQ(Parse(L"Ctrl+Shift+1"), (hotkey::Hotkey { hotkey::MOD_CONTROL_ | hotkey::MOD_SHIFT_, L'1' }));
}

TEST(Hotkey, RejectsInvalidShortcuts)
{
	for (const wchar_t *bad : { L"", L"T", L"Ctrl", L"Ctrl+", L"+T", L"Ctrl+Ctrl+T", L"Ctrl+T+Alt", L"Ctrl+T+U", L"Ctrl+F25", L"Ctrl+F0", L"Ctrl+Ñ", L"Alt++T" })
	{
		EXPECT_FALSE(Parse(bad).has_value()) << bad;
	}
}
