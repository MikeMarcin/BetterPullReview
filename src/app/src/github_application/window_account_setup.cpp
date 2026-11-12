#include "github_application/window_composition.hpp"

#include "bpr/application/account_setup_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/core/github_transition.hpp"
#include "bpr/qt/account_setup_view.hpp"

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

	/// \brief RFC-BPR-0032 amendment r1. The window's one account setup: the
	/// host, its projection into Home, and the window's gestures into the host.
	/// Every way in (Home, Settings, Use another account, a pasted link) uses it.
	void github_application::implementation::compose_account_setup()
	{
		if (Window == nullptr) {
			return;
		}
		auto weak    = std::weak_ptr<const int>{ WindowLifetime };
		AccountSetup = std::make_unique<application::account_setup_host>(
			*this, DecisionLane,
			[this, weak](const application::account_setup_projection& aProjection) {
				if (!weak.expired() && !Closing && Window != nullptr) {
					observe_account_setup_projection(aProjection);
					Window->present_account_setup(aProjection);
				}
			},
			Profile->cli_read_offered_here());
		auto intents     = qt::account_setup_intents{ };
		intents.Dispatch = [this, weak](core::account_setup_event aEvent) {
			if (!weak.expired() && !Closing && AccountSetup) {
				AccountSetup->dispatch(std::move(aEvent));
			}
		};
		intents.OpenVerificationAddress = [this, weak] {
			if (!weak.expired() && !Closing && AccountSetup) {
				AccountSetup->open_verification_address();
			}
		};
		// The token is held here, as credential material, only until the
		// enroll the setup asks for next; it never reaches the core.
		intents.SubmitToken = [this, weak](qt::sensitive_bytes aBytes) {
			if (weak.expired() || Closing) {
				return;
			}
			auto material = application::github_credential_material::create(aBytes.bytes());
			aBytes.reset();
			AccountSetupToken.reset();
			if (material.has_value()) {
				AccountSetupToken.emplace(std::move(*material));
			}
		};
		intents.MaximumTokenBytes = application::maximum_github_credential_bytes;
		Window->set_account_setup_intents(std::move(intents));
	}

	void github_application::implementation::retire_account_setup()
	{
		if (AccountSetup) {
			AccountSetup->shutdown();
		}
		AccountSetupCliRequest.reset();
		AccountSetupServerRequest.reset();
		AccountSetupToken.reset();
	}

	// When the setup opens, the window's session then is its baseline: only a
	// session established after it is an account the setup added. A setup
	// closed before an account was added ends a pasted link that waited on it.
	void github_application::implementation::observe_account_setup_projection(const application::account_setup_projection& aProjection)
	{
		const auto phase  = aProjection.State.Phase;
		const auto opened = AccountSetupLastPhase == core::account_setup_phase::idle && phase != core::account_setup_phase::idle;
		const auto closed = AccountSetupLastPhase != core::account_setup_phase::idle && phase == core::account_setup_phase::idle;
		if (opened) {
			const auto session   = Host ? Host->accepted_session() : std::nullopt;
			AccountSetupBaseline = session && session->Mode == core::github_access_mode::authenticated
				? std::optional{ std::pair{ session->Profile, session->Viewer.Account } }
				: std::nullopt;
			if (AccountSetup) {
				AccountSetup->observe_signed_in(AccountSetupBaseline.has_value());
				if (AccountSetupBaseline.has_value() && core::account_setup_superseded(aProjection.State)) {
					AccountSetup->dispatch(core::finish_account_setup{ });
				}
			}
		}
		if (closed && AccountSetupLastPhase != core::account_setup_phase::connected) {
			AccountSetupToken.reset();
			if (SetupDestination && SetupDestination->pending()) {
				retire_setup_effects();
				SetupOwner = next_setup_owner();
				SetupDestination->cancel();
			}
		}
		AccountSetupLastPhase = phase;
	}

	// The window's GitHub presentation, as the setup needs it: an account
	// newly accepted, or the session that was being established failing.
	void github_application::implementation::observe_account_setup_session(const application::github_presentation_update& aUpdate)
	{
		if (!AccountSetup) {
			return;
		}
		const auto accepted      = Host ? Host->accepted_session() : std::nullopt;
		const auto authenticated = accepted && accepted->Mode == core::github_access_mode::authenticated;
		AccountSetup->observe_signed_in(authenticated);
		if (!AccountSetup->active()) {
			return;
		}
		// Home began it because the window looked signed out; the window has an
		// account after all, and no way was started.
		if (authenticated && core::account_setup_superseded(AccountSetup->projection().State)) {
			AccountSetup->dispatch(core::finish_account_setup{ });
			return;
		}
		const auto status = aUpdate.Transition && aUpdate.Transition->Status ? std::optional{ aUpdate.Transition->Status->Status } : std::nullopt;
		if (aUpdate.Session && aUpdate.Session->Account.value() != 0U && !aUpdate.Session->Login.empty()) {
			const auto identity    = std::pair{ aUpdate.Session->Profile, aUpdate.Session->Account };
			const auto established = status == core::github_status::session_ready;
			if (established || AccountSetupBaseline != identity) {
				const auto* const profile = find_profile(aUpdate.Session->Profile);
				auto              session = core::account_setup_session_observed{ };
				session.Outcome           = core::account_setup_session::ready;
				session.Login             = aUpdate.Session->Login;
				session.Hostname          = profile != nullptr ? std::string{ profile->hostname() } : std::string{ };
				AccountSetup->observe_session(std::move(session));
				return;
			}
		}
		if (status == core::github_status::session_failed || status == core::github_status::session_cancelled) {
			auto session    = core::account_setup_session_observed{ };
			session.Outcome = *status == core::github_status::session_failed ? core::account_setup_session::failed : core::account_setup_session::cancelled;
			AccountSetup->observe_session(std::move(session));
		}
	}

	void github_application::implementation::observe_account_setup_device(const application::device_authorization_presentation& aPresentation)
	{
		if (AccountSetup) {
			AccountSetup->observe_device(aPresentation);
		}
	}

	// What gh's discovery found, for a setup that asked: the usable accounts
	// on the host asked about. Every answer names the setup's request, so one
	// that outlived it is dropped by the core.
	void github_application::implementation::answer_account_setup_cli(const std::vector<adapters::github::detail::gh_cli_inventory_account>* aDiscovered,
																	  const qt::github_cli_discovery_state                                   aFailure)
	{
		if (!AccountSetup || !AccountSetupCliRequest) {
			return;
		}
		auto answer     = core::github_cli_inspected{ };
		answer.Request  = AccountSetupCliRequest->first;
		const auto host = AccountSetupCliRequest->second;
		AccountSetupCliRequest.reset();
		if (aDiscovered == nullptr) {
			answer.Outcome = inspection_of(aFailure);
			AccountSetup->complete_github_cli(std::move(answer));
			return;
		}
		answer.Outcome  = core::github_cli_inspection::read;
		auto onThisHost = false;
		for (const auto& discovered : *aDiscovered) {
			if (discovered.Hostname != host) {
				continue;
			}
			onThisHost = true;
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
		if (answer.Accounts.empty() && onThisHost) {
			answer.Outcome = core::github_cli_inspection::no_usable_account;
		}
		AccountSetup->complete_github_cli(std::move(answer));
	}

	void github_application::implementation::inspect_github_cli(const std::uint64_t aRequest, const std::string& aHostname)
	{
		AccountSetupCliRequest = std::pair{ aRequest, aHostname };
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

	auto github_application::implementation::native_enterprise_profile(const std::string& aHostname) const -> std::optional<core::host_profile_id>
	{
		const auto found = std::ranges::find_if(Profiles, [&aHostname](const adapters::github::validated_github_host_profile& aProfile) {
			return aProfile.hostname() == aHostname && aProfile.authentication_transport() == application::github_authentication_transport::native_https
				&& aProfile.server_contract() != application::github_server_contract::github_com_2022_11_28;
		});
		return found == Profiles.end() ? std::nullopt : std::optional{ found->profile() };
	}

	// An Enterprise server has a profile once the queue keeps it: one already
	// kept answers at once; another is added as Settings' draft added it, and
	// answers when `synchronize_setup_servers` registers it or the add fails.
	void github_application::implementation::prepare_enterprise_server(const std::uint64_t aRequest, const std::string& aHostname)
	{
		if (!AccountSetup) {
			return;
		}
		auto answer    = core::enterprise_server_prepared{ };
		answer.Request = aRequest;
		if (const auto profile = native_enterprise_profile(aHostname); profile.has_value()) {
			answer.Profile = profile;
			AccountSetup->complete_enterprise_server(answer);
			return;
		}
		if (!QueueHost || !SetupQueueReady) {
			AccountSetup->complete_enterprise_server(answer);
			return;
		}
		AccountSetupServerRequest = std::pair{ aRequest, aHostname };
		QueueHost->apply(application::saved_server_intent{ .Action = application::saved_server_action::add, .Server = { .Hostname = aHostname, .Account = { } } });
	}

	void github_application::implementation::settle_account_setup_server(const bool aFailed)
	{
		if (!AccountSetup || !AccountSetupServerRequest) {
			return;
		}
		auto answer    = core::enterprise_server_prepared{ };
		answer.Request = AccountSetupServerRequest->first;
		answer.Profile = native_enterprise_profile(AccountSetupServerRequest->second);
		if (!answer.Profile && !aFailed) {
			return;
		}
		AccountSetupServerRequest.reset();
		AccountSetup->complete_enterprise_server(answer);
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
		// a device gesture becomes an effect. A pasted link waiting on the
		// sign-in takes the grant there.
		dispatch_device_authorization_gesture(core::begin_device_authorization_intent{ .Profile = native->profile() });
	}

	void github_application::implementation::cancel_browser_sign_in() { dispatch_device_authorization_gesture(core::cancel_device_authorization_intent{ }); }

	void github_application::implementation::connect_github_cli(const core::host_profile_id aProfile)
	{
		// Connect under the account's consent text is the consent; the
		// activation asks no question of its own.
		Profile->CliConsent.insert(aProfile);
		Window->connect_github_profile(aProfile);
	}

	// The token the setup's field handed over, for the profile the setup
	// names: staged for a pasted link that waits on this server, otherwise
	// enrolled as the manual sign-in was.
	void github_application::implementation::enroll_token(const core::host_profile_id aProfile)
	{
		auto material = std::exchange(AccountSetupToken, std::nullopt);
		if (!material || !Host || !DeviceHost) {
			if (AccountSetup) {
				auto session    = core::account_setup_session_observed{ };
				session.Outcome = core::account_setup_session::failed;
				AccountSetup->observe_session(std::move(session));
			}
			return;
		}
		const auto* const profile = find_profile(aProfile);
		if (profile != nullptr && SetupDestination && SetupDestination->pending() && SetupDestination->pending()->Destination.Hostname == profile->hostname()) {
			if (SetupDestination->pending()->Stage != core::setup_destination_stage::authenticating) {
				static_cast<void>(SetupDestination->begin_authentication(SetupDestination->pending()->Request, aProfile));
			}
			if (const auto pending = SetupDestination->pending(); pending && pending->Stage == core::setup_destination_stage::authenticating && pending->Owner == SetupOwner) {
				stage_setup_credential(*pending, std::move(*material));
				return;
			}
		}
		DeviceHost->release_refresh_authority();
		DeviceRefreshAuthority.reset();
		Host->enroll_profile(
			native_sign_in_profile(aProfile), std::move(*material), application::github_credential_provenance::manual, std::nullopt, false,
			[runtime = Profile.get()](const application::github_profile_authority& aAuthority, const application::github_viewer_record& aViewer) { return runtime->rebind_native_sign_in(aAuthority, aViewer); });
	}

	void github_application::implementation::open_verification_address() { dispatch_device_authorization_gesture(application::open_verification_address_gesture{ }); }

	// RFC-BPR-0032 amendment r1. A sign-in the window needs but no gesture
	// asked of the setup (a profile activated without a credential, a gh
	// account without consent) opens the setup at the step it needs.
	void github_application::implementation::begin_account_setup_for(const adapters::github::validated_github_host_profile& aProfile)
	{
		if (Window == nullptr) {
			return;
		}
		const auto hostname  = std::string{ aProfile.hostname() };
		const auto githubCom = aProfile.server_contract() == application::github_server_contract::github_com_2022_11_28;
		const auto server    = githubCom ? core::account_setup_server::github_com : core::account_setup_server::enterprise;
		const auto cli       = aProfile.authentication_transport() == application::github_authentication_transport::github_cli;
		const auto way       = cli ? core::account_setup_way::github_cli : githubCom ? core::account_setup_way::browser
																					 : core::account_setup_way::token;
		Window->begin_account_setup(server, hostname, way);
	}
} // namespace bpr::app
