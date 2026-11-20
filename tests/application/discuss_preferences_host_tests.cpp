// [[BPR-2171]]. The profile's Discuss preferences: one for every window,
// written off the decision lane, latest first, kept for the session when the
// store cannot take them.
#include "bpr/application/discuss_preferences_host.hpp"

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
		void drain()
		{
			while (run_one()) { }
		}
		std::deque<std::function<void()>> Tasks;
	};

	class memory_store final : public application::discuss_preferences_port
	{
	public:
		auto store(const core::discuss_preferences& aPreferences) -> bool override
		{
			Writes.push_back(aPreferences);
			if (Fail) {
				return false;
			}
			Row = core::stored_discuss_preferences_of(aPreferences);
			return true;
		}
		std::optional<core::stored_discuss_preferences> Row;
		std::vector<core::discuss_preferences>          Writes;
		bool                                            Fail = false;
	};
}

auto main() -> int
{
	const auto repository = core::repository_mark_key("github.com", "o", "r");
	auto       lanes      = manual_executor{ };
	auto       store      = memory_store{ };
	{
		auto host = application::discuss_preferences_host{ store, lanes, lanes, core::read_discuss_preferences(std::nullopt) };
		expect(host.preferences() == core::discuss_preferences{ }, "nothing chosen at first: no marks");
		expect(host.mark(repository, "helper", true), "Mark as bot changes the preferences at once");
		expect(core::author_is_bot("helper", core::marks_of(host.preferences(), repository)), "every window asking now sees the mark");
		expect(host.mark(repository, "second", true), "a second mark");
		expect(!host.mark(repository, "second", true), "the same mark again changes nothing");
		lanes.drain();
		expect(store.Writes.size() == 2U && core::author_is_bot("second", core::marks_of(store.Writes.back(), repository)), "one write runs, the one waiting carries the latest");
		expect(host.quiescent() && host.durable(), "written");
		store.Fail = true;
		expect(host.mark(repository, "helper", false), "a mark undone");
		lanes.drain();
		expect(!host.durable() && !core::author_is_bot("helper", core::marks_of(host.preferences(), repository)), "a store that fails keeps the session's preferences");
		store.Fail = false;
		expect(host.mark(repository, "other", true), "another mark");
		host.shutdown();
		expect(!host.mark(repository, "late", true), "after shutdown nothing changes");
		lanes.drain();
	}
	const auto reread = core::read_discuss_preferences(store.Row);
	expect(core::author_is_bot("second", core::marks_of(reread.Preferences, repository)) && core::author_is_bot("other", core::marks_of(reread.Preferences, repository)), "a relaunch reads what was written");
	std::cout << "DISCUSS_PREFERENCES_HOST_OK\n";
	return EXIT_SUCCESS;
}
