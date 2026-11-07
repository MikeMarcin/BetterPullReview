#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "catalog_database.hpp"
#include "epoch_catalog_support.hpp"
#include "sqlite_connection.hpp"

#include <algorithm>
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
#include <vector>

namespace bpr::adapters::sqlite::epoch_catalog_detail {
	namespace {
		[[nodiscard]] auto require_readable_pull_request_scope(
			const pull_request_scope_state&                   aState,
			const app::epoch_catalog_binding&                 aBinding,
			const std::optional<core::epoch_catalog_revision> aExpected = std::nullopt)
			-> result<void>
		{
			if (aState.Cache != aBinding.Authority.Cache) {
				return std::unexpected(failure::authority_mismatch);
			}
			if (aExpected && aState.Revision != *aExpected) {
				return std::unexpected(failure::revision_conflict);
			}
			if (
				(aState.Lifecycle == 1 && aState.RepositoryLifecycle == 1)
				|| (aState.Lifecycle == 3
					&& (aState.RepositoryLifecycle == 3
						|| aState.RepositoryLifecycle == 4))) {
				return { };
			}
			if (aState.Lifecycle == 4) {
				return std::unexpected(failure::repair_required);
			}
			if (aState.RepositoryLifecycle == 2) {
				return std::unexpected(failure::operation_conflict);
			}
			return std::unexpected(failure::repair_required);
		}

		[[nodiscard]] auto write_occurrence(::sqlite3*                        aDatabase,
											const app::epoch_catalog_binding& aBinding, const core::epoch_occurrence& aOccurrence,
											std::stop_token aStop) -> result<void>
		{
			if (aOccurrence.DerivedComparisonBase && !aOccurrence.LocalEpoch) {
				return std::unexpected(failure::invalid_request);
			}
			if (aOccurrence.LocalEpoch) {
				auto epoch = load_epoch_summary(aDatabase, aBinding.Catalog.Incarnation,
												aBinding.Authority.Scope, aOccurrence.LocalEpoch->Ordinal);
				if (!epoch || epoch->Head != aOccurrence.Evidence.Head) {
					return std::unexpected(failure::invalid_request);
				}
				if (epoch->Cause.Origin == core::epoch_origin::explicit_historical_import) {
					// Timeline reconciliation cannot create or resurrect a comparison
					// binding. Only journaled historical finalization may establish it.
					auto prior = prepare(aDatabase, "SELECT local_ordinal FROM epoch_occurrence WHERE incarnation = ?1 AND pr_scope = ?2 AND occurrence_id = ?3");
					if (!prior || !bind_fixed(prior->Value, 1, aBinding.Catalog.Incarnation)
						|| !bind_fixed(prior->Value, 2, aBinding.Authority.Scope)
						|| !bind_fixed(prior->Value, 3, aOccurrence.Id)
						|| ::sqlite3_step(prior->Value) != SQLITE_ROW
						|| ::sqlite3_column_type(prior->Value, 0) != SQLITE_INTEGER
						|| ::sqlite3_column_int64(prior->Value, 0) != aOccurrence.LocalEpoch->Ordinal.value()
						|| !aOccurrence.DerivedComparisonBase
						|| *aOccurrence.DerivedComparisonBase != epoch->Base) {
						return std::unexpected(failure::invalid_request);
					}
				}
			}
			auto insert = prepare(aDatabase,
								  "INSERT INTO epoch_occurrence(incarnation, pr_scope, occurrence_id, kind, event_id, head_algorithm, head_digest, "
								  "local_ordinal, historical_base, derived_base, date_source, date_utc, first_seen) "
								  "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13) "
								  "ON CONFLICT(incarnation, pr_scope, occurrence_id) DO UPDATE SET "
								  "local_ordinal = CASE WHEN excluded.local_ordinal IS NULL THEN epoch_occurrence.local_ordinal ELSE excluded.local_ordinal END, "
								  "historical_base = CASE WHEN excluded.historical_base IS NULL THEN epoch_occurrence.historical_base ELSE excluded.historical_base END, "
								  "derived_base = CASE WHEN epoch_occurrence.local_ordinal IS NOT NULL AND excluded.derived_base IS NULL THEN epoch_occurrence.derived_base ELSE excluded.derived_base END, "
								  "date_source = CASE WHEN excluded.date_source = 0 THEN epoch_occurrence.date_source ELSE excluded.date_source END, "
								  "date_utc = CASE WHEN excluded.date_source = 0 THEN epoch_occurrence.date_utc ELSE excluded.date_utc END");
			if (!insert || !bind_fixed(insert->Value, 1, aBinding.Catalog.Incarnation)
				|| !bind_fixed(insert->Value, 2, aBinding.Authority.Scope) || !bind_fixed(insert->Value, 3, aOccurrence.Id)
				|| ::sqlite3_bind_int(insert->Value, 4, static_cast<int>(aOccurrence.Evidence.Kind)) != SQLITE_OK
				|| !bind_u64(insert->Value, 5, aOccurrence.Evidence.EventId)
				|| !bind_oid(insert->Value, 6, 7, aOccurrence.Evidence.Head)) {
				return std::unexpected(failure::unavailable);
			}
			const auto bindOptionalBase = [&](const int aIndex, const std::optional<core::git_commit_oid>& aBase) {
				return !aBase || bind_bytes(insert->Value, aIndex, std::span{ aBase->bytes() }.first(aBase->byte_count()));
			};
			if ((aOccurrence.LocalEpoch && ::sqlite3_bind_int64(insert->Value, 8, aOccurrence.LocalEpoch->Ordinal.value()) != SQLITE_OK)
				|| !bindOptionalBase(9, aOccurrence.HistoricalBase) || !bindOptionalBase(10, aOccurrence.DerivedComparisonBase)
				|| ::sqlite3_bind_int(insert->Value, 11, static_cast<int>(aOccurrence.DateSource)) != SQLITE_OK
				|| ::sqlite3_bind_int64(insert->Value, 12, aOccurrence.DateUtcMilliseconds) != SQLITE_OK
				|| ::sqlite3_bind_int64(insert->Value, 13, aOccurrence.FirstSeen.value()) != SQLITE_OK
				|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}
			return { };
		}

		[[nodiscard]] auto supersession_target(::sqlite3* aDatabase, const app::epoch_catalog_binding& aBinding,
											   const core::epoch_occurrence_id& aId, std::stop_token aStop) -> result<std::optional<core::epoch_occurrence_id>>
		{
			auto query = prepare(aDatabase, "SELECT replacement_id FROM epoch_occurrence_supersession WHERE incarnation = ?1 AND pr_scope = ?2 AND occurrence_id = ?3");
			if (!query || !bind_fixed(query->Value, 1, aBinding.Catalog.Incarnation)
				|| !bind_fixed(query->Value, 2, aBinding.Authority.Scope) || !bind_fixed(query->Value, 3, aId)) {
				return std::unexpected(failure::unavailable);
			}
			const auto code = ::sqlite3_step(query->Value);
			if (code == SQLITE_DONE) {
				return std::nullopt;
			}
			if (code != SQLITE_ROW) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}
			const auto id = read_fixed<core::epoch_occurrence_id>(query->Value, 0);
			if (!id) {
				return std::unexpected(failure::integrity_failure);
			}
			return id;
		}
	}

	auto sqlite_epoch_catalog_session::retained_residency(core::epoch_ordinal aOrdinal, std::stop_token aStop)
		-> result<app::git_revision_residency>
	{
		if (!core::valid(aOrdinal)) {
			return std::unexpected(failure::invalid_request);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		// Epoch and source-observation unique indexes bound every join to one row.
		auto query = prepare(database,
							 "SELECT r.residency, i.pack_algorithm, i.head_lineage FROM epoch e "
							 "JOIN pull_request_scope p ON p.incarnation=e.incarnation AND p.pr_scope=e.pr_scope "
							 "JOIN repository_scope c ON c.incarnation=p.incarnation AND c.cache_scope=p.cache_scope "
							 "JOIN operation o INDEXED BY operation_source_observation ON o.incarnation=e.incarnation "
							 "AND o.source_observation=e.source_observation AND o.kind=1 "
							 "AND o.pr_scope=e.pr_scope AND o.cache_scope=p.cache_scope AND o.reserved_ordinal=e.ordinal "
							 "JOIN append_install i ON i.incarnation=o.incarnation AND i.operation=o.operation "
							 "JOIN epoch_anchor b ON b.incarnation=e.incarnation AND b.pr_scope=e.pr_scope AND b.ordinal=e.ordinal AND b.endpoint=1 "
							 "JOIN epoch_anchor h ON h.incarnation=e.incarnation AND h.pr_scope=e.pr_scope AND h.ordinal=e.ordinal AND h.endpoint=2 "
							 "LEFT JOIN append_residency r ON r.incarnation=o.incarnation AND r.operation=o.operation "
							 "WHERE e.incarnation=?1 AND e.pr_scope=?2 AND e.ordinal=?3 "
							 "AND p.cache_scope=?4 AND p.catalog_revision=?5 AND p.lifecycle=1 AND c.lifecycle=1 "
							 "AND o.phase IN (8,9) AND b.availability=1 AND h.availability=1 "
							 "AND i.base_algorithm=b.object_algorithm AND i.base_digest=b.object_digest "
							 "AND i.head_algorithm=h.object_algorithm AND i.head_digest=h.object_digest "
							 "AND i.base_tree_algorithm=e.material_base_algorithm AND i.base_tree_digest=e.material_base_digest "
							 "AND i.head_tree_algorithm=e.material_head_algorithm AND i.head_tree_digest=e.material_head_digest");
		if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(query->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(query->Value, 3, aOrdinal.value()) != SQLITE_OK
			|| !bind_fixed(query->Value, 4, Binding.Authority.Cache)
			|| ::sqlite3_bind_int64(query->Value, 5, Binding.Catalog.Revision.value()) != SQLITE_OK) {
			return std::unexpected(query ? failure::unavailable : query.error());
		}
		const auto code = ::sqlite3_step(query->Value);
		if (code == SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		if (code != SQLITE_ROW) {
			return std::unexpected(sql_failure(database, aStop));
		}
		const auto type      = ::sqlite3_column_type(query->Value, 0);
		const auto value     = ::sqlite3_column_int(query->Value, 0);
		const auto algorithm = ::sqlite3_column_int(query->Value, 1);
		const auto lineage   = ::sqlite3_column_int(query->Value, 2);
		if ((type != SQLITE_NULL && type != SQLITE_INTEGER) || value < 0 || value > 1
			|| (value == 1 && (algorithm != 1 || (lineage != stored_enum(core::head_lineage::unknown) && lineage != stored_enum(core::head_lineage::not_applicable))))
			|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		return static_cast<app::git_revision_residency>(value);
	}

	auto sqlite_epoch_catalog_session::snapshot(std::stop_token aStop)
		-> result<app::epoch_catalog_snapshot>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		read_transaction read{ database, aStop };
		if (auto begun = read.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto state = load_pull_request_scope_state(
			database,
			Binding.Catalog.Incarnation,
			Binding.Authority.Scope);
		if (!state) {
			return std::unexpected(state.error());
		}
		if (auto readable = require_readable_pull_request_scope(
				*state, Binding);
			!readable) {
			return std::unexpected(readable.error());
		}
		auto query = prepare(
			database,
			"SELECT catalog_revision, latest_live_ordinal, "
			"previous_distinct_head_ordinal, retention_rehydration_required, "
			"next_ordinal "
			"FROM pull_request_scope "
			"WHERE incarnation = ?1 AND pr_scope = ?2");
		if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(query->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
			return std::unexpected(
				query ? sql_failure(database, aStop, failure::authority_mismatch)
					  : query.error());
		}
		const auto                         revision = ::sqlite3_column_int64(query->Value, 0);
		std::optional<core::epoch_ordinal> latest;
		std::optional<core::epoch_ordinal> previous;
		if (::sqlite3_column_type(query->Value, 1) == SQLITE_INTEGER) {
			latest = core::epoch_ordinal{
				::sqlite3_column_int64(query->Value, 1)
			};
		}
		if (::sqlite3_column_type(query->Value, 2) == SQLITE_INTEGER) {
			previous = core::epoch_ordinal{
				::sqlite3_column_int64(query->Value, 2)
			};
		}
		const auto                         rehydrationRequired = ::sqlite3_column_int(query->Value, 3);
		std::optional<core::epoch_ordinal> nextOrdinal;
		const auto                         nextOrdinalType = ::sqlite3_column_type(query->Value, 4);
		if (nextOrdinalType == SQLITE_INTEGER) {
			nextOrdinal = core::epoch_ordinal{
				::sqlite3_column_int64(query->Value, 4)
			};
		}
		if (revision < 0
			|| (rehydrationRequired != 0 && rehydrationRequired != 1)
			|| (latest && !core::valid(*latest))
			|| (previous && !core::valid(*previous))
			|| (nextOrdinalType != SQLITE_NULL
				&& (nextOrdinalType != SQLITE_INTEGER
					|| !nextOrdinal || !core::valid(*nextOrdinal)))
			|| (latest && nextOrdinal && latest->value() >= nextOrdinal->value())
			|| (previous && nextOrdinal
				&& previous->value() >= nextOrdinal->value())
			|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		auto snapshotBinding             = Binding;
		snapshotBinding.Catalog.Revision = core::epoch_catalog_revision{ revision };
		app::epoch_catalog_snapshot output{
			.Binding              = snapshotBinding,
			.NextOrdinal          = nextOrdinal,
			.RetentionRehydration = rehydrationRequired != 0
				? core::retention_rehydration_requirement::required
				: core::retention_rehydration_requirement::not_required,
		};
		if (latest) {
			auto loaded = load_epoch_summary(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				*latest);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			output.LatestLive = *loaded;
		}
		if (rehydrationRequired != 0
			&& (!output.LatestLive
				|| output.LatestLive->BaseAvailability
					!= core::anchor_availability::cache_deleted
				|| output.LatestLive->HeadAvailability
					!= core::anchor_availability::cache_deleted)) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (previous) {
			auto loaded = load_epoch_summary(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				*previous);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			output.PreviousDistinctHead = *loaded;
		}
		auto current = prepare(
			database,
			"SELECT association FROM current_viewer_review "
			"WHERE incarnation = ?1 AND pr_scope = ?2");
		if (!current || !bind_fixed(current->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(current->Value, 2, Binding.Authority.Scope)) {
			return std::unexpected(current ? failure::unavailable : current.error());
		}
		const auto code = ::sqlite3_step(current->Value);
		if (code == SQLITE_ROW) {
			auto association = read_fixed<core::review_association_id>(
				current->Value, 0);
			if (!association || ::sqlite3_step(current->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto loaded = load_review_association(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				Binding.Authority.Viewer,
				*association);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			output.CurrentViewerReview = *loaded;
		} else if (code != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		if (auto committed = read.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		Binding = snapshotBinding;
		return output;
	}

	auto sqlite_epoch_catalog_session::source_observation_exists(
		const core::epoch_source_observation_id aObservation,
		std::stop_token                         aStop) -> result<bool>
	{
		if (!aObservation.valid()) {
			return std::unexpected(failure::invalid_request);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		read_transaction read{ database, aStop };
		if (auto begun = read.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto exists = catalog_contains_source_observation(
			database,
			Binding.Catalog.Incarnation,
			aObservation,
			aStop);
		if (!exists) {
			return std::unexpected(exists.error());
		}
		if (auto committed = read.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return *exists;
	}

	auto sqlite_epoch_catalog_session::canonical_review(
		const app::canonical_review_request& aRequest,
		std::stop_token                      aStop) -> result<core::catalog_review_association>
	{
		if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog
			|| aRequest.Candidate.Actor != Binding.Authority.Viewer
			|| aRequest.Candidate.Commit.algorithm() != Binding.ObjectFormat
			|| (aRequest.ProjectedHead
				&& aRequest.ProjectedHead->Head.algorithm()
					!= Binding.ObjectFormat)) {
			return std::unexpected(failure::invalid_request);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		read_transaction read{ database, aStop };
		if (auto begun = read.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto state = load_pull_request_scope_state(
			database,
			Binding.Catalog.Incarnation,
			Binding.Authority.Scope);
		if (!state) {
			return std::unexpected(state.error());
		}
		if (auto readable = require_readable_pull_request_scope(
				*state, Binding, aRequest.Catalog.Revision);
			!readable) {
			return std::unexpected(readable.error());
		}

		auto existing = prepare(
			database,
			"SELECT association FROM review_association "
			"WHERE incarnation = ?1 AND pr_scope = ?2 AND review_id = ?3");
		if (!existing
			|| !bind_fixed(
				existing->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(existing->Value, 2, Binding.Authority.Scope)
			|| !bind_u64(existing->Value, 3, aRequest.Candidate.Review.value())) {
			return std::unexpected(
				existing ? failure::unavailable : existing.error());
		}
		std::optional<core::review_association_id> association;
		const auto                                 existingCode = ::sqlite3_step(existing->Value);
		if (existingCode == SQLITE_ROW) {
			association = read_fixed<core::review_association_id>(
				existing->Value, 0);
			if (!association || ::sqlite3_step(existing->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto loaded = load_review_association(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				Binding.Authority.Viewer,
				*association);
			if (!loaded || loaded->Review != aRequest.Candidate.Review) {
				return std::unexpected(
					loaded ? failure::schema_mismatch : loaded.error());
			}
		} else if (existingCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}

		if (!association) {
			auto identityCollision = prepare(
				database,
				"SELECT 1 FROM review_association WHERE incarnation = ?1 "
				"AND pr_scope = ?2 AND association = ?3 LIMIT 1");
			if (!identityCollision
				|| !bind_fixed(
					identityCollision->Value,
					1,
					Binding.Catalog.Incarnation)
				|| !bind_fixed(
					identityCollision->Value, 2, Binding.Authority.Scope)
				|| !bind_fixed(
					identityCollision->Value,
					3,
					aRequest.ProposedAssociation)) {
				return std::unexpected(
					identityCollision ? failure::unavailable
									  : identityCollision.error());
			}
			const auto collisionCode = ::sqlite3_step(identityCollision->Value);
			if (collisionCode == SQLITE_ROW) {
				return std::unexpected(failure::operation_conflict);
			}
			if (collisionCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
			association = aRequest.ProposedAssociation;
		}

		auto orderCollision = prepare(
			database,
			"SELECT 1 FROM review_association WHERE incarnation = ?1 "
			"AND pr_scope = ?2 AND review_order = ?3 AND review_id != ?4 "
			"LIMIT 1");
		if (!orderCollision
			|| !bind_fixed(
				orderCollision->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(orderCollision->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int(
				   orderCollision->Value, 3, aRequest.Candidate.Order.value())
				!= SQLITE_OK
			|| !bind_u64(
				orderCollision->Value, 4, aRequest.Candidate.Review.value())) {
			return std::unexpected(
				orderCollision ? failure::unavailable
							   : orderCollision.error());
		}
		const auto orderCode = ::sqlite3_step(orderCollision->Value);
		if (orderCode == SQLITE_ROW) {
			return std::unexpected(failure::operation_conflict);
		}
		if (orderCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}

		auto candidate = core::catalog_review_association{
			.Association  = *association,
			.Review       = aRequest.Candidate.Review,
			.Actor        = aRequest.Candidate.Actor,
			.Order        = aRequest.Candidate.Order,
			.State        = aRequest.Candidate.State,
			.Commit       = aRequest.Candidate.Commit,
			.Availability = core::anchor_availability::never_observed,
		};
		std::optional<std::pair<core::epoch_ordinal, core::git_commit_oid>>
			projected;
		if (aRequest.ProjectedHead) {
			projected = std::pair{
				aRequest.ProjectedHead->Ordinal,
				aRequest.ProjectedHead->Head
			};
		}
		auto canonical = canonicalize_review(
			database,
			Binding.Catalog.Incarnation,
			Binding.Authority.Scope,
			candidate,
			projected);
		if (!canonical) {
			return std::unexpected(canonical.error());
		}
		if (auto committed = read.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return canonical->Review;
	}

	auto sqlite_epoch_catalog_session::exact_anchors(
		const app::exact_anchor_lookup_request& aRequest,
		std::stop_token                         aStop) -> result<app::exact_anchor_lookup_result>
	{
		if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
			return std::unexpected(failure::invalid_request);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		read_transaction read{ database, aStop };
		if (auto begun = read.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto scope = load_pull_request_scope_state(
			database,
			Binding.Catalog.Incarnation,
			Binding.Authority.Scope);
		if (!scope) {
			return std::unexpected(scope.error());
		}
		if (auto readable = require_readable_pull_request_scope(
				*scope, Binding, aRequest.Catalog.Revision);
			!readable) {
			return std::unexpected(readable.error());
		}
		app::exact_anchor_lookup_result output{ .Catalog = aRequest.Catalog };
		output.Anchors.reserve(aRequest.Anchors.size());
		for (const auto& identity : aRequest.Anchors) {
			if (identity.Authority != Binding.Authority.Scope) {
				return std::unexpected(failure::authority_mismatch);
			}
			core::comparison_anchor anchor{ .Identity = identity };
			if (identity.Kind == core::comparison_anchor_kind::review_commit) {
				auto query = prepare(
					database,
					"SELECT actor_id, commit_algorithm, commit_digest, "
					"availability "
					"FROM review_association WHERE incarnation = ?1 "
					"AND pr_scope = ?2 AND association = ?3");
				if (!query
					|| !bind_fixed(
						query->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(query->Value, 2, Binding.Authority.Scope)
					|| !bind_fixed(query->Value, 3, identity.Review)) {
					return std::unexpected(query ? sql_failure(database, aStop) : query.error());
				}
				const auto code = ::sqlite3_step(query->Value);
				if (code == SQLITE_DONE) {
					continue;
				}
				if (code != SQLITE_ROW) {
					return std::unexpected(sql_failure(database, aStop));
				}
				const auto actor        = read_u64(query->Value, 0);
				auto       commit       = read_oid<core::git_commit_oid>(query->Value, 1, 2);
				const auto availability = ::sqlite3_column_int(query->Value, 3);
				if (!actor || *actor != Binding.Authority.Viewer.value() || !commit
					|| ::sqlite3_column_type(query->Value, 3) != SQLITE_INTEGER
					|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				anchor.Commit       = *commit;
				anchor.Availability = restored_enum<core::anchor_availability>(
					availability);
			} else {
				auto query = prepare(
					database,
					"SELECT object_algorithm, object_digest, availability "
					"FROM epoch_anchor WHERE incarnation = ?1 "
					"AND pr_scope = ?2 AND ordinal = ?3 AND endpoint = ?4");
				const auto endpoint = identity.Kind == core::comparison_anchor_kind::epoch_base
					? 1
					: 2;
				if (!query
					|| !bind_fixed(
						query->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(query->Value, 2, Binding.Authority.Scope)
					|| ::sqlite3_bind_int64(
						   query->Value, 3, identity.Epoch.value())
						!= SQLITE_OK
					|| ::sqlite3_bind_int(query->Value, 4, endpoint) != SQLITE_OK) {
					return std::unexpected(query ? sql_failure(database, aStop) : query.error());
				}
				const auto code = ::sqlite3_step(query->Value);
				if (code == SQLITE_DONE) {
					continue;
				}
				if (code != SQLITE_ROW) {
					return std::unexpected(sql_failure(database, aStop));
				}
				auto       commit       = read_oid<core::git_commit_oid>(query->Value, 0, 1);
				const auto availability = ::sqlite3_column_int(query->Value, 2);
				if (!commit
					|| ::sqlite3_column_type(query->Value, 2) != SQLITE_INTEGER
					|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				anchor.Commit       = *commit;
				anchor.Availability = restored_enum<core::anchor_availability>(
					availability);
			}
			output.Anchors.push_back(anchor);
		}
		if (auto committed = read.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::mark_anchor_availability(
		const app::mark_anchor_availability_request& aRequest,
		std::stop_token                              aStop) -> result<app::catalog_commit_result>
	{
		if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
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
		for (const auto& change : aRequest.Changes) {
			if (change.Anchor.Authority != Binding.Authority.Scope
				|| change.ExpectedCommit.algorithm() != Binding.ObjectFormat) {
				return std::unexpected(failure::authority_mismatch);
			}
			result<statement> update = std::unexpected(failure::unavailable);
			if (change.Anchor.Kind == core::comparison_anchor_kind::review_commit) {
				update = prepare(
					database,
					"UPDATE review_association SET availability = ?1 "
					"WHERE incarnation = ?2 AND pr_scope = ?3 "
					"AND association = ?4 AND commit_algorithm = ?5 "
					"AND commit_digest = ?6 AND availability = ?7");
				if (update
					&& (::sqlite3_bind_int(
							update->Value, 1, stored_enum(change.Next))
							!= SQLITE_OK
						|| !bind_fixed(
							update->Value, 2, Binding.Catalog.Incarnation)
						|| !bind_fixed(
							update->Value, 3, Binding.Authority.Scope)
						|| !bind_fixed(update->Value, 4, change.Anchor.Review)
						|| !bind_oid(
							update->Value, 5, 6, change.ExpectedCommit)
						|| ::sqlite3_bind_int(
							   update->Value, 7, stored_enum(change.Expected))
							!= SQLITE_OK)) {
					return std::unexpected(failure::unavailable);
				}
			} else {
				update = prepare(
					database,
					"UPDATE epoch_anchor SET availability = ?1 "
					"WHERE incarnation = ?2 AND pr_scope = ?3 "
					"AND ordinal = ?4 AND endpoint = ?5 "
					"AND object_algorithm = ?6 AND object_digest = ?7 "
					"AND availability = ?8");
				const auto endpoint = change.Anchor.Kind
						== core::comparison_anchor_kind::epoch_base
					? 1
					: 2;
				if (update
					&& (::sqlite3_bind_int(
							update->Value, 1, stored_enum(change.Next))
							!= SQLITE_OK
						|| !bind_fixed(
							update->Value, 2, Binding.Catalog.Incarnation)
						|| !bind_fixed(
							update->Value, 3, Binding.Authority.Scope)
						|| ::sqlite3_bind_int64(
							   update->Value, 4, change.Anchor.Epoch.value())
							!= SQLITE_OK
						|| ::sqlite3_bind_int(update->Value, 5, endpoint)
							!= SQLITE_OK
						|| !bind_oid(
							update->Value, 6, 7, change.ExpectedCommit)
						|| ::sqlite3_bind_int(
							   update->Value, 8, stored_enum(change.Expected))
							!= SQLITE_OK)) {
					return std::unexpected(failure::unavailable);
				}
			}
			if (!update || ::sqlite3_step(update->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
		}
		auto revision = prepare(
			database,
			"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1 "
			"WHERE incarnation = ?1 AND pr_scope = ?2 "
			"AND catalog_revision = ?3 "
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

	auto sqlite_epoch_catalog_session::occurrences(std::stop_token aStop)
		-> result<core::epoch_occurrence_history>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		read_transaction read{ database, aStop };
		if (auto begun = read.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto state = load_pull_request_scope_state(database, Binding.Catalog.Incarnation, Binding.Authority.Scope);
		if (!state) {
			return std::unexpected(state.error());
		}
		if (auto readable = require_readable_pull_request_scope(*state, Binding); !readable) {
			return std::unexpected(readable.error());
		}
		auto output = read_occurrence_rows(database, Binding, aStop);
		if (!output) {
			return output;
		}
		if (auto committed = read.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::record_occurrences(const core::epoch_occurrence_history& aHistory, std::stop_token aStop)
		-> result<void>
	{
		if (aHistory.Occurrences.size() > core::maximum_epoch_occurrence_count) {
			return std::unexpected(failure::quota_exceeded);
		}
		for (const auto& occurrence : aHistory.Occurrences) {
			if (!core::valid(occurrence) || occurrence.Evidence.Head.algorithm() != Binding.ObjectFormat
				|| (occurrence.LocalEpoch && (occurrence.LocalEpoch->Authority != Binding.Authority.Scope || occurrence.LocalEpoch->Incarnation != Binding.Catalog.Incarnation))) {
				return std::unexpected(failure::invalid_request);
			}
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		transaction      write{ database, aStop };
		if (auto begun = write.begin(); !begun) {
			return begun;
		}
		auto state = load_pull_request_scope_state(database, Binding.Catalog.Incarnation, Binding.Authority.Scope);
		if (!state) {
			return std::unexpected(state.error());
		}
		if (auto readable = require_readable_pull_request_scope(*state, Binding); !readable) {
			return readable;
		}
		if (state->Lifecycle != 1 || state->RepositoryLifecycle != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		auto existing = read_occurrence_rows(database, Binding, aStop);
		if (!existing) {
			return std::unexpected(existing.error());
		}
		for (const auto& occurrence : aHistory.Occurrences) {
			auto retired = supersession_target(database, Binding, occurrence.Id, aStop);
			if (!retired) {
				return std::unexpected(retired.error());
			}
			if (*retired) {
				return std::unexpected(failure::invalid_request);
			}
			const auto prior = std::ranges::find(existing->Occurrences, occurrence.Id, &core::epoch_occurrence::Id);
			if (prior != existing->Occurrences.end()) {
				if (prior->Evidence != occurrence.Evidence
					|| (prior->LocalEpoch && occurrence.LocalEpoch && prior->LocalEpoch != occurrence.LocalEpoch)
					|| (prior->HistoricalBase && occurrence.HistoricalBase && prior->HistoricalBase != occurrence.HistoricalBase)) {
					return std::unexpected(failure::invalid_request);
				}
			} else {
				if (existing->Occurrences.size() == core::maximum_epoch_occurrence_count) {
					return std::unexpected(failure::quota_exceeded);
				}
				existing->Occurrences.push_back(occurrence);
			}
			if (auto written = write_occurrence(database, Binding, occurrence, aStop); !written) {
				return written;
			}
			// Carry refinements forward so conflicting duplicates within this same batch also fail atomically.
			const auto updated = std::ranges::find(existing->Occurrences, occurrence.Id, &core::epoch_occurrence::Id);
			if (occurrence.LocalEpoch) {
				updated->LocalEpoch = occurrence.LocalEpoch;
			}
			if (occurrence.HistoricalBase) {
				updated->HistoricalBase = occurrence.HistoricalBase;
			}
		}
		auto status = prepare(database, "INSERT INTO epoch_occurrence_history(incarnation, pr_scope, incomplete, ambiguous) VALUES(?1, ?2, ?3, ?4) "
										"ON CONFLICT(incarnation, pr_scope) DO UPDATE SET incomplete = excluded.incomplete, ambiguous = excluded.ambiguous");
		if (!status || !bind_fixed(status->Value, 1, Binding.Catalog.Incarnation) || !bind_fixed(status->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int(status->Value, 3, aHistory.Incomplete || aHistory.Ambiguous) != SQLITE_OK
			|| ::sqlite3_bind_int(status->Value, 4, aHistory.Ambiguous) != SQLITE_OK
			|| ::sqlite3_step(status->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		return write.commit();
	}

	auto sqlite_epoch_catalog_session::supersede_occurrence(const core::epoch_occurrence& aExpected,
															const core::epoch_occurrence& aReplacement, std::stop_token aStop) -> result<void>
	{
		if (!core::valid(aExpected) || !core::valid(aReplacement)
			|| aExpected.Evidence.Kind != core::epoch_occurrence_kind::observed_head
			|| aReplacement.Evidence.Kind != core::epoch_occurrence_kind::force_push_event
			|| aExpected.Id == aReplacement.Id || aExpected.Evidence.Head != aReplacement.Evidence.Head
			|| aExpected.LocalEpoch != aReplacement.LocalEpoch
			|| aExpected.Evidence.Head.algorithm() != Binding.ObjectFormat
			|| (aExpected.LocalEpoch && (aExpected.LocalEpoch->Authority != Binding.Authority.Scope || aExpected.LocalEpoch->Incarnation != Binding.Catalog.Incarnation))) {
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
			return begun;
		}
		auto state = load_pull_request_scope_state(database, Binding.Catalog.Incarnation, Binding.Authority.Scope);
		if (!state) {
			return std::unexpected(state.error());
		}
		if (auto readable = require_readable_pull_request_scope(*state, Binding); !readable) {
			return readable;
		}
		if (state->Lifecycle != 1 || state->RepositoryLifecycle != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		auto active   = read_occurrence_rows(database, Binding, aStop);
		auto archived = read_occurrence_rows(database, Binding, aStop, true);
		if (!active || !archived) {
			return std::unexpected(active ? archived.error() : active.error());
		}
		const auto old     = std::ranges::find(active->Occurrences, aExpected.Id, &core::epoch_occurrence::Id);
		const auto target  = std::ranges::find(active->Occurrences, aReplacement.Id, &core::epoch_occurrence::Id);
		const auto retired = std::ranges::find(archived->Occurrences, aExpected.Id, &core::epoch_occurrence::Id);
		if (retired != archived->Occurrences.end()) {
			auto replacement = supersession_target(database, Binding, aExpected.Id, aStop);
			if (!replacement) {
				return std::unexpected(replacement.error());
			}
			if (*retired != aExpected || *replacement != aReplacement.Id
				|| old != active->Occurrences.end() || target == active->Occurrences.end() || *target != aReplacement) {
				return std::unexpected(failure::invalid_request);
			}
			return write.commit();
		}
		if (old == active->Occurrences.end() || *old != aExpected) {
			return std::unexpected(failure::revision_conflict);
		}
		if (archived->Occurrences.size() == core::maximum_epoch_occurrence_count) {
			return std::unexpected(failure::quota_exceeded);
		}
		if (target != active->Occurrences.end()) {
			auto boundTarget = *target;
			if (!boundTarget.LocalEpoch) {
				boundTarget.LocalEpoch = aReplacement.LocalEpoch;
			}
			if (boundTarget != aReplacement) {
				return std::unexpected(failure::invalid_request);
			}
		}
		// Deferred replacement FK permits archive, removal and installation in one transaction.
		auto archive = prepare(database, "INSERT INTO epoch_occurrence_supersession SELECT *, ?4 FROM epoch_occurrence "
										 "WHERE incarnation = ?1 AND pr_scope = ?2 AND occurrence_id = ?3");
		if (!archive || !bind_fixed(archive->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(archive->Value, 2, Binding.Authority.Scope) || !bind_fixed(archive->Value, 3, aExpected.Id)
			|| !bind_fixed(archive->Value, 4, aReplacement.Id) || ::sqlite3_step(archive->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		auto remove = prepare(database, "DELETE FROM epoch_occurrence WHERE incarnation = ?1 AND pr_scope = ?2 AND occurrence_id = ?3");
		if (!remove || !bind_fixed(remove->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(remove->Value, 2, Binding.Authority.Scope) || !bind_fixed(remove->Value, 3, aExpected.Id)
			|| ::sqlite3_step(remove->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		if (auto installed = write_occurrence(database, Binding, aReplacement, aStop); !installed) {
			return installed;
		}
		return write.commit();
	}

	auto sqlite_epoch_catalog_session::history(
		const app::epoch_history_page_request& aRequest,
		std::stop_token                        aStop) -> result<app::epoch_history_page>
	{
		if (!app::valid(Binding, aRequest)) {
			return std::unexpected(failure::invalid_request);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		read_transaction read{ database, aStop };
		if (auto begun = read.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto state = load_pull_request_scope_state(
			database,
			Binding.Catalog.Incarnation,
			Binding.Authority.Scope);
		if (!state) {
			return std::unexpected(state.error());
		}
		if (auto readable = require_readable_pull_request_scope(
				*state, Binding, aRequest.Catalog.Revision);
			!readable) {
			return std::unexpected(readable.error());
		}
		auto ordinals = prepare(
			database,
			aRequest.Cursor
				? "SELECT ordinal FROM epoch WHERE incarnation = ?1 "
				  "AND pr_scope = ?2 AND ordinal < ?3 "
				  "ORDER BY ordinal DESC LIMIT ?4"
				: "SELECT ordinal FROM epoch WHERE incarnation = ?1 "
				  "AND pr_scope = ?2 ORDER BY ordinal DESC LIMIT ?3");
		if (!ordinals
			|| !bind_fixed(ordinals->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(ordinals->Value, 2, Binding.Authority.Scope)) {
			return std::unexpected(ordinals ? failure::unavailable : ordinals.error());
		}
		const auto fetchLimit = static_cast<std::int64_t>(aRequest.Limit) + 1;
		if (aRequest.Cursor) {
			if (::sqlite3_bind_int64(
					ordinals->Value, 3, aRequest.Cursor->Before.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(ordinals->Value, 4, fetchLimit)
					!= SQLITE_OK) {
				return std::unexpected(failure::unavailable);
			}
		} else if (::sqlite3_bind_int64(ordinals->Value, 3, fetchLimit) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		std::vector<core::epoch_ordinal> values;
		while (true) {
			const auto code = ::sqlite3_step(ordinals->Value);
			if (code == SQLITE_DONE) {
				break;
			}
			if (code != SQLITE_ROW || aStop.stop_requested()) {
				return std::unexpected(sql_failure(database, aStop));
			}
			values.emplace_back(::sqlite3_column_int64(ordinals->Value, 0));
		}
		const auto hasMore = values.size() > aRequest.Limit;
		if (hasMore) {
			values.resize(aRequest.Limit);
		}
		app::epoch_history_page output{
			.Authority = Binding.Authority.Scope,
			.Catalog   = aRequest.Catalog,
		};
		output.Epochs.reserve(values.size());
		for (const auto ordinal : values) {
			auto loaded = load_epoch_summary(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				ordinal);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			output.Epochs.push_back(*loaded);
		}
		if (hasMore && !values.empty()) {
			output.Continuation = app::epoch_history_cursor{
				.Authority   = Binding.Authority.Scope,
				.Incarnation = Binding.Catalog.Incarnation,
				.Revision    = aRequest.Catalog.Revision,
				.Before      = values.back(),
			};
		}
		if (auto committed = read.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}
}
