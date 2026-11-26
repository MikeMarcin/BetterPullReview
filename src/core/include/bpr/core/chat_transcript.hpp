#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_TRANSCRIPT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CHAT_TRANSCRIPT_HPP

#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_cost.hpp"
#include "bpr/core/agent_follow_up.hpp"
#include "bpr/core/agent_steering.hpp"
#include "bpr/core/agent_turn.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
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

	/// \brief A local draft's check plan or report, under the reply that
	/// carries it (RFC-BPR-0120).
	struct chat_tour_check
	{
		tour_agent_run_id Run;
		local_check       Check;
		/// Why the plan's Run is unavailable; empty when it is available.
		std::string   Blocked;
		std::uint64_t Latest = 0U;
		/// The draft's grants, as the card offers them.
		bool Scratch = false;
		bool Builds  = false;

		auto operator==(const chat_tour_check&) const -> bool = default;
	};

	/// \brief The focus a whole tour's turn set, and whether it can be undone.
	struct chat_tour_focus
	{
		tour_agent_run_id Run;
		std::string       Focus;
		bool              Undone   = false;
		bool              Undoable = false;

		auto operator==(const chat_tour_focus&) const -> bool = default;
	};

	/// \brief A whole tour's revision waiting in the tour: which of the
	/// proposal's parts this turn proposed, and how each is decided.
	struct chat_tour_revision
	{
		tour_agent_run_id                   Run;
		std::vector<std::size_t>            Items;
		std::vector<tour_proposal_decision> Decisions;

		auto operator==(const chat_tour_revision&) const -> bool = default;
	};

	/// \brief The parts of a whole tour's revision the core refused, and why.
	struct chat_tour_refused
	{
		std::vector<tour_refused_part> Parts;

		auto operator==(const chat_tour_refused&) const -> bool = default;
	};

	/// \brief Under the newest answer that changed nothing: the way to ask for
	/// the change itself.
	struct chat_tour_unchanged
	{
		tour_agent_run_id Run;

		auto operator==(const chat_tour_unchanged&) const -> bool = default;
	};

	/// \brief One part a tour part's turn proposed, decided in its chat.
	struct chat_tour_proposal
	{
		/// Its place in the proposal, which Accept and Discard name.
		std::size_t        Index = 0U;
		tour_proposal_item Item;

		auto operator==(const chat_tour_proposal&) const -> bool = default;
	};

	/// \brief Where an attachment stands in its turn.
	enum class chat_attachment_place : std::uint8_t {
		/// After the ending line and what the run did.
		after_ending,
		/// Under the reply, before the ending line: what the reply is.
		after_reply,
	};

	/// \brief Something a chat's own projector hangs on a turn, drawn by the
	/// widget the chat registers for its kind.
	struct chat_attachment
	{
		/// Its identity within its turn.
		std::string                                                                                                                                             Key;
		std::variant<chat_follow_up_proposal, chat_tour_check, chat_tour_focus, chat_tour_revision, chat_tour_refused, chat_tour_unchanged, chat_tour_proposal> Payload;
		chat_attachment_place                                                                                                                                   Place = chat_attachment_place::after_ending;

		auto operator==(const chat_attachment&) const -> bool = default;
	};

	/// \brief What a turn's run did, folded once it ended.
	struct chat_activity
	{
		agent_turn_activity Activity;
		std::int64_t        Seconds = 0;

		auto operator==(const chat_activity&) const -> bool = default;
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
		/// A quiet line under the question in its bubble: when it goes (a
		/// whole tour's feedback waiting for its draft). Empty for none.
		std::string QuestionNote;
		/// What the reviewer steered into the turn while it ran.
		std::vector<agent_steered_message> Steered;
		/// The agent's reply, as Markdown; empty while none has come.
		std::string Reply;
		/// The turn's run is going.
		bool Running = false;
		/// How it ended; none while it runs and for a turn with no ending
		/// line.
		std::optional<chat_ending> Ending;
		/// Why it ended without an answer, as the turn says it; empty for
		/// none.
		std::string Failure;
		/// What Copy takes from a turn without an answer: the run's own words
		/// in full. Empty: `Failure`.
		std::string FailureDetail;
		/// What its runtime reported it doing, once the run ended (a running
		/// turn's activity is live, the chat's to present).
		std::optional<chat_activity> Activity;
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

	/// \brief What the tour's side chat knows besides the session.
	struct tour_thread_facts
	{
		/// The proposal waiting, whose parts a turn may hang.
		std::optional<tour_proposal> Proposal;
		/// A run of the tour session is going: no ended turn offers a
		/// recovery while it does.
		bool Running = false;
		/// The agent that answered each run, by run, where it is known.
		std::map<std::uint64_t, std::string> Agents;
		/// What each ended run did, by run, as its runtime reported it.
		std::map<std::uint64_t, chat_activity> Activity;
		/// Delete is offered on a turn that ended without an answer.
		bool Deletes = false;
		/// The local draft's grants, for its check cards.
		bool CheckScratch = false;
		bool CheckBuilds  = false;
	};

	/// \brief The name a tour run is said by when no agent is known.
	inline constexpr std::string_view tour_chat_default_agent = "The tour agent";

	/// \brief One part's (or the whole tour's) conversation as a transcript:
	/// a turn each of its runs, identity the run's; a reply, or why it did not
	/// finish, and the one ending line; the ended turn's actions
	/// (`decide_agent_turn_actions`); and as attachments a local draft's check
	/// card, the focus a turn set, the revision it proposed, the parts refused,
	/// the way to ask for the change under the newest answer that changed
	/// nothing, and each part a part's turn proposed. A whole tour's feedback
	/// waiting for its draft is the last turn, with when it goes.
	[[nodiscard]] auto project_tour_thread(const tour_agent_session& aSession, const tour_thread& aThread, const tour_thread_facts& aFacts) -> chat_transcript;
}

#endif
