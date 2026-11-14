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

	class agent_queue_host final
	{
	public:
		explicit agent_queue_host(agent_queue_dependencies aDependencies);

		/// \brief Reads the stored queues, which open paused.
		void               load();
		[[nodiscard]] auto enqueue(const std::string& aSession, core::queued_input aInput) -> core::queue_refusal;
		[[nodiscard]] auto withdraw(const std::string& aSession, std::uint64_t aId) -> core::queue_refusal;
		[[nodiscard]] auto resume(const std::string& aSession) -> core::queue_refusal;
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
		/// \brief The entry the session is sending, if any.
		[[nodiscard]] auto sending(const std::string& aSession) const -> std::optional<std::uint64_t>;

	private:
		void commit(const std::string& aSession);

		agent_queue_dependencies                 Dependencies;
		std::map<std::string, core::agent_queue> Queues;
		std::map<std::string, std::uint64_t>     Sending;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_QUEUE_HOST_HPP
