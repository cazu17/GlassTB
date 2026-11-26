#pragma once

enum class TaskbarType {
	Unknown,
	Classic, // Windows 10
	Mixed,   // Windows 11 22000 and early 22621
	XAML     // Windows 11 22621+
};

namespace compat {
struct SystemInfo;

struct Capabilities {
	bool blurAccentState = false;
	bool xamlTaskbarService = false;
	bool classicAccentPolicy = false;
};

Capabilities ComputeCapabilities(const SystemInfo &system, TaskbarType type) noexcept;
}
