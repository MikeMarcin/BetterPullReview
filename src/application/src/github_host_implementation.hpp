#ifndef BPR_SRC_APPLICATION_SRC_GITHUB_HOST_IMPLEMENTATION_HPP
#define BPR_SRC_APPLICATION_SRC_GITHUB_HOST_IMPLEMENTATION_HPP
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/operation_diagnostics.hpp"
#include "github_host_support.hpp"
#include "github_projection_mapping.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// The GitHub application host's state and its profile, credential and capability lanes
// (github_application_host::implementation).
namespace bpr::application {
	struct github_application_host::implementation final
	{
		enum class viewer_mode : std::uint8_t {
			none,
			cli,
			bootstrap,
			account,
			// Signed out. No viewer is ever requested in this mode, so the
			// enumerator names the absence of a viewer course rather than a
			// fourth way of obtaining one.
			public_preview,
		};

		enum class vault_operation : std::uint8_t {
			none,
			store,
			retrieve,
			delete_credential,
		};

		// [[BPR-0547]]. Whether an occasion's credential delete carries the
		// refresh slot's erasure with it. Erasing is what [[BPR-0540]]
		// established and what every occasion asks for but one, so it is the
		// default and the exception is named where it is known.
		enum class refresh_disposition : std::uint8_t {
			erase,
			retain,
		};

		enum class credential_lifecycle : std::uint8_t {
			none,
			provisional,
			persistent,
		};

		struct deferred_session_terminal
		{
			core::github_session_request_context  Context;
			std::optional<core::github_failure>   Failure;
			github_bootstrap_failure_stage        Stage = github_bootstrap_failure_stage::none;
			std::optional<github_host_diagnostic> Diagnostic;
		};

		struct session_candidate
		{
			core::github_session_request_context         Context;
			github_profile_authority                     ProfileAuthority;
			github_viewer_record                         Viewer;
			github_capability_cache_entry                CapabilityEvidence;
			github_account_transport                     AccountTransport;
			github_rate_limit_state                      RateLimits;
			std::optional<github_capability_cache_stage> CacheStage;
			core::github_access_mode                     Mode{ };
		};

		struct staged_enrollment
		{
			core::pending_setup_destination                                   Pending;
			std::uint64_t                                                     Attempt;
			core::github_identity_generation                                  OriginGeneration;
			github_credential_material                                        Credential;
			std::function<bool(const core::pending_setup_destination&, bool)> Accept;
			github_credential_provenance                                      Provenance;
			std::optional<device_binding_lifetime>                            Lifetime;
			bool                                                              RefreshSettled;
			std::variant<github_profile_request, github_viewer_request>       Operation;
		};

		struct deferred_dispatch
		{
			core::github_event Event;
		};

		struct deferred_enrollment
		{
			core::host_profile_id        Profile;
			github_credential_material   Credential;
			github_credential_provenance Provenance
				= github_credential_provenance::manual;
			std::optional<device_binding_lifetime> Lifetime;
			bool                                   RefreshSettled = false;
			github_enrollment_rebind               Rebind;
			std::optional<core::github_account_id> ExpectedAccount;
		};
		struct deferred_grant_invalidation
		{
			github_credential_authority_key  Authority;
			core::github_identity_generation Generation;
			core::github_failure             Failure = core::github_failure::authentication;
		};

		struct deferred_resume
		{
			core::host_profile_id Profile;
		};

		struct deferred_public_preview
		{
			core::host_profile_id Profile;
		};

		struct deferred_shutdown
		{
		};

		using deferred_ingress = std::variant<deferred_dispatch,
											  deferred_grant_invalidation,
											  deferred_enrollment,
											  deferred_resume,
											  deferred_public_preview,
											  deferred_shutdown>;

		using pending_provider_request = std::variant<std::monostate,
													  github_profile_request,
													  github_viewer_request,
													  github_capability_request,
													  github_pull_request_provider_request>;

		struct completion_gate
		{
			std::recursive_mutex         Barrier;
			std::atomic<implementation*> Host = nullptr;
		};

		implementation(core::github_state                 aInitialState,
					   github_profile_port&               aProfiles,
					   github_viewer_port&                aViewers,
					   github_capability_port&            aCapabilities,
					   github_pull_request_port&          aPullRequests,
					   github_credential_vault_port&      aCredentials,
					   github_credential_binding_catalog& aCredentialBindings,
					   decision_executor&                 aDecisionLane,
					   github_projection_port&            aProjections,
					   const github_credential_shutdown_policy
														 aCredentialShutdownPolicy,
					   github_epoch_handoff_port* const  aEpochs,
					   github_pull_request_port* const   aAnonymousPullRequests,
					   git_smart_transport_factory_port* aAnonymousSmartGit,
					   public_preview_projection_port* aPublicPreview, core::bpr_profile_id aBprProfile);

		~implementation();

		// [[BPR-1302]]. Nothing this host started is still owed an answer: no
		// provider, vault, binding, setup or restore operation and no read
		// window's work. Shutdown quiescence and running idleness are this one
		// fact under opposite lifecycles, so both predicates read it here.
		[[nodiscard]] auto outstanding_work_settled() const noexcept -> bool;

		template <typename Action>
		void run_turn(Action&& aAction)
		{
			TurnActive = true;
			try {
				std::forward<Action>(aAction)();
				const auto deferredCount = DeferredIngress.size();
				for (std::size_t index = 0U; index < deferredCount; ++index) {
					auto deferred = std::move(DeferredIngress.front());
					DeferredIngress.pop_front();
					std::visit(
						[this](auto&& aIngress) {
							using ingress_type = std::decay_t<decltype(aIngress)>;
							if constexpr (std::is_same_v<ingress_type,
														 deferred_dispatch>) {
								dispatch_now(aIngress.Event);
							} else if constexpr (std::is_same_v<ingress_type, deferred_grant_invalidation>) {
								invalidate_device_grant_now(aIngress.Authority, aIngress.Generation, aIngress.Failure);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_enrollment>) {
								enroll_profile_now(aIngress.Profile,
												   std::move(aIngress.Credential),
												   aIngress.Provenance, aIngress.Lifetime, aIngress.RefreshSettled, std::move(aIngress.Rebind), aIngress.ExpectedAccount);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_resume>) {
								resume_profile_now(aIngress.Profile);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_public_preview>) {
								activate_public_preview_now(aIngress.Profile);
							} else {
								shutdown_now();
							}
						},
						std::move(deferred));
				}
				fail_withdrawn_bindings();
				TurnActive = false;
				schedule_deferred_ingress();
			} catch (...) {
				DeferredIngress.clear();
				TurnActive = false;
				// [[BPR-1112]]. A turn that ends in an exception still fails the
				// requests the lane withdrew during it, before the exception
				// reaches its caller. A later turn may never come: after
				// `Closing` almost nothing starts one.
				fail_withdrawn_bindings();
				throw;
			}
		}

		void schedule_deferred_ingress() noexcept;

		void dispatch(const core::github_event& aEvent);

		void invalidate_device_grant_now(const github_credential_authority_key& aAuthority, const core::github_identity_generation aGeneration, const core::github_failure aFailure);

		void end_accepted_renewal(const core::github_failure aFailure);

		void dispatch_now(const core::github_event& aEvent);

		void enroll_profile(const core::host_profile_id                  aProfile,
							github_credential_material                   aCredential,
							const github_credential_provenance           aProvenance,
							const std::optional<device_binding_lifetime> aLifetime,
							const bool                                   aRefreshSettled,
							github_enrollment_rebind                     aRebind,
							const std::optional<core::github_account_id> aExpectedAccount = std::nullopt);

		void enroll_profile_now(const core::host_profile_id                  aProfile,
								github_credential_material                   aCredential,
								const github_credential_provenance           aProvenance,
								const std::optional<device_binding_lifetime> aLifetime,
								const bool                                   aRefreshSettled,
								github_enrollment_rebind                     aRebind          = { },
								const std::optional<core::github_account_id> aExpectedAccount = std::nullopt);

		void cancel_setup_enrollment();

		template <typename TCompletion, typename THandler>
		auto setup_callback(THandler aHandler) -> std::function<void(TCompletion)>
		{
			++OutstandingSetup;
			const auto gate      = std::weak_ptr<completion_gate>{ CompletionGate };
			auto*      lane      = &DecisionLane;
			const auto delivered = std::make_shared<std::atomic<bool>>(false);
			return [gate, lane, delivered, handler = std::move(aHandler)](TCompletion aCompletion) mutable {
				if (delivered->exchange(true)) {
					return;
				}
				const auto locked = gate.lock();
				if (!locked) {
					return;
				}
				const std::lock_guard barrier{ locked->Barrier };
				if (locked->Host.load() == nullptr) {
					return;
				}
				const auto payload = std::make_shared<TCompletion>(std::move(aCompletion));
				lane->post([gate, payload, laneHandler = std::move(handler)]() mutable {
					const auto target = gate.lock();
					if (!target) {
						return;
					}
					const std::lock_guard callbackBarrier{ target->Barrier };
					auto*                 host = target->Host.load();
					if (!host) {
						return;
					}
					--host->OutstandingSetup;
					host->run_turn([&] { laneHandler(*host, std::move(*payload)); });
				});
			};
		}

		void finish_setup_failure(const core::setup_request_id aRequest);

		void stage_setup_enrollment(core::pending_setup_destination aPending, github_credential_material aCredential,
									std::function<bool(const core::pending_setup_destination&, bool)> aAccept,
									const github_credential_provenance aProvenance, std::optional<device_binding_lifetime> aLifetime,
									const bool aRefreshSettled);

		void stage_setup_viewer(const core::setup_request_id aIdentity, const github_profile_authority& aAuthority,
								const core::github_session_request_context& aContext);

		void resume_profile(const core::host_profile_id aProfile);

		void resume_profile_now(const core::host_profile_id aProfile);

		void activate_public_preview(const core::host_profile_id aProfile);

		void activate_public_preview_now(const core::host_profile_id aProfile);

		[[nodiscard]] auto signed_out_mode() const noexcept -> bool;

		[[nodiscard]] auto is_public_preview() const noexcept -> bool;

		[[nodiscard]] auto anonymous_capability_evidence(
			const github_profile_authority& aAuthority) const noexcept
			-> github_capability_cache_entry;

		void begin_public_preview(
			const core::github_session_request_context& aContext,
			const github_profile_authority&             aAuthority);

		auto current_profile_session() const -> std::optional<github_profile_session>;
		void retire_windows();
		void invalidate_epoch_authority() noexcept;
		void revoke_review_mutation_transport() noexcept;
		void read(const core::github_window_read_event& aEvent);
		struct window_slot
		{
			core::bpr_window_context                 Context;
			core::application_session_id             Session;
			std::optional<github_profile_authority>  Authority;
			core::github_account_id                  Account;
			std::shared_ptr<github_window_read_host> Host;
			bool                                     Detaching = false;
		};
		bool attach_window(const std::shared_ptr<github_window_read_host>& aWindow);
		void observe_attached_read(const std::shared_ptr<github_window_read_host>& aWindow, const bool aFreshSnapshot);
		void detach_window(const core::bpr_window_context aContext);

		void publish_profile_session();

		void observe_account() noexcept;

		void dispatch_core(const core::github_event& aEvent, std::optional<github_host_diagnostic_projection> aFailureDiagnostic = std::nullopt);

		void publish_and_execute(const core::github_transition_output& aOutput, std::optional<github_host_diagnostic_projection> aFailureDiagnostic = std::nullopt);

		void publish_transition(
			const core::github_transition_output&            aOutput,
			const github_session_read_model* const           aSession           = nullptr,
			const github_pull_request_snapshot* const        aPullRequest       = nullptr,
			std::optional<github_host_diagnostic_projection> aFailureDiagnostic = std::nullopt) noexcept;

		void publish_diagnostic(const github_host_diagnostic& aDiagnostic, const github_bootstrap_failure_stage aStage = github_bootstrap_failure_stage::none) noexcept;

		[[nodiscard]] static auto projected_refusal_reason(
			const github_public_preview_refusal& aRefusal) noexcept
			-> public_preview_refusal_reason;

		[[nodiscard]] static auto project_review_comment(
			const github_review_comment_record& aComment)
			-> public_preview_review_comment;

		[[nodiscard]] static auto project_review_comment_group(
			const github_review_comment_group_record& aGroup)
			-> public_preview_review_comment_group;

		[[nodiscard]] auto project_public_preview() const
			-> public_preview_presentation;

		void publish_public_preview() noexcept;

		void observe_provider_error(const github_provider_error& aError);

		void publish_error_advisory(const github_provider_error& aError);

		[[nodiscard]] auto requires_shutdown_credential_cleanup() const noexcept
			-> bool;

		void execute(const core::github_host_request& aRequest);

		void invalidate(const core::invalidate_github_identity_request& aRequest);

		auto select_persistent_credential(
			const github_credential_authority_key&      aAuthority,
			const core::github_session_request_context& aContext) -> bool;

		void start_deferred_session();

		void submit_profile(const github_profile_request& aRequest);

		void handle_profile(const std::uint64_t       aToken,
							github_profile_completion aCompletion);

		void handle_profile_now(const std::uint64_t       aToken,
								github_profile_completion aCompletion);

		void submit_bootstrap_viewer(const github_viewer_request& aRequest);

		void submit_viewer(const github_viewer_request& aRequest,
						   github_viewer_authentication aAuthentication);

		void handle_viewer(const std::uint64_t      aToken,
						   github_viewer_completion aCompletion);

		void handle_viewer_now(const std::uint64_t      aToken,
							   github_viewer_completion aCompletion);

		[[nodiscard]] auto next_epoch() -> std::optional<github_freshness_epoch>;

		void begin_capabilities(const core::github_session_request_context& aContext,
								const github_profile_authority&             aProfile,
								const github_viewer_record&                 aViewer,
								const github_account_transport&             aTransport);

		void submit_capability(const github_capability_request& aRequest);

		void handle_capability(const std::uint64_t          aToken,
							   github_capability_completion aCompletion);

		void handle_capability_now(const std::uint64_t          aToken,
								   github_capability_completion aCompletion);

		void start_refresh_probe();

		void handle_refresh_probe(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedProbe, const bool aPresent);

		void handle_refresh_probe_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedProbe, const bool aPresent);

		[[nodiscard]] auto grant_projection() const noexcept
			-> std::optional<github_grant_projection>;

		void republish_accepted_session() noexcept;

		void accept_session(session_candidate aCandidate);

		void start_store(const core::github_session_request_context& aContext);

		void
		handle_store(const std::uint64_t                        aToken,
					 const github_credential_operation_context& aExpectedOperation,
					 github_credential_store_completion         aCompletion);

		void handle_store_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_store_completion         aCompletion);

		void start_retrieve(const github_provider_attempt aAttempt);

		void handle_retrieve(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_retrieve_completion      aCompletion);

		void handle_retrieve_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_retrieve_completion      aCompletion);

		void start_refresh_delete(
			const github_credential_operation_context& aOperation);

		void hold_retained_refresh(
			const github_credential_operation_context& aOperation);

		void erase_retained_refresh();

		void resolve_retained_refresh(
			const github_credential_authority_key& aAdopted);

		void report_refresh_erasure_failure(
			const github_credential_operation_context& aOperation);

		void start_delete_unblocked(const core::github_identity_generation   aGeneration,
									std::optional<deferred_session_terminal> aTerminal,
									const refresh_disposition                aRefresh
									= refresh_disposition::erase);

		void
		handle_delete(const std::uint64_t                        aToken,
					  const github_credential_operation_context& aExpectedOperation,
					  const refresh_disposition                  aRefresh,
					  github_credential_delete_completion        aCompletion);

		void handle_delete_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			const refresh_disposition                  aRefresh,
			github_credential_delete_completion        aCompletion);

		void fail_delete_operation();

		void fail_capability_session(
			const core::github_session_request_context& aContext,
			const core::github_failure                  aFailure,
			const github_capability_diagnostic          aDetail,
			std::optional<github_host_diagnostic>       aDiagnostic = std::nullopt);

		void keep_renewed_launch_grant(const core::github_session_request_context& aContext);

		void fail_session(const core::github_session_request_context& aContext,
						  const core::github_failure                  aFailure,
						  const bool                                  aDeletePersistentCredential = false,
						  const github_bootstrap_failure_stage        aStage                      = github_bootstrap_failure_stage::none,
						  std::optional<github_host_diagnostic>       aDiagnostic                 = std::nullopt);

		void
		fail_session_direct(const core::github_session_request_context&  aContext,
							const core::github_failure                   aFailure,
							const github_bootstrap_failure_stage         aStage      = github_bootstrap_failure_stage::none,
							const std::optional<github_host_diagnostic>& aDiagnostic = std::nullopt);

		[[nodiscard]] static auto account_mismatch() -> github_host_diagnostic;

		[[nodiscard]] auto remembered_device_grant() const noexcept -> bool;

		[[nodiscard]] auto remembered_grant_kept() const noexcept -> bool;

		void cancel_session(const core::github_session_request_context& aContext);

		void finish_deferred_terminal(const deferred_session_terminal& aTerminal);

		void reset_session_scratch();

		void cancel_provider();

		void cancel_vault();

		void cancel_refresh_probe();

		void shutdown();

		void shutdown_now();

		// [[BPR-1116]]. How an accepted renewal that gave up ends its session:
		// the renewal's failure, and whether the binding's row was durable.
		struct kept_session_ending
		{
			core::github_failure Failure = core::github_failure::authentication;
			bool                 Durable = false;
		};

		struct restore_completion
		{
			std::function<void(device_profile_restore_status)> Callback;
			bool                                               Scheduled = false;
		};

		void complete_restore(const std::shared_ptr<restore_completion>& aCompletion,
							  const device_profile_restore_status        aStatus) noexcept;

		template <typename TCallback>
		auto binding_callback(TCallback aCallback)
		{
			return [gate = std::weak_ptr<completion_gate>{ CompletionGate }, callback = std::move(aCallback)](auto aResult) mutable {
				const auto locked = gate.lock();
				if (!locked || locked->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				auto* host = locked->Host.load(std::memory_order_acquire);
				host->run_turn([&callback, &aResult] { callback(std::move(aResult)); });
			};
		}

		// [[BPR-1048]]. Calls the persistence lane with `aFinish` as the
		// request's terminal. The caller has already counted the request, and
		// may have set a pending flag. The lane throws only when it withdrew the
		// request: it could post neither the effect nor its terminal, and no
		// terminal follows ([[BPR-0884]]). A withdrawal is a failure, never a
		// success ([[BPR-0541]]), so `aFinish` gets the terminal a refused
		// effect gets, `unavailable`, and undoes the count and the flag itself.
		// The throw is not passed on: the request has had its one terminal.
		template <typename TCall>
		void call_binding_lane(TCall&& aCall, std::function<void(device_binding_result)> aFinish)
		{
			try {
				std::forward<TCall>(aCall)(binding_callback(aFinish));
			} catch (...) {
				fail_withdrawn_binding(std::move(aFinish));
			}
		}

		void fail_withdrawn_binding(std::function<void(device_binding_result)> aFinish);

		void fail_withdrawn_bindings();

		void binding_failure();

		void begin_launch_renewal();

		void end_launch_renewal(const core::github_failure aFailure);

		void keep_launch_renewal_slot(const github_credential_authority_key& aAuthority, const core::github_session_request_context& aContext);

		void adopt_launch_renewal_slot(const github_credential_authority_key& aAuthority, const core::github_session_request_context& aContext);

		void release_launch_renewal() noexcept;

		[[nodiscard]] auto owns_launch_renewal(const github_credential_authority_key& aAuthority, const core::github_identity_generation aGeneration) const noexcept -> bool;

		void persist_settled_device_binding();

		void start_delete(const core::github_identity_generation   aGeneration,
						  std::optional<deferred_session_terminal> aTerminal,
						  const refresh_disposition                aRefresh = refresh_disposition::erase);

		device_binding_persistence*          Persistence = nullptr;
		std::function<std::int64_t()>        WallClock;
		std::optional<device_binding_record> PendingDeviceRestore;
		bool                                 PendingDeviceRestoreRenews  = false;
		bool                                 PendingDeviceRestoreExpired = false;
		device_launch_renewal_sink           LaunchRenewalSink;
		std::optional<device_launch_renewal> LaunchRenewal;
		// The sink was asked to renew, so the device host holds a chain for it.
		bool LaunchRenewalBegun = false;
		// The selected provisional credential is a launch renewal's rotated
		// grant, whose `/user` named the binding's account and which has not
		// been accepted yet.
		bool LaunchRenewedGrant = false;
		// The rotated grant has enrolled and waits for its `/user` account.
		bool                                                    LaunchRenewalEnrolled = false;
		std::optional<device_binding_lifetime>                  NextDeviceLifetime;
		std::optional<device_binding_lifetime>                  DeviceLifetime;
		bool                                                    NextRefreshSettled    = false;
		bool                                                    DeviceRefreshSettled  = false;
		bool                                                    DeviceBindingDurable  = false;
		bool                                                    BindingStorePending   = false;
		bool                                                    BindingLoadPending    = false;
		bool                                                    BindingLoadUnresolved = false;
		std::optional<core::invalidate_github_identity_request> PendingRestoreCleanup;
		std::optional<github_credential_operation_context>      StartupCleanupOperation;
		bool                                                    BindingErasePending   = false;
		bool                                                    RestoredDeviceBinding = false;
		std::uint64_t                                           BindingGeneration     = 0;
		std::uint64_t                                           NextSetupAttempt      = 1U;
		std::optional<staged_enrollment>                        StagedEnrollment;
		std::optional<github_viewer_success>                    AcceptedSetupBootstrap;
		std::size_t                                             OutstandingSetup = 0U;

		// [[BPR-1829]]. Sign-in lines, and the account the last one saw
		// (compared only, never logged).
		std::shared_ptr<diagnostic_log_port>     DiagnosticLog;
		std::optional<core::github_account_id>   RecordedAccount;
		std::size_t                              OutstandingBindings           = 0;
		std::size_t                              OutstandingRestoreCompletions = 0;
		bool                                     RestoreIngressClosing         = false;
		core::github_identity_generation         BindingDeleteGeneration;
		std::optional<deferred_session_terminal> BindingDeleteTerminal;
		refresh_disposition                      BindingDeleteRefresh = refresh_disposition::erase;

		// [[BPR-1048]]. Terminals of binding requests the lane withdrew during
		// the current turn, failed at its end, or before its exception leaves
		// it when it ends in one ([[BPR-1112]]). Each is still counted, so the
		// host is not quiescent before they run. Queueing moves a
		// `std::function` into a free slot and allocates nothing; when every
		// slot is taken the failure is delivered at once.
		std::array<std::optional<std::function<void(device_binding_result)>>, 8> WithdrawnBindings;
		std::size_t                                                              WithdrawnBindingCount = 0U;

		std::shared_ptr<github_window_read_host> ReadWindow;
		std::vector<window_slot>                 AttachedWindows;
		std::uint64_t                            SharedSnapshotSequence = 0U;
		std::optional<github_profile_session>    PublishedSession;
		bool                                     PrimaryReadClosed       = false;
		bool                                     ExplicitPrimaryAttached = false;
		core::bpr_window_id                      LastAttachedWindow{ };
		std::array<core::bpr_window_id, 32>      RestoredWindows{ };
		std::size_t                              RestoredWindowCount = 0;
		bool                                     RestorationSealed   = false;
		core::github_state                       State;
		github_profile_port&                     Profiles;
		github_viewer_port&                      Viewers;
		github_capability_port&                  Capabilities;
		github_pull_request_port&                PullRequests;
		github_credential_vault_port&            Credentials;
		github_credential_binding_catalog&       CredentialBindings;
		const core::bpr_profile_id               BprProfile;
		github_capability_cache                  CapabilityCache;
		decision_executor&                       DecisionLane;
		github_projection_port&                  Projections;
		github_credential_shutdown_policy        CredentialShutdownPolicy = github_credential_shutdown_policy::retain;
		github_epoch_handoff_port*               Epochs                   = nullptr;
		github_pull_request_port*                AnonymousPullRequests    = nullptr;
		git_smart_transport_factory_port*        AnonymousSmartGit        = nullptr;
		public_preview_projection_port*          PublicPreview            = nullptr;
		public_preview_sign_in_sink              PublicPreviewSignIn;
		// The profile the signed-out session was activated for, kept so a
		// sign-in gesture names the host the reviewer is already reading
		// rather than whichever one a later picker change would resolve to.
		core::host_profile_id PublicPreviewProfile{ };
		// The last presentation delivered. Publication is by change, so the
		// four occasions the port promises are the occasions something
		// actually differs on, and a repeated observation is not a repeated
		// announcement.
		std::optional<public_preview_presentation> PublishedPublicPreview;
		std::shared_ptr<completion_gate>           CompletionGate;
		std::deque<deferred_ingress>               DeferredIngress;
		bool                                       TurnActive               = false;
		bool                                       DeferredIngressScheduled = false;

		pending_provider_request                            PendingProvider;
		std::uint64_t                                       ProviderOperationToken = 0U;
		std::optional<github_credential_material>           NextEnrollment;
		github_credential_provenance                        NextEnrollmentProvenance = github_credential_provenance::manual;
		std::optional<core::host_profile_id>                NextEnrollmentProfile;
		std::optional<github_credential_material>           SessionEnrollment;
		github_credential_provenance                        SessionEnrollmentProvenance = github_credential_provenance::manual;
		github_enrollment_rebind                            NextEnrollmentRebind;
		github_enrollment_rebind                            SessionEnrollmentRebind;
		std::optional<core::github_account_id>              NextExpectedAccount;
		std::optional<core::github_account_id>              SessionExpectedAccount;
		std::optional<core::host_profile_id>                NextResumeProfile;
		std::optional<core::host_profile_id>                SessionResumeProfile;
		std::optional<kept_session_ending>                  NextKeptEnding;
		std::optional<kept_session_ending>                  SessionKeptEnding;
		bool                                                NextPublicPreview    = false;
		bool                                                SessionPublicPreview = false;
		std::optional<github_public_preview_refusal>        PublicPreviewRefusal;
		std::optional<core::github_session_request_context> DeferredSession;
		std::optional<github_profile_authority>             CurrentProfile;
		std::optional<github_viewer_record>                 BootstrapViewer;
		viewer_mode                                         ViewerMode = viewer_mode::none;
		github_account_transport                            AccountTransport;

		vault_operation                                    ActiveVaultOperation = vault_operation::none;
		std::optional<github_credential_operation_context> ActiveVaultContext;
		std::uint64_t                                      VaultOperationToken = 0U;
		github_provider_attempt                            PendingRetrieveAttempt{ 1U };
		// The renewability probe runs beside the vault operation state rather
		// than inside it. It answers a question about a session that has
		// already been accepted, so it can neither advance nor abandon one,
		// and giving it its own token keeps the single-operation invariant the
		// establishment path depends on exactly as it was.
		std::optional<github_credential_operation_context> RefreshProbeContext;
		std::uint64_t                                      RefreshProbeToken = 0U;
		// The refresh slot's erasure runs beside the credential delete rather
		// than inside it, for the same reason the probe does: the
		// single-operation invariant the establishment path depends on is
		// about the credential slot. It is counted rather than tracked by
		// context because the only question anything asks of it is whether
		// the vault still owes this host a terminal, and quiescence is asked
		// from outside the decision lane. Decremented under the completion
		// gate's barrier, which is the same place every other vault terminal
		// establishes that this host is still alive.
		std::atomic<std::uint32_t>                     OutstandingRefreshDeletes   = 0U;
		bool                                           AcceptedCredentialRenewable = false;
		bool                                           RefreshSlotMissing          = false;
		std::optional<github_credential_authority_key> CredentialAuthority;
		std::optional<core::github_session_request_context>
												 CredentialSessionContext;
		credential_lifecycle                     CredentialLifecycle         = credential_lifecycle::none;
		github_credential_provenance             CredentialProvenance        = github_credential_provenance::manual;
		bool                                     CredentialBindingRemembered = false;
		std::optional<deferred_session_terminal> DeferredTerminal;
		std::optional<core::github_identity_generation>
			 DeferredInvalidationGeneration;
		bool DeferredInvalidationPreservesCredential = false;
		bool DeferredInvalidationRetainsRefresh      = false;
		// [[BPR-0549]]. The outgoing refresh slot whose erasure the retention
		// above deferred, carried as the exact context that would have erased
		// it until the incoming session's authority is known.
		std::optional<github_credential_operation_context>
			RetainedRefreshOperation;

		std::optional<session_candidate>             StagedSession;
		std::optional<github_pull_request_snapshot>  StagedSnapshot;
		std::optional<github_session_read_model>     AcceptedSession;
		std::optional<github_profile_authority>      AcceptedProfileAuthority;
		std::optional<github_capability_cache_entry> AcceptedCapabilityEvidence;
		std::optional<github_pull_request_snapshot>  AcceptedSnapshot;
		review_mutation_transport                    ReviewMutationTransport;
		github_rate_limit_state                      RateLimits;
		std::uint64_t                                LogicalEpoch               = 0U;
		bool                                         Closing                    = false;
		bool                                         ShutdownCleanupFailed      = false;
		bool                                         ProjectionDeliveryFailed   = false;
		bool                                         EpochHandoffDeliveryFailed = false;
		bool                                         EpochHandoffUnavailable    = false;
	};
}
#endif
