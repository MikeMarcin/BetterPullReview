#include "catalog_database.hpp"

#include "../../native/windows_private_storage.hpp"
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"

#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/core/hex.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <filesystem>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <Windows.h>
	#include <aclapi.h>
	#include <bcrypt.h>
#else
	#include <dirent.h>
	#include <fcntl.h>
	#include <limits.h>
	#include <sys/file.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <sys/mount.h>
	#elif defined(__linux__)
		#include <linux/fs.h>
		#include <linux/magic.h>
		#include <sys/statfs.h>
		#include <sys/syscall.h>
	#endif
	#if defined(__linux__)
		#include <sys/random.h>
	#endif
#endif

namespace bpr::adapters::sqlite::detail {
	namespace {

		constexpr std::string_view CatalogName   = "epoch-catalog.sqlite3";
		constexpr std::int32_t     ApplicationId = 0x42504543;
		constexpr std::int32_t     UserVersion   = 7;
#if defined(BPR_SQLITE_TESTING)
		std::atomic_bool                                    InterruptBootstrapBeforePublication = false;
		std::atomic_bool                                    InterruptMigrationBeforeCommit      = false;
		std::atomic<testing::private_store_open_checkpoint> PrivateStoreOpenCheckpoint{ nullptr };
#endif
		static_assert(
			application::maximum_retained_epoch_count_per_pull_request == 100'000U);

		struct schema_object
		{
			std::string_view Type;
			std::string_view Name;
			std::string_view Sql;
		};

		// These are the complete BPEC v7 objects. The schema contains typed facts
		// only. Ref names are represented by their versioned components, so no path,
		// provider string, opaque token, or generic payload enters the catalog.
		constexpr std::array SchemaObjects{
			schema_object{
				"table", "append_residency",
				R"sql(CREATE TABLE append_residency(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32),
            operation BLOB NOT NULL CHECK(length(operation) = 32),
            residency INTEGER NOT NULL CHECK(residency IN (0, 1)),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation) REFERENCES append_operation(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table", "append_occurrence",
				R"sql(CREATE TABLE append_occurrence(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32),
            operation BLOB NOT NULL CHECK(length(operation) = 32),
            occurrence_id BLOB NOT NULL CHECK(length(occurrence_id) = 32 AND occurrence_id != zeroblob(32)),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation) REFERENCES append_hidden_candidate(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table", "epoch_occurrence",
				R"sql(CREATE TABLE epoch_occurrence(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32),
            occurrence_id BLOB NOT NULL CHECK(length(occurrence_id) = 32 AND occurrence_id != zeroblob(32)),
            kind INTEGER NOT NULL CHECK(kind IN (0, 1)),
            event_id BLOB NOT NULL CHECK(length(event_id) = 8 AND ((kind = 0 AND event_id != zeroblob(8)) OR (kind = 1 AND event_id = zeroblob(8)))),
            head_algorithm INTEGER NOT NULL CHECK(head_algorithm IN (1, 2)),
            head_digest BLOB NOT NULL CHECK(length(head_digest) = CASE head_algorithm WHEN 1 THEN 20 ELSE 32 END AND head_digest != zeroblob(length(head_digest))),
            local_ordinal INTEGER CHECK(local_ordinal > 0),
            historical_base BLOB CHECK(historical_base IS NULL OR (length(historical_base) = length(head_digest) AND historical_base != zeroblob(length(historical_base)))),
            derived_base BLOB CHECK(derived_base IS NULL OR (length(derived_base) = length(head_digest) AND derived_base != zeroblob(length(derived_base)))),
            date_source INTEGER NOT NULL CHECK(date_source BETWEEN 0 AND 2),
            date_utc INTEGER NOT NULL CHECK(date_source != 0 OR date_utc = 0),
            first_seen INTEGER NOT NULL,
            PRIMARY KEY(incarnation, pr_scope, occurrence_id),
            UNIQUE(incarnation, pr_scope, local_ordinal),
            FOREIGN KEY(incarnation, pr_scope) REFERENCES pull_request_scope(incarnation, pr_scope),
            FOREIGN KEY(incarnation, pr_scope, local_ordinal) REFERENCES epoch(incarnation, pr_scope, ordinal)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table", "epoch_occurrence_supersession",
				R"sql(CREATE TABLE epoch_occurrence_supersession(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32),
            occurrence_id BLOB NOT NULL CHECK(length(occurrence_id) = 32 AND occurrence_id != zeroblob(32)),
            kind INTEGER NOT NULL CHECK(kind IN (0, 1)),
            event_id BLOB NOT NULL CHECK(length(event_id) = 8 AND ((kind = 0 AND event_id != zeroblob(8)) OR (kind = 1 AND event_id = zeroblob(8)))),
            head_algorithm INTEGER NOT NULL CHECK(head_algorithm IN (1, 2)),
            head_digest BLOB NOT NULL CHECK(length(head_digest) = CASE head_algorithm WHEN 1 THEN 20 ELSE 32 END AND head_digest != zeroblob(length(head_digest))),
            local_ordinal INTEGER CHECK(local_ordinal > 0),
            historical_base BLOB CHECK(historical_base IS NULL OR (length(historical_base) = length(head_digest) AND historical_base != zeroblob(length(historical_base)))),
            derived_base BLOB CHECK(derived_base IS NULL OR (length(derived_base) = length(head_digest) AND derived_base != zeroblob(length(derived_base)))),
            date_source INTEGER NOT NULL CHECK(date_source BETWEEN 0 AND 2),
            date_utc INTEGER NOT NULL CHECK(date_source != 0 OR date_utc = 0),
            first_seen INTEGER NOT NULL,
            replacement_id BLOB NOT NULL CHECK(length(replacement_id) = 32 AND replacement_id != occurrence_id),
            PRIMARY KEY(incarnation, pr_scope, occurrence_id),
            FOREIGN KEY(incarnation, pr_scope, replacement_id) REFERENCES epoch_occurrence(incarnation, pr_scope, occurrence_id) DEFERRABLE INITIALLY DEFERRED,
            FOREIGN KEY(incarnation, pr_scope) REFERENCES pull_request_scope(incarnation, pr_scope),
            FOREIGN KEY(incarnation, pr_scope, local_ordinal) REFERENCES epoch(incarnation, pr_scope, ordinal)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table", "epoch_occurrence_history",
				R"sql(CREATE TABLE epoch_occurrence_history(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32),
            incomplete INTEGER NOT NULL CHECK(incomplete IN (0, 1)),
            ambiguous INTEGER NOT NULL CHECK(ambiguous IN (0, 1)),
            PRIMARY KEY(incarnation, pr_scope),
            FOREIGN KEY(incarnation, pr_scope) REFERENCES pull_request_scope(incarnation, pr_scope)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"catalog_meta",
				R"sql(CREATE TABLE catalog_meta(
            singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
            schema_format INTEGER NOT NULL CHECK(schema_format = 7),
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"catalog_meta_incarnation",
				R"sql(CREATE UNIQUE INDEX catalog_meta_incarnation ON catalog_meta(incarnation))sql" },
			schema_object{
				"table",
				"repository_scope",
				R"sql(CREATE TABLE repository_scope(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            cache_scope BLOB NOT NULL CHECK(length(cache_scope) = 32 AND cache_scope != zeroblob(32)),
            profile_id BLOB NOT NULL CHECK(length(profile_id) = 8 AND profile_id != zeroblob(8)),
            profile_revision BLOB NOT NULL CHECK(length(profile_revision) = 8 AND profile_revision != zeroblob(8)),
            host_id BLOB NOT NULL CHECK(length(host_id) = 8 AND host_id != zeroblob(8)),
            host_authority BLOB NOT NULL CHECK(length(host_authority) = 32 AND host_authority != zeroblob(32)),
            account_id BLOB NOT NULL CHECK(length(account_id) = 8),
            access_mode INTEGER NOT NULL CHECK(
                access_mode IN (1, 2)
                AND (access_mode = 1) = (account_id != zeroblob(8))
            ),
            repository_id BLOB NOT NULL CHECK(length(repository_id) = 8 AND repository_id != zeroblob(8)),
            object_format INTEGER NOT NULL CHECK(object_format IN (1, 2)),
            lifecycle INTEGER NOT NULL CHECK(lifecycle IN (1, 2, 3, 4)),
            PRIMARY KEY(incarnation, cache_scope),
            FOREIGN KEY(incarnation) REFERENCES catalog_meta(incarnation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"repository_scope_active_authority",
				R"sql(CREATE UNIQUE INDEX repository_scope_active_authority ON repository_scope(
            incarnation, profile_id, profile_revision, host_id, host_authority,
            access_mode, account_id, repository_id, object_format
        ) WHERE lifecycle != 4)sql" },
			schema_object{
				"index",
				"repository_scope_mode",
				R"sql(CREATE UNIQUE INDEX repository_scope_mode ON repository_scope(
            incarnation, cache_scope, access_mode
        ))sql" },
			schema_object{
				"table",
				"pull_request_scope",
				R"sql(CREATE TABLE pull_request_scope(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            cache_scope BLOB NOT NULL CHECK(length(cache_scope) = 32 AND cache_scope != zeroblob(32)),
            viewer_id BLOB NOT NULL CHECK(length(viewer_id) = 8),
            access_mode INTEGER NOT NULL CHECK(
                access_mode IN (1, 2)
                AND (access_mode = 1) = (viewer_id != zeroblob(8))
            ),
            pull_request_id BLOB NOT NULL CHECK(length(pull_request_id) = 8 AND pull_request_id != zeroblob(8)),
            pull_request_number INTEGER NOT NULL CHECK(pull_request_number BETWEEN 1 AND 2147483647),
            next_ordinal INTEGER CHECK(next_ordinal IS NULL OR next_ordinal > 0),
            catalog_revision INTEGER NOT NULL CHECK(catalog_revision >= 0),
            retained_epoch_count INTEGER NOT NULL CHECK(retained_epoch_count BETWEEN 0 AND 100000),
            latest_live_ordinal INTEGER CHECK(latest_live_ordinal IS NULL OR latest_live_ordinal > 0),
            previous_distinct_head_ordinal INTEGER CHECK(previous_distinct_head_ordinal IS NULL OR previous_distinct_head_ordinal > 0),
            lifecycle INTEGER NOT NULL CHECK(lifecycle IN (1, 2, 3, 4)),
            retention_rehydration_required INTEGER NOT NULL CHECK(retention_rehydration_required IN (0, 1)),
            PRIMARY KEY(incarnation, pr_scope),
            FOREIGN KEY(incarnation, cache_scope, access_mode)
                REFERENCES repository_scope(incarnation, cache_scope, access_mode),
            FOREIGN KEY(incarnation, pr_scope, latest_live_ordinal)
                REFERENCES epoch(incarnation, pr_scope, ordinal),
            FOREIGN KEY(incarnation, pr_scope, previous_distinct_head_ordinal)
                REFERENCES epoch(incarnation, pr_scope, ordinal)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"pull_request_scope_cache_id",
				R"sql(CREATE UNIQUE INDEX pull_request_scope_cache_id ON pull_request_scope(
            incarnation, cache_scope, pull_request_id
        ))sql" },
			schema_object{
				"index",
				"pull_request_scope_cache_number",
				R"sql(CREATE UNIQUE INDEX pull_request_scope_cache_number ON pull_request_scope(
            incarnation, cache_scope, pull_request_number
        ))sql" },
			schema_object{
				"index",
				"pull_request_scope_cache_binding",
				R"sql(CREATE UNIQUE INDEX pull_request_scope_cache_binding ON pull_request_scope(
            incarnation, cache_scope, pr_scope
        ))sql" },
			schema_object{
				"table",
				"epoch",
				R"sql(CREATE TABLE epoch(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            ordinal INTEGER NOT NULL CHECK(ordinal > 0),
            source_observation BLOB NOT NULL CHECK(length(source_observation) = 32 AND source_observation != zeroblob(32)),
            source_snapshot BLOB NOT NULL CHECK(length(source_snapshot) = 8 AND source_snapshot != zeroblob(8)),
            identity_generation BLOB NOT NULL CHECK(length(identity_generation) = 8 AND identity_generation != zeroblob(8)),
            pull_request_generation BLOB NOT NULL CHECK(length(pull_request_generation) = 8 AND pull_request_generation != zeroblob(8)),
            base_branch BLOB NOT NULL CHECK(length(base_branch) = 32 AND base_branch != zeroblob(32)),
            observed_at INTEGER NOT NULL CHECK(observed_at BETWEEN 0 AND 253402300799999),
            origin INTEGER NOT NULL CHECK(origin BETWEEN 1 AND 4),
            base_transition INTEGER NOT NULL CHECK(base_transition BETWEEN 1 AND 4),
            head_transition INTEGER NOT NULL CHECK(head_transition BETWEEN 1 AND 5),
            material_relation INTEGER NOT NULL CHECK(material_relation BETWEEN 1 AND 3),
            primary_cause INTEGER NOT NULL CHECK(primary_cause BETWEEN 1 AND 8),
            material_base_algorithm INTEGER NOT NULL CHECK(material_base_algorithm IN (1, 2)),
            material_base_digest BLOB NOT NULL CHECK(length(material_base_digest) = CASE material_base_algorithm WHEN 1 THEN 20 ELSE 32 END AND material_base_digest != zeroblob(length(material_base_digest))),
            material_head_algorithm INTEGER NOT NULL CHECK(material_head_algorithm = material_base_algorithm),
            material_head_digest BLOB NOT NULL CHECK(length(material_head_digest) = CASE material_head_algorithm WHEN 1 THEN 20 ELSE 32 END AND material_head_digest != zeroblob(length(material_head_digest))),
            material_policy_revision BLOB NOT NULL CHECK(length(material_policy_revision) = 8 AND material_policy_revision != zeroblob(8)),
            PRIMARY KEY(incarnation, pr_scope, ordinal),
            FOREIGN KEY(incarnation, pr_scope)
                REFERENCES pull_request_scope(incarnation, pr_scope),
            CHECK(
                (material_base_algorithm IS NULL AND material_base_digest IS NULL
                 AND material_head_algorithm IS NULL AND material_head_digest IS NULL
                 AND material_policy_revision IS NULL)
                OR
                (material_base_algorithm IN (1, 2)
                 AND length(material_base_digest) = CASE material_base_algorithm WHEN 1 THEN 20 ELSE 32 END
                 AND material_base_digest != zeroblob(length(material_base_digest))
                 AND material_head_algorithm = material_base_algorithm
                 AND length(material_head_digest) = CASE material_head_algorithm WHEN 1 THEN 20 ELSE 32 END
                 AND material_head_digest != zeroblob(length(material_head_digest))
                 AND length(material_policy_revision) = 8
                 AND material_policy_revision != zeroblob(8))
            )
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"epoch_source_revision",
				R"sql(CREATE UNIQUE INDEX epoch_source_revision ON epoch(
            incarnation, source_observation
        ))sql" },
			schema_object{
				"table",
				"epoch_anchor",
				R"sql(CREATE TABLE epoch_anchor(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            ordinal INTEGER NOT NULL CHECK(ordinal > 0),
            endpoint INTEGER NOT NULL CHECK(endpoint IN (1, 2)),
            object_algorithm INTEGER NOT NULL CHECK(object_algorithm IN (1, 2)),
            object_digest BLOB NOT NULL CHECK(
                length(object_digest) = CASE object_algorithm WHEN 1 THEN 20 ELSE 32 END
                AND object_digest != zeroblob(length(object_digest))
            ),
            ref_format INTEGER NOT NULL CHECK(ref_format = 1),
            availability INTEGER NOT NULL CHECK(availability BETWEEN 1 AND 6),
            PRIMARY KEY(incarnation, pr_scope, ordinal, endpoint),
            FOREIGN KEY(incarnation, pr_scope, ordinal)
                REFERENCES epoch(incarnation, pr_scope, ordinal)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"epoch_anchor_retained_ref",
				R"sql(CREATE UNIQUE INDEX epoch_anchor_retained_ref ON epoch_anchor(
            incarnation, pr_scope, ref_format, ordinal, endpoint
        ))sql" },
			schema_object{
				"index",
				"epoch_anchor_object",
				R"sql(CREATE INDEX epoch_anchor_object ON epoch_anchor(
            incarnation, pr_scope, endpoint, object_algorithm, object_digest,
            ordinal DESC
        ))sql" },
			schema_object{
				"table",
				"review_association",
				R"sql(CREATE TABLE review_association(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            association BLOB NOT NULL CHECK(length(association) = 32 AND association != zeroblob(32)),
            review_id BLOB NOT NULL CHECK(length(review_id) = 8 AND review_id != zeroblob(8)),
            actor_id BLOB NOT NULL CHECK(length(actor_id) = 8 AND actor_id != zeroblob(8)),
            review_order INTEGER NOT NULL CHECK(review_order BETWEEN 1 AND 10000),
            review_state INTEGER NOT NULL CHECK(review_state BETWEEN 1 AND 5),
            commit_algorithm INTEGER NOT NULL CHECK(commit_algorithm IN (1, 2)),
            commit_digest BLOB NOT NULL CHECK(
                length(commit_digest) = CASE commit_algorithm WHEN 1 THEN 20 ELSE 32 END
                AND commit_digest != zeroblob(length(commit_digest))
            ),
            availability INTEGER NOT NULL CHECK(availability BETWEEN 1 AND 6),
            linked_ordinal INTEGER CHECK(linked_ordinal IS NULL OR linked_ordinal > 0),
            source_snapshot BLOB NOT NULL CHECK(length(source_snapshot) = 8 AND source_snapshot != zeroblob(8)),
            PRIMARY KEY(incarnation, pr_scope, association),
            FOREIGN KEY(incarnation, pr_scope)
                REFERENCES pull_request_scope(incarnation, pr_scope),
            FOREIGN KEY(incarnation, pr_scope, linked_ordinal)
                REFERENCES epoch(incarnation, pr_scope, ordinal)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"review_association_review",
				R"sql(CREATE UNIQUE INDEX review_association_review ON review_association(
            incarnation, pr_scope, review_id
        ))sql" },
			schema_object{
				"index",
				"review_association_order",
				R"sql(CREATE UNIQUE INDEX review_association_order ON review_association(
            incarnation, pr_scope, review_order
        ))sql" },
			schema_object{
				"table",
				"current_viewer_review",
				R"sql(CREATE TABLE current_viewer_review(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            association BLOB NOT NULL CHECK(length(association) = 32 AND association != zeroblob(32)),
            accepted_snapshot BLOB NOT NULL CHECK(length(accepted_snapshot) = 8 AND accepted_snapshot != zeroblob(8)),
            PRIMARY KEY(incarnation, pr_scope),
            FOREIGN KEY(incarnation, pr_scope, association)
                REFERENCES review_association(incarnation, pr_scope, association)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"operation",
				R"sql(CREATE TABLE operation(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            kind INTEGER NOT NULL CHECK(kind BETWEEN 1 AND 3),
            phase INTEGER NOT NULL,
            last_evidence INTEGER NOT NULL CHECK(last_evidence BETWEEN 1 AND 8),
            operation_revision INTEGER NOT NULL CHECK(operation_revision > 0),
            repair_from_phase INTEGER,
            recovery_input_compacted INTEGER NOT NULL DEFAULT 0 CHECK(recovery_input_compacted IN (0, 1)),
            cache_scope BLOB NOT NULL CHECK(length(cache_scope) = 32 AND cache_scope != zeroblob(32)),
            pr_scope BLOB CHECK(pr_scope IS NULL OR (length(pr_scope) = 32 AND pr_scope != zeroblob(32))),
            expected_revision INTEGER CHECK(expected_revision IS NULL OR expected_revision >= 0),
            application_session BLOB NOT NULL CHECK(length(application_session) = 8 AND application_session != zeroblob(8)),
            request_id BLOB CHECK(request_id IS NULL OR (length(request_id) = 8 AND request_id != zeroblob(8))),
            epoch_generation BLOB CHECK(epoch_generation IS NULL OR (length(epoch_generation) = 8 AND epoch_generation != zeroblob(8))),
            source_observation BLOB CHECK(source_observation IS NULL OR (length(source_observation) = 32 AND source_observation != zeroblob(32))),
            source_snapshot BLOB CHECK(source_snapshot IS NULL OR (length(source_snapshot) = 8 AND source_snapshot != zeroblob(8))),
            identity_generation BLOB CHECK(identity_generation IS NULL OR (length(identity_generation) = 8 AND identity_generation != zeroblob(8))),
            pull_request_generation BLOB CHECK(pull_request_generation IS NULL OR (length(pull_request_generation) = 8 AND pull_request_generation != zeroblob(8))),
            reserved_ordinal INTEGER CHECK(reserved_ordinal IS NULL OR reserved_ordinal > 0),
            detail_count INTEGER NOT NULL CHECK(detail_count BETWEEN 0 AND 10000),
            retained_epoch_count INTEGER NOT NULL CHECK(retained_epoch_count >= 0),
            terminal INTEGER NOT NULL CHECK(terminal BETWEEN 0 AND 3),
            PRIMARY KEY(incarnation, operation),
            UNIQUE(incarnation, operation, kind),
            FOREIGN KEY(incarnation, cache_scope)
                REFERENCES repository_scope(incarnation, cache_scope),
            FOREIGN KEY(incarnation, cache_scope, pr_scope)
                REFERENCES pull_request_scope(incarnation, cache_scope, pr_scope),
            CHECK(
                (kind = 1 AND phase IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12))
                OR (kind = 2 AND phase IN (8, 9, 10, 11, 12, 13, 14))
                OR (kind = 3 AND phase IN (9, 10, 11, 12, 15, 16, 17, 18, 19))
            ),
            CHECK(
                (kind = 3 AND detail_count BETWEEN 1 AND 10000
                 AND retained_epoch_count >= 0
                 AND pr_scope IS NULL AND expected_revision IS NULL
                 AND epoch_generation IS NULL AND source_observation IS NULL
                 AND source_snapshot IS NULL
                 AND identity_generation IS NULL AND pull_request_generation IS NULL
                 AND reserved_ordinal IS NULL AND request_id IS NULL)
                OR
                (kind = 2 AND detail_count BETWEEN 1 AND 100
                 AND retained_epoch_count = 0
                 AND pr_scope IS NOT NULL AND expected_revision IS NOT NULL
                 AND request_id IS NULL AND epoch_generation IS NULL
                 AND source_observation IS NULL AND source_snapshot IS NULL
                 AND identity_generation IS NULL
                 AND pull_request_generation IS NULL AND reserved_ordinal IS NULL)
                OR
                (kind = 1 AND detail_count = 0 AND retained_epoch_count = 0
                 AND pr_scope IS NOT NULL AND expected_revision IS NOT NULL
                 AND request_id IS NOT NULL AND epoch_generation IS NOT NULL
                 AND source_observation IS NOT NULL
                 AND source_snapshot IS NOT NULL AND identity_generation IS NOT NULL
                 AND pull_request_generation IS NOT NULL AND reserved_ordinal IS NOT NULL)
            ),
            CHECK(
                (phase = 9 AND terminal = 1)
                OR (phase = 11 AND terminal = 2)
                OR (phase = 12 AND terminal = 3)
                OR (phase NOT IN (9, 11, 12) AND terminal = 0)
            ),
            CHECK(
                (phase = 12 AND (
                    (kind = 1 AND repair_from_phase IN (1, 2, 3, 4, 5, 6, 7, 8, 10))
                    OR (kind = 2 AND repair_from_phase IN (8, 10, 13, 14))
                    OR (kind = 3 AND repair_from_phase IN (10, 15, 16, 17, 18, 19))
                ))
                OR (phase != 12 AND repair_from_phase IS NULL)
            ),
            CHECK(
                (kind = 1 AND phase IN (9, 11) AND recovery_input_compacted = 1)
                OR ((kind != 1 OR phase NOT IN (9, 11)) AND recovery_input_compacted = 0)
            )
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"operation_recovery",
				R"sql(CREATE INDEX operation_recovery ON operation(
            incarnation, cache_scope, terminal, kind, phase, operation
        ))sql" },
			schema_object{
				"index",
				"operation_source_observation",
				R"sql(CREATE UNIQUE INDEX operation_source_observation ON operation(
            incarnation, source_observation
        ) WHERE kind = 1)sql" },
			schema_object{
				"index",
				"operation_active_pull_request",
				R"sql(CREATE UNIQUE INDEX operation_active_pull_request ON operation(
            incarnation, pr_scope
        ) WHERE pr_scope IS NOT NULL AND terminal NOT IN (1, 2))sql" },
			schema_object{
				"index",
				"operation_pull_request_binding",
				R"sql(CREATE UNIQUE INDEX operation_pull_request_binding ON operation(
            incarnation, operation, pr_scope
        ))sql" },
			schema_object{
				"index",
				"operation_cache_binding",
				R"sql(CREATE UNIQUE INDEX operation_cache_binding ON operation(
            incarnation, operation, cache_scope
        ))sql" },
			schema_object{
				"table",
				"append_operation",
				R"sql(CREATE TABLE append_operation(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            kind INTEGER NOT NULL DEFAULT 1 CHECK(kind = 1),
            base_branch BLOB NOT NULL CHECK(length(base_branch) = 32 AND base_branch != zeroblob(32)),
            revision_origin INTEGER NOT NULL CHECK(revision_origin IN (1, 2)),
            base_algorithm INTEGER NOT NULL CHECK(base_algorithm IN (1, 2)),
            base_digest BLOB NOT NULL CHECK(length(base_digest) = CASE base_algorithm WHEN 1 THEN 20 ELSE 32 END AND base_digest != zeroblob(length(base_digest))),
            head_algorithm INTEGER NOT NULL CHECK(head_algorithm = base_algorithm),
            head_digest BLOB NOT NULL CHECK(length(head_digest) = CASE head_algorithm WHEN 1 THEN 20 ELSE 32 END AND head_digest != zeroblob(length(head_digest))),
            source_review_present INTEGER NOT NULL CHECK(source_review_present IN (0, 1)),
            staging_ref_format INTEGER NOT NULL CHECK(staging_ref_format = 1),
            final_ref_format INTEGER NOT NULL CHECK(final_ref_format = 1),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation, kind)
                REFERENCES operation(incarnation, operation, kind)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"append_source_review",
				R"sql(CREATE TABLE append_source_review(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            review_id BLOB NOT NULL CHECK(length(review_id) = 8 AND review_id != zeroblob(8)),
            actor_id BLOB NOT NULL CHECK(length(actor_id) = 8 AND actor_id != zeroblob(8)),
            review_order INTEGER NOT NULL CHECK(review_order BETWEEN 1 AND 10000),
            review_state INTEGER NOT NULL CHECK(review_state BETWEEN 2 AND 5),
            commit_algorithm INTEGER NOT NULL CHECK(commit_algorithm IN (1, 2)),
            commit_digest BLOB NOT NULL CHECK(length(commit_digest) = CASE commit_algorithm WHEN 1 THEN 20 ELSE 32 END AND commit_digest != zeroblob(length(commit_digest))),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_operation(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"append_receiving",
				R"sql(CREATE TABLE append_receiving(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            spool_identity BLOB NOT NULL CHECK(length(spool_identity) = 32 AND spool_identity != zeroblob(32)),
            maximum_bytes INTEGER NOT NULL CHECK(maximum_bytes > 0 AND maximum_bytes <= 4294967296),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_operation(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"append_receipt",
				R"sql(CREATE TABLE append_receipt(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            spool_identity BLOB NOT NULL CHECK(length(spool_identity) = 32 AND spool_identity != zeroblob(32)),
            spool_role INTEGER NOT NULL CHECK(spool_role = 1),
            spool_bytes INTEGER NOT NULL CHECK(spool_bytes > 0),
            expected_pack_algorithm INTEGER NOT NULL CHECK(expected_pack_algorithm IN (1, 2)),
            expected_pack_digest BLOB NOT NULL CHECK(length(expected_pack_digest) = CASE expected_pack_algorithm WHEN 1 THEN 20 ELSE 32 END AND expected_pack_digest != zeroblob(length(expected_pack_digest))),
            observed_pack_algorithm INTEGER NOT NULL CHECK(observed_pack_algorithm = expected_pack_algorithm),
            observed_pack_digest BLOB NOT NULL CHECK(
                length(observed_pack_digest) = CASE observed_pack_algorithm WHEN 1 THEN 20 ELSE 32 END
                AND observed_pack_digest != zeroblob(length(observed_pack_digest))
                AND observed_pack_digest = expected_pack_digest
            ),
            repository_bytes INTEGER NOT NULL CHECK(repository_bytes >= 0),
            expected_pack_preexisting INTEGER NOT NULL CHECK(expected_pack_preexisting IN (0, 1)),
            inventory_count INTEGER NOT NULL CHECK(inventory_count BETWEEN 0 AND 4096),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_operation(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"append_inventory",
				R"sql(CREATE TABLE append_inventory(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            position INTEGER NOT NULL CHECK(position >= 0 AND position < 4096),
            pack_algorithm INTEGER NOT NULL CHECK(pack_algorithm IN (1, 2)),
            pack_digest BLOB NOT NULL CHECK(length(pack_digest) = CASE pack_algorithm WHEN 1 THEN 20 ELSE 32 END AND pack_digest != zeroblob(length(pack_digest))),
            pack_bytes INTEGER NOT NULL CHECK(pack_bytes > 0),
            index_bytes INTEGER NOT NULL CHECK(index_bytes > 0),
            PRIMARY KEY(incarnation, operation, position),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_receipt(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"index",
				"append_inventory_pack",
				R"sql(CREATE UNIQUE INDEX append_inventory_pack ON append_inventory(
            incarnation, operation, pack_algorithm, pack_digest
        ))sql" },
			schema_object{
				"table",
				"append_install",
				R"sql(CREATE TABLE append_install(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            pack_algorithm INTEGER NOT NULL CHECK(pack_algorithm IN (1, 2)),
            pack_digest BLOB NOT NULL CHECK(length(pack_digest) = CASE pack_algorithm WHEN 1 THEN 20 ELSE 32 END AND pack_digest != zeroblob(length(pack_digest))),
            base_algorithm INTEGER NOT NULL CHECK(base_algorithm = pack_algorithm),
            base_digest BLOB NOT NULL CHECK(length(base_digest) = CASE base_algorithm WHEN 1 THEN 20 ELSE 32 END AND base_digest != zeroblob(length(base_digest))),
            base_tree_algorithm INTEGER NOT NULL CHECK(base_tree_algorithm = pack_algorithm),
            base_tree_digest BLOB NOT NULL CHECK(length(base_tree_digest) = CASE base_tree_algorithm WHEN 1 THEN 20 ELSE 32 END AND base_tree_digest != zeroblob(length(base_tree_digest))),
            head_algorithm INTEGER NOT NULL CHECK(head_algorithm = pack_algorithm),
            head_digest BLOB NOT NULL CHECK(length(head_digest) = CASE head_algorithm WHEN 1 THEN 20 ELSE 32 END AND head_digest != zeroblob(length(head_digest))),
            head_tree_algorithm INTEGER NOT NULL CHECK(head_tree_algorithm = pack_algorithm),
            head_tree_digest BLOB NOT NULL CHECK(length(head_tree_digest) = CASE head_tree_algorithm WHEN 1 THEN 20 ELSE 32 END AND head_tree_digest != zeroblob(length(head_tree_digest))),
            head_lineage INTEGER NOT NULL CHECK(head_lineage BETWEEN 1 AND 4),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_operation(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"append_installed_artifact",
				R"sql(CREATE TABLE append_installed_artifact(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            artifact_role INTEGER NOT NULL CHECK(artifact_role IN (2, 3)),
            artifact_identity BLOB NOT NULL CHECK(length(artifact_identity) = 32 AND artifact_identity != zeroblob(32)),
            artifact_bytes INTEGER NOT NULL CHECK(artifact_bytes > 0),
            PRIMARY KEY(incarnation, operation, artifact_role),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_install(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"append_hidden_candidate",
				R"sql(CREATE TABLE append_hidden_candidate(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            material_base_algorithm INTEGER,
            material_base_digest BLOB,
            material_head_algorithm INTEGER,
            material_head_digest BLOB,
            material_policy_revision BLOB,
            observed_at INTEGER NOT NULL CHECK(observed_at BETWEEN 0 AND 253402300799999),
            origin INTEGER NOT NULL CHECK(origin BETWEEN 1 AND 4),
            base_transition INTEGER NOT NULL CHECK(base_transition BETWEEN 1 AND 4),
            head_transition INTEGER NOT NULL CHECK(head_transition BETWEEN 1 AND 5),
            material_relation INTEGER NOT NULL CHECK(material_relation BETWEEN 1 AND 3),
            primary_cause INTEGER NOT NULL CHECK(primary_cause BETWEEN 1 AND 8),
            base_availability INTEGER NOT NULL CHECK(base_availability BETWEEN 1 AND 6),
            head_availability INTEGER NOT NULL CHECK(head_availability BETWEEN 1 AND 6),
            candidate_review_present INTEGER NOT NULL CHECK(candidate_review_present IN (0, 1)),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_operation(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"append_candidate_review",
				R"sql(CREATE TABLE append_candidate_review(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            association BLOB NOT NULL CHECK(length(association) = 32 AND association != zeroblob(32)),
            review_id BLOB NOT NULL CHECK(length(review_id) = 8 AND review_id != zeroblob(8)),
            actor_id BLOB NOT NULL CHECK(length(actor_id) = 8 AND actor_id != zeroblob(8)),
            review_order INTEGER NOT NULL CHECK(review_order BETWEEN 1 AND 10000),
            review_state INTEGER NOT NULL CHECK(review_state BETWEEN 2 AND 5),
            commit_algorithm INTEGER NOT NULL CHECK(commit_algorithm IN (1, 2)),
            commit_digest BLOB NOT NULL CHECK(length(commit_digest) = CASE commit_algorithm WHEN 1 THEN 20 ELSE 32 END AND commit_digest != zeroblob(length(commit_digest))),
            availability INTEGER NOT NULL CHECK(availability BETWEEN 1 AND 6),
            PRIMARY KEY(incarnation, operation),
            FOREIGN KEY(incarnation, operation)
                REFERENCES append_hidden_candidate(incarnation, operation)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"prune_operation",
				R"sql(CREATE TABLE prune_operation(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            kind INTEGER NOT NULL DEFAULT 2 CHECK(kind = 2),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            ordinal INTEGER NOT NULL CHECK(ordinal > 0),
            base_algorithm INTEGER NOT NULL CHECK(base_algorithm IN (1, 2)),
            base_digest BLOB NOT NULL CHECK(length(base_digest) = CASE base_algorithm WHEN 1 THEN 20 ELSE 32 END AND base_digest != zeroblob(length(base_digest))),
            base_availability INTEGER NOT NULL CHECK(base_availability IN (1, 2, 5)),
            head_algorithm INTEGER NOT NULL CHECK(head_algorithm = base_algorithm),
            head_digest BLOB NOT NULL CHECK(length(head_digest) = CASE head_algorithm WHEN 1 THEN 20 ELSE 32 END AND head_digest != zeroblob(length(head_digest))),
            head_availability INTEGER NOT NULL CHECK(head_availability IN (1, 2, 5)),
            ref_format INTEGER NOT NULL CHECK(ref_format = 1),
            PRIMARY KEY(incarnation, operation, pr_scope, ordinal),
            FOREIGN KEY(incarnation, operation, kind)
                REFERENCES operation(incarnation, operation, kind),
            FOREIGN KEY(incarnation, operation, pr_scope)
                REFERENCES operation(incarnation, operation, pr_scope),
            FOREIGN KEY(incarnation, pr_scope, ordinal)
                REFERENCES epoch(incarnation, pr_scope, ordinal)
        ) STRICT, WITHOUT ROWID)sql" },
			schema_object{
				"table",
				"cache_delete_scope",
				R"sql(CREATE TABLE cache_delete_scope(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            operation BLOB NOT NULL CHECK(length(operation) = 32 AND operation != zeroblob(32)),
            kind INTEGER NOT NULL DEFAULT 3 CHECK(kind = 3),
            cache_scope BLOB NOT NULL CHECK(length(cache_scope) = 32 AND cache_scope != zeroblob(32)),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            expected_revision INTEGER NOT NULL CHECK(expected_revision >= 0),
            epoch_count INTEGER NOT NULL CHECK(epoch_count >= 0),
            PRIMARY KEY(incarnation, operation, pr_scope),
            FOREIGN KEY(incarnation, operation, kind)
                REFERENCES operation(incarnation, operation, kind),
            FOREIGN KEY(incarnation, operation, cache_scope)
                REFERENCES operation(incarnation, operation, cache_scope),
            FOREIGN KEY(incarnation, cache_scope, pr_scope)
                REFERENCES pull_request_scope(incarnation, cache_scope, pr_scope)
        ) STRICT, WITHOUT ROWID)sql" },
		};

		[[nodiscard]] auto create_schema(
			::sqlite3*                          aDatabase,
			const std::array<std::uint8_t, 32>& aIncarnation,
			std::stop_token                     aStop) -> std::expected<void, failure>
		{
			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};
			if (auto app = execute(aDatabase, "PRAGMA application_id = 1112556867", aStop);
				!app) {
				rollback();
				return app;
			}
			if (auto version = execute(aDatabase, "PRAGMA user_version = 7", aStop);
				!version) {
				rollback();
				return version;
			}
			for (const auto& object : SchemaObjects) {
				if (auto result = execute(aDatabase, object.Sql, aStop); !result) {
					rollback();
					return result;
				}
			}
			auto insert = prepare(
				aDatabase,
				"INSERT INTO catalog_meta(singleton, schema_format, incarnation) "
				"VALUES(1, 7, ?1)");
			if (!insert) {
				rollback();
				return std::unexpected(insert.error());
			}
			if (::sqlite3_bind_blob(
					insert->Value,
					1,
					aIncarnation.data(),
					static_cast<int>(aIncarnation.size()),
					SQLITE_TRANSIENT)
				!= SQLITE_OK) {
				rollback();
				return std::unexpected(failure::unavailable);
			}
			if (::sqlite3_step(insert->Value) != SQLITE_DONE) {
				const auto writeFailure = catalog_sql_failure(aDatabase);
				rollback();
				return std::unexpected(writeFailure);
			}
			if (auto commit = execute(aDatabase, "COMMIT", aStop); !commit) {
				rollback();
				return commit;
			}
			return { };
		}

		[[nodiscard]] auto schema_sql(const std::string_view aName, const std::int64_t aVersion = UserVersion) noexcept
			-> std::string_view
		{
			if (aName == "catalog_meta" && aVersion == 6) {
				return R"sql(CREATE TABLE catalog_meta(
            singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
            schema_format INTEGER NOT NULL CHECK(schema_format = 6),
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))
        ) STRICT, WITHOUT ROWID)sql";
			}
			if (aName == "catalog_meta" && aVersion == 5) {
				return R"sql(CREATE TABLE catalog_meta(
            singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
            schema_format INTEGER NOT NULL CHECK(schema_format = 5),
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))
        ) STRICT, WITHOUT ROWID)sql";
			}
			if (aName == "catalog_meta" && aVersion == 4) {
				return R"sql(CREATE TABLE catalog_meta(
            singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
            schema_format INTEGER NOT NULL CHECK(schema_format = 4),
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))
        ) STRICT, WITHOUT ROWID)sql";
			}
			if (aName == "catalog_meta" && aVersion == 3) {
				return R"sql(CREATE TABLE catalog_meta(
            singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
            schema_format INTEGER NOT NULL CHECK(schema_format = 3),
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))
        ) STRICT, WITHOUT ROWID)sql";
			}
			if (aName == "catalog_meta" && aVersion == 2) {
				return R"sql(CREATE TABLE catalog_meta(
            singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
            schema_format INTEGER NOT NULL CHECK(schema_format = 2),
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))
        ) STRICT, WITHOUT ROWID)sql";
			}
			for (const auto& object : SchemaObjects) {
				if (object.Name == aName) {
					return object.Sql;
				}
			}
			return { };
		}

		// The v1 -> v2 upgrade. Version 1 held one authority shape, and every
		// row it holds is an authenticated one: the mode did not exist, so no
		// row could have been written under any other. The upgrade therefore
		// asserts nothing about the data it finds beyond what v1's own
		// constraints already guaranteed - it rewrites each row under the
		// discriminated schema with `access_mode` fixed at authenticated,
		// leaving account, viewer, repository, and cache identity untouched.
		//
		// The three tables whose constraints changed are rebuilt rather than
		// altered, because the change is a relaxation of a column CHECK
		// (`account_id`/`viewer_id` may now be zero, but only under the public
		// mode) and SQLite cannot drop a CHECK in place. Each is recreated
		// from the same DDL text `validate_schema` compares against, so a
		// migrated catalog and a freshly bootstrapped one are the same schema
		// byte for byte rather than merely equivalent - no rename is used,
		// because a rename would rewrite the stored text and the two would
		// stop matching.
		//
		// Everything runs in one transaction with foreign keys deferred to its
		// commit, so a failure at any point - including the injected one -
		// leaves a complete v1 catalog with its version, its schema, and its
		// rows exactly as they were. Nothing is ever recreated from empty: a
		// version this build cannot read is refused before this function
		// begins any work.
		[[nodiscard]] auto migrate_schema_v1(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			progress_scope progress{ aDatabase, aStop };
			const auto     applicationId = scalar_integer(aDatabase, "PRAGMA application_id");
			if (!applicationId || *applicationId != ApplicationId) {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto userVersion = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!userVersion) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (*userVersion == 2) {
				return { };
			}
			// Zero, a negative value, a version this build never wrote, and a
			// version a newer build wrote are all the same answer: this build
			// does not know what the file means, so it refuses it. None of
			// them is recreated, downgraded, or repaired - a catalog is
			// never silently discarded to make an open succeed.
			if (*userVersion != 1) {
				return std::unexpected(failure::schema_mismatch);
			}

			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};
			const auto step = [&](const std::string_view aSql)
				-> std::expected<void, failure> {
				if (aSql.empty()) {
					return std::unexpected(failure::schema_mismatch);
				}
				return execute(aDatabase, aSql, aStop);
			};
			const auto counted = [&](const std::string_view aSql)
				-> std::expected<std::int64_t, failure> {
				return scalar_integer(aDatabase, aSql);
			};

			const auto metaBefore       = counted("SELECT count(*) FROM catalog_meta");
			const auto repositoryBefore = counted("SELECT count(*) FROM repository_scope");
			const auto pullRequestBefore
				= counted("SELECT count(*) FROM pull_request_scope");
			if (!metaBefore || !repositoryBefore || !pullRequestBefore) {
				rollback();
				return std::unexpected(failure::schema_mismatch);
			}

			constexpr std::array<std::string_view, 3> rebuiltNames{
				"catalog_meta", "repository_scope", "pull_request_scope"
			};
			constexpr std::array<std::string_view, 6> rebuiltIndexNames{
				"catalog_meta_incarnation",
				"repository_scope_active_authority",
				"repository_scope_mode",
				"pull_request_scope_cache_id",
				"pull_request_scope_cache_number",
				"pull_request_scope_cache_binding"
			};
			const std::array<std::string_view, 12> preparation{
				// Deferred rather than disabled: every reference the rebuild
				// breaks is checked again at the commit that ends it, so a
				// migration that lost a row cannot commit.
				"PRAGMA defer_foreign_keys = ON",
				"CREATE TABLE migration_catalog_meta AS SELECT * FROM catalog_meta",
				"CREATE TABLE migration_repository_scope AS "
				"SELECT * FROM repository_scope",
				"CREATE TABLE migration_pull_request_scope AS "
				"SELECT * FROM pull_request_scope",
				"DROP TABLE pull_request_scope",
				"DROP TABLE repository_scope",
				"DROP TABLE catalog_meta",
				schema_sql(rebuiltNames[0], 2),
				schema_sql(rebuiltIndexNames[0]),
				schema_sql(rebuiltNames[1]),
				schema_sql(rebuiltIndexNames[1]),
				schema_sql(rebuiltIndexNames[2]),
			};
			const std::array<std::string_view, 4> remainder{
				schema_sql(rebuiltNames[2]),
				schema_sql(rebuiltIndexNames[3]),
				schema_sql(rebuiltIndexNames[4]),
				schema_sql(rebuiltIndexNames[5]),
			};
			const std::array<std::string_view, 3> copies{
				"INSERT INTO catalog_meta(singleton, schema_format, incarnation) "
				"SELECT singleton, 2, incarnation FROM migration_catalog_meta",
				// Every migrated row is authenticated. The mode is the only
				// column this adds; account, repository, cache, and lifecycle
				// identity are carried across unchanged.
				"INSERT INTO repository_scope(incarnation, cache_scope, profile_id, "
				"profile_revision, host_id, host_authority, account_id, access_mode, "
				"repository_id, object_format, lifecycle) "
				"SELECT incarnation, cache_scope, profile_id, profile_revision, "
				"host_id, host_authority, account_id, 1, repository_id, "
				"object_format, lifecycle FROM migration_repository_scope",
				"INSERT INTO pull_request_scope(incarnation, pr_scope, cache_scope, "
				"viewer_id, access_mode, pull_request_id, pull_request_number, "
				"next_ordinal, catalog_revision, retained_epoch_count, "
				"latest_live_ordinal, previous_distinct_head_ordinal, lifecycle, "
				"retention_rehydration_required) "
				"SELECT incarnation, pr_scope, cache_scope, viewer_id, 1, "
				"pull_request_id, pull_request_number, next_ordinal, "
				"catalog_revision, retained_epoch_count, latest_live_ordinal, "
				"previous_distinct_head_ordinal, lifecycle, "
				"retention_rehydration_required FROM migration_pull_request_scope",
			};
			const std::array<std::string_view, 4> completion{
				"DROP TABLE migration_pull_request_scope",
				"DROP TABLE migration_repository_scope",
				"DROP TABLE migration_catalog_meta",
				"PRAGMA user_version = 2",
			};

			for (const auto sql : preparation) {
				if (auto applied = step(sql); !applied) {
					rollback();
					return applied;
				}
			}
			for (const auto sql : remainder) {
				if (auto applied = step(sql); !applied) {
					rollback();
					return applied;
				}
			}
			for (const auto sql : copies) {
				if (auto applied = step(sql); !applied) {
					rollback();
					return applied;
				}
			}

			// The row counts are compared before the temporary copies are
			// dropped, so a copy that silently lost or duplicated a row is
			// caught while the evidence still exists.
			const auto metaAfter       = counted("SELECT count(*) FROM catalog_meta");
			const auto repositoryAfter = counted("SELECT count(*) FROM repository_scope");
			const auto pullRequestAfter
				= counted("SELECT count(*) FROM pull_request_scope");
			if (
				!metaAfter || !repositoryAfter || !pullRequestAfter
				|| *metaAfter != *metaBefore
				|| *repositoryAfter != *repositoryBefore
				|| *pullRequestAfter != *pullRequestBefore) {
				rollback();
				return std::unexpected(failure::integrity_failure);
			}

			for (const auto sql : completion) {
				if (auto applied = step(sql); !applied) {
					rollback();
					return applied;
				}
			}

			auto foreignKeyCheck = prepare(aDatabase, "PRAGMA foreign_key_check");
			if (!foreignKeyCheck
				|| ::sqlite3_step(foreignKeyCheck->Value) != SQLITE_DONE) {
				rollback();
				return std::unexpected(failure::integrity_failure);
			}

#if defined(BPR_SQLITE_TESTING)
			// Armed by a test to prove the rollback is real: everything above
			// has already run, so what survives is exactly what an interrupted
			// migration leaves behind.
			if (InterruptMigrationBeforeCommit.exchange(
					false, std::memory_order_relaxed)) {
				rollback();
				return std::unexpected(failure::cancelled);
			}
#endif

			if (auto commit = execute(aDatabase, "COMMIT", aStop); !commit) {
				rollback();
				return commit;
			}
			return { };
		}

		[[nodiscard]] auto validate_schema(::sqlite3* aDatabase, std::stop_token aStop, const std::int64_t aVersion = UserVersion)
			-> std::expected<void, failure>
		{
			progress_scope progress{ aDatabase, aStop };
			const auto     applicationId = scalar_integer(aDatabase, "PRAGMA application_id");
			const auto     userVersion   = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!applicationId || *applicationId != ApplicationId || !userVersion
				|| *userVersion != aVersion) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto meta = prepare(
				aDatabase,
				"SELECT schema_format, incarnation FROM catalog_meta WHERE singleton = 1");
			if (!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
				|| ::sqlite3_column_int64(meta->Value, 0) != aVersion
				|| ::sqlite3_column_type(meta->Value, 1) != SQLITE_BLOB
				|| ::sqlite3_column_bytes(meta->Value, 1) != 32) {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto* incarnation = static_cast<const std::uint8_t*>(
				::sqlite3_column_blob(meta->Value, 1));
			if (incarnation == nullptr
				|| std::all_of(incarnation, incarnation + 32, [](const auto aByte) {
					   return aByte == 0U;
				   })
				|| ::sqlite3_step(meta->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto objects = prepare(
				aDatabase,
				"SELECT type, name, sql FROM sqlite_schema "
				"WHERE name NOT LIKE 'sqlite_%' ORDER BY name");
			if (!objects) {
				return std::unexpected(objects.error());
			}
			std::vector<schema_object> expected{
				SchemaObjects.begin(), SchemaObjects.end()
			};
			if (aVersion < UserVersion) {
				std::erase_if(expected, [](const auto& aObject) { return aObject.Name == "append_residency"; });
				if (aVersion < 5) {
					std::erase_if(expected, [](const auto& aObject) { return aObject.Name == "append_occurrence"; });
				}
				for (auto& object : expected) {
					if (object.Name == "catalog_meta") {
						object.Sql = schema_sql("catalog_meta", aVersion);
					}
				}
			}
			if (aVersion < 4) {
				std::erase_if(expected, [](const auto& aObject) { return aObject.Name == "epoch_occurrence" || aObject.Name == "epoch_occurrence_history" || aObject.Name == "epoch_occurrence_supersession"; });
				if (aVersion == 2) {
					std::erase_if(expected, [](const auto& aObject) { return aObject.Name == "append_receiving"; });
				}
				for (auto& object : expected) {
					if (object.Name == "catalog_meta") {
						object.Sql = schema_sql("catalog_meta", aVersion);
					}
				}
			}
			std::ranges::sort(expected, { }, &schema_object::Name);
			std::size_t index = 0;
			while (true) {
				const auto code = ::sqlite3_step(objects->Value);
				if (code == SQLITE_DONE) {
					break;
				}
				if (code == SQLITE_INTERRUPT || aStop.stop_requested()) {
					return std::unexpected(failure::cancelled);
				}
				if (code != SQLITE_ROW || index >= expected.size()) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto readText = [&objects](const int aColumn)
					-> std::optional<std::string_view> {
					if (::sqlite3_column_type(objects->Value, aColumn) != SQLITE_TEXT) {
						return std::nullopt;
					}
					const auto* value = reinterpret_cast<const char*>(
						::sqlite3_column_text(objects->Value, aColumn));
					const auto bytes = ::sqlite3_column_bytes(objects->Value, aColumn);
					if (value == nullptr || bytes < 0) {
						return std::nullopt;
					}
					return std::string_view{ value, static_cast<std::size_t>(bytes) };
				};
				const auto type = readText(0);
				const auto name = readText(1);
				const auto sql  = readText(2);
				if (!type || !name || !sql || *type != expected[index].Type
					|| *name != expected[index].Name
					|| canonical_sql(*sql) != canonical_sql(expected[index].Sql)) {
					return std::unexpected(failure::schema_mismatch);
				}
				++index;
			}
			if (index != expected.size()) {
				return std::unexpected(failure::schema_mismatch);
			}

			const auto integrity = scalar_text(aDatabase, "PRAGMA integrity_check(1)");
			if (!integrity || *integrity != "ok") {
				return std::unexpected(failure::integrity_failure);
			}
			auto foreignKeyCheck = prepare(aDatabase, "PRAGMA foreign_key_check");
			if (!foreignKeyCheck
				|| ::sqlite3_step(foreignKeyCheck->Value) != SQLITE_DONE) {
				return std::unexpected(failure::integrity_failure);
			}
			return { };
		}

		[[nodiscard]] auto migrate_schema(::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto version = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!version || (*version != 1 && *version != 2 && *version != 3 && *version != 4 && *version != 5 && *version != 6 && *version != UserVersion)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (*version == UserVersion) {
				return { };
			}
			if (*version == 1) {
				if (auto migrated = migrate_schema_v1(aDatabase, aStop); !migrated) {
					return migrated;
				}
			}
			if (auto validated = validate_schema(aDatabase, aStop, *version >= 3 ? *version : 2); !validated) {
				return validated;
			}
			if (auto begun = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begun) {
				return begun;
			}
			const auto rollback = [&] { (void)::sqlite3_exec(aDatabase, "ROLLBACK", nullptr, nullptr, nullptr); };
			// Rebuild only the versioned metadata CHECK. Existing catalog, authority,
			// epoch and operation rows stay in place; legacy operations gain no facts.
			const std::array<std::string_view, 15> statements{
				"PRAGMA defer_foreign_keys = ON",
				"CREATE TABLE migration_catalog_meta AS SELECT * FROM catalog_meta",
				"DROP TABLE catalog_meta",
				schema_sql("catalog_meta"),
				schema_sql("catalog_meta_incarnation"),
				"INSERT INTO catalog_meta(singleton, schema_format, incarnation) SELECT singleton, 7, incarnation FROM migration_catalog_meta",
				"DROP TABLE migration_catalog_meta",
				*version >= 3 ? std::string_view{ "SELECT 1" } : schema_sql("append_receiving"),
				*version >= 4 ? std::string_view{ "SELECT 1" } : schema_sql("epoch_occurrence"),
				*version >= 4 ? std::string_view{ "SELECT 1" } : schema_sql("epoch_occurrence_history"),
				*version >= 4 ? std::string_view{ "SELECT 1" } : schema_sql("epoch_occurrence_supersession"),
				*version >= 5 ? std::string_view{ "SELECT 1" } : schema_sql("append_occurrence"),
				// Pre-v6 bindings lack selected-base provenance. Keep every retained
				// epoch and journal revision; only the active derived association expires.
				*version >= 6 ? std::string_view{ "SELECT 1" } : std::string_view{ "UPDATE epoch_occurrence SET local_ordinal = NULL, derived_base = NULL WHERE derived_base IS NOT NULL OR EXISTS (SELECT 1 FROM epoch e WHERE e.incarnation = epoch_occurrence.incarnation AND e.pr_scope = epoch_occurrence.pr_scope AND e.ordinal = epoch_occurrence.local_ordinal AND e.origin = 3)" },
				schema_sql("append_residency"),
				"PRAGMA user_version = 7",
			};
			for (const auto statement : statements) {
				if (auto applied = execute(aDatabase, statement, aStop); !applied) {
					rollback();
					return applied;
				}
			}
			if (auto validated = validate_schema(aDatabase, aStop); !validated) {
				rollback();
				return validated;
			}
#if defined(BPR_SQLITE_TESTING)
			if (InterruptMigrationBeforeCommit.exchange(false, std::memory_order_relaxed)) {
				rollback();
				return std::unexpected(failure::cancelled);
			}
#endif
			if (auto committed = execute(aDatabase, "COMMIT", aStop); !committed) {
				rollback();
				return committed;
			}
			return { };
		}

		[[nodiscard]] auto known_runtime_table(const char* aValue) noexcept -> bool
		{
			if (aValue == nullptr) {
				return false;
			}
			constexpr std::array<std::string_view, 24> tables{
				"append_occurrence",
				"epoch_occurrence",
				"epoch_occurrence_history",
				"epoch_occurrence_supersession",
				"catalog_meta",
				"repository_scope",
				"pull_request_scope",
				"epoch",
				"epoch_anchor",
				"review_association",
				"current_viewer_review",
				"operation",
				"append_operation",
				"append_source_review",
				"append_receiving",
				"append_receipt",
				"append_inventory",
				"append_install",
				"append_residency",
				"append_installed_artifact",
				"append_hidden_candidate",
				"append_candidate_review",
				"prune_operation",
				"cache_delete_scope",
			};
			return std::ranges::find(tables, std::string_view{ aValue }) != tables.end();
		}

		auto runtime_authorizer(
			void*,
			const int   aAction,
			const char* aFirst,
			const char* aSecond,
			const char*,
			const char*) noexcept -> int
		{
			switch (aAction) {
			case SQLITE_SELECT:
			case SQLITE_TRANSACTION:
			case SQLITE_SAVEPOINT:
			case SQLITE_RECURSIVE:
				return SQLITE_OK;
			case SQLITE_READ:
			case SQLITE_INSERT:
			case SQLITE_UPDATE:
			case SQLITE_DELETE:
				return known_runtime_table(aFirst) ? SQLITE_OK : SQLITE_DENY;
			case SQLITE_FUNCTION:
				if (aSecond != nullptr) {
					const std::string_view function{ aSecond };
					if (function == "count" || function == "length"
						|| function == "zeroblob") {
						return SQLITE_OK;
					}
				}
				return SQLITE_DENY;
			default:
				return SQLITE_DENY;
			}
		}

		[[nodiscard]] auto install_runtime_authorizer(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			const auto code = ::sqlite3_set_authorizer(aDatabase, runtime_authorizer, nullptr);
			return code == SQLITE_OK
				? std::expected<void, failure>{ }
				: std::expected<void, failure>{ std::unexpected(
					  catalog_failure_from_sqlite_result(code)) };
		}

		[[nodiscard]] auto open_sqlite(
			const std::filesystem::path& aPath,
			const char*                  aRequestedVfs = nullptr)
			-> std::expected<::sqlite3*, failure>
		{
			if (!sqlite_runtime_ready()) {
				return std::unexpected(failure::unavailable);
			}
			::sqlite3*     database = nullptr;
			constexpr auto flags    = SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX
				| SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_NOFOLLOW
				| SQLITE_OPEN_EXRESCODE;
#if defined(_WIN32)
			const auto encoded = aPath.u8string();
			const auto native  = std::string{
				reinterpret_cast<const char*>(encoded.data()), encoded.size()
			};
			const auto* vfs          = aRequestedVfs != nullptr
				? aRequestedVfs
				: "win32-longpath";
			const auto* selected_vfs = ::sqlite3_vfs_find(vfs);
			if (selected_vfs == nullptr
				|| native.size() + 8U
					>= static_cast<std::size_t>(selected_vfs->mxPathname)) {
				return std::unexpected(failure::invalid_path);
			}
#else
			const auto  native = aPath.string();
			const auto* vfs    = aRequestedVfs;
#endif
			const auto code = ::sqlite3_open_v2(native.c_str(), &database, flags, vfs);
			if (code != SQLITE_OK || database == nullptr) {
				const auto openFailure = database != nullptr
					? catalog_sql_failure(database)
					: catalog_failure_from_sqlite_result(code);
				if (database != nullptr) {
					(void)::sqlite3_close_v2(database);
				}
				return std::unexpected(openFailure);
			}
			return database;
		}

#if !defined(_WIN32)
		class file_descriptor final
		{
		public:
			file_descriptor() = default;
			explicit file_descriptor(const int aValue) noexcept
				: Value{ aValue }
			{ }
			file_descriptor(const file_descriptor&)                    = delete;
			auto operator=(const file_descriptor&) -> file_descriptor& = delete;
			file_descriptor(file_descriptor&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, -1) }
			{
			}
			auto operator=(file_descriptor&& aOther) noexcept -> file_descriptor&
			{
				if (this != &aOther) {
					if (Value >= 0) {
						(void)::close(Value);
					}
					Value = std::exchange(aOther.Value, -1);
				}
				return *this;
			}
			~file_descriptor()
			{
				if (Value >= 0) {
					(void)::close(Value);
				}
			}

			int Value = -1;
		};

		[[nodiscard]] auto descriptor_child_path(
			const file_descriptor& aParent,
			std::string_view       aName) -> std::filesystem::path
		{
	#if defined(__APPLE__)
			std::array<char, PATH_MAX> path{ };
			if (::fcntl(aParent.Value, F_GETPATH, path.data()) != 0) {
				return { };
			}
			return std::filesystem::path{ path.data() } / std::string{ aName };
	#elif defined(__linux__)
			constexpr std::string_view Prefix = "/proc/self/fd/";
	#else
			constexpr std::string_view Prefix{ };
	#endif
	#if !defined(__APPLE__)
			return std::filesystem::path{
				std::string{ Prefix } + std::to_string(aParent.Value)
			}
			/ std::string{ aName };
	#endif
		}

		[[nodiscard]] auto private_directory(const struct stat& aStatus) noexcept
			-> bool
		{
			return S_ISDIR(aStatus.st_mode) && aStatus.st_uid == ::geteuid()
				&& (aStatus.st_mode & static_cast<mode_t>(0777)) == 0700;
		}

		[[nodiscard]] auto private_file(const struct stat& aStatus) noexcept -> bool
		{
			return S_ISREG(aStatus.st_mode) && aStatus.st_uid == ::geteuid()
				&& (aStatus.st_mode & static_cast<mode_t>(0777)) == 0600 && aStatus.st_nlink == 1;
		}

		[[nodiscard]] auto open_private_directory(const std::filesystem::path& aPath)
			-> std::expected<file_descriptor, failure>
		{
			const auto      native = aPath.string();
			file_descriptor result{
				::open(native.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)
			};
			struct stat status{ };
			if (result.Value < 0 || ::fstat(result.Value, &status) != 0) {
				return std::unexpected(failure::invalid_path);
			}
			if (!private_directory(status)) {
				return std::unexpected(failure::invalid_permissions);
			}
			return result;
		}

		[[nodiscard]] auto ensure_private_child(
			const file_descriptor& aParent, std::string_view aName)
			-> std::expected<file_descriptor, failure>
		{
			const std::string ownedName{ aName };
			const auto        created = ::mkdirat(aParent.Value, ownedName.c_str(), 0700) == 0;
			if (!created && errno != EEXIST) {
				return std::unexpected(failure::invalid_path);
			}
			file_descriptor result{ ::openat(
				aParent.Value,
				ownedName.c_str(),
				O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW) };
			struct stat     status{ };
			if (result.Value < 0 || ::fstat(result.Value, &status) != 0) {
				return std::unexpected(failure::invalid_path);
			}
			if (!private_directory(status)) {
				return std::unexpected(failure::invalid_permissions);
			}
			if (::fsync(result.Value) != 0 || ::fsync(aParent.Value) != 0) {
				return std::unexpected(failure::unavailable);
			}
			return result;
		}

		[[nodiscard]] auto reliable_local_filesystem(
			const file_descriptor& aDirectory) noexcept -> bool
		{
			struct statfs filesystem{ };
			if (::fstatfs(aDirectory.Value, &filesystem) != 0) {
				return false;
			}
	#if defined(__APPLE__)
			return (filesystem.f_flags & MNT_LOCAL) != 0;
	#elif defined(__linux__)
			switch (static_cast<unsigned long>(filesystem.f_type)) {
			case EXT4_SUPER_MAGIC:
			case XFS_SUPER_MAGIC:
			case BTRFS_SUPER_MAGIC:
			case TMPFS_MAGIC:
			case F2FS_SUPER_MAGIC:
			case OVERLAYFS_SUPER_MAGIC:
				return true;
			default:
				return false;
			}
	#else
			(void)filesystem;
			return false;
	#endif
		}

		[[nodiscard]] auto random_bytes(std::span<std::uint8_t> aOutput) noexcept
			-> bool
		{
	#if defined(__APPLE__)
			::arc4random_buf(aOutput.data(), aOutput.size());
			return true;
	#elif defined(__linux__)
			std::size_t completed = 0;
			while (completed != aOutput.size()) {
				const auto result = ::getrandom(
					aOutput.data() + completed, aOutput.size() - completed, 0);
				if (result < 0 && errno == EINTR) {
					continue;
				}
				if (result <= 0) {
					return false;
				}
				completed += static_cast<std::size_t>(result);
			}
			return true;
	#else
			(void)aOutput;
			return false;
	#endif
		}

		[[nodiscard]] auto hex_name(const std::array<std::uint8_t, 32>& aRandom)
			-> std::string
		{
			std::string result = ".epoch-catalog.bootstrap-";
			core::append_lower_hex(result, aRandom);
			result.append(".sqlite3");
			return result;
		}

	#if defined(__linux__)
		class retained_linux_vfs final
		{
		public:
			retained_linux_vfs(const retained_linux_vfs&) = delete;
			auto operator=(const retained_linux_vfs&)
				-> retained_linux_vfs& = delete;

			~retained_linux_vfs()
			{
				if (Registered) {
					(void)::sqlite3_vfs_unregister(&Interface);
				}
			}

			[[nodiscard]] static auto create()
				-> std::expected<std::unique_ptr<retained_linux_vfs>, failure>
			{
				if (!sqlite_runtime_ready()) {
					return std::unexpected(failure::unavailable);
				}
				auto* const base = ::sqlite3_vfs_find(nullptr);
				if (base == nullptr || base->xFullPathname == nullptr) {
					return std::unexpected(failure::platform_unsupported);
				}

				auto random = std::array<std::uint8_t, 16U>{ };
				if (!random_bytes(random)) {
					return std::unexpected(failure::unavailable);
				}
				auto name = std::string{ "bpr-retained-linux-catalog-" };
				core::append_lower_hex(name, random);

				auto result = std::unique_ptr<retained_linux_vfs>{
					new retained_linux_vfs{ *base, std::move(name) }
				};
				if (::sqlite3_vfs_register(&result->Interface, 0) != SQLITE_OK) {
					return std::unexpected(failure::unavailable);
				}
				result->Registered = true;
				return result;
			}

			[[nodiscard]] auto name() const noexcept -> const char*
			{
				return Name.c_str();
			}

		private:
			retained_linux_vfs(::sqlite3_vfs& base, std::string name)
				: Interface{ base }
				, Name{ std::move(name) }
			{
				Interface.pNext         = nullptr;
				Interface.zName         = Name.c_str();
				Interface.xFullPathname = &full_pathname;
			}

			[[nodiscard]] static auto descriptor_path(
				const std::string_view path) noexcept -> bool
			{
				constexpr std::string_view Prefix = "/proc/self/fd/";
				if (!path.starts_with(Prefix)) {
					return false;
				}
				const auto separator = path.find('/', Prefix.size());
				if (separator == std::string_view::npos
					|| separator == Prefix.size()) {
					return false;
				}
				const auto descriptor = path.substr(
					Prefix.size(), separator - Prefix.size());
				const auto name = path.substr(separator + 1U);
				return std::ranges::all_of(descriptor, [](const char value) {
						   return value >= '0' && value <= '9';
					   })
					&& !name.empty() && name != "." && name != ".." && name.find('/') == std::string_view::npos;
			}

			static auto full_pathname(
				::sqlite3_vfs*,
				const char* input,
				const int   output_size,
				char*       output) noexcept -> int
			{
				if (input == nullptr || output == nullptr || output_size <= 0) {
					return SQLITE_CANTOPEN;
				}
				const auto path = std::string_view{ input };
				if (!descriptor_path(path)
					|| path.size() + 1U > static_cast<std::size_t>(output_size)) {
					return SQLITE_CANTOPEN;
				}
				std::memcpy(output, path.data(), path.size());
				output[path.size()] = '\0';
				return SQLITE_OK;
			}

			::sqlite3_vfs Interface{ };
			std::string   Name;
			bool          Registered = false;
		};
	#endif

		[[nodiscard]] auto bootstrap_artifact_name(std::string_view aName) noexcept
			-> bool
		{
			constexpr std::string_view                prefix = ".epoch-catalog.bootstrap-";
			constexpr std::array<std::string_view, 4> suffixes{
				".sqlite3",
				".sqlite3-wal",
				".sqlite3-shm",
				".sqlite3-journal",
			};
			for (const auto suffix : suffixes) {
				if (aName.size() != prefix.size() + 64U + suffix.size()
					|| !aName.starts_with(prefix) || !aName.ends_with(suffix)) {
					continue;
				}
				return std::ranges::all_of(
					aName.substr(prefix.size(), 64U), [](const char aValue) {
						return (aValue >= '0' && aValue <= '9')
							|| (aValue >= 'a' && aValue <= 'f');
					});
			}
			return false;
		}

		[[nodiscard]] auto cleanup_bootstrap_orphans(
			const file_descriptor& aParent) -> std::expected<void, failure>
		{
			const auto duplicate = ::dup(aParent.Value);
			if (duplicate < 0) {
				return std::unexpected(failure::unavailable);
			}
			auto* directory = ::fdopendir(duplicate);
			if (directory == nullptr) {
				(void)::close(duplicate);
				return std::unexpected(failure::unavailable);
			}
			auto closeDirectory = [&directory]() noexcept {
				if (directory != nullptr) {
					(void)::closedir(directory);
					directory = nullptr;
				}
			};
			auto        removed = false;
			std::size_t entries = 0U;
			errno               = 0;
			while (const auto* entry = ::readdir(directory)) {
				if (++entries > 4'096U) {
					closeDirectory();
					return std::unexpected(failure::unavailable);
				}
				const std::string_view name{ entry->d_name };
				if (!bootstrap_artifact_name(name)) {
					continue;
				}
				struct stat status{ };
				if (::fstatat(
						aParent.Value,
						entry->d_name,
						&status,
						AT_SYMLINK_NOFOLLOW)
						!= 0
					|| !private_file(status)) {
					closeDirectory();
					return std::unexpected(failure::invalid_permissions);
				}
				if (::unlinkat(aParent.Value, entry->d_name, 0) != 0) {
					closeDirectory();
					return std::unexpected(failure::unavailable);
				}
				removed = true;
				errno   = 0;
			}
			const auto scanError = errno;
			closeDirectory();
			if (scanError != 0 || (removed && ::fsync(aParent.Value) != 0)) {
				return std::unexpected(failure::unavailable);
			}
			return { };
		}

		[[nodiscard]] auto publish_bootstrap(
			const file_descriptor& aParent,
			std::string_view       aTemporaryName) -> std::expected<bool, failure>
		{
			const std::string temporary{ aTemporaryName };
			const std::string catalog{ CatalogName };
	#if defined(__APPLE__)
			const auto code = ::renameatx_np(
				aParent.Value,
				temporary.c_str(),
				aParent.Value,
				catalog.c_str(),
				RENAME_EXCL);
	#elif defined(__linux__)
			const auto code = static_cast<int>(::syscall(
				SYS_renameat2,
				aParent.Value,
				temporary.c_str(),
				aParent.Value,
				catalog.c_str(),
				RENAME_NOREPLACE));
	#else
			const auto code = -1;
			errno           = ENOTSUP;
	#endif
			if (code == 0) {
				if (::fsync(aParent.Value) != 0) {
					return std::unexpected(failure::unavailable);
				}
				return true;
			}
			const auto conflict = errno == EEXIST;
			if (::unlinkat(aParent.Value, temporary.c_str(), 0) != 0
				|| ::fsync(aParent.Value) != 0) {
				return std::unexpected(failure::unavailable);
			}
			if (!conflict) {
				return std::unexpected(failure::unavailable);
			}
			return false;
		}

		[[nodiscard]] auto validate_database_file(
			const file_descriptor& aParent,
			std::string_view       aName,
			bool                   aRequired) -> std::expected<void, failure>
		{
			const std::string ownedName{ aName };
			struct stat       status{ };
			if (::fstatat(
					aParent.Value, ownedName.c_str(), &status, AT_SYMLINK_NOFOLLOW)
				!= 0) {
				if (!aRequired && errno == ENOENT) {
					return { };
				}
				return std::unexpected(failure::invalid_path);
			}
			if (!private_file(status)) {
				return std::unexpected(failure::invalid_permissions);
			}
			return { };
		}

		[[nodiscard]] auto validate_database_paths(const file_descriptor& aParent)
			-> std::expected<void, failure>
		{
			constexpr std::array<std::string_view, 4> sidecars{
				"epoch-catalog.sqlite3-wal",
				"epoch-catalog.sqlite3-shm",
				"epoch-catalog.sqlite3-journal",
				"epoch-catalog.sqlite3-super-journal",
			};
			if (auto main = validate_database_file(aParent, CatalogName, true); !main) {
				return main;
			}
			for (const auto sidecar : sidecars) {
				if (auto result = validate_database_file(aParent, sidecar, false); !result) {
					return result;
				}
			}
			return { };
		}

		struct file_identity final
		{
			dev_t Device{ };
			ino_t Inode{ };

			auto operator==(const file_identity&) const -> bool = default;
		};

		[[nodiscard]] auto identity_of(const file_descriptor& aDescriptor)
			-> std::expected<file_identity, failure>
		{
			struct stat status{ };
			if (::fstat(aDescriptor.Value, &status) != 0) {
				return std::unexpected(failure::invalid_path);
			}
			return file_identity{ .Device = status.st_dev, .Inode = status.st_ino };
		}

		[[nodiscard]] auto identity_at(
			const file_descriptor& aParent,
			std::string_view       aName) -> std::expected<file_identity, failure>
		{
			const std::string ownedName{ aName };
			struct stat       status{ };
			if (::fstatat(
					aParent.Value,
					ownedName.c_str(),
					&status,
					AT_SYMLINK_NOFOLLOW)
				!= 0) {
				return std::unexpected(failure::invalid_path);
			}
			if (!private_file(status)) {
				return std::unexpected(failure::invalid_permissions);
			}
			return file_identity{ .Device = status.st_dev, .Inode = status.st_ino };
		}

		[[nodiscard]] auto sqlite_main_file_is_current(
			::sqlite3*             aDatabase,
			const file_descriptor& aParent,
			const file_identity    aExpected,
			std::string_view       aName = CatalogName) -> std::expected<void, failure>
		{
			auto current = identity_at(aParent, aName);
			auto moved   = 1;
			if (!current || *current != aExpected
				|| ::sqlite3_file_control(
					   aDatabase, "main", SQLITE_FCNTL_HAS_MOVED, &moved)
					!= SQLITE_OK
				|| moved != 0) {
				return std::unexpected(failure::invalid_path);
			}
			return { };
		}

		[[nodiscard]] auto create_bootstrap(
			const file_descriptor& aParent,
			std::stop_token        aStop,
			const char*            aRequestedVfs = nullptr) -> std::expected<bool, failure>
		{
			std::array<std::uint8_t, 32> nameRandom{ };
			std::array<std::uint8_t, 32> incarnation{ };
			if (!random_bytes(nameRandom) || !random_bytes(incarnation)) {
				return std::unexpected(failure::unavailable);
			}
			const auto      temporaryName = hex_name(nameRandom);
			file_descriptor temporary{ ::openat(
				aParent.Value,
				temporaryName.c_str(),
				O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
				0600) };
			if (temporary.Value < 0) {
				return std::unexpected(failure::unavailable);
			}
			auto expectedTemporary = identity_of(temporary);
			if (!expectedTemporary) {
				(void)::unlinkat(aParent.Value, temporaryName.c_str(), 0);
				return std::unexpected(expectedTemporary.error());
			}
			const auto temporaryPath = descriptor_child_path(aParent, temporaryName);
			auto       opened        = open_sqlite(temporaryPath, aRequestedVfs);
			if (!opened) {
				(void)::unlinkat(aParent.Value, temporaryName.c_str(), 0);
				return std::unexpected(opened.error());
			}
			::sqlite3* database        = *opened;
			auto       cleanupDatabase = [&database]() noexcept {
				if (database != nullptr) {
					(void)::sqlite3_close_v2(database);
					database = nullptr;
				}
			};
			auto configured = configure_connection(database);
			auto created    = configured
				? create_schema(database, incarnation, aStop)
				: std::expected<void, failure>{ std::unexpected(configured.error()) };
			if (created) {
				created = validate_schema(database, aStop);
			}
			if (created) {
				created = execute(database, "PRAGMA wal_checkpoint(TRUNCATE)", aStop);
			}
			if (created) {
				created = sqlite_main_file_is_current(
					database, aParent, *expectedTemporary, temporaryName);
			}
			cleanupDatabase();
			if (!created) {
				(void)::unlinkat(aParent.Value, temporaryName.c_str(), 0);
				return std::unexpected(created.error());
			}
			if (::fsync(temporary.Value) != 0) {
				(void)::unlinkat(aParent.Value, temporaryName.c_str(), 0);
				return std::unexpected(failure::unavailable);
			}
			auto durableTemporary = identity_at(aParent, temporaryName);
			if (!durableTemporary || *durableTemporary != *expectedTemporary) {
				(void)::unlinkat(aParent.Value, temporaryName.c_str(), 0);
				return std::unexpected(failure::invalid_path);
			}
	#if defined(BPR_SQLITE_TESTING)
			if (InterruptBootstrapBeforePublication.exchange(
					false, std::memory_order_relaxed)) {
				return std::unexpected(failure::unavailable);
			}
	#endif

			return publish_bootstrap(aParent, temporaryName);
		}
#else
		constexpr std::wstring_view CatalogNameWide         = L"epoch-catalog.sqlite3";
		constexpr std::wstring_view CatalogLockNameWide     = L"epoch-catalog.lock";
		constexpr std::size_t       MaximumDirectoryEntries = 4'096U;
		using native_storage::canonical_windows_root;
		using native_storage::component_is_reserved;
		using native_storage::ensure_private_child;
		using native_storage::file_identity;
		using native_storage::final_path_of;
		using native_storage::identity_of;
		using native_storage::local_memory;
		using native_storage::open_existing_object;
		using native_storage::open_private_directory;
		using native_storage::open_private_object;
		using native_storage::path_matches_handle;
		using native_storage::private_security;
		using native_storage::private_security_attributes;
		using native_storage::private_windows_object;
		using native_storage::protect_inherited_private_file;
		using native_storage::reliable_local_filesystem;
		using native_storage::same_windows_path;
		using native_storage::valid_windows_components;
		using native_storage::validate_ancestor_chain;
		using native_storage::validate_private_security;
		using native_storage::windows_error_failure;
		using native_storage::windows_handle;
		using native_storage::windows_object_kind;

		[[nodiscard]] auto random_bytes(std::span<std::uint8_t> output) noexcept
			-> bool
		{
			std::size_t offset = 0U;
			while (offset < output.size()) {
				const auto remaining = output.size() - offset;
				const auto chunk     = static_cast<ULONG>(std::min<std::size_t>(
					remaining, std::numeric_limits<ULONG>::max()));
				if (::BCryptGenRandom(
						nullptr,
						output.data() + offset,
						chunk,
						BCRYPT_USE_SYSTEM_PREFERRED_RNG)
					!= 0) {
					return false;
				}
				offset += static_cast<std::size_t>(chunk);
			}
			return true;
		}

		[[nodiscard]] auto hex_name(const std::array<std::uint8_t, 32>& random)
			-> std::wstring
		{
			const auto   digits = core::lower_hex(random);
			std::wstring result = L".epoch-catalog.bootstrap-";
			result.append(digits.begin(), digits.end());
			result.append(L".sqlite3");
			return result;
		}

		[[nodiscard]] auto bootstrap_artifact_name(
			const std::wstring_view name) noexcept -> bool
		{
			constexpr std::wstring_view                Prefix = L".epoch-catalog.bootstrap-";
			constexpr std::array<std::wstring_view, 4> Suffixes{
				L".sqlite3",
				L".sqlite3-wal",
				L".sqlite3-shm",
				L".sqlite3-journal",
			};
			for (const auto suffix : Suffixes) {
				if (name.size() != Prefix.size() + 64U + suffix.size()
					|| !name.starts_with(Prefix) || !name.ends_with(suffix)) {
					continue;
				}
				return std::ranges::all_of(
					name.substr(Prefix.size(), 64U), [](const wchar_t value) {
						return (value >= L'0' && value <= L'9')
							|| (value >= L'a' && value <= L'f');
					});
			}
			return false;
		}

		[[nodiscard]] auto path_missing(const std::filesystem::path& path)
			-> std::expected<bool, failure>
		{
			if (::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
				return false;
			}
			const auto error = ::GetLastError();
			if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
				return true;
			}
			return std::unexpected(windows_error_failure(error));
		}

		[[nodiscard]] auto delete_private_file(
			const std::filesystem::path& path,
			const private_security&      security,
			const bool                   protect_inherited = false,
			const file_identity*         expected_identity = nullptr)
			-> std::expected<void, failure>
		{
			auto missing = path_missing(path);
			if (!missing) {
				return std::unexpected(missing.error());
			}
			if (*missing) {
				return { };
			}
			auto opened = open_private_object(
				path,
				windows_object_kind::regular_file,
				security,
				DELETE | (protect_inherited ? WRITE_DAC : 0U),
				protect_inherited);
			if (!opened) {
				return std::unexpected(opened.error());
			}
			if (expected_identity != nullptr
				&& opened->Identity != *expected_identity) {
				return std::unexpected(failure::invalid_path);
			}
			FILE_DISPOSITION_INFO disposition{ .DeleteFile = TRUE };
			if (::SetFileInformationByHandle(
					opened->Handle.Value,
					FileDispositionInfo,
					&disposition,
					sizeof(disposition))
				== 0) {
				return std::unexpected(windows_error_failure(::GetLastError()));
			}
			opened->Handle.reset();
			missing = path_missing(path);
			if (!missing || !*missing) {
				return std::unexpected(
					missing ? failure::unavailable : missing.error());
			}
			return { };
		}

		class find_handle final
		{
		public:
			explicit find_handle(const HANDLE value) noexcept
				: Value{ value }
			{ }
			find_handle(const find_handle&)                    = delete;
			auto operator=(const find_handle&) -> find_handle& = delete;
			~find_handle()
			{
				if (Value != INVALID_HANDLE_VALUE) {
					(void)::FindClose(Value);
				}
			}
			HANDLE Value = INVALID_HANDLE_VALUE;
		};

		[[nodiscard]] auto cleanup_bootstrap_orphans(
			const private_windows_object& parent,
			const private_security&       security) -> std::expected<void, failure>
		{
			const auto       expected_parent = parent.Identity;
			WIN32_FIND_DATAW entry{ };
			const auto       search = parent.Path / L"*";
			find_handle      find{ ::FindFirstFileExW(
				search.c_str(),
				FindExInfoBasic,
				&entry,
				FindExSearchNameMatch,
				nullptr,
				0U) };
			if (find.Value == INVALID_HANDLE_VALUE) {
				const auto error = ::GetLastError();
				return error == ERROR_FILE_NOT_FOUND
					? std::expected<void, failure>{ }
					: std::expected<void, failure>{
						  std::unexpected(windows_error_failure(error))
					  };
			}
			std::size_t entries = 0U;
			for (;;) {
				if (++entries > MaximumDirectoryEntries) {
					return std::unexpected(failure::unavailable);
				}
				const auto name = std::wstring_view{ entry.cFileName };
				if (bootstrap_artifact_name(name)) {
					if (auto removed = delete_private_file(
							parent.Path / std::wstring{ name }, security, true);
						!removed) {
						return removed;
					}
				}
				if (::FindNextFileW(find.Value, &entry) == 0) {
					const auto error = ::GetLastError();
					if (error != ERROR_NO_MORE_FILES) {
						return std::unexpected(windows_error_failure(error));
					}
					break;
				}
			}
			auto live = open_private_directory(parent.Path, security);
			if (!live || live->Identity != expected_parent) {
				return std::unexpected(
					live ? failure::invalid_path : live.error());
			}
			return { };
		}

		class catalog_lock final
		{
		public:
			catalog_lock()                                       = default;
			catalog_lock(const catalog_lock&)                    = delete;
			auto operator=(const catalog_lock&) -> catalog_lock& = delete;
			catalog_lock(catalog_lock&& other) noexcept
				: Handle{ std::move(other.Handle) }
				, Locked{ std::exchange(other.Locked, false) }
			{
			}
			~catalog_lock()
			{
				if (Locked && Handle.valid()) {
					OVERLAPPED offset{ };
					(void)::UnlockFileEx(Handle.Value, 0U, 1U, 0U, &offset);
				}
			}

			[[nodiscard]] static auto acquire(
				const private_windows_object& parent,
				const private_security&       security,
				const std::stop_token         stop)
				-> std::expected<catalog_lock, failure>
			{
				const auto                  path = parent.Path / std::wstring{ CatalogLockNameWide };
				private_security_attributes attributes;
				if (!attributes.initialize(security, windows_object_kind::regular_file)) {
					return std::unexpected(failure::unavailable);
				}
				windows_handle handle{ ::CreateFileW(
					path.c_str(),
					GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					attributes.attributes(),
					CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
						| FILE_FLAG_WRITE_THROUGH,
					nullptr) };
				if (!handle.valid()) {
					const auto error = ::GetLastError();
					if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
						return std::unexpected(windows_error_failure(error));
					}
					windows_handle existing{ ::CreateFileW(
						path.c_str(),
						GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						nullptr,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
							| FILE_FLAG_WRITE_THROUGH,
						nullptr) };
					if (!existing.valid()) {
						return std::unexpected(
							windows_error_failure(::GetLastError()));
					}
					handle = std::move(existing);
				}
				auto expected_identity = identity_of(
					handle.Value, windows_object_kind::regular_file);
				if (!expected_identity) {
					return std::unexpected(expected_identity.error());
				}
				if (auto permissions = validate_private_security(
						handle.Value, security, windows_object_kind::regular_file);
					!permissions) {
					return std::unexpected(permissions.error());
				}
				if (auto matches = path_matches_handle(handle.Value, path); !matches) {
					return std::unexpected(matches.error());
				}
				for (;;) {
					if (stop.stop_requested()) {
						return std::unexpected(failure::cancelled);
					}
					OVERLAPPED offset{ };
					if (::LockFileEx(
							handle.Value,
							LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
							0U,
							1U,
							0U,
							&offset)
						!= 0) {
						break;
					}
					const auto error = ::GetLastError();
					if (error != ERROR_LOCK_VIOLATION && error != ERROR_IO_PENDING) {
						return std::unexpected(windows_error_failure(error));
					}
					std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
				}
				auto named = open_private_object(
					path, windows_object_kind::regular_file, security);
				if (!named || named->Identity != *expected_identity) {
					OVERLAPPED offset{ };
					(void)::UnlockFileEx(handle.Value, 0U, 1U, 0U, &offset);
					return std::unexpected(
						named ? failure::invalid_path : named.error());
				}
				catalog_lock result;
				result.Handle = std::move(handle);
				result.Locked = true;
				return result;
			}

		private:
			windows_handle Handle;
			bool           Locked = false;
		};

		[[nodiscard]] auto validate_database_file(
			const private_windows_object& parent,
			const std::wstring_view       name,
			const bool                    required,
			const private_security&       security,
			const bool                    protect_inherited = false) -> std::expected<void, failure>
		{
			const auto path    = parent.Path / std::wstring{ name };
			auto       missing = path_missing(path);
			if (!missing) {
				return std::unexpected(missing.error());
			}
			if (*missing) {
				return required
					? std::expected<void, failure>{
						  std::unexpected(failure::invalid_path)
					  }
					: std::expected<void, failure>{ };
			}
			auto opened = open_private_object(
				path,
				windows_object_kind::regular_file,
				security,
				protect_inherited ? WRITE_DAC : 0U,
				protect_inherited);
			if (!opened) {
				return std::unexpected(opened.error());
			}
			if (opened->Identity.Volume != parent.Identity.Volume) {
				return std::unexpected(failure::invalid_path);
			}
			return { };
		}

		[[nodiscard]] auto validate_database_paths(
			const private_windows_object& parent,
			const private_security&       security,
			const bool                    protect_inherited = false) -> std::expected<void, failure>
		{
			constexpr std::array<std::wstring_view, 4> Sidecars{
				L"epoch-catalog.sqlite3-wal",
				L"epoch-catalog.sqlite3-shm",
				L"epoch-catalog.sqlite3-journal",
				L"epoch-catalog.sqlite3-super-journal",
			};
			if (auto main = validate_database_file(
					parent, CatalogNameWide, true, security);
				!main) {
				return main;
			}
			for (const auto sidecar : Sidecars) {
				if (auto result = validate_database_file(
						parent,
						sidecar,
						false,
						security,
						protect_inherited);
					!result) {
					return result;
				}
			}
			return { };
		}

		[[nodiscard]] auto identity_at(
			const private_windows_object& parent,
			const std::wstring_view       name,
			const private_security&       security)
			-> std::expected<file_identity, failure>
		{
			auto opened = open_private_object(
				parent.Path / std::wstring{ name },
				windows_object_kind::regular_file,
				security);
			if (!opened) {
				return std::unexpected(opened.error());
			}
			if (opened->Identity.Volume != parent.Identity.Volume) {
				return std::unexpected(failure::invalid_path);
			}
			return opened->Identity;
		}

		enum class sqlite_catalog_leaf : std::uint8_t {
			main,
			wal,
			shm,
			journal,
		};

		class secure_windows_vfs final
		{
			struct guarded_file final
			{
				::sqlite3_file      Public;
				::sqlite3_file*     Base;
				secure_windows_vfs* Owner;
			};

		public:
			secure_windows_vfs(const secure_windows_vfs&) = delete;
			auto operator=(const secure_windows_vfs&)
				-> secure_windows_vfs& = delete;
			~secure_windows_vfs()
			{
				if (Registered) {
					(void)::sqlite3_vfs_unregister(&Interface);
				}
			}

			[[nodiscard]] static auto create(
				const private_windows_object& parent,
				const private_security&       security,
				const std::filesystem::path&  main_path)
				-> std::expected<std::unique_ptr<secure_windows_vfs>, failure>
			{
				if (!sqlite_runtime_ready()) {
					return std::unexpected(failure::unavailable);
				}
				auto* base = ::sqlite3_vfs_find("win32-longpath");
				if (base == nullptr || base->xOpen == nullptr
					|| base->xDelete == nullptr || base->xAccess == nullptr
					|| base->xFullPathname == nullptr) {
					return std::unexpected(failure::platform_unsupported);
				}
				auto random = std::array<std::uint8_t, 16U>{ };
				if (!random_bytes(random)) {
					return std::unexpected(failure::unavailable);
				}
				auto name = std::string{ "bpr-private-catalog-" };
				core::append_lower_hex(name, random);
				auto result = std::unique_ptr<secure_windows_vfs>{
					new secure_windows_vfs{
						base, parent, security, main_path, std::move(name) }
				};
				if (::sqlite3_vfs_register(&result->Interface, 0) != SQLITE_OK) {
					return std::unexpected(failure::unavailable);
				}
				result->Registered = true;
				return result;
			}

			[[nodiscard]] auto name() const noexcept -> const char*
			{
				return Name.c_str();
			}

			[[nodiscard]] auto recorded_failure() const noexcept
				-> std::optional<failure>
			{
				const auto value = Failure.load(std::memory_order_relaxed);
				return value < 0
					? std::optional<failure>{ }
					: std::optional<failure>{ static_cast<failure>(value) };
			}

			[[nodiscard]] auto failure_or(const failure fallback) const noexcept
				-> failure
			{
				const auto recorded = recorded_failure();
				return recorded ? *recorded : fallback;
			}

		private:
			secure_windows_vfs(
				::sqlite3_vfs*                base,
				const private_windows_object& parent,
				const private_security&       security,
				const std::filesystem::path&  main_path,
				std::string                   name)
				: Base{ base }
				, Security{ security }
				, ParentIdentity{ parent.Identity }
				, MainPath{ main_path }
				, Name{ std::move(name) }
			{
				const auto encoded = MainPath.u8string();
				MainName.assign(
					reinterpret_cast<const char*>(encoded.data()), encoded.size());
				WalName                     = MainName + "-wal";
				ShmName                     = MainName + "-shm";
				JournalName                 = MainName + "-journal";
				Interface                   = *Base;
				Interface.szOsFile          = static_cast<int>(sizeof(guarded_file));
				Interface.pNext             = nullptr;
				Interface.zName             = Name.c_str();
				Interface.pAppData          = this;
				Interface.xOpen             = vfs_open;
				Interface.xDelete           = vfs_delete;
				Interface.xAccess           = vfs_access;
				Interface.xFullPathname     = vfs_full_pathname;
				Interface.xDlOpen           = Base->xDlOpen != nullptr ? vfs_dl_open : nullptr;
				Interface.xDlError          = Base->xDlError != nullptr ? vfs_dl_error : nullptr;
				Interface.xDlSym            = Base->xDlSym != nullptr ? vfs_dl_sym : nullptr;
				Interface.xDlClose          = Base->xDlClose != nullptr ? vfs_dl_close : nullptr;
				Interface.xRandomness       = vfs_randomness;
				Interface.xSleep            = vfs_sleep;
				Interface.xCurrentTime      = vfs_current_time;
				Interface.xGetLastError     = vfs_get_last_error;
				Interface.xCurrentTimeInt64 = Base->xCurrentTimeInt64 != nullptr
					? vfs_current_time_int64
					: nullptr;
				Interface.xSetSystemCall    = Base->xSetSystemCall != nullptr
					? vfs_set_system_call
					: nullptr;
				Interface.xGetSystemCall    = Base->xGetSystemCall != nullptr
					? vfs_get_system_call
					: nullptr;
				Interface.xNextSystemCall   = Base->xNextSystemCall != nullptr
					? vfs_next_system_call
					: nullptr;
			}

			[[nodiscard]] auto path_for(const sqlite_catalog_leaf leaf) const
				-> std::filesystem::path
			{
				switch (leaf) {
				case sqlite_catalog_leaf::main:
					return MainPath;
				case sqlite_catalog_leaf::wal:
					return std::filesystem::path{ MainPath.native() + L"-wal" };
				case sqlite_catalog_leaf::shm:
					return std::filesystem::path{ MainPath.native() + L"-shm" };
				case sqlite_catalog_leaf::journal:
					return std::filesystem::path{ MainPath.native() + L"-journal" };
				}
				return { };
			}

			[[nodiscard]] static auto leaf_index(
				const sqlite_catalog_leaf leaf) noexcept -> std::size_t
			{
				return static_cast<std::size_t>(leaf);
			}

			[[nodiscard]] auto remember_identity(
				const sqlite_catalog_leaf leaf,
				const file_identity&      identity) noexcept -> bool
			{
				auto& expected = ExpectedIdentities[leaf_index(leaf)];
				if (expected && *expected != identity) {
					return false;
				}
				expected = identity;
				return true;
			}

			[[nodiscard]] auto leaf_for_name(const char* value) const noexcept
				-> std::optional<sqlite_catalog_leaf>
			{
				if (value == nullptr) {
					return std::nullopt;
				}
				const auto name = std::string_view{ value };
				if (name == MainName) {
					return sqlite_catalog_leaf::main;
				}
				if (name == WalName) {
					return sqlite_catalog_leaf::wal;
				}
				if (name == ShmName) {
					return sqlite_catalog_leaf::shm;
				}
				if (name == JournalName) {
					return sqlite_catalog_leaf::journal;
				}
				return std::nullopt;
			}

			[[nodiscard]] static auto leaf_for_open_flags(const int flags) noexcept
				-> std::optional<sqlite_catalog_leaf>
			{
				if ((flags & SQLITE_OPEN_MAIN_DB) != 0) {
					return sqlite_catalog_leaf::main;
				}
				if ((flags & SQLITE_OPEN_WAL) != 0) {
					return sqlite_catalog_leaf::wal;
				}
				if ((flags & SQLITE_OPEN_MAIN_JOURNAL) != 0) {
					return sqlite_catalog_leaf::journal;
				}
				return std::nullopt;
			}

			void record(const failure value) noexcept
			{
				auto expected = -1;
				(void)Failure.compare_exchange_strong(
					expected,
					static_cast<int>(value),
					std::memory_order_relaxed);
			}

			[[nodiscard]] static auto sqlite_failure_code(
				const failure value,
				const int     unavailable_code = SQLITE_IOERR) noexcept -> int
			{
				switch (value) {
				case failure::invalid_path:
				case failure::schema_mismatch:
				case failure::integrity_failure:
					return SQLITE_CANTOPEN;
				case failure::invalid_permissions:
					return SQLITE_PERM;
				case failure::quota_exceeded:
					return SQLITE_FULL;
				case failure::cancelled:
					return SQLITE_INTERRUPT;
				case failure::platform_unsupported:
					return SQLITE_CANTOPEN;
				case failure::unavailable:
					return unavailable_code;
				}
				return unavailable_code;
			}

			[[nodiscard]] auto open_guard(
				const sqlite_catalog_leaf leaf,
				const bool                create_if_missing)
				-> std::expected<private_windows_object, failure>
			{
				const auto path    = path_for(leaf);
				auto       missing = path_missing(path);
				if (!missing) {
					return std::unexpected(missing.error());
				}
				if (*missing && create_if_missing) {
					private_security_attributes attributes;
					if (!attributes.initialize(
							Security, windows_object_kind::regular_file)) {
						return std::unexpected(failure::unavailable);
					}
					windows_handle created{ ::CreateFileW(
						path.c_str(),
						GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						attributes.attributes(),
						CREATE_NEW,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
							| FILE_FLAG_WRITE_THROUGH,
						nullptr) };
					if (created.valid()) {
						auto identity = identity_of(
							created.Value, windows_object_kind::regular_file);
						if (!identity || identity->Volume != ParentIdentity.Volume) {
							return std::unexpected(
								identity ? failure::invalid_path : identity.error());
						}
						if (auto permissions = validate_private_security(
								created.Value,
								Security,
								windows_object_kind::regular_file);
							!permissions) {
							return std::unexpected(permissions.error());
						}
						if (auto matches = path_matches_handle(created.Value, path);
							!matches) {
							return std::unexpected(matches.error());
						}
						return private_windows_object{
							.Handle   = std::move(created),
							.Path     = path,
							.Identity = *identity,
						};
					}
					const auto error = ::GetLastError();
					if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
						return std::unexpected(windows_error_failure(error));
					}
				} else if (*missing) {
					return std::unexpected(failure::invalid_path);
				}
				auto opened = open_private_object(
					path,
					windows_object_kind::regular_file,
					Security,
					0U,
					false,
					false);
				if (!opened) {
					return std::unexpected(opened.error());
				}
				if (opened->Identity.Volume != ParentIdentity.Volume) {
					return std::unexpected(failure::invalid_path);
				}
				return opened;
			}

			[[nodiscard]] auto delete_leaf(const sqlite_catalog_leaf leaf)
				-> std::expected<void, failure>
			{
				if (leaf == sqlite_catalog_leaf::main) {
					return std::unexpected(failure::invalid_path);
				}
				auto& expected = ExpectedIdentities[leaf_index(leaf)];
				if (!expected) {
					auto current = open_guard(leaf, false);
					if (!current) {
						auto missing = path_missing(path_for(leaf));
						if (missing && *missing) {
							return { };
						}
						return std::unexpected(current.error());
					}
					expected = current->Identity;
				}
				auto removed = delete_private_file(
					path_for(leaf), Security, false, &*expected);
				if (removed) {
					expected.reset();
				}
				return removed;
			}

			[[nodiscard]] static auto file(::sqlite3_file* value) noexcept
				-> guarded_file*
			{
				return reinterpret_cast<guarded_file*>(value);
			}

			[[nodiscard]] static auto base(::sqlite3_file* value) noexcept
				-> ::sqlite3_file*
			{
				return file(value)->Base;
			}

			static auto io_close(::sqlite3_file* value) -> int
			{
				auto* guarded = file(value);
				auto  result  = SQLITE_OK;
				if (guarded->Base != nullptr) {
					if (guarded->Base->pMethods != nullptr) {
						result = guarded->Base->pMethods->xClose(guarded->Base);
					}
					::sqlite3_free(guarded->Base);
					guarded->Base = nullptr;
				}
				guarded->Public.pMethods = nullptr;
				return result;
			}

			static auto io_read(
				::sqlite3_file* value, void* output, int bytes, sqlite3_int64 offset)
				-> int
			{
				auto* inner = base(value);
				return inner->pMethods->xRead(inner, output, bytes, offset);
			}

			static auto io_write(
				::sqlite3_file* value,
				const void*     input,
				int             bytes,
				sqlite3_int64   offset) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xWrite(inner, input, bytes, offset);
			}

			static auto io_truncate(::sqlite3_file* value, sqlite3_int64 size) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xTruncate(inner, size);
			}

			static auto io_sync(::sqlite3_file* value, int flags) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xSync(inner, flags);
			}

			static auto io_file_size(::sqlite3_file* value, sqlite3_int64* size) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xFileSize(inner, size);
			}

			static auto io_lock(::sqlite3_file* value, int mode) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xLock(inner, mode);
			}

			static auto io_unlock(::sqlite3_file* value, int mode) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xUnlock(inner, mode);
			}

			static auto io_check_reserved_lock(::sqlite3_file* value, int* output)
				-> int
			{
				auto* inner = base(value);
				return inner->pMethods->xCheckReservedLock(inner, output);
			}

			static auto io_file_control(
				::sqlite3_file* value, int operation, void* argument) -> int
			{
				auto* guarded = file(value);
				if (operation == SQLITE_FCNTL_VFS_POINTER && argument != nullptr) {
					*static_cast<::sqlite3_vfs**>(argument) = &guarded->Owner->Interface;
					return SQLITE_OK;
				}
				return guarded->Base->pMethods->xFileControl(
					guarded->Base, operation, argument);
			}

			static auto io_sector_size(::sqlite3_file* value) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xSectorSize(inner);
			}

			static auto io_device_characteristics(::sqlite3_file* value) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xDeviceCharacteristics(inner);
			}

			static auto io_shm_map(
				::sqlite3_file* value,
				int             page,
				int             page_size,
				int             extend,
				void volatile** output) -> int
			{
				auto* guarded = file(value);
				auto  guard   = guarded->Owner->open_guard(
					sqlite_catalog_leaf::shm, true);
				if (!guard) {
					guarded->Owner->record(guard.error());
					return sqlite_failure_code(guard.error(), SQLITE_IOERR_SHMMAP);
				}
				if (!guarded->Owner->remember_identity(
						sqlite_catalog_leaf::shm, guard->Identity)) {
					guarded->Owner->record(failure::invalid_path);
					return SQLITE_IOERR_SHMMAP;
				}
				auto* inner = guarded->Base;
				if (inner->pMethods->iVersion < 2
					|| inner->pMethods->xShmMap == nullptr) {
					guarded->Owner->record(failure::platform_unsupported);
					return SQLITE_IOERR_SHMMAP;
				}
				const auto result = inner->pMethods->xShmMap(
					inner, page, page_size, extend, output);
				if (result != SQLITE_OK) {
					return result;
				}
				auto current = guarded->Owner->open_guard(
					sqlite_catalog_leaf::shm, false);
				if (!current || current->Identity != guard->Identity) {
					const auto problem = current
						? failure::invalid_path
						: current.error();
					guarded->Owner->record(problem);
					return sqlite_failure_code(problem, SQLITE_IOERR_SHMMAP);
				}
				return SQLITE_OK;
			}

			static auto io_shm_lock(
				::sqlite3_file* value, int offset, int count, int flags) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->xShmLock(inner, offset, count, flags);
			}

			static void io_shm_barrier(::sqlite3_file* value)
			{
				auto* inner = base(value);
				inner->pMethods->xShmBarrier(inner);
			}

			static auto io_shm_unmap(::sqlite3_file* value, int delete_file) -> int
			{
				auto*      guarded = file(value);
				auto*      inner   = guarded->Base;
				const auto result  = inner->pMethods->xShmUnmap(inner, 0);
				if (result != SQLITE_OK || delete_file == 0) {
					return result;
				}
				if (auto removed = guarded->Owner->delete_leaf(
						sqlite_catalog_leaf::shm);
					!removed) {
					guarded->Owner->record(removed.error());
					return sqlite_failure_code(
						removed.error(), SQLITE_IOERR_SHMOPEN);
				}
				return SQLITE_OK;
			}

			static auto io_fetch(
				::sqlite3_file* value,
				sqlite3_int64   offset,
				int             amount,
				void**          output) -> int
			{
				auto* inner = base(value);
				if (inner->pMethods->iVersion >= 3
					&& inner->pMethods->xFetch != nullptr) {
					return inner->pMethods->xFetch(inner, offset, amount, output);
				}
				if (output != nullptr) {
					*output = nullptr;
				}
				return SQLITE_OK;
			}

			static auto io_unfetch(
				::sqlite3_file* value, sqlite3_int64 offset, void* input) -> int
			{
				auto* inner = base(value);
				return inner->pMethods->iVersion >= 3
						&& inner->pMethods->xUnfetch != nullptr
					? inner->pMethods->xUnfetch(inner, offset, input)
					: SQLITE_OK;
			}

			inline static const ::sqlite3_io_methods GuardedMethods{
				.iVersion               = 3,
				.xClose                 = io_close,
				.xRead                  = io_read,
				.xWrite                 = io_write,
				.xTruncate              = io_truncate,
				.xSync                  = io_sync,
				.xFileSize              = io_file_size,
				.xLock                  = io_lock,
				.xUnlock                = io_unlock,
				.xCheckReservedLock     = io_check_reserved_lock,
				.xFileControl           = io_file_control,
				.xSectorSize            = io_sector_size,
				.xDeviceCharacteristics = io_device_characteristics,
				.xShmMap                = io_shm_map,
				.xShmLock               = io_shm_lock,
				.xShmBarrier            = io_shm_barrier,
				.xShmUnmap              = io_shm_unmap,
				.xFetch                 = io_fetch,
				.xUnfetch               = io_unfetch,
			};

			static auto vfs_open(
				::sqlite3_vfs*   vfs,
				sqlite3_filename name,
				::sqlite3_file*  output,
				int              flags,
				int*             output_flags) -> int
			{
				auto* owner   = static_cast<secure_windows_vfs*>(vfs->pAppData);
				auto* guarded = reinterpret_cast<guarded_file*>(output);
				std::memset(guarded, 0, sizeof(*guarded));
				guarded->Owner          = owner;
				const auto named_leaf   = owner->leaf_for_name(name);
				const auto flagged_leaf = leaf_for_open_flags(flags);
				if (!named_leaf || !flagged_leaf || *named_leaf != *flagged_leaf
					|| (flags & SQLITE_OPEN_EXCLUSIVE) != 0) {
					owner->record(failure::invalid_path);
					return SQLITE_CANTOPEN;
				}
				auto guard = owner->open_guard(
					*named_leaf, (flags & SQLITE_OPEN_CREATE) != 0);
				if (!guard) {
					owner->record(guard.error());
					return sqlite_failure_code(guard.error());
				}
				if (!owner->remember_identity(*named_leaf, guard->Identity)) {
					owner->record(failure::invalid_path);
					return SQLITE_CANTOPEN;
				}
				guarded->Base = static_cast<::sqlite3_file*>(
					::sqlite3_malloc64(static_cast<sqlite3_uint64>(
						owner->Base->szOsFile)));
				if (guarded->Base == nullptr) {
					owner->record(failure::unavailable);
					return SQLITE_NOMEM;
				}
				std::memset(
					guarded->Base,
					0,
					static_cast<std::size_t>(owner->Base->szOsFile));
				const auto result = owner->Base->xOpen(
					owner->Base, name, guarded->Base, flags, output_flags);
				if (result != SQLITE_OK || guarded->Base->pMethods == nullptr) {
					if (guarded->Base->pMethods != nullptr) {
						(void)guarded->Base->pMethods->xClose(guarded->Base);
					}
					::sqlite3_free(guarded->Base);
					guarded->Base = nullptr;
					return result != SQLITE_OK ? result : SQLITE_CANTOPEN;
				}
				HANDLE     native     = INVALID_HANDLE_VALUE;
				const auto controlled = guarded->Base->pMethods->xFileControl(
					guarded->Base, SQLITE_FCNTL_WIN32_GET_HANDLE, &native);
				auto identity    = controlled == SQLITE_OK && native != nullptr
						&& native != INVALID_HANDLE_VALUE
					? identity_of(native, windows_object_kind::regular_file)
					: std::expected<file_identity, failure>{
						  std::unexpected(failure::invalid_path)
					  };
				auto permissions = identity
					? validate_private_security(
						  native, owner->Security, windows_object_kind::regular_file)
					: std::expected<void, failure>{
						  std::unexpected(identity.error())
					  };
				auto matches     = permissions
					? path_matches_handle(native, owner->path_for(*named_leaf))
					: std::expected<void, failure>{
						  std::unexpected(permissions.error())
					  };
				if (!identity || *identity != guard->Identity || !permissions
					|| !matches) {
					const auto problem = !identity
						? identity.error()
						: (!permissions ? permissions.error()
										: (!matches ? matches.error()
													: failure::invalid_path));
					owner->record(problem);
					(void)guarded->Base->pMethods->xClose(guarded->Base);
					::sqlite3_free(guarded->Base);
					guarded->Base = nullptr;
					return sqlite_failure_code(problem);
				}
				guarded->Public.pMethods = &GuardedMethods;
				return SQLITE_OK;
			}

			static auto vfs_delete(
				::sqlite3_vfs* vfs, const char* name, int sync_directory) -> int
			{
				(void)sync_directory;
				auto*      owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				const auto leaf  = owner->leaf_for_name(name);
				if (!leaf || *leaf == sqlite_catalog_leaf::main) {
					owner->record(failure::invalid_path);
					return SQLITE_IOERR_DELETE;
				}
				auto removed = owner->delete_leaf(*leaf);
				if (!removed) {
					owner->record(removed.error());
					return sqlite_failure_code(removed.error(), SQLITE_IOERR_DELETE);
				}
				return SQLITE_OK;
			}

			static auto vfs_access(
				::sqlite3_vfs* vfs,
				const char*    name,
				int            flags,
				int*           output) -> int
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				if (output == nullptr
					|| (flags != SQLITE_ACCESS_EXISTS
						&& flags != SQLITE_ACCESS_READ
						&& flags != SQLITE_ACCESS_READWRITE)) {
					owner->record(failure::invalid_path);
					return SQLITE_IOERR_ACCESS;
				}
				const auto leaf = owner->leaf_for_name(name);
				if (!leaf) {
					owner->record(failure::invalid_path);
					return SQLITE_IOERR_ACCESS;
				}
				auto missing = path_missing(owner->path_for(*leaf));
				if (!missing) {
					owner->record(missing.error());
					return sqlite_failure_code(missing.error(), SQLITE_IOERR_ACCESS);
				}
				if (*missing) {
					*output = 0;
					return SQLITE_OK;
				}
				auto guard = owner->open_guard(*leaf, false);
				if (!guard) {
					owner->record(guard.error());
					return sqlite_failure_code(guard.error(), SQLITE_IOERR_ACCESS);
				}
				if (!owner->remember_identity(*leaf, guard->Identity)) {
					owner->record(failure::invalid_path);
					return SQLITE_IOERR_ACCESS;
				}
				*output = 1;
				return SQLITE_OK;
			}

			static auto vfs_full_pathname(
				::sqlite3_vfs* vfs,
				const char*    name,
				int            output_bytes,
				char*          output) -> int
			{
				auto*      owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				const auto leaf  = owner->leaf_for_name(name);
				if (!leaf || output == nullptr || output_bytes <= 0) {
					owner->record(failure::invalid_path);
					return SQLITE_CANTOPEN;
				}
				const auto result = owner->Base->xFullPathname(
					owner->Base, name, output_bytes, output);
				if (result != SQLITE_OK || owner->leaf_for_name(output) != leaf) {
					owner->record(failure::invalid_path);
					if (output_bytes > 0) {
						output[0] = '\0';
					}
					return result != SQLITE_OK ? result : SQLITE_CANTOPEN;
				}
				return SQLITE_OK;
			}

			static auto vfs_dl_open(::sqlite3_vfs* vfs, const char* name) -> void*
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xDlOpen(owner->Base, name);
			}

			static void vfs_dl_error(
				::sqlite3_vfs* vfs, int bytes, char* output)
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				owner->Base->xDlError(owner->Base, bytes, output);
			}

			static auto vfs_dl_sym(
				::sqlite3_vfs* vfs, void* handle, const char* name)
				-> void (*)(void)
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xDlSym(owner->Base, handle, name);
			}

			static void vfs_dl_close(::sqlite3_vfs* vfs, void* handle)
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				owner->Base->xDlClose(owner->Base, handle);
			}

			static auto vfs_randomness(
				::sqlite3_vfs* vfs, int bytes, char* output) -> int
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xRandomness(owner->Base, bytes, output);
			}

			static auto vfs_sleep(::sqlite3_vfs* vfs, int microseconds) -> int
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xSleep(owner->Base, microseconds);
			}

			static auto vfs_current_time(::sqlite3_vfs* vfs, double* output) -> int
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xCurrentTime(owner->Base, output);
			}

			static auto vfs_get_last_error(
				::sqlite3_vfs* vfs, int bytes, char* output) -> int
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xGetLastError(owner->Base, bytes, output);
			}

			static auto vfs_current_time_int64(
				::sqlite3_vfs* vfs, sqlite3_int64* output) -> int
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xCurrentTimeInt64(owner->Base, output);
			}

			static auto vfs_set_system_call(
				::sqlite3_vfs*      vfs,
				const char*         name,
				sqlite3_syscall_ptr function) -> int
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xSetSystemCall(
					owner->Base, name, function);
			}

			static auto vfs_get_system_call(
				::sqlite3_vfs* vfs, const char* name) -> sqlite3_syscall_ptr
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xGetSystemCall(owner->Base, name);
			}

			static auto vfs_next_system_call(
				::sqlite3_vfs* vfs, const char* name) -> const char*
			{
				auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
				return owner->Base->xNextSystemCall(owner->Base, name);
			}

			::sqlite3_vfs*                               Base = nullptr;
			::sqlite3_vfs                                Interface{ };
			private_security                             Security;
			file_identity                                ParentIdentity;
			std::filesystem::path                        MainPath;
			std::string                                  MainName;
			std::string                                  WalName;
			std::string                                  ShmName;
			std::string                                  JournalName;
			std::string                                  Name;
			std::array<std::optional<file_identity>, 4U> ExpectedIdentities{ };
			std::atomic_int                              Failure{ -1 };
			bool                                         Registered = false;
		};

		[[nodiscard]] auto sqlite_main_file_is_current(
			::sqlite3*                    database,
			const private_windows_object& parent,
			const file_identity&          expected,
			const private_security&       security,
			const std::wstring_view       name = CatalogNameWide)
			-> std::expected<void, failure>
		{
			auto   current       = identity_at(parent, name, security);
			HANDLE sqlite_handle = INVALID_HANDLE_VALUE;
			if (!current || *current != expected
				|| ::sqlite3_file_control(
					   database,
					   "main",
					   SQLITE_FCNTL_WIN32_GET_HANDLE,
					   &sqlite_handle)
					!= SQLITE_OK
				|| sqlite_handle == nullptr || sqlite_handle == INVALID_HANDLE_VALUE) {
				return std::unexpected(failure::invalid_path);
			}
			auto sqlite_identity = identity_of(
				sqlite_handle, windows_object_kind::regular_file);
			if (!sqlite_identity || *sqlite_identity != expected) {
				return std::unexpected(
					sqlite_identity ? failure::invalid_path : sqlite_identity.error());
			}
			if (auto permissions = validate_private_security(
					sqlite_handle, security, windows_object_kind::regular_file);
				!permissions) {
				return std::unexpected(permissions.error());
			}
			return path_matches_handle(
				sqlite_handle, parent.Path / std::wstring{ name });
		}

		[[nodiscard]] auto create_private_file(
			const std::filesystem::path& path,
			const private_security&      security)
			-> std::expected<private_windows_object, failure>
		{
			private_security_attributes attributes;
			if (!attributes.initialize(security, windows_object_kind::regular_file)) {
				return std::unexpected(failure::unavailable);
			}
			windows_handle handle{ ::CreateFileW(
				path.c_str(),
				GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				attributes.attributes(),
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
					| FILE_FLAG_WRITE_THROUGH,
				nullptr) };
			if (!handle.valid()) {
				return std::unexpected(windows_error_failure(::GetLastError()));
			}
			auto identity = identity_of(
				handle.Value, windows_object_kind::regular_file);
			if (!identity) {
				return std::unexpected(identity.error());
			}
			if (auto permissions = validate_private_security(
					handle.Value, security, windows_object_kind::regular_file);
				!permissions) {
				return std::unexpected(permissions.error());
			}
			if (auto matches = path_matches_handle(handle.Value, path); !matches) {
				return std::unexpected(matches.error());
			}
			return private_windows_object{
				.Handle   = std::move(handle),
				.Path     = path,
				.Identity = *identity,
			};
		}

		[[nodiscard]] auto bootstrap_sidecars_absent(
			const private_windows_object& parent,
			const std::wstring_view       temporary_name) -> std::expected<void, failure>
		{
			constexpr std::array<std::wstring_view, 3> Suffixes{
				L"-wal", L"-shm", L"-journal"
			};
			for (const auto suffix : Suffixes) {
				auto name = std::wstring{ temporary_name };
				name.append(suffix);
				auto missing = path_missing(parent.Path / name);
				if (!missing || !*missing) {
					return std::unexpected(
						missing ? failure::unavailable : missing.error());
				}
			}
			return { };
		}

		[[nodiscard]] auto publish_bootstrap(
			const private_windows_object& parent,
			const std::wstring_view       temporary_name,
			const file_identity&          expected,
			const private_security&       security) -> std::expected<bool, failure>
		{
			const auto source      = parent.Path / std::wstring{ temporary_name };
			const auto destination = parent.Path / std::wstring{ CatalogNameWide };
			auto       opened      = open_private_object(
				source,
				windows_object_kind::regular_file,
				security,
				GENERIC_READ | GENERIC_WRITE | DELETE,
				false,
				false);
			if (!opened) {
				return std::unexpected(opened.error());
			}
			if (opened->Identity != expected
				|| ::FlushFileBuffers(opened->Handle.Value) == 0) {
				return std::unexpected(
					opened->Identity != expected
						? failure::invalid_path
						: windows_error_failure(::GetLastError()));
			}
			// `SetFileInformationByHandle` does not honour `RootDirectory`. That
			// field belongs to the native `NtSetInformationFile`; the Win32
			// wrapper requires a fully qualified destination and a null root.
			// Measured against every access mask the parent handle could carry,
			// with and without room for a terminating null: the handle-relative
			// form fails with `ERROR_INVALID_PARAMETER` every time, and the
			// qualified form succeeds.
			//
			// The retained chain still supplies the substitution defence. Every
			// ancestor handle stays open across the rename, and the publication
			// is read back below through `identity_at(parent, ...)`, which is
			// handle-relative and compares the exact expected identity.
			const auto& native_destination = destination.native();
			const auto  name_bytes         = native_destination.size() * sizeof(wchar_t);
			const auto  structure_bytes    = offsetof(FILE_RENAME_INFO, FileName)
				+ name_bytes + sizeof(wchar_t);
			if (name_bytes > std::numeric_limits<DWORD>::max()
				|| structure_bytes > std::numeric_limits<DWORD>::max()) {
				return std::unexpected(failure::invalid_path);
			}
			auto  storage           = std::vector<std::byte>(structure_bytes);
			auto* rename            = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
			rename->ReplaceIfExists = FALSE;
			rename->RootDirectory   = nullptr;
			rename->FileNameLength  = static_cast<DWORD>(name_bytes);
			std::memcpy(rename->FileName, native_destination.data(), name_bytes);
			if (::SetFileInformationByHandle(
					opened->Handle.Value,
					FileRenameInfo,
					rename,
					static_cast<DWORD>(storage.size()))
				== 0) {
				const auto            error = ::GetLastError();
				FILE_DISPOSITION_INFO disposition{ .DeleteFile = TRUE };
				if (::SetFileInformationByHandle(
						opened->Handle.Value,
						FileDispositionInfo,
						&disposition,
						sizeof(disposition))
					== 0) {
					return std::unexpected(windows_error_failure(::GetLastError()));
				}
				return error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS
					? std::expected<bool, failure>{ false }
					: std::expected<bool, failure>{
						  std::unexpected(windows_error_failure(error))
					  };
			}
			if (auto identity = identity_of(
					opened->Handle.Value, windows_object_kind::regular_file);
				!identity || *identity != expected) {
				return std::unexpected(
					identity ? failure::invalid_path : identity.error());
			}
			if (auto matches = path_matches_handle(opened->Handle.Value, destination);
				!matches) {
				return std::unexpected(matches.error());
			}
			if (::FlushFileBuffers(opened->Handle.Value) == 0) {
				return std::unexpected(windows_error_failure(::GetLastError()));
			}
			auto published = identity_at(parent, CatalogNameWide, security);
			if (!published || *published != expected) {
				return std::unexpected(
					published ? failure::invalid_path : published.error());
			}
			return true;
		}

		[[nodiscard]] auto create_bootstrap(
			const private_windows_object& parent,
			const private_security&       security,
			const std::stop_token         stop) -> std::expected<bool, failure>
		{
			std::array<std::uint8_t, 32> name_random{ };
			std::array<std::uint8_t, 32> incarnation{ };
			if (!random_bytes(name_random) || !random_bytes(incarnation)) {
				return std::unexpected(failure::unavailable);
			}
			const auto temporary_name = hex_name(name_random);
			auto       temporary      = create_private_file(
				parent.Path / temporary_name, security);
			if (!temporary) {
				return std::unexpected(temporary.error());
			}
			const auto expected_temporary = temporary->Identity;
			auto       private_vfs        = secure_windows_vfs::create(
				parent, security, temporary->Path);
			if (!private_vfs) {
				temporary->Handle.reset();
				(void)cleanup_bootstrap_orphans(parent, security);
				return std::unexpected(private_vfs.error());
			}
			auto opened = open_sqlite(temporary->Path, (*private_vfs)->name());
			if (!opened) {
				temporary->Handle.reset();
				(void)cleanup_bootstrap_orphans(parent, security);
				return std::unexpected((*private_vfs)->failure_or(opened.error()));
			}
			::sqlite3* database         = *opened;
			auto       cleanup_database = [&database]() noexcept {
				if (database != nullptr) {
					(void)::sqlite3_close_v2(database);
					database = nullptr;
				}
			};
			auto created = sqlite_main_file_is_current(
				database,
				parent,
				expected_temporary,
				security,
				temporary_name);
			if (created) {
				created = configure_connection(database);
			}
			if (created) {
				created = create_schema(database, incarnation, stop);
			}
			if (created) {
				created = validate_schema(database, stop);
			}
			if (created) {
				created = execute(database, "PRAGMA wal_checkpoint(TRUNCATE)", stop);
			}
			if (created) {
				created = sqlite_main_file_is_current(
					database,
					parent,
					expected_temporary,
					security,
					temporary_name);
			}
			cleanup_database();
			if (const auto vfs_failure = (*private_vfs)->recorded_failure();
				created && vfs_failure) {
				created = std::unexpected(*vfs_failure);
			}
			if (created) {
				created = bootstrap_sidecars_absent(parent, temporary_name);
			}
			if (created && ::FlushFileBuffers(temporary->Handle.Value) == 0) {
				created = std::unexpected(failure::unavailable);
			}
			if (created) {
				auto durable = identity_at(parent, temporary_name, security);
				if (!durable || *durable != expected_temporary) {
					created = std::unexpected(
						durable ? failure::invalid_path : durable.error());
				}
			}
			if (!created) {
				temporary->Handle.reset();
				if (auto removed = cleanup_bootstrap_orphans(parent, security); !removed) {
					return std::unexpected(removed.error());
				}
				return std::unexpected(created.error());
			}
	#if defined(BPR_SQLITE_TESTING)
			if (InterruptBootstrapBeforePublication.exchange(
					false, std::memory_order_relaxed)) {
				return std::unexpected(failure::unavailable);
			}
	#endif
			temporary->Handle.reset();
			return publish_bootstrap(
				parent,
				temporary_name,
				expected_temporary,
				security);
		}
#endif
	} // namespace

	// The one bootstrap for `<root>/state/v1`. Both durable stores in this
	// adapter call it, so neither can establish the directory on terms the
	// other would have refused.
	//
	// It is defined here, beside the primitives it composes, rather than moved
	// into a translation unit of its own. Those primitives are not separable:
	// the handle types, the ownership and mode checks, the ancestor validation,
	// and the security descriptor are each used more by the catalog's lock,
	// files, and VFS than by this bootstrap, so relocating them would produce a
	// component whose name did not describe its contents. What the ledger
	// needed was one bootstrap rather than two, and calling one defined here
	// imposes no ordering on it: this is a function, not a composed adapter.
	struct state_directory::opened final
	{
#if defined(_WIN32)
		private_security                    Security;
		std::vector<private_windows_object> Directories;
#else
		file_descriptor Root;
		file_descriptor State;
		file_descriptor Version;
#endif
		std::filesystem::path Path;
	};

	state_directory::state_directory(std::unique_ptr<opened> aOpened) noexcept
		: Opened{ std::move(aOpened) }
	{
	}

	state_directory::state_directory(state_directory&&) noexcept = default;

	auto state_directory::operator=(state_directory&&) noexcept
		-> state_directory& = default;

	state_directory::~state_directory() = default;

	auto state_directory::path() const noexcept -> const std::filesystem::path&
	{
		return Opened->Path;
	}

	auto state_directory::handles() noexcept -> opened&
	{
		return *Opened;
	}

	namespace {
		[[nodiscard]] constexpr auto as_state_directory_failure(
			const failure aFailure) noexcept -> state_directory_failure
		{
			switch (aFailure) {
			case failure::invalid_path:
				return state_directory_failure::invalid_path;
			case failure::invalid_permissions:
				return state_directory_failure::invalid_permissions;
			case failure::platform_unsupported:
				return state_directory_failure::platform_unsupported;
			case failure::integrity_failure:
			case failure::schema_mismatch:
			case failure::cancelled:
			case failure::quota_exceeded:
			case failure::unavailable:
				break;
			}
			return state_directory_failure::unavailable;
		}

		[[nodiscard]] constexpr auto as_catalog_failure(
			const state_directory_failure aFailure) noexcept -> failure
		{
			switch (aFailure) {
			case state_directory_failure::invalid_path:
				return failure::invalid_path;
			case state_directory_failure::invalid_permissions:
				return failure::invalid_permissions;
			case state_directory_failure::platform_unsupported:
				return failure::platform_unsupported;
			case state_directory_failure::unavailable:
				break;
			}
			return failure::unavailable;
		}
	} // namespace

	auto validate_private_storage_root(const std::filesystem::path& aRoot)
		-> std::expected<void, state_directory_failure>
	{
		if (aRoot.empty() || !aRoot.is_absolute() || aRoot != aRoot.lexically_normal()) {
			return std::unexpected(state_directory_failure::invalid_path);
		}
#if defined(_WIN32)
		auto canonicalRoot = canonical_windows_root(aRoot);
		if (!canonicalRoot) {
			return std::unexpected(as_state_directory_failure(canonicalRoot.error()));
		}
		auto ancestors = validate_ancestor_chain(*canonicalRoot);
		if (!ancestors) {
			return std::unexpected(as_state_directory_failure(ancestors.error()));
		}
		auto security = private_security::load();
		if (!security) {
			return std::unexpected(as_state_directory_failure(security.error()));
		}
		auto root = open_private_directory(*canonicalRoot, *security, false);
#else
		std::error_code error;
		const auto      canonicalRoot = std::filesystem::canonical(aRoot, error);
		if (error || canonicalRoot != aRoot.lexically_normal()) {
			return std::unexpected(state_directory_failure::invalid_path);
		}
		auto root = open_private_directory(aRoot);
#endif
		if (!root) {
			return std::unexpected(as_state_directory_failure(root.error()));
		}
		if (!reliable_local_filesystem(*root)) {
			return std::unexpected(state_directory_failure::platform_unsupported);
		}
		return { };
	}

#if defined(_WIN32)
	class activation_native_lease final : public application::profile_runtime_lease
	{
	public:
		std::vector<private_windows_object> Ancestors;
		private_windows_object              Root;
		windows_handle                      Lock;
		DWORD                               Process = ::GetCurrentProcessId();
	};
#else
	class activation_native_lease final : public application::profile_runtime_lease
	{
	public:
		file_descriptor Root;
		pid_t           Process = ::getpid();
	};
#endif
	auto validates_profile_runtime_lease(const std::filesystem::path& aRoot, const std::shared_ptr<application::profile_runtime_lease>& aLease) -> bool
	{
#if defined(_WIN32)
		const auto lease     = std::dynamic_pointer_cast<activation_native_lease>(aLease);
		const auto canonical = canonical_windows_root(aRoot);
		return lease && canonical && lease->Process == ::GetCurrentProcessId() && lease->Lock.valid()
			&& validate_private_storage_root(aRoot) && lease->Root.Path == *canonical
			&& identity_of(lease->Root.Handle.Value, windows_object_kind::directory) == lease->Root.Identity
			&& path_matches_handle(lease->Root.Handle.Value, *canonical);
#else
		const auto  lease = std::dynamic_pointer_cast<activation_native_lease>(aLease);
		struct stat held{ };
		struct stat named{ };
		return lease && lease->Process == ::getpid() && validate_private_storage_root(aRoot)
			&& ::fstat(lease->Root.Value, &held) == 0 && ::lstat(aRoot.c_str(), &named) == 0
			&& held.st_dev == named.st_dev && held.st_ino == named.st_ino;
#endif
	}
	auto acquire_native_profile_runtime_lease(const std::filesystem::path& aRoot, const std::stop_token aStop)
		-> std::expected<std::shared_ptr<application::profile_runtime_lease>, application::profile_runtime_lease_failure>
	{
		using lease_failure = application::profile_runtime_lease_failure;
		if (aStop.stop_requested()) {
			return std::unexpected(lease_failure::cancelled);
		}
		if (!validate_private_storage_root(aRoot)) {
			return std::unexpected(lease_failure::unavailable);
		}
#if defined(_WIN32)
		auto result    = std::make_shared<activation_native_lease>();
		auto canonical = canonical_windows_root(aRoot);
		auto security  = private_security::load();
		if (!canonical || !security) {
			return std::unexpected(lease_failure::unavailable);
		}
		auto ancestors = validate_ancestor_chain(*canonical);
		auto root      = open_private_directory(*canonical, *security, false);
		if (!ancestors || !root || !reliable_local_filesystem(*root)) {
			return std::unexpected(lease_failure::unavailable);
		}
		result->Ancestors                = std::move(*ancestors);
		result->Root                     = std::move(*root);
		const auto                  path = result->Root.Path / L"profile-runtime.lock";
		private_security_attributes attributes;
		if (!attributes.initialize(*security, windows_object_kind::regular_file)) {
			return std::unexpected(lease_failure::unavailable);
		}
		// Zero sharing is the kernel ownership primitive and prevents replacement
		// of this file until the last non-inheritable handle closes.
		result->Lock = windows_handle{ ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
													 0U, attributes.attributes(), CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
		if (!result->Lock.valid()) {
			auto error = ::GetLastError();
			if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
				result->Lock = windows_handle{ ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
															 0U, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
				error        = result->Lock.valid() ? ERROR_SUCCESS : ::GetLastError();
			}
			if (!result->Lock.valid()) {
				return std::unexpected(error == ERROR_SHARING_VIOLATION ? lease_failure::busy : lease_failure::unavailable);
			}
		}
		if (!identity_of(result->Lock.Value, windows_object_kind::regular_file)
			|| !validate_private_security(result->Lock.Value, *security, windows_object_kind::regular_file)
			|| !path_matches_handle(result->Lock.Value, path)
			|| !::SetHandleInformation(result->Lock.Value, HANDLE_FLAG_INHERIT, 0U)) {
			return std::unexpected(lease_failure::unavailable);
		}
#else
		auto result = std::make_shared<activation_native_lease>();
		auto root   = open_private_directory(aRoot);
		if (!root || !reliable_local_filesystem(*root)) {
			return std::unexpected(lease_failure::unavailable);
		}
		result->Root = std::move(*root);
		// Lock the directory inode itself: no removable lock-file name can split
		// ownership, and independent opens conflict within this process too.
		if (::flock(result->Root.Value, LOCK_EX | LOCK_NB) != 0) {
			return std::unexpected(errno == EWOULDBLOCK || errno == EAGAIN ? lease_failure::busy : lease_failure::unavailable);
		}
		struct stat held{ };
		struct stat named{ };
		if (!validate_private_storage_root(aRoot) || ::fstat(result->Root.Value, &held) != 0
			|| ::lstat(aRoot.c_str(), &named) != 0 || held.st_dev != named.st_dev || held.st_ino != named.st_ino) {
			return std::unexpected(lease_failure::unavailable);
		}
#endif
		if (aStop.stop_requested()) {
			return std::unexpected(lease_failure::cancelled);
		}
		return result;
	}

	auto ensure_private_storage_child(const std::filesystem::path& aParent, const std::string_view aName)
		-> std::expected<std::filesystem::path, state_directory_failure>
	{
		if (aName.empty() || aName.size() > 64U || !std::ranges::all_of(aName, [](const char aCharacter) {
				return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-';
			})) {
			return std::unexpected(state_directory_failure::invalid_path);
		}
		if (auto valid = validate_private_storage_root(aParent); !valid) {
			return std::unexpected(valid.error());
		}
#if defined(_WIN32)
		auto canonicalParent = canonical_windows_root(aParent);
		if (!canonicalParent) {
			return std::unexpected(as_state_directory_failure(canonicalParent.error()));
		}
		auto security = private_security::load();
		if (!security) {
			return std::unexpected(as_state_directory_failure(security.error()));
		}
		auto parent = open_private_directory(*canonicalParent, *security, false);
		if (!parent) {
			return std::unexpected(as_state_directory_failure(parent.error()));
		}
		const auto name  = std::wstring{ aName.begin(), aName.end() };
		auto       child = ensure_private_child(*parent, name, *security);
#else
		auto parent = open_private_directory(aParent);
		if (!parent) {
			return std::unexpected(as_state_directory_failure(parent.error()));
		}
		auto child = ensure_private_child(*parent, aName);
#endif
		if (!child) {
			return std::unexpected(as_state_directory_failure(child.error()));
		}
		return aParent / std::string{ aName };
	}

	auto open_state_directory(const std::filesystem::path& aRoot)
		-> std::expected<state_directory, state_directory_failure>
	{
#if defined(_WIN32)
		try {
			auto canonicalRoot = canonical_windows_root(aRoot);
			if (!canonicalRoot) {
				return std::unexpected(
					as_state_directory_failure(canonicalRoot.error()));
			}
			auto directories = validate_ancestor_chain(*canonicalRoot);
			if (!directories) {
				return std::unexpected(
					as_state_directory_failure(directories.error()));
			}
			auto security = private_security::load();
			if (!security) {
				return std::unexpected(
					as_state_directory_failure(security.error()));
			}
			if (directories->empty()) {
				return std::unexpected(state_directory_failure::invalid_path);
			}
			directories->reserve(directories->size() + 2U);
			auto& root = directories->back();
			if (auto permissions = validate_private_security(
					root.Handle.Value,
					*security,
					windows_object_kind::directory);
				!permissions) {
				return std::unexpected(
					as_state_directory_failure(permissions.error()));
			}
			if (!reliable_local_filesystem(root)) {
				return std::unexpected(
					state_directory_failure::platform_unsupported);
			}
			// The ancestor chain's handles carry only attributes and control,
			// which impose no sharing restrictions — right for `C:\Users`,
			// wrong for the root this catalog must keep deletable by no one:
			// only a handle holding data access enforces its share mode, so
			// the root is re-held the way its children are (BPR-0425).
			if (auto retainedRoot
				= open_private_directory(root.Path, *security, false);
				retainedRoot) {
				root = std::move(*retainedRoot);
			} else {
				return std::unexpected(
					as_state_directory_failure(retainedRoot.error()));
			}
			auto state = ensure_private_child(root, L"state", *security);
			if (!state) {
				return std::unexpected(
					as_state_directory_failure(state.error()));
			}
			directories->push_back(std::move(*state));
			auto& retainedState = directories->back();
			auto  version       = ensure_private_child(retainedState, L"v1", *security);
			if (!version) {
				return std::unexpected(
					as_state_directory_failure(version.error()));
			}
			directories->push_back(std::move(*version));

			auto opened         = std::make_unique<state_directory::opened>();
			opened->Path        = directories->back().Path;
			opened->Security    = std::move(*security);
			opened->Directories = std::move(*directories);
			return state_directory{ std::move(opened) };
		} catch (const std::bad_alloc&) {
			return std::unexpected(state_directory_failure::unavailable);
		} catch (const std::filesystem::filesystem_error&) {
			return std::unexpected(state_directory_failure::invalid_path);
		} catch (...) {
			return std::unexpected(state_directory_failure::unavailable);
		}
#else
		if (aRoot.empty() || !aRoot.is_absolute()) {
			return std::unexpected(state_directory_failure::invalid_path);
		}
		std::error_code canonicalError;
		const auto      canonicalRoot
			= std::filesystem::canonical(aRoot, canonicalError);
		if (canonicalError || canonicalRoot != aRoot.lexically_normal()) {
			return std::unexpected(state_directory_failure::invalid_path);
		}
		auto root = open_private_directory(aRoot);
		if (!root) {
			return std::unexpected(as_state_directory_failure(root.error()));
		}
		if (!reliable_local_filesystem(*root)) {
			return std::unexpected(
				state_directory_failure::platform_unsupported);
		}
		auto state = ensure_private_child(*root, "state");
		if (!state) {
			return std::unexpected(as_state_directory_failure(state.error()));
		}
		auto version = ensure_private_child(*state, "v1");
		if (!version) {
			return std::unexpected(as_state_directory_failure(version.error()));
		}

		auto opened     = std::make_unique<state_directory::opened>();
		opened->Root    = std::move(*root);
		opened->State   = std::move(*state);
		opened->Version = std::move(*version);
		opened->Path    = aRoot / "state" / "v1";
		return state_directory{ std::move(opened) };
#endif
	}
	auto admit_private_store_file(state_directory& aDirectory, const std::string_view aName)
		-> std::expected<void, state_directory_failure>
	{
		if (aName.empty() || aName.find_first_of("/\\:") != std::string_view::npos || aName == "." || aName == "..") {
			return std::unexpected(state_directory_failure::invalid_path);
		}
#if defined(_WIN32)
		try {
			auto&      opened  = aDirectory.handles();
			const auto path    = opened.Path / std::filesystem::path{ std::wstring{ aName.begin(), aName.end() } };
			const auto missing = path_missing(path);
			if (!missing) {
				return std::unexpected(as_state_directory_failure(missing.error()));
			}
			if (*missing) {
				return { };
			}
			// [[BPR-0806]]. The same private-object rules the catalog applies:
			// a regular file with one link and no reparse point, owned by this
			// user, whose DACL grants only this user (and the AppContainer when
			// contained). A file sqlite created under state/v1 carries that one
			// grant inherited; it is accepted and made explicit. Anything wider
			// is refused rather than repaired.
			auto file = open_private_object(
				path, windows_object_kind::regular_file, opened.Security, WRITE_DAC, true);
			if (!file) {
				return std::unexpected(as_state_directory_failure(file.error()));
			}
			return { };
		} catch (const std::bad_alloc&) {
			return std::unexpected(state_directory_failure::unavailable);
		}
#else
		std::error_code error;
		const auto      status = std::filesystem::symlink_status(aDirectory.path() / std::string{ aName }, error);
		if (std::filesystem::exists(status)
			&& (!std::filesystem::is_regular_file(status)
				|| (status.permissions() & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) != std::filesystem::perms::none)) {
			return std::unexpected(state_directory_failure::invalid_permissions);
		}
		return { };
#endif
	}

	auto restrict_private_store_file(state_directory& aDirectory, const std::string_view aName)
		-> std::expected<void, state_directory_failure>
	{
		if (aName.empty() || aName.find_first_of("/\\:") != std::string_view::npos || aName == "." || aName == "..") {
			return std::unexpected(state_directory_failure::invalid_path);
		}
#if defined(_WIN32)
		try {
			auto&      opened = aDirectory.handles();
			const auto path   = opened.Path / std::filesystem::path{ std::wstring{ aName.begin(), aName.end() } };
			auto       file   = open_private_object(
				path, windows_object_kind::regular_file, opened.Security, WRITE_DAC, true);
			if (!file) {
				return std::unexpected(as_state_directory_failure(file.error()));
			}
			return { };
		} catch (const std::bad_alloc&) {
			return std::unexpected(state_directory_failure::unavailable);
		}
#else
		std::error_code error;
		std::filesystem::permissions(
			aDirectory.path() / std::string{ aName },
			std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace,
			error);
		if (error) {
			return std::unexpected(state_directory_failure::unavailable);
		}
		return { };
#endif
	}

	auto open_private_store_database(state_directory& aDirectory, const std::string_view aName, const int aFlags)
		-> std::expected<::sqlite3*, state_directory_failure>
	{
		if (auto admitted = admit_private_store_file(aDirectory, aName); !admitted) {
			return std::unexpected(admitted.error());
		}
		try {
			const auto path = aDirectory.path() / std::filesystem::path{ std::u8string{ aName.begin(), aName.end() } };
#if defined(BPR_SQLITE_TESTING)
			if (const auto checkpoint = PrivateStoreOpenCheckpoint.load(std::memory_order_acquire); checkpoint != nullptr) {
				checkpoint(testing::private_store_open_stage::admitted, path);
			}
#endif
			// UTF-8 on every platform: SQLite's Windows VFS converts it to UTF-16,
			// while a narrow `path.string()` is the ANSI code page there and cannot
			// name a root outside it.
			const auto encoded = path.u8string();
			const auto native  = std::string{ reinterpret_cast<const char*>(encoded.data()), encoded.size() };
			// [[BPR-0841]]. The connection is owned from the moment SQLite hands
			// it back, so every failure path closes it, an exception included; on
			// Windows an unclosed connection keeps the store file held.
			struct connection_owner final
			{
				::sqlite3* Value                                     = nullptr;
				connection_owner()                                   = default;
				connection_owner(const connection_owner&)            = delete;
				connection_owner& operator=(const connection_owner&) = delete;
				~connection_owner()
				{
					if (Value != nullptr) {
						(void)::sqlite3_close_v2(Value);
					}
				}
			};
			auto       owned    = connection_owner{ };
			auto&      database = owned.Value;
			const auto close    = [&database] {
				if (database != nullptr) {
					(void)::sqlite3_close_v2(database);
					database = nullptr;
				}
			};
#if defined(_WIN32)
			const auto* vfs = "win32-longpath";
#else
			const auto* vfs = static_cast<const char*>(nullptr);
#endif
			if (::sqlite3_open_v2(native.c_str(), &database, aFlags, vfs) != SQLITE_OK || database == nullptr) {
				close();
				return std::unexpected(state_directory_failure::unavailable);
			}
#if defined(BPR_SQLITE_TESTING)
			if (const auto checkpoint = PrivateStoreOpenCheckpoint.load(std::memory_order_acquire); checkpoint != nullptr) {
				checkpoint(testing::private_store_open_stage::opened, path);
			}
#endif
			if (auto restricted = restrict_private_store_file(aDirectory, aName); !restricted) {
				close();
				return std::unexpected(restricted.error());
			}
#if defined(_WIN32)
			// SQLite's default VFS opens by path and follows a reparse point
			// (SQLITE_OPEN_NOFOLLOW has no effect there), so admission and the
			// path-based restriction above describe the name, not necessarily the
			// file SQLite holds. Bind the held handle to the name.
			auto&      opened = aDirectory.handles();
			HANDLE     held   = INVALID_HANDLE_VALUE;
			const auto name   = opened.Path / std::filesystem::path{ std::wstring{ aName.begin(), aName.end() } };
			if (::sqlite3_file_control(database, "main", SQLITE_FCNTL_WIN32_GET_HANDLE, &held) != SQLITE_OK
				|| held == nullptr || held == INVALID_HANDLE_VALUE) {
				close();
				return std::unexpected(state_directory_failure::invalid_path);
			}
			const auto heldIdentity = identity_of(held, windows_object_kind::regular_file);
			const auto heldPrivate  = heldIdentity
				? validate_private_security(held, opened.Security, windows_object_kind::regular_file)
				: std::expected<void, failure>{ std::unexpected(heldIdentity.error()) };
			const auto heldNamed    = heldPrivate
				? path_matches_handle(held, name)
				: std::expected<void, failure>{ std::unexpected(heldPrivate.error()) };
			const auto current      = heldNamed
				? open_private_object(name, windows_object_kind::regular_file, opened.Security)
				: std::expected<private_windows_object, failure>{ std::unexpected(heldNamed.error()) };
			if (!current || current->Identity != *heldIdentity) {
				close();
				return std::unexpected(current ? state_directory_failure::invalid_path : as_state_directory_failure(current.error()));
			}
#endif
			return std::exchange(database, nullptr);
		} catch (const std::bad_alloc&) {
			return std::unexpected(state_directory_failure::unavailable);
		} catch (const std::system_error&) {
			// [[BPR-0841]]. `path::u8string()` throws for a Windows name that is
			// not valid UTF-16, such as a lone surrogate. Such a root cannot be
			// named to SQLite, which is an ordinary refusal, not a crash.
			return std::unexpected(state_directory_failure::invalid_path);
		}
	}

	auto private_store_database_failure(const state_directory_failure aFailure) noexcept -> failure
	{
		return as_catalog_failure(aFailure);
	}
} // namespace bpr::adapters::sqlite::detail

namespace bpr::adapters::sqlite {
	auto make_storage_root_owner_private(const std::filesystem::path& aRoot) -> bool
	{
#if defined(_WIN32)
		using namespace detail;
		try {
			// Callers arrive with whatever spelling their source used - Qt's
			// forward slashes included - and the root rules below read the
			// native spelling only. The directory is the same either way.
			auto canonicalRoot = canonical_windows_root(aRoot.lexically_normal());
			if (!canonicalRoot) {
				return false;
			}
			auto security = private_security::load();
			if (!security) {
				return false;
			}
			// The same descriptor the adapter writes on every directory it
			// creates, applied to the one it is handed. Opened without delete
			// sharing and without following a reparse point, exactly as the
			// later validation opens it, so what is narrowed is what will be
			// checked. WRITE_DAC is what an owner holds implicitly; ownership
			// itself is verified, never taken, so a root that belongs to
			// another principal is refused here as the store would refuse it.
			auto handle = open_existing_object(
				*canonicalRoot, windows_object_kind::directory, WRITE_DAC, false);
			if (!handle) {
				return false;
			}
			FILE_ATTRIBUTE_TAG_INFO tag{ };
			if (::GetFileInformationByHandleEx(
					handle->Value, FileAttributeTagInfo, &tag, sizeof(tag))
					== 0
				|| (tag.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U
				|| (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
				return false;
			}
			PSID                 owner      = nullptr;
			PSECURITY_DESCRIPTOR descriptor = nullptr;
			if (::GetSecurityInfo(
					handle->Value,
					SE_FILE_OBJECT,
					OWNER_SECURITY_INFORMATION,
					&owner,
					nullptr,
					nullptr,
					nullptr,
					&descriptor)
				!= ERROR_SUCCESS) {
				return false;
			}
			const auto owned = local_memory{ descriptor };
			if (owner == nullptr || ::EqualSid(owner, security->sid()) == 0) {
				return false;
			}
			private_security_attributes attributes;
			if (!attributes.initialize(*security, windows_object_kind::directory)) {
				return false;
			}
			if (::SetSecurityInfo(
					handle->Value,
					SE_FILE_OBJECT,
					DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
					nullptr,
					nullptr,
					attributes.acl(),
					nullptr)
				!= ERROR_SUCCESS) {
				return false;
			}
			return validate_private_security(
					   handle->Value, *security, windows_object_kind::directory)
				.has_value();
		} catch (...) {
			return false;
		}
#else
		std::error_code error;
		if (!std::filesystem::is_directory(aRoot, error) || error) {
			return false;
		}
		std::filesystem::permissions(
			aRoot,
			std::filesystem::perms::owner_all,
			std::filesystem::perm_options::replace,
			error);
		if (error) {
			return false;
		}
		const auto status = std::filesystem::symlink_status(aRoot, error);
		return !error && std::filesystem::is_directory(status)
			&& status.permissions() == std::filesystem::perms::owner_all;
#endif
	}

	namespace {
		/// The components of a storage root that do not exist yet, deepest
		/// first, and the deepest ancestor that does.
		struct storage_root_chain final
		{
			std::filesystem::path              Existing;
			std::vector<std::filesystem::path> Missing;
		};

		[[nodiscard]] auto find_storage_root_chain(const std::filesystem::path& aRoot)
			-> std::optional<storage_root_chain>
		{
			auto            chain   = storage_root_chain{ };
			auto            current = aRoot;
			std::error_code error;
			while (true) {
				const auto status = std::filesystem::symlink_status(current, error);
				if (status.type() != std::filesystem::file_type::not_found) {
					if (error) {
						return std::nullopt;
					}
					chain.Existing = std::move(current);
					return chain;
				}
				auto parent = current.parent_path();
				if (parent.empty() || parent == current) {
					return std::nullopt;
				}
				chain.Missing.push_back(std::move(current));
				current = std::move(parent);
			}
		}

		// The same test the application applied before it created anything: no
		// link, and no spelling that resolves somewhere else.
		[[nodiscard]] auto real_canonical_directory(const std::filesystem::path& aPath) -> bool
		{
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			if (error || status.type() != std::filesystem::file_type::directory) {
				return false;
			}
#if defined(_WIN32)
			// [[BPR-0833]]. `std::filesystem::canonical` asks for the DOS form of
			// the final path, which an AppContainer is denied. The adapter's own
			// rule asks the same questions without it: not a reparse point, and a
			// spelling that is already its own full path. The created root is then
			// validated with its whole ancestor chain.
			const auto attributes = ::GetFileAttributesW(aPath.c_str());
			return attributes != INVALID_FILE_ATTRIBUTES
				&& (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U
				&& detail::canonical_windows_root(aPath).has_value();
#else
			const auto canonical = std::filesystem::canonical(aPath, error);
			return !error && canonical == aPath;
#endif
		}

#if !defined(_WIN32)
		struct owner_private_component final
		{
			detail::file_descriptor Directory;
			bool                    Created = false;
		};

		// Created relative to the held parent and opened without following a
		// link, so the directory whose mode is set is the one just made. The
		// mode is set explicitly because `mkdirat` only ever narrows by the
		// umask. A directory someone else made first is accepted as it is.
		[[nodiscard]] auto create_owner_private_component(
			const detail::file_descriptor& aParent, const std::string& aName)
			-> std::optional<owner_private_component>
		{
			if (aName.empty() || aName == "." || aName == "..") {
				return std::nullopt;
			}
			const auto created = ::mkdirat(aParent.Value, aName.c_str(), S_IRWXU) == 0;
			if (!created && errno != EEXIST) {
				return std::nullopt;
			}
			auto result = owner_private_component{
				.Directory = detail::file_descriptor{ ::openat(
					aParent.Value, aName.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW) },
				.Created   = created,
			};
			struct stat status{ };
			if (result.Directory.Value < 0 || ::fstat(result.Directory.Value, &status) != 0
				|| !S_ISDIR(status.st_mode)) {
				return std::nullopt;
			}
			if (!created) {
				return result;
			}
			if (status.st_uid != ::geteuid() || ::fchmod(result.Directory.Value, S_IRWXU) != 0
				|| ::fstat(result.Directory.Value, &status) != 0 || !detail::private_directory(status)) {
				return std::nullopt;
			}
			return result;
		}

		// Every component this call creates, the root included, is narrowed
		// through its own descriptor here, never through its path.
		[[nodiscard]] auto create_owner_private_chain(const storage_root_chain& aChain) -> bool
		{
			auto parent = detail::file_descriptor{ ::open(
				aChain.Existing.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW) };
			if (parent.Value < 0) {
				return false;
			}
			for (const auto& component : aChain.Missing | std::views::reverse) {
				auto child = create_owner_private_component(parent, component.filename().string());
				if (!child) {
					return false;
				}
				parent = std::move(child->Directory);
			}
			return true;
		}
#endif
	} // namespace

	auto create_storage_root_owner_private(const std::filesystem::path& aRoot) -> bool
	{
		try {
			if (aRoot.empty() || !aRoot.is_absolute() || aRoot != aRoot.lexically_normal()) {
				return false;
			}
			const auto chain = find_storage_root_chain(aRoot);
			if (!chain) {
				return false;
			}
			// An existing root is never repaired here; it is accepted only when
			// the stores would already admit it.
			if (chain->Missing.empty()) {
				return detail::validate_private_storage_root(aRoot).has_value();
			}
			if (!real_canonical_directory(chain->Existing)) {
				return false;
			}
#if defined(_WIN32)
			// No umask: the folders above the root inherit, as before. The root
			// itself is created with the adapter's private security attributes,
			// the container SID included when contained ([[BPR-0833]]). Narrowing
			// afterwards needs WRITE_DAC, which an AppContainer does not hold on a
			// directory that inherited its parent's list, so the descriptor is
			// written at creation instead. A root someone else created first is
			// accepted only if it validates, exactly like a pre-existing root.
			const auto security = detail::private_security::load();
			if (!security) {
				return false;
			}
			std::error_code error;
			const auto      parent = aRoot.parent_path();
			if (!std::filesystem::exists(parent, error)
				&& (!std::filesystem::create_directories(parent, error) || error)) {
				return false;
			}
			detail::private_security_attributes attributes;
			if (!attributes.initialize(*security, detail::windows_object_kind::directory)) {
				return false;
			}
			if (::CreateDirectoryW(aRoot.c_str(), attributes.attributes()) == 0
				&& ::GetLastError() != ERROR_ALREADY_EXISTS) {
				return false;
			}
			return detail::validate_private_storage_root(aRoot).has_value();
#else
			// A trailing separator names no folder to create, and the stores
			// refuse that spelling of a root anyway.
			if (!aRoot.has_filename()) {
				return false;
			}
			// The chain already set a root it created to 0700 through its
			// descriptor. A path-based narrowing here could follow a link
			// swapped in for the root and change a directory this call did not
			// create, so a created root is validated exactly as a pre-existing
			// or concurrently created one is.
			return create_owner_private_chain(*chain) && detail::validate_private_storage_root(aRoot).has_value();
#endif
		} catch (...) {
			return false;
		}
	}
} // namespace bpr::adapters::sqlite

namespace bpr::adapters::sqlite::detail {

	struct catalog_database_platform_state final
	{
#if defined(_WIN32)
		std::vector<private_windows_object> Directories;
		std::unique_ptr<secure_windows_vfs> Vfs;
#elif defined(__linux__)
		std::vector<file_descriptor>        Directories;
		std::unique_ptr<retained_linux_vfs> Vfs;
#endif
	};

	auto classify_sqlite_result(const int aResultCode) noexcept
		-> sqlite_result_classification
	{
		switch (static_cast<unsigned>(aResultCode) & 0xFFU) {
		case SQLITE_BUSY:
		case SQLITE_LOCKED:
			return sqlite_result_classification::busy;
		case SQLITE_CORRUPT:
		case SQLITE_NOTADB:
			return sqlite_result_classification::integrity_failure;
		case SQLITE_INTERRUPT:
			return sqlite_result_classification::cancelled;
		case SQLITE_FULL:
			return sqlite_result_classification::quota_exceeded;
		default:
			return sqlite_result_classification::unavailable;
		}
	}

#if defined(BPR_SQLITE_TESTING)
	namespace testing {
		void set_private_store_open_checkpoint(const private_store_open_checkpoint aCheckpoint) noexcept
		{
			PrivateStoreOpenCheckpoint.store(aCheckpoint, std::memory_order_release);
		}

		void interrupt_next_bootstrap_before_publication() noexcept
		{
			InterruptBootstrapBeforePublication.store(true, std::memory_order_relaxed);
		}

		void interrupt_next_migration_before_commit() noexcept
		{
			InterruptMigrationBeforeCommit.store(true, std::memory_order_relaxed);
		}

		auto classify_sqlite_result_code(const int aResultCode) noexcept
			-> application::epoch_catalog_failure
		{
			using output = application::epoch_catalog_failure;
			switch (classify_sqlite_result(aResultCode)) {
			case sqlite_result_classification::busy:
				return output::busy;
			case sqlite_result_classification::integrity_failure:
				return output::integrity_failure;
			case sqlite_result_classification::cancelled:
				return output::cancelled;
			case sqlite_result_classification::quota_exceeded:
				return output::quota_exceeded;
			case sqlite_result_classification::unavailable:
				return output::unavailable;
			}
			return output::unavailable;
		}
	#if defined(_WIN32)
		// Whether an owner-private catalog object is usable by the process that
		// created it. The directory, the file, the write, and both validations run
		// through the product's own primitives, so the answer is about the
		// product's descriptor rather than about a copy of its rule kept in a test.
		auto probe_private_descriptor(const std::filesystem::path& aDirectory)
			-> private_descriptor_probe
		{
			const auto ace_count = [](const HANDLE handle) -> std::uint32_t {
				PACL                 dacl       = nullptr;
				PSECURITY_DESCRIPTOR descriptor = nullptr;
				if (::GetSecurityInfo(
						handle,
						SE_FILE_OBJECT,
						DACL_SECURITY_INFORMATION,
						nullptr,
						nullptr,
						&dacl,
						nullptr,
						&descriptor)
					!= ERROR_SUCCESS) {
					return 0U;
				}
				const auto           owned = local_memory{ descriptor };
				ACL_SIZE_INFORMATION information{ };
				if (dacl == nullptr
					|| ::GetAclInformation(
						   dacl, &information, sizeof(information), AclSizeInformation)
						== 0) {
					return 0U;
				}
				return static_cast<std::uint32_t>(information.AceCount);
			};

			private_descriptor_probe result;
			const auto               security = private_security::load();
			if (!security) {
				result.LastError = ::GetLastError();
				return result;
			}
			result.Contained = security->contained();

			const auto                  directory = aDirectory / L"private-descriptor-probe";
			const auto                  file      = directory / L"probe.bin";
			private_security_attributes directory_attributes;
			private_security_attributes file_attributes;
			if (!directory_attributes.initialize(
					*security, windows_object_kind::directory)
				|| !file_attributes.initialize(
					*security, windows_object_kind::regular_file)) {
				return result;
			}
			if (::CreateDirectoryW(
					directory.c_str(), directory_attributes.attributes())
				== 0) {
				result.LastError = ::GetLastError();
				return result;
			}
			result.Created = true;

			if (auto opened = open_existing_object(
					directory,
					windows_object_kind::directory,
					FILE_LIST_DIRECTORY | FILE_TRAVERSE);
				opened) {
				result.DirectoryValid    = validate_private_security(
											   opened->Value, *security, windows_object_kind::directory)
											   .has_value();
				result.DirectoryAceCount = ace_count(opened->Value);
			} else {
				result.LastError = ::GetLastError();
			}

			windows_handle handle{ ::CreateFileW(
				file.c_str(),
				GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				file_attributes.attributes(),
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
					| FILE_FLAG_WRITE_THROUGH,
				nullptr) };
			if (!handle.valid()) {
				result.LastError = ::GetLastError();
			} else {
				const auto payload = std::byte{ 0x42 };
				DWORD      written = 0U;
				result.Usable
					= ::WriteFile(handle.Value, &payload, 1U, &written, nullptr) != 0
					&& written == 1U && ::FlushFileBuffers(handle.Value) != 0;
				if (!result.Usable) {
					result.LastError = ::GetLastError();
				}
				result.FileValid    = validate_private_security(
										  handle.Value, *security, windows_object_kind::regular_file)
										  .has_value();
				result.FileAceCount = ace_count(handle.Value);
				handle.reset();
			}

			(void)::DeleteFileW(file.c_str());
			(void)::RemoveDirectoryW(directory.c_str());
			return result;
		}
	#endif
	} // namespace testing
#endif

	catalog_database::catalog_database(
		::sqlite3* aHandle, std::filesystem::path aPath) noexcept
		: catalog_database{ aHandle, std::move(aPath), nullptr }
	{
	}

	catalog_database::catalog_database(
		::sqlite3*                                       aHandle,
		std::filesystem::path                            aPath,
		std::unique_ptr<catalog_database_platform_state> aPlatformState) noexcept
		: Handle{ aHandle }
		, Path{ std::move(aPath) }
		, PlatformState{ std::move(aPlatformState) }
	{
	}

	catalog_database::catalog_database(catalog_database&& aOther) noexcept
		: Handle{ std::exchange(aOther.Handle, nullptr) }
		, Path{ std::move(aOther.Path) }
		, PlatformState{ std::move(aOther.PlatformState) }
	{
	}

	auto catalog_database::operator=(catalog_database&& aOther) noexcept
		-> catalog_database&
	{
		if (this != &aOther) {
			if (Handle != nullptr) {
				(void)::sqlite3_close_v2(Handle);
			}
			PlatformState.reset();
			Handle        = std::exchange(aOther.Handle, nullptr);
			Path          = std::move(aOther.Path);
			PlatformState = std::move(aOther.PlatformState);
		}
		return *this;
	}

	catalog_database::~catalog_database()
	{
		if (Handle != nullptr) {
			(void)::sqlite3_close_v2(Handle);
			Handle = nullptr;
		}
		PlatformState.reset();
	}

	auto catalog_database::open(
		const std::filesystem::path& aStorageRoot,
		std::stop_token              aStop)
		-> std::expected<catalog_database, catalog_database_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
#if defined(_WIN32)
		try {
			auto directory = open_state_directory(aStorageRoot);
			if (!directory) {
				return std::unexpected(as_catalog_failure(directory.error()));
			}
			auto& security         = directory->handles().Security;
			auto& retained_version = directory->handles().Directories.back();
			auto  lock             = catalog_lock::acquire(
				retained_version, security, aStop);
			if (!lock) {
				return std::unexpected(lock.error());
			}
			if (auto cleaned = cleanup_bootstrap_orphans(
					retained_version, security);
				!cleaned) {
				return std::unexpected(cleaned.error());
			}
			const auto native_final_path = retained_version.Path / std::wstring{ CatalogNameWide };
			auto       missing           = path_missing(native_final_path);
			if (!missing) {
				return std::unexpected(missing.error());
			}
			if (*missing) {
				auto created = create_bootstrap(
					retained_version, security, aStop);
				if (!created) {
					return std::unexpected(created.error());
				}
			}
			if (auto paths = validate_database_paths(
					retained_version, security, true);
				!paths) {
				return std::unexpected(paths.error());
			}
			auto expected_file = identity_at(
				retained_version, CatalogNameWide, security);
			if (!expected_file) {
				return std::unexpected(expected_file.error());
			}
			// The last three entries of the retained chain are the storage
			// root, `state`, and `v1`. They are read back below and compared
			// to what is on disk, so a directory replaced after the bootstrap
			// is refused rather than used.
			const auto& retained = directory->handles().Directories;
			const auto  expected_root
				= retained.at(retained.size() - 3U).Identity;
			const auto expected_state
				= retained.at(retained.size() - 2U).Identity;
			const auto expected_version = retained_version.Identity;
			auto       private_vfs      = secure_windows_vfs::create(
				retained_version, security, native_final_path);
			if (!private_vfs) {
				return std::unexpected(private_vfs.error());
			}
			auto opened = open_sqlite(
				native_final_path, (*private_vfs)->name());
			if (!opened) {
				return std::unexpected(
					(*private_vfs)->failure_or(opened.error()));
			}
			::sqlite3* database = *opened;
			auto       current  = sqlite_main_file_is_current(
				database,
				retained_version,
				*expected_file,
				security);
			auto configured = current
				? configure_connection(database)
				: std::expected<void, failure>{
					  std::unexpected(current.error())
				  };
			auto migrated   = configured
				? migrate_schema(database, aStop)
				: std::expected<void, failure>{
					  std::unexpected(configured.error())
				  };
			auto validated  = migrated
				? validate_schema(database, aStop)
				: std::expected<void, failure>{
					  std::unexpected(migrated.error())
				  };
			if (!validated) {
				(void)::sqlite3_close_v2(database);
				return std::unexpected(
					(*private_vfs)->failure_or(validated.error()));
			}
			auto live_root = open_private_directory(
				retained.at(retained.size() - 3U).Path, security);
			auto live_state = open_private_directory(
				retained.at(retained.size() - 2U).Path, security);
			auto live_version = open_private_directory(
				retained_version.Path, security);
			if (!live_root || !live_state || !live_version
				|| live_root->Identity != expected_root
				|| live_state->Identity != expected_state
				|| live_version->Identity != expected_version) {
				(void)::sqlite3_close_v2(database);
				return std::unexpected(failure::invalid_path);
			}
			if (auto final_current = sqlite_main_file_is_current(
					database,
					retained_version,
					*expected_file,
					security);
				!final_current) {
				(void)::sqlite3_close_v2(database);
				return std::unexpected(final_current.error());
			}
			if (auto paths = validate_database_paths(
					retained_version, security, true);
				!paths) {
				(void)::sqlite3_close_v2(database);
				return std::unexpected(paths.error());
			}
			if (const auto vfs_failure = (*private_vfs)->recorded_failure()) {
				(void)::sqlite3_close_v2(database);
				return std::unexpected(*vfs_failure);
			}
			if (auto authorizer = install_runtime_authorizer(database); !authorizer) {
				(void)::sqlite3_close_v2(database);
				return std::unexpected(authorizer.error());
			}
			const auto public_path = aStorageRoot / "state" / "v1"
				/ std::string{ CatalogName };
			auto platform_state = std::make_unique<catalog_database_platform_state>();
			platform_state->Directories
				= std::move(directory->handles().Directories);
			platform_state->Vfs = std::move(*private_vfs);
			return catalog_database{
				database, public_path, std::move(platform_state)
			};
		} catch (const std::bad_alloc&) {
			return std::unexpected(failure::unavailable);
		} catch (const std::filesystem::filesystem_error&) {
			return std::unexpected(failure::invalid_path);
		} catch (...) {
			return std::unexpected(failure::unavailable);
		}
#else
		auto directory = open_state_directory(aStorageRoot);
		if (!directory) {
			return std::unexpected(as_catalog_failure(directory.error()));
		}
		auto& version = directory->handles().Version;
	#if defined(__linux__)
		auto retained_vfs = retained_linux_vfs::create();
		if (!retained_vfs) {
			return std::unexpected(retained_vfs.error());
		}
		const auto* const requestedVfs = (*retained_vfs)->name();
	#else
		constexpr const char* requestedVfs = nullptr;
	#endif
		while (::flock(version.Value, LOCK_EX) != 0) {
			if (errno != EINTR) {
				return std::unexpected(failure::unavailable);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
		}
		if (auto cleaned = cleanup_bootstrap_orphans(version); !cleaned) {
			return std::unexpected(cleaned.error());
		}
		const auto        parentPath = aStorageRoot / "state" / "v1";
		const std::string catalogName{ CatalogName };
		struct stat       existing{ };
		const auto        status = ::fstatat(
			version.Value,
			catalogName.c_str(),
			&existing,
			AT_SYMLINK_NOFOLLOW);
		if (status != 0 && errno != ENOENT) {
			return std::unexpected(failure::invalid_path);
		}
		if (status != 0) {
			auto created = create_bootstrap(version, aStop, requestedVfs);
			if (!created) {
				return std::unexpected(created.error());
			}
		}
		if (auto paths = validate_database_paths(version); !paths) {
			return std::unexpected(paths.error());
		}
		auto expectedFile      = identity_at(version, CatalogName);
		auto expectedDirectory = identity_of(version);
		if (!expectedFile || !expectedDirectory) {
			return std::unexpected(
				expectedFile ? expectedDirectory.error() : expectedFile.error());
		}
		const auto finalPath = parentPath / CatalogName;
		auto       opened    = open_sqlite(
			descriptor_child_path(version, CatalogName), requestedVfs);
		if (!opened) {
			return std::unexpected(opened.error());
		}
		::sqlite3* database   = *opened;
		auto       configured = configure_connection(database);
		auto       migrated   = configured
			? migrate_schema(database, aStop)
			: std::expected<void, failure>{ std::unexpected(configured.error()) };
		auto       validated  = migrated
			? validate_schema(database, aStop)
			: std::expected<void, failure>{ std::unexpected(migrated.error()) };
		if (!validated) {
			(void)::sqlite3_close_v2(database);
			return std::unexpected(validated.error());
		}
		auto liveDirectory         = open_private_directory(parentPath);
		auto liveDirectoryIdentity = liveDirectory
			? identity_of(*liveDirectory)
			: std::expected<file_identity, failure>{
				  std::unexpected(liveDirectory.error())
			  };
		if (!liveDirectoryIdentity
			|| *liveDirectoryIdentity != *expectedDirectory) {
			(void)::sqlite3_close_v2(database);
			return std::unexpected(failure::invalid_path);
		}
		if (auto current = sqlite_main_file_is_current(
				database, version, *expectedFile);
			!current) {
			(void)::sqlite3_close_v2(database);
			return std::unexpected(current.error());
		}
		if (auto paths = validate_database_paths(version); !paths) {
			(void)::sqlite3_close_v2(database);
			return std::unexpected(paths.error());
		}
		if (auto authorizer = install_runtime_authorizer(database); !authorizer) {
			(void)::sqlite3_close_v2(database);
			return std::unexpected(authorizer.error());
		}
	#if defined(__linux__)
		auto platform_state = std::make_unique<catalog_database_platform_state>();
		platform_state->Directories.reserve(3U);
		platform_state->Directories.push_back(
			std::move(directory->handles().Root));
		platform_state->Directories.push_back(
			std::move(directory->handles().State));
		platform_state->Directories.push_back(
			std::move(directory->handles().Version));
		platform_state->Vfs = std::move(*retained_vfs);
		return catalog_database{
			database, finalPath, std::move(platform_state)
		};
	#else
		return catalog_database{ database, finalPath };
	#endif
#endif
	}

	auto catalog_database::handle() const noexcept -> ::sqlite3*
	{
		return Handle;
	}

	auto catalog_database::path() const noexcept
		-> const std::filesystem::path&
	{
		return Path;
	}

	auto secure_random_bytes(std::span<std::uint8_t> aOutput) noexcept -> bool
	{
		return random_bytes(aOutput);
	}
} // namespace bpr::adapters::sqlite::detail
