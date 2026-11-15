#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/agent_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

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
		/// \brief Starts one released message's turn; whether it started.
		[[nodiscard]] virtual auto start(const std::string& aSession, const core::queued_entry& aEntry) -> bool = 0;
		/// \brief RFC-BPR-0126 AC05 ([[BPR-2153]]). Gives the message to the
		/// session's running turn. None when it was sent; otherwise why not,
		/// in the words the Agent tab shows.
		[[nodiscard]] virtual auto steer(const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string> = 0;
	};

	/// \brief What a window is shown: semantic deltas only.
	struct agent_queue_subscriber
	{
		/// A session's queue changed (every session; the window shows the
		/// ones it presents).
		std::function<void(const std::string& aSession, const core::agent_queue& aQueue)> Changed;
		/// Messages waiting across every session (RFC-BPR-0126 AC09: Stop all
		/// shows while any waits).
		std::function<void(std::size_t aWaiting)> Waiting;
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

	class agent_queue_host final
	{
	public:
		explicit agent_queue_host(agent_queue_dependencies aDependencies);

		/// \brief Reads the stored queues, which open paused.
		void load();

		// Commands. `aSeen` is the revision the window last showed: an edit
		// made from an older one is refused (another window changed the
		// queue first). None is the current revision.
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
		/// \brief Steer on a queued message: held while it is offered to the
		/// running turn; sent, it leaves the queue; otherwise it goes next and
		/// the result says why.
		[[nodiscard]] auto steer(const std::string& aSession, std::uint64_t aId, std::optional<std::uint64_t> aSeen = std::nullopt) -> agent_queue_steer_result;

		// Events from the runner.
		/// \brief What the session's agent can take may have changed: the
		/// host releases its next message if it can.
		void agent_changed(const std::string& aSession);
		/// \brief The run of the turn the host started for `aSession` retired
		/// with `aOutcome` (none: it went; otherwise the queue pauses for it).
		void retired(const std::string& aSession, core::queue_pause aOutcome);
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
		/// \brief Messages waiting to be sent, across every session.
		[[nodiscard]] auto queued_count() const -> std::size_t;

	private:
		void commit(const std::string& aSession);
		void drain(const std::string& aSession);

		agent_queue_dependencies                        Dependencies;
		std::map<std::string, core::agent_queue>        Queues;
		std::map<std::string, std::uint64_t>            Sending;
		std::map<std::uint64_t, agent_queue_subscriber> Subscribers;
		std::uint64_t                                   LastSubscriber = 0U;
		bool                                            Draining       = false;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP
