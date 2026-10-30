#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CREDENTIAL_VAULT_DIAGNOSTICS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CREDENTIAL_VAULT_DIAGNOSTICS_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_credential_vault.hpp"

#include <memory>

namespace bpr::application {
	/// \brief [[BPR-1829]]. Wraps a credential vault so every store, load and
	/// delete of the account slot or its refresh slot leaves a
	/// `credential.vault` record.
	///
	/// \details Each submission records its start; its one terminal records
	/// `succeeded`, `failed` with the vault's typed failure (`locked`,
	/// `access_denied`, `corrupt`, ...), or `cancelled`, with the time from
	/// submission to terminal. A load counts 1 when the slot held material
	/// and 0 when it was absent; a delete counts 1 when it removed an entry
	/// and 0 when there was none. A submission that throws records
	/// `unavailable` and rethrows. No credential byte, length, account,
	/// hostname, profile or request identity reaches the line.
	///
	/// A record says what this vault port answered, not which platform
	/// store answered it or that an entry survives a restart: the native
	/// vault's own guarantees (M8/M9) are not proved here. Calls, answers
	/// and cancellation pass through unchanged. A null `aVault` gives null,
	/// and a null `aLog` gives the vault itself.
	[[nodiscard]] auto with_credential_vault_diagnostics(std::unique_ptr<github_credential_vault_port> aVault, std::shared_ptr<diagnostic_log_port> aLog)
		-> std::unique_ptr<github_credential_vault_port>;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_CREDENTIAL_VAULT_DIAGNOSTICS_HPP
