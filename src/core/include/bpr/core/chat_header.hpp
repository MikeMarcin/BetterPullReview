#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_HEADER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_HEADER_HPP

#include "bpr/core/agent_cost.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The one header row of a chat with an agent (RFC-BPR-0130, header A
/// as the owner amended it), and the words a chat says of its session.
/// \details Owner, 2026-11-24: "We might need to clean up the top controls on
/// the agent windows. We should also more closely reuse implementation
/// between the side chats and agent chat so that we're not constantly having
/// to double implement changes." The Agent tab, the tour's side chat, a local
/// draft's discussion and the side chat about one review item each say what
/// they are about and name their session, and this decides what the row then
/// holds: History where the chat keeps sessions, Copy conversation where it
/// keeps none, Close where the column closes. No row holds a New session
/// button; it is History's first item. Qt draws the row from this value and
/// decides nothing about which tool a chat has.
namespace bpr::core {
	/// \brief The chats with an agent.
	enum class chat_kind : std::uint8_t {
		/// The pull request's conversation, in the drawer.
		agent_tab,
		/// One part of a tour: a stop, its problem or its solution.
		tour_part,
		whole_tour,
		/// A local draft's discussion: the tour's column, one conversation
		/// and no sessions.
		local_draft_discussion,
		/// A finding's suggested comment, a draft or a thread of the report.
		review_item,
	};

	/// \brief What a chat's header and its History menu can offer.
	enum class chat_header_action : std::uint8_t {
		/// Shows what the chat is about, in the code.
		show,
		history,
		new_session,
		delete_all_sessions,
		/// Chats about other parts of the tour, to read.
		other_tour_chats,
		copy_conversation,
		/// Every run of the pull request's agents, in place of the chat.
		show_every_run,
		close,
	};

	/// \brief The one tool a chat's header shows for its conversation: History
	/// where the chat keeps sessions, Copy conversation where it keeps none.
	/// A chat with a single extra action shows it as its icon, not as a menu
	/// of one.
	enum class chat_header_tool : std::uint8_t {
		history,
		copy_conversation,
	};

	/// \brief How the header names its Show.
	enum class chat_show : std::uint8_t {
		/// Nothing to show: the pull request, the whole tour.
		none,
		/// "Show": the item's place.
		place,
		/// "Show in code": a tour part's lines.
		in_code,
	};

	/// \brief Whether a chat of `aKind` keeps sessions.
	[[nodiscard]] constexpr auto chat_keeps_sessions(const chat_kind aKind) noexcept -> bool
	{
		return aKind == chat_kind::agent_tab || aKind == chat_kind::tour_part || aKind == chat_kind::whole_tour;
	}

	/// \brief Everything a chat of `aKind` can offer from its header row and
	/// its History menu, in the order shown. `show` is listed for every chat
	/// whose subject can have a place; whether this subject has one is a fact
	/// (`chat_header_facts::CanShow`).
	[[nodiscard]] auto chat_actions_offered(chat_kind aKind) -> std::vector<chat_header_action>;

	/// \brief What a chat observes about itself for its header.
	struct chat_header_facts
	{
		chat_kind Kind = chat_kind::agent_tab;
		/// What the chat is about: the pull request's title, a stop and its
		/// title, the kind of review item.
		std::string Subject;
		/// Where that is: a file and its lines. Empty where it has no place.
		std::string Place;
		/// The place can be shown in the code now.
		bool CanShow = false;
		/// More about the subject than its row has room for (a finding's
		/// claim), for the title's tooltip. Empty for none.
		std::string About;
		/// The name the reviewer gave the open session; empty for none.
		std::string SessionName;
		/// The revision the open session reads, as hexadecimal of any length;
		/// empty while it is not known.
		std::string RevisionRead;
		/// What the conversation has cost so far, in a chat that keeps no
		/// sessions (a session's own cost is in its row of History).
		std::optional<agent_cost> Cost = std::nullopt;
	};

	/// \brief The header row, left to right.
	struct chat_header
	{
		/// The row's first line: the open session's name where the reviewer
		/// gave one, else what the chat is about. Drawn on one line, ending in
		/// an ellipsis where it does not fit.
		std::string Title;
		/// What the title's tooltip says besides the whole title: which
		/// subject a named session belongs to, or more about the subject.
		/// Empty for none.
		std::string TitleNote;
		/// Under the title, quieter, one line, ending in an ellipsis where it
		/// does not fit. Empty for none.
		std::string      Place;
		chat_show        Show = chat_show::none;
		chat_header_tool Tool = chat_header_tool::history;
		/// What the tool's tooltip adds: the cost of a conversation that has
		/// no session row to say it. Empty for none.
		std::string ToolNote;
		/// The column closes from its header.
		bool Close = false;
		/// The open session's name, empty for none, and the revision it
		/// reads, seven characters, empty while unknown.
		std::string SessionName;
		std::string RevisionRead;
		/// Everything the row and its menu offer, in the order shown.
		std::vector<chat_header_action> Offered;

		[[nodiscard]] auto offers(chat_header_action aAction) const noexcept -> bool;
		auto               operator==(const chat_header&) const -> bool = default;
	};

	/// \brief The header for what the chat observed.
	[[nodiscard]] auto plan_chat_header(const chat_header_facts& aFacts) -> chat_header;

	/// \brief A revision as every chat writes it: its first seven characters.
	[[nodiscard]] auto chat_revision_words(std::string_view aRevision) -> std::string;

	/// \name What a chat says of its session, in RFC-BPR-0130's accepted words
	/// \details The same in every chat. `aAgent` is the chosen agent's display
	/// name ("Claude Code", "Codex", a custom agent's own); no sentence names
	/// an agent of its own, and "The agent" stands where none is known. They
	/// replace "Native continuation", "Transcript replay" and the replay's
	/// limits in kibibytes, which stay in the diagnostic log.
	/// \{
	/// \brief How the open session goes on with the chosen agent.
	enum class chat_continuation : std::uint8_t {
		/// Where it stopped: the ordinary way, of which nothing is said.
		native,
		/// From its saved transcript.
		carried_over,
		/// Not at all, until a new session or a carry-over is chosen.
		cannot_continue,
	};
	[[nodiscard]] constexpr auto chat_continuation_of(const bool aCarriedOver, const bool aNativeSupported) noexcept -> chat_continuation
	{
		return aCarriedOver ? chat_continuation::carried_over : aNativeSupported ? chat_continuation::native
																				 : chat_continuation::cannot_continue;
	}
	/// \brief The notice on the question field; empty for a session that goes
	/// on in the ordinary way.
	[[nodiscard]] auto chat_session_notice(chat_continuation aContinuation, std::string_view aAgent) -> std::string;
	/// \brief History's item that carries the open session over.
	[[nodiscard]] auto chat_carry_over_words(std::string_view aAgent) -> std::string;
	/// \brief What carrying over leaves out, and what it left out of this
	/// conversation when it left anything.
	[[nodiscard]] auto chat_carry_over_tip(std::size_t aTurnsLeftOut, bool aTextCut) -> std::string;
	/// \brief History's last line: the revision the open session reads.
	[[nodiscard]] auto chat_revision_read_words(std::string_view aRevision) -> std::string;
	/// \}
}

#endif
