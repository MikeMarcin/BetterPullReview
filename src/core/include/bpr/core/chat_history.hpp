#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_HISTORY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_HISTORY_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

/// \file
/// \brief What the History menu of a chat with an agent offers, and whether
/// New session can be chosen now (RFC-BPR-0130, header A as the owner amended
/// it: "move new session into that menu as the top item").
/// \details Owner, attended review 2026-11-24: "New session either needs to
/// always work or get greyed out if it won't. Right now you click it and it
/// does nothing which feels broken." New session is offered only when
/// choosing it changes what the chat shows; otherwise it is refused with the
/// reason, which the menu says. The Agent tab and the tour's side chat build
/// the one menu from the one plan.
namespace bpr::core {
	/// \brief The chats that keep sessions.
	enum class chat_history_kind : std::uint8_t {
		agent_tab,
		tour_side_chat,
	};

	/// \brief Why New session cannot be chosen now.
	enum class new_session_refusal : std::uint8_t {
		/// It can: the open conversation is kept in History and the chat empties.
		none,
		/// No agent is set up to hold a session.
		no_agent,
		/// What the chat is about is gone (a stop removed from the tour): it
		/// takes no new question, so no new session.
		closed,
		/// The chat's sessions are still being read.
		loading,
		/// A turn runs in this chat.
		running,
		/// The chat's agent is at work for another part (a tour's other chat,
		/// its draft).
		busy,
		/// Nothing was asked in the open session: it is the new session.
		already_new,
	};

	/// \brief One item of the History menu, in the order shown.
	enum class chat_history_item : std::uint8_t {
		new_session,
		separator,
		/// One row a session: open, rename, delete, about.
		sessions,
		/// Continue the open conversation from its saved transcript with the
		/// chosen agent.
		carry_over,
		delete_all,
		/// Chats about the tour's other parts, to read.
		other_chats,
		/// The whole conversation as text, where the chat's header has no
		/// Copy of its own (the tour's side chat).
		copy_conversation,
		/// Every run of the pull request's agents, in place of the chat.
		show_every_run,
		reset_agents,
		/// Last, never chosen: the revision the open session reads.
		revision_read,
	};

	/// \brief What a chat observes about itself when its History opens.
	struct chat_history_facts
	{
		chat_history_kind Kind = chat_history_kind::agent_tab;
		/// An agent is chosen and set up.
		bool AgentReady = true;
		/// What the chat is about is gone.
		bool Closed  = false;
		bool Loading = false;
		bool Running = false;
		bool Busy    = false;
		/// Turns the open session has saved, and turns the chat shows now (a
		/// turn that failed before it was saved is shown and not saved).
		std::size_t SavedTurns = 0U;
		std::size_t ShownTurns = 0U;
		/// A question of the open session may have been sent before an
		/// interruption: whether it was used is not known.
		bool Uncertain = false;
		/// The chosen agent can continue a conversation where it stopped.
		bool NativeSupported = true;
		/// The open session continues from a saved transcript.
		bool CarriedOver = false;
		/// Chats about other parts of the tour.
		std::size_t OtherChats = 0U;
		/// The revision the open session reads is known.
		bool RevisionKnown = false;
		/// The chat has the private run list, and the way to reset agents.
		bool EveryRun    = false;
		bool ResetAgents = false;
	};

	/// \brief The menu, top to bottom.
	struct chat_history_plan
	{
		new_session_refusal NewSession = new_session_refusal::none;
		/// Carry over and Copy conversation can be chosen now.
		bool                           CarryOver = false;
		bool                           Copy      = false;
		std::vector<chat_history_item> Items;
		auto                           operator==(const chat_history_plan&) const -> bool = default;
	};

	/// \brief Whether New session changes anything now, and why not.
	/// \details A session nothing was asked in is the new session: choosing
	/// New session again would show the same empty chat under the same name.
	/// The one exception is an agent that cannot continue a conversation where
	/// it stopped, whose empty session New session turns into one it can use.
	[[nodiscard]] auto decide_new_session(const chat_history_facts& aFacts) noexcept -> new_session_refusal;

	/// \brief The History menu for what the chat observed.
	[[nodiscard]] auto plan_chat_history(const chat_history_facts& aFacts) -> chat_history_plan;

	/// \brief The one tool a chat's header shows for its conversation: History
	/// where the chat keeps sessions, Copy conversation where it keeps none.
	enum class chat_header_tool : std::uint8_t {
		history,
		copy_conversation,
	};
	[[nodiscard]] constexpr auto chat_header_tool_for(const bool aKeepsSessions) noexcept -> chat_header_tool
	{
		return aKeepsSessions ? chat_header_tool::history : chat_header_tool::copy_conversation;
	}
}

#endif
