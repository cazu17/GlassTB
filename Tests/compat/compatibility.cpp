#include <gtest/gtest.h>

#include "../../TranslucentTB/compat/backend.hpp"
#include "../../TranslucentTB/compat/systeminfo.hpp"

using compat::Backend;
using compat::ComputeCapabilities;
using compat::SelectBackend;
using compat::SystemInfo;

namespace {
void ExpectSelection(const SystemInfo &system, TaskbarType type, bool blur, bool xaml, bool classic, Backend backend)
{
	const auto capabilities = ComputeCapabilities(system, type);
	EXPECT_EQ(capabilities.blurAccentState, blur);
	EXPECT_EQ(capabilities.xamlTaskbarService, xaml);
	EXPECT_EQ(capabilities.classicAccentPolicy, classic);
	EXPECT_EQ(SelectBackend(type, capabilities), backend);
}
}

TEST(Compatibility, Windows10Classic)
{
	ExpectSelection({ .version = compat::WindowsVersion { 10, 0, 19045, 0 } },
		TaskbarType::Classic, true, false, true, Backend::AccentPolicy);
}

TEST(Compatibility, Windows11RtmBeforeBlurFix)
{
	ExpectSelection({ .version = compat::WindowsVersion { 10, 0, 22000, 281 }, .isWindows11 = true, .isWindows11Rtm = true },
		TaskbarType::Mixed, false, false, true, Backend::Mixed);
}

TEST(Compatibility, Windows11RtmAtBlurFix)
{
	ExpectSelection({ .version = compat::WindowsVersion { 10, 0, 22000, 282 }, .isWindows11 = true, .isWindows11Rtm = true },
		TaskbarType::Mixed, true, false, true, Backend::Mixed);
}

TEST(Compatibility, Windows11RtmWithMissingRevision)
{
	ExpectSelection({ .isWindows11 = true, .isWindows11Rtm = true },
		TaskbarType::Mixed, false, false, true, Backend::Mixed);
}

TEST(Compatibility, Windows11Build22621Mixed)
{
	ExpectSelection({ .version = compat::WindowsVersion { 10, 0, 22621, 500 }, .isWindows11 = true },
		TaskbarType::Mixed, false, false, true, Backend::Mixed);
}

TEST(Compatibility, Windows11Build22621Xaml)
{
	ExpectSelection({ .version = compat::WindowsVersion { 10, 0, 22621, 500 }, .isWindows11 = true },
		TaskbarType::XAML, false, true, false, Backend::XamlTap);
}

TEST(Compatibility, Windows11Build26100Xaml)
{
	ExpectSelection({ .version = compat::WindowsVersion { 10, 0, 26100, 0 }, .isWindows11 = true },
		TaskbarType::XAML, false, true, false, Backend::XamlTap);
}

TEST(Compatibility, UnknownTaskbarDefersBackend)
{
	ExpectSelection({ .version = compat::WindowsVersion { 10, 0, 19045, 0 } },
		TaskbarType::Unknown, true, false, false, Backend::None);
}

TEST(Compatibility, BackendRequiresMatchingCapability)
{
	EXPECT_EQ(SelectBackend(TaskbarType::Unknown, { true, true, true }), Backend::None);
	EXPECT_EQ(SelectBackend(TaskbarType::Classic, {}), Backend::None);
	EXPECT_EQ(SelectBackend(TaskbarType::Mixed, {}), Backend::None);
	EXPECT_EQ(SelectBackend(TaskbarType::XAML, {}), Backend::None);
}
