#include "github_host_implementation.hpp"

namespace bpr::application {
	// The one question composition asks about restored material: can this
	// session renew itself, or would telling its panel to renew end in a
	// vault that answers `absent` and a reviewer told to authorize again
	// for no reason? Only the device flow ever writes a refresh entry, so
	// the entry's existence under this authority is the whole answer.
	//
	// Retrieval is the only presence question this port answers, so the
	// probe retrieves and then destroys what it got, on the vault's own
	// callback, before anything crosses onto the decision lane. This host
	// has no business holding renewal material and after this does not.
	void github_application_host::implementation::start_refresh_probe()
	{
		if (Closing || !CredentialAuthority.has_value()
			|| !CredentialSessionContext.has_value()) {
			return;
		}
		const github_credential_operation_context probe{
			.Request            = CredentialSessionContext->Request,
			.ApplicationSession = CredentialSessionContext->ApplicationSession,
			.Authority          = *CredentialAuthority,
			.IdentityGeneration = CredentialSessionContext->IdentityGeneration,
		};
		RefreshProbeContext = probe;
		const auto  token   = ++RefreshProbeToken;
		const auto  gate    = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane    = &DecisionLane;
		const auto  callback =
			[gate, lane, token, probe, reportAbsent = RestoredDeviceBinding](
				github_credential_retrieve_completion aCompletion) noexcept {
				auto* const retrieved = std::get_if<
					github_credential_retrieve_success>(&aCompletion);
				const auto present = retrieved != nullptr && valid(aCompletion) && completion_context(aCompletion) == probe;
				if (retrieved == nullptr && !reportAbsent) {
					return;
				}
				if (retrieved != nullptr) {
					retrieved->Credential.reset();
				}
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					lane->post([gate, token, probe, present]() {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard completionBarrier{
							locked->Barrier
						};
						auto* const host = locked->Host.load(std::memory_order_acquire);
						if (host != nullptr) {
							host->handle_refresh_probe(token, probe, present);
						}
					});
				} catch (...) {
				}
			};
		try {
			Credentials.retrieve_refresh(probe, callback);
		} catch (...) {
			RefreshProbeContext.reset();
		}
	}

	void github_application_host::implementation::handle_refresh_probe(
		const std::uint64_t                        aToken,
		const github_credential_operation_context& aExpectedProbe, const bool aPresent)
	{
		run_turn([this, aToken, aExpectedProbe, aPresent]() {
			handle_refresh_probe_now(aToken, aExpectedProbe, aPresent);
		});
	}

	void github_application_host::implementation::handle_refresh_probe_now(
		const std::uint64_t                        aToken,
		const github_credential_operation_context& aExpectedProbe, const bool aPresent)
	{
		if (aToken != RefreshProbeToken || !RefreshProbeContext.has_value()
			|| *RefreshProbeContext != aExpectedProbe) {
			return;
		}
		RefreshProbeContext.reset();
		// Anything that invalidated the identity while the vault was
		// answering makes the answer about a session that no longer
		// exists, and a stale `true` here would arm renewal under a key
		// this host has already forgotten.
		if (Closing || !AcceptedSession.has_value()
			|| !CredentialAuthority.has_value()
			|| *CredentialAuthority != aExpectedProbe.Authority) {
			return;
		}
		// [[BPR-0540]]. Presence is not the whole answer. Both slots are
		// keyed by the same authority, so a token the reviewer pasted for
		// an account a device grant once enrolled lands beside whatever
		// that grant left behind, and spending it would take the reviewer
		// off the session they asked for and onto one they did not. The
		// entry renews *this* credential only when this credential itself
		// came from a grant.
		if (CredentialProvenance != github_credential_provenance::device_grant) {
			return;
		}
		if (!aPresent) {
			// [[BPR-1116]]. A restored grant whose refresh slot is missing
			// is not signed out and erased: its access token still works,
			// and the owner's rule of 2026-09-24 is that a remembered grant
			// is removed only when the person removes it. The session stays
			// and nothing is armed to renew it, so the card now says it
			// does not renew itself. Once its access lifetime has gone, the
			// next launch's renewal meets the missing material as a refusal
			// and keeps the grant, saying it needs signing in again.
			if (RestoredDeviceBinding) {
				RefreshSlotMissing = true;
				republish_accepted_session();
			}
			return;
		}
		AcceptedCredentialRenewable = true;
		republish_accepted_session();
	}

	// Renewability is a vault answer and the vault answers after the
	// session was published. Rather than hold an acceptance back on a
	// keychain round trip, the host publishes the accepted session once
	// more when it learns. Every reader keyed on the transition sees
	// nothing new -- there is none -- and the one reader that asks whether
	// this credential can renew itself now gets a settled answer.
	// [[BPR-0660]]. The grant facts the card states, from what the host
	// already holds: the provenance, the device lifetime and whether the
	// binding is durable. No credential leaves here.
	auto github_application_host::implementation::grant_projection() const noexcept
		-> std::optional<github_grant_projection>
	{
		if (!AcceptedSession.has_value()) {
			return std::nullopt;
		}
		if (CredentialProvenance != github_credential_provenance::device_grant) {
			return github_grant_projection{ .Kind = github_grant_kind_projection::token };
		}
		return github_grant_projection{
			.Kind       = github_grant_kind_projection::device_grant,
			.SignedInAt = DeviceLifetime.has_value() && DeviceLifetime->IssuedAt > 0
				? std::optional<std::int64_t>{ DeviceLifetime->IssuedAt }
				: std::nullopt,
			.Renews     = DeviceLifetime.has_value() && DeviceLifetime->RefreshExpiresAt > 0 && !RefreshSlotMissing,
			.Remembered = DeviceBindingDurable,
		};
	}

	void github_application_host::implementation::republish_accepted_session() noexcept
	{
		if (!AcceptedSession.has_value()) {
			return;
		}
		try {
			const github_presentation_update update{
				.Transition      = std::nullopt,
				.Session         = project_github_session(*AcceptedSession, grant_projection()),
				.PullRequest     = std::nullopt,
				.HostDiagnostics = { },
			};
			if (!PrimaryReadClosed) {
				Projections.publish(update);
			}
		} catch (...) {
			ProjectionDeliveryFailed = true;
		}
	}

	void github_application_host::implementation::accept_session(session_candidate aCandidate)
	{
		const github_session_read_model model{
			.Profile            = aCandidate.ProfileAuthority.Profile,
			.Host               = aCandidate.ProfileAuthority.Host,
			.Viewer             = aCandidate.Viewer,
			.CapabilityEvidence = {
				.Capabilities    = aCandidate.CapabilityEvidence.Capabilities,
				.Revision        = aCandidate.CapabilityEvidence.Revision,
				.ReadShape       = aCandidate.CapabilityEvidence.ReadShape,
				.ContractVersion = 1U,
			},
			.RateLimits = aCandidate.RateLimits,
			.Mode       = aCandidate.Mode,
		};
		if (!validate(model, aCandidate.Context).valid()) {
			fail_session(aCandidate.Context,
						 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::session_acceptance);
			return;
		}
		if (CredentialAuthority.has_value() && !CredentialBindingRemembered) {
			if (!CredentialBindings.remember(*CredentialAuthority,
											 CredentialProvenance)) {
				SessionEnrollment.reset();
				SessionEnrollmentProvenance = github_credential_provenance::manual;
				fail_session(aCandidate.Context, core::github_failure::vault);
				return;
			}
			CredentialBindingRemembered = true;
		}

		StagedSession = aCandidate;
		auto output   = core::transition(
			State,
			core::github_session_ready_event{
				.Context            = aCandidate.Context,
				.Host               = aCandidate.ProfileAuthority.Host,
				.Account            = aCandidate.Viewer.Account,
				.Capabilities       = aCandidate.CapabilityEvidence.Capabilities,
				.CapabilityRevision = aCandidate.CapabilityEvidence.Revision,
			});
		const core::accepted_session_delta expected{
			.Context            = aCandidate.Context,
			.Host               = aCandidate.ProfileAuthority.Host,
			.Account            = aCandidate.Viewer.Account,
			.Capabilities       = aCandidate.CapabilityEvidence.Capabilities,
			.CapabilityRevision = aCandidate.CapabilityEvidence.Revision,
		};
		if (!output.AcceptedSession.has_value()
			|| *output.AcceptedSession != expected) {
			StagedSession.reset();
			keep_renewed_launch_grant(aCandidate.Context);
			publish_and_execute(output, github_host_diagnostic_projection{ .BootstrapStage = github_bootstrap_failure_stage::session_acceptance, .GrantKept = remembered_grant_kept() });
			if (CredentialLifecycle == credential_lifecycle::provisional
				&& CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()
				&& ActiveVaultOperation == vault_operation::none) {
				start_delete(aCandidate.Context.IdentityGeneration,
							 std::nullopt);
			}
			return;
		}

		if (aCandidate.CacheStage.has_value()) {
			const auto committed = CapabilityCache.commit(*aCandidate.CacheStage);
			if (!committed.stored()
				|| committed.Entry != aCandidate.CapabilityEvidence) {
				StagedSession.reset();
				publish_diagnostic(github_host_diagnostic{
									   .Reason            = core::github_reason_code::malformed_response,
									   .RetryAfterSeconds = std::nullopt,
									   .RateLimits        = { },
									   .RateLimitCause    = std::nullopt,
								   },
								   github_bootstrap_failure_stage::session_acceptance);
				auto rollback = core::transition(
					State, core::deactivate_github_identity_intent{ });
				publish_and_execute(rollback);
				return;
			}
		}

		AcceptedSession            = model;
		AcceptedProfileAuthority   = aCandidate.ProfileAuthority;
		AcceptedCapabilityEvidence = aCandidate.CapabilityEvidence;
		AccountTransport           = aCandidate.AccountTransport;
		RateLimits                 = aCandidate.RateLimits;
		StagedSession.reset();
		SessionEnrollment.reset();
		SessionEnrollmentProvenance = github_credential_provenance::manual;
		SessionResumeProfile.reset();
		BootstrapViewer.reset();
		ViewerMode = viewer_mode::none;
		if (CredentialAuthority.has_value()) {
			CredentialLifecycle = credential_lifecycle::persistent;
		}
		LaunchRenewedGrant = false;
		persist_settled_device_binding();
		publish_profile_session();
		publish_transition(output, &*AcceptedSession);
		// Asked on every acceptance that has a slot, rather than only on
		// the ones a restart produced. The host cannot tell a credential
		// restored from a device grant from one restored from a pasted
		// token -- that is the whole reason the question is put to the
		// vault -- and a grant this run just authorized has not stored its
		// refresh material yet, so it answers `absent` and binds instead.
		if (CredentialAuthority.has_value()) {
			start_refresh_probe();
		}
	}

	void github_application_host::implementation::start_store(const core::github_session_request_context& aContext)
	{
		if (!CredentialAuthority.has_value() || !SessionEnrollment.has_value()) {
			fail_session(aContext, core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
			return;
		}
		const github_credential_operation_context operation{
			.Request            = aContext.Request,
			.ApplicationSession = aContext.ApplicationSession,
			.Authority          = *CredentialAuthority,
			.IdentityGeneration = aContext.IdentityGeneration,
		};
		ActiveVaultOperation = vault_operation::store;
		ActiveVaultContext   = operation;
		const auto  token    = ++VaultOperationToken;
		const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane     = &DecisionLane;
		const auto  callback =
			[gate, lane, token, operation](
				github_credential_store_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_credential_store_completion>(
						std::move(aCompletion));
					lane->post(
						[gate, payload, token, operation]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_store(
									token, operation, std::move(*payload));
							}
						});
				} catch (...) {
					try {
						lane->post(
							[gate, token, operation]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_store(
										token,
										operation,
										github_credential_store_error{
											.Context = operation,
											.Failure = github_credential_vault_failure::
												allocation_failure,
										});
								}
							});
					} catch (...) {
					}
				}
			};
		try {
			Credentials.store(
				github_credential_enrollment{
					.Context    = operation,
					.Credential = std::move(*SessionEnrollment),
				},
				callback);
		} catch (...) {
			callback(github_credential_store_error{
				.Context = operation,
				.Failure = github_credential_vault_failure::operation_failed,
			});
		}
		SessionEnrollment.reset();
		SessionEnrollmentProvenance = github_credential_provenance::manual;
	}

	void
	github_application_host::implementation::handle_store(const std::uint64_t                        aToken,
														  const github_credential_operation_context& aExpectedOperation,
														  github_credential_store_completion         aCompletion)
	{
		run_turn(
			[this,
			 aToken,
			 aExpectedOperation,
			 completion = std::move(aCompletion)]() mutable {
				handle_store_now(
					aToken, aExpectedOperation, std::move(completion));
			});
	}

	void github_application_host::implementation::handle_store_now(
		const std::uint64_t                        aToken,
		const github_credential_operation_context& aExpectedOperation,
		github_credential_store_completion         aCompletion)
	{
		if (aToken != VaultOperationToken) {
			return;
		}
		++VaultOperationToken;
		if (Closing) {
			if (ActiveVaultOperation == vault_operation::store
				&& ActiveVaultContext == aExpectedOperation) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				DeferredInvalidationGeneration.reset();
				if (requires_shutdown_credential_cleanup()) {
					start_delete(aExpectedOperation.IdentityGeneration,
								 std::nullopt);
				}
			} else {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				ShutdownCleanupFailed = true;
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
			}
			return;
		}
		if (DeferredInvalidationGeneration.has_value()) {
			const auto generation = *DeferredInvalidationGeneration;
			DeferredInvalidationGeneration.reset();
			const auto preserve = std::exchange(DeferredInvalidationPreservesCredential, false);
			// [[BPR-0547]]. The disposition was decided at the invalidate,
			// where the incoming enrollment is known; deferring the delete
			// past a running vault operation must not decide it again.
			const auto refresh   = std::exchange(DeferredInvalidationRetainsRefresh, false)
				? refresh_disposition::retain
				: refresh_disposition::erase;
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			if (preserve) {
				start_deferred_session();
			} else {
				start_delete(generation, std::nullopt, refresh);
			}
			return;
		}
		if (ActiveVaultOperation != vault_operation::store
			|| !ActiveVaultContext.has_value()
			|| *ActiveVaultContext != aExpectedOperation
			|| !CredentialSessionContext.has_value()) {
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			publish_diagnostic(github_host_diagnostic{
				.Reason            = core::github_reason_code::vault_unavailable,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			});
			if (CredentialSessionContext.has_value()) {
				fail_session(*CredentialSessionContext,
							 core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
			}
			return;
		}
		if (!valid(aCompletion)
			|| completion_context(aCompletion) != aExpectedOperation) {
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			if (CredentialSessionContext.has_value()) {
				fail_session(*CredentialSessionContext,
							 core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
			} else {
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
			}
			return;
		}
		const auto session   = *CredentialSessionContext;
		ActiveVaultOperation = vault_operation::none;
		ActiveVaultContext.reset();

		if (std::holds_alternative<github_credential_store_success>(aCompletion)) {
			ViewerMode = viewer_mode::account;
			start_retrieve(github_provider_attempt{ 1U });
			return;
		}
		if (std::holds_alternative<github_credential_store_cancelled>(
				aCompletion)) {
			cancel_session(session);
			return;
		}
		fail_session(session, core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
	}

	void github_application_host::implementation::start_retrieve(const github_provider_attempt aAttempt)
	{
		if (!CredentialAuthority.has_value()
			|| !CredentialSessionContext.has_value()) {
			return;
		}
		const auto&                               session = *CredentialSessionContext;
		const github_credential_operation_context operation{
			.Request            = session.Request,
			.ApplicationSession = session.ApplicationSession,
			.Authority          = *CredentialAuthority,
			.IdentityGeneration = session.IdentityGeneration,
		};
		PendingRetrieveAttempt = aAttempt;
		ActiveVaultOperation   = vault_operation::retrieve;
		ActiveVaultContext     = operation;
		const auto  token      = ++VaultOperationToken;
		const auto  gate       = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane       = &DecisionLane;
		const auto  callback =
			[gate, lane, token, operation](
				github_credential_retrieve_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_credential_retrieve_completion>(
						std::move(aCompletion));
					lane->post(
						[gate, payload, token, operation]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_retrieve(
									token, operation, std::move(*payload));
							}
						});
				} catch (...) {
					try {
						lane->post(
							[gate, token, operation]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_retrieve(
										token,
										operation,
										github_credential_retrieve_error{
											.Context = operation,
											.Failure = github_credential_vault_failure::
												allocation_failure,
										});
								}
							});
					} catch (...) {
					}
				}
			};
		try {
			Credentials.retrieve(operation, callback);
		} catch (...) {
			callback(github_credential_retrieve_error{
				.Context = operation,
				.Failure = github_credential_vault_failure::operation_failed,
			});
		}
	}

	void github_application_host::implementation::handle_retrieve(
		const std::uint64_t                        aToken,
		const github_credential_operation_context& aExpectedOperation,
		github_credential_retrieve_completion      aCompletion)
	{
		run_turn(
			[this,
			 aToken,
			 aExpectedOperation,
			 completion = std::move(aCompletion)]() mutable {
				handle_retrieve_now(
					aToken, aExpectedOperation, std::move(completion));
			});
	}
}
