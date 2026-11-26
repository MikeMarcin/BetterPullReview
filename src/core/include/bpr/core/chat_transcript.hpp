#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_TRANSCRIPT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_TRANSCRIPT_HPP

#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_cost.hpp"
#include "bpr/core/agent_follow_up.hpp"
#include "bpr/core/agent_steering.hpp"
#include "bpr/core/agent_turn.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// \file
/// \brief The conversation of a chat with an agent as one value, and what
/// changed between two of them (RFC-BPR-0130 stage 3, AC06).
/// \details The Agent tab, the tour's side chat and the side chat about one
/// review item each kept their conversation in a shape of their own and drew
/// it their own way; the two side chats drew every turn again on each change.
/// A chat's projector now turns its own state (which keeps its shape) into a
/// `chat_transcript`: turns by stable identity, each with the question, the
/// messages steered into it, the reply, how it ended, the actions it offers
/// and its attachments. `chat_transcript_delta` says which turns two
/// transcripts differ in, so the view changes those turns and keeps every
/// other turn's widgets. The ending line's words are decided here, once, for
/// every chat. Pure.
namespace bpr::core {
	/// \brief How one turn ended, as its ending line says it: who, how long,
	/// what it cost.
	struct chat_ending
	{
		/// The agent's display name ("Claude Code", "Codex", a custom
		/// agent's); empty while none is known.
		std::string        Agent;
		agent_turn_outcome Outcome = agent_turn_outcome::answered;
		/// How long the run took, in whole seconds; none when it was not
		/// timed (a turn kept before times were kept).
		std::optional<std::int64_t> Seconds;
		/// What the run cost, when the agent reported it.
		std::optional<agent_cost> Cost;
		/// The findings the turn delivered (the Agent tab's review turns);
		/// zero says nothing.
		std::size_t Findings = 0U;

		auto operator==(const chat_ending&) const -> bool = default;
	};

	/// \brief The agent's name as an ending line says it: the product without
	/// its "Code" ("Claude Code" is "Claude"), any other name as it is, and
	/// "The agent" where none is known.
	[[nodiscard]] auto chat_agent_short_name(std::string_view aAgent) -> std::string;
	/// \brief `m:ss`, the time a run took; `h:mm:ss` from an hour.
	[[nodiscard]] auto chat_elapsed_words(std::int64_t aSeconds) -> std::string;
	/// \brief How the run ended, without its cost: "Claude answered in 0:42",
	/// "Stopped after 0:12", "Stopped when the app closed, after 16:46", or
	/// the same without the time when it was not timed.
	[[nodiscard]] auto chat_ending_first_words(const chat_ending& aEnding) -> std::string;
	/// \brief The one ending line of every chat: how it ended, then what it
	/// cost and the findings it delivered, joined by " · "; a line of one part
	/// ends in a full stop. "Claude answered in 0:42 · $0.14",
	/// "Codex answered in 1:03."
	[[nodiscard]] auto chat_ending_words(const chat_ending& aEnding) -> std::string;

	/// \brief An item's proposal waiting in its side chat, with Use and
	/// Discard (the item's chat's attachment).
	struct chat_follow_up_proposal
	{
		follow_up_proposal Proposal;
		/// Use and Discard take a press: nothing runs.
		bool Usable = true;

		auto operator==(const chat_follow_up_proposal&) const -> bool = default;
	};

	/// \brief Something a chat's own projector hangs on a turn, drawn by the
	/// widget the chat registers for its kind.
	struct chat_attachment
	{
		/// Its identity within its turn.
		std::string                           Key;
		std::variant<chat_follow_up_proposal> Payload;

		auto operator==(const chat_attachment&) const -> bool = default;
	};

	/// \brief One turn of a conversation.
	struct chat_turn
	{
		/// Stable for as long as the turn is the same turn: a reply that
		/// grows, a turn that settles and an attachment that changes keep it.
		std::string Id;
		/// The reviewer's words; empty for a turn that shows only what it
		/// holds (a proposal waiting after its run).
		std::string Question;
		/// What the reviewer steered into the turn while it ran.
		std::vector<agent_steered_message> Steered;
		/// The agent's reply, as Markdown; empty while none has come.
		std::string Reply;
		/// The turn's run is going.
		bool Running = false;
		/// How it ended; none while it runs and for a turn with no ending
		/// line.
		std::optional<chat_ending> Ending;
		/// Why it ended without an answer, in the run's words; empty for
		/// none.
		std::string Failure;
		/// What its runtime reported it doing.
		std::optional<agent_turn_activity> Activity;
		/// What the ended turn offers under its reply.
		agent_turn_actions           Actions;
		std::vector<chat_attachment> Attachments;

		auto operator==(const chat_turn&) const -> bool = default;
	};

	/// \brief A conversation: its turns, oldest first.
	struct chat_transcript
	{
		std::vector<chat_turn> Turns;

		/// \brief The turn named `aId`, or null.
		[[nodiscard]] auto find(std::string_view aId) const noexcept -> const chat_turn*;
		auto               operator==(const chat_transcript&) const -> bool = default;
	};

	/// \brief How one turn differs between two transcripts.
	enum class chat_change_kind : std::uint8_t {
		/// A turn the earlier transcript did not hold, at `Index`.
		appended,
		/// A turn the later transcript no longer holds; `Index` is where the
		/// earlier one had it.
		removed,
		/// Only the running turn's reply differs.
		reply_grew,
		/// The turn's run ended.
		settled,
		/// Anything else about the turn differs: a question, an attachment,
		/// its actions.
		changed,
	};
	[[nodiscard]] auto stable_name(chat_change_kind aKind) noexcept -> std::string_view;

	/// \brief One turn's change.
	struct chat_change
	{
		chat_change_kind Kind = chat_change_kind::changed;
		std::string      Turn;
		/// The turn's place: in the later transcript, or in the earlier one
		/// for a removal.
		std::size_t Index = 0U;

		auto operator==(const chat_change&) const -> bool = default;
	};

	/// \brief What changed between two transcripts: removals first (by the
	/// earlier place), then each later turn that is new or differs, in the
	/// later order. At most one change a turn, and none for a turn that is
	/// the same. A turn whose place among the kept turns moved is removed and
	/// appended again.
	using chat_delta = std::vector<chat_change>;
	[[nodiscard]] auto chat_transcript_delta(const chat_transcript& aBefore, const chat_transcript& aAfter) -> chat_delta;

	/// \brief What the side chat about one item says it asked for while a
	/// proposal's run is going: "Update suggested comment", "Update draft",
	/// "Draft reply".
	[[nodiscard]] auto follow_up_apply_words(follow_up_apply aApply) -> std::string;

	/// \brief What the item's chat knows besides its state.
	struct follow_up_thread_facts
	{
		/// The chosen agent's display name.
		std::string Agent;
		/// How long each answered round took, by round; none where it was not
		/// timed (a round answered before the chat was opened).
		std::vector<std::optional<std::int64_t>> Seconds;
	};

	/// \brief The item's conversation as a transcript: a turn each round, the
	/// question going as a running turn, and a proposal (its run going, then
	/// waiting for Use or Discard) as a turn of its own whose attachment it
	/// is. Identities carry the item's key, so another item's chat shares no
	/// turn with this one.
	[[nodiscard]] auto project_follow_up(const follow_up_state& aState, const follow_up_thread_facts& aFacts) -> chat_transcript;
}

#endif
