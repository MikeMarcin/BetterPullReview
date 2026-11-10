// [[BPR-1991]]. RFC-BPR-0099's head table, decided without Qt or a store:
// where the tour stands, its count, and its one publication action.
#include "../support/review_tour_fixture.hpp"
#include "bpr/core/overview_tour.hpp"
#include "bpr/core/tour_publication_head.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using state  = core::tour_head_state;
	using action = core::tour_head_action;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto tour() -> core::review_tour
	{
		auto value = tests::review_tour_fixture();
		auto added = value.Stops.front();
		added.Id   = core::tour_stop_id{ 2U };
		added.Body = "A second explanation.";
		value.Stops.push_back(added);
		value.LastStopId = 2U;
		return value;
	}

	void test_private_draft()
	{
		expect(core::decide_tour_head(nullptr, { }).State == state::none, "no draft, no head");
		auto readOnly     = tour();
		readOnly.ReadOnly = true;
		expect(core::decide_tour_head(&readOnly, { }).State == state::none, "a reader's tour has no head");
		auto draft = tour();
		auto head  = core::decide_tour_head(&draft, { });
		expect(head.State == state::publishable && head.Stops == 2U && head.ToAccept == 0U && head.Actions == std::vector{ action::publish } && !head.PublishDisabled, "a complete draft is publishable with Publish…");
		draft.Stops[1].Preparation = core::tour_preparation::needs_attention;
		head                       = core::decide_tour_head(&draft, { });
		expect(head.State == state::not_publishable && head.ToAccept == 1U && head.Missing == core::tour_head_missing::acceptance && head.PublishDisabled, "a stop that needs attention is still to accept");
		draft.ObservedHead = *core::parse_git_commit_oid("2222222222222222222222222222222222222222");
		expect(core::decide_tour_head(&draft, { }).ToAccept == 2U, "an explanation not current at the observed head is still to accept");
		draft         = tour();
		draft.Problem = "  ";
		expect(core::decide_tour_head(&draft, { }).Missing == core::tour_head_missing::problem, "the Problem is named first");
		draft = tour();
		draft.Stops.clear();
		expect(core::decide_tour_head(&draft, { }).Missing == core::tour_head_missing::stops, "no stops is named");
	}

	void test_publication_progress()
	{
		auto draft = tour();
		auto head  = core::decide_tour_head(&draft, { core::tour_publication_progress::publishing, 3U, 7U });
		expect(head.State == state::publishing && head.Step == 3U && head.Steps == 7U && head.Actions == std::vector{ action::details }, "publishing: step N of M, Details");
		head = core::decide_tour_head(&draft, { core::tour_publication_progress::stopped, 4U, 7U });
		expect(head.State == state::stopped && head.Step == 4U && head.Actions == std::vector{ action::resume, action::details }, "stopped: Resume first, then Details");
	}

	void test_published()
	{
		auto draft   = tour();
		draft.Remote = core::tour_remote_binding{ .Entity = "PRR_1", .WebUrl = "https://github.com/o/r/pull/5#pullrequestreview-1", .Own = true };
		for (auto& stop : draft.Stops) {
			stop.Remote   = core::tour_remote_binding{ .Kind = core::tour_remote_kind::line_comment, .Entity = "C" + std::to_string(stop.Id.value()), .WebUrl = "https://github.com/o/r/pull/5#discussion_r" + std::to_string(stop.Id.value()), .Own = true };
			stop.Baseline = core::tour_field_baseline{ .Raw = stop.Body, .DraftText = stop.Body };
		}
		draft.ProblemBaseline->DraftText = draft.Problem;
		auto head                        = core::decide_tour_head(&draft, { });
		expect(head.State == state::published && head.Changes == 0U && head.Actions == std::vector{ action::on_github }, "published and unchanged: On GitHub");
		draft.Stops[0].Body = "Rewritten.";
		draft.Problem       = "A new problem statement.";
		head                = core::decide_tour_head(&draft, { });
		expect(head.State == state::published_changed && head.Changes == 2U && head.Actions == std::vector{ action::update, action::on_github }, "a changed stop and a changed Problem are two changes not on GitHub");
		auto added   = draft.Stops[1];
		added.Id     = core::tour_stop_id{ 3U };
		added.Remote = std::nullopt;
		draft.Stops.push_back(added);
		draft.LastStopId = 3U;
		draft.Retired.push_back(draft.Stops[1]);
		expect(core::decide_tour_head(&draft, { }).Changes == 4U, "a new stop and a removed published stop count too");
		expect(core::decide_tour_head(&draft, { core::tour_publication_progress::publishing, 1U, 3U }).State == state::publishing, "an update under way is publishing");
	}

	void test_lead_sentence()
	{
		expect(core::tour_lead_sentence("The pool hands back the last frame. Then it clears.") == "The pool hands back the last frame.", "the first sentence");
		expect(core::tour_lead_sentence("  `acquire()` returns the **retained** frame\nwhen its size matches\n\nMore.") == "acquire() returns the retained frame when its size matches", "marks go, lines join, the paragraph ends it");
		expect(core::tour_lead_sentence("").empty(), "nothing for nothing");
		const auto cut = core::tour_lead_sentence(std::string(300U, 'a'), 20U);
		expect(cut == std::string(20U, 'a') + "\xE2\x80\xA6", "a long sentence is cut and closed");
	}

	void test_overview_rows()
	{
		const auto draft   = tour();
		const auto summary = core::decide_overview_tour(&draft, nullptr, true);
		expect(summary.Stops.size() == 2U && summary.Stops[0].Path == "src/item.cpp" && summary.Stops[0].FirstLine == 4U && summary.Stops[0].LastLine == 6U && summary.Stops[1].Lead == "A second explanation.", "Overview's rows carry the stop's file, one-based lines and first sentence");
	}
}

auto main() -> int
{
	test_private_draft();
	test_publication_progress();
	test_published();
	test_lead_sentence();
	test_overview_rows();
	std::cout << "tour-publication-head:pass\n";
	return EXIT_SUCCESS;
}
