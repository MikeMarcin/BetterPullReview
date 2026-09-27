#include "../support/review_tour_fixture.hpp"
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
}
int main()
{
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
	std::cout << "TOUR_UPDATE_OK\n";
}
