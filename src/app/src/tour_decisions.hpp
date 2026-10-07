#ifndef BPR_APP_TOUR_DECISIONS_HPP
#define BPR_APP_TOUR_DECISIONS_HPP
#include "bpr/application/port.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
namespace bpr::app {
	class terminal_decisions final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			{
				const std::lock_guard lock{ Mutex };
				Tasks.push_back(std::move(aTask));
			}
			Posted.notify_one();
		}
		/// [[BPR-1302]] Returns when a completion is queued or the slice ends.
		/// A host's completions arrive here, so a waiting terminal wakes on the
		/// next one instead of sleeping a fixed interval; the slice bounds how
		/// long a signal or a Qt timer waits to be served.
		void wait_for_post(std::chrono::milliseconds aSlice)
		{
			std::unique_lock lock{ Mutex };
			Posted.wait_for(lock, aSlice, [this] { return !Tasks.empty(); });
		}
		void drain()
		{
			for (;;) {
				std::function<void()> task;
				{
					const std::lock_guard lock{ Mutex };
					if (Tasks.empty()) {
						return;
					}
					task = std::move(Tasks.front());
					Tasks.pop_front();
				}
				task();
			}
		}

	private:
		std::mutex                        Mutex;
		std::condition_variable           Posted;
		std::deque<std::function<void()>> Tasks;
	};
}
#endif
