#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUEUE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUEUE_HPP

#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0126 ([[BPR-2151]], core) as RFC-BPR-0128's Ask needs it
/// (owner, 2026-11-14: "a plain click queues it. We might not have set up the
/// agent yet or we might be running other things already"). One session's
/// pending inputs: stable identities, the displayed order, a revision that
/// refuses stale edits, the hold an edit puts on its entry (Q9), the pause and
/// its reason, and the limits (16 inputs, 256 KiB). A pure decision over what
/// the host saw; the host stores it, releases the next entry only after the
/// run before has really finished, and never retries on a timer.
namespace bpr::core {
	inline constexpr std::size_t maximum_queued_inputs = 16U;
	inline constexpr std::size_t maximum_queue_bytes   = 256U * 1024U;
	/// [[BPR-2181]]. The sessions whose queues hold messages at once, per
	/// profile. Each has an agent runtime while its queue can drain, so this
	/// also bounds the profile's runtimes.
	inline constexpr std::size_t maximum_queued_sessions = 32U;
	/// The existing question limit.
	inline constexpr std::size_t maximum_queued_text_bytes = 16U * 1024U;

	enum class queued_state : std::uint8_t {
		/// Waiting its turn.
		queued,
		/// Released to the agent; its turn runs.
		sending,
		/// The agent took it.
		sent,
		/// Released, but whether the agent took it is not known.
		delivery_unknown,
	};
	[[nodiscard]] auto stable_name(queued_state aState) noexcept -> std::string_view;

	/// \brief What one entry asks: a quick ask (by its macro, scope and
	/// subject) or a message typed for the session.
	struct queued_input
	{
		/// Empty for a typed message.
		std::string QuickAsk;
		/// `thread`, `file` or `pull-request` for a quick ask.
		std::string Scope;
		/// The subject captured at the press; never retargeted.
		std::string File;
		/// [[BPR-2181]] (AC06). The review threads it carries, in the order
		/// the press named them: one for a thread's ask or a message asked in
		/// a thread's conversation, several for a wider scope's Was this
		/// addressed?.
		std::vector<std::string> Threads;
		/// What the entry says in the queue (the quick ask's label, or the
		/// message).
		std::string Label;
		/// A typed message's text.
		std::string Text;
		/// [[BPR-2181]] (AC13). A suggested prompt's offer as decided at the
		/// press, its expansion included: the release sends it as it is and
		/// decides nothing again. None for a typed message or an action.
		std::optional<prompt_offer> Prompt;

		auto operator<=>(const queued_input&) const = default;
	};

	/// \brief [[BPR-2181]] (AC13). A queued input's decided prompt as the
	/// store keeps it in one text column, and read back; a value that is not
	/// one this wrote reads as none.
	[[nodiscard]] auto encode_queued_prompt(const prompt_offer& aOffer) -> std::string;
	[[nodiscard]] auto decode_queued_prompt(std::string_view aText) -> std::optional<prompt_offer>;

	/// \brief [[BPR-2181]] (AC06). Whose queue it is: the session a queue
	/// belongs to, typed. Its `session_key` is the stable name the host, the
	/// store and every window key the queue by.
	enum class queued_session_kind : std::uint8_t {
		/// The pull request's Agent tab.
		agent_tab,
		/// Feedback waiting for the pull request's tour draft.
		tour_feedback,
		/// [[BPR-2154]]. One part's conversation of the pull request's tour
		/// (the whole tour after its first draft, or a stop): tour parts
		/// never share inputs.
		tour_part,
		/// [[BPR-2154]] (Q2). One part's conversation of a local draft's
		/// discussion, keyed by the draft's identity, not a pull request.
		local_draft_discussion,
	};

	struct queued_session
	{
		queued_session_kind Kind = queued_session_kind::agent_tab;
		/// The pull request: its host name, owner, repository and number.
		std::string   Host;
		std::string   Owner;
		std::string   Repository;
		std::uint64_t Number = 0U;
		/// [[BPR-2154]]. A tour part's or local draft discussion's part
		/// (`tour_part_name`); empty for the other kinds.
		std::string Part;
		/// [[BPR-2154]]. A local draft discussion's draft identity; empty for
		/// the other kinds.
		std::string Draft;

		auto operator<=>(const queued_session&) const = default;
	};
	/// \brief The stable key, `agent-tab:host/owner/name#number` (or
	/// `tour-feedback:…`; `tour-part:host/owner/name#number/part`;
	/// `local-draft:draft/part`). Empty for a session that names no pull
	/// request (or no draft) or, for the part kinds, no part.
	[[nodiscard]] auto session_key(const queued_session& aSession) -> std::string;
	/// \brief The session a stable key names; none for one this does not make.
	[[nodiscard]] auto parse_session_key(std::string_view aKey) -> std::optional<queued_session>;
	/// \brief The same pull request's session of another kind.
	[[nodiscard]] auto with_kind(queued_session aSession, queued_session_kind aKind) -> queued_session;
	/// \brief [[BPR-2154]]. A tour thread's part as a session key names it:
	/// `tour` or `stop-ID`.
	[[nodiscard]] auto tour_part_name(const tour_thread& aThread) -> std::string;
	/// \brief The tour thread a part name names; none for one
	/// `tour_part_name` does not make.
	[[nodiscard]] auto tour_part_thread(std::string_view aPart) -> std::optional<tour_thread>;
	/// \brief [[BPR-2154]]. The pull request's session for one tour part.
	[[nodiscard]] auto tour_part_session(queued_session aPullRequest, const tour_thread& aThread) -> queued_session;
	/// \brief [[BPR-2154]] (Q2). A local draft's discussion session for one part.
	[[nodiscard]] auto local_draft_session(std::string aDraft, const tour_thread& aThread) -> queued_session;

	/// \brief [[BPR-2152]] (AC06). How a claimed entry goes to the agent.
	enum class queued_route : std::uint8_t {
		/// Released as the session's next turn.
		release,
		/// Steered into the running turn.
		steer,
	};
	[[nodiscard]] auto stable_name(queued_route aRoute) noexcept -> std::string_view;

	/// \brief [[BPR-2152]] (AC06). What a claimed entry is bound to, kept
	/// with it before any byte of it is written: the route, the BPR run it
	/// goes to (for Steer, the run whose turn it corrects; for a release,
	/// the run it was admitted as, once known), the native conversation
	/// (BPR's agent session) when one is known, and, once the runtime
	/// acknowledged a correction, the input identity it was written under.
	/// It keeps an uncertain entry identifiable after a crash.
	/// \brief The longest input identity a claim keeps.
	inline constexpr std::size_t maximum_claim_receipt_bytes = 128U;

	struct queued_claim
	{
		queued_route  Route        = queued_route::release;
		std::uint64_t Run          = 0U;
		std::uint64_t Conversation = 0U;
		/// The written input's identity (a correction's UUID); empty until
		/// the runtime acknowledged it.
		std::string Receipt;

		auto operator<=>(const queued_claim&) const = default;
	};
	/// \brief A claim as the store keeps it in one text column, and read
	/// back; a value this did not write reads as none.
	[[nodiscard]] auto encode_queued_claim(const queued_claim& aClaim) -> std::string;
	[[nodiscard]] auto decode_queued_claim(std::string_view aText) -> std::optional<queued_claim>;

	struct queued_entry
	{
		std::uint64_t Id = 0U;
		queued_input  Input;
		queued_state  State = queued_state::queued;
		/// An edit holds it: never sent and never skipped (Q9).
		bool Held = false;
		/// [[BPR-2152]]. Set when it was claimed (Sending) and kept while it
		/// is sent or uncertain.
		std::optional<queued_claim> Claim;

		auto operator<=>(const queued_entry&) const = default;
	};

	enum class queue_pause : std::uint8_t {
		none,
		/// The reviewer paused it (Pause queue).
		reviewer,
		stopped,
		failed,
		usage_limit,
		/// Restored after a restart.
		restarted,
		delivery_unknown,
		/// What an entry is about is no longer there.
		changed_target,
		/// [[BPR-2181]]. The review's authority ended: the account or its
		/// identity changed, or the pull request became unreadable.
		lost_access,
	};
	[[nodiscard]] auto stable_name(queue_pause aPause) noexcept -> std::string_view;

	/// \brief One session's queue.
	struct agent_queue
	{
		std::vector<queued_entry> Entries;
		std::uint64_t             Revision = 0U;
		std::uint64_t             LastId   = 0U;
		queue_pause               Paused   = queue_pause::none;

		auto operator<=>(const agent_queue&) const = default;
	};

	enum class queue_refusal : std::uint8_t {
		none,
		empty,
		too_long,
		full,
		too_large,
		/// Made from an older revision: nothing changed.
		stale,
		unknown_entry,
		/// Sending, sent or uncertain: no longer the reviewer's to change.
		not_editable,
		/// [[BPR-2181]] (AC05). The profile's store refused to keep it:
		/// nothing was queued.
		not_saved,
	};
	[[nodiscard]] auto stable_name(queue_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The payload bytes `aQueue`'s unsettled entries hold.
	[[nodiscard]] auto queue_bytes(const agent_queue& aQueue) noexcept -> std::size_t;

	/// \brief Adds `aInput` at the end. Enqueueing while paused does not
	/// resume. On a refusal nothing changes.
	[[nodiscard]] auto enqueue(agent_queue& aQueue, queued_input aInput, std::uint64_t aFrom) -> queue_refusal;
	/// \brief Removes a known-unsent entry.
	[[nodiscard]] auto withdraw(agent_queue& aQueue, std::uint64_t aId, std::uint64_t aFrom) -> queue_refusal;
	/// \brief Moves a known-unsent entry by `aBy` places among the unsent.
	[[nodiscard]] auto reorder(agent_queue& aQueue, std::uint64_t aId, int aBy, std::uint64_t aFrom) -> queue_refusal;
	/// \brief Q9 as refined: an edit holds its entry; nothing is sent past it,
	/// and the queue is not paused.
	[[nodiscard]] auto hold(agent_queue& aQueue, std::uint64_t aId, std::uint64_t aFrom) -> queue_refusal;
	/// \brief Save (with the new text) or Cancel (none) lifts the hold; the
	/// queue continues by itself unless paused for another reason.
	[[nodiscard]] auto release_hold(agent_queue& aQueue, std::uint64_t aId, std::optional<std::string> aText, std::uint64_t aFrom) -> queue_refusal;
	void               pause(agent_queue& aQueue, queue_pause aWhy);
	/// \brief Resume queue: never through an edit hold or unresolved delivery.
	[[nodiscard]] auto resume(agent_queue& aQueue, std::uint64_t aFrom) -> queue_refusal;

	/// \brief Whether the session's agent can take the next input.
	enum class queue_agent : std::uint8_t {
		/// No agent set up yet: entries wait; the Agent tab offers setup.
		not_set_up,
		/// A turn runs (or its run has not retired yet).
		busy,
		idle,
	};

	/// \brief The entry to release now, if any: the first known-unsent entry
	/// in order, only while the queue is not paused, nothing is sending or
	/// uncertain, the agent is idle, and that entry is not held.
	[[nodiscard]] auto next_release(const agent_queue& aQueue, queue_agent aAgent) -> std::optional<std::uint64_t>;
	/// \brief The host claimed `aId` and is about to start its turn.
	[[nodiscard]] auto mark_sending(agent_queue& aQueue, std::uint64_t aId) -> queue_refusal;
	/// \brief [[BPR-2152]] (AC06). Claims the exact entry `aId`, as of
	/// revision `aFrom`, for `aClaim`'s route: it becomes Sending, bound to
	/// the claim, and is no longer eligible for release, edit or removal. A
	/// release claims only an entry that is not held; a Steer claims a
	/// waiting entry whether held or not (the press takes it from its
	/// editor). The host writes nothing to the agent until this is stored.
	[[nodiscard]] auto claim(agent_queue& aQueue, std::uint64_t aId, queued_claim aClaim, std::uint64_t aFrom) -> queue_refusal;
	/// \brief [[BPR-2152]] (AC06). The run `aRun` was admitted for the
	/// released entry `aId`: its claim names that run.
	[[nodiscard]] auto bind_run(agent_queue& aQueue, std::uint64_t aId, std::uint64_t aRun) -> queue_refusal;
	/// \brief [[BPR-2152]] (AC06). The runtime acknowledged the correction
	/// `aId` as written, under the input identity `aReceipt`: it is Sent.
	[[nodiscard]] auto acknowledge(agent_queue& aQueue, std::uint64_t aId, std::string aReceipt) -> queue_refusal;

	/// \brief [[BPR-2152]] (AC06). How one correction of a run ended, by the
	/// input identity it was written under.
	enum class steered_ending : std::uint8_t {
		/// The runtime named it as taken.
		consumed,
		/// Proven not taken (never written, or refused without consuming it).
		not_taken,
		/// Written, and never reconciled.
		unknown,
	};
	struct steered_settlement
	{
		std::string    Receipt;
		steered_ending Ending = steered_ending::unknown;

		auto operator<=>(const steered_settlement&) const = default;
	};
	/// \brief [[BPR-2152]] (AC06). The run `aRun` ended: each correction
	/// claimed for it settles by its receipt. Consumed, it leaves the queue;
	/// not taken, it goes back first among the unsent with its identity; any
	/// other written correction (unknown, or one the run's result never
	/// named) stays as Delivery unknown and pauses the queue. A correction
	/// still awaiting its acknowledgment is left for it. Nothing is resent.
	void settle_steers(agent_queue& aQueue, std::uint64_t aRun, const std::vector<steered_settlement>& aEndings);
	/// \brief [[BPR-2181]]. The turn released for `aId` never took it (it did
	/// not start, or a window close stopped it before the agent answered):
	/// it is queued again, first among the unsent, and the queue is not
	/// paused for it (Q10).
	[[nodiscard]] auto return_unsent(agent_queue& aQueue, std::uint64_t aId) -> queue_refusal;
	/// \brief The turn `aId` started finished: sent and settled, it leaves
	/// the queue. A failure, Stop or usage limit pauses what remains.
	void settle(agent_queue& aQueue, std::uint64_t aId, queue_pause aOutcome);

	/// \brief RFC-BPR-0126 AC02 ([[BPR-2181]] slice 3). How the run a
	/// released message started ended, as the queue reads it.
	enum class queued_finish : std::uint8_t {
		/// The agent took the message and the run ended (answered or set aside).
		went,
		/// The run failed: it did not start, exited with an error, wrote
		/// what cannot be read, ran out of time, or was refused.
		failed,
		/// The agent's usage limit stopped it.
		usage_limit,
		/// The reviewer stopped it (Stop or Stop all).
		stopped,
		/// The run never took the message.
		not_taken,
		/// [[BPR-2181]] (AC03). The release was refused before any run took
		/// it (its material could not be read, the agent refused the send):
		/// it goes back first and the queue pauses as failed.
		refused,
		/// [[BPR-2152]] (AC06). The run may have been given the message but
		/// ended with no sign the agent took it (it failed before any
		/// output): Delivery unknown, kept identifiable, and the queue pauses.
		unknown,
	};
	[[nodiscard]] auto stable_name(queued_finish aFinish) noexcept -> std::string_view;
	/// \brief The pause a finish puts on what remains: none when the message
	/// went or was not taken; otherwise its own reason.
	[[nodiscard]] auto pause_for(queued_finish aFinish) noexcept -> queue_pause;
	/// \brief The run started for the sending entry `aId` finished: a message
	/// not taken goes back first, unpaused (`return_unsent`); a refused one
	/// goes back first and pauses the queue as failed; any other leaves
	/// the queue, and a failure, usage limit or Stop pauses what remains. A
	/// pause already in force (the reviewer's, a lost access) keeps its reason.
	/// Nothing changes for an entry that is not sending.
	[[nodiscard]] auto finish(agent_queue& aQueue, std::uint64_t aId, queued_finish aFinish) -> queue_refusal;
	/// \brief RFC-BPR-0126 AC04. Discard on a message whose delivery is not
	/// known (a crash while it was sending): it leaves the queue, and the
	/// queue stays paused until the reviewer resumes it. Refused from an older
	/// revision, for an unknown entry, or for one that is not uncertain.
	[[nodiscard]] auto discard_uncertain(agent_queue& aQueue, std::uint64_t aId, std::uint64_t aFrom) -> queue_refusal;
	/// \brief A restored queue opens paused; a sending or sent entry becomes
	/// uncertain, with its claim ([[BPR-2152]]): nothing is resent.
	void restore(agent_queue& aQueue);

	/// \brief RFC-BPR-0126 AC10 ([[BPR-2181]]). What Return does with a
	/// message typed in the Agent tab.
	enum class follow_up : std::uint8_t {
		/// Sent as the session's next turn.
		send,
		/// Queued behind what the session holds.
		queue,
		/// Offered to the running turn (it goes next if it cannot be).
		steer,
	};
	[[nodiscard]] auto stable_name(follow_up aFollowUp) noexcept -> std::string_view;

	struct follow_up_facts
	{
		/// The tab shows a turn of the session running.
		bool TurnShown = false;
		/// The session's queue holds a message: waiting, held, released and
		/// not yet settled, or uncertain. A message released in the gap
		/// before its turn shows is here, so a Return then is queued, never
		/// sent as (or credited to) that message's turn.
		bool QueueHolds = false;
		/// Nothing was typed.
		bool Empty = false;
		/// The reviewer's setting: Return steers rather than queues.
		bool SteerByDefault = false;
		/// Cmd+Return (Ctrl+Return off macOS): the other one, once.
		bool Opposite = false;
	};
	/// \brief Send while the session is free; otherwise a follow-up, queued or
	/// steered by the setting and the modifier. Nothing typed is sent, and
	/// the send decides its refusal.
	[[nodiscard]] auto decide_follow_up(const follow_up_facts& aFacts) noexcept -> follow_up;
	/// \brief The session's queue holds a message (`follow_up_facts::QueueHolds`).
	[[nodiscard]] auto holds_messages(const agent_queue& aQueue) noexcept -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUEUE_HPP
