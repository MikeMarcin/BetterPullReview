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

#if defined(__APPLE__)
	#include <malloc/malloc.h>
#endif

namespace bpr::e2e::large_review {
	struct live_heap_observation
	{
		std::uint64_t BaselineBytes = 0, PeakBytes = 0, FinalBytes = 0, Samples = 0;
		double        MaxSampleIntervalMs = 0, MaxSampleCostMs = 0;
	};

	/// \brief Samples all malloc zones independently of the GUI event loop.
	/// \details A sampled peak is a lower bound on the true live-heap peak.
	/// The observed largest interval remains in the report; no timer can prove
	/// an unsampled allocation peak absent. Never uses max_size_in_use, which
	/// the SDK defines as touched memory, not live allocated bytes.
	class live_heap_sampler final
	{
	public:
		explicit live_heap_sampler(bool aEnabled)
		{
#if defined(__APPLE__)
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

		static auto current_bytes() -> std::uint64_t
		{
#if defined(__APPLE__)
			malloc_statistics_t statistics{ };
			malloc_zone_statistics(nullptr, &statistics);
			return statistics.size_in_use;
#else
			return 0;
#endif
		}

	private:
		void sample(std::stop_token aStop)
		{
			auto previous = std::chrono::steady_clock::now();
			while (!aStop.stop_requested()) {
				const auto began                = std::chrono::steady_clock::now();
				Observation.MaxSampleIntervalMs = std::max(Observation.MaxSampleIntervalMs,
														   std::chrono::duration<double, std::milli>{ began - previous }.count());
				Observation.PeakBytes           = std::max(Observation.PeakBytes, current_bytes());
				Observation.MaxSampleCostMs     = std::max(Observation.MaxSampleCostMs,
														   std::chrono::duration<double, std::milli>{ std::chrono::steady_clock::now() - began }.count());
				++Observation.Samples;
				previous = began;
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
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
