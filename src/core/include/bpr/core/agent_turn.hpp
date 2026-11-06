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
		/// Send the turn's question again, as a new run. The run starts the
		/// agent and makes its workspace, so it checks both as it goes
		/// ([[BPR-1876]]).
		ask_again,
		/// Open the agent's settings: its program or protocol is wrong.
		agent_settings,
	};

	[[nodiscard]] auto stable_name(agent_turn_recovery aRecovery) noexcept -> std::string_view;

	/// \brief [[BPR-1876]]. The setup check whose passing says an error
	/// turn's cause is gone.
	enum class agent_turn_cause : std::uint8_t {
		/// The ending names no cause a setup check can clear.
		none,
		/// The agent did not start: the setup's agent check.
		agent_check,
		/// The agent's workspace could not be made: the setup's workspace
		/// check.
		workspace_check,
	};

	[[nodiscard]] auto stable_name(agent_turn_cause aCause) noexcept -> std::string_view;

	/// \brief The check that clears a turn's cause, for how it ended.
	[[nodiscard]] auto agent_turn_cause_of(agent_turn_outcome aOutcome) noexcept -> agent_turn_cause;

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
		/// [[BPR-1876]]. The check its cause names (`agent_turn_cause_of`)
		/// has run again since the turn ended, and passed.
		bool CausePassedSince = false;

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
		/// [[BPR-1876]]. The cause the error names is no longer true: it is
		/// said as fixed since, not as how things stand.
		bool FixedSince = false;

		auto operator<=>(const agent_turn_actions&) const = default;
	};

	/// \brief The actions one ended turn offers.
	///
	/// \details An answer with text offers Copy, Save as note and Draft
	/// comment and is not an error. Every other outcome is an error: it keeps
	/// Copy when it has words, never offers Save as note or Draft comment,
	/// and, on the latest turn while nothing runs, offers one recovery: an
	/// unreadable answer opens the agent's settings, and every other ending,
	/// a start or workspace failure among them, asks again. [[BPR-1876]]:
	/// the owner, 2026-11-05, read a failed answer's Check again, which re-ran
	/// only the setup check and left the old error up, as the check failing.
	/// A start or workspace failure whose check has passed since is fixed
	/// since, on any turn.
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

	/// \brief [[BPR-1872]]. What the Agent tab knows when a suggested prompt
	/// is pressed.
	struct agent_suggestion_press_facts
	{
		/// Shift was held: a Shift-click or Shift-Return on the prompt.
		bool EditFirst = false;
		/// The question field holds words the reviewer typed (not only
		/// spaces or line breaks).
		bool DraftHeld = false;
		/// A prompt can be sent now: the review is open and no turn runs.
		bool Usable = false;

		auto operator<=>(const agent_suggestion_press_facts&) const = default;
	};

	/// \brief What one press on a suggested prompt does.
	enum class agent_suggestion_press : std::uint8_t {
		/// Nothing: no prompt can be sent now.
		ignored,
		/// Sent as written, as if typed and sent.
		send,
		/// Sent as written; the reviewer's own words stay in the field.
		send_keeping_draft,
		/// Put in the empty field to edit, the cursor at its end.
		edit,
		/// Put after the reviewer's own words in the field, on its own line.
		edit_after_draft,
	};

	[[nodiscard]] auto stable_name(agent_suggestion_press aPress) noexcept -> std::string_view;

	/// \brief [[BPR-1872]]. Owner, 2026-11-05: "by default a click on a
	/// suggested prompt should submit it. Maybe a shift click to let you edit
	/// before submitting".
	///
	/// \details A plain press sends; a Shift press edits. Words already in the
	/// field are never replaced or sent with the prompt: a plain press sends
	/// the prompt alone and leaves them in the field, and a Shift press adds
	/// the prompt after them.
	[[nodiscard]] auto decide_agent_suggestion_press(const agent_suggestion_press_facts& aFacts) noexcept -> agent_suggestion_press;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_TURN_HPP
