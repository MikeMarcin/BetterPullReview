// [[BPR-1852]]. The draft state's decisions and overlay, without Qt or a provider.
#include "bpr/core/draft_state.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {
	using namespace bpr;
	using core::draft_state_change;
	using core::draft_state_outcome;
	using core::draft_state_refusal;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_the_author_is_offered_the_change_that_applies()
	{
		const auto offered = [](const std::string_view aViewer, const bool aOpen, const bool aDraft) {
			return core::draft_state_offered({ .Author = "octo-author", .Viewer = aViewer, .Open = aOpen, .Draft = aDraft });
		};
		expect(offered("octo-author", true, true) == std::optional{ draft_state_change::ready_for_review }, "an open draft offers its author Ready for review");
		expect(offered("OCTO-Author", true, true) == std::optional{ draft_state_change::ready_for_review }, "logins compare without ASCII case");
		expect(offered("octo-author", true, false) == std::optional{ draft_state_change::convert_to_draft }, "an open pull request offers Convert to draft");
		expect(!offered("octo-author", false, true).has_value(), "a closed draft offers nothing");
		expect(!offered("octo-author", false, false).has_value(), "a closed pull request offers nothing");
		expect(!offered("octo-reviewer", true, true).has_value(), "another account is offered nothing");
		expect(!offered("", true, true).has_value(), "an anonymous read is offered nothing");
		expect(!core::draft_state_offered({ .Author = "", .Viewer = "", .Open = true, .Draft = true }).has_value(), "an unknown author is nobody's");
	}

	void test_a_gesture_is_decided_in_order()
	{
		const auto decide = [](const std::string_view aViewer, const bool aOpen, const bool aDraft, const draft_state_change aChange) {
			return core::decide_draft_state_change({ .Author = "octo-author", .Viewer = aViewer, .Open = aOpen, .Draft = aDraft }, aChange);
		};
		expect(decide("octo-author", true, true, draft_state_change::ready_for_review) == draft_state_refusal::none, "a draft is made ready");
		expect(decide("octo-author", true, false, draft_state_change::convert_to_draft) == draft_state_refusal::none, "a ready one is made a draft");
		expect(decide("octo-reviewer", false, false, draft_state_change::ready_for_review) == draft_state_refusal::not_author, "not the author first");
		expect(decide("octo-author", false, false, draft_state_change::ready_for_review) == draft_state_refusal::not_open, "then not open");
		expect(decide("octo-author", true, false, draft_state_change::ready_for_review) == draft_state_refusal::already, "then already ready");
		expect(decide("octo-author", true, true, draft_state_change::convert_to_draft) == draft_state_refusal::already, "or already a draft");
	}

	void test_a_write_is_decided_from_github()
	{
		const auto decide = [](const bool aDraft, const bool aOpen, const bool aCan, const draft_state_change aChange) {
			return core::decide_draft_state_write({ .Draft = aDraft, .Open = aOpen, .ViewerCanUpdate = aCan }, aChange);
		};
		expect(decide(true, true, true, draft_state_change::ready_for_review) == draft_state_outcome::write, "a draft GitHub lets the viewer change is written");
		expect(decide(false, true, true, draft_state_change::ready_for_review) == draft_state_outcome::already, "already ready writes nothing");
		expect(decide(false, false, false, draft_state_change::ready_for_review) == draft_state_outcome::already, "already is settled before the rest");
		expect(decide(true, false, true, draft_state_change::ready_for_review) == draft_state_outcome::not_open, "a closed draft is not written");
		expect(decide(true, true, false, draft_state_change::ready_for_review) == draft_state_outcome::not_permitted, "GitHub's refusal is believed");
		expect(decide(false, true, true, draft_state_change::convert_to_draft) == draft_state_outcome::write, "a ready one is converted");
		expect(decide(true, true, true, draft_state_change::convert_to_draft) == draft_state_outcome::already, "a draft is not converted again");
	}

	void test_the_overlay_shows_the_change_until_a_read_agrees()
	{
		auto overlay = core::draft_state_overlay{ };
		expect(core::project_draft_state(true, overlay) == core::projected_draft_state{ .Draft = true }, "nothing held shows the read");
		core::begin_draft_state_change(overlay, 0U, draft_state_change::ready_for_review);
		expect(!overlay.held(), "request 0 is no request");

		core::begin_draft_state_change(overlay, 7U, draft_state_change::ready_for_review);
		expect(core::project_draft_state(true, overlay) == core::projected_draft_state{ .Draft = false, .Pending = true }, "the change shows at once, pending");
		core::observe_draft_state(overlay, true);
		expect(overlay.held(), "a read begun before the change does not end a pending one");
		core::settle_draft_state_change(overlay, 8U, false);
		expect(!overlay.Confirmed, "another request's answer changes nothing");
		core::settle_draft_state_change(overlay, 7U, false);
		expect(core::project_draft_state(true, overlay) == core::projected_draft_state{ .Draft = false }, "GitHub's answer confirms it");
		core::observe_draft_state(overlay, true);
		expect(overlay.held() && overlay.Disagreements == 1U, "one disagreeing read is not believed");
		core::observe_draft_state(overlay, false);
		expect(!overlay.held(), "a read that agrees ends it");

		core::begin_draft_state_change(overlay, 9U, draft_state_change::convert_to_draft);
		core::settle_draft_state_change(overlay, 9U, true);
		core::observe_draft_state(overlay, false);
		core::observe_draft_state(overlay, false);
		expect(!overlay.held(), "the second read that disagrees is believed");

		core::begin_draft_state_change(overlay, 10U, draft_state_change::ready_for_review);
		core::settle_draft_state_change(overlay, 10U, true);
		expect(!overlay.held(), "an answer in the other state withdraws the change");

		core::begin_draft_state_change(overlay, 11U, draft_state_change::ready_for_review);
		core::abandon_draft_state_change(overlay, 12U);
		expect(overlay.held(), "abandoning another request changes nothing");
		core::abandon_draft_state_change(overlay, 11U);
		expect(core::project_draft_state(true, overlay) == core::projected_draft_state{ .Draft = true }, "a failure shows the read again");

		core::begin_draft_state_change(overlay, 13U, draft_state_change::ready_for_review);
		core::begin_draft_state_change(overlay, 14U, draft_state_change::convert_to_draft);
		expect(overlay.Request == 14U && overlay.Draft, "a later change replaces the earlier");
	}

	void test_names()
	{
		expect(core::name(draft_state_change::ready_for_review) == "ready_for_review", "change name");
		expect(core::name(draft_state_refusal::not_open) == "not_open", "refusal name");
		expect(core::name(draft_state_outcome::not_permitted) == "not_permitted", "outcome name");
	}
} // namespace

auto main() -> int
{
	test_the_author_is_offered_the_change_that_applies();
	test_a_gesture_is_decided_in_order();
	test_a_write_is_decided_from_github();
	test_the_overlay_shows_the_change_until_a_read_agrees();
	test_names();
	std::cout << "core_draft_state: ok\n";
	return EXIT_SUCCESS;
}
