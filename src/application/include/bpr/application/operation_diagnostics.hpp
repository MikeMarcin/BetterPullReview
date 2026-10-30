#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OPERATION_DIAGNOSTICS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OPERATION_DIAGNOSTICS_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/operation_diagnosis.hpp"

#include <chrono>
#include <cstdint>

namespace bpr::application {
	/// \brief [[BPR-1828]]. The diagnostic operation an operation line's
	/// action is logged as.
	[[nodiscard]] auto diagnostic_operation_of(core::operation_action aAction) noexcept -> diagnostic_operation;

	/// \brief [[BPR-1828]]. Records one operation line: started, succeeded,
	/// failed or cancelled, the outcome following the diagnosis's failure
	/// (`cancelled` is a cancellation). Logging never changes what the
	/// operation does: a null log, an invalid line or a full queue is a
	/// dropped record and nothing else.
	void record_operation(diagnostic_log_port* aLog, const core::operation_diagnosis& aDiagnosis, bool aStarted = false) noexcept;

	/// \brief [[BPR-1828]]. Milliseconds since an operation started, on the
	/// steady clock, saturated to 32 bits.
	class operation_stopwatch
	{
	public:
		[[nodiscard]] auto milliseconds() const noexcept -> std::uint32_t;

	private:
		std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();
	};

	/// \brief [[BPR-1828]]. Each family's typed failure, as an operation line names it.
	[[nodiscard]] auto operation_failure_of(core::agent_setup_failure aFailure) noexcept -> core::operation_failure;
	[[nodiscard]] auto operation_failure_of(const agent_workspace_error& aError) noexcept -> core::operation_failure;
	[[nodiscard]] auto operation_failure_of(review_mutation_failure aFailure) noexcept -> core::operation_failure;
	[[nodiscard]] auto operation_process_of(agent_run_outcome aOutcome) noexcept -> core::operation_process;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OPERATION_DIAGNOSTICS_HPP
