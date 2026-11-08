#include "epoch_catalog_support.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace bpr::adapters::sqlite::epoch_catalog_detail {
	statement::statement(statement&& aOther) noexcept
		: Value{ std::exchange(aOther.Value, nullptr) }
	{
	}

	auto statement::operator=(statement&& aOther) noexcept -> statement&
	{
		if (this != &aOther) {
			if (Value != nullptr) {
				(void)::sqlite3_finalize(Value);
			}
			Value = std::exchange(aOther.Value, nullptr);
		}
		return *this;
	}

	statement::~statement()
	{
		if (Value != nullptr) {
			(void)::sqlite3_finalize(Value);
		}
	}

	auto stop_progress(void* aOpaque) noexcept -> int
	{
		const auto* context = static_cast<const stop_context*>(aOpaque);
		return context != nullptr && context->Stop.stop_requested() ? 1 : 0;
	}

	progress_guard::progress_guard(::sqlite3* aDatabase, std::stop_token aStop)
		: Context{ .Stop = aStop }
		, Handler{ aDatabase, 128, stop_progress, &Context }
	{
	}

	auto sql_failure(
		::sqlite3*      aDatabase,
		std::stop_token aStop,
		const failure   aFallback) noexcept -> failure
	{
		if (aStop.stop_requested()) {
			return failure::cancelled;
		}
		switch (detail::classify_sqlite_result(
			::sqlite3_extended_errcode(aDatabase))) {
		case detail::sqlite_result_classification::busy:
			return failure::busy;
		case detail::sqlite_result_classification::integrity_failure:
			return failure::integrity_failure;
		case detail::sqlite_result_classification::cancelled:
			return failure::cancelled;
		case detail::sqlite_result_classification::quota_exceeded:
			return failure::quota_exceeded;
		case detail::sqlite_result_classification::unavailable:
			return aFallback;
		}
		return aFallback;
	}

	auto prepare(::sqlite3* aDatabase, std::string_view aSql)
		-> result<statement>
	{
		statement   output;
		const char* tail = nullptr;
		const auto  code = ::sqlite3_prepare_v3(
			aDatabase,
			aSql.data(),
			static_cast<int>(aSql.size()),
			SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_NO_VTAB,
			&output.Value,
			&tail);
		if (code != SQLITE_OK || output.Value == nullptr) {
			return std::unexpected(sql_failure(aDatabase, { }));
		}
		while (tail != aSql.data() + aSql.size()) {
			const auto value = static_cast<unsigned char>(*tail);
			if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
				return std::unexpected(failure::schema_mismatch);
			}
			++tail;
		}
		return output;
	}

	auto execute(
		::sqlite3*       aDatabase,
		std::string_view aSql,
		std::stop_token  aStop) -> result<void>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		auto query = prepare(aDatabase, aSql);
		if (!query) {
			return std::unexpected(query.error());
		}
		if (::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(aDatabase, aStop));
		}
		return { };
	}

	transaction::transaction(::sqlite3* aDatabase, std::stop_token aStop)
		: Database{ aDatabase }
		, Stop{ aStop }
	{
	}

	transaction::~transaction()
	{
		if (Active) {
			(void)::sqlite3_exec(
				Database, "ROLLBACK", nullptr, nullptr, nullptr);
		}
	}

	auto transaction::begin() -> result<void>
	{
		auto opened = execute(Database, "BEGIN IMMEDIATE", Stop);
		Active      = opened.has_value();
		return opened;
	}

	auto transaction::commit() -> result<void>
	{
		auto committed = execute(Database, "COMMIT", Stop);
		if (committed) {
			Active = false;
		}
		return committed;
	}

	read_transaction::read_transaction(::sqlite3* aDatabase, std::stop_token aStop)
		: Database{ aDatabase }
		, Stop{ aStop }
	{
	}

	read_transaction::~read_transaction()
	{
		if (Active) {
			(void)::sqlite3_exec(
				Database, "ROLLBACK", nullptr, nullptr, nullptr);
		}
	}

	auto read_transaction::begin() -> result<void>
	{
		auto opened = execute(Database, "BEGIN", Stop);
		Active      = opened.has_value();
		return opened;
	}

	auto read_transaction::commit() -> result<void>
	{
		auto committed = execute(Database, "COMMIT", Stop);
		if (committed) {
			Active = false;
		}
		return committed;
	}

	auto bind_u64(
		::sqlite3_stmt* aQuery, const int aIndex, const std::uint64_t aValue) noexcept
		-> bool
	{
		const auto encoded = encode_u64(aValue);
		return bind_bytes(aQuery, aIndex, encoded);
	}

	auto bind_operation(
		::sqlite3_stmt*                 aQuery,
		const int                       aIndex,
		const core::epoch_operation_id& aOperation) noexcept -> bool
	{
		return bind_bytes(aQuery, aIndex, aOperation.bytes());
	}

	auto bind_oid(
		::sqlite3_stmt*                     aQuery,
		const int                           aAlgorithmIndex,
		const int                           aDigestIndex,
		const core::git_hash_algorithm      aAlgorithm,
		const std::array<std::uint8_t, 32>& aBytes) noexcept -> bool
	{
		const auto count = aAlgorithm == core::git_hash_algorithm::sha1 ? 20U
			: aAlgorithm == core::git_hash_algorithm::sha256            ? 32U
																		: 0U;
		return count != 0U
			&& ::sqlite3_bind_int(
				   aQuery, aAlgorithmIndex, static_cast<int>(aAlgorithm))
			== SQLITE_OK
			&& ::sqlite3_bind_blob(
				   aQuery,
				   aDigestIndex,
				   aBytes.data(),
				   static_cast<int>(count),
				   SQLITE_TRANSIENT)
			== SQLITE_OK;
	}

	auto read_blob(
		::sqlite3_stmt*         aQuery,
		const int               aColumn,
		std::span<std::uint8_t> aOutput) noexcept -> bool
	{
		if (::sqlite3_column_type(aQuery, aColumn) != SQLITE_BLOB
			|| ::sqlite3_column_bytes(aQuery, aColumn)
				!= static_cast<int>(aOutput.size())) {
			return false;
		}
		const auto* value = static_cast<const std::uint8_t*>(
			::sqlite3_column_blob(aQuery, aColumn));
		if (value == nullptr) {
			return false;
		}
		std::memcpy(aOutput.data(), value, aOutput.size());
		return true;
	}

	auto read_u64(
		::sqlite3_stmt* aQuery, const int aColumn) noexcept
		-> std::optional<std::uint64_t>
	{
		std::array<std::uint8_t, 8> encoded{ };
		if (!read_blob(aQuery, aColumn, encoded)) {
			return std::nullopt;
		}
		std::uint64_t output = 0U;
		for (const auto byte : encoded) {
			output = (output << 8U) | byte;
		}
		if (output == 0U) {
			return std::nullopt;
		}
		return output;
	}

	auto read_u64_including_absent(
		::sqlite3_stmt* aQuery, const int aColumn) noexcept
		-> std::optional<std::uint64_t>
	{
		std::array<std::uint8_t, 8> encoded{ };
		if (!read_blob(aQuery, aColumn, encoded)) {
			return std::nullopt;
		}
		std::uint64_t output = 0U;
		for (const auto byte : encoded) {
			output = (output << 8U) | byte;
		}
		return output;
	}

	catalog_state::catalog_state(detail::catalog_database aDatabase)
		: Database{ std::move(aDatabase) }
	{
	}

	sqlite_epoch_catalog_session::sqlite_epoch_catalog_session(
		std::shared_ptr<catalog_state> aState,
		app::epoch_catalog_binding     aBinding,
		core::application_session_id   aApplicationSession)
		: State{ std::move(aState) }
		, Binding{ aBinding }
		, ApplicationSession{ aApplicationSession }
	{
	}

	auto sqlite_epoch_catalog_session::binding() const noexcept
		-> const app::epoch_catalog_binding&
	{
		return Binding;
	}

	sqlite_epoch_catalog_port::sqlite_epoch_catalog_port(std::shared_ptr<catalog_state> aState)
		: State{ std::move(aState) }
	{
	}

	auto load_epoch_summary(
		::sqlite3*                                   aDatabase,
		const core::epoch_catalog_incarnation_id&    aIncarnation,
		const core::pull_request_authority_scope_id& aAuthority,
		const core::epoch_ordinal                    aOrdinal) -> result<core::review_epoch_summary>
	{
		auto query = prepare(
			aDatabase,
			"SELECT e.source_observation, e.source_snapshot, "
			"e.identity_generation, e.pull_request_generation, e.base_branch, "
			"e.observed_at, "
			"e.origin, e.base_transition, e.head_transition, "
			"e.material_relation, e.primary_cause, "
			"e.material_base_algorithm, e.material_base_digest, "
			"e.material_head_algorithm, e.material_head_digest, "
			"e.material_policy_revision, "
			"b.object_algorithm, b.object_digest, b.availability, "
			"h.object_algorithm, h.object_digest, h.availability "
			"FROM epoch e "
			"JOIN epoch_anchor b ON b.incarnation = e.incarnation "
			"AND b.pr_scope = e.pr_scope AND b.ordinal = e.ordinal "
			"AND b.endpoint = 1 "
			"JOIN epoch_anchor h ON h.incarnation = e.incarnation "
			"AND h.pr_scope = e.pr_scope AND h.ordinal = e.ordinal "
			"AND h.endpoint = 2 "
			"WHERE e.incarnation = ?1 AND e.pr_scope = ?2 "
			"AND e.ordinal = ?3");
		if (!query || !bind_fixed(query->Value, 1, aIncarnation)
			|| !bind_fixed(query->Value, 2, aAuthority)
			|| ::sqlite3_bind_int64(query->Value, 3, aOrdinal.value()) != SQLITE_OK) {
			return std::unexpected(query ? failure::unavailable : query.error());
		}
		if (::sqlite3_step(query->Value) != SQLITE_ROW) {
			return std::unexpected(failure::repair_required);
		}
		const auto observation        = read_fixed<core::epoch_source_observation_id>(query->Value, 0);
		const auto snapshot           = read_u64(query->Value, 1);
		const auto identityGeneration = read_u64(query->Value, 2);
		const auto prGeneration       = read_u64(query->Value, 3);
		const auto branch             = read_fixed<core::base_branch_id>(query->Value, 4);
		const auto base               = read_oid<core::git_commit_oid>(query->Value, 16, 17);
		const auto head               = read_oid<core::git_commit_oid>(query->Value, 19, 20);
		if (!observation || !snapshot || !identityGeneration || !prGeneration
			|| !branch || !base || !head
			|| ::sqlite3_column_type(query->Value, 5) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 6) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 7) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 8) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 9) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 10) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 18) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 21) != SQLITE_INTEGER) {
			return std::unexpected(failure::schema_mismatch);
		}
		std::optional<core::epoch_material_identity> material;
		if (::sqlite3_column_type(query->Value, 11) != SQLITE_NULL) {
			const auto baseTree = read_oid<core::git_tree_oid>(query->Value, 11, 12);
			const auto headTree = read_oid<core::git_tree_oid>(query->Value, 13, 14);
			const auto policy   = read_u64(query->Value, 15);
			if (!baseTree || !headTree || !policy) {
				return std::unexpected(failure::schema_mismatch);
			}
			material = core::epoch_material_identity{
				.BaseTree       = *baseTree,
				.HeadTree       = *headTree,
				.PolicyRevision = core::comparison_policy_revision{ *policy },
			};
		}
		core::review_epoch_summary output{
			.Id = core::review_epoch_id{
				.Authority   = aAuthority,
				.Incarnation = aIncarnation,
				.Ordinal     = aOrdinal,
			},
			.Source = core::pull_request_source_revision{
				.Observation           = *observation,
				.Snapshot              = core::provider_snapshot_id{ *snapshot },
				.IdentityGeneration    = core::github_identity_generation{ *identityGeneration },
				.PullRequestGeneration = core::request_generation{ *prGeneration },
			},
			.BaseBranch = *branch,
			.Base       = *base,
			.Head       = *head,
			.Material   = material,
			.Cause      = core::epoch_cause{
				.Origin   = restored_enum<core::epoch_origin>(::sqlite3_column_int(query->Value, 6)),
				.Base     = restored_enum<core::epoch_base_transition>(::sqlite3_column_int(query->Value, 7)),
				.Head     = restored_enum<core::epoch_head_transition>(::sqlite3_column_int(query->Value, 8)),
				.Material = restored_enum<core::epoch_material_relation>(::sqlite3_column_int(query->Value, 9)),
				.Primary  = restored_enum<core::epoch_primary_cause>(::sqlite3_column_int(query->Value, 10)),
			},
			.ObservedAt       = core::observed_utc_milliseconds{ ::sqlite3_column_int64(query->Value, 5) },
			.BaseAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(query->Value, 18)),
			.HeadAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(query->Value, 21)),
		};
		if (::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (!app::valid(output)) {
			return std::unexpected(failure::schema_mismatch);
		}
		return output;
	}

	auto load_pull_request_scope_state(
		::sqlite3*                                   aDatabase,
		const core::epoch_catalog_incarnation_id&    aIncarnation,
		const core::pull_request_authority_scope_id& aAuthority)
		-> result<pull_request_scope_state>
	{
		auto query = prepare(
			aDatabase,
			"SELECT p.catalog_revision, p.cache_scope, p.retained_epoch_count, "
			"p.lifecycle, r.lifecycle, (SELECT count(*) FROM epoch_anchor a "
			"WHERE a.incarnation = p.incarnation AND a.pr_scope = p.pr_scope "
			"AND a.endpoint = 1 AND a.availability NOT IN (3, 4)) "
			"FROM pull_request_scope p JOIN repository_scope r "
			"ON r.incarnation = p.incarnation "
			"AND r.cache_scope = p.cache_scope "
			"WHERE p.incarnation = ?1 AND p.pr_scope = ?2");
		if (!query || !bind_fixed(query->Value, 1, aIncarnation)
			|| !bind_fixed(query->Value, 2, aAuthority)) {
			return std::unexpected(query ? failure::unavailable : query.error());
		}
		if (::sqlite3_step(query->Value) != SQLITE_ROW
			|| ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 2) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 3) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 4) != SQLITE_INTEGER
			|| ::sqlite3_column_type(query->Value, 5) != SQLITE_INTEGER) {
			return std::unexpected(failure::authority_mismatch);
		}
		auto cache = read_fixed<core::repository_cache_scope_id>(query->Value, 1);
		if (!cache) {
			return std::unexpected(failure::schema_mismatch);
		}
		const auto               retained         = ::sqlite3_column_int64(query->Value, 2);
		const auto               observedRetained = ::sqlite3_column_int64(query->Value, 5);
		pull_request_scope_state output{
			.Revision = core::epoch_catalog_revision{
				::sqlite3_column_int64(query->Value, 0) },
			.Cache               = *cache,
			.RetainedEpochCount  = static_cast<std::uint64_t>(retained),
			.Lifecycle           = ::sqlite3_column_int(query->Value, 3),
			.RepositoryLifecycle = ::sqlite3_column_int(query->Value, 4),
		};
		if (retained < 0 || observedRetained < 0
			|| retained != observedRetained
			|| output.RetainedEpochCount
				> app::maximum_retained_epoch_count_per_pull_request
			|| !core::valid(output.Revision) || output.Lifecycle < 1
			|| output.Lifecycle > 4 || output.RepositoryLifecycle < 1
			|| output.RepositoryLifecycle > 4
			|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		return output;
	}

	auto require_active_pull_request_scope(
		::sqlite3*                         aDatabase,
		const app::epoch_catalog_binding&  aBinding,
		const core::epoch_catalog_revision aExpected,
		const bool                         aRequireAdvanceCapacity,
		const bool                         aRequireRetainedCapacity) -> result<void>
	{
		auto state = load_pull_request_scope_state(
			aDatabase,
			aBinding.Catalog.Incarnation,
			aBinding.Authority.Scope);
		if (!state) {
			return std::unexpected(state.error());
		}
		if (state->Cache != aBinding.Authority.Cache || state->Lifecycle != 1
			|| state->RepositoryLifecycle != 1) {
			return std::unexpected(failure::authority_mismatch);
		}
		if (state->Revision != aExpected
			|| (aRequireAdvanceCapacity
				&& state->Revision.value()
					>= core::maximum_epoch_catalog_revision)) {
			return std::unexpected(failure::revision_conflict);
		}
		if (
			aRequireRetainedCapacity
			&& state->RetainedEpochCount
				>= app::maximum_retained_epoch_count_per_pull_request) {
			return std::unexpected(failure::quota_exceeded);
		}
		return { };
	}

	auto require_no_blocking_operation(
		::sqlite3*                        aDatabase,
		const app::epoch_catalog_binding& aBinding,
		const bool                        aWholeCache) -> result<void>
	{
		auto query = prepare(
			aDatabase,
			aWholeCache
				? "SELECT 1 FROM operation WHERE incarnation = ?1 "
				  "AND cache_scope = ?2 AND terminal = 0 "
				  "LIMIT 1"
				: "SELECT 1 FROM operation WHERE incarnation = ?1 "
				  "AND cache_scope = ?2 AND pr_scope = ?3 "
				  "AND terminal NOT IN (1, 2) LIMIT 1");
		if (!query || !bind_fixed(query->Value, 1, aBinding.Catalog.Incarnation)
			|| !bind_fixed(query->Value, 2, aBinding.Authority.Cache)
			|| (!aWholeCache
				&& !bind_fixed(query->Value, 3, aBinding.Authority.Scope))) {
			return std::unexpected(query ? failure::unavailable : query.error());
		}
		const auto code = ::sqlite3_step(query->Value);
		if (code == SQLITE_ROW) {
			return std::unexpected(failure::operation_conflict);
		}
		if (code != SQLITE_DONE) {
			return std::unexpected(sql_failure(aDatabase, { }));
		}
		return { };
	}

	auto load_review_association(
		::sqlite3*                                   aDatabase,
		const core::epoch_catalog_incarnation_id&    aIncarnation,
		const core::pull_request_authority_scope_id& aAuthority,
		const core::github_viewer_actor_id           aViewer,
		const core::review_association_id&           aAssociation)
		-> result<core::catalog_review_association>
	{
		auto query = prepare(
			aDatabase,
			"SELECT review_id, actor_id, review_order, review_state, "
			"commit_algorithm, commit_digest, availability "
			"FROM review_association WHERE incarnation = ?1 "
			"AND pr_scope = ?2 AND association = ?3");
		if (!query || !bind_fixed(query->Value, 1, aIncarnation)
			|| !bind_fixed(query->Value, 2, aAuthority)
			|| !bind_fixed(query->Value, 3, aAssociation)) {
			return std::unexpected(query ? failure::unavailable : query.error());
		}
		if (::sqlite3_step(query->Value) != SQLITE_ROW) {
			return std::unexpected(failure::repair_required);
		}
		const auto review = read_u64(query->Value, 0);
		const auto actor  = read_u64(query->Value, 1);
		const auto commit = read_oid<core::git_commit_oid>(query->Value, 4, 5);
		if (!review || !actor || *actor != aViewer.value() || !commit) {
			return std::unexpected(failure::schema_mismatch);
		}
		core::catalog_review_association output{
			.Association = aAssociation,
			.Review      = core::github_review_id{ *review },
			.Actor       = core::github_viewer_actor_id{ *actor },
			.Order       = core::github_review_order{
				static_cast<std::uint16_t>(
					::sqlite3_column_int(query->Value, 2)) },
			.State        = restored_enum<core::github_review_state>(::sqlite3_column_int(query->Value, 3)),
			.Commit       = *commit,
			.Availability = restored_enum<core::anchor_availability>(::sqlite3_column_int(query->Value, 6)),
		};
		if (::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (!app::valid(output)) {
			return std::unexpected(failure::schema_mismatch);
		}
		return output;
	}

	auto canonicalize_review(
		::sqlite3*                                   aDatabase,
		const core::epoch_catalog_incarnation_id&    aIncarnation,
		const core::pull_request_authority_scope_id& aAuthority,
		const core::catalog_review_association&      aReview,
		const std::optional<std::pair<core::epoch_ordinal, core::git_commit_oid>>&
			aProjectedHead) -> result<canonical_review_fact>
	{
		canonical_review_fact output{ .Review = aReview };
		if (aProjectedHead && aReview.Commit == aProjectedHead->second) {
			output.Review.Availability = core::anchor_availability::available;
			output.LinkedOrdinal       = aProjectedHead->first;
			return output;
		}
		auto matching = prepare(
			aDatabase,
			"SELECT ordinal, availability FROM epoch_anchor "
			"WHERE incarnation = ?1 AND pr_scope = ?2 AND endpoint = 2 "
			"AND object_algorithm = ?3 AND object_digest = ?4 "
			"ORDER BY ordinal DESC LIMIT 1");
		if (!matching || !bind_fixed(matching->Value, 1, aIncarnation)
			|| !bind_fixed(matching->Value, 2, aAuthority)
			|| !bind_oid(matching->Value, 3, 4, aReview.Commit)) {
			return std::unexpected(matching ? failure::unavailable
											: matching.error());
		}
		const auto code = ::sqlite3_step(matching->Value);
		if (code == SQLITE_ROW) {
			output.LinkedOrdinal = core::epoch_ordinal{
				::sqlite3_column_int64(matching->Value, 0)
			};
			output.Review.Availability = restored_enum<core::anchor_availability>(
				::sqlite3_column_int(matching->Value, 1));
			if (!core::valid(*output.LinkedOrdinal)
				|| ::sqlite3_column_type(matching->Value, 1) != SQLITE_INTEGER
				|| ::sqlite3_step(matching->Value) != SQLITE_DONE
				|| !app::valid(output.Review)) {
				return std::unexpected(failure::schema_mismatch);
			}
			return output;
		}
		if (code != SQLITE_DONE) {
			return std::unexpected(sql_failure(aDatabase, { }));
		}
		if (aReview.Availability != core::anchor_availability::unavailable
			&& aReview.Availability
				!= core::anchor_availability::never_observed) {
			return std::unexpected(failure::operation_conflict);
		}
		return output;
	}

	auto catalog_contains_source_observation(
		::sqlite3*                                aDatabase,
		const core::epoch_catalog_incarnation_id& aIncarnation,
		const core::epoch_source_observation_id&  aObservation,
		std::stop_token                           aStop) -> result<bool>
	{
		auto query = prepare(
			aDatabase,
			"SELECT EXISTS(SELECT 1 FROM epoch "
			"INDEXED BY epoch_source_revision "
			"WHERE incarnation = ?1 AND source_observation = ?2) "
			"OR EXISTS(SELECT 1 FROM operation "
			"INDEXED BY operation_source_observation "
			"WHERE incarnation = ?3 AND kind = 1 "
			"AND source_observation = ?4)");
		if (!query || !bind_fixed(query->Value, 1, aIncarnation)
			|| !bind_fixed(query->Value, 2, aObservation)
			|| !bind_fixed(query->Value, 3, aIncarnation)
			|| !bind_fixed(query->Value, 4, aObservation)) {
			return std::unexpected(query ? failure::unavailable : query.error());
		}
		if (::sqlite3_step(query->Value) != SQLITE_ROW
			|| ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER) {
			return std::unexpected(sql_failure(aDatabase, aStop));
		}
		const auto exists = ::sqlite3_column_int(query->Value, 0);
		if ((exists != 0 && exists != 1)
			|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		return exists != 0;
	}

	auto read_occurrence_rows(::sqlite3*                        aDatabase,
							  const app::epoch_catalog_binding& aBinding, std::stop_token aStop, const bool aArchived)
		-> result<core::epoch_occurrence_history>
	{
		core::epoch_occurrence_history output;
		auto                           query = prepare(aDatabase, aArchived ? "SELECT occurrence_id, kind, event_id, head_algorithm, head_digest, local_ordinal, historical_base, derived_base, date_source, date_utc, first_seen FROM epoch_occurrence_supersession WHERE incarnation = ?1 AND pr_scope = ?2 ORDER BY occurrence_id LIMIT 1001" : "SELECT occurrence_id, kind, event_id, head_algorithm, head_digest, local_ordinal, historical_base, derived_base, date_source, date_utc, first_seen FROM epoch_occurrence WHERE incarnation = ?1 AND pr_scope = ?2 ORDER BY occurrence_id LIMIT 1001");
		if (!query || !bind_fixed(query->Value, 1, aBinding.Catalog.Incarnation)
			|| !bind_fixed(query->Value, 2, aBinding.Authority.Scope)) {
			return std::unexpected(failure::unavailable);
		}
		while (true) {
			const auto code = ::sqlite3_step(query->Value);
			if (code == SQLITE_DONE) {
				break;
			}
			if (code != SQLITE_ROW || aStop.stop_requested()) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}
			const auto id    = read_fixed<core::epoch_occurrence_id>(query->Value, 0);
			const auto event = read_u64_including_absent(query->Value, 2);
			const auto head  = read_oid<core::git_commit_oid>(query->Value, 3, 4);
			if (!id || !event || !head) {
				return std::unexpected(failure::integrity_failure);
			}
			core::epoch_occurrence occurrence{
				.Id                  = *id,
				.Evidence            = { .Kind = static_cast<core::epoch_occurrence_kind>(::sqlite3_column_int(query->Value, 1)), .EventId = *event, .Head = *head },
				.DateSource          = static_cast<core::epoch_occurrence_date_source>(::sqlite3_column_int(query->Value, 8)),
				.DateUtcMilliseconds = ::sqlite3_column_int64(query->Value, 9),
				.FirstSeen           = core::observed_utc_milliseconds{ ::sqlite3_column_int64(query->Value, 10) },
			};
			if (::sqlite3_column_type(query->Value, 5) != SQLITE_NULL) {
				occurrence.LocalEpoch = core::review_epoch_id{ aBinding.Authority.Scope, aBinding.Catalog.Incarnation,
															   core::epoch_ordinal{ ::sqlite3_column_int64(query->Value, 5) } };
			}
			if (::sqlite3_column_type(query->Value, 6) != SQLITE_NULL) {
				occurrence.HistoricalBase = read_oid<core::git_commit_oid>(query->Value, 3, 6);
				if (!occurrence.HistoricalBase) {
					return std::unexpected(failure::integrity_failure);
				}
			}
			if (::sqlite3_column_type(query->Value, 7) != SQLITE_NULL) {
				occurrence.DerivedComparisonBase = read_oid<core::git_commit_oid>(query->Value, 3, 7);
				if (!occurrence.DerivedComparisonBase) {
					return std::unexpected(failure::integrity_failure);
				}
			}
			if (!core::valid(occurrence)) {
				return std::unexpected(failure::integrity_failure);
			}
			output.Occurrences.push_back(occurrence);
		}
		if (output.Occurrences.size() > core::maximum_epoch_occurrence_count) {
			return std::unexpected(failure::quota_exceeded);
		}
		auto status = prepare(aDatabase, "SELECT incomplete, ambiguous FROM epoch_occurrence_history WHERE incarnation = ?1 AND pr_scope = ?2");
		if (!status || !bind_fixed(status->Value, 1, aBinding.Catalog.Incarnation)
			|| !bind_fixed(status->Value, 2, aBinding.Authority.Scope)) {
			return std::unexpected(failure::unavailable);
		}
		const auto statusCode = ::sqlite3_step(status->Value);
		if (statusCode == SQLITE_ROW) {
			output.Incomplete = ::sqlite3_column_int(status->Value, 0) != 0;
			output.Ambiguous  = ::sqlite3_column_int(status->Value, 1) != 0;
		} else if (statusCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(aDatabase, aStop));
		}
		auto unresolved = prepare(aDatabase, "SELECT EXISTS(SELECT 1 FROM epoch e WHERE e.incarnation = ?1 AND e.pr_scope = ?2 "
											 "AND NOT EXISTS(SELECT 1 FROM epoch_occurrence o WHERE o.incarnation = e.incarnation AND o.pr_scope = e.pr_scope AND o.local_ordinal = e.ordinal))");
		if (!unresolved || !bind_fixed(unresolved->Value, 1, aBinding.Catalog.Incarnation)
			|| !bind_fixed(unresolved->Value, 2, aBinding.Authority.Scope)
			|| ::sqlite3_step(unresolved->Value) != SQLITE_ROW) {
			return std::unexpected(failure::unavailable);
		}
		output.UnresolvedLegacy = ::sqlite3_column_int(unresolved->Value, 0) != 0;
		output.Incomplete       = output.Incomplete || output.Ambiguous || output.UnresolvedLegacy
			|| std::ranges::any_of(output.Occurrences, [](const auto& aOccurrence) { return aOccurrence.DateSource == core::epoch_occurrence_date_source::unavailable; });
		std::ranges::sort(output.Occurrences, core::epoch_occurrence_less);
		return output;
	}

	auto coherent_install_facts(
		const app::durable_git_spool_fact&     aSpool,
		const app::durable_git_preimport_fact& aPreimport,
		const app::durable_git_install_fact&   aInstall) noexcept -> bool
	{
		if (aInstall.Residency != aSpool.Residency || aInstall.Pack != aSpool.ExpectedPack
			|| aInstall.InstalledPack.Bytes != aSpool.Spool.Bytes) {
			return false;
		}
		if (!aPreimport.ExpectedPackPreexisting) {
			return true;
		}
		const auto expected = std::ranges::find_if(
			aPreimport.Packs, [&aSpool](const auto& aEntry) {
				return aEntry.Pack == aSpool.ExpectedPack;
			});
		return expected != aPreimport.Packs.end()
			&& aInstall.InstalledPack.Bytes == expected->PackBytes
			&& aInstall.InstalledIndex.Bytes == expected->IndexBytes;
	}

	auto map_failure(
		const detail::catalog_database_failure aValue) noexcept -> failure
	{
		using source = detail::catalog_database_failure;
		switch (aValue) {
		case source::invalid_path:
			return failure::invalid_path;
		case source::invalid_permissions:
			return failure::invalid_permissions;
		case source::unavailable:
			return failure::unavailable;
		case source::schema_mismatch:
			return failure::schema_mismatch;
		case source::integrity_failure:
			return failure::integrity_failure;
		case source::quota_exceeded:
			return failure::quota_exceeded;
		case source::cancelled:
			return failure::cancelled;
		case source::platform_unsupported:
			return failure::platform_unsupported;
		}
		return failure::unavailable;
	}
}
