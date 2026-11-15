#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_RUNNER_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_RUNNER_HPP

#include "bpr/application/agent_queue_host.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0126 ([[BPR-2181]]). The profile's turn runner: the agent
/// runtimes the session queue host starts turns on. The host alone decides
/// when a session's next message goes; this says what the session's agent
/// can take and starts a turn on a runtime that runs that session. Each
/// window composes an agent runtime today and offers it here.
namespace bpr::application {
	/// \brief One agent runtime: the sessions it runs and how.
	struct agent_session_runtime
	{
		std::function<bool(const std::string& aSession)>              Runs;
		std::function<core::queue_agent(const std::string& aSession)> State;
		/// How the release was admitted, keyed by the run it was admitted as.
		std::function<queued_admission(const std::string& aSession, const core::queued_entry& aEntry)> Start;
		std::function<std::optional<std::string>(const std::string&, const core::queued_entry&)>       Steer;
	};

	class agent_session_runner final : public agent_turn_runner_port
	{
	public:
		void               bind(agent_queue_host& aHost);
		[[nodiscard]] auto offer(agent_session_runtime aRuntime) -> std::uint64_t;
		/// \brief The runtime leaves (its window is closing). A turn it runs
		/// stays this session's until its run retires (`retired`): nothing
		/// else starts meanwhile.
		void withdraw(std::uint64_t aRuntime);
		/// \brief The run `aRun` of the turn `aRuntime` started for `aSession`
		/// retired and ended as `aFinish`. Only the run the release was
		/// admitted as settles it (AC03): another run's retirement, a turn the
		/// window sent directly, is not this message's.
		void retired(std::uint64_t aRuntime, const std::string& aSession, std::uint64_t aRun, core::queued_finish aFinish);
		/// \brief As `retired`, for a run that never took its message.
		void               returned(std::uint64_t aRuntime, const std::string& aSession);
		[[nodiscard]] auto running(const std::string& aSession) const -> std::optional<std::uint64_t>;
		/// \brief The sessions whose turn `aRuntime` runs.
		[[nodiscard]] auto running_on(std::uint64_t aRuntime) const -> std::vector<std::string>;

		[[nodiscard]] auto state(const std::string& aSession) -> core::queue_agent override;
		[[nodiscard]] auto start(const std::string& aSession, const core::queued_entry& aEntry) -> queued_admission override;
		[[nodiscard]] auto steer(const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string> override;

	private:
		agent_queue_host*                              Host = nullptr;
		std::map<std::uint64_t, agent_session_runtime> Runtimes;
		struct running_release
		{
			std::uint64_t Runtime = 0U;
			/// Zero until its admission names it: a run that retires inside
			/// its own start settles it.
			std::uint64_t Run = 0U;
		};
		std::map<std::string, running_release> Running;
		std::uint64_t                          LastRuntime = 0U;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_RUNNER_HPP
