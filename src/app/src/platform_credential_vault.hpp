#ifndef BPR_APP_PLATFORM_CREDENTIAL_VAULT_HPP
#define BPR_APP_PLATFORM_CREDENTIAL_VAULT_HPP

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"
#include <memory>

namespace bpr::app {
	/// \brief Creates the platform vault for one exact BPR profile on a retained worker.
	[[nodiscard]] auto make_platform_vault(application::effect_executor& aEffects, core::bpr_profile_id aProfile)
		-> std::unique_ptr<application::github_credential_vault_port>;
}
#endif
