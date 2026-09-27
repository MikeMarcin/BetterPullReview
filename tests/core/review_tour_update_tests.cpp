#include "../support/review_tour_fixture.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
namespace {
	using namespace bpr::core;
	void expect(bool aValue, const char* aReason)
	{
		if (!aValue) {
			std::cerr << aReason << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	auto published() -> review_tour
	{
		auto draft      = bpr::tests::review_tour_fixture();
		draft.Remote    = tour_remote_binding{ .Kind = tour_remote_kind::review, .DatabaseId = 9, .Entity = "review9", .Actor = "author", .WebUrl = "https://github.com/o/r/pull/7#pullrequestreview-9", .Own = true };
		const auto seed = draft.Stops.front();
		draft.Stops.clear();
		std::vector<tour_markdown_link> entries;
		for (std::size_t i = 0; i < 5; ++i) {
			auto stop   = seed;
			stop.Id     = tour_stop_id{ i + 1 };
			stop.Title  = "Stop " + std::to_string(i + 1);
			stop.Body   = "Exact explanation " + std::to_string(i + 1);
			stop.Remote = tour_remote_binding{ .Kind = tour_remote_kind::line_comment, .DatabaseId = 20 + i, .Entity = "c" + std::to_string(i), .Actor = "author", .WebUrl = "https://github.com/o/r/pull/7#discussion_r" + std::to_string(20 + i), .Review = "9", .Own = true };
			entries.push_back({ stop.Title, stop.Remote->WebUrl });
			draft.Stops.push_back(stop);
		}
		draft.LastStopId = 5;
		for (std::size_t i = 0; i < 5; ++i) {
			std::vector<tour_markdown_link> nav;
			if (i) {
				nav.push_back({ "Previous", entries[i - 1].Destination });
			}
			nav.push_back({ "Tour overview", draft.Remote->WebUrl });
			if (i + 1 < 5) {
				nav.push_back({ "Next", entries[i + 1].Destination });
			}
			draft.Stops[i].Baseline = tour_field_baseline{ .Raw = *render_tour_stop(draft.Stops[i].Title, i + 1, draft.Stops[i].Body, nav), .RemoteEntity = draft.Stops[i].Remote->Entity };
		}
		draft.IndexBaseline = tour_field_baseline{ .Raw = "Preserve this custom orientation.\n\n" + *render_tour_index(entries) + "\nUnrelated tail.\n", .RemoteEntity = "review9" };
		return draft;
	}
	void reconciliation()
	{
		expect(merge_tour_text("α left / right", "α LEFT / right", "α left / RIGHT") == std::optional<std::string>{ "α LEFT / RIGHT" }, "nonoverlapping UTF-8 edits merge");
		expect(!merge_tour_text("same", "local", "remote"), "overlap preserves explicit conflict");
		auto base                     = published();
		base.Problem                  = "old problem";
		base.Solution                 = "old solution";
		const std::string description = "Template α\r\n\r\n## Problem\r\n\r\nold problem\r\n\r\n## Solution\r\n\r\nold solution\r\n\r\n## Tests\r\n\r\nKEEP EXACT\r\n";
		retain_tour_brief_baselines(base, description, "PR17");
		auto remote    = base;
		remote.Problem = "website problem";
		auto raw       = description;
		raw.replace(raw.find("old problem"), 11, "website problem");
		retain_tour_brief_baselines(remote, raw, "PR17");
		auto local     = base;
		local.Solution = "local solution";
		auto merged    = reconcile_tour(local, remote);
		expect(merged.Conflicts.empty() && !merged.RemoteStale && merged.Problem == "website problem" && merged.Solution == "local solution", "remote truth merges distinct brief fields");
		const auto proposed = propose_tour_description(merged, raw);
		expect(proposed && proposed->starts_with("Template α\r\n") && proposed->ends_with("## Tests\r\n\r\nKEEP EXACT\r\n") && proposed->contains("\r\nlocal solution\r\n\r\n"), "narrow description replacement preserves unrelated template and CRLF framing");
		local.Problem = "local problem";
		merged        = reconcile_tour(local, remote);
		expect(merged.Conflicts.size() == 1 && merged.Conflicts.front().Baseline == "old problem" && merged.Problem == "local problem" && !plan_tour_update(merged, { }), "overlap retains B/L/R and refuses publication");
		auto resolved = resolve_tour_conflict(merged, 0, false);
		expect(resolved && resolved->Conflicts.empty() && resolved->Problem == "local problem" && resolved->ProblemBaseline->DraftText == "website problem", "explicit local choice keeps fresh remote baseline");
		resolved = resolve_tour_conflict(merged, 0, true);
		expect(resolved && resolved->Problem == "website problem", "explicit website choice adopts remote");
		auto drift = remote;
		drift.ProblemBaseline.reset();
		merged = reconcile_tour(local, drift);
		expect(merged.RemoteStale && merged.Problem == local.Problem, "same-position heading change cannot authorize an obsolete source span");
		remote          = base;
		remote.ReadOnly = true;
		remote.Stops.clear();
		merged = reconcile_tour(local, remote);
		expect(merged.RemoteStale && merged.Stops == local.Stops && merged.Problem == local.Problem, "partial read never deletes or replaces local work");
		remote               = base;
		remote.Owner.Account = github_account_id{ 99 };
		expect(reconcile_tour(local, remote) == local, "foreign account observation ignored");
		remote              = base;
		local               = base;
		local.Stops[0].Body = "local explanation";
		remote.Stops[0].Baseline->Raw.insert(0, "Website preface.\n\n");
		merged = reconcile_tour(local, remote);
		expect(merged.Conflicts.empty() && merged.Stops[0].Body.contains("local explanation") && merged.Stops[0].Baseline->Raw.starts_with("Website preface"), "nonoverlapping website prefix and local narrative survive span drift");
		remote = base;
		local  = base;
		std::swap(local.Stops[0], local.Stops[1]);
		std::swap(remote.Stops[3], remote.Stops[4]);
		const auto newRoute = plan_tour_update(remote, { });
		expect(newRoute.has_value(), "prepare website route");
		remote.IndexBaseline->Raw = newRoute->Index;
		local.Stops[0].Body       = "preserve local narrative";
		merged                    = reconcile_tour(local, remote);
		expect(merged.Conflicts.size() == 1 && merged.Conflicts.front().Field == tour_conflict_field::route, "concurrent reorders require explicit route choice");
		resolved = resolve_tour_conflict(merged, 0, true);
		expect(resolved && resolved->Stops[1].Body.contains("preserve local narrative") && resolved->Stops[3].Remote == base.Stops[4].Remote, "accepting website route retains separately merged local text and remote identities");
	}

}
int main()
{
	reconciliation();
	auto draft = published();
	auto plan  = plan_tour_update(draft, { });
	expect(plan && plan->Edits.empty(), "equal body route is zero writes");
	std::swap(draft.Stops[1], draft.Stops[2]);
	plan = plan_tour_update(draft, { });
	expect(plan && plan->Edits.size() == 5 && plan->Edits.back().Remote == *draft.Remote, "adjacent move affects only four neighbors/ordinals and final index");
	expect(plan->Index.starts_with("Preserve this custom orientation.") && plan->Index.ends_with("Unrelated tail.\n"), "root patch preserves non-tour prose exactly");
	expect(std::ranges::none_of(plan->Edits, [&](const auto& edit) { return edit.Remote == *draft.Stops.back().Remote; }), "distant unaffected stop receives no write");
	draft = published();
	draft.Retired.push_back(draft.Stops[2]);
	draft.Stops.erase(draft.Stops.begin() + 2);
	plan = plan_tour_update(draft, { });
	expect(plan && std::ranges::any_of(plan->Edits, [&](const auto& edit) { return edit.Remote == *draft.Retired.front().Remote && edit.Desired.contains("Exact explanation 3") && edit.Desired.contains("Tour overview") && !edit.Desired.contains("[Next]"); }), "retirement preserves discussion and overview without route navigation");
	draft            = published();
	auto inserted    = draft.Stops.front();
	inserted.Id      = tour_stop_id{ 6 };
	draft.LastStopId = 6;
	inserted.Remote.reset();
	inserted.Baseline.reset();
	draft.Stops.insert(draft.Stops.begin() + 2, inserted);
	expect(!plan_tour_update(draft, { }), "new destination must exist before index update");
	const tour_created_destination created{ inserted.Id, tour_remote_binding{ .Kind = tour_remote_kind::line_comment, .DatabaseId = 40, .Entity = "new", .Actor = "author", .WebUrl = "https://github.com/o/r/pull/7#discussion_r40", .Review = "10", .Own = true }, *render_tour_stop(inserted.Title, 1, inserted.Body, std::vector<tour_markdown_link>{ { "Tour overview", draft.Remote->WebUrl } }) };
	plan = plan_tour_update(draft, std::vector{ created });
	expect(plan && plan->Index.contains("discussion_r40") && plan->Edits.back().Remote == *draft.Remote, "second batch joins original index last using returned URL");
	draft                       = published();
	draft.Stops[0].Mapped       = draft.Stops[0].Original;
	draft.Stops[0].Mapped->Path = "moved.cpp";
	draft.Stops[0].Mapping      = tour_mapping::unchanged_material;
	auto replacement            = created;
	replacement.Stop            = draft.Stops[0].Id;
	plan                        = plan_tour_update(draft, std::vector{ replacement });
	expect(plan && plan->StopBodies[0].contains("original discussion") && std::ranges::any_of(plan->Edits, [](const auto& edit) { return edit.Desired.contains("replacement stop"); }), "immutable remote subject gets visibly linked replacement and preserved original thread");
	draft = published();
	draft.Stops[0].Baseline->Raw += "\nUnmapped website addition.\n";
	expect(!plan_tour_update(draft, { }), "ambiguous footer layout cannot be overwritten");
	draft                      = published();
	draft.Stops[0].Remote->Own = false;
	expect(!plan_tour_update(draft, { }), "third-party ownership refuses body editing");
	draft = published();
	draft.Stops.front().Baseline->Raw.insert(draft.Stops.front().Baseline->Raw.find("[Next]"), "  ");
	draft.Stops.front().Body                = draft.Stops.front().Baseline->Raw;
	draft.Stops.front().Baseline->DraftText = draft.Stops.front().Body;
	plan                                    = plan_tour_update(draft, { });
	expect(plan && plan->Edits.empty(), "unchanged route preserves exact website footer wording without normalization writes");
	draft.Stops.front().Body += "\nExplicit whole-comment correction.";
	plan = plan_tour_update(draft, { });
	expect(plan && plan->Edits.size() == 1 && plan->Edits.front().Desired.ends_with("Explicit whole-comment correction."), "mapped whole-comment editor retains explicit custom-layout changes");

	std::cout << "TOUR_UPDATE_OK\n";
}
