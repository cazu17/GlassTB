#include "processmetrics.hpp"
#include "arch.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wil/resource.h>

#include "wilx.hpp"

namespace {
	std::uint64_t ToUInt(const FILETIME &ft) noexcept
	{
		return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	}
}

ProcessMetrics::Sample ProcessMetrics::Take()
{
	Sample sample;
	const HANDLE process = GetCurrentProcess();

	FILETIME creation, exit, kernel, user, now;
	GetSystemTimeAsFileTime(&now);
	if (GetProcessTimes(process, &creation, &exit, &kernel, &user))
	{
		const std::uint64_t cpu = ToUInt(kernel) + ToUInt(user);
		const std::uint64_t wall = ToUInt(now);
		if (m_LastWall != 0 && wall > m_LastWall)
		{
			SYSTEM_INFO info;
			GetSystemInfo(&info);
			const double cores = info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
			sample.cpuPercent = 100.0 * static_cast<double>(cpu - m_LastCpu) / static_cast<double>(wall - m_LastWall) / cores;
		}
		m_LastCpu = cpu;
		m_LastWall = wall;
	}

	PROCESS_MEMORY_COUNTERS_EX memory = { .cb = sizeof(memory) };
	if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), sizeof(memory)))
	{
		sample.privateBytes = memory.PrivateUsage;
		sample.workingSet = memory.WorkingSetSize;
	}

	DWORD handles = 0;
	if (GetProcessHandleCount(process, &handles))
	{
		sample.handles = handles;
	}
	sample.gdiObjects = GetGuiResources(process, GR_GDIOBJECTS);
	sample.userObjects = GetGuiResources(process, GR_USEROBJECTS);

	if (const wil::unique_tool_help_snapshot snapshot { CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0) })
	{
		const DWORD pid = GetCurrentProcessId();
		THREADENTRY32 entry = { .dwSize = sizeof(entry) };
		for (BOOL ok = Thread32First(snapshot.get(), &entry); ok; ok = Thread32Next(snapshot.get(), &entry))
		{
			if (entry.th32OwnerProcessID == pid)
			{
				++sample.threads;
			}
		}
	}

	return sample;
}
