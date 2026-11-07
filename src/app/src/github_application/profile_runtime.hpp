#ifndef BPR_APP_GITHUB_APPLICATION_PROFILE_RUNTIME_HPP
#define BPR_APP_GITHUB_APPLICATION_PROFILE_RUNTIME_HPP
#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/sqlite/storage_diagnostics.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/app/diagnostic_controls.hpp"
#include "bpr/app/github_application.hpp"
#include "bpr/app/profile_data_reset.hpp"
#include "bpr/app/release_maintenance_controls.hpp"
#include "bpr/app/release_update_desktop.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/appearance_preferences_application_host.hpp"
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/cached_review_overview_port.hpp"
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/device_binding_store.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_read_admission.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/host_operation_diagnostics.hpp"
#include "bpr/application/insights_host.hpp"
#include "bpr/application/markdown_image_ports.hpp"
#include "bpr/application/markdown_image_service.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/observing_review_ledger_store.hpp"
#include "bpr/application/personal_discovery_host.hpp"
#include "bpr/application/profile_activation.hpp"
#include "bpr/application/profile_epoch_actor_registry.hpp"
#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/application/profile_startup_admission.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/pull_request_activity_host.hpp"
#include "bpr/application/release_update_port.hpp"
#include "bpr/application/remembered_connection_host.hpp"
#include "bpr/application/remembered_connection_store.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/shared_draft_application_host.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/application/view_filter_memory_host.hpp"
#include "bpr/application/window_review_restoration_host.hpp"
#include "bpr/application/window_review_restoration_ports.hpp"
#include "bpr/application/workbench_preferences_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/markdown_image.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/release_maintenance.hpp"
#include "bpr/core/release_update.hpp"
#include "bpr/core/remembered_connection.hpp"
#include "bpr/core/review_queue.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/core/window_review_restoration.hpp"
#include "bpr/logging/quill_log.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/event_loop_device_timer.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/event_loop_metrics.hpp"
#include "bpr/qt/insights_window.hpp"
#include "bpr/qt/markdown_image_decoder.hpp"
#include "bpr/qt/pull_request_activity_notifications.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/system_tray_notification_sink.hpp"
#include "gh_cli_transport.hpp"
#include "github_application/device_authorization_composition.hpp"
#include "github_application/github_presentation_projections.hpp"
#include "github_application/public_preview_wire_transport.hpp"
#include "github_application/queue_notifications.hpp"
#include "github_application/review_mutation_selection.hpp"
#include "github_application/session_settings.hpp"
#include "github_application/theme_document_file.hpp"
#include "github_application/window_review_lanes.hpp"
#include "github_device_authorization_transport.hpp"
#include "github_host_profile.hpp"
#include "github_provider.hpp"
#include "github_release_update.hpp"
#include "platform_body_digest.hpp"
#include "private_review_workflow.hpp"
#include "profile_window_layout_port.hpp"
#include "system_browser_opener.hpp"
#include "tour_owner_service.hpp"

#include <QMetaObject>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include "review_submission_fixture.hpp"
#endif
#if defined(BPR_LOCAL_AUTOMATION)
	#include "cycle_owner_service.hpp"
#endif

// One BPR profile's composition: its providers, hosts, stores, effect
// lanes and windows, from construction to shutdown.
namespace bpr::app {
	class github_profile_runtime final : public std::enable_shared_from_this<github_profile_runtime>, public application::github_projection_port, public application::queue_window_context_port, public application::queue_landing_projection_port, public application::shared_draft_context_port, public application::window_review_restoration_projection_port
	{
	public:
		// Root lease and execution lanes outlive all services and their callbacks.
		std::shared_ptr<void>                                   StartupScratch;
		std::shared_ptr<application::profile_runtime_lease>     RuntimeLease;
		std::shared_ptr<application::profile_activation_server> ActivationServer;
		std::shared_ptr<application::tour_owner_server>         TourOwnerServer;
		std::shared_ptr<const tour_owner_service>               TourOwnerService;
#if defined(BPR_LOCAL_AUTOMATION)
		/// [[BPR-1168]]. Forwarded cycle commands, run one at a time on their own lane.
		std::shared_ptr<cycle_owner_service> CycleOwnerService;
		std::stop_source                     CycleOwnerStop;
#endif
		/// A routed command whose live draft is not yet durable; answered by the decision-lane tick.
		struct waiting_tour_request
		{
			std::string                                          Token;
			std::string                                          Json;
			application::tour_channel_bytes                      Read;
			std::function<void(application::tour_channel_bytes)> Respond;
			std::chrono::steady_clock::time_point                Deadline;
		};
		std::deque<waiting_tour_request> WaitingTourRequests;
		worker_executor                  ProviderEffects;
		worker_executor                  VaultEffects;
		worker_executor                  DeviceBindingEffects;
		worker_executor                  DeviceTransportEffects;
		worker_executor                  EpochEffects;
		worker_executor                  DiffEffects;
		worker_executor                  SearchEffects;
		/// [[BPR-1327]]. Prepares the files a reviewer is about to open: tree
		/// lookups queued on the repository actor and batched blob reads, off
		/// the actor and off the diff lane.
		worker_executor PrepareEffects;
#if defined(BPR_AGENT_INTEGRATIONS)
		worker_executor AgentEffects;
		worker_executor McpEffects;
		/// [[BPR-1900]]. The tour session's own lane, so a drafting run in the
		/// background never waits on, or holds up, the Agent tab's chat.
		worker_executor TourAgentEffects;
#endif
		worker_executor ConnectionEffects;
		worker_executor WorkbenchEffects;
		worker_executor InsightsEffects;
		worker_executor AppearanceEffects;
		worker_executor QueueEffects;
		worker_executor ReviewMutationEffects;
		/// RFC-BPR-0084. Body images: the decoder outlives the lanes that call
		/// it; four serial fetch lanes, each with its own media transport, and
		/// one decode lane; the profile's image service and cache. The disk
		/// cache is used only on the decode lane, which is joined before it goes.
		qt::markdown_image_decoder                                          ImageDecoder;
		std::unique_ptr<application::markdown_image_disk_cache_port>        ImageDisk;
		worker_executor                                                     ImageDecodeEffects;
		std::array<worker_executor, core::markdown_image_fetch_concurrency> ImageFetchEffects;
		/// [[BPR-1680]]. Home's discovery lanes, one per discovery read in
		/// flight, beside the provider's lane.
		std::array<worker_executor, application::maximum_personal_discovery_in_flight> DiscoveryEffects;
		/// [[BPR-1831]]. Image, avatar and source search ends, aggregated per
		/// minute, and the fetch port that records them; before the service
		/// that holds it.
		std::shared_ptr<application::aggregated_operation_log> OperationAggregate;
		std::unique_ptr<application::diagnosed_image_fetch>    ImageFetchDiagnostics;
		std::unique_ptr<application::markdown_image_service>   Images;
#if defined(BPR_LOCAL_AUTOMATION)
		/// [[BPR-1168]]. Forwarded cycle commands: a command blocks on its own
		/// session, never on a lane a window's work runs on.
		worker_executor CycleOwnerEffects;
#endif
		/// [[BPR-1254]]. Held by whatever touches the profile's review-ledger
		/// file: a ledger store task, or a private review store operation.
		/// Declared before the lane so it outlives the lane's last task.
		std::mutex ReviewLedgerFile;
		/// [[BPR-1211]]. Every window's review-ledger storage, off the decision lane.
		worker_executor ReviewLedgerEffects;
		/// [[BPR-1254]]. The lane above, each task under `ReviewLedgerFile`;
		/// the one every ledger host is given.
		exclusive_file_effect_executor   ReviewLedgerStoreEffects{ ReviewLedgerEffects, ReviewLedgerFile };
		private_review_recovery_registry PrivateRecovery;
		worker_executor                  ReviewSubmissionEffects;
		qt::event_loop_executor          DecisionLane;
#if defined(BPR_BUILD_TESTING)
		gated_review_mutation_decision_executor RestorationDecisionGate{ DecisionLane };
		gated_review_mutation_decision_executor WindowCloseDecisionGate{ DecisionLane };
		// A closed window can leave ledger work on this profile's effect lane.
		// Its test gate survives through that lane's drain, like DecisionLane.
		std::vector<std::unique_ptr<gated_review_mutation_decision_executor>> ReviewLedgerDecisionGates;
#endif
		std::shared_ptr<application::github_read_admission> ReadAdmission = std::make_shared<application::github_read_admission>();
		/// [[BPR-1447]]. Feedback body digests on platform crypto, taken on the
		/// provider's thread for every window's reads. Stateless, and outlives
		/// the windows, which this profile owns.
		adapters::github::platform_body_digest BodyDigests;
		struct window_observer
		{
			std::weak_ptr<const void>                                                                 Lifetime;
			std::function<void(const application::github_presentation_update&)>                       Github;
			std::function<void(const application::device_authorization_presentation&)>                Device;
			std::function<void(const application::device_authorization_refresh_observation&)>         Refresh;
			std::function<void(const application::public_preview_presentation&)>                      Preview;
			std::function<void(core::host_profile_id)>                                                SignIn;
			std::function<bool(const core::pull_request_ref&)>                                        SelectQueue;
			std::function<void(qt::settings_window&, qt::settings_page)>                              Settings;
			std::function<void()>                                                                     UnmountSettings;
			std::function<void(const core::window_review_open_request&)>                              RestoreReview;
			std::function<void(const QString&)>                                                       RestoreNotice;
			std::function<void()>                                                                     RecentChanged;
			std::function<void(const application::workbench_pull_request_authority&, ledger_changes)> Ledger;
			std::function<void(const application::github_profile_option&)>                            SetupProfile;
			std::function<void(std::string_view)>                                                     RetireServerSetup;
			std::function<void()>                                                                     AgentWorkspaceRepair;
			/// The window's read lane, for Home's discovery to give way to
			/// (`core::home_loading_yields`).
			std::function<std::optional<core::github_state>()> ReadLane;
			/// [[BPR-1850]]. The pull request the window shows while it is the
			/// active window; opening a notified pull request in it; showing the
			/// activity setting.
			std::function<std::optional<core::pull_request_ref>()>            FocusedPullRequest;
			std::function<bool(const std::optional<core::pull_request_ref>&)> OpenNotified;
			std::function<void(bool, const QString&)>                         PresentActivity;
			/// [[BPR-1867]]. Pull requests a poll found new activity on: a
			/// window showing one reads it again by itself.
			std::function<void(const std::vector<core::pull_request_ref>&)> ActivityArrived;
		};
		// [[BPR-0733]]. The recently viewed list is profile state; every window
		// projects its own view of it for the session it has.
		void recent_changed() override;
		void publish(const application::github_presentation_update& aUpdate) override;
		void adopt_discovery();
		/// \brief [[BPR-1850]]. Activity notifications: the account session they
		/// poll under, what Settings shows, the setting, and a click.
		void               adopt_activity();
		void               present_activity();
		void               set_activity_enabled(bool aEnabled);
		void               activate_activity();
		[[nodiscard]] auto activity_focus() -> std::vector<core::pull_request_ref>;
		/// \brief [[BPR-1867]]. Hands a poll's active pull requests to every window.
		void activity_arrived(const std::vector<core::pull_request_ref>& aActive);
		/// \brief Home's discovery gives way while a window waits on a read, and
		/// resumes once none does, as `core::home_loading_yields` decides.
		void yield_discovery();
		template <class Function>
		void for_each_window(Function aFunction)
		{
			const auto observers = Observers;
			for (const auto& [context, observer] : observers) {
				if (!observer.Lifetime.expired() && Observers.contains(context)) {
					aFunction(observer);
				}
			}
		}
		std::map<core::bpr_window_context, window_observer, detail::window_context_less> Observers;
		/// \brief Coalesces committed store changes onto the profile decision lane.
		[[nodiscard]] auto ledger_observer(application::workbench_pull_request_authority aAuthority, core::bpr_window_context aOrigin)
			-> std::function<void(application::ledger_change)>;
		std::unique_ptr<qt::insights_window>                  Insights;
		std::shared_ptr<application::insights_host>           InsightsHost;
		std::shared_ptr<application::insights_recorder>       InsightsRecorder;
		std::unique_ptr<qt::event_loop_metrics>               WorkMetrics;
		std::shared_ptr<application::insights_energy_sampler> EnergySampler;
		std::shared_ptr<application::process_metrics_port>    EnergyMetrics;
		std::unique_ptr<QTimer>                               EnergyTimer;
		QMetaObject::Connection                               EnergyActivation;
		std::optional<core::bpr_window_context>               InsightsOwner;
		bool                                                  InsightsInitializing = true;
		std::map<std::string, std::string>                    InsightsRepositoryLabels;
		void                                                  present_insights(core::bpr_window_context aContext, QWidget* aOpener, const qt::appearance_profile* aAppearance);
		std::unique_ptr<qt::settings_window>                  Settings;
		std::unique_ptr<release_maintenance_controls>         Maintenance;
		release_maintenance_retirement                        MaintenanceRetirement;
		std::optional<profile_data_reset_report>              DataResetReport;
		std::optional<core::bpr_window_context>               SettingsOwner;
		void                                                  present_settings(core::bpr_window_context aContext, qt::settings_page aPage);
		void                                                  present_diagnostic_export(core::bpr_window_context aContext, core::diagnostic_export_policy aPolicy);
		std::optional<core::bpr_window_context>               Focused;
		[[nodiscard]] auto                                    current(const core::bpr_window_context& aContext) const noexcept -> bool override;
		void                                                  publish(const application::queue_landing_projection& aProjection) override;
		void                                                  observe_recent_drafts(const application::queue_landing_projection& aProjection);
		class epoch_contexts final : public application::epoch_window_context_port
		{
		public:
			explicit epoch_contexts(github_profile_runtime& aOwner);
			[[nodiscard]] auto current(core::bpr_window_context aContext) const noexcept -> bool override;

		private:
			github_profile_runtime& Owner;
		};
		epoch_contexts                                EpochContexts{ *this };
		std::function<bool(core::bpr_window_context)> WindowAdmission;
		/// [[BPR-1342]]. The profile desktop's New Window, carrying one destination.
		std::function<bool(const core::window_review_destination&)> ReviewWindowRequest;
		[[nodiscard]] auto                                          current_window(core::bpr_window_context aContext) const noexcept -> bool;
		struct draft_owner
		{
			core::review_draft_scope_id                                 Scope;
			std::shared_ptr<application::shared_draft_application_host> Host;
		};
		std::vector<draft_owner>                                                 DraftOwners;
		std::uint64_t                                                            LastDraftScope = 0;
		[[nodiscard]] auto                                                       drafts(const application::shared_draft_authority& aAuthority) -> std::optional<draft_owner>;
		github_application_config                                                Config;
		core::github_identity_generation                                         QueueIdentityGeneration;
		std::optional<core::bpr_window_context>                                  FirstWindow;
		std::shared_ptr<profile_window_layout_service>                           WindowLayouts;
		application::profile_startup_admission                                   Admission;
		bool                                                                     Desktop                  = false;
		bool                                                                     RememberedStartupStarted = false;
		bool                                                                     RestorationOrderly       = false;
		std::unique_ptr<effect_window_review_store>                              ReviewRestorationStore;
		std::unique_ptr<application::cached_review_overview_port>                CachedOverviews;
		std::unique_ptr<application::window_review_restoration_host>             ReviewRestoration;
		std::optional<core::window_review_startup_request>                       RestorationStartup;
		std::map<core::bpr_window_context, QString, detail::window_context_less> RestorationNotices;
		std::optional<QString>                                                   RestorationGlobalNotice;
		std::unique_ptr<application::remembered_connection_store_port>           ConnectionStore;
		std::unique_ptr<application::remembered_connection_host>                 Connections;
		std::optional<core::github_identity_generation>                          RememberedGeneration;
		std::set<core::host_profile_id>                                          CliConsent;
		void                                                                     connection_notice(application::connection_metadata_observation aObservation);
		[[nodiscard]] auto                                                       selected_connection(std::optional<std::string_view> aHostname = std::nullopt) const -> std::optional<core::remembered_connection>;
		[[nodiscard]] auto                                                       suppress_device(std::string_view aHostname) const -> bool;
		void                                                                     observe_connection();
		/// [[BPR-1494]]. A native sign-in begins on a profile minted before its
		/// account is known. Once the host's bootstrap `/user` names the account,
		/// core decides whether this profile already holds that account's own
		/// native identity on the same server, and composition re-binds the sign-in
		/// there so the authority-scoped private state saved under it resumes.
		/// Startup rebuilds only the selected connection's profile, so a retired
		/// identity from an earlier run is registered again exactly as it was.
		[[nodiscard]] auto rebind_native_sign_in(const application::github_profile_authority& aAuthority, const application::github_viewer_record& aViewer) -> std::optional<core::host_profile_id>;
		auto               restore_cli(const core::remembered_connection& aRow) -> bool;
		void               remember_startup(std::optional<core::request_id> aRequest = std::nullopt);
		void               select_startup(const core::window_review_startup_request& aRequest) override;
		void               observe_restoration_preview();
		void               observe_restoration_session();
		void               open_review(const core::window_review_open_request& aRequest) override;
		void               fail(const core::window_review_error& aError) override;
		bool               Closing          = false;
		bool               UpdateShutdown   = false;
		bool               ServicesClosing  = false;
		bool               ShutdownComplete = false;
		bool               CleanupFailed    = false;
		// [[BPR-0875]]. Which of the profile's cleanups refused first.
		std::string CleanupDetail;
		void        note_profile_cleanup_failure(std::string aDetail);
		void        begin_shutdown();
		/// RFC-BPR-0084. A fixture run serves images from its pack and never the
		/// network; every other run fetches through native HTTPS.
		[[nodiscard]] auto markdown_image_media_factory(adapters::github::detail::github_provider::media_transport_factory aProduction)
			-> adapters::github::detail::github_provider::media_transport_factory;

		void poll_shutdown();

		explicit github_profile_runtime(
			github_application_config aConfig, application::profile_startup_admission aAdmission, std::filesystem::path aRegistryRoot,
			std::optional<application::durable_queue_state> aSavedQueue, std::unique_ptr<application::remembered_connection_store_port> aConnectionStore, application::remembered_connection_store_result<core::remembered_connection_state> aConnections, std::shared_ptr<void> aScratch, bool aDesktop,
			std::shared_ptr<application::profile_runtime_lease> aRuntimeLease
#if defined(BPR_BUILD_TESTING)
			,
			const std::optional<detail::github_public_path_fixture> aFixture        = std::nullopt,
			detail::github_public_path_fixture_options              aFixtureOptions = { }
#endif
		);
		void               start_services();
		void               initialize_identity();
		[[nodiscard]] auto credential_vault() noexcept
			-> application::github_credential_vault_port&;
		struct release_lease
		{
			application::github_credential_authority_key                          Authority;
			core::github_identity_generation                                      Generation;
			std::weak_ptr<adapters::github::detail::native_release_update_access> Access;
		};
		std::vector<release_lease> ReleaseLeases;
		unsigned                   ReleaseAccessPending = 0U;
		void                       reconcile_release_access();
		[[nodiscard]] auto         release_account() -> std::optional<release_update_account>;
		void                       acquire_release_access(application::github_credential_operation_context aContext, core::release_update_access_id aIdentity,
														  std::shared_ptr<application::release_update_archive_store>                    aStore,
														  std::function<void(std::shared_ptr<application::release_update_access_port>)> aDone);
		void                       bind_device_refresh_authority(
			const application::github_presentation_update& aUpdate);
		setup_device_enrollment::enrollment               SetupCredential;
		std::optional<core::bpr_window_context>           SetupWindow;
		std::uint64_t                                     LastSetupOwner = 0;
		std::atomic<bool>                                 AgentWorkspaceRecoveryPending{ false };
		std::atomic<bool>                                 AgentWorkspaceRecoveryPostFailed{ false };
		core::diff_compute_limits                         DiffComputeLimits;
		std::filesystem::path                             RegistryRoot;
		std::function<void(int)>                          DesktopQuit;
		std::shared_ptr<adapters::logging::quill_log>     OwnedDiagnosticLog;
		std::unique_ptr<diagnostic_controls>              Diagnostics;
		std::shared_ptr<application::diagnostic_log_port> DiagnosticLog;
		/// [[BPR-1831]]. The SQLite stores record to this profile's log while it lives.
		std::unique_ptr<adapters::sqlite::storage_diagnostics_scope> StorageDiagnostics;
		std::deque<adapters::github::validated_github_host_profile>  Profiles;
		std::vector<application::github_profile_option>              ProfileOptions;
#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<detail::github_public_path_fixture_components> Fixture;
#endif
#if defined(BPR_BUILD_TESTING)
		std::shared_ptr<detail::review_submission_fixture_state> ReviewSubmissionServer;
#endif
		std::unique_ptr<application::github_credential_vault_port>                    OwnedVault;
		std::shared_ptr<adapters::github::detail::github_wire_transport_factory_port> TransportFactory;
		std::shared_ptr<adapters::github::detail::github_wire_transport_factory_port>
			AnonymousTransportFactory;
#if defined(BPR_BUILD_TESTING)
		std::shared_ptr<shipped_anonymous_wire_transport_factory>
			ShippedAnonymousTransports;
#endif
#if defined(BPR_BUILD_TESTING)
		bool PublicPreviewSelectionAvailable = false;
#endif
		std::unique_ptr<adapters::github::detail::github_provider>        Provider;
		std::unique_ptr<adapters::github::detail::github_provider>        AnonymousProvider;
		std::unique_ptr<counting_anonymous_pull_requests>                 CountedAnonymousPullRequests;
		std::unique_ptr<anonymous_git_smart_transport_factory>            AnonymousSmartGit;
		bool                                                              PublicPreviewComposed = false;
		application::github_credential_binding_catalog                    CredentialBindings;
		std::unique_ptr<application::git_cache_port>                      GitCaches;
		std::unique_ptr<application::epoch_catalog_port>                  EpochCatalogs;
		std::filesystem::path                                             ApplicationDataRoot;
		std::filesystem::path                                             InstalledGit; // [[BPR-1177]] What Publish as Pull Request's catalog runs.
		std::unique_ptr<application::workbench_settings_port>             WorkbenchSettings;
		bool                                                              WorkbenchPersistenceAvailable = false;
		std::unique_ptr<application::appearance_settings_port>            AppearanceSettings;
		bool                                                              AppearancePersistenceAvailable = false;
		bounded_theme_document_reader                                     ThemeDocuments;
		std::unique_ptr<application::epoch_entropy_port>                  EpochEntropy;
		std::unique_ptr<application::epoch_clock_port>                    EpochClock;
		std::optional<application::checkout_observation>                  ExistingCheckout;
		bool                                                              NativePlatformVaultSelected = false;
		application::review_mutation_execution_grant                      LiveReviewMutationGrant;
		bool                                                              AttendedReviewMutationMode = false;
		std::shared_ptr<adapters::github::detail::github_profile_catalog> ProfileCatalog;
		bool                                                              RememberDeviceSignIn = true;
#if defined(BPR_BUILD_TESTING)
		std::int64_t DeviceWallOffsetSeconds = 0;
		/// [[BPR-1848]]. A course's offset to discovery's wall clock.
		std::shared_ptr<std::atomic<std::int64_t>> DiscoveryWallOffset;
#endif
#if defined(BPR_BUILD_TESTING)
		bool RegisteredDeviceCourse = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DeviceRefreshCourse = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool SuppressDeviceBrowser = false;
		// [[BPR-1338]]. See `github_public_path_fixture_options`.
		bool AttendedWindowBrowser = false;
		bool AttendedAgentSearch   = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool LabelHelper = false;
		/// [[BPR-1088]]. Set once the first window asked the profile's
		/// preferences for the helper, so later windows do not ask again.
		bool LabelHelperRequested = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableDeviceAuthorizationPanelComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool NetworkDenied = false;
		std::optional<std::vector<detail::github_public_path_fixture_options::github_cli_discovery_account>>
											GithubCliDiscoveryAccounts;
		std::shared_ptr<std::atomic_size_t> GithubCliInventoryChecks;
#endif
		/// [[BPR-1373]]. gh's full inventory from the last check, and whether
		/// gh has been checked in this run.
		std::vector<adapters::github::detail::gh_cli_inventory_account> GithubCliInventory;
		bool                                                            GithubCliInventoryChecked = false;
		/// [[BPR-1373]]. Records this profile's pinned gh account, if its
		/// accepted session is on one, for every profile's "Used by".
		void record_cli_account_use();
#if defined(BPR_BUILD_TESTING)
		// [[BPR-0797]]. Whether this run is standing inside the automated test
		// boundary. A tests-disabled graph has no member, because it has no
		// question: it holds one browser capability and one CLI answer, and
		// both are the real ones.
		bool InsideAutomatedTestBoundary = false;
#endif
		// [[BPR-0797]]. Which browser capability this run bound, reported so a
		// course can assert the identity rather than the mere presence.
		composed_device_browser ComposedDeviceBrowser
			= composed_device_browser::none;
		// [[BPR-0797]]. The run-time form of the question the two GitHub CLI
		// gates used to put to the preprocessor.
		[[nodiscard]] auto cli_read_offered_here() const noexcept -> bool;
#if defined(BPR_BUILD_TESTING)
		bool PublicPreviewEnabledCourse = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool PublicPreviewGhesCourse = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableWorkbenchSelectionWire = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableReviewLedgerGestureWire = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableAgentProcessLauncherComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableAgentHostComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableAgentGestureWire = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableAgentFindingHostComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableChatSessionHostComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableCommentWorkshopComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableMcpHelperComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableSideChatPanelComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableFindingQueueComposition = false;
#endif
#if defined(BPR_BUILD_TESTING)
		std::optional<std::filesystem::path> ExternalAgentExecutable;
#endif
#if defined(BPR_BUILD_TESTING)
		/// [[BPR-1879]]. The attended Repository-step look shows a fixture
		/// review's agent setup as a review read sparsely through `gh`.
		bool                                             SparseAgentSetupLook = false;
		std::optional<std::filesystem::path>             CodexAgentExecutable;
		std::optional<std::filesystem::path>             ClaudeAgentExecutable;
		std::optional<detail::claude_search_environment> ClaudeSearch;
		std::optional<detail::claude_search_environment> CodexSearch;
#endif
#if defined(BPR_BUILD_TESTING)
		bool UseGrantedNotificationSink = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool CaptureSystemNotificationDisplay = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableNotificationRequestWire = false;
#endif
#if defined(BPR_BUILD_TESTING)
		bool DisableNotificationSelectionWire = false;
#endif
		std::unique_ptr<composition_github_projection> GithubProjectionFanOut;
		public_preview_projection_sink                 PublicPreviewProjections;
		std::unique_ptr<composition_public_preview_projection>
																 PublicPreviewProjectionFanOut;
		std::unique_ptr<application::device_binding_store_port>  DeviceBindingStore;
		std::unique_ptr<application::device_binding_persistence> DevicePersistence;
		core::github_identity_generation                         DeviceRefreshGeneration;
		unregistered_device_authorization_transport              UnregisteredDeviceTransport;
		std::unique_ptr<adapters::github::detail::github_device_authorization_transport>
														DeviceWireTransport;
		bool                                            DeviceCapabilityRegistered = false;
		core::host_profile_id                           DeviceCapabilityProfile;
		bool                                            DeviceBrowserComposed = false;
		application::device_authorization_browser_port* ComposedBrowser       = nullptr;
		// [[BPR-1338]]. The opener the window's own gestures use: Open on
		// GitHub and body links. `ComposedBrowser` except in an attended
		// fixture look, where a person is watching for the page.
		application::device_authorization_browser_port* WindowBrowser = nullptr;
		system_browser_opener                           SystemBrowser;
#if defined(BPR_BUILD_TESTING)
		recording_device_authorization_browser RecordingBrowser;
#endif
		qt::event_loop_device_timer          DeviceTimer;
		device_authorization_projection_sink DeviceProjections;
		std::unique_ptr<composition_device_authorization_projection>
			DeviceProjectionFanOut;
		std::optional<application::github_credential_authority_key>
					DeviceRefreshAuthority;
		bool        DeviceStageAuthorized     = false;
		std::size_t DeviceRefreshBindings     = 0U;
		std::size_t DeviceRefreshResumes      = 0U;
		std::size_t DeviceRefreshPublications = 0U;
		application::device_authorization_refresh_observation
			DeviceRefreshObservation;
		std::unique_ptr<application::device_authorization_clock_port>
																  DeviceClock;
		std::unique_ptr<application::queue_enrollment_store_port> QueueStore;
		std::unique_ptr<composition_queue_refresh>                QueueRefresh;
		// [[BPR-0990]]. The profile's one discovery scheduler and its clock.
		// Constructed after the queue host and destroyed before it.
		std::unique_ptr<application::personal_discovery_clock_port> DiscoveryClock;
		bool                                                        DiscoveryReconcilePosted = false;
		bool                                                        DiscoveryResumePosted    = false;
		std::optional<queue_activity_baseline>                      QueueActivityBaseline;
		std::optional<queue_activity_fact>                          QueueActivity;
		std::vector<queue_notification_transition>                  ActiveQueueNotifications;
		std::unique_ptr<application::notification_sink_port>        NotificationSink;
		qt::system_tray_notification_sink*                          SystemNotificationSink = nullptr;
		std::unique_ptr<application::review_queue_notifier>         Notifications;
		std::size_t                                                 NotificationRequestsAttempted = 0U;
		std::size_t                                                 NotificationSelections        = 0U;
		std::size_t                                                 NotificationSelectionFailures = 0U;
#if defined(BPR_BUILD_TESTING)
		fixture_notification_sink* FixtureNotificationSink = nullptr;
#endif
#if defined(BPR_BUILD_TESTING)
		std::size_t CapturedNotificationDisplays = 0U;
#endif
		std::unique_ptr<adapters::syntax::packaged_definition_catalog> SyntaxDefinitions;
		std::unique_ptr<QTemporaryDir>                                 AgentWorkspaceRoot;
		std::filesystem::path                                          AgentWorkspaceNeighbourhood;
		std::optional<adapters::git::installed_agent_workspace_parent>
			 AgentWorkspaceParent;
		bool AgentWorkspaceCleanupAttempted = false;
		bool AgentWorkspaceCleanupFailed    = false;
		// [[BPR-0875]]. The workspace recovery's refusal, as its enum value, or
		// -1 when it did not refuse. A value, not a string, because it is set on
		// a posted lane. [[BPR-0817]]: a refusal is presented in every window and
		// reported in the status, and it is not a cleanup failure of the quit.
		int  AgentWorkspaceRecoveryFailure = -1;
		bool AgentWorkspaceRecoveryRefused = false;
		/// [[BPR-1857]]. What the refusal says to the reviewer: the check, the
		/// folder and what to do. Empty keeps the general sentence.
		QString AgentWorkspaceRecoveryReason;
		// Dependents are destroyed before their retained ports and execution lanes.
		std::unique_ptr<application::github_application_host> Host;
		std::unique_ptr<application::github_host_device_authorization_enrollment>
												 DeviceEnrollment;
		std::unique_ptr<setup_device_enrollment> SetupDeviceEnrollment;
#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<counting_device_authorization_enrollment>
			CountedDeviceEnrollment;
#endif
		std::unique_ptr<application::device_authorization_application_host>
																			  DeviceHost;
		std::unique_ptr<application::profile_epoch_actor_registry>            EpochActors;
		std::unique_ptr<application::workbench_preferences_host>              WorkbenchPreferences;
		std::shared_ptr<application::appearance_preferences_application_host> AppearancePreferences;
		std::unique_ptr<application::review_queue_application_host>           QueueHost;
		std::unique_ptr<application::personal_discovery_host>                 Discovery;
		// [[BPR-1844]]. The store outlives the host that writes through it.
		std::unique_ptr<view_filter_memory_store>             ViewFilterStore;
		std::unique_ptr<application::view_filter_memory_host> ViewFilterMemory;
		// [[BPR-1850]]. The store, sink and timer outlive the host.
		std::unique_ptr<pull_request_activity_store>                 ActivityStore;
		std::unique_ptr<qt::pull_request_activity_notification_sink> ActivitySink;
		std::unique_ptr<qt::pull_request_activity_timer>             ActivityTimer;
		std::unique_ptr<application::pull_request_activity_host>     Activity;
	};
}
#endif
