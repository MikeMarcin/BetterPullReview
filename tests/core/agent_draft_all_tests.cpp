// [[BPR-2061]]. Draft all: which of one reply's suggested comments are
// drafted, which are passed over and why, and what the reply then counts.

#include "bpr/core/agent_draft_all.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "agent draft all failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr auto open_card      = draft_all_candidate{ .State = draft_all_card_state::open, .Suggested = true, .Placeable = true };
	constexpr auto drafted_card   = draft_all_candidate{ .State = draft_all_card_state::drafted, .Suggested = true, .Placeable = true };
	constexpr auto gone_card      = draft_all_candidate{ .State = draft_all_card_state::open, .Suggested = true, .Placeable = false };
	constexpr auto dismissed_card = draft_all_candidate{ .State = draft_all_card_state::dismissed, .Suggested = true, .Placeable = true };
	constexpr auto working_card   = draft_all_candidate{ .State = draft_all_card_state::working, .Suggested = true, .Placeable = true };
	constexpr auto kept_card      = draft_all_candidate{ .State = draft_all_card_state::kept_local, .Suggested = true, .Placeable = true };
	constexpr auto plain_card     = draft_all_candidate{ .State = draft_all_card_state::open, .Suggested = false, .Placeable = true };

	void test_offer_needs_several_suggestions()
	{
		const auto none = std::vector<draft_all_candidate>{ };
		expect(draft_all_offer(none) == 0U, "no cards offer nothing");
		const auto one = std::vector{ open_card };
		expect(draft_all_offer(one) == 0U, "one suggestion is its own card's action");
		const auto oneAndPlain = std::vector{ open_card, plain_card, plain_card };
		expect(draft_all_offer(oneAndPlain) == 0U, "cards with no suggested comment do not make several");
		const auto two = std::vector{ open_card, open_card };
		expect(draft_all_offer(two) == 2U, "two suggestions offer Draft all (2)");
	}

	void test_count_is_undrafted_and_placeable()
	{
		const auto cards = std::vector{ open_card, drafted_card, gone_card, open_card, plain_card, kept_card };
		expect(draft_all_suggestions(cards) == 5U, "five carry a suggested comment");
		expect(draft_all_offer(cards) == 2U, "the count is the open placeable ones only; RFC-BPR-0127: a card kept local is not drafted");
		const auto plan = plan_draft_all(cards);
		expect(plan.Draft == std::vector<std::size_t>{ 0U, 3U }, "drafted in the cards' order, by position");
		expect(plan.AlreadyDrafted == 1U && plan.NotInDiff == 1U && plan.KeptLocal == 1U && plan.Dismissed == 0U && plan.Busy == 0U, "each other suggestion is counted under its reason");
		expect(start_draft_all(plan).KeptLocal == 1U && stable_name(draft_all_skip::kept_local) == "kept_local", "and the outcome says it was kept local");
	}

	void test_nothing_left_offers_nothing()
	{
		const auto cards = std::vector{ drafted_card, drafted_card, gone_card };
		expect(draft_all_offer(cards) == 0U, "a reply with everything drafted or gone offers nothing");
		const auto plan = plan_draft_all(cards);
		expect(plan.Draft.empty() && plan.AlreadyDrafted == 2U && plan.NotInDiff == 1U, "and the plan says why");
	}

	void test_dismissed_and_busy_are_passed_over()
	{
		const auto cards = std::vector{ dismissed_card, working_card, open_card };
		const auto plan  = plan_draft_all(cards);
		expect(plan.Draft == std::vector<std::size_t>{ 2U }, "only the open card is drafted");
		expect(plan.Dismissed == 1U && plan.Busy == 1U, "a dismissed card and one under way are counted");
		expect(stable_name(draft_all_skip::not_in_diff) == "not_in_diff" && stable_name(draft_all_skip::already_drafted) == "already_drafted", "stable names");
	}

	void test_outcome_counts_what_happened()
	{
		const auto cards   = std::vector{ open_card, drafted_card, gone_card, open_card, open_card, dismissed_card };
		const auto plan    = plan_draft_all(cards);
		auto       outcome = start_draft_all(plan);
		expect(outcome.Drafted == 0U && outcome.skipped() == 3U, "the plan's own skips come first");
		outcome = settle_draft_all(outcome, draft_all_result::drafted);
		outcome = settle_draft_all(outcome, draft_all_result::not_in_diff);
		outcome = settle_draft_all(outcome, draft_all_result::refused);
		expect(outcome.Drafted == 1U, "one drafted");
		expect(outcome.NotInDiff == 2U && outcome.Refused == 1U && outcome.AlreadyDrafted == 1U && outcome.Dismissed == 1U, "a line found gone joins the planned ones");
		expect(outcome.skipped() == 5U, "every suggestion is drafted or skipped once");
		expect(outcome.Drafted + outcome.skipped() == draft_all_suggestions(cards), "nothing is lost or counted twice");
	}
}

auto main() -> int
{
	test_offer_needs_several_suggestions();
	test_count_is_undrafted_and_placeable();
	test_nothing_left_offers_nothing();
	test_dismissed_and_busy_are_passed_over();
	test_outcome_counts_what_happened();
	std::cout << "agent-draft-all-tests:pass\n";
	return EXIT_SUCCESS;
}
