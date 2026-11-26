#pragma once
#include <cstdint>

// Samples real resource usage of the GlassTB process (engine and settings UI share it).
class ProcessMetrics {
public:
	struct Sample {
		double cpuPercent = 0;          // average over the interval since the previous sample, all cores = 100 %
		std::uint64_t privateBytes = 0;
		std::uint64_t workingSet = 0;
		std::uint32_t handles = 0;
		std::uint32_t threads = 0;
		std::uint32_t gdiObjects = 0;
		std::uint32_t userObjects = 0;
	};

	Sample Take();

private:
	std::uint64_t m_LastCpu = 0;  // 100 ns units (kernel + user)
	std::uint64_t m_LastWall = 0; // 100 ns units
};
