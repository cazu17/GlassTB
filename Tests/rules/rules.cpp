#include <gtest/gtest.h>

#include "rules/rules.hpp"

using namespace rules;

namespace {
	Condition Leaf(ConditionType type)
	{
		Condition c;
		c.type = type;
		return c;
	}

	Rule MakeRule(std::wstring name, int priority, Condition condition, bool enabled = true)
	{
		Rule r;
		r.name = std::move(name);
		r.priority = priority;
		r.enabled = enabled;
		r.condition = std::move(condition);
		r.actions.push_back({ ActionType::RestoreWindows });
		return r;
	}
}

TEST(Rules, LogicalOperators)
{
	SystemSnapshot s;
	s.onBattery = true;

	Condition all { ConditionType::All };
	all.children = { Leaf(ConditionType::OnBattery), Leaf(ConditionType::Fullscreen) };
	EXPECT_FALSE(Evaluate(all, s));

	Condition any { ConditionType::Any };
	any.children = all.children;
	EXPECT_TRUE(Evaluate(any, s));

	Condition notC { ConditionType::Not };
	notC.children = { Leaf(ConditionType::Fullscreen) };
	EXPECT_TRUE(Evaluate(notC, s));
}

TEST(Rules, TimeRangeWrapsMidnight)
{
	Condition night { ConditionType::TimeRange };
	night.from = 22 * 60;
	night.to = 6 * 60;

	SystemSnapshot s;
	s.minutesOfDay = 23 * 60;
	EXPECT_TRUE(Evaluate(night, s));
	s.minutesOfDay = 5 * 60 + 59;
	EXPECT_TRUE(Evaluate(night, s));
	s.minutesOfDay = 6 * 60;
	EXPECT_FALSE(Evaluate(night, s));
	s.minutesOfDay = 12 * 60;
	EXPECT_FALSE(Evaluate(night, s));
}

TEST(Rules, AppsAndBattery)
{
	SystemSnapshot s;
	s.foregroundExe = L"game.exe";
	s.runningExes = { L"steam.exe" };
	s.batteryPercent = 15;

	Condition fg { ConditionType::ForegroundApp };
	fg.apps = { L"game.exe" };
	EXPECT_TRUE(Evaluate(fg, s));

	Condition running { ConditionType::AppRunning };
	running.apps = { L"obs64.exe", L"steam.exe" };
	EXPECT_TRUE(Evaluate(running, s));
	EXPECT_TRUE(running.NeedsRunningApps());
	EXPECT_FALSE(fg.NeedsRunningApps());

	Condition low { ConditionType::BatteryBelow };
	low.value = 20;
	EXPECT_TRUE(Evaluate(low, s));
	s.batteryPercent = -1;
	EXPECT_FALSE(Evaluate(low, s));
}

TEST(Rules, ResolutionIsDeterministic)
{
	SystemSnapshot s;
	s.onBattery = true;
	s.fullscreen = true;

	std::vector<Rule> list = {
		MakeRule(L"a", 5, Leaf(ConditionType::OnBattery)),
		MakeRule(L"b", 10, Leaf(ConditionType::Fullscreen)),
		MakeRule(L"c", 10, Leaf(ConditionType::OnBattery)),
		MakeRule(L"d", 50, Leaf(ConditionType::OnBattery), false),
	};

	// b and c tie on priority 10: the first one listed wins; d is disabled.
	EXPECT_EQ(Resolve(list, s), std::optional<std::size_t>(1));

	s.fullscreen = false;
	EXPECT_EQ(Resolve(list, s), std::optional<std::size_t>(2));

	s.onBattery = false;
	EXPECT_EQ(Resolve(list, s), std::nullopt);
}

TEST(Rules, EffectiveConfigDoesNotTouchBase)
{
	Config base;
	base.ActiveProfile = L"Mine";
	Rule rule = MakeRule(L"r", 0, Leaf(ConditionType::OnBattery));
	rule.actions = { { ActionType::ActivateProfile, L"OLED" }, { ActionType::SetOpacity } };
	rule.actions[1].opacity = 128;

	const auto effective = BuildEffectiveConfig(base, rule, [](std::wstring_view name) -> std::optional<Profile>
	{
		for (const auto &p : Profile::BuiltIn())
		{
			if (p.Name == name)
			{
				return p;
			}
		}
		return std::nullopt;
	});

	EXPECT_EQ(effective.DesktopAppearance.Accent, ACCENT_ENABLE_GRADIENT);
	EXPECT_EQ(effective.DesktopAppearance.Color.A, 128);
	EXPECT_EQ(effective.ActiveProfile, L"Mine");
	EXPECT_EQ(base.DesktopAppearance.Accent, Config { }.DesktopAppearance.Accent);
}

TEST(Rules, ParsesValidDocument)
{
	std::wstring error;
	const auto parsed = FromJson(LR"({
		"schema_version": 1,
		"rules": [ {
			"name": "Noche",
			"priority": 3,
			"condition": { "type": "all", "conditions": [
				{ "type": "time_range", "from": "22:00", "to": "06:30" },
				{ "type": "not", "conditions": [ { "type": "foreground_app", "apps": [ "Code.EXE" ] } ] },
				{ "type": "weekdays", "days": [ 1, 2, 3, 4, 5 ] }
			] },
			"actions": [ { "type": "set_material", "material": "acrylic" }, { "type": "set_color", "color": "#10203040" } ]
		} ]
	})", error);

	ASSERT_TRUE(parsed.has_value()) << error;
	ASSERT_EQ(parsed->size(), 1u);
	const auto &rule = parsed->front();
	EXPECT_EQ(rule.priority, 3);
	EXPECT_EQ(rule.condition.children[0].from, 22 * 60);
	EXPECT_EQ(rule.condition.children[0].to, 6 * 60 + 30);
	EXPECT_EQ(rule.condition.children[1].children[0].apps[0], L"code.exe");
	EXPECT_EQ(rule.condition.children[2].weekdays, 0b0111110);
	EXPECT_EQ(rule.actions[0].accent, ACCENT_ENABLE_ACRYLICBLURBEHIND);
	EXPECT_TRUE(rule.condition.NeedsClock());
}

TEST(Rules, RejectsMalformedDocuments)
{
	const wchar_t *bad[] = {
		L"{",
		L"[]",
		L"{ \"rules\": {} }",
		L"{ \"schema_version\": 2, \"rules\": [] }",
		L"{ \"rules\": [ { \"condition\": { \"type\": \"on_battery\" }, \"actions\": [ { \"type\": \"restore_windows\" } ] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"actions\": [ { \"type\": \"restore_windows\" } ] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"condition\": { \"type\": \"nope\" }, \"actions\": [ { \"type\": \"restore_windows\" } ] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"condition\": { \"type\": \"time_range\", \"from\": \"25:00\", \"to\": \"01:00\" }, \"actions\": [ { \"type\": \"restore_windows\" } ] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"condition\": { \"type\": \"not\", \"conditions\": [ { \"type\": \"on_battery\" }, { \"type\": \"fullscreen\" } ] }, \"actions\": [ { \"type\": \"restore_windows\" } ] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"condition\": { \"type\": \"on_battery\" }, \"actions\": [] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"condition\": { \"type\": \"on_battery\" }, \"actions\": [ { \"type\": \"set_opacity\", \"opacity\": 300 } ] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"condition\": { \"type\": \"on_battery\" }, \"actions\": [ { \"type\": \"activate_profile\", \"profile\": \"a/b\" } ] } ] }",
		L"{ \"rules\": [ { \"name\": \"x\", \"condition\": { \"type\": \"on_battery\" }, \"actions\": [ { \"type\": \"set_color\", \"color\": \"blue\" } ] } ] }",
	};

	for (const auto *doc : bad)
	{
		std::wstring error;
		EXPECT_FALSE(FromJson(doc, error).has_value()) << doc;
		EXPECT_FALSE(error.empty()) << doc;
	}
}

TEST(Rules, RejectsDeepNesting)
{
	std::wstring condition = L"{ \"type\": \"on_battery\" }";
	for (int i = 0; i < 12; ++i)
	{
		condition = L"{ \"type\": \"not\", \"conditions\": [ " + condition + L" ] }";
	}
	const std::wstring doc = L"{ \"rules\": [ { \"name\": \"x\", \"condition\": " + condition + L", \"actions\": [ { \"type\": \"restore_windows\" } ] } ] }";

	std::wstring error;
	EXPECT_FALSE(FromJson(doc, error).has_value());
}

TEST(Rules, TemplateParses)
{
	std::wstring error;
	const auto parsed = FromJson(TEMPLATE, error);
	ASSERT_TRUE(parsed.has_value()) << error;
	EXPECT_EQ(parsed->size(), 1u);
	EXPECT_FALSE(parsed->front().enabled);
}

TEST(Rules, SerializationRoundTrip)
{
	std::wstring error;
	const auto original = FromJson(LR"({ "rules": [
		{ "name": "Quote \" and \\ slash", "enabled": false, "priority": -5,
		  "condition": { "type": "any", "conditions": [
			{ "type": "weekdays", "days": [0, 6] },
			{ "type": "not", "conditions": [ { "type": "app_running", "apps": ["obs64.exe"] } ] },
			{ "type": "battery_below", "percent": 30 },
			{ "type": "monitors_at_least", "count": 2 },
			{ "type": "time_range", "from": "08:05", "to": "17:00" }
		  ] },
		  "actions": [ { "type": "activate_profile", "profile": "Glass" }, { "type": "set_material", "material": "clear" },
		               { "type": "set_color", "color": "#11223344" }, { "type": "set_opacity", "opacity": 7 } ] }
	] })", error);
	ASSERT_TRUE(original.has_value()) << error;

	const auto json = ToJson(*original);
	const auto reparsed = FromJson(json, error);
	ASSERT_TRUE(reparsed.has_value()) << error << L"\n" << json;
	EXPECT_EQ(ToJson(*reparsed), json);
	EXPECT_EQ(reparsed->front().name, L"Quote \" and \\ slash");
	EXPECT_EQ(reparsed->front().actions[3].opacity, 7);

	EXPECT_TRUE(FromJson(ToJson({ }), error).has_value()) << error;
}
