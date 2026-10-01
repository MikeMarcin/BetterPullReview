// [[BPR-0967]]. Native process-memory counters for the large-review courses.
// Platform headers stay in this translation unit so Windows macros never reach
// the benchmark's Qt and application headers.
#include "large_review_observers.hpp"

#include <cstdint>
#include <stdexcept>

#if defined(__APPLE__)
	#include <mach/mach.h>
	#include <malloc/malloc.h>
#elif defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	// windows.h supplies the base types psapi.h assumes are declared.
	#include <windows.h>

	#include <psapi.h>
#elif defined(__linux__)
	#include <malloc.h>

	#include <fstream>
	#include <optional>
	#include <string>
#endif

namespace bpr::e2e::large_review {
	auto native_memory_method() -> const char*
	{
#if defined(__APPLE__)
		return "macOS TASK_VM_INFO phys_footprint with ledger_phys_footprint_peak and resident_size; "
			   "live heap is malloc_zone_statistics size_in_use over every zone";
#elif defined(_WIN32)
		return "Windows GetProcessMemoryInfo PrivateUsage (commit charge) with PeakPagefileUsage "
			   "(process-lifetime peak commit) and WorkingSetSize; live heap is "
			   "HeapSummary(GetProcessHeap()).cbAllocated, the heap the UCRT allocates from";
#elif defined(__linux__)
		return "Linux /proc/self/status VmRSS with VmHWM (process-lifetime peak resident set); "
			   "live heap is glibc mallinfo2 uordblks plus hblkhd";
#else
		return "unavailable";
#endif
	}

	auto native_process_memory_sample() -> native_process_memory
	{
#if defined(__APPLE__)
		task_vm_info_data_t    information{ };
		mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
		if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&information), &count) != KERN_SUCCESS) {
			throw std::runtime_error{ "native process memory sample failed" };
		}
		if (count < TASK_VM_INFO_REV3_COUNT || information.ledger_phys_footprint_peak < 0) {
			throw std::runtime_error{ "native process peak footprint sample unavailable" };
		}
		return { .ResidentBytes              = information.resident_size,
				 .FootprintBytes             = information.phys_footprint,
				 .LifetimePeakFootprintBytes = static_cast<std::uint64_t>(information.ledger_phys_footprint_peak) };
#elif defined(_WIN32)
		PROCESS_MEMORY_COUNTERS_EX counters{ };
		counters.cb = sizeof(counters);
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) == FALSE) {
			throw std::runtime_error{ "native process memory sample failed" };
		}
		return { .ResidentBytes              = counters.WorkingSetSize,
				 .FootprintBytes             = counters.PrivateUsage,
				 .LifetimePeakFootprintBytes = counters.PeakPagefileUsage };
#elif defined(__linux__)
		std::ifstream                status{ "/proc/self/status" };
		std::optional<std::uint64_t> resident, highWater;
		for (std::string line; std::getline(status, line);) {
			// "VmRSS:	  123456 kB"
			const auto kibibytes = [&line](const char* aField) -> std::optional<std::uint64_t> {
				const auto field = std::string{ aField };
				if (!line.starts_with(field)) {
					return std::nullopt;
				}
				return std::stoull(line.substr(field.size())) * 1024U;
			};
			resident  = resident ? resident : kibibytes("VmRSS:");
			highWater = highWater ? highWater : kibibytes("VmHWM:");
		}
		if (!resident || !highWater) {
			throw std::runtime_error{ "native process memory sample failed" };
		}
		return { .ResidentBytes = *resident, .FootprintBytes = *resident, .LifetimePeakFootprintBytes = *highWater };
#else
		throw std::runtime_error{ "no native process memory observation on this platform" };
#endif
	}

	auto native_live_heap_bytes() -> std::uint64_t
	{
#if defined(__APPLE__)
		malloc_statistics_t statistics{ };
		malloc_zone_statistics(nullptr, &statistics);
		return statistics.size_in_use;
#elif defined(_WIN32)
		HEAP_SUMMARY summary{ };
		summary.cb = sizeof(summary);
		if (HeapSummary(GetProcessHeap(), 0, &summary) == FALSE) {
			return 0;
		}
		return summary.cbAllocated;
#elif defined(__linux__)
		const auto statistics = mallinfo2();
		return statistics.uordblks + statistics.hblkhd;
#else
		return 0;
#endif
	}
}
