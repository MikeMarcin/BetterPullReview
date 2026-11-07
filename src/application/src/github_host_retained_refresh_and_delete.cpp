#include "github_host_implementation.hpp"

namespace bpr::application {
	namespace {
		// [[BPR-0540]]. Whether a refresh-slot erasure left renewal material
		// in the vault. A deleted slot and an absent one are the same
		// outcome: nothing renews this credential any more. Every error is
		// the other outcome, `unavailable` included -- since BPR-0542 and
		// BPR-0543 every shipped platform has a real second slot, so an
		// `unavailable` answer means the store could not be reached, exactly
		// as it does for the base slot beside it, and material this host
		// meant to destroy may still be resident under an authority the
		// reviewer has finished with. So is a terminal for the wrong
		// operation and a cancellation this host never asks for.
		[[nodiscard]] auto erasure_left_material(
			const github_credential_delete_completion& aCompletion,
			const github_credential_operation_context& aOperation) noexcept -> bool
		{
			if (aCompletion.valueless_by_exception() || !valid(aCompletion)
				|| completion_context(aCompletion) != aOperation) {
				return true;
			}
			return !std::holds_alternative<github_credential_delete_success>(
				aCompletion);
		}
	}

	void github_application_host::implementation::handle_retrieve_now(
		const std::uint64_t                        aToken,
		const github_credential_operation_context& aExpectedOperation,
		github_credential_retrieve_completion      aCompletion)
	{
		if (aToken != VaultOperationToken) {
			return;
		}
		++VaultOperationToken;
		if (Closing) {
			if (ActiveVaultOperation == vault_operation::retrieve
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
		if (ActiveVaultOperation != vault_operation::retrieve
			|| !ActiveVaultContext.has_value()
			|| *ActiveVaultContext != aExpectedOperation
			|| !CurrentProfile.has_value()
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
							 core::github_failure::vault);
			}
			return;
		}
		if (!valid(aCompletion)
			|| completion_context(aCompletion) != aExpectedOperation) {
			const auto context   = *CredentialSessionContext;
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			fail_session(context, core::github_failure::vault);
			return;
		}
		ActiveVaultOperation = vault_operation::none;
		ActiveVaultContext.reset();
		const auto context = *CredentialSessionContext;

		if (auto* success = std::get_if<github_credential_retrieve_success>(&aCompletion)) {
			submit_viewer(
				github_viewer_request{
					.Context   = context,
					.Authority = *CurrentProfile,
					.Attempt   = PendingRetrieveAttempt,
				},
				std::move(*success));
			return;
		}
		if (RestoredDeviceBinding) {
			// [[BPR-0644]]. A locked or refused keychain, a missing access
			// slot and an unreadable one all keep the remembered grant, under
			// the owner's rule of 2026-09-24: the launch says the credential
			// store failed, and the next launch tries again. Nothing resumes
			// from it meanwhile, and Sign out still erases both slots.
			fail_session(context, core::github_failure::vault);
			return;
		}
		if (std::holds_alternative<github_credential_retrieve_absent>(
				aCompletion)) {
			if (CredentialBindingRemembered) {
				CredentialBindings.forget(aExpectedOperation.Authority);
			}
			CredentialAuthority.reset();
			CredentialSessionContext.reset();
			CredentialLifecycle         = credential_lifecycle::none;
			CredentialProvenance        = github_credential_provenance::manual;
			CredentialBindingRemembered = false;
			fail_session_direct(context, core::github_failure::vault);
			return;
		}
		if (std::holds_alternative<github_credential_retrieve_cancelled>(
				aCompletion)) {
			cancel_session(context);
			return;
		}
		fail_session(context, core::github_failure::vault);
	}

	// The refresh slot's own erasure. It carries no answer back on the
	// ordinary occasions: an erased slot, an absent one, and a platform
	// whose store has no second slot are all "nothing renews this
	// credential any more", which is what the host already believes and
	// what the reviewer asked for. Only a failure crosses onto the
	// decision lane, because a failure means material this host meant to
	// destroy is still in the vault and somebody has to be told.
	//
	// The count, not the answer, is what quiescence reads: composition
	// holds this host and the vault alive until the terminal has been
	// delivered, exactly as it does for the credential delete beside it.
	void github_application_host::implementation::start_refresh_delete(
		const github_credential_operation_context& aOperation)
	{
		OutstandingRefreshDeletes.fetch_add(1U, std::memory_order_acq_rel);
		const auto  gate = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane = &DecisionLane;
		const auto  callback =
			[gate, lane, aOperation](
				github_credential_delete_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				auto* const           host = callbackTarget->Host.load(std::memory_order_acquire);
				if (host == nullptr) {
					return;
				}
				const auto leftMaterial
					= erasure_left_material(aCompletion, aOperation);
				// [[BPR-0545]], residual R1. The count is released on the
				// decision lane after the terminal has been applied, never
				// here beside the answer: released here, quiescence could
				// be observed between the release and the report, so an
				// orderly shutdown finished believing its cleanup was
				// exact while the report saying material is still in the
				// vault was still queued on the lane.
				try {
					lane->post([gate, aOperation, leftMaterial]() {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard completionBarrier{
							locked->Barrier
						};
						auto* const target = locked->Host.load(std::memory_order_acquire);
						if (target == nullptr) {
							return;
						}
						if (leftMaterial) {
							target->report_refresh_erasure_failure(aOperation);
						}
						target->OutstandingRefreshDeletes.fetch_sub(
							1U, std::memory_order_acq_rel);
						if (target->StartupCleanupOperation == aOperation) {
							target->run_turn([target] {
								target->StartupCleanupOperation.reset();
								target->start_deferred_session();
							});
						}
					});
				} catch (...) {
					// The lane refused the post, so no terminal will ever
					// be applied. Releasing the count here is what keeps a
					// shutdown from waiting on an answer that can no
					// longer arrive.
					host->OutstandingRefreshDeletes.fetch_sub(
						1U, std::memory_order_acq_rel);
				}
			};
		try {
			Credentials.delete_refresh(aOperation, callback);
		} catch (...) {
			callback(github_credential_delete_error{
				.Context = aOperation,
				.Failure = github_credential_vault_failure::operation_failed,
			});
		}
	}

	// [[BPR-0549]]. The window [[BPR-0547]]'s retention opens, and the
	// three ways out of it. The retention is decided at the invalidate,
	// where the incoming enrollment's provenance and profile are known but
	// its account is not: that arrives with the bootstrap `/user`
	// observation the enrollment goes on to make. So the slot is held --
	// with the exact operation context that would have erased it -- only
	// for as long as the incoming session can still turn out to name the
	// authority the slot is keyed under.
	//
	// Reachability is the whole test, and it is decided by the entire
	// authority: every platform vault keys the slot by the whole key, so
	// one that differs anywhere -- another account on this profile above
	// all -- is a slot the incoming session's own refresh probe cannot
	// name. Equality is therefore exactly "something may still spend
	// this", which is the rule [[BPR-0540]] set, and inequality erases
	// under the outgoing authority rather than the incoming one.
	void github_application_host::implementation::hold_retained_refresh(
		const github_credential_operation_context& aOperation)
	{
		// A shutdown that arrives while the deferred delete is still in
		// flight has already run its cleanup by the time this answers, so
		// there is no later occasion to hold the slot for.
		if (Closing) {
			start_refresh_delete(aOperation);
			return;
		}
		RetainedRefreshOperation = aOperation;
	}

	void github_application_host::implementation::erase_retained_refresh()
	{
		if (!RetainedRefreshOperation.has_value()) {
			return;
		}
		const auto operation = *RetainedRefreshOperation;
		RetainedRefreshOperation.reset();
		start_refresh_delete(operation);
	}

	void github_application_host::implementation::resolve_retained_refresh(
		const github_credential_authority_key& aAdopted)
	{
		if (!RetainedRefreshOperation.has_value()) {
			return;
		}
		if (RetainedRefreshOperation->Authority == aAdopted) {
			RetainedRefreshOperation.reset();
			return;
		}
		erase_retained_refresh();
	}

	void github_application_host::implementation::report_refresh_erasure_failure(
		const github_credential_operation_context& aOperation)
	{
		run_turn([this, aOperation] {
			static_cast<void>(aOperation);
			if (Closing) {
				ShutdownCleanupFailed = true;
			}
			publish_diagnostic(github_host_diagnostic{
				.Reason            = core::github_reason_code::vault_unavailable,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			});
		});
	}

	void github_application_host::implementation::start_delete_unblocked(const core::github_identity_generation   aGeneration,
																		 std::optional<deferred_session_terminal> aTerminal,
																		 const refresh_disposition                aRefresh)
	{
		if (!CredentialAuthority.has_value()
			|| !CredentialSessionContext.has_value()) {
			if (aTerminal.has_value()) {
				finish_deferred_terminal(*aTerminal);
			}
			return;
		}
		DeferredTerminal                                  = std::move(aTerminal);
		const auto&                               session = *CredentialSessionContext;
		const github_credential_operation_context operation{
			.Request            = session.Request,
			.ApplicationSession = session.ApplicationSession,
			.Authority          = *CredentialAuthority,
			.IdentityGeneration = aGeneration,
		};
		if (StartupCleanupOperation && StartupCleanupOperation->Authority == operation.Authority) {
			// A newer invalidation may have superseded the generation while the
			// tombstone was queued. Wait for the operation actually submitted.
			StartupCleanupOperation = operation;
		}
		ActiveVaultOperation = vault_operation::delete_credential;
		ActiveVaultContext   = operation;
		// [[BPR-0540]]. The refresh slot leaves with the credential it
		// renews, under the same authority, through the same port, on the
		// same occasion -- an orderly shutdown that deletes what it
		// retained, a sign-out, or an enrollment superseding the session
		// that stored it. Deleting one and keeping the other is what
		// orphaned six months of renewal material behind a session the
		// reviewer had already ended.
		//
		// [[BPR-0545]]. It is issued from this delete's own terminal
		// rather than beside it. Every shipped vault keys its pending
		// operations by the exact `github_credential_operation_context`
		// and answers a duplicate `invalid_request` before queuing, so
		// submitting both halves under one context lost whichever arrived
		// second -- ordinarily the base credential's own delete, because
		// the main thread wins the race to the second call. Sequencing
		// rather than a second context because request identity is the
		// core's to allocate and this host has no allocator of its own:
		// inventing one here to tell two halves of one occasion apart
		// would move identity into the host. What sequencing costs is a
		// crash between the two, which leaves renewable material behind an
		// erased credential; nothing can spend it, because the resume
		// answers only for device-grant provenance ([[BPR-0540]]).
		const auto  token = ++VaultOperationToken;
		const auto  gate  = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane  = &DecisionLane;
		const auto  callback =
			[gate, lane, token, operation, aRefresh](
				github_credential_delete_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_credential_delete_completion>(
						std::move(aCompletion));
					lane->post(
						[gate, payload, token, operation, aRefresh]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_delete(token,
													operation,
													aRefresh,
													std::move(*payload));
							}
						});
				} catch (...) {
					try {
						lane->post(
							[gate, token, operation, aRefresh]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_delete(
										token,
										operation,
										aRefresh,
										github_credential_delete_error{
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
			Credentials.delete_credential(operation, callback);
		} catch (...) {
			callback(github_credential_delete_error{
				.Context = operation,
				.Failure = github_credential_vault_failure::operation_failed,
			});
		}
	}

	void
	github_application_host::implementation::handle_delete(const std::uint64_t                        aToken,
														   const github_credential_operation_context& aExpectedOperation,
														   const refresh_disposition                  aRefresh,
														   github_credential_delete_completion        aCompletion)
	{
		run_turn(
			[this,
			 aToken,
			 aExpectedOperation,
			 aRefresh,
			 completion = std::move(aCompletion)]() mutable {
				handle_delete_now(aToken,
								  aExpectedOperation,
								  aRefresh,
								  std::move(completion));
			});
	}

	void github_application_host::implementation::handle_delete_now(
		const std::uint64_t                        aToken,
		const github_credential_operation_context& aExpectedOperation,
		const refresh_disposition                  aRefresh,
		github_credential_delete_completion        aCompletion)
	{
		if (aToken != VaultOperationToken) {
			return;
		}
		++VaultOperationToken;
		// [[BPR-0545]]. The occasion's other erasure, under the same
		// context, now that the credential's delete has left the vault's
		// pending map. It runs in every branch below, the failing ones
		// included: a credential delete that failed says nothing about
		// whether renewal material is still beside it, and the occasion
		// asked for both to go. The count is raised inside this turn, so
		// no quiescence window opens between the two operations.
		//
		// [[BPR-0547]]. Except on the occasion that decided at its
		// invalidate that the slot outlives this credential, because the
		// enrollment replacing it renews from that same slot.
		if (aRefresh == refresh_disposition::erase) {
			start_refresh_delete(aExpectedOperation);
		} else {
			hold_retained_refresh(aExpectedOperation);
		}
		if (Closing) {
			if (ActiveVaultOperation != vault_operation::delete_credential
				|| !ActiveVaultContext.has_value()
				|| *ActiveVaultContext != aExpectedOperation) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				DeferredTerminal.reset();
				ShutdownCleanupFailed = true;
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
				return;
			}

			if (!valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				DeferredTerminal.reset();
				ShutdownCleanupFailed = true;
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
				return;
			}

			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			DeferredTerminal.reset();
			if (std::holds_alternative<github_credential_delete_success>(
					aCompletion)) {
				if (CredentialBindingRemembered) {
					CredentialBindings.forget(aExpectedOperation.Authority);
				}
				CredentialAuthority.reset();
				CredentialSessionContext.reset();
				CredentialLifecycle         = credential_lifecycle::none;
				CredentialProvenance        = github_credential_provenance::manual;
				CredentialBindingRemembered = false;
			} else {
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
		if (ActiveVaultOperation != vault_operation::delete_credential
			|| !ActiveVaultContext.has_value()
			|| *ActiveVaultContext != aExpectedOperation || !valid(aCompletion)
			|| completion_context(aCompletion) != aExpectedOperation) {
			fail_delete_operation();
			return;
		}
		if (!std::holds_alternative<github_credential_delete_success>(
				aCompletion)) {
			fail_delete_operation();
			return;
		}

		const auto terminal = DeferredTerminal;
		DeferredTerminal.reset();
		ActiveVaultOperation = vault_operation::none;
		ActiveVaultContext.reset();
		if (CredentialBindingRemembered) {
			CredentialBindings.forget(aExpectedOperation.Authority);
		}
		CredentialAuthority.reset();
		CredentialSessionContext.reset();
		CredentialLifecycle         = credential_lifecycle::none;
		CredentialProvenance        = github_credential_provenance::manual;
		CredentialBindingRemembered = false;
		if (terminal.has_value()) {
			finish_deferred_terminal(*terminal);
		} else {
			start_deferred_session();
		}
	}
}
