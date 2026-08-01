#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <variant>

namespace bpr::application
{
inline constexpr std::uint64_t maximum_git_pack_input_bytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t maximum_git_ref_transaction_updates = 4U;
enum class git_hash_algorithm : std::uint8_t
{
    sha1,
    sha256,
};

template<typename Kind>
struct git_object_identity
{
    git_hash_algorithm Algorithm = git_hash_algorithm::sha1;
    std::array<std::uint8_t, 32U> Digest{};

    auto operator<=>(const git_object_identity&) const = default;
};

struct git_commit_identity_kind;
struct git_tree_identity_kind;
struct git_pack_identity_kind;

using git_commit_id = git_object_identity<git_commit_identity_kind>;
using git_tree_id = git_object_identity<git_tree_identity_kind>;
using git_pack_id = git_object_identity<git_pack_identity_kind>;

template<typename Kind>
[[nodiscard]] constexpr auto valid(
    const git_object_identity<Kind>& object) noexcept -> bool
{
    if(
        object.Algorithm != git_hash_algorithm::sha1
        && object.Algorithm != git_hash_algorithm::sha256)
    {
        return false;
    }
    const auto digest_size =
        object.Algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
    bool nonzero = false;
    for(std::size_t index = 0; index < digest_size; ++index)
    {
        nonzero = nonzero || object.Digest[index] != 0U;
    }
    if(!nonzero)
    {
        return false;
    }
    for(std::size_t index = digest_size; index < object.Digest.size(); ++index)
    {
        if(object.Digest[index] != 0U)
        {
            return false;
        }
    }
    return true;
}

struct git_operation_identity
{
    std::array<std::uint8_t, 32U> Bytes{};

    auto operator<=>(const git_operation_identity&) const = default;
};

struct git_pull_request_scope_identity
{
    std::array<std::uint8_t, 32U> Bytes{};

    auto operator<=>(const git_pull_request_scope_identity&) const = default;
};

template<typename Identity>
[[nodiscard]] constexpr auto valid_identity(const Identity& identity) noexcept
    -> bool
{
    for(const auto byte : identity.Bytes)
    {
        if(byte != 0U)
        {
            return true;
        }
    }
    return false;
}

enum class git_private_ref_endpoint : std::uint8_t
{
    base,
    head,
};

struct git_staging_ref
{
    git_operation_identity Operation;
    git_private_ref_endpoint Endpoint = git_private_ref_endpoint::base;

    auto operator<=>(const git_staging_ref&) const = default;
};

struct git_epoch_ref
{
    git_pull_request_scope_identity Scope;
    std::uint64_t Ordinal = 0U;
    git_private_ref_endpoint Endpoint = git_private_ref_endpoint::base;

    auto operator<=>(const git_epoch_ref&) const = default;
};

using git_private_ref = std::variant<git_staging_ref, git_epoch_ref>;

struct git_ref_expected_absent
{
    auto operator<=>(const git_ref_expected_absent&) const = default;
};

struct git_ref_expected_object
{
    git_commit_id Object;

    auto operator<=>(const git_ref_expected_object&) const = default;
};

using git_ref_expected_state =
    std::variant<git_ref_expected_absent, git_ref_expected_object>;

struct git_ref_write
{
    git_private_ref Ref;
    git_commit_id DesiredObject;
    git_ref_expected_state ExpectedOld;

    auto operator<=>(const git_ref_write&) const = default;
};

struct git_ref_remove
{
    git_private_ref Ref;
    git_commit_id ExpectedOld;

    auto operator<=>(const git_ref_remove&) const = default;
};

using git_ref_mutation = std::variant<git_ref_write, git_ref_remove>;

struct git_initialize_repository_request
{
    std::filesystem::path Repository;
    git_hash_algorithm Algorithm = git_hash_algorithm::sha1;
};

struct git_index_pack_request
{
    std::filesystem::path Repository;
    git_operation_identity Operation;
    std::uint64_t SpoolBytes = 0U;
    std::uint64_t MaximumInputBytes = 0U;
    git_pack_id ExpectedPack;
};

struct git_commit_request
{
    std::filesystem::path Repository;
    git_commit_id Commit;
};

struct git_ancestry_request
{
    std::filesystem::path Repository;
    git_commit_id Ancestor;
    git_commit_id Descendant;
};

struct git_connectivity_request
{
    std::filesystem::path Repository;
    git_commit_id Base;
    git_commit_id Head;
};

struct git_update_refs_request
{
    std::filesystem::path Repository;
    std::span<const git_ref_mutation> Mutations;
};

struct git_inspect_ref_request
{
    std::filesystem::path Repository;
    git_private_ref Ref;
};

struct git_installation
{
    std::uint32_t VersionMajor = 0U;
    std::uint32_t VersionMinor = 0U;
    std::uint32_t VersionPatch = 0U;

    auto operator<=>(const git_installation&) const = default;
};

struct git_index_pack_success
{
    git_pack_id Pack;

    auto operator<=>(const git_index_pack_success&) const = default;
};

struct git_commit_inspection
{
    git_tree_id Tree;

    auto operator<=>(const git_commit_inspection&) const = default;
};

enum class git_ancestry : std::uint8_t
{
    ancestor,
    not_ancestor,
};

enum class git_process_failure : std::uint8_t
{
    invalid_configuration,
    unsupported_git,
    invalid_request,
    invalid_repository,
    process_start,
    standard_input,
    standard_output_limit,
    standard_error_limit,
    timeout,
    cancelled,
    process_crashed,
    nonzero_exit,
    object_unavailable,
    malformed_output,
    reap_failure,
};

template<typename Value>
using git_process_result = std::expected<Value, git_process_failure>;

class git_process_port
{
public:
    virtual ~git_process_port() = default;

    [[nodiscard]] virtual auto installation() const noexcept
        -> const git_installation& = 0;
    [[nodiscard]] virtual auto pack_spool_path(
        const git_operation_identity& operation) const
        -> git_process_result<std::filesystem::path> = 0;
    [[nodiscard]] virtual auto initialize_bare_repository(
        const git_initialize_repository_request& request,
        std::stop_token stop) -> git_process_result<void> = 0;

    // Calls are synchronous so the application host can run them on its
    // effect executor. The caller retains the stop_source for the supplied
    // token and can therefore cancel a blocked child from another thread.
    [[nodiscard]] virtual auto index_pack(
        const git_index_pack_request& request,
        std::stop_token stop) -> git_process_result<git_index_pack_success> = 0;
    [[nodiscard]] virtual auto inspect_commit(
        const git_commit_request& request,
        std::stop_token stop) -> git_process_result<git_commit_inspection> = 0;
    [[nodiscard]] virtual auto is_ancestor(
        const git_ancestry_request& request,
        std::stop_token stop) -> git_process_result<git_ancestry> = 0;
    [[nodiscard]] virtual auto verify_connectivity(
        const git_connectivity_request& request,
        std::stop_token stop) -> git_process_result<void> = 0;
    [[nodiscard]] virtual auto update_refs(
        const git_update_refs_request& request,
        std::stop_token stop) -> git_process_result<void> = 0;
    [[nodiscard]] virtual auto inspect_ref(
        const git_inspect_ref_request& request,
        std::stop_token stop)
        -> git_process_result<std::optional<git_commit_id>> = 0;

    // Reads only the exact direct ref target. Recovery uses this separately
    // from object inspection so a ref whose object is missing or corrupt is
    // still reported as physical ref evidence rather than as an absent ref.
    [[nodiscard]] virtual auto inspect_ref_target(
        const git_inspect_ref_request& request,
        std::stop_token stop)
        -> git_process_result<std::optional<git_commit_id>> = 0;
};
}
