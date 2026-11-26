#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Parsing of user-configured global shortcuts such as "Ctrl+Alt+T" or "Win+Shift+F9".
// Values match the Win32 RegisterHotKey constants so the result can be passed as-is.
namespace hotkey {
	inline constexpr std::uint32_t MOD_ALT_ = 0x0001;
	inline constexpr std::uint32_t MOD_CONTROL_ = 0x0002;
	inline constexpr std::uint32_t MOD_SHIFT_ = 0x0004;
	inline constexpr std::uint32_t MOD_WIN_ = 0x0008;

	struct Hotkey {
		std::uint32_t modifiers = 0;
		std::uint32_t vk = 0;
		bool operator==(const Hotkey &) const = default;
	};

	namespace detail {
		inline std::wstring Upper(std::wstring_view text)
		{
			std::wstring out;
			for (wchar_t c : text)
			{
				if (c == L' ' || c == L'\t')
				{
					continue;
				}
				out += (c >= L'a' && c <= L'z') ? static_cast<wchar_t>(c - L'a' + L'A') : c;
			}
			return out;
		}

		inline std::optional<std::uint32_t> KeyCode(const std::wstring &key)
		{
			if (key.size() == 1)
			{
				const wchar_t c = key[0];
				if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9'))
				{
					return static_cast<std::uint32_t>(c);
				}
			}
			if (key.size() >= 2 && key.size() <= 3 && key[0] == L'F')
			{
				int n = 0;
				for (std::size_t i = 1; i < key.size(); ++i)
				{
					if (key[i] < L'0' || key[i] > L'9')
					{
						return std::nullopt;
					}
					n = n * 10 + (key[i] - L'0');
				}
				if (n >= 1 && n <= 24)
				{
					return 0x70u + static_cast<std::uint32_t>(n - 1); // VK_F1..VK_F24
				}
			}

			struct Named { std::wstring_view name; std::uint32_t vk; };
			static constexpr Named names[] = {
				{ L"SPACE", 0x20 }, { L"ENTER", 0x0D }, { L"TAB", 0x09 }, { L"ESC", 0x1B }, { L"ESCAPE", 0x1B },
				{ L"BACKSPACE", 0x08 }, { L"DELETE", 0x2E }, { L"DEL", 0x2E }, { L"INSERT", 0x2D }, { L"INS", 0x2D },
				{ L"HOME", 0x24 }, { L"END", 0x23 }, { L"PAGEUP", 0x21 }, { L"PAGEDOWN", 0x22 },
				{ L"LEFT", 0x25 }, { L"UP", 0x26 }, { L"RIGHT", 0x27 }, { L"DOWN", 0x28 },
				{ L"PAUSE", 0x13 }, { L"PRINTSCREEN", 0x2C },
			};
			for (const auto &n : names)
			{
				if (key == n.name)
				{
					return n.vk;
				}
			}
			return std::nullopt;
		}
	}

	// Returns std::nullopt for invalid text. At least one modifier is required so a single
	// key is never stolen from every application.
	inline std::optional<Hotkey> Parse(std::wstring_view text)
	{
		const std::wstring upper = detail::Upper(text);
		if (upper.empty())
		{
			return std::nullopt;
		}

		Hotkey result;
		std::size_t start = 0;
		std::optional<std::uint32_t> key;
		while (start <= upper.size())
		{
			const auto end = upper.find(L'+', start);
			const std::wstring part = upper.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
			if (part.empty())
			{
				return std::nullopt;
			}

			std::uint32_t mod = 0;
			if (part == L"CTRL" || part == L"CONTROL") mod = MOD_CONTROL_;
			else if (part == L"ALT") mod = MOD_ALT_;
			else if (part == L"SHIFT") mod = MOD_SHIFT_;
			else if (part == L"WIN" || part == L"WINDOWS") mod = MOD_WIN_;

			if (mod)
			{
				if (key || (result.modifiers & mod))
				{
					return std::nullopt; // modifier after the key, or repeated
				}
				result.modifiers |= mod;
			}
			else
			{
				if (key)
				{
					return std::nullopt; // two non-modifier keys
				}
				key = detail::KeyCode(part);
				if (!key)
				{
					return std::nullopt;
				}
			}

			if (end == std::wstring::npos)
			{
				break;
			}
			start = end + 1;
		}

		if (!key || result.modifiers == 0)
		{
			return std::nullopt;
		}
		result.vk = *key;
		return result;
	}
}
