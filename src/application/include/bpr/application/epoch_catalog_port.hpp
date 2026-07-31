#pragma once

#include "bpr/application/git_cache_port.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <variant>
#include <vector>

namespace bpr::application
{
inline constexpr std::uint16_t maximum_epoch_history_page_size = 100U;
inline constexpr std::uint16_t maximum_epoch_recovery_page_size = 100U;
inline constexpr std::size_t maximum_exact_anchor_lookup_count = 100U;
inline constexpr std::size_t maximum_prune_epoch_count = 100U;
inline constexpr std::size_t maximum_cache_delete_scope_count = 10'000U;

struct epoch_catalog_operation_revision_tag;
using epoch_catalog_operation_revision = core::integral_identity<
    epoch_catalog_operation_revision_tag,
    std::int64_t>;
inline constexpr auto maximum_epoch_catalog_operation_revision =
    std::numeric_limits<std::int64_t>::max();

[[nodiscard]] constexpr auto valid(
    const epoch_catalog_operation_revision revision) noexcept -> bool
{
    return revision.value() > 0
        && revision.value()
            <= maximum_epoch_catalog_operation_revision;
}

[[nodiscard]] constexpr auto can_advance(
    const epoch_catalog_operation_revision revision) noexcept -> bool
{
    return application::valid(revision)
        && revision.value()
            < maximum_epoch_catalog_operation_revision;
}

struct epoch_catalog_authority_key
{
    core::host_profile_id Profile;
    core::github_profile_revision ProfileRevision;
    core::git_host_id Host;
    core::host_authority_fingerprint HostAuthority;
    core::github_account_id Account;
    core::github_viewer_actor_id Viewer;
    core::repository_id Repository;
    core::pull_request_id PullRequest;
    core::pull_request_number Number;
    core::git_hash_algorithm ObjectFormat =
        core::git_hash_algorithm::invalid;

    auto operator<=>(const epoch_catalog_authority_key&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const epoch_catalog_authority_key& key) noexcept -> bool
{
    return key.Profile.value() != 0U
        && key.ProfileRevision.value() != 0U
        && key.Host.value() != 0U && key.HostAuthority.valid()
        && key.Account.value() != 0U && key.Viewer.value() != 0U
        && key.Repository.value() != 0U
        && key.PullRequest.value() != 0U && key.Number.valid()
        && (
            key.ObjectFormat == core::git_hash_algorithm::sha1
            || key.ObjectFormat == core::git_hash_algorithm::sha256);
}

struct epoch_catalog_open_request
{
    core::application_session_id ApplicationSession;
    epoch_catalog_authority_key Authority;

    auto operator<=>(const epoch_catalog_open_request&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const epoch_catalog_open_request& request) noexcept -> bool
{
    return request.ApplicationSession.value() != 0U
        && valid(request.Authority);
}

struct epoch_catalog_binding
{
    core::pull_request_authority Authority;
    core::epoch_catalog_identity Catalog;
    core::git_hash_algorithm ObjectFormat =
        core::git_hash_algorithm::invalid;

    auto operator<=>(const epoch_catalog_binding&) const = default;
};

[[nodiscard]] inline auto valid(
    const epoch_catalog_binding& binding) noexcept -> bool
{
    return core::valid(binding.Authority)
        && core::valid(binding.Catalog)
        && (
            binding.ObjectFormat == core::git_hash_algorithm::sha1
            || binding.ObjectFormat == core::git_hash_algorithm::sha256);
}

struct epoch_catalog_snapshot
{
    epoch_catalog_binding Binding;
    std::optional<core::review_epoch_summary> LatestLive;
    std::optional<core::review_epoch_summary> PreviousDistinctHead;
    std::optional<core::catalog_review_association> CurrentViewerReview;

    auto operator<=>(const epoch_catalog_snapshot&) const = default;
};

struct exact_anchor_lookup_request
{
    core::epoch_catalog_identity Catalog;
    std::vector<core::comparison_anchor_identity> Anchors;

    auto operator<=>(const exact_anchor_lookup_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const exact_anchor_lookup_request& request) noexcept -> bool
{
    if(
        !core::valid(request.Catalog) || request.Anchors.empty()
        || request.Anchors.size() > maximum_exact_anchor_lookup_count)
    {
        return false;
    }
    for(std::size_t index = 0U; index < request.Anchors.size(); ++index)
    {
        const auto& anchor = request.Anchors[index];
        if(
            !core::valid(anchor)
            || anchor.Incarnation != request.Catalog.Incarnation
            || (
                index != 0U
                && !(request.Anchors[index - 1U] < anchor)))
        {
            return false;
        }
    }
    return true;
}

struct exact_anchor_lookup_result
{
    core::epoch_catalog_identity Catalog;
    std::vector<core::comparison_anchor> Anchors;

    auto operator<=>(const exact_anchor_lookup_result&) const = default;
};

struct anchor_availability_change
{
    core::comparison_anchor_identity Anchor;
    core::git_commit_oid ExpectedCommit;
    core::anchor_availability Expected =
        core::anchor_availability::unavailable;
    core::anchor_availability Next =
        core::anchor_availability::corrupt;

    auto operator<=>(const anchor_availability_change&) const = default;
};

struct mark_anchor_availability_request
{
    core::epoch_catalog_identity Catalog;
    std::vector<anchor_availability_change> Changes;

    auto operator<=>(const mark_anchor_availability_request&) const =
        default;
};

[[nodiscard]] inline auto valid(
    const mark_anchor_availability_request& request) noexcept -> bool
{
    if(
        !core::valid(request.Catalog) || request.Changes.empty()
        || request.Changes.size() > maximum_exact_anchor_lookup_count)
    {
        return false;
    }
    for(std::size_t index = 0U; index < request.Changes.size(); ++index)
    {
        const auto& change = request.Changes[index];
        if(
            !core::valid(change.Anchor)
            || change.Anchor.Incarnation
                != request.Catalog.Incarnation
            || !change.ExpectedCommit.valid()
            || static_cast<std::uint8_t>(change.Expected)
                > static_cast<std::uint8_t>(
                    core::anchor_availability::never_observed)
            || (
                change.Next != core::anchor_availability::unavailable
                && change.Next != core::anchor_availability::pruned
                && change.Next
                    != core::anchor_availability::cache_deleted
                && change.Next != core::anchor_availability::corrupt)
            || (
                index != 0U
                && !(request.Changes[index - 1U].Anchor
                    < change.Anchor)))
        {
            return false;
        }
    }
    return true;
}

struct epoch_history_cursor
{
    core::pull_request_authority_scope_id Authority;
    core::epoch_catalog_incarnation_id Incarnation;
    core::epoch_catalog_revision Revision;
    core::epoch_ordinal Before;

    auto operator<=>(const epoch_history_cursor&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const epoch_history_cursor& cursor) noexcept -> bool
{
    return cursor.Authority.valid() && cursor.Incarnation.valid()
        && core::valid(cursor.Revision) && core::valid(cursor.Before);
}

struct epoch_history_page_request
{
    core::epoch_catalog_identity Catalog;
    std::optional<epoch_history_cursor> Cursor;
    std::uint16_t Limit = maximum_epoch_history_page_size;

    auto operator<=>(const epoch_history_page_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const epoch_catalog_binding& binding,
    const epoch_history_page_request& request) noexcept -> bool
{
    if(
        !valid(binding) || !core::valid(request.Catalog)
        || request.Catalog != binding.Catalog || request.Limit == 0U
        || request.Limit > maximum_epoch_history_page_size)
    {
        return false;
    }
    return !request.Cursor
        || (
            valid(*request.Cursor)
            && request.Cursor->Authority == binding.Authority.Scope
            && request.Cursor->Incarnation
                == binding.Catalog.Incarnation
            && request.Cursor->Revision == binding.Catalog.Revision);
}

struct epoch_history_page
{
    core::pull_request_authority_scope_id Authority;
    core::epoch_catalog_identity Catalog;
    std::vector<core::review_epoch_summary> Epochs;
    std::optional<epoch_history_cursor> Continuation;

    auto operator<=>(const epoch_history_page&) const = default;
};

enum class epoch_catalog_operation_kind : std::uint8_t
{
    append = 1U,
    prune = 2U,
    cache_delete = 3U,
};

enum class epoch_catalog_operation_phase : std::uint8_t
{
    preparing = 1U,
    spool_recorded = 2U,
    imported = 3U,
    staging_ready = 4U,
    commit_authorized = 5U,
    ref_commit_in_progress = 6U,
    refs_committed = 7U,
    visible_cleanup_pending = 8U,
    complete = 9U,
    abort_requested = 10U,
    aborted = 11U,
    repair_required = 12U,
    prune_intent = 13U,
    ref_delete_in_progress = 14U,
    cache_delete_building_exact_set = 15U,
    cache_delete_intent_sealed = 16U,
    quarantine_rename_in_progress = 17U,
    quarantined = 18U,
    catalog_tombstoned_cleanup_pending = 19U,
};

enum class epoch_catalog_transition_evidence : std::uint8_t
{
    catalog_transaction = 1U,
    exact_staging_pair = 2U,
    core_commit_authorization = 3U,
    exact_final_pair = 4U,
    exact_cleanup_complete = 5U,
    exact_refs_absent = 6U,
    exact_quarantine = 7U,
    repair_ambiguity = 8U,
};

[[nodiscard]] constexpr auto point_of_no_cancellation(
    const epoch_catalog_operation_phase phase) noexcept -> bool
{
    return phase == epoch_catalog_operation_phase::ref_commit_in_progress
        || phase == epoch_catalog_operation_phase::refs_committed
        || phase
            == epoch_catalog_operation_phase::visible_cleanup_pending
        || phase == epoch_catalog_operation_phase::complete
        || phase
            == epoch_catalog_operation_phase::ref_delete_in_progress
        || phase
            == epoch_catalog_operation_phase::
                quarantine_rename_in_progress
        || phase == epoch_catalog_operation_phase::quarantined
        || phase
            == epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending;
}

[[nodiscard]] constexpr auto valid_phase(
    const epoch_catalog_operation_kind kind,
    const epoch_catalog_operation_phase phase) noexcept -> bool
{
    using enum epoch_catalog_operation_phase;

    if(phase == repair_required || phase == complete)
    {
        return true;
    }
    switch(kind)
    {
    case epoch_catalog_operation_kind::append:
        return phase == preparing || phase == spool_recorded
            || phase == imported || phase == staging_ready
            || phase == commit_authorized
            || phase == ref_commit_in_progress
            || phase == refs_committed
            || phase == visible_cleanup_pending
            || phase == abort_requested || phase == aborted;
    case epoch_catalog_operation_kind::prune:
        return phase == prune_intent
            || phase == ref_delete_in_progress
            || phase == visible_cleanup_pending
            || phase == abort_requested || phase == aborted;
    case epoch_catalog_operation_kind::cache_delete:
        return phase == cache_delete_building_exact_set
            || phase == cache_delete_intent_sealed
            || phase == quarantine_rename_in_progress
            || phase == quarantined
            || phase == catalog_tombstoned_cleanup_pending
            || phase == abort_requested || phase == aborted;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_phase_transition(
    const epoch_catalog_operation_kind kind,
    const epoch_catalog_operation_phase from,
    const epoch_catalog_operation_phase to) noexcept -> bool
{
    if(!valid_phase(kind, from) || !valid_phase(kind, to))
    {
        return false;
    }
    if(from == to)
    {
        return true;
    }
    if(
        to == epoch_catalog_operation_phase::repair_required
        && from != epoch_catalog_operation_phase::complete
        && from != epoch_catalog_operation_phase::aborted)
    {
        return true;
    }

    switch(kind)
    {
    case epoch_catalog_operation_kind::append:
        switch(from)
        {
        case epoch_catalog_operation_phase::preparing:
            return to == epoch_catalog_operation_phase::spool_recorded
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::spool_recorded:
            return to == epoch_catalog_operation_phase::imported
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::imported:
            return to == epoch_catalog_operation_phase::staging_ready
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::staging_ready:
            return to
                    == epoch_catalog_operation_phase::commit_authorized
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::commit_authorized:
            return to
                    == epoch_catalog_operation_phase::
                        ref_commit_in_progress
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::ref_commit_in_progress:
            return to == epoch_catalog_operation_phase::refs_committed;
        case epoch_catalog_operation_phase::refs_committed:
            return to
                == epoch_catalog_operation_phase::
                    visible_cleanup_pending;
        case epoch_catalog_operation_phase::visible_cleanup_pending:
            return to == epoch_catalog_operation_phase::complete;
        case epoch_catalog_operation_phase::abort_requested:
            return to == epoch_catalog_operation_phase::aborted;
        default:
            return false;
        }
    case epoch_catalog_operation_kind::prune:
        switch(from)
        {
        case epoch_catalog_operation_phase::prune_intent:
            return to
                == epoch_catalog_operation_phase::
                    ref_delete_in_progress
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::ref_delete_in_progress:
            return to
                == epoch_catalog_operation_phase::
                    visible_cleanup_pending;
        case epoch_catalog_operation_phase::visible_cleanup_pending:
            return to == epoch_catalog_operation_phase::complete;
        case epoch_catalog_operation_phase::abort_requested:
            return to == epoch_catalog_operation_phase::aborted;
        default:
            return false;
        }
    case epoch_catalog_operation_kind::cache_delete:
        switch(from)
        {
        case epoch_catalog_operation_phase::
            cache_delete_building_exact_set:
            return to
                == epoch_catalog_operation_phase::
                    cache_delete_intent_sealed
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::cache_delete_intent_sealed:
            return to
                == epoch_catalog_operation_phase::
                    quarantine_rename_in_progress
                || to
                    == epoch_catalog_operation_phase::abort_requested;
        case epoch_catalog_operation_phase::
            quarantine_rename_in_progress:
            return to == epoch_catalog_operation_phase::quarantined;
        case epoch_catalog_operation_phase::quarantined:
            return to
                == epoch_catalog_operation_phase::
                    catalog_tombstoned_cleanup_pending;
        case epoch_catalog_operation_phase::
            catalog_tombstoned_cleanup_pending:
            return to == epoch_catalog_operation_phase::complete;
        case epoch_catalog_operation_phase::abort_requested:
            return to == epoch_catalog_operation_phase::aborted;
        default:
            return false;
        }
    }
    return false;
}

[[nodiscard]] constexpr auto generic_transition_allowed(
    const epoch_catalog_operation_kind kind,
    const epoch_catalog_operation_phase from,
    const epoch_catalog_operation_phase to) noexcept -> bool
{
    using enum epoch_catalog_operation_phase;

    if(!valid_phase_transition(kind, from, to) || from == to)
    {
        return false;
    }
    if(to == repair_required || to == abort_requested || to == aborted)
    {
        return true;
    }
    switch(kind)
    {
    case epoch_catalog_operation_kind::append:
        return (from == imported && to == staging_ready)
            || (
                from == commit_authorized
                && to == ref_commit_in_progress)
            || (
                from == ref_commit_in_progress
                && to == refs_committed)
            || (from == visible_cleanup_pending && to == complete);
    case epoch_catalog_operation_kind::prune:
        return (from == prune_intent && to == ref_delete_in_progress)
            || (from == visible_cleanup_pending && to == complete);
    case epoch_catalog_operation_kind::cache_delete:
        return (
                   from == cache_delete_intent_sealed
                   && to == quarantine_rename_in_progress)
            || (
                from == quarantine_rename_in_progress
                && to == quarantined)
            || (
                from == catalog_tombstoned_cleanup_pending
                && to == complete);
    }
    return false;
}

[[nodiscard]] constexpr auto required_transition_evidence(
    const epoch_catalog_operation_kind kind,
    const epoch_catalog_operation_phase from,
    const epoch_catalog_operation_phase to) noexcept
    -> std::optional<epoch_catalog_transition_evidence>
{
    using enum epoch_catalog_operation_phase;
    using enum epoch_catalog_transition_evidence;

    if(!valid_phase_transition(kind, from, to))
    {
        return std::nullopt;
    }
    if(from == to)
    {
        return catalog_transaction;
    }
    if(to == repair_required)
    {
        return repair_ambiguity;
    }
    if(to == staging_ready)
    {
        return exact_staging_pair;
    }
    if(to == commit_authorized)
    {
        return core_commit_authorization;
    }
    if(to == refs_committed)
    {
        return exact_final_pair;
    }
    if(
        (from == abort_requested && to == aborted)
        || (from == visible_cleanup_pending && to == complete)
        || (
            from == catalog_tombstoned_cleanup_pending
            && to == complete))
    {
        return exact_cleanup_complete;
    }
    if(
        kind == epoch_catalog_operation_kind::prune
        && from == ref_delete_in_progress
        && to == visible_cleanup_pending)
    {
        return exact_refs_absent;
    }
    if(
        kind == epoch_catalog_operation_kind::cache_delete
        && from == quarantine_rename_in_progress
        && to == quarantined)
    {
        return exact_quarantine;
    }
    return catalog_transaction;
}

struct epoch_catalog_ref_identity
{
    core::pull_request_authority_scope_id Authority;
    core::epoch_ordinal Ordinal;
    core::epoch_operation_id Operation;
    bool Staging = false;
    bool Head = false;

    auto operator<=>(const epoch_catalog_ref_identity&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const epoch_catalog_ref_identity& identity) noexcept -> bool
{
    if(!identity.Authority.valid() || !core::valid(identity.Ordinal))
    {
        return false;
    }
    return identity.Staging ? identity.Operation.valid()
                            : !identity.Operation.valid();
}

struct durable_git_spool_fact
{
    core::epoch_operation_id Operation;
    git_pack_oid ExpectedPack;
    git_pack_oid ObservedPack;
    git_artifact_fact Spool{
        .Role = git_artifact_role::completed_spool};

    auto operator<=>(const durable_git_spool_fact&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const durable_git_spool_fact& fact) noexcept -> bool
{
    return fact.Operation.valid() && application::valid(fact.ExpectedPack)
        && fact.ExpectedPack == fact.ObservedPack
        && application::valid(
            fact.Spool,
            git_artifact_role::completed_spool)
        && fact.Spool.Bytes <= maximum_git_cache_operation_pack_bytes;
}

[[nodiscard]] constexpr auto durable_fact(
    const completed_git_pack_receipt& receipt) noexcept
    -> durable_git_spool_fact
{
    return {
        .Operation = receipt.Operation,
        .ExpectedPack = receipt.ExpectedPack,
        .ObservedPack = receipt.ObservedPack,
        .Spool = receipt.Spool,
    };
}

struct durable_git_preimport_fact
{
    core::epoch_operation_id Operation;
    git_pack_oid ExpectedPack;
    std::vector<git_pack_inventory_entry> Packs;
    std::uint64_t RepositoryBytes = 0U;
    bool ExpectedPackPreexisting = false;

    auto operator<=>(const durable_git_preimport_fact&) const = default;
};

[[nodiscard]] inline auto valid(
    const durable_git_preimport_fact& fact) noexcept -> bool
{
    if(
        !fact.Operation.valid() || !application::valid(fact.ExpectedPack)
        || fact.Packs.size() > maximum_git_cache_pack_count
        || fact.RepositoryBytes > maximum_git_repository_cache_bytes)
    {
        return false;
    }

    auto expected_present = false;
    for(std::size_t index = 0U; index < fact.Packs.size(); ++index)
    {
        const auto& pack = fact.Packs[index];
        if(
            !application::valid(pack)
            || pack.PackBytes > fact.RepositoryBytes
            || pack.IndexBytes > fact.RepositoryBytes
            || pack.Pack.Algorithm != fact.ExpectedPack.Algorithm
            || (
                index != 0U
                && !(fact.Packs[index - 1U].Pack < pack.Pack)))
        {
            return false;
        }
        expected_present =
            expected_present || pack.Pack == fact.ExpectedPack;
    }
    return expected_present == fact.ExpectedPackPreexisting;
}

[[nodiscard]] inline auto durable_fact(
    const git_preimport_observation& observation)
    -> durable_git_preimport_fact
{
    return {
        .Operation = observation.Operation,
        .ExpectedPack = observation.ExpectedPack,
        .Packs = observation.Packs,
        .RepositoryBytes = observation.RepositoryBytes,
        .ExpectedPackPreexisting =
            observation.ExpectedPackPreexisting,
    };
}

struct durable_git_install_fact
{
    core::epoch_operation_id Operation;
    git_pack_oid Pack;
    git_artifact_fact InstalledPack{
        .Role = git_artifact_role::installed_pack};
    git_artifact_fact InstalledIndex{
        .Role = git_artifact_role::installed_index};
    core::git_commit_oid Base;
    core::git_tree_oid BaseTree;
    core::git_commit_oid Head;
    core::git_tree_oid HeadTree;

    auto operator<=>(const durable_git_install_fact&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const durable_git_install_fact& fact) noexcept -> bool
{
    return fact.Operation.valid() && application::valid(fact.Pack)
        && application::valid(
            fact.InstalledPack,
            git_artifact_role::installed_pack)
        && application::valid(
            fact.InstalledIndex,
            git_artifact_role::installed_index)
        && fact.InstalledPack.Bytes
            <= maximum_git_cache_operation_pack_bytes
        && fact.InstalledIndex.Bytes
            <= maximum_git_repository_cache_bytes
        && fact.Base.valid() && fact.BaseTree.valid()
        && fact.Head.valid() && fact.HeadTree.valid()
        && fact.Base.algorithm() == fact.Pack.Algorithm
        && fact.Base.algorithm() == fact.BaseTree.algorithm()
        && fact.Base.algorithm() == fact.Head.algorithm()
        && fact.Base.algorithm() == fact.HeadTree.algorithm();
}

[[nodiscard]] constexpr auto durable_fact(
    const git_prepared_revision& revision) noexcept
    -> durable_git_install_fact
{
    return {
        .Operation = revision.Operation,
        .Pack = revision.Pack,
        .InstalledPack = revision.InstalledPack,
        .InstalledIndex = revision.InstalledIndex,
        .Base = revision.Base,
        .BaseTree = revision.BaseTree,
        .Head = revision.Head,
        .HeadTree = revision.HeadTree,
    };
}

[[nodiscard]] constexpr auto valid(
    const core::catalog_review_association& review) noexcept -> bool
{
    return review.Association.valid() && review.Review.value() != 0U
        && review.Actor.value() != 0U && core::valid(review.Order)
        && review.State != core::github_review_state::pending
        && static_cast<std::uint8_t>(review.State)
            <= static_cast<std::uint8_t>(
                core::github_review_state::dismissed)
        && review.Commit.valid()
        && static_cast<std::uint8_t>(review.Availability)
            <= static_cast<std::uint8_t>(
                core::anchor_availability::never_observed);
}

[[nodiscard]] constexpr auto valid(
    const core::review_epoch_summary& epoch) noexcept -> bool
{
    if(
        !core::valid(epoch.Id) || epoch.Source.Snapshot.value() == 0U
        || epoch.Source.IdentityGeneration.value() == 0U
        || epoch.Source.PullRequestGeneration.value() == 0U
        || !epoch.BaseBranch.valid() || !epoch.Base.valid()
        || !epoch.Head.valid()
        || epoch.Base.algorithm() != epoch.Head.algorithm()
        || !core::valid(epoch.ObservedAt)
        || static_cast<std::uint8_t>(epoch.Cause.Origin)
            > static_cast<std::uint8_t>(
                core::epoch_origin::explicit_historical_import)
        || static_cast<std::uint8_t>(epoch.Cause.Base)
            > static_cast<std::uint8_t>(
                core::epoch_base_transition::base_branch_changed)
        || static_cast<std::uint8_t>(epoch.Cause.Head)
            > static_cast<std::uint8_t>(
                core::epoch_head_transition::lineage_unknown)
        || static_cast<std::uint8_t>(epoch.Cause.Material)
            > static_cast<std::uint8_t>(
                core::epoch_material_relation::unknown)
        || static_cast<std::uint8_t>(epoch.Cause.Primary)
            > static_cast<std::uint8_t>(
                core::epoch_primary_cause::base_update)
        || static_cast<std::uint8_t>(epoch.BaseAvailability)
            > static_cast<std::uint8_t>(
                core::anchor_availability::never_observed)
        || static_cast<std::uint8_t>(epoch.HeadAvailability)
            > static_cast<std::uint8_t>(
                core::anchor_availability::never_observed))
    {
        return false;
    }
    return !epoch.Material
        || (
            epoch.Material->BaseTree.valid()
            && epoch.Material->HeadTree.valid()
            && epoch.Material->PolicyRevision.value() != 0U
            && epoch.Material->BaseTree.algorithm()
                == epoch.Base.algorithm()
            && epoch.Material->HeadTree.algorithm()
                == epoch.Base.algorithm());
}

struct append_operation_context
{
    core::application_session_id ApplicationSession;
    core::epoch_request_id Request;
    core::epoch_generation Generation;
    core::epoch_operation_id Operation;
    core::accepted_pull_request_revision Revision;
    core::epoch_catalog_identity Catalog;

    auto operator<=>(const append_operation_context&) const = default;
};

[[nodiscard]] inline auto valid(
    const append_operation_context& context) noexcept -> bool
{
    return context.ApplicationSession.value() != 0U
        && context.Request.value() != 0U
        && context.Generation.value() != 0U
        && context.Operation.valid() && core::valid(context.Revision)
        && core::valid(context.Catalog);
}

struct append_reservation_request
{
    append_operation_context Context;
    core::epoch_ordinal ProposedOrdinal;

    auto operator<=>(const append_reservation_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const append_reservation_request& request) noexcept -> bool
{
    return valid(request.Context) && core::valid(request.ProposedOrdinal);
}

struct append_operation_record
{
    append_operation_context Context;
    core::epoch_ordinal ReservedOrdinal;
    epoch_catalog_operation_revision OperationRevision;
    epoch_catalog_operation_phase Phase =
        epoch_catalog_operation_phase::preparing;
    epoch_catalog_ref_identity StagingBase;
    epoch_catalog_ref_identity StagingHead;
    epoch_catalog_ref_identity FinalBase;
    epoch_catalog_ref_identity FinalHead;
    std::optional<durable_git_spool_fact> Spool;
    std::optional<durable_git_preimport_fact> Preimport;
    std::optional<durable_git_install_fact> Install;
    std::optional<core::append_epoch_mutation> Candidate;

    auto operator<=>(const append_operation_record&) const = default;
};

[[nodiscard]] inline auto valid(
    const append_operation_record& record) noexcept -> bool
{
    if(
        !valid(record.Context) || !core::valid(record.ReservedOrdinal)
        || !application::valid(record.OperationRevision)
        || !valid_phase(
            epoch_catalog_operation_kind::append,
            record.Phase)
        || !application::valid(record.StagingBase)
        || !application::valid(record.StagingHead)
        || !application::valid(record.FinalBase)
        || !application::valid(record.FinalHead))
    {
        return false;
    }
    const auto& operation = record.Context.Operation;
    const auto same_ref_owner =
        record.StagingBase.Authority
            == record.StagingHead.Authority
        && record.StagingBase.Authority
            == record.FinalBase.Authority
        && record.StagingBase.Authority
            == record.FinalHead.Authority
        && record.StagingBase.Ordinal == record.ReservedOrdinal
        && record.StagingHead.Ordinal == record.ReservedOrdinal
        && record.FinalBase.Ordinal == record.ReservedOrdinal
        && record.FinalHead.Ordinal == record.ReservedOrdinal
        && record.StagingBase.Operation == operation
        && record.StagingHead.Operation == operation
        && record.StagingBase.Staging && record.StagingHead.Staging
        && !record.FinalBase.Staging && !record.FinalHead.Staging
        && !record.StagingBase.Head && record.StagingHead.Head
        && !record.FinalBase.Head && record.FinalHead.Head;
    if(!same_ref_owner)
    {
        return false;
    }
    if(
        record.Spool
        && (
            !application::valid(*record.Spool)
            || record.Spool->Operation != operation))
    {
        return false;
    }
    if(
        record.Preimport
        && (
            !application::valid(*record.Preimport)
            || record.Preimport->Operation != operation))
    {
        return false;
    }
    if(
        record.Install
        && (
            !application::valid(*record.Install)
            || record.Install->Operation != operation))
    {
        return false;
    }
    if(record.Candidate)
    {
        if(
            !application::valid(record.Candidate->Epoch)
            || record.Candidate->Epoch.Id.Authority
                != record.StagingBase.Authority
            || record.Candidate->Epoch.Id.Incarnation
                != record.Context.Catalog.Incarnation
            || record.Candidate->Epoch.Id.Ordinal
                != record.ReservedOrdinal
            || (
                record.Candidate->Review
                && !application::valid(
                    *record.Candidate->Review)))
        {
            return false;
        }
    }
    return (!record.Spool && !record.Preimport)
        || (
            record.Spool && record.Preimport
            && record.Spool->ExpectedPack
                == record.Preimport->ExpectedPack);
}

struct record_spool_request
{
    core::epoch_catalog_identity Catalog;
    core::epoch_operation_id Operation;
    epoch_catalog_operation_revision ExpectedOperationRevision;
    durable_git_spool_fact Spool;
    durable_git_preimport_fact Preimport;

    auto operator<=>(const record_spool_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const record_spool_request& request) noexcept -> bool
{
    return core::valid(request.Catalog) && request.Operation.valid()
        && application::can_advance(request.ExpectedOperationRevision)
        && application::valid(request.Spool)
        && application::valid(request.Preimport)
        && request.Spool.Operation == request.Operation
        && request.Preimport.Operation == request.Operation
        && request.Spool.ExpectedPack
            == request.Preimport.ExpectedPack;
}

struct record_install_request
{
    core::epoch_catalog_identity Catalog;
    core::epoch_operation_id Operation;
    epoch_catalog_operation_revision ExpectedOperationRevision;
    durable_git_install_fact Install;

    auto operator<=>(const record_install_request&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const record_install_request& request) noexcept -> bool
{
    return core::valid(request.Catalog) && request.Operation.valid()
        && application::can_advance(request.ExpectedOperationRevision)
        && application::valid(request.Install)
        && request.Install.Operation == request.Operation;
}

struct record_candidate_request
{
    core::epoch_catalog_identity Catalog;
    core::epoch_operation_id Operation;
    epoch_catalog_operation_revision ExpectedOperationRevision;
    core::append_epoch_mutation Candidate;

    auto operator<=>(const record_candidate_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const record_candidate_request& request) noexcept -> bool
{
    return core::valid(request.Catalog) && request.Operation.valid()
        && application::can_advance(request.ExpectedOperationRevision)
        && application::valid(request.Candidate.Epoch)
        && (
            !request.Candidate.Review
            || application::valid(*request.Candidate.Review))
        && request.Candidate.Epoch.Id.Incarnation
            == request.Catalog.Incarnation;
}

struct operation_transition_request
{
    core::epoch_catalog_identity Catalog;
    core::epoch_operation_id Operation;
    epoch_catalog_operation_kind Kind =
        epoch_catalog_operation_kind::append;
    epoch_catalog_operation_phase Expected =
        epoch_catalog_operation_phase::preparing;
    epoch_catalog_operation_phase Next =
        epoch_catalog_operation_phase::preparing;
    epoch_catalog_transition_evidence Evidence =
        epoch_catalog_transition_evidence::catalog_transaction;
    epoch_catalog_operation_revision ExpectedOperationRevision;

    auto operator<=>(const operation_transition_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const operation_transition_request& request) noexcept -> bool
{
    const auto required = required_transition_evidence(
        request.Kind,
        request.Expected,
        request.Next);
    return core::valid(request.Catalog) && request.Operation.valid()
        && application::can_advance(request.ExpectedOperationRevision)
        && generic_transition_allowed(
            request.Kind,
            request.Expected,
            request.Next)
        && required && request.Evidence == *required;
}

struct finalize_append_request
{
    core::epoch_catalog_identity Catalog;
    core::epoch_operation_id Operation;
    epoch_catalog_operation_revision ExpectedOperationRevision;
    core::append_epoch_mutation Mutation;
    git_retained_ref_pair ObservedRefs;

    auto operator<=>(const finalize_append_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const finalize_append_request& request) noexcept -> bool
{
    const auto& epoch = request.Mutation.Epoch;
    return core::valid(request.Catalog) && request.Operation.valid()
        && application::can_advance(request.ExpectedOperationRevision)
        && application::valid(epoch)
        && epoch.Id.Incarnation == request.Catalog.Incarnation
        && (
            !request.Mutation.Review
            || application::valid(*request.Mutation.Review))
        && application::valid(request.ObservedRefs)
        && request.ObservedRefs.Scope == epoch.Id.Authority
        && request.ObservedRefs.Ordinal == epoch.Id.Ordinal
        && request.ObservedRefs.Base == epoch.Base
        && request.ObservedRefs.Head == epoch.Head;
}

struct associate_review_request
{
    core::epoch_catalog_identity Catalog;
    core::catalog_review_association Review;
    core::provider_snapshot_id AcceptedSnapshot;

    auto operator<=>(const associate_review_request&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const associate_review_request& request) noexcept -> bool
{
    return core::valid(request.Catalog)
        && application::valid(request.Review)
        && request.AcceptedSnapshot.value() != 0U;
}

struct catalog_commit_result
{
    core::epoch_catalog_identity Catalog;
    bool Changed = false;
    std::optional<core::review_epoch_id> InsertedEpoch;

    auto operator<=>(const catalog_commit_result&) const = default;
};

struct prune_epoch_fact
{
    core::review_epoch_id Epoch;
    git_retained_ref_pair Retained;
    epoch_catalog_ref_identity BaseRef;
    epoch_catalog_ref_identity HeadRef;

    auto operator<=>(const prune_epoch_fact&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const prune_epoch_fact& fact) noexcept -> bool
{
    return core::valid(fact.Epoch)
        && application::valid(fact.Retained)
        && application::valid(fact.BaseRef)
        && application::valid(fact.HeadRef)
        && fact.Retained.Scope == fact.Epoch.Authority
        && fact.Retained.Ordinal == fact.Epoch.Ordinal
        && fact.BaseRef.Authority == fact.Epoch.Authority
        && fact.HeadRef.Authority == fact.Epoch.Authority
        && fact.BaseRef.Ordinal == fact.Epoch.Ordinal
        && fact.HeadRef.Ordinal == fact.Epoch.Ordinal
        && !fact.BaseRef.Staging && !fact.HeadRef.Staging
        && !fact.BaseRef.Head && fact.HeadRef.Head;
}

struct begin_prune_request
{
    core::epoch_catalog_identity Catalog;
    core::epoch_operation_id Operation;
    std::vector<prune_epoch_fact> Epochs;

    auto operator<=>(const begin_prune_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const begin_prune_request& request) noexcept -> bool
{
    if(
        !core::valid(request.Catalog) || !request.Operation.valid()
        || request.Epochs.empty()
        || request.Epochs.size() > maximum_prune_epoch_count)
    {
        return false;
    }
    for(std::size_t index = 0U; index < request.Epochs.size(); ++index)
    {
        const auto& epoch = request.Epochs[index];
        if(
            !application::valid(epoch)
            || epoch.Epoch.Incarnation
                != request.Catalog.Incarnation
            || (
                index != 0U
                && !(request.Epochs[index - 1U].Epoch
                    < epoch.Epoch)))
        {
            return false;
        }
    }
    return true;
}

struct cache_delete_scope_fact
{
    core::pull_request_authority_scope_id Authority;
    core::epoch_catalog_revision ExpectedRevision;

    auto operator<=>(const cache_delete_scope_fact&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const cache_delete_scope_fact& fact) noexcept -> bool
{
    return fact.Authority.valid() && core::valid(fact.ExpectedRevision);
}

struct cache_delete_impact
{
    core::epoch_catalog_incarnation_id Incarnation;
    core::repository_cache_scope_id Cache;
    std::vector<cache_delete_scope_fact> Scopes;
    std::uint64_t RetainedEpochCount = 0U;

    auto operator<=>(const cache_delete_impact&) const = default;
};

[[nodiscard]] inline auto valid(
    const cache_delete_impact& impact) noexcept -> bool
{
    if(
        !impact.Incarnation.valid() || !impact.Cache.valid()
        || impact.Scopes.empty()
        || impact.Scopes.size() > maximum_cache_delete_scope_count)
    {
        return false;
    }
    for(std::size_t index = 0U; index < impact.Scopes.size(); ++index)
    {
        if(
            !application::valid(impact.Scopes[index])
            || (
                index != 0U
                && !(impact.Scopes[index - 1U].Authority
                    < impact.Scopes[index].Authority)))
        {
            return false;
        }
    }
    return true;
}

struct begin_cache_delete_request
{
    core::epoch_operation_id Operation;
    cache_delete_impact Impact;

    auto operator<=>(const begin_cache_delete_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const begin_cache_delete_request& request) noexcept -> bool
{
    return request.Operation.valid() && application::valid(request.Impact);
}

struct maintenance_operation_record
{
    core::epoch_catalog_incarnation_id Incarnation;
    core::epoch_operation_id Operation;
    epoch_catalog_operation_kind Kind =
        epoch_catalog_operation_kind::prune;
    epoch_catalog_operation_phase Phase =
        epoch_catalog_operation_phase::prune_intent;
    epoch_catalog_operation_revision OperationRevision;
    core::repository_cache_scope_id Cache;
    std::vector<prune_epoch_fact> Epochs;
    std::vector<cache_delete_scope_fact> Scopes;

    auto operator<=>(const maintenance_operation_record&) const = default;
};

[[nodiscard]] inline auto valid(
    const maintenance_operation_record& record) noexcept -> bool
{
    if(
        !record.Incarnation.valid() || !record.Operation.valid()
        || !application::valid(record.OperationRevision)
        || !valid_phase(record.Kind, record.Phase))
    {
        return false;
    }
    if(record.Kind == epoch_catalog_operation_kind::prune)
    {
        if(!record.Cache.valid() || record.Epochs.empty()
           || !record.Scopes.empty()
           || record.Epochs.size() > maximum_prune_epoch_count)
        {
            return false;
        }
        for(std::size_t index = 0U; index < record.Epochs.size(); ++index)
        {
            if(
                !application::valid(record.Epochs[index])
                || (
                    index != 0U
                    && !(record.Epochs[index - 1U].Epoch
                        < record.Epochs[index].Epoch)))
            {
                return false;
            }
        }
        return true;
    }
    if(record.Kind == epoch_catalog_operation_kind::cache_delete)
    {
        if(!record.Cache.valid() || !record.Epochs.empty()
           || record.Scopes.empty()
           || record.Scopes.size() > maximum_cache_delete_scope_count)
        {
            return false;
        }
        for(std::size_t index = 0U; index < record.Scopes.size(); ++index)
        {
            if(
                !application::valid(record.Scopes[index])
                || (
                    index != 0U
                    && !(record.Scopes[index - 1U].Authority
                        < record.Scopes[index].Authority)))
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

struct finalize_prune_request
{
    operation_transition_request Transition;
    std::vector<git_retained_ref_pair> ObservedAbsent;

    auto operator<=>(const finalize_prune_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const finalize_prune_request& request) noexcept -> bool
{
    if(
        !core::valid(request.Transition.Catalog)
        || !request.Transition.Operation.valid()
        || !application::can_advance(
            request.Transition.ExpectedOperationRevision)
        || request.Transition.Kind
            != epoch_catalog_operation_kind::prune
        || request.Transition.Expected
            != epoch_catalog_operation_phase::
                ref_delete_in_progress
        || request.Transition.Next
            != epoch_catalog_operation_phase::
                visible_cleanup_pending
        || request.Transition.Evidence
            != epoch_catalog_transition_evidence::exact_refs_absent
        || request.ObservedAbsent.empty()
        || request.ObservedAbsent.size() > maximum_prune_epoch_count)
    {
        return false;
    }
    for(std::size_t index = 0U; index < request.ObservedAbsent.size(); ++index)
    {
        if(
            !application::valid(request.ObservedAbsent[index])
            || (
                index != 0U
                && !(
                    request.ObservedAbsent[index - 1U].Ordinal
                    < request.ObservedAbsent[index].Ordinal)))
        {
            return false;
        }
    }
    return true;
}

struct finalize_cache_delete_request
{
    operation_transition_request Transition;

    auto operator<=>(const finalize_cache_delete_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const finalize_cache_delete_request& request) noexcept -> bool
{
    return core::valid(request.Transition.Catalog)
        && request.Transition.Operation.valid()
        && application::can_advance(
            request.Transition.ExpectedOperationRevision)
        && request.Transition.Kind
            == epoch_catalog_operation_kind::cache_delete
        && request.Transition.Expected
            == epoch_catalog_operation_phase::quarantined
        && request.Transition.Next
            == epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending
        && request.Transition.Evidence
            == epoch_catalog_transition_evidence::
                catalog_transaction;
}

using epoch_recovery_operation =
    std::variant<append_operation_record, maintenance_operation_record>;

struct epoch_recovery_page
{
    core::epoch_catalog_incarnation_id Incarnation;
    std::vector<epoch_recovery_operation> Operations;
    std::optional<core::epoch_operation_id> Continuation;

    auto operator<=>(const epoch_recovery_page&) const = default;
};

[[nodiscard]] inline auto valid(
    const epoch_recovery_page& page) noexcept -> bool
{
    if(
        !page.Incarnation.valid()
        || page.Operations.size() > maximum_epoch_recovery_page_size
        || (
            page.Continuation
            && !page.Continuation->valid()))
    {
        return false;
    }

    std::optional<core::epoch_operation_id> prior;
    for(const auto& operation : page.Operations)
    {
        const auto current = std::visit(
            [](const auto& value) {
                if constexpr(
                    std::is_same_v<
                        std::remove_cvref_t<decltype(value)>,
                        append_operation_record>)
                {
                    return value.Context.Operation;
                }
                else
                {
                    return value.Operation;
                }
            },
            operation);
        const auto record_valid = std::visit(
            [](const auto& value) {
                return application::valid(value);
            },
            operation);
        const auto incarnation = std::visit(
            [](const auto& value) {
                if constexpr(
                    std::is_same_v<
                        std::remove_cvref_t<decltype(value)>,
                        append_operation_record>)
                {
                    return value.Context.Catalog.Incarnation;
                }
                else
                {
                    return value.Incarnation;
                }
            },
            operation);
        if(
            !record_valid || incarnation != page.Incarnation
            || (prior && !(*prior < current)))
        {
            return false;
        }
        prior = current;
    }
    return !page.Continuation
        || !prior || *prior < *page.Continuation;
}

enum class epoch_catalog_failure : std::uint8_t
{
    invalid_request,
    invalid_path,
    invalid_permissions,
    unavailable,
    schema_mismatch,
    integrity_failure,
    authority_mismatch,
    catalog_mismatch,
    revision_conflict,
    operation_conflict,
    phase_conflict,
    quota_exceeded,
    ordinal_exhausted,
    busy,
    repair_required,
    cancelled,
    platform_unsupported,
};

template<typename Value>
using epoch_catalog_result =
    std::expected<Value, epoch_catalog_failure>;

class epoch_catalog_session
{
public:
    epoch_catalog_session() = default;
    epoch_catalog_session(const epoch_catalog_session&) = delete;
    auto operator=(const epoch_catalog_session&)
        -> epoch_catalog_session& = delete;
    epoch_catalog_session(epoch_catalog_session&&) = delete;
    auto operator=(epoch_catalog_session&&)
        -> epoch_catalog_session& = delete;
    virtual ~epoch_catalog_session() = default;

    [[nodiscard]] virtual auto binding() const noexcept
        -> const epoch_catalog_binding& = 0;

    [[nodiscard]] virtual auto snapshot(std::stop_token stop)
        -> epoch_catalog_result<epoch_catalog_snapshot> = 0;

    [[nodiscard]] virtual auto exact_anchors(
        const exact_anchor_lookup_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<exact_anchor_lookup_result> = 0;

    [[nodiscard]] virtual auto history(
        const epoch_history_page_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<epoch_history_page> = 0;

    [[nodiscard]] virtual auto mark_anchor_availability(
        const mark_anchor_availability_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<catalog_commit_result> = 0;

    [[nodiscard]] virtual auto reserve_append(
        const append_reservation_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<append_operation_record> = 0;

    [[nodiscard]] virtual auto record_spool(
        const record_spool_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<append_operation_record> = 0;

    [[nodiscard]] virtual auto record_install(
        const record_install_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<append_operation_record> = 0;

    [[nodiscard]] virtual auto record_candidate(
        const record_candidate_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<append_operation_record> = 0;

    [[nodiscard]] virtual auto transition_operation(
        const operation_transition_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<epoch_recovery_operation> = 0;

    [[nodiscard]] virtual auto finalize_append(
        const finalize_append_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<catalog_commit_result> = 0;

    [[nodiscard]] virtual auto associate_review(
        const associate_review_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<catalog_commit_result> = 0;

    [[nodiscard]] virtual auto begin_prune(
        const begin_prune_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<maintenance_operation_record> = 0;

    [[nodiscard]] virtual auto finalize_prune(
        const finalize_prune_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<catalog_commit_result> = 0;

    [[nodiscard]] virtual auto begin_cache_delete(
        const begin_cache_delete_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<maintenance_operation_record> = 0;

    [[nodiscard]] virtual auto cache_delete_impact(
        core::repository_cache_scope_id cache,
        std::stop_token stop)
        -> epoch_catalog_result<application::cache_delete_impact> = 0;

    [[nodiscard]] virtual auto finalize_cache_delete(
        const finalize_cache_delete_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<void> = 0;

    [[nodiscard]] virtual auto recovery_page(
        std::optional<core::epoch_operation_id> after,
        std::uint16_t limit,
        std::stop_token stop)
        -> epoch_catalog_result<epoch_recovery_page> = 0;
};

class epoch_catalog_port
{
public:
    epoch_catalog_port() = default;
    epoch_catalog_port(const epoch_catalog_port&) = delete;
    auto operator=(const epoch_catalog_port&)
        -> epoch_catalog_port& = delete;
    epoch_catalog_port(epoch_catalog_port&&) = delete;
    auto operator=(epoch_catalog_port&&)
        -> epoch_catalog_port& = delete;
    virtual ~epoch_catalog_port() = default;

    [[nodiscard]] virtual auto open(
        const epoch_catalog_open_request& request,
        std::stop_token stop)
        -> epoch_catalog_result<std::unique_ptr<epoch_catalog_session>> = 0;
};
} // namespace bpr::application
