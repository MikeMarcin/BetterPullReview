#ifndef BPR_TESTS_INSIGHTS_OBSERVER_HPP
#define BPR_TESTS_INSIGHTS_OBSERVER_HPP
#include "bpr/application/insights_observer.hpp"
#include <atomic>
#include <mutex>
namespace bpr::tests {
	class insights_observer final : public application::insights_observer
	{
	public:
		[[nodiscard]] auto foreground() const noexcept -> bool override { return Foreground.load(); }
		void               set_foreground(bool aForeground) { Foreground.store(aForeground); }
		void               record(core::insights_event aEvent) noexcept override
		{
			std::lock_guard lock{ Mutex };
			Events.push_back(std::move(aEvent));
		}
		void record_energy(core::insights_energy_sample aSample) noexcept override
		{
			std::lock_guard lock{ Mutex };
			Energy.push_back(std::move(aSample));
		}
		auto energy() -> std::vector<core::insights_energy_sample>
		{
			std::lock_guard lock{ Mutex };
			return Energy;
		}
		void record_work(core::insights_work_sample aSample) noexcept override
		{
			std::lock_guard lock{ Mutex };
			Work.push_back(std::move(aSample));
		}
		auto work() -> std::vector<core::insights_work_sample>
		{
			std::lock_guard lock{ Mutex };
			return Work;
		}
		void select(core::insights_scope) noexcept override { }
		void record_review(core::insights_review_event aEvent) noexcept override
		{
			std::lock_guard lock{ Mutex };
			Reviews.push_back(std::move(aEvent));
		}
		auto reviews() -> std::vector<core::insights_review_event>
		{
			std::lock_guard lock{ Mutex };
			return Reviews;
		}
		auto events() -> std::vector<core::insights_event>
		{
			std::lock_guard lock{ Mutex };
			return Events;
		}

	private:
		std::atomic_bool                          Foreground = false;
		std::mutex                                Mutex;
		std::vector<core::insights_work_sample>   Work;
		std::vector<core::insights_energy_sample> Energy;
		std::vector<core::insights_review_event>  Reviews;
		std::vector<core::insights_event>         Events;
	};
}
#endif
