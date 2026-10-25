// [[BPR-0967]]. Native process-memory counters for the large-review courses,
// and since [[BPR-1598]] the scheduling clocks their timed intervals are
// attributed with. Platform headers stay in this translation unit so Windows
// macros never reach the benchmark's Qt and application headers.
#include "large_review_observers.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <stdexcept>

#if defined(BPR_BENCH_MIMALLOC)
	#include <mimalloc.h>
#endif

#if defined(__APPLE__)
	#include <libproc.h>
	#include <mach/mach.h>
	#include <mach/mach_time.h>
	#include <malloc/malloc.h>
	#include <pthread.h>
	#include <sys/resource.h>
	#include <unistd.h>

	#include <atomic>
	#include <optional>
	#include <thread>
#elif defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	// windows.h supplies the base types psapi.h assumes are declared.
	#include <windows.h>

	#include <malloc.h>
	#include <psapi.h>
#elif defined(__linux__)
	#include <malloc.h>

	#include <filesystem>
	#include <fstream>
	#include <optional>
	#include <string>
#endif

namespace bpr::e2e::large_review {
	auto native_allocator_release() -> native_allocator_release_result
	{
#if defined(BPR_BENCH_MIMALLOC)
		mi_collect(true);
		return { .Method = "mi_collect(true)", .Supported = true };
#elif defined(__APPLE__)
		const auto result = malloc_zone_pressure_relief(nullptr, 0);
		return { .Method = "malloc_zone_pressure_relief(all zones)", .Result = static_cast<std::int64_t>(result), .Supported = true };
#elif defined(__linux__)
		return { .Method = "malloc_trim(0)", .Result = malloc_trim(0), .Supported = true };
#elif defined(_WIN32)
		return { .Method = "_heapmin()", .Result = _heapmin(), .Supported = true };
#else
		return { };
#endif
	}

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

#if defined(__APPLE__)
	namespace {
		/// proc_pid_rusage reports its times in Mach absolute-time units.
		auto mach_nanoseconds(std::uint64_t aTicks) -> std::uint64_t
		{
			static const auto base = [] {
				mach_timebase_info_data_t information{ };
				mach_timebase_info(&information);
				return information;
			}();
			return aTicks * base.numer / base.denom;
		}

		struct thread_state
		{
			std::int64_t  WallNs   = 0;
			std::uint64_t CpuNs    = 0;
			bool          Runnable = false;
		};

		auto read_thread_state(thread_act_t aThread) -> std::optional<thread_state>
		{
			thread_basic_info_data_t basic{ };
			mach_msg_type_number_t   count = THREAD_BASIC_INFO_COUNT;
			const auto               wall  = std::chrono::steady_clock::now().time_since_epoch();
			if (thread_info(aThread, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&basic), &count) != KERN_SUCCESS) {
				return std::nullopt;
			}
			const auto microseconds = (static_cast<std::uint64_t>(basic.user_time.seconds) + static_cast<std::uint64_t>(basic.system_time.seconds)) * 1'000'000U
				+ static_cast<std::uint64_t>(basic.user_time.microseconds) + static_cast<std::uint64_t>(basic.system_time.microseconds);
			return thread_state{ .WallNs   = std::chrono::duration_cast<std::chrono::nanoseconds>(wall).count(),
								 .CpuNs    = microseconds * 1'000U,
								 .Runnable = basic.run_state == TH_STATE_RUNNING };
		}

		/// [[BPR-1598]]. macOS keeps runnable time per process only, and an
		/// idle application's own threads (timers, pools) can wait for a
		/// processor as long as a stalled GUI thread sleeps: at a load
		/// average of 68 the process waited 131 ms through a 120 ms sleep of
		/// the GUI thread. So the observed thread's wait is sampled: Mach
		/// reports TH_STATE_RUNNING both on a processor and queued for one,
		/// and between two samples that both find it so, the time it did not
		/// spend on a processor was spent waiting for one. An interval with a
		/// blocked end counts nothing, so a sleep, a lock or a pipe is never
		/// taken for the scheduler's; a wait shorter than a sample is missed,
		/// which only leaves an interval longer.
		class observed_thread_wait final
		{
		public:
			void observe(thread_act_t aThread, pthread_t aIdentity)
			{
				if (Worker.joinable()) {
					return;
				}
				Identity = aIdentity;
				Worker   = std::jthread{ [this, aThread](std::stop_token aStop) { sample(aThread, aStop); } };
			}

			[[nodiscard]] auto wait_ns_for_calling_thread() const -> std::uint64_t
			{
				return Worker.joinable() && pthread_equal(Identity, pthread_self()) != 0 ? Wait.load(std::memory_order_relaxed) : 0U;
			}

		private:
			void sample(thread_act_t aThread, std::stop_token aStop)
			{
				auto previous = read_thread_state(aThread);
				while (!aStop.stop_requested() && previous) {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
					const auto current = read_thread_state(aThread);
					if (!current) {
						return;
					}
					const auto wall = static_cast<std::uint64_t>(std::max<std::int64_t>(0, current->WallNs - previous->WallNs));
					const auto cpu  = current->CpuNs > previous->CpuNs ? current->CpuNs - previous->CpuNs : 0U;
					if (previous->Runnable && current->Runnable && wall > cpu) {
						Wait.fetch_add(wall - cpu, std::memory_order_relaxed);
					}
					previous = current;
				}
			}

			pthread_t                  Identity{ };
			std::atomic<std::uint64_t> Wait{ 0U };
			std::jthread               Worker;
		};

		auto observed_wait() -> observed_thread_wait&
		{
			static observed_thread_wait observer;
			return observer;
		}
	}
#endif

	auto native_schedule_method() -> const char*
	{
#if defined(__APPLE__)
		return "macOS: thread wait is the observed thread's time in TH_STATE_RUNNING off a processor, from THREAD_BASIC_INFO "
			   "run state and processor time sampled each millisecond; process wait is proc_pid_rusage RUSAGE_INFO_V4 "
			   "ri_runnable_time less ri_user_time and ri_system_time; thread CPU is CLOCK_THREAD_CPUTIME_ID; per-thread CPU is "
			   "task_threads THREAD_BASIC_INFO";
#elif defined(_WIN32)
		return "Windows: no wait is measured, so an interval is its wall time; thread CPU is GetThreadTimes";
#elif defined(__linux__)
		return "Linux: thread wait is /proc/thread-self/schedstat run_delay; process wait sums run_delay over /proc/self/task; "
			   "thread CPU is CLOCK_THREAD_CPUTIME_ID; per-thread CPU is /proc/self/task/*/schedstat";
#else
		return "unavailable: an interval is its wall time";
#endif
	}

	auto native_schedule_wait_measured() -> bool
	{
#if defined(__APPLE__)
		return true;
#elif defined(__linux__)
		static const bool measured = [] {
			std::ifstream statistics{ "/proc/thread-self/schedstat" };
			std::uint64_t running = 0, waiting = 0;
			return static_cast<bool>(statistics >> running >> waiting);
		}();
		return measured;
#else
		return false;
#endif
	}

	void native_schedule_observe_this_thread()
	{
#if defined(__APPLE__)
		observed_wait().observe(pthread_mach_thread_np(pthread_self()), pthread_self());
#endif
	}

	auto native_schedule_now() -> native_schedule_mark
	{
		auto mark   = native_schedule_mark{ };
		mark.WallNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#if defined(__APPLE__)
		mark.ThreadCpuNs  = clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID);
		mark.ThreadWaitNs = observed_wait().wait_ns_for_calling_thread();
#elif defined(_WIN32)
		FILETIME created{ }, exited{ }, kernel{ }, user{ };
		if (GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user) != FALSE) {
			const auto ticks = [](const FILETIME& aTime) {
				return (static_cast<std::uint64_t>(aTime.dwHighDateTime) << 32U) | aTime.dwLowDateTime;
			};
			mark.ThreadCpuNs = (ticks(kernel) + ticks(user)) * 100U;
		}
#elif defined(__linux__)
		timespec cpu{ };
		if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu) == 0) {
			mark.ThreadCpuNs = static_cast<std::uint64_t>(cpu.tv_sec) * 1'000'000'000U + static_cast<std::uint64_t>(cpu.tv_nsec);
		}
		std::ifstream statistics{ "/proc/thread-self/schedstat" };
		std::uint64_t running = 0, waiting = 0;
		if (statistics >> running >> waiting) {
			mark.ThreadWaitNs = waiting;
		}
#endif
		return mark;
	}

	auto native_process_wait_ns() -> std::uint64_t
	{
#if defined(__APPLE__)
		rusage_info_v4 usage{ };
		if (proc_pid_rusage(getpid(), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t*>(&usage)) != 0) {
			return 0U;
		}
		const auto onProcessor = usage.ri_user_time + usage.ri_system_time;
		return usage.ri_runnable_time > onProcessor ? mach_nanoseconds(usage.ri_runnable_time - onProcessor) : 0U;
#elif defined(__linux__)
		auto            total = std::uint64_t{ 0U };
		std::error_code failure;
		for (const auto& entry : std::filesystem::directory_iterator{ "/proc/self/task", failure }) {
			std::ifstream statistics{ entry.path() / "schedstat" };
			std::uint64_t running = 0, waiting = 0;
			if (statistics >> running >> waiting) {
				total += waiting;
			}
		}
		return total;
#else
		return 0U;
#endif
	}

	auto native_thread_cpu_times() -> std::vector<native_thread_cpu>
	{
		auto result = std::vector<native_thread_cpu>{ };
#if defined(__APPLE__)
		thread_act_array_t     threads = nullptr;
		mach_msg_type_number_t count   = 0;
		if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS) {
			return result;
		}
		result.reserve(count);
		for (mach_msg_type_number_t index = 0; index < count; ++index) {
			thread_identifier_info_data_t identity{ };
			mach_msg_type_number_t        identityCount = THREAD_IDENTIFIER_INFO_COUNT;
			const auto                    state         = read_thread_state(threads[index]);
			if (state && thread_info(threads[index], THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&identity), &identityCount) == KERN_SUCCESS) {
				result.push_back({ .Thread = identity.thread_id, .CpuNs = state->CpuNs });
			}
			mach_port_deallocate(mach_task_self(), threads[index]);
		}
		vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), count * sizeof(thread_act_t));
#elif defined(__linux__)
		std::error_code failure;
		for (const auto& entry : std::filesystem::directory_iterator{ "/proc/self/task", failure }) {
			std::ifstream statistics{ entry.path() / "schedstat" };
			std::uint64_t running = 0;
			if (statistics >> running) {
				result.push_back({ .Thread = std::stoull(entry.path().filename().string()), .CpuNs = running });
			}
		}
#endif
		return result;
	}
}
