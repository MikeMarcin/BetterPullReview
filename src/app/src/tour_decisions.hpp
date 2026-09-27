#ifndef BPR_APP_TOUR_DECISIONS_HPP
#define BPR_APP_TOUR_DECISIONS_HPP
#include "bpr/application/port.hpp"
#include <deque>
#include <mutex>
namespace bpr::app {
	class terminal_decisions final : public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override
		{
			const std::lock_guard lock{ Mutex };
			Tasks.push_back(std::move(aTask));
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
		std::deque<std::function<void()>> Tasks;
	};
}
#endif
