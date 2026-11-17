#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/agent_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0126 ([[BPR-2151]], [[BPR-2181]]). The session queue host:
/// the profile's one owner of every agent session's queue. The owner,
/// 2026-11-14: "Queues should be managed by something like a session manager
/// at a higher level and have nothing to do with the UI, the UI is just a
/// presentation layer". It decides when a session's next message goes and
/// starts it through the session's turn runner; it releases the next only on
/// the previous run's own retirement event (never a timer). Windows send it
/// commands and subscribe to its semantic deltas; none decides when to send,
/// and none has to be open for a queue to drain.
namespace bpr::application {
	/// \brief Where a profile keeps its queues.
	class agent_queue_store_port
	{
	public:
		virtual ~agent_queue_store_port() = default;
		/// \brief Every session's queue; none when the store cannot be read.
		[[nodiscard]] virtual auto load() -> std::optional<std::map<std::string, core::agent_queue>> = 0;
		/// \brief Writes one session's queue whole.
		[[nodiscard]] virtual auto store(const std::string& aSession, const core::agent_queue& aQueue) -> bool = 0;
	};

	/// \brief RFC-BPR-0126 AC03 ([[BPR-2181]]). How a released message was
	/// taken. No window's words are read as the verdict: it is what the
	/// runtime saw.
	enum class queued_admission_kind : std::uint8_t {
		/// Its run started: that run's retirement settles it.
		started,
		/// Its run starts on a later pass (its material is read first): that
		/// run's retirement, or a typed refusal for the same run, settles it.
		awaiting,
		/// Answered with no run (a slash command): it went.
		answered,
		/// Nothing started: it goes back first and the queue pauses.
		refused,
	};
	[[nodiscard]] auto stable_name(queued_admission_kind aKind) noexcept -> std::string_view;

	/// \brief A release's admission, keyed by the run it was admitted as.
	struct queued_admission
	{
		queued_admission_kind Kind = queued_admission_kind::refused;
		/// The runtime's run identity; only that run's retirement or refusal
		/// settles the message. Zero when nothing was admitted.
		std::uint64_t Run = 0U;

		auto operator<=>(const queued_admission&) const = default;
	};

	/// \brief Runs a session's turns: the agent runtime behind the queue.
	/// The runner tells the host `agent_changed` when what the session's
	/// agent can take changes, and `retired` when the run of a turn the host
	/// started has retired.
	class agent_turn_runner_port
	{
	public:
		virtual ~agent_turn_runner_port() = default;
		/// \brief What the session's agent can take now. `busy` while a turn
		/// of the session runs or its run has not retired.
		[[nodiscard]] virtual auto state(const std::string& aSession) -> core::queue_agent = 0;
		/// \brief Starts one released message's turn, and says how it was
		/// admitted (AC03).
		[[nodiscard]] virtual auto start(const std::string& aSession, const core::queued_entry& aEntry) -> queued_admission = 0;
		/// \brief RFC-BPR-0126 AC05 ([[BPR-2153]]). Gives the message to the
		/// session's running turn. None when it was sent; otherwise why not,
		/// in the words the Agent tab shows.
		[[nodiscard]] virtual auto steer(const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string> = 0;
		/// \brief [[BPR-2152]] (AC06). What a correction pressed now would be
		/// bound to: the run of the session's running turn and its native
		/// conversation (BPR's agent session), zero when not known.
		[[nodiscard]] virtual auto bound(const std::string& aSession) -> core::queued_claim
		{
			static_cast<void>(aSession);
			return { };
		}
	};

	/// \brief What a window is shown: semantic deltas only.
	struct agent_queue_subscriber
	{
		/// A session's queue changed (every session; the window shows the
		/// ones it presents).
		std::function<void(const std::string& aSession, const core::agent_queue& aQueue)> Changed;
		/// Messages waiting to go across every session, as `waiting_count`
		/// (RFC-BPR-0126 AC09: Stop all shows while any waits).
		std::function<void(std::size_t aWaiting)> Waiting;
		/// [[BPR-2181]] (AC05). The store refused to keep a message just
		/// queued: it left the queue and is handed back as it was asked, so
		/// the window that queued it can return it to its composer. Optional.
		std::function<void(const std::string& aSession, const core::queued_input& aInput)> Unsaved;
	};

	struct agent_queue_dependencies
	{
		/// Optional: without one the queues live for this run only.
		agent_queue_store_port* Store = nullptr;
		/// Optional: without one nothing is released.
		agent_turn_runner_port* Runner = nullptr;
		/// [[BPR-2181]] (AC05). Optional: a serial lane the store's writes run
		/// on, off the decision lane, each with the queue as it was committed.
		/// Without one they run where the queue changed.
		effect_executor* Effects = nullptr;
		/// [[BPR-2181]] (AC05). Where a write the store refused on `Effects`
		/// comes back to: a message it was queuing leaves the queue and is
		/// handed back (`agent_queue_subscriber::Unsaved`). Optional.
		decision_executor* DecisionLane = nullptr;
	};

	/// \brief What Steer did with a queued message.
	struct agent_queue_steer_result
	{
		/// Not `none`: the press was refused (stale, unknown, not editable)
		/// and nothing changed.
		core::queue_refusal Refused = core::queue_refusal::none;
		/// It went into the running turn and left the queue.
		bool Sent = false;
		/// Not sent: why; the message now goes next.
		std::string Why;
	};

	/// \brief [[BPR-2152]] (AC06). Told once what a Steer did, after its
	/// claim was stored: on the decision lane, possibly after `steer`
	/// returned.
	using agent_queue_steer_done = std::function<void(const agent_queue_steer_result& aResult)>;

	class agent_queue_host final
	{
	public:
		explicit agent_queue_host(agent_queue_dependencies aDependencies);
		~agent_queue_host();
		agent_queue_host(const agent_queue_host&)                    = delete;
		auto operator=(const agent_queue_host&) -> agent_queue_host& = delete;

		/// \brief Reads the stored queues, which open paused.
		void load();

		// Commands. `aSeen` is the revision the window last showed: an edit
		// made from an older one is refused (another window changed the
		// queue first). None is the current revision.
		/// \brief Queues `aInput`. A store write refused while it runs here is
		/// `not_saved` and nothing was queued; one refused on the effect lane
		/// comes back as `agent_queue_subscriber::Unsaved` (AC05).
		[[nodiscard]] auto enqueue(const std::string& aSession, core::queued_input aInput) -> core::queue_refusal;
		[[nodiscard]] auto withdraw(const std::string& aSession, std::uint64_t aId, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		[[nodiscard]] auto reorder(const std::string& aSession, std::uint64_t aId, int aBy, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		/// \brief Q9 as refined: an edit holds its entry; Save (new text) or
		/// Cancel (none) lifts the hold and the queue continues.
		[[nodiscard]] auto hold(const std::string& aSession, std::uint64_t aId, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		[[nodiscard]] auto release_hold(const std::string& aSession, std::uint64_t aId, std::optional<std::string> aText) -> core::queue_refusal;
		void               pause(const std::string& aSession, core::queue_pause aWhy);
		[[nodiscard]] auto resume(const std::string& aSession, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		/// \brief RFC-BPR-0126 AC09, Stop all: every session's queue pauses.
		void pause_all(core::queue_pause aWhy);
		/// \brief Steer on a queued message. [[BPR-2152]] (AC06): the entry
		/// is claimed (Sending, bound to the running turn's run and
		/// conversation) and stored before it is offered to the turn. Handed
		/// to the turn, it stays Sending, then Sent on the runtime's
		/// acknowledgment, until the run's end settles it
		/// (`settle_steers`); not handed, it goes next. Returns only the
		/// press's refusal; `aDone` is told the outcome once.
		[[nodiscard]] auto steer(const std::string& aSession, std::uint64_t aId, std::optional<std::uint64_t> aSeen = std::nullopt, agent_queue_steer_done aDone = { }) -> core::queue_refusal;
		/// \brief [[BPR-2152]] (AC06). The runtime's answer for the
		/// correction claimed as `aId`: written (`aReceipt` names its input
		/// identity) makes it Sent, kept; proven unwritten or refused, it goes
		/// back first with its identity. A late or second answer changes
		/// nothing. Never resends.
		void steer_answered(const std::string& aSession, std::uint64_t aId, bool aWritten, std::string aReceipt);
		/// \brief [[BPR-2152]] (AC06). The run `aRun` ended: its corrections
		/// settle by receipt (core `settle_steers`), stored before anything
		/// else is released.
		void settle_steers(const std::string& aSession, std::uint64_t aRun, const std::vector<core::steered_settlement>& aEndings);
		/// \brief RFC-BPR-0126 AC04. Discard on a message whose delivery is not
		/// known: it leaves the queue (core `discard_uncertain`).
		[[nodiscard]] auto discard(const std::string& aSession, std::uint64_t aId, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;

		// Events from the runner.
		/// \brief What the session's agent can take may have changed: the
		/// host releases its next message if it can.
		void agent_changed(const std::string& aSession);
		/// \brief The run of the turn the host started for `aSession` retired
		/// and ended as `aFinish`: the core decides what it does to the
		/// message and whether what remains pauses (RFC-BPR-0126 AC02).
		void retired(const std::string& aSession, core::queued_finish aFinish);
		/// \brief The run of the turn the host started for `aSession` retired
		/// without the agent taking its message (a window close stopped it):
		/// the message goes back first, the queue is not paused (Q10), and
		/// the next turn may start.
		void returned(const std::string& aSession);

		// Semantic deltas.
		[[nodiscard]] auto subscribe(agent_queue_subscriber aSubscriber) -> std::uint64_t;
		void               unsubscribe(std::uint64_t aSubscriber);
		/// \brief Shows `aSession`'s queue to every subscriber again.
		void show(const std::string& aSession);

		[[nodiscard]] auto queue(const std::string& aSession) const -> const core::agent_queue*;
		/// \brief The entry the session is sending, if any.
		[[nodiscard]] auto sending(const std::string& aSession) const -> std::optional<std::uint64_t>;
		/// \brief Messages that go next on their own, across every session:
		/// those queued in a queue that is not paused. [[BPR-2245]]: a paused
		/// queue's message starts only on Resume, so Stop all, which shows
		/// while this is not zero or a turn runs, has nothing to stop for it.
		[[nodiscard]] auto waiting_count() const -> std::size_t;

	private:
		/// Writes and shows the session's queue. `aAdded`: the entry this
		/// change queued, which leaves again if the store refuses it. Whether
		/// a write that ran here was kept.
		auto commit(const std::string& aSession, std::optional<std::uint64_t> aAdded = std::nullopt) -> bool;
		/// [[BPR-2152]] (AC06). Writes and shows the session's queue, then
		/// tells `aThen` on the decision lane whether the write was kept;
		/// only then may a claimed entry be written to an agent.
		void commit_then(const std::string& aSession, std::function<void(bool aKept)> aThen);
		/// The release claimed as `aId` was stored (or not): start its turn.
		void start_claimed(const std::string& aSession, std::uint64_t aId, bool aKept);
		/// The correction claimed as `aId` was stored (or not): offer it.
		void offer_claimed(const std::string& aSession, std::uint64_t aId, bool aKept, const agent_queue_steer_done& aDone);
		void unsaved(const std::string& aSession, std::uint64_t aAdded);
		void notify_waiting();
		void drain(const std::string& aSession);

		agent_queue_dependencies                        Dependencies;
		std::map<std::string, core::agent_queue>        Queues;
		std::map<std::string, std::uint64_t>            Sending;
		std::map<std::uint64_t, agent_queue_subscriber> Subscribers;
		std::uint64_t                                   LastSubscriber = 0U;
		bool                                            Draining       = false;
		/// Lets a refused write's verdict know the host is gone.
		std::shared_ptr<bool> Alive = std::make_shared<bool>(true);
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP
