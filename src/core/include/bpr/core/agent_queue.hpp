#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUEUE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUEUE_HPP

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
		std::string Thread;
		/// What the entry says in the queue (the quick ask's label, or the
		/// message).
		std::string Label;
		/// A typed message's text.
		std::string Text;

		auto operator<=>(const queued_input&) const = default;
	};

	struct queued_entry
	{
		std::uint64_t Id = 0U;
		queued_input  Input;
		queued_state  State = queued_state::queued;
		/// An edit holds it: never sent and never skipped (Q9).
		bool Held = false;

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
	};
	[[nodiscard]] auto stable_name(queued_finish aFinish) noexcept -> std::string_view;
	/// \brief The pause a finish puts on what remains: none when the message
	/// went or was not taken; otherwise its own reason.
	[[nodiscard]] auto pause_for(queued_finish aFinish) noexcept -> queue_pause;
	/// \brief The run started for the sending entry `aId` finished: a message
	/// not taken goes back first, unpaused (`return_unsent`); any other leaves
	/// the queue, and a failure, usage limit or Stop pauses what remains. A
	/// pause already in force (the reviewer's, a lost access) keeps its reason.
	/// Nothing changes for an entry that is not sending.
	[[nodiscard]] auto finish(agent_queue& aQueue, std::uint64_t aId, queued_finish aFinish) -> queue_refusal;
	/// \brief RFC-BPR-0126 AC04. Discard on a message whose delivery is not
	/// known (a crash while it was sending): it leaves the queue, and the
	/// queue stays paused until the reviewer resumes it. Refused from an older
	/// revision, for an unknown entry, or for one that is not uncertain.
	[[nodiscard]] auto discard_uncertain(agent_queue& aQueue, std::uint64_t aId, std::uint64_t aFrom) -> queue_refusal;
	/// \brief A restored queue opens paused; a sending entry becomes
	/// uncertain.
	void restore(agent_queue& aQueue);
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_QUEUE_HPP
