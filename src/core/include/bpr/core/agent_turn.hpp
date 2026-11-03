#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_TURN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_TURN_HPP

#include "bpr/core/agent_setup.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1847]]. How one turn of the Agent tab's thread ended.
	///
	/// \details Owner, 2026-11-03: "Agent wizard errors shouldn't get the
	/// save as note and draft comment treatment." A turn is either an answer
	/// or one of the ways a run ends without one; the thread keeps which, so a
	/// turn read back from the stored conversation is presented as it ended.
	enum class agent_turn_outcome : std::uint8_t {
		/// The agent answered.
		answered,
		/// The agent wrote in a form this application cannot read.
		unreadable,
		/// The agent exited with an error before answering.
		exited_with_error,
		/// The agent did not start.
		failed_to_start,
		/// The agent's workspace could not be made, so it did not start.
		workspace_unavailable,
		/// Private history could not record the run, so it did not start.
		history_unavailable,
		/// The reviewer stopped it before it answered.
		stopped,
		/// It ran out of its time limit before answering.
		timed_out,
		/// The pull request moved on while it worked; its answer was set aside.
		set_aside,
		/// A turn kept before outcomes were kept, whose ending says it
		/// stopped without an answer; why is not known.
		ended_without_answer,
	};

	inline constexpr std::size_t agent_turn_outcome_count = 10U;

	static_assert(static_cast<std::size_t>(agent_turn_outcome::ended_without_answer) + 1U == agent_turn_outcome_count,
				  "agent_turn_outcome_count must cover every declared outcome");

	[[nodiscard]] auto stable_name(agent_turn_outcome aOutcome) noexcept -> std::string_view;

	/// \brief The outcome a stored ordinal names, or none for one this
	/// version does not declare.
	[[nodiscard]] auto agent_turn_outcome_of(std::uint8_t aOrdinal) noexcept -> std::optional<agent_turn_outcome>;

	/// \brief How a turn stored before its outcome was kept ended, from the
	/// ending line this application wrote for it: "Stopped after …" is a run
	/// that ended without an answer; anything else was an answer.
	[[nodiscard]] auto legacy_agent_turn_outcome(std::string_view aEnding) noexcept -> agent_turn_outcome;

	/// \brief The one way back an error turn offers.
	enum class agent_turn_recovery : std::uint8_t {
		none,
		/// Send the turn's question again.
		ask_again,
		/// Open the setup at Check it works and run the check.
		check_setup,
		/// Open the agent's settings: its program or protocol is wrong.
		agent_settings,
	};

	[[nodiscard]] auto stable_name(agent_turn_recovery aRecovery) noexcept -> std::string_view;

	/// \brief What the thread knows about one ended turn.
	struct agent_turn_facts
	{
		agent_turn_outcome Outcome = agent_turn_outcome::answered;
		/// The turn shows some text: the answer, or the error's words.
		bool HasText = false;
		/// It is the thread's last turn, the one a recovery acts for.
		bool Latest = false;
		/// Another turn is running now.
		bool Working = false;

		auto operator<=>(const agent_turn_facts&) const = default;
	};

	/// \brief How one ended turn is presented and what it offers.
	struct agent_turn_actions
	{
		/// Presented as an error, not an answer.
		bool Error = false;
		/// Copy: the answer, or the error's words and what the agent wrote
		/// to its error stream, for a report.
		bool Copy = false;
		/// Save as note and Draft comment: only for an answer.
		bool                SaveNote     = false;
		bool                DraftComment = false;
		agent_turn_recovery Recovery     = agent_turn_recovery::none;

		auto operator<=>(const agent_turn_actions&) const = default;
	};

	/// \brief The actions one ended turn offers.
	///
	/// \details An answer with text offers Copy, Save as note and Draft
	/// comment and is not an error. Every other outcome is an error: it keeps
	/// Copy when it has words, never offers Save as note or Draft comment,
	/// and, on the latest turn while nothing runs, offers one recovery: an
	/// unreadable answer opens the agent's settings, a start or workspace
	/// failure checks the setup again, and any other ending asks again.
	[[nodiscard]] auto decide_agent_turn_actions(const agent_turn_facts& aFacts) noexcept -> agent_turn_actions;

	/// \brief What the Agent tab knows when it would show suggested prompts.
	struct agent_suggestion_facts
	{
		/// An agent is offered to ask.
		bool AgentOffered = false;
		/// The setup is on screen in place of the conversation.
		bool SetupShowing = false;
		/// The setup's first unfinished step (`current_setup_step`).
		setup_step SetupStep = setup_step::agent;
		/// The setup's two checks as they stand this session.
		setup_check_state AgentCheck     = setup_check_state::not_run;
		setup_check_state WorkspaceCheck = setup_check_state::not_run;
		/// The setup's Ready line is still shown above the conversation.
		bool ReadyShowing = false;
		/// A turn is running.
		bool Working = false;
		/// The reviewer's Suggestions toggle is on.
		bool Toggled = true;

		auto operator<=>(const agent_suggestion_facts&) const = default;
	};

	/// \brief Whether the suggested prompts show, or why not.
	enum class agent_suggestions_verdict : std::uint8_t {
		shown,
		/// No agent is offered: the tab shows how to set one up.
		no_agent,
		/// The setup has not finished: an agent or repository is still to be
		/// chosen, a check failed or runs, the setup is on screen, or its
		/// Ready line has not been read yet.
		setup_unfinished,
		/// A turn is running ([[BPR-1755]]).
		working,
		/// The reviewer hid them.
		hidden,
	};

	[[nodiscard]] auto stable_name(agent_suggestions_verdict aVerdict) noexcept -> std::string_view;

	/// \brief [[BPR-1847]]. Owner, 2026-11-03: "doesn't make sense to show
	/// suggested prompts before the wizard completes." Decided in this
	/// order: an agent offered, the setup finished, no turn running, the
	/// reviewer's toggle on.
	///
	/// \details The setup counts as finished at its Ready step, or at Check
	/// it works when neither check has failed or is running this session: the
	/// checks are not kept, so a reopened application that was set up before
	/// stands at Check it works with both not run.
	[[nodiscard]] auto decide_agent_suggestions(const agent_suggestion_facts& aFacts) noexcept -> agent_suggestions_verdict;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_TURN_HPP
