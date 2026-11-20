// [[BPR-2327]]. Merge's blockers in order and the heading they decide.
#include "bpr/core/merge_blockers.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::merge_blocker;
	using core::merge_blocker_kind;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_the_most_serious_kind_leads()
	{
		const auto ordered = core::order_merge_blockers({
			{ .Kind = merge_blocker_kind::check_waiting, .Subject = "ci/queue/required" },
			{ .Kind = merge_blocker_kind::review_required },
			{ .Kind = merge_blocker_kind::conflicts },
			{ .Kind = merge_blocker_kind::check_waiting, .Subject = "lint" },
		});
		expect(ordered.size() == 4U && ordered[0].Kind == merge_blocker_kind::conflicts, "conflicts lead");
		expect(ordered[1].Kind == merge_blocker_kind::review_required, "a missing review is before a check still waiting");
		expect(ordered[2].Subject == "ci/queue/required" && ordered[3].Subject == "lint", "checks keep the order they were given");
		const auto headline = core::decide_merge_headline(ordered);
		expect(headline.Lead == merge_blocker_kind::conflicts && headline.LeadCount == 1U && headline.Others == 3U, "the conflict leads, three more follow");
	}

	void test_a_waiting_check_alone_is_not_a_conflict()
	{
		const auto ordered  = core::order_merge_blockers({ { .Kind = merge_blocker_kind::check_waiting, .Subject = "ci/queue/required" } });
		const auto headline = core::decide_merge_headline(ordered);
		expect(headline.Lead == merge_blocker_kind::check_waiting && headline.LeadCount == 1U && headline.Others == 0U, "a waiting check leads by itself");
	}

	void test_blockers_of_the_lead_kind_are_counted_together()
	{
		const auto ordered  = core::order_merge_blockers({
			{ .Kind = merge_blocker_kind::check_failed, .Subject = "build" },
			{ .Kind = merge_blocker_kind::check_failed, .Subject = "test" },
			{ .Kind = merge_blocker_kind::check_failed, .Subject = "build" },
			{ .Kind = merge_blocker_kind::behind },
		});
		const auto headline = core::decide_merge_headline(ordered);
		expect(ordered.size() == 3U, "a duplicate is named once");
		expect(headline.Lead == merge_blocker_kind::check_failed && headline.LeadCount == 2U && headline.Others == 1U, "two failed checks lead, one more follows");
	}

	void test_nothing_blocks()
	{
		expect(core::decide_merge_headline({ }) == core::merge_headline{ }, "no blockers, no lead");
	}
} // namespace

auto main() -> int
{
	test_the_most_serious_kind_leads();
	test_a_waiting_check_alone_is_not_a_conflict();
	test_blockers_of_the_lead_kind_are_counted_together();
	test_nothing_blocks();
	std::cout << "merge-blockers-core-tests:pass\n";
	return EXIT_SUCCESS;
}
