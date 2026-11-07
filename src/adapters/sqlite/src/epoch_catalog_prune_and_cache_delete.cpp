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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::sqlite::epoch_catalog_detail {
	namespace {
		[[nodiscard]] auto linked_review_count(
			::sqlite3*                                aDatabase,
			const core::epoch_catalog_incarnation_id& aIncarnation,
			const app::prune_epoch_fact&              aFact,
			const std::string_view                    aPredicate) -> result<std::int64_t>
		{
			auto query = prepare(
				aDatabase,
				std::string{
					"SELECT count(*) FROM review_association "
					"WHERE incarnation = ?1 AND pr_scope = ?2 "
					"AND linked_ordinal = ?3 AND commit_algorithm = ?4 "
					"AND commit_digest = ?5 AND " }
					+ std::string{ aPredicate });
			if (!query || !bind_fixed(query->Value, 1, aIncarnation)
				|| !bind_fixed(query->Value, 2, aFact.Epoch.Authority)
				|| ::sqlite3_bind_int64(
					   query->Value, 3, aFact.Epoch.Ordinal.value())
					!= SQLITE_OK
				|| !bind_oid(query->Value, 4, 5, aFact.Retained.Head)
				|| ::sqlite3_step(query->Value) != SQLITE_ROW
				|| ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			const auto count = ::sqlite3_column_int64(query->Value, 0);
			if (count < 0 || ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return count;
		}

		[[nodiscard]] auto load_cache_impact(
			::sqlite3*                                aDatabase,
			const core::epoch_catalog_incarnation_id& aIncarnation,
			const core::repository_cache_scope_id&    aCache)
			-> result<app::cache_delete_impact>
		{
			app::cache_delete_impact output{
				.Incarnation = aIncarnation,
				.Cache       = aCache,
			};
			auto scopes = prepare(
				aDatabase,
				"SELECT p.pr_scope, p.catalog_revision, p.retained_epoch_count, "
				"(SELECT count(*) FROM epoch e "
				" JOIN epoch_anchor a ON a.incarnation = e.incarnation "
				" AND a.pr_scope = e.pr_scope AND a.ordinal = e.ordinal "
				" AND a.endpoint = 1 AND a.availability NOT IN (3, 4) "
				" WHERE e.incarnation = p.incarnation AND e.pr_scope = p.pr_scope), "
				"p.lifecycle, r.lifecycle "
				"FROM pull_request_scope p JOIN repository_scope r "
				"ON r.incarnation = p.incarnation AND r.cache_scope = p.cache_scope "
				"WHERE p.incarnation = ?1 AND p.cache_scope = ?2 "
				"ORDER BY p.pr_scope LIMIT 10001");
			if (!scopes || !bind_fixed(scopes->Value, 1, aIncarnation)
				|| !bind_fixed(scopes->Value, 2, aCache)) {
				return std::unexpected(scopes ? failure::unavailable : scopes.error());
			}
			while (true) {
				const auto code = ::sqlite3_step(scopes->Value);
				if (code == SQLITE_DONE) {
					break;
				}
				if (code != SQLITE_ROW) {
					return std::unexpected(sql_failure(aDatabase, { }));
				}
				auto authority = read_fixed<core::pull_request_authority_scope_id>(
					scopes->Value, 0);
				const auto revision            = ::sqlite3_column_int64(scopes->Value, 1);
				const auto retained            = ::sqlite3_column_int64(scopes->Value, 2);
				const auto count               = ::sqlite3_column_int64(scopes->Value, 3);
				const auto prLifecycle         = ::sqlite3_column_int(scopes->Value, 4);
				const auto repositoryLifecycle = ::sqlite3_column_int(scopes->Value, 5);
				if (!authority || revision < 0 || retained < 0 || count < 0
					|| retained != count
					|| output.RetainedEpochCount
						> std::numeric_limits<std::uint64_t>::max()
							- static_cast<std::uint64_t>(count)) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (repositoryLifecycle != 1 && repositoryLifecycle != 2) {
					return std::unexpected(failure::authority_mismatch);
				}
				if (prLifecycle != 1 && prLifecycle != 4) {
					return std::unexpected(failure::repair_required);
				}
				if (output.Scopes.size() >= app::maximum_cache_delete_scope_count) {
					return std::unexpected(failure::schema_mismatch);
				}
				output.Scopes.push_back(app::cache_delete_scope_fact{
					.Authority        = *authority,
					.ExpectedRevision = core::epoch_catalog_revision{ revision },
				});
				output.RetainedEpochCount += static_cast<std::uint64_t>(count);
			}
			if (!app::valid(output)) {
				return std::unexpected(failure::authority_mismatch);
			}
			return output;
		}
	}

	auto sqlite_epoch_catalog_session::begin_prune(
		const app::begin_prune_request& aRequest,
		std::stop_token                 aStop) -> result<app::maintenance_operation_record>
	{
		if (!app::valid(aRequest)
			|| aRequest.Catalog.Incarnation != Binding.Catalog.Incarnation
			|| std::ranges::any_of(aRequest.Epochs, [this](const auto& aValue) {
				   return aValue.Epoch.Authority != Binding.Authority.Scope;
			   })) {
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
		const auto verifyAnchors = [&](const auto& aFacts, const bool aFinalized)
			-> result<void> {
			for (const auto& fact : aFacts) {
				auto epoch = load_epoch_summary(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					fact.Epoch.Ordinal);
				if (!epoch || epoch->Base != fact.Retained.Base
					|| epoch->Head != fact.Retained.Head
					|| epoch->BaseAvailability
						!= (aFinalized
								? core::anchor_availability::pruned
								: fact.ExpectedBaseAvailability)
					|| epoch->HeadAvailability
						!= (aFinalized
								? core::anchor_availability::pruned
								: fact.ExpectedHeadAvailability)) {
					return std::unexpected(failure::authority_mismatch);
				}
				if (aFinalized) {
					auto nonprunedReviews = linked_review_count(
						database,
						Binding.Catalog.Incarnation,
						fact,
						"availability != 3");
					if (!nonprunedReviews || *nonprunedReviews != 0) {
						return std::unexpected(failure::operation_conflict);
					}
				}
			}
			return { };
		};
		auto existing = load_maintenance(aRequest.Operation);
		if (existing) {
			if (existing->Kind == app::epoch_catalog_operation_kind::prune
				&& existing->Epochs == aRequest.Epochs) {
				auto expected = prepare(
					database,
					"SELECT expected_revision FROM operation "
					"WHERE incarnation = ?1 AND operation = ?2 AND kind = 2 "
					"AND cache_scope = ?3 AND pr_scope = ?4");
				if (!expected
					|| !bind_fixed(
						expected->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(expected->Value, 2, aRequest.Operation)
					|| !bind_fixed(
						expected->Value, 3, Binding.Authority.Cache)
					|| !bind_fixed(
						expected->Value, 4, Binding.Authority.Scope)
					|| ::sqlite3_step(expected->Value) != SQLITE_ROW
					|| ::sqlite3_column_type(expected->Value, 0)
						!= SQLITE_INTEGER
					|| ::sqlite3_column_int64(expected->Value, 0)
						!= aRequest.Catalog.Revision.value()
					|| ::sqlite3_step(expected->Value) != SQLITE_DONE) {
					return std::unexpected(failure::operation_conflict);
				}
				const auto finalized = existing->Phase
						== app::epoch_catalog_operation_phase::
							visible_cleanup_pending
					|| existing->Phase
						== app::epoch_catalog_operation_phase::complete;
				if (finalized) {
					auto scope = load_pull_request_scope_state(
						database,
						Binding.Catalog.Incarnation,
						Binding.Authority.Scope);
					if (!scope || scope->Cache != Binding.Authority.Cache
						|| scope->Lifecycle != 1
						|| scope->RepositoryLifecycle != 1
						|| scope->Revision != Binding.Catalog.Revision
						|| !advanced_once(
							aRequest.Catalog.Revision, scope->Revision)) {
						return std::unexpected(failure::operation_conflict);
					}
				} else {
					if (aRequest.Catalog != Binding.Catalog) {
						return std::unexpected(failure::operation_conflict);
					}
					if (auto active = require_active_pull_request_scope(
							database, Binding, aRequest.Catalog.Revision, true);
						!active) {
						return std::unexpected(active.error());
					}
				}
				if (auto verified = verifyAnchors(existing->Epochs, finalized);
					!verified) {
					return std::unexpected(failure::operation_conflict);
				}
				return existing;
			}
			return std::unexpected(failure::operation_conflict);
		}
		if (aRequest.Catalog != Binding.Catalog) {
			return std::unexpected(failure::invalid_request);
		}
		if (auto active = require_active_pull_request_scope(
				database, Binding, aRequest.Catalog.Revision, true);
			!active) {
			return std::unexpected(active.error());
		}
		auto liveScope = prepare(
			database,
			"SELECT 1 FROM pull_request_scope WHERE incarnation = ?1 "
			"AND pr_scope = ?2 AND catalog_revision = ?3 AND lifecycle = 1");
		if (!liveScope
			|| !bind_fixed(
				liveScope->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(liveScope->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   liveScope->Value, 3, aRequest.Catalog.Revision.value())
				!= SQLITE_OK
			|| ::sqlite3_step(liveScope->Value) != SQLITE_ROW
			|| ::sqlite3_step(liveScope->Value) != SQLITE_DONE) {
			return std::unexpected(failure::revision_conflict);
		}
		if (auto verified = verifyAnchors(aRequest.Epochs, false); !verified) {
			return std::unexpected(verified.error());
		}
		auto operation = prepare(
			database,
			"INSERT INTO operation(incarnation, operation, kind, phase, "
			"last_evidence, operation_revision, cache_scope, pr_scope, "
			"expected_revision, application_session, request_id, "
			"epoch_generation, source_observation, source_snapshot, "
			"identity_generation, pull_request_generation, reserved_ordinal, "
			"detail_count, "
			"retained_epoch_count, terminal) "
			"VALUES(?1, ?2, 2, 13, 1, 1, ?3, ?4, ?5, ?6, NULL, NULL, "
			"NULL, NULL, NULL, NULL, NULL, ?7, 0, 0)");
		if (!operation
			|| !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(operation->Value, 2, aRequest.Operation)
			|| !bind_fixed(operation->Value, 3, Binding.Authority.Cache)
			|| !bind_fixed(operation->Value, 4, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   operation->Value, 5, aRequest.Catalog.Revision.value())
				!= SQLITE_OK
			|| !bind_u64(operation->Value, 6, ApplicationSession.value())
			|| ::sqlite3_bind_int64(
				   operation->Value,
				   7,
				   static_cast<std::int64_t>(aRequest.Epochs.size()))
				!= SQLITE_OK
			|| ::sqlite3_step(operation->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(
				database, aStop, failure::operation_conflict));
		}
		auto detail = prepare(
			database,
			"INSERT INTO prune_operation(incarnation, operation, pr_scope, "
			"ordinal, base_algorithm, base_digest, base_availability, "
			"head_algorithm, head_digest, head_availability, ref_format) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1)");
		if (!detail) {
			return std::unexpected(detail.error());
		}
		for (const auto& fact : aRequest.Epochs) {
			if (::sqlite3_reset(detail->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(detail->Value) != SQLITE_OK
				|| !bind_fixed(
					detail->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(detail->Value, 2, aRequest.Operation)
				|| !bind_fixed(detail->Value, 3, fact.Epoch.Authority)
				|| ::sqlite3_bind_int64(
					   detail->Value, 4, fact.Epoch.Ordinal.value())
					!= SQLITE_OK
				|| !bind_oid(detail->Value, 5, 6, fact.Retained.Base)
				|| ::sqlite3_bind_int(
					   detail->Value,
					   7,
					   stored_enum(fact.ExpectedBaseAvailability))
					!= SQLITE_OK
				|| !bind_oid(detail->Value, 8, 9, fact.Retained.Head)
				|| ::sqlite3_bind_int(
					   detail->Value,
					   10,
					   stored_enum(fact.ExpectedHeadAvailability))
					!= SQLITE_OK
				|| ::sqlite3_step(detail->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
		}
		auto output = load_maintenance(aRequest.Operation);
		if (!output) {
			return std::unexpected(output.error());
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::finalize_prune(
		const app::finalize_prune_request& aRequest,
		std::stop_token                    aStop) -> result<app::catalog_commit_result>
	{
		if (!app::valid(aRequest)
			|| aRequest.Transition.Catalog.Incarnation
				!= Binding.Catalog.Incarnation) {
			return std::unexpected(failure::invalid_request);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, { } };
		transaction      write{ database, { } };
		if (auto begun = write.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto operation = load_maintenance(aRequest.Transition.Operation);
		if (!operation
			|| operation->Kind != app::epoch_catalog_operation_kind::prune) {
			return std::unexpected(failure::phase_conflict);
		}
		std::vector<app::git_retained_ref_pair> expected;
		expected.reserve(operation->Epochs.size());
		for (const auto& epoch : operation->Epochs) {
			expected.push_back(epoch.Retained);
		}
		if (expected != aRequest.ObservedAbsent) {
			return std::unexpected(failure::operation_conflict);
		}
		if (
			operation->Phase
				== app::epoch_catalog_operation_phase::visible_cleanup_pending
			|| operation->Phase == app::epoch_catalog_operation_phase::complete) {
			auto scope = load_pull_request_scope_state(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope);
			if (!scope || scope->Cache != Binding.Authority.Cache
				|| scope->Lifecycle != 1 || scope->RepositoryLifecycle != 1
				|| !advanced_once(
					aRequest.Transition.Catalog.Revision, scope->Revision)
				|| !finalized_operation_revision_matches(
					operation->Phase,
					aRequest.Transition.ExpectedOperationRevision,
					operation->OperationRevision,
					app::epoch_catalog_operation_phase::
						visible_cleanup_pending)) {
				return std::unexpected(failure::operation_conflict);
			}
			for (const auto& fact : operation->Epochs) {
				auto epoch = load_epoch_summary(
					database,
					Binding.Catalog.Incarnation,
					fact.Epoch.Authority,
					fact.Epoch.Ordinal);
				if (!epoch || epoch->Base != fact.Retained.Base
					|| epoch->Head != fact.Retained.Head
					|| epoch->BaseAvailability
						!= core::anchor_availability::pruned
					|| epoch->HeadAvailability
						!= core::anchor_availability::pruned) {
					return std::unexpected(failure::operation_conflict);
				}
				auto nonprunedReviews = linked_review_count(
					database,
					Binding.Catalog.Incarnation,
					fact,
					"availability != 3");
				if (!nonprunedReviews || *nonprunedReviews != 0) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			Binding.Catalog.Revision = scope->Revision;
			return app::catalog_commit_result{
				.Catalog = Binding.Catalog,
				.Changed = true,
			};
		}
		if (aStop.stop_requested()
			&& !app::point_of_no_cancellation(operation->Phase)) {
			return std::unexpected(failure::cancelled);
		}
		if (aRequest.Transition.Catalog != Binding.Catalog) {
			return std::unexpected(failure::invalid_request);
		}
		if (auto active = require_active_pull_request_scope(
				database,
				Binding,
				aRequest.Transition.Catalog.Revision,
				true);
			!active) {
			return std::unexpected(active.error());
		}
		if (operation->Phase
				!= app::epoch_catalog_operation_phase::ref_delete_in_progress
			|| operation->OperationRevision
				!= aRequest.Transition.ExpectedOperationRevision) {
			return std::unexpected(failure::phase_conflict);
		}
		auto anchor = prepare(
			database,
			"UPDATE epoch_anchor SET availability = ?1 "
			"WHERE incarnation = ?2 AND pr_scope = ?3 AND ordinal = ?4 "
			"AND endpoint = ?5 AND object_algorithm = ?6 "
			"AND object_digest = ?7 AND availability = ?8");
		if (!anchor) {
			return std::unexpected(anchor.error());
		}
		auto reviews = prepare(
			database,
			"UPDATE review_association SET availability = ?1 "
			"WHERE incarnation = ?2 AND pr_scope = ?3 "
			"AND linked_ordinal = ?4 AND commit_algorithm = ?5 "
			"AND commit_digest = ?6 AND availability IN (1, 2, 5, 6)");
		if (!reviews) {
			return std::unexpected(reviews.error());
		}
		for (const auto& fact : operation->Epochs) {
			auto destructiveReviews = linked_review_count(
				database,
				Binding.Catalog.Incarnation,
				fact,
				"availability IN (3, 4)");
			if (!destructiveReviews || *destructiveReviews != 0) {
				return std::unexpected(failure::operation_conflict);
			}
			const std::array anchors{
				std::pair{
					fact.Retained.Base,
					fact.ExpectedBaseAvailability,
				},
				std::pair{
					fact.Retained.Head,
					fact.ExpectedHeadAvailability,
				},
			};
			for (std::size_t index = 0; index < anchors.size(); ++index) {
				if (::sqlite3_reset(anchor->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(anchor->Value) != SQLITE_OK
					|| ::sqlite3_bind_int(
						   anchor->Value,
						   1,
						   stored_enum(core::anchor_availability::pruned))
						!= SQLITE_OK
					|| !bind_fixed(
						anchor->Value, 2, Binding.Catalog.Incarnation)
					|| !bind_fixed(anchor->Value, 3, fact.Epoch.Authority)
					|| ::sqlite3_bind_int64(
						   anchor->Value, 4, fact.Epoch.Ordinal.value())
						!= SQLITE_OK
					|| ::sqlite3_bind_int(
						   anchor->Value, 5, static_cast<int>(index + 1U))
						!= SQLITE_OK
					|| !bind_oid(anchor->Value, 6, 7, anchors[index].first)
					|| ::sqlite3_bind_int(
						   anchor->Value,
						   8,
						   stored_enum(anchors[index].second))
						!= SQLITE_OK
					|| ::sqlite3_step(anchor->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database) != 1) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			if (::sqlite3_reset(reviews->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(reviews->Value) != SQLITE_OK
				|| ::sqlite3_bind_int(
					   reviews->Value,
					   1,
					   stored_enum(core::anchor_availability::pruned))
					!= SQLITE_OK
				|| !bind_fixed(
					reviews->Value, 2, Binding.Catalog.Incarnation)
				|| !bind_fixed(reviews->Value, 3, fact.Epoch.Authority)
				|| ::sqlite3_bind_int64(
					   reviews->Value, 4, fact.Epoch.Ordinal.value())
					!= SQLITE_OK
				|| !bind_oid(reviews->Value, 5, 6, fact.Retained.Head)
				|| ::sqlite3_step(reviews->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			auto nonprunedReviews = linked_review_count(
				database,
				Binding.Catalog.Incarnation,
				fact,
				"availability != 3");
			if (!nonprunedReviews || *nonprunedReviews != 0) {
				return std::unexpected(failure::operation_conflict);
			}
		}
		auto revision = prepare(
			database,
			"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1, "
			"retained_epoch_count = retained_epoch_count - ?1 "
			"WHERE incarnation = ?2 AND pr_scope = ?3 "
			"AND catalog_revision = ?4 AND lifecycle = 1 "
			"AND retained_epoch_count >= ?1 "
			"AND catalog_revision < 9223372036854775807");
		if (!revision
			|| ::sqlite3_bind_int64(
				   revision->Value,
				   1,
				   static_cast<std::int64_t>(operation->Epochs.size()))
				!= SQLITE_OK
			|| !bind_fixed(revision->Value, 2, Binding.Catalog.Incarnation)
			|| !bind_fixed(revision->Value, 3, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   revision->Value, 4, Binding.Catalog.Revision.value())
				!= SQLITE_OK
			|| ::sqlite3_step(revision->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::revision_conflict);
		}
		auto advance = prepare(
			database,
			"UPDATE operation SET phase = 8, last_evidence = 6, "
			"operation_revision = operation_revision + 1 "
			"WHERE incarnation = ?1 AND operation = ?2 AND kind = 2 "
			"AND phase = 14 AND operation_revision = ?3 "
			"AND cache_scope = ?4 AND pr_scope = ?5 "
			"AND operation_revision < 9223372036854775806");
		if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(advance->Value, 2, aRequest.Transition.Operation)
			|| ::sqlite3_bind_int64(
				   advance->Value,
				   3,
				   aRequest.Transition.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
			|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
			|| ::sqlite3_step(advance->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		Binding.Catalog.Revision = core::epoch_catalog_revision{
			aRequest.Transition.Catalog.Revision.value() + 1
		};
		return app::catalog_commit_result{
			.Catalog = Binding.Catalog,
			.Changed = true,
		};
	}

	auto sqlite_epoch_catalog_session::cache_delete_impact(
		const core::repository_cache_scope_id aCache,
		std::stop_token                       aStop) -> result<app::cache_delete_impact>
	{
		if (!aCache.valid() || aCache != Binding.Authority.Cache) {
			return std::unexpected(failure::authority_mismatch);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		progress_guard   progress{ State->Database.handle(), aStop };
		return load_cache_impact(
			State->Database.handle(), Binding.Catalog.Incarnation, aCache);
	}

	auto sqlite_epoch_catalog_session::begin_cache_delete(
		const app::begin_cache_delete_request& aRequest,
		std::stop_token                        aStop) -> result<app::maintenance_operation_record>
	{
		if (!app::valid(aRequest)
			|| aRequest.Impact.Incarnation != Binding.Catalog.Incarnation
			|| aRequest.Impact.Cache != Binding.Authority.Cache) {
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
		if (aRequest.Impact.RetainedEpochCount
			> static_cast<std::uint64_t>(
				std::numeric_limits<std::int64_t>::max())) {
			return std::unexpected(failure::quota_exceeded);
		}
		auto existing = load_maintenance(aRequest.Operation);
		if (existing) {
			if (existing->Kind == app::epoch_catalog_operation_kind::cache_delete
				&& existing->Cache == aRequest.Impact.Cache
				&& existing->Scopes == aRequest.Impact.Scopes) {
				auto retained = prepare(
					database,
					"SELECT retained_epoch_count FROM operation "
					"WHERE incarnation = ?1 AND operation = ?2 "
					"AND kind = 3 AND cache_scope = ?3 AND pr_scope IS NULL");
				if (!retained
					|| !bind_fixed(
						retained->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(retained->Value, 2, aRequest.Operation)
					|| !bind_fixed(retained->Value, 3, aRequest.Impact.Cache)
					|| ::sqlite3_step(retained->Value) != SQLITE_ROW
					|| ::sqlite3_column_int64(retained->Value, 0)
						!= static_cast<std::int64_t>(
							aRequest.Impact.RetainedEpochCount)
					|| ::sqlite3_step(retained->Value) != SQLITE_DONE) {
					return std::unexpected(failure::operation_conflict);
				}
				return existing;
			}
			return std::unexpected(failure::operation_conflict);
		}
		if (auto idle = require_no_blocking_operation(database, Binding, true);
			!idle) {
			return std::unexpected(idle.error());
		}
		auto current = load_cache_impact(
			database, Binding.Catalog.Incarnation, aRequest.Impact.Cache);
		if (!current || *current != aRequest.Impact) {
			return std::unexpected(failure::revision_conflict);
		}
		for (const auto& scope : aRequest.Impact.Scopes) {
			auto state = load_pull_request_scope_state(
				database, Binding.Catalog.Incarnation, scope.Authority);
			if (!state || state->Cache != aRequest.Impact.Cache
				|| (state->Lifecycle != 1 && state->Lifecycle != 4)
				|| (state->RepositoryLifecycle != 1
					&& state->RepositoryLifecycle != 2)) {
				return std::unexpected(
					state ? failure::authority_mismatch : state.error());
			}
			if (state->Revision != scope.ExpectedRevision
				|| state->Revision.value()
					>= core::maximum_epoch_catalog_revision) {
				return std::unexpected(failure::revision_conflict);
			}
		}
		auto operation = prepare(
			database,
			"INSERT INTO operation(incarnation, operation, kind, phase, "
			"last_evidence, operation_revision, cache_scope, pr_scope, "
			"expected_revision, application_session, request_id, "
			"epoch_generation, source_observation, source_snapshot, "
			"identity_generation, pull_request_generation, reserved_ordinal, "
			"detail_count, "
			"retained_epoch_count, terminal) "
			"VALUES(?1, ?2, 3, 16, 1, 1, ?3, NULL, NULL, ?4, NULL, NULL, "
			"NULL, NULL, NULL, NULL, NULL, ?5, ?6, 0)");
		if (!operation
			|| !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(operation->Value, 2, aRequest.Operation)
			|| !bind_fixed(operation->Value, 3, aRequest.Impact.Cache)
			|| !bind_u64(operation->Value, 4, ApplicationSession.value())
			|| ::sqlite3_bind_int64(
				   operation->Value,
				   5,
				   static_cast<std::int64_t>(aRequest.Impact.Scopes.size()))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   operation->Value,
				   6,
				   static_cast<std::int64_t>(aRequest.Impact.RetainedEpochCount))
				!= SQLITE_OK
			|| ::sqlite3_step(operation->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(
				database, aStop, failure::operation_conflict));
		}
		auto detail = prepare(
			database,
			"INSERT INTO cache_delete_scope(incarnation, operation, cache_scope, "
			"pr_scope, expected_revision, epoch_count) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6)");
		if (!detail) {
			return std::unexpected(detail.error());
		}
		for (const auto& scope : aRequest.Impact.Scopes) {
			auto countQuery = prepare(
				database,
				"SELECT count(*) FROM epoch e JOIN epoch_anchor a "
				"ON a.incarnation = e.incarnation AND a.pr_scope = e.pr_scope "
				"AND a.ordinal = e.ordinal AND a.endpoint = 1 "
				"AND a.availability NOT IN (3, 4) "
				"WHERE e.incarnation = ?1 AND e.pr_scope = ?2");
			if (!countQuery
				|| !bind_fixed(
					countQuery->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(countQuery->Value, 2, scope.Authority)
				|| ::sqlite3_step(countQuery->Value) != SQLITE_ROW) {
				return std::unexpected(failure::unavailable);
			}
			const auto count = ::sqlite3_column_int64(countQuery->Value, 0);
			if (count < 0 || ::sqlite3_step(countQuery->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (::sqlite3_reset(detail->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(detail->Value) != SQLITE_OK
				|| !bind_fixed(
					detail->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(detail->Value, 2, aRequest.Operation)
				|| !bind_fixed(detail->Value, 3, aRequest.Impact.Cache)
				|| !bind_fixed(detail->Value, 4, scope.Authority)
				|| ::sqlite3_bind_int64(
					   detail->Value, 5, scope.ExpectedRevision.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(detail->Value, 6, count) != SQLITE_OK
				|| ::sqlite3_step(detail->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
		}
		auto mark = prepare(
			database,
			"UPDATE repository_scope SET lifecycle = 2 "
			"WHERE incarnation = ?1 AND cache_scope = ?2 "
			"AND lifecycle IN (1, 2)");
		if (!mark || !bind_fixed(mark->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(mark->Value, 2, aRequest.Impact.Cache)
			|| ::sqlite3_step(mark->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::revision_conflict);
		}
		auto output = load_maintenance(aRequest.Operation);
		if (!output) {
			return std::unexpected(output.error());
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::finalize_cache_delete(
		const app::finalize_cache_delete_request& aRequest,
		std::stop_token                           aStop) -> result<void>
	{
		if (!app::valid(aRequest)
			|| aRequest.Transition.Catalog.Incarnation
				!= Binding.Catalog.Incarnation) {
			return std::unexpected(failure::invalid_request);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, { } };
		transaction      write{ database, { } };
		if (auto begun = write.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		auto operation = load_maintenance(aRequest.Transition.Operation);
		if (!operation
			|| operation->Kind
				!= app::epoch_catalog_operation_kind::cache_delete
			|| operation->Cache != Binding.Authority.Cache) {
			return std::unexpected(failure::phase_conflict);
		}
		if (
			operation->Phase
				== app::epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending
			|| operation->Phase == app::epoch_catalog_operation_phase::complete) {
			if (!finalized_operation_revision_matches(
					operation->Phase,
					aRequest.Transition.ExpectedOperationRevision,
					operation->OperationRevision,
					app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending)) {
				return std::unexpected(failure::operation_conflict);
			}
			auto repository = prepare(
				database,
				"SELECT lifecycle FROM repository_scope "
				"WHERE incarnation = ?1 AND cache_scope = ?2");
			const auto expectedLifecycle = operation->Phase == app::epoch_catalog_operation_phase::complete
				? 4
				: 3;
			if (!repository
				|| !bind_fixed(
					repository->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(repository->Value, 2, operation->Cache)
				|| ::sqlite3_step(repository->Value) != SQLITE_ROW
				|| ::sqlite3_column_int(repository->Value, 0)
					!= expectedLifecycle
				|| ::sqlite3_step(repository->Value) != SQLITE_DONE) {
				return std::unexpected(failure::operation_conflict);
			}
			std::optional<core::epoch_catalog_revision> boundRevision;
			for (const auto& scopeFact : operation->Scopes) {
				auto scope = load_pull_request_scope_state(
					database,
					Binding.Catalog.Incarnation,
					scopeFact.Authority);
				if (!scope || scope->Lifecycle != 3
					|| scope->RepositoryLifecycle != expectedLifecycle
					|| !advanced_once(
						scopeFact.ExpectedRevision, scope->Revision)
					|| scope->RetainedEpochCount != 0U) {
					return std::unexpected(failure::operation_conflict);
				}
				if (scopeFact.Authority == Binding.Authority.Scope) {
					boundRevision = scope->Revision;
				}
			}
			if (!boundRevision) {
				return std::unexpected(failure::authority_mismatch);
			}
			Binding.Catalog.Revision = *boundRevision;
			return { };
		}
		if (aStop.stop_requested()
			&& !app::point_of_no_cancellation(operation->Phase)) {
			return std::unexpected(failure::cancelled);
		}
		if (aRequest.Transition.Catalog != Binding.Catalog
			|| operation->Phase
				!= app::epoch_catalog_operation_phase::quarantined
			|| operation->OperationRevision
				!= aRequest.Transition.ExpectedOperationRevision) {
			return std::unexpected(failure::phase_conflict);
		}
		for (const auto& scopeFact : operation->Scopes) {
			auto scope = load_pull_request_scope_state(
				database,
				Binding.Catalog.Incarnation,
				scopeFact.Authority);
			if (!scope || scope->Cache != operation->Cache
				|| (scope->Lifecycle != 1 && scope->Lifecycle != 4)
				|| scope->RepositoryLifecycle != 2) {
				return std::unexpected(
					scope ? failure::authority_mismatch : scope.error());
			}
			if (scope->Revision != scopeFact.ExpectedRevision
				|| scope->Revision.value()
					>= core::maximum_epoch_catalog_revision) {
				return std::unexpected(failure::revision_conflict);
			}
		}
		auto saturatedRepair = prepare(
			database,
			"SELECT 1 FROM operation WHERE incarnation = ?1 "
			"AND cache_scope = ?2 AND terminal = 3 "
			"AND operation_revision = 9223372036854775807 LIMIT 1");
		if (!saturatedRepair
			|| !bind_fixed(
				saturatedRepair->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(saturatedRepair->Value, 2, operation->Cache)) {
			return std::unexpected(
				saturatedRepair ? failure::unavailable
								: saturatedRepair.error());
		}
		const auto saturatedCode = ::sqlite3_step(saturatedRepair->Value);
		if (saturatedCode == SQLITE_ROW) {
			return std::unexpected(failure::operation_conflict);
		}
		if (saturatedCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		auto compactRepairReceiving = prepare(database,
											  "DELETE FROM append_receiving WHERE incarnation = ?1 AND operation IN "
											  "(SELECT operation FROM operation WHERE incarnation = ?1 AND cache_scope = ?2 AND kind = 1 AND terminal = 3)");
		if (!compactRepairReceiving || !bind_fixed(compactRepairReceiving->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(compactRepairReceiving->Value, 2, operation->Cache)
			|| ::sqlite3_step(compactRepairReceiving->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		auto compactRepairInventory = prepare(
			database,
			"DELETE FROM append_inventory WHERE incarnation = ?1 "
			"AND operation IN (SELECT operation FROM operation "
			"WHERE incarnation = ?1 AND cache_scope = ?2 "
			"AND kind = 1 AND terminal = 3)");
		auto compactRepairReceipt = prepare(
			database,
			"DELETE FROM append_receipt WHERE incarnation = ?1 "
			"AND operation IN (SELECT operation FROM operation "
			"WHERE incarnation = ?1 AND cache_scope = ?2 "
			"AND kind = 1 AND terminal = 3)");
		if (!compactRepairInventory || !compactRepairReceipt
			|| !bind_fixed(
				compactRepairInventory->Value,
				1,
				Binding.Catalog.Incarnation)
			|| !bind_fixed(
				compactRepairInventory->Value, 2, operation->Cache)
			|| ::sqlite3_step(compactRepairInventory->Value) != SQLITE_DONE
			|| !bind_fixed(
				compactRepairReceipt->Value,
				1,
				Binding.Catalog.Incarnation)
			|| !bind_fixed(
				compactRepairReceipt->Value, 2, operation->Cache)
			|| ::sqlite3_step(compactRepairReceipt->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		auto supersedeRepair = prepare(
			database,
			"UPDATE operation SET phase = 11, last_evidence = 5, "
			"operation_revision = operation_revision + 1, terminal = 2, "
			"repair_from_phase = NULL, recovery_input_compacted = "
			"CASE WHEN kind = 1 THEN 1 ELSE 0 END WHERE incarnation = ?1 "
			"AND cache_scope = ?2 AND operation != ?3 AND terminal = 3 "
			"AND operation_revision < 9223372036854775807");
		if (!supersedeRepair
			|| !bind_fixed(
				supersedeRepair->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(supersedeRepair->Value, 2, operation->Cache)
			|| !bind_operation(
				supersedeRepair->Value, 3, aRequest.Transition.Operation)
			|| ::sqlite3_step(supersedeRepair->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		auto tombstone = prepare(
			database,
			"UPDATE pull_request_scope SET lifecycle = 3, "
			"catalog_revision = catalog_revision + 1, retained_epoch_count = 0 "
			"WHERE incarnation = ?1 AND pr_scope = ?2 "
			"AND catalog_revision = ?3 AND lifecycle IN (1, 4) "
			"AND catalog_revision < 9223372036854775807");
		auto anchors = prepare(
			database,
			"UPDATE epoch_anchor SET availability = 4 "
			"WHERE incarnation = ?1 AND pr_scope = ?2 "
			"AND availability != 4");
		auto reviews = prepare(
			database,
			"UPDATE review_association SET availability = 4 "
			"WHERE incarnation = ?1 AND pr_scope = ?2 "
			"AND availability != 4");
		if (!tombstone || !anchors || !reviews) {
			return std::unexpected(failure::unavailable);
		}
		for (const auto& scope : operation->Scopes) {
			if (::sqlite3_reset(anchors->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(anchors->Value) != SQLITE_OK
				|| !bind_fixed(
					anchors->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(anchors->Value, 2, scope.Authority)
				|| ::sqlite3_step(anchors->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			if (::sqlite3_reset(reviews->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(reviews->Value) != SQLITE_OK
				|| !bind_fixed(
					reviews->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(reviews->Value, 2, scope.Authority)
				|| ::sqlite3_step(reviews->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			if (::sqlite3_reset(tombstone->Value) != SQLITE_OK
				|| ::sqlite3_clear_bindings(tombstone->Value) != SQLITE_OK
				|| !bind_fixed(
					tombstone->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(tombstone->Value, 2, scope.Authority)
				|| ::sqlite3_bind_int64(
					   tombstone->Value, 3, scope.ExpectedRevision.value())
					!= SQLITE_OK
				|| ::sqlite3_step(tombstone->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
		}
		auto repository = prepare(
			database,
			"UPDATE repository_scope SET lifecycle = 3 "
			"WHERE incarnation = ?1 AND cache_scope = ?2 AND lifecycle = 2");
		if (!repository
			|| !bind_fixed(repository->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(repository->Value, 2, operation->Cache)
			|| ::sqlite3_step(repository->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::revision_conflict);
		}
		auto advance = prepare(
			database,
			"UPDATE operation SET phase = 19, last_evidence = 1, "
			"operation_revision = operation_revision + 1 "
			"WHERE incarnation = ?1 AND operation = ?2 AND kind = 3 "
			"AND phase = 18 AND operation_revision = ?3 "
			"AND cache_scope = ?4 AND pr_scope IS NULL "
			"AND operation_revision < 9223372036854775806");
		if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(advance->Value, 2, aRequest.Transition.Operation)
			|| ::sqlite3_bind_int64(
				   advance->Value,
				   3,
				   aRequest.Transition.ExpectedOperationRevision.value())
				!= SQLITE_OK
			|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
			|| ::sqlite3_step(advance->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::operation_conflict);
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		for (const auto& scope : operation->Scopes) {
			if (scope.Authority == Binding.Authority.Scope) {
				Binding.Catalog.Revision = core::epoch_catalog_revision{
					scope.ExpectedRevision.value() + 1
				};
				break;
			}
		}
		return { };
	}
}
