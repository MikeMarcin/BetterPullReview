#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TESTING_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TESTING_HPP

#if defined(BPR_APPLICATION_TESTING)
	#include <cstdint>

namespace bpr::application::detail::testing {
	// Names the recoverable allocation boundaries inside epoch-host job
	// admission. Each one can fail independently, and each must leave the job
	// ledger, the terminal reservation, and the active authority exactly as
	// they were before the attempt.
	/// \brief Describes the epoch admission allocation stage record.
	enum class epoch_admission_allocation_stage : std::uint8_t {
		job_control,
		decision_task,
		job_ledger,
	};

	// Arms one test-only allocation failure at the requested admission stage.
	// The next admission that reaches that stage throws, and the seam disarms
	// itself so a single arming cannot affect later work.
	/// \brief Performs the fail next epoch job admission operation.
	void fail_next_epoch_job_admission(
		epoch_admission_allocation_stage aStage) noexcept;

	// Disarms any pending admission failure.
	/// \brief Performs the clear epoch job admission failure operation.
	void clear_epoch_job_admission_failure() noexcept;
} // namespace bpr::application::detail::testing
#endif

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_TESTING_HPP
