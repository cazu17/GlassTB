#pragma once
#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

// Pure helpers for the backups/ folder. File names embed a sortable timestamp,
// so lexicographic order equals chronological order.
namespace backups {
	struct Timestamp {
		std::uint16_t year, month, day, hour, minute, second;
	};

	// e.g. TimestampedName(L"config", ts) -> L"config-20261126-051530.json"
	inline std::wstring TimestampedName(std::wstring_view prefix, const Timestamp &ts)
	{
		return std::format(L"{}-{:04}{:02}{:02}-{:02}{:02}{:02}.json", prefix, ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second);
	}

	// True when name looks like "<prefix>-YYYYMMDD-HHMMSS.json" (the exact shape TimestampedName produces).
	inline bool MatchesPrefix(std::wstring_view name, std::wstring_view prefix) noexcept
	{
		// prefix + '-' + 8 digits + '-' + 6 digits + ".json"
		const std::size_t expected = prefix.size() + 1 + 8 + 1 + 6 + 5;
		if (name.size() != expected || !name.starts_with(prefix) || name[prefix.size()] != L'-' || !name.ends_with(L".json"))
		{
			return false;
		}

		const auto digits = [&name](std::size_t pos, std::size_t count)
		{
			return std::all_of(name.begin() + pos, name.begin() + pos + count, [](wchar_t c) { return c >= L'0' && c <= L'9'; });
		};
		const std::size_t start = prefix.size() + 1;
		return digits(start, 8) && name[start + 8] == L'-' && digits(start + 9, 6);
	}

	// Given the file names present in the folder, returns the ones with this prefix
	// that should be deleted so that only the newest `keep` remain.
	inline std::vector<std::wstring> SelectForDeletion(const std::vector<std::wstring> &names, std::wstring_view prefix, std::size_t keep)
	{
		std::vector<std::wstring> matching;
		for (const auto &name : names)
		{
			if (MatchesPrefix(name, prefix))
			{
				matching.push_back(name);
			}
		}

		std::ranges::sort(matching);
		if (matching.size() <= keep)
		{
			return { };
		}

		matching.resize(matching.size() - keep);
		return matching;
	}

	// True when the newest backup with this prefix was made on the same calendar day.
	inline bool HasBackupForDay(const std::vector<std::wstring> &names, std::wstring_view prefix, const Timestamp &ts)
	{
		const std::wstring day = std::format(L"{}-{:04}{:02}{:02}-", prefix, ts.year, ts.month, ts.day);
		return std::ranges::any_of(names, [&](const std::wstring &name)
		{
			return MatchesPrefix(name, prefix) && name.starts_with(day);
		});
	}
}
