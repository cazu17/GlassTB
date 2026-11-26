#include "log.hpp"
#include <algorithm>
#include <chrono>
#include <ShlObj.h>
#include <vector>
#include <wil/resource.h>
#include <debugapi.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/logger.h>
#include "winrt.hpp"

#include "appinfo.hpp"
#include "config/config.hpp"
#include "error/error.hpp"
#include "error/std.hpp"
#include "error/win32.hpp"
#include "error/winrt.hpp"
#include "error/std.hpp"

std::atomic<Log::InitStatus> Log::s_LogInitStatus = Log::InitStatus::NotInitialized;
std::weak_ptr<lazy_file_sink_mt> Log::s_LogSink;

std::time_t Log::GetProcessCreationTime() noexcept
{
	FILETIME creationTime, exitTime, kernelTime, userTime;
	if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
	{
		using winrt::clock;
		return clock::to_time_t(clock::from_file_time(creationTime));
	}
	else
	{
		// Fallback to current time
		using std::chrono::system_clock;
		return system_clock::to_time_t(system_clock::now());
	}
}

std::filesystem::path Log::GetPath(const std::optional<std::filesystem::path> &)
{
	// GlassTB keeps its logs next to its other data, in %LOCALAPPDATA%\GlassTB\logs
	// (redirected to the package folder for MSIX installs). Old logs are pruned so the
	// folder does not grow without bound.
	static constexpr std::size_t LOGS_TO_KEEP = 10;

	wil::unique_cotaskmem_string localAppData;
	if (const HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, localAppData.put()); FAILED(hr))
	{
		HresultHandle(hr, spdlog::level::err, L"Failed to get log file path. Logs won't be available during this session");
		return { };
	}

	std::filesystem::path path = localAppData.get();
	path /= L"GlassTB";
	path /= L"logs";

	std::error_code code;
	std::filesystem::create_directories(path, code);
	if (code)
	{
		StdErrorCodeHandle(code, spdlog::level::err, L"Failed to create log folder. Logs won't be available during this session");
		return { };
	}

	std::vector<std::filesystem::path> logs;
	for (std::filesystem::directory_iterator it(path, code), end; !code && it != end; it.increment(code))
	{
		if (it->path().extension() == L".log")
		{
			logs.push_back(it->path());
		}
	}
	if (logs.size() >= LOGS_TO_KEEP)
	{
		// File names are process creation timestamps, so lexicographic order is chronological
		// for names of equal length.
		std::ranges::sort(logs, [](const auto &a, const auto &b)
		{
			const auto &na = a.native(), &nb = b.native();
			return na.size() != nb.size() ? na.size() < nb.size() : na < nb;
		});
		for (std::size_t i = 0; i + LOGS_TO_KEEP - 1 < logs.size(); ++i)
		{
			std::filesystem::remove(logs[i], code);
		}
	}

	path /= std::format(L"{}.log", GetProcessCreationTime());
	return path;
}

void Log::LogErrorHandler(const std::string &message)
{
	if (IsDebuggerPresent())
	{
		static std::atomic_size_t counter = 0;
		OutputDebugStringA(std::format("[*** LOG ERROR {:04} ***] {}\n", counter++, message).c_str());
	}
}

bool Log::IsInitialized() noexcept
{
	return s_LogInitStatus == InitStatus::Initialized;
}

std::shared_ptr<lazy_file_sink_mt> Log::GetSink() noexcept
{
	if (IsInitialized())
	{
		return s_LogSink.lock();
	}
	else
	{
		return nullptr;
	}
}

void Log::Initialize(const std::optional<std::filesystem::path> &storageFolder)
{
	auto expected = InitStatus::NotInitialized;
	if (s_LogInitStatus.compare_exchange_strong(expected, InitStatus::Initializing))
	{
		auto defaultLogger = std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::windebug_sink_st>());

		if (auto path = GetPath(storageFolder); !path.empty())
		{
			auto fileLog = std::make_shared<lazy_file_sink_mt>(std::move(path));
			fileLog->set_level(Config::DEFAULT_LOG_VERBOSITY);
			s_LogSink = fileLog;

			defaultLogger->sinks().push_back(std::move(fileLog));
		}

		spdlog::set_formatter(std::make_unique<spdlog::pattern_formatter>(spdlog::pattern_time_type::utc));
		spdlog::set_level(spdlog::level::trace);
		spdlog::flush_on(spdlog::level::off);
		spdlog::set_error_handler(LogErrorHandler);
		spdlog::initialize_logger(defaultLogger);
		spdlog::set_default_logger(std::move(defaultLogger));

		s_LogInitStatus = InitStatus::Initialized;
	}
	else
	{
		throw std::logic_error("Log::Initialize should only be called once");
	}
}
