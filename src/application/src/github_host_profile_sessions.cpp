#include "github_host_implementation.hpp"

namespace bpr::application {
	void github_application_host::implementation::execute(const core::github_host_request& aRequest)
	{
		std::visit(
			[this](const auto& aTypedRequest) {
				using request_type = std::decay_t<decltype(aTypedRequest)>;
				if constexpr (std::is_same_v<
								  request_type,
								  core::invalidate_github_identity_request>) {
					invalidate(aTypedRequest);
				} else if constexpr (std::is_same_v<
										 request_type,
										 core::establish_github_session_request>) {
					DeferredSession             = aTypedRequest.Context;
					SessionEnrollmentRebind     = std::exchange(NextEnrollmentRebind, github_enrollment_rebind{ });
					SessionExpectedAccount      = std::exchange(NextExpectedAccount, std::nullopt);
					SessionEnrollment           = std::move(NextEnrollment);
					DeviceLifetime              = std::exchange(NextDeviceLifetime, std::nullopt);
					DeviceRefreshSettled        = std::exchange(NextRefreshSettled, false);
					SessionEnrollmentProvenance = NextEnrollmentProvenance;
					NextEnrollment.reset();
					NextEnrollmentProvenance = github_credential_provenance::manual;
					NextEnrollmentProfile.reset();
					SessionResumeProfile = NextResumeProfile;
					SessionKeptEnding    = std::exchange(NextKeptEnding, std::nullopt);
					auto beginsRenewal   = false;
					auto endsRenewal     = false;
					if (PendingDeviceRestore && SessionResumeProfile == PendingDeviceRestore->Authority.Profile) {
						if (PendingDeviceRestoreRenews) {
							// [[BPR-0644]]. The stored access token is past its
							// lifetime, so this session waits for the renewal
							// rather than presenting it to `/user`. One that
							// cannot renew at all -- its refresh lifetime is
							// gone, or nothing is composed to renew through --
							// is held the same way and ended at once, so the
							// reviewer is told it needs signing in again.
							LaunchRenewal         = device_launch_renewal{ .Authority = PendingDeviceRestore->Authority, .IdentityGeneration = aTypedRequest.Context.IdentityGeneration, .Lifetime = PendingDeviceRestore->Lifetime };
							LaunchRenewalEnrolled = false;
							LaunchRenewalBegun    = !PendingDeviceRestoreExpired;
							beginsRenewal         = !PendingDeviceRestoreExpired;
							endsRenewal           = PendingDeviceRestoreExpired;
						} else {
							DeviceLifetime        = PendingDeviceRestore->Lifetime;
							DeviceRefreshSettled  = true;
							DeviceBindingDurable  = true;
							RestoredDeviceBinding = true;
						}
					}
					PendingDeviceRestore.reset();
					PendingDeviceRestoreRenews  = false;
					PendingDeviceRestoreExpired = false;
					NextResumeProfile.reset();
					SessionPublicPreview = NextPublicPreview;
					NextPublicPreview    = false;
					if (ActiveVaultOperation == vault_operation::none) {
						start_deferred_session();
					}
					if (beginsRenewal) {
						begin_launch_renewal();
					}
					if (endsRenewal) {
						// Ended on its own turn, like a sink that refused:
						// this runs inside the transition that admitted the
						// held session.
						DeferredIngress.emplace_back(deferred_grant_invalidation{ LaunchRenewal->Authority, LaunchRenewal->IdentityGeneration, core::github_failure::authentication });
						schedule_deferred_ingress();
					}
				} else {
					static_cast<void>(aTypedRequest);
				}
			},
			aRequest);
	}

	void github_application_host::implementation::invalidate(const core::invalidate_github_identity_request& aRequest)
	{
		if (BindingLoadPending) {
			PendingRestoreCleanup = aRequest;
		}
		// [[BPR-0644]]. The rotated grant's own enrollment -- a device grant
		// for the renewal's profile whose refresh slot is already settled,
		// which a new device flow's first enrollment never is -- carries the
		// renewal on to its `/user` account check. Anything else that
		// replaces the held or enrolling session -- Sign out, another
		// enrollment, a preview -- abandons it: the device host is told so
		// that no rotated token is enrolled over that choice, and the
		// renewal's exact slot becomes the credential this invalidate erases
		// below, so no spent or rotated material stays behind the tombstone.
		if (LaunchRenewal) {
			const auto own = !LaunchRenewalEnrolled && NextEnrollment.has_value() && NextEnrollmentProvenance == github_credential_provenance::device_grant
				&& NextEnrollmentProfile == LaunchRenewal->Authority.Profile && NextRefreshSettled;
			if (own) {
				LaunchRenewalEnrolled = true;
			} else {
				const auto renewal = *LaunchRenewal;
				LaunchRenewal.reset();
				LaunchRenewalEnrolled = false;
				if (std::exchange(LaunchRenewalBegun, false)) {
					release_launch_renewal();
				}
				if (!CredentialAuthority.has_value()) {
					adopt_launch_renewal_slot(renewal.Authority, core::github_session_request_context{ .Request = aRequest.Request, .ApplicationSession = aRequest.ApplicationSession, .Profile = renewal.Authority.Profile, .IdentityGeneration = aRequest.NewGeneration });
				}
			}
		}
		++BindingGeneration;
		DeviceBindingDurable  = false;
		RestoredDeviceBinding = false;
		LaunchRenewedGrant    = false;
		invalidate_epoch_authority();
		cancel_provider();
		// The answer described the identity being invalidated. Keeping it
		// would let a renewal be armed under a key this host has already
		// let go of.
		cancel_refresh_probe();
		// [[BPR-0549]]. A retention window still open at an invalidate
		// belongs to a session this invalidate is discarding before it
		// ever named an authority, so nothing is coming to spend the slot
		// it holds. The window this invalidate may open for itself is
		// opened later, when its own credential delete answers.
		erase_retained_refresh();
		const auto* resumeBinding          = NextResumeProfile.has_value()
			? CredentialBindings.find(*NextResumeProfile)
			: nullptr;
		const auto  preserveForExactResume = CredentialLifecycle == credential_lifecycle::persistent
			&& CredentialAuthority.has_value()
			&& CredentialSessionContext.has_value() && resumeBinding != nullptr
			&& *resumeBinding == *CredentialAuthority;
		// [[BPR-0547]]. The one occasion whose credential delete must not
		// carry the refresh slot with it. A renewal ends by storing the
		// rotated refresh material and then enrolling the rotated access
		// token through the device-grant enrollment boundary; that
		// enrollment activates the profile, and this invalidate is the
		// activation's other half. Erasing here destroys the slot the
		// rotation filled a moment earlier on this same lane, and the next
		// renewal answers `refresh_absent` -- renewal works exactly once.
		//
		// Retaining is not orphaning, which is the test [[BPR-0540]] set.
		// The enrollment superseding this session is itself a device grant
		// for the profile the slot is keyed under, so the session about to
		// be established binds that same authority and the renewal that
		// spends the slot is armed on it. Every other occasion still
		// erases: an orderly shutdown, a sign-out, an enrollment from any
		// other source, and a grant for another profile all end with
		// nobody able to spend what the slot holds.
		//
		// [[BPR-0549]] narrows that premise, which is only provisional
		// here: the incoming enrollment's profile is known at this point
		// and its account is not, so a grant for a different account on
		// this same profile satisfies every condition below. The slot is
		// held rather than kept, and the bootstrap `/user` that names the
		// incoming account either confirms this or erases it then.
		const auto supersededByItsOwnRenewal = NextEnrollment.has_value()
			&& NextEnrollmentProvenance
				== github_credential_provenance::device_grant
			&& CredentialProvenance
				== github_credential_provenance::device_grant
			&& CredentialAuthority.has_value()
			&& NextEnrollmentProfile.has_value()
			&& *NextEnrollmentProfile == CredentialAuthority->Profile;
		const auto invalidationRefresh   = supersededByItsOwnRenewal
			? refresh_disposition::retain
			: refresh_disposition::erase;
		const auto vaultOperationRunning = ActiveVaultOperation == vault_operation::store
			|| ActiveVaultOperation == vault_operation::retrieve;
		if (vaultOperationRunning) {
			if (ActiveVaultContext.has_value()) {
				try {
					Credentials.cancel(*ActiveVaultContext);
				} catch (...) {
				}
			}
			DeferredInvalidationGeneration          = aRequest.NewGeneration;
			DeferredInvalidationPreservesCredential = preserveForExactResume;
			DeferredInvalidationRetainsRefresh
				= invalidationRefresh == refresh_disposition::retain;
		} else {
			cancel_vault();
			DeferredInvalidationGeneration.reset();
			DeferredInvalidationPreservesCredential = false;
			DeferredInvalidationRetainsRefresh      = false;
		}
		StagedSession.reset();
		StagedSnapshot.reset();
		revoke_review_mutation_transport();
		AcceptedSnapshot.reset();
		RateLimits = { };
		AccountTransport.reset();
		CurrentProfile.reset();
		BootstrapViewer.reset();
		SessionEnrollment.reset();
		SessionEnrollmentProvenance = github_credential_provenance::manual;
		SessionEnrollmentRebind     = { };
		SessionExpectedAccount.reset();
		SessionResumeProfile.reset();
		SessionKeptEnding.reset();
		SessionPublicPreview = false;
		PublicPreviewRefusal.reset();
		EpochHandoffUnavailable = false;
		ViewerMode              = viewer_mode::none;
		DeferredTerminal.reset();
		DeferredSession.reset();
		publish_public_preview();

		if (aRequest.Previous.Profile.has_value()
			&& aRequest.Previous.Host.has_value()
			&& aRequest.Previous.Account.has_value()) {
			static_cast<void>(
				CapabilityCache.invalidate_identity(*aRequest.Previous.Profile,
													*aRequest.Previous.Host,
													*aRequest.Previous.Account));
		} else if (aRequest.Previous.Profile.has_value()) {
			static_cast<void>(
				CapabilityCache.invalidate_profile(*aRequest.Previous.Profile));
		}

		AcceptedSession.reset();
		AcceptedProfileAuthority.reset();
		AcceptedCapabilityEvidence.reset();
		if (CredentialAuthority.has_value()
			&& CredentialSessionContext.has_value()) {
			if (!vaultOperationRunning && !preserveForExactResume) {
				start_delete(aRequest.NewGeneration,
							 std::nullopt,
							 invalidationRefresh);
			}
		}
	}

	auto github_application_host::implementation::select_persistent_credential(
		const github_credential_authority_key&      aAuthority,
		const core::github_session_request_context& aContext) -> bool
	{
		if (aAuthority.BprProfile != BprProfile) {
			return false;
		}
		resolve_retained_refresh(aAuthority);
		CredentialAuthority      = aAuthority;
		CredentialSessionContext = aContext;
		CredentialLifecycle      = credential_lifecycle::persistent;
		// The origin travels with the binding rather than with the
		// gesture. A resume names a profile and nothing else, so this is
		// the only place a restored session can learn that what it is
		// about to be established from came from a device grant.
		CredentialProvenance        = CredentialBindings.provenance(aAuthority.Profile);
		CredentialBindingRemembered = true;
		return true;
	}

	void github_application_host::implementation::start_deferred_session()
	{
		if (Closing || !DeferredSession.has_value()
			|| ActiveVaultOperation != vault_operation::none || BindingErasePending || BindingLoadPending || StartupCleanupOperation || (LaunchRenewal && !LaunchRenewalEnrolled)) {
			return;
		}

		const auto context = *DeferredSession;
		DeferredSession.reset();
		if (const auto* binding = CredentialBindings.find(context.Profile); binding != nullptr && binding->BprProfile != BprProfile) {
			fail_session(context, core::github_failure::vault);
			return;
		}
		if (BindingLoadUnresolved) {
			// An unread binding may still describe retained vault material.
			// Refuse replacement for this lifetime instead of overwriting it.
			binding_failure();
			fail_session(context, core::github_failure::vault);
			return;
		}
		if (SessionKeptEnding.has_value()) {
			// [[BPR-1116]]. The session an accepted renewal gave up on,
			// resumed only to fail: its binding is selected again, as a
			// restored one whose row is live when it was durable, so the
			// failure keeps it and the card offers Sign out beside Sign in.
			const auto  ending = *std::exchange(SessionKeptEnding, std::nullopt);
			const auto* kept   = CredentialBindings.find(context.Profile);
			if (kept != nullptr && select_persistent_credential(*kept, context)) {
				RestoredDeviceBinding = ending.Durable;
				DeviceBindingDurable  = ending.Durable;
			}
			fail_session(context, ending.Failure);
			return;
		}
		if (SessionResumeProfile.has_value()) {
			if (const auto* retained = CredentialBindings.find(*SessionResumeProfile);
				retained != nullptr) {
				if (!select_persistent_credential(*retained, context)) {
					fail_session(context, core::github_failure::vault);
					return;
				}
			}
		}
		const github_profile_request request{
			.Context = context,
			.Attempt = github_provider_attempt{ 1U },
		};
		submit_profile(request);
	}

	void github_application_host::implementation::submit_profile(const github_profile_request& aRequest)
	{
		PendingProvider      = aRequest;
		const auto  token    = ++ProviderOperationToken;
		const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane     = &DecisionLane;
		const auto  callback = [gate, lane, token, aRequest](
								   github_profile_completion aCompletion) noexcept {
			const auto callbackTarget = gate.lock();
			if (!callbackTarget) {
				return;
			}
			const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
			if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
				return;
			}
			try {
				auto payload = std::make_shared<github_profile_completion>(
					std::move(aCompletion));
				lane->post(
					[gate, payload, token]() mutable {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard completionBarrier{
							locked->Barrier
						};
						auto* const host = locked->Host.load(std::memory_order_acquire);
						if (host != nullptr) {
							host->handle_profile(token, std::move(*payload));
						}
					});
			} catch (...) {
				try {
					lane->post(
						[gate, token, aRequest]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_profile(
									token,
									github_profile_error{
										.Request = aRequest,
										.Error   = {
											.Failure = github_provider_failure::
												response_limit,
											.RetryAfterSeconds = std::nullopt,
											.RateLimits        = { },
											.RateLimitCause    = std::nullopt,
										},
									});
							}
						});
				} catch (...) {
				}
			}
		};
		try {
			Profiles.resolve_profile(aRequest, callback);
		} catch (...) {
			callback(github_profile_error{
				.Request = aRequest,
				.Error   = {
					.Failure           = github_provider_failure::response_limit,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			});
		}
	}

	void github_application_host::implementation::handle_profile(const std::uint64_t       aToken,
																 github_profile_completion aCompletion)
	{
		run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_profile_now(aToken, std::move(completion)); });
	}

	void github_application_host::implementation::handle_profile_now(const std::uint64_t       aToken,
																	 github_profile_completion aCompletion)
	{
		if (Closing || aToken != ProviderOperationToken) {
			return;
		}
		const auto* pending = std::get_if<github_profile_request>(&PendingProvider);
		if (pending == nullptr) {
			return;
		}
		const auto current = *pending;
		if (aCompletion.valueless_by_exception()
			|| provider_completion_request(aCompletion) != current) {
			PendingProvider = std::monostate{ };
			fail_session(current.Context,
						 core::github_failure::malformed_response);
			return;
		}

		if (auto* success = std::get_if<github_profile_success>(&aCompletion)) {
			const auto context = success->Request.Context;
			PendingProvider    = std::monostate{ };
			if (!valid(success->Authority)
				|| success->Authority.Profile != context.Profile) {
				fail_session(context, core::github_failure::malformed_response);
				return;
			}
			CurrentProfile = success->Authority;

			if (SessionPublicPreview) {
				// The signed-out course ends here. No viewer is requested,
				// no capability is probed, and no vault is consulted: the
				// authority the profile resolved to is the whole of what
				// this mode observes.
				begin_public_preview(context, success->Authority);
				return;
			}

			if (success->Authority.Authentication
				== github_authentication_transport::github_cli) {
				const auto hasNativeMaterial = SessionEnrollment.has_value()
					|| SessionResumeProfile.has_value();
				if (const auto* retained = CredentialBindings.find(context.Profile);
					retained != nullptr) {
					if (!select_persistent_credential(*retained, context)) {
						fail_session(context, core::github_failure::vault);
						return;
					}
					if (hasNativeMaterial) {
						SessionEnrollment.reset();
						SessionEnrollmentProvenance = github_credential_provenance::manual;
						SessionResumeProfile.reset();
						fail_session(context,
									 core::github_failure::malformed_response,
									 true);
					} else {
						DeferredSession = context;
						start_delete(context.IdentityGeneration, std::nullopt);
					}
					return;
				}
				if (hasNativeMaterial) {
					SessionEnrollment.reset();
					SessionEnrollmentProvenance = github_credential_provenance::manual;
					SessionResumeProfile.reset();
					fail_session(context,
								 core::github_failure::malformed_response);
					return;
				}
				ViewerMode = viewer_mode::cli;
				submit_viewer(
					github_viewer_request{
						.Context   = context,
						.Authority = success->Authority,
						.Attempt   = github_provider_attempt{ 1U },
					},
					std::monostate{ });
				return;
			}

			if (SessionEnrollment.has_value()
				&& SessionResumeProfile.has_value()) {
				fail_session(context, core::github_failure::malformed_response);
				return;
			}
			if (SessionEnrollment.has_value()) {
				if (const auto* retained = CredentialBindings.find(context.Profile);
					retained != nullptr) {
					if (!select_persistent_credential(*retained, context)) {
						fail_session(context, core::github_failure::vault);
						return;
					}
					DeferredSession = context;
					start_delete(context.IdentityGeneration, std::nullopt);
					return;
				}
				ViewerMode = viewer_mode::bootstrap;
				submit_bootstrap_viewer(github_viewer_request{
					.Context   = context,
					.Authority = success->Authority,
					.Attempt   = github_provider_attempt{ 1U },
				});
				return;
			}
			if (!SessionResumeProfile.has_value()
				|| *SessionResumeProfile != context.Profile) {
				fail_session(context, core::github_failure::vault);
				return;
			}

			const auto* retained = CredentialBindings.find(context.Profile);
			if (retained == nullptr) {
				fail_session(context, core::github_failure::vault);
				return;
			}
			const auto resumeAuthority = *retained;
			if (!select_persistent_credential(resumeAuthority, context)) {
				fail_session(context, core::github_failure::vault);
				return;
			}
			SessionResumeProfile.reset();
			if (resumeAuthority.Profile != success->Authority.Profile
				|| resumeAuthority.ProfileRevision
					!= success->Authority.ProfileRevision
				|| resumeAuthority.Host != success->Authority.Host
				|| resumeAuthority.Hostname != success->Authority.Hostname
				|| resumeAuthority.ServerContract
					!= success->Authority.ServerContract) {
				fail_session(
					context, core::github_failure::malformed_response, true);
				return;
			}
			ViewerMode = viewer_mode::account;
			start_retrieve(github_provider_attempt{ 1U });
			return;
		}

		if (auto* error = std::get_if<github_profile_error>(&aCompletion)) {
			const auto errorValid = valid(error->Error)
				&& !has_observations(error->Error.RateLimits);
			if (errorValid && retryable(error->Error)
				&& error->Request.Attempt.value()
					< maximum_github_provider_attempts) {
				auto retry    = error->Request;
				retry.Attempt = github_provider_attempt{
					static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
				};
				submit_profile(retry);
				return;
			}
			PendingProvider = std::monostate{ };
			if (errorValid) {
				publish_error_advisory(error->Error);
			}
			fail_session(error->Request.Context,
						 errorValid
							 ? mapped_failure(error->Error.Failure)
							 : core::github_failure::malformed_response);
			return;
		}

		const auto& cancelled = std::get<github_profile_cancelled>(aCompletion);
		PendingProvider       = std::monostate{ };
		cancel_session(cancelled.Request.Context);
	}
}
