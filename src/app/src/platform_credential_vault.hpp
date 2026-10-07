#ifndef BPR_APP_PLATFORM_CREDENTIAL_VAULT_HPP
#define BPR_APP_PLATFORM_CREDENTIAL_VAULT_HPP

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"
#include <memory>
#include <optional>
#include <string>

namespace bpr::app {
	/// \brief Creates the platform vault for one exact BPR profile on a retained worker.
	[[nodiscard]] auto make_platform_vault(application::effect_executor& aEffects, core::bpr_profile_id aProfile)
		-> std::unique_ptr<application::github_credential_vault_port>;

	/// \brief [[BPR-1068]] Why a native sign-in started now could not be kept, or nothing.
	///
	/// A terminal asks this before it shows a person a device code. On Linux it
	/// asks the Secret Service whether a store would reach an unlocked default
	/// collection and, if not, returns the setup instruction to print; no item is
	/// read or written. Elsewhere the platform vault needs no session-scoped
	/// service, so the answer is always nothing.
	[[nodiscard]] auto platform_vault_setup_required() -> std::optional<std::string>;
}
#endif
