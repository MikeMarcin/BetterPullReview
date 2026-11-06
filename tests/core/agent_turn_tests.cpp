/// \file
/// \brief [[BPR-1847]]. An ended agent turn's actions from its outcome,
/// and the suggested prompts' gate on the setup having finished.

#include "bpr/core/agent_turn.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string_view>

namespace {
	using namespace bpr;
	using core::agent_suggestion_facts;
	using core::agent_suggestions_verdict;
	using core::agent_turn_cause;
	using core::agent_turn_facts;
	using core::agent_turn_outcome;
	using core::agent_turn_recovery;
	using core::setup_check_state;
	using core::setup_step;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_names_and_ordinals()
	{
		auto names = std::set<std::string_view>{ };
		for (auto ordinal = std::uint8_t{ 0U }; ordinal < core::agent_turn_outcome_count; ++ordinal) {
			const auto outcome = core::agent_turn_outcome_of(ordinal);
			expect(outcome.has_value() && static_cast<std::uint8_t>(*outcome) == ordinal, "every declared ordinal reads back as itself");
			names.insert(core::stable_name(*outcome));
		}
		expect(names.size() == core::agent_turn_outcome_count && !names.contains(std::string_view{ }), "every outcome has its own stable name");
		expect(!core::agent_turn_outcome_of(static_cast<std::uint8_t>(core::agent_turn_outcome_count)).has_value(), "an undeclared ordinal is none");
		expect(core::legacy_agent_turn_outcome("Stopped after 0:15.") == agent_turn_outcome::ended_without_answer, "a kept stopped ending is not an answer");
		expect(core::legacy_agent_turn_outcome("Claude Code answered in 0:12.") == agent_turn_outcome::answered, "a kept answered ending is an answer");
		expect(core::legacy_agent_turn_outcome("") == agent_turn_outcome::answered, "an empty kept ending is taken as an answer, as before");
	}

	void test_answer_actions()
	{
		const auto answered = core::decide_agent_turn_actions({ .Outcome = agent_turn_outcome::answered, .HasText = true, .Latest = true });
		expect(!answered.Error && answered.Copy && answered.SaveNote && answered.DraftComment && answered.Recovery == agent_turn_recovery::none,
			   "an answer offers Copy, Save as note and Draft comment");
		const auto empty = core::decide_agent_turn_actions({ .Outcome = agent_turn_outcome::answered, .HasText = false, .Latest = true });
		expect(!empty.Error && !empty.Copy && !empty.SaveNote && !empty.DraftComment, "an answer with no words offers nothing");
	}

	void test_error_actions()
	{
		for (auto ordinal = std::uint8_t{ 0U }; ordinal < core::agent_turn_outcome_count; ++ordinal) {
			const auto outcome = *core::agent_turn_outcome_of(ordinal);
			if (outcome == agent_turn_outcome::answered) {
				continue;
			}
			for (const auto latest : { false, true }) {
				for (const auto working : { false, true }) {
					const auto actions = core::decide_agent_turn_actions({ .Outcome = outcome, .HasText = true, .Latest = latest, .Working = working });
					expect(actions.Error, "every non-answer is an error");
					expect(!actions.SaveNote && !actions.DraftComment, "an error never offers Save as note or Draft comment");
					expect(actions.Copy, "an error with words keeps Copy, for a report");
					expect((actions.Recovery != agent_turn_recovery::none) == (latest && !working), "only the latest error, while nothing runs, offers a recovery");
				}
			}
			expect(!core::decide_agent_turn_actions({ .Outcome = outcome, .HasText = false, .Latest = true }).Copy, "an error with no words has nothing to copy");
		}
		const auto recovery = [](const agent_turn_outcome aOutcome) {
			return core::decide_agent_turn_actions({ .Outcome = aOutcome, .HasText = true, .Latest = true }).Recovery;
		};
		expect(recovery(agent_turn_outcome::unreadable) == agent_turn_recovery::agent_settings, "an unreadable answer opens the agent's settings");
		// [[BPR-1876]]. A start or workspace failure resends too: the new
		// run starts the agent and makes its workspace, which is the check.
		for (const auto outcome : { agent_turn_outcome::failed_to_start, agent_turn_outcome::workspace_unavailable, agent_turn_outcome::exited_with_error, agent_turn_outcome::history_unavailable,
									agent_turn_outcome::stopped, agent_turn_outcome::timed_out, agent_turn_outcome::set_aside, agent_turn_outcome::ended_without_answer }) {
			expect(recovery(outcome) == agent_turn_recovery::resend, "any other ending resends");
		}
	}

	void test_fixed_since()
	{
		expect(core::agent_turn_cause_of(agent_turn_outcome::failed_to_start) == agent_turn_cause::agent_check, "a start failure is the agent check's");
		expect(core::agent_turn_cause_of(agent_turn_outcome::workspace_unavailable) == agent_turn_cause::workspace_check, "a workspace failure is the workspace check's");
		auto names = std::set<std::string_view>{ };
		for (const auto cause : { agent_turn_cause::none, agent_turn_cause::agent_check, agent_turn_cause::workspace_check }) {
			names.insert(core::stable_name(cause));
		}
		expect(names.size() == 3U && !names.contains(std::string_view{ }), "every cause has its own stable name");
		for (auto ordinal = std::uint8_t{ 0U }; ordinal < core::agent_turn_outcome_count; ++ordinal) {
			const auto outcome   = *core::agent_turn_outcome_of(ordinal);
			const auto clearable = core::agent_turn_cause_of(outcome) != agent_turn_cause::none;
			expect(clearable == (outcome == agent_turn_outcome::failed_to_start || outcome == agent_turn_outcome::workspace_unavailable), "only a start or workspace failure names a check");
			for (const auto latest : { false, true }) {
				for (const auto working : { false, true }) {
					const auto held = core::decide_agent_turn_actions({ .Outcome = outcome, .HasText = true, .Latest = latest, .Working = working });
					expect(!held.FixedSince, "while its check has not passed since, the cause holds");
					const auto cleared = core::decide_agent_turn_actions({ .Outcome = outcome, .HasText = true, .Latest = latest, .Working = working, .CausePassedSince = true });
					expect(cleared.FixedSince == clearable, "a cleared cause is fixed since, on any turn, and only one a check names");
					expect(cleared.Recovery == held.Recovery && cleared.Error == held.Error && cleared.Copy == held.Copy, "fixed since changes nothing else");
				}
			}
		}
		const auto fixed = core::decide_agent_turn_actions({ .Outcome = agent_turn_outcome::failed_to_start, .HasText = true, .Latest = true, .CausePassedSince = true });
		expect(fixed.FixedSince && fixed.Recovery == agent_turn_recovery::resend, "the latest fixed failure offers Resend");
	}

	void test_delete_never_sent()
	{
		for (auto ordinal = std::uint8_t{ 0U }; ordinal < core::agent_turn_outcome_count; ++ordinal) {
			const auto outcome = *core::agent_turn_outcome_of(ordinal);
			const auto unsent  = outcome == agent_turn_outcome::failed_to_start || outcome == agent_turn_outcome::workspace_unavailable || outcome == agent_turn_outcome::history_unavailable;
			expect(core::agent_turn_never_sent(outcome) == unsent, "only a run that did not start, had no workspace or could not be recorded never reached the agent");
			for (const auto latest : { false, true }) {
				for (const auto working : { false, true }) {
					const auto actions = core::decide_agent_turn_actions({ .Outcome = outcome, .HasText = true, .Latest = latest, .Working = working });
					expect(actions.Delete == (unsent && !working), "a message never sent can be deleted, on any turn, while nothing runs; one the agent received cannot");
				}
			}
		}
		const auto unsent = core::decide_agent_turn_actions({ .Outcome = agent_turn_outcome::failed_to_start, .HasText = true, .Latest = true });
		expect(unsent.Delete && unsent.Recovery == agent_turn_recovery::resend, "a message never sent offers Delete beside Resend");
	}

	void test_suggestions()
	{
		const auto ready = agent_suggestion_facts{ .AgentOffered = true, .SetupStep = setup_step::ready, .AgentCheck = setup_check_state::passed, .WorkspaceCheck = setup_check_state::passed };
		expect(core::decide_agent_suggestions(ready) == agent_suggestions_verdict::shown, "a finished setup shows the suggestions");
		auto facts         = ready;
		facts.AgentOffered = false;
		expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::no_agent, "no agent: none");
		for (const auto step : { setup_step::agent, setup_step::repository }) {
			facts           = ready;
			facts.SetupStep = step;
			expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::setup_unfinished, "an agent or repository still to choose: none");
		}
		facts              = ready;
		facts.SetupShowing = true;
		expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::setup_unfinished, "the setup on screen: none");
		// A reopened application that was set up before stands at Check it
		// works with both checks not run, and is finished.
		facts = { .AgentOffered = true, .SetupStep = setup_step::check };
		expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::shown, "checks not run this session: shown");
		for (const auto state : { setup_check_state::failed, setup_check_state::running }) {
			facts = { .AgentOffered = true, .SetupStep = setup_step::check, .AgentCheck = state };
			expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::setup_unfinished, "an agent check failed or running: none");
			facts = { .AgentOffered = true, .SetupStep = setup_step::check, .WorkspaceCheck = state };
			expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::setup_unfinished, "a workspace check failed or running: none");
		}
		facts         = ready;
		facts.Working = true;
		expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::working, "a turn running: none");
		facts         = ready;
		facts.Toggled = false;
		expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::hidden, "hidden by the reviewer: none");
		facts              = ready;
		facts.SetupShowing = true;
		facts.Toggled      = false;
		facts.Working      = true;
		expect(core::decide_agent_suggestions(facts) == agent_suggestions_verdict::setup_unfinished, "an unfinished setup is said before the rest");
		expect(core::stable_name(agent_suggestions_verdict::setup_unfinished) == "setup_unfinished", "the verdict's stable name");
	}

	// [[BPR-1872]]. Owner, 2026-11-05: a click sends; a Shift-click edits.
	void test_suggestion_press()
	{
		using core::agent_suggestion_press;
		using core::agent_suggestion_press_facts;
		const auto decide = [](const bool aEditFirst, const bool aDraftHeld, const bool aUsable) {
			return core::decide_agent_suggestion_press(agent_suggestion_press_facts{ .EditFirst = aEditFirst, .DraftHeld = aDraftHeld, .Usable = aUsable });
		};
		expect(decide(false, false, true) == agent_suggestion_press::send, "a plain press on an empty field sends");
		expect(decide(true, false, true) == agent_suggestion_press::edit, "a Shift press on an empty field puts the prompt there to edit");
		expect(decide(false, true, true) == agent_suggestion_press::send_keeping_draft, "a plain press with words in the field sends the prompt and keeps the words");
		expect(decide(true, true, true) == agent_suggestion_press::edit_after_draft, "a Shift press with words in the field adds the prompt after them");
		for (const auto edit : { false, true }) {
			for (const auto draft : { false, true }) {
				expect(decide(edit, draft, false) == agent_suggestion_press::ignored, "nothing can be sent: the press does nothing");
			}
		}
		expect(core::stable_name(agent_suggestion_press::send_keeping_draft) == "send_keeping_draft" && core::stable_name(agent_suggestion_press::edit_after_draft) == "edit_after_draft",
			   "the press's stable names");
	}
} // namespace

auto main() -> int
{
	test_names_and_ordinals();
	test_answer_actions();
	test_error_actions();
	test_fixed_since();
	test_delete_never_sent();
	test_suggestions();
	test_suggestion_press();
	std::cout << "agent-turn-tests:pass\n";
	return EXIT_SUCCESS;
}
