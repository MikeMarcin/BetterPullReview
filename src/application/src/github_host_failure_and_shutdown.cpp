#include "github_host_implementation.hpp"

namespace bpr::application {
	void github_application_host::implementation::fail_delete_operation()
	{
		const auto terminal = DeferredTerminal;
		DeferredTerminal.reset();
		ActiveVaultOperation = vault_operation::none;
		ActiveVaultContext.reset();
		publish_diagnostic(github_host_diagnostic{
			.Reason            = core::github_reason_code::vault_unavailable,
			.RetryAfterSeconds = std::nullopt,
			.RateLimits        = { },
			.RateLimitCause    = std::nullopt,
		});
		if (terminal.has_value()) {
			finish_deferred_terminal(deferred_session_terminal{
				.Context = terminal->Context,
				.Failure = core::github_failure::vault,
			});
		} else if (DeferredSession.has_value()) {
			const auto context = *DeferredSession;
			DeferredSession.reset();
			fail_session_direct(context, core::github_failure::vault);
		}
	}

	void github_application_host::implementation::fail_capability_session(
		const core::github_session_request_context& aContext,
		const core::github_failure                  aFailure,
		const github_capability_diagnostic          aDetail,
		std::optional<github_host_diagnostic>       aDiagnostic)
	{
		auto diagnostic             = aDiagnostic.value_or(github_host_diagnostic{ });
		diagnostic.CapabilityDetail = bounded_capability_diagnostic(aDetail);
		fail_session(aContext, aFailure, false,
					 github_bootstrap_failure_stage::capability_discovery, std::move(diagnostic));
	}

	// [[BPR-0644]]. A launch renewal's rotated grant that fails before it is
	// accepted is the remembered grant, not a provisional one: its row was
	// never tombstoned and its refresh slot already holds the rotated
	// material, so the next launch renews from it. Before its `/user`
	// named the account there is no selected credential yet, and the
	// renewal's own slot is taken; after, the selected one is converted.
	// Either way the device host's chain is released.
	void github_application_host::implementation::keep_renewed_launch_grant(const core::github_session_request_context& aContext)
	{
		if (AcceptedSession.has_value()) {
			return;
		}
		if (LaunchRenewal && LaunchRenewalEnrolled && !CredentialAuthority.has_value()) {
			const auto renewal = *LaunchRenewal;
			LaunchRenewal.reset();
			LaunchRenewalEnrolled = false;
			LaunchRenewalBegun    = false;
			release_launch_renewal();
			keep_launch_renewal_slot(renewal.Authority, aContext);
			return;
		}
		if (std::exchange(LaunchRenewedGrant, false) && CredentialAuthority.has_value() && CredentialSessionContext.has_value()) {
			release_launch_renewal();
			CredentialLifecycle   = credential_lifecycle::persistent;
			RestoredDeviceBinding = true;
			DeviceBindingDurable  = true;
		}
	}

	// [[BPR-0644]]. A failure before acceptance erases a provisional
	// credential, and a persistent one only when its caller asks: a
	// restored device binding is no longer erased because its session
	// failed. The owner's rule of 2026-09-24 is that a remembered grant is
	// removed only when the person removes it or a new sign-in replaces
	// it. The callers that still ask are the ones whose material is not
	// the binding's: a profile the binding never named. An account it never
	// named is kept too, since the owner's decision of 2026-09-24.
	void github_application_host::implementation::fail_session(const core::github_session_request_context& aContext,
															   const core::github_failure                  aFailure,
															   const bool                                  aDeletePersistentCredential,
															   const github_bootstrap_failure_stage        aStage,
															   std::optional<github_host_diagnostic>       aDiagnostic)
	{
		cancel_provider();
		if (!aDeletePersistentCredential) {
			keep_renewed_launch_grant(aContext);
		}
		if (CredentialAuthority.has_value()
			&& CredentialSessionContext.has_value()
			&& !AcceptedSession.has_value()
			&& ActiveVaultOperation == vault_operation::none
			&& ((CredentialLifecycle == credential_lifecycle::provisional && !RestoredDeviceBinding)
				|| aDeletePersistentCredential)) {
			start_delete(aContext.IdentityGeneration,
						 deferred_session_terminal{
							 .Context    = aContext,
							 .Failure    = aFailure,
							 .Stage      = aStage,
							 .Diagnostic = std::move(aDiagnostic),
						 });
			return;
		}
		fail_session_direct(aContext, aFailure, aStage, std::move(aDiagnostic));
	}

	void
	github_application_host::implementation::fail_session_direct(const core::github_session_request_context&  aContext,
																 const core::github_failure                   aFailure,
																 const github_bootstrap_failure_stage         aStage,
																 const std::optional<github_host_diagnostic>& aDiagnostic)
	{
		// [[BPR-0644]]. Said with the failure, so the card can offer Sign
		// out for a grant that is still on this machine.
		const auto kept = remembered_grant_kept();
		reset_session_scratch();
		auto diagnostic           = aDiagnostic ? project_github_host_diagnostic(*aDiagnostic) : github_host_diagnostic_projection{ };
		diagnostic.BootstrapStage = aStage;
		diagnostic.GrantKept      = kept;
		dispatch_core(core::github_session_failed_event{
						  .Context = aContext,
						  .Failure = aFailure,
					  },
					  aStage == github_bootstrap_failure_stage::none && !kept ? std::nullopt : std::optional{ std::move(diagnostic) });
	}

	// [[BPR-0644]]. What a failure says when its credential signed in as an
	// account other than the one its binding named.
	auto github_application_host::implementation::account_mismatch() -> github_host_diagnostic
	{
		return github_host_diagnostic{ .Reason = core::reason_for(core::github_failure::malformed_response), .AccountMismatch = true };
	}

	// [[BPR-0644]]. The selected credential is a device grant someone
	// signed in with before this session: restored or resumed from its
	// binding, or a launch renewal's rotated grant. A failure keeps it.
	auto github_application_host::implementation::remembered_device_grant() const noexcept -> bool
	{
		return CredentialAuthority.has_value() && CredentialSessionContext.has_value() && !AcceptedSession.has_value()
			&& CredentialProvenance == github_credential_provenance::device_grant
			&& (CredentialLifecycle == credential_lifecycle::persistent || LaunchRenewedGrant);
	}

	// A restored device grant that failed before acceptance and was not
	// erased: its material and its row are still on this machine.
	auto github_application_host::implementation::remembered_grant_kept() const noexcept -> bool
	{
		return RestoredDeviceBinding && CredentialAuthority.has_value() && CredentialSessionContext.has_value()
			&& !AcceptedSession.has_value() && CredentialLifecycle == credential_lifecycle::persistent
			&& CredentialProvenance == github_credential_provenance::device_grant;
	}

	void github_application_host::implementation::cancel_session(const core::github_session_request_context& aContext)
	{
		cancel_provider();
		if (CredentialAuthority.has_value()
			&& CredentialSessionContext.has_value()
			&& !AcceptedSession.has_value()
			&& ActiveVaultOperation == vault_operation::none
			&& CredentialLifecycle == credential_lifecycle::provisional) {
			start_delete(aContext.IdentityGeneration,
						 deferred_session_terminal{
							 .Context = aContext,
							 .Failure = std::nullopt,
						 });
			return;
		}
		reset_session_scratch();
		dispatch_core(core::github_session_cancelled_event{
			.Context = aContext,
		});
	}

	void github_application_host::implementation::finish_deferred_terminal(const deferred_session_terminal& aTerminal)
	{
		if (aTerminal.Failure.has_value()) {
			fail_session_direct(aTerminal.Context, *aTerminal.Failure, aTerminal.Stage, aTerminal.Diagnostic);
		} else {
			reset_session_scratch();
			dispatch_core(core::github_session_cancelled_event{
				.Context = aTerminal.Context,
			});
		}
	}

	void github_application_host::implementation::reset_session_scratch()
	{
		// [[BPR-0549]]. The window's failure exit. Every session terminal
		// and the orderly shutdown pass through here, and each of them
		// means the session that was going to name an authority never
		// will -- so a slot held for it can no longer be spent by
		// anything, and the erasure that was deferred happens now. It is
		// deliberately not a way to leak material by failing at the right
		// moment.
		erase_retained_refresh();
		PendingProvider = std::monostate{ };
		StagedSession.reset();
		CurrentProfile.reset();
		BootstrapViewer.reset();
		AccountTransport.reset();
		SessionEnrollment.reset();
		SessionEnrollmentProvenance = github_credential_provenance::manual;
		SessionResumeProfile.reset();
		SessionKeptEnding.reset();
		SessionPublicPreview = false;
		ViewerMode           = viewer_mode::none;
	}

	void github_application_host::implementation::cancel_provider()
	{
		std::visit(
			[this](const auto& aRequest) {
				using request_type = std::decay_t<decltype(aRequest)>;
				if constexpr (std::is_same_v<request_type,
											 github_profile_request>) {
					try {
						Profiles.cancel_profile(aRequest);
					} catch (...) {
					}
				} else if constexpr (std::is_same_v<request_type,
													github_viewer_request>) {
					try {
						Viewers.cancel_viewer(aRequest);
					} catch (...) {
					}
				} else if constexpr (std::is_same_v<request_type,
													github_capability_request>) {
					try {
						Capabilities.cancel_capabilities(aRequest);
					} catch (...) {
					}
				} else if constexpr (std::is_same_v<
										 request_type,
										 github_pull_request_provider_request>) {
					try {
						PullRequests.cancel_pull_request(
							aRequest);
					} catch (...) {
					}
				}
			},
			PendingProvider);
		PendingProvider = std::monostate{ };
		++ProviderOperationToken;
	}

	void github_application_host::implementation::cancel_vault()
	{
		if (ActiveVaultContext.has_value()) {
			try {
				Credentials.cancel(*ActiveVaultContext);
			} catch (...) {
			}
		}
		++VaultOperationToken;
		ActiveVaultOperation = vault_operation::none;
		ActiveVaultContext.reset();
	}

	// Not cancelled at the vault, deliberately. Only an affirmative answer
	// is ever carried back, so the host cannot tell an outstanding probe
	// from a finished one, and cancelling on that guess would submit a
	// cancellation for work that already ended. There is nothing to
	// reclaim either way: the probe holds no material past its own
	// callback, and the completion gate drops a late one.
	void github_application_host::implementation::cancel_refresh_probe()
	{
		++RefreshProbeToken;
		RefreshProbeContext.reset();
		AcceptedCredentialRenewable = false;
		RefreshSlotMissing          = false;
	}

	void github_application_host::implementation::shutdown()
	{
		RestoreIngressClosing = true;
		cancel_setup_enrollment();
		if (TurnActive) {
			DeferredIngress.emplace_back(deferred_shutdown{ });
			return;
		}
		run_turn([this] { shutdown_now(); });
	}

	void github_application_host::implementation::shutdown_now()
	{
		cancel_setup_enrollment();
		AcceptedSetupBootstrap.reset();
		{
			const std::lock_guard barrier{ CompletionGate->Barrier };
			if (Closing) {
				return;
			}

			if (State.ActiveProfile.has_value()
				|| State.PendingSession.has_value()
				|| State.PendingPullRequest.has_value()) {
				static_cast<void>(core::transition(
					State, core::deactivate_github_identity_intent{ }));
			}

			Closing = true;
		}

		if (Persistence != nullptr && !(BindingLoadPending && PendingRestoreCleanup)) {
			Persistence->cancel_pending();
		}
		LaunchRenewal.reset();
		LaunchRenewalEnrolled = false;
		LaunchRenewalBegun    = false;
		LaunchRenewedGrant    = false;

		// Wipe raw host-owned material before an external cancellation can
		// block or re-enter. PendingProvider remains intact until its exact
		// cancellation has been submitted.
		NextEnrollment.reset();
		SessionEnrollment.reset();
		SessionEnrollmentProvenance = github_credential_provenance::manual;
		NextResumeProfile.reset();
		SessionResumeProfile.reset();
		SessionKeptEnding.reset();

		// Never call an external port while holding the completion gate.
		// A cancellation implementation may wait for a worker callback that
		// itself needs the gate in order to enqueue its terminal.
		if (ReadWindow) {
			ReadWindow->shutdown();
		}
		for (auto& slot : AttachedWindows) {
			slot.Host->shutdown();
			slot.Detaching = true;
		}
		invalidate_epoch_authority();
		cancel_provider();
		cancel_refresh_probe();
		if ((ActiveVaultOperation == vault_operation::store
			 || ActiveVaultOperation == vault_operation::retrieve)
			&& ActiveVaultContext.has_value()) {
			try {
				Credentials.cancel(*ActiveVaultContext);
			} catch (...) {
			}
		} else if (BindingErasePending
				   || (ActiveVaultOperation == vault_operation::delete_credential && ActiveVaultContext.has_value())) {
			// The durable barrier or exact vault cleanup already owns the
			// terminal required for shutdown quiescence.
		} else if (requires_shutdown_credential_cleanup()) {
			start_delete(CredentialSessionContext->IdentityGeneration,
						 std::nullopt);
		} else {
			cancel_vault();
		}
		static_cast<void>(CapabilityCache.clear());
		DeferredSession.reset();
		revoke_review_mutation_transport();
		reset_session_scratch();
		StagedSnapshot.reset();
		AcceptedSession.reset();
		AcceptedSnapshot.reset();
		AcceptedProfileAuthority.reset();
		AcceptedCapabilityEvidence.reset();
	}

	void github_application_host::implementation::complete_restore(const std::shared_ptr<restore_completion>& aCompletion,
																   const device_profile_restore_status        aStatus) noexcept
	{
		if (!aCompletion || aCompletion->Scheduled) {
			return;
		}
		aCompletion->Scheduled = true;
		if (!aCompletion->Callback) {
			--OutstandingRestoreCompletions;
			return;
		}
		try {
			DecisionLane.post([gate = std::weak_ptr<completion_gate>{ CompletionGate }, aCompletion, aStatus] {
				const auto locked = gate.lock();
				if (!locked) {
					return;
				}
				const std::lock_guard barrier{ locked->Barrier };
				auto*                 host = locked->Host.load(std::memory_order_acquire);
				if (!host) {
					return;
				}
				auto callback = std::move(aCompletion->Callback);
				try {
					host->run_turn([&] {
						try {
							callback(host->Closing ? device_profile_restore_status::cancelled : aStatus);
						} catch (...) {
							host->ProjectionDeliveryFailed = true;
						}
					});
				} catch (...) {
					host->ProjectionDeliveryFailed = true;
				}
				--host->OutstandingRestoreCompletions;
			});
		} catch (...) {
			--OutstandingRestoreCompletions;
			ProjectionDeliveryFailed = true;
		}
	}

	// Inside a turn the failure waits for the turn's end, where the lane's
	// own terminal would have found it, or for the turn's exception
	// ([[BPR-1112]]). A turn can set up after the call what that failure
	// decides by: a replacement's session is recorded after the
	// invalidation that asked for the tombstone. Outside a turn the call is
	// the only work in progress, so it fails now. Either way a throwing
	// terminal goes no further, as on the lane.
	void github_application_host::implementation::fail_withdrawn_binding(std::function<void(device_binding_result)> aFinish)
	{
		if (TurnActive && WithdrawnBindingCount < WithdrawnBindings.size()) {
			WithdrawnBindings.at(WithdrawnBindingCount++).emplace(std::move(aFinish));
			return;
		}
		try {
			aFinish(std::unexpected(device_binding_failure::unavailable));
		} catch (...) {
		}
	}

	// A failure may make another request that is withdrawn in turn. It
	// joins the queue and is failed before the turn ends. As on the lane,
	// a throwing terminal cannot stop the ones behind it or end the turn.
	void github_application_host::implementation::fail_withdrawn_bindings()
	{
		while (WithdrawnBindingCount != 0U) {
			auto finish = std::move(*WithdrawnBindings.front());
			std::move(WithdrawnBindings.begin() + 1, WithdrawnBindings.begin() + static_cast<std::ptrdiff_t>(WithdrawnBindingCount), WithdrawnBindings.begin());
			WithdrawnBindings.at(--WithdrawnBindingCount).reset();
			try {
				finish(std::unexpected(device_binding_failure::unavailable));
			} catch (...) {
			}
		}
	}

	void github_application_host::implementation::binding_failure()
	{
		DeviceBindingDurable = false;
		publish_diagnostic(github_host_diagnostic{ .Reason = core::github_reason_code::vault_unavailable, .RetryAfterSeconds = std::nullopt, .RateLimits = { }, .RateLimitCause = std::nullopt });
	}

	// [[BPR-0644]]. The renewal itself is the device host's existing
	// rotation: it asks `prepare_device_rotation`, which writes no
	// tombstone for a launch renewal, and ends by enrolling the rotated
	// access token or by invalidating this exact grant.
	void github_application_host::implementation::begin_launch_renewal()
	{
		const auto renewal = *LaunchRenewal;
		try {
			LaunchRenewalSink(renewal);
		} catch (...) {
			// Ended on its own turn: this runs inside the transition that
			// admitted the held session. Nothing was asked of the server or
			// the grant, so it is tried again at the next launch.
			DeferredIngress.emplace_back(deferred_grant_invalidation{ renewal.Authority, renewal.IdentityGeneration, core::github_failure::vault });
			schedule_deferred_ingress();
		}
	}

	// [[BPR-0644]]. A renewal that cannot finish -- refused, out of
	// attempts, unable to reach the credential store, or never able to
	// start -- keeps its grant. That is the owner's rule of 2026-09-24: a
	// remembered grant is removed only when the person removes it or a new
	// sign-in replaces it. No tombstone was written for the renewal, so its
	// row stays live and the next launch tries again. The renewal's exact
	// slot becomes the selected credential, so Sign out erases both slots
	// through the ordered path. The held session fails with the failure the
	// renewal named, which every startup observer already treats as a
	// terminal, and the device sign-in is offered beside it.
	void github_application_host::implementation::end_launch_renewal(const core::github_failure aFailure)
	{
		if (!DeferredSession || CredentialAuthority.has_value()) {
			// Unreachable while the session is held. Nothing is adopted
			// here, so no other credential's slot can be named.
			DeviceBindingDurable = false;
			LaunchRenewal.reset();
			LaunchRenewalBegun = false;
			dispatch_now(core::deactivate_github_identity_intent{ });
			return;
		}
		const auto renewal = *LaunchRenewal;
		const auto context = *DeferredSession;
		const auto begun   = std::exchange(LaunchRenewalBegun, false);
		LaunchRenewal.reset();
		DeferredSession.reset();
		// The device host's chain has ended; released, it neither renews nor
		// asks for a new code in this process. A renewal that never began
		// gave it nothing to release.
		if (begun) {
			release_launch_renewal();
		}
		keep_launch_renewal_slot(renewal.Authority, context);
		fail_session(context, aFailure);
	}

	// The renewal's grant, kept: selected as a restored binding whose row
	// is still live, so a failure keeps it, an orderly quit keeps it, and
	// Sign out erases it.
	void github_application_host::implementation::keep_launch_renewal_slot(const github_credential_authority_key& aAuthority, const core::github_session_request_context& aContext)
	{
		adopt_launch_renewal_slot(aAuthority, aContext);
		RestoredDeviceBinding = true;
		DeviceBindingDurable  = true;
	}

	// The renewal's exact slot becomes the credential this host erases.
	// Only ever the renewal's own authority, and only while no other
	// credential is selected, so nothing a replacing grant owns is named.
	void github_application_host::implementation::adopt_launch_renewal_slot(const github_credential_authority_key& aAuthority, const core::github_session_request_context& aContext)
	{
		CredentialAuthority         = aAuthority;
		CredentialSessionContext    = aContext;
		CredentialLifecycle         = credential_lifecycle::persistent;
		CredentialProvenance        = github_credential_provenance::device_grant;
		CredentialBindingRemembered = false;
	}

	// Releases whatever renewal chain the device host holds: a launch
	// renewal's, or an accepted session's that gave up ([[BPR-1116]]).
	void github_application_host::implementation::release_launch_renewal() noexcept
	{
		try {
			LaunchRenewalSink(std::nullopt);
		} catch (...) {
		}
	}

	auto github_application_host::implementation::owns_launch_renewal(const github_credential_authority_key& aAuthority, const core::github_identity_generation aGeneration) const noexcept -> bool
	{
		return !Closing && LaunchRenewal.has_value() && !LaunchRenewalEnrolled && LaunchRenewal->Authority == aAuthority && LaunchRenewal->IdentityGeneration == aGeneration;
	}

	void github_application_host::implementation::persist_settled_device_binding()
	{
		if (Closing || Persistence == nullptr || !DeviceRefreshSettled || !DeviceLifetime || !CredentialAuthority || !CredentialSessionContext || !AcceptedSession || CredentialProvenance != github_credential_provenance::device_grant || DeviceBindingDurable || BindingStorePending) {
			return;
		}
		if (remaining_access_lifetime(*DeviceLifetime, WallClock()) == 0U) {
			binding_failure();
			return;
		}
		const auto record     = device_binding_record{ .Authority = *CredentialAuthority, .Lifetime = *DeviceLifetime };
		const auto generation = BindingGeneration;
		auto       finish     = std::function<void(device_binding_result)>{ [this, record, generation](device_binding_result aResult) {
			--OutstandingBindings;
			BindingStorePending = false;
			if (Closing || generation != BindingGeneration || CredentialAuthority != record.Authority || DeviceLifetime != record.Lifetime) {
				return;
			}
			if (!aResult || remaining_access_lifetime(record.Lifetime, WallClock()) == 0U) {
				binding_failure();
				return;
			}
			DeviceBindingDurable = true;
			// [[BPR-0660]]. "Remembered on this Mac" is a fact the
			// card states, so the session is published again once
			// it is one.
			republish_accepted_session();
		} };
		BindingStorePending = true;
		++OutstandingBindings;
		call_binding_lane([this, &record](auto aCallback) { Persistence->store(record, std::move(aCallback)); }, std::move(finish));
	}

	void github_application_host::implementation::start_delete(const core::github_identity_generation   aGeneration,
															   std::optional<deferred_session_terminal> aTerminal,
															   const refresh_disposition                aRefresh)
	{
		if (Persistence == nullptr || !CredentialAuthority || !CredentialSessionContext) {
			start_delete_unblocked(aGeneration, std::move(aTerminal), aRefresh);
			return;
		}
		// [[BPR-1112]]. The request's terminal handler is built before any
		// state changes. If building it throws, the caller gets the
		// exception and this site has changed nothing: no durability, no
		// generation, no stored terminal, no count and no pending flag.
		const auto authority = *CredentialAuthority;
		auto       finish    = BindingErasePending ? std::function<void(device_binding_result)>{ } : std::function<void(device_binding_result)>{ [this, authority](device_binding_result aResult) {
			--OutstandingBindings;
			BindingErasePending = false;
			if (CredentialAuthority != authority) {
				binding_failure();
				return;
			}
			if (!aResult) {
				ShutdownCleanupFailed = Closing;
				if (Closing || !DeferredSession) {
					binding_failure();
					start_delete_unblocked(BindingDeleteGeneration, std::move(BindingDeleteTerminal), BindingDeleteRefresh);
					return;
				}
				StartupCleanupOperation.reset();
				DeferredTerminal = std::move(BindingDeleteTerminal);
				fail_delete_operation();
				return;
			}
			start_delete_unblocked(BindingDeleteGeneration, std::move(BindingDeleteTerminal), BindingDeleteRefresh);
		} };
		DeviceBindingDurable = false;
		++BindingGeneration;
		BindingDeleteGeneration = aGeneration;
		BindingDeleteTerminal   = std::move(aTerminal);
		BindingDeleteRefresh    = aRefresh;
		if (BindingErasePending) {
			return;
		}
		BindingErasePending = true;
		++OutstandingBindings;
		call_binding_lane([this, &authority](auto aCallback) { Persistence->erase(authority, std::move(aCallback)); }, std::move(finish));
	}
}
