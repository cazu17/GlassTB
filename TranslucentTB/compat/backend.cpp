#include "backend.hpp"

compat::Backend compat::SelectBackend(TaskbarType type, const Capabilities &capabilities) noexcept
{
	switch (type)
	{
	case TaskbarType::Classic:
		return capabilities.classicAccentPolicy ? Backend::AccentPolicy : Backend::None;
	case TaskbarType::Mixed:
		return capabilities.classicAccentPolicy ? Backend::Mixed : Backend::None;
	case TaskbarType::XAML:
		return capabilities.xamlTaskbarService ? Backend::XamlTap : Backend::None;
	case TaskbarType::Unknown:
		return Backend::None;
	}

	return Backend::None;
}

std::wstring_view compat::ToString(TaskbarType type) noexcept
{
	switch (type)
	{
	case TaskbarType::Classic:
		return L"Classic";
	case TaskbarType::Mixed:
		return L"Mixed";
	case TaskbarType::XAML:
		return L"XAML";
	case TaskbarType::Unknown:
		return L"Unknown";
	}

	return L"Unknown";
}

std::wstring_view compat::ToString(Backend backend) noexcept
{
	switch (backend)
	{
	case Backend::AccentPolicy:
		return L"AccentPolicy";
	case Backend::XamlTap:
		return L"XamlTap";
	case Backend::Mixed:
		return L"Mixed";
	case Backend::None:
		return L"None";
	}

	return L"None";
}
