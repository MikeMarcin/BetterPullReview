#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUNTIME_STEER_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUNTIME_STEER_HPP

#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_queue_host.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-2153]], RFC-BPR-0126 AC05. The Steer of an agent runtime
/// (`agent_session_runtime::Steer`): a queued message offered to the running
/// turn goes to `agent_invocation_host::steer` for the run the window shows.
namespace bpr::application {
	/// \brief What an agent runtime's Steer works with.
	struct agent_runtime_steer_binding
	{
		/// The runtime's invocation host; none while it has none.
		std::function<agent_invocation_host*()> Runs;
		/// The profile's session queue host, which outlives every runtime.
		agent_queue_host* Queue = nullptr;
		/// The run the window shows as running, by the identity it was
		/// admitted with; none when it shows none.
		std::function<std::optional<agent_invocation_projection_identity>()> Shown;
		/// Says, for the window showing `aSession`, why a correction handed
		/// to the turn came back unsent after it left the queue.
		std::function<void(const std::string& aSession, const std::string& aWhy)> Returned;
	};

	/// \brief Makes the runtime's Steer.
	///
	/// \details Answers synchronously, as the turn runner asks: none when the
	/// correction was handed to the shown run's kept connection, otherwise
	/// why not, in the Agent tab's words (the queue host then moves the
	/// message next). Support is read first (`steer_support`), from the run
	/// identity and the version its runtime reported, never a probe. The
	/// connection's own answer comes later; if it says the correction was not
	/// written after all (the turn ended first, Codex refused it), the
	/// message is given back to the queue, first in line, through the queue
	/// host's own commands, never resent and never lost, and `Returned` says
	/// why. A quick ask is written when it is released, so it cannot steer.
	[[nodiscard]] auto agent_runtime_steer(agent_runtime_steer_binding aBinding)
		-> std::function<std::optional<std::string>(const std::string& aSession, const core::queued_entry& aEntry)>;

	/// \brief What the Agent tab says when a message was handed to the
	/// running turn.
	inline constexpr std::string_view agent_steered_words = "Steered into the running turn: the agent reads it after its current step.";

	/// \brief The words for a correction that was not steered; the message
	/// goes next.
	[[nodiscard]] auto agent_steer_goes_next_words(core::agent_steer_support aSupport) -> std::string;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_RUNTIME_STEER_HPP
