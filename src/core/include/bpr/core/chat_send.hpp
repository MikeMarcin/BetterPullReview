#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_SEND_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_SEND_HPP

#include <cstdint>

/// \file
/// \brief The one Send and Stop rule of every chat with an agent
/// (RFC-BPR-0130 stage 2, AC05).
/// \details The Agent tab, the tour's side chat and the side chat about one
/// review item each turned Send into Stop, or put Stop beside it, by their
/// own rule. One rule now, the tour chat's, which keeps Stop in reach while
/// words typed meanwhile can wait as a follow-up: Send becomes Stop while a
/// turn runs and nothing typed could follow it; when typed words would be a
/// follow-up (queued, or steered into the running turn) Send says so and
/// Stop stands beside it. A chat that cannot take follow-ups (an item's)
/// only ever turns Send into Stop.
namespace bpr::core {
	/// \brief What the composer's main button says and does.
	enum class chat_send_face : std::uint8_t {
		/// Starts a run with the words typed.
		send,
		/// Puts the words in the queue, to be sent after the running turn.
		queue,
		/// Sends the words into the running turn.
		steer,
		/// Stops the running turn.
		stop,
	};

	/// \brief What a chat observes when its composer is drawn.
	struct chat_send_facts
	{
		/// A turn of this chat runs, so it can be stopped.
		bool Running = false;
		/// Words typed now would be a follow-up rather than a new turn: a
		/// turn runs, or the chat's queue holds messages. Always false in a
		/// chat that takes no follow-ups.
		bool FollowsUp = false;
		/// The field holds words to send.
		bool Typed = false;
		/// A follow-up steers by default (the reviewer's setting); Queue
		/// otherwise.
		bool SteerFirst = false;
	};

	/// \brief The composer's buttons.
	struct chat_send_controls
	{
		chat_send_face Send = chat_send_face::send;
		/// Stop stands beside Send: a turn runs while Send queues or steers.
		bool StopBeside = false;

		auto operator==(const chat_send_controls&) const -> bool = default;
	};

	/// \brief The one rule, for what the chat observed.
	[[nodiscard]] constexpr auto decide_chat_send(const chat_send_facts& aFacts) noexcept -> chat_send_controls
	{
		if (aFacts.FollowsUp && aFacts.Typed) {
			return { .Send = aFacts.SteerFirst ? chat_send_face::steer : chat_send_face::queue, .StopBeside = aFacts.Running };
		}
		return { .Send = aFacts.Running ? chat_send_face::stop : chat_send_face::send, .StopBeside = false };
	}
}

#endif
