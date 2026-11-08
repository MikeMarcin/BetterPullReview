#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "catalog_database.hpp"
#include "epoch_catalog_support.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <stop_token>
#include <string_view>
#include <utility>

namespace bpr::adapters::sqlite::epoch_catalog_detail {
	namespace {
		[[nodiscard]] auto persist_review(
			::sqlite3*                                   aDatabase,
			const core::epoch_catalog_incarnation_id&    aIncarnation,
			const core::pull_request_authority_scope_id& aAuthority,
			const core::catalog_review_association&      aReview,
			const core::provider_snapshot_id             aSnapshot,
			const std::optional<core::epoch_ordinal>     aLinked,
			std::stop_token                              aStop) -> result<void>
		{
			auto upsert = prepare(
				aDatabase,
				"INSERT INTO review_association(incarnation, pr_scope, association, "
				"review_id, actor_id, review_order, review_state, commit_algorithm, "
				"commit_digest, availability, linked_ordinal, source_snapshot) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12) "
				"ON CONFLICT(incarnation, pr_scope, association) DO UPDATE SET "
				"review_id = excluded.review_id, actor_id = excluded.actor_id, "
				"review_order = excluded.review_order, "
				"review_state = excluded.review_state, "
				"commit_algorithm = excluded.commit_algorithm, "
				"commit_digest = excluded.commit_digest, "
				"availability = excluded.availability, "
				"linked_ordinal = excluded.linked_ordinal, "
				"source_snapshot = excluded.source_snapshot");
			if (!upsert || !bind_fixed(upsert->Value, 1, aIncarnation)
				|| !bind_fixed(upsert->Value, 2, aAuthority)
				|| !bind_fixed(upsert->Value, 3, aReview.Association)
				|| !bind_u64(upsert->Value, 4, aReview.Review.value())
				|| !bind_u64(upsert->Value, 5, aReview.Actor.value())
				|| ::sqlite3_bind_int(upsert->Value, 6, aReview.Order.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   upsert->Value, 7, stored_enum(aReview.State))
					!= SQLITE_OK
				|| !bind_oid(upsert->Value, 8, 9, aReview.Commit)
				|| ::sqlite3_bind_int(
					   upsert->Value, 10, stored_enum(aReview.Availability))
					!= SQLITE_OK
				|| (aLinked
						? ::sqlite3_bind_int64(
							  upsert->Value, 11, aLinked->value())
						: ::sqlite3_bind_null(upsert->Value, 11))
					!= SQLITE_OK
				|| !bind_u64(upsert->Value, 12, aSnapshot.value())
				|| ::sqlite3_step(upsert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(
					aDatabase, aStop, failure::operation_conflict));
			}
			auto pointer = prepare(
				aDatabase,
				"INSERT INTO current_viewer_review(incarnation, pr_scope, "
				"association, accepted_snapshot) VALUES(?1, ?2, ?3, ?4) "
				"ON CONFLICT(incarnation, pr_scope) DO UPDATE SET "
				"association = excluded.association, "
				"accepted_snapshot = excluded.accepted_snapshot");
			if (!pointer || !bind_fixed(pointer->Value, 1, aIncarnation)
				|| !bind_fixed(pointer->Value, 2, aAuthority)
				|| !bind_fixed(pointer->Value, 3, aReview.Association)
				|| !bind_u64(pointer->Value, 4, aSnapshot.value())
				|| ::sqlite3_step(pointer->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}
			return { };
		}
	}

	auto sqlite_epoch_catalog_session::finalize_append(
		const app::finalize_append_request& aRequest,
		std::stop_token                     aStop) -> result<app::catalog_commit_result>
	{
		if (!app::valid(aRequest)
			|| aRequest.Catalog.Incarnation != Binding.Catalog.Incarnation
			|| aRequest.Mutation.Epoch.Id.Authority != Binding.Authority.Scope
			|| aRequest.Mutation.Epoch.Base.algorithm() != Binding.ObjectFormat) {
			return std::unexpected(failure::invalid_request);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, { } };
		transaction      write{ database, { } };
		if (auto begun = write.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto operation = load_append(aRequest.Operation);
		if (!operation) {
			return std::unexpected(operation.error());
		}
		if (
			operation->Phase
				== app::epoch_catalog_operation_phase::visible_cleanup_pending
			|| operation->Phase == app::epoch_catalog_operation_phase::complete) {
			auto scope = load_pull_request_scope_state(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope);
			auto epoch = load_epoch_summary(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				aRequest.Mutation.Epoch.Id.Ordinal);
			if (!scope || scope->Cache != Binding.Authority.Cache
				|| scope->Lifecycle != 1 || scope->RepositoryLifecycle != 1
				|| !advanced_once(aRequest.Catalog.Revision, scope->Revision)
				|| !finalized_operation_revision_matches(
					operation->Phase,
					aRequest.ExpectedOperationRevision,
					operation->OperationRevision,
					app::epoch_catalog_operation_phase::
						visible_cleanup_pending)
				|| operation->Context.Catalog != aRequest.Catalog
				|| operation->HistoricalOccurrence != aRequest.HistoricalOccurrence
				|| operation->Candidate != aRequest.Mutation || !epoch
				|| *epoch != aRequest.Mutation.Epoch) {
				return std::unexpected(failure::operation_conflict);
			}
			if (aRequest.Mutation.Epoch.Cause.Origin
				!= core::epoch_origin::explicit_historical_import) {
				auto retention = prepare(
					database,
					"SELECT retention_rehydration_required "
					"FROM pull_request_scope WHERE incarnation = ?1 "
					"AND pr_scope = ?2");
				if (!retention
					|| !bind_fixed(
						retention->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(
						retention->Value, 2, Binding.Authority.Scope)
					|| ::sqlite3_step(retention->Value) != SQLITE_ROW
					|| ::sqlite3_column_int(retention->Value, 0) != 0
					|| ::sqlite3_step(retention->Value) != SQLITE_DONE) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			if (aRequest.Mutation.Review) {
				auto canonical = canonicalize_review(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					*aRequest.Mutation.Review);
				if (!canonical) {
					return std::unexpected(canonical.error());
				}
				auto review = load_review_association(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					Binding.Authority.Viewer,
					aRequest.Mutation.Review->Association);
				auto current = prepare(
					database,
					"SELECT association, accepted_snapshot "
					"FROM current_viewer_review WHERE incarnation = ?1 "
					"AND pr_scope = ?2");
				if (!review || *review != canonical->Review || !current
					|| !bind_fixed(
						current->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(
						current->Value, 2, Binding.Authority.Scope)
					|| ::sqlite3_step(current->Value) != SQLITE_ROW) {
					return std::unexpected(failure::operation_conflict);
				}
				auto association = read_fixed<core::review_association_id>(
					current->Value, 0);
				auto snapshot = read_u64(current->Value, 1);
				if (!association || !snapshot
					|| *association != aRequest.Mutation.Review->Association
					|| *snapshot
						!= operation->Context.Revision.Source.Snapshot.value()
					|| ::sqlite3_step(current->Value) != SQLITE_DONE) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			Binding.Catalog.Revision = scope->Revision;
			return app::catalog_commit_result{
				.Catalog       = Binding.Catalog,
				.Changed       = true,
				.InsertedEpoch = aRequest.Mutation.Epoch.Id,
			};
		}
		if (aStop.stop_requested()
			&& !app::point_of_no_cancellation(operation->Phase)) {
			return std::unexpected(failure::cancelled);
		}
		if (aRequest.Catalog != Binding.Catalog) {
			return std::unexpected(failure::invalid_request);
		}
		if (auto active = require_active_pull_request_scope(
				database,
				Binding,
				aRequest.Catalog.Revision,
				true,
				true);
			!active) {
			return std::unexpected(active.error());
		}
		if (operation->Phase != app::epoch_catalog_operation_phase::refs_committed
			|| operation->OperationRevision != aRequest.ExpectedOperationRevision
			|| operation->HistoricalOccurrence != aRequest.HistoricalOccurrence
			|| operation->Candidate != aRequest.Mutation
			|| aRequest.Mutation.Epoch.Id.Authority != Binding.Authority.Scope) {
			return std::unexpected(failure::phase_conflict);
		}
		if (!coherent_candidate_review(
				aRequest.Mutation.Review,
				operation->Context.Revision.CurrentViewerReview,
				Binding.Authority.Viewer)) {
			return std::unexpected(failure::operation_conflict);
		}
		const auto&                        epoch = aRequest.Mutation.Epoch;
		std::optional<core::epoch_ordinal> latest;
		std::optional<core::epoch_ordinal> previous;
		auto                               pointers = prepare(
			database,
			"SELECT latest_live_ordinal, previous_distinct_head_ordinal, "
			"retention_rehydration_required FROM pull_request_scope "
			"WHERE incarnation = ?1 AND pr_scope = ?2 "
			"AND catalog_revision = ?3 AND lifecycle = 1");
		if (!pointers
			|| !bind_fixed(pointers->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(pointers->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   pointers->Value, 3, aRequest.Catalog.Revision.value())
				!= SQLITE_OK
			|| ::sqlite3_step(pointers->Value) != SQLITE_ROW) {
			return std::unexpected(failure::revision_conflict);
		}
		if (::sqlite3_column_type(pointers->Value, 0) == SQLITE_INTEGER) {
			latest = core::epoch_ordinal{
				::sqlite3_column_int64(pointers->Value, 0)
			};
		}
		if (::sqlite3_column_type(pointers->Value, 1) == SQLITE_INTEGER) {
			previous = core::epoch_ordinal{
				::sqlite3_column_int64(pointers->Value, 1)
			};
		}
		const auto retentionRequired = ::sqlite3_column_int(pointers->Value, 2);
		if ((retentionRequired != 0 && retentionRequired != 1)
			|| ::sqlite3_step(pointers->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		std::optional<core::review_epoch_summary> priorLatest;
		if (latest) {
			auto loaded = load_epoch_summary(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				*latest);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			priorLatest = *loaded;
		}
		const auto rehydrating = epoch.Cause.Origin
			== core::epoch_origin::retention_rehydration;
		if (core::historical_comparison_selection_changed(operation->Context.Revision, priorLatest)) {
			auto unlink = prepare(database, "UPDATE epoch_occurrence SET local_ordinal = NULL, derived_base = NULL "
											"WHERE incarnation = ?1 AND pr_scope = ?2 AND (derived_base IS NOT NULL OR EXISTS "
											"(SELECT 1 FROM epoch e WHERE e.incarnation = epoch_occurrence.incarnation AND e.pr_scope = epoch_occurrence.pr_scope AND e.ordinal = epoch_occurrence.local_ordinal AND e.origin = 3))");
			if (!unlink || !bind_fixed(unlink->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(unlink->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_step(unlink->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
		}
		const auto historical = epoch.Cause.Origin
			== core::epoch_origin::explicit_historical_import;
		const auto sameSemanticRevision = priorLatest
			&& priorLatest->BaseBranch == epoch.BaseBranch
			&& priorLatest->Base == epoch.Base
			&& priorLatest->Head == epoch.Head;
		const auto freshSourceObservation = priorLatest
			&& priorLatest->Source.Observation != epoch.Source.Observation;
		const auto semanticRevisionChanged = priorLatest
			&& (priorLatest->BaseBranch != epoch.BaseBranch
				|| priorLatest->Base != epoch.Base
				|| priorLatest->Head != epoch.Head);
		if (rehydrating
			&& (retentionRequired == 0 || !sameSemanticRevision
				|| !freshSourceObservation
				|| priorLatest->BaseAvailability
					!= core::anchor_availability::cache_deleted
				|| priorLatest->HeadAvailability
					!= core::anchor_availability::cache_deleted)) {
			return std::unexpected(failure::operation_conflict);
		}
		if (retentionRequired != 0 && !historical && !rehydrating
			&& !semanticRevisionChanged) {
			return std::unexpected(failure::operation_conflict);
		}
		auto insert = prepare(
			database,
			"INSERT INTO epoch(incarnation, pr_scope, ordinal, "
			"source_observation, source_snapshot, identity_generation, "
			"pull_request_generation, base_branch, "
			"observed_at, origin, base_transition, head_transition, "
			"material_relation, primary_cause, material_base_algorithm, "
			"material_base_digest, material_head_algorithm, "
			"material_head_digest, material_policy_revision) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, "
			"?13, ?14, ?15, ?16, ?17, ?18, ?19)");
		if (!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(insert->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(insert->Value, 3, epoch.Id.Ordinal.value())
				!= SQLITE_OK
			|| !bind_fixed(insert->Value, 4, epoch.Source.Observation)
			|| !bind_u64(insert->Value, 5, epoch.Source.Snapshot.value())
			|| !bind_u64(
				insert->Value, 6, epoch.Source.IdentityGeneration.value())
			|| !bind_u64(
				insert->Value, 7, epoch.Source.PullRequestGeneration.value())
			|| !bind_fixed(insert->Value, 8, epoch.BaseBranch)
			|| ::sqlite3_bind_int64(insert->Value, 9, epoch.ObservedAt.value())
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 10, stored_enum(epoch.Cause.Origin))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 11, stored_enum(epoch.Cause.Base))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 12, stored_enum(epoch.Cause.Head))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 13, stored_enum(epoch.Cause.Material))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 14, stored_enum(epoch.Cause.Primary))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (!epoch.Material
			|| !bind_oid(insert->Value, 15, 16, epoch.Material->BaseTree)
			|| !bind_oid(insert->Value, 17, 18, epoch.Material->HeadTree)
			|| !bind_u64(
				insert->Value, 19, epoch.Material->PolicyRevision.value())) {
			return std::unexpected(failure::operation_conflict);
		}
		if (::sqlite3_step(insert->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(
				database, { }, failure::operation_conflict));
		}
		auto anchor = prepare(
			database,
			"INSERT INTO epoch_anchor(incarnation, pr_scope, ordinal, endpoint, "
			"object_algorithm, object_digest, ref_format, availability) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, 1, ?7)");
		if (!anchor) {
			return std::unexpected(anchor.error());
		}
		const std::array anchorValues{
			std::pair{ epoch.Base, epoch.BaseAvailability },
			std::pair{ epoch.Head, epoch.HeadAvailability },
		};
		for (std::size_t index = 0; index < anchorValues.size(); ++index) {
			if (::sqlite3_reset(anchor->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(anchor->Value) != SQLITE_OK
				|| !bind_fixed(
					anchor->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(anchor->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   anchor->Value, 3, epoch.Id.Ordinal.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   anchor->Value, 4, static_cast<int>(index + 1U))
					!= SQLITE_OK
				|| !bind_oid(anchor->Value, 5, 6, anchorValues[index].first)
				|| ::sqlite3_bind_int(
					   anchor->Value,
					   7,
					   stored_enum(anchorValues[index].second))
					!= SQLITE_OK
				|| ::sqlite3_step(anchor->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
		}
		if (aRequest.Mutation.Review) {
			auto canonical = canonicalize_review(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				*aRequest.Mutation.Review);
			if (!canonical) {
				return std::unexpected(canonical.error());
			}
			auto persisted = persist_review(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				canonical->Review,
				operation->Context.Revision.Source.Snapshot,
				canonical->LinkedOrdinal,
				{ });
			if (!persisted) {
				return std::unexpected(persisted.error());
			}
		}
		if (aRequest.HistoricalOccurrence) {
			auto occurrence = prepare(database, "UPDATE epoch_occurrence SET local_ordinal = ?1, derived_base = ?2 "
												"WHERE incarnation = ?3 AND pr_scope = ?4 AND occurrence_id = ?5 AND local_ordinal IS NULL "
												"AND head_algorithm = ?6 AND head_digest = ?7");
			if (!occurrence || !bind_fixed(occurrence->Value, 3, Binding.Catalog.Incarnation)
				|| !bind_fixed(occurrence->Value, 4, Binding.Authority.Scope)
				|| !bind_fixed(occurrence->Value, 5, *aRequest.HistoricalOccurrence)
				|| ::sqlite3_bind_int64(occurrence->Value, 1, epoch.Id.Ordinal.value()) != SQLITE_OK
				|| !bind_bytes(occurrence->Value, 2, std::span{ epoch.Base.bytes() }.first(epoch.Base.byte_count()))
				|| !bind_oid(occurrence->Value, 6, 7, epoch.Head)
				|| ::sqlite3_step(occurrence->Value) != SQLITE_DONE || ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
		}
		if (epoch.Cause.Origin != core::epoch_origin::explicit_historical_import) {
			if (latest && priorLatest->Head != epoch.Head) {
				previous = latest;
			}
			latest = epoch.Id.Ordinal;
		}
		auto advanceScope = prepare(
			database,
			"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1, "
			"retained_epoch_count = retained_epoch_count + 1, "
			"latest_live_ordinal = ?1, previous_distinct_head_ordinal = ?2, "
			"retention_rehydration_required = CASE WHEN ?3 = 1 THEN "
			"retention_rehydration_required ELSE 0 END "
			"WHERE incarnation = ?4 AND pr_scope = ?5 "
			"AND catalog_revision = ?6 AND lifecycle = 1 "
			"AND retained_epoch_count < ?7 "
			"AND catalog_revision < 9223372036854775807");
		if (!advanceScope
			|| (latest
					? ::sqlite3_bind_int64(
						  advanceScope->Value, 1, latest->value())
					: ::sqlite3_bind_null(advanceScope->Value, 1))
				!= SQLITE_OK
			|| (previous
					? ::sqlite3_bind_int64(
						  advanceScope->Value, 2, previous->value())
					: ::sqlite3_bind_null(advanceScope->Value, 2))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(advanceScope->Value, 3, historical ? 1 : 0)
				!= SQLITE_OK
			|| !bind_fixed(
				advanceScope->Value, 4, Binding.Catalog.Incarnation)
			|| !bind_fixed(advanceScope->Value, 5, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   advanceScope->Value, 6, aRequest.Catalog.Revision.value())
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   advanceScope->Value,
				   7,
				   static_cast<std::int64_t>(
					   app::maximum_retained_epoch_count_per_pull_request))
				!= SQLITE_OK
			|| ::sqlite3_step(advanceScope->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::revision_conflict);
		}
		auto advanceOperation = prepare(
			database,
			"UPDATE operation SET phase = 8, last_evidence = 1, "
			"operation_revision = operation_revision + 1 "
			"WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
			"AND phase = 7 AND operation_revision = ?3 "
			"AND cache_scope = ?4 AND pr_scope = ?5 "
			"AND operation_revision < 9223372036854775806");
		if (!advanceOperation
			|| !bind_fixed(
				advanceOperation->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(advanceOperation->Value, 2, aRequest.Operation)
			|| ::sqlite3_bind_int64(
				   advanceOperation->Value,
				   3,
				   aRequest.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(
				advanceOperation->Value, 4, Binding.Authority.Cache)
			|| !bind_fixed(
				advanceOperation->Value, 5, Binding.Authority.Scope)
			|| ::sqlite3_step(advanceOperation->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		Binding.Catalog.Revision = core::epoch_catalog_revision{
			aRequest.Catalog.Revision.value() + 1
		};
		return app::catalog_commit_result{
			.Catalog       = Binding.Catalog,
			.Changed       = true,
			.InsertedEpoch = epoch.Id,
		};
	}

	auto sqlite_epoch_catalog_session::associate_review(
		const app::associate_review_request& aRequest,
		std::stop_token                      aStop) -> result<app::catalog_commit_result>
	{
		if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog
			|| aRequest.Review.Actor != Binding.Authority.Viewer
			|| aRequest.Review.Commit.algorithm() != Binding.ObjectFormat) {
			return std::unexpected(failure::invalid_request);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		transaction      write{ database, aStop };
		if (auto begun = write.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		if (auto active = require_active_pull_request_scope(
				database, Binding, aRequest.Catalog.Revision, true);
			!active) {
			return std::unexpected(active.error());
		}
		if (auto idle = require_no_blocking_operation(database, Binding, false);
			!idle) {
			return std::unexpected(idle.error());
		}
		auto canonical = canonicalize_review(
			database,
			Binding.Catalog.Incarnation,
			Binding.Authority.Scope,
			aRequest.Review);
		if (!canonical) {
			return std::unexpected(canonical.error());
		}
		const auto& reviewFact = canonical->Review;
		auto        current    = prepare(
			database,
			"SELECT c.association, c.accepted_snapshot, r.review_id, "
			"r.actor_id, r.review_order, r.review_state, r.commit_algorithm, "
			"r.commit_digest, r.availability "
			"FROM current_viewer_review c JOIN review_association r "
			"ON r.incarnation = c.incarnation AND r.pr_scope = c.pr_scope "
			"AND r.association = c.association "
			"WHERE c.incarnation = ?1 AND c.pr_scope = ?2");
		if (!current || !bind_fixed(current->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(current->Value, 2, Binding.Authority.Scope)) {
			return std::unexpected(current ? failure::unavailable : current.error());
		}
		const auto currentCode = ::sqlite3_step(current->Value);
		if (currentCode == SQLITE_ROW) {
			const auto association = read_fixed<core::review_association_id>(current->Value, 0);
			const auto snapshot    = read_u64(current->Value, 1);
			const auto review      = read_u64(current->Value, 2);
			const auto actor       = read_u64(current->Value, 3);
			const auto commit      = read_oid<core::git_commit_oid>(current->Value, 6, 7);
			if (!association || !snapshot || !review || !actor || !commit) {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto exact = *association == reviewFact.Association
				&& *snapshot == aRequest.AcceptedSnapshot.value()
				&& *review == reviewFact.Review.value()
				&& *actor == reviewFact.Actor.value()
				&& ::sqlite3_column_int(current->Value, 4)
					== reviewFact.Order.value()
				&& ::sqlite3_column_int(current->Value, 5)
					== stored_enum(reviewFact.State)
				&& *commit == reviewFact.Commit
				&& ::sqlite3_column_int(current->Value, 8)
					== stored_enum(reviewFact.Availability);
			if (::sqlite3_step(current->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (exact) {
				return app::catalog_commit_result{
					.Catalog = Binding.Catalog,
					.Changed = false,
				};
			}
		} else if (currentCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		auto persisted = persist_review(
			database,
			Binding.Catalog.Incarnation,
			Binding.Authority.Scope,
			reviewFact,
			aRequest.AcceptedSnapshot,
			canonical->LinkedOrdinal,
			aStop);
		if (!persisted) {
			return std::unexpected(persisted.error());
		}
		auto revision = prepare(
			database,
			"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1 "
			"WHERE incarnation = ?1 AND pr_scope = ?2 "
			"AND catalog_revision = ?3 AND lifecycle = 1 "
			"AND catalog_revision < 9223372036854775807");
		if (!revision
			|| !bind_fixed(revision->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(revision->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   revision->Value, 3, aRequest.Catalog.Revision.value())
				!= SQLITE_OK
			|| ::sqlite3_step(revision->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::revision_conflict);
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		Binding.Catalog.Revision = core::epoch_catalog_revision{
			aRequest.Catalog.Revision.value() + 1
		};
		return app::catalog_commit_result{
			.Catalog = Binding.Catalog,
			.Changed = true,
		};
	}
}
