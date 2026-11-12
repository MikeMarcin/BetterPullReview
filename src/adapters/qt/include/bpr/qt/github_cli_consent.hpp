#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CLI_CONSENT_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_GITHUB_CLI_CONSENT_HPP

#include "bpr/application/review_mutation_port.hpp"

#include <QString>

namespace bpr::qt {
	/// \brief RFC-BPR-0015's complete residual-risk text for a GitHub CLI
	/// read. RFC-BPR-0032 amendment r1: the account setup shows it behind
	/// What connecting allows, under the account's consent sentence.
	[[nodiscard]] auto github_cli_read_consent_details() -> QString;

	/// \brief Fixed actionable text for typed CLI refusal in the existing publication surface.
	[[nodiscard]] auto github_cli_publication_failure_text(application::review_mutation_failure aFailure,
														   const QString& aLogin, const QString& aHost, bool aStarted = false) -> QString;
	/// \brief Explains a refused reconciliation without claiming the earlier attempt failed.
	[[nodiscard]] auto github_cli_publication_recovery_text(application::review_mutation_failure aFailure,
															const QString& aLogin, const QString& aHost) -> QString;

}

#endif
