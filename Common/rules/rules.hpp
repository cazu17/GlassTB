#pragma once
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <rapidjson/document.h>
// rapidjson/error/en.h uses RAPIDJSON_ERROR_STRING=UTIL_WIDEN (see CppProject.props).
#include "../util/string_macros.hpp"
#include <rapidjson/error/en.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "../config/config.hpp"
#include "../config/profile.hpp"
#include "../config/rapidjsonhelper.hpp"
#include "../util/color.hpp"

// Automation rules: pure data model, evaluation and conflict resolution.
// The runtime part that gathers system state lives in TranslucentTB/rules.
namespace rules {
	// State of the system at one instant. Strings are lowercase executable file names (e.g. L"game.exe").
	struct SystemSnapshot {
		std::wstring foregroundExe;
		bool foregroundMaximized = false;
		bool fullscreen = false;
		std::vector<std::wstring> runningExes;
		int minutesOfDay = 0;   // 0..1439, local time
		int weekday = 0;        // 0 = Sunday .. 6 = Saturday
		bool onBattery = false;
		bool batterySaver = false;
		int batteryPercent = -1; // -1 when unknown / no battery
		bool darkTheme = false;
		int monitorCount = 1;
	};

	enum class ConditionType {
		All, Any, Not,
		ForegroundApp, AppRunning, ForegroundMaximized, Fullscreen,
		TimeRange, Weekdays,
		OnBattery, BatterySaver, BatteryBelow,
		DarkTheme, MonitorsAtLeast
	};

	struct Condition {
		ConditionType type = ConditionType::All;
		std::vector<Condition> children;   // All / Any / Not (Not has exactly one)
		std::vector<std::wstring> apps;    // ForegroundApp / AppRunning (lowercase)
		int from = 0, to = 0;              // TimeRange, minutes of day; wraps past midnight when from > to
		std::uint8_t weekdays = 0;         // bit 0 = Sunday
		int value = 0;                     // BatteryBelow percent / MonitorsAtLeast count

		// True if evaluating this condition needs the list of running processes (costly to gather).
		bool NeedsRunningApps() const
		{
			return type == ConditionType::AppRunning || std::ranges::any_of(children, [](const Condition &c) { return c.NeedsRunningApps(); });
		}

		bool NeedsClock() const
		{
			return type == ConditionType::TimeRange || type == ConditionType::Weekdays ||
				std::ranges::any_of(children, [](const Condition &c) { return c.NeedsClock(); });
		}
	};

	enum class ActionType { ActivateProfile, RestoreWindows, SetMaterial, SetColor, SetOpacity };

	struct Action {
		ActionType type = ActionType::ActivateProfile;
		std::wstring profile;
		ACCENT_STATE accent = ACCENT_NORMAL;
		Util::Color color;
		std::uint8_t opacity = 255;
	};

	struct Rule {
		std::wstring name;
		bool enabled = true;
		int priority = 0;
		Condition condition;
		std::vector<Action> actions;
	};

	inline bool ContainsApp(const std::vector<std::wstring> &apps, std::wstring_view exe)
	{
		return std::ranges::find(apps, exe) != apps.end();
	}

	inline bool Evaluate(const Condition &c, const SystemSnapshot &s)
	{
		switch (c.type)
		{
		case ConditionType::All:
			return std::ranges::all_of(c.children, [&s](const Condition &child) { return Evaluate(child, s); });
		case ConditionType::Any:
			return std::ranges::any_of(c.children, [&s](const Condition &child) { return Evaluate(child, s); });
		case ConditionType::Not:
			return c.children.size() == 1 && !Evaluate(c.children.front(), s);
		case ConditionType::ForegroundApp:
			return !s.foregroundExe.empty() && ContainsApp(c.apps, s.foregroundExe);
		case ConditionType::AppRunning:
			return std::ranges::any_of(c.apps, [&s](const std::wstring &app) { return ContainsApp(s.runningExes, app); });
		case ConditionType::ForegroundMaximized:
			return s.foregroundMaximized;
		case ConditionType::Fullscreen:
			return s.fullscreen;
		case ConditionType::TimeRange:
			return c.from <= c.to
				? s.minutesOfDay >= c.from && s.minutesOfDay < c.to
				: s.minutesOfDay >= c.from || s.minutesOfDay < c.to;
		case ConditionType::Weekdays:
			return s.weekday >= 0 && s.weekday < 7 && (c.weekdays >> s.weekday) & 1;
		case ConditionType::OnBattery:
			return s.onBattery;
		case ConditionType::BatterySaver:
			return s.batterySaver;
		case ConditionType::BatteryBelow:
			return s.batteryPercent >= 0 && s.batteryPercent < c.value;
		case ConditionType::DarkTheme:
			return s.darkTheme;
		case ConditionType::MonitorsAtLeast:
			return s.monitorCount >= c.value;
		}
		return false;
	}

	// Deterministic conflict resolution: among enabled rules whose condition holds,
	// the highest priority wins; on equal priority, the rule listed first wins.
	inline std::optional<std::size_t> Resolve(const std::vector<Rule> &list, const SystemSnapshot &snapshot)
	{
		std::optional<std::size_t> best;
		for (std::size_t i = 0; i < list.size(); ++i)
		{
			const auto &rule = list[i];
			if (rule.enabled && (!best || rule.priority > list[*best].priority) && Evaluate(rule.condition, snapshot))
			{
				best = i;
			}
		}
		return best;
	}

	// Builds the configuration the engine should use while the rule is active.
	// Nothing is written to disk: the result is an in-memory override.
	// findProfile returns std::nullopt for unknown names; those actions are skipped.
	inline Config BuildEffectiveConfig(const Config &base, const Rule &rule, const std::function<std::optional<Profile>(std::wstring_view)> &findProfile)
	{
		Config effective = base;
		const auto forEachAppearance = [&effective](auto &&fn)
		{
			fn(static_cast<TaskbarAppearance &>(effective.DesktopAppearance));
			fn(static_cast<TaskbarAppearance &>(effective.VisibleWindowAppearance));
			fn(static_cast<TaskbarAppearance &>(effective.MaximisedWindowAppearance));
			fn(static_cast<TaskbarAppearance &>(effective.StartOpenedAppearance));
			fn(static_cast<TaskbarAppearance &>(effective.SearchOpenedAppearance));
			fn(static_cast<TaskbarAppearance &>(effective.TaskViewOpenedAppearance));
			fn(static_cast<TaskbarAppearance &>(effective.BatterySaverAppearance));
		};

		for (const auto &action : rule.actions)
		{
			switch (action.type)
			{
			case ActionType::ActivateProfile:
				if (const auto profile = findProfile(action.profile))
				{
					const auto keepName = effective.ActiveProfile;
					profile->ApplyTo(effective);
					effective.ActiveProfile = keepName;
				}
				break;
			case ActionType::RestoreWindows:
				forEachAppearance([](TaskbarAppearance &a) { a.Accent = ACCENT_NORMAL; });
				break;
			case ActionType::SetMaterial:
				forEachAppearance([&action](TaskbarAppearance &a) { a.Accent = action.accent; });
				break;
			case ActionType::SetColor:
				forEachAppearance([&action](TaskbarAppearance &a) { a.Color = action.color; });
				break;
			case ActionType::SetOpacity:
				forEachAppearance([&action](TaskbarAppearance &a) { a.Color.A = action.opacity; });
				break;
			}
		}

		return effective;
	}

	// ---- JSON (rules.json) ----

	static constexpr std::uint32_t SCHEMA_VERSION = 1;
	static constexpr std::size_t MAX_RULES = 200;
	static constexpr std::size_t MAX_DEPTH = 8;
	static constexpr std::size_t MAX_CHILDREN = 32;

	namespace detail {
		struct TypeName { ConditionType type; std::wstring_view name; };
		inline constexpr TypeName CONDITION_NAMES[] = {
			{ ConditionType::All, L"all" }, { ConditionType::Any, L"any" }, { ConditionType::Not, L"not" },
			{ ConditionType::ForegroundApp, L"foreground_app" }, { ConditionType::AppRunning, L"app_running" },
			{ ConditionType::ForegroundMaximized, L"foreground_maximized" }, { ConditionType::Fullscreen, L"fullscreen" },
			{ ConditionType::TimeRange, L"time_range" }, { ConditionType::Weekdays, L"weekdays" },
			{ ConditionType::OnBattery, L"on_battery" }, { ConditionType::BatterySaver, L"battery_saver" },
			{ ConditionType::BatteryBelow, L"battery_below" }, { ConditionType::DarkTheme, L"dark_theme" },
			{ ConditionType::MonitorsAtLeast, L"monitors_at_least" }
		};

		struct ActionName { ActionType type; std::wstring_view name; };
		inline constexpr ActionName ACTION_NAMES[] = {
			{ ActionType::ActivateProfile, L"activate_profile" }, { ActionType::RestoreWindows, L"restore_windows" },
			{ ActionType::SetMaterial, L"set_material" }, { ActionType::SetColor, L"set_color" },
			{ ActionType::SetOpacity, L"set_opacity" }
		};

		inline constexpr std::wstring_view ACCENT_NAMES[] = { L"normal", L"opaque", L"clear", L"blur", L"acrylic" };
		inline constexpr ACCENT_STATE ACCENT_VALUES[] = { ACCENT_NORMAL, ACCENT_ENABLE_GRADIENT, ACCENT_ENABLE_TRANSPARENTGRADIENT, ACCENT_ENABLE_BLURBEHIND, ACCENT_ENABLE_ACRYLICBLURBEHIND };

		[[noreturn]] inline void Fail(std::wstring message)
		{
			throw rjh::DeserializationError { std::move(message) };
		}

		inline std::wstring Lower(std::wstring_view text)
		{
			std::wstring out(text);
			std::ranges::transform(out, out.begin(), [](wchar_t c) { return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c; });
			return out;
		}

		inline const rjh::value_t *Member(const rjh::value_t &obj, std::wstring_view key)
		{
			const auto it = obj.FindMember(rjh::StringViewToValue(key));
			return it == obj.MemberEnd() ? nullptr : &it->value;
		}

		inline std::wstring_view RequireString(const rjh::value_t &obj, std::wstring_view key)
		{
			const auto *v = Member(obj, key);
			if (!v || !v->IsString())
			{
				Fail(std::format(L"\"{}\" must be a string", key));
			}
			return rjh::ValueToStringView(*v);
		}

		inline int RequireInt(const rjh::value_t &obj, std::wstring_view key, int min, int max)
		{
			const auto *v = Member(obj, key);
			if (!v || !v->IsInt() || v->GetInt() < min || v->GetInt() > max)
			{
				Fail(std::format(L"\"{}\" must be an integer between {} and {}", key, min, max));
			}
			return v->GetInt();
		}

		// "HH:MM" -> minutes of day
		inline int ParseTime(std::wstring_view text)
		{
			if (text.size() != 5 || text[2] != L':' || !std::iswdigit(text[0]) || !std::iswdigit(text[1]) || !std::iswdigit(text[3]) || !std::iswdigit(text[4]))
			{
				Fail(std::format(L"Invalid time \"{}\", expected HH:MM", text));
			}
			const int h = (text[0] - L'0') * 10 + (text[1] - L'0');
			const int m = (text[3] - L'0') * 10 + (text[4] - L'0');
			if (h > 23 || m > 59)
			{
				Fail(std::format(L"Invalid time \"{}\"", text));
			}
			return h * 60 + m;
		}

		inline Condition ParseCondition(const rjh::value_t &v, std::size_t depth)
		{
			if (depth > MAX_DEPTH)
			{
				Fail(L"Conditions are nested too deeply");
			}
			if (!v.IsObject())
			{
				Fail(L"A condition must be an object");
			}

			Condition c;
			const auto typeName = RequireString(v, L"type");
			const auto found = std::ranges::find(CONDITION_NAMES, typeName, &TypeName::name);
			if (found == std::end(CONDITION_NAMES))
			{
				Fail(std::format(L"Unknown condition type \"{}\"", typeName));
			}
			c.type = found->type;

			switch (c.type)
			{
			case ConditionType::All:
			case ConditionType::Any:
			case ConditionType::Not:
			{
				const auto *list = Member(v, L"conditions");
				if (!list || !list->IsArray() || list->Empty() || list->Size() > MAX_CHILDREN)
				{
					Fail(std::format(L"\"{}\" needs a non-empty \"conditions\" array (max {})", typeName, MAX_CHILDREN));
				}
				if (c.type == ConditionType::Not && list->Size() != 1)
				{
					Fail(L"\"not\" takes exactly one condition");
				}
				for (const auto &child : list->GetArray())
				{
					c.children.push_back(ParseCondition(child, depth + 1));
				}
				break;
			}
			case ConditionType::ForegroundApp:
			case ConditionType::AppRunning:
			{
				const auto *list = Member(v, L"apps");
				if (!list || !list->IsArray() || list->Empty() || list->Size() > MAX_CHILDREN)
				{
					Fail(L"\"apps\" must be a non-empty array of executable names");
				}
				for (const auto &app : list->GetArray())
				{
					if (!app.IsString() || app.GetStringLength() == 0 || app.GetStringLength() > 260)
					{
						Fail(L"Each app must be a non-empty executable name");
					}
					c.apps.push_back(Lower(rjh::ValueToStringView(app)));
				}
				break;
			}
			case ConditionType::TimeRange:
				c.from = ParseTime(RequireString(v, L"from"));
				c.to = ParseTime(RequireString(v, L"to"));
				if (c.from == c.to)
				{
					Fail(L"\"time_range\" needs different start and end times");
				}
				break;
			case ConditionType::Weekdays:
			{
				const auto *list = Member(v, L"days");
				if (!list || !list->IsArray() || list->Empty())
				{
					Fail(L"\"days\" must be a non-empty array of integers 0 (Sunday) to 6 (Saturday)");
				}
				for (const auto &day : list->GetArray())
				{
					if (!day.IsInt() || day.GetInt() < 0 || day.GetInt() > 6)
					{
						Fail(L"Days must be integers 0 (Sunday) to 6 (Saturday)");
					}
					c.weekdays |= static_cast<std::uint8_t>(1u << day.GetInt());
				}
				break;
			}
			case ConditionType::BatteryBelow:
				c.value = RequireInt(v, L"percent", 1, 100);
				break;
			case ConditionType::MonitorsAtLeast:
				c.value = RequireInt(v, L"count", 1, 16);
				break;
			default:
				break;
			}

			return c;
		}

		inline Action ParseAction(const rjh::value_t &v)
		{
			if (!v.IsObject())
			{
				Fail(L"An action must be an object");
			}

			Action a;
			const auto typeName = RequireString(v, L"type");
			const auto found = std::ranges::find(ACTION_NAMES, typeName, &ActionName::name);
			if (found == std::end(ACTION_NAMES))
			{
				Fail(std::format(L"Unknown action type \"{}\"", typeName));
			}
			a.type = found->type;

			switch (a.type)
			{
			case ActionType::ActivateProfile:
				a.profile = RequireString(v, L"profile");
				if (!Profile::IsValidName(a.profile))
				{
					Fail(std::format(L"Invalid profile name \"{}\"", a.profile));
				}
				break;
			case ActionType::SetMaterial:
			{
				const auto material = RequireString(v, L"material");
				const auto it = std::ranges::find(ACCENT_NAMES, material);
				if (it == std::end(ACCENT_NAMES))
				{
					Fail(std::format(L"Unknown material \"{}\"", material));
				}
				a.accent = ACCENT_VALUES[it - std::begin(ACCENT_NAMES)];
				break;
			}
			case ActionType::SetColor:
				try
				{
					a.color = Util::Color::FromString(RequireString(v, L"color"));
				}
				catch (const std::invalid_argument &)
				{
					Fail(L"\"color\" must be a #RRGGBB or #RRGGBBAA string");
				}
				break;
			case ActionType::SetOpacity:
				a.opacity = static_cast<std::uint8_t>(RequireInt(v, L"opacity", 0, 255));
				break;
			case ActionType::RestoreWindows:
				break;
			}

			return a;
		}
	}

	// Parses rules.json. Throws nothing: returns std::nullopt and sets error on failure.
	inline std::optional<std::vector<Rule>> FromJson(std::wstring_view json, std::wstring &error)
	{
		rj::GenericDocument<rj::UTF16LE<>> doc;
		const std::wstring copy(json);
		if (const rj::ParseResult result = doc.Parse<rj::kParseCommentsFlag | rj::kParseTrailingCommasFlag>(copy.c_str()); !result)
		{
			error = std::format(L"Invalid JSON at offset {}: {}", result.Offset(), std::wstring_view { rj::GetParseError_En(result.Code()) });
			return std::nullopt;
		}

		try
		{
			if (!doc.IsObject())
			{
				detail::Fail(L"The root must be an object");
			}
			if (const auto *version = detail::Member(doc, L"schema_version"); version && (!version->IsUint() || version->GetUint() > SCHEMA_VERSION))
			{
				detail::Fail(L"Unsupported schema_version");
			}

			const auto *list = detail::Member(doc, L"rules");
			if (!list)
			{
				return std::vector<Rule> { };
			}
			if (!list->IsArray() || list->Size() > MAX_RULES)
			{
				detail::Fail(std::format(L"\"rules\" must be an array of at most {} rules", MAX_RULES));
			}

			std::vector<Rule> parsed;
			for (const auto &item : list->GetArray())
			{
				if (!item.IsObject())
				{
					detail::Fail(L"Each rule must be an object");
				}

				Rule rule;
				rule.name = detail::RequireString(item, L"name");
				if (rule.name.empty() || rule.name.size() > 100)
				{
					detail::Fail(L"Rule names must have 1 to 100 characters");
				}
				if (const auto *enabled = detail::Member(item, L"enabled"))
				{
					if (!enabled->IsBool())
					{
						detail::Fail(L"\"enabled\" must be true or false");
					}
					rule.enabled = enabled->GetBool();
				}
				if (detail::Member(item, L"priority"))
				{
					rule.priority = detail::RequireInt(item, L"priority", -1000, 1000);
				}

				const auto *condition = detail::Member(item, L"condition");
				if (!condition)
				{
					detail::Fail(std::format(L"Rule \"{}\" has no condition", rule.name));
				}
				rule.condition = detail::ParseCondition(*condition, 0);

				const auto *actions = detail::Member(item, L"actions");
				if (!actions || !actions->IsArray() || actions->Empty() || actions->Size() > MAX_CHILDREN)
				{
					detail::Fail(std::format(L"Rule \"{}\" needs a non-empty \"actions\" array", rule.name));
				}
				for (const auto &action : actions->GetArray())
				{
					rule.actions.push_back(detail::ParseAction(action));
				}

				parsed.push_back(std::move(rule));
			}

			return parsed;
		}
		catch (const rjh::DeserializationError &err)
		{
			error = err.what;
			return std::nullopt;
		}
	}

	namespace detail {
		inline std::wstring Escape(std::wstring_view text)
		{
			std::wstring out;
			for (const wchar_t c : text)
			{
				switch (c)
				{
				case L'"': out += L"\\\""; break;
				case L'\\': out += L"\\\\"; break;
				case L'\n': out += L"\\n"; break;
				case L'\r': out += L"\\r"; break;
				case L'\t': out += L"\\t"; break;
				default:
					if (c < 0x20)
					{
						out += std::format(L"\\u{:04x}", static_cast<unsigned>(c));
					}
					else
					{
						out += c;
					}
				}
			}
			return out;
		}

		inline std::wstring TimeText(int minutes)
		{
			return std::format(L"{:02}:{:02}", minutes / 60, minutes % 60);
		}

		inline std::wstring ConditionToJson(const Condition &c)
		{
			const auto name = std::ranges::find(CONDITION_NAMES, c.type, &TypeName::type)->name;
			std::wstring out = std::format(L"{{ \"type\": \"{}\"", name);
			switch (c.type)
			{
			case ConditionType::All:
			case ConditionType::Any:
			case ConditionType::Not:
				out += L", \"conditions\": [ ";
				for (std::size_t i = 0; i < c.children.size(); ++i)
				{
					out += (i ? L", " : L"") + ConditionToJson(c.children[i]);
				}
				out += L" ]";
				break;
			case ConditionType::ForegroundApp:
			case ConditionType::AppRunning:
				out += L", \"apps\": [ ";
				for (std::size_t i = 0; i < c.apps.size(); ++i)
				{
					out += std::format(L"{}\"{}\"", i ? L", " : L"", Escape(c.apps[i]));
				}
				out += L" ]";
				break;
			case ConditionType::TimeRange:
				out += std::format(L", \"from\": \"{}\", \"to\": \"{}\"", TimeText(c.from), TimeText(c.to));
				break;
			case ConditionType::Weekdays:
			{
				out += L", \"days\": [ ";
				bool first = true;
				for (int d = 0; d < 7; ++d)
				{
					if ((c.weekdays >> d) & 1)
					{
						out += std::format(L"{}{}", first ? L"" : L", ", d);
						first = false;
					}
				}
				out += L" ]";
				break;
			}
			case ConditionType::BatteryBelow:
				out += std::format(L", \"percent\": {}", c.value);
				break;
			case ConditionType::MonitorsAtLeast:
				out += std::format(L", \"count\": {}", c.value);
				break;
			default:
				break;
			}
			out += L" }";
			return out;
		}

		inline std::wstring ActionToJson(const Action &a)
		{
			const auto name = std::ranges::find(ACTION_NAMES, a.type, &ActionName::type)->name;
			std::wstring out = std::format(L"{{ \"type\": \"{}\"", name);
			switch (a.type)
			{
			case ActionType::ActivateProfile:
				out += std::format(L", \"profile\": \"{}\"", Escape(a.profile));
				break;
			case ActionType::SetMaterial:
			{
				const auto it = std::ranges::find(ACCENT_VALUES, a.accent);
				out += std::format(L", \"material\": \"{}\"", it == std::end(ACCENT_VALUES) ? L"normal" : ACCENT_NAMES[it - std::begin(ACCENT_VALUES)]);
				break;
			}
			case ActionType::SetColor:
				out += std::format(L", \"color\": \"{}\"", a.color.ToString());
				break;
			case ActionType::SetOpacity:
				out += std::format(L", \"opacity\": {}", a.opacity);
				break;
			case ActionType::RestoreWindows:
				break;
			}
			out += L" }";
			return out;
		}
	}

	// Serializes rules back to rules.json (comments in the original file are not preserved).
	inline std::wstring ToJson(const std::vector<Rule> &list)
	{
		std::wstring out = L"{\n  \"schema_version\": 1,\n  \"rules\": [";
		for (std::size_t i = 0; i < list.size(); ++i)
		{
			const auto &r = list[i];
			out += std::format(L"{}\n    {{\n      \"name\": \"{}\",\n      \"enabled\": {},\n      \"priority\": {},\n      \"condition\": {},\n      \"actions\": [ ",
				i ? L"," : L"", detail::Escape(r.name), r.enabled ? L"true" : L"false", r.priority, detail::ConditionToJson(r.condition));
			for (std::size_t j = 0; j < r.actions.size(); ++j)
			{
				out += (j ? L", " : L"") + detail::ActionToJson(r.actions[j]);
			}
			out += L" ]\n    }";
		}
		out += list.empty() ? L"]\n}\n" : L"\n  ]\n}\n";
		return out;
	}

	// Template written the first time the user opens the rules file.
	inline constexpr std::wstring_view TEMPLATE = LR"({
  // GlassTB automation rules. The enabled rule with the highest priority whose
  // condition holds is applied in memory (config.json is not modified).
  // Documentation: docs/glasstb/automatizacion.md
  "schema_version": 1,
  "rules": [
    {
      "name": "Ejemplo: modo rendimiento con batería baja",
      "enabled": false,
      "priority": 10,
      "condition": { "type": "all", "conditions": [
        { "type": "on_battery" },
        { "type": "battery_below", "percent": 20 }
      ] },
      "actions": [ { "type": "restore_windows" } ]
    }
  ]
}
)";
}
