// RFC-BPR-0128 ([[BPR-2166]], [[BPR-2167]]). A report by its shape (Q7), its
// outline and card, and a commit listing with a merge of the base branch as
// one line and the rest grouped by area (AC07).
#include "bpr/core/agent_report.hpp"
#include "bpr/core/agent_report_keep.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
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

	/// [[BPR-2246]]. The owner's 0.5.0 report: "Was this addressed? (32
	/// threads)" stopped before the agent answered still wore a Report
	/// attachment whose page only repeated the error.
	void test_report_attachment_needs_an_answer()
	{
		const auto stopped = std::string{ "Claude stopped before answering. The agent's copy of this pull request could not be made." };
		expect(shows_report_attachment({ .AlwaysReport = true, .Ended = true, .Answered = true }, "Two of the threads are addressed."), "an always-report ask's answer is a report");
		expect(shows_report_attachment({ .AlwaysReport = true, .Ended = false, .Answered = false }, "Two of the threads"), "and so is its answer while it arrives");
		expect(!shows_report_attachment({ .AlwaysReport = true, .Ended = true, .Answered = false }, stopped), "a turn that stopped before answering wears no report");
		auto lines = std::string{ };
		for (auto index = 0; index < 41; ++index) {
			lines += "error line\n";
		}
		expect(!shows_report_attachment({ .AlwaysReport = false, .Ended = true, .Answered = false }, lines), "not even when its error text is report-length");
		expect(!shows_report_attachment({ .AlwaysReport = true, .Ended = true, .Answered = true }, " \n\t\n"), "an empty answer is no report");
		expect(!shows_report_attachment({ .AlwaysReport = false, .Ended = true, .Answered = true }, "Partly."), "a short reply to an ordinary ask is no report");
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
		// docs/design/report-card-v4 (owner, 2026-11-14): the card is the
		// summary, rendered, and a row naming the report and its sections.
		// Defects 2 and 3: sections are counted without the title, and the
		// summary is what stands between the title and the first section.
		const auto card = report_card_of(report);
		expect(card.Title == "What changed since your review" && card.Sections == 1U && card.Summary == "18 commits: the frame pool, and a merge of main.\nNothing else.",
			   "the card: title, one section, and the summary as written: " + card.Summary);
		expect(card.Outline.size() == 2U && card.Outline[0].Title == "Rendering" && card.Outline[0].Depth == 0U && card.Outline[1].Title == "Threads" && card.Outline[1].Depth == 1U,
			   "the outline the report's menu lists: the sections without the title, sub-sections one deeper");
		const auto review = report_card_of("# Review of #21\n\n## Must fix\n\nThe `clamp` is wrong.\n\nMore.\n\n### A zero height\n\n## Worth a look\n\nx\n");
		expect(review.Sections == 2U && review.Summary == "The `clamp` is wrong.", "no prose before the first section: the first paragraph after the title, wherever it stands: " + review.Summary);
		expect(report_card_of("First line\nsecond").Title == "First line" && report_card_of("First line\nsecond").Summary == "second", "no heading: the first line is the title");
		// Q4: an answer that is a report by what was asked keeps its first
		// line in the summary and takes the question's title.
		const auto asked = report_card_of("First line\nsecond\n\nlater", "Explain a.cpp");
		expect(asked.Title == "Explain a.cpp" && asked.Summary == "First line\nsecond" && asked.Sections == 0U, "no heading, a title given: the title, and the summary from the start");
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
		const auto files = std::vector<std::string>{ "src/render/renderer.cpp", "src/render/frame_pool.cpp", "docs/pool.md" };
		auto       all   = std::vector<std::string>{ };
		for (const auto& commit : commits) {
			all.push_back(commit.Oid);
		}
		const auto listing = fold_commit_listing(commits, all, "main", files, { { "b2000000000000000000000000000000000000bb", 24U } });
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
		expect(fold_commit_listing(commits, all, "main", files) == fold_commit_listing(commits, all, "main", files), "deterministic");
		// [[BPR-2184]] (review N7). The slice since the review is not the whole
		// pull request: a merge whose second parent is one of the pull
		// request's own commits from before the review is not a merge of
		// main, though that commit is outside the slice.
		const auto topic = listed_commit{ .Oid = "f6000000000000000000000000000000000000ff", .Headline = "Merge topic into feature", .Parents = { "e5000000000000000000000000000000000000ee", "0older" } };
		const auto slice = std::vector<listed_commit>{ topic };
		auto       whole = all;
		whole.insert(whole.begin(), "0older");
		expect(fold_commit_listing(slice, whole, "main", files).Merges.empty(), "the pull request's own earlier commit makes it a merge within the branch");
		expect(fold_commit_listing(slice, all, "main", files).Merges.size() == 1U, "without it in the whole set, it would read as a merge of main");
		// [[BPR-2184]] (review N3): which merges are counted, decided here.
		const auto counted = merges_to_count(commits, all, "main");
		expect(counted.size() == 2U && counted[0].Merge == "e5000000000000000000000000000000000000ee" && counted[0].First == "d4000000000000000000000000000000000000dd" && counted[0].Second == "m12"
				   && counted[1].Merge == "b2000000000000000000000000000000000000bb",
			   "each merge of main in the slice, newest first, with its parents");
		auto many = std::vector<listed_commit>{ };
		for (auto index = 0; index < 12; ++index) {
			many.push_back({ .Oid = "m" + std::to_string(index), .Headline = "Merge branch 'main' into topic", .Parents = { "p" + std::to_string(index), "q" + std::to_string(index) } });
		}
		expect(merges_to_count(many, { }, "main").size() == maximum_counted_merges, "at most eight a listing");
	}

	/// docs/design/report-templates-v1 and v2: the convention read for
	/// dressing. A Was this addressed? report: its title, summary, sections
	/// named for verdicts, each item's place, quote, code numbered from its
	/// place and the tally; the claims block is not shown.
	void test_dressed_was_addressed()
	{
		const auto answer = std::string_view{ "# Was this addressed?\n\nOf three threads, one is addressed.\n\n## Not addressed\n\n### Should this clamp to the maximum?\n\n`src/ui/widgets/slider.cpp:18`\n\n"
											  "> Should this clamp? A value of 1.2 draws past the track.\n\nThe lines are as they were.\n\n```cpp\nValue = aValue;\nif (Value > Maximum) { notify(); }\n```\n\n"
											  "## Partly\n\n### Clamp the size\n\n`src/engine/renderer.cpp:12`\n\nThe width is clamped.\n\n## Addressed\n\n### Name the magic number\n\n`src/ui/widgets/button.cpp:7`\n\n`7` is now `corner_radius`.\n\n"
											  "```bpr-claims\nPRRT_1: Not addressed, unchanged\n```\n" };
		const auto report = dress_report(answer);
		expect(report.Title == "Was this addressed?" && report.Summary == "Of three threads, one is addressed.", "the title and the summary");
		expect(report.Sections.size() == 3U && report.Sections[0].Name == "Not addressed" && report.Sections[0].Tone == report_tone::open && report.Sections[1].Tone == report_tone::partial
				   && report.Sections[2].Tone == report_tone::done,
			   "sections named for verdicts carry their colour");
		const auto& item = report.Sections[0].Items.at(0U);
		expect(item.Label == "Not addressed" && item.Tone == report_tone::open && item.Heading == "Should this clamp to the maximum?" && item.Place == "src/ui/widgets/slider.cpp:18",
			   "an item carries its section's verdict, its heading and its place");
		expect(item.Body.size() == 3U && item.Body[0].Kind == dressed_block_kind::quote && item.Body[1].Kind == dressed_block_kind::paragraph && item.Body[2].Kind == dressed_block_kind::code
				   && item.Body[2].FirstLine == 18U && item.Body[2].Lines.size() == 2U && item.Body[2].Language == "cpp",
			   "the quote, the reasons, and code numbered from the place");
		expect(report.Sections[0].Outline == 1U && item.Outline == 2U && report.Sections[2].Items[0].Outline == 6U, "each heading's row in the menu");
		expect(report.Tally.size() == 3U && report.Tally[0].Word == "not addressed" && report.Tally[0].Count == 1U && report.Tally[0].Tone == report_tone::open && report.Tally[2].Word == "addressed",
			   "the tally by verdict, in order");
		for (const auto& section : report.Sections) {
			for (const auto& each : section.Items) {
				for (const auto& block : each.Body) {
					expect(block.Language != "bpr-claims" && block.Text.find("PRRT_1") == std::string::npos, "the claims block is not shown");
				}
			}
		}
		expect(dress_report(answer) == report, "deterministic");
	}

	/// A review: items headed with a Conventional Comments label, a
	/// suggestion replacing the lines its item names, and rows.
	void test_dressed_review_and_rows()
	{
		const auto  answer = std::string_view{ "# Review of #21\n\nOne thing must be fixed.\n\n## Must fix\n\n### issue (blocking): A zero height still asserts\n\n`src/engine/renderer.cpp:12-13`\n\nThe height is not clamped.\n\n"
											   "```cpp\nconst auto width = std::max(aSize.Width, 1);\nTarget.resize(extent{ width, aSize.Height });\n```\n\n```suggestion\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```\n\n"
											   "## Good\n\n### praise: The move keeps history\n\n`src/engine/renderer.cpp:1`\n\nBlame still reaches the originals.\n\n"
											   "## Questions for the author\n\n- `src/engine/shaders/post/tonemap.frag:5` Is this pass a placeholder?\n\n"
											   "## Your conversations\n\n- **Lines changed** `src/engine/renderer.cpp:12` Clamp the size.\n- **Unchanged** `src/ui/widgets/button.cpp:7` Name the magic number.\n" };
		const auto  report = dress_report(answer);
		const auto& issue  = report.Sections.at(0U).Items.at(0U);
		expect(issue.Label == "issue (blocking)" && issue.Tone == report_tone::open && issue.Heading == "A zero height still asserts", "a Conventional Comments label leads its heading");
		expect(issue.Body.size() == 3U && issue.Body[2].Kind == dressed_block_kind::suggestion && issue.Body[2].FirstLine == 12U && issue.Body[2].Replaced.size() == 2U
				   && issue.Body[2].Replaced[1] == "Target.resize(extent{ width, aSize.Height });" && issue.Body[1].FirstLine == 12U,
			   "a suggestion replaces the lines its item names, read from the code above it");
		expect(report.Sections[1].Items[0].Label == "praise" && report.Sections[1].Items[0].Tone == report_tone::done, "praise is done");
		const auto& rows = report.Sections.at(3U).Intro.at(0U).Rows;
		expect(report.Sections[2].Intro.at(0U).Rows.at(0U).Place == "src/engine/shaders/post/tonemap.frag:5" && rows.size() == 2U && rows[0].Label == "Lines changed" && rows[0].Tone == report_tone::partial
				   && rows[0].Place == "src/engine/renderer.cpp:12" && rows[0].Text == "Clamp the size." && rows[1].Tone == report_tone::neutral,
			   "rows: the bold label, the place, the rest");
		expect(report.Tally.size() == 2U && report.Tally[0].Word == "issue" && report.Tally[1].Word == "praise", "the tally by label");
		// [[BPR-2331]]. Each count names where its first item is in the menu.
		expect(report.Tally[0].First == issue.Outline && report.Tally[1].First == report.Sections[1].Items[0].Outline && report.Tally[0].First != 0U && report.Tally[1].First > report.Tally[0].First,
			   "a count names its first item's row");
		const auto place = read_report_place("src/a.cpp:12-14");
		expect(place && place->Path == "src/a.cpp" && place->First == 12U && place->Last == 14U && !read_report_place("src/a.cpp") && !read_report_place("a b:1"), "places");
		expect(dress_report("Just an answer.\n\nNo headings.").Sections.empty(), "an answer without sections is not dressed");
		const auto linked = dress_report("# R\n\nS.\n\n## Partly\n\n### One\n\n[`src/a.cpp:3`](src/a.cpp#L3)\n\nWhy.\n\n- [`src/b.cpp:4`](src/b.cpp#L4) A row.\n");
		expect(linked.Sections.at(0U).Items.at(0U).Place == "src/a.cpp:3" && linked.Sections[0].Items[0].PlaceLink == "src/a.cpp#L3" && linked.Sections[0].Items[0].Body.at(1U).Rows.at(0U).PlaceLink == "src/b.cpp#L4"
				   && linked.Sections[0].Items[0].Body[1].Rows[0].Text == "A row.",
			   "a place the answer made a link keeps where it goes");
	}

	/// What changed: areas under Changed group rows, and the tally counts
	/// them.
	void test_dressed_areas()
	{
		const auto report = dress_report("# What changed since your review\n\nTwo commits.\n\n## Changed\n\n### Rendering\n\n- `src/engine/renderer.cpp:12` Clamped.\n- `src/engine/renderer.h:31` Commented.\n\n"
										 "```cpp\nconst auto width = 1;\n```\n\n### Tests\n\n- `tests/engine/renderer_tests.cpp:6` Covered.\n\n## Merged main\n\n- Merged main (9 commits). None touched this pull request's files.\n");
		expect(report.Tally.size() == 2U && report.Tally[0].Word == "rendering" && report.Tally[0].Count == 2U && report.Tally[1].Word == "tests", "areas tally their rows");
		expect(report.Tally[0].First == report.Sections.at(0U).Items.at(0U).Outline && report.Tally[1].First == report.Sections[0].Items.at(1U).Outline, "an area's count names its row");
		const auto& area = report.Sections.at(0U).Items.at(0U);
		expect(area.Label.empty() && area.Place.empty() && area.Body.at(1U).FirstLine == 12U, "an area's code is numbered from its first row's place");
		expect(report.Sections.at(1U).Intro.at(0U).Rows.at(0U).Place.empty(), "a row without a place");
	}

	/// [[BPR-2219]]. What the agent said while it worked is not the report;
	/// a review's verdict opens its summary; each finding has its keep action
	/// and the comment it starts from.
	void test_review_answer_verdict_and_findings()
	{
		const auto report_text = std::string{ "# Review of #21\n\n**Verdict:** Needs changes.\n\nOne thing must be fixed before this merges.\n\n## Must fix\n\n### issue (blocking): A zero height still asserts\n\n`src/engine/renderer.cpp:12-13`\n\n"
											  "The height is not clamped.\n\nA minimised window reports zero.\n\n```cpp\nconst auto width = std::max(aSize.Width, 1);\nTarget.resize(extent{ width, aSize.Height });\n```\n\n"
											  "```suggestion\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```\n\n"
											  "## Worth a look\n\n### question: Is the pass a placeholder?\n\n`src/engine/shaders/post/tonemap.frag:5`\n\nIt writes the scene straight through.\n\n"
											  "### nitpick: The README still names the old folder\n\n`README.md`\n\nIt says `render/`.\n\n### thought: The pass order is implicit\n\nNothing names it.\n\n"
											  "## Good\n\n### praise: The frame bracket is easy to read\n\n`src/engine/renderer.cpp:3-7`\n\nBegin, draw, present.\n\n## Not reviewed\n\n- `docs/api/reference.md` Prose only.\n" };
		const auto answer      = "I'll start with the brief and the diff.\n\nNow the callers of `resize`.\n\n```sh\n# not a title\ngit diff\n```\n\nI have what I need; here is the review.\n\n" + report_text;
		const auto read        = read_report_answer(answer);
		expect(read.Report == report_text && read.Notes.starts_with("I'll start with the brief") && read.Notes.ends_with("here is the review."), "what the agent said while it worked is cut at the report's title: " + read.Notes);
		expect(read_report_answer(report_text) == report_answer{ .Notes = { }, .Report = report_text }, "an answer that opens with its report is the report");
		expect(read_report_answer("Looked around.\n\n# Only a title\n\nNo sections.").Notes.empty() && read_report_answer("No headings at all.").Report == "No headings at all.", "no title a section follows: the answer is the report");
		expect(read_report_answer("Working.\n\n## Likely\n\nx\n\n## Possible\n\ny").Notes.empty(), "a report that has no level-one title keeps everything: its summary may stand there");
		expect(read_report_answer(answer) == read, "deterministic");
		// Look review-report-v002, as the owner saw it: the agent's text
		// blocks joined with nothing between them, the title mid-line.
		const auto glued = read_report_answer("I'll start with the brief and the diff, then read the files the change touches.The resize is the change that matters.I have what I need. Now I'll write up the review." + report_text);
		expect(glued.Report == report_text && glued.Notes.ends_with("Now I'll write up the review.") && dress_report(glued.Report).Title == "Review of #21" && report_card_of(glued.Report).Title == "Review of #21",
			   "a title glued to the end of the agent's last note is still the report's start: " + glued.Notes);
		expect(read_report_answer("See issue # 5 and C# code.\n\nNothing more.").Notes.empty() && read_report_answer("Use `x# y` here.\n\n## A\n\nb").Notes.empty()
				   && read_report_answer("Written in C# for speed.\n\n## A\n\nb").Notes.empty() && read_report_answer("It ends `here.# x` so.\n\n## A\n\nb").Notes.empty(),
			   "a mark after a space, after a letter or in code is not a title");

		const auto report = dress_report(read.Report);
		expect(report.Verdict == "Needs changes" && report.VerdictTone == report_tone::open && report.Summary == "One thing must be fixed before this merges.", "the verdict opens the summary and is not part of it: " + report.Summary);
		expect(dress_report("# R\n\n**Verdict:** Ready with comments\nTwo nits.\n\n## Worth a look\n\n### nitpick: x\n\n`a.cpp:1`\n\ny\n").VerdictTone == report_tone::partial
				   && dress_report("# R\n\n**Verdict:** Ready to merge\n\nClean.\n\n## Good\n\n### praise: x\n\n`a.cpp:1`\n\ny\n").VerdictTone == report_tone::done
				   && dress_report("# R\n\nNo verdict here.\n\n## Good\n\n### praise: x\n\n`a.cpp:1`\n\ny\n").Verdict.empty(),
			   "a verdict's colour; none when the report gives none");

		const auto findings = report_findings(report);
		expect(findings.size() == 5U, "every item headed with a Conventional Comments label is a finding");
		expect(findings[0].Keep == report_keep::draft_comment && findings[0].Path == "src/engine/renderer.cpp" && findings[0].First == 12U && findings[0].Last == 13U && findings[0].Outline == 2U,
			   "a finding at lines offers Draft comment there");
		expect(findings[0].Comment
				   == "issue (blocking): A zero height still asserts\n\nThe height is not clamped.\n\nA minimised window reports zero.\n\n```suggestion\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```",
			   "its comment: the label and the finding, why, and the change as a suggestion; the quoted code is left out:\n" + findings[0].Comment);
		expect(findings[1].Keep == report_keep::draft_comment && findings[1].First == 5U && findings[1].Last == 5U && findings[1].Comment == "question: Is the pass a placeholder?\n\nIt writes the scene straight through.",
			   "a finding without a suggestion drafts too");
		expect(findings[2].Keep == report_keep::keep_local && findings[2].Path == "README.md" && findings[2].First == 0U, "a finding that names only a file is kept local on it");
		expect(findings[3].Keep == report_keep::keep_local && findings[3].Path.empty(), "a finding with no place is kept local on the pull request");
		expect(findings[4].Keep == report_keep::draft_comment && findings[4].Comment.starts_with("praise: "), "praise can be said too");
		expect(stable_name(report_keep::draft_comment) == "draft_comment" && stable_name(report_keep::keep_local) == "keep_local" && stable_name(report_keep::none) == "none", "stable names");

		// The agent's own comment for the same place is the draft's text; a
		// comment for another place is not taken.
		const auto suggested = report_findings(report, { { .Path = "src/engine/renderer.cpp", .First = 13U, .Last = 13U, .Comment = "issue: clamp the height as well as the width." }, { .Path = "src/other.cpp", .First = 5U, .Last = 5U, .Comment = "Not this one." } });
		expect(suggested[0].Comment == "issue: clamp the height as well as the width.\n\n```suggestion\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```"
				   && suggested[1].Comment == findings[1].Comment,
			   "the agent's suggested comment for an overlapping place, with the report's suggestion; others as the report says them");
		// [[BPR-2331]]. The report holds the card at its finding's place; the
		// card at another place is the Agent tab's to show.
		expect(report_held_cards(suggested, 2U) == std::vector<bool>{ true, false } && report_held_cards(findings, 3U) == std::vector<bool>(3U, false),
			   "the report holds only the cards its findings are");
		expect(report_findings(dress_report("# What changed\n\nS.\n\n## Changed\n\n### Rendering\n\n- `src/a.cpp:1` x\n")).empty(), "a report without findings offers nothing");

		expect(report_summary_comment(report)
				   == "**Verdict:** Needs changes\n\nOne thing must be fixed before this merges.\n\n- issue (blocking): A zero height still asserts (`src/engine/renderer.cpp:12-13`)\n"
					  "- question: Is the pass a placeholder? (`src/engine/shaders/post/tonemap.frag:5`)\n- nitpick: The README still names the old folder (`README.md`)\n- thought: The pass order is implicit",
			   "the summary as a comment on the pull request: verdict, summary, a line a finding, praise left out:\n" + report_summary_comment(report));
		expect(report_summary_comment(dress_report("# R\n\n## Good\n\n### praise: x\n\n`a.cpp:1`\n\ny\n")).empty(), "no summary, nothing to keep");

		// What a real agent wrote (MikeMarcin/BPR-Test#185, 2026-11-16): two
		// places on the place line, and two lines written with a comma.
		const auto  real = dress_report("# Review of #185\n\nNot neutral.\n\n## Must fix\n\n### issue (blocking): the cache key ignores query params\n\n`feedsync/client.py:55`, `feedsync/cache.py:21-22`\n\nThe key is the source's URL.\n\n"
										"## Worth a look\n\n### nitpick: error detail and User-Agent\n\n`feedsync/client.py:30,71`\n\nThe message is dropped.\n");
		const auto& two  = real.Sections.at(0U).Items.at(0U);
		expect(two.Place == "feedsync/client.py:55" && two.OtherPlaces == "`feedsync/cache.py:21-22`" && two.Body.size() == 1U, "several places on the line: the first is the item's, the others are kept beside it: " + two.Place);
		const auto drafted = report_findings(real);
		expect(drafted.size() == 2U && drafted[0].Keep == report_keep::draft_comment && drafted[0].Path == "feedsync/client.py" && drafted[0].First == 55U && drafted[1].Keep == report_keep::draft_comment
				   && drafted[1].Path == "feedsync/client.py" && drafted[1].First == 30U && drafted[1].Last == 30U,
			   "both draft at their first place; lines written with a comma start at the first");
		// A prompt edited to another severity scheme, or with no verdict:
		// in a review every item with a place still drafts, and the report
		// shows without a verdict.
		const auto own = dress_report("# Review of #7\n\nTwo things.\n\n## Blockers\n\n### P1: The lock is dropped\n\n`src/a.cpp:9`\n\nTake it again.\n\n## Notes\n\n### The name misleads\n\n`src/b.cpp:3`\n\nRename it.\n");
		expect(own.Verdict.empty() && own.Sections.size() == 2U && report_findings(own).size() == 2U && report_findings(own)[0].Keep == report_keep::draft_comment
				   && report_findings(own)[0].Comment == "P1: The lock is dropped\n\nTake it again." && report_findings(own)[1].Comment == "The name misleads\n\nRename it.",
			   "a review's items draft whatever they are headed with, and a missing verdict is no error");
		expect(report_findings(dress_report("# What `a.cpp` does\n\nS.\n\n## How it works\n\n### resize\n\n`src/a.cpp:9`\n\nIt resizes.\n")).empty(), "a report that is not a review offers no keep action on its items");

		// [[BPR-2352]] (owner, 2026-11-21: "Reserve actual github suggestion
		// blocks for very clear and obviously correct fixes"). A fix written
		// in the file's language, after the quoted lines, drafts as written:
		// nothing depends on a suggestion being there.
		const auto languageFix      = dress_report("# Review of #9\n\n**Verdict:** Ready with comments\n\nOne thing.\n\n## Worth a look\n\n### suggestion: Cache the measure\n\n`src/a.cpp:20-21`\n\nThe width is measured twice a frame.\n\n"
												   "```cpp\nauto w = measure(row);\nauto h = measure(row);\n```\n\nMeasure once and keep both.\n\n```cpp\nconst auto size = measure_both(row); // ``` stays code\n```\n");
		const auto languageFindings = report_findings(languageFix);
		expect(languageFindings.size() == 1U && languageFindings[0].Keep == report_keep::draft_comment && languageFindings[0].First == 20U && languageFindings[0].Last == 21U
				   && languageFindings[0].Comment
					   == "suggestion: Cache the measure\n\nThe width is measured twice a frame.\n\nMeasure once and keep both.\n\n````cpp\nconst auto size = measure_both(row); // ``` stays code\n````",
			   "a fix in the file's language drafts at its place, after the quoted lines it leaves out, in a fence its code cannot close:\n" + languageFindings[0].Comment);
		const auto& languageItem = languageFix.Sections.at(0U).Items.at(0U);
		expect(std::ranges::none_of(languageItem.Body, [](const dressed_block& aBlock) { return aBlock.Kind == dressed_block_kind::suggestion; })
				   && std::ranges::count(languageItem.Body, dressed_block_kind::code, &dressed_block::Kind) == 2,
			   "a language block is code to show, never a suggested change");
	}

	/// [[BPR-2231]], [[BPR-2232]]. A report's finding is its card when one is
	/// at its place; its one state is kept where the card's is, or under the
	/// report's own digest; Draft all from the report plans over those states.
	void test_report_findings_share_their_cards_state()
	{
		const auto report = dress_report("# Review of #9\n\n**Verdict:** Needs changes\n\nTwo things.\n\n## Must fix\n\n### issue: A\n\n`src/a.cpp:3`\n\nWhy A.\n\n"
										 "### issue: B\n\n`src/b.cpp:7-8`\n\nWhy B.\n\n### nitpick: C\n\n`src/c.cpp:2`\n\nWhy C.\n\n### thought: D\n\n`README.md`\n\nWhy D.\n");
		// Cards in the answer's order: B's with a comment, a commentless one
		// at A, and one at a place the report does not name.
		const auto cards    = std::vector<report_suggested_comment>{ { .Path = "src/b.cpp", .First = 8U, .Last = 8U, .Comment = "issue: B, as the agent put it." },
																	 { .Path = "src/a.cpp", .First = 3U, .Last = 3U, .Comment = "" },
																	 { .Path = "src/z.cpp", .First = 1U, .Last = 1U, .Comment = "Elsewhere." } };
		const auto findings = report_findings(report, cards);
		expect(findings.size() == 4U && findings[0].Card == std::optional<std::size_t>{ 1U } && findings[0].Comment.starts_with("issue: A") && findings[1].Card == std::optional<std::size_t>{ 0U }
				   && findings[1].Comment == "issue: B, as the agent put it." && !findings[2].Card && !findings[3].Card,
			   "a finding is the card at its place, with or without the agent's comment; one with none is no card's");
		const auto commentFirst = report_findings(report, { { .Path = "src/a.cpp", .First = 3U, .Last = 3U, .Comment = "" }, { .Path = "src/a.cpp", .First = 2U, .Last = 4U, .Comment = "issue: A, said." } });
		expect(commentFirst[0].Card == std::optional<std::size_t>{ 1U } && commentFirst[0].Comment == "issue: A, said.", "a card with the agent's comment is taken before one without");

		const auto block  = digest_agent_findings("[{\"path\":\"src/b.cpp\"}]");
		const auto keys   = std::vector<report_card_key>{ { .Findings = block, .Ordinal = 0U }, { .Findings = block, .Ordinal = 1U }, { .Findings = block, .Ordinal = 2U } };
		const auto own    = digest_report_findings("# Review of #9\n");
		const auto keyOfA = report_finding_key_of(findings[0], keys, own);
		const auto keyOfC = report_finding_key_of(findings[2], keys, own);
		expect(keyOfA == report_finding_key{ .Findings = block, .Ordinal = 1U, .Card = true }, "a card's finding is kept under its card's block and ordinal");
		expect(keyOfC == report_finding_key{ .Findings = own, .Ordinal = findings[2].Outline, .Card = false } && own != digest_agent_findings("# Review of #9\n") && own == digest_report_findings("# Review of #9\n"),
			   "a finding no card carries is kept under the report's own digest, which no block's equals, at its row");

		// One memory: drafted from the report is the card's state, and a
		// restart (the records written and adopted again) keeps both.
		auto memory = agent_finding_card_memory{ };
		expect(memory.remember(report_keep_record(keyOfA, report_keep::draft_comment, true)), "Draft comment from the report keeps the card drafted");
		expect(memory.remember(report_keep_record(keyOfC, report_keep::draft_comment, true)), "and a card-less finding under the report's key");
		expect(memory.recall(block, 1U).State == agent_finding_card_disposition::drafted, "the card reads Drafted");
		auto restarted = agent_finding_card_memory{ };
		restarted.adopt(memory.records());
		expect(report_finding_standing_of(restarted.recall(keyOfA.Findings, keyOfA.Ordinal).State) == report_finding_standing::drafted
				   && report_finding_standing_of(restarted.recall(keyOfC.Findings, keyOfC.Ordinal).State) == report_finding_standing::drafted,
			   "after a restart both read Drafted");
		const auto kept    = report_keep_record(keyOfC, report_keep::keep_local, true, { }, 41U);
		const auto refused = report_keep_record(keyOfC, report_keep::draft_comment, false, "Line 2 is gone.");
		expect(kept.State == agent_finding_card_disposition::noted && kept.Note == 41U && refused.State == agent_finding_card_disposition::refused && refused.Why == "Line 2 is gone."
				   && report_finding_standing_of(kept.State) == report_finding_standing::kept_local && report_finding_standing_of(agent_finding_card_disposition::dismissed) == report_finding_standing::dismissed,
			   "Keep local keeps its note; a refusal says why");
		expect(stable_name(report_finding_standing::kept_local) == "kept_local" && stable_name(report_finding_standing::refused) == "refused", "stable names");

		// Draft all from the report: in the report's order, the drafted one
		// counted as already drafted, the unplaceable one as not in the diff,
		// the file-only one neither drafted nor counted.
		const auto facts      = std::vector<report_draft_all_fact>{ { .Standing = report_finding_standing::drafted, .Placeable = true },
																	{ .Standing = report_finding_standing::open, .Placeable = true },
																	{ .Standing = report_finding_standing::refused, .Placeable = false },
																	{ .Standing = report_finding_standing::open, .Placeable = true } };
		const auto candidates = report_draft_all_candidates(findings, facts);
		const auto plan       = plan_draft_all(candidates);
		expect(candidates.size() == 4U && !candidates[3].Suggested && plan.Draft == std::vector<std::size_t>{ 1U } && plan.AlreadyDrafted == 1U && plan.NotInDiff == 1U && draft_all_offer(candidates) == 1U,
			   "Draft all drafts what is not yet drafted, in the report's order, and counts the rest");
		const auto busy = report_draft_all_candidates(findings, std::vector<report_draft_all_fact>{ { .Working = true, .Placeable = true }, { .Standing = report_finding_standing::dismissed, .Placeable = true } });
		expect(busy[0].State == draft_all_card_state::working && busy[1].State == draft_all_card_state::dismissed && busy[2].State == draft_all_card_state::open && !busy[2].Placeable,
			   "under way and dismissed as the Agent tab's Draft all reads them; a missing fact is open and not placeable");
	}

	/// [[BPR-2236]]. The tone a finding's severity wears is core's, as its
	/// label's is; the card says the verdict apart from its summary.
	void test_tones_of_severity_and_the_card_verdict()
	{
		expect(report_tone_of(defect_severity::critical) == report_tone::open && report_tone_of(defect_severity::major) == report_tone::open
				   && report_tone_of(defect_severity::minor) == report_tone::partial && report_tone_of(defect_severity::trivial) == report_tone::neutral,
			   "Major and Critical need work, Minor is half way, Trivial is nothing in particular");
		expect(report_tone_of(defect_severity::major) == report_tone_of("issue (blocking)") && report_tone_of(defect_severity::minor) == report_tone_of("suggestion (non-blocking)"),
			   "a Major finding and its issue label wear one tone, a Minor one and its suggestion another");
		const auto card = report_card_of("# Review of #21\n\n**Verdict:** Needs changes.\n\nOne thing must be fixed.\n\n## Must fix\n\nx\n");
		expect(card.Verdict == "Needs changes" && card.VerdictTone == report_tone::open && card.Summary == "One thing must be fixed.",
			   "the card's verdict and tone, and its summary without the verdict line");
		const auto plain = report_card_of("# What a.cpp does\n\nIt resizes.\n\n## How\n\nx\n");
		expect(plain.Verdict.empty() && plain.VerdictTone == report_tone::neutral && plain.Summary == "It resizes.", "a report with no verdict keeps its summary whole");
	}
} // namespace

auto main() -> int
{
	test_report_attachment_needs_an_answer();
	test_shape_outline_and_card();
	test_merges_fold_and_areas_group();
	test_dressed_was_addressed();
	test_dressed_review_and_rows();
	test_dressed_areas();
	test_review_answer_verdict_and_findings();
	test_tones_of_severity_and_the_card_verdict();
	test_report_findings_share_their_cards_state();
	std::cout << "agent-report-tests:pass\n";
	return EXIT_SUCCESS;
}
