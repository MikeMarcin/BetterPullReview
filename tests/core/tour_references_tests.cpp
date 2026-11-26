/// \file
/// \brief [[BPR-1995]], [[BPR-1996]], RFC-BPR-0100. Code references in a
/// tour's text, and Enhance on a selection replacing only the selection.

#include "bpr/core/tour_agent_session.hpp"
#include "bpr/core/tour_references.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	// NOLINTBEGIN(bugprone-throwing-static-initialization): fixture values; a failure to build them ends the test.
	const auto files = core::agent_reference_files{ .Changed = { "src/engine/renderer.cpp", "src/engine/frame_pool.cpp", "src/a/util.hpp", "src/b/util.hpp" } };
	// NOLINTEND(bugprone-throwing-static-initialization)

	auto permalink(const core::tour_text_reference& aReference) -> std::optional<std::string>
	{
		const auto range = "#L" + std::to_string(aReference.First) + (aReference.Last != aReference.First ? "-L" + std::to_string(aReference.Last) : std::string{ });
		const auto link  = "https://github.com/o/r/blob/" + std::string(aReference.Before ? "base" : "head") + "/" + aReference.Path + range;
		return aReference.Alone ? link : "[" + aReference.Written + "](" + link + ")";
	}

	void test_inline_and_alone()
	{
		const auto body   = std::string{ "The loop clears first (`frame_pool.cpp:41-47`), see renderer.cpp:3.\n\nsrc/engine/frame_pool.cpp:41-47\n\n```\nrenderer.cpp:9\n```\n" };
		const auto result = core::rewrite_tour_references(body, files, permalink);
		expect(result.Unresolved.empty(), "every reference resolves");
		expect(result.Text
				   == "The loop clears first ([`frame_pool.cpp:41-47`](https://github.com/o/r/blob/head/src/engine/frame_pool.cpp#L41-L47)), see "
					  "[renderer.cpp:3](https://github.com/o/r/blob/head/src/engine/renderer.cpp#L3).\n\nhttps://github.com/o/r/blob/head/src/engine/frame_pool.cpp#L41-L47\n\n```\nrenderer.cpp:9\n```\n",
			   "an inline reference becomes a link, one alone on its line a bare permalink, fenced code is left");
	}

	void test_unresolved_is_left_and_reported()
	{
		const auto body   = std::string{ "See util.hpp:3, missing.cpp:4 and missing.cpp:4 again; renderer.cpp:5." };
		const auto result = core::rewrite_tour_references(body, files, permalink);
		expect(result.Unresolved == std::vector<std::string>{ "util.hpp:3", "missing.cpp:4" }, "an ambiguous and a missing reference are reported once each, in order");
		expect(result.Text.starts_with("See util.hpp:3, missing.cpp:4 and missing.cpp:4 again; [renderer.cpp:5]"), "they are left as written; the rest still link");
		expect(core::unresolved_tour_references(body, files) == result.Unresolved, "the report alone");
		expect(core::rewrite_tour_references("No references here.", files, permalink) == core::tour_reference_rewrite{ .Text = "No references here." }, "a text without references is unchanged");
	}

	void test_snippet_bound()
	{
		const auto longRange = core::tour_text_reference{ .Path = "a.cpp", .First = 10U, .Last = 40U };
		const auto shortOne  = core::tour_text_reference{ .Path = "a.cpp", .First = 10U, .Last = 15U };
		expect(core::tour_reference_snippet_last(longRange) == 21U && core::tour_reference_snippet_last(shortOne) == 15U, "a snippet shows at most 12 lines");
	}

	void test_enhance_selection_replaces_only_the_selection()
	{
		auto draft = core::review_tour{ };
		auto stop  = core::tour_stop{ };
		stop.Id    = core::tour_stop_id{ 7U };
		stop.Body  = "First part. Middle part. Last part.";
		draft.Stops.push_back(stop);
		auto stray = core::tour_ask_answer{ .Reply = "Tightened.", .Changes = { .Stops = { { "a.cpp", 1U, 2U, "Stray", "Stray." } } }, .StopTexts = { { 1U, "First part. Centre part. Last part." } } };
		auto kept  = core::confine_to_passage(stray, draft, core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = stop.Id }, core::tour_passage{ .Field = core::tour_passage_field::stop, .Text = "Middle part." });
		expect(kept.StopTexts.size() == 1U && kept.StopTexts[0].Explanation == "First part. Centre part. Last part." && kept.Changes.Stops.empty() && kept.Reply == "Tightened.",
			   "a whole part given back unchanged outside the passage is read as the passage; other fields go");
		kept = core::confine_to_passage(stray, draft, core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = stop.Id }, core::tour_passage{ .Field = core::tour_passage_field::stop, .Text = "not there" });
		expect(kept.StopTexts.empty(), "a passage no longer in the part proposes nothing");
		auto stopAnswer = core::tour_ask_answer{ .Reply = "Done.", .StopTexts = { { 1U, "Centre part." } } };
		kept            = core::confine_to_passage(stopAnswer, draft, core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = stop.Id }, core::tour_passage{ .Field = core::tour_passage_field::stop, .Text = "Middle part." });
		expect(kept.StopTexts.size() == 1U && kept.StopTexts[0].Stop == 1U && kept.StopTexts[0].Explanation == "First part. Centre part. Last part.", "a stop's selection is replaced in the stop's text");
		draft.Stops[0].Body = "Twice. Twice.";
		kept                = core::confine_to_passage(stopAnswer, draft, core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = stop.Id }, core::tour_passage{ .Field = core::tour_passage_field::stop, .Text = "Twice." });
		expect(kept.StopTexts.empty(), "a passage that stands twice cannot be placed, so nothing is proposed");
	}
} // namespace

auto main() -> int
{
	test_inline_and_alone();
	test_unresolved_is_left_and_reported();
	test_snippet_bound();
	test_enhance_selection_replaces_only_the_selection();
	std::cout << "tour-references-tests:pass\n";
	return EXIT_SUCCESS;
}
