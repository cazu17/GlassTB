#pragma once
#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <string>
#include <string_view>
#include <vector>

#include "config.hpp"
#include "optionaltaskbarappearance.hpp"
#include "rapidjsonhelper.hpp"
#include "taskbarappearance.hpp"

// A profile is a named set of taskbar appearances (one per state). It does not
// carry window rules, ignored windows or advanced settings: applying a profile
// only changes appearance fields and keeps everything else in the config.
struct Profile {
	static constexpr std::uint32_t SCHEMA_VERSION = 1;
	static constexpr std::size_t MAX_NAME_LENGTH = 64;

	std::wstring Name;
	TaskbarAppearance Desktop;
	OptionalTaskbarAppearance VisibleWindow;
	OptionalTaskbarAppearance MaximisedWindow;
	OptionalTaskbarAppearance StartOpened;
	OptionalTaskbarAppearance SearchOpened;
	OptionalTaskbarAppearance TaskViewOpened;
	OptionalTaskbarAppearance BatterySaver;

	static Profile FromConfig(const Config &config, std::wstring name)
	{
		Profile p;
		p.Name = std::move(name);
		p.Desktop = config.DesktopAppearance;
		p.VisibleWindow = config.VisibleWindowAppearance;
		p.MaximisedWindow = config.MaximisedWindowAppearance;
		p.StartOpened = config.StartOpenedAppearance;
		p.SearchOpened = config.SearchOpenedAppearance;
		p.TaskViewOpened = config.TaskViewOpenedAppearance;
		p.BatterySaver = config.BatterySaverAppearance;
		return p;
	}

	// Copies the appearance fields into the config. Slicing assignment is intended:
	// it keeps the window rules stored in the RuledTaskbarAppearance part.
	void ApplyTo(Config &config) const
	{
		config.DesktopAppearance = Desktop;
		static_cast<OptionalTaskbarAppearance &>(config.VisibleWindowAppearance) = VisibleWindow;
		static_cast<OptionalTaskbarAppearance &>(config.MaximisedWindowAppearance) = MaximisedWindow;
		config.StartOpenedAppearance = StartOpened;
		config.SearchOpenedAppearance = SearchOpened;
		config.TaskViewOpenedAppearance = TaskViewOpened;
		config.BatterySaverAppearance = BatterySaver;
		config.ActiveProfile = Name;
	}

	template<class Writer>
	void Serialize(Writer &writer) const
	{
		rjh::WriteKey(writer, SCHEMA_VERSION_KEY);
		writer.Uint(SCHEMA_VERSION);
		rjh::Serialize(writer, std::wstring_view { Name }, NAME_KEY);
		rjh::Serialize(writer, Desktop, DESKTOP_KEY);
		rjh::Serialize(writer, VisibleWindow, VISIBLE_KEY);
		rjh::Serialize(writer, MaximisedWindow, MAXIMISED_KEY);
		rjh::Serialize(writer, StartOpened, START_KEY);
		rjh::Serialize(writer, SearchOpened, SEARCH_KEY);
		rjh::Serialize(writer, TaskViewOpened, TASKVIEW_KEY);
		rjh::Serialize(writer, BatterySaver, BATTERYSAVER_KEY);
	}

	// Throws rjh::DeserializationError on invalid content.
	void Deserialize(const rjh::value_t &obj, void (*unknownKeyCallback)(std::wstring_view) = nullptr)
	{
		rjh::EnsureType(rj::Type::kObjectType, obj.GetType(), L"profile");

		bool hasName = false;
		for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it)
		{
			rjh::EnsureType(rj::Type::kStringType, it->name.GetType(), L"member name");
			const auto key = rjh::ValueToStringView(it->name);

			if (key == SCHEMA_VERSION_KEY)
			{
				if (!it->value.IsUint())
				{
					throw rjh::DeserializationError { L"schema_version must be a non-negative integer" };
				}
				if (it->value.GetUint() > SCHEMA_VERSION)
				{
					throw rjh::DeserializationError {
						std::format(L"Profile schema version {} is newer than the supported version {}", it->value.GetUint(), SCHEMA_VERSION)
					};
				}
			}
			else if (key == NAME_KEY)
			{
				rjh::EnsureType(rj::Type::kStringType, it->value.GetType(), key);
				Name = rjh::ValueToStringView(it->value);
				hasName = true;
			}
			else if (key == DESKTOP_KEY) rjh::Deserialize(it->value, Desktop, key, unknownKeyCallback);
			else if (key == VISIBLE_KEY) rjh::Deserialize(it->value, VisibleWindow, key, unknownKeyCallback);
			else if (key == MAXIMISED_KEY) rjh::Deserialize(it->value, MaximisedWindow, key, unknownKeyCallback);
			else if (key == START_KEY) rjh::Deserialize(it->value, StartOpened, key, unknownKeyCallback);
			else if (key == SEARCH_KEY) rjh::Deserialize(it->value, SearchOpened, key, unknownKeyCallback);
			else if (key == TASKVIEW_KEY) rjh::Deserialize(it->value, TaskViewOpened, key, unknownKeyCallback);
			else if (key == BATTERYSAVER_KEY) rjh::Deserialize(it->value, BatterySaver, key, unknownKeyCallback);
			else if (unknownKeyCallback)
			{
				unknownKeyCallback(key);
			}
		}

		if (!hasName || !IsValidName(Name))
		{
			throw rjh::DeserializationError { L"Profile has a missing or invalid name" };
		}
	}

	// Names are shown in the UI and used to build file names, so they are restricted
	// to printable characters that are valid in Windows file names.
	static bool IsValidName(std::wstring_view name) noexcept
	{
		if (name.empty() || name.length() > MAX_NAME_LENGTH)
		{
			return false;
		}
		if (name.front() == L' ' || name.back() == L' ' || name.back() == L'.')
		{
			return false;
		}

		static constexpr std::wstring_view forbidden = L"<>:\"/\\|?*";
		return std::ranges::none_of(name, [](wchar_t c)
		{
			return c < 0x20 || forbidden.find(c) != std::wstring_view::npos;
		});
	}

	// Lowercase file stem used for the profile on disk. Callers must validate the name first.
	static std::wstring FileStem(std::wstring_view name)
	{
		std::wstring stem(name);
		std::ranges::transform(stem, stem.begin(), [](wchar_t c)
		{
			return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c;
		});
		return stem;
	}

	static std::wstring ToJson(const Profile &profile)
	{
		rj::GenericStringBuffer<rj::UTF16LE<>> buffer;
		rj::PrettyWriter<rj::GenericStringBuffer<rj::UTF16LE<>>, rj::UTF16LE<>, rj::UTF16LE<>> writer(buffer);
		writer.SetIndent(L' ', 2);
		writer.StartObject();
		profile.Serialize(writer);
		writer.EndObject();
		return { buffer.GetString(), buffer.GetLength() };
	}

	// Parses and validates a profile document. Returns std::nullopt and sets error on failure.
	static std::optional<Profile> FromJson(std::wstring_view json, std::wstring &error)
	{
		rj::GenericDocument<rj::UTF16LE<>> doc;
		const std::wstring copy(json);
		if (const rj::ParseResult result = doc.Parse<rj::kParseCommentsFlag | rj::kParseTrailingCommasFlag>(copy.c_str()); !result)
		{
			// RAPIDJSON_ERROR_CHARTYPE is wchar_t project-wide (see CppProject.props).
			error = std::format(L"Invalid JSON at offset {}: {}", result.Offset(), std::wstring_view { rj::GetParseError_En(result.Code()) });
			return std::nullopt;
		}

		try
		{
			Profile profile;
			profile.Deserialize(doc);
			return profile;
		}
		catch (const rjh::DeserializationError &err)
		{
			error = err.what;
			return std::nullopt;
		}
		catch (const std::exception &err)
		{
			// e.g. Util::Color::FromString throws std::invalid_argument for bad colors.
			const std::string_view what = err.what();
			error = std::format(L"Invalid profile: {}", std::wstring(what.begin(), what.end()));
			return std::nullopt;
		}
	}

	// Profiles that ship with GlassTB. Each one only uses materials that the existing
	// engine can apply (AccentPolicy / XAML brushes). Effects that need geometry or
	// lighting changes (floating dock, neon glow, Liquid Glass) are not included.
	static std::vector<Profile> BuiltIn()
	{
		std::vector<Profile> list;
		const auto make = [&list](std::wstring name, TaskbarAppearance desktop, OptionalTaskbarAppearance visible, OptionalTaskbarAppearance maximised)
		{
			Profile p = FromConfig(Config { }, std::move(name));
			p.Desktop = desktop;
			p.VisibleWindow = visible;
			p.MaximisedWindow = maximised;
			list.push_back(std::move(p));
		};

		using C = Util::Color;
		make(L"Invisible",
			{ ACCENT_ENABLE_TRANSPARENTGRADIENT, C(0, 0, 0, 0), false, false, 9.0f },
			{ true, ACCENT_ENABLE_TRANSPARENTGRADIENT, C(0, 0, 0, 0), false, false, 9.0f },
			{ true, ACCENT_ENABLE_TRANSPARENTGRADIENT, C(0, 0, 0, 0), false, false, 9.0f });
		make(L"Glass",
			{ ACCENT_ENABLE_ACRYLICBLURBEHIND, C(255, 255, 255, 24), false, false, 9.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(255, 255, 255, 32), true, false, 9.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(32, 32, 32, 96), true, true, 9.0f });
		make(L"Frosted",
			{ ACCENT_ENABLE_ACRYLICBLURBEHIND, C(230, 236, 245, 72), false, false, 12.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(230, 236, 245, 96), true, false, 12.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(230, 236, 245, 128), true, true, 12.0f });
		make(L"Midnight",
			{ ACCENT_ENABLE_ACRYLICBLURBEHIND, C(10, 15, 30, 150), false, false, 9.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(10, 15, 30, 190), true, false, 9.0f },
			{ true, ACCENT_ENABLE_GRADIENT, C(10, 15, 30, 255), true, true, 9.0f });
		make(L"OLED",
			{ ACCENT_ENABLE_GRADIENT, C(0, 0, 0, 255), false, false, 9.0f },
			{ true, ACCENT_ENABLE_GRADIENT, C(0, 0, 0, 255), false, false, 9.0f },
			{ true, ACCENT_ENABLE_GRADIENT, C(0, 0, 0, 255), false, false, 9.0f });
		make(L"Cyberpunk",
			{ ACCENT_ENABLE_ACRYLICBLURBEHIND, C(255, 0, 160, 70), false, false, 9.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(40, 0, 70, 170), true, false, 9.0f },
			{ true, ACCENT_ENABLE_GRADIENT, C(26, 0, 51, 255), true, true, 9.0f });
		make(L"Gamer",
			{ ACCENT_ENABLE_TRANSPARENTGRADIENT, C(200, 0, 0, 40), false, false, 9.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(20, 0, 0, 160), true, false, 9.0f },
			{ true, ACCENT_ENABLE_GRADIENT, C(0, 0, 0, 255), true, true, 9.0f });
		make(L"Productividad",
			{ ACCENT_ENABLE_TRANSPARENTGRADIENT, C(0, 0, 0, 0), false, false, 9.0f },
			{ true, ACCENT_ENABLE_ACRYLICBLURBEHIND, C(0, 0, 0, 64), true, false, 9.0f },
			{ true, ACCENT_NORMAL, C(0, 0, 0, 0), true, true, 9.0f });
		make(L"Windows clásico",
			{ ACCENT_NORMAL, C(0, 0, 0, 0), true, true, 9.0f },
			{ false, ACCENT_NORMAL, C(0, 0, 0, 0), true, true, 9.0f },
			{ false, ACCENT_NORMAL, C(0, 0, 0, 0), true, true, 9.0f });
		return list;
	}

private:
	static constexpr std::wstring_view SCHEMA_VERSION_KEY = L"schema_version";
	static constexpr std::wstring_view NAME_KEY = L"name";
	static constexpr std::wstring_view DESKTOP_KEY = L"desktop_appearance";
	static constexpr std::wstring_view VISIBLE_KEY = L"visible_window_appearance";
	static constexpr std::wstring_view MAXIMISED_KEY = L"maximized_window_appearance";
	static constexpr std::wstring_view START_KEY = L"start_opened_appearance";
	static constexpr std::wstring_view SEARCH_KEY = L"search_opened_appearance";
	static constexpr std::wstring_view TASKVIEW_KEY = L"task_view_opened_appearance";
	static constexpr std::wstring_view BATTERYSAVER_KEY = L"battery_saver_appearance";
};
