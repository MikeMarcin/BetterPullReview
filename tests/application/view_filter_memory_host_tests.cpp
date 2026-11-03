// [[BPR-1844]]. The profile's remembered view filters: one memory for every
// window, written off the decision lane, latest first, restored on relaunch.
#include "bpr/application/view_filter_memory_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }

		auto run_one() -> bool
		{
			if (Tasks.empty()) {
				return false;
			}
			auto task = std::move(Tasks.front());
			Tasks.pop_front();
			task();
			return true;
		}

		std::deque<std::function<void()>> Tasks;
	};

	/// An in-memory row: what a store holds across a relaunch.
	class memory_store final : public application::view_filter_memory_port
	{
	public:
		auto store(const core::view_filter_defaults& aDefaults) -> bool override
		{
			Writes.push_back(aDefaults);
			if (Fail) {
				return false;
			}
			Row = core::stored_view_filter_memory(aDefaults);
			return true;
		}
		[[nodiscard]] auto load() const -> core::view_filter_memory_read { return core::read_view_filter_memory(Row); }

		std::optional<core::stored_view_filter_row> Row;
		std::vector<core::view_filter_defaults>     Writes;
		bool                                        Fail = false;
	};

	void drain(manual_executor& aEffects, manual_executor& aDecisions)
	{
		while (aEffects.run_one() || aDecisions.run_one()) { }
	}

	constexpr auto only_new = core::view_filter_choice{ .OnlyNew = true, .HideResolved = false };
	constexpr auto hide     = core::view_filter_choice{ .OnlyNew = false, .HideResolved = true };
	constexpr auto both     = core::view_filter_choice{ .OnlyNew = true, .HideResolved = true };

	void test_windows_diverge_and_the_next_pull_request_carries_the_memory()
	{
		auto       store     = memory_store{ };
		auto       effects   = manual_executor{ };
		auto       decisions = manual_executor{ };
		auto       host      = application::view_filter_memory_host{ store, effects, decisions, store.load() };
		const auto first     = application::first_filter_choice(application::review_comments_filter::all);

		// Two windows open their pull requests from the same memory.
		auto windowA = core::open_view_filters(host.defaults(), first);
		auto windowB = core::open_view_filters(host.defaults(), first);
		expect(windowA == core::view_filter_state{ } && windowB == windowA, "a fresh profile opens every view unfiltered");

		// A chooses Hide resolved on Discuss and Only new on Review's Comments.
		for (const auto& [view, choice] : { std::pair{ core::filter_view::discuss, hide }, std::pair{ core::filter_view::review_comments, only_new } }) {
			const auto step = core::choose_view_filter(windowA, view, choice);
			windowA         = step.State;
			host.remember(*step.Remember);
		}
		expect(host.defaults().Discuss == hide && host.defaults().ReviewComments == only_new, "the memory changes at once");
		expect(windowB == core::view_filter_state{ }, "B, already open, is not told: its filters are its own");

		// B chooses on Threads; A's choices stay remembered.
		const auto stepB = core::choose_view_filter(windowB, core::filter_view::threads, both);
		windowB          = stepB.State;
		host.remember(*stepB.Remember);
		expect(windowA.Threads == core::view_filter_choice{ } && windowB.Discuss == core::view_filter_choice{ }, "neither window took the other's filters");

		// A opens another pull request: it starts from the memory, B's
		// Threads choice included.
		windowA = core::open_view_filters(host.defaults(), first);
		expect(windowA.Discuss == hide && windowA.Threads == both && windowA.ReviewComments == only_new, "the next pull request opens with the last choice of each view");

		// Writes run off the decision lane: one running, the latest waiting.
		expect(store.Writes.empty() && effects.Tasks.size() == 1U, "one write is posted; the others wait for it");
		expect(!host.quiescent(), "a write is pending");
		drain(effects, decisions);
		expect(store.Writes.size() == 2U, "a burst of choices costs the running write and one more");
		expect(store.Writes.back() == host.defaults() && host.quiescent() && host.durable(), "the last write carries the latest memory");
	}

	void test_relaunch_restores_the_memory()
	{
		auto store = memory_store{ };
		{
			auto effects   = manual_executor{ };
			auto decisions = manual_executor{ };
			auto host      = application::view_filter_memory_host{ store, effects, decisions, store.load() };
			host.remember({ .View = core::filter_view::threads, .Choice = only_new });
			host.remember({ .View = core::filter_view::review_comments, .Choice = core::view_filter_choice{ } });
			drain(effects, decisions);
		}
		auto effects   = manual_executor{ };
		auto decisions = manual_executor{ };
		auto relaunch  = application::view_filter_memory_host{ store, effects, decisions, store.load() };
		expect(relaunch.defaults().Threads == only_new && relaunch.defaults().ReviewComments == core::view_filter_choice{ } && !relaunch.defaults().Discuss,
			   "a relaunched profile reads the memory it wrote");
		const auto opened = core::open_view_filters(relaunch.defaults(), application::first_filter_choice(application::review_comments_filter::unresolved));
		expect(opened.Threads == only_new && opened.ReviewComments == core::view_filter_choice{ }, "a remembered Review Comments choice wins over the first-filter preference");
		const auto unseeded = core::open_view_filters({ }, application::first_filter_choice(application::review_comments_filter::unresolved));
		expect(unseeded.ReviewComments == hide, "until one is remembered, the preference still names Review's Comments' first filter");
	}

	void test_unwritable_and_failing_stores()
	{
		auto store     = memory_store{ };
		store.Row      = core::stored_view_filter_row{ .Version = core::view_filter_memory_version + 1, .Discuss = 3 };
		auto effects   = manual_executor{ };
		auto decisions = manual_executor{ };
		auto newer     = application::view_filter_memory_host{ store, effects, decisions, store.load() };
		newer.remember({ .View = core::filter_view::discuss, .Choice = only_new });
		expect(newer.defaults().Discuss == only_new, "a newer build's row still lets this session remember");
		expect(effects.Tasks.empty() && store.Row->Version == core::view_filter_memory_version + 1, "but the newer row is never overwritten");

		auto failing = memory_store{ };
		failing.Fail = true;
		auto host    = application::view_filter_memory_host{ failing, effects, decisions, failing.load() };
		host.remember({ .View = core::filter_view::discuss, .Choice = only_new });
		drain(effects, decisions);
		expect(!host.durable() && host.quiescent() && host.defaults().Discuss == only_new, "a failed write is reported and the session keeps the memory");

		host.remember({ .View = core::filter_view::discuss, .Choice = only_new });
		expect(effects.Tasks.empty(), "remembering what is already remembered writes nothing");

		host.shutdown();
		host.remember({ .View = core::filter_view::threads, .Choice = both });
		expect(effects.Tasks.empty() && !host.defaults().Threads, "after shutdown nothing is taken");
	}

	void test_shutdown_writes_the_waiting_choice()
	{
		auto store     = memory_store{ };
		auto effects   = manual_executor{ };
		auto decisions = manual_executor{ };
		auto host      = application::view_filter_memory_host{ store, effects, decisions, store.load() };
		host.remember({ .View = core::filter_view::discuss, .Choice = only_new });
		host.remember({ .View = core::filter_view::threads, .Choice = hide });
		expect(effects.Tasks.size() == 1U, "the second choice waits behind the first write");
		host.shutdown();
		expect(effects.Tasks.size() == 2U, "shutdown posts the waiting choice rather than dropping it");
		while (effects.run_one()) { }
		expect(store.Writes.size() == 2U && store.load().Defaults == host.defaults(), "draining the effect lane leaves the latest memory stored");
	}
}

auto main() -> int
{
	test_shutdown_writes_the_waiting_choice();
	test_windows_diverge_and_the_next_pull_request_carries_the_memory();
	test_relaunch_restores_the_memory();
	test_unwritable_and_failing_stores();
	std::cout << "view filter memory host: ok\n";
	return EXIT_SUCCESS;
}
