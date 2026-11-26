#pragma once
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "rapidjsonhelper.hpp"

// Settings for the color extracted from the desktop wallpaper.
struct AdaptiveColorSettings {
	bool Enabled = false;
	bool UseSecondary = false;
	std::uint8_t Intensity = 160;     // alpha of the resulting tint
	std::uint8_t Saturation = 100;    // percent, 0..200
	std::uint8_t Contrast = 50;       // percent, 0..100
	std::vector<std::wstring> Excluded; // wallpaper file names (lowercase) that never drive the color

	template<class Writer>
	void Serialize(Writer &writer) const
	{
		rjh::Serialize(writer, Enabled, ENABLED_KEY);
		rjh::Serialize(writer, UseSecondary, SECONDARY_KEY);
		rjh::WriteKey(writer, INTENSITY_KEY);
		writer.Uint(Intensity);
		rjh::WriteKey(writer, SATURATION_KEY);
		writer.Uint(Saturation);
		rjh::WriteKey(writer, CONTRAST_KEY);
		writer.Uint(Contrast);
		rjh::WriteKey(writer, EXCLUDED_KEY);
		writer.StartArray();
		for (const auto &name : Excluded)
		{
			rjh::WriteString(writer, name);
		}
		writer.EndArray();
	}

	void Deserialize(const rjh::value_t &obj, void (*unknownKeyCallback)(std::wstring_view))
	{
		rjh::EnsureType(rj::Type::kObjectType, obj.GetType(), L"adaptive_color");
		for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it)
		{
			rjh::EnsureType(rj::Type::kStringType, it->name.GetType(), L"member name");
			const auto key = rjh::ValueToStringView(it->name);
			if (key == ENABLED_KEY) rjh::Deserialize(it->value, Enabled, key);
			else if (key == SECONDARY_KEY) rjh::Deserialize(it->value, UseSecondary, key);
			else if (key == INTENSITY_KEY) Intensity = ReadByte(it->value, key, 255);
			else if (key == SATURATION_KEY) Saturation = ReadByte(it->value, key, 200);
			else if (key == CONTRAST_KEY) Contrast = ReadByte(it->value, key, 100);
			else if (key == EXCLUDED_KEY)
			{
				rjh::EnsureType(rj::Type::kArrayType, it->value.GetType(), key);
				Excluded.clear();
				for (const auto &item : it->value.GetArray())
				{
					rjh::EnsureType(rj::Type::kStringType, item.GetType(), key);
					Excluded.emplace_back(rjh::ValueToStringView(item));
				}
			}
			else if (unknownKeyCallback)
			{
				unknownKeyCallback(key);
			}
		}
	}

private:
	static std::uint8_t ReadByte(const rjh::value_t &value, std::wstring_view key, unsigned max)
	{
		if (!value.IsUint() || value.GetUint() > max)
		{
			throw rjh::DeserializationError { std::format(L"{} must be an integer between 0 and {}", key, max) };
		}
		return static_cast<std::uint8_t>(value.GetUint());
	}

	static constexpr std::wstring_view ENABLED_KEY = L"enabled";
	static constexpr std::wstring_view SECONDARY_KEY = L"use_secondary";
	static constexpr std::wstring_view INTENSITY_KEY = L"intensity";
	static constexpr std::wstring_view SATURATION_KEY = L"saturation";
	static constexpr std::wstring_view CONTRAST_KEY = L"contrast";
	static constexpr std::wstring_view EXCLUDED_KEY = L"excluded_wallpapers";
};
