#include "epoch_host_implementation.hpp"

#include "bpr/core/epoch_transition.hpp"

#include <limits>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto review_anchor(
			const core::pull_request_authority&     aAuthority,
			const core::epoch_catalog_identity      aCatalog,
			const core::catalog_review_association& aReview)
			-> core::comparison_anchor
		{
			return {
				.Identity = {
					.Authority   = aAuthority.Scope,
					.Incarnation = aCatalog.Incarnation,
					.Kind        = core::comparison_anchor_kind::review_commit,
					.Review      = aReview.Association,
				},
				.Commit       = aReview.Commit,
				.Availability = aReview.Availability,
			};
		}

		[[nodiscard]] auto exact_prune_intent(
			const maintenance_operation_record&   aRecord,
			const begin_prune_request&            aRequest,
			const core::repository_cache_scope_id aCache) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Incarnation == aRequest.Catalog.Incarnation
				&& aRecord.Operation == aRequest.Operation
				&& aRecord.Kind == epoch_catalog_operation_kind::prune
				&& aRecord.Phase == epoch_catalog_operation_phase::prune_intent
				&& aRecord.OperationRevision == epoch_catalog_operation_revision{ 1 }
			&& aRecord.Cache == aCache && aRecord.Epochs == aRequest.Epochs
				&& aRecord.Scopes.empty() && !aRecord.RepairFromPhase;
		}

		[[nodiscard]] auto exact_cache_delete_intent(
			const maintenance_operation_record& aRecord,
			const begin_cache_delete_request&   aRequest) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Incarnation == aRequest.Impact.Incarnation
				&& aRecord.Operation == aRequest.Operation
				&& aRecord.Kind == epoch_catalog_operation_kind::cache_delete
				&& aRecord.Phase
				== epoch_catalog_operation_phase::cache_delete_intent_sealed
				&& aRecord.OperationRevision == epoch_catalog_operation_revision{ 1 }
			&& aRecord.Cache == aRequest.Impact.Cache && aRecord.Epochs.empty()
				&& aRecord.Scopes == aRequest.Impact.Scopes && !aRecord.RepairFromPhase;
		}

		[[nodiscard]] auto exact_candidate_record(
			const append_operation_record&  aPrior,
			const append_operation_record&  aNext,
			const record_candidate_request& aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase
				== epoch_catalog_operation_phase::commit_authorized
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Receiving == aPrior.Receiving
				&& aNext.Spool == aPrior.Spool && aNext.Preimport == aPrior.Preimport
				&& aNext.Install == aPrior.Install
				&& aNext.Candidate == std::optional{ aRequest.Candidate };
		}
	}

	void epoch_application_host::implementation::start_catalog_commit(const core::commit_epoch_catalog_request& aRequest)
	{
		auto context = exact_context(
			aRequest.Context.Authority, aRequest.Context.Revision);
		if (!context) {
			return;
		}
		auto* const entropy = &Entropy;
		auto* const caches  = &Caches;
		auto        actor   = context->Actor;
		submit_effect(
			[context, aRequest, entropy, caches](const std::stop_token aStop)
				-> worker_completion {
				const auto cancelled = [aRequest](const core::epoch_commit_progress
													  aProgress)
					-> worker_completion {
					return [aRequest, aProgress](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_catalog_commit_cancelled_event{
								.Context  = aRequest.Context,
								.Mutation = aRequest.Mutation,
								.Progress = aProgress,
							});
					};
				};
				const auto failed = [aRequest](const core::epoch_failure aFailure)
					-> worker_completion {
					return [aRequest, aFailure](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_catalog_commit_failed_event{
								.Context  = aRequest.Context,
								.Mutation = aRequest.Mutation,
								.Failure  = aFailure,
							});
					};
				};
				const auto succeeded = [aRequest, context, aStop](
										   const core::epoch_catalog_revision aRevision) -> worker_completion {
					auto history = std::optional<core::epoch_occurrence_history>{ };
					auto current = std::optional<core::review_epoch_summary>{ };
					try {
						if (auto stored = context->Catalog->occurrences(aStop)) {
							history = *stored;
							if (const auto snapshot = context->Catalog->snapshot(aStop);
								snapshot && snapshot->Binding == context->Catalog->binding()) {
								current = snapshot->LatestLive;
							}
							auto reconciled = std::move(*stored);
							reconciled      = reconcile_local_occurrences(context->Handoff, std::move(reconciled), *context->Catalog, current, aStop);
							enrich_commit_dates(reconciled, context->Actor->Cache.get(), aStop);
							std::ranges::sort(reconciled.Occurrences, core::epoch_occurrence_less);
							if (context->Catalog->record_occurrences(reconciled, aStop)) {
								history = std::move(reconciled);
							}
						}
					} catch (...) {
						// The durable live commit succeeded. Supplementary evidence
						// must never replace its terminal with a failure or suppress
						// publication of the loaded, durably unlinked history.
					}
					const auto catalog = context->Catalog->binding().Catalog;
					return [aRequest, aRevision, context, catalog,
							completedHistory = std::move(history), completedCurrent = std::move(current)](
							   implementation& aHost) {
						aHost.dispatch_core(core::epoch_catalog_committed_event{
							.Context     = aRequest.Context,
							.Mutation    = aRequest.Mutation,
							.NewRevision = aRevision,
						});
						if (aHost.Active == context
							&& aHost.State.Authority == context->Authority
							&& aHost.State.Catalog == catalog && (completedHistory || context->OccurrenceHistory)) {
							context->OccurrenceHistory = completedHistory.value_or(core::epoch_occurrence_history{ });
							aHost.publish_occurrences(catalog, *context->OccurrenceHistory, completedCurrent);
							aHost.start_historical_import(context, aRequest.Context.Request,
														  aRequest.Context.Generation);
						}
					};
				};

				auto claimed = context->take(aRequest.Context.Operation);
				if (!claimed) {
					return failed(core::epoch_failure::catalog_conflict);
				}
				auto       operation       = std::move(*claimed);
				const auto expectedCatalog = core::epoch_catalog_identity{
					.Incarnation = aRequest.Context.CatalogIncarnation,
					.Revision    = aRequest.Context.ExpectedCatalogRevision,
				};
				const auto abortAndFail =
					[&](const core::epoch_failure aFailure)
					-> worker_completion {
					const auto safe = abort_preparation(*context, operation, *caches);
					return failed(
						safe ? aFailure : core::epoch_failure::repair_required);
				};
				const auto abortAndCancel = [&]() -> worker_completion {
					const auto safe = abort_preparation(*context, operation, *caches);
					return safe
						? cancelled(
							  core::epoch_commit_progress::before_ref_commit)
						: failed(core::epoch_failure::repair_required);
				};

				if (const auto* association = std::get_if<core::associate_review_mutation>(
						&aRequest.Mutation)) {
					if (operation.Kind != prepared_kind::observation_only) {
						return abortAndFail(
							core::epoch_failure::malformed_observation);
					}
					if (aStop.stop_requested()) {
						return cancelled(
							core::epoch_commit_progress::before_ref_commit);
					}
					const auto visibleAssociation = [&]()
						-> std::optional<core::epoch_catalog_revision> {
						try {
							const auto snapshot = context->Catalog->snapshot({ });
							if (
								!snapshot
								|| !application::valid(snapshot->Binding)
								|| snapshot->Binding
									!= context->Catalog->binding()
								|| snapshot->Binding.Catalog.Incarnation
									!= expectedCatalog.Incarnation
								|| expectedCatalog.Revision.value()
									>= core::maximum_epoch_catalog_revision
								|| snapshot->Binding.Catalog.Revision.value()
									!= expectedCatalog.Revision.value() + 1
								|| snapshot->CurrentViewerReview
									!= std::optional{ association->Review }) {
								return std::nullopt;
							}
							return snapshot->Binding.Catalog.Revision;
						} catch (...) {
							return std::nullopt;
						}
					};
					epoch_catalog_result<catalog_commit_result> committed = std::unexpected{ epoch_catalog_failure::unavailable };
					try {
						committed = context->Catalog->associate_review(
							associate_review_request{
								.Catalog          = expectedCatalog,
								.Review           = association->Review,
								.AcceptedSnapshot = aRequest.Context.Revision.Source.Snapshot,
							},
							aStop);
					} catch (...) {
						const auto visible = visibleAssociation();
						return visible ? succeeded(*visible)
									   : failed(
											 core::epoch_failure::
												 repair_required);
					}
					if (!committed) {
						if (const auto visible = visibleAssociation()) {
							return succeeded(*visible);
						}
						return committed.error()
								== epoch_catalog_failure::cancelled
							? cancelled(core::epoch_commit_progress::
											before_ref_commit)
							: failed(catalog_failure(committed.error()));
					}
					if (
						!exact_catalog_commit(
							*committed, expectedCatalog, std::nullopt)
						|| context->Catalog->binding().Catalog
							!= committed->Catalog) {
						const auto visible = visibleAssociation();
						return visible ? succeeded(*visible)
									   : failed(
											 core::epoch_failure::
												 catalog_conflict);
					}
					return succeeded(committed->Catalog.Revision);
				}

				const auto* append = std::get_if<core::append_epoch_mutation>(&aRequest.Mutation);
				if (append == nullptr || operation.Kind != prepared_kind::append
					|| !operation.Record || !operation.Git
					|| operation.Record->Context.Catalog != expectedCatalog
					|| operation.Record->ReservedOrdinal
						!= append->Epoch.Id.Ordinal
					|| operation.Git->Base != append->Epoch.Base
					|| operation.Git->Head != append->Epoch.Head) {
					return abortAndFail(
						core::epoch_failure::malformed_observation);
				}
				const auto expectedCache = git_cache_open_request{
					.ApplicationSession = aRequest.Context.ApplicationSession,
					.Authority          = context->Authority,
					.ObjectFormat       = context->Handoff.ObjectFormat,
				};
				const auto cache = open_actor_cache(
					*context->Actor, *caches, expectedCache, aStop);
				if (!cache) {
					return cache.error() == git_cache_failure::cancelled
						? abortAndCancel()
						: abortAndFail(cache_failure(cache.error()));
				}
				if (aStop.stop_requested()) {
					return abortAndCancel();
				}
				const auto durableCommitToken = entropy->generate();
				if (!durableCommitToken
					|| !application::valid(*durableCommitToken)) {
					return abortAndFail(core::epoch_failure::unavailable);
				}

				const auto visibleAppend = [&]()
					-> std::optional<core::epoch_catalog_revision> {
					try {
						const auto snapshot = context->Catalog->snapshot({ });
						if (
							!snapshot
							|| !application::valid(snapshot->Binding)
							|| snapshot->Binding
								!= context->Catalog->binding()
							|| snapshot->Binding.Catalog.Incarnation
								!= expectedCatalog.Incarnation
							|| expectedCatalog.Revision.value()
								>= core::maximum_epoch_catalog_revision
							|| snapshot->Binding.Catalog.Revision.value()
								!= expectedCatalog.Revision.value() + 1
							|| snapshot->LatestLive
								!= std::optional{ append->Epoch }) {
							return std::nullopt;
						}
						return snapshot->Binding.Catalog.Revision;
					} catch (...) {
						return std::nullopt;
					}
				};
				const auto resolveAmbiguity = [&, context](
												  const core::epoch_failure aFallback,
												  const bool                aCancelledBeforePonr) -> worker_completion {
					if (const auto visible = visibleAppend()) {
						return succeeded(*visible);
					}
					try {
						const auto pending = pending_append(
							*context->Catalog, operation.Operation);
						if (!pending) {
							return failed(
								core::epoch_failure::repair_required);
						}
						if (!*pending) {
							if (const auto visible = visibleAppend()) {
								return succeeded(*visible);
							}
							return aCancelledBeforePonr
								? cancelled(core::epoch_commit_progress::
												before_ref_commit)
								: failed(aFallback);
						}
						const auto& durable = **pending;
						if (
							!same_append_identity(
								*operation.Record, durable)
							|| durable.Spool != operation.Record->Spool
							|| durable.Preimport
								!= operation.Record->Preimport
							|| durable.Install != operation.Record->Install
							|| (durable.Candidate
								&& durable.Candidate
									!= std::optional{ *append })) {
							return failed(
								core::epoch_failure::repair_required);
						}
						operation.Record = durable;
						if (!point_of_no_cancellation(durable.Phase)) {
							const auto safe = abort_preparation(*context, operation, *caches);
							if (!safe) {
								return failed(
									core::epoch_failure::repair_required);
							}
							return aCancelledBeforePonr
								? cancelled(core::epoch_commit_progress::
												before_ref_commit)
								: failed(aFallback);
						}

						const auto recovered = recover_append(
							*context->Catalog,
							**cache,
							*entropy,
							durable,
							{ });
						if (const auto visible = visibleAppend()) {
							return succeeded(*visible);
						}
						return recovered
							? failed(core::epoch_failure::repair_required)
							: failed(recovered.error());
					} catch (...) {
						if (const auto visible = visibleAppend()) {
							return succeeded(*visible);
						}
						return failed(
							core::epoch_failure::repair_required);
					}
				};

				const auto candidateRequest = record_candidate_request{
					.Catalog                   = expectedCatalog,
					.Operation                 = operation.Operation,
					.ExpectedOperationRevision = operation.Record->OperationRevision,
					.Candidate                 = *append,
				};
				epoch_catalog_result<append_operation_record> record = std::unexpected{ epoch_catalog_failure::unavailable };
				try {
					record = context->Catalog->record_candidate(
						candidateRequest, aStop);
				} catch (...) {
					return resolveAmbiguity(
						core::epoch_failure::repair_required, false);
				}
				if (!record) {
					const auto wasCancelled = record.error() == epoch_catalog_failure::cancelled;
					return resolveAmbiguity(
						wasCancelled
							? core::epoch_failure::unavailable
							: catalog_failure(record.error()),
						wasCancelled);
				}
				if (!exact_candidate_record(
						*operation.Record, *record, candidateRequest)) {
					return resolveAmbiguity(
						core::epoch_failure::repair_required, false);
				}
				operation.Record = *record;
				if (aStop.stop_requested()) {
					return abortAndCancel();
				}
				epoch_catalog_result<append_operation_record>
					pointOfNoReturn = std::unexpected{
						epoch_catalog_failure::unavailable
					};
				try {
					pointOfNoReturn = advance_append(
						*context,
						*operation.Record,
						expectedCatalog,
						epoch_catalog_operation_phase::
							ref_commit_in_progress,
						aStop);
				} catch (...) {
					return resolveAmbiguity(
						core::epoch_failure::repair_required, false);
				}
				if (!pointOfNoReturn) {
					const auto wasCancelled = pointOfNoReturn.error()
						== epoch_catalog_failure::cancelled;
					return resolveAmbiguity(
						wasCancelled
							? core::epoch_failure::unavailable
							: catalog_failure(
								  pointOfNoReturn.error()),
						wasCancelled);
				}
				operation.Record = *pointOfNoReturn;

				const auto resolveRecovery = [&]() -> worker_completion {
					return resolveAmbiguity(
						core::epoch_failure::repair_required, false);
				};

				try {
					auto retained = context->Actor->Cache->promote_retained(
						*operation.Git,
						git_ref_promotion_authorization{
							.CatalogIncarnation      = expectedCatalog.Incarnation,
							.ExpectedCatalogRevision = expectedCatalog.Revision,
							.Operation               = operation.Operation,
							.Scope                   = context->Authority.Scope,
							.Ordinal                 = append->Epoch.Id.Ordinal,
							.Base                    = append->Epoch.Base,
							.Head                    = append->Epoch.Head,
							.DurableCommitToken      = *durableCommitToken,
						},
						{ });
					if (!retained) {
						return resolveRecovery();
					}
					if (*retained != expected_retained_pair(*operation.Record)) {
						return resolveRecovery();
					}
					record = advance_append(
						*context,
						*operation.Record,
						expectedCatalog,
						epoch_catalog_operation_phase::refs_committed,
						{ });
					if (!record) {
						return resolveRecovery();
					}
					operation.Record     = *record;
					const auto inspected = operation.Git->Residency
							== git_revision_residency::sparse_commit_trees
						? context->Actor->Cache->inspect_sparse_retained(
							  append->Epoch.Id.Ordinal, { })
						: context->Actor->Cache->inspect_retained(
							  append->Epoch.Id.Ordinal, { });
					if (!inspected) {
						return resolveRecovery();
					}
					if (
						*inspected
						!= expected_retained_pair(*operation.Record)) {
						return resolveRecovery();
					}
					const auto finalized = context->Catalog->finalize_append(
						finalize_append_request{
							.Catalog                   = expectedCatalog,
							.Operation                 = operation.Operation,
							.ExpectedOperationRevision = operation.Record->OperationRevision,
							.Mutation                  = *append,
							.ObservedRefs              = *inspected,
						},
						{ });
					if (!finalized) {
						return resolveRecovery();
					}
					if (
						!exact_catalog_commit(
							*finalized,
							expectedCatalog,
							append->Epoch.Id)
						|| context->Catalog->binding().Catalog
							!= finalized->Catalog) {
						return resolveRecovery();
					}

					// Catalog visibility is the durable-success linearization
					// point. Cleanup failure remains recoverable and must not
					// replace this committed terminal with failure/cancellation.
					auto cleanupRecord              = *operation.Record;
					cleanupRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
					cleanupRecord.OperationRevision = epoch_catalog_operation_revision{
						cleanupRecord.OperationRevision.value() + 1
					};
					const auto cleanupRequest = recovery_request(cleanupRecord);
					if (!cleanupRequest) {
						mark_append_repair(*context->Catalog, cleanupRecord);
					} else {
						const auto cleaned = context->Actor->Cache->complete_recovered_operation(
							*cleanupRequest, { });
						if (cleaned) {
							static_cast<void>(advance_append(
								*context,
								cleanupRecord,
								finalized->Catalog,
								epoch_catalog_operation_phase::complete,
								{ }));
						} else if (
							cache_failure(cleaned.error())
							== core::epoch_failure::repair_required) {
							mark_append_repair(
								*context->Catalog, cleanupRecord);
						}
					}
					return succeeded(finalized->Catalog.Revision);
				} catch (...) {
					return resolveRecovery();
				}
			},
			[aRequest](implementation& aHost) {
				aHost.dispatch_core(core::epoch_catalog_commit_failed_event{
					.Context  = aRequest.Context,
					.Mutation = aRequest.Mutation,
					.Failure  = core::epoch_failure::repair_required,
				});
			},
			[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
				aHost.dispatch_core(core::epoch_catalog_commit_failed_event{
					.Context  = aRequest.Context,
					.Mutation = aRequest.Mutation,
					.Failure  = aFailure,
				});
			},
			aRequest.Context.Request,
			aRequest.Context.Generation,
			std::nullopt,
			std::nullopt,
			std::move(actor));
	}

	void epoch_application_host::implementation::discard_preparation(
		const core::discard_epoch_preparation_request& aRequest)
	{
		auto context = exact_context(
			aRequest.Context.Authority, aRequest.Context.Revision);
		if (!context) {
			return;
		}
		static_cast<void>(discard_context_preparation(
			std::move(context), aRequest.Context.Operation));
	}

	void epoch_application_host::implementation::start_epoch_maintenance(
		const core::perform_epoch_maintenance_request& aRequest)
	{
		auto context = Active;
		if (!context || context->Authority != aRequest.Context.Authority
			|| context->Revision.Source != aRequest.Context.Source) {
			return;
		}
		auto* const caches  = &Caches;
		auto* const entropy = &Entropy;
		auto        actor   = context->Actor;
		submit_effect(
			[context, aRequest, caches, entropy](
				const std::stop_token aStop) -> worker_completion {
				const auto completed =
					[aRequest](const core::epoch_catalog_identity aCatalog)
					-> worker_completion {
					return [aRequest, aCatalog](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_maintenance_completed_event{
								.Context    = aRequest.Context,
								.NewCatalog = aCatalog,
							});
					};
				};
				const auto failed =
					[aRequest](
						const core::epoch_failure aFailure) -> worker_completion {
					return [aRequest, aFailure](implementation& aHost) {
						aHost.dispatch_core(core::epoch_maintenance_failed_event{
							.Context = aRequest.Context,
							.Failure = aFailure,
						});
					};
				};
				const auto cancelled = [aRequest]() -> worker_completion {
					return [aRequest](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_maintenance_cancelled_event{
								.Context = aRequest.Context,
							});
					};
				};
				const auto& binding = context->Catalog->binding();
				if (!application::valid(binding)
					|| binding.Authority != aRequest.Context.Authority
					|| binding.Catalog != aRequest.Context.Catalog
					|| aRequest.Context.Cache != binding.Authority.Cache
					|| aRequest.Context.ApplicationSession
						!= context->Handoff.ApplicationSession
					|| aRequest.Context.Generation.value() == 0U
					|| aRequest.Context.Request.value() == 0U
					|| !aRequest.Context.TargetDigest.valid()) {
					return failed(core::epoch_failure::catalog_conflict);
				}

				const auto snapshot = context->Catalog->snapshot(aStop);
				if (!snapshot) {
					return snapshot.error() == epoch_catalog_failure::cancelled
						? cancelled()
						: failed(catalog_failure(snapshot.error()));
				}
				if (!application::valid(snapshot->Binding)
					|| snapshot->Binding != binding) {
					return failed(core::epoch_failure::repair_required);
				}

				std::optional<maintenance_operation_record> operation;
				for (std::size_t attempt = 0U;
					 attempt < maximum_identity_attempts;
					 ++attempt) {
					const auto identity = mint_identity<core::epoch_operation_id>(*entropy);
					if (!identity) {
						return failed(core::epoch_failure::unavailable);
					}

					if (aRequest.Context.Kind
						== core::epoch_maintenance_kind::prune_epochs) {
						if (aRequest.Context.Epochs.empty()
							|| aRequest.Context.Epochs.size()
								> maximum_prune_epoch_count) {
							return failed(
								core::epoch_failure::malformed_observation);
						}
						std::vector<core::comparison_anchor_identity>
							identities;
						identities.reserve(aRequest.Context.Epochs.size() * 2U);
						for (std::size_t index = 0U;
							 index < aRequest.Context.Epochs.size();
							 ++index) {
							const auto& epoch = aRequest.Context.Epochs[index];
							if (!core::valid(epoch)
								|| epoch.Authority != binding.Authority.Scope
								|| epoch.Incarnation
									!= binding.Catalog.Incarnation
								|| (index != 0U
									&& !(aRequest.Context.Epochs[index - 1U]
										 < epoch))) {
								return failed(
									core::epoch_failure::malformed_observation);
							}
							identities.push_back(
								core::comparison_anchor_identity{
									.Authority   = epoch.Authority,
									.Incarnation = epoch.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_base,
									.Epoch = epoch.Ordinal,
								});
							identities.push_back(
								core::comparison_anchor_identity{
									.Authority   = epoch.Authority,
									.Incarnation = epoch.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_head,
									.Epoch = epoch.Ordinal,
								});
						}
						std::ranges::sort(identities);
						const auto anchors = context->Catalog->exact_anchors(
							exact_anchor_lookup_request{
								.Catalog = aRequest.Context.Catalog,
								.Anchors = identities,
							},
							aStop);
						if (!anchors) {
							return anchors.error()
									== epoch_catalog_failure::cancelled
								? cancelled()
								: failed(catalog_failure(anchors.error()));
						}
						if (anchors->Catalog != aRequest.Context.Catalog
							|| anchors->Anchors.size() != identities.size()) {
							return failed(core::epoch_failure::repair_required);
						}
						std::vector<prune_epoch_fact> facts;
						facts.reserve(aRequest.Context.Epochs.size());
						for (const auto& epoch : aRequest.Context.Epochs) {
							const auto find = [&](const auto aKind)
								-> const core::comparison_anchor* {
								const auto anchorIdentity = core::comparison_anchor_identity{
									.Authority   = epoch.Authority,
									.Incarnation = epoch.Incarnation,
									.Kind        = aKind,
									.Epoch       = epoch.Ordinal,
								};
								const auto found = std::ranges::find(anchors->Anchors,
																	 anchorIdentity,
																	 &core::comparison_anchor::Identity);
								return found == anchors->Anchors.end()
									? nullptr
									: &*found;
							};
							const auto* base = find(core::comparison_anchor_kind::epoch_base);
							const auto* head = find(core::comparison_anchor_kind::epoch_head);
							if (base == nullptr || head == nullptr
								|| !core::valid(*base) || !core::valid(*head)
								|| !valid_prune_anchor_availability(
									base->Availability)
								|| !valid_prune_anchor_availability(
									head->Availability)
								|| base->Commit.algorithm()
									!= context->Handoff.ObjectFormat
								|| head->Commit.algorithm()
									!= context->Handoff.ObjectFormat) {
								return failed(
									core::epoch_failure::repair_required);
							}
							facts.push_back(prune_epoch_fact{
								.Epoch    = epoch,
								.Retained = git_retained_ref_pair{
									.Scope   = epoch.Authority,
									.Ordinal = epoch.Ordinal,
									.Base    = base->Commit,
									.Head    = head->Commit,
								},
								.BaseRef = epoch_catalog_ref_identity{
									.Authority = epoch.Authority,
									.Ordinal   = epoch.Ordinal,
								},
								.HeadRef = epoch_catalog_ref_identity{
									.Authority = epoch.Authority,
									.Ordinal   = epoch.Ordinal,
									.Head      = true,
								},
								.ExpectedBaseAvailability = base->Availability,
								.ExpectedHeadAvailability = head->Availability,
							});
						}
						const auto begin = begin_prune_request{
							.Catalog   = aRequest.Context.Catalog,
							.Operation = *identity,
							.Epochs    = std::move(facts),
						};
						auto begun = context->Catalog->begin_prune(begin, aStop);
						if (begun) {
							if (!exact_prune_intent(*begun,
													begin,
													binding.Authority.Cache)) {
								return failed(
									core::epoch_failure::repair_required);
							}
							operation = std::move(*begun);
							break;
						}
						if (begun.error() == epoch_catalog_failure::cancelled) {
							return cancelled();
						}
						if (begun.error()
								!= epoch_catalog_failure::operation_conflict
							&& begun.error()
								!= epoch_catalog_failure::
									operation_identity_conflict) {
							return failed(catalog_failure(begun.error()));
						}
					} else if (aRequest.Context.Kind
							   == core::epoch_maintenance_kind::
								   delete_repository_cache) {
						if (!aRequest.Context.Epochs.empty()) {
							return failed(
								core::epoch_failure::malformed_observation);
						}
						const auto impact = context->Catalog->cache_delete_impact(
							aRequest.Context.Cache,
							aStop);
						if (!impact) {
							return impact.error()
									== epoch_catalog_failure::cancelled
								? cancelled()
								: failed(catalog_failure(impact.error()));
						}
						const auto currentScope = std::ranges::find(impact->Scopes,
																	binding.Authority.Scope,
																	&cache_delete_scope_fact::Authority);
						if (!application::valid(*impact)
							|| impact->Incarnation
								!= binding.Catalog.Incarnation
							|| impact->Cache != binding.Authority.Cache
							|| currentScope == impact->Scopes.end()
							|| currentScope->ExpectedRevision
								!= binding.Catalog.Revision) {
							return failed(core::epoch_failure::repair_required);
						}
						const auto begin = begin_cache_delete_request{
							.Operation = *identity,
							.Impact    = *impact,
						};
						auto begun = context->Catalog->begin_cache_delete(begin, aStop);
						if (begun) {
							if (!exact_cache_delete_intent(*begun, begin)) {
								return failed(
									core::epoch_failure::repair_required);
							}
							operation = std::move(*begun);
							break;
						}
						if (begun.error() == epoch_catalog_failure::cancelled) {
							return cancelled();
						}
						if (begun.error()
								!= epoch_catalog_failure::operation_conflict
							&& begun.error()
								!= epoch_catalog_failure::
									operation_identity_conflict) {
							return failed(catalog_failure(begun.error()));
						}
					} else {
						return failed(
							core::epoch_failure::malformed_observation);
					}
				}
				if (!operation) {
					return failed(core::epoch_failure::catalog_conflict);
				}

				if (aStop.stop_requested()) {
					const auto aborted = recover_maintenance(*context->Catalog,
															 *caches,
															 context->Handoff,
															 context->Actor->Cache,
															 *operation,
															 { });
					return aborted ? cancelled() : failed(aborted.error());
				}
				const auto next       = operation->Kind == epoch_catalog_operation_kind::prune
					? epoch_catalog_operation_phase::ref_delete_in_progress
					: epoch_catalog_operation_phase::
						  quarantine_rename_in_progress;
				auto       authorized = advance_maintenance(*context->Catalog,
															*operation,
															aRequest.Context.Catalog,
															next,
															aStop);
				if (!authorized) {
					if (authorized.error() == epoch_catalog_failure::cancelled) {
						const auto aborted = recover_maintenance(*context->Catalog,
																 *caches,
																 context->Handoff,
																 context->Actor->Cache,
																 *operation,
																 { });
						return aborted ? cancelled() : failed(aborted.error());
					}
					return failed(catalog_failure(authorized.error()));
				}
				const auto recovered = recover_maintenance(*context->Catalog,
														   *caches,
														   context->Handoff,
														   context->Actor->Cache,
														   *authorized,
														   { });
				return recovered ? completed(*recovered)
								 : failed(recovered.error());
			},
			[aRequest](implementation& aHost) {
				aHost.dispatch_core(core::epoch_maintenance_failed_event{
					.Context = aRequest.Context,
					.Failure = core::epoch_failure::unavailable,
				});
			},
			[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
				aHost.dispatch_core(core::epoch_maintenance_failed_event{
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

	void epoch_application_host::implementation::start_history_page(
		const core::load_epoch_history_page_request& aRequest)
	{
		auto context = Active;
		if (!context || context->Authority != aRequest.Context.Authority
			|| context->Revision.Source != aRequest.Context.Source) {
			return;
		}
		auto actor = context->Actor;
		submit_effect(
			[context, aRequest](const std::stop_token aStop)
				-> worker_completion {
				if (
					!application::valid(context->Catalog->binding())
					|| context->Catalog->binding().Authority
						!= aRequest.Context.Authority
					|| context->Catalog->binding().Catalog
						!= aRequest.Context.Catalog) {
					return [aRequest](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_history_page_load_failed_event{
								.Context = aRequest.Context,
								.Failure = core::epoch_failure::catalog_conflict,
							});
					};
				}
				auto page = context->Catalog->history(
					epoch_history_page_request{
						.Catalog = aRequest.Context.Catalog,
						.Cursor  = aRequest.Context.Cursor,
						.Limit   = aRequest.Context.Limit,
					},
					aStop);
				if (!page) {
					if (page.error() == epoch_catalog_failure::cancelled) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_history_page_load_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					}
					const auto failure = catalog_failure(page.error());
					return [aRequest, failure](implementation& aHost) {
						aHost.dispatch_core(
							core::epoch_history_page_load_failed_event{
								.Context = aRequest.Context,
								.Failure = failure,
							});
					};
				}
				auto observation = core::epoch_history_page_observation{
					.Authority    = page->Authority,
					.Catalog      = page->Catalog,
					.Epochs       = std::move(page->Epochs),
					.Continuation = page->Continuation,
				};
				return [aRequest, pageObservation = std::move(observation)](
						   implementation& aHost) {
					aHost.dispatch_core(core::epoch_history_page_loaded_event{
						.Context = aRequest.Context,
						.Page    = pageObservation,
					});
				};
			},
			[aRequest](implementation& aHost) {
				aHost.dispatch_core(
					core::epoch_history_page_load_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
			},
			[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
				aHost.dispatch_core(
					core::epoch_history_page_load_failed_event{
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

	void epoch_application_host::implementation::start_comparison_resolution(
		const core::resolve_comparison_request& aRequest)
	{
		auto context = Active;
		if (!context || context->Authority != aRequest.Context.Authority
			|| context->Revision.Source != aRequest.Context.Source) {
			return;
		}
		auto* const caches = &Caches;
		auto        actor  = context->Actor;
		submit_effect(
			[context, aRequest, caches](const std::stop_token aStop)
				-> worker_completion {
				const auto snapshot = context->Catalog->snapshot(aStop);
				if (!snapshot) {
					if (snapshot.error() == epoch_catalog_failure::cancelled) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					}
					const auto failure = catalog_failure(snapshot.error());
					return [aRequest, failure](implementation& aHost) {
						aHost.dispatch_core(core::comparison_resolve_failed_event{
							.Context = aRequest.Context,
							.Failure = failure,
						});
					};
				}
				if (
					!application::valid(snapshot->Binding)
					|| snapshot->Binding != context->Catalog->binding()) {
					return [aRequest](implementation& aHost) {
						aHost.dispatch_core(
							core::comparison_resolve_failed_event{
								.Context = aRequest.Context,
								.Failure = core::epoch_failure::repair_required,
							});
					};
				}
				const auto expectedCatalog = core::epoch_catalog_identity{
					.Incarnation = aRequest.Context.CatalogIncarnation,
					.Revision    = aRequest.Context.CatalogRevision,
				};
				if (snapshot->Binding.Catalog != expectedCatalog) {
					return [aRequest](implementation& aHost) {
						aHost.dispatch_core(core::comparison_resolve_failed_event{
							.Context = aRequest.Context,
							.Failure = core::epoch_failure::catalog_conflict,
						});
					};
				}

				core::comparison_candidates candidates{
					.Catalog              = snapshot->Binding.Catalog,
					.LatestLive           = snapshot->LatestLive,
					.PreviousDistinctHead = snapshot->PreviousDistinctHead,
				};
				if (snapshot->CurrentViewerReview) {
					candidates.CurrentViewerReview = review_anchor(
						context->Authority,
						snapshot->Binding.Catalog,
						*snapshot->CurrentViewerReview);
				}
				if (const auto* explicitPair = std::get_if<core::explicit_comparison_selection>(
						&aRequest.Context.Selection)) {
					std::vector<core::comparison_anchor_identity> identities{
						explicitPair->Left,
						explicitPair->Right,
					};
					std::ranges::sort(identities);
					identities.erase(
						std::unique(identities.begin(), identities.end()),
						identities.end());
					const auto exact = context->Catalog->exact_anchors(
						exact_anchor_lookup_request{
							.Catalog = snapshot->Binding.Catalog,
							.Anchors = identities,
						},
						aStop);
					if (!exact) {
						if (exact.error() == epoch_catalog_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = catalog_failure(exact.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_failed_event{
									.Context = aRequest.Context,
									.Failure = failure,
								});
						};
					}
					std::vector<core::comparison_anchor_identity>
						returnedIdentities;
					returnedIdentities.reserve(exact->Anchors.size());
					for (const auto& anchor : exact->Anchors) {
						if (
							!core::valid(anchor)
							|| !std::ranges::binary_search(
								identities, anchor.Identity)) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_failed_event{
										.Context = aRequest.Context,
										.Failure = core::epoch_failure::
											repair_required,
									});
							};
						}
						returnedIdentities.push_back(anchor.Identity);
					}
					std::ranges::sort(returnedIdentities);
					if (
						exact->Catalog != snapshot->Binding.Catalog
						|| exact->Anchors.size() > identities.size()
						|| std::ranges::adjacent_find(returnedIdentities)
							!= returnedIdentities.end()) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_failed_event{
									.Context = aRequest.Context,
									.Failure = core::epoch_failure::
										repair_required,
								});
						};
					}
					const auto findAnchor = [&exact](const auto& aIdentity)
						-> std::optional<core::comparison_anchor> {
						const auto found = std::ranges::find(
							exact->Anchors,
							aIdentity,
							&core::comparison_anchor::Identity);
						return found == exact->Anchors.end()
							? std::nullopt
							: std::optional{ *found };
					};
					candidates.ExplicitLeft  = findAnchor(explicitPair->Left);
					candidates.ExplicitRight = findAnchor(explicitPair->Right);
				}

				const auto expectedCache = git_cache_open_request{
					.ApplicationSession = aRequest.Context.ApplicationSession,
					.Authority          = context->Authority,
					.ObjectFormat       = context->Handoff.ObjectFormat,
				};
				const auto openedCache = open_actor_cache(
					*context->Actor, *caches, expectedCache, aStop);
				if (!openedCache) {
					if (openedCache.error() == git_cache_failure::cancelled) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					}
					const auto failure = cache_failure(openedCache.error());
					return [aRequest, failure](implementation& aHost) {
						aHost.dispatch_core(
							core::comparison_resolve_failed_event{
								.Context = aRequest.Context,
								.Failure = failure,
							});
					};
				}
				std::vector<std::pair<core::git_commit_oid, bool>> verified;
				const auto                                         sparseCandidate = candidates.LatestLive
					&& candidates.LatestLive->Material.has_value()
					&& candidates.LatestLive->Base == context->Revision.Base
					&& candidates.LatestLive->Head == context->Revision.Head
					&& object_loading_for(context->Handoff.Shape)
						== pull_request_object_loading::selected_objects
					&& context->Handoff.SmartGit
					&& context->Handoff.SmartGit->supports_sparse_commit_trees();
				const auto sparseCurrentPair = [&] {
					if (!sparseCandidate) {
						return false;
					}
					const auto recorded = context->Catalog->retained_residency(
						candidates.LatestLive->Id.Ordinal, aStop);
					if (!recorded || *recorded != git_revision_residency::sparse_commit_trees) {
						return false;
					}
					const auto retained = context->Actor->Cache->inspect_sparse_retained(
						candidates.LatestLive->Id.Ordinal, aStop);
					return retained && application::valid(*retained)
						&& retained->Residency == git_revision_residency::sparse_commit_trees
						&& retained->Scope == context->Authority.Scope
						&& retained->Ordinal == candidates.LatestLive->Id.Ordinal
						&& retained->Base == context->Revision.Base
						&& retained->Head == context->Revision.Head;
				}();
				const auto isVerified = [&](const core::git_commit_oid aCommit) {
					const auto prior = std::ranges::find_if(
						verified,
						[&](const auto& aEntry) {
							return aEntry.first == aCommit;
						});
					if (prior != verified.end()) {
						return prior->second;
					}
					const auto inspected = context->Actor->Cache->inspect_commit_object(
						aCommit, aStop);
					auto exact = inspected
						&& application::valid(*inspected)
						&& inspected->Commit == aCommit
						&& inspected->Residency == git_revision_residency::complete_closure;
					if (!exact && sparseCurrentPair
						&& (aCommit == context->Revision.Base
							|| aCommit == context->Revision.Head)) {
						const auto sparse = context->Actor->Cache->inspect_sparse_commit_object(
							aCommit, aStop);
						exact = sparse && application::valid(*sparse)
							&& sparse->Commit == aCommit
							&& sparse->Residency == git_revision_residency::sparse_commit_trees
							&& sparse->Tree == (aCommit == context->Revision.Base ? candidates.LatestLive->Material->BaseTree : candidates.LatestLive->Material->HeadTree);
					}
					if (!exact && sparseCandidate) {
						const auto matchesSparseAnchor = [&](const std::optional<core::comparison_anchor>& aAnchor) {
							if (!aAnchor || aAnchor->Commit != aCommit
								|| (aAnchor->Identity.Kind != core::comparison_anchor_kind::epoch_base
									&& aAnchor->Identity.Kind != core::comparison_anchor_kind::epoch_head)
								|| aAnchor->Identity.Authority != context->Authority.Scope
								|| aAnchor->Identity.Incarnation != snapshot->Binding.Catalog.Incarnation) {
								return false;
							}
							const auto recorded = context->Catalog->retained_residency(aAnchor->Identity.Epoch, aStop);
							if (!recorded || *recorded != git_revision_residency::sparse_commit_trees) {
								return false;
							}
							const auto retained = context->Actor->Cache->inspect_sparse_retained(aAnchor->Identity.Epoch, aStop);
							if (!retained || !application::valid(*retained)
								|| retained->Residency != git_revision_residency::sparse_commit_trees
								|| retained->Scope != context->Authority.Scope
								|| retained->Ordinal != aAnchor->Identity.Epoch
								|| (aAnchor->Identity.Kind == core::comparison_anchor_kind::epoch_base
										? retained->Base
										: retained->Head)
									!= aCommit) {
								return false;
							}
							const auto sparse = context->Actor->Cache->inspect_sparse_commit_object(aCommit, aStop);
							return sparse && application::valid(*sparse) && sparse->Commit == aCommit
								&& sparse->Residency == git_revision_residency::sparse_commit_trees;
						};
						// [[BPR-1560]]. Previous upload's left side is the
						// preceding update's retained head, proved exactly as
						// an explicitly chosen one is.
						const auto previousHead = candidates.PreviousDistinctHead
							? std::optional{ core::comparison_anchor{
								  .Identity     = { .Authority   = candidates.PreviousDistinctHead->Id.Authority,
													.Incarnation = candidates.PreviousDistinctHead->Id.Incarnation,
													.Kind        = core::comparison_anchor_kind::epoch_head,
													.Epoch       = candidates.PreviousDistinctHead->Id.Ordinal },
								  .Commit       = candidates.PreviousDistinctHead->Head,
								  .Availability = candidates.PreviousDistinctHead->HeadAvailability } }
							: std::nullopt;
						exact                   = matchesSparseAnchor(candidates.ExplicitLeft)
							|| matchesSparseAnchor(candidates.ExplicitRight) || matchesSparseAnchor(previousHead);
					}
					verified.emplace_back(aCommit, exact);
					return exact;
				};
				const auto verifyEpoch = [&](auto& aEpoch) {
					if (
						aEpoch.BaseAvailability
							== core::anchor_availability::available
						&& !isVerified(aEpoch.Base)) {
						aEpoch.BaseAvailability = core::anchor_availability::unavailable;
					}
					if (
						aEpoch.HeadAvailability
							== core::anchor_availability::available
						&& !isVerified(aEpoch.Head)) {
						aEpoch.HeadAvailability = core::anchor_availability::unavailable;
					}
				};
				// [[BPR-1560]]. On a selected-objects review an imported
				// update GitHub recorded is retained with the current
				// comparison base as its base, unproven for that head: its
				// base is no anchor to compare from ("Show this update
				// alone"). Its head stays comparable.
				auto       importedEpochs = std::optional<std::vector<core::epoch_ordinal>>{ };
				const auto importedBase   = [&](const core::comparison_anchor& aAnchor) {
					if (aAnchor.Identity.Kind != core::comparison_anchor_kind::epoch_base
						|| object_loading_for(context->Handoff.Shape) != pull_request_object_loading::selected_objects) {
						return false;
					}
					if (!importedEpochs) {
						importedEpochs.emplace();
						if (const auto history = context->Catalog->occurrences(aStop); history) {
							for (const auto& occurrence : history->Occurrences) {
								if (occurrence.LocalEpoch && occurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event) {
									importedEpochs->push_back(occurrence.LocalEpoch->Ordinal);
								}
							}
						} else {
							return true;
						}
					}
					return std::ranges::find(*importedEpochs, aAnchor.Identity.Epoch) != importedEpochs->end()
						&& (!candidates.LatestLive || candidates.LatestLive->Id.Ordinal != aAnchor.Identity.Epoch);
				};
				const auto verifyAnchor = [&](auto& aAnchor) {
					if (
						aAnchor
						&& aAnchor->Availability
							== core::anchor_availability::available
						&& (!isVerified(aAnchor->Commit) || importedBase(*aAnchor))) {
						aAnchor->Availability = core::anchor_availability::unavailable;
					}
				};
				if (candidates.LatestLive) {
					verifyEpoch(*candidates.LatestLive);
				}
				if (candidates.PreviousDistinctHead) {
					verifyEpoch(*candidates.PreviousDistinctHead);
				}
				verifyAnchor(candidates.CurrentViewerReview);
				verifyAnchor(candidates.ExplicitLeft);
				verifyAnchor(candidates.ExplicitRight);
				return [aRequest, resolvedCandidates = std::move(candidates)](
						   implementation& aHost) {
					aHost.dispatch_core(core::comparison_resolved_event{
						.Context    = aRequest.Context,
						.Candidates = resolvedCandidates,
					});
				};
			},
			[aRequest](implementation& aHost) {
				aHost.dispatch_core(core::comparison_resolve_failed_event{
					.Context = aRequest.Context,
					.Failure = core::epoch_failure::unavailable,
				});
			},
			[aRequest](implementation& aHost, const core::epoch_failure aFailure) {
				aHost.dispatch_core(core::comparison_resolve_failed_event{
					.Context = aRequest.Context,
					.Failure = aFailure,
				});
			},
			aRequest.Context.Request,
			std::nullopt,
			aRequest.Context.Generation,
			std::nullopt,
			std::move(actor));
	}

	void epoch_application_host::implementation::remember_earlier_head(const core::pull_request_authority& aAuthority, const core::git_commit_oid& aHead)
	{
		auto entry = std::pair{ aAuthority, aHead };
		std::erase(EarlierHeads, entry);
		EarlierHeads.push_back(std::move(entry));
		if (EarlierHeads.size() > maximum_earlier_heads) {
			EarlierHeads.erase(EarlierHeads.begin());
		}
	}
}
