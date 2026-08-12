#ifndef BPR_SRC_ADAPTERS_AGENT_SRC_WINDOWS_AGENT_PROCESS_HPP
#define BPR_SRC_ADAPTERS_AGENT_SRC_WINDOWS_AGENT_PROCESS_HPP

#include "bpr/application/agent_launch.hpp"

#include <functional>

namespace bpr::adapters::agent::detail {
	using cancellation_probe = std::function<bool()>;

	[[nodiscard]] auto run_windows_agent_process(
		const application::agent_launch_request& aRequest,
		int                                      aTimeoutMilliseconds,
		const application::agent_output_sink&    aSink,
		const cancellation_probe&                aCancelled)
		-> application::agent_run_result;

#if defined(BPR_BUILD_TESTING)
	void force_windows_job_assignment_failure_for_testing(bool aEnabled) noexcept;
#endif
} // namespace bpr::adapters::agent::detail

#endif // BPR_SRC_ADAPTERS_AGENT_SRC_WINDOWS_AGENT_PROCESS_HPP
