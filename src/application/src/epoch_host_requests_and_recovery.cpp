#include "epoch_host_implementation.hpp"

#include "bpr/application/git_pack_stream.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <limits>
#include <string_view>

namespace bpr::application {
	namespace {
		inline constexpr std::size_t maximum_recovery_operations = 1'000U;

		inline constexpr std::size_t maximum_cache_actors = 1'000U;

		[[nodiscard]] auto projected_revision(
			const core::accepted_pull_request_revision& aRevision)
			-> epoch_revision_projection
		{
			return {
				.Snapshot              = aRevision.Source.Snapshot,
				.IdentityGeneration    = aRevision.Source.IdentityGeneration,
				.PullRequestGeneration = aRevision.Source.PullRequestGeneration,
				.Origin                = aRevision.Origin,
				.BaseBranch            = aRevision.BaseBranch,
				.Base                  = aRevision.Base,
				.Head                  = aRevision.Head,
				.CurrentViewerReview   = aRevision.CurrentViewerReview,
			};
		}

		[[nodiscard]] auto projected_row(const core::review_epoch_summary& aEpoch)
			-> epoch_row_projection
		{
			return {
				.Id                    = aEpoch.Id,
				.Snapshot              = aEpoch.Source.Snapshot,
				.IdentityGeneration    = aEpoch.Source.IdentityGeneration,
				.PullRequestGeneration = aEpoch.Source.PullRequestGeneration,
				.BaseBranch            = aEpoch.BaseBranch,
				.Base                  = aEpoch.Base,
				.Head                  = aEpoch.Head,
				.Material              = aEpoch.Material,
				.Cause                 = aEpoch.Cause,
				.ObservedAt            = aEpoch.ObservedAt,
				.BaseAvailability      = aEpoch.BaseAvailability,
				.HeadAvailability      = aEpoch.HeadAvailability,
			};
		}

		[[nodiscard]] auto recovered_prepared_revision(
			const append_operation_record& aRecord)
			-> std::optional<git_prepared_revision>
		{
			if (!aRecord.Install) {
				return std::nullopt;
			}
			auto prepared = git_prepared_revision{
				.Operation      = aRecord.Context.Operation,
				.Pack           = aRecord.Install->Pack,
				.InstalledPack  = aRecord.Install->InstalledPack,
				.InstalledIndex = aRecord.Install->InstalledIndex,
				.Base           = aRecord.Install->Base,
				.BaseTree       = aRecord.Install->BaseTree,
				.Head           = aRecord.Install->Head,
				.HeadTree       = aRecord.Install->HeadTree,
				.HeadLineage    = aRecord.Install->HeadLineage,
				.ImportedBytes  = aRecord.Install->InstalledPack.Bytes,
				.Residency      = aRecord.Install->Residency,
			};
			return application::valid(prepared) ? std::optional{ std::move(prepared) }
												: std::nullopt;
		}

		[[nodiscard]] auto same_maintenance_identity(
			const maintenance_operation_record& aPrior,
			const maintenance_operation_record& aNext) noexcept -> bool
		{
			return application::valid(aNext) && aNext.Incarnation == aPrior.Incarnation
				&& aNext.Operation == aPrior.Operation && aNext.Kind == aPrior.Kind
				&& aNext.Cache == aPrior.Cache && aNext.Epochs == aPrior.Epochs
				&& aNext.Scopes == aPrior.Scopes;
		}

		[[nodiscard]] auto exact_maintenance_transition(
			const maintenance_operation_record& aPrior,
			const maintenance_operation_record& aNext,
			const epoch_catalog_operation_phase aPhase) noexcept -> bool
		{
			return same_maintenance_identity(aPrior, aNext)
				&& application::can_advance(aPrior.OperationRevision)
				&& aNext.OperationRevision.value() == aPrior.OperationRevision.value() + 1
				&& aNext.Phase == aPhase
				&& (aPhase == epoch_catalog_operation_phase::repair_required
						? aNext.RepairFromPhase == std::optional{ aPrior.Phase }
						: !aNext.RepairFromPhase);
		}

		[[nodiscard]] auto recovery_operation_identity(
			const epoch_recovery_operation& aOperation) noexcept
			-> core::epoch_operation_id
		{
			return std::visit(
				[](const auto& aValue) {
					using value_type = std::remove_cvref_t<decltype(aValue)>;
					if constexpr (
						std::is_same_v<value_type, append_operation_record>) {
						return aValue.Context.Operation;
					} else {
						return aValue.Operation;
					}
				},
				aOperation);
		}

		[[nodiscard]] auto exact_append_transition(
			const append_operation_record&      aPrior,
			const append_operation_record&      aNext,
			const epoch_catalog_operation_phase aPhase) noexcept -> bool
		{
			if (
				!same_append_identity(aPrior, aNext)
				|| !application::can_advance(aPrior.OperationRevision)
				|| aNext.OperationRevision.value()
					!= aPrior.OperationRevision.value() + 1
				|| aNext.Phase != aPhase) {
				return false;
			}
			const auto terminal = aPhase == epoch_catalog_operation_phase::complete
				|| aPhase == epoch_catalog_operation_phase::aborted;
			const auto exactRepairOrigin = aPhase == epoch_catalog_operation_phase::repair_required
				? aNext.RepairFromPhase == std::optional{ aPrior.Phase }
				: !aNext.RepairFromPhase;
			return exactRepairOrigin && aNext.Install == aPrior.Install
				&& aNext.Candidate == aPrior.Candidate
				&& (terminal
						? aNext.RecoveryInputCompacted && !aNext.Receiving && !aNext.Spool
							&& !aNext.Preimport
						: !aNext.RecoveryInputCompacted
							&& aNext.Receiving == aPrior.Receiving
							&& aNext.Spool == aPrior.Spool
							&& aNext.Preimport == aPrior.Preimport);
		}
	}

	auto epoch_application_host::implementation::advance_append(
		epoch_catalog_session&              aCatalogSession,
		const append_operation_record&      aRecord,
		const core::epoch_catalog_identity  aCatalog,
		const epoch_catalog_operation_phase aNext,
		const std::stop_token               aStop)
		-> epoch_catalog_result<append_operation_record>
	{
		const auto evidence = required_transition_evidence(
			epoch_catalog_operation_kind::append,
			aRecord.Phase,
			aNext);
		if (!evidence) {
			return std::unexpected{ epoch_catalog_failure::phase_conflict };
		}
		auto advanced = aCatalogSession.transition_operation(
			operation_transition_request{
				.Catalog                   = aCatalog,
				.Operation                 = aRecord.Context.Operation,
				.Kind                      = epoch_catalog_operation_kind::append,
				.Expected                  = aRecord.Phase,
				.Next                      = aNext,
				.Evidence                  = *evidence,
				.ExpectedOperationRevision = aRecord.OperationRevision,
			},
			aStop);
		if (!advanced) {
			return std::unexpected{ advanced.error() };
		}
		const auto* append = std::get_if<append_operation_record>(&*advanced);
		if (
			append == nullptr
			|| !exact_append_transition(aRecord, *append, aNext)) {
			return std::unexpected{ epoch_catalog_failure::operation_conflict };
		}
		return *append;
	}

	auto epoch_application_host::implementation::advance_append(
		authority_context&                  aContext,
		const append_operation_record&      aRecord,
		const core::epoch_catalog_identity  aCatalog,
		const epoch_catalog_operation_phase aNext,
		const std::stop_token               aStop)
		-> epoch_catalog_result<append_operation_record>
	{
		return advance_append(
			*aContext.Catalog, aRecord, aCatalog, aNext, aStop);
	}

	auto epoch_application_host::implementation::advance_maintenance(
		epoch_catalog_session&              aCatalog,
		const maintenance_operation_record& aRecord,
		const core::epoch_catalog_identity  aExpectedCatalog,
		const epoch_catalog_operation_phase aNext,
		const std::stop_token               aStop)
		-> epoch_catalog_result<maintenance_operation_record>
	{
		const auto evidence = required_transition_evidence(aRecord.Kind, aRecord.Phase, aNext);
		if (!evidence) {
			return std::unexpected{ epoch_catalog_failure::phase_conflict };
		}
		auto advanced = aCatalog.transition_operation(
			operation_transition_request{
				.Catalog                   = aExpectedCatalog,
				.Operation                 = aRecord.Operation,
				.Kind                      = aRecord.Kind,
				.Expected                  = aRecord.Phase,
				.Next                      = aNext,
				.Evidence                  = *evidence,
				.ExpectedOperationRevision = aRecord.OperationRevision,
			},
			aStop);
		if (!advanced) {
			return std::unexpected{ advanced.error() };
		}
		const auto* maintenance = std::get_if<maintenance_operation_record>(&*advanced);
		if (maintenance == nullptr
			|| !exact_maintenance_transition(aRecord, *maintenance, aNext)) {
			return std::unexpected{ epoch_catalog_failure::operation_conflict };
		}
		return *maintenance;
	}

	auto epoch_application_host::implementation::open_actor_cache(authority_effect_actor&       aActor,
																  git_cache_port&               aCaches,
																  const git_cache_open_request& aExpected,
																  const std::stop_token         aStop)
		-> git_cache_result<git_cache_session*>
	{
		if (aActor.Cache && aActor.Cache->request() != aExpected) {
			aActor.Cache.reset();
		}
		if (!aActor.Cache) {
			auto opened = aCaches.open(aExpected, aStop);
			if (!opened) {
				return std::unexpected{ opened.error() };
			}
			if (
				!*opened
				|| !application::valid((*opened)->request())
				|| (*opened)->request() != aExpected) {
				return std::unexpected{
					git_cache_failure::repair_required
				};
			}
			aActor.Cache = std::move(*opened);
		}
		return aActor.Cache.get();
	}

	void epoch_application_host::implementation::mark_append_repair(
		epoch_catalog_session&         aCatalog,
		const append_operation_record& aRecord)
	{
		if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
			return;
		}
		static_cast<void>(advance_append(
			aCatalog,
			aRecord,
			aCatalog.binding().Catalog,
			epoch_catalog_operation_phase::repair_required,
			{ }));
	}

	void epoch_application_host::implementation::mark_maintenance_repair(epoch_catalog_session&              aCatalog,
																		 const maintenance_operation_record& aRecord)
	{
		if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
			return;
		}
		static_cast<void>(advance_maintenance(aCatalog,
											  aRecord,
											  aCatalog.binding().Catalog,
											  epoch_catalog_operation_phase::repair_required,
											  { }));
	}

	auto epoch_application_host::implementation::recover_append(
		epoch_catalog_session&  aCatalog,
		git_cache_session&      aCache,
		epoch_entropy_port&     aEntropy,
		append_operation_record aRecord,
		const std::stop_token   aStop)
		-> std::expected<void, core::epoch_failure>
	{
		const auto failedCache = [&](const git_cache_error& aFailure)
			-> std::expected<void, core::epoch_failure> {
			const auto mapped = cache_failure(aFailure);
			if (mapped == core::epoch_failure::repair_required) {
				mark_append_repair(aCatalog, aRecord);
			}
			return std::unexpected{ mapped };
		};
		const auto request = recovery_request(aRecord);
		if (!request) {
			mark_append_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
			return std::unexpected{ core::epoch_failure::repair_required };
		}

		const auto beforePointOfNoReturn = aRecord.Phase == epoch_catalog_operation_phase::preparing
			|| aRecord.Phase
				== epoch_catalog_operation_phase::spool_recorded
			|| aRecord.Phase == epoch_catalog_operation_phase::imported
			|| aRecord.Phase
				== epoch_catalog_operation_phase::staging_ready
			|| aRecord.Phase
				== epoch_catalog_operation_phase::abort_requested;
		if (beforePointOfNoReturn) {
			if (
				aRecord.Phase
				!= epoch_catalog_operation_phase::abort_requested) {
				const auto requested = advance_append(
					aCatalog,
					aRecord,
					aCatalog.binding().Catalog,
					epoch_catalog_operation_phase::abort_requested,
					aStop);
				if (!requested) {
					return std::unexpected{
						catalog_failure(requested.error())
					};
				}
				aRecord = *requested;
			}
			auto cleanup = aCache.abort_recovered_operation(*request, aStop);
			if (!cleanup) {
				cleanup = aCache.abort_recovered_operation(*request, aStop);
			}
			if (!cleanup) {
				if (cleanup.error() == git_cache_failure::repair_required) {
					mark_append_repair(aCatalog, aRecord);
				}
				return failedCache(cleanup.error());
			}
			const auto completed = advance_append(
				aCatalog,
				aRecord,
				aCatalog.binding().Catalog,
				epoch_catalog_operation_phase::aborted,
				aStop);
			if (!completed) {
				return std::unexpected{ catalog_failure(completed.error()) };
			}
			return { };
		}

		if (
			aRecord.Phase
				!= epoch_catalog_operation_phase::commit_authorized
			&& aRecord.Phase
				!= epoch_catalog_operation_phase::ref_commit_in_progress
			&& aRecord.Phase
				!= epoch_catalog_operation_phase::refs_committed
			&& aRecord.Phase
				!= epoch_catalog_operation_phase::visible_cleanup_pending) {
			mark_append_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (!aRecord.Install || !aRecord.Candidate) {
			mark_append_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}

		const auto physicalStop = aRecord.Phase
				== epoch_catalog_operation_phase::commit_authorized
			? aStop
			: std::stop_token{ };
		auto       physical     = aCache.inspect_operation_recovery(*request, physicalStop);
		if (!physical) {
			return failedCache(physical.error());
		}
		if (physical->Operation != request->Operation) {
			mark_append_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		const auto durablePhysicalMatch = physical->Installed == git_recovery_match::exact
			&& physical->Objects == git_recovery_object_match::exact
			&& (physical->Spool == git_recovery_match::absent
				|| physical->Spool == git_recovery_match::exact);
		if (!durablePhysicalMatch) {
			mark_append_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}

		if (
			aRecord.Phase
			== epoch_catalog_operation_phase::commit_authorized) {
			if (physical->Refs != git_recovery_ref_topology::staging) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto started = advance_append(
				aCatalog,
				aRecord,
				aCatalog.binding().Catalog,
				epoch_catalog_operation_phase::ref_commit_in_progress,
				aStop);
			if (!started) {
				return std::unexpected{ catalog_failure(started.error()) };
			}
			aRecord = *started;
		}

		const auto postCommitStop = std::stop_token{ };

		if (
			aRecord.Phase
			== epoch_catalog_operation_phase::ref_commit_in_progress) {
			if (physical->Refs == git_recovery_ref_topology::staging) {
				const auto prepared = recovered_prepared_revision(aRecord);
				const auto token    = aEntropy.generate();
				if (!prepared || !token || !application::valid(*token)) {
					return std::unexpected{ core::epoch_failure::unavailable };
				}
				const auto promoted = aCache.promote_retained(
					*prepared,
					git_ref_promotion_authorization{
						.CatalogIncarnation      = aRecord.Context.Catalog.Incarnation,
						.ExpectedCatalogRevision = aRecord.Context.Catalog.Revision,
						.Operation               = aRecord.Context.Operation,
						.Scope                   = aRecord.FinalBase.Authority,
						.Ordinal                 = aRecord.ReservedOrdinal,
						.Base                    = aRecord.Context.Revision.Base,
						.Head                    = aRecord.Context.Revision.Head,
						.DurableCommitToken      = *token,
					},
					postCommitStop);
				if (!promoted) {
					auto afterPromotion = aCache.inspect_operation_recovery(
						*request, postCommitStop);
					if (!afterPromotion) {
						return failedCache(afterPromotion.error());
					}
					if (afterPromotion->Operation != request->Operation) {
						mark_append_repair(aCatalog, aRecord);
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
					const auto exactAfter = afterPromotion->Installed
							== git_recovery_match::exact
						&& afterPromotion->Objects
							== git_recovery_object_match::exact
						&& (afterPromotion->Spool
								== git_recovery_match::absent
							|| afterPromotion->Spool
								== git_recovery_match::exact);
					if (
						exactAfter
						&& afterPromotion->Refs
							== git_recovery_ref_topology::final) {
						physical = std::move(afterPromotion);
					} else if (
						exactAfter
						&& afterPromotion->Refs
							== git_recovery_ref_topology::staging) {
						return std::unexpected{
							core::epoch_failure::unavailable
						};
					} else {
						mark_append_repair(aCatalog, aRecord);
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
				} else if (*promoted != expected_retained_pair(aRecord)) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{
						core::epoch_failure::repair_required
					};
				}
			} else if (physical->Refs != git_recovery_ref_topology::final) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto committed = advance_append(
				aCatalog,
				aRecord,
				aCatalog.binding().Catalog,
				epoch_catalog_operation_phase::refs_committed,
				postCommitStop);
			if (!committed) {
				return std::unexpected{ catalog_failure(committed.error()) };
			}
			aRecord = *committed;
		} else if (
			aRecord.Phase
					== epoch_catalog_operation_phase::visible_cleanup_pending
				? (
					  physical->Final != git_recovery_match::exact
					  || (physical->Staging != git_recovery_match::absent
						  && physical->Staging
							  != git_recovery_match::exact))
				: physical->Refs != git_recovery_ref_topology::final) {
			mark_append_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}

		const auto retained = aRecord.Install
				&& aRecord.Install->Residency == git_revision_residency::sparse_commit_trees
			? aCache.inspect_sparse_retained(aRecord.ReservedOrdinal, postCommitStop)
			: aCache.inspect_retained(aRecord.ReservedOrdinal, postCommitStop);
		if (!retained) {
			return failedCache(retained.error());
		}
		if (*retained != expected_retained_pair(aRecord)) {
			mark_append_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}

		auto cleanupCatalog = aCatalog.binding().Catalog;
		if (
			aRecord.Phase == epoch_catalog_operation_phase::refs_committed) {
			if (aCatalog.binding().Catalog != aRecord.Context.Catalog) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto finalized = aCatalog.finalize_append(
				finalize_append_request{
					.Catalog                   = aRecord.Context.Catalog,
					.Operation                 = aRecord.Context.Operation,
					.ExpectedOperationRevision = aRecord.OperationRevision,
					.Mutation                  = *aRecord.Candidate,
					.ObservedRefs              = *retained,
					.HistoricalOccurrence      = aRecord.HistoricalOccurrence,
				},
				postCommitStop);
			if (!finalized) {
				return std::unexpected{ catalog_failure(finalized.error()) };
			}
			if (
				!exact_catalog_commit(
					*finalized,
					aRecord.Context.Catalog,
					aRecord.Candidate->Epoch.Id)
				|| aCatalog.binding().Catalog != finalized->Catalog) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{
					core::epoch_failure::repair_required
				};
			}
			cleanupCatalog            = finalized->Catalog;
			aRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
			aRecord.OperationRevision = epoch_catalog_operation_revision{
				aRecord.OperationRevision.value() + 1
			};
		}

		auto cleaned = aCache.complete_recovered_operation(
			*request, postCommitStop);
		if (!cleaned) {
			const auto afterCleanup = aCache.inspect_operation_recovery(
				*request, postCommitStop);
			if (!afterCleanup) {
				return failedCache(afterCleanup.error());
			}
			const auto exactCleanupState = afterCleanup->Operation == request->Operation
				&& afterCleanup->Installed == git_recovery_match::exact
				&& afterCleanup->Objects
					== git_recovery_object_match::exact
				&& afterCleanup->Final == git_recovery_match::exact
				&& (afterCleanup->Staging == git_recovery_match::absent
					|| afterCleanup->Staging == git_recovery_match::exact)
				&& (afterCleanup->Spool == git_recovery_match::absent
					|| afterCleanup->Spool == git_recovery_match::exact);
			if (!exactCleanupState) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{
					core::epoch_failure::repair_required
				};
			}
			cleaned = aCache.complete_recovered_operation(
				*request, postCommitStop);
			if (!cleaned) {
				return std::unexpected{ core::epoch_failure::unavailable };
			}
		}
		const auto completedResult = advance_append(
			aCatalog,
			aRecord,
			cleanupCatalog,
			epoch_catalog_operation_phase::complete,
			postCommitStop);
		if (!completedResult) {
			return std::unexpected{
				catalog_failure(completedResult.error())
			};
		}
		return { };
	}

	auto epoch_application_host::implementation::recover_maintenance(
		epoch_catalog_session&               aCatalog,
		git_cache_port&                      aCaches,
		const accepted_pull_request_handoff& aHandoff,
		std::unique_ptr<git_cache_session>&  aCache,
		maintenance_operation_record         aRecord,
		const std::stop_token                aStop)
		-> std::expected<core::epoch_catalog_identity, core::epoch_failure>
	{
		const auto failCatalog = [&](const epoch_catalog_failure aFailure)
			-> std::expected<core::epoch_catalog_identity, core::epoch_failure> {
			const auto mapped = catalog_failure(aFailure);
			if (mapped == core::epoch_failure::repair_required) {
				mark_maintenance_repair(aCatalog, aRecord);
			}
			return std::unexpected{ mapped };
		};
		const auto failCache = [&](const git_cache_error& aFailure)
			-> std::expected<core::epoch_catalog_identity, core::epoch_failure> {
			const auto mapped = cache_failure(aFailure);
			if (mapped == core::epoch_failure::repair_required) {
				mark_maintenance_repair(aCatalog, aRecord);
			}
			return std::unexpected{ mapped };
		};
		if (!application::valid(aRecord)
			|| aRecord.Incarnation != aCatalog.binding().Catalog.Incarnation
			|| aRecord.Cache != aCatalog.binding().Authority.Cache) {
			mark_maintenance_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (aRecord.Phase == epoch_catalog_operation_phase::complete
			|| aRecord.Phase == epoch_catalog_operation_phase::aborted) {
			return aCatalog.binding().Catalog;
		}

		const auto beforePointOfNoReturn = !point_of_no_cancellation(aRecord.Phase);
		if (beforePointOfNoReturn) {
			if (aRecord.Phase != epoch_catalog_operation_phase::abort_requested) {
				const auto requested = advance_maintenance(aCatalog,
														   aRecord,
														   aCatalog.binding().Catalog,
														   epoch_catalog_operation_phase::abort_requested,
														   aStop);
				if (!requested) {
					return failCatalog(requested.error());
				}
				aRecord = *requested;
			}
			const auto aborted = advance_maintenance(aCatalog,
													 aRecord,
													 aCatalog.binding().Catalog,
													 epoch_catalog_operation_phase::aborted,
													 aStop);
			if (!aborted) {
				return failCatalog(aborted.error());
			}
			return aCatalog.binding().Catalog;
		}

		const auto expectedCache = git_cache_open_request{
			.ApplicationSession = aHandoff.ApplicationSession,
			.Authority          = aCatalog.binding().Authority,
			.ObjectFormat       = aHandoff.ObjectFormat,
		};
		if (aCache && aCache->request() != expectedCache) {
			aCache.reset();
		}
		const auto durableToken  = aRecord.Operation.bytes();
		const auto postPointStop = std::stop_token{ };

		if (aRecord.Kind == epoch_catalog_operation_kind::prune) {
			if (aRecord.Phase
				== epoch_catalog_operation_phase::ref_delete_in_progress) {
				if (!aCache) {
					auto opened = aCaches.open(expectedCache, postPointStop);
					if (!opened) {
						return failCache(opened.error());
					}
					if (!*opened || !application::valid((*opened)->request())
						|| (*opened)->request() != expectedCache) {
						mark_maintenance_repair(aCatalog, aRecord);
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
					aCache = std::move(*opened);
				}
				std::vector<git_retained_ref_pair> absent;
				absent.reserve(aRecord.Epochs.size());
				for (const auto& fact : aRecord.Epochs) {
					const auto pruned = aCache->prune_retained(
						git_ref_prune_authorization{
							.CatalogIncarnation      = aRecord.Incarnation,
							.ExpectedCatalogRevision = aCatalog.binding().Catalog.Revision,
							.Expected                = fact.Retained,
							.DurableIntentToken      = durableToken,
						},
						postPointStop);
					if (!pruned) {
						return failCache(pruned.error());
					}
					absent.push_back(fact.Retained);
				}
				const auto before    = aCatalog.binding().Catalog;
				const auto finalized = aCatalog.finalize_prune(
					finalize_prune_request{
						.Transition = operation_transition_request{
							.Catalog   = before,
							.Operation = aRecord.Operation,
							.Kind      = aRecord.Kind,
							.Expected  = aRecord.Phase,
							.Next      = epoch_catalog_operation_phase::
								visible_cleanup_pending,
							.Evidence = epoch_catalog_transition_evidence::
								exact_refs_absent,
							.ExpectedOperationRevision = aRecord.OperationRevision,
						},
						.ObservedAbsent = std::move(absent),
					},
					postPointStop);
				if (!finalized) {
					return failCatalog(finalized.error());
				}
				if (!exact_catalog_commit(*finalized, before, std::nullopt)) {
					mark_maintenance_repair(aCatalog, aRecord);
					return std::unexpected{
						core::epoch_failure::repair_required
					};
				}
				aRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
				aRecord.OperationRevision = epoch_catalog_operation_revision{
					aRecord.OperationRevision.value() + 1
				};
			}
			if (aRecord.Phase
				!= epoch_catalog_operation_phase::visible_cleanup_pending) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto completed = advance_maintenance(aCatalog,
													   aRecord,
													   aCatalog.binding().Catalog,
													   epoch_catalog_operation_phase::complete,
													   postPointStop);
			if (!completed) {
				return failCatalog(completed.error());
			}
			return aCatalog.binding().Catalog;
		}

		if (aRecord.Kind != epoch_catalog_operation_kind::cache_delete) {
			mark_maintenance_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		const auto deletePhysicalCache =
			[&]() -> std::expected<void, core::epoch_failure> {
			aCache.reset();
			const auto deleted = aCaches.delete_cache(expectedCache,
													  git_cache_delete_authorization{
														  .ApplicationSession = aHandoff.ApplicationSession,
														  .Cache              = aRecord.Cache,
														  .DurableIntentToken = durableToken,
													  },
													  postPointStop);
			if (!deleted) {
				const auto mapped = cache_failure(deleted.error());
				if (mapped == core::epoch_failure::repair_required) {
					mark_maintenance_repair(aCatalog, aRecord);
				}
				return std::unexpected{ mapped };
			}
			return { };
		};
		if (aRecord.Phase
			== epoch_catalog_operation_phase::quarantine_rename_in_progress) {
			if (const auto deleted = deletePhysicalCache(); !deleted) {
				return std::unexpected{ deleted.error() };
			}
			const auto quarantined = advance_maintenance(aCatalog,
														 aRecord,
														 aCatalog.binding().Catalog,
														 epoch_catalog_operation_phase::quarantined,
														 postPointStop);
			if (!quarantined) {
				return failCatalog(quarantined.error());
			}
			aRecord = *quarantined;
		}
		if (aRecord.Phase == epoch_catalog_operation_phase::quarantined) {
			const auto before    = aCatalog.binding();
			const auto finalized = aCatalog.finalize_cache_delete(
				finalize_cache_delete_request{
					.Transition = operation_transition_request{
						.Catalog   = before.Catalog,
						.Operation = aRecord.Operation,
						.Kind      = aRecord.Kind,
						.Expected  = aRecord.Phase,
						.Next      = epoch_catalog_operation_phase::
							catalog_tombstoned_cleanup_pending,
						.Evidence = epoch_catalog_transition_evidence::
							catalog_transaction,
						.ExpectedOperationRevision = aRecord.OperationRevision,
					},
				},
				postPointStop);
			if (!finalized) {
				return failCatalog(finalized.error());
			}
			const auto& after = aCatalog.binding();
			if (!application::valid(after) || after.Authority != before.Authority
				|| after.Catalog.Incarnation != before.Catalog.Incarnation
				|| before.Catalog.Revision.value()
					>= core::maximum_epoch_catalog_revision
				|| after.Catalog.Revision.value()
					!= before.Catalog.Revision.value() + 1) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			aRecord.Phase = epoch_catalog_operation_phase::
				catalog_tombstoned_cleanup_pending;
			aRecord.OperationRevision = epoch_catalog_operation_revision{
				aRecord.OperationRevision.value() + 1
			};
		}
		if (aRecord.Phase
			!= epoch_catalog_operation_phase::
				catalog_tombstoned_cleanup_pending) {
			mark_maintenance_repair(aCatalog, aRecord);
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (const auto deleted = deletePhysicalCache(); !deleted) {
			return std::unexpected{ deleted.error() };
		}
		const auto completed = advance_maintenance(aCatalog,
												   aRecord,
												   aCatalog.binding().Catalog,
												   epoch_catalog_operation_phase::complete,
												   postPointStop);
		if (!completed) {
			return failCatalog(completed.error());
		}
		return aCatalog.binding().Catalog;
	}

	auto epoch_application_host::implementation::recover_catalog(epoch_catalog_session&               aCatalog,
																 git_cache_port&                      aCaches,
																 epoch_entropy_port&                  aEntropy,
																 const accepted_pull_request_handoff& aHandoff,
																 std::unique_ptr<git_cache_session>&  aCache,
																 const std::stop_token                aStop)
		-> std::expected<void, core::epoch_failure>
	{
		std::optional<core::epoch_operation_id> after;
		std::size_t                             recoveredCount = 0U;
		while (true) {
			const auto page = aCatalog.recovery_page(
				after, maximum_epoch_recovery_page_size, aStop);
			if (!page) {
				return std::unexpected{ catalog_failure(page.error()) };
			}
			if (
				!application::valid(*page)
				|| page->Incarnation
					!= aCatalog.binding().Catalog.Incarnation
				|| (page->Continuation
					&& page->Operations.empty())
				|| (after && page->Operations.empty())
				|| (after && page->Continuation
					&& !(*after < *page->Continuation))
				|| page->Operations.size()
					> maximum_recovery_operations - recoveredCount) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (after && !page->Operations.empty()) {
				const auto first = recovery_operation_identity(
					page->Operations.front());
				// SQLite's continuation is the first omitted identity, so
				// the next page includes an identity equal to `after`.
				if (first != *after) {
					return std::unexpected{
						core::epoch_failure::repair_required
					};
				}
			}
			recoveredCount += page->Operations.size();
			for (const auto& operation : page->Operations) {
				const auto* append = std::get_if<append_operation_record>(&operation);
				if (append == nullptr) {
					const auto* maintenance = std::get_if<maintenance_operation_record>(&operation);
					if (maintenance == nullptr) {
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
					const auto recovered = recover_maintenance(aCatalog,
															   aCaches,
															   aHandoff,
															   aCache,
															   *maintenance,
															   aStop);
					if (!recovered) {
						return std::unexpected{ recovered.error() };
					}
					continue;
				}
				if (!aCache) {
					const auto expected = git_cache_open_request{
						.ApplicationSession = aHandoff.ApplicationSession,
						.Authority          = aCatalog.binding().Authority,
						.ObjectFormat       = aHandoff.ObjectFormat,
					};
					auto opened = aCaches.open(expected, aStop);
					if (!opened) {
						return std::unexpected{ cache_failure(opened.error()) };
					}
					if (!*opened) {
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
					if (
						!application::valid((*opened)->request())
						|| (*opened)->request() != expected) {
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
					aCache = std::move(*opened);
				}
				const auto recovered = recover_append(
					aCatalog, *aCache, aEntropy, *append, aStop);
				if (!recovered) {
					return recovered;
				}
			}
			if (!page->Continuation) {
				return { };
			}
			after = page->Continuation;
		}
	}

	auto epoch_application_host::implementation::pending_append(
		epoch_catalog_session&         aCatalog,
		const core::epoch_operation_id aOperation)
		-> std::expected<
			std::optional<append_operation_record>,
			core::epoch_failure>
	{
		auto page = aCatalog.recovery_page(aOperation, 1U, { });
		if (!page) {
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (
			!application::valid(*page)
			|| page->Incarnation
				!= aCatalog.binding().Catalog.Incarnation
			|| page->Operations.size() > 1U
			|| (page->Continuation && page->Operations.empty())) {
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (page->Operations.empty()) {
			return std::optional<append_operation_record>{ };
		}
		const auto* append = std::get_if<append_operation_record>(&page->Operations.front());
		const auto  found  = recovery_operation_identity(
			page->Operations.front());
		if (found < aOperation) {
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		if (found != aOperation) {
			return std::optional<append_operation_record>{ };
		}
		if (append == nullptr) {
			return std::unexpected{ core::epoch_failure::repair_required };
		}
		return std::optional{ *append };
	}

	auto epoch_application_host::implementation::abort_preparation(
		authority_context&  aContext,
		prepared_operation& aOperation, git_cache_port& aCaches) -> bool
	{
		if (aOperation.Kind == prepared_kind::observation_only) {
			return true;
		}
		if (!aOperation.Record) {
			return false;
		}
		auto record = *aOperation.Record;
		if (record.Phase == epoch_catalog_operation_phase::aborted
			|| record.Phase == epoch_catalog_operation_phase::complete) {
			return true;
		}
		if (record.Phase == epoch_catalog_operation_phase::repair_required
			|| point_of_no_cancellation(record.Phase)) {
			return false;
		}
		const auto markRepair = [&aContext, &record, &aOperation]() {
			const auto repaired = advance_append(
				aContext,
				record,
				record.Context.Catalog,
				epoch_catalog_operation_phase::repair_required,
				{ });
			if (repaired) {
				record            = *repaired;
				aOperation.Record = record;
			}
		};
		if (record.Phase != epoch_catalog_operation_phase::abort_requested) {
			const auto requested = advance_append(
				aContext,
				record,
				record.Context.Catalog,
				epoch_catalog_operation_phase::abort_requested,
				{ });
			if (!requested) {
				markRepair();
				return false;
			}
			record = *requested;
		}
		if (aContext.Actor->Cache) {
			const auto cache = open_actor_cache(*aContext.Actor, aCaches,
												git_cache_open_request{ .ApplicationSession = aContext.Handoff.ApplicationSession,
																		.Authority          = aContext.Authority,
																		.ObjectFormat       = aContext.Handoff.ObjectFormat },
												{ });
			if (!cache) {
				markRepair();
				return false;
			}
			const auto request = recovery_request(record);
			if (!request) {
				markRepair();
				return false;
			}
			auto aborted = aContext.Actor->Cache->abort_recovered_operation(
				*request, { });
			if (!aborted) {
				aborted = aContext.Actor->Cache->abort_recovered_operation(
					*request, { });
			}
			if (!aborted) {
				markRepair();
				return false;
			}
		}
		const auto completed = advance_append(
			aContext,
			record,
			record.Context.Catalog,
			epoch_catalog_operation_phase::aborted,
			{ });
		if (!completed) {
			markRepair();
			return false;
		}
		aOperation.Record = *completed;
		return true;
	}

	auto epoch_application_host::implementation::discard_context_preparation(
		std::shared_ptr<authority_context>            aContext,
		const std::optional<core::epoch_operation_id> aExpected,
		std::shared_ptr<job_control>                  aAdmittedControl)
		-> bool
	{
		if (!aContext->requires_cache_cleanup(aExpected)) {
			if (aExpected) {
				static_cast<void>(aContext->take(*aExpected));
			} else {
				aContext->Preparations.clear();
			}
			if (aAdmittedControl) {
				finish_job(aAdmittedControl);
			}
			return true;
		}
		auto        actor  = aContext->Actor;
		auto* const caches = &Caches;
		return submit_effect(
			[context = std::move(aContext), aExpected, caches](
				const std::stop_token aStop)
				-> worker_completion {
				const auto requiresCache = std::ranges::any_of(
					context->Preparations,
					[&](const auto& aOperation) {
						return aOperation.Kind == prepared_kind::append
							&& (!aExpected
								|| aOperation.Operation == *aExpected);
					});
				if (requiresCache) {
					const auto cache = open_actor_cache(
						*context->Actor,
						*caches,
						git_cache_open_request{
							.ApplicationSession = context->Handoff.ApplicationSession,
							.Authority          = context->Authority,
							.ObjectFormat       = context->Handoff.ObjectFormat,
						},
						aStop);
					if (!cache) {
						return [](implementation& aHost) {
							aHost.publish_diagnostic(
								core::epoch_reason_code::unavailable);
						};
					}
				}
				if (aExpected) {
					auto operation = context->take(*aExpected);
					if (operation) {
						static_cast<void>(
							abort_preparation(*context, *operation, *caches));
					}
				} else {
					while (!context->Preparations.empty()) {
						auto operation = context->take(
							context->Preparations.back().Operation);
						if (operation) {
							static_cast<void>(
								abort_preparation(*context, *operation, *caches));
						}
					}
				}
				return [](implementation&) { };
			},
			[](implementation& aHost) {
				aHost.publish_diagnostic(
					core::epoch_reason_code::unavailable);
			},
			[](implementation& aHost, const core::epoch_failure aFailure) {
				aHost.publish_diagnostic(core::reason_for(aFailure));
			},
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::move(actor),
			std::move(aAdmittedControl));
	}

	void epoch_application_host::implementation::publish_diagnostic(const core::epoch_reason_code aReason) noexcept
	{
		try {
			auto update       = epoch_presentation_update{ };
			update.Transition = epoch_transition_projection{
				.Changed     = core::epoch_changed_domain::diagnostics,
				.Diagnostics = { core::epoch_diagnostic{ .Code = aReason } },
			};
			publish(update);
		} catch (...) {
			ProjectionDeliveryFailed = true;
		}
	}

	void epoch_application_host::implementation::publish(const epoch_presentation_update& aUpdate) noexcept
	{
		if (!Legacy && !Consumer->current()) {
			return;
		}
		try {
			Projections.publish(aUpdate);
		} catch (...) {
			ProjectionDeliveryFailed = true;
		}
	}

	void epoch_application_host::implementation::publish_transition(const core::epoch_transition_output& aOutput)
	{
		if (aOutput.Changed == core::epoch_changed_domain::none
			&& aOutput.Diagnostics.empty()) {
			return;
		}
		epoch_presentation_update   update;
		epoch_transition_projection transition{
			.Changed     = aOutput.Changed,
			.Selection   = aOutput.Selection,
			.Status      = aOutput.Status,
			.Commands    = aOutput.Commands,
			.Diagnostics = aOutput.Diagnostics,
		};
		if (aOutput.Authority) {
			transition.Authority = epoch_authority_projection{
				.Authority  = aOutput.Authority->Authority,
				.Generation = aOutput.Authority->Generation,
			};
			if (aOutput.Authority->Revision) {
				transition.Authority->Revision = projected_revision(*aOutput.Authority->Revision);
			}
		}
		if (aOutput.Comparison) {
			auto reason = core::epoch_reason_code::none;
			if (!aOutput.Comparison->Pair && !aOutput.Diagnostics.empty()) {
				reason = aOutput.Diagnostics.back().Code;
			}
			transition.Comparison = epoch_comparison_projection{
				.Selection        = aOutput.Comparison->Selection,
				.Pair             = aOutput.Comparison->Pair,
				.Generation       = State.ComparisonGeneration,
				.Revision         = aOutput.Comparison->Revision,
				.AvailableForDiff = aOutput.Comparison->AvailableForDiff,
				.Reason           = reason,
				.PullRequestBase  = aOutput.Comparison->PullRequestBase,
			};
		}
		update.Transition = std::move(transition);

		if (aOutput.Catalog || aOutput.AcceptedEpoch || aOutput.History) {
			update.Catalog = epoch_catalog_projection{ };
			if (aOutput.Catalog) {
				update.Catalog->Catalog                   = aOutput.Catalog->Catalog;
				update.Catalog->Replace                   = !aOutput.Catalog->Catalog.has_value();
				update.Catalog->AvailabilityChangedEpochs = aOutput.Catalog->AvailabilityChangedEpochs;
				update.Catalog->AvailabilityChangedTo     = aOutput.Catalog->AvailabilityChangedTo;
				update.Catalog->AllEpochAvailability      = aOutput.Catalog->AllEpochAvailability;
			}
			if (aOutput.AcceptedEpoch) {
				update.Catalog->Catalog = core::epoch_catalog_identity{
					.Incarnation = aOutput.AcceptedEpoch->Epoch.Id.Incarnation,
					.Revision    = aOutput.AcceptedEpoch->CatalogRevision,
				};
				const bool historyWindowInvalidated = aOutput.History
					&& aOutput.History->Phase
						== core::epoch_history_page_phase::invalidated;
				if (!historyWindowInvalidated) {
					update.Catalog->Changes.push_back(
						epoch_row_change_projection{
							.Kind = epoch_row_change_kind::insert,
							.Id   = aOutput.AcceptedEpoch->Epoch.Id,
							.Row  = projected_row(aOutput.AcceptedEpoch->Epoch),
						});
				}
			}
			if (aOutput.History) {
				if (!aOutput.Catalog) {
					update.Catalog->Catalog = aOutput.History->Catalog;
				}
				auto page = epoch_history_page_projection{
					.Phase           = aOutput.History->Phase,
					.Catalog         = aOutput.History->Catalog,
					.Navigation      = aOutput.History->Navigation,
					.RequestedCursor = aOutput.History->RequestedCursor,
					.Limit           = aOutput.History->Limit,
					.Continuation    = aOutput.History->Continuation,
					.Reason          = aOutput.History->Reason,
				};
				page.Rows.reserve(aOutput.History->Epochs.size());
				for (const auto& epoch : aOutput.History->Epochs) {
					page.Rows.push_back(projected_row(epoch));
				}
				update.Catalog->HistoryPage = std::move(page);
			}
		}
		publish(update);
	}

	void epoch_application_host::implementation::publish_occurrences(
		const core::epoch_catalog_identity               aCatalog,
		const core::epoch_occurrence_history&            aHistory,
		const std::optional<core::review_epoch_summary>& aCurrent)
	{
		auto update    = epoch_presentation_update{ };
		update.Catalog = epoch_catalog_projection{
			.Catalog               = aCatalog,
			.OccurrenceHistory     = aHistory,
			.AllowOccurrenceImport = Active
				&& object_loading_for(Active->Handoff.Shape) == pull_request_object_loading::selected_objects
				&& Active->Handoff.SmartGit
				&& Active->Handoff.SmartGit->supports_sparse_commit_trees(),
		};
		// The accepted selection precedes its durable live append. Never expose
		// comparisons derived under the prior base while that append is pending.
		if (Active && core::historical_comparison_selection_changed(Active->Revision, aCurrent)) {
			core::invalidate_derived_comparison_bindings(*update.Catalog->OccurrenceHistory);
		}
		if (aCurrent) {
			update.Catalog->CurrentEpoch = projected_row(*aCurrent);
		}
		publish(update);
	}

	auto epoch_application_host::implementation::dispatch_core(
		const core::epoch_event& aEvent,
		const std::function<void(const core::epoch_transition_output&)>&
			aBeforeRequests)
		-> core::epoch_transition_output
	{
		auto output = core::transition(State, aEvent);
		publish_transition(output);
		if (aBeforeRequests) {
			aBeforeRequests(output);
		}
		for (const auto& request : output.Requests) {
			handle_request(request);
		}
		return output;
	}

	void epoch_application_host::implementation::handle_request(const core::epoch_host_request& aRequest)
	{
		std::visit(
			[this](const auto& aValue) {
				using request_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<
								  request_type,
								  core::invalidate_epoch_authority_request>) {
					cancel_authority_jobs();
				} else if constexpr (std::is_same_v<request_type,
													core::import_epoch_occurrence_request>) {
					auto context = exact_context(aValue.Context.Authority, aValue.Context.Revision);
					if (!context || context->Handoff.ApplicationSession != aValue.Context.ApplicationSession
						|| State.Catalog != aValue.Context.ExpectedCatalog
						|| State.EpochGeneration != aValue.Context.Generation) {
						dispatch_core(core::epoch_occurrence_import_failed_event{
							.Context = aValue.Context,
							.Failure = core::epoch_failure::catalog_conflict,
						});
					} else {
						start_historical_import(context, aValue.Context.Request,
												aValue.Context.Generation, aValue.Context);
					}
				} else if constexpr (std::is_same_v<
										 request_type,
										 core::cancel_epoch_work_request>) {
					cancel_work(aValue);
				} else if constexpr (std::is_same_v<
										 request_type,
										 core::load_epoch_catalog_request>) {
					start_catalog_load(aValue);
				} else if constexpr (std::is_same_v<
										 request_type,
										 core::prepare_epoch_revision_request>) {
					start_revision_preparation(aValue);
				} else if constexpr (std::is_same_v<
										 request_type,
										 core::commit_epoch_catalog_request>) {
					start_catalog_commit(aValue);
				} else if constexpr (std::is_same_v<
										 request_type,
										 core::discard_epoch_preparation_request>) {
					discard_preparation(aValue);
				} else if constexpr (std::is_same_v<
										 request_type,
										 core::load_epoch_history_page_request>) {
					start_history_page(aValue);
				} else if constexpr (std::is_same_v<request_type,
													core::perform_epoch_maintenance_request>) {
					start_epoch_maintenance(aValue);
				} else {
					start_comparison_resolution(aValue);
				}
			},
			aRequest);
	}

	auto epoch_application_host::implementation::exact_context(
		const core::pull_request_authority&         aAuthority,
		const core::accepted_pull_request_revision& aRevision) const
		-> std::shared_ptr<authority_context>
	{
		if (Active && Active->Authority == aAuthority
			&& Active->Revision == aRevision) {
			return Active;
		}
		return { };
	}

	auto epoch_application_host::implementation::actor_for(
		const cache_actor_key& aKey)
		-> std::shared_ptr<authority_effect_actor>
	{
		std::scoped_lock lock{ CacheActorsMutex };
		const auto       found = std::ranges::find(
			CacheActors, aKey, &decltype(CacheActors)::value_type::first);
		if (found != CacheActors.end()) {
			return found->second;
		}
		if (CacheActors.size() >= maximum_cache_actors) {
			return { };
		}
		auto shared = Consumer->acquire(aKey);
		if (!shared) {
			return { };
		}
		auto actor = std::make_shared<authority_effect_actor>(std::move(*shared));
		CacheActors.emplace_back(aKey, actor);
		return actor;
	}

	/// \brief Mints and authorizes one selected-diff context.
	///
	/// \details One body, two callers, because the accepted-state checks
	/// they need are the same checks. With no binding this is the
	/// workbench's own context: the accepted comparison, authorized into
	/// the slot every resolve is measured against. With a binding it is
	/// BPR-0532's re-alignment context: the same context with its two
	/// anchors moved to the pair the binding names, authorized into a
	/// second slot so the workbench keeps the one it is using.
	///
	/// The host mints it rather than validating one a caller supplies, so
	/// no caller can name a comparison the host did not decide to serve;
	/// and what it decides to serve is bounded to this review — the pair
	/// must name the same host, repository, and pull request, and the
	/// capability is scoped to the cache actor that only ever holds this
	/// pull request's objects. A pair that review's own cache cannot
	/// answer for refuses as a missing endpoint, which is the answer a
	/// re-alignment of a revision whose objects are gone should get.
	auto epoch_application_host::implementation::authorized_selected_diff_context(
		const workbench_pull_request_authority&            aAuthority,
		const std::optional<core::agent_revision_binding>& aBinding, const bool aGrant) const
		-> std::optional<workbench_selected_diff_context>
	{
		if (!Consumer->current() || !application::valid(aAuthority)
			|| !State.Authority.has_value()
			|| !State.AcceptedRevision.has_value()
			|| !State.AcceptedComparison.has_value()
			|| !core::valid(State.AcceptedComparisonRevision)
			// [[BPR-1560]]. Whole PR without a merge base GitHub named has
			// a base the core marked unavailable; its recorded base is in
			// the cache, but a comparison from it is not GitHub's.
			|| (core::retained_recorded_base(State.AcceptedBasis)
				&& !core::available_for_diff(*State.AcceptedComparison))) {
			return std::nullopt;
		}
		const auto& authority = *State.Authority;
		const auto& revision  = *State.AcceptedRevision;
		if (aAuthority.ApplicationSession != State.ApplicationSession
			|| aAuthority.Profile != authority.Profile
			|| aAuthority.ProfileRevision != authority.ProfileRevision
			|| aAuthority.Host != authority.Host
			|| aAuthority.HostAuthority != authority.HostAuthority
			|| aAuthority.Account != authority.Account
			|| aAuthority.Viewer != authority.Viewer
			|| aAuthority.Repository != authority.Repository
			|| aAuthority.PullRequest != authority.PullRequest
			|| aAuthority.Number != authority.Number
			|| aAuthority.Snapshot != revision.Source.Snapshot
			|| aAuthority.IdentityGeneration
				!= revision.Source.IdentityGeneration
			|| aAuthority.PullRequestGeneration
				!= revision.Source.PullRequestGeneration) {
			return std::nullopt;
		}
		const auto options = core::normalize_diff_options(core::diff_options{ });
		if (!options.has_value()) {
			return std::nullopt;
		}
		const auto digest = core::options_digest(*options);
		if (!digest.has_value()) {
			return std::nullopt;
		}
		auto context = workbench_selected_diff_context{
			.ApplicationSession = State.ApplicationSession,
			.Authority          = authority,
			.Comparison         = *State.AcceptedComparison,
			.ComparisonRevision = State.AcceptedComparisonRevision,
			.Options            = *options,
			.OptionsDigest      = *digest,
			.Algorithm          = core::supported_diff_algorithm_revision,
		};
		if (aBinding.has_value()) {
			if (aBinding->Host != authority.Host
				|| aBinding->Repository != authority.Repository
				|| aBinding->PullRequest != authority.PullRequest) {
				return std::nullopt;
			}
			// The whole retargeting, and deliberately nothing else: the
			// two anchors name the pair the binding was started against,
			// and the authority, options, and algorithm stay what they
			// are, so the key this produces is the key that revision's
			// alignment already had.
			context.Comparison.Left.Identity.Epoch  = aBinding->LeftEpoch;
			context.Comparison.Left.Commit          = aBinding->BaseOid;
			context.Comparison.Right.Identity.Epoch = aBinding->RightEpoch;
			context.Comparison.Right.Commit         = aBinding->HeadOid;
		}
		if (!Active || Active->Authority != authority
			|| Active->Revision != revision) {
			return std::nullopt;
		}
		const auto granted = selected_diff_authority{
			.Workbench = aAuthority,
			.Diff      = context,
		};
		if (aGrant) {
			if (aBinding.has_value()) {
				authorize_realignment(Active->Actor, granted);
			} else {
				authorize_selected_diff(Active->Actor, granted);
			}
		}
		return context;
	}

	auto epoch_application_host::implementation::selected_diff_context(
		const workbench_pull_request_authority& aAuthority) const
		-> std::optional<workbench_selected_diff_context>
	{
		return authorized_selected_diff_context(aAuthority, std::nullopt);
	}

	auto epoch_application_host::implementation::retained_source(authority_effect_actor& aActor, git_cache_port& aCaches, epoch_catalog_port& aCatalogs,
																 const cached_review_source_request& aRequest, std::stop_token aStop) -> cached_review_source_projection
	{
		auto result = cached_review_source_projection{ .Path    = aRequest.File.Path,
													   .BaseOid = std::string{ core::canonical_hex(aRequest.Base).view() },
													   .HeadOid = std::string{ core::canonical_hex(aRequest.Head).view() } };
		if (aStop.stop_requested()) {
			return result;
		}
		const auto opened = aCatalogs.open(aRequest.Catalog, aStop);
		if (!opened || !*opened) {
			return result;
		}
		const auto snapshot = (*opened)->snapshot(aStop);
		if (!snapshot || !valid(snapshot->Binding) || snapshot->Binding != (*opened)->binding() || !snapshot->LatestLive
			|| snapshot->LatestLive->Id.Authority != snapshot->Binding.Authority.Scope
			|| snapshot->LatestLive->Id.Incarnation != snapshot->Binding.Catalog.Incarnation
			|| snapshot->LatestLive->Base != aRequest.Base || snapshot->LatestLive->Head != aRequest.Head
			|| snapshot->LatestLive->BaseAvailability != core::anchor_availability::available
			|| snapshot->LatestLive->HeadAvailability != core::anchor_availability::available
			|| snapshot->RetentionRehydration != core::retention_rehydration_requirement::not_required) {
			return result;
		}
		const auto& authority = snapshot->Binding.Authority;
		const auto& expected  = aRequest.Catalog.Authority;
		if (authority.Profile != expected.Profile || authority.ProfileRevision != expected.ProfileRevision
			|| authority.Host != expected.Host || authority.HostAuthority != expected.HostAuthority
			|| authority.Account != expected.Account || authority.Viewer != expected.Viewer
			|| authority.Repository != expected.Repository || authority.PullRequest != expected.PullRequest
			|| authority.Number != expected.Number || authority.Mode != expected.Mode || snapshot->Binding.ObjectFormat != expected.ObjectFormat) {
			return result;
		}
		const auto cacheRequest = git_cache_open_request{ .ApplicationSession = aRequest.Catalog.ApplicationSession, .Authority = authority, .ObjectFormat = expected.ObjectFormat };
		// The profile actor owns one physical repository session. Never replace a
		// sibling's active session merely to read this historical pair.
		if (aActor.Cache) {
			const auto& current = aActor.Cache->request();
			if (current.ObjectFormat != expected.ObjectFormat
				|| current.Authority.Profile != authority.Profile || current.Authority.ProfileRevision != authority.ProfileRevision
				|| current.Authority.Host != authority.Host || current.Authority.HostAuthority != authority.HostAuthority
				|| current.Authority.Account != authority.Account || current.Authority.Viewer != authority.Viewer || current.Authority.Repository != authority.Repository
				|| current.Authority.Cache != authority.Cache || current.Authority.Mode != authority.Mode) {
				return result;
			}
		} else if (!open_actor_cache(aActor, aCaches, cacheRequest, aStop)) {
			return result;
		}
		result.Status   = cached_source_status::available;
		const auto read = [&](core::git_commit_oid aCommit, const std::string& aPath, std::optional<std::string>& aText) {
			const auto blob = aActor.Cache->find_blob_at_path({ .Commit = aCommit, .Path = aPath }, aStop);
			if (!blob) {
				result.Status = cached_source_status::unavailable;
				return;
			}
			if (!*blob) {
				return;
			}
			const auto bytes = aActor.Cache->materialize_blob({ .Blob = **blob, .MaximumBytes = maximum_cached_source_bytes }, aStop);
			if (!bytes || bytes->size() > maximum_cached_source_bytes) {
				result.Status = !bytes && bytes.error().Code != git_cache_failure::quota_exceeded ? cached_source_status::unavailable : cached_source_status::too_large;
				return;
			}
			if (std::ranges::find(*bytes, std::uint8_t{ 0 }) != bytes->end()) {
				result.Status = cached_source_status::binary;
				return;
			}
			aText.emplace(bytes->begin(), bytes->end());
		};
		read(aRequest.Base, aRequest.File.PreviousPath.value_or(aRequest.File.Path), result.BaseText);
		if (result.Status == cached_source_status::available) {
			read(aRequest.Head, aRequest.File.Path, result.HeadText);
		}
		if (aStop.stop_requested()) {
			result.Status = cached_source_status::unavailable;
		}
		if (result.Status != cached_source_status::available) {
			result.BaseText.reset();
			result.HeadText.reset();
		}
		return result;
	}

	auto epoch_application_host::implementation::read_cached_source(cached_review_source_request aRequest, std::stop_token aStop, cached_review_source_completion aCompletion) -> bool
	{
		if (Closing || !Consumer->current() || !aCompletion || aStop.stop_requested()
			|| !valid(aRequest.Catalog) || !aRequest.Catalog.ExistingOnly
			|| aRequest.Catalog.ApplicationSession != State.ApplicationSession
			|| aRequest.Catalog.Authority.Mode != core::github_access_mode::authenticated
			|| !aRequest.Base.valid() || !aRequest.Head.valid() || aRequest.Base.algorithm() != aRequest.Head.algorithm()
			|| aRequest.Base.algorithm() != aRequest.Catalog.Authority.ObjectFormat
			|| !core::valid_cached_catalogue({ aRequest.File })) {
			return false;
		}
		const auto& key   = aRequest.Catalog.Authority;
		auto        actor = actor_for(cache_actor_key{ .Profile = key.Profile, .ProfileRevision = key.ProfileRevision, .Host = key.Host, .HostAuthority = key.HostAuthority, .Account = key.Account, .Repository = key.Repository, .ObjectFormat = key.ObjectFormat, .Mode = key.Mode });
		if (!actor) {
			return false;
		}
		const auto unavailable = cached_review_source_projection{ .Path = aRequest.File.Path, .BaseOid = std::string{ core::canonical_hex(aRequest.Base).view() }, .HeadOid = std::string{ core::canonical_hex(aRequest.Head).view() } };
		return submit_effect([this, actor, request = std::move(aRequest), aStop, completion = aCompletion](std::stop_token aJobStop) -> worker_completion {
			std::stop_source combined;
			std::stop_callback external{aStop, [&] { combined.request_stop(); }};
			std::stop_callback job{aJobStop, [&] { combined.request_stop(); }};
			auto result = retained_source(*actor, Caches, Catalogs, request, combined.get_token());
			return [answer=std::move(result), completion](implementation&) mutable { completion(std::move(answer)); }; }, [completion = aCompletion, unavailable](implementation&) { completion(unavailable); },
							 [](implementation&, core::epoch_failure) { }, std::nullopt, std::nullopt, std::nullopt, std::nullopt, actor);
	}
}
