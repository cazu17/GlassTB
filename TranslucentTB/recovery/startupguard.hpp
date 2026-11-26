#pragma once
#include <filesystem>
#include <string>

// Writes a marker in %LOCALAPPDATA%\GlassTB while the app runs and removes it on clean
// exit. Repeated unclean exits within 10 minutes start the next run in safe mode.
class StartupGuard {
public:
	StartupGuard();
	~StartupGuard();

	StartupGuard(const StartupGuard &) = delete;
	StartupGuard &operator=(const StartupGuard &) = delete;

	bool SafeMode() const noexcept { return m_SafeMode; }
	const std::wstring &Reason() const noexcept { return m_Reason; }

	// Call right before exiting normally.
	void MarkCleanExit() noexcept;

private:
	std::filesystem::path m_Marker;
	bool m_SafeMode = false;
	std::wstring m_Reason;
};
