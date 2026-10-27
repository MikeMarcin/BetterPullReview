// [[BPR-1784]]. The tour's three steps and where each stands, without Qt.
#include "bpr/core/tour_steps.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using core::tour_step;
	using state = core::tour_step_state;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto stop(const std::uint64_t aId, const core::tour_preparation aPreparation) -> core::tour_stop
	{
		auto value        = core::tour_stop{ };
		value.Id          = core::tour_stop_id{ aId };
		value.Title       = "A stop";
		value.Preparation = aPreparation;
		return value;
	}

	[[nodiscard]] auto states(const core::tour_steps& aSteps, const state aProblem, const state aSolution, const state aStops) -> bool
	{
		return aSteps.state(tour_step::problem) == aProblem && aSteps.state(tour_step::solution) == aSolution && aSteps.state(tour_step::stops) == aStops;
	}

	void test_empty_tour_opens_problem()
	{
		const auto empty = core::review_tour{ };
		const auto steps = core::decide_tour_steps(&empty, std::nullopt);
		expect(steps.Current == tour_step::problem && states(steps, state::current, state::to_do, state::to_do), "an empty tour opens on Problem, the rest to do");
		expect(core::decide_tour_steps(nullptr, std::nullopt) == steps, "no draft reads as an empty one");
		auto blank    = core::review_tour{ };
		blank.Problem = " \n\t";
		expect(core::decide_tour_steps(&blank, std::nullopt).Current == tour_step::problem, "whitespace is not a written Problem");
	}

	void test_written_steps_are_done_and_the_next_opens()
	{
		auto tour    = core::review_tour{ };
		tour.Problem = "Slow start";
		auto steps   = core::decide_tour_steps(&tour, std::nullopt);
		expect(steps.Current == tour_step::solution && states(steps, state::done, state::current, state::to_do), "Problem written: done, Solution opens");
		tour.Solution = "Cache it";
		steps         = core::decide_tour_steps(&tour, std::nullopt);
		expect(steps.Current == tour_step::stops && states(steps, state::done, state::done, state::current), "both written: Stops opens");
	}

	void test_the_open_step_is_current_whatever_its_progress()
	{
		auto tour    = core::review_tour{ };
		tour.Problem = "Slow start";
		auto steps   = core::decide_tour_steps(&tour, tour_step::problem);
		expect(steps.Current == tour_step::problem && states(steps, state::current, state::to_do, state::to_do),
			   "a written Problem the author keeps open stays current: moving on is explicit");
		tour.Solution = "Cache it";
		tour.Stops    = { stop(1, core::tour_preparation::ready) };
		steps         = core::decide_tour_steps(&tour, tour_step::solution);
		expect(states(steps, state::done, state::current, state::done), "Change reopens a done step; the others keep their ticks");
	}

	void test_passed_unwritten_steps_need_attention()
	{
		auto tour     = core::review_tour{ };
		tour.Solution = "Cache it";
		auto steps    = core::decide_tour_steps(&tour, tour_step::stops);
		expect(states(steps, state::needs_attention, state::done, state::current), "a Problem passed unwritten needs attention");
		steps = core::decide_tour_steps(&tour, tour_step::solution);
		expect(states(steps, state::needs_attention, state::current, state::to_do), "one after the open step is still to do");
		steps = core::decide_tour_steps(&tour, tour_step::problem);
		expect(states(steps, state::current, state::done, state::to_do), "and opening it again is always allowed");
	}

	void test_stops_are_done_only_when_every_stop_is_ready()
	{
		auto tour     = core::review_tour{ };
		tour.Problem  = "Slow start";
		tour.Solution = "Cache it";
		tour.Stops    = { stop(1, core::tour_preparation::ready), stop(2, core::tour_preparation::editing) };
		auto steps    = core::decide_tour_steps(&tour, tour_step::problem);
		expect(steps.state(tour_step::stops) == state::to_do && steps.Stops == 2U && steps.ReadyStops == 1U, "a stop still being written leaves Stops to do");
		tour.Stops[1].Preparation = core::tour_preparation::ready;
		steps                     = core::decide_tour_steps(&tour, tour_step::problem);
		expect(steps.state(tour_step::stops) == state::done && steps.ReadyStops == 2U, "every stop Ready: Stops done");
		tour.Stops[1].Preparation = core::tour_preparation::needs_attention;
		steps                     = core::decide_tour_steps(&tour, tour_step::problem);
		expect(steps.state(tour_step::stops) == state::needs_attention, "a stop that needs the author: Stops needs attention");
		tour.Stops[1].Preparation = core::tour_preparation::ready;
		tour.Conflicts.push_back({ });
		expect(core::decide_tour_steps(&tour, tour_step::problem).state(tour_step::stops) == state::needs_attention, "a website conflict needs attention");
		tour.Conflicts.clear();
		tour.Stops.clear();
		expect(core::decide_tour_steps(&tour, tour_step::problem).state(tour_step::stops) == state::to_do, "no stops is not done");
	}

	void test_order_and_names()
	{
		expect(core::next_tour_step(tour_step::problem) == tour_step::solution && core::next_tour_step(tour_step::solution) == tour_step::stops && !core::next_tour_step(tour_step::stops), "Next goes Problem, Solution, Stops, then no further");
		expect(core::stable_name(tour_step::problem) == "problem" && core::stable_name(tour_step::stops) == "stops", "steps have stable names");
		expect(core::stable_name(state::needs_attention) == "needs_attention" && core::stable_name(state::to_do) == "to_do", "states have stable names");
	}

	void test_equal_inputs_give_equal_results()
	{
		auto tour    = core::review_tour{ };
		tour.Problem = "Slow start";
		tour.Stops   = { stop(1, core::tour_preparation::suggested) };
		expect(core::decide_tour_steps(&tour, std::nullopt) == core::decide_tour_steps(&tour, std::nullopt), "deterministic");
	}
}

auto main() -> int
{
	test_empty_tour_opens_problem();
	test_written_steps_are_done_and_the_next_opens();
	test_the_open_step_is_current_whatever_its_progress();
	test_passed_unwritten_steps_need_attention();
	test_stops_are_done_only_when_every_stop_is_ready();
	test_order_and_names();
	test_equal_inputs_give_equal_results();
	std::cout << "tour-steps:pass\n";
	return EXIT_SUCCESS;
}
