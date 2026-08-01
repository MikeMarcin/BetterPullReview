#pragma once

#include "bpr/application/git_pack_stream.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

namespace bpr::application
{
inline constexpr std::uint64_t maximum_git_cache_operation_pack_bytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t maximum_git_repository_cache_bytes =
    64ULL * 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::uint32_t maximum_git_cache_pack_count = 4096U;
inline constexpr std::uint32_t maximum_git_cache_private_ref_count = 200'000U;

[[nodiscard]] constexpr auto valid_cache_token(
    const std::array<std::uint8_t, 32U>& token) noexcept -> bool
{
    for(const auto byte : token)
    {
        if(byte != 0U)
        {
            return true;
        }
    }
    return false;
}

struct git_cache_limits
{
    std::uint64_t OperationPackBytes =
        maximum_git_cache_operation_pack_bytes;
    std::uint64_t RepositoryBytes =
        maximum_git_repository_cache_bytes;
    std::uint32_t PackCount = maximum_git_cache_pack_count;
    std::uint32_t PrivateRefCount =
        maximum_git_cache_private_ref_count;

    auto operator<=>(const git_cache_limits&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_cache_limits& limits) noexcept -> bool
{
    return limits.OperationPackBytes != 0U
        && limits.OperationPackBytes
            <= maximum_git_cache_operation_pack_bytes
        && limits.RepositoryBytes >= limits.OperationPackBytes
        && limits.RepositoryBytes
            <= maximum_git_repository_cache_bytes
        && limits.PackCount != 0U
        && limits.PackCount <= maximum_git_cache_pack_count
        && limits.PrivateRefCount >= 2U
        && limits.PrivateRefCount
            <= maximum_git_cache_private_ref_count;
}

struct git_cache_open_request
{
    core::application_session_id ApplicationSession;
    core::pull_request_authority Authority;
    core::git_hash_algorithm ObjectFormat =
        core::git_hash_algorithm::invalid;
    git_cache_limits Limits;

    auto operator<=>(const git_cache_open_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const git_cache_open_request& request) noexcept -> bool
{
    return request.ApplicationSession.value() != 0U
        && core::valid(request.Authority)
        && (request.ObjectFormat == core::git_hash_algorithm::sha1
            || request.ObjectFormat == core::git_hash_algorithm::sha256)
        && valid(request.Limits);
}

struct git_pack_inventory_entry
{
    git_pack_oid Pack;
    std::uint64_t PackBytes = 0U;
    std::uint64_t IndexBytes = 0U;

    auto operator<=>(const git_pack_inventory_entry&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_pack_inventory_entry& entry) noexcept -> bool
{
    return valid(entry.Pack) && entry.PackBytes != 0U
        && entry.IndexBytes != 0U;
}

struct git_preimport_observation
{
    core::epoch_operation_id Operation;
    git_pack_oid ExpectedPack;
    std::vector<git_pack_inventory_entry> Packs;
    std::uint64_t RepositoryBytes = 0U;
    bool ExpectedPackPreexisting = false;
    std::array<std::uint8_t, 32U> OpaqueToken{};

    auto operator<=>(const git_preimport_observation&) const = default;
};

[[nodiscard]] inline auto valid(
    const git_preimport_observation& observation) noexcept -> bool
{
    if(
        !observation.Operation.valid()
        || !valid(observation.ExpectedPack)
        || observation.Packs.size() > maximum_git_cache_pack_count
        || observation.RepositoryBytes
            > maximum_git_repository_cache_bytes
        || !valid_cache_token(observation.OpaqueToken))
    {
        return false;
    }
    auto expected_pack_present = false;
    std::uint64_t inventory_bytes = 0U;
    for(std::size_t index = 0U; index < observation.Packs.size(); ++index)
    {
        const auto& entry = observation.Packs[index];
        if(
            !valid(entry)
            || entry.PackBytes
                > observation.RepositoryBytes - inventory_bytes
            || entry.IndexBytes
                > observation.RepositoryBytes
                    - inventory_bytes - entry.PackBytes
            || entry.Pack.Algorithm
                != observation.ExpectedPack.Algorithm
            || (
                index != 0U
                && !(observation.Packs[index - 1U].Pack
                    < entry.Pack)))
        {
            return false;
        }
        inventory_bytes += entry.PackBytes + entry.IndexBytes;
        expected_pack_present =
            expected_pack_present
            || entry.Pack == observation.ExpectedPack;
    }
    return expected_pack_present
        == observation.ExpectedPackPreexisting;
}

struct git_import_authorization
{
    core::epoch_catalog_incarnation_id CatalogIncarnation;
    core::epoch_catalog_revision ExpectedCatalogRevision;
    core::epoch_operation_id Operation;
    git_pack_oid ExpectedPack;
    std::array<std::uint8_t, 32U> ReceiptToken{};
    std::array<std::uint8_t, 32U> InventoryToken{};
    std::array<std::uint8_t, 32U> DurableRecordToken{};

    auto operator<=>(const git_import_authorization&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_import_authorization& authorization) noexcept -> bool
{
    return authorization.CatalogIncarnation.valid()
        && core::valid(authorization.ExpectedCatalogRevision)
        && authorization.Operation.valid()
        && valid(authorization.ExpectedPack)
        && valid_cache_token(authorization.ReceiptToken)
        && valid_cache_token(authorization.InventoryToken)
        && valid_cache_token(authorization.DurableRecordToken);
}

struct git_prepare_revision_request
{
    completed_git_pack_receipt Receipt;
    git_preimport_observation Inventory;
    git_import_authorization Authorization;
    core::git_commit_oid Base;
    core::git_commit_oid Head;
    std::optional<core::git_commit_oid> PriorHead;

    auto operator<=>(const git_prepare_revision_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const git_prepare_revision_request& request) noexcept -> bool
{
    return agrees(request.Receipt) && valid(request.Inventory)
        && valid(request.Authorization) && request.Base.valid()
        && request.Head.valid()
        && request.Base.algorithm() == request.Head.algorithm()
        && (
            !request.PriorHead.has_value()
            || (
                request.PriorHead->valid()
                && request.PriorHead->algorithm()
                    == request.Head.algorithm()))
        && request.Base.algorithm()
            == request.Receipt.ExpectedPack.Algorithm
        && request.Receipt.Operation == request.Inventory.Operation
        && request.Receipt.Operation
            == request.Authorization.Operation
        && request.Receipt.ExpectedPack
            == request.Inventory.ExpectedPack
        && request.Receipt.ExpectedPack
            == request.Authorization.ExpectedPack
        && request.Receipt.OpaqueToken
            == request.Authorization.ReceiptToken
        && request.Inventory.OpaqueToken
            == request.Authorization.InventoryToken;
}

struct git_prepared_revision
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
    core::head_lineage HeadLineage =
        core::head_lineage::not_applicable;
    std::uint64_t ImportedBytes = 0U;

    auto operator<=>(const git_prepared_revision&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_prepared_revision& revision) noexcept -> bool
{
    return revision.Operation.valid() && valid(revision.Pack)
        && valid(
            revision.InstalledPack,
            git_artifact_role::installed_pack)
        && valid(
            revision.InstalledIndex,
            git_artifact_role::installed_index)
        && revision.Base.valid() && revision.BaseTree.valid()
        && revision.Head.valid() && revision.HeadTree.valid()
        && revision.Base.algorithm() == revision.Pack.Algorithm
        && revision.Base.algorithm() == revision.BaseTree.algorithm()
        && revision.Base.algorithm() == revision.Head.algorithm()
        && revision.Base.algorithm() == revision.HeadTree.algorithm()
        && (
            revision.HeadLineage
                == core::head_lineage::not_applicable
            || revision.HeadLineage
                == core::head_lineage::descendant
            || revision.HeadLineage
                == core::head_lineage::non_fast_forward
            || revision.HeadLineage
                == core::head_lineage::unknown)
        && revision.ImportedBytes != 0U
        && revision.ImportedBytes
            == revision.InstalledPack.Bytes;
}

// Durable catalog facts are projected into this Git-owned recovery contract.
// The cache never interprets a physical final ref as catalog visibility or
// durable success; it only reports exact physical evidence to the host.
struct git_recovery_spool_expectation
{
    git_pack_oid ExpectedPack;
    git_artifact_fact CompletedSpool{
        .Role = git_artifact_role::completed_spool};
    std::vector<git_pack_inventory_entry> PreimportPacks;
    std::uint64_t PreimportRepositoryBytes = 0U;
    bool ExpectedPackPreexisting = false;

    auto operator<=>(const git_recovery_spool_expectation&) const =
        default;
};

[[nodiscard]] inline auto valid(
    const git_recovery_spool_expectation& expectation) noexcept -> bool
{
    if(
        !valid(expectation.ExpectedPack)
        || !valid(
            expectation.CompletedSpool,
            git_artifact_role::completed_spool)
        || expectation.CompletedSpool.Bytes
            > maximum_git_cache_operation_pack_bytes
        || expectation.PreimportPacks.size()
            > maximum_git_cache_pack_count
        || expectation.PreimportRepositoryBytes
            > maximum_git_repository_cache_bytes)
    {
        return false;
    }
    auto expected_present = false;
    std::uint64_t inventory_bytes = 0U;
    for(
        std::size_t index = 0U;
        index < expectation.PreimportPacks.size();
        ++index)
    {
        const auto& entry = expectation.PreimportPacks[index];
        if(
            !valid(entry)
            || entry.PackBytes
                > expectation.PreimportRepositoryBytes - inventory_bytes
            || entry.IndexBytes
                > expectation.PreimportRepositoryBytes
                    - inventory_bytes - entry.PackBytes
            || entry.Pack.Algorithm
                != expectation.ExpectedPack.Algorithm
            || (
                index != 0U
                && !(expectation.PreimportPacks[index - 1U].Pack
                    < entry.Pack)))
        {
            return false;
        }
        inventory_bytes += entry.PackBytes + entry.IndexBytes;
        expected_present =
            expected_present || entry.Pack == expectation.ExpectedPack;
    }
    return expected_present == expectation.ExpectedPackPreexisting;
}

struct git_recovery_install_expectation
{
    git_pack_oid Pack;
    git_artifact_fact InstalledPack{
        .Role = git_artifact_role::installed_pack};
    git_artifact_fact InstalledIndex{
        .Role = git_artifact_role::installed_index};
    core::git_tree_oid BaseTree;
    core::git_tree_oid HeadTree;

    auto operator<=>(const git_recovery_install_expectation&) const =
        default;
};

[[nodiscard]] constexpr auto valid(
    const git_recovery_install_expectation& expectation) noexcept -> bool
{
    return valid(expectation.Pack)
        && valid(
            expectation.InstalledPack,
            git_artifact_role::installed_pack)
        && valid(
            expectation.InstalledIndex,
            git_artifact_role::installed_index)
        && expectation.InstalledPack.Bytes
            <= maximum_git_cache_operation_pack_bytes
        && expectation.InstalledIndex.Bytes
            <= maximum_git_repository_cache_bytes
        && expectation.BaseTree.valid() && expectation.HeadTree.valid()
        && expectation.BaseTree.algorithm() == expectation.Pack.Algorithm
        && expectation.HeadTree.algorithm() == expectation.Pack.Algorithm;
}

struct git_operation_recovery_request
{
    core::epoch_operation_id Operation;
    core::pull_request_authority_scope_id Scope;
    core::epoch_ordinal Ordinal;
    core::git_commit_oid Base;
    core::git_commit_oid Head;
    std::optional<git_recovery_spool_expectation> Spool;
    std::optional<git_recovery_install_expectation> Install;

    auto operator<=>(const git_operation_recovery_request&) const =
        default;
};

[[nodiscard]] inline auto valid(
    const git_operation_recovery_request& request) noexcept -> bool
{
    return request.Operation.valid() && request.Scope.valid()
        && core::valid(request.Ordinal) && request.Base.valid()
        && request.Head.valid()
        && request.Base.algorithm() == request.Head.algorithm()
        && (
            !request.Spool
            || (
                valid(*request.Spool)
                && request.Spool->ExpectedPack.Algorithm
                    == request.Base.algorithm()))
        && (
            !request.Install
            || (
                valid(*request.Install)
                && request.Install->Pack.Algorithm
                    == request.Base.algorithm()))
        && (
            !request.Spool || !request.Install
            || (
                request.Spool->ExpectedPack == request.Install->Pack
                && request.Spool->CompletedSpool.Bytes
                    == request.Install->InstalledPack.Bytes));
}

enum class git_recovery_match : std::uint8_t
{
    absent,
    exact,
    mismatch,
    ambiguous,
};

enum class git_recovery_object_match : std::uint8_t
{
    not_checked,
    exact,
    unavailable,
    mismatch,
};

enum class git_recovery_ref_topology : std::uint8_t
{
    absent,
    staging,
    final,
    ambiguous,
};

struct git_operation_recovery_observation
{
    core::epoch_operation_id Operation;
    git_recovery_match Spool = git_recovery_match::absent;
    git_recovery_match Installed = git_recovery_match::absent;
    git_recovery_match Staging = git_recovery_match::absent;
    git_recovery_match Final = git_recovery_match::absent;
    git_recovery_object_match Objects =
        git_recovery_object_match::not_checked;
    git_recovery_ref_topology Refs =
        git_recovery_ref_topology::absent;

    auto operator<=>(const git_operation_recovery_observation&) const =
        default;
};

struct git_retained_ref_pair
{
    core::pull_request_authority_scope_id Scope;
    core::epoch_ordinal Ordinal;
    core::git_commit_oid Base;
    core::git_commit_oid Head;

    auto operator<=>(const git_retained_ref_pair&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_retained_ref_pair& pair) noexcept -> bool
{
    return pair.Scope.valid() && core::valid(pair.Ordinal)
        && pair.Base.valid() && pair.Head.valid()
        && pair.Base.algorithm() == pair.Head.algorithm();
}

struct git_cache_commit_inspection
{
    core::git_commit_oid Commit;
    core::git_tree_oid Tree;

    auto operator<=>(const git_cache_commit_inspection&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_cache_commit_inspection& inspection) noexcept -> bool
{
    return inspection.Commit.valid() && inspection.Tree.valid()
        && inspection.Commit.algorithm() == inspection.Tree.algorithm();
}

struct git_ref_promotion_authorization
{
    core::epoch_catalog_incarnation_id CatalogIncarnation;
    core::epoch_catalog_revision ExpectedCatalogRevision;
    core::epoch_operation_id Operation;
    core::pull_request_authority_scope_id Scope;
    core::epoch_ordinal Ordinal;
    core::git_commit_oid Base;
    core::git_commit_oid Head;
    std::array<std::uint8_t, 32U> DurableCommitToken{};

    auto operator<=>(const git_ref_promotion_authorization&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_ref_promotion_authorization& authorization) noexcept -> bool
{
    return authorization.CatalogIncarnation.valid()
        && core::valid(authorization.ExpectedCatalogRevision)
        && authorization.Operation.valid() && authorization.Scope.valid()
        && core::valid(authorization.Ordinal)
        && authorization.Base.valid() && authorization.Head.valid()
        && authorization.Base.algorithm()
            == authorization.Head.algorithm()
        && valid_cache_token(authorization.DurableCommitToken);
}

struct git_ref_prune_authorization
{
    core::epoch_catalog_incarnation_id CatalogIncarnation;
    core::epoch_catalog_revision ExpectedCatalogRevision;
    git_retained_ref_pair Expected;
    std::array<std::uint8_t, 32U> DurableIntentToken{};

    auto operator<=>(const git_ref_prune_authorization&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_ref_prune_authorization& authorization) noexcept -> bool
{
    return authorization.CatalogIncarnation.valid()
        && core::valid(authorization.ExpectedCatalogRevision)
        && valid(authorization.Expected)
        && valid_cache_token(authorization.DurableIntentToken);
}

struct git_cache_delete_authorization
{
    core::application_session_id ApplicationSession;
    core::repository_cache_scope_id Cache;
    std::array<std::uint8_t, 32U> DurableIntentToken{};

    auto operator<=>(const git_cache_delete_authorization&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const git_cache_delete_authorization& authorization) noexcept -> bool
{
    return authorization.ApplicationSession.value() != 0U
        && authorization.Cache.valid()
        && valid_cache_token(authorization.DurableIntentToken);
}

enum class git_cache_failure : std::uint8_t
{
    invalid_request,
    invalid_authority,
    invalid_path,
    invalid_manifest,
    invalid_repository,
    lock_unavailable,
    quota_exceeded,
    spool_failure,
    inventory_mismatch,
    authorization_mismatch,
    process_failure,
    object_mismatch,
    ref_conflict,
    repair_required,
    cancelled,
};

template<typename Value>
using git_cache_result = std::expected<Value, git_cache_failure>;

class git_cache_session
{
public:
    git_cache_session() = default;
    git_cache_session(const git_cache_session&) = delete;
    auto operator=(const git_cache_session&) -> git_cache_session& = delete;
    git_cache_session(git_cache_session&&) = delete;
    auto operator=(git_cache_session&&) -> git_cache_session& = delete;
    virtual ~git_cache_session() = default;

    [[nodiscard]] virtual auto request() const noexcept
        -> const git_cache_open_request& = 0;

    [[nodiscard]] virtual auto create_pack_sink(
        core::epoch_operation_id operation,
        std::uint64_t maximum_bytes,
        std::stop_token stop)
        -> git_cache_result<std::unique_ptr<git_pack_stream_sink>> = 0;

    [[nodiscard]] virtual auto observe_preimport(
        const completed_git_pack_receipt& receipt,
        std::stop_token stop)
        -> git_cache_result<git_preimport_observation> = 0;

    [[nodiscard]] virtual auto prepare_revision(
        const git_prepare_revision_request& request,
        std::stop_token stop)
        -> git_cache_result<git_prepared_revision> = 0;

    [[nodiscard]] virtual auto promote_retained(
        const git_prepared_revision& prepared,
        const git_ref_promotion_authorization& authorization,
        std::stop_token stop)
        -> git_cache_result<git_retained_ref_pair> = 0;

    [[nodiscard]] virtual auto inspect_retained(
        core::epoch_ordinal ordinal,
        std::stop_token stop)
        -> git_cache_result<git_retained_ref_pair> = 0;

    // Verifies an exact commit independently of catalog availability or a
    // retained epoch ref. This is the restart-safe gate for protocol-v2 haves
    // and review-commit anchors: commit type, tree, and reachable closure are
    // checked under the exact cache writer lock.
    [[nodiscard]] virtual auto inspect_commit_object(
        core::git_commit_oid commit,
        std::stop_token stop)
        -> git_cache_result<git_cache_commit_inspection> = 0;

    [[nodiscard]] virtual auto prune_retained(
        const git_ref_prune_authorization& authorization,
        std::stop_token stop) -> git_cache_result<void> = 0;

    [[nodiscard]] virtual auto abort_operation(
        core::epoch_operation_id operation,
        std::stop_token stop) -> git_cache_result<void> = 0;

    // This is a physical recovery inspection only. The catalog phase remains
    // the sole durable visibility/success authority even when final refs are
    // exact. Every filesystem scan and Git subprocess remains bounded and
    // cancellable under the exact cache writer lock.
    [[nodiscard]] virtual auto inspect_operation_recovery(
        const git_operation_recovery_request& request,
        std::stop_token stop)
        -> git_cache_result<git_operation_recovery_observation> = 0;

    // Re-inspects under the session's operation lock and removes only an
    // exact completed spool and exact operation staging pair. Final refs,
    // installed packs/indexes, mismatches, and ambiguous residue are never
    // deleted by this operation-scoped abort.
    [[nodiscard]] virtual auto abort_recovered_operation(
        const git_operation_recovery_request& request,
        std::stop_token stop) -> git_cache_result<void> = 0;

    // Successful catalog cleanup is intentionally distinct from abort. It
    // requires the exact final pair and object closure, removes only an exact
    // completed spool/staging residue, and never marks the operation aborted
    // or consumes bounded abort-history capacity.
    [[nodiscard]] virtual auto complete_recovered_operation(
        const git_operation_recovery_request& request,
        std::stop_token stop) -> git_cache_result<void> = 0;
};

class git_cache_port
{
public:
    git_cache_port() = default;
    git_cache_port(const git_cache_port&) = delete;
    auto operator=(const git_cache_port&) -> git_cache_port& = delete;
    git_cache_port(git_cache_port&&) = delete;
    auto operator=(git_cache_port&&) -> git_cache_port& = delete;
    virtual ~git_cache_port() = default;

    [[nodiscard]] virtual auto open(
        const git_cache_open_request& request,
        std::stop_token stop)
        -> git_cache_result<std::unique_ptr<git_cache_session>> = 0;

    [[nodiscard]] virtual auto delete_cache(
        const git_cache_open_request& expected,
        const git_cache_delete_authorization& authorization,
        std::stop_token stop) -> git_cache_result<void> = 0;
};
} // namespace bpr::application
