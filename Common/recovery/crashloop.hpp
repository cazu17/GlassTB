#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Crash-loop detection. A marker file records the start time of every run that did
// not exit cleanly; when too many unclean exits happen within a short window, the
// next start uses safe mode (no Explorer injection, no taskbar changes).
namespace recovery {
	static constexpr std::size_t UNCLEAN_EXITS_FOR_SAFE_MODE = 3;
	static constexpr std::int64_t WINDOW_SECONDS = 10 * 60;

	struct Decision {
		bool safeMode;
		std::vector<std::int64_t> history; // unclean start times to keep (seconds since epoch)
	};

	// previous: start times of earlier runs that did not exit cleanly (oldest first).
	// previousRunUnclean: whether the marker of the last run was still present at startup.
	inline Decision Evaluate(std::vector<std::int64_t> previous, bool previousRunUnclean, std::int64_t now)
	{
		if (!previousRunUnclean)
		{
			// The last run exited cleanly: the loop (if any) is over.
			return { false, { } };
		}

		std::erase_if(previous, [now](std::int64_t t) { return t > now || now - t > WINDOW_SECONDS; });
		const bool safeMode = previous.size() >= UNCLEAN_EXITS_FOR_SAFE_MODE;
		if (safeMode)
		{
			// Start over so that a later normal start is not immediately sent back to safe mode.
			previous.clear();
		}
		return { safeMode, std::move(previous) };
	}

	// Marker format: one decimal timestamp per line. Invalid lines are ignored.
	inline std::vector<std::int64_t> ParseMarker(std::string_view text)
	{
		std::vector<std::int64_t> times;
		std::size_t start = 0;
		while (start < text.size())
		{
			auto end = text.find('\n', start);
			if (end == std::string_view::npos)
			{
				end = text.size();
			}
			auto line = text.substr(start, end - start);
			if (!line.empty() && line.back() == '\r')
			{
				line.remove_suffix(1);
			}
			if (!line.empty() && line.size() <= 19 && std::ranges::all_of(line, [](char c) { return c >= '0' && c <= '9'; }))
			{
				std::int64_t value = 0;
				for (const char c : line)
				{
					value = value * 10 + (c - '0');
				}
				times.push_back(value);
			}
			start = end + 1;
		}
		if (times.size() > 32)
		{
			times.erase(times.begin(), times.end() - 32);
		}
		return times;
	}

	inline std::string FormatMarker(const std::vector<std::int64_t> &times)
	{
		std::string text;
		for (const auto t : times)
		{
			text += std::to_string(t);
			text += '\n';
		}
		return text;
	}
}
