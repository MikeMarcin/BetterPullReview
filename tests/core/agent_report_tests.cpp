// RFC-BPR-0128 ([[BPR-2166]], [[BPR-2167]]). A report by its shape (Q7), its
// outline and card, and a commit listing with a merge of the base branch as
// one line and the rest grouped by area (AC07).
#include "bpr/core/agent_report.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_shape_outline_and_card()
	{
		const auto report = std::string{ "# What changed since your review\n\n18 commits: the frame pool, and a merge of main.\nNothing else.\n\n## Rendering\n\nThe clear now waits.\n\n```cpp\n# not a heading\n```\n\n### Threads\n\n- line 6\n" };
		expect(is_report_answer(report), "sections make a report");
		expect(!is_report_answer("Partly. The resize clamps now, but the zero-width frame is still drawn."), "a short answer is a reply");
		auto lines = std::string{ };
		for (auto index = 0; index < 41; ++index) {
			lines += "line\n";
		}
		expect(is_report_answer(lines), "a long answer is a report too");
		const auto outline = report_outline(report);
		expect(outline.size() == 3U && outline[0] == report_heading{ .Level = 1U, .Title = "What changed since your review", .Line = 0U } && outline[1].Title == "Rendering" && outline[1].Level == 2U
				   && outline[2].Title == "Threads" && outline[2].Level == 3U,
			   "the outline is the headings, fenced code left out");
		const auto card = report_card_of(report);
		expect(card.Title == "What changed since your review" && card.Sections == 3U && card.Lead == "18 commits: the frame pool, and a merge of main.\nNothing else.\nThe clear now waits.",
			   "the card: title, sections and the first lines: " + card.Lead);
		expect(report_card_of("First line\nsecond").Title == "First line", "no heading: the first line is the title");
		// Q4: an answer that is a report by what was asked keeps its first
		// line in the lead and takes the question's title.
		const auto asked = report_card_of("First line\nsecond", "Explain a.cpp");
		expect(asked.Title == "Explain a.cpp" && asked.Lead.find("First line") != std::string::npos, "no heading, a title given: the title, and the lead from the start");
	}

	void test_merges_fold_and_areas_group()
	{
		const auto commits = std::vector<listed_commit>{
			{ .Oid = "a1000000000000000000000000000000000000aa", .Headline = "Render retained frames", .Parents = { "base" }, .Paths = std::vector<std::string>{ "src/render/renderer.cpp", "src/render/frame_pool.cpp" } },
			{ .Oid = "b2000000000000000000000000000000000000bb", .Headline = "Merge branch 'main' into retained-frames", .Parents = { "a1000000000000000000000000000000000000aa", "m9" }, .Paths = std::vector<std::string>{ "docs/guide.md", "src/render/renderer.cpp" } },
			{ .Oid = "c3000000000000000000000000000000000000cc", .Headline = "Document the pool", .Parents = { "b2000000000000000000000000000000000000bb" }, .Paths = std::vector<std::string>{ "docs/pool.md" } },
			{ .Oid = "d4000000000000000000000000000000000000dd", .Headline = "Clamp the resize", .Parents = { "c3000000000000000000000000000000000000cc" }, .Paths = std::vector<std::string>{ "src/render/renderer.cpp" } },
			{ .Oid = "e5000000000000000000000000000000000000ee", .Headline = "Merge remote-tracking branch 'origin/main'", .Parents = { "d4000000000000000000000000000000000000dd", "m12" }, .Paths = std::vector<std::string>{ "tools/ci.yml" } },
		};
		const auto files   = std::vector<std::string>{ "src/render/renderer.cpp", "src/render/frame_pool.cpp", "docs/pool.md" };
		const auto listing = fold_commit_listing(commits, "main", files, { { "b2000000000000000000000000000000000000bb", 24U } });
		expect(listing.Merges.size() == 2U && listing.Merges[0].Brought == 24U && listing.Merges[0].ChangedPullRequestFiles == true && !listing.Merges[1].Brought && listing.Merges[1].ChangedPullRequestFiles == false,
			   "each merge of main is one line, with its count where known and whether it changed this pull request's files");
		expect(listing.Areas.size() == 2U && listing.Areas[0].Area == "src" && listing.Areas[0].Commits.size() == 2U && listing.Areas[1].Area == "docs" && listing.Areas[1].Commits.size() == 1U,
			   "the other commits are grouped by area, in order");
		const auto text = render_commit_listing(listing);
		expect(text.find("- Merged main (24 commits), changed this pull request's files (b200000)") != std::string::npos && text.find("- Merged main, changed none of this pull request's files") != std::string::npos
				   && text.find("src/\n- a100000 Render retained frames\n- d400000 Clamp the resize\n") != std::string::npos,
			   "rendered: " + text);
		expect(!merges_base(commits[0], "main", { }), "one parent is no merge");
		const auto feature = listed_commit{ .Oid = "f", .Headline = "Merge pull request #7 from fork/topic", .Parents = { "a", "b" } };
		expect(merges_base(feature, "main", { }) && !merges_base(feature, "main", { "a", "b" }), "a merge whose second parent is the pull request's own is not a merge of main");
		expect(fold_commit_listing(commits, "main", files) == fold_commit_listing(commits, "main", files), "deterministic");
	}
} // namespace

auto main() -> int
{
	test_shape_outline_and_card();
	test_merges_fold_and_areas_group();
	std::cout << "agent-report-tests:pass\n";
	return EXIT_SUCCESS;
}
