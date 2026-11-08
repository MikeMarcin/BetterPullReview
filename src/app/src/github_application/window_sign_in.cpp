#include "github_application/window_composition.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/remembered_connection_host.hpp"
#include "bpr/application/review_queue_facts.hpp"
#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_transition.hpp"
#include "bpr/core/remembered_connection.hpp"
#include "bpr/core/review_queue_attention.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/server_card.hpp"
#include "github_application/cli_accounts.hpp"
#include "github_application/credential_prompt.hpp"
#include "github_application/profile_hosts.hpp"
#include "github_application/queue_notifications.hpp"
#include "github_cli_process.hpp"
#include "github_cli_process_runner.hpp"
#include "github_cli_runtime.hpp"
#include "github_insights.hpp"
#include "github_provider.hpp"
#include "terminal_diagnostics.hpp"

#include <QByteArray>
#include <QByteArrayView>
#include <QDesktopServices>
#include <QTextStream>

#include <algorithm>
#include <deque>
#include <limits>
#include <stdexcept>
#include <variant>

#if defined(BPR_BUILD_TESTING)
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
#endif

namespace bpr::app {
	void github_application::implementation::accept(application::accepted_pull_request_handoff aHandoff)
	{
		if (!Closing && EpochHost != nullptr) {
			const auto destination = core::window_review_destination{ aHandoff.Mode, std::string{ aHandoff.Hostname.value() }, aHandoff.Locator, aHandoff.Number, aHandoff.Account };
			const auto opened      = ReadingDestination != destination;
			ReadingDestination     = destination;
			adopt_tab_destination(destination);
			// [[BPR-1365]]. Recently viewed says which of its entries is open.
			if (opened) {
				publish_recent();
			}
			AgentReview = agent_review_facts{
				.Host        = aHandoff.Host,
				.Account     = aHandoff.Account,
				.Generation  = aHandoff.IdentityGeneration,
				.Repository  = aHandoff.Repository,
				.PullRequest = aHandoff.PullRequest,
				.Number      = aHandoff.Number,
				.Hostname    = std::string{ aHandoff.Hostname.value() },
				.Locator     = aHandoff.Locator,
				.Sparse      = application::object_loading_for(aHandoff.Shape) == application::pull_request_object_loading::selected_objects && aHandoff.SmartGit
					&& !aHandoff.SmartGit->revoked() && aHandoff.SmartGit->supports_sparse_commit_trees(),
			};
#if defined(BPR_BUILD_TESTING)
			// [[BPR-1879]]. The attended Repository-step look: the fixture
			// review's agent setup reads as a sparse one, where the choice is.
			if (SparseAgentSetupLook) {
				AgentReview->Sparse = true;
			}
#endif
			// [[BPR-1879]]. GitHub's `size` is in kibibytes.
			if (Host != nullptr) {
				if (const auto read = WindowRead->accepted_snapshot(); read && read->Repository.SizeKilobytes && *read->Repository.SizeKilobytes <= (std::numeric_limits<std::uint64_t>::max() / 1024U)) {
					AgentReview->ApproximateBytes = *read->Repository.SizeKilobytes * 1024U;
				}
			}
#if defined(BPR_AGENT_INTEGRATIONS)
			observe_clone_repository();
			observe_conversation();
#endif
			const auto authority = workbench_authority(aHandoff);
			const auto snapshot  = Host != nullptr
				? WindowRead->accepted_snapshot()
				: std::optional<application::github_pull_request_snapshot>{ };
			compose_review_ledger(aHandoff);
			WorkbenchReview.adopt(
				aHandoff,
				snapshot);
			// RFC-BPR-0089. Before the workbench observes the files below.
			adopt_commit_filter(aHandoff, snapshot);
			if (snapshot.has_value() && QueueHost != nullptr
				&& QueueRefresh != nullptr) {
				auto observation = queue_observation_for(aHandoff, *snapshot);
				auto session     = application::queue_session_id{
					.Host          = observation.Ref.Host,
					.Account       = observation.Ref.Account,
					.Generation    = aHandoff.IdentityGeneration.value(),
					.ViewerAccount = aHandoff.Account,
				};
				auto activity    = core::queue_activity_observation{ };
				activity.Waiting = application::queue_waiting_for(*snapshot);
				if (snapshot->PullRequest.Author) {
					activity.Authored = snapshot->PullRequest.Author->Actor == snapshot->Viewer.Actor;
				}
				activity.ReviewersAwaited = activity.Waiting.Requests.value_or(0);
				activity.FactRevision     = QueueFactRevision;
				// Opening a PR does not review it. Its accepted review remains
				// the comparison baseline even on this process's first read.
				activity.ReviewedByMe           = aHandoff.CurrentViewerReview.has_value();
				activity.HeadMovedSinceMyReview = activity.ReviewedByMe
					&& aHandoff.CurrentViewerReview->Commit != aHandoff.Head;
				const auto sameBaseline = QueueActivityBaseline.has_value()
					&& QueueActivityBaseline->Session == session
					&& QueueActivityBaseline->Ref == observation.Ref;
				if (!sameBaseline) {
					QueueActivityBaseline = queue_activity_baseline{
						.Session = session,
						.Ref     = observation.Ref,
					};
					ActiveQueueNotifications.clear();
				}
				// [[BPR-0659]]. The unseen count is the ledger's read state
				// of this pull request's threads, composed a moment ago.
				if (const auto observed = observe_read_state(); observed.has_value()) {
					activity.UnseenComments = observed->UnreadPosts;
				}
				QueueActivity = queue_activity_fact{
					.Session     = session,
					.Ref         = observation.Ref,
					.Activity    = activity,
					.Observation = observation,
				};
				QueueHost->adopt(session);
				QueueRefresh->retain(
					std::move(session),
					observation);
				QueueHost->refresh(
					observation.Ref,
					activity);
			}
			load_defects();
			LastAcceptedHandoff = clone_without_smart_git(aHandoff);
			// [[BPR-1527]]. This observation retires every workbench
			// selection. A tab driver that already selected its file is
			// waiting for retired material and cannot select it again.
			// Let the accepted comparison and returned files restart that
			// active view, rather than leave it blocking the owed refresh.
			abandon_tab_load();
			// [[BPR-1303]]. A refresh that moves the head changes the
			// tab's comparison, and that reloads the tab; one that keeps
			// the diff leaves the workbench with no file and is owed it.
			RefreshKeptTabView = preserves_selected_diff(EpochHost->state(), aHandoff);
			if (!RefreshKeptTabView) {
				clear_diff_selection();
			}
			if (WorkbenchHost != nullptr) {
				WorkbenchHost->observe(authority);
			}
			compose_review_mutation(aHandoff);
			compose_review_submission(aHandoff);
			compose_review_tour(aHandoff);
			EpochHost->accept(std::move(aHandoff));
			// [[BPR-1212]]. The accept stops queued epoch work. When it keeps
			// the comparison and its base numbering is neither known nor
			// asked, ask now: no new publication need follow.
			if (RefreshKeptTabView && !BaseNumbering) {
				BaseNumberingCancellations = 0U;
				observe_base_numbering();
			}
		}
	}

	void github_application::implementation::invalidate(
		application::pull_request_authority_invalidation aInvalidation)
	{
		if (Closing || EpochHost == nullptr) {
			return;
		}
		if (ReadingRestoration && RestorationReadAccepted) {
			finish_reading_restoration(false);
		}
		if (PrFind) {
			PrFind->reset();
			SearchComparisonGesture = false;
			SearchOpenFile.reset();
			SearchCreatedTab.reset();
			SetAsideOpenFile.reset();
			SearchCreatedComparison.reset();
		}
		if (PaneFind) {
			PaneFind->reset();
		}
		const auto closed = ReadingDestination.has_value();
		ReadingDestination.reset();
		AcceptedReadingPosition.reset();
		AgentReview.reset();
#if defined(BPR_AGENT_INTEGRATIONS)
		observe_clone_repository();
		observe_conversation();
#endif
		// [[BPR-1365]]. No recent entry is open any more.
		if (closed) {
			publish_recent();
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		if (AgentHost != nullptr) {
			AgentHost->clear();
		}
		if (TourSession != nullptr) {
			TourSession->clear();
		}
#endif
		if (Window != nullptr) {
			Window->clear_dif_context();
		}
		cancel_preparation();
		const auto retiresLedger = matches_review_ledger(aInvalidation);
		if (CommitFilterReview.has_value() && std::get<0>(*CommitFilterReview) == aInvalidation.Host
			&& std::get<1>(*CommitFilterReview) == aInvalidation.Account && std::get<2>(*CommitFilterReview) == aInvalidation.Repository
			&& std::get<3>(*CommitFilterReview) == aInvalidation.PullRequest) {
			CommitFilter.clear();
			CommitFilterWanted = false;
			CommitFilterReview.reset();
			CommitFilterDestination.reset();
			publish_commit_filter();
		}
		if (auto authority = WorkbenchReview.invalidate(aInvalidation);
			authority.has_value() && WorkbenchHost != nullptr) {
#if defined(BPR_BUILD_TESTING)
			++WorkbenchInvalidations;
#endif
			WorkbenchHost->invalidate(*authority);
			// A pending first choice has no tab comparison to bind it.
			// Only this exact revoked authority retires that ephemeral open.
			if (TabHost && !TabHost->state().Active && TabHost->state().Chosen) {
				TabHost->reset();
			}
		}
		invalidate_diff_if([&aInvalidation](const auto& aAuthority) {
			return aAuthority.Profile == aInvalidation.Profile
				&& aAuthority.ProfileRevision == aInvalidation.ProfileRevision
				&& aAuthority.Host == aInvalidation.Host
				&& aAuthority.HostAuthority == aInvalidation.HostAuthority
				&& aAuthority.Account == aInvalidation.Account
				&& aAuthority.Viewer == aInvalidation.Viewer
				&& aAuthority.Repository == aInvalidation.Repository
				&& aAuthority.PullRequest == aInvalidation.PullRequest;
		});
		if (ReviewSubmission) {
			ReviewSubmission->invalidate(aInvalidation);
			if (TourPublication) {
				TourPublication->invalidate();
				if (ConversationReply) {
					ConversationReply->invalidate();
				}
			}
		}
		if (TourOwner && TourOwner->Host == aInvalidation.Host && TourOwner->Account == aInvalidation.Account && TourOwner->Repository == aInvalidation.Repository && TourOwner->PullRequest == aInvalidation.PullRequest) {
			suspend_review_tour();
		}
		if (retiresLedger) {
			PendingUnsentReplyCommands.reset();
			retire_review_ledger();
			core::retire_conversation_mutations(ConversationMutations, ConversationOverlay);
			ConversationMutationBinding.reset();
			retire_review_mutation_host(core::conversation_mutation_retirement::cancel);
			if (SharedDrafts) {
				SharedDrafts->detach(DraftConsumer);
				SharedDrafts.reset();
			}
			present_saved_drafts();
			ReviewDraft.reset();
			ReviewDraftRevision = 0U;
			ReviewProjection.clear();
			publish_preflight_selection();
		}
		EpochHost->invalidate(std::move(aInvalidation));
	}

	auto github_application::implementation::find_profile(const core::host_profile_id aProfile) const
		-> const adapters::github::validated_github_host_profile*
	{
		const auto found = std::ranges::find_if(
			Profiles,
			[aProfile](const adapters::github::validated_github_host_profile& aCandidate) { return aCandidate.profile() == aProfile; });
		return found == Profiles.end() ? nullptr : &*found;
	}

	auto github_application::implementation::native_sign_in_profile(core::host_profile_id aProfile) -> core::host_profile_id
	{
		const auto* old = find_profile(aProfile);
		if (!old || old->authentication_transport() != application::github_authentication_transport::native_https) {
			return aProfile;
		}
		if (aProfile.value() > Profile->Connections->state().LastProfileId) {
			// [[BPR-1494]]. A sign-in re-bound to its account's earlier
			// profile leaves the profile it began on unremembered. A later
			// sign-in that begins there takes the device capability back, as
			// a freshly minted profile does below.
			const auto* registered = find_profile(Profile->DeviceCapabilityProfile);
			if (Profile->DeviceCapabilityRegistered && registered && DeviceHost && registered->profile() != aProfile
				&& registered->authentication_transport() == application::github_authentication_transport::native_https
				&& registered->hostname() == old->hostname() && registered->server_contract() == old->server_contract()
				&& DeviceHost->replace_profile(Profile->DeviceCapabilityProfile, aProfile)) {
				Profile->DeviceCapabilityProfile = aProfile;
			}
			return aProfile;
		}
		auto profileHigh = Profile->Connections->state().LastProfileId;
		auto hostHigh    = Profile->Connections->state().LastHostId;
		for (const auto& candidate : Profiles) {
			profileHigh = std::max(profileHigh, candidate.profile().value());
			hostHigh    = std::max(hostHigh, candidate.host().value());
		}
		if (profileHigh == std::numeric_limits<std::uint64_t>::max() || hostHigh == std::numeric_limits<std::uint64_t>::max()) {
			return { };
		}
		const auto& rows      = Profile->Connections->state().Connections;
		const auto  row       = std::ranges::find(rows, old->hostname(), &core::remembered_connection::Hostname);
		const auto  host      = row != rows.end() && row->Host.value() != 0U ? row->Host : old->host().value() > Profile->Connections->state().LastHostId ? old->host()
																																						  : core::git_host_id{ ++hostHigh };
		const auto  candidate = adapters::github::validate_github_host_profile(native_profile_candidate(core::host_profile_id{ ++profileHigh }, host, application::github_profile_revision{ 1U }, old->server_contract(), old->authentication_transport(), std::string{ old->hostname() }, old->provenance()));
		if (!candidate || !ProfileCatalog->register_profile(*candidate)) {
			return { };
		}
		const auto* registered = find_profile(Profile->DeviceCapabilityProfile);
		if (Profile->DeviceCapabilityRegistered && registered && DeviceHost
			&& registered->hostname() == candidate->hostname() && registered->server_contract() == candidate->server_contract()
			&& candidate->authentication_transport() == application::github_authentication_transport::native_https
			&& DeviceHost->replace_profile(Profile->DeviceCapabilityProfile, candidate->profile())) {
			Profile->DeviceCapabilityProfile = candidate->profile();
		}
		Profiles.push_back(*candidate);
		auto option = profile_option(*candidate, std::string{ candidate->hostname() } + " — Native HTTPS");
		ProfileOptions.push_back(option);
		Profile->for_each_window([&](const auto& aObserver) { if (aObserver.SetupProfile) { aObserver.SetupProfile(option); } });
		return candidate->profile();
	}

	auto github_application::implementation::owner() const -> core::setup_owner_id { return SetupOwner; }

	auto github_application::implementation::profiles() const -> std::vector<application::setup_profile_observation>
	{
		auto       result  = std::vector<application::setup_profile_observation>{ };
		const auto session = Host ? Host->accepted_session() : std::nullopt;
		for (const auto& profile : Profiles) {
			const auto authenticated = session && session->Mode == core::github_access_mode::authenticated
				&& session->Profile == profile.profile() && session->Host == profile.host();
			const auto remembered = Profile->selected_connection(profile.hostname());
			// [[BPR-0732]]. Preview is the composition's anonymous authority
			// on the GitHub.com contract; the host refuses GHES by policy,
			// so a GHES profile with no account keeps the sign-in stop.
			result.push_back({ .Profile             = profile.profile(),
							   .Hostname            = std::string{ profile.hostname() },
							   .Authenticated       = authenticated,
							   .PreviewAvailable    = PublicPreviewComposed && profile.authentication_transport() == application::github_authentication_transport::native_https && profile.server_contract() == application::github_server_contract::github_com_2022_11_28,
							   .RevalidationAllowed = remembered && remembered->Profile == profile.profile() && profile.authentication_transport() == application::github_authentication_transport::github_cli });
		}
		std::stable_partition(result.begin(), result.end(), [this](const auto& aRow) { const auto chosen = Profile->selected_connection(aRow.Hostname); return chosen && chosen->Profile == aRow.Profile; });
		return result;
	}

	auto github_application::implementation::shorthand_hostname() const -> std::string
	{
		const auto  selected = Host && Host->state().ActiveProfile ? Host->state().ActiveProfile : Window->session_panel()->selected_profile();
		const auto* profile  = selected ? find_profile(*selected) : nullptr;
		return profile ? std::string{ profile->hostname() } : std::string{ };
	}

	void github_application::implementation::present(const application::setup_destination_projection& aProjection)
	{
		if (Window && !Closing) {
			Window->present_setup_destination(aProjection);
		}
	}

	void github_application::implementation::configure(const core::pending_setup_destination& aPending)
	{
		if (!QueueHost || aPending.Owner != SetupOwner) {
			return;
		}
		SetupConfigurationRequest = aPending.Request;
		if (SetupQueueReady && !SetupConfigurationSubmitted) {
			SetupConfigurationSubmitted = true;
			QueueHost->apply(application::saved_server_intent{ .Action = application::saved_server_action::add, .Server = { .Hostname = aPending.Destination.Hostname, .Account = { } } });
		}
	}

	/// [[BPR-1373]]. The GitHub CLI profile pinned to one gh account on one
	/// host, registered the first time gh reports that account signed in
	/// with a keychain token. Returns null if it cannot be registered.
	auto github_application::implementation::register_discovered_cli_account(
		const adapters::github::detail::gh_cli_inventory_account& aAccount)
		-> const adapters::github::validated_github_host_profile*
	{
		const auto existing = std::ranges::find_if(
			Profiles,
			[&](const adapters::github::validated_github_host_profile& aProfile) {
				return aProfile.hostname() == aAccount.Hostname
					&& aProfile.authentication_transport() == application::github_authentication_transport::github_cli
					&& aProfile.cli_account_login() == aAccount.Login && aProfile.cli_account().value() == 0U
					&& aProfile.profile().value() > Profile->Connections->state().LastProfileId;
			});
		if (existing != Profiles.end()) {
			return &*existing;
		}
		if (!adapters::github::valid_cli_account_login(aAccount.Login)) {
			return nullptr;
		}

		auto nextProfile = Profile->Connections->state().LastProfileId + 1U;
		auto nextHost    = Profile->Connections->state().LastHostId + 1U;
		auto host        = std::optional<core::git_host_id>{ };
		for (const auto& profile : Profiles) {
			if (profile.profile().value() == std::numeric_limits<std::uint64_t>::max()
				|| profile.host().value() == std::numeric_limits<std::uint64_t>::max()) {
				return nullptr;
			}
			nextProfile = std::max(nextProfile, profile.profile().value() + 1U);
			nextHost    = std::max(nextHost, profile.host().value() + 1U);
			if (profile.hostname() == aAccount.Hostname && (profile.host().value() > Profile->Connections->state().LastHostId || std::ranges::any_of(Profile->Connections->state().Connections, [&](const auto& aRow) { return aRow.Hostname == aAccount.Hostname && aRow.Host.value() != 0U; }))) {
				host = profile.host();
			}
		}

		const auto githubCom = aAccount.Hostname == "github.com";
		auto       candidate = native_profile_candidate(
			core::host_profile_id{ nextProfile },
			host.value_or(core::git_host_id{ nextHost }),
			application::github_profile_revision{ 1U },
			githubCom
				? application::github_server_contract::github_com_2022_11_28
				: application::github_server_contract::ghes_3_18_2022_11_28,
			application::github_authentication_transport::github_cli,
			aAccount.Hostname,
			adapters::github::github_profile_provenance::explicit_user);
		candidate.CliAccountLogin = aAccount.Login;
		const auto validated      = adapters::github::validate_github_host_profile(candidate);
		if (!validated || !ProfileCatalog
			|| !ProfileCatalog->register_profile(*validated)) {
			return nullptr;
		}

		Profiles.push_back(*validated);
		auto option = profile_option(
			*validated,
			(githubCom ? std::string{ "GitHub.com" }
					   : std::string{ "GHES 3.18" })
				+ " — GitHub CLI (" + aAccount.Login + (aAccount.Active ? ", active in gh)" : ")"));
		ProfileOptions.push_back(option);
		Profile->for_each_window([&](const github_profile_runtime::window_observer& aObserver) {
			if (aObserver.SetupProfile) {
				aObserver.SetupProfile(option);
			}
		});
		return &Profiles.back();
	}

	/// [[BPR-1373]]. gh's accounts by hostname, as Accounts & hosts lists
	/// them: each with the profile pinned to it and who in this run uses it.
	void github_application::implementation::present_github_cli_accounts()
	{
		if (Window == nullptr || !Profile->GithubCliInventoryChecked) {
			return;
		}
		auto listed = std::map<std::string, std::vector<qt::server_card_cli_account>, std::less<>>{ };
		for (const auto& account : Profile->GithubCliInventory) {
			const adapters::github::validated_github_host_profile* pinned = nullptr;
			for (const auto& candidate : Profiles) {
				if (candidate.hostname() == account.Hostname && candidate.authentication_transport() == application::github_authentication_transport::github_cli && candidate.cli_account_login() == account.Login) {
					pinned = &candidate;
				}
			}
			const auto state = !account.SignedIn ? qt::server_card_cli_account_state::signed_out
				: !account.KeychainHeld          ? qt::server_card_cli_account_state::file_token
												 : qt::server_card_cli_account_state::signed_in;
			listed[account.Hostname].push_back(qt::server_card_cli_account{
				.Login      = account.Login,
				.ActiveInGh = account.Active,
				.State      = state,
				.Profile    = pinned ? std::optional{ pinned->profile() } : std::nullopt,
				.UsedBy     = cli_account_users().users(account.Hostname, account.Login),
			});
		}
		// A pinned profile whose account gh no longer lists is signed out
		// in gh; it stays listed so its card can say so.
		for (const auto& profile : Profiles) {
			if (profile.cli_account_login().empty()) {
				continue;
			}
			auto& host = listed[std::string{ profile.hostname() }];
			if (std::ranges::none_of(host, [&](const qt::server_card_cli_account& aAccount) { return aAccount.Login == profile.cli_account_login(); })) {
				host.push_back(qt::server_card_cli_account{
					.Login   = std::string{ profile.cli_account_login() },
					.State   = qt::server_card_cli_account_state::signed_out,
					.Profile = profile.profile(),
					.UsedBy  = cli_account_users().users(profile.hostname(), profile.cli_account_login()),
				});
			}
		}
		Window->present_cli_accounts(std::move(listed));
	}

	/// [[BPR-1373]]. A refused session on a pinned profile: gh may have
	/// signed its account out and fallen back to the active one. Checked
	/// once per refusal, so the card can say which.
	void github_application::implementation::observe_cli_session(const application::github_presentation_update& aUpdate)
	{
		if (!aUpdate.Transition || !aUpdate.Transition->Status) {
			return;
		}
		present_github_cli_accounts();
		const auto status = aUpdate.Transition->Status->Status;
		if (status == core::github_status::session_ready) {
			GithubCliRecheckedFor.reset();
			return;
		}
		const auto  failed  = Host ? Host->state().ActiveProfile : std::nullopt;
		const auto* profile = failed ? find_profile(*failed) : nullptr;
		if (status == core::github_status::session_failed && profile != nullptr && !profile->cli_account_login().empty()
			&& GithubCliRecheckedFor != failed && Profile->cli_read_offered_here()) {
			GithubCliRecheckedFor = failed;
			// Strict acquisition refusal never starts ambient account verification.
		}
	}

	/// \brief Asks gh for its accounts. [[BPR-1373]]: the full inventory.
	/// \details From Use GitHub CLI, one signed-in account for the host
	/// asked about connects at once; Check gh again (\p aConnectSingle
	/// false) only lists.
	void github_application::implementation::discover_github_cli_accounts(const bool aConnectSingle)
	{
#if defined(BPR_GITHUB_CLI_PLATFORM)
		if (Closing || GithubCliDiscoveryPending || Window == nullptr || Host == nullptr
			|| !core::github_connection_setup_available(Host->state()) || !Profile->cli_read_offered_here()) {
			return;
		}
		GithubCliDiscoveryPending      = true;
		const auto discoveryGeneration = ++GithubCliDiscoveryGeneration;
		const auto requestedHost       = SetupDestination && SetupDestination->pending() ? SetupDestination->pending()->Destination.Hostname : std::string{ };
		Window->present_github_cli_discovery(
			qt::github_cli_discovery_state::discovering);

		auto        weak            = std::weak_ptr<const int>{ WindowLifetime };
		auto* const lane            = &DecisionLane;
		auto        fixtureAccounts = std::optional<std::vector<
			adapters::github::detail::gh_cli_inventory_account>>{ };
	#if defined(BPR_BUILD_TESTING)
		if (Profile->GithubCliInventoryChecks) {
			++*Profile->GithubCliInventoryChecks;
		}
		if (Profile->GithubCliDiscoveryAccounts.has_value()) {
			fixtureAccounts.emplace();
			fixtureAccounts->reserve(Profile->GithubCliDiscoveryAccounts->size());
			for (const auto& account : *Profile->GithubCliDiscoveryAccounts) {
				fixtureAccounts->push_back({ .Hostname = account.Hostname, .Login = account.Login, .Active = account.Active, .SignedIn = account.SignedIn, .KeychainHeld = account.KeychainHeld });
			}
		}
	#endif
		try {
			ProviderEffects.post([this, weak, lane, discoveryGeneration, requestedHost, aConnectSingle, fixtureResult = std::move(fixtureAccounts)] {
				auto accounts = std::optional<std::vector<
					adapters::github::detail::gh_cli_inventory_account>>{ };
				auto failure  = qt::github_cli_discovery_state::process_failed;
				// Reuse the bounded terminal sink. Never send child output, arguments,
				// inventory JSON or environment values to diagnostics or presentation.
				const auto probe = [&failure, log = DiagnosticLog](adapters::github::detail::github_cli_process_port&   aProcess,
																   adapters::github::detail::github_cli_process_request aRequest, const char* aStage) {
					using adapters::github::detail::github_cli_process_failure;
					aRequest.Observation = std::make_shared<adapters::github::detail::github_cli_process_observation>();
					const auto  started  = std::chrono::steady_clock::now();
					auto        result   = aProcess.run(aRequest, std::stop_token{ });
					const auto  elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
					const char* category = "completed";
					if (!result) {
						switch (result.error()) {
						case github_cli_process_failure::timeout:
							category = "timeout";
							failure  = qt::github_cli_discovery_state::timed_out;
							break;
						case github_cli_process_failure::start_failure:
							category = "start_failure";
							failure  = qt::github_cli_discovery_state::launch_failed;
							break;
						case github_cli_process_failure::nonzero_exit:
							category = "nonzero_exit";
							failure  = qt::github_cli_discovery_state::exited_unsuccessfully;
							break;
						case github_cli_process_failure::standard_output_limit:
							category = "stdout_limit";
							failure  = qt::github_cli_discovery_state::output_rejected;
							break;
						case github_cli_process_failure::standard_error_limit:
							category = "stderr_limit";
							failure  = qt::github_cli_discovery_state::output_rejected;
							break;
						case github_cli_process_failure::unexpected_standard_error:
							category = "unexpected_stderr";
							failure  = qt::github_cli_discovery_state::output_rejected;
							break;
						case github_cli_process_failure::invalid_request: category = "invalid_request"; break;
						case github_cli_process_failure::write_failure:   category = "stdin_failure"; break;
						case github_cli_process_failure::cancelled:       category = "cancelled"; break;
						case github_cli_process_failure::process_crash:   category = "crash"; break;
						case github_cli_process_failure::reap_failure:    category = "reap_failure"; break;
						}
					} else if (result->ExitCode != 0 && result->ExitCode != 1) {
						failure = qt::github_cli_discovery_state::exited_unsuccessfully;
					}
					const auto summaryText = QStringLiteral("GH_DISCOVERY stage=%1 category=%2 elapsed_ms=%3 exit=%4 stdout_bytes=%5 stderr_bytes=%6")
												 .arg(QString::fromLatin1(aStage), QString::fromLatin1(category))
												 .arg(elapsed)
												 .arg(aRequest.Observation->ExitCode.load())
												 .arg(static_cast<qulonglong>(aRequest.Observation->StandardOutputBytes.load()))
												 .arg(static_cast<qulonglong>(aRequest.Observation->StandardErrorBytes.load()))
												 .toLatin1();
					if (log) {
						auto event          = application::diagnostic_event{ };
						event.Outcome       = result ? application::diagnostic_outcome::succeeded : application::diagnostic_outcome::failed;
						event.Summary.State = application::diagnostic_summary_state::sanitized;
						event.Summary.Size  = static_cast<std::uint16_t>(std::min<std::size_t>(static_cast<std::size_t>(summaryText.size()), event.Summary.Text.size()));
						std::copy_n(summaryText.constData(), event.Summary.Size, event.Summary.Text.begin());
						static_cast<void>(log->record(event));
					}
					bpr::app::detail::terminal_text_stream{ } << summaryText << "\n";
					return result;
				};
				try {
					if (fixtureResult.has_value()) {
						accounts = std::move(fixtureResult);
					} else {
						auto configuration = live_gh_cli_configuration();
						auto process       = adapters::github::detail::make_github_cli_process_runner();
						if (process && Profile->InsightsRecorder) {
							auto channel = std::make_shared<adapters::github::detail::github_insights_channel>(Profile->InsightsRecorder, 0);
							channel->bind(0);
							process->set_insights_channel(std::move(channel));
						}
						if (!configuration) {
							const auto available = gh_cli_executable_available();
							if (!available) {
								failure = qt::github_cli_discovery_state::missing_executable;
							}
							bpr::app::detail::terminal_text_stream{ } << "GH_DISCOVERY stage=configuration category=" << (!available ? "missing_executable" : "invalid_configuration") << "\n";
						}
						if (configuration && process) {
							const auto version = probe(*process, adapters::github::detail::compile_gh_cli_version_request(*configuration), "version");
							if (version && version->ExitCode != 0) {
								failure = qt::github_cli_discovery_state::exited_unsuccessfully;
							} else if (version && !adapters::github::detail::supported_gh_cli_auth_status_version(version->StandardOutput)) {
								failure = qt::github_cli_discovery_state::unsupported_version;
							} else if (version) {
								// gh exits 1 when any account fails its check; the
								// JSON still says which, so it is read either way.
								const auto status = probe(*process,
														  adapters::github::detail::compile_gh_cli_auth_inventory_request(*configuration), "inventory");
								if (status && (status->ExitCode == 0 || status->ExitCode == 1)) {
									auto parsed = adapters::github::detail::parse_gh_cli_auth_inventory(
										status->StandardOutput);
									if (parsed) {
										accounts = std::move(*parsed);
									} else {
										failure = qt::github_cli_discovery_state::malformed_response;
									}
								}
							}
						}
					}
				} catch (...) { }

				try {
					lane->post([this, weak, discoveryGeneration, requestedHost, aConnectSingle, discovered = std::move(accounts), failure]() mutable {
						if (weak.expired() || Closing || Window == nullptr || GithubCliDiscoveryGeneration != discoveryGeneration) {
							return;
						}
						if (!core::github_connection_setup_available(Host->state())) {
							retire_cli_discovery();
							return;
						}
						GithubCliDiscoveryPending = false;
						if (!discovered.has_value()) {
							Window->present_github_cli_discovery(failure);
							return;
						}
						auto choices = std::vector<core::host_profile_id>{ };
						for (const auto& account : *discovered) {
							if (!account.SignedIn || !account.KeychainHeld) {
								continue;
							}
							const auto* const registered = register_discovered_cli_account(account);
							if (registered == nullptr) {
								Window->present_github_cli_discovery(
									qt::github_cli_discovery_state::malformed_response);
								return;
							}
							if (requestedHost.empty() || account.Hostname == requestedHost) {
								choices.push_back(registered->profile());
							}
						}
						const auto hadAccounts = !discovered->empty();
						bpr::app::detail::terminal_text_stream{ } << "GH_DISCOVERY stage=selection eligible=" << static_cast<qulonglong>(choices.size()) << " total=" << static_cast<qulonglong>(discovered->size()) << "\n";
						Profile->GithubCliInventory        = std::move(*discovered);
						Profile->GithubCliInventoryChecked = true;
						present_github_cli_accounts();
						Window->present_cli_account_choices(choices);
						Window->present_github_cli_discovery(
							choices.empty()
								? (hadAccounts ? qt::github_cli_discovery_state::ineligible_accounts : qt::github_cli_discovery_state::empty)
								: qt::github_cli_discovery_state::ready,
							choices.size());
						if (aConnectSingle && choices.size() == 1U && GithubCliDiscoveryGeneration == discoveryGeneration) {
							Window->connect_github_profile(choices.front());
						}
					});
				} catch (...) { }
			});
		} catch (...) {
			GithubCliDiscoveryPending = false;
			Window->present_github_cli_discovery(
				qt::github_cli_discovery_state::process_failed);
		}
#else
		static_cast<void>(aConnectSingle);
#endif
	}

	void github_application::implementation::synchronize_setup_servers(const application::queue_landing_projection& aProjection)
	{
		if (!ProfileCatalog || !SetupDestination) {
			return;
		}
		SetupQueueReady = aProjection.Status == application::queue_landing_status::ready;
		if (SetupConfigurationRequest && aProjection.Failure) {
			SetupConfigurationRequest.reset();
			SetupConfigurationSubmitted = false;
			present({ .Pending = SetupDestination->pending(), .Notice = "The server could not be saved. Retry Add server and continue.", .Opened = false });
			return;
		}

		for (const auto& server : aProjection.SavedServers) {
			if (std::ranges::any_of(Profiles, [&](const auto& aProfile) {
					return aProfile.hostname() == server.Hostname
						&& aProfile.authentication_transport()
						== application::github_authentication_transport::native_https;
				})) {
				continue;
			}
			auto nextProfile = Profile->Connections->state().LastProfileId + 1U;
			auto nextHost    = Profile->Connections->state().LastHostId + 1U;
			auto host        = std::optional<core::git_host_id>{ };
			for (const auto& profile : Profiles) {
				nextProfile = std::max(nextProfile, profile.profile().value() + 1U);
				nextHost    = std::max(nextHost, profile.host().value() + 1U);
				if (profile.hostname() == server.Hostname) {
					host = profile.host();
				}
			}
			const auto candidate = adapters::github::validate_github_host_profile(native_profile_candidate(core::host_profile_id{ nextProfile }, host.value_or(core::git_host_id{ nextHost }), application::github_profile_revision{ 1U }, application::github_server_contract::ghes_3_18_2022_11_28, application::github_authentication_transport::native_https, server.Hostname, adapters::github::github_profile_provenance::explicit_user));
			if (!candidate || !ProfileCatalog->register_profile(*candidate)) {
				continue;
			}
			Profiles.push_back(*candidate);
			auto option = profile_option(*candidate, "GHES 3.18 — " + server.Hostname);
			ProfileOptions.push_back(std::move(option));
		}
		for (const auto& option : ProfileOptions) {
			Window->add_setup_profile(option);
		}
		if (const auto pending = SetupDestination->pending(); pending && pending->Stage == core::setup_destination_stage::add_server) {
			SetupDestination->configured(pending->Request);
			if (SetupDestination->pending() && SetupDestination->pending()->Stage != core::setup_destination_stage::add_server) {
				SetupConfigurationRequest.reset();
				SetupConfigurationSubmitted = false;
			} else if (SetupConfigurationRequest == pending->Request && SetupQueueReady && !SetupConfigurationSubmitted) {
				configure(*pending);
			}
		}
	}

	auto github_application::implementation::next_setup_owner() -> core::setup_owner_id
	{
		if (Profile->LastSetupOwner == std::numeric_limits<std::uint64_t>::max()) {
			throw std::runtime_error{ "setup identity exhausted" };
		}
		return core::setup_owner_id{ ++Profile->LastSetupOwner };
	}

	void github_application::implementation::retire_setup_effects()
	{
		// [[BPR-0987]]. A newer gesture or an account change retires the
		// location an earlier link was still waiting to place.
		PendingLinkLocation.reset();
		SetupConfigurationRequest.reset();
		SetupConfigurationSubmitted = false;
		SetupAcceptedGeneration.reset();
		const auto ownsSetup = Profile->SetupWindow == DraftWindow;
		if (ownsSetup) {
			Profile->SetupWindow.reset();
			Profile->SetupCredential = { };
			if (Host) {
				Host->cancel_setup_enrollment();
			}
		}
		const auto device = ownsSetup && SetupDeviceRequest.has_value();
		SetupDeviceRequest.reset();
		if (device && DeviceHost) {
			DeviceHost->dispatch(core::cancel_device_authorization_intent{ });
		}
	}

	void github_application::implementation::stage_setup_credential(const core::pending_setup_destination& aPending, application::github_credential_material aCredential,
																	const application::github_credential_provenance     aProvenance,
																	std::optional<application::device_binding_lifetime> aLifetime, const bool aSettled)
	{
		Profile->SetupWindow = DraftWindow;
		auto weak            = std::weak_ptr<const int>{ WindowLifetime };
		Host->stage_setup_enrollment(aPending, std::move(aCredential), [this, weak](const core::pending_setup_destination& aExpected, const bool aSuccess) {
			if (weak.expired()) { return false; }
			if (Closing || !Profile->current_window(DraftWindow) || Profile->SetupWindow != DraftWindow || !SetupDestination || !aExpected.Profile) { return false; }
			const auto accepted = core::accept_setup_destination(SetupDestination->pending(), aExpected.Request, SetupOwner, *aExpected.Profile, aExpected.Destination.Hostname);
			if (!accepted) { return false; }
			if (!aSuccess) {
				SetupDestination->cancel();
				present({.Pending=std::nullopt, .Notice="Sign-in did not complete. The current review is unchanged; retry the pull request link.", .Opened=false});
				return false;
			}
			SetupAcceptedGeneration = core::github_identity_generation{Host->state().IdentityGeneration.value() + 1U};
			SetupDestination->committed(aExpected.Request);
			return SetupDestination->pending() && SetupDestination->pending()->Request == aExpected.Request && SetupOwner == aExpected.Owner; }, aProvenance, std::move(aLifetime), aSettled);
	}

	void github_application::implementation::authenticate(const core::pending_setup_destination& aOriginal)
	{
		auto aPending = aOriginal;
		if (aPending.Profile) {
			const auto selected = native_sign_in_profile(*aPending.Profile);
			if (selected != *aPending.Profile) {
				if (!SetupDestination->begin_authentication(aPending.Request, selected)) {
					return;
				}
				aPending = *SetupDestination->pending();
			}
		}
		if (!aPending.Profile || !Host || !DeviceHost) {
			return;
		}
		const auto* profile = find_profile(*aPending.Profile);
		if (!profile) {
			return;
		}
		if (profile->authentication_transport() == application::github_authentication_transport::github_cli) {
			dispatch_intent(core::activate_host_profile_intent{ .Profile = *aPending.Profile });
			return;
		}
		if (profile->product_kind() == application::github_product_kind::github_com) {
			SetupDeviceRequest       = aPending.Request;
			Profile->SetupWindow     = DraftWindow;
			auto weak                = std::weak_ptr<const int>{ WindowLifetime };
			Profile->SetupCredential = [this, weak, aPending](core::host_profile_id aProfile, application::github_credential_material aCredential, std::optional<application::device_binding_lifetime> aLifetime, bool aSettled) {
				if (weak.expired() || Closing || !Profile->current_window(DraftWindow) || Profile->SetupWindow != DraftWindow || aPending.Profile != aProfile || SetupDeviceRequest != aPending.Request) {
					return;
				}
				stage_setup_credential(aPending, std::move(aCredential), application::github_credential_provenance::device_grant, aLifetime, aSettled);
			};
			Window->open_sign_in(aPending.Destination.Hostname);
			DeviceHost->dispatch(core::begin_device_authorization_intent{ .Profile = *aPending.Profile });
			SetupDeviceGeneration = DeviceHost->state().Generation;
			return;
		}
		authenticate_manual(aPending);
	}

	void github_application::implementation::authenticate_manual(core::pending_setup_destination aPending)
	{
		auto material = std::optional<application::github_credential_material>{ };
#if defined(BPR_BUILD_TESTING)
		if (Fixture) {
			material.emplace(Fixture->take_enrollment_credential());
		} else
#endif
		{
			material = prompt_for_credential(Window.get());
		}
		if (!material) {
			retire_setup_effects();
			SetupDestination->cancel();
			return;
		}
		if (SetupDestination->pending() && SetupDestination->pending()->Request == aPending.Request && SetupOwner == aPending.Owner) {
			stage_setup_credential(aPending, std::move(*material));
		}
	}

	// [[BPR-0732]]. The signed-out open from the Home bar. The intent is
	// not routed through `dispatch_intent`, which retires the pending
	// destination; the generation the activation will stamp is armed so
	// the identity transition it publishes is the expected one.
	void github_application::implementation::preview(const core::pending_setup_destination& aPending)
	{
		if (!aPending.Profile || !Host || !SetupDestination) {
			return;
		}
		const auto* profile = find_profile(*aPending.Profile);
		if (!profile) {
			return;
		}
		const auto& session = Host->accepted_session();
		if (session && session->Mode == core::github_access_mode::public_preview && session->Profile == *aPending.Profile) {
			SetupDestination->previewed(aPending.Request, profile->profile(), profile->hostname());
			return;
		}
		SetupAcceptedGeneration = core::github_identity_generation{ Host->state().IdentityGeneration.value() + 1U };
		if (DeviceHost) {
			DeviceHost->release_refresh_authority();
		}
		DeviceRefreshAuthority.reset();
		Host->activate_public_preview(*aPending.Profile);
		if (Host->public_preview_refusal() && SetupDestination->pending() && SetupDestination->pending()->Request == aPending.Request) {
			SetupAcceptedGeneration.reset();
			SetupDestination->cancel();
			present({ .Pending = std::nullopt, .Notice = "Reading without signing in is unavailable here. The requested link is retained; sign in to open it.", .Opened = false });
		}
	}

	auto github_application::implementation::open(const core::host_profile_id aProfile, const core::setup_destination& aDestination) -> application::setup_destination_opening
	{
		const auto& session = Host ? Host->accepted_session() : std::optional<application::github_session_read_model>{ };
		const auto* profile = find_profile(aProfile);
		// [[BPR-0732]]. A signed-out session opens too, for the exact
		// profile it was activated for; it still has no authority.
		const auto previewing = session && session->Mode == core::github_access_mode::public_preview && session->Profile == aProfile
			&& profile != nullptr && profile->hostname() == aDestination.Hostname;
		// Accepted sessions cover both native credentials and token-free CLI leases.
		const auto authenticated = session && session->Mode == core::github_access_mode::authenticated && session->Profile == aProfile
			&& profile != nullptr && session->Host == profile->host() && profile->hostname() == aDestination.Hostname;
		if (aDestination.Kind == core::setup_destination_kind::repository) {
			return authenticated ? application::setup_destination_opening::repository_prepared
								 : application::setup_destination_opening::unavailable;
		}
		PendingLinkLocation.reset();
		if (!previewing && !authenticated) {
			return application::setup_destination_opening::reading;
		}
		// [[BPR-0987]]. The link opens here and goes to the location once
		// this window accepts it. [[BPR-1475]]. That holds for the pull
		// request this window already shows too: it is read fresh (owner,
		// 2026-10-13), not reused.
		if (aDestination.Target.Kind != core::pull_request_location_kind::pull_request) {
			PendingLinkLocation = link_location{ .Destination = aDestination };
		}
		WindowRead->dispatch(core::open_pull_request_intent{ .Repository = aDestination.Repository, .Number = aDestination.Number });
		return application::setup_destination_opening::reading;
	}

	void github_application::implementation::observe_setup_session(const application::github_presentation_update& aUpdate)
	{
		if (!SetupDestination || !SetupDestination->pending() || !Host) {
			return;
		}
		if (aUpdate.Transition && aUpdate.Transition->Identity
			&& (!SetupAcceptedGeneration || aUpdate.Transition->Identity->Generation != *SetupAcceptedGeneration)) {
			retire_setup_effects();
			SetupOwner = next_setup_owner();
			SetupDestination->cancel();
			return;
		}
		if (aUpdate.Transition && aUpdate.Transition->Status
			&& (aUpdate.Transition->Status->Status == core::github_status::session_failed || aUpdate.Transition->Status->Status == core::github_status::session_cancelled)) {
			const auto previewing = SetupDestination->pending()->Stage == core::setup_destination_stage::public_preview;
			retire_setup_effects();
			SetupDestination->cancel();
			present({ .Pending = std::nullopt,
					  .Notice  = previewing ? "Could not open without signing in. The requested link is retained; choose Open to retry, or sign in."
											: "Sign-in could not finish. The requested link is retained; choose Open to retry.",
					  .Opened  = false });
			return;
		}

		const auto pending = *SetupDestination->pending();
		if (aUpdate.Session && SetupAcceptedGeneration && Host->accepted_identity_generation() == *SetupAcceptedGeneration && pending.Profile == aUpdate.Session->Profile) {
			const auto* profile = find_profile(aUpdate.Session->Profile);
			SetupAcceptedGeneration.reset();
			SetupDeviceRequest.reset();
			if (profile && pending.Stage == core::setup_destination_stage::public_preview) {
				SetupDestination->previewed(pending.Request, profile->profile(), profile->hostname());
			} else if (profile) {
				SetupDestination->authenticated(pending.Request, profile->profile(), profile->hostname());
			}
		}
	}

	void github_application::implementation::retire_cli_discovery()
	{
		++GithubCliDiscoveryGeneration;
		GithubCliDiscoveryPending = false;
		Window->present_github_cli_discovery(qt::github_cli_discovery_state::idle);
	}
}
