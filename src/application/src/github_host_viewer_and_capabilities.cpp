#include "github_host_implementation.hpp"

#include <limits>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto capability_key(const github_profile_authority& aProfile,
										  const core::github_account_id   aAccount)
			-> github_capability_authority_key
		{
			return {
				.Profile               = aProfile.Profile,
				.ProfileRevision       = aProfile.ProfileRevision,
				.Host                  = aProfile.Host,
				.Account               = aAccount,
				.ServerContract        = aProfile.ServerContract,
				.ProbeContractRevision = aProfile.ProbeContractRevision,
			};
		}

		[[nodiscard]] auto credential_key(const core::bpr_profile_id aBprProfile, const github_profile_authority& aProfile,
										  const core::github_account_id aAccount)
			-> github_credential_authority_key
		{
			return {
				.Profile         = aProfile.Profile,
				.ProfileRevision = aProfile.ProfileRevision,
				.Host            = aProfile.Host,
				.Hostname        = aProfile.Hostname,
				.Account         = aAccount,
				.ServerContract  = aProfile.ServerContract,
				.BprProfile      = aBprProfile,
			};
		}

		[[nodiscard]] constexpr auto valid_capability_rate_limits(
			const github_profile_authority& aAuthority,
			const github_rate_limit_state&  aRateLimits) noexcept -> bool
		{
			return valid_rate_limit_resources(aRateLimits, false, false, true)
				&& !is_secondary_limited(aRateLimits)
				&& (!requires_rate_limit_observations(aAuthority.ServerContract)
					|| aRateLimits.Graphql.has_value());
		}

		[[nodiscard]] constexpr auto
		valid_provider_error_resources(const github_provider_error& aError,
									   const bool                   aAllowCore,
									   const bool                   aAllowSearch,
									   const bool                   aAllowGraphql) noexcept -> bool
		{
			return valid(aError)
				&& valid_rate_limit_resources(
					   aError.RateLimits, aAllowCore, aAllowSearch, aAllowGraphql);
		}
	}

	void github_application_host::implementation::submit_bootstrap_viewer(const github_viewer_request& aRequest)
	{
		if (!SessionEnrollment.has_value()) {
			fail_session(aRequest.Context, core::github_failure::vault);
			return;
		}
		if (AcceptedSetupBootstrap) {
			auto accepted = std::move(*AcceptedSetupBootstrap);
			AcceptedSetupBootstrap.reset();
			if (accepted.Request.Authority != aRequest.Authority) {
				fail_session(aRequest.Context, core::github_failure::malformed_response);
				return;
			}
			accepted.Request = aRequest;
			PendingProvider  = aRequest;
			const auto token = ++ProviderOperationToken;
			handle_viewer_now(token, std::move(accepted));
			return;
		}
		auto copy = github_credential_material::create(SessionEnrollment->bytes());
		if (!copy.has_value()) {
			fail_session(aRequest.Context, core::github_failure::vault);
			return;
		}
		submit_viewer(aRequest,
					  github_bootstrap_credential{
						  .Credential = std::move(*copy),
					  });
	}

	void github_application_host::implementation::submit_viewer(const github_viewer_request& aRequest,
																github_viewer_authentication aAuthentication)
	{
		PendingProvider      = aRequest;
		const auto  token    = ++ProviderOperationToken;
		const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane     = &DecisionLane;
		const auto  callback = [gate, lane, token, aRequest](
								   github_viewer_completion aCompletion) noexcept {
			const auto callbackTarget = gate.lock();
			if (!callbackTarget) {
				return;
			}
			const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
			if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
				return;
			}
			try {
				auto payload = std::make_shared<github_viewer_completion>(
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
							host->handle_viewer(token, std::move(*payload));
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
								host->handle_viewer(
									token,
									github_viewer_error{
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
			Viewers.observe_viewer(
				github_viewer_submission{
					.Request        = aRequest,
					.Authentication = std::move(aAuthentication),
				},
				callback);
		} catch (...) {
			callback(github_viewer_error{
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

	void github_application_host::implementation::handle_viewer(const std::uint64_t      aToken,
																github_viewer_completion aCompletion)
	{
		run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_viewer_now(aToken, std::move(completion)); });
	}

	void github_application_host::implementation::handle_viewer_now(const std::uint64_t      aToken,
																	github_viewer_completion aCompletion)
	{
		if (Closing || aToken != ProviderOperationToken) {
			return;
		}
		const auto* pending = std::get_if<github_viewer_request>(&PendingProvider);
		if (pending == nullptr) {
			return;
		}
		const auto current = *pending;
		if (aCompletion.valueless_by_exception()
			|| provider_completion_request(aCompletion) != current) {
			PendingProvider = std::monostate{ };
			fail_session(current.Context,
						 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
			return;
		}

		if (auto* success = std::get_if<github_viewer_success>(&aCompletion)) {
			const auto context   = success->Request.Context;
			const auto authority = success->Request.Authority;
			PendingProvider      = std::monostate{ };
			if (!valid_viewer(
					authority, context, success->Viewer, success->RateLimits)) {
				fail_session(context, core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
				return;
			}

			if (ViewerMode == viewer_mode::bootstrap) {
				if (success->AccountTransport != nullptr) {
					fail_session(context,
								 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
					return;
				}
				static_cast<void>(merge(RateLimits, success->RateLimits));
				BootstrapViewer = success->Viewer;
				// [[BPR-1494]]. Material re-bound to its account's earlier
				// profile must name that same account here, or nothing is kept.
				if (SessionExpectedAccount && success->Viewer.Account != *SessionExpectedAccount) {
					SessionEnrollment.reset();
					SessionEnrollmentProvenance = github_credential_provenance::manual;
					fail_session(context, core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity, account_mismatch());
					return;
				}
				// [[BPR-1494]]. Composition minted this profile before the
				// account was known. Now that `/user` names it, the account's
				// own earlier profile, if composition names one, takes the
				// same material before any vault write, so its
				// authority-scoped private state resumes.
				if (SessionEnrollmentRebind && SessionEnrollment.has_value() && !LaunchRenewal) {
					const auto rebind = std::exchange(SessionEnrollmentRebind, github_enrollment_rebind{ });
					auto       target = std::optional<core::host_profile_id>{ };
					try {
						target = rebind(authority, success->Viewer);
					} catch (...) {
						target.reset();
					}
					if (target && target->value() != 0U && *target != authority.Profile) {
						auto material = std::move(*SessionEnrollment);
						SessionEnrollment.reset();
						enroll_profile(*target, std::move(material), SessionEnrollmentProvenance, DeviceLifetime, DeviceRefreshSettled, { }, success->Viewer.Account);
						return;
					}
				}
				const auto adopted = credential_key(BprProfile, authority, success->Viewer.Account);
				// [[BPR-0644]]. A renewed grant resumes only the account its
				// binding named. Any other account's token is refused before
				// it reaches the vault.
				if (LaunchRenewal && LaunchRenewalEnrolled) {
					const auto renewal = *LaunchRenewal;
					LaunchRenewal.reset();
					LaunchRenewalEnrolled = false;
					LaunchRenewalBegun    = false;
					if (adopted != renewal.Authority) {
						// The owner's decision of 2026-09-24: material that
						// signs in as an account the binding never named is
						// kept like every other failure, not erased on its
						// own. The renewal's slot stays the remembered grant,
						// the failure says the account differs, and Sign out
						// or a new sign-in is what removes it.
						SessionEnrollment.reset();
						SessionEnrollmentProvenance = github_credential_provenance::manual;
						release_launch_renewal();
						keep_launch_renewal_slot(renewal.Authority, context);
						fail_session(context, core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity, account_mismatch());
						return;
					}
					// Until it is accepted, a failure keeps this grant as the
					// remembered one rather than erasing it as provisional.
					LaunchRenewedGrant = true;
				}
				resolve_retained_refresh(adopted);
				CredentialAuthority      = adopted;
				CredentialSessionContext = context;
				CredentialLifecycle      = credential_lifecycle::provisional;
				CredentialProvenance     = SessionEnrollmentProvenance;
				if (!CredentialBindings.remember(*CredentialAuthority,
												 CredentialProvenance)) {
					fail_session(context, core::github_failure::vault);
					return;
				}
				CredentialBindingRemembered = true;
				start_store(context);
				return;
			}

			if (success->AccountTransport == nullptr) {
				fail_session(context, core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
				return;
			}
			if (ViewerMode == viewer_mode::account) {
				if (!CredentialAuthority.has_value()
					|| success->Viewer.Account != CredentialAuthority->Account) {
					// [[BPR-0644]]. A remembered device grant whose `/user`
					// answers another account is kept, as the owner decided
					// on 2026-09-24, and the failure says the account
					// differs. Anything else is erased as before: a
					// provisional credential never became anyone's
					// remembered sign-in.
					fail_session(context,
								 core::github_failure::malformed_response,
								 !remembered_device_grant(), github_bootstrap_failure_stage::account_identity,
								 CredentialAuthority.has_value() ? std::optional{ account_mismatch() } : std::nullopt);
					return;
				}
				if (BootstrapViewer.has_value()
					&& success->Viewer != *BootstrapViewer) {
					fail_session(context,
								 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
					return;
				}
			}

			static_cast<void>(merge(RateLimits, success->RateLimits));
			AccountTransport = success->AccountTransport;
			begin_capabilities(
				context, authority, success->Viewer, success->AccountTransport);
			return;
		}

		if (auto* error = std::get_if<github_viewer_error>(&aCompletion)) {
			const auto errorValid = valid_provider_error_resources(
				error->Error, true, false, false);
			if (errorValid) {
				observe_provider_error(error->Error);
			}
			if (errorValid && retryable(error->Error)
				&& error->Request.Attempt.value()
					< maximum_github_provider_attempts) {
				auto retry    = error->Request;
				retry.Attempt = github_provider_attempt{
					static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
				};
				if (ViewerMode == viewer_mode::bootstrap) {
					submit_bootstrap_viewer(retry);
				} else if (ViewerMode == viewer_mode::account) {
					PendingProvider = std::monostate{ };
					start_retrieve(retry.Attempt);
				} else {
					submit_viewer(retry, std::monostate{ });
				}
				return;
			}
			PendingProvider = std::monostate{ };
			if (errorValid) {
				publish_error_advisory(error->Error);
			}
			fail_session(error->Request.Context,
						 errorValid
							 ? mapped_failure(error->Error.Failure)
							 : core::github_failure::malformed_response,
						 false, github_bootstrap_failure_stage::account_identity,
						 errorValid ? std::optional{ github_host_diagnostic{
										  .Reason            = core::reason_for(mapped_failure(error->Error.Failure)),
										  .RetryAfterSeconds = error->Error.RetryAfterSeconds,
										  .RateLimits        = RateLimits,
										  .RateLimitCause    = error->Error.RateLimitCause,
										  // [[BPR-0644]]. The provider's own account
										  // check refused a stored credential that
										  // answered as another account.
										  .AccountMismatch = error->Error.AccountMismatch,
									  } }
									: std::nullopt);
			return;
		}

		const auto& cancelled = std::get<github_viewer_cancelled>(aCompletion);
		PendingProvider       = std::monostate{ };
		cancel_session(cancelled.Request.Context);
	}

	auto github_application_host::implementation::next_epoch() -> std::optional<github_freshness_epoch>
	{
		if (LogicalEpoch == std::numeric_limits<std::uint64_t>::max()) {
			return std::nullopt;
		}
		++LogicalEpoch;
		return github_freshness_epoch{ LogicalEpoch };
	}

	void github_application_host::implementation::begin_capabilities(const core::github_session_request_context& aContext,
																	 const github_profile_authority&             aProfile,
																	 const github_viewer_record&                 aViewer,
																	 const github_account_transport&             aTransport)
	{
		const auto epoch = next_epoch();
		if (!epoch.has_value()) {
			fail_capability_session(aContext, core::github_failure::malformed_response, github_capability_diagnostic::host_epoch);
			return;
		}
		const auto key    = capability_key(aProfile, aViewer.Account);
		const auto lookup = CapabilityCache.lookup(key, *epoch);
		if (lookup.hit()) {
			accept_session(session_candidate{
				.Context            = aContext,
				.ProfileAuthority   = aProfile,
				.Viewer             = aViewer,
				.CapabilityEvidence = *lookup.Entry,
				.AccountTransport   = aTransport,
				.RateLimits         = RateLimits,
				.CacheStage         = std::nullopt,
			});
			return;
		}
		if (lookup.Status != github_capability_cache_lookup_status::miss
			&& lookup.Status != github_capability_cache_lookup_status::expired) {
			fail_capability_session(aContext, core::github_failure::malformed_response, github_capability_diagnostic::host_cache_lookup);
			return;
		}

		submit_capability(github_capability_request{
			.Context          = aContext,
			.ProfileAuthority = aProfile,
			.Viewer           = aViewer,
			.AccountTransport = aTransport,
			.Attempt          = github_provider_attempt{ 1U },
		});
	}

	void github_application_host::implementation::submit_capability(const github_capability_request& aRequest)
	{
		PendingProvider   = aRequest;
		const auto  token = ++ProviderOperationToken;
		const auto  gate  = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const lane  = &DecisionLane;
		const auto  callback =
			[gate, lane, token, aRequest](
				github_capability_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_capability_completion>(
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
								host->handle_capability(token, std::move(*payload));
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
									host->handle_capability(
										token,
										github_capability_error{
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
			Capabilities.probe_capabilities(aRequest, callback);
		} catch (...) {
			callback(github_capability_error{
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

	void github_application_host::implementation::handle_capability(const std::uint64_t          aToken,
																	github_capability_completion aCompletion)
	{
		run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_capability_now(aToken, std::move(completion)); });
	}

	void github_application_host::implementation::handle_capability_now(const std::uint64_t          aToken,
																		github_capability_completion aCompletion)
	{
		if (Closing || aToken != ProviderOperationToken) {
			return;
		}
		const auto* pending = std::get_if<github_capability_request>(&PendingProvider);
		if (pending == nullptr) {
			return;
		}
		const auto current = *pending;
		if (aCompletion.valueless_by_exception()
			|| provider_completion_request(aCompletion) != current) {
			PendingProvider = std::monostate{ };
			fail_capability_session(current.Context,
									core::github_failure::malformed_response, github_capability_diagnostic::host_correlation);
			return;
		}

		if (auto* success = std::get_if<github_capability_success>(&aCompletion)) {
			PendingProvider  = std::monostate{ };
			const auto epoch = next_epoch();
			if (!epoch.has_value()
				|| !valid_capability_rate_limits(
					success->Request.ProfileAuthority, success->RateLimits)) {
				fail_capability_session(success->Request.Context,
										core::github_failure::malformed_response, !epoch.has_value() ? github_capability_diagnostic::host_epoch : github_capability_diagnostic::host_rate_state);
				return;
			}
			const github_capability_probe_observation observation{
				.Authority   = capability_key(success->Request.ProfileAuthority,
											  success->Request.Viewer.Account),
				.ObservedAt  = *epoch,
				.Facts       = success->Facts,
				.SchemaFacts = success->SchemaFacts,
			};
			const auto staged = CapabilityCache.stage(
				github_capability_probe_result{ observation });
			if (!staged.staged()) {
				fail_capability_session(
					success->Request.Context,
					staged.Error
							== github_capability_cache_error::capacity_exhausted
						? core::github_failure::response_too_large
						: core::github_failure::malformed_response,
					github_capability_diagnostic::host_cache_stage);
				return;
			}
			static_cast<void>(merge(RateLimits, success->RateLimits));
			accept_session(session_candidate{
				.Context            = success->Request.Context,
				.ProfileAuthority   = success->Request.ProfileAuthority,
				.Viewer             = success->Request.Viewer,
				.CapabilityEvidence = staged.Stage->entry(),
				.AccountTransport   = success->Request.AccountTransport,
				.RateLimits         = RateLimits,
				.CacheStage         = staged.Stage,
			});
			return;
		}

		if (auto* error = std::get_if<github_capability_error>(&aCompletion)) {
			const auto errorValid = valid_provider_error_resources(
				error->Error, false, false, true);
			if (errorValid) {
				observe_provider_error(error->Error);
			}
			if (errorValid && retryable(error->Error)
				&& error->Request.Attempt.value()
					< maximum_github_provider_attempts) {
				auto retry    = error->Request;
				retry.Attempt = github_provider_attempt{
					static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
				};
				submit_capability(retry);
				return;
			}
			PendingProvider = std::monostate{ };
			if (errorValid) {
				publish_error_advisory(error->Error);
			}
			std::optional<github_host_diagnostic> diagnostic;
			if (errorValid) {
				diagnostic = github_host_diagnostic{
					.Reason            = core::reason_for(mapped_failure(error->Error.Failure)),
					.RetryAfterSeconds = error->Error.RetryAfterSeconds,
					.RateLimits        = RateLimits,
					.RateLimitCause    = error->Error.RateLimitCause,
				};
			}
			fail_capability_session(error->Request.Context,
									errorValid ? mapped_failure(error->Error.Failure) : core::github_failure::malformed_response,
									errorValid ? bounded_capability_diagnostic(error->CapabilityDetail) : github_capability_diagnostic::host_provider_error,
									std::move(diagnostic));
			return;
		}

		const auto& cancelled = std::get<github_capability_cancelled>(aCompletion);
		PendingProvider       = std::monostate{ };
		cancel_session(cancelled.Request.Context);
	}
}
