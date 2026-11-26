#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_LIVE_TURN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_LIVE_TURN_HPP

#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_cost.hpp"
#include "bpr/core/agent_steering.hpp"
#include "bpr/core/agent_turn.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief The Agent tab's running turn as one value, folded from what the
/// host observes of its run (RFC-BPR-0130 stage 5, AC07).
/// \details The tab kept the running turn in its widget: the phase it last
/// saw, what the run had written, what was steered into it, what it had cost
/// and was doing, and it decided there what the turn says it is doing and how
/// it ended. The host now hands each observation of the run to
/// `reduce_agent_live_turn`, which says what the turn is and what the
/// observation changed (a turn began, its steered messages changed, it
/// ended); `decide_agent_live_end` says what an ended turn's answer is made
/// of. The words stay the presentation's. Pure.
namespace bpr::core {
	/// \brief Where the run of the Agent tab's question stands.
	enum class agent_live_phase : std::uint8_t {
		/// No run.
		idle,
		/// Its workspace is being made or checked.
		preparing,
		/// The agent runs.
		running,
	};

	/// \brief What the host observed of the run: what it reports each time the
	/// run moves on.
	struct agent_live_observation
	{
		agent_live_phase Phase = agent_live_phase::idle;
		/// While preparing: the pull request's retained workspace is checked,
		/// and nothing is made.
		bool WorkspaceChecking = false;
		/// What the agent has written so far, trimmed.
		std::string Written;
		/// The last line it wrote to any stream.
		std::string LatestLine;
		/// What the reviewer steered into it, and how each ended.
		std::vector<agent_steered_message> Steered;
		/// What it has cost so far, once the agent said.
		std::optional<agent_cost> Cost;
		/// What its runtime says it is doing, where the runtime reports it,
		/// and when the run started on the activity clock.
		std::optional<agent_turn_activity> Activity;
		std::int64_t                       ActivityStartedMs = 0;
		/// A review with no clone: it reads only the pull request's files.
		bool FilesOnly = false;

		auto operator==(const agent_live_observation&) const -> bool = default;
	};

	/// \brief How the running turn's line says the run stands.
	enum class agent_live_stage : std::uint8_t {
		preparing,
		/// The retained workspace is checked; nothing is made.
		checking,
		/// It runs and has written nothing an answer is made of.
		thinking,
		writing,
	};

	/// \brief The running turn as the core holds it.
	struct agent_live_turn
	{
		agent_live_phase Phase = agent_live_phase::idle;
		agent_live_stage Stage = agent_live_stage::preparing;
		/// The answer as it grows.
		std::string Answer;
		/// Said after "thinking" while it has written nothing; empty for none
		/// and once it writes.
		std::string                        Latest;
		std::vector<agent_steered_message> Steered;
		std::optional<agent_cost>          Cost;
		std::optional<agent_turn_activity> Activity;
		std::int64_t                       ActivityStartedMs = 0;
		bool                               FilesOnly         = false;

		auto operator==(const agent_live_turn&) const -> bool = default;
	};

	/// \brief One observation folded: the turn as it now is and what changed.
	struct agent_live_step
	{
		agent_live_turn Turn;
		/// A run began: the question became a turn.
		bool Began = false;
		/// The run's phase changed (it began, it started running, it ended).
		bool PhaseChanged = false;
		/// What was steered into the turn changed.
		bool SteeredChanged = false;
		/// The run ended: the turn settles.
		bool Ended = false;

		auto operator==(const agent_live_step&) const -> bool = default;
	};

	/// \brief Folds \p aSeen into \p aTurn. A run begins when an idle turn is
	/// seen preparing; it ends when a turn that was preparing or running is
	/// seen idle, and keeps what it had (what it cost and did) with the words it
	/// ended on. While it prepares the stage is preparing or checking and
	/// the answer is what it has written; while it runs the stage is writing
	/// once it has written, else thinking with its latest line.
	[[nodiscard]] auto reduce_agent_live_turn(const agent_live_turn& aTurn, const agent_live_observation& aSeen) -> agent_live_step;

	/// \brief What an ended turn's answer is made of.
	enum class agent_live_end : std::uint8_t {
		/// What the agent wrote.
		answered,
		/// It finished and wrote nothing: the turn says so.
		answered_silently,
		/// It finished, and every file change it made failed: what it wrote,
		/// then that nothing was written, with the paths.
		writes_failed,
		/// It was stopped after writing: what arrived is kept, and the stop
		/// is said under it.
		kept_on_stop,
		/// Its runtime ended under helpers still out: what arrived is kept,
		/// and the helpers are named under it.
		kept_on_helpers,
		/// It ended without an answer: why, then the tail of what it wrote.
		failed,
	};

	/// \brief What the host knows of the run once it ended.
	struct agent_live_end_facts
	{
		agent_turn_outcome Outcome = agent_turn_outcome::answered;
		/// What it wrote, trimmed, and what a stop kept of it.
		std::string Written;
		std::string Kept;
		/// Its runtime ended with helpers unfinished.
		bool HelpersUnfinished = false;
		/// The file changes it made that all failed; empty when none failed.
		std::vector<std::string> FailedWrites;

		auto operator==(const agent_live_end_facts&) const -> bool = default;
	};

	[[nodiscard]] auto decide_agent_live_end(const agent_live_end_facts& aFacts) -> agent_live_end;
}

#endif
