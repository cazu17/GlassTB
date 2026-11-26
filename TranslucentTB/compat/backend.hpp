#pragma once
#include <string_view>

#include "capabilities.hpp"

namespace compat {
enum class Backend {
	None,
	AccentPolicy,
	XamlTap,
	Mixed
};

Backend SelectBackend(TaskbarType type, const Capabilities &capabilities) noexcept;
std::wstring_view ToString(TaskbarType type) noexcept;
std::wstring_view ToString(Backend backend) noexcept;
}
