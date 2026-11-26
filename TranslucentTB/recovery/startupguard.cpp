#include "startupguard.hpp"
#include <chrono>
#include <fstream>
#include <iterator>
#include <ShlObj.h>
#include <wil/resource.h>

#include "recovery/crashloop.hpp"
#include "../../ProgramLog/error/win32.hpp"
#include "../../ProgramLog/log.hpp"

StartupGuard::StartupGuard()
{
	wil::unique_cotaskmem_string localAppData;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, localAppData.put())))
	{
		std::error_code errc;
		const auto folder = std::filesystem::path(localAppData.get()) / L"GlassTB";
		std::filesystem::create_directories(folder, errc);
		m_Marker = folder / L"running.marker";
	}

	const std::wstring_view commandLine = GetCommandLineW();
	if (commandLine.find(L"--safe-mode") != std::wstring_view::npos)
	{
		m_SafeMode = true;
		m_Reason = L"--safe-mode";
	}
	else if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		m_SafeMode = true;
		m_Reason = L"Shift";
	}

	if (m_Marker.empty())
	{
		return;
	}

	std::error_code errc;
	const bool previousUnclean = std::filesystem::exists(m_Marker, errc);
	std::string text;
	if (previousUnclean)
	{
		std::ifstream in(m_Marker, std::ios::binary);
		text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	auto decision = recovery::Evaluate(recovery::ParseMarker(text), previousUnclean, now);
	if (decision.safeMode && !m_SafeMode)
	{
		m_SafeMode = true;
		m_Reason = L"crash-loop";
		MessagePrint(spdlog::level::warn, L"Several unclean exits in a short time: starting in safe mode");
	}

	decision.history.push_back(now);
	std::ofstream(m_Marker, std::ios::binary | std::ios::trunc) << recovery::FormatMarker(decision.history);
}

StartupGuard::~StartupGuard() = default;

void StartupGuard::MarkCleanExit() noexcept
{
	if (!m_Marker.empty())
	{
		std::error_code errc;
		std::filesystem::remove(m_Marker, errc);
	}
}
