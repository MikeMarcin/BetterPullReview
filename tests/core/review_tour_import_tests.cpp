#include "../support/review_tour_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/review_tour_import.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
#include <limits>
namespace {
	using namespace bpr;
	using tests::expect;
	using namespace core;
	auto seed() -> review_tour
	{
		auto value = tests::review_tour_fixture();
		value.Stops.clear();
		value.LastStopId = 0;
		return value;
	}
	auto objects() -> std::vector<tour_remote_object>
	{
		const auto link   = [](std::string_view aId) { return "https://github.com/o/r/pull/7#" + std::string{ aId }; };
		const auto root   = link("pullrequestreview-9");
		const auto first  = link("discussion_r18446744073709551615");
		const auto second = link("issuecomment-8");
		return {
			{ .Binding = { .Kind = tour_remote_kind::conversation_comment, .DatabaseId = 8, .Entity = "comment8", .Actor = "actor", .WebUrl = second, .Own = true }, .Raw = "custom **second**\r\n", .Root = true },
			{ .Binding = { .Kind = tour_remote_kind::review, .DatabaseId = 9, .Entity = "review9", .Actor = "actor", .WebUrl = root, .Own = true }, .Raw = "## Read this\r\n1. [same](" + first + ")\r\n1. [same](" + second + ")\r\n", .Root = true },
			{ .Binding = { .Kind = tour_remote_kind::line_comment, .DatabaseId = std::numeric_limits<std::uint64_t>::max(), .Entity = "opaque-node-not-a-database-id", .Actor = "actor", .WebUrl = first, .Own = true, .Resolved = true, .Outdated = true }, .Raw = "### PR Tour · 77 — same\n\n**first**\n\n[Tour overview](" + root + ")\n" }
		};
	}
	constexpr std::string_view brief = "JIRA\r\n## Problem\r\nCrash.\r\n## Solution\r\nSnapshot.\r\n## Checks\r\n- [x] kept\r\n";
}
int main()
{
	auto records    = objects();
	auto candidates = discover_tours(seed(), brief, "pull7", records, true);
	expect(candidates.size() == 1 && !candidates[0].Draft.ReadOnly, "normal arbitrary named ordered root discovered");
	const auto& draft = candidates[0].Draft;
	expect(draft.Stops.size() == 2 && draft.Stops[0].Remote->DatabaseId == std::numeric_limits<std::uint64_t>::max() && draft.Stops[0].Id != draft.Stops[1].Id, "textual order independent of server order, title, ordinal and uint64 identity");
	expect(draft.Stops[0].Remote->Resolved && draft.Stops[0].Remote->Outdated, "resolved outdated stops retained");
	expect(draft.IndexBaseline->Raw == records[1].Raw && !draft.ImportNotice.contains("mapping"), "raw index baseline retained exactly; a brief in the description is only read");
	expect(draft.Stops[1].Body == records[0].Raw && !draft.Stops[1].Baseline->SourceSpan, "custom remote body stays whole and manually editable");
	std::reverse(records.begin(), records.end());
	expect(discover_tours(seed(), brief, "pull7", records, true)[0].Draft == draft, "server reorder changes no reconstructed route");
	expect(discover_tours(seed(), brief, "pull7", records, false)[0].Draft.ReadOnly, "partial collection cannot grant edits");
	records                = objects();
	records[2].Binding.Own = false;
	expect(discover_tours(seed(), brief, "pull7", records, true)[0].Draft.ReadOnly, "third party cannot grant edits");
	records                  = objects();
	records[2].Binding.Actor = "other";
	expect(discover_tours(seed(), brief, "pull7", records, true)[0].Draft.ReadOnly, "mixed authors cannot grant edits");
	records        = objects();
	records[1].Raw = "## Read this\r\n1. [same](https://github.com/o/r/pull/7/files#r18446744073709551615)\r\n1. [same](https://github.com/o/r/pull/7#issuecomment-8)\r\n";
	expect(discover_tours(seed(), brief, "pull7", records, true)[0].Draft.Stops == draft.Stops, "[[BPR-1222]] a Files changed stop link names the same comment as its Conversation link");
	records = objects();
	records[1].Raw += "1. [again](" + records[2].Binding.WebUrl + ")\r\n";
	auto duplicate = discover_tours(seed(), brief, "pull7", records, true)[0].Draft;
	expect(duplicate.ReadOnly && duplicate.Stops.size() == 3, "duplicate links remain visible as three entries");
	for (const auto bad : { "https://github.com/o/r/pull/8#discussion_r1", "https://evil.invalid/stop", "https://credential-canary@github.com/o/r/pull/7#discussion_r1", "https://github.com/o/other/pull/7#discussion_r1", "javascript:publish", "file:///private/source", "https://github.com/o/r/pull/7#discussion_r18446744073709551616", "https://github.com/o/r/pull/7#pullrequestreview-9", "https://github.com/o/r/pull/8/files#r18446744073709551615", "malformed" }) {
		records        = objects();
		records[1].Raw = "1. [bad](" + std::string{ bad } + ")\n";
		auto broken    = discover_tours(seed(), brief, "pull7", records, true);
		expect(broken.size() == 1 && broken[0].Draft.ReadOnly && broken[0].Draft.Stops.size() == 1, "foreign self missing malformed entries visible with no fetch or authority");
	}
	records              = objects();
	records[1].Available = false;
	expect(discover_tours(seed(), brief, "pull7", records, true).empty(), "deleted root is not inferred from cached title");
	records             = objects();
	auto root           = records[1];
	root.Binding.Entity = "review10";
	root.Binding.WebUrl = "https://github.com/o/r/pull/7#pullrequestreview-10";
	records.push_back(root);
	expect(discover_tours(seed(), brief, "pull7", records, true).size() == 2, "multiple roots require selection, no newest heuristic");
	// [[BPR-2274]]. A tour published before this change: its description
	// carries the Problem and Solution an older build inserted under the
	// tour's link, and its comment the line pointing there. It loads as its
	// stops alone; the brief is not a field and not a stop, nothing asks
	// for mapping, and an update writes no description.
	records              = objects();
	records[1].Raw       = "The problem and solution are in the pull request description.\n\n## Review tour \u2014 Start here\n\n1. [same](" + records[2].Binding.WebUrl + ")\n1. [same](" + records[0].Binding.WebUrl + ")\n";
	const auto described = "[Review tour](" + records[1].Binding.WebUrl + ")\n\n## Problem\nCrash.\n\n## Solution\nSnapshot.\n\n## Testing\nTemplate.\n";
	auto       fresh     = discover_tours(seed(), described, "pull7", records, true);
	expect(fresh.size() == 1 && fresh[0].Preferred && !fresh[0].Draft.ReadOnly && !fresh[0].Draft.ImportNotice.contains("mapping"), "[[BPR-2274]] a tour whose description holds an old brief loads without a notice");
	expect(fresh[0].Draft.Stops.size() == 2 && std::ranges::none_of(fresh[0].Draft.Stops, [](const tour_stop& aStop) { return aStop.Body.contains("Crash.") || aStop.Body.contains("Snapshot.") || aStop.Title == "Problem" || aStop.Title == "Solution"; }), "[[BPR-2274]] the old brief becomes no stop");
	expect(fresh[0].Draft.IndexBaseline->Raw == records[1].Raw, "[[BPR-2274]] the comment's old note stays as published");
	const auto reread = reconcile_tour(fresh[0].Draft, fresh[0].Draft);
	expect(reread.Conflicts.empty() && !reread.RemoteStale, "[[BPR-2274]] a reread of the same tour reconciles cleanly");
	const auto update = render_tour_update(fresh[0].Draft, { });
	expect(update && update->Edits.empty() && update->Index == records[1].Raw, "[[BPR-2274]] an unchanged tour plans no write, its comment kept byte for byte");
	// A tour published before BPR-1787 carries its brief in its own comment:
	// kept there as published text, never read as a field.
	records        = objects();
	records[1].Raw = "## Problem\n\nOld crash.\n\n## Solution\n\nOld snapshot.\n\n## Review tour \u2014 Start here\n\n1. [same](" + records[2].Binding.WebUrl + ")\n1. [same](" + records[0].Binding.WebUrl + ")\n";
	auto old       = discover_tours(seed(), "Untouched template.\n", "pull7", records, true);
	expect(old.size() == 1 && old[0].Draft.Stops.size() == 2 && !old[0].Draft.ImportNotice.contains("mapping") && old[0].Draft.IndexBaseline->Raw == records[1].Raw, "[[BPR-2274]] an old tour's comment brief is kept as written and read as no field");
	const auto oldUpdate = render_tour_update(old[0].Draft, { });
	expect(oldUpdate && oldUpdate->Edits.empty() && oldUpdate->Index.starts_with("## Problem\n\nOld crash."), "[[BPR-2274]] an update never strips the old comment's brief");
}
