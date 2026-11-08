#include "github_host_implementation.hpp"

#include <stdexcept>

namespace bpr::application {
	github_application_host::implementation::implementation(core::github_state                 aInitialState,
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
															public_preview_projection_port* aPublicPreview, core::bpr_profile_id aBprProfile)
		: State{ std::move(aInitialState) }
		, Profiles{ aProfiles }
		, Viewers{ aViewers }
		, Capabilities{ aCapabilities }
		, PullRequests{ aPullRequests }
		, Credentials{ aCredentials }
		, CredentialBindings{ aCredentialBindings }
		, BprProfile{ aBprProfile }
		, CapabilityCache{ create_host_capability_cache() }
		, DecisionLane{ aDecisionLane }
		, Projections{ aProjections }
		, CredentialShutdownPolicy{ aCredentialShutdownPolicy }
		, Epochs{ aEpochs }
		, AnonymousPullRequests{ aAnonymousPullRequests }
		, AnonymousSmartGit{ aAnonymousSmartGit }
		, PublicPreview{ aPublicPreview }
		, CompletionGate{ std::make_shared<completion_gate>() }
	{
		if (BprProfile.value() == 0U) {
			throw std::invalid_argument("BPR profile admission requires a nonzero identity");
		}
		CompletionGate->Host.store(this, std::memory_order_release);
	}

	github_application_host::implementation::~implementation()
	{
		const std::lock_guard barrier{ CompletionGate->Barrier };
		CompletionGate->Host.store(nullptr, std::memory_order_release);
	}

	void github_application_host::implementation::schedule_deferred_ingress() noexcept
	{
		if (DeferredIngress.empty() || DeferredIngressScheduled) {
			return;
		}
		DeferredIngressScheduled = true;
		const auto gate          = std::weak_ptr<completion_gate>{ CompletionGate };
		try {
			DecisionLane.post(
				[gate] {
					const auto locked = gate.lock();
					if (!locked) {
						return;
					}
					const std::lock_guard completionBarrier{ locked->Barrier };
					auto* const           host = locked->Host.load(std::memory_order_acquire);
					if (host == nullptr) {
						return;
					}
					host->DeferredIngressScheduled = false;
					host->run_turn([] { });
				});
		} catch (...) {
			DeferredIngressScheduled = false;
			DeferredIngress.clear();
			ProjectionDeliveryFailed = true;
		}
	}

	void github_application_host::implementation::dispatch(const core::github_event& aEvent)
	{
		cancel_setup_enrollment();
		if (Closing) {
			return;
		}
		if (TurnActive) {
			DeferredIngress.emplace_back(deferred_dispatch{ aEvent });
			return;
		}
		if (DeferredIngressScheduled || !DeferredIngress.empty()) {
			DeferredIngress.emplace_back(deferred_dispatch{ aEvent });
			schedule_deferred_ingress();
			return;
		}
		run_turn([this, &aEvent] { dispatch_now(aEvent); });
	}

	void github_application_host::implementation::invalidate_device_grant_now(const github_credential_authority_key& aAuthority, const core::github_identity_generation aGeneration, const core::github_failure aFailure)
	{
		if (owns_launch_renewal(aAuthority, aGeneration)) {
			end_launch_renewal(aFailure);
			return;
		}
		if (!Closing && AcceptedSession && CredentialAuthority == aAuthority && CredentialSessionContext
			&& CredentialSessionContext->IdentityGeneration == aGeneration
			&& CredentialProvenance == github_credential_provenance::device_grant) {
			end_accepted_renewal(aFailure);
		}
	}

	// [[BPR-1116]]. An accepted session's renewal that gives up -- out of
	// attempts on the network, unable to reach the credential store, or
	// refused -- keeps its grant, as a launch renewal does ([[BPR-0644]]).
	// The owner's rule of 2026-09-24: a remembered grant is removed only
	// when the person removes it or a new sign-in replaces it. The session
	// ends visibly instead. It is resumed from its own remembered binding,
	// which the invalidation that goes with a resume leaves in place, and
	// the resumed session fails at once with the renewal's failure. So the
	// row and both slots stay, the credential stays selected for Sign out,
	// and the card says how the grant ends. The device host's chain has
	// ended, and is released as a launch renewal's is: left to report
	// `reenrollment_required`, its sign-in row would stand where the card
	// offers Sign out beside Sign in.
	void github_application_host::implementation::end_accepted_renewal(const core::github_failure aFailure)
	{
		release_launch_renewal();
		NextKeptEnding = kept_session_ending{ .Failure = aFailure, .Durable = DeviceBindingDurable };
		try {
			resume_profile_now(CredentialAuthority->Profile);
		} catch (...) {
			NextKeptEnding.reset();
			throw;
		}
		NextKeptEnding.reset();
	}

	void github_application_host::implementation::dispatch_now(const core::github_event& aEvent)
	{
		if (Closing) {
			return;
		}
		const auto intent = std::holds_alternative<core::activate_host_profile_intent>(aEvent)
			|| std::holds_alternative<core::deactivate_github_identity_intent>(
								aEvent)
			|| std::holds_alternative<core::open_pull_request_intent>(aEvent)
			|| std::holds_alternative<core::refresh_pull_request_intent>(aEvent);
		if (!intent) {
			return;
		}
		if (std::holds_alternative<core::activate_host_profile_intent>(aEvent)) {
			NextEnrollment.reset();
			NextResumeProfile.reset();
		}
		cancel_setup_enrollment();
		AcceptedSetupBootstrap.reset();
		if (const auto* open = std::get_if<core::open_pull_request_intent>(&aEvent)) {
			read(*open);
			return;
		}
		if (const auto* refresh = std::get_if<core::refresh_pull_request_intent>(&aEvent)) {
			read(*refresh);
			return;
		}
		// [[BPR-1829]]. Sign out leaves a start and an end.
		if (std::holds_alternative<core::deactivate_github_identity_intent>(aEvent)) {
			const auto watch      = operation_stopwatch{ };
			const auto hadSession = State.ActiveProfile.has_value() || State.PendingSession.has_value();
			record_operation(DiagnosticLog.get(), { .Action = core::operation_action::sign_out }, true);
			dispatch_core(aEvent);
			record_operation(DiagnosticLog.get(), { .Action = core::operation_action::sign_out, .Failure = hadSession ? core::operation_failure::none : core::operation_failure::no_session, .DurationMilliseconds = watch.milliseconds() });
			return;
		}
		dispatch_core(aEvent);
	}

	void github_application_host::implementation::enroll_profile(const core::host_profile_id                  aProfile,
																 github_credential_material                   aCredential,
																 const github_credential_provenance           aProvenance,
																 const std::optional<device_binding_lifetime> aLifetime,
																 const bool                                   aRefreshSettled,
																 github_enrollment_rebind                     aRebind,
																 const std::optional<core::github_account_id> aExpectedAccount)
	{
		cancel_setup_enrollment();
		AcceptedSetupBootstrap.reset();
		if (Closing) {
			return;
		}
		if (TurnActive) {
			DeferredIngress.emplace_back(deferred_enrollment{
				.Profile         = aProfile,
				.Credential      = std::move(aCredential),
				.Provenance      = aProvenance,
				.Lifetime        = aLifetime,
				.RefreshSettled  = aRefreshSettled,
				.Rebind          = std::move(aRebind),
				.ExpectedAccount = aExpectedAccount,
			});
			return;
		}
		if (DeferredIngressScheduled || !DeferredIngress.empty()) {
			DeferredIngress.emplace_back(deferred_enrollment{
				.Profile         = aProfile,
				.Credential      = std::move(aCredential),
				.Provenance      = aProvenance,
				.Lifetime        = aLifetime,
				.RefreshSettled  = aRefreshSettled,
				.Rebind          = std::move(aRebind),
				.ExpectedAccount = aExpectedAccount,
			});
			schedule_deferred_ingress();
			return;
		}
		run_turn([this, aProfile, aProvenance, aLifetime, aRefreshSettled, aExpectedAccount, rebind = std::move(aRebind), credential = std::move(aCredential)]() mutable { enroll_profile_now(aProfile, std::move(credential), aProvenance, aLifetime, aRefreshSettled, std::move(rebind), aExpectedAccount); });
	}

	void github_application_host::implementation::enroll_profile_now(const core::host_profile_id                  aProfile,
																	 github_credential_material                   aCredential,
																	 const github_credential_provenance           aProvenance,
																	 const std::optional<device_binding_lifetime> aLifetime,
																	 const bool                                   aRefreshSettled,
																	 github_enrollment_rebind                     aRebind,
																	 const std::optional<core::github_account_id> aExpectedAccount)
	{
		if (Closing) {
			return;
		}
		NextEnrollment.reset();
		NextEnrollmentProvenance = github_credential_provenance::manual;
		NextEnrollmentProfile.reset();
		NextResumeProfile.reset();
		if (valid(aCredential)) {
			NextEnrollment.emplace(std::move(aCredential));
			NextEnrollmentProvenance = aProvenance;
			NextEnrollmentProfile    = aProfile;
			NextDeviceLifetime       = aLifetime;
			NextRefreshSettled       = aRefreshSettled;
			NextEnrollmentRebind     = std::move(aRebind);
			NextExpectedAccount      = aExpectedAccount;
		}
		try {
			dispatch_core(
				core::github_session_revalidation_event{ .Profile = aProfile });
		} catch (...) {
			NextEnrollment.reset();
			NextEnrollmentProvenance = github_credential_provenance::manual;
			NextEnrollmentProfile.reset();
			NextEnrollmentRebind = { };
			NextExpectedAccount.reset();
			throw;
		}
		// A valid activation synchronously moved this material into the
		// session scratch state. An invalid activation emitted no request, so
		// retaining it here would let a later activation consume the wrong
		// enrollment secret.
		NextEnrollment.reset();
		NextEnrollmentProvenance = github_credential_provenance::manual;
		NextEnrollmentProfile.reset();
		NextEnrollmentRebind = { };
		NextExpectedAccount.reset();
	}

	void github_application_host::implementation::cancel_setup_enrollment()
	{
		if (!StagedEnrollment) {
			return;
		}
		const auto operation = StagedEnrollment->Operation;
		StagedEnrollment.reset();
		try {
			std::visit([this](const auto& aRequest) {
				if constexpr (std::is_same_v<std::decay_t<decltype(aRequest)>, github_profile_request>) {
					Profiles.cancel_profile(aRequest);
				} else {
					Viewers.cancel_viewer(aRequest);
				}
			},
					   operation);
		} catch (...) { }
	}

	void github_application_host::implementation::finish_setup_failure(const core::setup_request_id aRequest)
	{
		if (!StagedEnrollment || StagedEnrollment->Pending.Request != aRequest) {
			return;
		}
		auto staged = std::move(*StagedEnrollment);
		StagedEnrollment.reset();
		if (!Closing && staged.Accept) {
			static_cast<void>(staged.Accept(staged.Pending, false));
		}
	}

	void github_application_host::implementation::stage_setup_enrollment(core::pending_setup_destination aPending, github_credential_material aCredential,
																		 std::function<bool(const core::pending_setup_destination&, bool)> aAccept,
																		 const github_credential_provenance aProvenance, std::optional<device_binding_lifetime> aLifetime,
																		 const bool aRefreshSettled)
	{
		cancel_setup_enrollment();
		if (Closing || !aPending.Profile || !valid(aCredential) || !aAccept
			|| NextSetupAttempt >= (std::uint64_t{ 1U } << 63U)) {
			return;
		}
		const auto attempt = NextSetupAttempt++;
		// Setup requests use the upper request namespace and their own callback slot.
		// They cannot cancel an ordinary review operation with the same request number.
		const auto request = github_profile_request{
			.Context = { .Request            = core::request_id{ (std::uint64_t{ 1U } << 63U) | attempt },
						 .ApplicationSession = State.ApplicationSession,
						 .Profile            = *aPending.Profile,
						 .IdentityGeneration = core::github_identity_generation{ State.IdentityGeneration.value() + 1U } },
		};
		const auto identity = aPending.Request;
		StagedEnrollment.emplace(staged_enrollment{ std::move(aPending), attempt, State.IdentityGeneration,
													std::move(aCredential), std::move(aAccept), aProvenance, std::move(aLifetime), aRefreshSettled, request });
		auto callback = setup_callback<github_profile_completion>([identity, request, attempt](implementation& aHost, github_profile_completion aCompletion) {
			if (!aHost.StagedEnrollment || aHost.StagedEnrollment->Pending.Request != identity || aHost.StagedEnrollment->Attempt != attempt) {
				return;
			}
			auto* success = std::get_if<github_profile_success>(&aCompletion);
			if (aHost.Closing || aHost.State.IdentityGeneration != aHost.StagedEnrollment->OriginGeneration
				|| !success || success->Request != request || !valid(success->Authority)
				|| success->Authority.Profile != request.Context.Profile
				|| success->Authority.Hostname.value() != aHost.StagedEnrollment->Pending.Destination.Hostname
				|| success->Authority.Authentication != github_authentication_transport::native_https) {
				aHost.finish_setup_failure(identity);
				return;
			}
			aHost.stage_setup_viewer(identity, success->Authority, request.Context);
		});
		try {
			Profiles.resolve_profile(request, callback);
		} catch (...) {
			callback(github_profile_cancelled{ .Request = request });
		}
	}

	void github_application_host::implementation::stage_setup_viewer(const core::setup_request_id aIdentity, const github_profile_authority& aAuthority,
																	 const core::github_session_request_context& aContext)
	{
		auto credential = github_credential_material::create(StagedEnrollment->Credential.bytes());
		if (!credential) {
			finish_setup_failure(aIdentity);
			return;
		}
		const auto attempt          = StagedEnrollment->Attempt;
		const auto request          = github_viewer_request{ .Context = aContext, .Authority = aAuthority };
		StagedEnrollment->Operation = request;
		auto callback               = setup_callback<github_viewer_completion>([aIdentity, request, attempt](implementation& aHost, github_viewer_completion aCompletion) {
			if (!aHost.StagedEnrollment || aHost.StagedEnrollment->Pending.Request != aIdentity || aHost.StagedEnrollment->Attempt != attempt) {
				return;
			}
			auto* success = std::get_if<github_viewer_success>(&aCompletion);
			if (aHost.Closing || aHost.State.IdentityGeneration != aHost.StagedEnrollment->OriginGeneration
				|| !success || success->Request != request || success->AccountTransport
				|| !valid_viewer(request.Authority, request.Context, success->Viewer, success->RateLimits)) {
				aHost.finish_setup_failure(aIdentity);
				return;
			}
			const auto pending  = aHost.StagedEnrollment->Pending;
			const auto accept   = aHost.StagedEnrollment->Accept;
			const auto accepted = accept(pending, true);
			if (!aHost.StagedEnrollment || aHost.StagedEnrollment->Pending != pending || aHost.StagedEnrollment->Attempt != attempt) {
				return;
			}
			if (!accepted || aHost.State.IdentityGeneration != aHost.StagedEnrollment->OriginGeneration) {
				aHost.StagedEnrollment.reset();
				return;
			}
			auto staged = std::move(*aHost.StagedEnrollment);
			aHost.StagedEnrollment.reset();
			aHost.AcceptedSetupBootstrap = *success;
			aHost.enroll_profile_now(*pending.Profile, std::move(staged.Credential), staged.Provenance, staged.Lifetime, staged.RefreshSettled);
		});
		try {
			Viewers.observe_viewer({ .Request = request, .Authentication = github_bootstrap_credential{ .Credential = std::move(*credential) } }, callback);
		} catch (...) {
			callback(github_viewer_cancelled{ .Request = request });
		}
	}

	void github_application_host::implementation::resume_profile(const core::host_profile_id aProfile)
	{
		cancel_setup_enrollment();
		if (Closing) {
			return;
		}
		if (TurnActive) {
			DeferredIngress.emplace_back(deferred_resume{
				.Profile = aProfile,
			});
			return;
		}
		if (DeferredIngressScheduled || !DeferredIngress.empty()) {
			DeferredIngress.emplace_back(deferred_resume{
				.Profile = aProfile,
			});
			schedule_deferred_ingress();
			return;
		}
		run_turn([this, aProfile] { resume_profile_now(aProfile); });
	}

	void github_application_host::implementation::resume_profile_now(const core::host_profile_id aProfile)
	{
		if (Closing || aProfile.value() == 0U || (CredentialBindings.find(aProfile) != nullptr && CredentialBindings.find(aProfile)->BprProfile != BprProfile)) {
			return;
		}
		NextEnrollment.reset();
		NextResumeProfile = aProfile;
		try {
			dispatch_core(core::github_session_revalidation_event{
				.Profile = aProfile,
			});
		} catch (...) {
			NextResumeProfile.reset();
			throw;
		}
		NextResumeProfile.reset();
	}

	void github_application_host::implementation::activate_public_preview(const core::host_profile_id aProfile)
	{
		cancel_setup_enrollment();
		if (Closing) {
			return;
		}
		if (TurnActive) {
			DeferredIngress.emplace_back(deferred_public_preview{
				.Profile = aProfile,
			});
			return;
		}
		if (DeferredIngressScheduled || !DeferredIngress.empty()) {
			DeferredIngress.emplace_back(deferred_public_preview{
				.Profile = aProfile,
			});
			schedule_deferred_ingress();
			return;
		}
		run_turn([this, aProfile] { activate_public_preview_now(aProfile); });
	}
}
