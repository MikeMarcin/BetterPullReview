#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "catalog_database.hpp"
#include "epoch_catalog_support.hpp"

#include <algorithm>
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

namespace bpr::adapters::sqlite::epoch_catalog_detail {
	namespace {
		[[nodiscard]] auto read_catalog_identity(::sqlite3* aDatabase)
			-> result<core::epoch_catalog_incarnation_id>
		{
			auto query = prepare(
				aDatabase,
				"SELECT incarnation FROM catalog_meta WHERE singleton = 1");
			if (!query || ::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(
					query ? failure::schema_mismatch : query.error());
			}
			auto incarnation = read_fixed<core::epoch_catalog_incarnation_id>(
				query->Value, 0);
			if (!incarnation || ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return *incarnation;
		}

		[[nodiscard]] auto random_identity() -> result<std::array<std::uint8_t, 32>>
		{
			std::array<std::uint8_t, 32> bytes{ };
			if (!detail::secure_random_bytes(bytes)
				|| std::ranges::all_of(bytes, [](const auto aByte) {
					   return aByte == 0U;
				   })) {
				return std::unexpected(failure::unavailable);
			}
			return bytes;
		}

		// The catalog stores the access mode as a small positive integer
		// rather than the enumerator's own value, so the schema's CHECK
		// constraints read as constraints rather than as a coincidence of C++
		// enumerator ordering, and a reordering of that enumeration cannot
		// silently re-read every stored row as the other mode.
		[[nodiscard]] constexpr auto access_mode_column(
			const core::github_access_mode aMode) noexcept -> int
		{
			switch (aMode) {
			case core::github_access_mode::authenticated:
				return 1;
			case core::github_access_mode::public_preview:
				return 2;
			}
			return 0;
		}

		// The mode is bound with the rest of the authority and never
		// separately, so no query can filter on an identity while ignoring
		// which mode owns it. It is bound last so a caller's own trailing
		// parameters stay numbered relative to the whole key.
		[[nodiscard]] auto bind_authority_key(
			::sqlite3_stmt*                         aQuery,
			const app::epoch_catalog_authority_key& aKey,
			const int                               aFirst) noexcept -> bool
		{
			const auto mode = access_mode_column(aKey.Mode);
			return mode != 0 && bind_u64(aQuery, aFirst, aKey.Profile.value())
				&& bind_u64(aQuery, aFirst + 1, aKey.ProfileRevision.value())
				&& bind_u64(aQuery, aFirst + 2, aKey.Host.value())
				&& bind_fixed(aQuery, aFirst + 3, aKey.HostAuthority)
				&& bind_u64(aQuery, aFirst + 4, aKey.Account.value())
				&& bind_u64(aQuery, aFirst + 5, aKey.Repository.value())
				&& ::sqlite3_bind_int(
					   aQuery, aFirst + 6, static_cast<int>(aKey.ObjectFormat))
				== SQLITE_OK
				&& ::sqlite3_bind_int(aQuery, aFirst + 7, mode) == SQLITE_OK;
		}

		[[nodiscard]] auto compact_retired_cache_journals(
			::sqlite3*                                aDatabase,
			const core::epoch_catalog_incarnation_id& aIncarnation,
			const core::repository_cache_scope_id&    aCache,
			std::stop_token                           aStop) -> result<void>
		{
			auto nonterminal = prepare(
				aDatabase,
				"SELECT 1 FROM operation WHERE incarnation = ?1 "
				"AND cache_scope = ?2 AND terminal NOT IN (1, 2) LIMIT 1");
			if (!nonterminal || !bind_fixed(nonterminal->Value, 1, aIncarnation)
				|| !bind_fixed(nonterminal->Value, 2, aCache)) {
				return std::unexpected(
					nonterminal ? failure::unavailable : nonterminal.error());
			}
			const auto nonterminalCode = ::sqlite3_step(nonterminal->Value);
			if (nonterminalCode == SQLITE_ROW) {
				return std::unexpected(failure::repair_required);
			}
			if (nonterminalCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}

			struct cleanup_statement final
			{
				std::string_view Delete;
				std::string_view Verify;
			};
			constexpr std::array cleanup{
				cleanup_statement{
					"DELETE FROM append_residency WHERE incarnation = ?1 AND operation IN (SELECT operation FROM operation WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_residency d JOIN operation o ON o.incarnation = d.incarnation AND o.operation = d.operation WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_occurrence WHERE incarnation = ?1 AND operation IN (SELECT operation FROM operation WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_occurrence d JOIN operation o ON o.incarnation = d.incarnation AND o.operation = d.operation WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_candidate_review WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_candidate_review d "
					"JOIN operation o ON o.incarnation = d.incarnation "
					"AND o.operation = d.operation WHERE d.incarnation = ?1 "
					"AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_hidden_candidate WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_hidden_candidate d "
					"JOIN operation o ON o.incarnation = d.incarnation "
					"AND o.operation = d.operation WHERE d.incarnation = ?1 "
					"AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_installed_artifact WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_installed_artifact d "
					"JOIN operation o ON o.incarnation = d.incarnation "
					"AND o.operation = d.operation WHERE d.incarnation = ?1 "
					"AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_install WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_install d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_inventory WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_inventory d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_receiving WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_receiving d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_receipt WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_receipt d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_source_review WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_source_review d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_operation WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_operation d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM prune_operation WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM prune_operation d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM cache_delete_scope WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM cache_delete_scope d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
			};
			for (const auto& cleanupStatement : cleanup) {
				auto removed = prepare(aDatabase, cleanupStatement.Delete);
				if (!removed || !bind_fixed(removed->Value, 1, aIncarnation)
					|| !bind_fixed(removed->Value, 2, aCache)
					|| ::sqlite3_step(removed->Value) != SQLITE_DONE) {
					return std::unexpected(
						removed ? sql_failure(aDatabase, aStop) : removed.error());
				}
				auto remaining = prepare(aDatabase, cleanupStatement.Verify);
				if (!remaining || !bind_fixed(remaining->Value, 1, aIncarnation)
					|| !bind_fixed(remaining->Value, 2, aCache)
					|| ::sqlite3_step(remaining->Value) != SQLITE_ROW
					|| ::sqlite3_column_type(remaining->Value, 0) != SQLITE_INTEGER
					|| ::sqlite3_column_int64(remaining->Value, 0) != 0
					|| ::sqlite3_step(remaining->Value) != SQLITE_DONE) {
					return std::unexpected(
						remaining ? failure::repair_required : remaining.error());
				}
			}

			auto removedOperations = prepare(
				aDatabase,
				"DELETE FROM operation WHERE incarnation = ?1 "
				"AND cache_scope = ?2 AND terminal IN (1, 2)");
			if (!removedOperations
				|| !bind_fixed(removedOperations->Value, 1, aIncarnation)
				|| !bind_fixed(removedOperations->Value, 2, aCache)
				|| ::sqlite3_step(removedOperations->Value) != SQLITE_DONE) {
				return std::unexpected(
					removedOperations ? sql_failure(aDatabase, aStop)
									  : removedOperations.error());
			}
			auto remainingOperations = prepare(
				aDatabase,
				"SELECT count(*) FROM operation WHERE incarnation = ?1 "
				"AND cache_scope = ?2");
			if (!remainingOperations
				|| !bind_fixed(remainingOperations->Value, 1, aIncarnation)
				|| !bind_fixed(remainingOperations->Value, 2, aCache)
				|| ::sqlite3_step(remainingOperations->Value) != SQLITE_ROW
				|| ::sqlite3_column_type(remainingOperations->Value, 0)
					!= SQLITE_INTEGER
				|| ::sqlite3_column_int64(remainingOperations->Value, 0) != 0
				|| ::sqlite3_step(remainingOperations->Value) != SQLITE_DONE) {
				return std::unexpected(
					remainingOperations ? failure::repair_required
										: remainingOperations.error());
			}
			return { };
		}
	}

	auto sqlite_epoch_catalog_port::open(
		const app::epoch_catalog_open_request& aRequest,
		std::stop_token                        aStop)
		-> result<std::unique_ptr<app::epoch_catalog_session>>
	{
		if (!app::valid(aRequest)) {
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
		auto incarnation = read_catalog_identity(database);
		if (!incarnation) {
			return std::unexpected(incarnation.error());
		}

		std::optional<core::repository_cache_scope_id> cacheScope;
		auto                                           repositoryLifecycle = 0;
		auto                                           repository          = prepare(
			database,
			"SELECT cache_scope, lifecycle FROM repository_scope "
			"WHERE incarnation = ?1 AND profile_id = ?2 "
			"AND profile_revision = ?3 AND host_id = ?4 "
			"AND host_authority = ?5 AND account_id = ?6 "
			"AND repository_id = ?7 AND object_format = ?8 "
			"AND access_mode = ?9 AND lifecycle != 4");
		if (!repository || !bind_fixed(repository->Value, 1, *incarnation)
			|| !bind_authority_key(repository->Value, aRequest.Authority, 2)) {
			return std::unexpected(
				repository ? failure::unavailable : repository.error());
		}
		auto code = ::sqlite3_step(repository->Value);
		if (code == SQLITE_ROW) {
			cacheScope = read_fixed<core::repository_cache_scope_id>(
				repository->Value, 0);
			const auto lifecycle = ::sqlite3_column_int(repository->Value, 1);
			if (!cacheScope || lifecycle < 1 || lifecycle > 3
				|| ::sqlite3_step(repository->Value) != SQLITE_DONE) {
				return std::unexpected(failure::repair_required);
			}
			repositoryLifecycle = lifecycle;
		} else if (code != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}

		if (!cacheScope) {
			if (aRequest.ExistingOnly) {
				return std::unexpected(failure::unavailable);
			}
			constexpr std::size_t scopeAllocationAttempts = 8U;
			for (std::size_t attempt = 0U;
				 attempt < scopeAllocationAttempts && !cacheScope;
				 ++attempt) {
				auto identity = random_identity();
				if (!identity) {
					return std::unexpected(identity.error());
				}
				const auto candidate = core::repository_cache_scope_id{ *identity };
				auto       insert    = prepare(
					database,
					"INSERT INTO repository_scope(incarnation, cache_scope, "
					"profile_id, profile_revision, host_id, host_authority, "
					"account_id, repository_id, object_format, access_mode, "
					"lifecycle) "
					"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1)");
				if (!insert || !bind_fixed(insert->Value, 1, *incarnation)
					|| !bind_fixed(insert->Value, 2, candidate)
					|| !bind_authority_key(
						insert->Value, aRequest.Authority, 3)) {
					return std::unexpected(failure::unavailable);
				}
				const auto insertCode = ::sqlite3_step(insert->Value);
				if (insertCode == SQLITE_DONE) {
					cacheScope          = candidate;
					repositoryLifecycle = 1;
				} else if ((static_cast<unsigned>(insertCode) & 0xFFU) != SQLITE_CONSTRAINT) {
					return std::unexpected(sql_failure(database, aStop));
				}
			}
			if (!cacheScope) {
				return std::unexpected(failure::authority_mismatch);
			}
		}

		if (repositoryLifecycle == 1 && !aRequest.ExistingOnly) {
			auto activeScopeCount = prepare(
				database,
				"SELECT count(*) FROM pull_request_scope "
				"WHERE incarnation = ?1 AND cache_scope = ?2");
			if (!activeScopeCount
				|| !bind_fixed(
					activeScopeCount->Value, 1, *incarnation)
				|| !bind_fixed(activeScopeCount->Value, 2, *cacheScope)
				|| ::sqlite3_step(activeScopeCount->Value) != SQLITE_ROW) {
				return std::unexpected(
					activeScopeCount ? failure::unavailable
									 : activeScopeCount.error());
			}
			const auto activeCount = ::sqlite3_column_int64(activeScopeCount->Value, 0);
			if (activeCount < 0
				|| ::sqlite3_step(activeScopeCount->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto retired = prepare(
				database,
				"SELECT p.pr_scope, p.cache_scope, p.viewer_id, "
				"p.pull_request_number, p.lifecycle "
				"FROM repository_scope r JOIN pull_request_scope p "
				"ON p.incarnation = r.incarnation "
				"AND p.cache_scope = r.cache_scope "
				"WHERE r.incarnation = ?1 AND r.profile_id = ?2 "
				"AND r.profile_revision = ?3 AND r.host_id = ?4 "
				"AND r.host_authority = ?5 AND r.account_id = ?6 "
				"AND r.repository_id = ?7 AND r.object_format = ?8 "
				"AND r.access_mode = ?9 "
				"AND r.lifecycle = 4 AND p.pull_request_id = ?10");
			if (!retired || !bind_fixed(retired->Value, 1, *incarnation)
				|| !bind_authority_key(retired->Value, aRequest.Authority, 2)
				|| !bind_u64(
					retired->Value, 10, aRequest.Authority.PullRequest.value())) {
				return std::unexpected(
					retired ? failure::unavailable : retired.error());
			}
			const auto retiredCode = ::sqlite3_step(retired->Value);
			if (retiredCode == SQLITE_ROW) {
				if (activeCount
					>= static_cast<std::int64_t>(
						app::maximum_cache_delete_scope_count)) {
					return std::unexpected(failure::quota_exceeded);
				}
				const auto retiredScope = read_fixed<core::pull_request_authority_scope_id>(
					retired->Value, 0);
				const auto retiredCache = read_fixed<core::repository_cache_scope_id>(
					retired->Value, 1);
				const auto viewer = read_u64(retired->Value, 2);
				if (!retiredScope || !retiredCache || !viewer
					|| *viewer != aRequest.Authority.Viewer.value()
					|| ::sqlite3_column_int64(retired->Value, 3)
						!= aRequest.Authority.Number.value()
					|| ::sqlite3_column_int(retired->Value, 4) != 3
					|| ::sqlite3_step(retired->Value) != SQLITE_DONE) {
					return std::unexpected(failure::repair_required);
				}
				if (auto compacted = compact_retired_cache_journals(
						database, *incarnation, *retiredCache, aStop);
					!compacted) {
					return std::unexpected(compacted.error());
				}
				auto reactivate = prepare(
					database,
					"UPDATE pull_request_scope SET cache_scope = ?1, "
					"lifecycle = 1, retained_epoch_count = 0, "
					"retention_rehydration_required = CASE "
					"WHEN latest_live_ordinal IS NOT NULL AND "
					"(SELECT count(*) FROM epoch_anchor a WHERE "
					"a.incarnation = pull_request_scope.incarnation AND "
					"a.pr_scope = pull_request_scope.pr_scope AND "
					"a.ordinal = pull_request_scope.latest_live_ordinal AND "
					"a.availability = 4) = 2 THEN 1 ELSE 0 END "
					"WHERE incarnation = ?2 AND pr_scope = ?3 "
					"AND cache_scope = ?4 AND lifecycle = 3");
				if (!reactivate
					|| !bind_fixed(reactivate->Value, 1, *cacheScope)
					|| !bind_fixed(reactivate->Value, 2, *incarnation)
					|| !bind_fixed(reactivate->Value, 3, *retiredScope)
					|| !bind_fixed(reactivate->Value, 4, *retiredCache)
					|| ::sqlite3_step(reactivate->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database) != 1) {
					return std::unexpected(failure::repair_required);
				}
			} else if (retiredCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
		}

		std::optional<core::pull_request_authority_scope_id> prScope;
		std::int64_t                                         catalogRevision = 0;
		auto                                                 pullRequest     = prepare(
			database,
			"SELECT pr_scope, viewer_id, pull_request_number, "
			"catalog_revision, lifecycle FROM pull_request_scope "
			"WHERE incarnation = ?1 AND cache_scope = ?2 "
			"AND pull_request_id = ?3");
		if (!pullRequest || !bind_fixed(pullRequest->Value, 1, *incarnation)
			|| !bind_fixed(pullRequest->Value, 2, *cacheScope)
			|| !bind_u64(
				pullRequest->Value, 3, aRequest.Authority.PullRequest.value())) {
			return std::unexpected(
				pullRequest ? failure::unavailable : pullRequest.error());
		}
		code = ::sqlite3_step(pullRequest->Value);
		if (code == SQLITE_ROW) {
			prScope = read_fixed<core::pull_request_authority_scope_id>(
				pullRequest->Value, 0);
			const auto viewer
				= read_u64_including_absent(pullRequest->Value, 1);
			catalogRevision = ::sqlite3_column_int64(pullRequest->Value, 3);
			if (!prScope || !viewer
				|| *viewer != aRequest.Authority.Viewer.value()
				|| ::sqlite3_column_int64(pullRequest->Value, 2)
					!= aRequest.Authority.Number.value()
				|| catalogRevision < 0
				|| (::sqlite3_column_int(pullRequest->Value, 4) != 1
					&& ::sqlite3_column_int(pullRequest->Value, 4) != 3
					&& ::sqlite3_column_int(pullRequest->Value, 4) != 4)
				|| ::sqlite3_step(pullRequest->Value) != SQLITE_DONE) {
				return std::unexpected(failure::authority_mismatch);
			}
		} else if (code != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}

		if (!prScope) {
			if (aRequest.ExistingOnly) {
				return std::unexpected(failure::unavailable);
			}
			if (repositoryLifecycle != 1) {
				return std::unexpected(failure::operation_conflict);
			}
			auto scopeCount = prepare(
				database,
				"SELECT count(*) FROM pull_request_scope "
				"WHERE incarnation = ?1 AND cache_scope = ?2");
			if (!scopeCount
				|| !bind_fixed(scopeCount->Value, 1, *incarnation)
				|| !bind_fixed(scopeCount->Value, 2, *cacheScope)
				|| ::sqlite3_step(scopeCount->Value) != SQLITE_ROW) {
				return std::unexpected(
					scopeCount ? failure::unavailable : scopeCount.error());
			}
			const auto count = ::sqlite3_column_int64(scopeCount->Value, 0);
			if (count < 0
				|| ::sqlite3_step(scopeCount->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (count
				>= static_cast<std::int64_t>(
					app::maximum_cache_delete_scope_count)) {
				return std::unexpected(failure::quota_exceeded);
			}
			constexpr std::size_t scopeAllocationAttempts = 8U;
			for (std::size_t attempt = 0U;
				 attempt < scopeAllocationAttempts && !prScope;
				 ++attempt) {
				auto identity = random_identity();
				if (!identity) {
					return std::unexpected(identity.error());
				}
				const auto candidate = core::pull_request_authority_scope_id{ *identity };
				auto       insert    = prepare(
					database,
					"INSERT INTO pull_request_scope(incarnation, pr_scope, "
					"cache_scope, viewer_id, access_mode, pull_request_id, "
					"pull_request_number, next_ordinal, catalog_revision, "
					"retained_epoch_count, "
					"latest_live_ordinal, previous_distinct_head_ordinal, "
					"lifecycle, retention_rehydration_required) "
					"VALUES(?1, ?2, ?3, ?4, ?7, ?5, ?6, 1, 0, 0, NULL, NULL, "
					"1, 0)");
				if (!insert || !bind_fixed(insert->Value, 1, *incarnation)
					|| !bind_fixed(insert->Value, 2, candidate)
					|| !bind_fixed(insert->Value, 3, *cacheScope)
					|| !bind_u64(
						insert->Value, 4, aRequest.Authority.Viewer.value())
					|| !bind_u64(
						insert->Value,
						5,
						aRequest.Authority.PullRequest.value())
					|| ::sqlite3_bind_int64(
						   insert->Value, 6, aRequest.Authority.Number.value())
						!= SQLITE_OK
					// The pull-request scope carries the mode its
					// repository scope carries; the schema's foreign key
					// refuses any other pairing, so a mixed-authority row
					// cannot be written even by mistake here.
					|| ::sqlite3_bind_int(
						   insert->Value,
						   7,
						   access_mode_column(aRequest.Authority.Mode))
						!= SQLITE_OK) {
					return std::unexpected(failure::unavailable);
				}
				const auto insertCode = ::sqlite3_step(insert->Value);
				if (insertCode == SQLITE_DONE) {
					prScope = candidate;
				} else if ((static_cast<unsigned>(insertCode) & 0xFFU) != SQLITE_CONSTRAINT) {
					return std::unexpected(sql_failure(database, aStop));
				}
			}
			if (!prScope) {
				return std::unexpected(failure::authority_mismatch);
			}
		}
		auto scopeIntegrity = prepare(
			database,
			"SELECT p.catalog_revision, p.retained_epoch_count, p.lifecycle, "
			"r.lifecycle, p.retention_rehydration_required, "
			"(SELECT count(*) FROM epoch_anchor a "
			"WHERE a.incarnation = p.incarnation AND a.pr_scope = p.pr_scope "
			"AND a.endpoint = 1 AND a.availability NOT IN (3, 4)) "
			"FROM pull_request_scope p JOIN repository_scope r "
			"ON r.incarnation = p.incarnation AND r.cache_scope = p.cache_scope "
			"WHERE p.incarnation = ?1 AND p.pr_scope = ?2 "
			"AND p.cache_scope = ?3");
		if (!scopeIntegrity
			|| !bind_fixed(scopeIntegrity->Value, 1, *incarnation)
			|| !bind_fixed(scopeIntegrity->Value, 2, *prScope)
			|| !bind_fixed(scopeIntegrity->Value, 3, *cacheScope)
			|| ::sqlite3_step(scopeIntegrity->Value) != SQLITE_ROW) {
			return std::unexpected(
				scopeIntegrity ? failure::repair_required
							   : scopeIntegrity.error());
		}
		const auto verifiedRevision            = ::sqlite3_column_int64(scopeIntegrity->Value, 0);
		const auto retained                    = ::sqlite3_column_int64(scopeIntegrity->Value, 1);
		const auto prLifecycle                 = ::sqlite3_column_int(scopeIntegrity->Value, 2);
		const auto verifiedRepositoryLifecycle = ::sqlite3_column_int(scopeIntegrity->Value, 3);
		const auto observedRetained            = ::sqlite3_column_int64(scopeIntegrity->Value, 5);
		const auto rehydrationRequired         = ::sqlite3_column_int(scopeIntegrity->Value, 4);
		const auto lifecycleCoherent           = (prLifecycle == 1
												  && (verifiedRepositoryLifecycle == 1
													  || verifiedRepositoryLifecycle == 2))
			|| (prLifecycle == 4
				&& (verifiedRepositoryLifecycle == 1
					|| verifiedRepositoryLifecycle == 2))
			|| (prLifecycle == 3 && verifiedRepositoryLifecycle == 3);
		if ((aRequest.ExistingOnly && (prLifecycle != 1 || verifiedRepositoryLifecycle != 1 || rehydrationRequired != 0))
			|| verifiedRevision != catalogRevision || retained < 0
			|| retained != observedRetained
			|| retained
				> static_cast<std::int64_t>(
					app::maximum_retained_epoch_count_per_pull_request)
			|| verifiedRepositoryLifecycle != repositoryLifecycle
			|| !lifecycleCoherent
			|| (rehydrationRequired != 0 && rehydrationRequired != 1)
			|| (rehydrationRequired == 1 && prLifecycle != 1)
			|| ::sqlite3_step(scopeIntegrity->Value) != SQLITE_DONE) {
			return std::unexpected(failure::repair_required);
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}

		app::epoch_catalog_binding binding{
			.Authority = core::pull_request_authority{
				.Profile         = aRequest.Authority.Profile,
				.ProfileRevision = aRequest.Authority.ProfileRevision,
				.Host            = aRequest.Authority.Host,
				.HostAuthority   = aRequest.Authority.HostAuthority,
				.Account         = aRequest.Authority.Account,
				.Viewer          = aRequest.Authority.Viewer,
				.Repository      = aRequest.Authority.Repository,
				.Cache           = *cacheScope,
				.PullRequest     = aRequest.Authority.PullRequest,
				.Number          = aRequest.Authority.Number,
				.Scope           = *prScope,
				// Without this the binding a public-preview key opens
				// would hand back an authenticated authority, and every
				// key derived from it - the Git cache scope, the
				// comparison digest - would land in the signed-in
				// namespace.
				.Mode = aRequest.Authority.Mode,
			},
			.Catalog = core::epoch_catalog_identity{
				.Incarnation = *incarnation,
				.Revision    = core::epoch_catalog_revision{ catalogRevision },
			},
			.ObjectFormat = aRequest.Authority.ObjectFormat,
		};
		return std::make_unique<sqlite_epoch_catalog_session>(
			State, binding, aRequest.ApplicationSession);
	}
}
