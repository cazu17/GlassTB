#include <gtest/gtest.h>

#include "color/palette.hpp"

using Util::Color;

TEST(Palette, HslRoundTrip)
{
	for (const Color c : { Color(255, 0, 0), Color(0, 128, 255), Color(12, 200, 80), Color(128, 128, 128), Color(0, 0, 0), Color(255, 255, 255) })
	{
		const auto back = palette::FromHsl(palette::ToHsl(c));
		EXPECT_NEAR(back.R, c.R, 1);
		EXPECT_NEAR(back.G, c.G, 1);
		EXPECT_NEAR(back.B, c.B, 1);
	}
}

TEST(Palette, EmptyOrTransparentImageHasNoResult)
{
	EXPECT_FALSE(palette::Extract({ }).has_value());
	const std::vector<Color> transparent(16, Color(255, 0, 0, 0));
	EXPECT_FALSE(palette::Extract(transparent).has_value());
}

TEST(Palette, ColorfulAreaBeatsLargerGrayArea)
{
	std::vector<Color> pixels(700, Color(120, 120, 120));
	pixels.insert(pixels.end(), 300, Color(20, 90, 220));
	const auto result = palette::Extract(pixels);
	ASSERT_TRUE(result.has_value());
	EXPECT_GT(result->dominant.B, 180);
	EXPECT_LT(result->dominant.R, 60);
}

TEST(Palette, SecondaryHasDifferentHue)
{
	std::vector<Color> pixels(600, Color(220, 40, 40));
	pixels.insert(pixels.end(), 400, Color(40, 200, 60));
	const auto result = palette::Extract(pixels);
	ASSERT_TRUE(result.has_value());
	const auto d = palette::ToHsl(result->dominant);
	const auto s = palette::ToHsl(result->secondary);
	EXPECT_GE(palette::HueDistance(d.h, s.h), 40.0f);
}

TEST(Palette, ThemeAdaptationKeepsReadableLightness)
{
	const palette::Tuning tuning { 1.0f, 0.0f, 200 };
	const auto dark = palette::ForTheme(Color(250, 240, 200), true, tuning);
	EXPECT_LE(palette::ToHsl(dark).l, 0.46f);
	EXPECT_EQ(dark.A, 200);

	const auto light = palette::ForTheme(Color(10, 20, 40), false, tuning);
	EXPECT_GE(palette::ToHsl(light).l, 0.59f);
}
