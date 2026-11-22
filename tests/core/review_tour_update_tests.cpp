#include "../support/review_tour_fixture.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "bpr/core/review_tour_revision.hpp"
#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
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
		entries.reserve(5U);
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
		return bpr::tests::accept_tour_fixture(std::move(draft));
	}
	void published_source_baseline()
	{
		auto       tour     = published();
		auto&      stop     = tour.Stops.front();
		const auto original = stop.Original;
		stop.Mapped         = stop.Original;
		stop.Mapped->Path   = "repaired.cpp";
		stop.Mapping        = tour_mapping::author_selected;
		expect(needs_tour_destination(stop) && !plan_tour_update(tour, { }), "an unpublished repair of a remote subject still requires replacement");
		stop.Destination = destination_of(*stop.Mapped);
		// [[BPR-2014]]. The stop moved to the repaired lines; the author accepts it there.
		stop.Acceptance      = accept_tour_stop(tour, stop, { });
		const auto unchanged = plan_tour_update(tour, { });
		expect(!needs_tour_destination(stop) && unchanged && unchanged->Edits.empty() && stop.Original == original, "the confirmed published repair supports zero-write maintenance without overwriting its original");
		auto cold = tour;
		cold.Stops.front().Original.reset();
		cold.Stops.front().Mapped.reset();
		cold.Stops.front().Mapping = tour_mapping::unavailable;
		expect(observed_tour_changes(tour, cold).empty(), "cold imported published subject does not report the preserved original as a website source change");
		stop.Mapped = stop.Original;
		expect(needs_tour_destination(stop) && !plan_tour_update(tour, { }), "selecting the original again is a new retarget relative to the repaired remote subject");
		stop.Mapped->Path = "another.cpp";
		expect(needs_tour_destination(stop), "a subsequent explicit source change still needs a replacement");
		stop.Remote.reset();
		expect(needs_tour_destination(stop), "a copied local stop with retained source has no remote destination");
	}
	void legacy_published_source()
	{
		auto       tour                    = published();
		auto&      stop                    = tour.Stops.front();
		const auto original                = stop.Original;
		stop.Destination                   = destination_of(*stop.Original);
		stop.Destination->Path             = "before-retarget.cpp";
		const auto stale                   = stop.Destination;
		auto       observed                = tour;
		observed.Stops.front().Destination = destination_of(*original);
		expect(observed_tour_changes(tour, observed).empty(), "the stale imported Destination is not a website source change");
		for (const bool returnToConfirmed : { false, true }) {
			auto value   = stop;
			value.Mapped = value.Original;
			if (!returnToConfirmed) {
				value.Mapped->Path = "next-retarget.cpp";
			}
			value.Mapping = tour_mapping::author_selected;
			tour_command command{ .Draft = tour.Id, .Owner = tour.Owner, .ExpectedRevision = tour.Revision, .Kind = tour_command_kind::edit_stop, .Stop = stop.Id, .Value = value };
			const auto   edited = apply(tour, command);
			const auto&  next   = edited.Accepted.Stops.front();
			expect(edited.Refusal == tour_refusal::none && edited.Changed && next.Destination == std::optional{ destination_of(*original) } && next.Original == original && next.Mapped == value.Mapped && needs_tour_destination(next) != returnToConfirmed, "a later legacy edit retains its confirmed subject before a new mapping, without changing source bytes");
		}
		tour_command observe{ .Draft = tour.Id, .Owner = tour.Owner, .ExpectedRevision = tour.Revision, .Kind = tour_command_kind::observe_head, .Head = *parse_git_commit_oid("4444444444444444444444444444444444444444") };
		const auto   advanced = apply(tour, observe);
		expect(advanced.Refusal == tour_refusal::none && advanced.Changed && advanced.Accepted.Stops.front().Destination == std::optional{ destination_of(*original) } && advanced.Accepted.Stops.front().Original == original && !advanced.Accepted.Stops.front().Mapped && stop.Destination == stale, "head observation retains the legacy confirmed subject before invalidating its mapping and leaves the input intact");
	}
	void website_deltas()
	{
		const auto base     = published();
		auto       local    = base;
		local.Stops[0].Body = "private draft only";
		std::reverse(local.Stops.begin(), local.Stops.end());
		local.Retired.push_back(local.Stops.back());
		local.Stops.pop_back();
		auto privateStop = local.Stops.front();
		privateStop.Id   = tour_stop_id{ 6 };
		privateStop.Remote.reset();
		privateStop.Baseline.reset();
		local.Stops.push_back(privateStop);
		local.LastStopId = 6;
		expect(observed_tour_changes(local, base).empty(), "private text/reorder/removal/insertion is never website history");
		auto remote = base;
		remote.Stops[0].Baseline->Raw += "website edit";
		remote.Stops[1].Destination = destination_of(*remote.Stops[1].Original);
		remote.Stops[1].Original.reset();
		remote.Stops[1].Destination->Path = "renamed.cpp";
		remote.Stops.erase(remote.Stops.begin() + 2);
		auto added           = remote.Stops.back();
		added.Id             = tour_stop_id{ 7 };
		added.Remote->Entity = "new601";
		added.Remote->WebUrl = "https://github.com/o/r/pull/7#discussion_r601";
		remote.Stops.push_back(added);
		std::reverse(remote.Stops.begin(), remote.Stops.end());
		std::vector<tour_markdown_link> entries;
		entries.reserve(remote.Stops.size());
		for (const auto& stop : remote.Stops) {
			entries.push_back({ stop.Title, stop.Remote->WebUrl });
		}
		remote.IndexBaseline->Raw = *render_tour_index(entries);
		const auto changes        = observed_tour_changes(base, remote);
		for (const auto kind : { tour_change_kind::added, tour_change_kind::removed, tour_change_kind::reordered, tour_change_kind::text, tour_change_kind::source }) {
			expect(std::ranges::any_of(changes, [&](const auto& aC) { return aC.Kind == kind; }), "locally retained raw observations distinguish every website delta");
		}
		auto cold = base;
		cold.IndexBaseline.reset();
		expect(observed_tour_changes(cold, remote).empty(), "absent prior raw version does not infer history");
	}

	void reconciliation()
	{
		expect(merge_tour_text("α left / right", "α LEFT / right", "α left / RIGHT") == std::optional<std::string>{ "α LEFT / RIGHT" }, "nonoverlapping UTF-8 edits merge");
		expect(!merge_tour_text("same", "local", "remote"), "overlap preserves explicit conflict");
		auto base           = published();
		auto local          = base;
		local.Stops[0].Body = "local explanation";
		auto remote         = base;
		remote.ReadOnly     = true;
		remote.Stops.clear();
		auto merged = reconcile_tour(local, remote);
		expect(merged.RemoteStale && merged.Stops == local.Stops, "partial read never deletes or replaces local work");
		std::optional<review_tour> resolved;
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
	{
		auto prefixed                   = published();
		prefixed.Stops[0].Baseline->Raw = "Website prefix.\r\n\r\n" + prefixed.Stops[0].Baseline->Raw;
		std::swap(prefixed.Stops[0], prefixed.Stops[1]);
		const auto update = plan_tour_update(prefixed, { });
		expect(update && update->StopBodies[1].starts_with("Website prefix.\r\n\r\n"), "reorder patches unique managed spans and preserves unrelated website prefix");
	}
	{
		auto generated                 = published();
		generated.Stops[0].Suggestion  = tour_suggestion{ "selected-source", "Private reason", "", true };
		generated.Stops[0].Preparation = tour_preparation::editing;
		// [[BPR-2014]]. What gates it now: the stop is not accepted.
		generated.Stops[0].Acceptance.reset();
		const std::vector<tour_destination_plan> destinations(generated.Stops.size(), { tour_representation::native_line, true, false, false, false });
		expect(!plan_tour_update(generated, { }) && assess_tour_update(generated, destinations, false, tour_operation_support::supported) == tour_publication_readiness::incomplete, "published generated stop cannot bypass re-completion through update");
		generated.Stops[0].Preparation = tour_preparation::ready;
		generated                      = bpr::tests::accept_tour_fixture(std::move(generated));
		std::swap(generated.Stops[0], generated.Stops[1]);
		expect(plan_tour_update(generated, { }).has_value(), "derived numbering and footer changes retain generated completion");
		auto       remote = generated;
		const auto at     = remote.Stops[1].Baseline->Raw.find("Exact explanation 1");
		remote.Stops[1].Baseline->Raw.replace(at, 19, "Website explanation");
		const auto reconciled = reconcile_tour(generated, remote);
		expect(reconciled.Stops[1].Suggestion && reconciled.Stops[1].Preparation != tour_preparation::ready, "website narrative change clears local generated acknowledgement");
	}
	{
		// [[BPR-2116]], RFC-BPR-0100 AC06. Which published stops an update
		// writes anew: with the author's text kept, only one edited since; a
		// stop published before that record, only one whose text differs.
		auto tour = published();
		expect(std::ranges::none_of(tour.Stops, tour_stop_text_edited), "a stop published as written is not edited");
		tour.Stops[0].Body += " more";
		expect(tour_stop_text_edited(tour.Stops[0]), "a changed stop is edited");
		const auto navigation = std::array{
			tour_markdown_link{ "Previous", tour.Stops[0].Remote->WebUrl },
			tour_markdown_link{ "Tour overview", tour.Remote->WebUrl },
			tour_markdown_link{ "Next", tour.Stops[2].Remote->WebUrl },
		};
		tour.Stops[1].Baseline->DraftText = tour.Stops[1].Body;
		tour.Stops[1].Body                = "Exact explanation 2 ([`review.cpp:2-4`](https://github.com/o/r/blob/abc/review.cpp#L2-L4))";
		const auto rendered               = render_tour_stop(tour.Stops[1].Title, 2, tour.Stops[1].Body, navigation);
		expect(rendered.has_value(), "the published fixture has a valid navigation footer");
		tour.Stops[1].Baseline->Raw = *rendered;
		tour.Stops[1].Body          = *tour.Stops[1].Baseline->DraftText;
		expect(!tour_stop_text_edited(tour.Stops[1]), "a stop whose author's text is the published one is not edited, though its comment carries permalinks");
		tour.Stops[1].Body += " (`review.cpp:3-4`)";
		expect(tour_stop_text_edited(tour.Stops[1]), "an edit since publishing is edited");
		tour.Stops[2].Baseline.reset();
		expect(!tour_stop_text_edited(tour.Stops[2]), "an unpublished stop is not an edited published one");
	}
	published_source_baseline();
	legacy_published_source();
	website_deltas();
	reconciliation();
	auto draft = published();
	auto plan  = plan_tour_update(draft, { });
	expect(plan && plan->Edits.empty(), "equal body route is zero writes");
	std::swap(draft.Stops[1], draft.Stops[2]);
	plan = plan_tour_update(draft, { });
	expect(plan && plan->Edits.size() == 5 && plan->Edits.back().Remote == *draft.Remote, "adjacent move affects only four neighbors/ordinals and final index");
	expect(plan->Index.starts_with("Preserve this custom orientation.") && plan->Index.ends_with("Unrelated tail.\n"), "root patch preserves non-tour prose exactly");
	expect(std::ranges::none_of(plan->Edits, [&](const auto& aEdit) { return aEdit.Remote == *draft.Stops.back().Remote; }), "distant unaffected stop receives no write");
	draft = published();
	draft.Retired.push_back(draft.Stops[2]);
	draft.Stops.erase(draft.Stops.begin() + 2);
	plan = plan_tour_update(draft, { });
	expect(plan && std::ranges::any_of(plan->Edits, [&](const auto& aEdit) { return aEdit.Remote == *draft.Retired.front().Remote && aEdit.Desired.contains("Exact explanation 3") && aEdit.Desired.contains("Tour overview") && !aEdit.Desired.contains("[Next]"); }), "retirement preserves discussion and overview without route navigation");
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
	expect(plan && plan->Index.contains("pull/7/files#r40") && plan->Edits.back().Remote == *draft.Remote, "second batch joins original index last using returned URL");
	draft                       = published();
	draft.Stops[0].Mapped       = draft.Stops[0].Original;
	draft.Stops[0].Mapped->Path = "moved.cpp";
	draft.Stops[0].Mapping      = tour_mapping::author_selected;
	auto replacement            = created;
	replacement.Stop            = draft.Stops[0].Id;
	draft                       = bpr::tests::accept_tour_fixture(std::move(draft));
	plan                        = plan_tour_update(draft, std::vector{ replacement });
	expect(plan && plan->StopBodies[0].contains("original discussion") && std::ranges::any_of(plan->Edits, [](const auto& aEdit) { return aEdit.Desired.contains("replacement stop"); }), "immutable remote subject gets visibly linked replacement and preserved original thread");
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
	draft                                   = bpr::tests::accept_tour_fixture(std::move(draft));
	plan                                    = plan_tour_update(draft, { });
	expect(plan && plan->Edits.empty(), "unchanged route preserves exact website footer wording without normalization writes");
	draft.Stops.front().Body += "\nExplicit whole-comment correction.";
	draft = bpr::tests::accept_tour_fixture(std::move(draft));
	plan  = plan_tour_update(draft, { });
	expect(plan && plan->Edits.size() == 1 && plan->Edits.front().Desired.ends_with("Explicit whole-comment correction."), "mapped whole-comment editor retains explicit custom-layout changes");
	// [[BPR-1785]]. A tour published before the navigation note gains it on
	// its next update, under the links, with every other byte kept.
	draft = published();
	for (auto& stop : draft.Stops) {
		const auto note = stop.Baseline->Raw.find(std::string{ tour_navigation_note } + "\n");
		if (note != std::string::npos) {
			stop.Baseline->Raw.erase(note);
		}
	}
	plan = plan_tour_update(draft, { });
	expect(plan && plan->Edits.size() == 5 && std::ranges::none_of(plan->Edits, [&](const auto& aEdit) { return aEdit.Remote == *draft.Remote; }), "each old stop gains the note and the index is untouched");
	for (std::size_t i = 0; i < draft.Stops.size(); ++i) {
		expect(plan->StopBodies[i] == draft.Stops[i].Baseline->Raw + std::string{ tour_navigation_note } + "\n", "only the note is added");
	}

	std::cout << "TOUR_UPDATE_OK\n";
}
