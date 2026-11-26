#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_NOTICE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_NOTICE_HPP

#include "bpr/core/chat_header.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The one notice of every chat with an agent (RFC-BPR-0130 stage 6,
/// AC08).
/// \details A send refused, a run that stopped before answering, a session
/// whose pull request moved on and a conversation carried over (or one the
/// chosen agent cannot pick up) were each said by the chat that reached them,
/// in its own place and words: the Agent tab's refusal strip and state
/// banner, the tour chat's "Head changed. This chat reads …" line and its
/// Continue old revision button, the item chat's status line. Each is now a
/// `chat_notice` planned here, with its words and its choices, and every chat
/// draws it with the one notice view. A chat says what it observed; which
/// words and which choices follow is decided once. Pure.
namespace bpr::core {
	/// \brief What a notice says.
	enum class chat_notice_kind : std::uint8_t {
		/// A send was refused; the words stay in the field.
		send_refused,
		/// The run stopped before it answered.
		stopped,
		/// The session reads a revision the pull request has moved on from
		/// (the pinned-revision card).
		head_moved,
		/// The open session goes on from a saved transcript.
		carried_over,
		/// The chosen agent cannot pick the open session up.
		cannot_continue,
		/// What a chat says of its own run or its own items, in its own
		/// words (the Agent tab's working and finished reports, an item's
		/// proposal that could not be used).
		said,
	};

	/// \brief How a notice is drawn.
	enum class chat_notice_form : std::uint8_t {
		/// One quiet line over the question field.
		line,
		/// One line in a card with a close, over the question field.
		strip,
		/// A card with a title, its words and its choices.
		card,
	};

	/// \brief The card's tone.
	enum class chat_notice_tone : std::uint8_t {
		quiet,
		/// Asks for a choice, or reports a run.
		callout,
		/// A run that finished.
		ready,
		/// Something did not happen.
		danger,
	};

	/// \brief What a notice's choice does.
	enum class chat_notice_action : std::uint8_t {
		/// Keep the session and ask about the revision it read.
		continue_at,
		/// History's New session.
		new_session,
		/// Send the kept message again.
		resend,
		/// Show or fold what the stopped run wrote.
		toggle_log,
		/// Stop the running turn.
		stop,
	};

	/// \brief One choice on a notice.
	struct chat_notice_choice
	{
		chat_notice_action Action = chat_notice_action::new_session;
		std::string        Words;
		/// The choice's words where the card is too narrow for `Words`;
		/// empty when `Words` always fits.
		std::string ShortWords;
		std::string Tip;
		/// Choosing it starts a run (it wears the agent's accent).
		bool Starts = false;
		/// It can be chosen now; `Tip` says why not when it cannot.
		bool Enabled = true;
		/// An action beside the card's choice rather than a choice of its
		/// own (Show the log): drawn bare at rest.
		bool Aside = false;

		auto operator==(const chat_notice_choice&) const -> bool = default;
	};

	/// \brief One notice. A notice with no title and no words says nothing
	/// and is not drawn.
	struct chat_notice
	{
		chat_notice_kind Kind = chat_notice_kind::said;
		chat_notice_form Form = chat_notice_form::line;
		chat_notice_tone Tone = chat_notice_tone::quiet;
		std::string      Title;
		std::string      Words;
		/// More than the words say, for the notice's tooltip; empty for none.
		std::string                     Tip;
		std::vector<chat_notice_choice> Choices;
		/// It closes with its own close.
		bool Dismissable = false;

		[[nodiscard]] auto says() const noexcept -> bool { return !Title.empty() || !Words.empty(); }
		auto               operator==(const chat_notice&) const -> bool = default;
	};

	/// \brief A send refused: "Not sent. ", why, and the host's own detail
	/// in brackets when it gave one.
	[[nodiscard]] auto plan_send_refused_notice(std::string_view aWhy, std::string_view aDetail = { }) -> chat_notice;

	/// \brief What a chat observed of a session whose pull request moved on.
	struct chat_moved_head_facts
	{
		/// The revision the session reads, as hexadecimal of any length;
		/// empty while it is not known.
		std::string Read;
		/// A question was refused for it, and is kept to be sent with the
		/// choice.
		bool Asked = false;
		/// Why it was refused, said when nothing else in the chat says it;
		/// empty otherwise.
		std::string Reason;
		/// The session can go on at the revision it read.
		bool ContinueOffered = true;
		/// Why New session cannot be chosen now; empty when it can.
		std::string NewSessionRefused;
		/// The chat delivered findings at the revision read (the Agent tab).
		bool KeepsFindings = false;
	};

	/// \brief The pinned-revision card: which revision the session read, and
	/// the two ways on, Continue at that revision and New session, in that
	/// order, the same in every chat.
	[[nodiscard]] auto plan_moved_head_notice(const chat_moved_head_facts& aFacts) -> chat_notice;
	/// \brief What a send or a suggested prompt waits on while the card asks:
	/// its choices by the words on them.
	[[nodiscard]] auto chat_moved_head_wait_words(std::string_view aRead, bool aContinueOffered = true) -> std::string;

	/// \brief What a chat observed of a run that stopped before answering.
	struct chat_stopped_facts
	{
		/// The agent's display name; "The agent" where none is known.
		std::string Agent;
		/// Why it stopped and what to do, said when the stopped turn does
		/// not say it; empty otherwise.
		std::string Reason;
		/// Sending again would go another way, so Resend is offered.
		bool ResendOffered = true;
		/// What the run wrote is shown under the card.
		bool LogShown = false;
	};

	/// \brief The card of a run that stopped before answering: Resend, and
	/// Show the log beside it.
	[[nodiscard]] auto plan_stopped_notice(const chat_stopped_facts& aFacts) -> chat_notice;

	/// \brief What the chat says of its open session: nothing when it goes
	/// on in the ordinary way, else that it was carried over or cannot be
	/// picked up, naming `aAgent` (`chat_session_notice`'s words). `aTip` is
	/// what was left out, when anything was.
	[[nodiscard]] auto plan_session_notice(chat_continuation aContinuation, std::string_view aAgent, std::string aTip = { }) -> chat_notice;

	/// \brief A chat's own quiet line.
	[[nodiscard]] auto plan_said_notice(std::string aWords, std::string aTip = { }) -> chat_notice;
}

#endif
