#ifndef BPR_TESTS_INSIGHTS_OBSERVER_HPP
#define BPR_TESTS_INSIGHTS_OBSERVER_HPP
#include "bpr/application/insights_observer.hpp"
#include <mutex>
namespace bpr::tests {
	class insights_observer final : public application::insights_observer
	{
	public:
		void record(core::insights_event aEvent) noexcept override
		{
			std::lock_guard lock{ Mutex };
			Events.push_back(std::move(aEvent));
		}
		void select(core::insights_scope) noexcept override { }
		auto events() -> std::vector<core::insights_event>
		{
			std::lock_guard lock{ Mutex };
			return Events;
		}

	private:
		std::mutex                        Mutex;
		std::vector<core::insights_event> Events;
	};
}
#endif
