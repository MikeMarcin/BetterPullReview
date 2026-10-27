// [[BPR-1781]]. Overview's Tour tab decision, without Qt or a store.
#include "bpr/core/overview_tour.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using core::overview_tour_kind;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto stop(const std::uint64_t aId, std::string aTitle, const core::tour_preparation aPreparation) -> core::tour_stop
	{
		auto value        = core::tour_stop{ };
		value.Id          = core::tour_stop_id{ aId };
		value.Title       = std::move(aTitle);
		value.Preparation = aPreparation;
		return value;
	}

	[[nodiscard]] auto published(const bool aOwn) -> core::tour_remote_binding
	{
		auto binding   = core::tour_remote_binding{ };
		binding.Entity = "IC_root";
		binding.Own    = aOwn;
		return binding;
	}

	void test_no_tour()
	{
		const auto seed = core::review_tour{ };
		expect(core::decide_overview_tour(&seed, nullptr, false).Kind == overview_tour_kind::hidden, "no tour, not the author: no tab");
		expect(core::decide_overview_tour(nullptr, nullptr, false).Kind == overview_tour_kind::hidden, "nothing loaded, not the author: no tab");
		expect(core::decide_overview_tour(&seed, nullptr, true).Kind == overview_tour_kind::prepare, "the author with no tour is offered Prepare");
	}

	void test_my_draft()
	{
		auto draft     = core::review_tour{ };
		draft.Problem  = "Slow start";
		draft.Solution = "Cache it";
		draft.Stops    = { stop(1, "The cache", core::tour_preparation::ready), stop(2, "", core::tour_preparation::needs_attention) };
		for (const auto author : { true, false }) {
			const auto shown = core::decide_overview_tour(&draft, nullptr, author);
			expect(shown.Kind == overview_tour_kind::writer && !shown.Published, "a private draft is mine, whoever wrote the pull request");
			expect(shown.Problem == "Slow start" && shown.Solution == "Cache it" && shown.Stops.size() == 2U, "the brief and every stop");
			expect(shown.Stops[0].Title == "The cache" && shown.Stops[0].Preparation == core::tour_preparation::ready
					   && shown.Stops[1].Preparation == core::tour_preparation::needs_attention,
				   "each stop with its readiness, in order");
		}
		draft.Remote = published(true);
		expect(core::decide_overview_tour(&draft, nullptr, true).Published, "a tour I published says so");
		auto brief    = core::review_tour{ };
		brief.Problem = "Only a problem so far";
		expect(core::decide_overview_tour(&brief, nullptr, true).Kind == overview_tour_kind::writer, "a draft with only its brief is begun");
	}

	void test_reader()
	{
		auto offered     = core::review_tour{ };
		offered.Remote   = published(false);
		offered.Stops    = { stop(1, "Entry point", core::tour_preparation::ready) };
		offered.Problem  = "Why";
		const auto seed  = core::review_tour{ };
		const auto shown = core::decide_overview_tour(&seed, &offered, false);
		expect(shown.Kind == overview_tour_kind::reader && shown.Published && !shown.Imported && shown.Stops.size() == 1U && shown.Problem == "Why",
			   "a published tour on offer is read-only, with its brief and stop titles");
		auto imported     = offered;
		imported.ReadOnly = true;
		const auto taken  = core::decide_overview_tour(&imported, &offered, false);
		expect(taken.Kind == overview_tour_kind::reader && taken.Imported, "an imported published tour is still the reader's, and needs no import");
		auto mine        = offered;
		mine.Remote->Own = true;
		expect(core::decide_overview_tour(&seed, &mine, true).Kind == overview_tour_kind::prepare, "my own published tour, not yet loaded, is continued from Prepare");
		auto empty  = offered;
		empty.Stops = { };
		expect(core::decide_overview_tour(&seed, &empty, false).Kind == overview_tour_kind::hidden, "an offer with no stops is no tour");
		auto unresolved     = core::review_tour{ };
		unresolved.Problem  = "Read only, unpublished";
		unresolved.ReadOnly = true;
		expect(core::decide_overview_tour(&unresolved, nullptr, false).Kind == overview_tour_kind::hidden, "a read-only draft with no published tour is no tour to show");
	}
}

auto main() -> int
{
	test_no_tour();
	test_my_draft();
	test_reader();
	std::cout << "overview tour core tests passed\n";
	return EXIT_SUCCESS;
}
