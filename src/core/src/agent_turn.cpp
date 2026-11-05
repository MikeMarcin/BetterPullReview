#include "bpr/core/agent_turn.hpp"

namespace bpr::core {
	auto stable_name(const agent_turn_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case agent_turn_outcome::answered:              return "answered";
		case agent_turn_outcome::unreadable:            return "unreadable";
		case agent_turn_outcome::exited_with_error:     return "exited_with_error";
		case agent_turn_outcome::failed_to_start:       return "failed_to_start";
		case agent_turn_outcome::workspace_unavailable: return "workspace_unavailable";
		case agent_turn_outcome::history_unavailable:   return "history_unavailable";
		case agent_turn_outcome::stopped:               return "stopped";
		case agent_turn_outcome::timed_out:             return "timed_out";
		case agent_turn_outcome::set_aside:             return "set_aside";
		case agent_turn_outcome::ended_without_answer:  return "ended_without_answer";
		}
		return { };
	}

	auto agent_turn_outcome_of(const std::uint8_t aOrdinal) noexcept -> std::optional<agent_turn_outcome>
	{
		if (aOrdinal >= agent_turn_outcome_count) {
			return std::nullopt;
		}
		return static_cast<agent_turn_outcome>(aOrdinal);
	}

	auto legacy_agent_turn_outcome(const std::string_view aEnding) noexcept -> agent_turn_outcome
	{
		return aEnding.starts_with("Stopped after ") ? agent_turn_outcome::ended_without_answer : agent_turn_outcome::answered;
	}

	auto stable_name(const agent_turn_recovery aRecovery) noexcept -> std::string_view
	{
		switch (aRecovery) {
		case agent_turn_recovery::none:           return "none";
		case agent_turn_recovery::ask_again:      return "ask_again";
		case agent_turn_recovery::check_setup:    return "check_setup";
		case agent_turn_recovery::agent_settings: return "agent_settings";
		}
		return { };
	}

	auto decide_agent_turn_actions(const agent_turn_facts& aFacts) noexcept -> agent_turn_actions
	{
		if (aFacts.Outcome == agent_turn_outcome::answered) {
			return { .Error = false, .Copy = aFacts.HasText, .SaveNote = aFacts.HasText, .DraftComment = aFacts.HasText, .Recovery = agent_turn_recovery::none };
		}
		auto actions = agent_turn_actions{ .Error = true, .Copy = aFacts.HasText };
		if (!aFacts.Latest || aFacts.Working) {
			return actions;
		}
		switch (aFacts.Outcome) {
		case agent_turn_outcome::unreadable:            actions.Recovery = agent_turn_recovery::agent_settings; break;
		case agent_turn_outcome::failed_to_start:
		case agent_turn_outcome::workspace_unavailable: actions.Recovery = agent_turn_recovery::check_setup; break;
		case agent_turn_outcome::exited_with_error:
		case agent_turn_outcome::history_unavailable:
		case agent_turn_outcome::stopped:
		case agent_turn_outcome::timed_out:
		case agent_turn_outcome::set_aside:
		case agent_turn_outcome::ended_without_answer:  actions.Recovery = agent_turn_recovery::ask_again; break;
		case agent_turn_outcome::answered:              break;
		}
		return actions;
	}

	auto stable_name(const agent_suggestions_verdict aVerdict) noexcept -> std::string_view
	{
		switch (aVerdict) {
		case agent_suggestions_verdict::shown:            return "shown";
		case agent_suggestions_verdict::no_agent:         return "no_agent";
		case agent_suggestions_verdict::setup_unfinished: return "setup_unfinished";
		case agent_suggestions_verdict::working:          return "working";
		case agent_suggestions_verdict::hidden:           return "hidden";
		}
		return { };
	}

	auto decide_agent_suggestions(const agent_suggestion_facts& aFacts) noexcept -> agent_suggestions_verdict
	{
		if (!aFacts.AgentOffered) {
			return agent_suggestions_verdict::no_agent;
		}
		const auto unsettled = [](const setup_check_state aState) { return aState == setup_check_state::failed || aState == setup_check_state::running; };
		const auto finished  = aFacts.SetupStep == setup_step::ready
			|| (aFacts.SetupStep == setup_step::check && !unsettled(aFacts.AgentCheck) && !unsettled(aFacts.WorkspaceCheck));
		if (aFacts.SetupShowing || aFacts.ReadyShowing || !finished) {
			return agent_suggestions_verdict::setup_unfinished;
		}
		if (aFacts.Working) {
			return agent_suggestions_verdict::working;
		}
		if (!aFacts.Toggled) {
			return agent_suggestions_verdict::hidden;
		}
		return agent_suggestions_verdict::shown;
	}
	auto stable_name(const agent_suggestion_press aPress) noexcept -> std::string_view
	{
		switch (aPress) {
		case agent_suggestion_press::ignored:            return "ignored";
		case agent_suggestion_press::send:               return "send";
		case agent_suggestion_press::send_keeping_draft: return "send_keeping_draft";
		case agent_suggestion_press::edit:               return "edit";
		case agent_suggestion_press::edit_after_draft:   return "edit_after_draft";
		}
		return { };
	}

	auto decide_agent_suggestion_press(const agent_suggestion_press_facts& aFacts) noexcept -> agent_suggestion_press
	{
		if (!aFacts.Usable) {
			return agent_suggestion_press::ignored;
		}
		if (aFacts.EditFirst) {
			return aFacts.DraftHeld ? agent_suggestion_press::edit_after_draft : agent_suggestion_press::edit;
		}
		return aFacts.DraftHeld ? agent_suggestion_press::send_keeping_draft : agent_suggestion_press::send;
	}
} // namespace bpr::core
