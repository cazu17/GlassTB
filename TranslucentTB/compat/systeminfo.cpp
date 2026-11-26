#include "systeminfo.hpp"
#include "win32.hpp"
#include <wow64apiset.h>

namespace {
std::wstring_view MachineName(USHORT machine) noexcept
{
	switch (machine)
	{
	case IMAGE_FILE_MACHINE_AMD64:
		return L"x64";
	case IMAGE_FILE_MACHINE_I386:
		return L"x86";
	case IMAGE_FILE_MACHINE_ARM64:
		return L"ARM64";
	case IMAGE_FILE_MACHINE_ARMNT:
		return L"ARM";
	default:
		return L"Unknown";
	}
}
}

compat::SystemInfo compat::GetSystemInfo()
{
	SystemInfo system;
	system.isWindows11 = win32::IsAtLeastBuild(22000);
	system.isWindows11Rtm = win32::IsExactBuild(22000);
	if (const auto [version, hr] = win32::GetWindowsBuild(); SUCCEEDED(hr))
	{
		system.version = WindowsVersion { version.Major, version.Minor, version.Build, version.Revision };
	}

	USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
	USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
	if (IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine))
	{
		system.nativeArchitecture = MachineName(nativeMachine);
		system.processArchitecture = processMachine == IMAGE_FILE_MACHINE_UNKNOWN
			? system.nativeArchitecture : MachineName(processMachine);
	}
	else
	{
		system.nativeArchitecture = win32::GetProcessorArchitecture();
	}

	return system;
}
