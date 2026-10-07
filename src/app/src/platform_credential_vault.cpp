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

	auto platform_vault_setup_required() -> std::optional<std::string>
	{
#if defined(__linux__)
		const auto ready = adapters::github::linux_secret_service_store_readiness();
		if (ready) {
			return std::nullopt;
		}
		if (ready.error() == application::github_credential_vault_failure::locked) {
			return "The Secret Service default collection is locked, so a sign-in could not be stored. "
				   "Nothing was requested from GitHub. BPR never unlocks or prompts: unlock the default "
				   "(login) keyring in this session, then rerun this command.";
		}
		return "No unlocked Secret Service default collection is reachable on this session's D-Bus, "
			   "so a sign-in could not be stored. Nothing was requested from GitHub. Run this command "
			   "in a session whose bus has a Secret Service provider (for example gnome-keyring) with "
			   "an unlocked default collection. Without a desktop, start one first, for example "
			   "dbus-run-session -- sh, then gnome-keyring-daemon --unlock --components=secrets "
			   "with the keyring password on standard input, then rerun.";
#else
		return std::nullopt;
#endif
	}
}
