#pragma once
#include <array>
#include <format>
#include <string>
#include <string_view>

#include "rapidjsonhelper.hpp"
#include "../input/hotkey.hpp"

// Global shortcuts. Empty strings mean "not assigned" (the default, to avoid stealing
// shortcuts other applications use). Invalid strings are rejected when loading.
struct HotkeySettings {
	enum Action : std::size_t { OpenSettings, ToggleEffects, NextProfile, ToggleAutomation, Count };

	static constexpr std::array<std::wstring_view, Count> KEYS = {
		L"open_settings", L"toggle_effects", L"next_profile", L"toggle_automation"
	};

	std::array<std::wstring, Count> Bindings;

	template<class Writer>
	void Serialize(Writer &writer) const
	{
		for (std::size_t i = 0; i < Count; ++i)
		{
			rjh::Serialize(writer, std::wstring_view { Bindings[i] }, KEYS[i]);
		}
	}

	void Deserialize(const rjh::value_t &obj, void (*unknownKeyCallback)(std::wstring_view))
	{
		rjh::EnsureType(rj::Type::kObjectType, obj.GetType(), L"hotkeys");
		for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it)
		{
			rjh::EnsureType(rj::Type::kStringType, it->name.GetType(), L"member name");
			const auto key = rjh::ValueToStringView(it->name);
			bool known = false;
			for (std::size_t i = 0; i < Count; ++i)
			{
				if (key == KEYS[i])
				{
					rjh::EnsureType(rj::Type::kStringType, it->value.GetType(), key);
					const auto value = rjh::ValueToStringView(it->value);
					if (!value.empty() && !hotkey::Parse(value))
					{
						throw rjh::DeserializationError { std::format(L"Invalid shortcut \"{}\" for {}", value, key) };
					}
					Bindings[i] = value;
					known = true;
				}
			}
			if (!known && unknownKeyCallback)
			{
				unknownKeyCallback(key);
			}
		}
	}
};
