#include "epoch_host_implementation.hpp"

#include "bpr/application/git_pack_stream.hpp"
#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/core/epoch_transition.hpp"

namespace bpr::application {
	namespace {
		[[nodiscard]] auto smart_transport_failure(const git_smart_transport_error& aFailure) noexcept
			-> core::epoch_failure
		{
			switch (aFailure.Step) {
			case git_smart_transport_step::git_version:
			case git_smart_transport_step::git_capability_probe:
				return core::epoch_failure::git_tool_probe_failed;
			case git_smart_transport_step::git_qualification:
				return core::epoch_failure::git_tool_unsupported;
			case git_smart_transport_step::cli_version:
			case git_smart_transport_step::cli_capability_probe:
				return core::epoch_failure::gh_tool_probe_failed;
			case git_smart_transport_step::cli_qualification:
				return core::epoch_failure::gh_tool_unsupported;
			default:
				break;
			}
			// [[BPR-1857]]. Every other failure used to collapse into
			// `unavailable`, so a Windows clone root that could not exist read
			// "the exact comparison is unavailable" and nothing more. The kind
			// says which of five things to look at; a step that only touches
			// local storage says it was the folder.
			switch (aFailure.Failure) {
			case git_smart_transport_failure::authentication:
			case git_smart_transport_failure::permission:
				return core::epoch_failure::fetch_sign_in_refused;
			case git_smart_transport_failure::network:
			case git_smart_transport_failure::certificate:
			case git_smart_transport_failure::proxy:
			case git_smart_transport_failure::redirect:
			case git_smart_transport_failure::timeout:
				return core::epoch_failure::fetch_network;
			case git_smart_transport_failure::request_limit:
			case git_smart_transport_failure::response_limit:
				return core::epoch_failure::fetch_limit;
			case git_smart_transport_failure::collision_detected:
				return core::epoch_failure::unsupported_object;
			case git_smart_transport_failure::invalid_request:
			case git_smart_transport_failure::unavailable:
			case git_smart_transport_failure::cancelled:
			case git_smart_transport_failure::malformed_response:
			case git_smart_transport_failure::consumer_rejected:
				break;
			}
			if (aFailure.Failure == git_smart_transport_failure::cancelled) {
				return core::epoch_failure::unavailable;
			}
			switch (aFailure.Step) {
			case git_smart_transport_step::temporary_store:
			case git_smart_transport_step::process_runner:
			case git_smart_transport_step::local_store:
				return core::epoch_failure::fetch_storage;
			case git_smart_transport_step::clone:
				return core::epoch_failure::fetch_failed;
			default:
				return core::epoch_failure::unavailable;
			}
		}

		[[nodiscard]] auto exact_append_reservation(
			const append_operation_record&              aRecord,
			const append_reservation_request&           aRequest,
			const core::pull_request_authority_scope_id aAuthority) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Context == aRequest.Context
				&& aRecord.ReservedOrdinal == aRequest.ProposedOrdinal
				&& aRecord.OperationRevision
				== epoch_catalog_operation_revision{ 1 }
			&& aRecord.Phase == epoch_catalog_operation_phase::preparing
				&& !aRecord.RepairFromPhase && !aRecord.RecoveryInputCompacted
				&& !aRecord.Receiving && !aRecord.Spool && !aRecord.Preimport && !aRecord.Install
				&& !aRecord.Candidate
				&& aRecord.StagingBase
				== epoch_catalog_ref_identity{
					   .Authority = aAuthority,
					   .Ordinal   = aRequest.ProposedOrdinal,
					   .Operation = aRequest.Context.Operation,
					   .Staging   = true,
				   }
			&& aRecord.StagingHead == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
				   .Operation = aRequest.Context.Operation,
				   .Staging   = true,
				   .Head      = true,
			   }
			&& aRecord.FinalBase == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
			   }
			&& aRecord.FinalHead == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
				   .Head      = true,
			   };
		}

		[[nodiscard]] auto exact_spool_record(
			const append_operation_record& aPrior,
			const append_operation_record& aNext,
			const record_spool_request&    aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase == epoch_catalog_operation_phase::spool_recorded
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Receiving == aPrior.Receiving
				&& aNext.Spool == std::optional{ aRequest.Spool }
			&& aNext.Preimport == std::optional{ aRequest.Preimport }
			&& aNext.Install == aPrior.Install
				&& aNext.Candidate == aPrior.Candidate;
		}

		[[nodiscard]] auto exact_install_record(
			const append_operation_record& aPrior,
			const append_operation_record& aNext,
			const record_install_request&  aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase == epoch_catalog_operation_phase::imported
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Receiving == aPrior.Receiving
				&& aNext.Spool == aPrior.Spool && aNext.Preimport == aPrior.Preimport
				&& aNext.Install == std::optional{ aRequest.Install }
			&& aNext.Candidate == aPrior.Candidate;
		}

		[[nodiscard]] auto exact_prepared_revision(
			const git_prepared_revision&        aPrepared,
			const git_prepare_revision_request& aRequest,
			const git_revision_residency        aResidency) noexcept -> bool
		{
			return application::valid(aPrepared)
				&& aPrepared.Residency == aResidency
				&& aPrepared.Operation == aRequest.Authorization.Operation
				&& aPrepared.Pack == aRequest.Authorization.ExpectedPack
				&& aPrepared.Base == aRequest.Base && aPrepared.Head == aRequest.Head
				&& aPrepared.ImportedBytes == aRequest.Receipt.Spool.Bytes
				&& aPrepared.InstalledPack.Bytes == aRequest.Receipt.Spool.Bytes;
		}
	}

	auto epoch_application_host::implementation::active_matches(
		const pull_request_authority_invalidation& aInvalidation) const noexcept
		-> bool
	{
		return Active
			&& aInvalidation.ApplicationSession
			== Active->Handoff.ApplicationSession
			&& aInvalidation.Profile == Active->Authority.Profile
			&& aInvalidation.ProfileRevision
			== Active->Authority.ProfileRevision
			&& aInvalidation.Host == Active->Authority.Host
			&& aInvalidation.HostAuthority == Active->Authority.HostAuthority
			&& aInvalidation.Account == Active->Authority.Account
			&& aInvalidation.Viewer == Active->Authority.Viewer
			&& aInvalidation.Repository == Active->Authority.Repository
			&& aInvalidation.PullRequest == Active->Authority.PullRequest
			&& aInvalidation.IdentityGeneration
			== Active->Revision.Source.IdentityGeneration;
	}

	void epoch_application_host::implementation::invalidate(const pull_request_authority_invalidation aInvalidation)
	{
		if (Closing || !application::valid(aInvalidation)) {
			return;
		}
		if (active_matches(aInvalidation)) {
			cancel_authority_jobs();
			revoke_blob_authority(
				Active->Actor, blob_authority_for(*Active));
		}
		if (TurnActive || deferred_work_pending()) {
			static_cast<void>(defer_invalidation(aInvalidation));
			return;
		}
		run_turn([this, &aInvalidation] {
			invalidate_now(aInvalidation);
		});
	}

	void epoch_application_host::implementation::invalidate_now(
		const pull_request_authority_invalidation& aInvalidation)
	{
		if (PendingAuthority == std::optional{ aInvalidation }) {
			++OfferToken;
			if (PendingOfferToken) {
				cancel_offer_job(*PendingOfferToken);
			}
			PendingAuthority.reset();
			PendingOfferToken.reset();
		}
		if (!active_matches(aInvalidation)) {
			return;
		}
		cancel_authority_jobs();
		revoke_blob_authority(
			Active->Actor, blob_authority_for(*Active));
		if (Active->Handoff.SmartGit) {
			Active->Handoff.SmartGit->revoke();
		}
		dispatch_core(core::pull_request_revision_invalidated_event{
			.ApplicationSession = aInvalidation.ApplicationSession,
			.Authority          = Active->Authority,
			.Revision           = Active->Revision,
			.Generation         = State.EpochGeneration,
		});
		Active.reset();
	}

	void epoch_application_host::implementation::dispatch(const epoch_intent aIntent)
	{
		if (!Consumer->current() || Closing) {
			return;
		}
		if (TurnActive || deferred_work_pending()) {
			static_cast<void>(defer_ordinary(deferred_dispatch{ aIntent }));
			return;
		}
		run_turn([this, &aIntent] {
			dispatch_now(aIntent);
		});
	}

	void epoch_application_host::implementation::dispatch_now(const epoch_intent& aIntent)
	{
		if (!Consumer->current() || Closing) {
			return;
		}
		std::visit(
			[this](const auto& aValue) {
				dispatch_core(core::epoch_event{ aValue });
			},
			aIntent);
	}

	void epoch_application_host::implementation::prepare_runtime()
	{
		// [[BPR-1888]]. Best effort before this window's first epoch. Leave
		// two slots free for the first comparison and its newest history
		// page; constrained or already-busy hosts keep demand-driven setup.
		// The normal job ledger owns
		// cancellation, completion delivery and shutdown accounting.
		if (!Consumer->current() || Closing || DeferredShutdown || TurnActive || deferred_work_pending()
			|| RuntimePreparationStarted || Active || job_count() != 0U || Limits.OutstandingJobs < 3U) {
			return;
		}
		run_turn([this] {
			RuntimePreparationStarted = true;
			submit_effect(
				[caches = &Caches](const std::stop_token aStop) -> worker_completion {
					if (!aStop.stop_requested()) {
						caches->prepare_runtime(aStop);
					}
					return [](implementation&) { };
				},
				[](implementation&) { },
				[](implementation&, core::epoch_failure) { });
		});
	}

	void epoch_application_host::implementation::shutdown()
	{
		Consumer->retire();
		close_blob_ingress_and_cancel_jobs();
		if (TurnActive || deferred_work_pending()) {
			defer_shutdown();
			return;
		}
		run_turn([this] {
			shutdown_now();
		});
	}

	void epoch_application_host::implementation::shutdown_now()
	{
		if (Closing) {
			return;
		}
		Closing = true;
		++OfferToken;
		close_blob_ingress_and_cancel_jobs();
		PendingAuthority.reset();
		PendingOfferToken.reset();
		if (Active) {
			revoke_blob_authority(
				Active->Actor, blob_authority_for(*Active));
			if (Active->Handoff.SmartGit) {
				Active->Handoff.SmartGit->revoke();
			}
			dispatch_core(core::pull_request_revision_invalidated_event{
				.ApplicationSession = State.ApplicationSession,
				.Authority          = Active->Authority,
				.Revision           = Active->Revision,
				.Generation         = State.EpochGeneration,
			});
			Active.reset();
		}
		std::scoped_lock actorsLock{ CacheActorsMutex };
		CacheActors.clear();
	}

	void epoch_application_host::implementation::start_catalog_load(const core::load_epoch_catalog_request& aRequest)
	{
		auto context = exact_context(
			aRequest.Context.Authority, aRequest.Context.Revision);
		if (!context) {
			return;
		}
		auto actor = context->Actor;
		submit_effect(
			[context, aRequest, diagnostics = epoch_diagnostics{ DiagnosticLog, aRequest.Context.Request.value(), aRequest.Context.ApplicationSession.value() }](const std::stop_token aStop)
				-> worker_completion {
				const auto snapshot = diagnostics.perform(
					diagnostic_epoch_stage::catalog_prepare,
					[&] { return context->Catalog->snapshot(aStop); });
				if (!snapshot) {
					if (snapshot.error() == epoch_catalog_failure::cancelled) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_catalog_load_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					}
					const auto failure = catalog_failure(snapshot.error());
					return [aRequest, failure](implementation& aHost) {
						aHost.dispatch_core(core::epoch_catalog_load_failed_event{
							.Context = aRequest.Context,
							.Failure = failure,
						});
					};
				}
				if (
					!application::valid(snapshot->Binding)
					|| snapshot->Binding != context->Catalog->binding()) {
					return [aRequest](implementation& aHost) {
						aHost.dispatch_core(core::epoch_catalog_load_failed_event{
							.Context = aRequest.Context,
							.Failure = core::epoch_failure::repair_required,
						});
					};
				}
				const auto catalog = snapshot->Binding.Catalog;
				return [aRequest, catalog](implementation& aHost) {
					aHost.dispatch_core(core::epoch_catalog_loaded_event{
						.Context = aRequest.Context,
						.Catalog = catalog,
					});
				};
			},
			[aRequest](implementation& aHost) {
				aHost.dispatch_core(core::epoch_catalog_load_failed_event{
					.Context = aRequest.Context,
					.Failure = core::epoch_failure::unavailable,
				});
			},
			[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
				aHost.dispatch_core(core::epoch_catalog_load_failed_event{
					.Context = aRequest.Context,
					.Failure = aFailure,
				});
			},
			aRequest.Context.Request,
			aRequest.Context.Generation,
			std::nullopt,
			std::nullopt,
			std::move(actor));
	}

	void epoch_application_host::implementation::start_revision_preparation(
		const core::prepare_epoch_revision_request& aRequest)
	{
		auto context = exact_context(
			aRequest.Context.Authority, aRequest.Context.Revision);
		if (!context) {
			return;
		}
		auto* const caches  = &Caches;
		auto* const entropy = &Entropy;
		auto* const clock   = &Clock;
		auto        actor   = context->Actor;
		submit_effect(
			[context, aRequest, caches, entropy, clock, diagnostics = epoch_diagnostics{ DiagnosticLog, aRequest.Context.Request.value(), aRequest.Context.ApplicationSession.value() }](
				const std::stop_token aStop) -> worker_completion {
				const auto cancelled = [aRequest]() -> worker_completion {
					return [aRequest](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_revision_prepare_cancelled_event{
								.Context = aRequest.Context,
							});
					};
				};
				const auto failed = [aRequest, diagnostics](const core::epoch_failure aFailure)
					-> worker_completion {
					diagnostics.failed(diagnostic_epoch_stage::revision_prepare, aFailure);
					return [aRequest, aFailure](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_revision_prepare_failed_event{
								.Context = aRequest.Context,
								.Failure = aFailure,
							});
					};
				};
				if (aStop.stop_requested()) {
					return cancelled();
				}
				const auto snapshot = diagnostics.perform(
					diagnostic_epoch_stage::catalog_prepare,
					[&] { return context->Catalog->snapshot(aStop); });
				if (!snapshot) {
					return snapshot.error() == epoch_catalog_failure::cancelled
						? cancelled()
						: failed(catalog_failure(snapshot.error()));
				}
				if (
					!application::valid(snapshot->Binding)
					|| snapshot->Binding != context->Catalog->binding()) {
					return failed(core::epoch_failure::repair_required);
				}
				const auto expectedCatalog = core::epoch_catalog_identity{
					.Incarnation = aRequest.Context.CatalogIncarnation,
					.Revision    = aRequest.Context.ExpectedCatalogRevision,
				};
				if (snapshot->Binding.Catalog != expectedCatalog) {
					return failed(core::epoch_failure::catalog_conflict);
				}

				const auto effect = core::plan_epoch_preparation(
					context->Authority,
					aRequest.Context.Revision,
					expectedCatalog,
					core::epoch_preparation_catalog_observation{
						.LatestLive           = snapshot->LatestLive,
						.RetentionRehydration = snapshot->RetentionRehydration,
					});
				if (effect.Rejection != core::epoch_reason_code::none) {
					return failed(core::epoch_failure::malformed_observation);
				}

				const auto canonicalReview =
					[&](const std::optional<projected_append_head> aProjected)
					-> std::expected<
						std::optional<core::catalog_review_association>,
						core::epoch_failure> {
					if (!aRequest.Context.Revision.CurrentViewerReview) {
						return std::nullopt;
					}
					for (std::size_t attempt = 0U;
						 attempt < maximum_identity_attempts;
						 ++attempt) {
						const auto association = mint_identity<core::review_association_id>(
							*entropy);
						if (!association) {
							return std::unexpected{
								core::epoch_failure::unavailable
							};
						}
						const auto review = diagnostics.perform(
							diagnostic_epoch_stage::catalog_prepare,
							[&] { return context->Catalog->canonical_review(
									  canonical_review_request{
										  .Catalog             = expectedCatalog,
										  .Candidate           = *aRequest.Context.Revision.CurrentViewerReview,
										  .ProposedAssociation = *association,
										  .ProjectedHead       = aProjected,
									  },
									  aStop); });
						if (review) {
							return std::optional{ *review };
						}
						if (review.error()
							!= epoch_catalog_failure::
								operation_identity_conflict) {
							return std::unexpected{
								catalog_failure(review.error())
							};
						}
					}
					return std::unexpected{
						core::epoch_failure::catalog_conflict
					};
				};

				const auto observedTime = [&]()
					-> std::optional<core::observed_utc_milliseconds> {
					const auto observed = clock->observe_utc();
					return observed && core::valid(*observed)
						? observed
						: std::nullopt;
				};

				if (effect.Effect == core::epoch_preparation_effect::none) {
					const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
					const auto review    = canonicalReview(std::nullopt);
					const auto observed  = observedTime();
					if (!operation || !review || !observed) {
						return failed(
							review ? core::epoch_failure::unavailable
								   : review.error());
					}
					const auto prepared = prepared_operation{
						.Kind      = prepared_kind::observation_only,
						.Operation = *operation,
					};
					if (!context->store(prepared)) {
						return failed(core::epoch_failure::unavailable);
					}
					const auto event = core::epoch_revision_prepared_event{
						.Context     = aRequest.Context,
						.Operation   = *operation,
						.Observation = {
							.LatestLive                  = snapshot->LatestLive,
							.CatalogCurrentViewerReview  = snapshot->CurrentViewerReview,
							.ProposedCurrentViewerReview = *review,
							.ObservedAt                  = *observed,
							.HeadLineage                 = core::head_lineage::not_applicable,
							.RetentionRehydration        = snapshot->RetentionRehydration,
						},
					};
					return [context, event](implementation& aHost) {
						const auto output        = aHost.dispatch_core(event);
						const auto liveUnchanged = std::ranges::any_of(
							output.Requests,
							[](const auto& aNext) {
								return std::holds_alternative<core::resolve_comparison_request>(aNext);
							});
						const auto accepted = std::ranges::any_of(
							output.Requests,
							[](const auto& aNext) {
								return std::holds_alternative<
										   core::commit_epoch_catalog_request>(
										   aNext)
									|| std::holds_alternative<
										   core::discard_epoch_preparation_request>(
										   aNext);
							});
						if (!accepted) {
							static_cast<void>(
								aHost.discard_context_preparation(
									context, event.Operation));
						} else if (liveUnchanged && aHost.Active == context
								   && aHost.State.Catalog == context->Catalog->binding().Catalog) {
							aHost.start_historical_import(context, event.Context.Request,
														  event.Context.Generation);
						}
					};
				}

				if (!snapshot->NextOrdinal) {
					return failed(core::epoch_failure::ordinal_exhausted);
				}

				auto prepared = prepared_operation{
					.Kind = prepared_kind::append,
				};
				for (std::size_t attempt = 0U;
					 attempt < maximum_identity_attempts;
					 ++attempt) {
					const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
					if (!operation) {
						return failed(core::epoch_failure::unavailable);
					}
					const auto reservation = append_reservation_request{
						.Context = {
							.ApplicationSession = aRequest.Context.ApplicationSession,
							.Request            = aRequest.Context.Request,
							.Generation         = aRequest.Context.Generation,
							.Operation          = *operation,
							.Revision           = aRequest.Context.Revision,
							.Catalog            = expectedCatalog,
						},
						.ProposedOrdinal = *snapshot->NextOrdinal,
					};
					const auto reserved = diagnostics.perform(
						diagnostic_epoch_stage::catalog_prepare,
						[&] { return context->Catalog->reserve_append(
								  reservation, aStop); });
					if (reserved) {
						if (!exact_append_reservation(
								*reserved,
								reservation,
								context->Authority.Scope)) {
							return failed(
								core::epoch_failure::repair_required);
						}
						prepared.Operation = *operation;
						prepared.Record    = *reserved;
						break;
					}
					if (reserved.error()
						== epoch_catalog_failure::operation_identity_conflict) {
						continue;
					}
					if (reserved.error()
						== epoch_catalog_failure::source_observation_conflict) {
						std::optional<core::epoch_source_observation_id>
							replacement;
						for (std::size_t sourceAttempt = 0U;
							 sourceAttempt < maximum_identity_attempts;
							 ++sourceAttempt) {
							replacement = mint_identity<
								core::epoch_source_observation_id>(*entropy);
							if (!replacement) {
								break;
							}
							const auto exists = diagnostics.perform(
								diagnostic_epoch_stage::catalog_prepare,
								[&] { return context->Catalog->source_observation_exists(*replacement, aStop); });
							if (!exists) {
								return failed(catalog_failure(exists.error()));
							}
							if (!*exists) {
								break;
							}
							replacement.reset();
						}
						if (!replacement) {
							return failed(core::epoch_failure::unavailable);
						}
						auto nextRevision               = context->Revision;
						nextRevision.Source.Observation = *replacement;
						return [context,
								prior = aRequest.Context.Revision,
								nextRevision](implementation& aHost) {
							if (aHost.Active != context
								|| aHost.State.AcceptedRevision
									!= std::optional{ prior }) {
								return;
							}
							context->Revision = nextRevision;
							aHost.dispatch_core(
								core::pull_request_revision_accepted_event{
									.ApplicationSession = context->Handoff.ApplicationSession,
									.Authority          = context->Authority,
									.Revision           = nextRevision,
									.Basis              = context->Handoff.BaseBasis,
								});
							if (aHost.State.AcceptedRevision
								!= std::optional{ nextRevision }) {
								context->Revision = prior;
							}
						};
					}
					return reserved.error()
							== epoch_catalog_failure::cancelled
						? cancelled()
						: failed(catalog_failure(reserved.error()));
				}
				if (!prepared.Record) {
					return failed(core::epoch_failure::catalog_conflict);
				}

				const auto failPrepared =
					[&](const core::epoch_failure aFailure)
					-> worker_completion {
					const auto safe = abort_preparation(*context, prepared, *caches);
					return failed(
						safe ? aFailure : core::epoch_failure::repair_required);
				};
				const auto cancelPrepared = [&]() -> worker_completion {
					const auto safe = abort_preparation(*context, prepared, *caches);
					return safe
						? cancelled()
						: failed(core::epoch_failure::repair_required);
				};

				const auto expectedCache = git_cache_open_request{
					.ApplicationSession = aRequest.Context.ApplicationSession,
					.Authority          = context->Authority,
					.ObjectFormat       = context->Handoff.ObjectFormat,
				};
				const auto cache = diagnostics.perform(
					diagnostic_epoch_stage::cache_open,
					[&] { return open_actor_cache(
							  *context->Actor, *caches, expectedCache, aStop); });
				if (!cache) {
					return cache.error() == git_cache_failure::cancelled
						? cancelPrepared()
						: failPrepared(cache_failure(cache.error()));
				}
				if (!context->Handoff.SmartGit
					|| context->Handoff.SmartGit->revoked()) {
					return failPrepared(core::epoch_failure::unavailable);
				}
				bool                                 receivingWitnessed      = false;
				bool                                 invalidReceivingWitness = false;
				std::optional<epoch_catalog_failure> receivingFailure;
				const auto                           maximumPackBytes = context->Actor->Cache->request().Limits.OperationPackBytes;
				const auto                           persistReceiving = [&](const git_spool_ownership& aOwnership) {
					if (receivingWitnessed || receivingFailure || invalidReceivingWitness || !valid(aOwnership)
						|| aOwnership.Operation != prepared.Operation || aOwnership.MaximumBytes != maximumPackBytes) {
						invalidReceivingWitness = true;
						return false;
					}
					const auto request = record_receiving_request{
						.Catalog                   = expectedCatalog,
						.Operation                 = prepared.Operation,
						.ExpectedOperationRevision = prepared.Record->OperationRevision,
						.Receiving                 = aOwnership,
					};
					const auto record = diagnostics.perform(diagnostic_epoch_stage::catalog_prepare,
															[&] { return context->Catalog->record_receiving(request, aStop); });
					if (!record) {
						receivingFailure = record.error();
						return false;
					}
					auto expected              = *prepared.Record;
					expected.Receiving         = aOwnership;
					expected.OperationRevision = epoch_catalog_operation_revision{ request.ExpectedOperationRevision.value() + 1 };
					if (!valid(*record) || *record != expected) {
						invalidReceivingWitness = true;
						return false;
					}
					prepared.Record    = *record;
					receivingWitnessed = true;
					return true;
				};
				auto sink = diagnostics.perform(
					diagnostic_epoch_stage::pack_sink,
					[&] { return context->Actor->Cache->create_pack_sink(
							  prepared.Operation,
							  maximumPackBytes,
							  aStop, persistReceiving); });
				if (invalidReceivingWitness) {
					return failPrepared(core::epoch_failure::repair_required);
				}
				if (receivingFailure) {
					return *receivingFailure == epoch_catalog_failure::cancelled
						? cancelPrepared()
						: failPrepared(catalog_failure(*receivingFailure));
				}
				if (!sink) {
					return sink.error() == git_cache_failure::cancelled
						? cancelPrepared()
						: failPrepared(cache_failure(sink.error()));
				}
				if (!*sink || !receivingWitnessed) {
					return failPrepared(
						core::epoch_failure::repair_required);
				}
				const auto smartContext = git_smart_request_context{
					.ApplicationSession = aRequest.Context.ApplicationSession,
					.Authority          = context->Authority,
					.Revision           = aRequest.Context.Revision,
					.Repository         = context->Handoff.Locator,
					.BaseRef            = context->Handoff.BaseRef,
					.AcquisitionMode    = object_loading_for(context->Handoff.Shape)
								== pull_request_object_loading::selected_objects
							&& context->Handoff.SmartGit->supports_sparse_commit_trees()
						? git_smart_acquisition_mode::sparse_commit_trees
						: git_smart_acquisition_mode::complete_closure,
				};
				auto transport = diagnostics.perform(
					diagnostic_epoch_stage::smart_acquire,
					[&] { return context->Handoff.SmartGit->acquire(smartContext); });
				if (!transport) {
					(*sink)->abort();
					return aStop.stop_requested()
							|| transport.error().Failure
								== git_smart_transport_failure::cancelled
						? cancelPrepared()
						: failPrepared(smart_transport_failure(transport.error()));
				}
				if (!*transport) {
					(*sink)->abort();
					return failPrepared(
						core::epoch_failure::repair_required);
				}
				std::vector<git_protocol_v2_have> haves;
				if (snapshot->LatestLive) {
					const auto appendHave = [&](
												const core::git_commit_oid aOid,
												const core::anchor_availability
													aAvailability) {
						if (aAvailability != core::anchor_availability::available
							|| std::ranges::any_of(
								haves,
								[&](const auto& aHave) {
									return aHave.Commit == aOid;
								})) {
							return;
						}
						const auto inspected = context->Actor->Cache->inspect_commit_object(
							aOid, aStop);
						if (
							!inspected || !application::valid(*inspected)
							|| inspected->Commit != aOid) {
							return;
						}
						haves.push_back(git_protocol_v2_have{
							.Cache          = context->Authority.Cache,
							.Commit         = aOid,
							.CatalogVisible = true,
							.ObjectVerified = true,
						});
					};
					appendHave(
						snapshot->LatestLive->Base,
						snapshot->LatestLive->BaseAvailability);
					appendHave(
						snapshot->LatestLive->Head,
						snapshot->LatestLive->HeadAvailability);
				}
				const auto acquisition = diagnostics.perform(
					diagnostic_epoch_stage::protocol_acquire,
					[&] { return acquire_git_protocol_v2_pack(
							  **transport,
							  smartContext,
							  haves,
							  **sink,
							  git_protocol_v2_fetch_limits{
								  .MaximumWireBytes     = maximum_git_smart_fetch_response_bytes,
								  .MaximumPackBytes     = context->Actor->Cache->request().Limits.OperationPackBytes,
								  .MaximumProgressBytes = maximum_git_protocol_v2_progress_bytes,
								  .MaximumPackets       = maximum_git_protocol_v2_fetch_packets,
							  },
							  aStop); });
				if (!acquisition) {
					(*transport)->revoke();
					const auto wasCancelled = aStop.stop_requested()
						|| (std::holds_alternative<git_smart_transport_error>(
								acquisition.error())
							&& std::get<git_smart_transport_error>(
								   acquisition.error())
									.Failure
								== git_smart_transport_failure::cancelled);
					const auto* transportFailure = std::get_if<git_smart_transport_error>(&acquisition.error());
					return wasCancelled
						? cancelPrepared()
						: failPrepared(transportFailure != nullptr
										   ? smart_transport_failure(*transportFailure)
										   : core::epoch_failure::unavailable);
				}
				const auto inventory = diagnostics.perform(
					diagnostic_epoch_stage::cache_inventory,
					[&] { return context->Actor->Cache->observe_preimport(
							  acquisition->Fetch.Pack, aStop); });
				if (!inventory) {
					return inventory.error() == git_cache_failure::cancelled
						? cancelPrepared()
						: failPrepared(cache_failure(inventory.error()));
				}
				auto spoolFact          = durable_fact(acquisition->Fetch.Pack);
				spoolFact.Residency     = smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
					? git_revision_residency::sparse_commit_trees
					: git_revision_residency::complete_closure;
				const auto spoolRequest = record_spool_request{
					.Catalog                   = expectedCatalog,
					.Operation                 = prepared.Operation,
					.ExpectedOperationRevision = prepared.Record->OperationRevision,
					.Spool                     = spoolFact,
					.Preimport                 = durable_fact(*inventory),
				};
				auto record = diagnostics.perform(
					diagnostic_epoch_stage::catalog_prepare,
					[&] { return context->Catalog->record_spool(
							  spoolRequest, aStop); });
				if (!record) {
					return record.error() == epoch_catalog_failure::cancelled
						? cancelPrepared()
						: failPrepared(catalog_failure(record.error()));
				}
				if (!exact_spool_record(
						*prepared.Record, *record, spoolRequest)) {
					return failPrepared(
						core::epoch_failure::repair_required);
				}
				prepared.Record = *record;

				const auto durableRecordToken = entropy->generate();
				if (!durableRecordToken
					|| !application::valid(*durableRecordToken)) {
					return failPrepared(core::epoch_failure::unavailable);
				}
				std::optional<core::git_commit_oid> priorHead;
				if (snapshot->LatestLive) {
					priorHead = snapshot->LatestLive->Head;
				}
				const auto gitRequest = git_prepare_revision_request{
					.Receipt       = acquisition->Fetch.Pack,
					.Inventory     = *inventory,
					.Authorization = {
						.CatalogIncarnation      = expectedCatalog.Incarnation,
						.ExpectedCatalogRevision = expectedCatalog.Revision,
						.Operation               = prepared.Operation,
						.ExpectedPack            = acquisition->Fetch.Pack.ExpectedPack,
						.ReceiptToken            = acquisition->Fetch.Pack.OpaqueToken,
						.InventoryToken          = inventory->OpaqueToken,
						.DurableRecordToken      = *durableRecordToken,
					},
					.Base      = aRequest.Context.Revision.Base,
					.Head      = aRequest.Context.Revision.Head,
					.PriorHead = priorHead,
				};
				const auto normalizeLineage = [&](git_prepared_revision aPrepared) {
					if (snapshot->LatestLive
						&& snapshot->LatestLive->Head != aRequest.Context.Revision.Head
						&& snapshot->LatestLive->HeadAvailability != core::anchor_availability::available) {
						aPrepared.HeadLineage = core::head_lineage::unknown;
					}
					return aPrepared;
				};
				bool                               witnessed        = false;
				bool                               witnessCancelled = false;
				std::optional<core::epoch_failure> witnessFailure;
				const auto                         persistInstallation = [&](const git_prepared_revision& aInstallation) {
					if (witnessed || witnessFailure || witnessCancelled
						|| !exact_prepared_revision(
							aInstallation, gitRequest,
							smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
								? git_revision_residency::sparse_commit_trees
								: git_revision_residency::complete_closure)) {
						witnessFailure = core::epoch_failure::repair_required;
						return false;
					}
					const auto installRequest = record_install_request{
						.Catalog                   = expectedCatalog,
						.Operation                 = prepared.Operation,
						.ExpectedOperationRevision = prepared.Record->OperationRevision,
						.Install                   = durable_fact(normalizeLineage(aInstallation)),
					};
					const auto installed = diagnostics.perform(diagnostic_epoch_stage::catalog_prepare,
															   [&] { return context->Catalog->record_install(installRequest, aStop); });
					if (!installed) {
						witnessCancelled = installed.error() == epoch_catalog_failure::cancelled;
						witnessFailure   = catalog_failure(installed.error());
						return false;
					}
					if (!exact_install_record(*prepared.Record, *installed, installRequest)) {
						witnessFailure = core::epoch_failure::repair_required;
						return false;
					}
					prepared.Record = *installed;
					witnessed       = true;
					return true;
				};
				auto git = diagnostics.perform(
					diagnostic_epoch_stage::revision_prepare,
					[&] {
						return smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
							? context->Actor->Cache->prepare_sparse_revision(gitRequest, aStop, persistInstallation)
							: context->Actor->Cache->prepare_revision(gitRequest, aStop, persistInstallation);
					});
				if (witnessFailure) {
					return witnessCancelled ? cancelPrepared() : failPrepared(*witnessFailure);
				}
				if (!git) {
					return git.error() == git_cache_failure::cancelled
						? cancelPrepared()
						: failPrepared(cache_failure(git.error()));
				}
				if (!witnessed || !exact_prepared_revision(*git, gitRequest, smartContext.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees ? git_revision_residency::sparse_commit_trees : git_revision_residency::complete_closure)) {
					return failPrepared(core::epoch_failure::repair_required);
				}
				*git = normalizeLineage(*git);
				if (prepared.Record->Install != durable_fact(*git)) {
					return failPrepared(core::epoch_failure::repair_required);
				}
				prepared.Git       = *git;
				const auto staging = diagnostics.perform(
					diagnostic_epoch_stage::catalog_prepare,
					[&] { return advance_append(
							  *context,
							  *prepared.Record,
							  expectedCatalog,
							  epoch_catalog_operation_phase::staging_ready,
							  aStop); });
				if (!staging) {
					return staging.error()
							== epoch_catalog_failure::cancelled
						? cancelPrepared()
						: failPrepared(catalog_failure(staging.error()));
				}
				prepared.Record = *staging;

				const auto review   = canonicalReview(projected_append_head{
					.Ordinal = prepared.Record->ReservedOrdinal,
					.Head    = aRequest.Context.Revision.Head,
				});
				const auto observed = observedTime();
				if (!review || !observed) {
					return failPrepared(
						review ? core::epoch_failure::unavailable
							   : review.error());
				}
				if (!context->store(prepared)) {
					const auto safe = abort_preparation(*context, prepared, *caches);
					return failed(
						safe ? core::epoch_failure::unavailable
							 : core::epoch_failure::repair_required);
				}
				const auto event = core::epoch_revision_prepared_event{
					.Context     = aRequest.Context,
					.Operation   = prepared.Operation,
					.Observation = {
						.LatestLive                  = snapshot->LatestLive,
						.CatalogCurrentViewerReview  = snapshot->CurrentViewerReview,
						.ProposedCurrentViewerReview = *review,
						.ProposedOrdinal             = prepared.Record->ReservedOrdinal,
						.ObservedAt                  = *observed,
						.HeadLineage                 = git->HeadLineage,
						.Material                    = core::epoch_material_identity{
							.BaseTree       = git->BaseTree,
							.HeadTree       = git->HeadTree,
							.PolicyRevision = core::comparison_policy_revision{ 1U },
						},
						.RetentionRehydration = snapshot->RetentionRehydration,
					},
				};
				return [context, event](implementation& aHost) {
					const auto output   = aHost.dispatch_core(event);
					const auto accepted = std::ranges::any_of(
						output.Requests,
						[](const auto& aNext) {
							return std::holds_alternative<
									   core::commit_epoch_catalog_request>(aNext)
								|| std::holds_alternative<
									   core::discard_epoch_preparation_request>(
									   aNext);
						});
					if (!accepted) {
						static_cast<void>(
							aHost.discard_context_preparation(
								context, event.Operation));
					}
				};
			},
			[aRequest](implementation& aHost) {
				aHost.dispatch_core(
					core::epoch_revision_prepare_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
			},
			[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
				aHost.dispatch_core(
					core::epoch_revision_prepare_failed_event{
						.Context = aRequest.Context,
						.Failure = aFailure,
					});
			},
			aRequest.Context.Request,
			aRequest.Context.Generation,
			std::nullopt,
			std::nullopt,
			std::move(actor));
	}

	void epoch_application_host::implementation::resume_pending_occurrence_import(const std::shared_ptr<authority_context>& aContext)
	{
		if (aContext->HistoricalImportPending || Active != aContext
			|| State.Authority != aContext->Authority
			|| State.AcceptedRevision != aContext->Revision
			|| !State.Catalog || !State.PendingOccurrence) {
			return;
		}
		const auto pending = *State.PendingOccurrence;
		start_historical_import(aContext, pending.Request, State.EpochGeneration,
								core::epoch_occurrence_import_context{
									State.ApplicationSession, pending.Request, *State.Authority,
									*State.AcceptedRevision, State.EpochGeneration,
									State.ComparisonGeneration, *State.Catalog,
									pending.Occurrence, pending.Side });
	}

	void epoch_application_host::implementation::start_historical_import(const std::shared_ptr<authority_context>&                  aContext,
																		 const core::epoch_request_id                               aRequest,
																		 const core::epoch_generation                               aGeneration,
																		 const std::optional<core::epoch_occurrence_import_context> aSelection)
	{
		// A historical import is supplementary. One actor serializes it with live
		// retention, and each successful completion may admit another event.
		// Large CLI reviews keep older comparison objects out of the initial
		// load. Unproven historical anchors remain unavailable; a separate
		// selected-epoch acquisition must prove them before comparison.
		const bool sparse = object_loading_for(aContext->Handoff.Shape)
				== pull_request_object_loading::selected_objects
			&& aContext->Handoff.SmartGit
			&& aContext->Handoff.SmartGit->supports_sparse_commit_trees();
		if (sparse && !aSelection) {
			return;
		}
		const auto failSelection = [aSelection](implementation& aHost) {
			if (aSelection) {
				aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
					.Context = *aSelection,
					.Failure = core::epoch_failure::unavailable,
				});
			}
		};
		if (aSelection && !sparse) {
			failSelection(*this);
			return;
		}
		if (aContext->HistoricalImportPending) {
			// The latest typed selection remains pending in core and is resumed
			// after this actor's current historical effect completes.
			return;
		}
		if (aContext->HistoricalImports + aContext->HistoricalSkipped.size() >= 8U
			|| !aContext->OccurrenceHistory || !aContext->Handoff.SmartGit
			|| aContext->Handoff.SmartGit->revoked()) {
			failSelection(*this);
			return;
		}
		aContext->HistoricalImportPending = true;
		auto* const entropy               = &Entropy;
		auto* const caches                = &Caches;
		auto* const clock                 = &Clock;
		const auto  submitted             = submit_effect(
			[context = aContext, aRequest, aGeneration, aSelection, sparse, entropy, caches, clock](
				const std::stop_token aStop) -> worker_completion {
				const auto noImport = [context, aSelection](implementation& aHost) {
					context->HistoricalImportPending = false;
					if (aSelection) {
						aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
							.Context = *aSelection,
							.Failure = core::epoch_failure::unavailable,
						});
					}
					aHost.resume_pending_occurrence_import(context);
				};
				if (aStop.stop_requested()) {
					return noImport;
				}
				const auto snapshot = context->Catalog->snapshot(aStop);
				const auto history  = context->Catalog->occurrences(aStop);
				if (!snapshot || !history || !snapshot->LatestLive
					|| snapshot->Binding != context->Catalog->binding()
					|| (aSelection && snapshot->Binding.Catalog != aSelection->ExpectedCatalog)) {
					return noImport;
				}
				const auto event = std::ranges::find_if(history->Occurrences,
														[&](const core::epoch_occurrence& aOccurrence) {
															return aOccurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event
																&& !aOccurrence.LocalEpoch
																&& (aSelection ? aOccurrence.Id == aSelection->Occurrence
																			   : std::ranges::find(context->HistoricalSkipped, aOccurrence.Id) == context->HistoricalSkipped.end());
														});
				if (event == history->Occurrences.end() || !snapshot->NextOrdinal
					|| !context->Handoff.SmartGit || context->Handoff.SmartGit->revoked()) {
					return noImport;
				}
				const auto skip = [context, aRequest, aGeneration, aSelection, id = event->Id](implementation& aHost) {
					context->HistoricalImportPending = false;
					if (aSelection) {
						aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
							.Context = *aSelection,
							.Failure = core::epoch_failure::unavailable,
						});
						aHost.resume_pending_occurrence_import(context);
						return;
					}
					context->HistoricalSkipped.push_back(id);
					if (aHost.Active == context && aHost.State.Authority == context->Authority) {
						aHost.start_historical_import(context, aRequest, aGeneration);
					}
					aHost.resume_pending_occurrence_import(context);
				};
				const auto expectedCache = git_cache_open_request{
					.ApplicationSession = context->Handoff.ApplicationSession,
					.Authority          = context->Authority,
					.ObjectFormat       = context->Handoff.ObjectFormat,
				};
				if (!open_actor_cache(*context->Actor, *caches, expectedCache, aStop)) {
					return noImport;
				}
				std::optional<core::epoch_source_observation_id> source;
				for (std::size_t attempt = 0U; attempt < maximum_identity_attempts; ++attempt) {
					source = mint_identity<core::epoch_source_observation_id>(*entropy);
					if (!source) {
						return noImport;
					}
					const auto exists = context->Catalog->source_observation_exists(*source, aStop);
					if (!exists) {
						return noImport;
					}
					if (!*exists) {
						break;
					}
					source.reset();
				}
				const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
				if (!source || !operation) {
					return noImport;
				}
				auto historical               = context->Revision;
				historical.Source.Observation = *source;
				historical.Origin             = core::pull_request_revision_origin::explicit_historical_import;
				historical.Head               = event->Evidence.Head;
				historical.CurrentViewerReview.reset();
				const auto reservation = append_reservation_request{
					.Context = {
						.ApplicationSession = context->Handoff.ApplicationSession,
						.Request            = aRequest,
						.Generation         = aGeneration,
						.Operation          = *operation,
						.Revision           = historical,
						.Catalog            = snapshot->Binding.Catalog,
					},
					.ProposedOrdinal = *snapshot->NextOrdinal,
				};
				auto record = context->Catalog->reserve_append(reservation, aStop);
				if (!record || !exact_append_reservation(*record, reservation, context->Authority.Scope)) {
					return noImport;
				}
				auto prepared = prepared_operation{
					.Kind      = prepared_kind::append,
					.Operation = *operation,
					.Record    = *record,
				};
				const auto fail = [&]() -> worker_completion {
					const auto aborted = abort_preparation(*context, prepared, *caches);
					if (aborted && !aStop.stop_requested()) {
						return skip;
					}
					return noImport;
				};
				const auto maximumPackBytes   = context->Actor->Cache->request().Limits.OperationPackBytes;
				bool       receivingWitnessed = false;
				auto       sink               = context->Actor->Cache->create_pack_sink(
					*operation, maximumPackBytes, aStop,
					[&](const git_spool_ownership& aOwnership) {
						if (receivingWitnessed || !valid(aOwnership) || aOwnership.Operation != *operation
							|| aOwnership.MaximumBytes != maximumPackBytes) {
							return false;
						}
						const auto next = context->Catalog->record_receiving({
																				 .Catalog                   = snapshot->Binding.Catalog,
																				 .Operation                 = *operation,
																				 .ExpectedOperationRevision = prepared.Record->OperationRevision,
																				 .Receiving                 = aOwnership,
																			 },
																			 aStop);
						if (!next) {
							return false;
						}
						prepared.Record    = *next;
						receivingWitnessed = true;
						return true;
					});
				if (!sink || !*sink || !receivingWitnessed) {
					return fail();
				}
				const auto smartContext = git_smart_request_context{
					.ApplicationSession = context->Handoff.ApplicationSession,
					.Authority          = context->Authority,
					.Revision           = context->Revision,
					.Repository         = context->Handoff.Locator,
					.BaseRef            = context->Handoff.BaseRef,
					.AcquisitionMode    = sparse ? git_smart_acquisition_mode::sparse_commit_trees
												 : git_smart_acquisition_mode::complete_closure,
				};
				auto transport = context->Handoff.SmartGit->acquire(smartContext);
				if (!transport || !*transport) {
					(*sink)->abort();
					return fail();
				}
				const auto acquisition = acquire_git_protocol_v2_historical_pack(
					**transport, smartContext, event->Evidence.Head, { }, **sink,
					git_protocol_v2_fetch_limits{
						.MaximumWireBytes     = maximum_git_smart_fetch_response_bytes,
						.MaximumPackBytes     = maximumPackBytes,
						.MaximumProgressBytes = maximum_git_protocol_v2_progress_bytes,
						.MaximumPackets       = maximum_git_protocol_v2_fetch_packets,
					},
					aStop);
				if (!acquisition) {
					(*transport)->revoke();
					return fail();
				}
				const auto inventory = context->Actor->Cache->observe_preimport(acquisition->Fetch.Pack, aStop);
				if (!inventory) {
					return fail();
				}
				auto spoolFact      = durable_fact(acquisition->Fetch.Pack);
				spoolFact.Residency = sparse ? git_revision_residency::sparse_commit_trees
											 : git_revision_residency::complete_closure;
				const auto spooled  = context->Catalog->record_spool({
																		 .Catalog                   = snapshot->Binding.Catalog,
																		 .Operation                 = *operation,
																		 .ExpectedOperationRevision = prepared.Record->OperationRevision,
																		 .Spool                     = spoolFact,
																		 .Preimport                 = durable_fact(*inventory),
																	 },
																	 aStop);
				if (!spooled) {
					return fail();
				}
				prepared.Record  = *spooled;
				const auto token = entropy->generate();
				if (!token || !application::valid(*token)) {
					return fail();
				}
				const auto authorization = git_import_authorization{
					.CatalogIncarnation      = snapshot->Binding.Catalog.Incarnation,
					.ExpectedCatalogRevision = snapshot->Binding.Catalog.Revision,
					.Operation               = *operation,
					.ExpectedPack            = acquisition->Fetch.Pack.ExpectedPack,
					.ReceiptToken            = acquisition->Fetch.Pack.OpaqueToken,
					.InventoryToken          = inventory->OpaqueToken,
					.DurableRecordToken      = *token,
				};
				const auto sparseRequest = git_prepare_revision_request{
					.Receipt       = acquisition->Fetch.Pack,
					.Inventory     = *inventory,
					.Authorization = authorization,
					.Base          = context->Revision.Base,
					.Head          = event->Evidence.Head,
				};
				bool       installationWitnessed = false;
				const auto persistInstallation   = [&](const git_prepared_revision& aInstallation) {
					if (installationWitnessed || !application::valid(aInstallation)
						|| aInstallation.Head != event->Evidence.Head
						|| (sparse && !exact_prepared_revision(aInstallation, sparseRequest, git_revision_residency::sparse_commit_trees))
						|| aInstallation.Residency != (sparse ? git_revision_residency::sparse_commit_trees : git_revision_residency::complete_closure)) {
						return false;
					}
					const auto next = context->Catalog->record_install({
																		   .Catalog                   = snapshot->Binding.Catalog,
																		   .Operation                 = *operation,
																		   .ExpectedOperationRevision = prepared.Record->OperationRevision,
																		   .Install                   = durable_fact(aInstallation),
																		   .DerivedComparisonBase     = aInstallation.Base,
																	   },
																	   aStop);
					if (!next) {
						return false;
					}
					prepared.Record       = *next;
					installationWitnessed = true;
					return true;
				};
				auto installed = sparse
					? context->Actor->Cache->prepare_sparse_revision(sparseRequest, aStop, persistInstallation)
					: context->Actor->Cache->prepare_historical_revision({
																			 .Receipt       = acquisition->Fetch.Pack,
																			 .Inventory     = *inventory,
																			 .Authorization = authorization,
																			 .SelectedBase  = context->Revision.Base,
																			 .Head          = event->Evidence.Head,
																		 },
																		 aStop, persistInstallation);
				if (!installed || !installationWitnessed
					|| (sparse && !exact_prepared_revision(*installed, sparseRequest, git_revision_residency::sparse_commit_trees))
					|| prepared.Record->Context.Revision.Base != installed->Base) {
					return fail();
				}
				prepared.Git       = *installed;
				const auto staging = advance_append(*context, *prepared.Record, snapshot->Binding.Catalog,
													epoch_catalog_operation_phase::staging_ready, aStop);
				if (!staging) {
					return fail();
				}
				prepared.Record     = *staging;
				const auto observed = clock->observe_utc();
				if (!observed || !core::valid(*observed)) {
					return fail();
				}
				const auto plan = core::plan_historical_epoch_mutation(
					context->Authority, context->Revision, historical.Source,
					snapshot->Binding.Catalog, *event, installed->Base,
					core::epoch_preparation_observation{
						.LatestLive      = snapshot->LatestLive,
						.ProposedOrdinal = prepared.Record->ReservedOrdinal,
						.ObservedAt      = *observed,
						.HeadLineage     = core::head_lineage::not_applicable,
						.Material        = core::epoch_material_identity{
							.BaseTree       = installed->BaseTree,
							.HeadTree       = installed->HeadTree,
							.PolicyRevision = core::comparison_policy_revision{ 1U },
						},
						.RetentionRehydration = snapshot->RetentionRehydration,
					});
				const auto* append = plan.Mutation
					? std::get_if<core::append_epoch_mutation>(&*plan.Mutation)
					: nullptr;
				if (!append) {
					return fail();
				}
				const auto candidate = context->Catalog->record_candidate({
																			  .Catalog                   = snapshot->Binding.Catalog,
																			  .Operation                 = *operation,
																			  .ExpectedOperationRevision = prepared.Record->OperationRevision,
																			  .Candidate                 = *append,
																			  .HistoricalOccurrence      = event->Id,
																		  },
																		  aStop);
				if (!candidate || candidate->HistoricalOccurrence != event->Id
					|| candidate->Candidate != std::optional{ *append }) {
					return fail();
				}
				prepared.Record      = *candidate;
				const auto committed = recover_append(*context->Catalog, *context->Actor->Cache,
													  *entropy, *prepared.Record, aStop);
				if (!committed) {
					return noImport;
				}
				// The append may have crossed its durable point of no return even
				// when the caller has since cancelled. Read its committed identity
				// before delivering any selection or allowing a newer selection
				// to reuse this actor.
				const auto postCommitStop = std::stop_token{ };
				const auto current        = context->Catalog->snapshot(postCommitStop);
				const auto updated        = context->Catalog->occurrences(postCommitStop);
				if (!current || !updated || !current->LatestLive
					|| current->LatestLive != snapshot->LatestLive) {
					return noImport;
				}
				if (aSelection) {
					const auto linked = std::ranges::find(updated->Occurrences,
														  aSelection->Occurrence, &core::epoch_occurrence::Id);
					if (linked == updated->Occurrences.end()
						|| linked->LocalEpoch != append->Epoch.Id
						|| linked->Evidence.Head != append->Epoch.Head
						|| linked->DerivedComparisonBase != append->Epoch.Base
						|| current->Binding.Catalog == snapshot->Binding.Catalog) {
						return noImport;
					}
				}
				return [context, aRequest, aGeneration, expectedCatalog = snapshot->Binding.Catalog, catalog = current->Binding.Catalog,
						currentEpoch = current->LatestLive, completedHistory = *updated,
						aSelection, imported                                 = append->Epoch](implementation& aHost) {
					context->HistoricalImportPending = false;
					++context->HistoricalImports;
					if (aHost.Active == context && aHost.State.Authority == context->Authority) {
						const auto importedHead = core::comparison_anchor{
							.Identity = core::comparison_anchor_identity{
								.Authority   = imported.Id.Authority,
								.Incarnation = imported.Id.Incarnation,
								.Kind        = core::comparison_anchor_kind::epoch_head,
								.Epoch       = imported.Id.Ordinal,
							},
							.Commit       = imported.Head,
							.Availability = imported.HeadAvailability,
						};
						if (aSelection) {
							aHost.dispatch_core(core::epoch_occurrence_imported_event{
								.Context    = *aSelection,
								.NewCatalog = catalog,
								.Head       = importedHead,
							});
							// A repeated gesture can supersede the request after its
							// append is durable. The same verified link satisfies that
							// latest exact occurrence without fetching it again.
							if (aHost.State.Catalog == expectedCatalog
								&& aHost.State.PendingOccurrence
								&& aHost.State.PendingOccurrence->Occurrence == aSelection->Occurrence) {
								const auto pending = *aHost.State.PendingOccurrence;
								aHost.dispatch_core(core::epoch_occurrence_imported_event{
									.Context = core::epoch_occurrence_import_context{
										aHost.State.ApplicationSession, pending.Request,
										*aHost.State.Authority, *aHost.State.AcceptedRevision,
										aHost.State.EpochGeneration, aHost.State.ComparisonGeneration,
										expectedCatalog, pending.Occurrence, pending.Side },
									.NewCatalog = catalog,
									.Head       = importedHead,
								});
							}
						}
						if (aHost.State.Catalog == expectedCatalog) {
							aHost.dispatch_core(core::epoch_historical_catalog_advanced_event{
								.ApplicationSession = context->Handoff.ApplicationSession,
								.Authority          = context->Authority,
								.Revision           = context->Revision,
								.ExpectedCatalog    = expectedCatalog,
								.NewCatalog         = catalog,
							});
						}
						if (aHost.State.Catalog == catalog) {
							context->OccurrenceHistory = completedHistory;
							aHost.publish_occurrences(catalog, completedHistory, currentEpoch);
						}
						if (!aSelection) {
							aHost.start_historical_import(context, aRequest, aGeneration);
						}
						aHost.resume_pending_occurrence_import(context);
					}
				};
			},
			[context = aContext, aSelection](implementation& aHost) {
				context->HistoricalImportPending = false;
				if (aSelection) {
					aHost.dispatch_core(core::epoch_occurrence_import_cancelled_event{
						.Context = *aSelection,
					});
				}
				aHost.resume_pending_occurrence_import(context);
			},
			[context = aContext, aSelection](implementation& aHost, core::epoch_failure aFailure) {
				context->HistoricalImportPending = false;
				if (aSelection) {
					aHost.dispatch_core(core::epoch_occurrence_import_failed_event{
						.Context = *aSelection,
						.Failure = aFailure,
					});
				}
				aHost.resume_pending_occurrence_import(context);
			},
			aRequest, aGeneration, std::nullopt, std::nullopt, aContext->Actor);
		if (!submitted) {
			aContext->HistoricalImportPending = false;
			failSelection(*this);
		}
	}
}
