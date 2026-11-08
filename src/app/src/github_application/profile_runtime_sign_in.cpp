#include "github_application/profile_runtime.hpp"

#include "bpr/application/profile_storage_authority.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/device_authorization_model.hpp"
#include "github_application/cli_accounts.hpp"
#include "github_application/profile_hosts.hpp"
#include "github_cli_runtime.hpp"

#include <QDesktopServices>

#include <algorithm>
#include <variant>

namespace bpr::app {
	void github_profile_runtime::connection_notice(application::connection_metadata_observation aObservation)
	{
		const auto message      = aObservation.Saved ? QString{ } : aObservation.Action == application::connection_metadata_action::remember ? QStringLiteral("This connection is session-only because it could not be saved. Connect again on your next launch.")
																																			 : QStringLiteral("The connection change could not be saved. Automatic sign-in may return on your next launch; retry the change.");
		RestorationGlobalNotice = message.isEmpty() ? std::nullopt : std::optional{ message };
		for_each_window([&](const window_observer& aObserver) { if (aObserver.RestoreNotice) { aObserver.RestoreNotice(message); } });
	}

	auto github_profile_runtime::selected_connection(std::optional<std::string_view> aHostname) const -> std::optional<core::remembered_connection>
	{
		return Connections && Connections->available() ? core::select_remembered_connection(Connections->state(), aHostname) : std::nullopt;
	}

	auto github_profile_runtime::suppress_device(std::string_view aHostname) const -> bool
	{
		if (!Connections || !Connections->available()) {
			return true;
		}
		const auto& rows = Connections->state().Connections;
		const auto  row  = std::ranges::find(rows, aHostname, &core::remembered_connection::Hostname);
		// Once metadata has recorded a decision, an absent row means no
		// remembered authority. Forget must not revive a legacy device grant.
		return row == rows.end() ? Connections->state().Revision > 1U : !row->ReuseEnabled || row->Transport == core::remembered_connection_transport::github_cli;
	}

	void github_profile_runtime::observe_connection()
	{
		if (Closing || !Connections || !Host || !Host->accepted_session()) {
			return;
		}
		const auto& session = *Host->accepted_session();
		if (session.Mode != core::github_access_mode::authenticated || RememberedGeneration == Host->accepted_identity_generation()) {
			return;
		}
		RememberedGeneration = Host->accepted_identity_generation();
		const auto profile   = std::ranges::find(Profiles, session.Profile, &adapters::github::validated_github_host_profile::profile);
		if (profile == Profiles.end()) {
			return;
		}
		const auto cli = profile->authentication_transport() == application::github_authentication_transport::github_cli;
		if (cli && (!CliConsent.contains(session.Profile) || profile->cli_account_login().empty())) {
			return;
		}
		if (!cli && !Host->accepted_device_connection()) {
			static_cast<void>(Connections->disable(std::string{ profile->hostname() }));
			return;
		}
		if (!cli && !RememberDeviceSignIn) {
			return;
		}
		core::remembered_connection row;
		row.Profile         = session.Profile;
		row.ProfileRevision = profile->revision().value();
		row.Host            = session.Host;
		row.Hostname        = profile->hostname();
		row.Login           = session.Viewer.Login;
		row.Account         = session.Viewer.Account;
		row.Server          = profile->product_kind() == application::github_product_kind::github_com ? core::remembered_connection_server::github_com : core::remembered_connection_server::ghes_3_18;
		row.Transport       = cli ? core::remembered_connection_transport::github_cli : core::remembered_connection_transport::native_device;
		static_cast<void>(Connections->remember(std::move(row)));
	}

	auto github_profile_runtime::rebind_native_sign_in(const application::github_profile_authority& aAuthority, const application::github_viewer_record& aViewer) -> std::optional<core::host_profile_id>
	{
		if (Closing || !Connections || !Connections->available() || !ProfileCatalog) {
			return std::nullopt;
		}
		const auto found = std::ranges::find(Profiles, aAuthority.Profile, &adapters::github::validated_github_host_profile::profile);
		if (found == Profiles.end() || found->authentication_transport() != application::github_authentication_transport::native_https) {
			return std::nullopt;
		}
		// Profiles is a deque: an element's address survives a push_back.
		const auto*                 begun = &*found;
		core::remembered_connection signIn;
		signIn.Profile         = begun->profile();
		signIn.ProfileRevision = begun->revision().value();
		signIn.Host            = begun->host();
		signIn.Hostname        = begun->hostname();
		signIn.Account         = aViewer.Account;
		signIn.Login           = aViewer.Login;
		signIn.Server          = begun->product_kind() == application::github_product_kind::github_com ? core::remembered_connection_server::github_com : core::remembered_connection_server::ghes_3_18;
		signIn.Transport       = core::remembered_connection_transport::native_device;
		const auto adopted     = core::adopt_native_connection(Connections->state(), signIn);
		if (!adopted) {
			return std::nullopt;
		}
		const auto  registered = std::ranges::find(Profiles, adopted->Profile, &adapters::github::validated_github_host_profile::profile);
		const auto* earlier    = registered != Profiles.end() ? &*registered : nullptr;
		if (earlier == nullptr) {
			const auto candidate = adapters::github::validate_github_host_profile(native_profile_candidate(adopted->Profile, adopted->Host, application::github_profile_revision{ adopted->ProfileRevision }, begun->server_contract(), application::github_authentication_transport::native_https, std::string{ begun->hostname() }, begun->provenance()));
			if (!candidate || !ProfileCatalog->register_profile(*candidate)) {
				return std::nullopt;
			}
			Profiles.push_back(*candidate);
			earlier     = &Profiles.back();
			auto option = profile_option(*candidate, std::string{ candidate->hostname() } + " — Native HTTPS");
			ProfileOptions.push_back(option);
			for_each_window([&](const window_observer& aObserver) { if (aObserver.SetupProfile) { aObserver.SetupProfile(option); } });
		}
		if (earlier->authentication_transport() != application::github_authentication_transport::native_https
			|| earlier->revision().value() != adopted->ProfileRevision || earlier->host() != adopted->Host || earlier->hostname() != adopted->Hostname
			|| earlier->server_contract() != begun->server_contract()) {
			return std::nullopt;
		}
		// The device grant's refresh binding follows the session's profile.
		if (DeviceHost && DeviceCapabilityRegistered && DeviceCapabilityProfile == begun->profile()) {
			if (!DeviceHost->replace_profile(DeviceCapabilityProfile, earlier->profile())) {
				return std::nullopt;
			}
			DeviceCapabilityProfile = earlier->profile();
		}
		return earlier->profile();
	}

	auto github_profile_runtime::restore_cli(const core::remembered_connection& aRow) -> bool
	{
		if (aRow.Transport != core::remembered_connection_transport::github_cli) {
			return false;
		}
		CliConsent.insert(aRow.Profile);
		Host->dispatch(core::activate_host_profile_intent{ .Profile = aRow.Profile });
		return true;
	}

	void github_profile_runtime::remember_startup(std::optional<core::request_id> aRequest)
	{
		if (RememberedStartupStarted || Closing) {
			return;
		}
		RememberedStartupStarted = true;
		if (const auto row = selected_connection(); row && restore_cli(*row)) {
			return;
		}
		const auto selected = selected_connection();
		const auto native   = selected ? std::ranges::find(Profiles, selected->Profile, &adapters::github::validated_github_host_profile::profile) : Profiles.begin();
		if (!RememberDeviceSignIn || native == Profiles.end() || suppress_device(native->hostname())) {
			return;
		}
		const auto& profile = *native;
		auto        current = std::function<bool()>{ };
		if (aRequest) {
			current = [weak = weak_from_this(), request = *aRequest] {
				const auto self = weak.lock();
				return self && !self->Closing && self->ReviewRestoration && self->ReviewRestoration->current_startup(request);
			};
		}
		Host->restore_device_profile(application::github_profile_authority{
										 .Profile = profile.profile(), .ProfileRevision = profile.revision(), .Host = profile.host(), .Hostname = application::github_canonical_hostname{ profile.hostname() }, .ServerContract = profile.server_contract(), .Authentication = profile.authentication_transport(), .ProbeContractRevision = application::github_probe_contract_revision{ 1U } },
									 { }, std::move(current));
	}

	void github_profile_runtime::select_startup(const core::window_review_startup_request& aRequest)
	{
		if (Closing || !ReviewRestoration || !ReviewRestoration->current_startup(aRequest.Request)) {
			return;
		}
		if (!aRequest.Authority) {
			remember_startup(aRequest.Request);
			return;
		}
		RestorationStartup  = aRequest;
		const auto selected = selected_connection(aRequest.Authority->Hostname);
		if (aRequest.Authority->Mode == core::github_access_mode::authenticated && selected && restore_cli(*selected)) {
			return;
		}
		const auto found = std::ranges::find_if(Profiles, [&](const auto& aProfile) { return aProfile.hostname() == aRequest.Authority->Hostname && aProfile.authentication_transport() == application::github_authentication_transport::native_https; });
		if (found == Profiles.end()) {
			ReviewRestoration->complete_startup(aRequest.Request, std::unexpected(core::window_review_failure::missing_server));
			return;
		}
		if (aRequest.Authority->Mode == core::github_access_mode::public_preview) {
			Host->activate_public_preview(found->profile());
			observe_restoration_preview();
			observe_restoration_session();
			return;
		}
		if (!RememberDeviceSignIn || suppress_device(aRequest.Authority->Hostname)) {
			ReviewRestoration->complete_startup(aRequest.Request, std::unexpected(core::window_review_failure::unavailable));
			return;
		}
		const auto& profile  = *found;
		const auto  admitted = Host->restore_device_profile(application::github_profile_authority{ .Profile = profile.profile(), .ProfileRevision = profile.revision(), .Host = profile.host(), .Hostname = application::github_canonical_hostname{ profile.hostname() }, .ServerContract = profile.server_contract(), .Authentication = profile.authentication_transport(), .ProbeContractRevision = application::github_probe_contract_revision{ 1U } }, [weak = weak_from_this(), request = aRequest.Request](application::device_profile_restore_status aStatus) {
				const auto self = weak.lock();
				if (!self || self->Closing || !self->ReviewRestoration || !self->ReviewRestoration->current_startup(request)) { return; }
				if (aStatus != application::device_profile_restore_status::ready_for_session) {
					self->ReviewRestoration->complete_startup(request, std::unexpected(aStatus == application::device_profile_restore_status::cancelled ? core::window_review_failure::cancelled : core::window_review_failure::unavailable));
				} else { self->observe_restoration_session(); } }, [weak = weak_from_this(), request = aRequest.Request] {
				const auto self = weak.lock();
				return self && !self->Closing && self->ReviewRestoration && self->ReviewRestoration->current_startup(request); });
		if (!admitted) {
			ReviewRestoration->complete_startup(aRequest.Request, std::unexpected(core::window_review_failure::unavailable));
		}
	}

	void github_profile_runtime::observe_restoration_preview()
	{
		if (ReviewRestoration && RestorationStartup && RestorationStartup->Authority
			&& RestorationStartup->Authority->Mode == core::github_access_mode::public_preview
			&& ReviewRestoration->current_startup(RestorationStartup->Request) && Host->public_preview_refusal()) {
			ReviewRestoration->complete_startup(RestorationStartup->Request, std::unexpected(core::window_review_failure::unavailable));
		}
	}

	void github_profile_runtime::observe_restoration_session()
	{
		if (!ReviewRestoration || !RestorationStartup || !ReviewRestoration->current_startup(RestorationStartup->Request)) {
			return;
		}
		const auto& session = Host->accepted_session();
		if (!session) {
			return;
		}
		const auto found = std::ranges::find_if(Profiles, [&](const auto& aProfile) { return aProfile.profile() == session->Profile && aProfile.host() == session->Host; });
		if (found == Profiles.end() || !RestorationStartup->Authority || session->Mode != RestorationStartup->Authority->Mode || found->hostname() != RestorationStartup->Authority->Hostname) {
			return;
		}
		ReviewRestoration->complete_startup(RestorationStartup->Request, core::window_review_authority{ session->Mode, std::string{ found->hostname() } });
	}

	void github_profile_runtime::open_review(const core::window_review_open_request& aRequest)
	{
		const auto found = Observers.find(aRequest.Context);
		if (!Closing && found != Observers.end() && !found->second.Lifetime.expired() && found->second.RestoreReview) {
			found->second.RestoreReview(aRequest);
		}
	}

	void github_profile_runtime::fail(const core::window_review_error& aError)
	{
		if (RestorationOrderly && aError.Reason != core::window_review_failure::cancelled) {
			note_profile_cleanup_failure(
				std::string{ "window-review-restoration-failed:reason=" }
				+ std::to_string(static_cast<int>(aError.Reason)));
			CleanupFailed = true;
		}
		auto message = QStringLiteral("The saved reading could not be resumed. Choose a review to continue.");
		switch (aError.Reason) {
		case core::window_review_failure::missing_server:        message = QStringLiteral("The previous review's server is not configured. Choose a review to continue."); break;
		case core::window_review_failure::conflicting_authority: message = QStringLiteral("The saved reading belongs to a different server or access mode. Choose a review to continue."); break;
		case core::window_review_failure::corrupt:
		case core::window_review_failure::unsupported_schema:
		case core::window_review_failure::conflict:              message = QStringLiteral("Saved window reviews are unavailable. Choose a review to continue."); break;
		case core::window_review_failure::cancelled:             return;
		default:                                                 break;
		}
		if (aError.Context) {
			if (aError.Context->Profile != Admission.Authority.profile() || (WindowAdmission && !WindowAdmission(*aError.Context))) {
				return;
			}
			RestorationNotices.insert_or_assign(*aError.Context, message);
			const auto found = Observers.find(*aError.Context);
			if (found != Observers.end() && !found->second.Lifetime.expired() && found->second.RestoreNotice) {
				found->second.RestoreNotice(message);
			}
		} else {
			RestorationGlobalNotice = message;
			for_each_window([&](const window_observer& aObserver) { if (aObserver.RestoreNotice) { aObserver.RestoreNotice(message); } });
		}
	}

	void github_profile_runtime::bind_device_refresh_authority(
		const application::github_presentation_update& aUpdate)
	{
		if (
			Closing || Host == nullptr || DeviceHost == nullptr
			|| Profiles.empty() || !aUpdate.Session.has_value()) {
			return;
		}
		if (aUpdate.Session->Profile != DeviceCapabilityProfile) {
			return;
		}
		const auto authority = Host->accepted_credential_authority();
		if (!authority.has_value()) {
			return;
		}
		// [[BPR-0546]]. The slot's authority and the identity generation
		// that owns it travel together. The device host's own state carries
		// the generation this composition seeded the device flow with,
		// which is zero and stays zero, and every platform vault refuses a
		// context carrying it -- so before this the composed refresh
		// lifecycle stored nothing on any platform.
		const auto generation = Host->accepted_identity_generation();
		if (generation.value() == 0U) {
			return;
		}
		if (
			DeviceRefreshAuthority.has_value()
			&& *DeviceRefreshAuthority == *authority && DeviceRefreshGeneration == generation) {
			return;
		}
		if (DeviceRefreshAuthority == authority && Host->accepted_device_lifetime()) {
			// A rotation established a new access session for this same chain.
			// Rebind its generation; this is not another startup resume.
			DeviceRefreshGeneration = generation;
			DeviceHost->bind_refresh_authority(*authority, generation);
			return;
		}
		if (!Host->accepted_device_lifetime()) {
			DeviceRefreshAuthority.reset();
			DeviceHost->release_refresh_authority();
			return;
		}
		if (
			DeviceHost->state().Stage
			== core::device_authorization_stage::authorized) {
			DeviceRefreshAuthority  = *authority;
			DeviceRefreshGeneration = generation;
			++DeviceRefreshBindings;
			DeviceHost->bind_refresh_authority(*authority, generation);
			return;
		}
		// The restart. Nothing about this run says a device flow happened,
		// so the question goes to the host that owns the vault: is there
		// material beside this credential that renews it? Only a device
		// grant leaves any, so an affirmative answer is the enrollment
		// evidence the stage cannot supply after a restart.
		if (!Host->accepted_credential_renewable()) {
			// A pasted token, or an answer the vault has not given yet.
			// Nothing is recorded either way, so the republication that
			// follows a settled answer is still free to act.
			return;
		}
		DeviceRefreshAuthority  = *authority;
		DeviceRefreshGeneration = generation;
		++DeviceRefreshResumes;
		DeviceHost->resume_refresh(*authority, generation, Host->accepted_device_lifetime());
	}

	void github_profile_runtime::record_cli_account_use()
	{
		auto use = std::optional<cli_account_use>{ };
		if (!Closing && Host != nullptr && Host->access_mode() == core::github_access_mode::authenticated) {
			if (const auto& session = Host->accepted_session(); session.has_value()) {
				const auto profile = std::ranges::find_if(Profiles, [&](const auto& aProfile) { return aProfile.profile() == session->Profile; });
				if (profile != Profiles.end() && !profile->cli_account_login().empty()) {
					use = cli_account_use{ .Profile = Admission.Label, .Hostname = std::string{ profile->hostname() }, .Login = std::string{ profile->cli_account_login() } };
				}
			}
		}
		cli_account_users().set(this, std::move(use));
	}

	auto github_profile_runtime::cli_read_offered_here() const noexcept -> bool
	{
#if defined(BPR_BUILD_TESTING)
	#if defined(BPR_GITHUB_CLI_PLATFORM)
		return GithubCliDiscoveryAccounts.has_value()
			|| cli_read_offered(InsideAutomatedTestBoundary);
	#else
		return false;
	#endif
#else
		return cli_read_offered(false);
#endif
	}
}
