#include "capabilities.hpp"
#include "systeminfo.hpp"

compat::Capabilities compat::ComputeCapabilities(const SystemInfo &system, TaskbarType type) noexcept
{
	// KB5006746 fixed the AccentPolicy blur issue on the Windows 11 RTM build.
	const bool blurAccentState = system.isWindows11Rtm && system.version
		? system.version->revision >= 282
		: !system.isWindows11;

	return {
		.blurAccentState = blurAccentState,
		.xamlTaskbarService = type == TaskbarType::XAML,
		.classicAccentPolicy = type == TaskbarType::Classic || type == TaskbarType::Mixed,
	};
}
