#include "github_application/window_review_lanes.hpp"

#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"

namespace bpr::app {
	window_queue_projection::window_queue_projection(sink aSink)
		: Sink{ std::move(aSink) }
	{ }

	void window_queue_projection::publish(const core::bpr_window_context& aContext, const application::queue_landing_projection& aProjection) { Sink(aContext, aProjection); }

	window_read_decision_lane::window_read_decision_lane(application::decision_executor& aDecision)
		: Decision{ aDecision }
	{ }

	void window_read_decision_lane::post(std::function<void()> aTask)
	{
		auto state = State;
		Decision.post([state, task = std::move(aTask)]() mutable {
			if (state->Held) {
				state->Pending.push_back(std::move(task));
			} else {
				task();
			}
		});
	}

	void window_read_decision_lane::hold(bool aHold)
	{
		State->Held = aHold;
		if (!aHold) {
			auto pending = std::move(State->Pending);
			State->Pending.clear();
			for (auto& task : pending) {
				Decision.post(std::move(task));
			}
		}
	}

	auto window_read_decision_lane::held() const noexcept -> std::size_t { return State->Pending.size(); }

	effect_window_review_store::effect_window_review_store(std::filesystem::path aRoot)
		: Root{ std::move(aRoot) }
	{ }

	auto effect_window_review_store::load(std::stop_token aStop) -> application::window_review_store_result<application::window_review_snapshot>
	{
		auto store = adapters::sqlite::make_window_review_restoration_store(Root);
		return store->load(aStop);
	}

	auto effect_window_review_store::save(const application::window_review_snapshot& aCandidate, std::uint64_t aRevision, std::stop_token aStop) -> application::window_review_store_result<application::window_review_snapshot>
	{
		auto store = adapters::sqlite::make_window_review_restoration_store(Root);
		return store->save(aCandidate, aRevision, aStop);
	}

	exclusive_file_effect_executor::exclusive_file_effect_executor(application::effect_executor& aLane, std::mutex& aFile) noexcept
		: Lane{ aLane }
		, File{ aFile }
	{
	}

	void exclusive_file_effect_executor::post(std::function<void()> aTask)
	{
		Lane.post([&file = File, task = std::move(aTask)] {
			const auto lock = std::scoped_lock{ file };
			task();
		});
	}
}
