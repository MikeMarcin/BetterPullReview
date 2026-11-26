#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_RUN_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_RUN_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_cost.hpp"
#include "bpr/core/agent_question.hpp"
#include "bpr/core/agent_session.hpp"

#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bpr::qt {
	enum class finding_queue_action : std::uint8_t;

	/// \brief Where the Agent tab's run stands: nothing runs, its workspace is
	/// prepared, or the agent runs.
	enum class agent_tab_run_phase : std::uint8_t {
		idle,
		preparing,
		running,
	};

	/// \brief RFC-BPR-0130 stage 5 (AC07). What the Agent tab presents of
	/// its agent's run: the agent chosen, the run as it goes, how the last one
	/// ended, and the last refusal of a request.
	///
	/// \details A value the window composes from what the run's presenter
	/// was told and hands to the tab whenever any of it changes, so the tab
	/// reads no other widget. Every field is what that presenter says now;
	/// none is the tab's own.
	struct agent_tab_run
	{
		/// The chosen agent's name ("Claude Code"), empty while none is.
		QString Agent;
		/// The chosen agent, when one is offered and chosen.
		std::optional<core::agent_profile_id> Profile;
		/// The chosen agent's name when it is no longer offered.
		QString UnavailableAgent;
		/// How many agents are offered.
		std::size_t Offered = 0U;
		/// A review can start, and a run can be stopped.
		bool Startable = false;
		bool Stoppable = false;
		/// What the run's presenter says of it ("Finished: success").
		QString             Status;
		agent_tab_run_phase Phase = agent_tab_run_phase::idle;
		/// Its kept workspace is being checked, before the run.
		bool WorkspaceChecking = false;
		/// What the agent has written, and its last line.
		QString                                         Output;
		QString                                         LatestLine;
		std::vector<application::agent_steered_message> Steered;
		std::optional<core::agent_cost>                 RunningCost;
		core::agent_turn_activity                       Activity;
		std::int64_t                                    ActivityStartedMs = 0;
		/// The runtime has said what it does.
		bool ActivityReported = false;
		/// It reads only the pull request's files.
		bool FilesOnly = false;
		/// How the last run ended, and what it kept.
		std::optional<application::agent_run_outcome>       Outcome;
		QString                                             StopReason;
		QString                                             Kept;
		std::optional<application::agent_protocol_failure>  Failure;
		std::vector<std::string>                            UnfinishedHelpers;
		std::vector<std::string>                            FailedWrites;
		std::uint32_t                                       FailedWriteCount = 0U;
		QString                                             ReportedModel;
		std::optional<core::agent_cost>                     TurnCost;
		std::optional<core::agent_cost>                     SessionCost;
		std::optional<core::pinned_conversation_offer>      Pinned;
		std::optional<core::conversation_move_decision>     MoveRefusal;
		std::optional<application::agent_conversation_move> Move;
		/// The session cannot be continued: its history is gone.
		bool StoppedForGood = false;
		/// How many requests were refused, and the words of the last.
		std::uint64_t Refusals = 0U;
		QString       LastRefusal;

		auto operator==(const agent_tab_run&) const -> bool = default;
	};

	/// \brief What made the window hand the tab its run again: the run moved
	/// (a phase, its words, a refusal), an action became possible or not, the
	/// chosen agent changed, or something the tab only reads later.
	enum class agent_tab_run_change : std::uint8_t {
		progressed,
		actions,
		agent,
		quiet,
	};

	/// \brief A question leaving the tab: its words, the question it is
	/// asked as, and a skill's text the agent gets with it.
	struct agent_tab_send
	{
		QString                             Text;
		std::optional<core::agent_question> Question;
		std::optional<std::string>          SkillText;
	};

	/// \brief RFC-BPR-0130 stage 5 (AC07). What the Agent tab asks of the
	/// window: each is a gesture, and the window maps it to the host it
	/// already reaches.
	struct agent_tab_intents
	{
		/// Review this pull request.
		std::function<void()> Start;
		/// Stop the running turn.
		std::function<void()> Stop;
		/// Choose another offered agent; answers whether it is offered.
		std::function<bool(std::uint64_t)> ChooseAgent;
		/// Send a question to the session.
		std::function<void(const agent_tab_send&)> Send;
		/// Answer the session's moved head: New session, or Continue at the
		/// revision it read.
		std::function<void(core::agent_continuation)> Continue;
		/// Act on a finding of the delivery shown, by its place in it.
		std::function<void(std::size_t, finding_queue_action)> Finding;
		/// [[BPR-2051]]. Open the one agent, model and effort picker (the
		/// window's footer holds it), for a bare /model or /effort.
		std::function<void()> OpenRunSettings;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_TAB_RUN_HPP
