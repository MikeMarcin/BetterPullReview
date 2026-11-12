#include "github_application/window_composition.hpp"

#include "bpr/application/account_setup_host.hpp"
#include "bpr/core/github_transition.hpp"

#include <algorithm>
#include <utility>

namespace bpr::app {
	namespace {
		[[nodiscard]] auto inspection_of(const qt::github_cli_discovery_state aFailure) noexcept -> core::github_cli_inspection
		{
			switch (aFailure) {
			case qt::github_cli_discovery_state::missing_executable:  return core::github_cli_inspection::missing;
			case qt::github_cli_discovery_state::unsupported_version: return core::github_cli_inspection::outdated;
			case qt::github_cli_discovery_state::ineligible_accounts: return core::github_cli_inspection::no_usable_account;
			default:                                                  return core::github_cli_inspection::failed;
			}
		}
	} // namespace

	/// \brief RFC-BPR-0032 amendment r1. Home's account setup for this
	/// window: the host, its projection into the window, and the window's
	/// gestures into the host. Composed only where the profile asks for it.
	void github_application::implementation::compose_account_setup()
	{
		if (!Profile->AccountSetupInWindow || Window == nullptr) {
			return;
		}
		auto weak    = std::weak_ptr<const int>{ WindowLifetime };
		AccountSetup = std::make_unique<application::account_setup_host>(
			*this, DecisionLane,
			[this, weak](const application::account_setup_projection& aProjection) {
				if (!weak.expired() && !Closing && Window != nullptr) {
					Window->present_account_setup(aProjection);
				}
			},
			Profile->cli_read_offered_here());
		Window->set_account_setup_dispatch(
			[this, weak](core::account_setup_event aEvent) {
				if (!weak.expired() && !Closing && AccountSetup) {
					AccountSetup->dispatch(std::move(aEvent));
				}
			},
			[this, weak] {
				if (!weak.expired() && !Closing && AccountSetup) {
					AccountSetup->open_verification_address();
				}
			});
	}

	void github_application::implementation::retire_account_setup()
	{
		if (AccountSetup) {
			AccountSetup->shutdown();
		}
		AccountSetupCliRequest.reset();
	}

	// The window's GitHub presentation, as the setup needs it: an account
	// accepted, or the session that was being established failing.
	void github_application::implementation::observe_account_setup_session(const application::github_presentation_update& aUpdate)
	{
		if (!AccountSetup || !AccountSetup->active()) {
			return;
		}
		if (aUpdate.Session && aUpdate.Session->Account.value() != 0U && !aUpdate.Session->Login.empty()) {
			const auto* const profile = find_profile(aUpdate.Session->Profile);
			auto              session = core::account_setup_session_observed{ };
			session.Outcome           = core::account_setup_session::ready;
			session.Login             = aUpdate.Session->Login;
			session.Hostname          = profile != nullptr ? std::string{ profile->hostname() } : std::string{ };
			AccountSetup->observe_session(std::move(session));
			return;
		}
		if (aUpdate.Transition && aUpdate.Transition->Status) {
			const auto status = aUpdate.Transition->Status->Status;
			if (status == core::github_status::session_failed || status == core::github_status::session_cancelled) {
				auto session    = core::account_setup_session_observed{ };
				session.Outcome = status == core::github_status::session_failed ? core::account_setup_session::failed : core::account_setup_session::cancelled;
				AccountSetup->observe_session(std::move(session));
			}
		}
	}

	void github_application::implementation::observe_account_setup_device(const application::device_authorization_presentation& aPresentation)
	{
		if (AccountSetup) {
			AccountSetup->observe_device(aPresentation);
		}
	}

	// What gh's discovery found, for a setup that asked. Every answer names
	// the setup's request, so one that outlived it is dropped by the core.
	void github_application::implementation::answer_account_setup_cli(const std::vector<adapters::github::detail::gh_cli_inventory_account>* aDiscovered,
																	  const qt::github_cli_discovery_state                                   aFailure)
	{
		if (!AccountSetup || !AccountSetupCliRequest) {
			return;
		}
		auto answer    = core::github_cli_inspected{ };
		answer.Request = *AccountSetupCliRequest;
		AccountSetupCliRequest.reset();
		if (aDiscovered == nullptr) {
			answer.Outcome = inspection_of(aFailure);
			AccountSetup->complete_github_cli(std::move(answer));
			return;
		}
		answer.Outcome = core::github_cli_inspection::read;
		for (const auto& discovered : *aDiscovered) {
			if (!discovered.SignedIn || !discovered.KeychainHeld) {
				continue;
			}
			// Registered by the discovery that just ran; the pinned profile is
			// the one Connect activates.
			const auto pinned = std::ranges::find_if(Profiles, [&discovered](const adapters::github::validated_github_host_profile& aProfile) {
				return aProfile.hostname() == discovered.Hostname && aProfile.authentication_transport() == application::github_authentication_transport::github_cli
					&& aProfile.cli_account_login() == discovered.Login;
			});
			if (pinned == Profiles.end()) {
				continue;
			}
			auto account       = core::account_setup_account{ };
			account.Profile    = pinned->profile();
			account.Login      = discovered.Login;
			account.Hostname   = discovered.Hostname;
			account.ActiveInGh = discovered.Active;
			answer.Accounts.push_back(std::move(account));
		}
		if (answer.Accounts.empty() && !aDiscovered->empty()) {
			answer.Outcome = core::github_cli_inspection::no_usable_account;
		}
		AccountSetup->complete_github_cli(std::move(answer));
	}

	void github_application::implementation::inspect_github_cli(const std::uint64_t aRequest)
	{
		AccountSetupCliRequest = aRequest;
		// A check already running answers this request too.
		if (!GithubCliDiscoveryPending) {
			discover_github_cli_accounts(false);
		}
		if (!GithubCliDiscoveryPending) {
			// Discovery could not start here (no gh offered, or no connection
			// can be set up now); the setup says gh could not be checked.
			answer_account_setup_cli(nullptr, qt::github_cli_discovery_state::process_failed);
		}
	}

	void github_application::implementation::begin_browser_sign_in()
	{
		const auto native = std::ranges::find_if(Profiles, [](const adapters::github::validated_github_host_profile& aProfile) {
			return aProfile.authentication_transport() == application::github_authentication_transport::native_https
				&& aProfile.server_contract() == application::github_server_contract::github_com_2022_11_28;
		});
		if (native == Profiles.end() || DeviceHost == nullptr || Host == nullptr || !core::github_connection_setup_available(Host->state())) {
			if (AccountSetup) {
				AccountSetup->dispatch(core::account_setup_device_observed{ .Stage = core::device_authorization_stage::unavailable });
			}
			return;
		}
		// The device panel's own begin: the same intent, through the one place
		// a device gesture becomes an effect.
		dispatch_device_authorization_gesture(core::begin_device_authorization_intent{ .Profile = native->profile() });
	}

	void github_application::implementation::cancel_browser_sign_in() { dispatch_device_authorization_gesture(core::cancel_device_authorization_intent{ }); }

	void github_application::implementation::connect_github_cli(const core::host_profile_id aProfile)
	{
		// Connect under the account's consent text is the consent the shared
		// dialog asked for, so the activation asks no question of its own.
		Profile->CliConsent.insert(aProfile);
		Window->connect_github_profile(aProfile);
	}

	void github_application::implementation::open_verification_address() { dispatch_device_authorization_gesture(application::open_verification_address_gesture{ }); }
} // namespace bpr::app
