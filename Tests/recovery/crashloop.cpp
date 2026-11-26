#include <gtest/gtest.h>

#include "recovery/crashloop.hpp"

TEST(CrashLoop, CleanPreviousExitResetsHistory)
{
	const auto d = recovery::Evaluate({ 100, 200, 300 }, false, 400);
	EXPECT_FALSE(d.safeMode);
	EXPECT_TRUE(d.history.empty());
}

TEST(CrashLoop, ThreeRecentUncleanExitsTriggerSafeMode)
{
	const std::int64_t now = 10'000;
	EXPECT_FALSE(recovery::Evaluate({ now - 50, now - 20 }, true, now).safeMode);
	const auto d = recovery::Evaluate({ now - 90, now - 50, now - 20 }, true, now);
	EXPECT_TRUE(d.safeMode);
	EXPECT_TRUE(d.history.empty());
}

TEST(CrashLoop, OldExitsAreForgotten)
{
	const std::int64_t now = 100'000;
	const auto d = recovery::Evaluate({ now - 5000, now - 4000, now - 30 }, true, now);
	EXPECT_FALSE(d.safeMode);
	ASSERT_EQ(d.history.size(), 1u);
	EXPECT_EQ(d.history[0], now - 30);
}

TEST(CrashLoop, MarkerRoundTripIgnoresGarbage)
{
	EXPECT_EQ(recovery::ParseMarker(recovery::FormatMarker({ 1, 22, 333 })), (std::vector<std::int64_t> { 1, 22, 333 }));
	EXPECT_EQ(recovery::ParseMarker("12\r\nabc\n-5\n\n7"), (std::vector<std::int64_t> { 12, 7 }));
	EXPECT_TRUE(recovery::ParseMarker("").empty());
}
