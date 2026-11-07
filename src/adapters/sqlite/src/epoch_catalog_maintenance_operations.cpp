#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "catalog_database.hpp"
#include "epoch_catalog_support.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::sqlite::epoch_catalog_detail {
	namespace {
#if defined(BPR_SQLITE_TESTING)
		void observe_journal_transaction(detail::testing::epoch_journal_stage aStage)
		{
			if (const auto checkpoint = JournalCheckpoint.load(std::memory_order_acquire)) {
				checkpoint(aStage);
			}
		}
#endif
		[[nodiscard]] auto bind_pack(
			::sqlite3_stmt*          aQuery,
			const int                aAlgorithmIndex,
			const int                aDigestIndex,
			const app::git_pack_oid& aPack) noexcept -> bool
		{
			return bind_oid(
				aQuery,
				aAlgorithmIndex,
				aDigestIndex,
				aPack.Algorithm,
				aPack.Digest);
		}
	}

	auto sqlite_epoch_catalog_session::record_receiving(
		const app::record_receiving_request& aRequest,
		std::stop_token                      aStop) -> result<app::append_operation_record>
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
				database, Binding, aRequest.Catalog.Revision);
			!active) {
			return std::unexpected(active.error());
		}
		auto existing = load_append(aRequest.Operation);
		if (!existing) {
			return std::unexpected(existing.error());
		}
		if (existing->Phase == app::epoch_catalog_operation_phase::preparing
			&& existing->OperationRevision.value() == aRequest.ExpectedOperationRevision.value() + 1
			&& existing->Receiving == aRequest.Receiving) {
			return existing;
		}
		if (existing->Phase != app::epoch_catalog_operation_phase::preparing
			|| existing->OperationRevision != aRequest.ExpectedOperationRevision || existing->Receiving) {
			return std::unexpected(failure::phase_conflict);
		}
		auto receiving = prepare(database,
								 "INSERT INTO append_receiving(incarnation, operation, spool_identity, maximum_bytes) VALUES(?1, ?2, ?3, ?4)");
		if (!receiving || !bind_fixed(receiving->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(receiving->Value, 2, aRequest.Operation)
			|| !bind_bytes(receiving->Value, 3, aRequest.Receiving.Identity.Bytes)
			|| ::sqlite3_bind_int64(receiving->Value, 4, static_cast<std::int64_t>(aRequest.Receiving.MaximumBytes)) != SQLITE_OK
			|| ::sqlite3_step(receiving->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		auto advance = prepare(
			database,
			"UPDATE operation SET "
			"operation_revision = operation_revision + 1 "
			"WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
			"AND phase = 1 AND operation_revision = ?3 "
			"AND cache_scope = ?4 AND pr_scope = ?5 "
			"AND operation_revision < 9223372036854775806");
		if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(advance->Value, 2, aRequest.Operation)
			|| ::sqlite3_bind_int64(
				   advance->Value, 3, aRequest.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
			|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
			|| ::sqlite3_step(advance->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		auto output = load_append(aRequest.Operation);
		if (!output) {
			return std::unexpected(output.error());
		}
#if defined(BPR_SQLITE_TESTING)
		observe_journal_transaction(detail::testing::epoch_journal_stage::receiving);
#endif
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::record_spool(
		const app::record_spool_request& aRequest,
		std::stop_token                  aStop) -> result<app::append_operation_record>
	{
		if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog
			|| aRequest.Spool.ExpectedPack.Algorithm != Binding.ObjectFormat) {
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
				database, Binding, aRequest.Catalog.Revision);
			!active) {
			return std::unexpected(active.error());
		}
		auto existing = load_append(aRequest.Operation);
		if (!existing) {
			return std::unexpected(existing.error());
		}
		if (existing->Phase == app::epoch_catalog_operation_phase::spool_recorded
			&& existing->OperationRevision.value()
				== aRequest.ExpectedOperationRevision.value() + 1
			&& existing->Spool == aRequest.Spool
			&& existing->Preimport == aRequest.Preimport) {
			return existing;
		}
		if (existing->Phase != app::epoch_catalog_operation_phase::preparing
			|| existing->OperationRevision != aRequest.ExpectedOperationRevision) {
			return std::unexpected(failure::phase_conflict);
		}
		if (existing->Receiving && (existing->Receiving->Identity != aRequest.Spool.Spool.Identity || aRequest.Spool.Spool.Bytes > existing->Receiving->MaximumBytes)) {
			return std::unexpected(failure::invalid_request);
		}
		auto receipt = prepare(
			database,
			"INSERT INTO append_receipt(incarnation, operation, spool_identity, "
			"spool_role, spool_bytes, expected_pack_algorithm, "
			"expected_pack_digest, observed_pack_algorithm, "
			"observed_pack_digest, repository_bytes, "
			"expected_pack_preexisting, inventory_count) "
			"VALUES(?1, ?2, ?3, 1, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)");
		if (!receipt || !bind_fixed(receipt->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(receipt->Value, 2, aRequest.Operation)
			|| !bind_bytes(receipt->Value, 3, aRequest.Spool.Spool.Identity.Bytes)
			|| ::sqlite3_bind_int64(
				   receipt->Value,
				   4,
				   static_cast<std::int64_t>(aRequest.Spool.Spool.Bytes))
				!= SQLITE_OK
			|| !bind_pack(receipt->Value, 5, 6, aRequest.Spool.ExpectedPack)
			|| !bind_pack(receipt->Value, 7, 8, aRequest.Spool.ObservedPack)
			|| ::sqlite3_bind_int64(
				   receipt->Value,
				   9,
				   static_cast<std::int64_t>(aRequest.Preimport.RepositoryBytes))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   receipt->Value,
				   10,
				   aRequest.Preimport.ExpectedPackPreexisting ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   receipt->Value,
				   11,
				   static_cast<std::int64_t>(aRequest.Preimport.Packs.size()))
				!= SQLITE_OK
			|| ::sqlite3_step(receipt->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		auto residency = prepare(database,
								 "INSERT INTO append_residency(incarnation, operation, residency) VALUES(?1, ?2, ?3)");
		if (!residency || !bind_fixed(residency->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(residency->Value, 2, aRequest.Operation)
			|| ::sqlite3_bind_int(residency->Value, 3, static_cast<int>(aRequest.Spool.Residency)) != SQLITE_OK
			|| ::sqlite3_step(residency->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}

		auto inventory = prepare(
			database,
			"INSERT INTO append_inventory(incarnation, operation, position, "
			"pack_algorithm, pack_digest, pack_bytes, index_bytes) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)");
		if (!inventory) {
			return std::unexpected(inventory.error());
		}
		for (std::size_t index = 0; index < aRequest.Preimport.Packs.size(); ++index) {
			const auto& pack = aRequest.Preimport.Packs[index];
			if (::sqlite3_reset(inventory->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(inventory->Value) != SQLITE_OK
				|| !bind_fixed(
					inventory->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(inventory->Value, 2, aRequest.Operation)
				|| ::sqlite3_bind_int64(
					   inventory->Value, 3, static_cast<std::int64_t>(index))
					!= SQLITE_OK
				|| !bind_pack(inventory->Value, 4, 5, pack.Pack)
				|| ::sqlite3_bind_int64(
					   inventory->Value,
					   6,
					   static_cast<std::int64_t>(pack.PackBytes))
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(
					   inventory->Value,
					   7,
					   static_cast<std::int64_t>(pack.IndexBytes))
					!= SQLITE_OK
				|| ::sqlite3_step(inventory->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
		}
		auto advance = prepare(
			database,
			"UPDATE operation SET phase = 2, last_evidence = 1, "
			"operation_revision = operation_revision + 1 "
			"WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
			"AND phase = 1 AND operation_revision = ?3 "
			"AND cache_scope = ?4 AND pr_scope = ?5 "
			"AND operation_revision < 9223372036854775806");
		if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(advance->Value, 2, aRequest.Operation)
			|| ::sqlite3_bind_int64(
				   advance->Value, 3, aRequest.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
			|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
			|| ::sqlite3_step(advance->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		auto output = load_append(aRequest.Operation);
		if (!output) {
			return std::unexpected(output.error());
		}
#if defined(BPR_SQLITE_TESTING)
		observe_journal_transaction(detail::testing::epoch_journal_stage::spool);
#endif
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::record_install(
		const app::record_install_request& aRequest,
		std::stop_token                    aStop) -> result<app::append_operation_record>
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
				database, Binding, aRequest.Catalog.Revision);
			!active) {
			return std::unexpected(active.error());
		}
		auto existing = load_append(aRequest.Operation);
		if (!existing) {
			return std::unexpected(existing.error());
		}
		if (aRequest.DerivedComparisonBase) {
			if (existing->Context.Revision.Origin != core::pull_request_revision_origin::explicit_historical_import
				|| aRequest.DerivedComparisonBase->algorithm() != Binding.ObjectFormat) {
				return std::unexpected(failure::invalid_request);
			}
			if (existing->Phase == app::epoch_catalog_operation_phase::spool_recorded
				&& existing->OperationRevision == aRequest.ExpectedOperationRevision) {
				auto derived = prepare(database, "UPDATE append_operation SET base_algorithm = ?1, base_digest = ?2 WHERE incarnation = ?3 AND operation = ?4");
				if (!derived || !bind_oid(derived->Value, 1, 2, *aRequest.DerivedComparisonBase)
					|| !bind_fixed(derived->Value, 3, Binding.Catalog.Incarnation)
					|| !bind_operation(derived->Value, 4, aRequest.Operation)
					|| ::sqlite3_step(derived->Value) != SQLITE_DONE || ::sqlite3_changes(database) != 1) {
					return std::unexpected(failure::operation_conflict);
				}
				existing->Context.Revision.Base = *aRequest.DerivedComparisonBase;
			}
		}
		if (aRequest.Install.Base != existing->Context.Revision.Base
			|| aRequest.Install.Head != existing->Context.Revision.Head
			|| aRequest.Install.Pack.Algorithm != Binding.ObjectFormat) {
			return std::unexpected(failure::operation_conflict);
		}
		if (!existing->Spool || !existing->Preimport
			|| !coherent_install_facts(
				*existing->Spool, *existing->Preimport, aRequest.Install)) {
			return std::unexpected(failure::operation_conflict);
		}
		if (existing->Phase == app::epoch_catalog_operation_phase::imported
			&& existing->OperationRevision.value()
				== aRequest.ExpectedOperationRevision.value() + 1
			&& existing->Install == aRequest.Install) {
			return existing;
		}
		if (existing->Phase != app::epoch_catalog_operation_phase::spool_recorded
			|| existing->OperationRevision != aRequest.ExpectedOperationRevision
			|| !existing->Spool || !existing->Preimport
			|| existing->Spool->ExpectedPack != aRequest.Install.Pack) {
			return std::unexpected(failure::phase_conflict);
		}
		const auto& install = aRequest.Install;
		auto        insert  = prepare(
			database,
			"INSERT INTO append_install(incarnation, operation, pack_algorithm, "
			"pack_digest, base_algorithm, base_digest, base_tree_algorithm, "
			"base_tree_digest, head_algorithm, head_digest, "
			"head_tree_algorithm, head_tree_digest, head_lineage) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, "
			"?13)");
		if (!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(insert->Value, 2, aRequest.Operation)
			|| !bind_pack(insert->Value, 3, 4, install.Pack)
			|| !bind_oid(insert->Value, 5, 6, install.Base)
			|| !bind_oid(insert->Value, 7, 8, install.BaseTree)
			|| !bind_oid(insert->Value, 9, 10, install.Head)
			|| !bind_oid(insert->Value, 11, 12, install.HeadTree)
			|| ::sqlite3_bind_int(
				   insert->Value, 13, stored_enum(install.HeadLineage))
				!= SQLITE_OK
			|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		auto artifact = prepare(
			database,
			"INSERT INTO append_installed_artifact(incarnation, operation, "
			"artifact_role, artifact_identity, artifact_bytes) "
			"VALUES(?1, ?2, ?3, ?4, ?5)");
		if (!artifact) {
			return std::unexpected(artifact.error());
		}
		const std::array artifacts{ install.InstalledPack, install.InstalledIndex };
		for (const auto& value : artifacts) {
			if (::sqlite3_reset(artifact->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(artifact->Value) != SQLITE_OK
				|| !bind_fixed(
					artifact->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(artifact->Value, 2, aRequest.Operation)
				|| ::sqlite3_bind_int(
					   artifact->Value, 3, static_cast<int>(value.Role))
					!= SQLITE_OK
				|| !bind_bytes(artifact->Value, 4, value.Identity.Bytes)
				|| ::sqlite3_bind_int64(
					   artifact->Value, 5, static_cast<std::int64_t>(value.Bytes))
					!= SQLITE_OK
				|| ::sqlite3_step(artifact->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
		}
		auto advance = prepare(
			database,
			"UPDATE operation SET phase = 3, last_evidence = 1, "
			"operation_revision = operation_revision + 1 "
			"WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
			"AND phase = 2 AND operation_revision = ?3 "
			"AND cache_scope = ?4 AND pr_scope = ?5 "
			"AND operation_revision < 9223372036854775806");
		if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(advance->Value, 2, aRequest.Operation)
			|| ::sqlite3_bind_int64(
				   advance->Value, 3, aRequest.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
			|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
			|| ::sqlite3_step(advance->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		auto output = load_append(aRequest.Operation);
		if (!output) {
			return std::unexpected(output.error());
		}
#if defined(BPR_SQLITE_TESTING)
		observe_journal_transaction(detail::testing::epoch_journal_stage::installation);
#endif
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::record_candidate(
		const app::record_candidate_request& aRequest,
		std::stop_token                      aStop) -> result<app::append_operation_record>
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
		auto existing = load_append(aRequest.Operation);
		if (!existing) {
			return std::unexpected(existing.error());
		}
		const auto& epoch             = aRequest.Candidate.Epoch;
		const auto  candidateCoherent = coherent_candidate_review(
			aRequest.Candidate.Review,
			existing->Context.Revision.CurrentViewerReview,
			Binding.Authority.Viewer);
		const auto materialCoherent = epoch.Material && existing->Install
			&& epoch.Material->BaseTree == existing->Install->BaseTree
			&& epoch.Material->HeadTree == existing->Install->HeadTree;
		const auto provenanceCoherent = (existing->Context.Revision.Origin
										 == core::pull_request_revision_origin::explicit_historical_import)
			== (epoch.Cause.Origin
				== core::epoch_origin::explicit_historical_import);
		if (!candidateCoherent || !materialCoherent || !provenanceCoherent
			|| epoch.BaseAvailability != core::anchor_availability::available
			|| epoch.HeadAvailability != core::anchor_availability::available) {
			return std::unexpected(failure::operation_conflict);
		}
		if (aRequest.HistoricalOccurrence) {
			if (!aRequest.HistoricalOccurrence->valid() || epoch.Cause.Origin != core::epoch_origin::explicit_historical_import
				|| aRequest.Candidate.Review) {
				return std::unexpected(failure::invalid_request);
			}
			auto history = read_occurrence_rows(database, Binding, aStop);
			if (!history) {
				return std::unexpected(history.error());
			}
			const auto found = std::ranges::find(history->Occurrences, *aRequest.HistoricalOccurrence, &core::epoch_occurrence::Id);
			if (found == history->Occurrences.end() || found->LocalEpoch || found->Evidence.Head != epoch.Head) {
				return std::unexpected(failure::operation_conflict);
			}
		}
		if (aRequest.Candidate.Review) {
			auto canonical = canonicalize_review(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				*aRequest.Candidate.Review,
				std::pair{ epoch.Id.Ordinal, epoch.Head });
			if (!canonical) {
				return std::unexpected(canonical.error());
			}
			if (canonical->Review != *aRequest.Candidate.Review) {
				return std::unexpected(failure::operation_conflict);
			}
		}
		if (existing->Phase == app::epoch_catalog_operation_phase::commit_authorized
			&& existing->Candidate == aRequest.Candidate
			&& existing->HistoricalOccurrence == aRequest.HistoricalOccurrence
			&& existing->OperationRevision.value()
				== aRequest.ExpectedOperationRevision.value() + 1) {
			return existing;
		}
		if (existing->Phase != app::epoch_catalog_operation_phase::staging_ready
			|| existing->OperationRevision != aRequest.ExpectedOperationRevision
			|| !existing->Install || epoch.Id.Authority != Binding.Authority.Scope
			|| epoch.Id.Ordinal != existing->ReservedOrdinal
			|| epoch.Source != existing->Context.Revision.Source
			|| epoch.BaseBranch != existing->Context.Revision.BaseBranch
			|| epoch.Base != existing->Context.Revision.Base
			|| epoch.Head != existing->Context.Revision.Head) {
			return std::unexpected(failure::operation_conflict);
		}
		auto insert = prepare(
			database,
			"INSERT INTO append_hidden_candidate(incarnation, operation, "
			"material_base_algorithm, material_base_digest, "
			"material_head_algorithm, material_head_digest, "
			"material_policy_revision, observed_at, origin, base_transition, "
			"head_transition, material_relation, primary_cause, "
			"base_availability, head_availability, candidate_review_present) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, "
			"?12, ?13, ?14, ?15, ?16)");
		if (!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(insert->Value, 2, aRequest.Operation)) {
			return std::unexpected(insert ? failure::unavailable : insert.error());
		}
		const auto& material = *epoch.Material;
		if (!bind_oid(insert->Value, 3, 4, material.BaseTree)
			|| !bind_oid(insert->Value, 5, 6, material.HeadTree)
			|| !bind_u64(insert->Value, 7, material.PolicyRevision.value())) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_bind_int64(
				insert->Value, 8, epoch.ObservedAt.value())
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 9, stored_enum(epoch.Cause.Origin))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 10, stored_enum(epoch.Cause.Base))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 11, stored_enum(epoch.Cause.Head))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 12, stored_enum(epoch.Cause.Material))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 13, stored_enum(epoch.Cause.Primary))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 14, stored_enum(epoch.BaseAvailability))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 15, stored_enum(epoch.HeadAvailability))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   insert->Value, 16, aRequest.Candidate.Review ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		if (aRequest.HistoricalOccurrence) {
			auto occurrence = prepare(database, "INSERT INTO append_occurrence(incarnation, operation, occurrence_id) VALUES(?1, ?2, ?3)");
			if (!occurrence || !bind_fixed(occurrence->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(occurrence->Value, 2, aRequest.Operation)
				|| !bind_fixed(occurrence->Value, 3, *aRequest.HistoricalOccurrence)
				|| ::sqlite3_step(occurrence->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
		}
		if (aRequest.Candidate.Review) {
			const auto& review       = *aRequest.Candidate.Review;
			auto        reviewInsert = prepare(
				database,
				"INSERT INTO append_candidate_review(incarnation, operation, "
				"association, review_id, actor_id, review_order, review_state, "
				"commit_algorithm, commit_digest, availability) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
			if (!reviewInsert
				|| !bind_fixed(
					reviewInsert->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(reviewInsert->Value, 2, aRequest.Operation)
				|| !bind_fixed(reviewInsert->Value, 3, review.Association)
				|| !bind_u64(reviewInsert->Value, 4, review.Review.value())
				|| !bind_u64(reviewInsert->Value, 5, review.Actor.value())
				|| ::sqlite3_bind_int(
					   reviewInsert->Value, 6, review.Order.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   reviewInsert->Value, 7, stored_enum(review.State))
					!= SQLITE_OK
				|| !bind_oid(reviewInsert->Value, 8, 9, review.Commit)
				|| ::sqlite3_bind_int(
					   reviewInsert->Value,
					   10,
					   stored_enum(review.Availability))
					!= SQLITE_OK
				|| ::sqlite3_step(reviewInsert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
		}
		auto advance = prepare(
			database,
			"UPDATE operation SET phase = 5, "
			"operation_revision = operation_revision + 1, "
			"last_evidence = 3 WHERE incarnation = ?1 AND operation = ?2 "
			"AND kind = 1 AND phase = 4 AND operation_revision = ?3 "
			"AND cache_scope = ?4 AND pr_scope = ?5 "
			"AND operation_revision < 9223372036854775806");
		if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(advance->Value, 2, aRequest.Operation)
			|| ::sqlite3_bind_int64(
				   advance->Value, 3, aRequest.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
			|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
			|| ::sqlite3_step(advance->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		auto output = load_append(aRequest.Operation);
		if (!output) {
			return std::unexpected(output.error());
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::load_maintenance(
		const core::epoch_operation_id aOperation)
		-> result<app::maintenance_operation_record>
	{
		auto* database = State->Database.handle();
		auto  query    = prepare(
			database,
			"SELECT o.kind, o.phase, o.operation_revision, o.repair_from_phase, "
			"o.cache_scope, "
			"o.detail_count, o.retained_epoch_count "
			"FROM operation o WHERE o.incarnation = ?1 "
			"AND o.operation = ?2 AND ("
			"(o.kind = 2 AND o.cache_scope = ?3 AND o.pr_scope = ?4) OR "
			"(o.kind = 3 AND o.pr_scope IS NULL AND EXISTS("
			"SELECT 1 FROM repository_scope current "
			"JOIN repository_scope owned "
			"ON owned.incarnation = current.incarnation "
			"AND owned.profile_id = current.profile_id "
			"AND owned.profile_revision = current.profile_revision "
			"AND owned.host_id = current.host_id "
			"AND owned.host_authority = current.host_authority "
			"AND owned.account_id = current.account_id "
			"AND owned.access_mode = current.access_mode "
			"AND owned.repository_id = current.repository_id "
			"AND owned.object_format = current.object_format "
			"WHERE current.incarnation = o.incarnation "
			"AND current.cache_scope = ?3 "
			"AND owned.cache_scope = o.cache_scope)))");
		if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(query->Value, 2, aOperation)
			|| !bind_fixed(query->Value, 3, Binding.Authority.Cache)
			|| !bind_fixed(query->Value, 4, Binding.Authority.Scope)
			|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
			return std::unexpected(
				query ? failure::operation_conflict : query.error());
		}
		auto                              cache               = read_fixed<core::repository_cache_scope_id>(query->Value, 4);
		const auto                        expectedDetailCount = ::sqlite3_column_int64(query->Value, 5);
		const auto                        retainedEpochCount  = ::sqlite3_column_int64(query->Value, 6);
		app::maintenance_operation_record output{
			.Incarnation = Binding.Catalog.Incarnation,
			.Operation   = aOperation,
			.Kind        = static_cast<app::epoch_catalog_operation_kind>(
				::sqlite3_column_int(query->Value, 0)),
			.Phase = static_cast<app::epoch_catalog_operation_phase>(
				::sqlite3_column_int(query->Value, 1)),
			.RepairFromPhase   = ::sqlite3_column_type(query->Value, 3) == SQLITE_NULL
				? std::nullopt
				: std::optional{ static_cast<app::epoch_catalog_operation_phase>(
					  ::sqlite3_column_int(query->Value, 3)) },
			.OperationRevision = app::epoch_catalog_operation_revision{
				::sqlite3_column_int64(query->Value, 2) },
		};
		if (!cache || expectedDetailCount < 1
			|| expectedDetailCount
				> static_cast<std::int64_t>(
					app::maximum_cache_delete_scope_count)
			|| retainedEpochCount < 0
			|| !app::valid(output.OperationRevision)
			|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		output.Cache                             = *cache;
		std::uint64_t observedRetainedEpochCount = 0U;
		if (output.Kind == app::epoch_catalog_operation_kind::prune) {
			auto rows = prepare(
				database,
				"SELECT pr_scope, ordinal, base_algorithm, base_digest, "
				"base_availability, head_algorithm, head_digest, "
				"head_availability FROM prune_operation "
				"WHERE incarnation = ?1 AND operation = ?2 "
				"ORDER BY pr_scope, ordinal LIMIT 101");
			if (!rows || !bind_fixed(rows->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(rows->Value, 2, aOperation)) {
				return std::unexpected(rows ? failure::unavailable : rows.error());
			}
			while (true) {
				const auto code = ::sqlite3_step(rows->Value);
				if (code == SQLITE_DONE) {
					break;
				}
				if (code != SQLITE_ROW) {
					return std::unexpected(sql_failure(database, { }));
				}
				const auto authority = read_fixed<core::pull_request_authority_scope_id>(
					rows->Value, 0);
				const auto ordinal = core::epoch_ordinal{
					::sqlite3_column_int64(rows->Value, 1)
				};
				const auto base             = read_oid<core::git_commit_oid>(rows->Value, 2, 3);
				const auto head             = read_oid<core::git_commit_oid>(rows->Value, 5, 6);
				const auto baseAvailability = restored_enum<core::anchor_availability>(
					::sqlite3_column_int(rows->Value, 4));
				const auto headAvailability = restored_enum<core::anchor_availability>(
					::sqlite3_column_int(rows->Value, 7));
				if (!authority || *authority != Binding.Authority.Scope
					|| !core::valid(ordinal) || !base || !head
					|| ::sqlite3_column_type(rows->Value, 4) != SQLITE_INTEGER
					|| ::sqlite3_column_type(rows->Value, 7) != SQLITE_INTEGER
					|| !app::valid_prune_anchor_availability(baseAvailability)
					|| !app::valid_prune_anchor_availability(headAvailability)) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (output.Epochs.size() >= app::maximum_prune_epoch_count) {
					return std::unexpected(failure::schema_mismatch);
				}
				output.Epochs.push_back(app::prune_epoch_fact{
					.Epoch = core::review_epoch_id{
						.Authority   = *authority,
						.Incarnation = Binding.Catalog.Incarnation,
						.Ordinal     = ordinal,
					},
					.Retained = app::git_retained_ref_pair{
						.Scope   = *authority,
						.Ordinal = ordinal,
						.Base    = *base,
						.Head    = *head,
					},
					.BaseRef = app::epoch_catalog_ref_identity{
						.Authority = *authority,
						.Ordinal   = ordinal,
					},
					.HeadRef = app::epoch_catalog_ref_identity{
						.Authority = *authority,
						.Ordinal   = ordinal,
						.Head      = true,
					},
					.ExpectedBaseAvailability = baseAvailability,
					.ExpectedHeadAvailability = headAvailability,
				});
			}
		} else if (output.Kind == app::epoch_catalog_operation_kind::cache_delete) {
			auto rows = prepare(
				database,
				"SELECT cache_scope, pr_scope, expected_revision, epoch_count "
				"FROM cache_delete_scope "
				"WHERE incarnation = ?1 AND operation = ?2 "
				"ORDER BY pr_scope LIMIT 10001");
			if (!rows || !bind_fixed(rows->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(rows->Value, 2, aOperation)) {
				return std::unexpected(rows ? failure::unavailable : rows.error());
			}
			while (true) {
				const auto code = ::sqlite3_step(rows->Value);
				if (code == SQLITE_DONE) {
					break;
				}
				if (code != SQLITE_ROW) {
					return std::unexpected(sql_failure(database, { }));
				}
				auto rowCache = read_fixed<core::repository_cache_scope_id>(
					rows->Value, 0);
				auto authority = read_fixed<core::pull_request_authority_scope_id>(
					rows->Value, 1);
				const auto revision   = ::sqlite3_column_int64(rows->Value, 2);
				const auto epochCount = ::sqlite3_column_int64(rows->Value, 3);
				if (!rowCache || *rowCache != output.Cache || !authority
					|| revision < 0 || epochCount < 0
					|| observedRetainedEpochCount
						> static_cast<std::uint64_t>(
							  std::numeric_limits<std::int64_t>::max())
							- static_cast<std::uint64_t>(epochCount)) {
					return std::unexpected(failure::schema_mismatch);
				}
				observedRetainedEpochCount += static_cast<std::uint64_t>(epochCount);
				if (output.Scopes.size()
					>= app::maximum_cache_delete_scope_count) {
					return std::unexpected(failure::schema_mismatch);
				}
				output.Scopes.push_back(app::cache_delete_scope_fact{
					.Authority        = *authority,
					.ExpectedRevision = core::epoch_catalog_revision{ revision },
				});
			}
		} else {
			return std::unexpected(failure::schema_mismatch);
		}
		const auto actualDetailCount = output.Kind
				== app::epoch_catalog_operation_kind::prune
			? output.Epochs.size()
			: output.Scopes.size();
		if (actualDetailCount
			!= static_cast<std::size_t>(expectedDetailCount)) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (
			output.Kind == app::epoch_catalog_operation_kind::prune
			&& (expectedDetailCount
					> static_cast<std::int64_t>(app::maximum_prune_epoch_count)
				|| retainedEpochCount != 0)) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (
			output.Kind == app::epoch_catalog_operation_kind::cache_delete
			&& observedRetainedEpochCount
				!= static_cast<std::uint64_t>(retainedEpochCount)) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (!app::valid(output)) {
			return std::unexpected(failure::schema_mismatch);
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::transition_operation(
		const app::operation_transition_request& aRequest,
		std::stop_token                          aStop) -> result<app::epoch_recovery_operation>
	{
		if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
			return std::unexpected(failure::invalid_request);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, { } };
		transaction      write{ database, { } };
		if (auto begun = write.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto       currentAppend      = aRequest.Kind == app::epoch_catalog_operation_kind::append
			? load_append(aRequest.Operation)
			: result<app::append_operation_record>{
				  std::unexpected(failure::operation_conflict)
			  };
		auto       currentMaintenance = aRequest.Kind != app::epoch_catalog_operation_kind::append
			? load_maintenance(aRequest.Operation)
			: result<app::maintenance_operation_record>{
				  std::unexpected(failure::operation_conflict)
			  };
		const auto phase              = currentAppend ? currentAppend->Phase
			: currentMaintenance                      ? currentMaintenance->Phase
													  : aRequest.Expected;
		const auto revision           = currentAppend ? currentAppend->OperationRevision
			: currentMaintenance                      ? currentMaintenance->OperationRevision
													  : app::epoch_catalog_operation_revision{ };
		if (!currentAppend && !currentMaintenance) {
			return std::unexpected(failure::operation_conflict);
		}
		if (currentMaintenance && currentMaintenance->Kind != aRequest.Kind) {
			return std::unexpected(failure::operation_conflict);
		}
		if (aStop.stop_requested() && !app::point_of_no_cancellation(phase)) {
			return std::unexpected(failure::cancelled);
		}
		if (phase == aRequest.Next
			&& revision.value() == aRequest.ExpectedOperationRevision.value() + 1) {
			if (currentAppend) {
				return app::epoch_recovery_operation{ *currentAppend };
			}
			return app::epoch_recovery_operation{ *currentMaintenance };
		}
		if (phase != aRequest.Expected
			|| revision != aRequest.ExpectedOperationRevision) {
			return std::unexpected(failure::phase_conflict);
		}
		if (currentAppend) {
			if (aRequest.Next == app::epoch_catalog_operation_phase::staging_ready
				&& !currentAppend->Install) {
				return std::unexpected(failure::phase_conflict);
			}
		}
		if (aRequest.Kind != app::epoch_catalog_operation_kind::cache_delete) {
			if (auto active = require_active_pull_request_scope(
					database, Binding, aRequest.Catalog.Revision);
				!active) {
				return std::unexpected(active.error());
			}
		}
		if (aRequest.Next == app::epoch_catalog_operation_phase::repair_required) {
			const auto repairingTombstonedCache = aRequest.Kind
					== app::epoch_catalog_operation_kind::cache_delete
				&& aRequest.Expected
					== app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending;
			if (repairingTombstonedCache) {
				for (const auto& scopeFact : currentMaintenance->Scopes) {
					auto scope = load_pull_request_scope_state(
						database,
						Binding.Catalog.Incarnation,
						scopeFact.Authority);
					if (!scope || scope->Cache != currentMaintenance->Cache
						|| scope->Lifecycle != 3
						|| scope->RepositoryLifecycle != 3
						|| scope->RetainedEpochCount != 0U) {
						return std::unexpected(
							scope ? failure::operation_conflict : scope.error());
					}
				}
			}
			auto failClosed = prepare(
				database,
				aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
					? repairingTombstonedCache
						? "UPDATE repository_scope SET lifecycle = 3 "
						  "WHERE incarnation = ?1 AND cache_scope = ?2 "
						  "AND lifecycle = 3"
						: "UPDATE repository_scope SET lifecycle = 2 "
						  "WHERE incarnation = ?1 AND cache_scope = ?2 "
						  "AND lifecycle IN (1, 2)"
					: "UPDATE pull_request_scope SET lifecycle = 4 "
					  "WHERE incarnation = ?1 AND pr_scope = ?2 "
					  "AND cache_scope = ?3 AND lifecycle = 1");
			if (!failClosed) {
				return std::unexpected(failClosed.error());
			}
			const auto authorityBound = aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
				? bind_fixed(
					  failClosed->Value, 2, Binding.Authority.Cache)
				: bind_fixed(
					  failClosed->Value, 2, Binding.Authority.Scope)
					&& bind_fixed(
						failClosed->Value, 3, Binding.Authority.Cache);
			if (!bind_fixed(
					failClosed->Value, 1, Binding.Catalog.Incarnation)
				|| !authorityBound
				|| ::sqlite3_step(failClosed->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
		}
		if (
			aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
			&& aRequest.Expected
				== app::epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending
			&& aRequest.Next == app::epoch_catalog_operation_phase::complete) {
			auto retire = prepare(
				database,
				"UPDATE repository_scope SET lifecycle = 4 "
				"WHERE incarnation = ?1 AND cache_scope = ?2 "
				"AND lifecycle = 3");
			if (!retire
				|| !bind_fixed(
					retire->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(retire->Value, 2, Binding.Authority.Cache)
				|| ::sqlite3_step(retire->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
		}
		if (
			aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
			&& aRequest.Expected
				== app::epoch_catalog_operation_phase::abort_requested
			&& aRequest.Next == app::epoch_catalog_operation_phase::aborted) {
			auto reactivate = prepare(
				database,
				"UPDATE repository_scope SET lifecycle = 1 "
				"WHERE incarnation = ?1 AND cache_scope = ?2 "
				"AND lifecycle = 2");
			if (!reactivate
				|| !bind_fixed(
					reactivate->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(
					reactivate->Value, 2, Binding.Authority.Cache)
				|| ::sqlite3_step(reactivate->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(
					reactivate ? failure::operation_conflict
							   : reactivate.error());
			}
		}
		if (
			currentAppend
			&& (aRequest.Next == app::epoch_catalog_operation_phase::complete
				|| aRequest.Next
					== app::epoch_catalog_operation_phase::aborted)) {
			auto inventory = prepare(
				database,
				"DELETE FROM append_inventory WHERE incarnation = ?1 "
				"AND operation = ?2");
			if (!inventory
				|| !bind_fixed(
					inventory->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(inventory->Value, 2, aRequest.Operation)
				|| ::sqlite3_step(inventory->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database)
					!= static_cast<int>(
						currentAppend->Preimport
							? currentAppend->Preimport->Packs.size()
							: 0U)) {
				return std::unexpected(failure::operation_conflict);
			}
			auto receipt = prepare(
				database,
				"DELETE FROM append_receipt WHERE incarnation = ?1 "
				"AND operation = ?2");
			if (!receipt
				|| !bind_fixed(
					receipt->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(receipt->Value, 2, aRequest.Operation)
				|| ::sqlite3_step(receipt->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database)
					!= (currentAppend->Spool ? 1 : 0)) {
				return std::unexpected(failure::operation_conflict);
			}
			auto receiving = prepare(
				database,
				"DELETE FROM append_receiving WHERE incarnation = ?1 "
				"AND operation = ?2");
			if (!receiving
				|| !bind_fixed(
					receiving->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(receiving->Value, 2, aRequest.Operation)
				|| ::sqlite3_step(receiving->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database)
					!= (currentAppend->Receiving ? 1 : 0)) {
				return std::unexpected(failure::operation_conflict);
			}
		}
		const auto terminal = aRequest.Next == app::epoch_catalog_operation_phase::complete ? 1
			: aRequest.Next == app::epoch_catalog_operation_phase::aborted                  ? 2
			: aRequest.Next == app::epoch_catalog_operation_phase::repair_required
			? 3
			: 0;
		auto       update   = prepare(
			database,
			"UPDATE operation SET phase = ?1, last_evidence = ?2, "
			"operation_revision = operation_revision + 1, terminal = ?3 "
			", repair_from_phase = CASE WHEN ?1 = 12 THEN ?7 ELSE NULL END "
			", recovery_input_compacted = CASE "
			"WHEN ?6 = 1 AND ?1 IN (9, 11) THEN 1 ELSE 0 END "
			"WHERE incarnation = ?4 AND operation = ?5 AND kind = ?6 "
			"AND phase = ?7 AND operation_revision = ?8 "
			"AND cache_scope = ?9 AND ((kind IN (1, 2) AND pr_scope = ?10) "
			"OR (kind = 3 AND pr_scope IS NULL)) "
			"AND operation_revision < ?11");
		if (!update
			|| ::sqlite3_bind_int(
				   update->Value, 1, static_cast<int>(aRequest.Next))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   update->Value, 2, static_cast<int>(aRequest.Evidence))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(update->Value, 3, terminal) != SQLITE_OK
			|| !bind_fixed(update->Value, 4, Binding.Catalog.Incarnation)
			|| !bind_operation(update->Value, 5, aRequest.Operation)
			|| ::sqlite3_bind_int(
				   update->Value, 6, static_cast<int>(aRequest.Kind))
				!= SQLITE_OK
			|| ::sqlite3_bind_int(
				   update->Value, 7, static_cast<int>(aRequest.Expected))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   update->Value, 8, aRequest.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(update->Value, 9, Binding.Authority.Cache)
			|| !bind_fixed(update->Value, 10, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   update->Value,
				   11,
				   app::terminal(aRequest.Next)
					   ? app::maximum_epoch_catalog_operation_revision
					   : app::maximum_epoch_catalog_operation_revision - 1)
				!= SQLITE_OK
			|| ::sqlite3_step(update->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::phase_conflict);
		}
		result<app::epoch_recovery_operation> loaded = std::unexpected(failure::operation_conflict);
		if (aRequest.Kind == app::epoch_catalog_operation_kind::append) {
			auto append = load_append(aRequest.Operation);
			if (!append) {
				return std::unexpected(append.error());
			}
			loaded = app::epoch_recovery_operation{ *append };
		} else {
			auto maintenance = load_maintenance(aRequest.Operation);
			if (!maintenance) {
				return std::unexpected(maintenance.error());
			}
			loaded = app::epoch_recovery_operation{ *maintenance };
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return loaded;
	}

	auto sqlite_epoch_catalog_session::recovery_page(
		const std::optional<core::epoch_operation_id> aAfter,
		const std::uint16_t                           aLimit,
		std::stop_token                               aStop) -> result<app::epoch_recovery_page>
	{
		if ((aAfter && !aAfter->valid()) || aLimit == 0U
			|| aLimit > app::maximum_epoch_recovery_page_size) {
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
		auto query = prepare(
			database,
			aAfter
				? "SELECT o.operation, o.kind FROM operation o "
				  "WHERE o.incarnation = ?1 AND o.terminal NOT IN (1, 2) "
				  "AND ((o.kind IN (1, 2) AND o.cache_scope = ?2 "
				  "AND o.pr_scope = ?3) OR (o.kind = 3 "
				  "AND o.pr_scope IS NULL AND EXISTS(SELECT 1 "
				  "FROM repository_scope current JOIN repository_scope owned "
				  "ON owned.incarnation = current.incarnation "
				  "AND owned.profile_id = current.profile_id "
				  "AND owned.profile_revision = current.profile_revision "
				  "AND owned.host_id = current.host_id "
				  "AND owned.host_authority = current.host_authority "
				  "AND owned.account_id = current.account_id "
				  "AND owned.access_mode = current.access_mode "
				  "AND owned.repository_id = current.repository_id "
				  "AND owned.object_format = current.object_format "
				  "WHERE current.incarnation = o.incarnation "
				  "AND current.cache_scope = ?2 "
				  "AND owned.cache_scope = o.cache_scope))) "
				  "AND o.operation >= ?4 ORDER BY o.operation LIMIT ?5"
				: "SELECT o.operation, o.kind FROM operation o "
				  "WHERE o.incarnation = ?1 AND o.terminal NOT IN (1, 2) "
				  "AND ((o.kind IN (1, 2) AND o.cache_scope = ?2 "
				  "AND o.pr_scope = ?3) OR (o.kind = 3 "
				  "AND o.pr_scope IS NULL AND EXISTS(SELECT 1 "
				  "FROM repository_scope current JOIN repository_scope owned "
				  "ON owned.incarnation = current.incarnation "
				  "AND owned.profile_id = current.profile_id "
				  "AND owned.profile_revision = current.profile_revision "
				  "AND owned.host_id = current.host_id "
				  "AND owned.host_authority = current.host_authority "
				  "AND owned.account_id = current.account_id "
				  "AND owned.access_mode = current.access_mode "
				  "AND owned.repository_id = current.repository_id "
				  "AND owned.object_format = current.object_format "
				  "WHERE current.incarnation = o.incarnation "
				  "AND current.cache_scope = ?2 "
				  "AND owned.cache_scope = o.cache_scope))) "
				  "ORDER BY o.operation LIMIT ?4");
		if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(query->Value, 2, Binding.Authority.Cache)
			|| !bind_fixed(query->Value, 3, Binding.Authority.Scope)) {
			return std::unexpected(query ? failure::unavailable : query.error());
		}
		const auto fetch = static_cast<std::int64_t>(aLimit) + 1;
		if (aAfter) {
			if (!bind_operation(query->Value, 4, *aAfter)
				|| ::sqlite3_bind_int64(query->Value, 5, fetch) != SQLITE_OK) {
				return std::unexpected(failure::unavailable);
			}
		} else if (::sqlite3_bind_int64(query->Value, 4, fetch) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		std::vector<std::pair<core::epoch_operation_id, int>> identities;
		while (true) {
			const auto code = ::sqlite3_step(query->Value);
			if (code == SQLITE_DONE) {
				break;
			}
			if (code != SQLITE_ROW || aStop.stop_requested()) {
				return std::unexpected(sql_failure(database, aStop));
			}
			auto operation = read_fixed<core::epoch_operation_id>(query->Value, 0);
			if (!operation) {
				return std::unexpected(failure::schema_mismatch);
			}
			identities.emplace_back(
				*operation, ::sqlite3_column_int(query->Value, 1));
		}
		app::epoch_recovery_page output{
			.Incarnation = Binding.Catalog.Incarnation,
		};
		if (identities.size() > aLimit) {
			output.Continuation = identities.back().first;
			identities.pop_back();
		}
		output.Operations.reserve(identities.size());
		for (const auto& [operation, kind] : identities) {
			if (kind == static_cast<int>(app::epoch_catalog_operation_kind::append)) {
				auto loaded = load_append(operation);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				output.Operations.emplace_back(*loaded);
			} else {
				auto loaded = load_maintenance(operation);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				output.Operations.emplace_back(*loaded);
			}
		}
		if (!app::valid(output)) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (auto committed = read.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}
}
