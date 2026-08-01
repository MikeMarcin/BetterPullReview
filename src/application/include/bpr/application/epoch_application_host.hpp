#pragma once

#include "bpr/application/epoch_catalog_port.hpp"
#include "bpr/application/epoch_projection.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <memory>

namespace bpr::application
{
class epoch_application_host final : public github_epoch_handoff_port
{
public:
    // Ingress and state/quiescence inspection are confined to the serialized
    // decision lane supplied at construction. Worker effects communicate
    // only by posting completions to that lane; this object is intentionally
    // not a general-purpose concurrently callable container.
    // shutdown() closes ingress without waiting. Composition keeps every
    // referenced port, executor, and projection alive until is_quiescent().
    epoch_application_host(
        core::epoch_state initial_state,
        git_cache_port& caches,
        epoch_catalog_port& catalogs,
        epoch_entropy_port& entropy,
        epoch_clock_port& clock,
        effect_executor& effects,
        decision_executor& decision_lane,
        epoch_projection_port& projections);
    ~epoch_application_host();

    epoch_application_host(const epoch_application_host&) = delete;
    auto operator=(const epoch_application_host&)
        -> epoch_application_host& = delete;
    epoch_application_host(epoch_application_host&&) = delete;
    auto operator=(epoch_application_host&&)
        -> epoch_application_host& = delete;

    void accept(accepted_pull_request_handoff handoff) override;
    // The issuer revokes its lease before notification. The epoch host also
    // revokes its retained handle idempotently before dispatching core
    // invalidation and canceling old-generation work.
    void invalidate(
        pull_request_authority_invalidation invalidation) override;
    void dispatch(epoch_intent intent);
    void shutdown();

    [[nodiscard]] auto state() const noexcept -> const core::epoch_state&;
    [[nodiscard]] auto is_closing() const noexcept -> bool;
    [[nodiscard]] auto is_quiescent() const noexcept -> bool;
    [[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

private:
    struct implementation;
    std::unique_ptr<implementation> Implementation;
};
} // namespace bpr::application
