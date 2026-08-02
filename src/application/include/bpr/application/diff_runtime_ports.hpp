#pragma once

#include "bpr/core/diff_transition.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace bpr::application
{
inline constexpr std::size_t maximum_diff_outstanding_jobs = 64U;
inline constexpr std::size_t maximum_diff_cache_entries = 512U;
inline constexpr std::uint64_t maximum_diff_cache_bytes =
    256U * 1024U * 1024U;

struct diff_host_limits
{
    std::size_t OutstandingJobs = 4U;
    std::size_t CacheEntries = 64U;
    std::uint64_t CacheBytes = 64U * 1024U * 1024U;

    auto operator<=>(const diff_host_limits&) const = default;
};

[[nodiscard]] constexpr auto valid(const diff_host_limits& limits) noexcept
    -> bool
{
    return limits.OutstandingJobs > 0U
        && limits.OutstandingJobs <= maximum_diff_outstanding_jobs
        && limits.CacheEntries > 0U
        && limits.CacheEntries <= maximum_diff_cache_entries
        && limits.CacheBytes > 0U
        && limits.CacheBytes <= maximum_diff_cache_bytes;
}

enum class diff_blob_status : std::uint8_t
{
    available,
    missing,
    binary,
    decode_failure,
    materialization_limit,
    generated_file_unsupported,
    cancelled,
    failure,
};

struct diff_blob_request
{
    core::diff_request_context Context;
    core::diff_side Side = core::diff_side::left;
    core::git_blob_oid Blob;
    std::uint64_t MaximumBytes = 0U;

    auto operator<=>(const diff_blob_request&) const = default;
};

struct diff_blob_observation
{
    core::diff_request_context Context;
    core::diff_side Side = core::diff_side::left;
    core::git_blob_oid Blob;
    diff_blob_status Status = diff_blob_status::failure;
    std::vector<std::uint8_t> Bytes;

    auto operator<=>(const diff_blob_observation&) const = default;
};

class diff_blob_port
{
public:
    virtual ~diff_blob_port() = default;

    [[nodiscard]] virtual auto materialize(
        const diff_blob_request& request,
        std::stop_token stop) -> diff_blob_observation = 0;
};

struct diff_publication
{
    core::diff_request_context Context;
    std::shared_ptr<const core::computed_file_diff> Result;
    bool CacheHit = false;
};

struct diff_failure_publication
{
    core::diff_request_context Context;
    core::diff_reason_code Reason = core::diff_reason_code::host_failure;

    auto operator<=>(const diff_failure_publication&) const = default;
};

class diff_projection_port
{
public:
    virtual ~diff_projection_port() = default;

    // Result is immutable. Presentation may retain the shared value for its
    // projection lifetime but cannot mutate or become semantic authority.
    virtual void publish(const diff_publication& publication) = 0;
    virtual void fail(const diff_failure_publication& failure) = 0;
    virtual void invalidate(const core::pull_request_authority& authority) = 0;
};
} // namespace bpr::application
