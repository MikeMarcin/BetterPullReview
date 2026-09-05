#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_SUBMISSION_RECOVERY_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_SUBMISSION_RECOVERY_STORE_HPP
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/application/review_submission_recovery_port.hpp"
namespace bpr::adapters::sqlite {
	/// \brief Opens anchor-free recovery in the same exact authority-scoped v3 database.
	[[nodiscard]] auto make_review_submission_recovery_store(const review_mutation_recovery_configuration& aConfiguration) -> std::expected<std::unique_ptr<application::review_submission_recovery_port>, application::review_mutation_recovery_failure>;
}
#endif
