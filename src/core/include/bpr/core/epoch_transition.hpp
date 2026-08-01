#pragma once

#include "bpr/core/epoch_model.hpp"

namespace bpr::core
{
[[nodiscard]] auto valid(const pull_request_authority& authority) noexcept
    -> bool;
[[nodiscard]] auto valid(const pull_request_source_revision& revision) noexcept
    -> bool;
[[nodiscard]] auto valid(
    const accepted_pull_request_revision& revision) noexcept -> bool;
[[nodiscard]] auto valid(const epoch_cause& cause) noexcept -> bool;
[[nodiscard]] auto valid(const epoch_catalog_identity& catalog) noexcept
    -> bool;
[[nodiscard]] auto valid(const review_epoch_id& epoch) noexcept -> bool;
[[nodiscard]] auto valid(const comparison_anchor_identity& anchor) noexcept
    -> bool;
[[nodiscard]] auto valid(const comparison_anchor& anchor) noexcept -> bool;
[[nodiscard]] auto available_for_diff(const comparison_pair& pair) noexcept
    -> bool;
[[nodiscard]] auto epoch_commands(const epoch_state& state) noexcept
    -> std::array<epoch_command_projection, 3>;

[[nodiscard]] auto material_relation(
    const pull_request_authority& current_authority,
    const std::optional<epoch_material_identity>& current,
    const pull_request_authority& prior_authority,
    const std::optional<epoch_material_identity>& prior) noexcept
    -> epoch_material_relation;

struct planned_epoch_mutation
{
    std::optional<epoch_catalog_mutation> Mutation;
    epoch_reason_code Rejection = epoch_reason_code::none;

    auto operator<=>(const planned_epoch_mutation&) const = default;
};

[[nodiscard]] auto plan_epoch_mutation(
    const pull_request_authority& authority,
    const accepted_pull_request_revision& revision,
    const epoch_catalog_identity& catalog,
    const epoch_preparation_observation& observation) noexcept
    -> planned_epoch_mutation;

struct resolved_comparison
{
    std::optional<comparison_pair> Pair;
    epoch_reason_code Reason = epoch_reason_code::none;

    auto operator<=>(const resolved_comparison&) const = default;
};

[[nodiscard]] auto resolve_comparison(
    const pull_request_authority& authority,
    const comparison_selection& selection,
    const comparison_candidates& candidates) noexcept -> resolved_comparison;

[[nodiscard]] auto transition(epoch_state& state, const epoch_event& event)
    -> epoch_transition_output;
} // namespace bpr::core
