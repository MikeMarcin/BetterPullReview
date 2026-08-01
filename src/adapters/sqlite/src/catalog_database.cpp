#include "catalog_database.hpp"

#include "bpr/application/epoch_catalog_port.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
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

namespace bpr::adapters::sqlite::detail
{
namespace
{
using failure = catalog_database_failure;

constexpr std::string_view CatalogName = "epoch-catalog.sqlite3";
constexpr std::int32_t ApplicationId = 0x42504543;
constexpr std::int32_t UserVersion = 1;
#if defined(BPR_SQLITE_TESTING)
std::atomic_bool InterruptBootstrapBeforePublication = false;
#endif
static_assert(
    application::maximum_retained_epoch_count_per_pull_request == 100'000U);

struct schema_object
{
    std::string_view Type;
    std::string_view Name;
    std::string_view Sql;
};

// These are the complete BPEC v1 objects. The schema contains typed facts
// only. Ref names are represented by their versioned components, so no path,
// provider string, opaque token, or generic payload enters the catalog.
constexpr std::array SchemaObjects{
    schema_object{
        "table",
        "catalog_meta",
        R"sql(CREATE TABLE catalog_meta(
            singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
            schema_format INTEGER NOT NULL CHECK(schema_format = 1),
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "catalog_meta_incarnation",
        R"sql(CREATE UNIQUE INDEX catalog_meta_incarnation ON catalog_meta(incarnation))sql"},
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
            account_id BLOB NOT NULL CHECK(length(account_id) = 8 AND account_id != zeroblob(8)),
            repository_id BLOB NOT NULL CHECK(length(repository_id) = 8 AND repository_id != zeroblob(8)),
            object_format INTEGER NOT NULL CHECK(object_format IN (1, 2)),
            lifecycle INTEGER NOT NULL CHECK(lifecycle IN (1, 2, 3, 4)),
            PRIMARY KEY(incarnation, cache_scope),
            FOREIGN KEY(incarnation) REFERENCES catalog_meta(incarnation)
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "repository_scope_active_authority",
        R"sql(CREATE UNIQUE INDEX repository_scope_active_authority ON repository_scope(
            incarnation, profile_id, profile_revision, host_id, host_authority,
            account_id, repository_id, object_format
        ) WHERE lifecycle != 4)sql"},
    schema_object{
        "table",
        "pull_request_scope",
        R"sql(CREATE TABLE pull_request_scope(
            incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32)),
            pr_scope BLOB NOT NULL CHECK(length(pr_scope) = 32 AND pr_scope != zeroblob(32)),
            cache_scope BLOB NOT NULL CHECK(length(cache_scope) = 32 AND cache_scope != zeroblob(32)),
            viewer_id BLOB NOT NULL CHECK(length(viewer_id) = 8 AND viewer_id != zeroblob(8)),
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
            FOREIGN KEY(incarnation, cache_scope)
                REFERENCES repository_scope(incarnation, cache_scope),
            FOREIGN KEY(incarnation, pr_scope, latest_live_ordinal)
                REFERENCES epoch(incarnation, pr_scope, ordinal),
            FOREIGN KEY(incarnation, pr_scope, previous_distinct_head_ordinal)
                REFERENCES epoch(incarnation, pr_scope, ordinal)
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "pull_request_scope_cache_id",
        R"sql(CREATE UNIQUE INDEX pull_request_scope_cache_id ON pull_request_scope(
            incarnation, cache_scope, pull_request_id
        ))sql"},
    schema_object{
        "index",
        "pull_request_scope_cache_number",
        R"sql(CREATE UNIQUE INDEX pull_request_scope_cache_number ON pull_request_scope(
            incarnation, cache_scope, pull_request_number
        ))sql"},
    schema_object{
        "index",
        "pull_request_scope_cache_binding",
        R"sql(CREATE UNIQUE INDEX pull_request_scope_cache_binding ON pull_request_scope(
            incarnation, cache_scope, pr_scope
        ))sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "epoch_source_revision",
        R"sql(CREATE UNIQUE INDEX epoch_source_revision ON epoch(
            incarnation, source_observation
        ))sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "epoch_anchor_retained_ref",
        R"sql(CREATE UNIQUE INDEX epoch_anchor_retained_ref ON epoch_anchor(
            incarnation, pr_scope, ref_format, ordinal, endpoint
        ))sql"},
    schema_object{
        "index",
        "epoch_anchor_object",
        R"sql(CREATE INDEX epoch_anchor_object ON epoch_anchor(
            incarnation, pr_scope, endpoint, object_algorithm, object_digest,
            ordinal DESC
        ))sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "review_association_review",
        R"sql(CREATE UNIQUE INDEX review_association_review ON review_association(
            incarnation, pr_scope, review_id
        ))sql"},
    schema_object{
        "index",
        "review_association_order",
        R"sql(CREATE UNIQUE INDEX review_association_order ON review_association(
            incarnation, pr_scope, review_order
        ))sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "operation_recovery",
        R"sql(CREATE INDEX operation_recovery ON operation(
            incarnation, cache_scope, terminal, kind, phase, operation
        ))sql"},
    schema_object{
        "index",
        "operation_source_observation",
        R"sql(CREATE UNIQUE INDEX operation_source_observation ON operation(
            incarnation, source_observation
        ) WHERE kind = 1)sql"},
    schema_object{
        "index",
        "operation_active_pull_request",
        R"sql(CREATE UNIQUE INDEX operation_active_pull_request ON operation(
            incarnation, pr_scope
        ) WHERE pr_scope IS NOT NULL AND terminal NOT IN (1, 2))sql"},
    schema_object{
        "index",
        "operation_pull_request_binding",
        R"sql(CREATE UNIQUE INDEX operation_pull_request_binding ON operation(
            incarnation, operation, pr_scope
        ))sql"},
    schema_object{
        "index",
        "operation_cache_binding",
        R"sql(CREATE UNIQUE INDEX operation_cache_binding ON operation(
            incarnation, operation, cache_scope
        ))sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
    schema_object{
        "index",
        "append_inventory_pack",
        R"sql(CREATE UNIQUE INDEX append_inventory_pack ON append_inventory(
            incarnation, operation, pack_algorithm, pack_digest
        ))sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
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
        ) STRICT, WITHOUT ROWID)sql"},
};

class runtime final
{
public:
    runtime() noexcept : Result{::sqlite3_initialize()} {}
    ~runtime()
    {
        if(Result == SQLITE_OK)
        {
            (void)::sqlite3_shutdown();
        }
    }

    int Result = SQLITE_ERROR;
};

[[nodiscard]] auto sqlite_runtime() noexcept -> runtime&
{
    static runtime value;
    return value;
}

struct statement final
{
    statement() = default;
    statement(const statement&) = delete;
    auto operator=(const statement&) -> statement& = delete;
    statement(statement&& other) noexcept
        : Value{std::exchange(other.Value, nullptr)}
    {
    }
    auto operator=(statement&& other) noexcept -> statement&
    {
        if(this != &other)
        {
            if(Value != nullptr)
            {
                (void)::sqlite3_finalize(Value);
            }
            Value = std::exchange(other.Value, nullptr);
        }
        return *this;
    }
    ~statement()
    {
        if(Value != nullptr)
        {
            (void)::sqlite3_finalize(Value);
        }
    }

    ::sqlite3_stmt* Value = nullptr;
};

struct progress_context
{
    std::stop_token Stop;
};

[[nodiscard]] auto catalog_failure_from_sqlite_result(
    const int result_code,
    const failure fallback = failure::unavailable) noexcept -> failure
{
    switch(classify_sqlite_result(result_code))
    {
    case sqlite_result_classification::integrity_failure:
        return failure::integrity_failure;
    case sqlite_result_classification::cancelled:
        return failure::cancelled;
    case sqlite_result_classification::quota_exceeded:
        return failure::quota_exceeded;
    case sqlite_result_classification::busy:
    case sqlite_result_classification::unavailable:
        return fallback;
    }
    return fallback;
}

[[nodiscard]] auto catalog_sql_failure(
    ::sqlite3* database,
    const failure fallback = failure::unavailable) noexcept -> failure
{
    return catalog_failure_from_sqlite_result(
        ::sqlite3_extended_errcode(database), fallback);
}

auto progress_callback(void* opaque) noexcept -> int
{
    const auto* context = static_cast<const progress_context*>(opaque);
    return context != nullptr && context->Stop.stop_requested() ? 1 : 0;
}

class progress_scope final
{
public:
    progress_scope(::sqlite3* database, std::stop_token stop)
        : Database{database}, Context{.Stop = stop}
    {
        ::sqlite3_progress_handler(
            Database, 256, progress_callback, &Context);
    }
    ~progress_scope()
    {
        ::sqlite3_progress_handler(Database, 0, nullptr, nullptr);
    }

private:
    ::sqlite3* Database;
    progress_context Context;
};

[[nodiscard]] auto prepare(
    ::sqlite3* database, std::string_view sql)
    -> std::expected<statement, failure>
{
    statement result;
    const char* tail = nullptr;
    const auto code = ::sqlite3_prepare_v3(
        database,
        sql.data(),
        static_cast<int>(sql.size()),
        SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_NO_VTAB,
        &result.Value,
        &tail);
    if(code != SQLITE_OK || result.Value == nullptr)
    {
        return std::unexpected(catalog_failure_from_sqlite_result(code));
    }
    while(tail != sql.data() + sql.size())
    {
        if(std::isspace(static_cast<unsigned char>(*tail)) == 0)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        ++tail;
    }
    return result;
}

[[nodiscard]] auto execute(
    ::sqlite3* database,
    std::string_view sql,
    std::stop_token stop = {}) -> std::expected<void, failure>
{
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    progress_scope progress{database, stop};
    auto prepared = prepare(database, sql);
    if(!prepared)
    {
        return std::unexpected(prepared.error());
    }
    const auto code = ::sqlite3_step(prepared->Value);
    if(code == SQLITE_INTERRUPT || stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    if(code != SQLITE_DONE && code != SQLITE_ROW)
    {
        return std::unexpected(catalog_sql_failure(database));
    }
    if(code == SQLITE_ROW)
    {
        const auto done = ::sqlite3_step(prepared->Value);
        if(done != SQLITE_DONE)
        {
            return std::unexpected(catalog_sql_failure(database));
        }
    }
    return {};
}

[[nodiscard]] auto scalar_integer(
    ::sqlite3* database, std::string_view sql)
    -> std::expected<std::int64_t, failure>
{
    auto prepared = prepare(database, sql);
    if(!prepared)
    {
        return std::unexpected(prepared.error());
    }
    const auto row = ::sqlite3_step(prepared->Value);
    if(row != SQLITE_ROW)
    {
        return std::unexpected(catalog_sql_failure(
            database, failure::schema_mismatch));
    }
    if(::sqlite3_column_type(prepared->Value, 0) != SQLITE_INTEGER)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    const auto result = ::sqlite3_column_int64(prepared->Value, 0);
    if(::sqlite3_step(prepared->Value) != SQLITE_DONE)
    {
        return std::unexpected(catalog_sql_failure(
            database, failure::schema_mismatch));
    }
    return result;
}

[[nodiscard]] auto scalar_text(
    ::sqlite3* database, std::string_view sql)
    -> std::expected<std::string, failure>
{
    auto prepared = prepare(database, sql);
    if(!prepared)
    {
        return std::unexpected(prepared.error());
    }
    const auto row = ::sqlite3_step(prepared->Value);
    if(row != SQLITE_ROW)
    {
        return std::unexpected(catalog_sql_failure(
            database, failure::schema_mismatch));
    }
    if(::sqlite3_column_type(prepared->Value, 0) != SQLITE_TEXT)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    const auto* text = reinterpret_cast<const char*>(
        ::sqlite3_column_text(prepared->Value, 0));
    const auto bytes = ::sqlite3_column_bytes(prepared->Value, 0);
    if(text == nullptr || bytes < 0)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    std::string result{text, static_cast<std::size_t>(bytes)};
    if(::sqlite3_step(prepared->Value) != SQLITE_DONE)
    {
        return std::unexpected(catalog_sql_failure(
            database, failure::schema_mismatch));
    }
    return result;
}

[[nodiscard]] auto canonical_sql(std::string_view sql) -> std::string
{
    std::string result;
    result.reserve(sql.size());
    for(const auto value : sql)
    {
        if(std::isspace(static_cast<unsigned char>(value)) == 0
           && value != ';')
        {
            result.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(value))));
        }
    }
    return result;
}

[[nodiscard]] auto configure_connection(::sqlite3* database)
    -> std::expected<void, failure>
{
    constexpr std::int64_t CatalogPageSize = 4'096;
    constexpr std::int64_t CatalogMaximumPageCount = 262'144;

    ::sqlite3_extended_result_codes(database, 1);
    (void)::sqlite3_busy_timeout(database, 2'000);

    (void)::sqlite3_limit(database, SQLITE_LIMIT_LENGTH, 1 << 20);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_SQL_LENGTH, 1 << 18);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_COLUMN, 128);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_EXPR_DEPTH, 64);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_COMPOUND_SELECT, 8);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_VDBE_OP, 250'000);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_FUNCTION_ARG, 32);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_ATTACHED, 0);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_LIKE_PATTERN_LENGTH, 256);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_VARIABLE_NUMBER, 128);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_TRIGGER_DEPTH, 0);
    (void)::sqlite3_limit(database, SQLITE_LIMIT_WORKER_THREADS, 0);
    if(::sqlite3_limit(database, SQLITE_LIMIT_TRIGGER_DEPTH, -1) != 0)
    {
        return std::unexpected(failure::unavailable);
    }

    constexpr std::array configs{
        std::pair{SQLITE_DBCONFIG_DEFENSIVE, 1},
        std::pair{SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0},
        std::pair{SQLITE_DBCONFIG_DQS_DDL, 0},
        std::pair{SQLITE_DBCONFIG_DQS_DML, 0},
        std::pair{SQLITE_DBCONFIG_ENABLE_FKEY, 1},
        std::pair{SQLITE_DBCONFIG_ENABLE_TRIGGER, 0},
        std::pair{SQLITE_DBCONFIG_ENABLE_VIEW, 0},
        std::pair{SQLITE_DBCONFIG_WRITABLE_SCHEMA, 0},
        std::pair{SQLITE_DBCONFIG_ENABLE_QPSG, 1},
    };
    for(const auto& [setting, expected] : configs)
    {
        auto actual = -1;
        const auto code =
            ::sqlite3_db_config(database, setting, expected, &actual);
        if(code != SQLITE_OK)
        {
            return std::unexpected(catalog_failure_from_sqlite_result(code));
        }
        if(actual != expected)
        {
            return std::unexpected(failure::unavailable);
        }
    }
    if(::sqlite3_compileoption_used("OMIT_LOAD_EXTENSION") == 0
       || ::sqlite3_compileoption_used("OMIT_SHARED_CACHE") == 0
       || ::sqlite3_compileoption_used("OMIT_AUTOINIT") == 0
       || ::sqlite3_compileoption_used("DQS=0") == 0
       || ::sqlite3_compileoption_used("MAX_ATTACHED=0") == 0
       || ::sqlite3_compileoption_used("MAX_PAGE_COUNT=262144") == 0)
    {
        return std::unexpected(failure::unavailable);
    }

    if(auto set_page_size = execute(database, "PRAGMA page_size = 4096");
       !set_page_size)
    {
        return set_page_size;
    }
    const auto page_size = scalar_integer(database, "PRAGMA page_size");
    if(!page_size)
    {
        return std::unexpected(page_size.error());
    }
    if(*page_size != CatalogPageSize)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    const auto maximum_page_count =
        scalar_integer(database, "PRAGMA max_page_count = 262144");
    if(!maximum_page_count)
    {
        return std::unexpected(maximum_page_count.error());
    }
    if(*maximum_page_count != CatalogMaximumPageCount)
    {
        return std::unexpected(
            *maximum_page_count > CatalogMaximumPageCount
                ? failure::quota_exceeded
                : failure::unavailable);
    }

    constexpr std::array pragmas{
        "PRAGMA temp_store = MEMORY",
        "PRAGMA mmap_size = 0",
        "PRAGMA wal_autocheckpoint = 256",
        "PRAGMA synchronous = FULL",
    };
    for(const auto sql : pragmas)
    {
        if(auto result = execute(database, sql); !result)
        {
            return result;
        }
    }
    auto journal = scalar_text(database, "PRAGMA journal_mode = WAL");
    if(!journal)
    {
        return std::unexpected(journal.error());
    }
    if(*journal != "wal")
    {
        return std::unexpected(failure::unavailable);
    }
    auto synchronous = scalar_integer(database, "PRAGMA synchronous");
    auto foreign_keys = scalar_integer(database, "PRAGMA foreign_keys");
    auto trusted = scalar_integer(database, "PRAGMA trusted_schema");
    auto mmap = scalar_integer(database, "PRAGMA mmap_size");
    auto autocheckpoint = scalar_integer(database, "PRAGMA wal_autocheckpoint");
    auto verified_page_size = scalar_integer(database, "PRAGMA page_size");
    auto verified_maximum_page_count =
        scalar_integer(database, "PRAGMA max_page_count");
    if(!verified_page_size)
    {
        return std::unexpected(verified_page_size.error());
    }
    if(!verified_maximum_page_count)
    {
        return std::unexpected(verified_maximum_page_count.error());
    }
    if(*verified_page_size != CatalogPageSize)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(*verified_maximum_page_count != CatalogMaximumPageCount)
    {
        return std::unexpected(
            *verified_maximum_page_count > CatalogMaximumPageCount
                ? failure::quota_exceeded
                : failure::unavailable);
    }
    if(!synchronous || *synchronous != 2 || !foreign_keys
       || *foreign_keys != 1 || !trusted || *trusted != 0 || !mmap
       || *mmap != 0 || !autocheckpoint || *autocheckpoint != 256)
    {
        return std::unexpected(failure::unavailable);
    }
    return {};
}

[[nodiscard]] auto create_schema(
    ::sqlite3* database,
    const std::array<std::uint8_t, 32>& incarnation,
    std::stop_token stop) -> std::expected<void, failure>
{
    if(auto begin = execute(database, "BEGIN IMMEDIATE", stop); !begin)
    {
        return begin;
    }
    auto rollback = [&database]() noexcept {
        (void)::sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
    };
    if(auto app = execute(database, "PRAGMA application_id = 1112556867", stop);
       !app)
    {
        rollback();
        return app;
    }
    if(auto version = execute(database, "PRAGMA user_version = 1", stop);
       !version)
    {
        rollback();
        return version;
    }
    for(const auto& object : SchemaObjects)
    {
        if(auto result = execute(database, object.Sql, stop); !result)
        {
            rollback();
            return result;
        }
    }
    auto insert = prepare(
        database,
        "INSERT INTO catalog_meta(singleton, schema_format, incarnation) "
        "VALUES(1, 1, ?1)");
    if(!insert)
    {
        rollback();
        return std::unexpected(insert.error());
    }
    if(::sqlite3_bind_blob(
              insert->Value,
              1,
              incarnation.data(),
              static_cast<int>(incarnation.size()),
              SQLITE_TRANSIENT)
           != SQLITE_OK)
    {
        rollback();
        return std::unexpected(failure::unavailable);
    }
    if(::sqlite3_step(insert->Value) != SQLITE_DONE)
    {
        const auto write_failure = catalog_sql_failure(database);
        rollback();
        return std::unexpected(write_failure);
    }
    if(auto commit = execute(database, "COMMIT", stop); !commit)
    {
        rollback();
        return commit;
    }
    return {};
}

[[nodiscard]] auto validate_schema(::sqlite3* database, std::stop_token stop)
    -> std::expected<void, failure>
{
    progress_scope progress{database, stop};
    const auto application_id = scalar_integer(database, "PRAGMA application_id");
    const auto user_version = scalar_integer(database, "PRAGMA user_version");
    if(!application_id || *application_id != ApplicationId || !user_version
       || *user_version != UserVersion)
    {
        return std::unexpected(failure::schema_mismatch);
    }

    auto meta = prepare(
        database,
        "SELECT schema_format, incarnation FROM catalog_meta WHERE singleton = 1");
    if(!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
       || ::sqlite3_column_int64(meta->Value, 0) != UserVersion
       || ::sqlite3_column_type(meta->Value, 1) != SQLITE_BLOB
       || ::sqlite3_column_bytes(meta->Value, 1) != 32)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    const auto* incarnation = static_cast<const std::uint8_t*>(
        ::sqlite3_column_blob(meta->Value, 1));
    if(incarnation == nullptr
       || std::all_of(incarnation, incarnation + 32, [](const auto byte) {
              return byte == 0U;
          })
       || ::sqlite3_step(meta->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }

    auto objects = prepare(
        database,
        "SELECT type, name, sql FROM sqlite_schema "
        "WHERE name NOT LIKE 'sqlite_%' ORDER BY name");
    if(!objects)
    {
        return std::unexpected(objects.error());
    }
    std::vector<schema_object> expected{
        SchemaObjects.begin(), SchemaObjects.end()};
    std::ranges::sort(expected, {}, &schema_object::Name);
    std::size_t index = 0;
    while(true)
    {
        const auto code = ::sqlite3_step(objects->Value);
        if(code == SQLITE_DONE)
        {
            break;
        }
        if(code == SQLITE_INTERRUPT || stop.stop_requested())
        {
            return std::unexpected(failure::cancelled);
        }
        if(code != SQLITE_ROW || index >= expected.size())
        {
            return std::unexpected(failure::schema_mismatch);
        }
        const auto read_text = [&objects](const int column)
            -> std::optional<std::string_view> {
            if(::sqlite3_column_type(objects->Value, column) != SQLITE_TEXT)
            {
                return std::nullopt;
            }
            const auto* value = reinterpret_cast<const char*>(
                ::sqlite3_column_text(objects->Value, column));
            const auto bytes = ::sqlite3_column_bytes(objects->Value, column);
            if(value == nullptr || bytes < 0)
            {
                return std::nullopt;
            }
            return std::string_view{value, static_cast<std::size_t>(bytes)};
        };
        const auto type = read_text(0);
        const auto name = read_text(1);
        const auto sql = read_text(2);
        if(!type || !name || !sql || *type != expected[index].Type
           || *name != expected[index].Name
           || canonical_sql(*sql) != canonical_sql(expected[index].Sql))
        {
            return std::unexpected(failure::schema_mismatch);
        }
        ++index;
    }
    if(index != expected.size())
    {
        return std::unexpected(failure::schema_mismatch);
    }

    const auto integrity = scalar_text(database, "PRAGMA integrity_check(1)");
    if(!integrity || *integrity != "ok")
    {
        return std::unexpected(failure::integrity_failure);
    }
    auto foreign_key_check = prepare(database, "PRAGMA foreign_key_check");
    if(!foreign_key_check
       || ::sqlite3_step(foreign_key_check->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::integrity_failure);
    }
    return {};
}

[[nodiscard]] auto known_runtime_table(const char* value) noexcept -> bool
{
    if(value == nullptr)
    {
        return false;
    }
    constexpr std::array<std::string_view, 18> Tables{
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
        "append_receipt",
        "append_inventory",
        "append_install",
        "append_installed_artifact",
        "append_hidden_candidate",
        "append_candidate_review",
        "prune_operation",
        "cache_delete_scope",
    };
    return std::ranges::find(Tables, std::string_view{value}) != Tables.end();
}

auto runtime_authorizer(
    void*,
    const int action,
    const char* first,
    const char* second,
    const char*,
    const char*) noexcept -> int
{
    switch(action)
    {
    case SQLITE_SELECT:
    case SQLITE_TRANSACTION:
    case SQLITE_SAVEPOINT:
    case SQLITE_RECURSIVE:
        return SQLITE_OK;
    case SQLITE_READ:
    case SQLITE_INSERT:
    case SQLITE_UPDATE:
    case SQLITE_DELETE:
        return known_runtime_table(first) ? SQLITE_OK : SQLITE_DENY;
    case SQLITE_FUNCTION:
        if(second != nullptr)
        {
            const std::string_view function{second};
            if(function == "count" || function == "length"
               || function == "zeroblob")
            {
                return SQLITE_OK;
            }
        }
        return SQLITE_DENY;
    default:
        return SQLITE_DENY;
    }
}

[[nodiscard]] auto install_runtime_authorizer(::sqlite3* database)
    -> std::expected<void, failure>
{
    const auto code =
        ::sqlite3_set_authorizer(database, runtime_authorizer, nullptr);
    return code == SQLITE_OK
        ? std::expected<void, failure>{}
        : std::expected<void, failure>{std::unexpected(
              catalog_failure_from_sqlite_result(code))};
}

[[nodiscard]] auto open_sqlite(
    const std::filesystem::path& path,
    const char* requested_vfs = nullptr)
    -> std::expected<::sqlite3*, failure>
{
    if(sqlite_runtime().Result != SQLITE_OK)
    {
        return std::unexpected(failure::unavailable);
    }
    ::sqlite3* database = nullptr;
    constexpr auto flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX
        | SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_NOFOLLOW
        | SQLITE_OPEN_EXRESCODE;
#if defined(_WIN32)
    const auto encoded = path.u8string();
    const auto native = std::string{
        reinterpret_cast<const char*>(encoded.data()), encoded.size()};
    const auto* vfs = requested_vfs != nullptr
        ? requested_vfs
        : "win32-longpath";
    const auto* selected_vfs = ::sqlite3_vfs_find(vfs);
    if(selected_vfs == nullptr
       || native.size() + 8U
           >= static_cast<std::size_t>(selected_vfs->mxPathname))
    {
        return std::unexpected(failure::invalid_path);
    }
#else
    const auto native = path.string();
    const auto* vfs = requested_vfs;
#endif
    const auto code =
        ::sqlite3_open_v2(native.c_str(), &database, flags, vfs);
    if(code != SQLITE_OK || database == nullptr)
    {
        const auto open_failure = database != nullptr
            ? catalog_sql_failure(database)
            : catalog_failure_from_sqlite_result(code);
        if(database != nullptr)
        {
            (void)::sqlite3_close_v2(database);
        }
        return std::unexpected(open_failure);
    }
    return database;
}

#if !defined(_WIN32)
class file_descriptor final
{
public:
    file_descriptor() = default;
    explicit file_descriptor(const int value) noexcept : Value{value} {}
    file_descriptor(const file_descriptor&) = delete;
    auto operator=(const file_descriptor&) -> file_descriptor& = delete;
    file_descriptor(file_descriptor&& other) noexcept
        : Value{std::exchange(other.Value, -1)}
    {
    }
    auto operator=(file_descriptor&& other) noexcept -> file_descriptor&
    {
        if(this != &other)
        {
            if(Value >= 0)
            {
                (void)::close(Value);
            }
            Value = std::exchange(other.Value, -1);
        }
        return *this;
    }
    ~file_descriptor()
    {
        if(Value >= 0)
        {
            (void)::close(Value);
        }
    }

    int Value = -1;
};

[[nodiscard]] auto descriptor_child_path(
    const file_descriptor& parent,
    std::string_view name) -> std::filesystem::path
{
#if defined(__APPLE__)
    std::array<char, PATH_MAX> path{};
    if(::fcntl(parent.Value, F_GETPATH, path.data()) != 0)
    {
        return {};
    }
    return std::filesystem::path{path.data()} / std::string{name};
#elif defined(__linux__)
    constexpr std::string_view Prefix = "/proc/self/fd/";
#else
    constexpr std::string_view Prefix{};
#endif
#if !defined(__APPLE__)
    return std::filesystem::path{
        std::string{Prefix} + std::to_string(parent.Value)}
        / std::string{name};
#endif
}

[[nodiscard]] auto private_directory(const struct stat& status) noexcept
    -> bool
{
    return S_ISDIR(status.st_mode) && status.st_uid == ::geteuid()
        && (status.st_mode & 0777) == 0700;
}

[[nodiscard]] auto private_file(const struct stat& status) noexcept -> bool
{
    return S_ISREG(status.st_mode) && status.st_uid == ::geteuid()
        && (status.st_mode & 0777) == 0600 && status.st_nlink == 1;
}

[[nodiscard]] auto open_private_directory(const std::filesystem::path& path)
    -> std::expected<file_descriptor, failure>
{
    const auto native = path.string();
    file_descriptor result{
        ::open(native.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)};
    struct stat status{};
    if(result.Value < 0 || ::fstat(result.Value, &status) != 0)
    {
        return std::unexpected(failure::invalid_path);
    }
    if(!private_directory(status))
    {
        return std::unexpected(failure::invalid_permissions);
    }
    return result;
}

[[nodiscard]] auto ensure_private_child(
    const file_descriptor& parent, std::string_view name)
    -> std::expected<file_descriptor, failure>
{
    const std::string owned_name{name};
    const auto created = ::mkdirat(parent.Value, owned_name.c_str(), 0700) == 0;
    if(!created && errno != EEXIST)
    {
        return std::unexpected(failure::invalid_path);
    }
    file_descriptor result{::openat(
        parent.Value,
        owned_name.c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)};
    struct stat status{};
    if(result.Value < 0 || ::fstat(result.Value, &status) != 0)
    {
        return std::unexpected(failure::invalid_path);
    }
    if(!private_directory(status))
    {
        return std::unexpected(failure::invalid_permissions);
    }
    if(::fsync(result.Value) != 0 || ::fsync(parent.Value) != 0)
    {
        return std::unexpected(failure::unavailable);
    }
    return result;
}

[[nodiscard]] auto reliable_local_filesystem(
    const file_descriptor& directory) noexcept -> bool
{
    struct statfs filesystem{};
    if(::fstatfs(directory.Value, &filesystem) != 0)
    {
        return false;
    }
#if defined(__APPLE__)
    return (filesystem.f_flags & MNT_LOCAL) != 0;
#elif defined(__linux__)
    switch(static_cast<unsigned long>(filesystem.f_type))
    {
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

[[nodiscard]] auto random_bytes(std::span<std::uint8_t> output) noexcept
    -> bool
{
#if defined(__APPLE__)
    ::arc4random_buf(output.data(), output.size());
    return true;
#elif defined(__linux__)
    std::size_t completed = 0;
    while(completed != output.size())
    {
        const auto result = ::getrandom(
            output.data() + completed, output.size() - completed, 0);
        if(result < 0 && errno == EINTR)
        {
            continue;
        }
        if(result <= 0)
        {
            return false;
        }
        completed += static_cast<std::size_t>(result);
    }
    return true;
#else
    (void)output;
    return false;
#endif
}

[[nodiscard]] auto hex_name(const std::array<std::uint8_t, 32>& random)
    -> std::string
{
    constexpr std::string_view Digits = "0123456789abcdef";
    std::string result = ".epoch-catalog.bootstrap-";
    result.reserve(result.size() + random.size() * 2U + 8U);
    for(const auto byte : random)
    {
        result.push_back(Digits[byte >> 4U]);
        result.push_back(Digits[byte & 0x0FU]);
    }
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
        if(Registered)
        {
            (void)::sqlite3_vfs_unregister(&Interface);
        }
    }

    [[nodiscard]] static auto create()
        -> std::expected<std::unique_ptr<retained_linux_vfs>, failure>
    {
        if(sqlite_runtime().Result != SQLITE_OK)
        {
            return std::unexpected(failure::unavailable);
        }
        auto* const base = ::sqlite3_vfs_find(nullptr);
        if(base == nullptr || base->xFullPathname == nullptr)
        {
            return std::unexpected(failure::platform_unsupported);
        }

        auto random = std::array<std::uint8_t, 16U>{};
        if(!random_bytes(random))
        {
            return std::unexpected(failure::unavailable);
        }
        auto name = std::string{"bpr-retained-linux-catalog-"};
        constexpr std::string_view Digits = "0123456789abcdef";
        name.reserve(name.size() + random.size() * 2U);
        for(const auto byte : random)
        {
            name.push_back(Digits[byte >> 4U]);
            name.push_back(Digits[byte & 0x0FU]);
        }

        auto result = std::unique_ptr<retained_linux_vfs>{
            new retained_linux_vfs{*base, std::move(name)}};
        if(::sqlite3_vfs_register(&result->Interface, 0) != SQLITE_OK)
        {
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
        : Interface{base}, Name{std::move(name)}
    {
        Interface.pNext = nullptr;
        Interface.zName = Name.c_str();
        Interface.xFullPathname = &full_pathname;
    }

    [[nodiscard]] static auto descriptor_path(
        const std::string_view path) noexcept -> bool
    {
        constexpr std::string_view Prefix = "/proc/self/fd/";
        if(!path.starts_with(Prefix))
        {
            return false;
        }
        const auto separator = path.find('/', Prefix.size());
        if(separator == std::string_view::npos
           || separator == Prefix.size())
        {
            return false;
        }
        const auto descriptor = path.substr(
            Prefix.size(), separator - Prefix.size());
        const auto name = path.substr(separator + 1U);
        return std::ranges::all_of(descriptor, [](const char value) {
                   return value >= '0' && value <= '9';
               })
            && !name.empty() && name != "." && name != ".."
            && name.find('/') == std::string_view::npos;
    }

    static auto full_pathname(
        ::sqlite3_vfs*,
        const char* input,
        const int output_size,
        char* output) noexcept -> int
    {
        if(input == nullptr || output == nullptr || output_size <= 0)
        {
            return SQLITE_CANTOPEN;
        }
        const auto path = std::string_view{input};
        if(!descriptor_path(path)
           || path.size() + 1U > static_cast<std::size_t>(output_size))
        {
            return SQLITE_CANTOPEN;
        }
        std::memcpy(output, path.data(), path.size());
        output[path.size()] = '\0';
        return SQLITE_OK;
    }

    ::sqlite3_vfs Interface{};
    std::string Name;
    bool Registered = false;
};
#endif

[[nodiscard]] auto bootstrap_artifact_name(std::string_view name) noexcept
    -> bool
{
    constexpr std::string_view Prefix = ".epoch-catalog.bootstrap-";
    constexpr std::array<std::string_view, 4> Suffixes{
        ".sqlite3",
        ".sqlite3-wal",
        ".sqlite3-shm",
        ".sqlite3-journal",
    };
    for(const auto suffix : Suffixes)
    {
        if(name.size() != Prefix.size() + 64U + suffix.size()
           || !name.starts_with(Prefix) || !name.ends_with(suffix))
        {
            continue;
        }
        return std::ranges::all_of(
            name.substr(Prefix.size(), 64U), [](const char value) {
                return (value >= '0' && value <= '9')
                    || (value >= 'a' && value <= 'f');
            });
    }
    return false;
}

[[nodiscard]] auto cleanup_bootstrap_orphans(
    const file_descriptor& parent) -> std::expected<void, failure>
{
    const auto duplicate = ::dup(parent.Value);
    if(duplicate < 0)
    {
        return std::unexpected(failure::unavailable);
    }
    auto* directory = ::fdopendir(duplicate);
    if(directory == nullptr)
    {
        (void)::close(duplicate);
        return std::unexpected(failure::unavailable);
    }
    auto close_directory = [&directory]() noexcept {
        if(directory != nullptr)
        {
            (void)::closedir(directory);
            directory = nullptr;
        }
    };
    auto removed = false;
    std::size_t entries = 0U;
    errno = 0;
    while(const auto* entry = ::readdir(directory))
    {
        if(++entries > 4'096U)
        {
            close_directory();
            return std::unexpected(failure::unavailable);
        }
        const std::string_view name{entry->d_name};
        if(!bootstrap_artifact_name(name))
        {
            continue;
        }
        struct stat status{};
        if(::fstatat(
               parent.Value,
               entry->d_name,
               &status,
               AT_SYMLINK_NOFOLLOW)
               != 0
           || !private_file(status))
        {
            close_directory();
            return std::unexpected(failure::invalid_permissions);
        }
        if(::unlinkat(parent.Value, entry->d_name, 0) != 0)
        {
            close_directory();
            return std::unexpected(failure::unavailable);
        }
        removed = true;
        errno = 0;
    }
    const auto scan_error = errno;
    close_directory();
    if(scan_error != 0 || (removed && ::fsync(parent.Value) != 0))
    {
        return std::unexpected(failure::unavailable);
    }
    return {};
}

[[nodiscard]] auto publish_bootstrap(
    const file_descriptor& parent,
    std::string_view temporary_name) -> std::expected<bool, failure>
{
    const std::string temporary{temporary_name};
    const std::string catalog{CatalogName};
#if defined(__APPLE__)
    const auto code = ::renameatx_np(
        parent.Value,
        temporary.c_str(),
        parent.Value,
        catalog.c_str(),
        RENAME_EXCL);
#elif defined(__linux__)
    const auto code = static_cast<int>(::syscall(
        SYS_renameat2,
        parent.Value,
        temporary.c_str(),
        parent.Value,
        catalog.c_str(),
        RENAME_NOREPLACE));
#else
    const auto code = -1;
    errno = ENOTSUP;
#endif
    if(code == 0)
    {
        if(::fsync(parent.Value) != 0)
        {
            return std::unexpected(failure::unavailable);
        }
        return true;
    }
    const auto conflict = errno == EEXIST;
    if(::unlinkat(parent.Value, temporary.c_str(), 0) != 0
       || ::fsync(parent.Value) != 0)
    {
        return std::unexpected(failure::unavailable);
    }
    if(!conflict)
    {
        return std::unexpected(failure::unavailable);
    }
    return false;
}

[[nodiscard]] auto validate_database_file(
    const file_descriptor& parent,
    std::string_view name,
    bool required) -> std::expected<void, failure>
{
    const std::string owned_name{name};
    struct stat status{};
    if(::fstatat(
           parent.Value, owned_name.c_str(), &status, AT_SYMLINK_NOFOLLOW)
       != 0)
    {
        if(!required && errno == ENOENT)
        {
            return {};
        }
        return std::unexpected(failure::invalid_path);
    }
    if(!private_file(status))
    {
        return std::unexpected(failure::invalid_permissions);
    }
    return {};
}

[[nodiscard]] auto validate_database_paths(const file_descriptor& parent)
    -> std::expected<void, failure>
{
    constexpr std::array<std::string_view, 4> Sidecars{
        "epoch-catalog.sqlite3-wal",
        "epoch-catalog.sqlite3-shm",
        "epoch-catalog.sqlite3-journal",
        "epoch-catalog.sqlite3-super-journal",
    };
    if(auto main = validate_database_file(parent, CatalogName, true); !main)
    {
        return main;
    }
    for(const auto sidecar : Sidecars)
    {
        if(auto result = validate_database_file(parent, sidecar, false); !result)
        {
            return result;
        }
    }
    return {};
}

struct file_identity final
{
    dev_t Device{};
    ino_t Inode{};

    auto operator==(const file_identity&) const -> bool = default;
};

[[nodiscard]] auto identity_of(const file_descriptor& descriptor)
    -> std::expected<file_identity, failure>
{
    struct stat status{};
    if(::fstat(descriptor.Value, &status) != 0)
    {
        return std::unexpected(failure::invalid_path);
    }
    return file_identity{.Device = status.st_dev, .Inode = status.st_ino};
}

[[nodiscard]] auto identity_at(
    const file_descriptor& parent,
    std::string_view name) -> std::expected<file_identity, failure>
{
    const std::string owned_name{name};
    struct stat status{};
    if(::fstatat(
           parent.Value,
           owned_name.c_str(),
           &status,
           AT_SYMLINK_NOFOLLOW)
       != 0)
    {
        return std::unexpected(failure::invalid_path);
    }
    if(!private_file(status))
    {
        return std::unexpected(failure::invalid_permissions);
    }
    return file_identity{.Device = status.st_dev, .Inode = status.st_ino};
}

[[nodiscard]] auto sqlite_main_file_is_current(
    ::sqlite3* database,
    const file_descriptor& parent,
    const file_identity expected,
    std::string_view name = CatalogName) -> std::expected<void, failure>
{
    auto current = identity_at(parent, name);
    auto moved = 1;
    if(!current || *current != expected
       || ::sqlite3_file_control(
              database, "main", SQLITE_FCNTL_HAS_MOVED, &moved)
            != SQLITE_OK
       || moved != 0)
    {
        return std::unexpected(failure::invalid_path);
    }
    return {};
}

[[nodiscard]] auto create_bootstrap(
    const file_descriptor& parent,
    std::stop_token stop,
    const char* requested_vfs = nullptr) -> std::expected<bool, failure>
{
    std::array<std::uint8_t, 32> name_random{};
    std::array<std::uint8_t, 32> incarnation{};
    if(!random_bytes(name_random) || !random_bytes(incarnation))
    {
        return std::unexpected(failure::unavailable);
    }
    const auto temporary_name = hex_name(name_random);
    file_descriptor temporary{::openat(
        parent.Value,
        temporary_name.c_str(),
        O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600)};
    if(temporary.Value < 0)
    {
        return std::unexpected(failure::unavailable);
    }
    auto expected_temporary = identity_of(temporary);
    if(!expected_temporary)
    {
        (void)::unlinkat(parent.Value, temporary_name.c_str(), 0);
        return std::unexpected(expected_temporary.error());
    }
    const auto temporary_path = descriptor_child_path(parent, temporary_name);
    auto opened = open_sqlite(temporary_path, requested_vfs);
    if(!opened)
    {
        (void)::unlinkat(parent.Value, temporary_name.c_str(), 0);
        return std::unexpected(opened.error());
    }
    ::sqlite3* database = *opened;
    auto cleanup_database = [&database]() noexcept {
        if(database != nullptr)
        {
            (void)::sqlite3_close_v2(database);
            database = nullptr;
        }
    };
    auto configured = configure_connection(database);
    auto created = configured
        ? create_schema(database, incarnation, stop)
        : std::expected<void, failure>{std::unexpected(configured.error())};
    if(created)
    {
        created = validate_schema(database, stop);
    }
    if(created)
    {
        created = execute(database, "PRAGMA wal_checkpoint(TRUNCATE)", stop);
    }
    if(created)
    {
        created = sqlite_main_file_is_current(
            database, parent, *expected_temporary, temporary_name);
    }
    cleanup_database();
    if(!created)
    {
        (void)::unlinkat(parent.Value, temporary_name.c_str(), 0);
        return std::unexpected(created.error());
    }
    if(::fsync(temporary.Value) != 0)
    {
        (void)::unlinkat(parent.Value, temporary_name.c_str(), 0);
        return std::unexpected(failure::unavailable);
    }
    auto durable_temporary = identity_at(parent, temporary_name);
    if(!durable_temporary || *durable_temporary != *expected_temporary)
    {
        (void)::unlinkat(parent.Value, temporary_name.c_str(), 0);
        return std::unexpected(failure::invalid_path);
    }
#if defined(BPR_SQLITE_TESTING)
    if(InterruptBootstrapBeforePublication.exchange(
           false, std::memory_order_relaxed))
    {
        return std::unexpected(failure::unavailable);
    }
#endif

    return publish_bootstrap(parent, temporary_name);
}
#else
constexpr std::wstring_view CatalogNameWide = L"epoch-catalog.sqlite3";
constexpr std::wstring_view CatalogLockNameWide = L"epoch-catalog.lock";
constexpr std::size_t MaximumDirectoryEntries = 4'096U;
constexpr std::size_t MaximumWindowsPathCharacters = 32'767U;
constexpr std::size_t ReservedCatalogPathCharacters = 128U;

enum class windows_object_kind : std::uint8_t
{
    regular_file,
    directory,
};

class windows_handle final
{
public:
    windows_handle() = default;
    explicit windows_handle(const HANDLE value) noexcept : Value{value} {}
    windows_handle(const windows_handle&) = delete;
    auto operator=(const windows_handle&) -> windows_handle& = delete;
    windows_handle(windows_handle&& other) noexcept
        : Value{std::exchange(other.Value, INVALID_HANDLE_VALUE)}
    {
    }
    auto operator=(windows_handle&& other) noexcept -> windows_handle&
    {
        if(this != &other)
        {
            reset();
            Value = std::exchange(other.Value, INVALID_HANDLE_VALUE);
        }
        return *this;
    }
    ~windows_handle()
    {
        reset();
    }

    [[nodiscard]] auto valid() const noexcept -> bool
    {
        return Value != nullptr && Value != INVALID_HANDLE_VALUE;
    }

    void reset() noexcept
    {
        if(valid())
        {
            (void)::CloseHandle(Value);
        }
        Value = INVALID_HANDLE_VALUE;
    }

    HANDLE Value = INVALID_HANDLE_VALUE;
};

class local_memory final
{
public:
    local_memory() = default;
    explicit local_memory(void* value) noexcept : Value{value} {}
    local_memory(const local_memory&) = delete;
    auto operator=(const local_memory&) -> local_memory& = delete;
    ~local_memory()
    {
        if(Value != nullptr)
        {
            (void)::LocalFree(Value);
        }
    }

    void* Value = nullptr;
};

[[nodiscard]] auto windows_error_failure(
    const DWORD error,
    const failure fallback = failure::unavailable) noexcept -> failure
{
    switch(error)
    {
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
        return failure::invalid_permissions;
    case ERROR_BAD_PATHNAME:
    case ERROR_DIRECTORY:
    case ERROR_FILE_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_INVALID_NAME:
    case ERROR_PATH_NOT_FOUND:
        return failure::invalid_path;
    case ERROR_INVALID_FUNCTION:
    case ERROR_NOT_SUPPORTED:
        return failure::platform_unsupported;
    default:
        return fallback;
    }
}

[[nodiscard]] auto same_windows_path(
    const std::wstring_view left,
    const std::wstring_view right) noexcept -> bool
{
    if(left.size() != right.size()
       || left.size()
           > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }
    return ::CompareStringOrdinal(
               left.data(),
               static_cast<int>(left.size()),
               right.data(),
               static_cast<int>(right.size()),
               FALSE)
        == CSTR_EQUAL;
}

[[nodiscard]] auto final_path_of(const HANDLE handle)
    -> std::expected<std::wstring, failure>
{
    constexpr DWORD Flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    const auto required = ::GetFinalPathNameByHandleW(handle, nullptr, 0U, Flags);
    if(required == 0U
       || required >= static_cast<DWORD>(MaximumWindowsPathCharacters))
    {
        return std::unexpected(windows_error_failure(::GetLastError()));
    }
    auto buffer = std::vector<wchar_t>(
        static_cast<std::size_t>(required) + 1U, L'\0');
    const auto written = ::GetFinalPathNameByHandleW(
        handle,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        Flags);
    if(written == 0U || written >= buffer.size())
    {
        return std::unexpected(windows_error_failure(::GetLastError()));
    }
    return std::wstring{buffer.data(), static_cast<std::size_t>(written)};
}

[[nodiscard]] auto path_matches_handle(
    const HANDLE handle,
    const std::filesystem::path& expected) -> std::expected<void, failure>
{
    const auto actual = final_path_of(handle);
    if(!actual || !same_windows_path(*actual, expected.native()))
    {
        return std::unexpected(
            actual ? failure::invalid_path : actual.error());
    }
    return {};
}

class private_security final
{
public:
    private_security() = default;
    private_security(const private_security&) = default;
    auto operator=(const private_security&) -> private_security& = default;
    private_security(private_security&&) noexcept = default;
    auto operator=(private_security&&) noexcept -> private_security& = default;

    [[nodiscard]] static auto load()
        -> std::expected<private_security, failure>
    {
        windows_handle token;
        if(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token.Value)
           == 0)
        {
            return std::unexpected(
                windows_error_failure(::GetLastError()));
        }
        DWORD required = 0U;
        (void)::GetTokenInformation(
            token.Value, TokenUser, nullptr, 0U, &required);
        if(required == 0U || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            return std::unexpected(failure::unavailable);
        }
        auto information = std::vector<std::byte>(required);
        if(::GetTokenInformation(
               token.Value,
               TokenUser,
               information.data(),
               required,
               &required)
           == 0)
        {
            return std::unexpected(
                windows_error_failure(::GetLastError()));
        }
        const auto* user = reinterpret_cast<const TOKEN_USER*>(
            information.data());
        if(user->User.Sid == nullptr || ::IsValidSid(user->User.Sid) == 0)
        {
            return std::unexpected(failure::invalid_permissions);
        }
        const auto sid_bytes = ::GetLengthSid(user->User.Sid);
        if(sid_bytes == 0U)
        {
            return std::unexpected(failure::invalid_permissions);
        }
        private_security result;
        result.Sid = std::vector<std::byte>(sid_bytes);
        if(::CopySid(sid_bytes, result.Sid.data(), user->User.Sid) == 0)
        {
            return std::unexpected(failure::unavailable);
        }
        return result;
    }

    [[nodiscard]] auto sid() const noexcept -> PSID
    {
        return const_cast<std::byte*>(Sid.data());
    }

private:
    std::vector<std::byte> Sid;
};

class private_security_attributes final
{
public:
    [[nodiscard]] auto initialize(
        const private_security& security,
        const windows_object_kind kind) -> bool
    {
        const auto sid_bytes = ::GetLengthSid(security.sid());
        const auto required = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE)
            - sizeof(DWORD) + static_cast<std::size_t>(sid_bytes);
        if(required > std::numeric_limits<DWORD>::max())
        {
            return false;
        }
        Acl = std::vector<std::byte>(required);
        auto* acl = reinterpret_cast<PACL>(Acl.data());
        const auto inheritance = kind == windows_object_kind::directory
            ? static_cast<DWORD>(OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)
            : 0U;
        if(::InitializeAcl(
               acl, static_cast<DWORD>(Acl.size()), ACL_REVISION)
               == 0
           || ::AddAccessAllowedAceEx(
                  acl,
                  ACL_REVISION,
                  inheritance,
                  FILE_ALL_ACCESS,
                  security.sid())
               == 0
           || ::InitializeSecurityDescriptor(
                  &Descriptor, SECURITY_DESCRIPTOR_REVISION)
               == 0
           || ::SetSecurityDescriptorOwner(
                  &Descriptor, security.sid(), FALSE)
               == 0
           || ::SetSecurityDescriptorDacl(
                  &Descriptor, TRUE, acl, FALSE)
               == 0
           || ::SetSecurityDescriptorControl(
                  &Descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED)
               == 0)
        {
            return false;
        }
        Attributes = SECURITY_ATTRIBUTES{
            .nLength = sizeof(SECURITY_ATTRIBUTES),
            .lpSecurityDescriptor = &Descriptor,
            .bInheritHandle = FALSE,
        };
        return true;
    }

    [[nodiscard]] auto attributes() noexcept -> SECURITY_ATTRIBUTES*
    {
        return &Attributes;
    }

    [[nodiscard]] auto acl() noexcept -> PACL
    {
        return reinterpret_cast<PACL>(Acl.data());
    }

private:
    std::vector<std::byte> Acl;
    SECURITY_DESCRIPTOR Descriptor{};
    SECURITY_ATTRIBUTES Attributes{};
};

[[nodiscard]] auto validate_private_security(
    const HANDLE handle,
    const private_security& expected,
    const windows_object_kind kind,
    const bool allow_inherited = false) -> std::expected<void, failure>
{
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    const auto status = ::GetSecurityInfo(
        handle,
        SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner,
        nullptr,
        &dacl,
        nullptr,
        &descriptor);
    const auto owned_descriptor = local_memory{descriptor};
    if(status != ERROR_SUCCESS)
    {
        return std::unexpected(windows_error_failure(status));
    }
    SECURITY_DESCRIPTOR_CONTROL control = 0U;
    DWORD revision = 0U;
    ACL_SIZE_INFORMATION information{};
    if(owner == nullptr || dacl == nullptr
       || ::EqualSid(owner, expected.sid()) == 0
       || ::GetSecurityDescriptorControl(
              descriptor, &control, &revision)
            == 0
       || (control & SE_DACL_PRESENT) == 0U
       || (!allow_inherited && (control & SE_DACL_PROTECTED) == 0U)
       || ::GetAclInformation(
              dacl,
              &information,
              sizeof(information),
              AclSizeInformation)
            == 0
       || information.AceCount != 1U)
    {
        return std::unexpected(failure::invalid_permissions);
    }
    void* raw_ace = nullptr;
    if(::GetAce(dacl, 0U, &raw_ace) == 0 || raw_ace == nullptr)
    {
        return std::unexpected(failure::invalid_permissions);
    }
    const auto* ace = static_cast<const ACCESS_ALLOWED_ACE*>(raw_ace);
    const auto expected_flags = kind == windows_object_kind::directory
        ? static_cast<BYTE>(OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)
        : static_cast<BYTE>(0U);
    const auto exact_protected =
        (control & SE_DACL_PROTECTED) != 0U
        && ace->Header.AceFlags == expected_flags;
    const auto exact_inherited_file =
        kind == windows_object_kind::regular_file && allow_inherited
        && (control & SE_DACL_PROTECTED) == 0U
        && ace->Header.AceFlags == static_cast<BYTE>(INHERITED_ACE);
    if(ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE
       || ace->Mask != FILE_ALL_ACCESS
       || (!exact_protected && !exact_inherited_file)
       || ::EqualSid(
              const_cast<DWORD*>(&ace->SidStart), expected.sid())
            == 0)
    {
        return std::unexpected(failure::invalid_permissions);
    }
    return {};
}

[[nodiscard]] auto protect_inherited_private_file(
    const HANDLE handle,
    const private_security& security) -> std::expected<void, failure>
{
    if(auto private_result = validate_private_security(
           handle, security, windows_object_kind::regular_file, true);
       !private_result)
    {
        return private_result;
    }
    private_security_attributes attributes;
    if(!attributes.initialize(security, windows_object_kind::regular_file))
    {
        return std::unexpected(failure::unavailable);
    }
    const auto status = ::SetSecurityInfo(
        handle,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        attributes.acl(),
        nullptr);
    if(status != ERROR_SUCCESS)
    {
        return std::unexpected(windows_error_failure(status));
    }
    return validate_private_security(
        handle, security, windows_object_kind::regular_file);
}

struct file_identity final
{
    std::uint64_t Volume = 0U;
    std::array<std::uint8_t, 16U> Object{};
    windows_object_kind Kind = windows_object_kind::regular_file;

    auto operator==(const file_identity&) const -> bool = default;
};

[[nodiscard]] auto identity_of(
    const HANDLE handle,
    const windows_object_kind expected_kind)
    -> std::expected<file_identity, failure>
{
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    FILE_STANDARD_INFO standard{};
    FILE_ID_INFO identity{};
    if(::GetFileInformationByHandleEx(
           handle,
           FileAttributeTagInfo,
           &attributes,
           sizeof(attributes))
               == 0
       || ::GetFileInformationByHandleEx(
              handle, FileStandardInfo, &standard, sizeof(standard))
            == 0
       || ::GetFileInformationByHandleEx(
              handle, FileIdInfo, &identity, sizeof(identity))
            == 0)
    {
        return std::unexpected(
            windows_error_failure(::GetLastError(), failure::invalid_path));
    }
    const auto is_directory =
        (attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
    if((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
       || standard.DeletePending != FALSE
       || is_directory
           != (expected_kind == windows_object_kind::directory)
       || (expected_kind == windows_object_kind::regular_file
           && standard.NumberOfLinks != 1U))
    {
        return std::unexpected(failure::invalid_path);
    }
    file_identity result{
        .Volume = identity.VolumeSerialNumber,
        .Kind = expected_kind,
    };
    std::ranges::copy(identity.FileId.Identifier, result.Object.begin());
    if(result.Volume == 0U
       || std::ranges::all_of(result.Object, [](const auto value) {
              return value == 0U;
          }))
    {
        return std::unexpected(failure::invalid_path);
    }
    return result;
}

struct private_windows_object final
{
    windows_handle Handle;
    std::filesystem::path Path;
    file_identity Identity;
};

[[nodiscard]] auto open_existing_object(
    const std::filesystem::path& path,
    const windows_object_kind kind,
    const DWORD additional_access = 0U,
    const bool allow_delete_sharing = true)
    -> std::expected<windows_handle, failure>
{
    (void)kind;
    const auto flags =
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
    windows_handle handle{::CreateFileW(
        path.c_str(),
        FILE_READ_ATTRIBUTES | READ_CONTROL | additional_access,
        FILE_SHARE_READ | FILE_SHARE_WRITE
            | (allow_delete_sharing ? FILE_SHARE_DELETE : 0U),
        nullptr,
        OPEN_EXISTING,
        flags,
        nullptr)};
    if(!handle.valid())
    {
        return std::unexpected(windows_error_failure(::GetLastError()));
    }
    return handle;
}

[[nodiscard]] auto open_private_object(
    const std::filesystem::path& path,
    const windows_object_kind kind,
    const private_security& security,
    const DWORD additional_access = 0U,
    const bool protect_inherited = false,
    const bool allow_delete_sharing = true)
    -> std::expected<private_windows_object, failure>
{
    auto opened = open_existing_object(
        path, kind, additional_access, allow_delete_sharing);
    if(!opened)
    {
        return std::unexpected(opened.error());
    }
    auto identity = identity_of(opened->Value, kind);
    if(!identity)
    {
        return std::unexpected(identity.error());
    }
    if(auto matches = path_matches_handle(opened->Value, path); !matches)
    {
        return std::unexpected(matches.error());
    }
    const auto permissions = protect_inherited
        ? protect_inherited_private_file(opened->Value, security)
        : validate_private_security(opened->Value, security, kind);
    if(!permissions)
    {
        return std::unexpected(permissions.error());
    }
    return private_windows_object{
        .Handle = std::move(*opened),
        .Path = path,
        .Identity = *identity,
    };
}

[[nodiscard]] auto component_is_reserved(std::wstring_view component) noexcept
    -> bool
{
    const auto dot = component.find(L'.');
    component = component.substr(0U, dot);
    auto upper = std::array<wchar_t, 4U>{};
    if(component.size() > upper.size())
    {
        return false;
    }
    for(std::size_t index = 0U; index < component.size(); ++index)
    {
        const auto value = component[index];
        upper[index] = value >= L'a' && value <= L'z'
            ? static_cast<wchar_t>(value - L'a' + L'A')
            : value;
    }
    const auto name = std::wstring_view{upper.data(), component.size()};
    if(name == L"CON" || name == L"PRN" || name == L"AUX"
       || name == L"NUL")
    {
        return true;
    }
    return name.size() == 4U
        && (name.starts_with(L"COM") || name.starts_with(L"LPT"))
        && name[3U] >= L'1' && name[3U] <= L'9';
}

[[nodiscard]] auto valid_windows_components(
    const std::wstring_view path,
    const std::size_t maximum_component = 255U) noexcept -> bool
{
    if(path.size() < 3U || path[1U] != L':' || path[2U] != L'\\'
       || !((path[0U] >= L'A' && path[0U] <= L'Z')
            || (path[0U] >= L'a' && path[0U] <= L'z')))
    {
        return false;
    }
    std::size_t offset = 3U;
    while(offset < path.size())
    {
        const auto separator = path.find(L'\\', offset);
        const auto end = separator == std::wstring_view::npos
            ? path.size()
            : separator;
        const auto component = path.substr(offset, end - offset);
        if(component.empty() || component.size() > maximum_component
           || component == L"." || component == L".."
           || component.back() == L'.' || component.back() == L' '
           || component.find(L':') != std::wstring_view::npos
           || component_is_reserved(component))
        {
            return false;
        }
        offset = end + 1U;
    }
    return true;
}

[[nodiscard]] auto canonical_windows_root(
    const std::filesystem::path& input)
    -> std::expected<std::filesystem::path, failure>
{
    const auto raw = input.native();
    if(raw.empty() || raw.find(L'/') != std::wstring::npos
       || raw.find(L'\0') != std::wstring::npos
       || raw.size() + ReservedCatalogPathCharacters
           >= MaximumWindowsPathCharacters
       || (raw.size() > 3U && raw.back() == L'\\')
       || !valid_windows_components(raw))
    {
        return std::unexpected(failure::invalid_path);
    }
    const auto required = ::GetFullPathNameW(raw.c_str(), 0U, nullptr, nullptr);
    if(required == 0U
       || required >= static_cast<DWORD>(MaximumWindowsPathCharacters))
    {
        return std::unexpected(failure::invalid_path);
    }
    auto full = std::vector<wchar_t>(
        static_cast<std::size_t>(required) + 1U, L'\0');
    const auto written = ::GetFullPathNameW(
        raw.c_str(), static_cast<DWORD>(full.size()), full.data(), nullptr);
    if(written == 0U || written >= full.size())
    {
        return std::unexpected(failure::invalid_path);
    }
    const auto normalized =
        std::wstring_view{full.data(), static_cast<std::size_t>(written)};
    if(!same_windows_path(raw, normalized)
       || !valid_windows_components(normalized))
    {
        return std::unexpected(failure::invalid_path);
    }
    auto extended = std::wstring{L"\\\\?\\"};
    extended.append(normalized);
    if(extended.size() + ReservedCatalogPathCharacters
       >= MaximumWindowsPathCharacters)
    {
        return std::unexpected(failure::invalid_path);
    }
    return std::filesystem::path{std::move(extended)};
}

[[nodiscard]] auto validate_ancestor_chain(
    const std::filesystem::path& path)
    -> std::expected<std::vector<private_windows_object>, failure>
{
    const auto native = path.native();
    if(!native.starts_with(L"\\\\?\\") || native.size() < 7U)
    {
        return std::unexpected(failure::invalid_path);
    }
    auto candidate = std::wstring{native.substr(0U, 7U)};
    std::size_t offset = 7U;
    auto retained = std::vector<private_windows_object>{};
    retained.reserve(1U + static_cast<std::size_t>(
        std::ranges::count(native, L'\\')));
    for(;;)
    {
        auto opened = open_existing_object(
            std::filesystem::path{candidate},
            windows_object_kind::directory,
            0U,
            false);
        if(!opened)
        {
            return std::unexpected(opened.error());
        }
        auto identity = identity_of(
            opened->Value, windows_object_kind::directory);
        if(!identity)
        {
            return std::unexpected(identity.error());
        }
        if(auto matches = path_matches_handle(
               opened->Value, std::filesystem::path{candidate});
           !matches)
        {
            return std::unexpected(matches.error());
        }
        retained.push_back(private_windows_object{
            .Handle = std::move(*opened),
            .Path = std::filesystem::path{candidate},
            .Identity = *identity,
        });
        if(offset >= native.size())
        {
            return retained;
        }
        const auto separator = native.find(L'\\', offset);
        const auto end = separator == std::wstring::npos
            ? native.size()
            : separator;
        candidate.append(native.substr(offset - 1U, end - offset + 1U));
        offset = end + 1U;
    }
}

[[nodiscard]] auto open_private_directory(
    const std::filesystem::path& path,
    const private_security& security,
    const bool allow_delete_sharing = true)
    -> std::expected<private_windows_object, failure>
{
    return open_private_object(
        path,
        windows_object_kind::directory,
        security,
        FILE_LIST_DIRECTORY | FILE_TRAVERSE,
        false,
        allow_delete_sharing);
}

[[nodiscard]] auto ensure_private_child(
    const private_windows_object& parent,
    const std::wstring_view name,
    const private_security& security)
    -> std::expected<private_windows_object, failure>
{
    const auto path = parent.Path / std::wstring{name};
    private_security_attributes attributes;
    if(!attributes.initialize(security, windows_object_kind::directory))
    {
        return std::unexpected(failure::unavailable);
    }
    if(::CreateDirectoryW(path.c_str(), attributes.attributes()) == 0)
    {
        const auto error = ::GetLastError();
        if(error != ERROR_ALREADY_EXISTS)
        {
            return std::unexpected(windows_error_failure(error));
        }
    }
    auto result = open_private_directory(path, security, false);
    if(!result || result->Identity.Volume != parent.Identity.Volume)
    {
        return std::unexpected(
            result ? failure::invalid_path : result.error());
    }
    return result;
}

[[nodiscard]] auto reliable_local_filesystem(
    const private_windows_object& root) -> bool
{
    const auto native = root.Path.native();
    if(native.size() < 7U)
    {
        return false;
    }
    const std::array<wchar_t, 4U> drive_root{
        native[4U], L':', L'\\', L'\0'};
    if(::GetDriveTypeW(drive_root.data()) != DRIVE_FIXED)
    {
        return false;
    }
    DWORD serial = 0U;
    DWORD maximum_component = 0U;
    DWORD flags = 0U;
    std::array<wchar_t, 32U> filesystem{};
    if(::GetVolumeInformationByHandleW(
           root.Handle.Value,
           nullptr,
           0U,
           &serial,
           &maximum_component,
           &flags,
           filesystem.data(),
           static_cast<DWORD>(filesystem.size()))
           == 0
       || serial == 0U || maximum_component == 0U
       || (flags & FILE_PERSISTENT_ACLS) == 0U
       || (flags & FILE_CASE_PRESERVED_NAMES) == 0U
       || (flags & FILE_UNICODE_ON_DISK) == 0U
       || (flags & FILE_SUPPORTS_OPEN_BY_FILE_ID) == 0U
       || (flags & FILE_READ_ONLY_VOLUME) != 0U
       || !valid_windows_components(
           std::wstring_view{native}.substr(4U), maximum_component))
    {
        return false;
    }
    const auto name = std::wstring_view{filesystem.data()};
    return same_windows_path(name, L"NTFS")
        || same_windows_path(name, L"ReFS");
}

[[nodiscard]] auto random_bytes(std::span<std::uint8_t> output) noexcept
    -> bool
{
    std::size_t offset = 0U;
    while(offset < output.size())
    {
        const auto remaining = output.size() - offset;
        const auto chunk = static_cast<ULONG>(std::min<std::size_t>(
            remaining, std::numeric_limits<ULONG>::max()));
        if(::BCryptGenRandom(
               nullptr,
               output.data() + offset,
               chunk,
               BCRYPT_USE_SYSTEM_PREFERRED_RNG)
           != 0)
        {
            return false;
        }
        offset += static_cast<std::size_t>(chunk);
    }
    return true;
}

[[nodiscard]] auto hex_name(const std::array<std::uint8_t, 32>& random)
    -> std::wstring
{
    constexpr std::wstring_view Digits = L"0123456789abcdef";
    std::wstring result = L".epoch-catalog.bootstrap-";
    result.reserve(result.size() + random.size() * 2U + 8U);
    for(const auto byte : random)
    {
        result.push_back(Digits[byte >> 4U]);
        result.push_back(Digits[byte & 0x0FU]);
    }
    result.append(L".sqlite3");
    return result;
}

[[nodiscard]] auto bootstrap_artifact_name(
    const std::wstring_view name) noexcept -> bool
{
    constexpr std::wstring_view Prefix = L".epoch-catalog.bootstrap-";
    constexpr std::array<std::wstring_view, 4> Suffixes{
        L".sqlite3",
        L".sqlite3-wal",
        L".sqlite3-shm",
        L".sqlite3-journal",
    };
    for(const auto suffix : Suffixes)
    {
        if(name.size() != Prefix.size() + 64U + suffix.size()
           || !name.starts_with(Prefix) || !name.ends_with(suffix))
        {
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
    if(::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    const auto error = ::GetLastError();
    if(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
    {
        return true;
    }
    return std::unexpected(windows_error_failure(error));
}

[[nodiscard]] auto delete_private_file(
    const std::filesystem::path& path,
    const private_security& security,
    const bool protect_inherited = false,
    const file_identity* expected_identity = nullptr)
    -> std::expected<void, failure>
{
    auto missing = path_missing(path);
    if(!missing)
    {
        return std::unexpected(missing.error());
    }
    if(*missing)
    {
        return {};
    }
    auto opened = open_private_object(
        path,
        windows_object_kind::regular_file,
        security,
        DELETE | (protect_inherited ? WRITE_DAC : 0U),
        protect_inherited);
    if(!opened)
    {
        return std::unexpected(opened.error());
    }
    if(expected_identity != nullptr
       && opened->Identity != *expected_identity)
    {
        return std::unexpected(failure::invalid_path);
    }
    FILE_DISPOSITION_INFO disposition{.DeleteFile = TRUE};
    if(::SetFileInformationByHandle(
           opened->Handle.Value,
           FileDispositionInfo,
           &disposition,
           sizeof(disposition))
       == 0)
    {
        return std::unexpected(windows_error_failure(::GetLastError()));
    }
    opened->Handle.reset();
    missing = path_missing(path);
    if(!missing || !*missing)
    {
        return std::unexpected(
            missing ? failure::unavailable : missing.error());
    }
    return {};
}

class find_handle final
{
public:
    explicit find_handle(const HANDLE value) noexcept : Value{value} {}
    find_handle(const find_handle&) = delete;
    auto operator=(const find_handle&) -> find_handle& = delete;
    ~find_handle()
    {
        if(Value != INVALID_HANDLE_VALUE)
        {
            (void)::FindClose(Value);
        }
    }
    HANDLE Value = INVALID_HANDLE_VALUE;
};

[[nodiscard]] auto cleanup_bootstrap_orphans(
    const private_windows_object& parent,
    const private_security& security) -> std::expected<void, failure>
{
    const auto expected_parent = parent.Identity;
    WIN32_FIND_DATAW entry{};
    const auto search = parent.Path / L"*";
    find_handle find{::FindFirstFileExW(
        search.c_str(),
        FindExInfoBasic,
        &entry,
        FindExSearchNameMatch,
        nullptr,
        0U)};
    if(find.Value == INVALID_HANDLE_VALUE)
    {
        const auto error = ::GetLastError();
        return error == ERROR_FILE_NOT_FOUND
            ? std::expected<void, failure>{}
            : std::expected<void, failure>{
                  std::unexpected(windows_error_failure(error))};
    }
    std::size_t entries = 0U;
    for(;;)
    {
        if(++entries > MaximumDirectoryEntries)
        {
            return std::unexpected(failure::unavailable);
        }
        const auto name = std::wstring_view{entry.cFileName};
        if(bootstrap_artifact_name(name))
        {
            if(auto removed = delete_private_file(
                   parent.Path / std::wstring{name}, security, true);
               !removed)
            {
                return removed;
            }
        }
        if(::FindNextFileW(find.Value, &entry) == 0)
        {
            const auto error = ::GetLastError();
            if(error != ERROR_NO_MORE_FILES)
            {
                return std::unexpected(windows_error_failure(error));
            }
            break;
        }
    }
    auto live = open_private_directory(parent.Path, security);
    if(!live || live->Identity != expected_parent)
    {
        return std::unexpected(
            live ? failure::invalid_path : live.error());
    }
    return {};
}

class catalog_lock final
{
public:
    catalog_lock() = default;
    catalog_lock(const catalog_lock&) = delete;
    auto operator=(const catalog_lock&) -> catalog_lock& = delete;
    catalog_lock(catalog_lock&& other) noexcept
        : Handle{std::move(other.Handle)},
          Locked{std::exchange(other.Locked, false)}
    {
    }
    ~catalog_lock()
    {
        if(Locked && Handle.valid())
        {
            OVERLAPPED offset{};
            (void)::UnlockFileEx(Handle.Value, 0U, 1U, 0U, &offset);
        }
    }

    [[nodiscard]] static auto acquire(
        const private_windows_object& parent,
        const private_security& security,
        const std::stop_token stop)
        -> std::expected<catalog_lock, failure>
    {
        const auto path = parent.Path / std::wstring{CatalogLockNameWide};
        private_security_attributes attributes;
        if(!attributes.initialize(security, windows_object_kind::regular_file))
        {
            return std::unexpected(failure::unavailable);
        }
        windows_handle handle{::CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            attributes.attributes(),
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
                | FILE_FLAG_WRITE_THROUGH,
            nullptr)};
        if(!handle.valid())
        {
            const auto error = ::GetLastError();
            if(error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
            {
                return std::unexpected(windows_error_failure(error));
            }
            windows_handle existing{::CreateFileW(
                path.c_str(),
                GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
                    | FILE_FLAG_WRITE_THROUGH,
                nullptr)};
            if(!existing.valid())
            {
                return std::unexpected(
                    windows_error_failure(::GetLastError()));
            }
            handle = std::move(existing);
        }
        auto expected_identity = identity_of(
            handle.Value, windows_object_kind::regular_file);
        if(!expected_identity)
        {
            return std::unexpected(expected_identity.error());
        }
        if(auto permissions = validate_private_security(
               handle.Value, security, windows_object_kind::regular_file);
           !permissions)
        {
            return std::unexpected(permissions.error());
        }
        if(auto matches = path_matches_handle(handle.Value, path); !matches)
        {
            return std::unexpected(matches.error());
        }
        for(;;)
        {
            if(stop.stop_requested())
            {
                return std::unexpected(failure::cancelled);
            }
            OVERLAPPED offset{};
            if(::LockFileEx(
                   handle.Value,
                   LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                   0U,
                   1U,
                   0U,
                   &offset)
               != 0)
            {
                break;
            }
            const auto error = ::GetLastError();
            if(error != ERROR_LOCK_VIOLATION && error != ERROR_IO_PENDING)
            {
                return std::unexpected(windows_error_failure(error));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        auto named = open_private_object(
            path, windows_object_kind::regular_file, security);
        if(!named || named->Identity != *expected_identity)
        {
            OVERLAPPED offset{};
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
    bool Locked = false;
};

[[nodiscard]] auto validate_database_file(
    const private_windows_object& parent,
    const std::wstring_view name,
    const bool required,
    const private_security& security,
    const bool protect_inherited = false) -> std::expected<void, failure>
{
    const auto path = parent.Path / std::wstring{name};
    auto missing = path_missing(path);
    if(!missing)
    {
        return std::unexpected(missing.error());
    }
    if(*missing)
    {
        return required
            ? std::expected<void, failure>{
                  std::unexpected(failure::invalid_path)}
            : std::expected<void, failure>{};
    }
    auto opened = open_private_object(
        path,
        windows_object_kind::regular_file,
        security,
        protect_inherited ? WRITE_DAC : 0U,
        protect_inherited);
    if(!opened)
    {
        return std::unexpected(opened.error());
    }
    if(opened->Identity.Volume != parent.Identity.Volume)
    {
        return std::unexpected(failure::invalid_path);
    }
    return {};
}

[[nodiscard]] auto validate_database_paths(
    const private_windows_object& parent,
    const private_security& security,
    const bool protect_inherited = false) -> std::expected<void, failure>
{
    constexpr std::array<std::wstring_view, 4> Sidecars{
        L"epoch-catalog.sqlite3-wal",
        L"epoch-catalog.sqlite3-shm",
        L"epoch-catalog.sqlite3-journal",
        L"epoch-catalog.sqlite3-super-journal",
    };
    if(auto main = validate_database_file(
           parent, CatalogNameWide, true, security);
       !main)
    {
        return main;
    }
    for(const auto sidecar : Sidecars)
    {
        if(auto result = validate_database_file(
               parent,
               sidecar,
               false,
               security,
               protect_inherited);
           !result)
        {
            return result;
        }
    }
    return {};
}

[[nodiscard]] auto identity_at(
    const private_windows_object& parent,
    const std::wstring_view name,
    const private_security& security)
    -> std::expected<file_identity, failure>
{
    auto opened = open_private_object(
        parent.Path / std::wstring{name},
        windows_object_kind::regular_file,
        security);
    if(!opened)
    {
        return std::unexpected(opened.error());
    }
    if(opened->Identity.Volume != parent.Identity.Volume)
    {
        return std::unexpected(failure::invalid_path);
    }
    return opened->Identity;
}

enum class sqlite_catalog_leaf : std::uint8_t
{
    main,
    wal,
    shm,
    journal,
};

class secure_windows_vfs final
{
    struct guarded_file final
    {
        ::sqlite3_file Public;
        ::sqlite3_file* Base;
        secure_windows_vfs* Owner;
    };

public:
    secure_windows_vfs(const secure_windows_vfs&) = delete;
    auto operator=(const secure_windows_vfs&)
        -> secure_windows_vfs& = delete;
    ~secure_windows_vfs()
    {
        if(Registered)
        {
            (void)::sqlite3_vfs_unregister(&Interface);
        }
    }

    [[nodiscard]] static auto create(
        const private_windows_object& parent,
        const private_security& security,
        const std::filesystem::path& main_path)
        -> std::expected<std::unique_ptr<secure_windows_vfs>, failure>
    {
        if(sqlite_runtime().Result != SQLITE_OK)
        {
            return std::unexpected(failure::unavailable);
        }
        auto* base = ::sqlite3_vfs_find("win32-longpath");
        if(base == nullptr || base->xOpen == nullptr
           || base->xDelete == nullptr || base->xAccess == nullptr
           || base->xFullPathname == nullptr)
        {
            return std::unexpected(failure::platform_unsupported);
        }
        auto random = std::array<std::uint8_t, 16U>{};
        if(!random_bytes(random))
        {
            return std::unexpected(failure::unavailable);
        }
        auto name = std::string{"bpr-private-catalog-"};
        constexpr std::string_view Digits = "0123456789abcdef";
        name.reserve(name.size() + random.size() * 2U);
        for(const auto byte : random)
        {
            name.push_back(Digits[byte >> 4U]);
            name.push_back(Digits[byte & 0x0FU]);
        }
        auto result = std::unique_ptr<secure_windows_vfs>{
            new secure_windows_vfs{
                base, parent, security, main_path, std::move(name)}};
        if(::sqlite3_vfs_register(&result->Interface, 0) != SQLITE_OK)
        {
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
            ? std::optional<failure>{}
            : std::optional<failure>{static_cast<failure>(value)};
    }

    [[nodiscard]] auto failure_or(const failure fallback) const noexcept
        -> failure
    {
        const auto recorded = recorded_failure();
        return recorded ? *recorded : fallback;
    }

private:
    secure_windows_vfs(
        ::sqlite3_vfs* base,
        const private_windows_object& parent,
        const private_security& security,
        const std::filesystem::path& main_path,
        std::string name)
        : Base{base},
          Security{security},
          ParentIdentity{parent.Identity},
          MainPath{main_path},
          Name{std::move(name)}
    {
        const auto encoded = MainPath.u8string();
        MainName.assign(
            reinterpret_cast<const char*>(encoded.data()), encoded.size());
        WalName = MainName + "-wal";
        ShmName = MainName + "-shm";
        JournalName = MainName + "-journal";
        Interface = *Base;
        Interface.szOsFile = static_cast<int>(sizeof(guarded_file));
        Interface.pNext = nullptr;
        Interface.zName = Name.c_str();
        Interface.pAppData = this;
        Interface.xOpen = vfs_open;
        Interface.xDelete = vfs_delete;
        Interface.xAccess = vfs_access;
        Interface.xFullPathname = vfs_full_pathname;
        Interface.xDlOpen = Base->xDlOpen != nullptr ? vfs_dl_open : nullptr;
        Interface.xDlError =
            Base->xDlError != nullptr ? vfs_dl_error : nullptr;
        Interface.xDlSym = Base->xDlSym != nullptr ? vfs_dl_sym : nullptr;
        Interface.xDlClose =
            Base->xDlClose != nullptr ? vfs_dl_close : nullptr;
        Interface.xRandomness = vfs_randomness;
        Interface.xSleep = vfs_sleep;
        Interface.xCurrentTime = vfs_current_time;
        Interface.xGetLastError = vfs_get_last_error;
        Interface.xCurrentTimeInt64 = Base->xCurrentTimeInt64 != nullptr
            ? vfs_current_time_int64
            : nullptr;
        Interface.xSetSystemCall = Base->xSetSystemCall != nullptr
            ? vfs_set_system_call
            : nullptr;
        Interface.xGetSystemCall = Base->xGetSystemCall != nullptr
            ? vfs_get_system_call
            : nullptr;
        Interface.xNextSystemCall = Base->xNextSystemCall != nullptr
            ? vfs_next_system_call
            : nullptr;
    }

    [[nodiscard]] auto path_for(const sqlite_catalog_leaf leaf) const
        -> std::filesystem::path
    {
        switch(leaf)
        {
        case sqlite_catalog_leaf::main:
            return MainPath;
        case sqlite_catalog_leaf::wal:
            return std::filesystem::path{MainPath.native() + L"-wal"};
        case sqlite_catalog_leaf::shm:
            return std::filesystem::path{MainPath.native() + L"-shm"};
        case sqlite_catalog_leaf::journal:
            return std::filesystem::path{MainPath.native() + L"-journal"};
        }
        return {};
    }

    [[nodiscard]] static auto leaf_index(
        const sqlite_catalog_leaf leaf) noexcept -> std::size_t
    {
        return static_cast<std::size_t>(leaf);
    }

    [[nodiscard]] auto remember_identity(
        const sqlite_catalog_leaf leaf,
        const file_identity& identity) noexcept -> bool
    {
        auto& expected = ExpectedIdentities[leaf_index(leaf)];
        if(expected && *expected != identity)
        {
            return false;
        }
        expected = identity;
        return true;
    }

    [[nodiscard]] auto leaf_for_name(const char* value) const noexcept
        -> std::optional<sqlite_catalog_leaf>
    {
        if(value == nullptr)
        {
            return std::nullopt;
        }
        const auto name = std::string_view{value};
        if(name == MainName)
        {
            return sqlite_catalog_leaf::main;
        }
        if(name == WalName)
        {
            return sqlite_catalog_leaf::wal;
        }
        if(name == ShmName)
        {
            return sqlite_catalog_leaf::shm;
        }
        if(name == JournalName)
        {
            return sqlite_catalog_leaf::journal;
        }
        return std::nullopt;
    }

    [[nodiscard]] static auto leaf_for_open_flags(const int flags) noexcept
        -> std::optional<sqlite_catalog_leaf>
    {
        if((flags & SQLITE_OPEN_MAIN_DB) != 0)
        {
            return sqlite_catalog_leaf::main;
        }
        if((flags & SQLITE_OPEN_WAL) != 0)
        {
            return sqlite_catalog_leaf::wal;
        }
        if((flags & SQLITE_OPEN_MAIN_JOURNAL) != 0)
        {
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
        const int unavailable_code = SQLITE_IOERR) noexcept -> int
    {
        switch(value)
        {
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
        const bool create_if_missing)
        -> std::expected<private_windows_object, failure>
    {
        const auto path = path_for(leaf);
        auto missing = path_missing(path);
        if(!missing)
        {
            return std::unexpected(missing.error());
        }
        if(*missing && create_if_missing)
        {
            private_security_attributes attributes;
            if(!attributes.initialize(
                   Security, windows_object_kind::regular_file))
            {
                return std::unexpected(failure::unavailable);
            }
            windows_handle created{::CreateFileW(
                path.c_str(),
                GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                attributes.attributes(),
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
                    | FILE_FLAG_WRITE_THROUGH,
                nullptr)};
            if(created.valid())
            {
                auto identity = identity_of(
                    created.Value, windows_object_kind::regular_file);
                if(!identity || identity->Volume != ParentIdentity.Volume)
                {
                    return std::unexpected(
                        identity ? failure::invalid_path : identity.error());
                }
                if(auto permissions = validate_private_security(
                       created.Value,
                       Security,
                       windows_object_kind::regular_file);
                   !permissions)
                {
                    return std::unexpected(permissions.error());
                }
                if(auto matches = path_matches_handle(created.Value, path);
                   !matches)
                {
                    return std::unexpected(matches.error());
                }
                return private_windows_object{
                    .Handle = std::move(created),
                    .Path = path,
                    .Identity = *identity,
                };
            }
            const auto error = ::GetLastError();
            if(error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
            {
                return std::unexpected(windows_error_failure(error));
            }
        }
        else if(*missing)
        {
            return std::unexpected(failure::invalid_path);
        }
        auto opened = open_private_object(
            path,
            windows_object_kind::regular_file,
            Security,
            0U,
            false,
            false);
        if(!opened)
        {
            return std::unexpected(opened.error());
        }
        if(opened->Identity.Volume != ParentIdentity.Volume)
        {
            return std::unexpected(failure::invalid_path);
        }
        return opened;
    }

    [[nodiscard]] auto delete_leaf(const sqlite_catalog_leaf leaf)
        -> std::expected<void, failure>
    {
        if(leaf == sqlite_catalog_leaf::main)
        {
            return std::unexpected(failure::invalid_path);
        }
        auto& expected = ExpectedIdentities[leaf_index(leaf)];
        if(!expected)
        {
            auto current = open_guard(leaf, false);
            if(!current)
            {
                auto missing = path_missing(path_for(leaf));
                if(missing && *missing)
                {
                    return {};
                }
                return std::unexpected(current.error());
            }
            expected = current->Identity;
        }
        auto removed = delete_private_file(
            path_for(leaf), Security, false, &*expected);
        if(removed)
        {
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
        auto result = SQLITE_OK;
        if(guarded->Base != nullptr)
        {
            if(guarded->Base->pMethods != nullptr)
            {
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
        const void* input,
        int bytes,
        sqlite3_int64 offset) -> int
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
        if(operation == SQLITE_FCNTL_VFS_POINTER && argument != nullptr)
        {
            *static_cast<::sqlite3_vfs**>(argument) =
                &guarded->Owner->Interface;
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
        int page,
        int page_size,
        int extend,
        void volatile** output) -> int
    {
        auto* guarded = file(value);
        auto guard = guarded->Owner->open_guard(
            sqlite_catalog_leaf::shm, true);
        if(!guard)
        {
            guarded->Owner->record(guard.error());
            return sqlite_failure_code(guard.error(), SQLITE_IOERR_SHMMAP);
        }
        if(!guarded->Owner->remember_identity(
               sqlite_catalog_leaf::shm, guard->Identity))
        {
            guarded->Owner->record(failure::invalid_path);
            return SQLITE_IOERR_SHMMAP;
        }
        auto* inner = guarded->Base;
        if(inner->pMethods->iVersion < 2
           || inner->pMethods->xShmMap == nullptr)
        {
            guarded->Owner->record(failure::platform_unsupported);
            return SQLITE_IOERR_SHMMAP;
        }
        const auto result = inner->pMethods->xShmMap(
            inner, page, page_size, extend, output);
        if(result != SQLITE_OK)
        {
            return result;
        }
        auto current = guarded->Owner->open_guard(
            sqlite_catalog_leaf::shm, false);
        if(!current || current->Identity != guard->Identity)
        {
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
        auto* guarded = file(value);
        auto* inner = guarded->Base;
        const auto result = inner->pMethods->xShmUnmap(inner, 0);
        if(result != SQLITE_OK || delete_file == 0)
        {
            return result;
        }
        if(auto removed = guarded->Owner->delete_leaf(
               sqlite_catalog_leaf::shm);
           !removed)
        {
            guarded->Owner->record(removed.error());
            return sqlite_failure_code(
                removed.error(), SQLITE_IOERR_SHMOPEN);
        }
        return SQLITE_OK;
    }

    static auto io_fetch(
        ::sqlite3_file* value,
        sqlite3_int64 offset,
        int amount,
        void** output) -> int
    {
        auto* inner = base(value);
        if(inner->pMethods->iVersion >= 3
           && inner->pMethods->xFetch != nullptr)
        {
            return inner->pMethods->xFetch(inner, offset, amount, output);
        }
        if(output != nullptr)
        {
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
        .iVersion = 3,
        .xClose = io_close,
        .xRead = io_read,
        .xWrite = io_write,
        .xTruncate = io_truncate,
        .xSync = io_sync,
        .xFileSize = io_file_size,
        .xLock = io_lock,
        .xUnlock = io_unlock,
        .xCheckReservedLock = io_check_reserved_lock,
        .xFileControl = io_file_control,
        .xSectorSize = io_sector_size,
        .xDeviceCharacteristics = io_device_characteristics,
        .xShmMap = io_shm_map,
        .xShmLock = io_shm_lock,
        .xShmBarrier = io_shm_barrier,
        .xShmUnmap = io_shm_unmap,
        .xFetch = io_fetch,
        .xUnfetch = io_unfetch,
    };

    static auto vfs_open(
        ::sqlite3_vfs* vfs,
        sqlite3_filename name,
        ::sqlite3_file* output,
        int flags,
        int* output_flags) -> int
    {
        auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
        auto* guarded = reinterpret_cast<guarded_file*>(output);
        std::memset(guarded, 0, sizeof(*guarded));
        guarded->Owner = owner;
        const auto named_leaf = owner->leaf_for_name(name);
        const auto flagged_leaf = leaf_for_open_flags(flags);
        if(!named_leaf || !flagged_leaf || *named_leaf != *flagged_leaf
           || (flags & SQLITE_OPEN_EXCLUSIVE) != 0)
        {
            owner->record(failure::invalid_path);
            return SQLITE_CANTOPEN;
        }
        auto guard = owner->open_guard(
            *named_leaf, (flags & SQLITE_OPEN_CREATE) != 0);
        if(!guard)
        {
            owner->record(guard.error());
            return sqlite_failure_code(guard.error());
        }
        if(!owner->remember_identity(*named_leaf, guard->Identity))
        {
            owner->record(failure::invalid_path);
            return SQLITE_CANTOPEN;
        }
        guarded->Base = static_cast<::sqlite3_file*>(
            ::sqlite3_malloc64(static_cast<sqlite3_uint64>(
                owner->Base->szOsFile)));
        if(guarded->Base == nullptr)
        {
            owner->record(failure::unavailable);
            return SQLITE_NOMEM;
        }
        std::memset(
            guarded->Base,
            0,
            static_cast<std::size_t>(owner->Base->szOsFile));
        const auto result = owner->Base->xOpen(
            owner->Base, name, guarded->Base, flags, output_flags);
        if(result != SQLITE_OK || guarded->Base->pMethods == nullptr)
        {
            if(guarded->Base->pMethods != nullptr)
            {
                (void)guarded->Base->pMethods->xClose(guarded->Base);
            }
            ::sqlite3_free(guarded->Base);
            guarded->Base = nullptr;
            return result != SQLITE_OK ? result : SQLITE_CANTOPEN;
        }
        HANDLE native = INVALID_HANDLE_VALUE;
        const auto controlled = guarded->Base->pMethods->xFileControl(
            guarded->Base, SQLITE_FCNTL_WIN32_GET_HANDLE, &native);
        auto identity = controlled == SQLITE_OK && native != nullptr
                && native != INVALID_HANDLE_VALUE
            ? identity_of(native, windows_object_kind::regular_file)
            : std::expected<file_identity, failure>{
                  std::unexpected(failure::invalid_path)};
        auto permissions = identity
            ? validate_private_security(
                  native, owner->Security, windows_object_kind::regular_file)
            : std::expected<void, failure>{
                  std::unexpected(identity.error())};
        auto matches = permissions
            ? path_matches_handle(native, owner->path_for(*named_leaf))
            : std::expected<void, failure>{
                  std::unexpected(permissions.error())};
        if(!identity || *identity != guard->Identity || !permissions
           || !matches)
        {
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
        auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
        const auto leaf = owner->leaf_for_name(name);
        if(!leaf || *leaf == sqlite_catalog_leaf::main)
        {
            owner->record(failure::invalid_path);
            return SQLITE_IOERR_DELETE;
        }
        auto removed = owner->delete_leaf(*leaf);
        if(!removed)
        {
            owner->record(removed.error());
            return sqlite_failure_code(removed.error(), SQLITE_IOERR_DELETE);
        }
        return SQLITE_OK;
    }

    static auto vfs_access(
        ::sqlite3_vfs* vfs,
        const char* name,
        int flags,
        int* output) -> int
    {
        auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
        if(output == nullptr
           || (flags != SQLITE_ACCESS_EXISTS
               && flags != SQLITE_ACCESS_READ
               && flags != SQLITE_ACCESS_READWRITE))
        {
            owner->record(failure::invalid_path);
            return SQLITE_IOERR_ACCESS;
        }
        const auto leaf = owner->leaf_for_name(name);
        if(!leaf)
        {
            owner->record(failure::invalid_path);
            return SQLITE_IOERR_ACCESS;
        }
        auto missing = path_missing(owner->path_for(*leaf));
        if(!missing)
        {
            owner->record(missing.error());
            return sqlite_failure_code(missing.error(), SQLITE_IOERR_ACCESS);
        }
        if(*missing)
        {
            *output = 0;
            return SQLITE_OK;
        }
        auto guard = owner->open_guard(*leaf, false);
        if(!guard)
        {
            owner->record(guard.error());
            return sqlite_failure_code(guard.error(), SQLITE_IOERR_ACCESS);
        }
        if(!owner->remember_identity(*leaf, guard->Identity))
        {
            owner->record(failure::invalid_path);
            return SQLITE_IOERR_ACCESS;
        }
        *output = 1;
        return SQLITE_OK;
    }

    static auto vfs_full_pathname(
        ::sqlite3_vfs* vfs,
        const char* name,
        int output_bytes,
        char* output) -> int
    {
        auto* owner = static_cast<secure_windows_vfs*>(vfs->pAppData);
        const auto leaf = owner->leaf_for_name(name);
        if(!leaf || output == nullptr || output_bytes <= 0)
        {
            owner->record(failure::invalid_path);
            return SQLITE_CANTOPEN;
        }
        const auto result = owner->Base->xFullPathname(
            owner->Base, name, output_bytes, output);
        if(result != SQLITE_OK || owner->leaf_for_name(output) != leaf)
        {
            owner->record(failure::invalid_path);
            if(output_bytes > 0)
            {
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
        ::sqlite3_vfs* vfs,
        const char* name,
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

    ::sqlite3_vfs* Base = nullptr;
    ::sqlite3_vfs Interface{};
    private_security Security;
    file_identity ParentIdentity;
    std::filesystem::path MainPath;
    std::string MainName;
    std::string WalName;
    std::string ShmName;
    std::string JournalName;
    std::string Name;
    std::array<std::optional<file_identity>, 4U> ExpectedIdentities{};
    std::atomic_int Failure{-1};
    bool Registered = false;
};

[[nodiscard]] auto sqlite_main_file_is_current(
    ::sqlite3* database,
    const private_windows_object& parent,
    const file_identity& expected,
    const private_security& security,
    const std::wstring_view name = CatalogNameWide)
    -> std::expected<void, failure>
{
    auto current = identity_at(parent, name, security);
    HANDLE sqlite_handle = INVALID_HANDLE_VALUE;
    if(!current || *current != expected
       || ::sqlite3_file_control(
              database,
              "main",
              SQLITE_FCNTL_WIN32_GET_HANDLE,
              &sqlite_handle)
            != SQLITE_OK
       || sqlite_handle == nullptr || sqlite_handle == INVALID_HANDLE_VALUE)
    {
        return std::unexpected(failure::invalid_path);
    }
    auto sqlite_identity = identity_of(
        sqlite_handle, windows_object_kind::regular_file);
    if(!sqlite_identity || *sqlite_identity != expected)
    {
        return std::unexpected(
            sqlite_identity ? failure::invalid_path : sqlite_identity.error());
    }
    if(auto permissions = validate_private_security(
           sqlite_handle, security, windows_object_kind::regular_file);
       !permissions)
    {
        return std::unexpected(permissions.error());
    }
    return path_matches_handle(
        sqlite_handle, parent.Path / std::wstring{name});
}

[[nodiscard]] auto create_private_file(
    const std::filesystem::path& path,
    const private_security& security)
    -> std::expected<private_windows_object, failure>
{
    private_security_attributes attributes;
    if(!attributes.initialize(security, windows_object_kind::regular_file))
    {
        return std::unexpected(failure::unavailable);
    }
    windows_handle handle{::CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE | READ_CONTROL,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        attributes.attributes(),
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT
            | FILE_FLAG_WRITE_THROUGH,
        nullptr)};
    if(!handle.valid())
    {
        return std::unexpected(windows_error_failure(::GetLastError()));
    }
    auto identity = identity_of(
        handle.Value, windows_object_kind::regular_file);
    if(!identity)
    {
        return std::unexpected(identity.error());
    }
    if(auto permissions = validate_private_security(
           handle.Value, security, windows_object_kind::regular_file);
       !permissions)
    {
        return std::unexpected(permissions.error());
    }
    if(auto matches = path_matches_handle(handle.Value, path); !matches)
    {
        return std::unexpected(matches.error());
    }
    return private_windows_object{
        .Handle = std::move(handle),
        .Path = path,
        .Identity = *identity,
    };
}

[[nodiscard]] auto bootstrap_sidecars_absent(
    const private_windows_object& parent,
    const std::wstring_view temporary_name) -> std::expected<void, failure>
{
    constexpr std::array<std::wstring_view, 3> Suffixes{
        L"-wal", L"-shm", L"-journal"};
    for(const auto suffix : Suffixes)
    {
        auto name = std::wstring{temporary_name};
        name.append(suffix);
        auto missing = path_missing(parent.Path / name);
        if(!missing || !*missing)
        {
            return std::unexpected(
                missing ? failure::unavailable : missing.error());
        }
    }
    return {};
}

[[nodiscard]] auto publish_bootstrap(
    const private_windows_object& parent,
    const std::wstring_view temporary_name,
    const file_identity& expected,
    const private_security& security) -> std::expected<bool, failure>
{
    const auto source = parent.Path / std::wstring{temporary_name};
    const auto destination = parent.Path / std::wstring{CatalogNameWide};
    auto opened = open_private_object(
        source,
        windows_object_kind::regular_file,
        security,
        GENERIC_READ | GENERIC_WRITE | DELETE,
        false,
        false);
    if(!opened)
    {
        return std::unexpected(opened.error());
    }
    if(opened->Identity != expected
       || ::FlushFileBuffers(opened->Handle.Value) == 0)
    {
        return std::unexpected(
            opened->Identity != expected
                ? failure::invalid_path
                : windows_error_failure(::GetLastError()));
    }
    const auto name_bytes = CatalogNameWide.size() * sizeof(wchar_t);
    const auto structure_bytes = offsetof(FILE_RENAME_INFO, FileName)
        + name_bytes;
    if(name_bytes > std::numeric_limits<DWORD>::max()
       || structure_bytes > std::numeric_limits<DWORD>::max())
    {
        return std::unexpected(failure::invalid_path);
    }
    auto storage = std::vector<std::byte>(structure_bytes);
    auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
    rename->ReplaceIfExists = FALSE;
    rename->RootDirectory = parent.Handle.Value;
    rename->FileNameLength = static_cast<DWORD>(name_bytes);
    std::memcpy(rename->FileName, CatalogNameWide.data(), name_bytes);
    if(::SetFileInformationByHandle(
           opened->Handle.Value,
           FileRenameInfo,
           rename,
           static_cast<DWORD>(storage.size()))
       == 0)
    {
        const auto error = ::GetLastError();
        FILE_DISPOSITION_INFO disposition{.DeleteFile = TRUE};
        if(::SetFileInformationByHandle(
               opened->Handle.Value,
               FileDispositionInfo,
               &disposition,
               sizeof(disposition))
           == 0)
        {
            return std::unexpected(windows_error_failure(::GetLastError()));
        }
        return error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS
            ? std::expected<bool, failure>{false}
            : std::expected<bool, failure>{
                  std::unexpected(windows_error_failure(error))};
    }
    if(auto identity = identity_of(
           opened->Handle.Value, windows_object_kind::regular_file);
       !identity || *identity != expected)
    {
        return std::unexpected(
            identity ? failure::invalid_path : identity.error());
    }
    if(auto matches = path_matches_handle(opened->Handle.Value, destination);
       !matches)
    {
        return std::unexpected(matches.error());
    }
    if(::FlushFileBuffers(opened->Handle.Value) == 0)
    {
        return std::unexpected(windows_error_failure(::GetLastError()));
    }
    auto published = identity_at(parent, CatalogNameWide, security);
    if(!published || *published != expected)
    {
        return std::unexpected(
            published ? failure::invalid_path : published.error());
    }
    return true;
}

[[nodiscard]] auto create_bootstrap(
    const private_windows_object& parent,
    const private_security& security,
    const std::stop_token stop) -> std::expected<bool, failure>
{
    std::array<std::uint8_t, 32> name_random{};
    std::array<std::uint8_t, 32> incarnation{};
    if(!random_bytes(name_random) || !random_bytes(incarnation))
    {
        return std::unexpected(failure::unavailable);
    }
    const auto temporary_name = hex_name(name_random);
    auto temporary = create_private_file(
        parent.Path / temporary_name, security);
    if(!temporary)
    {
        return std::unexpected(temporary.error());
    }
    const auto expected_temporary = temporary->Identity;
    auto private_vfs = secure_windows_vfs::create(
        parent, security, temporary->Path);
    if(!private_vfs)
    {
        temporary->Handle.reset();
        (void)cleanup_bootstrap_orphans(parent, security);
        return std::unexpected(private_vfs.error());
    }
    auto opened = open_sqlite(temporary->Path, (*private_vfs)->name());
    if(!opened)
    {
        temporary->Handle.reset();
        (void)cleanup_bootstrap_orphans(parent, security);
        return std::unexpected((*private_vfs)->failure_or(opened.error()));
    }
    ::sqlite3* database = *opened;
    auto cleanup_database = [&database]() noexcept {
        if(database != nullptr)
        {
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
    if(created)
    {
        created = configure_connection(database);
    }
    if(created)
    {
        created = create_schema(database, incarnation, stop);
    }
    if(created)
    {
        created = validate_schema(database, stop);
    }
    if(created)
    {
        created = execute(database, "PRAGMA wal_checkpoint(TRUNCATE)", stop);
    }
    if(created)
    {
        created = sqlite_main_file_is_current(
            database,
            parent,
            expected_temporary,
            security,
            temporary_name);
    }
    cleanup_database();
    if(const auto vfs_failure = (*private_vfs)->recorded_failure();
       created && vfs_failure)
    {
        created = std::unexpected(*vfs_failure);
    }
    if(created)
    {
        created = bootstrap_sidecars_absent(parent, temporary_name);
    }
    if(created && ::FlushFileBuffers(temporary->Handle.Value) == 0)
    {
        created = std::unexpected(failure::unavailable);
    }
    if(created)
    {
        auto durable = identity_at(parent, temporary_name, security);
        if(!durable || *durable != expected_temporary)
        {
            created = std::unexpected(
                durable ? failure::invalid_path : durable.error());
        }
    }
    if(!created)
    {
        temporary->Handle.reset();
        if(auto removed = cleanup_bootstrap_orphans(parent, security); !removed)
        {
            return std::unexpected(removed.error());
        }
        return std::unexpected(created.error());
    }
#if defined(BPR_SQLITE_TESTING)
    if(InterruptBootstrapBeforePublication.exchange(
           false, std::memory_order_relaxed))
    {
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

struct catalog_database_platform_state final
{
#if defined(_WIN32)
    std::vector<private_windows_object> Directories;
    std::unique_ptr<secure_windows_vfs> Vfs;
#elif defined(__linux__)
    std::vector<file_descriptor> Directories;
    std::unique_ptr<retained_linux_vfs> Vfs;
#endif
};

auto classify_sqlite_result(const int result_code) noexcept
    -> sqlite_result_classification
{
    switch(result_code & 0xFF)
    {
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
namespace testing
{
void interrupt_next_bootstrap_before_publication() noexcept
{
    InterruptBootstrapBeforePublication.store(true, std::memory_order_relaxed);
}

auto classify_sqlite_result_code(const int result_code) noexcept
    -> application::epoch_catalog_failure
{
    using output = application::epoch_catalog_failure;
    switch(classify_sqlite_result(result_code))
    {
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
} // namespace testing
#endif

catalog_database::catalog_database(
    ::sqlite3* handle, std::filesystem::path path) noexcept
    : catalog_database{handle, std::move(path), nullptr}
{
}

catalog_database::catalog_database(
    ::sqlite3* handle,
    std::filesystem::path path,
    std::unique_ptr<catalog_database_platform_state> platform_state) noexcept
    : Handle{handle},
      Path{std::move(path)},
      PlatformState{std::move(platform_state)}
{
}

catalog_database::catalog_database(catalog_database&& other) noexcept
    : Handle{std::exchange(other.Handle, nullptr)},
      Path{std::move(other.Path)},
      PlatformState{std::move(other.PlatformState)}
{
}

auto catalog_database::operator=(catalog_database&& other) noexcept
    -> catalog_database&
{
    if(this != &other)
    {
        if(Handle != nullptr)
        {
            (void)::sqlite3_close_v2(Handle);
        }
        PlatformState.reset();
        Handle = std::exchange(other.Handle, nullptr);
        Path = std::move(other.Path);
        PlatformState = std::move(other.PlatformState);
    }
    return *this;
}

catalog_database::~catalog_database()
{
    if(Handle != nullptr)
    {
        (void)::sqlite3_close_v2(Handle);
        Handle = nullptr;
    }
    PlatformState.reset();
}

auto catalog_database::open(
    const std::filesystem::path& storage_root,
    std::stop_token stop)
    -> std::expected<catalog_database, catalog_database_failure>
{
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
#if defined(_WIN32)
    try
    {
        auto canonical_root = canonical_windows_root(storage_root);
        if(!canonical_root)
        {
            return std::unexpected(canonical_root.error());
        }
        auto directories = validate_ancestor_chain(*canonical_root);
        if(!directories)
        {
            return std::unexpected(directories.error());
        }
        auto security = private_security::load();
        if(!security)
        {
            return std::unexpected(security.error());
        }
        if(directories->empty())
        {
            return std::unexpected(failure::invalid_path);
        }
        directories->reserve(directories->size() + 2U);
        auto& root = directories->back();
        if(auto permissions = validate_private_security(
               root.Handle.Value, *security, windows_object_kind::directory);
           !permissions)
        {
            return std::unexpected(permissions.error());
        }
        if(!reliable_local_filesystem(root))
        {
            return std::unexpected(failure::platform_unsupported);
        }
        auto state = ensure_private_child(root, L"state", *security);
        if(!state)
        {
            return std::unexpected(state.error());
        }
        directories->push_back(std::move(*state));
        auto& retained_state = directories->back();
        auto version = ensure_private_child(
            retained_state, L"v1", *security);
        if(!version)
        {
            return std::unexpected(version.error());
        }
        directories->push_back(std::move(*version));
        auto& retained_version = directories->back();
        auto lock = catalog_lock::acquire(
            retained_version, *security, stop);
        if(!lock)
        {
            return std::unexpected(lock.error());
        }
        if(auto cleaned = cleanup_bootstrap_orphans(
               retained_version, *security);
           !cleaned)
        {
            return std::unexpected(cleaned.error());
        }
        const auto native_final_path =
            retained_version.Path / std::wstring{CatalogNameWide};
        auto missing = path_missing(native_final_path);
        if(!missing)
        {
            return std::unexpected(missing.error());
        }
        if(*missing)
        {
            auto created = create_bootstrap(
                retained_version, *security, stop);
            if(!created)
            {
                return std::unexpected(created.error());
            }
        }
        if(auto paths = validate_database_paths(
               retained_version, *security, true);
           !paths)
        {
            return std::unexpected(paths.error());
        }
        auto expected_file = identity_at(
            retained_version, CatalogNameWide, *security);
        if(!expected_file)
        {
            return std::unexpected(expected_file.error());
        }
        const auto expected_root = root.Identity;
        const auto expected_state = retained_state.Identity;
        const auto expected_version = retained_version.Identity;
        auto private_vfs = secure_windows_vfs::create(
            retained_version, *security, native_final_path);
        if(!private_vfs)
        {
            return std::unexpected(private_vfs.error());
        }
        auto opened = open_sqlite(
            native_final_path, (*private_vfs)->name());
        if(!opened)
        {
            return std::unexpected(
                (*private_vfs)->failure_or(opened.error()));
        }
        ::sqlite3* database = *opened;
        auto current = sqlite_main_file_is_current(
            database,
            retained_version,
            *expected_file,
            *security);
        auto configured = current
            ? configure_connection(database)
            : std::expected<void, failure>{
                  std::unexpected(current.error())};
        auto validated = configured
            ? validate_schema(database, stop)
            : std::expected<void, failure>{
                  std::unexpected(configured.error())};
        if(!validated)
        {
            (void)::sqlite3_close_v2(database);
            return std::unexpected(
                (*private_vfs)->failure_or(validated.error()));
        }
        auto live_root = open_private_directory(*canonical_root, *security);
        auto live_state = open_private_directory(
            retained_state.Path, *security);
        auto live_version = open_private_directory(
            retained_version.Path, *security);
        if(!live_root || !live_state || !live_version
           || live_root->Identity != expected_root
           || live_state->Identity != expected_state
           || live_version->Identity != expected_version)
        {
            (void)::sqlite3_close_v2(database);
            return std::unexpected(failure::invalid_path);
        }
        if(auto final_current = sqlite_main_file_is_current(
               database,
               retained_version,
               *expected_file,
               *security);
           !final_current)
        {
            (void)::sqlite3_close_v2(database);
            return std::unexpected(final_current.error());
        }
        if(auto paths = validate_database_paths(
               retained_version, *security, true);
           !paths)
        {
            (void)::sqlite3_close_v2(database);
            return std::unexpected(paths.error());
        }
        if(const auto vfs_failure = (*private_vfs)->recorded_failure())
        {
            (void)::sqlite3_close_v2(database);
            return std::unexpected(*vfs_failure);
        }
        if(auto authorizer = install_runtime_authorizer(database); !authorizer)
        {
            (void)::sqlite3_close_v2(database);
            return std::unexpected(authorizer.error());
        }
        const auto public_path = storage_root / "state" / "v1"
            / std::string{CatalogName};
        auto platform_state =
            std::make_unique<catalog_database_platform_state>();
        platform_state->Directories = std::move(*directories);
        platform_state->Vfs = std::move(*private_vfs);
        return catalog_database{
            database, public_path, std::move(platform_state)};
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected(failure::unavailable);
    }
    catch(const std::filesystem::filesystem_error&)
    {
        return std::unexpected(failure::invalid_path);
    }
    catch(...)
    {
        return std::unexpected(failure::unavailable);
    }
#else
    if(storage_root.empty() || !storage_root.is_absolute())
    {
        return std::unexpected(failure::invalid_path);
    }
    std::error_code canonical_error;
    const auto canonical_root =
        std::filesystem::canonical(storage_root, canonical_error);
    if(canonical_error
       || canonical_root != storage_root.lexically_normal())
    {
        return std::unexpected(failure::invalid_path);
    }
    auto root = open_private_directory(storage_root);
    if(!root)
    {
        return std::unexpected(root.error());
    }
    if(!reliable_local_filesystem(*root))
    {
        return std::unexpected(failure::platform_unsupported);
    }
    auto state = ensure_private_child(*root, "state");
    if(!state)
    {
        return std::unexpected(state.error());
    }
    auto version = ensure_private_child(*state, "v1");
    if(!version)
    {
        return std::unexpected(version.error());
    }
#if defined(__linux__)
    auto retained_vfs = retained_linux_vfs::create();
    if(!retained_vfs)
    {
        return std::unexpected(retained_vfs.error());
    }
    const auto* const requested_vfs = (*retained_vfs)->name();
#else
    constexpr const char* requested_vfs = nullptr;
#endif
    while(::flock(version->Value, LOCK_EX) != 0)
    {
        if(errno != EINTR)
        {
            return std::unexpected(failure::unavailable);
        }
        if(stop.stop_requested())
        {
            return std::unexpected(failure::cancelled);
        }
    }
    if(auto cleaned = cleanup_bootstrap_orphans(*version); !cleaned)
    {
        return std::unexpected(cleaned.error());
    }
    const auto parent_path = storage_root / "state" / "v1";
    const std::string catalog_name{CatalogName};
    struct stat existing{};
    const auto status = ::fstatat(
        version->Value,
        catalog_name.c_str(),
        &existing,
        AT_SYMLINK_NOFOLLOW);
    if(status != 0 && errno != ENOENT)
    {
        return std::unexpected(failure::invalid_path);
    }
    if(status != 0)
    {
        auto created = create_bootstrap(*version, stop, requested_vfs);
        if(!created)
        {
            return std::unexpected(created.error());
        }
    }
    if(auto paths = validate_database_paths(*version); !paths)
    {
        return std::unexpected(paths.error());
    }
    auto expected_file = identity_at(*version, CatalogName);
    auto expected_directory = identity_of(*version);
    if(!expected_file || !expected_directory)
    {
        return std::unexpected(
            expected_file ? expected_directory.error() : expected_file.error());
    }
    const auto final_path = parent_path / CatalogName;
    auto opened = open_sqlite(
        descriptor_child_path(*version, CatalogName), requested_vfs);
    if(!opened)
    {
        return std::unexpected(opened.error());
    }
    ::sqlite3* database = *opened;
    auto configured = configure_connection(database);
    auto validated = configured
        ? validate_schema(database, stop)
        : std::expected<void, failure>{std::unexpected(configured.error())};
    if(!validated)
    {
        (void)::sqlite3_close_v2(database);
        return std::unexpected(validated.error());
    }
    auto live_directory = open_private_directory(parent_path);
    auto live_directory_identity = live_directory
        ? identity_of(*live_directory)
        : std::expected<file_identity, failure>{
              std::unexpected(live_directory.error())};
    if(!live_directory_identity
       || *live_directory_identity != *expected_directory)
    {
        (void)::sqlite3_close_v2(database);
        return std::unexpected(failure::invalid_path);
    }
    if(auto current = sqlite_main_file_is_current(
           database, *version, *expected_file);
       !current)
    {
        (void)::sqlite3_close_v2(database);
        return std::unexpected(current.error());
    }
    if(auto paths = validate_database_paths(*version); !paths)
    {
        (void)::sqlite3_close_v2(database);
        return std::unexpected(paths.error());
    }
    if(auto authorizer = install_runtime_authorizer(database); !authorizer)
    {
        (void)::sqlite3_close_v2(database);
        return std::unexpected(authorizer.error());
    }
#if defined(__linux__)
    auto platform_state =
        std::make_unique<catalog_database_platform_state>();
    platform_state->Directories.reserve(3U);
    platform_state->Directories.push_back(std::move(*root));
    platform_state->Directories.push_back(std::move(*state));
    platform_state->Directories.push_back(std::move(*version));
    platform_state->Vfs = std::move(*retained_vfs);
    return catalog_database{
        database, final_path, std::move(platform_state)};
#else
    return catalog_database{database, final_path};
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

auto secure_random_bytes(std::span<std::uint8_t> output) noexcept -> bool
{
    return random_bytes(output);
}
} // namespace bpr::adapters::sqlite::detail
