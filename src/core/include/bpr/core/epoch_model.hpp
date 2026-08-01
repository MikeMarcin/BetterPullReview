#pragma once

#include "bpr/core/github_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace bpr::core
{
enum class git_hash_algorithm : std::uint8_t
{
    invalid,
    sha1,
    sha256,
};

template<typename Kind>
class git_object_oid
{
public:
    constexpr git_object_oid() = default;

    explicit constexpr git_object_oid(
        const git_hash_algorithm algorithm,
        const std::array<std::uint8_t, 32>& bytes) noexcept
        : Algorithm{algorithm}, Bytes{bytes}
    {
    }

    [[nodiscard]] constexpr auto algorithm() const noexcept
        -> git_hash_algorithm
    {
        return Algorithm;
    }

    [[nodiscard]] constexpr auto bytes() const noexcept
        -> const std::array<std::uint8_t, 32>&
    {
        return Bytes;
    }

    [[nodiscard]] constexpr auto byte_count() const noexcept -> std::size_t
    {
        switch(Algorithm)
        {
        case git_hash_algorithm::sha1:
            return 20U;
        case git_hash_algorithm::sha256:
            return 32U;
        case git_hash_algorithm::invalid:
            return 0U;
        }
        return 0U;
    }

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        const auto size = byte_count();
        if(size == 0U)
        {
            return false;
        }

        bool nonzero = false;
        for(std::size_t index = 0; index < size; ++index)
        {
            nonzero = nonzero || Bytes[index] != 0U;
        }
        for(std::size_t index = size; index < Bytes.size(); ++index)
        {
            if(Bytes[index] != 0U)
            {
                return false;
            }
        }
        return nonzero;
    }

    auto operator<=>(const git_object_oid&) const = default;

private:
    git_hash_algorithm Algorithm = git_hash_algorithm::invalid;
    std::array<std::uint8_t, 32> Bytes{};
};

struct git_commit_oid_kind;
struct git_tree_oid_kind;

using git_commit_oid = git_object_oid<git_commit_oid_kind>;
using git_tree_oid = git_object_oid<git_tree_oid_kind>;

class canonical_git_oid
{
public:
    constexpr canonical_git_oid() = default;

    explicit constexpr canonical_git_oid(
        const std::array<char, 64>& characters,
        const std::uint8_t length) noexcept
        : Characters{characters},
          Length{static_cast<std::uint8_t>(
              length == 40U || length == 64U ? length : 0U)}
    {
    }

    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view
    {
        return {Characters.data(), Length};
    }

    auto operator<=>(const canonical_git_oid&) const = default;

private:
    std::array<char, 64> Characters{};
    std::uint8_t Length = 0;
};

[[nodiscard]] auto parse_git_commit_oid(std::string_view text) noexcept
    -> std::optional<git_commit_oid>;
[[nodiscard]] auto parse_git_tree_oid(std::string_view text) noexcept
    -> std::optional<git_tree_oid>;
[[nodiscard]] auto canonical_hex(const git_commit_oid& oid) noexcept
    -> canonical_git_oid;
[[nodiscard]] auto canonical_hex(const git_tree_oid& oid) noexcept
    -> canonical_git_oid;

template<typename Tag>
class fixed_identity_256
{
public:
    constexpr fixed_identity_256() = default;

    explicit constexpr fixed_identity_256(
        const std::array<std::uint8_t, 32>& bytes) noexcept
        : Bytes{bytes}
    {
    }

    [[nodiscard]] constexpr auto bytes() const noexcept
        -> const std::array<std::uint8_t, 32>&
    {
        return Bytes;
    }

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        for(const auto byte : Bytes)
        {
            if(byte != 0U)
            {
                return true;
            }
        }
        return false;
    }

    auto operator<=>(const fixed_identity_256&) const = default;

private:
    std::array<std::uint8_t, 32> Bytes{};
};

template<typename Tag, typename Representation>
class integral_identity
{
public:
    constexpr integral_identity() = default;

    explicit constexpr integral_identity(const Representation value) noexcept
        : Value{value}
    {
    }

    [[nodiscard]] constexpr auto value() const noexcept -> Representation
    {
        return Value;
    }

    auto operator<=>(const integral_identity&) const = default;

private:
    Representation Value = 0;
};

struct github_profile_revision_tag;
struct github_viewer_actor_id_tag;
struct github_review_id_tag;
struct comparison_policy_revision_tag;
struct epoch_request_id_tag;
struct epoch_ordinal_tag;
struct epoch_catalog_revision_tag;
struct epoch_generation_tag;
struct comparison_generation_tag;
struct comparison_revision_tag;
struct observed_utc_milliseconds_tag;
struct github_review_order_tag;
struct pull_request_authority_scope_id_tag;
struct epoch_catalog_incarnation_id_tag;
struct base_branch_id_tag;
struct review_association_id_tag;
struct repository_cache_scope_id_tag;
struct host_authority_fingerprint_tag;
struct epoch_operation_id_tag;
struct epoch_source_observation_id_tag;

using github_profile_revision = strong_id<github_profile_revision_tag>;
using github_viewer_actor_id = strong_id<github_viewer_actor_id_tag>;
using github_review_id = strong_id<github_review_id_tag>;
using comparison_policy_revision = strong_id<comparison_policy_revision_tag>;
using epoch_request_id = strong_id<epoch_request_id_tag>;
using epoch_ordinal = integral_identity<epoch_ordinal_tag, std::int64_t>;
using epoch_catalog_revision =
    integral_identity<epoch_catalog_revision_tag, std::int64_t>;
using epoch_generation = strong_id<epoch_generation_tag>;
using comparison_generation = strong_id<comparison_generation_tag>;
using comparison_revision =
    integral_identity<comparison_revision_tag, std::int64_t>;
using observed_utc_milliseconds =
    integral_identity<observed_utc_milliseconds_tag, std::int64_t>;
using github_review_order =
    integral_identity<github_review_order_tag, std::uint16_t>;
using pull_request_authority_scope_id =
    fixed_identity_256<pull_request_authority_scope_id_tag>;
using epoch_catalog_incarnation_id =
    fixed_identity_256<epoch_catalog_incarnation_id_tag>;
using base_branch_id = fixed_identity_256<base_branch_id_tag>;
using review_association_id = fixed_identity_256<review_association_id_tag>;
using repository_cache_scope_id =
    fixed_identity_256<repository_cache_scope_id_tag>;
using host_authority_fingerprint =
    fixed_identity_256<host_authority_fingerprint_tag>;
using epoch_operation_id = fixed_identity_256<epoch_operation_id_tag>;
using epoch_source_observation_id =
    fixed_identity_256<epoch_source_observation_id_tag>;

inline constexpr auto maximum_epoch_ordinal =
    std::numeric_limits<std::int64_t>::max();
inline constexpr auto maximum_epoch_catalog_revision =
    std::numeric_limits<std::int64_t>::max();
inline constexpr auto maximum_comparison_revision =
    std::numeric_limits<std::int64_t>::max();
inline constexpr std::int64_t maximum_observed_utc_milliseconds =
    253'402'300'799'999LL;
inline constexpr std::uint16_t maximum_github_review_order = 10'000U;
[[nodiscard]] constexpr auto valid(const epoch_ordinal value) noexcept -> bool
{
    return value.value() > 0 && value.value() <= maximum_epoch_ordinal;
}

[[nodiscard]] constexpr auto valid(const epoch_catalog_revision value) noexcept
    -> bool
{
    return value.value() >= 0
        && value.value() <= maximum_epoch_catalog_revision;
}

[[nodiscard]] constexpr auto valid(const comparison_revision value) noexcept
    -> bool
{
    return value.value() > 0 && value.value() <= maximum_comparison_revision;
}

[[nodiscard]] constexpr auto valid(
    const observed_utc_milliseconds value) noexcept -> bool
{
    return value.value() >= 0
        && value.value() <= maximum_observed_utc_milliseconds;
}

[[nodiscard]] constexpr auto valid(const github_review_order value) noexcept
    -> bool
{
    return value.value() > 0 && value.value() <= maximum_github_review_order;
}

enum class pull_request_revision_origin : std::uint8_t
{
    live,
    explicit_historical_import,
};

struct pull_request_authority
{
    host_profile_id Profile;
    github_profile_revision ProfileRevision;
    git_host_id Host;
    host_authority_fingerprint HostAuthority;
    github_account_id Account;
    github_viewer_actor_id Viewer;
    repository_id Repository;
    repository_cache_scope_id Cache;
    pull_request_id PullRequest;
    pull_request_number Number;
    pull_request_authority_scope_id Scope;

    auto operator<=>(const pull_request_authority&) const = default;
};

struct pull_request_source_revision
{
    epoch_source_observation_id Observation;
    provider_snapshot_id Snapshot;
    github_identity_generation IdentityGeneration;
    request_generation PullRequestGeneration;

    auto operator<=>(const pull_request_source_revision&) const = default;
};

enum class anchor_availability : std::uint8_t
{
    available,
    unavailable,
    pruned,
    cache_deleted,
    corrupt,
    never_observed,
};

enum class github_review_state : std::uint8_t
{
    pending,
    approved,
    changes_requested,
    commented,
    dismissed,
};

struct current_viewer_review_candidate
{
    github_review_id Review;
    github_viewer_actor_id Actor;
    github_review_order Order;
    github_review_state State = github_review_state::pending;
    git_commit_oid Commit;

    auto operator<=>(const current_viewer_review_candidate&) const = default;
};

struct catalog_review_association
{
    review_association_id Association;
    github_review_id Review;
    github_viewer_actor_id Actor;
    github_review_order Order;
    github_review_state State = github_review_state::pending;
    git_commit_oid Commit;
    anchor_availability Availability = anchor_availability::unavailable;

    auto operator<=>(const catalog_review_association&) const = default;
};

struct accepted_pull_request_revision
{
    pull_request_source_revision Source;
    pull_request_revision_origin Origin = pull_request_revision_origin::live;
    base_branch_id BaseBranch;
    git_commit_oid Base;
    git_commit_oid Head;
    std::optional<current_viewer_review_candidate> CurrentViewerReview;

    auto operator<=>(const accepted_pull_request_revision&) const = default;
};

struct epoch_catalog_identity
{
    epoch_catalog_incarnation_id Incarnation;
    epoch_catalog_revision Revision;

    auto operator<=>(const epoch_catalog_identity&) const = default;
};

struct review_epoch_id
{
    pull_request_authority_scope_id Authority;
    epoch_catalog_incarnation_id Incarnation;
    epoch_ordinal Ordinal;

    auto operator<=>(const review_epoch_id&) const = default;
};

struct epoch_material_identity
{
    git_tree_oid BaseTree;
    git_tree_oid HeadTree;
    comparison_policy_revision PolicyRevision;

    auto operator<=>(const epoch_material_identity&) const = default;
};

enum class epoch_origin : std::uint8_t
{
    initial_observation,
    live_observation,
    explicit_historical_import,
    retention_rehydration,
};

enum class epoch_base_transition : std::uint8_t
{
    not_applicable,
    unchanged,
    base_oid_changed,
    base_branch_changed,
};

enum class epoch_head_transition : std::uint8_t
{
    not_applicable,
    unchanged,
    descendant,
    non_fast_forward,
    lineage_unknown,
};

enum class epoch_material_relation : std::uint8_t
{
    same,
    different,
    unknown,
};

enum class epoch_primary_cause : std::uint8_t
{
    initial_observation,
    explicit_historical_import,
    base_branch_change,
    force_push_rebase,
    author_upload,
    head_change_lineage_unavailable,
    base_update,
    retention_rehydration,
};

struct epoch_cause
{
    epoch_origin Origin = epoch_origin::initial_observation;
    epoch_base_transition Base = epoch_base_transition::not_applicable;
    epoch_head_transition Head = epoch_head_transition::not_applicable;
    epoch_material_relation Material = epoch_material_relation::unknown;
    epoch_primary_cause Primary = epoch_primary_cause::initial_observation;

    auto operator<=>(const epoch_cause&) const = default;
};

enum class head_lineage : std::uint8_t
{
    not_applicable,
    descendant,
    non_fast_forward,
    unknown,
};

struct review_epoch_summary
{
    review_epoch_id Id;
    pull_request_source_revision Source;
    base_branch_id BaseBranch;
    git_commit_oid Base;
    git_commit_oid Head;
    std::optional<epoch_material_identity> Material;
    epoch_cause Cause;
    observed_utc_milliseconds ObservedAt;
    anchor_availability BaseAvailability = anchor_availability::unavailable;
    anchor_availability HeadAvailability = anchor_availability::unavailable;

    auto operator<=>(const review_epoch_summary&) const = default;
};

enum class comparison_anchor_kind : std::uint8_t
{
    epoch_base,
    epoch_head,
    review_commit,
};

struct comparison_anchor_identity
{
    pull_request_authority_scope_id Authority;
    epoch_catalog_incarnation_id Incarnation;
    comparison_anchor_kind Kind = comparison_anchor_kind::epoch_base;
    epoch_ordinal Epoch;
    review_association_id Review;

    auto operator<=>(const comparison_anchor_identity&) const = default;
};

struct comparison_anchor
{
    comparison_anchor_identity Identity;
    git_commit_oid Commit;
    anchor_availability Availability = anchor_availability::unavailable;

    auto operator<=>(const comparison_anchor&) const = default;
};

struct comparison_pair
{
    comparison_anchor Left;
    comparison_anchor Right;

    auto operator<=>(const comparison_pair&) const = default;
};

enum class named_comparison : std::uint8_t
{
    whole_pr,
    previous_upload_to_latest,
    my_last_reviewed_to_latest,
};

struct named_comparison_selection
{
    named_comparison Alias = named_comparison::whole_pr;

    auto operator<=>(const named_comparison_selection&) const = default;
};

struct explicit_comparison_selection
{
    comparison_anchor_identity Left;
    comparison_anchor_identity Right;

    auto operator<=>(const explicit_comparison_selection&) const = default;
};

using comparison_selection =
    std::variant<named_comparison_selection, explicit_comparison_selection>;

[[nodiscard]] constexpr auto named_selection(
    const named_comparison alias) noexcept -> comparison_selection
{
    return named_comparison_selection{.Alias = alias};
}

[[nodiscard]] constexpr auto explicit_selection(
    const comparison_anchor_identity& left,
    const comparison_anchor_identity& right) noexcept -> comparison_selection
{
    return explicit_comparison_selection{
        .Left = left,
        .Right = right,
    };
}

struct comparison_candidates
{
    epoch_catalog_identity Catalog;
    std::optional<review_epoch_summary> LatestLive;
    std::optional<review_epoch_summary> PreviousDistinctHead;
    std::optional<comparison_anchor> CurrentViewerReview;
    std::optional<comparison_anchor> ExplicitLeft;
    std::optional<comparison_anchor> ExplicitRight;

    auto operator<=>(const comparison_candidates&) const = default;
};

enum class retention_rehydration_requirement : std::uint8_t
{
    not_required,
    required,
};

struct epoch_preparation_observation
{
    std::optional<review_epoch_summary> LatestLive;
    std::optional<catalog_review_association> CatalogCurrentViewerReview;
    std::optional<catalog_review_association> ProposedCurrentViewerReview;
    std::optional<epoch_ordinal> ProposedOrdinal;
    observed_utc_milliseconds ObservedAt;
    head_lineage HeadLineage = head_lineage::not_applicable;
    std::optional<epoch_material_identity> Material;
    retention_rehydration_requirement RetentionRehydration =
        retention_rehydration_requirement::not_required;

    auto operator<=>(const epoch_preparation_observation&) const = default;
};

enum class epoch_catalog_mutation_kind : std::uint8_t
{
    append_epoch,
    associate_review,
};

struct append_epoch_mutation
{
    review_epoch_summary Epoch;
    std::optional<catalog_review_association> Review;

    auto operator<=>(const append_epoch_mutation&) const = default;
};

struct associate_review_mutation
{
    catalog_review_association Review;

    auto operator<=>(const associate_review_mutation&) const = default;
};

using epoch_catalog_mutation =
    std::variant<append_epoch_mutation, associate_review_mutation>;

[[nodiscard]] constexpr auto mutation_kind(
    const epoch_catalog_mutation& mutation) noexcept
    -> epoch_catalog_mutation_kind
{
    return std::holds_alternative<append_epoch_mutation>(mutation)
        ? epoch_catalog_mutation_kind::append_epoch
        : epoch_catalog_mutation_kind::associate_review;
}

struct epoch_load_context
{
    epoch_request_id Request;
    application_session_id ApplicationSession;
    pull_request_authority Authority;
    accepted_pull_request_revision Revision;
    epoch_generation Generation;

    auto operator<=>(const epoch_load_context&) const = default;
};

struct epoch_operation_context
{
    epoch_request_id Request;
    application_session_id ApplicationSession;
    pull_request_authority Authority;
    accepted_pull_request_revision Revision;
    epoch_generation Generation;
    epoch_catalog_incarnation_id CatalogIncarnation;
    epoch_catalog_revision ExpectedCatalogRevision;
    epoch_operation_id Operation;

    auto operator<=>(const epoch_operation_context&) const = default;
};

struct epoch_preparation_context
{
    epoch_request_id Request;
    application_session_id ApplicationSession;
    pull_request_authority Authority;
    accepted_pull_request_revision Revision;
    epoch_generation Generation;
    epoch_catalog_incarnation_id CatalogIncarnation;
    epoch_catalog_revision ExpectedCatalogRevision;

    auto operator<=>(const epoch_preparation_context&) const = default;
};

struct comparison_context
{
    epoch_request_id Request;
    application_session_id ApplicationSession;
    pull_request_authority Authority;
    pull_request_source_revision Source;
    epoch_catalog_incarnation_id CatalogIncarnation;
    epoch_catalog_revision CatalogRevision;
    comparison_generation Generation;
    comparison_selection Selection;

    auto operator<=>(const comparison_context&) const = default;
};

struct pull_request_revision_accepted_event
{
    application_session_id ApplicationSession;
    pull_request_authority Authority;
    accepted_pull_request_revision Revision;

    auto operator<=>(const pull_request_revision_accepted_event&) const =
        default;
};

struct pull_request_revision_invalidated_event
{
    application_session_id ApplicationSession;
    pull_request_authority Authority;
    accepted_pull_request_revision Revision;
    epoch_generation Generation;

    auto operator<=>(const pull_request_revision_invalidated_event&) const =
        default;
};

struct epoch_catalog_loaded_event
{
    epoch_load_context Context;
    epoch_catalog_identity Catalog;

    auto operator<=>(const epoch_catalog_loaded_event&) const = default;
};

enum class epoch_failure : std::uint8_t
{
    unavailable,
    malformed_observation,
    catalog_conflict,
    quota_exceeded,
    ordinal_exhausted,
    repair_required,
};

struct epoch_catalog_load_failed_event
{
    epoch_load_context Context;
    epoch_failure Failure = epoch_failure::unavailable;

    auto operator<=>(const epoch_catalog_load_failed_event&) const = default;
};

struct epoch_catalog_load_cancelled_event
{
    epoch_load_context Context;

    auto operator<=>(const epoch_catalog_load_cancelled_event&) const = default;
};

struct epoch_revision_prepared_event
{
    epoch_preparation_context Context;
    epoch_operation_id Operation;
    epoch_preparation_observation Observation;

    auto operator<=>(const epoch_revision_prepared_event&) const = default;
};

struct epoch_revision_prepare_failed_event
{
    epoch_preparation_context Context;
    epoch_failure Failure = epoch_failure::unavailable;

    auto operator<=>(const epoch_revision_prepare_failed_event&) const =
        default;
};

struct epoch_revision_prepare_cancelled_event
{
    epoch_preparation_context Context;

    auto operator<=>(const epoch_revision_prepare_cancelled_event&) const =
        default;
};

struct epoch_catalog_committed_event
{
    epoch_operation_context Context;
    epoch_catalog_mutation Mutation;
    epoch_catalog_revision NewRevision;

    auto operator<=>(const epoch_catalog_committed_event&) const = default;
};

struct epoch_catalog_commit_failed_event
{
    epoch_operation_context Context;
    epoch_catalog_mutation Mutation;
    epoch_failure Failure = epoch_failure::unavailable;

    auto operator<=>(const epoch_catalog_commit_failed_event&) const = default;
};

enum class epoch_commit_progress : std::uint8_t
{
    before_ref_commit,
    ref_commit_in_progress,
};

struct epoch_catalog_commit_cancelled_event
{
    epoch_operation_context Context;
    epoch_catalog_mutation Mutation;
    epoch_commit_progress Progress = epoch_commit_progress::before_ref_commit;

    auto operator<=>(const epoch_catalog_commit_cancelled_event&) const =
        default;
};

struct comparison_resolved_event
{
    comparison_context Context;
    comparison_candidates Candidates;

    auto operator<=>(const comparison_resolved_event&) const = default;
};

struct comparison_resolve_failed_event
{
    comparison_context Context;
    epoch_failure Failure = epoch_failure::unavailable;

    auto operator<=>(const comparison_resolve_failed_event&) const = default;
};

struct comparison_resolve_cancelled_event
{
    comparison_context Context;

    auto operator<=>(const comparison_resolve_cancelled_event&) const = default;
};

struct select_named_comparison_intent
{
    named_comparison Alias = named_comparison::whole_pr;
    epoch_catalog_revision ExpectedCatalogRevision;

    auto operator<=>(const select_named_comparison_intent&) const = default;
};

struct refresh_epochs_intent
{
    auto operator<=>(const refresh_epochs_intent&) const = default;
};

struct select_comparison_anchors_intent
{
    comparison_anchor_identity Left;
    comparison_anchor_identity Right;
    epoch_catalog_revision ExpectedCatalogRevision;

    auto operator<=>(const select_comparison_anchors_intent&) const = default;
};

enum class comparison_side : std::uint8_t
{
    left,
    right,
};

struct select_comparison_anchor_intent
{
    comparison_side Side = comparison_side::left;
    comparison_anchor_identity Anchor;
    epoch_catalog_revision ExpectedCatalogRevision;

    auto operator<=>(const select_comparison_anchor_intent&) const = default;
};

struct cancel_epoch_refresh_intent
{
    auto operator<=>(const cancel_epoch_refresh_intent&) const = default;
};

using epoch_event = std::variant<
    pull_request_revision_accepted_event,
    pull_request_revision_invalidated_event,
    epoch_catalog_loaded_event,
    epoch_catalog_load_failed_event,
    epoch_catalog_load_cancelled_event,
    epoch_revision_prepared_event,
    epoch_revision_prepare_failed_event,
    epoch_revision_prepare_cancelled_event,
    epoch_catalog_committed_event,
    epoch_catalog_commit_failed_event,
    epoch_catalog_commit_cancelled_event,
    comparison_resolved_event,
    comparison_resolve_failed_event,
    comparison_resolve_cancelled_event,
    refresh_epochs_intent,
    select_named_comparison_intent,
    select_comparison_anchors_intent,
    select_comparison_anchor_intent,
    cancel_epoch_refresh_intent>;

struct invalidate_epoch_authority_request
{
    application_session_id ApplicationSession;
    std::optional<pull_request_authority> PreviousAuthority;
    epoch_generation NewGeneration;

    auto operator<=>(const invalidate_epoch_authority_request&) const = default;
};

struct load_epoch_catalog_request
{
    epoch_load_context Context;

    auto operator<=>(const load_epoch_catalog_request&) const = default;
};

struct prepare_epoch_revision_request
{
    epoch_preparation_context Context;

    auto operator<=>(const prepare_epoch_revision_request&) const = default;
};

struct commit_epoch_catalog_request
{
    epoch_operation_context Context;
    epoch_catalog_mutation Mutation;

    auto operator<=>(const commit_epoch_catalog_request&) const = default;
};

struct discard_epoch_preparation_request
{
    epoch_operation_context Context;

    auto operator<=>(const discard_epoch_preparation_request&) const = default;
};

struct resolve_comparison_request
{
    comparison_context Context;

    auto operator<=>(const resolve_comparison_request&) const = default;
};

struct cancel_epoch_work_request
{
    epoch_request_id Request;
    std::variant<epoch_generation, comparison_generation> Generation;

    auto operator<=>(const cancel_epoch_work_request&) const = default;
};

using epoch_host_request = std::variant<
    invalidate_epoch_authority_request,
    load_epoch_catalog_request,
    prepare_epoch_revision_request,
    commit_epoch_catalog_request,
    discard_epoch_preparation_request,
    resolve_comparison_request,
    cancel_epoch_work_request>;

enum class epoch_reason_code : std::uint8_t
{
    none,
    invalid_identity,
    invalid_oid,
    invalid_revision,
    invalid_catalog,
    invalid_observation,
    invalid_selection,
    no_active_authority,
    no_catalog,
    no_resolved_comparison,
    catalog_revision_mismatch,
    stale_completion,
    completion_identity_mismatch,
    request_counter_exhausted,
    epoch_generation_exhausted,
    comparison_generation_exhausted,
    epoch_ordinal_exhausted,
    catalog_revision_exhausted,
    comparison_revision_exhausted,
    unavailable,
    catalog_conflict,
    quota_exceeded,
    repair_required,
    cancelled,
    cancellation_after_point_of_no_return,
};

[[nodiscard]] auto reason_for(epoch_failure failure) noexcept
    -> epoch_reason_code;

enum class epoch_status : std::uint8_t
{
    inactive,
    loading_catalog,
    preparing,
    committing,
    resolving,
    ready,
    unavailable,
    failed,
    cancelled,
};

enum class epoch_command_id : std::uint8_t
{
    refresh_epochs,
    select_comparison,
    cancel_epoch_refresh,
};

struct epoch_command_projection
{
    epoch_command_id Id = epoch_command_id::refresh_epochs;
    bool Enabled = false;
    epoch_reason_code DisabledReason = epoch_reason_code::no_active_authority;

    auto operator<=>(const epoch_command_projection&) const = default;
};

struct epoch_command_delta
{
    std::array<epoch_command_projection, 3> Commands{};

    auto operator<=>(const epoch_command_delta&) const = default;
};

struct epoch_authority_delta
{
    std::optional<pull_request_authority> Authority;
    std::optional<accepted_pull_request_revision> Revision;
    epoch_generation Generation;

    auto operator<=>(const epoch_authority_delta&) const = default;
};

struct epoch_catalog_delta
{
    std::optional<epoch_catalog_identity> Catalog;
    std::optional<review_epoch_id> InsertedEpoch;
    std::optional<review_association_id> ChangedReviewAssociation;

    auto operator<=>(const epoch_catalog_delta&) const = default;
};

struct accepted_epoch_delta
{
    review_epoch_summary Epoch;
    epoch_catalog_revision CatalogRevision;

    auto operator<=>(const accepted_epoch_delta&) const = default;
};

struct comparison_selection_delta
{
    comparison_selection Selection;

    auto operator<=>(const comparison_selection_delta&) const = default;
};

struct comparison_resolution_delta
{
    comparison_selection Selection;
    std::optional<comparison_pair> Pair;
    comparison_revision Revision;
    bool AvailableForDiff = false;

    auto operator<=>(const comparison_resolution_delta&) const = default;
};

struct epoch_status_delta
{
    epoch_status Status = epoch_status::inactive;

    auto operator<=>(const epoch_status_delta&) const = default;
};

struct epoch_diagnostic
{
    epoch_reason_code Code = epoch_reason_code::none;

    auto operator<=>(const epoch_diagnostic&) const = default;
};

enum class epoch_changed_domain : std::uint16_t
{
    none = 0,
    authority = 1U << 0U,
    catalog = 1U << 1U,
    epoch = 1U << 2U,
    selection = 1U << 3U,
    comparison = 1U << 4U,
    status = 1U << 5U,
    diagnostics = 1U << 6U,
    commands = 1U << 7U,
};

[[nodiscard]] constexpr auto operator|(
    const epoch_changed_domain left, const epoch_changed_domain right) noexcept
    -> epoch_changed_domain
{
    return static_cast<epoch_changed_domain>(
        static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr auto has_changed(
    const epoch_changed_domain value,
    const epoch_changed_domain expected) noexcept -> bool
{
    return (static_cast<std::uint16_t>(value)
            & static_cast<std::uint16_t>(expected))
        != 0U;
}

struct epoch_transition_output
{
    epoch_changed_domain Changed = epoch_changed_domain::none;
    std::vector<epoch_host_request> Requests;
    std::optional<epoch_authority_delta> Authority;
    std::optional<epoch_catalog_delta> Catalog;
    std::optional<accepted_epoch_delta> AcceptedEpoch;
    std::optional<comparison_selection_delta> Selection;
    std::optional<comparison_resolution_delta> Comparison;
    std::optional<epoch_status_delta> Status;
    std::optional<epoch_command_delta> Commands;
    std::vector<epoch_diagnostic> Diagnostics;

    auto operator<=>(const epoch_transition_output&) const = default;
};

enum class epoch_ingestion_phase : std::uint8_t
{
    none,
    loading_catalog,
    preparing,
    committing,
};

struct pending_epoch_ingestion
{
    epoch_request_id Request;
    epoch_generation Generation;
    epoch_ingestion_phase Phase = epoch_ingestion_phase::none;
    bool CancellationRequested = false;
    epoch_operation_id Operation;
    std::optional<epoch_catalog_mutation> Mutation;

    auto operator<=>(const pending_epoch_ingestion&) const = default;
};

struct pending_comparison_resolution
{
    epoch_request_id Request;
    comparison_generation Generation;

    auto operator<=>(const pending_comparison_resolution&) const = default;
};

struct epoch_state
{
    application_session_id ApplicationSession;
    epoch_request_id NextRequest{1};
    bool RequestCounterExhausted = false;
    epoch_generation EpochGeneration;
    comparison_generation ComparisonGeneration;
    std::optional<pull_request_authority> Authority;
    std::optional<accepted_pull_request_revision> AcceptedRevision;
    std::optional<epoch_catalog_identity> Catalog;
    comparison_selection RequestedSelection =
        named_selection(named_comparison::whole_pr);
    std::optional<comparison_pair> AcceptedComparison;
    comparison_revision AcceptedComparisonRevision;
    std::optional<pending_epoch_ingestion> PendingIngestion;
    std::optional<pending_comparison_resolution> PendingComparison;
    epoch_status Status = epoch_status::inactive;

    auto operator<=>(const epoch_state&) const = default;
};

static_assert(std::is_trivially_copyable_v<epoch_state>);
static_assert(std::is_trivially_destructible_v<epoch_state>);
static_assert(sizeof(epoch_state) <= 2'048U);
} // namespace bpr::core
