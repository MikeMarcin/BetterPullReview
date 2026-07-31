#pragma once

#include "bpr/core/epoch_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>

namespace bpr::application
{
struct git_pack_oid
{
    core::git_hash_algorithm Algorithm = core::git_hash_algorithm::invalid;
    std::array<std::uint8_t, 32U> Digest{};

    auto operator<=>(const git_pack_oid&) const = default;
};

[[nodiscard]] constexpr auto valid(const git_pack_oid& pack) noexcept -> bool
{
    const auto digest_size =
        pack.Algorithm == core::git_hash_algorithm::sha1
        ? 20U
        : pack.Algorithm == core::git_hash_algorithm::sha256 ? 32U : 0U;
    if(digest_size == 0U)
    {
        return false;
    }
    bool nonzero = false;
    for(std::size_t index = 0U; index < digest_size; ++index)
    {
        nonzero = nonzero || pack.Digest[index] != 0U;
    }
    for(std::size_t index = digest_size; index < pack.Digest.size(); ++index)
    {
        if(pack.Digest[index] != 0U)
        {
            return false;
        }
    }
    return nonzero;
}

struct completed_git_pack_receipt
{
    core::epoch_operation_id Operation;
    git_pack_oid ExpectedPack;
    git_pack_oid ObservedPack;
    std::uint64_t Bytes = 0U;
    std::array<std::uint8_t, 32U> OpaqueToken{};

    auto operator<=>(const completed_git_pack_receipt&) const = default;
};

[[nodiscard]] constexpr auto valid(
    const completed_git_pack_receipt& receipt) noexcept -> bool
{
    bool token_nonzero = false;
    for(const auto byte : receipt.OpaqueToken)
    {
        token_nonzero = token_nonzero || byte != 0U;
    }
    return receipt.Operation.valid() && valid(receipt.ExpectedPack)
        && valid(receipt.ObservedPack)
        && receipt.ExpectedPack.Algorithm
            == receipt.ObservedPack.Algorithm
        && receipt.Bytes != 0U && token_nonzero;
}

[[nodiscard]] constexpr auto agrees(
    const completed_git_pack_receipt& receipt) noexcept -> bool
{
    return valid(receipt)
        && receipt.ExpectedPack == receipt.ObservedPack;
}

enum class git_pack_stream_failure : std::uint8_t
{
    invalid_request,
    invalid_state,
    input_limit,
    storage,
    malformed_pack,
    cancelled,
};

template<typename Value>
using git_pack_stream_result =
    std::expected<Value, git_pack_stream_failure>;

class git_pack_stream_sink
{
public:
    git_pack_stream_sink() = default;
    git_pack_stream_sink(const git_pack_stream_sink&) = delete;
    auto operator=(const git_pack_stream_sink&)
        -> git_pack_stream_sink& = delete;
    git_pack_stream_sink(git_pack_stream_sink&&) = delete;
    auto operator=(git_pack_stream_sink&&)
        -> git_pack_stream_sink& = delete;
    virtual ~git_pack_stream_sink() = default;

    [[nodiscard]] virtual auto append(
        std::span<const std::byte> bytes,
        std::stop_token stop) -> git_pack_stream_result<void> = 0;
    [[nodiscard]] virtual auto finish(std::stop_token stop)
        -> git_pack_stream_result<completed_git_pack_receipt> = 0;
    virtual auto abort() noexcept -> void = 0;
};
} // namespace bpr::application
