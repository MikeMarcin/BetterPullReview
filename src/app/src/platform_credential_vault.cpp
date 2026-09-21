#include "platform_credential_vault.hpp"

#if defined(__APPLE__)
	#include "macos_keychain.hpp"
#elif defined(_WIN32)
	#include "windows_credential_manager.hpp"
#elif defined(__linux__)
	#include "linux_secret_service.hpp"
#endif

namespace bpr::app {
	auto make_platform_vault(application::effect_executor& aEffects, core::bpr_profile_id aProfile)
		-> std::unique_ptr<application::github_credential_vault_port>
	{
#if defined(__APPLE__)
		auto vault = adapters::github::make_macos_keychain_vault(aEffects, aProfile);
#elif defined(_WIN32)
		auto vault = adapters::github::make_windows_credential_manager_vault(aEffects, aProfile);
#elif defined(__linux__)
		auto vault = adapters::github::make_linux_secret_service_vault(aEffects, aProfile);
#else
		static_cast<void>(aEffects);
		static_cast<void>(aProfile);
		return { };
#endif
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
		return vault ? std::move(*vault) : nullptr;
#endif
	}
}
