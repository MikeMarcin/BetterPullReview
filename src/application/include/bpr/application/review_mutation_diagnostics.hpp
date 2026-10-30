#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_DIAGNOSTICS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_DIAGNOSTICS_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/review_mutation_port.hpp"

#include <memory>

namespace bpr::application {
	/// \brief [[BPR-1828]]. Wraps a review mutation provider so every call
	/// leaves a `github.review_mutation` record.
	///
	/// \details A write (publish, resolve, unresolve, react, edit, request a
	/// reviewer) records its start and its end; a read made for a write
	/// (observe the target, search reviewers, reconcile or repair a
	/// publication) records its end. The end is `succeeded`, `failed` with
	/// the provider's typed failure, or `cancelled`, with the call's
	/// duration, whether a write was sent and whether its outcome was read
	/// back. No thread, comment, body, login, query or repository reaches
	/// the line. The wrapped provider's answers pass through unchanged; a
	/// null `aProvider` gives null, and a null `aLog` gives the provider
	/// itself.
	[[nodiscard]] auto with_review_mutation_diagnostics(std::unique_ptr<review_mutation_port> aProvider, std::shared_ptr<diagnostic_log_port> aLog)
		-> std::unique_ptr<review_mutation_port>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_DIAGNOSTICS_HPP
