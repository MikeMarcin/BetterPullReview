#pragma once

#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/port.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace bpr::application
{
class diff_application_host final
{
public:
    // Public operations and inspection are confined to the supplied decision
    // lane. Effect work posts immutable terminals back to that lane.
    // Referenced ports and executors remain live until is_quiescent().
    diff_application_host(
        core::diff_state initial_state,
        diff_blob_port& blobs,
        effect_executor& effects,
        decision_executor& decision_lane,
        diff_projection_port& projections,
        diff_host_limits limits = {},
        core::diff_compute_limits compute_limits = {});
    ~diff_application_host();

    diff_application_host(const diff_application_host&) = delete;
    auto operator=(const diff_application_host&)
        -> diff_application_host& = delete;
    diff_application_host(diff_application_host&&) = delete;
    auto operator=(diff_application_host&&)
        -> diff_application_host& = delete;

    void request(core::file_comparison_key key, core::diff_options options);
    void cancel();
    void invalidate(const core::pull_request_authority& authority);
    void shutdown();

    [[nodiscard]] auto state() const noexcept -> const core::diff_state&;
    [[nodiscard]] auto is_closing() const noexcept -> bool;
    [[nodiscard]] auto is_quiescent() const noexcept -> bool;
    [[nodiscard]] auto outstanding_jobs() const noexcept -> std::size_t;
    [[nodiscard]] auto cache_entries() const noexcept -> std::size_t;
    [[nodiscard]] auto cache_bytes() const noexcept -> std::uint64_t;
    [[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

private:
    struct implementation;
    std::unique_ptr<implementation> Implementation;
};
} // namespace bpr::application
