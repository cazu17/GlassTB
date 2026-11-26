#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace compat {
struct WindowsVersion {
	std::uint16_t major;
	std::uint16_t minor;
	std::uint16_t build;
	std::uint16_t revision;
};

struct SystemInfo {
	std::optional<WindowsVersion> version;
	bool isWindows11 = false;
	bool isWindows11Rtm = false;
	std::wstring_view nativeArchitecture = L"Unknown";
	std::wstring_view processArchitecture = L"Unknown";
};

SystemInfo GetSystemInfo();
}
