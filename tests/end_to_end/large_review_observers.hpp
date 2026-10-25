#ifndef BPR_TESTS_END_TO_END_LARGE_REVIEW_OBSERVERS_HPP
#define BPR_TESTS_END_TO_END_LARGE_REVIEW_OBSERVERS_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
	#define BPR_LARGE_REVIEW_NATIVE_MEMORY 1
#else
	#define BPR_LARGE_REVIEW_NATIVE_MEMORY 0
#endif

namespace bpr::e2e::large_review {
	/// \brief One native process-memory observation ([[BPR-0967]]).
	/// \details Each platform pairs a current figure with the process-lifetime
	/// peak of the same figure, so peak growth above a sampled baseline is
	/// meaningful: macOS physical footprint and its ledger peak, Windows
	/// private commit and its peak, Linux resident set and its high-water
	/// mark. native_memory_method() names the exact counters.
	struct native_process_memory
	{
		std::uint64_t ResidentBytes = 0, FootprintBytes = 0, LifetimePeakFootprintBytes = 0;
	};

	/// Throws when the platform refuses the observation.
	[[nodiscard]] auto native_process_memory_sample() -> native_process_memory;
	/// Live heap bytes in use: every malloc zone on macOS, the process heap
	/// (which the UCRT allocates from) on Windows, glibc's in-use arena plus
	/// mmapped chunks on Linux. Zero where no method exists.
	[[nodiscard]] auto native_live_heap_bytes() -> std::uint64_t;
	[[nodiscard]] auto native_memory_method() -> const char*;

	struct native_allocator_release_result
	{
		const char*  Method    = "unavailable";
		std::int64_t Result    = 0;
		bool         Supported = false;
	};
	/// Requests unused memory from the selected allocator; benchmark use only.
	[[nodiscard]] auto native_allocator_release() -> native_allocator_release_result;

	/// \brief One reading of the clocks a course interval is attributed with
	/// ([[BPR-1598]]).
	/// \details A thread's wall time is its time on a processor, its time
	/// blocked (a timer, a lock, a pipe), and its time runnable while the
	/// scheduler ran something else. Only the last is the machine's: another
	/// process held the processors. ThreadWaitNs counts it for the thread
	/// native_schedule_observe_this_thread() named, and is zero on any other
	/// thread: on Linux from /proc/thread-self/schedstat run_delay, on macOS
	/// from that thread's run state and processor time sampled each
	/// millisecond (the kernel publishes runnable time only per process).
	/// Windows measures none. ThreadCpuNs is the calling thread's processor
	/// time, which load does not inflate.
	struct native_schedule_mark
	{
		std::int64_t  WallNs      = 0;
		std::uint64_t ThreadCpuNs = 0, ThreadWaitNs = 0;
	};
	struct native_thread_cpu
	{
		std::uint64_t Thread = 0, CpuNs = 0;
	};

	/// Starts measuring the calling thread's scheduler wait. Call once, from
	/// the thread whose intervals the course times; the measurement runs
	/// until the process exits.
	void               native_schedule_observe_this_thread();
	[[nodiscard]] auto native_schedule_now() -> native_schedule_mark;
	/// The time every thread of the process has spent runnable without a
	/// processor: macOS proc_pid_rusage ri_runnable_time (which also counts
	/// time on a processor) less ri_user_time and ri_system_time; Linux the
	/// run_delay of every /proc/self/task; zero on Windows. Summed over
	/// threads, it can exceed the wall time of the interval it covers.
	[[nodiscard]] auto native_process_wait_ns() -> std::uint64_t;
	/// Every live thread of the process with its processor time so far; empty
	/// where the platform has no method here (Windows).
	[[nodiscard]] auto native_thread_cpu_times() -> std::vector<native_thread_cpu>;
	/// True where scheduler waits are measured.
	[[nodiscard]] auto native_schedule_wait_measured() -> bool;
	[[nodiscard]] auto native_schedule_method() -> const char*;

	struct live_heap_observation
	{
		std::uint64_t BaselineBytes = 0, PeakBytes = 0, FinalBytes = 0, Samples = 0;
		double        MaxSampleIntervalMs = 0, MaxSampleCostMs = 0;
	};

	/// \brief Samples the live heap independently of the GUI event loop.
	/// \details A sampled peak is a lower bound on the true live-heap peak.
	/// The observed largest interval remains in the report; no timer can prove
	/// an unsampled allocation peak absent. Never uses the macOS SDK's
	/// max_size_in_use, which is touched memory, not live allocated bytes.
	class live_heap_sampler final
	{
	public:
		explicit live_heap_sampler(bool aEnabled)
		{
#if BPR_LARGE_REVIEW_NATIVE_MEMORY
			if (aEnabled) {
				Observation.BaselineBytes = current_bytes();
				Observation.PeakBytes     = Observation.BaselineBytes;
				Worker                    = std::jthread{ [this](std::stop_token aStop) { sample(aStop); } };
			}
#else
			static_cast<void>(aEnabled);
#endif
		}

		auto finish() -> live_heap_observation
		{
			if (Worker.joinable()) {
				Worker.request_stop();
				Worker.join();
				Observation.FinalBytes = current_bytes();
				Observation.PeakBytes  = std::max(Observation.PeakBytes, Observation.FinalBytes);
			}
			return Observation;
		}

		static auto current_bytes() -> std::uint64_t { return native_live_heap_bytes(); }

	private:
		void sample(std::stop_token aStop)
		{
			auto previous = std::chrono::steady_clock::now();
			while (!aStop.stop_requested()) {
				const auto began                = std::chrono::steady_clock::now();
				Observation.MaxSampleIntervalMs = std::max(Observation.MaxSampleIntervalMs,
														   std::chrono::duration<double, std::milli>{ began - previous }.count());
				Observation.PeakBytes           = std::max(Observation.PeakBytes, current_bytes());
				const auto cost                 = std::chrono::steady_clock::now() - began;
				Observation.MaxSampleCostMs     = std::max(Observation.MaxSampleCostMs, std::chrono::duration<double, std::milli>{ cost }.count());
				++Observation.Samples;
				previous = began;
				// [[BPR-1297]]. A sample holds the heap's lock while it counts the
				// heap, so the GUI thread's allocations wait for it. On Windows
				// HeapSummary cost 5-10 ms a sample in an optimized build and about
				// 14 ms in Debug, so a fixed 1 ms pause kept the lock taken 85-93%
				// of the time and slowed the course it measures (a Debug
				// cache-pressure selection: 135-372 s with it, 10.5-19.1 s
				// without). Waiting at least four times the last sample's cost
				// bounds that share to a fifth; a cheap sample still repeats every
				// millisecond. The interval actually reached stays in the report.
				std::this_thread::sleep_for(std::max<std::chrono::steady_clock::duration>(std::chrono::milliseconds{ 1 }, cost * 4));
			}
			Observation.MaxSampleIntervalMs = std::max(Observation.MaxSampleIntervalMs,
													   std::chrono::duration<double, std::milli>{ std::chrono::steady_clock::now() - previous }.count());
		}

		live_heap_observation Observation;
		std::jthread          Worker;
	};

	/// \brief A bounded, continuously entered fixture provider operation.
	/// \details The real host supplies its stop token. This gate models a
	/// waiting provider effect, not CPU occupancy or a live network operation.
	class held_provider_read final
	{
	public:
		static auto now_ns() -> std::int64_t
		{
			return std::chrono::duration_cast<std::chrono::nanoseconds>(
					   std::chrono::steady_clock::now().time_since_epoch())
				.count();
		}

		void wait(std::stop_token aStop)
		{
			const auto acknowledgement = std::stop_callback{ aStop, [this] { CancelledAt.store(now_ns()); } };
			EnteredAt.store(now_ns());
			auto lock = std::unique_lock{ Mutex };
			Condition.wait_for(lock, aStop, std::chrono::seconds{ 10 }, [] { return false; });
			Stopped.store(aStop.stop_requested());
			ExitedAt.store(now_ns());
		}

		std::atomic<std::int64_t> EnteredAt{ 0 }, CancelledAt{ 0 }, ExitedAt{ 0 };
		std::atomic<bool>         Stopped{ false };

	private:
		std::mutex                  Mutex;
		std::condition_variable_any Condition;
	};
}

#endif
