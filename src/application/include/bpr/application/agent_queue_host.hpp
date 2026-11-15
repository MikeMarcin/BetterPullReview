#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP

#include "bpr/core/agent_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

/// \file
/// \brief RFC-BPR-0126 ([[BPR-2151]], host) as RFC-BPR-0128's Ask needs it.
/// The queues belong to their sessions, not to a window (owner Q10): kept by
/// session key, stored with the profile, restored paused. The host releases
/// the next entry only when it is told the session's agent is idle, after the
/// turn before has really finished; nothing is retried on a timer.
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

	struct agent_queue_dependencies
	{
		/// Optional: without one the queues live for this run only.
		agent_queue_store_port* Store = nullptr;
		/// Starts one released entry's turn; returns whether it started.
		std::function<bool(const std::string& aSession, const core::queued_entry& aEntry)> Release;
		/// A session's queue changed (for its windows to show).
		std::function<void(const std::string& aSession, const core::agent_queue& aQueue)> Changed;
	};

	/// \brief RFC-BPR-0126 AC04, Q10 as overridden ("Queue must be tied to
	/// session not active UI windows"). One window of the profile: the
	/// sessions it shows, what its agent can take for them, and how it runs a
	/// released entry. Every window showing a session shows and edits the one
	/// queue; the host drains it through whichever of them is free.
	struct agent_queue_viewer
	{
		/// Whether it shows `aSession` now (and can run its turns).
		std::function<bool(const std::string& aSession)> Shows;
		/// What its agent can take for `aSession` now.
		std::function<core::queue_agent(const std::string& aSession)> Agent;
		/// Starts one released entry's turn; returns whether it started.
		std::function<bool(const std::string& aSession, const core::queued_entry& aEntry)> Release;
		/// A session it shows changed.
		std::function<void(const std::string& aSession, const core::agent_queue& aQueue)> Changed;
		/// Messages waiting across every session (RFC-BPR-0126 AC09: Stop all
		/// shows while any waits).
		std::function<void(std::size_t aWaiting)> Waiting;
	};

	class agent_queue_host final
	{
	public:
		explicit agent_queue_host(agent_queue_dependencies aDependencies);

		/// \brief Reads the stored queues, which open paused.
		void               load();
		[[nodiscard]] auto enqueue(const std::string& aSession, core::queued_input aInput) -> core::queue_refusal;
		/// \brief The edits a window makes from what it shows: `aSeen`, the
		/// revision it last showed, refuses an edit made from an older one
		/// (another window changed the queue first). None is the current one.
		[[nodiscard]] auto withdraw(const std::string& aSession, std::uint64_t aId, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		[[nodiscard]] auto resume(const std::string& aSession, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		[[nodiscard]] auto reorder(const std::string& aSession, std::uint64_t aId, int aBy, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		/// \brief Q9 as refined: an edit holds its entry; Save (new text) or
		/// Cancel (none) lifts the hold and the queue continues.
		[[nodiscard]] auto hold(const std::string& aSession, std::uint64_t aId, std::optional<std::uint64_t> aSeen = std::nullopt) -> core::queue_refusal;
		[[nodiscard]] auto release_hold(const std::string& aSession, std::uint64_t aId, std::optional<std::string> aText) -> core::queue_refusal;
		void               pause(const std::string& aSession, core::queue_pause aWhy);
		/// \brief RFC-BPR-0126 AC09, Stop all: every session's queue pauses.
		void pause_all(core::queue_pause aWhy);
		/// \brief Messages waiting to be sent, across every session.
		[[nodiscard]] auto queued_count() const -> std::size_t;
		/// \brief What the session's agent can take now: releases the next
		/// entry when it is idle.
		void observe(const std::string& aSession, core::queue_agent aAgent);
		/// \brief The turn the host released for `aSession` finished.
		void               settle(const std::string& aSession, core::queue_pause aOutcome);
		[[nodiscard]] auto queue(const std::string& aSession) const -> const core::agent_queue*;
		/// \brief A window joins: it is shown every session it shows as it
		/// changes, and may run their turns. Returns its token.
		[[nodiscard]] auto attach(agent_queue_viewer aViewer) -> std::uint64_t;
		/// \brief A window closed. Its sessions' queues are not paused or
		/// handed over (Q10): another window showing the session drains it.
		/// A turn it was running ended with it; its message went, so that
		/// entry settles as sent.
		void detach(std::uint64_t aViewer);
		/// \brief Shows `aSession`'s queue to every window showing it.
		void show(const std::string& aSession);
		/// \brief Releases `aSession`'s next entry through a free window when
		/// no window showing it is busy (one conversation, one turn at a time).
		void drain(const std::string& aSession);
		/// \brief Whether any open window shows `aSession`.
		[[nodiscard]] auto shown(const std::string& aSession) const -> bool;
		/// \brief The window running `aSession`'s released entry, if any.
		[[nodiscard]] auto dispatcher(const std::string& aSession) const -> std::optional<std::uint64_t>;
		/// \brief The entry the session is sending, if any.
		[[nodiscard]] auto sending(const std::string& aSession) const -> std::optional<std::uint64_t>;

	private:
		void commit(const std::string& aSession);
		void start(const std::string& aSession, core::queue_agent aAgent, const std::function<bool(const core::queued_entry&)>& aRelease, std::optional<std::uint64_t> aViewer);

		agent_queue_dependencies                    Dependencies;
		std::map<std::string, core::agent_queue>    Queues;
		std::map<std::string, std::uint64_t>        Sending;
		std::map<std::uint64_t, agent_queue_viewer> Viewers;
		std::map<std::string, std::uint64_t>        Dispatchers;
		std::uint64_t                               LastViewer = 0U;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP
