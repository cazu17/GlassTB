#include <gtest/gtest.h>

#include "config/backups.hpp"

TEST(Backups, TimestampedNameIsSortable)
{
	EXPECT_EQ(backups::TimestampedName(L"config", { 2026, 1, 2, 3, 4, 5 }), L"config-20260102-030405.json");
}

TEST(Backups, MatchesOnlyExactPrefixShape)
{
	EXPECT_TRUE(backups::MatchesPrefix(L"config-20261126-051530.json", L"config"));
	EXPECT_FALSE(backups::MatchesPrefix(L"config-invalid-20261126-051530.json", L"config"));
	EXPECT_TRUE(backups::MatchesPrefix(L"config-invalid-20261126-051530.json", L"config-invalid"));
	EXPECT_FALSE(backups::MatchesPrefix(L"config-2026112-051530.json", L"config"));
	EXPECT_FALSE(backups::MatchesPrefix(L"config-20261126-051530.txt", L"config"));
	EXPECT_FALSE(backups::MatchesPrefix(L"config-2026112a-051530.json", L"config"));
}

TEST(Backups, SelectForDeletionKeepsNewest)
{
	const std::vector<std::wstring> names = {
		L"config-20261103-000000.json",
		L"config-20261101-000000.json",
		L"config-invalid-20261101-000000.json",
		L"config-20261102-000000.json",
		L"readme.txt"
	};

	const auto toDelete = backups::SelectForDeletion(names, L"config", 2);
	ASSERT_EQ(toDelete.size(), 1u);
	EXPECT_EQ(toDelete[0], L"config-20261101-000000.json");
	EXPECT_TRUE(backups::SelectForDeletion(names, L"config", 3).empty());
}

TEST(Backups, HasBackupForDay)
{
	const std::vector<std::wstring> names = { L"config-20261126-010203.json" };
	EXPECT_TRUE(backups::HasBackupForDay(names, L"config", { 2026, 11, 26, 23, 0, 0 }));
	EXPECT_FALSE(backups::HasBackupForDay(names, L"config", { 2026, 11, 27, 0, 0, 0 }));
}
