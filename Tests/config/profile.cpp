#include <gtest/gtest.h>

#include "config/profile.hpp"

TEST(Profile, NameValidation)
{
	EXPECT_TRUE(Profile::IsValidName(L"Mi perfil"));
	EXPECT_TRUE(Profile::IsValidName(L"Windows clásico"));
	EXPECT_FALSE(Profile::IsValidName(L""));
	EXPECT_FALSE(Profile::IsValidName(L" leading"));
	EXPECT_FALSE(Profile::IsValidName(L"trailing "));
	EXPECT_FALSE(Profile::IsValidName(L"dot."));
	EXPECT_FALSE(Profile::IsValidName(L"a/b"));
	EXPECT_FALSE(Profile::IsValidName(L"a:b"));
	EXPECT_FALSE(Profile::IsValidName(std::wstring(Profile::MAX_NAME_LENGTH + 1, L'x')));
}

TEST(Profile, RoundTrip)
{
	Profile original = Profile::FromConfig(Config { }, L"Prueba");
	original.Desktop = { ACCENT_ENABLE_ACRYLICBLURBEHIND, Util::Color(1, 2, 3, 4), false, true, 12.5f };
	original.MaximisedWindow.Enabled = false;

	std::wstring error;
	const auto parsed = Profile::FromJson(Profile::ToJson(original), error);
	ASSERT_TRUE(parsed.has_value()) << error;
	EXPECT_EQ(parsed->Name, L"Prueba");
	EXPECT_EQ(parsed->Desktop.Accent, ACCENT_ENABLE_ACRYLICBLURBEHIND);
	EXPECT_EQ(parsed->Desktop.Color.ToRGBA(), Util::Color(1, 2, 3, 4).ToRGBA());
	EXPECT_FALSE(parsed->Desktop.ShowPeek);
	EXPECT_FLOAT_EQ(parsed->Desktop.BlurRadius, 12.5f);
	EXPECT_FALSE(parsed->MaximisedWindow.Enabled);
}

TEST(Profile, RejectsMalformedInput)
{
	std::wstring error;
	EXPECT_FALSE(Profile::FromJson(L"{ not json", error).has_value());
	EXPECT_FALSE(error.empty());

	EXPECT_FALSE(Profile::FromJson(L"[]", error).has_value());
	EXPECT_FALSE(Profile::FromJson(L"{ \"desktop_appearance\": {} }", error).has_value()); // no name
	EXPECT_FALSE(Profile::FromJson(L"{ \"name\": \"a/b\" }", error).has_value());
	EXPECT_FALSE(Profile::FromJson(L"{ \"name\": \"ok\", \"schema_version\": 99 }", error).has_value());
	EXPECT_FALSE(Profile::FromJson(L"{ \"name\": \"ok\", \"schema_version\": -1 }", error).has_value());
	EXPECT_FALSE(Profile::FromJson(L"{ \"name\": \"ok\", \"desktop_appearance\": { \"accent\": \"nope\" } }", error).has_value());
	EXPECT_FALSE(Profile::FromJson(L"{ \"name\": \"ok\", \"desktop_appearance\": { \"color\": 5 } }", error).has_value());
	EXPECT_FALSE(Profile::FromJson(L"{ \"name\": \"ok\", \"desktop_appearance\": { \"color\": \"#zz\" } }", error).has_value());
}

TEST(Profile, AcceptsMinimalDocument)
{
	std::wstring error;
	const auto parsed = Profile::FromJson(L"// comment\n{ \"name\": \"Solo nombre\", }", error);
	ASSERT_TRUE(parsed.has_value()) << error;
	EXPECT_EQ(parsed->Name, L"Solo nombre");
}

TEST(Profile, ApplyKeepsRulesAndAdvancedSettings)
{
	Config config;
	config.HideTray = true;
	config.MaximisedWindowAppearance.ClassRules.insert({ L"Notepad", { } });

	const auto builtIn = Profile::BuiltIn();
	ASSERT_FALSE(builtIn.empty());
	builtIn.front().ApplyTo(config);

	EXPECT_EQ(config.ActiveProfile, builtIn.front().Name);
	EXPECT_EQ(config.DesktopAppearance.Accent, builtIn.front().Desktop.Accent);
	EXPECT_EQ(config.MaximisedWindowAppearance.ClassRules.size(), 1u);
	EXPECT_EQ(config.HideTray, std::optional<bool>(true));
}

TEST(Profile, BuiltInProfilesAreValidAndUnique)
{
	const auto builtIn = Profile::BuiltIn();
	for (std::size_t i = 0; i < builtIn.size(); ++i)
	{
		EXPECT_TRUE(Profile::IsValidName(builtIn[i].Name)) << i;
		std::wstring error;
		EXPECT_TRUE(Profile::FromJson(Profile::ToJson(builtIn[i]), error).has_value()) << error;
		for (std::size_t j = i + 1; j < builtIn.size(); ++j)
		{
			EXPECT_NE(Profile::FileStem(builtIn[i].Name), Profile::FileStem(builtIn[j].Name));
		}
	}
}

TEST(Profile, ConfigSchemaVersion)
{
	rj::GenericDocument<rj::UTF16LE<>> doc;
	doc.Parse(L"{ \"verbosity\": \"warn\" }");
	Config legacy;
	legacy.Deserialize(doc);
	EXPECT_EQ(legacy.LoadedSchemaVersion, 0u);

	doc.Parse(L"{ \"schema_version\": 1, \"active_profile\": \"Glass\" }");
	Config current;
	current.Deserialize(doc);
	EXPECT_EQ(current.LoadedSchemaVersion, 1u);
	EXPECT_EQ(current.ActiveProfile, L"Glass");
}
