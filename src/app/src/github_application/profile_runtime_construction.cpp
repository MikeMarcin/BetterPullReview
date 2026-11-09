#include "github_application/profile_runtime.hpp"

#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/git/installed_git_cache.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "bpr/adapters/sqlite/markdown_image_disk_cache.hpp"
#include "bpr/adapters/sqlite/queue_enrollment_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/credential_vault_diagnostics.hpp"
#include "bpr/application/diagnostic_settings.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/profile_storage_authority.hpp"
#include "bpr/application/review_queue_notification.hpp"
#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/device_authorization_model.hpp"
#include "github_anonymous_read_admission.hpp"
#include "github_application/agent_composition.hpp"
#include "github_application/insights_metrics.hpp"
#include "github_application/profile_hosts.hpp"
#include "github_cli_runtime.hpp"
#include "native_https_transport.hpp"
#include "platform_credential_vault.hpp"
#include "terminal_diagnostics.hpp"

#include <QApplication>
#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QObject>
#include <QScopeGuard>

#include <algorithm>
#include <stdexcept>

#if !defined(BPR_BUILD_TESTING)
	#include "designated_review_public_observer.hpp"
	#include "review_mutation_execution_authority.hpp"
#endif
#if defined(BPR_BUILD_TESTING)
	#include <QSystemTrayIcon>
#endif

namespace bpr::app {
	github_profile_runtime::github_profile_runtime(
		github_application_config aConfig, application::profile_startup_admission aAdmission, std::filesystem::path aRegistryRoot,
		std::optional<application::durable_queue_state> aSavedQueue, std::unique_ptr<application::remembered_connection_store_port> aConnectionStore, application::remembered_connection_store_result<core::remembered_connection_state> aConnections, std::shared_ptr<void> aScratch, bool aDesktop,
		std::shared_ptr<application::profile_runtime_lease> aRuntimeLease
#if defined(BPR_BUILD_TESTING)
		,
		const std::optional<detail::github_public_path_fixture> aFixture,
		detail::github_public_path_fixture_options              aFixtureOptions
#endif
		)
		: RuntimeLease{ std::move(aRuntimeLease) }
		, Config{ aConfig }
		, Admission{ aAdmission }
	{
		Desktop                     = aDesktop;
		StartupScratch              = std::move(aScratch);
		RegistryRoot                = std::move(aRegistryRoot);
		aConfig.ApplicationDataRoot = aAdmission.StorageRoot;
		aConfig.Profile             = aAdmission.Authority.profile();
		Config                      = aConfig;

#if defined(BPR_BUILD_TESTING)
		aFixtureOptions.ApplicationDataRoot = aAdmission.StorageRoot;
		aFixtureOptions.Profile             = aConfig.Profile;
		if (aConfig.AttendedDesignatedLiveApproval.has_value()) {
			throw std::invalid_argument{
				"attended live approval is unavailable in a tests-enabled composition"
			};
		}
#else
		if (aConfig.AttendedDesignatedLiveApproval.has_value()) {
			if (!aConfig.ApplicationDataRoot.has_value()) {
				throw std::runtime_error{ "explicit replay state unavailable" };
			}
			auto replay = application::detail::designated_review_replay_initializer::load(
				designated_review_replay_namespace_path(
					aConfig.ApplicationDataRoot->lexically_normal()));
			if (!replay.has_value()) {
				throw std::runtime_error{ "replay state unavailable" };
			}
			auto observer
				= adapters::github::detail::make_designated_review_public_observer();
			if (observer == nullptr) {
				throw std::runtime_error{ "public observer unavailable" };
			}
			auto authority = application::detail::designated_review_authority_bootstrap::run(
				application::detail::designated_review_bootstrap_request{
					.ApprovalPath       = *aConfig.AttendedDesignatedLiveApproval,
					.ExecutablePath     = aConfig.RunningExecutable,
					.SourceSha256       = aConfig.AttendedSourceSha256,
					.BuildConfiguration = aConfig.BuildConfiguration,
					.SupervisorSha256   = aConfig.AttendedSupervisorSha256,
					.InventorySha256    = aConfig.AttendedInventorySha256,
					.Replay             = std::move(*replay),
					.BuildTesting       = aConfig.BuildTesting,
				},
				*observer);
			if (!authority.has_value() || *authority == nullptr) {
				throw std::runtime_error{ "attended live approval refused" };
			}
			LiveReviewMutationGrant    = std::move(*authority);
			AttendedReviewMutationMode = true;
		}
#endif
		if (aConfig.ExistingCheckout.has_value()) {
			std::error_code checkoutError;
			const auto      canonical = std::filesystem::canonical(
				*aConfig.ExistingCheckout,
				checkoutError);
			if (!checkoutError && canonical.is_absolute()
				&& std::filesystem::is_directory(canonical, checkoutError)
				&& !checkoutError) {
				ExistingCheckout = application::checkout_observation{
					.Path        = canonical.string(),
					.Cleanliness = application::checkout_cleanliness::unknown,
				};
			}
		}
#if defined(BPR_BUILD_TESTING)
		// [[BPR-0515]]. Adopted outside the fixture branch below, because
		// the gate this exists for selects no fixture: it drives the
		// shipped composition and only wants the connect denied.
		NetworkDenied = aConfig.NetworkDenied;
		// [[BPR-0797]]. Decided here for the same reason: the marker
		// answers a question that has nothing to do with which fixture
		// this run selected. A selected fixture answers it too, on its
		// own: a fixture run is by definition not a person's session, and
		// a fixture course started by hand, outside CTest and so without
		// the marker, would otherwise hand a recorded verification
		// address to the real opener.
		//
		// [[BPR-0802]]. Decided here, where the browser and the GitHub
		// CLI factory are chosen, from the composition's one reader. It
		// used to be read in `src/app/main.cpp` and passed in, so the seam
		// covered the application binary and nothing that composes this
		// class without going through `main`: an in-process composition
		// root with no fixture was outside the boundary whatever its
		// CTest row said, and bound the real opener. Every composition
		// root now gets the same answer, because there is only one reader
		// (`inside_automated_test_boundary`, which the updater and the
		// update lifetime ask too, [[BPR-0983]]).
		InsideAutomatedTestBoundary = inside_automated_test_boundary() || aFixture.has_value();

		GithubCliDiscoveryAccounts = aFixture.has_value()
			? aFixtureOptions.GithubCliDiscoveryAccounts
			: std::nullopt;
		GithubCliInventoryChecks   = aFixture.has_value() ? aFixtureOptions.GithubCliInventoryChecks : nullptr;
		DiffComputeLimits          = aFixture.has_value()
			? aFixtureOptions.DiffComputeLimits
			: core::diff_compute_limits{ };
		// [[BPR-0316]]. Read outside the fixture branch below, because
		// this is the one option that means something without a fixture:
		// it forces the shipped selection available over the production
		// composition rather than over recorded traffic.
		PublicPreviewSelectionAvailable
			= aFixtureOptions.PublicPreviewSelectionAvailable
			&& !aFixture.has_value();
		if (aFixture.has_value()) {
			EnergyMetrics          = aFixtureOptions.InsightsMetrics;
			ReviewSubmissionServer = aFixtureOptions.ReviewSubmissionServer ? aFixtureOptions.ReviewSubmissionServer : std::make_shared<detail::review_submission_fixture_state>();
			DisableWorkbenchSelectionWire
				= aFixtureOptions.DisableWorkbenchSelectionWire;
			DisableReviewLedgerGestureWire
				= aFixtureOptions.DisableReviewLedgerGestureWire;
			UseGrantedNotificationSink
				= aFixtureOptions.UseGrantedNotificationSink;
			CaptureSystemNotificationDisplay
				= aFixtureOptions.CaptureSystemNotificationDisplay;
			DisableNotificationRequestWire
				= aFixtureOptions.DisableNotificationRequestWire;
			DisableNotificationSelectionWire
				= aFixtureOptions.DisableNotificationSelectionWire;
			DisableAgentProcessLauncherComposition
				= aFixtureOptions.DisableAgentProcessLauncherComposition;
			DisableAgentHostComposition
				= aFixtureOptions.DisableAgentHostComposition;
			DisableAgentGestureWire
				= aFixtureOptions.DisableAgentGestureWire;
			DisableAgentFindingHostComposition
				= aFixtureOptions.DisableAgentFindingHostComposition;
			DisableChatSessionHostComposition
				= aFixtureOptions.DisableChatSessionHostComposition;
			DisableCommentWorkshopComposition
				= aFixtureOptions.DisableCommentWorkshopComposition;
			DisableMcpHelperComposition
				= aFixtureOptions.DisableMcpHelperComposition;
			DisableSideChatPanelComposition
				= aFixtureOptions.DisableSideChatPanelComposition;
			DisableFindingQueueComposition
				= aFixtureOptions.DisableFindingQueueComposition;
			ExternalAgentExecutable
				= aFixtureOptions.ExternalAgentExecutable;
			CodexAgentExecutable    = aFixtureOptions.CodexAgentExecutable;
			SparseAgentSetupLook    = aFixtureOptions.SparseAgentSetupLook;
			ClaudeAgentExecutable   = aFixtureOptions.ClaudeAgentExecutable;
			PersonalSkillHome       = aFixtureOptions.PersonalSkillHome;
			ClaudeSearch            = aFixtureOptions.ClaudeSearch;
			CodexSearch             = aFixtureOptions.CodexSearch;
			RegisteredDeviceCourse  = aFixtureOptions.RegisteredDeviceCourse;
			DeviceRefreshCourse     = aFixtureOptions.DeviceRefreshCourse;
			RememberDeviceSignIn    = aFixtureOptions.PersistDeviceBindings;
			DeviceWallOffsetSeconds = aFixtureOptions.DeviceWallOffsetSeconds;
			DiscoveryWallOffset     = aFixtureOptions.DiscoveryWallOffsetSeconds;
			SuppressDeviceBrowser   = aFixtureOptions.SuppressDeviceBrowser;
			AttendedWindowBrowser   = aFixtureOptions.AttendedWindowBrowser;
			AttendedAgentSearch     = aFixtureOptions.AttendedAgentSearch;
			LabelHelper             = aFixtureOptions.LabelHelper;
			DisableDeviceAuthorizationPanelComposition
				= aFixtureOptions
					  .DisableDeviceAuthorizationPanelComposition;
			PublicPreviewEnabledCourse
				= aFixtureOptions.PublicPreviewEnabledCourse;
			PublicPreviewGhesCourse = aFixtureOptions.PublicPreviewGhesCourse;
			const auto host         = fixture_host(*aFixture);
			append_profile(
				Profiles,
				ProfileOptions,
				native_profile_candidate(
					host.Profile,
					host.Host,
					application::github_profile_revision{ 1U },
					host.Server,
					application::github_authentication_transport::native_https,
					std::string{ host.Hostname },
					aFixtureOptions.SetupUnconfiguredEnterprise ? adapters::github::github_profile_provenance::explicit_user : adapters::github::github_profile_provenance::builtin),
				std::string{ host.Label });
			const auto recordedProfile = Profiles.front();
			if (aFixtureOptions.SetupUnconfiguredEnterprise) {
				Profiles.clear();
				ProfileOptions.clear();
				for (const auto id : { 1U, 2U }) {
					append_profile(Profiles, ProfileOptions, native_profile_candidate(core::host_profile_id{ id }, core::git_host_id{ 1U }, application::github_profile_revision{ 1U }, application::github_server_contract::github_com_2022_11_28, id == 1U ? application::github_authentication_transport::native_https : application::github_authentication_transport::github_cli, "github.com", adapters::github::github_profile_provenance::builtin), "GitHub.com");
				}
			}

			Fixture = std::make_unique<
				detail::github_public_path_fixture_components>(
				*aFixture,
				recordedProfile,
				VaultEffects,
				std::move(aFixtureOptions));
		} else
#endif
		{
			append_profile(
				Profiles,
				ProfileOptions,
				native_profile_candidate(
					core::host_profile_id{ 1U },
					core::git_host_id{ 1U },
					application::github_profile_revision{ 1U },
					application::github_server_contract::
						github_com_2022_11_28,
					application::github_authentication_transport::native_https,
					"github.com",
					adapters::github::github_profile_provenance::builtin),
				"GitHub.com — Native HTTPS");
			if (aConfig.EnterpriseHostname.has_value()) {
				append_profile(
					Profiles,
					ProfileOptions,
					native_profile_candidate(
						core::host_profile_id{ 3U },
						core::git_host_id{ 2U },
						application::github_profile_revision{ 1U },
						application::github_server_contract::
							ghes_3_18_2022_11_28,
						application::github_authentication_transport::
							native_https,
						*aConfig.EnterpriseHostname,
						adapters::github::github_profile_provenance::explicit_user),
					"GHES 3.18 — Native HTTPS");
			}
		}

#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr) {
			TransportFactory = Fixture->transport_factory();
		} else
#endif
		{
			// [[BPR-0797]]. The label above and this factory answer the
			// same question and must answer it the same way: a row that
			// offers the CLI sign-in over a factory whose `supports_cli()`
			// is false is the `(unavailable)` defect wearing the other
			// label.
#if defined(BPR_GITHUB_CLI_PLATFORM)
			if (cli_read_offered_here()) {
				TransportFactory = make_opted_in_cli_transport_factory();
			} else {
				TransportFactory = std::make_shared<adapters::github::detail::native_github_wire_transport_factory>();
			}
#else
			TransportFactory = std::make_shared<adapters::github::detail::native_github_wire_transport_factory>();
#endif
		}

		auto applicationDataRoot = std::filesystem::path{ };
		auto gitExecutable       = std::filesystem::path{ };
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr) {
			applicationDataRoot = Fixture->application_data_root();
			gitExecutable       = Fixture->git_executable();
		} else
#endif
		{
			applicationDataRoot = aAdmission.StorageRoot;
			gitExecutable       = native_git_executable(aConfig);
		}
		if (applicationDataRoot.empty() || gitExecutable.empty()) {
			throw std::runtime_error{
				"Application data or installed Git is unavailable"
			};
		}
		ApplicationDataRoot = applicationDataRoot;
#if defined(BPR_AGENT_INTEGRATIONS)
		NativeAgentSessions = adapters::sqlite::make_native_agent_session_store(ApplicationDataRoot);
#endif
		ConnectionStore = std::move(aConnectionStore);
		Connections     = std::make_unique<application::remembered_connection_host>(*ConnectionStore, std::move(aConnections), Admission.Authority.profile(), ConnectionEffects, DecisionLane, [this](auto aObservation) { connection_notice(aObservation); });
		if (!Connections->available()) {
			RestorationGlobalNotice = QStringLiteral("Saved connections could not be read. Sign in explicitly to continue; the saved data has been preserved.");
		}
		InstalledGit  = gitExecutable;
		DiagnosticLog = aConfig.DiagnosticLog;
		if (!DiagnosticLog) {
			try {
				const auto loaded  = adapters::sqlite::load_diagnostic_preferences(applicationDataRoot);
				OwnedDiagnosticLog = std::make_shared<adapters::logging::quill_log>(applicationDataRoot, adapters::logging::diagnostic_file_limits{ }, loaded.Preferences);
				Diagnostics        = std::make_unique<diagnostic_controls>(applicationDataRoot, OwnedDiagnosticLog, loaded);
				DiagnosticLog      = OwnedDiagnosticLog;
			} catch (...) { /* Diagnostics allocation must not prevent application startup. */
			}
		}
		InsightsRecorder = std::make_shared<application::insights_recorder>(InsightsEffects);
		WorkMetrics      = std::make_unique<qt::event_loop_metrics>([recorder = InsightsRecorder](auto aSample) { recorder->record_work(std::move(aSample)); });
		InsightsRecorder->set_foreground(QGuiApplication::applicationState() == Qt::ApplicationActive);
		TransportFactory->Insights = InsightsRecorder;
		DiagnosticLog              = application::observe_insights_problems(DiagnosticLog, InsightsRecorder);
		if (DiagnosticLog) {
			static_cast<void>(DiagnosticLog->record({ .Operation = application::diagnostic_operation::application, .Outcome = application::diagnostic_outcome::started }));
			// [[BPR-1831]].
			StorageDiagnostics = std::make_unique<adapters::sqlite::storage_diagnostics_scope>(DiagnosticLog);
			OperationAggregate = std::make_shared<application::aggregated_operation_log>(DiagnosticLog);
		}

		// [[BPR-0660]]. The enterprise servers the reviewer kept are
		// offered beside the configured ones. The queue's own durable
		// state is the record; it holds a hostname and an account name
		// and never a token, so the server is offered and the sign-in
		// is still the device flow.
		{
			auto restored = std::move(aSavedQueue);
			if (restored.has_value()) {
				auto next = std::uint64_t{ 0U };
				for (const auto& profile : Profiles) {
					next = std::max(next, profile.profile().value());
				}
				for (const auto& server : restored->SavedServers) {
					const auto known = std::ranges::any_of(
						Profiles, [&server](const adapters::github::validated_github_host_profile& aProfile) {
							return aProfile.hostname() == server.Hostname;
						});
					if (known) {
						continue;
					}
					++next;
					append_profile(
						Profiles,
						ProfileOptions,
						native_profile_candidate(
							core::host_profile_id{ next },
							core::git_host_id{ 2U },
							application::github_profile_revision{ 1U },
							application::github_server_contract::ghes_3_18_2022_11_28,
							application::github_authentication_transport::native_https,
							server.Hostname,
							adapters::github::github_profile_provenance::explicit_user),
						"GHES 3.18 — " + server.Hostname);
				}
			}
		}

		if (Connections->state().LastProfileId != 0U || Connections->state().LastHostId != 0U) {
			const auto originals = Profiles;
			Profiles.clear();
			ProfileOptions.clear();
			auto        nextProfile = Connections->state().LastProfileId;
			auto        nextHost    = Connections->state().LastHostId;
			const auto& rows        = Connections->state().Connections;
			for (const auto& original : originals) {
				const auto row          = std::ranges::find(rows, original.hostname(), &core::remembered_connection::Hostname);
				const auto native       = original.authentication_transport() == application::github_authentication_transport::native_https && row != rows.end() && row->Profile.value() != 0U && row->Transport == core::remembered_connection_transport::native_device;
				const auto profileId    = native ? row->Profile : core::host_profile_id{ Connections->state().LastProfileId == 0U ? original.profile().value() : ++nextProfile };
				const auto existingHost = std::ranges::find(Profiles, original.hostname(), &adapters::github::validated_github_host_profile::hostname);
				const auto hostId       = existingHost != Profiles.end() ? existingHost->host() : row != rows.end() && row->Host.value() != 0U ? row->Host
																																			   : core::git_host_id{ Connections->state().LastHostId == 0U ? original.host().value() : ++nextHost };
				append_profile(Profiles, ProfileOptions, native_profile_candidate(profileId, hostId, native ? application::github_profile_revision{ row->ProfileRevision } : original.revision(), original.server_contract(), original.authentication_transport(), std::string{ original.hostname() }, original.provenance()), std::string{ original.hostname() } + (original.authentication_transport() == application::github_authentication_transport::github_cli ? " — GitHub CLI" : " — Native HTTPS"));
			}
			for (const auto& row : rows) {
				if (row.Profile.value() == 0U || row.Transport != core::remembered_connection_transport::github_cli) {
					continue;
				}
				auto candidate            = native_profile_candidate(row.Profile, row.Host, application::github_profile_revision{ row.ProfileRevision }, row.Server == core::remembered_connection_server::github_com ? application::github_server_contract::github_com_2022_11_28 : application::github_server_contract::ghes_3_18_2022_11_28, application::github_authentication_transport::github_cli, row.Hostname, adapters::github::github_profile_provenance::explicit_user);
				candidate.CliAccountLogin = row.Login;
				candidate.CliAccount      = row.Account;
				append_profile(Profiles, ProfileOptions, candidate, row.Hostname + " — GitHub CLI (" + row.Login + ")");
			}
		}
		ProfileCatalog = std::make_shared<adapters::github::detail::github_profile_catalog>();
		for (const auto& profile : Profiles) {
			if (!ProfileCatalog->register_profile(profile)) {
				throw std::runtime_error{ "GitHub profile catalog initialization failed" };
			}
		}

		auto provider = adapters::github::detail::make_github_provider(
			ProfileCatalog,
			ProviderEffects,
			TransportFactory, adapters::github::detail::github_provider_read_authority::authenticated, DiagnosticLog);
		if (!provider.has_value()) {
			throw std::runtime_error{ "GitHub provider composition failed" };
		}
		Provider = std::move(*provider);
		// RFC-BPR-0084. Image fetches run on their own serial lanes, each
		// with a credential-free media transport per profile; a credentialed
		// GHES hop runs on the provider's lane through the account transport.
		{
			std::vector<application::effect_executor*> lanes;
			lanes.reserve(ImageFetchEffects.size());
			for (auto& lane : ImageFetchEffects) {
				lanes.push_back(&lane);
			}
			Provider->configure_markdown_images(std::move(lanes), markdown_image_media_factory(adapters::github::detail::make_native_markdown_image_media_factory()));
		}
		// [[BPR-1846]]. A gh CLI GHES profile's avatars come through the
		// shipped avatar helper beside the application, which asks gh for
		// the token itself. A missing helper leaves no path, and those
		// avatars take the web route.
		{
#if defined(BPR_BUILD_TESTING)
			const auto helper = QString::fromUtf8(BPR_GH_AVATAR_HELPER_EXECUTABLE);
#else
			const auto helper = QCoreApplication::applicationDirPath() + QStringLiteral("/" BPR_GH_AVATAR_HELPER_FILE_NAME);
#endif
			const auto canonical = QFileInfo{ helper }.canonicalFilePath();
			Provider->configure_gh_cli_avatar_helper(canonical.isEmpty() ? std::filesystem::path{ } : native_filesystem_path(canonical));
		}
		// [[BPR-1680]]. Discovery's own lanes: a CLI account's discovery
		// reads run there, on its background gh lanes, beside every pull
		// request read on the provider's lane.
		{
			std::vector<application::effect_executor*> lanes;
			lanes.reserve(DiscoveryEffects.size());
			for (auto& lane : DiscoveryEffects) {
				lanes.push_back(&lane);
			}
			Provider->configure_discovery_lanes(std::move(lanes));
		}
		ImageDisk = adapters::sqlite::make_markdown_image_disk_cache(ApplicationDataRoot);
		// [[BPR-1831]]. Every image and avatar fetch leaves an aggregated record.
		ImageFetchDiagnostics = std::make_unique<application::diagnosed_image_fetch>(*Provider, OperationAggregate);
		Images                = std::make_unique<application::markdown_image_service>(DecisionLane, ImageDecodeEffects, ImageFetchDiagnostics.get(), &ImageDecoder,
																					  core::markdown_image_memory_cache_bytes, ImageDisk.get());

		// RFC-BPR-0017's credential-free read authority, over the same
		// profiles and the same transport factory. A second provider
		// rather than a mode on the first one: the authority is fixed at
		// construction, so no request can decay from authenticated into
		// anonymous or back, and the GHES, viewer-identity, GraphQL and
		// mutation refusals happen inside it before a request is built.
		//
		// Constructed in both profiles and on every platform, including
		// the ones whose selection is unavailable. Construction sends
		// nothing; what the selection below decides is whether the host is
		// ever given this authority at all.
		//
		// [[BPR-0316]]. The live candidate replaces the factory for this
		// second provider alone, with one that names the shipped
		// anonymous entry, counts what reaches the wire, and refuses both
		// credentialed entries. The authenticated provider above keeps the
		// factory it had, so this changes what the signed-out authority is
		// built from and nothing else. What the anonymous provider was
		// actually constructed with is kept below, so the evidence is a
		// pointer identity rather than a claim about a call site.
		AnonymousTransportFactory = TransportFactory;
#if defined(BPR_BUILD_TESTING)
		if (PublicPreviewSelectionAvailable) {
			ShippedAnonymousTransports
				= std::make_shared<shipped_anonymous_wire_transport_factory>();
			AnonymousTransportFactory = ShippedAnonymousTransports;
		}
#endif
		AnonymousTransportFactory->Insights = InsightsRecorder;
		auto anonymousProvider              = adapters::github::detail::make_github_provider(
			ProfileCatalog,
			ProviderEffects,
			AnonymousTransportFactory,
			adapters::github::detail::github_provider_read_authority::
				anonymous_public,
			DiagnosticLog);
		if (!anonymousProvider.has_value()) {
			throw std::runtime_error{
				"Anonymous GitHub provider composition failed"
			};
		}
		AnonymousProvider = std::move(*anonymousProvider);
		CountedAnonymousPullRequests
			= std::make_unique<counting_anonymous_pull_requests>(
				*AnonymousProvider);
		// One profile carries the signed-out mode, and it is the
		// GitHub.com one: `make_native_https_anonymous_smart_transport`
		// refuses every other server contract before a transport exists.
		AnonymousSmartGit
			= std::make_unique<anonymous_git_smart_transport_factory>(
				Profiles.front()
#if defined(BPR_BUILD_TESTING)
					,
				Fixture != nullptr ? Fixture->anonymous_smart_transport_factory() : nullptr
#endif
			);

#if defined(BPR_BUILD_TESTING)
		if (Fixture == nullptr)
#endif
		{
			OwnedVault                  = make_platform_vault(VaultEffects, aConfig.Profile);
			NativePlatformVaultSelected = OwnedVault != nullptr;
			if (OwnedVault == nullptr) {
				OwnedVault = std::make_unique<unavailable_credential_vault>(
					VaultEffects);
			}
			// [[BPR-1829]]. Every store, load and delete of the account
			// and refresh slots leaves a credential.vault record.
			OwnedVault = application::with_credential_vault_diagnostics(std::move(OwnedVault), DiagnosticLog);
		}

		auto cache = adapters::git::make_installed_git_cache({
			.GitExecutable     = gitExecutable,
			.StorageRoot       = applicationDataRoot,
			.WindowsSystemRoot = windows_system_root(),
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
			.WorkspaceOwnershipRegistry = applicationDataRoot / "agent-workspace-ownership-v1",
#endif
			.Insights = InsightsRecorder,
		});
		auto catalog = adapters::sqlite::make_epoch_catalog({
			.StorageRoot = applicationDataRoot,
		});
		// Two stores, two refusals; a developer build reading
		// STARTUP_FAILED_CAUSE needs to know which one (BPR-0436).
		if (!cache) {
			throw std::runtime_error{ "Git cache composition failed" };
		}
		if (!catalog) {
			throw std::runtime_error{
				"epoch catalog composition failed ("
				+ std::to_string(static_cast<unsigned int>(catalog.error())) + ")"
			};
		}
		if (!cache || !catalog) {
			throw std::runtime_error{
				"Git cache or epoch catalog composition failed"
			};
		}
		GitCaches     = std::move(*cache);
		EpochCatalogs = std::move(*catalog);

		// The workbench layout is a third database beside the catalog and
		// the ledger, for the same reason those two are apart: deleting one
		// kind of local state must not destroy another. Losing a drawer
		// width must not lose reading progress. Composition failure is not
		// fatal - a reviewer whose layout cannot be stored still gets the
		// composed defaults.
		//
		// The desktop path keeps every adapter lifetime inside an effect
		// call (BPR-0708), so the port it installs opens the store on each
		// use. Whether the store can open is still decided here, by one
		// open that is not kept: a lazy port over an obstructed store
		// would report persistence it never had and never fall back, and
		// the composition contract's forced obstruction proved exactly
		// that (BPR-0718).
		if (aDesktop) {
			if (adapters::sqlite::make_workbench_defaults_store({ .StorageRoot = applicationDataRoot })) {
				WorkbenchSettings             = std::make_unique<lazy_workbench_defaults_port>(applicationDataRoot);
				WorkbenchPersistenceAvailable = true;
			} else {
				WorkbenchSettings = std::make_unique<session_workbench_settings>();
			}
		} else {
			auto layout = adapters::sqlite::make_workbench_defaults_store({
				.StorageRoot = applicationDataRoot,
			});
			if (layout) {
				WorkbenchSettings             = std::move(*layout);
				WorkbenchPersistenceAvailable = true;
			} else {
				WorkbenchSettings
					= std::make_unique<session_workbench_settings>();
			}
		}

		// A fourth database, kept apart for the same reason as the rest.
		// Composition failure is not fatal: a reviewer whose theme cannot
		// be stored still gets the composed defaults.
		auto appearance = adapters::sqlite::make_appearance_settings_store({
			.StorageRoot = applicationDataRoot,
		});
		if (appearance) {
			AppearanceSettings             = std::move(*appearance);
			AppearancePersistenceAvailable = true;
		} else {
			AppearanceSettings
				= std::make_unique<session_only_appearance_settings>();
		}

		EpochEntropy = application::make_system_epoch_entropy();
		EpochClock   = application::make_system_epoch_clock();
		if (!EpochEntropy || !EpochClock) {
			throw std::runtime_error{ "Epoch runtime ports are unavailable" };
		}

		EpochActors          = std::make_unique<application::profile_epoch_actor_registry>(Config.Profile, EpochContexts, *GitCaches, EpochEffects);
		WorkbenchPreferences = std::make_unique<application::workbench_preferences_host>(*WorkbenchSettings, WorkbenchEffects, DecisionLane, Config.Profile);
		// The shared owner also serves defaults-only windows, which have no window-record restore.
		WorkbenchPreferences->restore();
		// [[BPR-1844]]. Read once, before any window opens a pull request,
		// so a relaunch opens with the filters last chosen.
		ViewFilterStore       = std::make_unique<view_filter_memory_store>(applicationDataRoot);
		ViewFilterMemory      = std::make_unique<application::view_filter_memory_host>(*ViewFilterStore, WorkbenchEffects, DecisionLane, ViewFilterStore->load());
		AppearancePreferences = std::make_shared<application::appearance_preferences_application_host>(qt::default_appearance_settings(), qt::builtin_appearance_palettes(), *AppearanceSettings, ThemeDocuments, AppearanceEffects, DecisionLane, core::application_session_id{ 1U });
		SyntaxDefinitions     = std::make_unique<adapters::syntax::packaged_definition_catalog>();
		QueueStore            = adapters::sqlite::make_queue_enrollment_store({
			.StorageRoot = applicationDataRoot,
		});
		QueueRefresh          = std::make_unique<composition_queue_refresh>();
		QueueHost             = std::make_unique<application::review_queue_application_host>(
			*QueueStore,
			*QueueRefresh,
			QueueEffects,
			DecisionLane,
			Config.Profile, *this, application::queue_session_id{ },
			std::shared_ptr<application::queue_landing_projection_port>(this, [](auto*) { }));
		// [[BPR-0990]]. One discovery per profile, over the authenticated
		// provider's personal reads and this profile's queue. It adopts the
		// accepted account session as each presentation publishes.
		DiscoveryClock = std::make_unique<system_discovery_clock>();
#if defined(BPR_BUILD_TESTING)
		if (DiscoveryWallOffset != nullptr) {
			DiscoveryClock = std::make_unique<system_discovery_clock>(DiscoveryWallOffset);
		}
#endif
		Discovery = std::make_unique<application::personal_discovery_host>(*Provider, DecisionLane, *QueueHost, *DiscoveryClock, DiagnosticLog.get());
		// Queue projections arrive at this composition first. It forwards the
		// accepted value unchanged to the real window, then asks this notifier
		// only for newly warranting session/ref/bucket transitions.
#if defined(BPR_BUILD_TESTING)
		if (UseGrantedNotificationSink) {
			auto sink               = std::make_unique<fixture_notification_sink>();
			FixtureNotificationSink = sink.get();
			NotificationSink        = std::move(sink);
		}
#endif
		if (NotificationSink == nullptr) {
			auto sink = std::unique_ptr<qt::system_tray_notification_sink>{ };
#if defined(BPR_BUILD_TESTING)
			// A fixture run also takes the captured boundary when the
			// platform truthfully reports no tray — the offscreen
			// platform on Windows — because the courses that leave
			// capture off assert what reaches the sink, not the native
			// display mechanics; where a tray exists, nothing changes
			// (BPR-0442). Production composition is untouched.
			if (CaptureSystemNotificationDisplay
				|| (Fixture != nullptr
					&& !QSystemTrayIcon::isSystemTrayAvailable())) {
				sink = std::make_unique<qt::system_tray_notification_sink>(
					[this](QSystemTrayIcon&, const QString&, const QString&) {
						++CapturedNotificationDisplays;
					},
					nullptr);
			}
#endif
			// [[BPR-1250]]. Through the desktop's status item where it keeps
			// one, so the menu bar's Dif does not leave with this profile.
			if (sink == nullptr && Config.StatusItem) {
				sink = std::make_unique<qt::system_tray_notification_sink>(
					*Config.StatusItem,
					nullptr);
			}
			if (sink == nullptr) {
				sink = std::make_unique<qt::system_tray_notification_sink>(
					nullptr);
			}
			SystemNotificationSink = sink.get();
			NotificationSink       = std::move(sink);
		}
		Notifications = std::make_unique<application::review_queue_notifier>(*NotificationSink, [this](application::queue_notification_target aTarget) {
																				 if (Closing || !Focused) {
																					 return;
																				 }
																				 const auto found = Observers.find(*Focused);
																				 if (found == Observers.end() || found->second.Lifetime.expired() || !found->second.SelectQueue) {
																					 return;
																				 }
#if defined(BPR_BUILD_TESTING)
																				 if (DisableNotificationSelectionWire) {
																					 return;
																				 }
#endif
																				 if (found->second.SelectQueue(aTarget.Ref)) {
																					 ++NotificationSelections;
																				 } else {
																					 ++NotificationSelectionFailures;
																				 } },
																			 DiagnosticLog); // [[BPR-1831]]
		const auto activation = [this](
									application::queue_notification_target aTarget) {
			if (Notifications) {
				Notifications->activate(aTarget);
			}
		};
		if (SystemNotificationSink != nullptr) {
			SystemNotificationSink->set_activation_sink(activation);
		}
#if defined(BPR_BUILD_TESTING)
		if (FixtureNotificationSink != nullptr) {
			FixtureNotificationSink->set_activation_sink(activation);
		}
#endif
		// [[BPR-1850]]. One poll host and one timer for every window of the
		// profile. A fixture run never polls: its transports serve pull
		// requests, not notifications.
		{
			auto sink = std::unique_ptr<qt::pull_request_activity_notification_sink>{ };
			// The tray the review queue's notifications use, so the desktop
			// shows one icon.
			if (Config.StatusItem) {
				sink = std::make_unique<qt::pull_request_activity_notification_sink>(*Config.StatusItem, nullptr);
			} else if (SystemNotificationSink != nullptr && SystemNotificationSink->tray_icon() != nullptr) {
				sink = std::make_unique<qt::pull_request_activity_notification_sink>(*SystemNotificationSink->tray_icon(), nullptr);
			} else {
				sink = std::make_unique<qt::pull_request_activity_notification_sink>(nullptr);
			}
			ActivitySink  = std::move(sink);
			ActivityTimer = std::make_unique<qt::pull_request_activity_timer>([this] {
				if (!Closing && Activity) {
					Activity->tick();
				}
			});
			ActivityStore = std::make_unique<pull_request_activity_store>(applicationDataRoot);
			Activity      = std::make_unique<application::pull_request_activity_host>(
				*Provider, *ActivityStore, WorkbenchEffects, DecisionLane, *ActivityTimer, *ActivitySink,
				[] { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); },
				[this] { return activity_focus(); }, ActivityStore->load(), DiagnosticLog);
			ActivitySink->set_activation([this] { activate_activity(); });
			Activity->set_observer([this] { present_activity(); });
			Activity->set_refresh([this](const std::vector<core::pull_request_ref>& aActive) { activity_arrived(aActive); });
#if defined(BPR_BUILD_TESTING)
			if (Fixture != nullptr) {
				Activity->shutdown();
			}
#endif
		}
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
		// Persistent private neighbourhood on the cache filesystem, outside
		// its isolation root. Only effect workers create or recover children.
		// [[BPR-0622]]. The leaf is appended natively: a narrow string() of a
		// non-ASCII Windows profile name would throw.
		auto agentWorkspaceLeaf = ApplicationDataRoot.filename();
		agentWorkspaceLeaf += "-agent-workspaces-v1";
		const auto agentWorkspacePath = ApplicationDataRoot.parent_path() / agentWorkspaceLeaf;
#else
		// git pack-objects creates its temporary pack inside the --git-dir's
		// own object store and renames it onto the requested output base.
		// A workspace on another filesystem makes that rename EXDEV, and
		// git reports "Invalid cross-device link" rather than anything this
		// side can interpret. So the workspace has to share a filesystem
		// with the retained cache.
		//
		// The system temporary directory does not, on a stock Linux host:
		// /tmp is tmpfs and an ordinary checkout is not. macOS never showed
		// it because the per-user temporary directory and a checkout are on
		// one APFS volume.
		//
		// A sibling of the application data root shares that filesystem and
		// stays disjoint from the cache, which the guard below still checks
		// rather than assumes.
		const auto workspaceNeighbourhood
			= std::filesystem::weakly_canonical(ApplicationDataRoot)
				  .parent_path();
		const auto workspaceTemplate = workspaceNeighbourhood.empty()
			? std::filesystem::path{ QDir::tempPath().toStdString() }
				/ "betterpullreview-agent-XXXXXX"
			: workspaceNeighbourhood / "betterpullreview-agent-XXXXXX";
		AgentWorkspaceRoot           = std::make_unique<QTemporaryDir>(
			QString::fromStdString(workspaceTemplate.string()));
		if (!AgentWorkspaceRoot->isValid()) {
			throw std::runtime_error{ "Agent workspace root is unavailable" };
		}
		// Exact installed-Git cleanup must be the only recursive authority.
		// A generic temporary-directory sweep could delete a same-path
		// substitute after exact cleanup correctly refused it.
		AgentWorkspaceRoot->setAutoRemove(false);
		const auto agentWorkspacePath = std::filesystem::canonical(
			AgentWorkspaceRoot->path().toStdString());
		AgentWorkspaceParent
			= adapters::git::capture_installed_agent_workspace_parent(
				agentWorkspacePath);
		if (!AgentWorkspaceParent.has_value()) {
			throw std::runtime_error{
				"Agent workspace root identity is unavailable"
			};
		}
		const auto cachePath = std::filesystem::canonical(ApplicationDataRoot);
		const auto within    = [](const std::filesystem::path& aPath,
								  const std::filesystem::path& aPrefix) {
			auto path   = aPath.begin();
			auto prefix = aPrefix.begin();
			for (; prefix != aPrefix.end(); ++prefix, ++path) {
				if (path == aPath.end() || *path != *prefix) {
					return false;
				}
			}
			return true;
		};
		if (within(agentWorkspacePath, cachePath)
			|| within(cachePath, agentWorkspacePath)) {
			throw std::runtime_error{ "Agent workspace root crosses Git cache" };
		}
#endif
		AgentWorkspaceNeighbourhood = agentWorkspacePath;

		initialize_identity();
	}

	void github_profile_runtime::start_services()
	{
		// Start only after construction succeeds; callbacks require a live decision lane.
		try {
			InsightsEffects.post([this, root = ApplicationDataRoot] {
				std::shared_ptr<application::insights_storage_port> storage;
				try {
					storage = adapters::sqlite::make_insights_storage(root, [root] { return adapters::git::measure_installed_git_storage(root); });
				} catch (...) { }
				DecisionLane.post([this, admittedStorage = std::shared_ptr<application::insights_storage_port>{ std::move(storage) }] {
					InsightsInitializing = false;
					if (!admittedStorage) {
						if (Insights) {
							Insights->apply({ });
						}
						return;
					}
					InsightsRecorder->attach(admittedStorage);
					if (!Closing) {
						EnergySampler     = std::make_shared<application::insights_energy_sampler>(EnergyMetrics ? EnergyMetrics : std::make_shared<native_insights_metrics>(), InsightsRecorder, InsightsEffects);
						EnergyTimer       = std::make_unique<QTimer>();
						const auto sample = [this] {
							const auto foreground = QGuiApplication::applicationState() == Qt::ApplicationActive;
							EnergySampler->observe(foreground);
							EnergyTimer->start(application::insights_energy_sampler::interval_seconds(foreground) * 1000);
						};
						QObject::connect(EnergyTimer.get(), &QTimer::timeout, EnergyTimer.get(), sample);
						EnergyActivation = QObject::connect(qApp, &QGuiApplication::applicationStateChanged, EnergyTimer.get(), [sample](Qt::ApplicationState) { sample(); });
						sample();
					}
					InsightsHost = std::make_shared<application::insights_host>(admittedStorage, InsightsEffects, DecisionLane, [this](const auto& aSnapshot) {
						if (!Closing && Insights && Insights->isVisible()) {
							Insights->apply(aSnapshot);
						}
					});
					if (Closing) {
						InsightsHost->shutdown();
					} else if (Insights) {
						Insights->refresh();
					}
				});
			});
		} catch (...) {
			InsightsInitializing = false;
		}
		QueueHost->restore();
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
		AgentWorkspaceRecoveryPending.store(true, std::memory_order_release);
		try {
			EpochEffects.post([this, registry = ApplicationDataRoot / "agent-workspace-ownership-v1", neighbourhood = AgentWorkspaceNeighbourhood] {
				auto found     = adapters::git::workspace_recovery_refusal{ };
				auto recovered = adapters::git::recover_installed_agent_workspaces(registry, neighbourhood, { }, &found);
				try {
					DecisionLane.post([this, result = std::move(recovered), refusal = std::move(found)] {
						// [[BPR-1870]]. One line per workspace folder or record
						// that was narrowed in place rather than refused.
						for (const auto check : refusal.Repaired) {
							bpr::app::detail::terminal_text_stream{ } << "AGENT_WORKSPACE_PRIVATE_REPAIRED:path=omitted:check="
																	  << QString::fromUtf8(adapters::git::workspace_refusal_check_name(check)) << '\n';
						}
						// [[BPR-0817]]. A refusal is decided here, at startup, and it
						// is not a failure of the quit. So it no longer sets the
						// cleanup flag that a real quit turns into exit 1. Every
						// window states it in its status bar instead, under
						// RFC-BPR-0033's owner-delegated amendment. [[BPR-0875]]'s
						// name for it is printed once, here, where it happens.
						if (!result) {
							AgentWorkspaceRecoveryFailure = static_cast<int>(result.error());
							AgentWorkspaceRecoveryRefused = true;
							// [[BPR-1857]]. The check that refused rides the
							// line; the folder stays out of it, as before.
							AgentWorkspaceRecoveryReason = agent_workspace_refusal_text(refusal);
							bpr::app::detail::terminal_text_stream{ } << "AGENT_WORKSPACE_RECOVERY_REFUSED:path=omitted:"
																	  << QString::fromStdString(named_workspace_ownership_failure(AgentWorkspaceRecoveryFailure))
																	  << ":check=" << QString::fromUtf8(adapters::git::workspace_refusal_check_name(refusal.Check)) << '\n';
						}
						// [[BPR-0808]]. Recovery is profile work, but its refusal is
						// presented by every window: those open now, and any opened
						// later, which replay the recorded refusal in `open_window`.
						if (!result && !Closing) {
							for_each_window([](const window_observer& aObserver) { if (aObserver.AgentWorkspaceRepair) { aObserver.AgentWorkspaceRepair(); } });
						}
						AgentWorkspaceRecoveryPending.store(false, std::memory_order_release);
					});
				} catch (...) {
					AgentWorkspaceRecoveryPostFailed.store(true, std::memory_order_release);
					AgentWorkspaceRecoveryPending.store(false, std::memory_order_release);
				}
			});
		} catch (...) {
			note_profile_cleanup_failure("agent-workspace-recovery:post-refused");
			AgentWorkspaceCleanupFailed = true;
			AgentWorkspaceRecoveryPending.store(false, std::memory_order_release);
		}
#endif
		if (!Desktop) {
			remember_startup();
		}
	}

	void github_profile_runtime::initialize_identity()
	{
		const auto& aConfig             = Config;
		const auto& applicationDataRoot = ApplicationDataRoot;
		PublicPreviewProjectionFanOut   = std::make_unique<composition_public_preview_projection>(PublicPreviewProjections,
																								  [this](const application::public_preview_presentation& aPresentation) {
																									observe_restoration_preview();
																									for_each_window([&](const window_observer& aObserver) { if (aObserver.Preview){ aObserver.Preview(aPresentation);
} });
																								  });
		// A platform whose shipped selection is unavailable supplies no
		// anonymous authority, so the host refuses before any request.
		auto publicPreview = shipped_public_preview_capability();
#if defined(BPR_BUILD_TESTING)
		// The forced courses. A tests-enabled build with the recorded
		// fixture overrides the shipped selection so the composed
		// anonymous authority runs through the real host, the real
		// provider and the real anonymous admission rules, which is the
		// only place "no header, no vault, and a public snapshot" can be
		// observed rather than argued.
		if ((PublicPreviewEnabledCourse || PublicPreviewGhesCourse)
			&& Fixture != nullptr) {
			publicPreview = public_preview_capability{ .Available = true };
		}
		// [[BPR-0316]]. The same override with no fixture underneath it:
		// the authority handed to the host is the production one, so a
		// read issued through it leaves the machine. Nothing here issues
		// one; the run driving the composition decides that.
		if (PublicPreviewSelectionAvailable && Fixture == nullptr) {
			publicPreview = public_preview_capability{ .Available = true };
		}
#endif
		PublicPreviewComposed = publicPreview.Available;
		// The session fan-out. The window keeps its place at the front, and
		// composition reads acceptances behind it so the device host's
		// refresh authority can be bound the moment one exists.
		GithubProjectionFanOut = std::make_unique<composition_github_projection>(*this, composition_github_projection::accepted_sink{ });
		Host                   = std::make_unique<application::github_application_host>(
			initial_github_state(),
			*Provider,
			*Provider,
			*Provider,
			*Provider,
			credential_vault(),
			CredentialBindings,
			DecisionLane,
			*GithubProjectionFanOut,
			RememberDeviceSignIn ? application::github_credential_shutdown_policy::retain_accepted_device_grants : application::github_credential_shutdown_policy::delete_retained_on_shutdown,
			nullptr,
			PublicPreviewComposed
				? static_cast<application::github_pull_request_port*>(
					  CountedAnonymousPullRequests.get())
				: nullptr,
			PublicPreviewComposed
				? static_cast<application::git_smart_transport_factory_port*>(
					  AnonymousSmartGit.get())
				: nullptr,
			PublicPreviewProjectionFanOut.get(), aConfig.Profile);
		Host->set_diagnostic_log(DiagnosticLog); // [[BPR-1829]] Sign out, account changes
		// The switch-to-sign-in gesture, end to end. The ring in the
		// review header dispatches it, the host names the profile the
		// signed-out session was activated for, and the device flow for
		// that same profile begins -- so sign-in returns the reviewer to
		// the pull request they were reading rather than to a picker.
		Host->set_public_preview_sign_in([this](core::host_profile_id aProfile) {
			if (Focused) {
				const auto found = Observers.find(*Focused);
				if (found != Observers.end() && !found->second.Lifetime.expired() && found->second.SignIn) {
					found->second.SignIn(aProfile);
				}
			}
		});

		// The device-authorization path. It is composed on every platform,
		// registered or not, because the visible choice and its typed
		// answer are the product behaviour the RFC asks for: on an
		// unregistered platform selecting device sign-in must say
		// `device_flow_unavailable`, not do nothing, and the capability
		// gate refuses before a request is built.
		DeviceEnrollment = std::make_unique<
			application::github_host_device_authorization_enrollment>(*Host);
		SetupDeviceEnrollment = std::make_unique<setup_device_enrollment>(*DeviceEnrollment,
																		  [this](core::host_profile_id aProfile, application::github_credential_material aCredential, std::optional<application::device_binding_lifetime> aLifetime, bool aSettled) {
																			  if (SetupCredential) {
																				  auto accept = std::move(SetupCredential);
																				  accept(aProfile, std::move(aCredential), aLifetime, aSettled);
																			  } else {
																				  Host->enroll_profile(aProfile, std::move(aCredential), application::github_credential_provenance::device_grant, aLifetime, aSettled, [this](const application::github_profile_authority& aAuthority, const application::github_viewer_record& aViewer) { return rebind_native_sign_in(aAuthority, aViewer); });
																			  }
																		  });

#if defined(BPR_BUILD_TESTING)
		CountedDeviceEnrollment = std::make_unique<
			counting_device_authorization_enrollment>(*SetupDeviceEnrollment);
		auto& deviceEnrollment
			= static_cast<application::device_authorization_enrollment_port&>(
				*CountedDeviceEnrollment);
#else
		auto& deviceEnrollment
			= static_cast<application::device_authorization_enrollment_port&>(
				*SetupDeviceEnrollment);
#endif
		DeviceClock = application::make_system_device_authorization_clock();
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr && RememberDeviceSignIn) {
			DeviceClock = std::make_unique<composition_device_wall_clock>(std::move(DeviceClock), DeviceWallOffsetSeconds);
		}
#endif

#if defined(BPR_BUILD_TESTING)
		// [[BPR-0536]]. The one input a fixture cannot supply is time.
		// A renewal is armed a lead before an eight-hour access token
		// expires, and no course can wait that out; the transport seam
		// beside this one replaces the network, and this replaces the
		// wait. Tests-enabled only, and composed only under the refresh
		// course, so a shipped image has one clock and it is the system's.
		//
		// It steps on the binding rather than on the authorized stage. The
		// host captures `AuthorizedAt` from this same clock at the moment
		// the stage turns, so an offset that stepped there would move both
		// ends of `elapsed` and change nothing. Composition's
		// `bind_refresh_authority` falls strictly between that capture and
		// the `schedule_refresh` that reads the clock again, which is the
		// only window where a one-way step shortens the wait to zero.
		//
		// [[BPR-0547]]. The host arms every renewal after the first one for
		// itself, and composition is told about it only once the timer is
		// already set -- too late to shorten anything. What it can read in
		// time is the host's own live rotation count, which the re-arm
		// raises inside that same window, between the capture and the read.
		// So the armings are the bind plus the rotations, and each one buys
		// one more step until the course has the rotations it drives.
		if (DeviceRefreshCourse && Fixture != nullptr) {
			DeviceClock = std::make_unique<composition_device_refresh_clock>(
				std::move(DeviceClock),
				[this]() noexcept -> std::size_t {
					const auto bound = DeviceRefreshBindings > 0U
						? std::size_t{ 1U }
						: std::size_t{ 0U };
					if (DeviceHost == nullptr) {
						return bound;
					}
					return bound
						+ static_cast<std::size_t>(
							   DeviceHost->refresh_observation().Rotations);
				},
				device_refresh_course_step,
				device_refresh_course_rotations);
		}
#endif
		// The shipped selection. A registered platform composes the real
		// credential-free wire bound to the recorded distributable client;
		// an unregistered platform composes the refusing transport whose
		// reached() count proves the gate held. A composition failure
		// fails closed to the refusal: a half-wired flow must answer
		// `device_flow_unavailable`, not invent one.
		auto* deviceTransport
			= static_cast<application::device_authorization_transport_port*>(
				&UnregisteredDeviceTransport);
		auto deviceCapability
			= application::device_authorization_capability{ .Registered
															= false };
		if constexpr (shipped_device_authorization_capability().Registered) {
			const auto deviceContext = core::github_session_request_context{
				.Request            = core::request_id{ 907U },
				.ApplicationSession = core::application_session_id{ 908U },
				.Profile            = Profiles.front().profile(),
				.IdentityGeneration = core::github_identity_generation{ 909U },
			};
			auto deviceWire = [this, &deviceContext]() {
#if defined(BPR_BUILD_TESTING)
				// [[BPR-0515]]. The same binding, the same credential-free
				// request, the same headers - only the proxy policy differs,
				// and it differs into a closed port. Selected by nothing but
				// `--network-denied`, which exists only here.
				if (NetworkDenied) {
					return adapters::github::detail::
						make_native_https_credential_free_transport_for_test(
							Profiles.front(),
							deviceContext,
							denied_loopback_network());
				}
#endif
				return adapters::github::detail::
					make_native_https_credential_free_transport(
						Profiles.front(), deviceContext);
			}();
			if (deviceWire.has_value()) {
				auto deviceAdapter = adapters::github::detail::
					make_github_device_authorization_transport(
						std::shared_ptr<
							adapters::github::detail::native_https_transport>{
							std::move(*deviceWire) },
						std::string{ production_device_client_id },
						DeviceTransportEffects);
				if (deviceAdapter.has_value()) {
					DeviceWireTransport = std::move(*deviceAdapter);
					deviceTransport     = DeviceWireTransport.get();
					deviceCapability
						= shipped_device_authorization_capability();
					// The identity is registered for one host, and the
					// wire above is bound to that host's profile. The
					// capability names the same profile so the host can
					// refuse a begin for any other one before it builds
					// a request ([[BPR-0535]]); the picker can bind the
					// mounted panel to a GHES profile, and this is what
					// keeps that gesture out of GitHub.com's flow.
					deviceCapability.Profile
						= Profiles.front().profile();
				}
			}
		}
#if defined(BPR_BUILD_TESTING)
		// The registered course. A tests-enabled build with the recorded
		// fixture overrides the shipped selection with BPR-0276's private
		// recorded grant: it runs through the real host, the real
		// enrollment boundary, and the real vault, which is the only place
		// "exactly one enrollment, and no settings or SQLite copy" can be
		// observed rather than argued.
		if (RegisteredDeviceCourse && Fixture != nullptr
			&& !Profiles.empty()) {
			deviceTransport  = &Fixture->device_authorization_transport();
			deviceCapability = application::device_authorization_capability{
				.Registered = true,
				// The same profile the course begins for, by the same
				// rule as the shipped selection above.
				.Profile = Profiles.front().profile(),
			};
		}
#endif
		DeviceCapabilityRegistered = deviceCapability.Registered;
		DeviceCapabilityProfile    = deviceCapability.Profile;
		// The browser handoff. Both graphs bind the real opener; a
		// tests-enabled graph binds the recording one instead when this
		// run is inside the automated test boundary, and none at all
		// under the construction cut.
		//
		// The rule this protects has not changed: a fixture transport
		// reached by mistake writes a line to a log, and a browser opened
		// by mistake opens a browser on whoever's machine is running the
		// suite. What changed is which question decides ([[BPR-0797]]).
		// The seam used to be `BPR_BUILD_TESTING`, which answers "was this
		// build configured with tests"; `BUILD_TESTING` is on by default,
		// so the answer was yes in every build a developer makes, and the
		// application in their hands pressed "Open browser" into a recorder
		// that returns `true` without opening anything -- while the account
		// row said "Opened in your browser." The boundary marker answers
		// the question that was meant, and it is injected by the single
		// registration seam every isolated test on every platform goes
		// through, so a suite cannot lose it by forgetting a flag.
		//
		// `SystemBrowser` is constructed either way. Its constructor is
		// the anchor `verify_composed_hosts.py` requires in both
		// profiles, so a build that stopped composing the opener fails
		// the audit rather than silently going back to answering
		// `unavailable` forever, which is the defect [[BPR-0516]] found.
		auto browserIdentity = composed_device_browser::system;
#if defined(BPR_BUILD_TESTING)
		if (SuppressDeviceBrowser) {
			browserIdentity = composed_device_browser::none;
		} else if (InsideAutomatedTestBoundary) {
			browserIdentity = composed_device_browser::recording;
		}
#endif
		application::device_authorization_browser_port* deviceBrowser
			= nullptr;
		if (browserIdentity == composed_device_browser::system) {
			deviceBrowser = &SystemBrowser;
		}
#if defined(BPR_BUILD_TESTING)
		if (browserIdentity == composed_device_browser::recording) {
			deviceBrowser = &RecordingBrowser;
		}
#endif
		ComposedBrowser       = deviceBrowser;
		ComposedDeviceBrowser = browserIdentity;
		WindowBrowser         = deviceBrowser;
#if defined(BPR_BUILD_TESTING)
		if (AttendedWindowBrowser && browserIdentity != composed_device_browser::none) {
			WindowBrowser = &SystemBrowser;
		}
#endif
		DeviceBrowserComposed = deviceBrowser != nullptr;
		// One port, two readers. The recording sink keeps its place at the
		// front of the fan-out, so every existing device contract still
		// observes the identical sequence of publications; the mounted
		// panel is told the same thing afterwards when the composition
		// adopted one.
		DeviceProjectionFanOut = std::make_unique<
			composition_device_authorization_projection>(
			DeviceProjections,
			[this](
				const application::device_authorization_presentation&
					aPresentation) {
				const auto authorized = aPresentation.Stage == core::device_authorization_stage::authorized;
				if (authorized && !DeviceStageAuthorized) {
					DeviceRefreshAuthority.reset();
				}
				DeviceStageAuthorized = authorized;
				for_each_window([&](const window_observer& aObserver) { if (aObserver.Device){ aObserver.Device(aPresentation);
} });
			},
			[this](
				const application::device_authorization_refresh_observation&
					aObservation) {
				DeviceRefreshObservation = aObservation;
				++DeviceRefreshPublications;
				for_each_window([&](const window_observer& aObserver) { if (aObserver.Refresh){ aObserver.Refresh(aObservation);
} });
			});
		DeviceHost = std::make_unique<
			application::device_authorization_application_host>(
			initial_device_authorization_state(),
			*deviceTransport,
			credential_vault(),
			deviceEnrollment,
			*DeviceClock,
			DeviceTimer,
			DecisionLane,
			*DeviceProjectionFanOut,
			application::device_authorization_host_limits{ },
			application::device_authorization_browser_handoff{
				// Null only under the construction cut, where the host
				// reports `unavailable` rather than inventing an address.
				// The authority travels with the pointer so a profile
				// that is not GitHub.com cannot inherit GitHub.com's.
				.Browser   = deviceBrowser,
				.Authority = std::string{
					application::default_device_verification_authority },
			},
			deviceCapability);
		DeviceHost->set_diagnostic_log(DiagnosticLog); // [[BPR-1829]] device sign-in, renewal
		if (RememberDeviceSignIn) {
			DeviceBindingStore = adapters::sqlite::make_device_binding_store(applicationDataRoot, aConfig.Profile);
			DevicePersistence  = std::make_unique<application::device_binding_persistence>(*DeviceBindingStore, DeviceBindingEffects, DecisionLane);
			// [[BPR-0644]]. A remembered grant restored after its access token
			// expired renews through the device host that owns the rotation.
			Host->configure_device_persistence(*DevicePersistence, [this] { return DeviceClock->observe_wall_seconds(); }, [this](const std::optional<application::device_launch_renewal>& aRenewal) {
				if (!aRenewal) {
					DeviceHost->release_refresh_authority();
					return;
				}
				DeviceHost->renew_restored_grant(aRenewal->Authority, aRenewal->IdentityGeneration, aRenewal->Lifetime); });
		}
	}
}
