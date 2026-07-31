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
    for(std::size_t index = 0U; index < observation.Packs.size(); ++index)
    {
        const auto& entry = observation.Packs[index];
        if(
            !valid(entry)
            || entry.Pack.Algorithm
                != observation.ExpectedPack.Algorithm
            || (
                index != 0U
                && !(observation.Packs[index - 1U].Pack
                    < entry.Pack)))
        {
            return false;
        }
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

    auto operator<=>(const git_prepare_revision_request&) const = default;
};

[[nodiscard]] inline auto valid(
    const git_prepare_revision_request& request) noexcept -> bool
{
    return agrees(request.Receipt) && valid(request.Inventory)
        && valid(request.Authorization) && request.Base.valid()
        && request.Head.valid()
        && request.Base.algorithm() == request.Head.algorithm()
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
        && revision.ImportedBytes != 0U
        && revision.ImportedBytes
            == revision.InstalledPack.Bytes;
}

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

    [[nodiscard]] virtual auto prune_retained(
        const git_ref_prune_authorization& authorization,
        std::stop_token stop) -> git_cache_result<void> = 0;

    [[nodiscard]] virtual auto abort_operation(
        core::epoch_operation_id operation,
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
