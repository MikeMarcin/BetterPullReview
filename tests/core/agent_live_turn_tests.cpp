// The Agent tab's running turn folded from what the host observes of its run,
// and what an ended turn's answer is made of (RFC-BPR-0130 stage 5, AC07).
#include "bpr/core/agent_live_turn.hpp"

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

	[[nodiscard]] auto seen(const agent_live_phase aPhase, const std::string_view aWritten) -> agent_live_observation
	{
		auto made    = agent_live_observation{ };
		made.Phase   = aPhase;
		made.Written = std::string{ aWritten };
		return made;
	}

	void test_a_run_begins_runs_and_ends()
	{
		// Seen preparing from idle: the question became a turn.
		auto preparing              = seen(agent_live_phase::preparing, "");
		preparing.WorkspaceChecking = true;
		auto step                   = reduce_agent_live_turn({ }, preparing);
		expect(step.Began && step.PhaseChanged && !step.Ended, "a run that prepares from idle begins a turn");
		expect(step.Turn.Phase == agent_live_phase::preparing && step.Turn.Stage == agent_live_stage::checking, "checking the retained workspace is said as checking");
		preparing.WorkspaceChecking = false;
		step                        = reduce_agent_live_turn(step.Turn, preparing);
		expect(!step.Began && !step.PhaseChanged && step.Turn.Stage == agent_live_stage::preparing, "a workspace being made is said as preparing; nothing began again");

		// Running, nothing written: thinking, with the latest line.
		auto running       = seen(agent_live_phase::running, "");
		running.LatestLine = "Reading src/clamp.cpp";
		running.FilesOnly  = true;
		step               = reduce_agent_live_turn(step.Turn, running);
		expect(step.PhaseChanged && step.Turn.Stage == agent_live_stage::thinking && step.Turn.Latest == "Reading src/clamp.cpp" && step.Turn.FilesOnly, "a run that has written nothing thinks, its latest line said");

		// It writes, costs and reports what it does.
		running.Written             = "It clamps";
		running.Cost                = agent_cost{ 41U };
		running.Activity            = agent_turn_activity{ };
		running.Activity->FilesRead = 2U;
		running.ActivityStartedMs   = 1200;
		step                        = reduce_agent_live_turn(step.Turn, running);
		expect(!step.PhaseChanged && step.Turn.Stage == agent_live_stage::writing && step.Turn.Answer == "It clamps" && step.Turn.Latest.empty(), "once it writes, it writes, and the latest line is not said");
		expect(step.Turn.Cost == std::optional<agent_cost>{ agent_cost{ 41U } } && step.Turn.Activity && step.Turn.Activity->FilesRead == 2U && step.Turn.ActivityStartedMs == 1200, "what it has cost and is doing, as reported");
		running.Activity.reset();
		step = reduce_agent_live_turn(step.Turn, running);
		expect(!step.Turn.Activity && step.Turn.ActivityStartedMs == 0, "what it does is said only while its runtime says it");

		// Steered: said once, as a change.
		running.Steered = { agent_steered_message{ .Text = "Look at the caller too" } };
		step            = reduce_agent_live_turn(step.Turn, running);
		expect(step.SteeredChanged && step.Turn.Steered.size() == 1U, "a steered message is a change");
		step = reduce_agent_live_turn(step.Turn, running);
		expect(!step.SteeredChanged, "the same steered messages again change nothing");

		// Idle: it ended and keeps what it had.
		step = reduce_agent_live_turn(step.Turn, seen(agent_live_phase::idle, "It clamps to its range."));
		expect(step.Ended && step.PhaseChanged && !step.Began && step.Turn.Phase == agent_live_phase::idle, "seen idle, the run ended");
		expect(step.Turn.Answer == "It clamps to its range." && step.Turn.Steered.size() == 1U && step.Turn.Cost.has_value(), "an ended turn keeps what it had for its ending, with the words it ended on");
		step = reduce_agent_live_turn(step.Turn, seen(agent_live_phase::idle, "Other words"));
		expect(!step.Ended && !step.PhaseChanged && step.Turn.Answer == "It clamps to its range.", "idle again: nothing ended, and nothing is folded");

		// The next question starts afresh.
		step = reduce_agent_live_turn(step.Turn, seen(agent_live_phase::preparing, ""));
		expect(step.Began && step.Turn.Answer.empty() && step.Turn.Steered.empty() && !step.Turn.Cost, "the next run carries nothing of the last");
	}

	void test_what_an_ended_answer_is_made_of()
	{
		const auto ended = [](const agent_turn_outcome aOutcome, const std::string_view aWritten, const std::string_view aKept) {
			auto facts    = agent_live_end_facts{ };
			facts.Outcome = aOutcome;
			facts.Written = std::string{ aWritten };
			facts.Kept    = std::string{ aKept };
			return facts;
		};
		expect(decide_agent_live_end(ended(agent_turn_outcome::answered, "It clamps.", "")) == agent_live_end::answered, "an answer is what it wrote");
		expect(decide_agent_live_end(ended(agent_turn_outcome::answered, "", "")) == agent_live_end::answered_silently, "an answer without words says so");
		auto failedWrites         = ended(agent_turn_outcome::answered, "Changed two files.", "");
		failedWrites.FailedWrites = { "src/a.cpp" };
		expect(decide_agent_live_end(failedWrites) == agent_live_end::writes_failed, "every change failed: it says nothing was written");
		expect(decide_agent_live_end(ended(agent_turn_outcome::stopped, "It cl", "It cl")) == agent_live_end::kept_on_stop, "stopped after writing: what arrived is kept");
		expect(decide_agent_live_end(ended(agent_turn_outcome::stopped, "", "")) == agent_live_end::failed, "stopped before writing: why it stopped");
		auto helpers              = ended(agent_turn_outcome::unreadable, "Half", "Half");
		helpers.HelpersUnfinished = true;
		expect(decide_agent_live_end(helpers) == agent_live_end::kept_on_helpers, "ended under its helpers: what arrived is kept, the helpers named");
		expect(decide_agent_live_end(ended(agent_turn_outcome::exited_with_error, "trace", "")) == agent_live_end::failed, "an error is why, then what it wrote");
	}
}

auto main() -> int
{
	test_a_run_begins_runs_and_ends();
	test_what_an_ended_answer_is_made_of();
	std::cout << "agent-live-turn-tests:pass\n";
	return EXIT_SUCCESS;
}
