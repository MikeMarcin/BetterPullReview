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
    -> std::array<epoch_command_projection, 6>;

[[nodiscard]] auto material_relation(
    const pull_request_authority& current_authority,
    const std::optional<epoch_material_identity>& current,
    const pull_request_authority& prior_authority,
    const std::optional<epoch_material_identity>& prior) noexcept
    -> epoch_material_relation;

enum class epoch_preparation_effect : std::uint8_t
{
    none,
    retain_revision,
};

struct epoch_preparation_catalog_observation
{
    std::optional<review_epoch_summary> LatestLive;
    retention_rehydration_requirement RetentionRehydration =
        retention_rehydration_requirement::not_required;

    auto operator<=>(const epoch_preparation_catalog_observation&) const =
        default;
};

struct planned_epoch_preparation
{
    epoch_preparation_effect Effect = epoch_preparation_effect::none;
    epoch_reason_code Rejection = epoch_reason_code::none;

    auto operator<=>(const planned_epoch_preparation&) const = default;
};

// This is the single core-owned decision that bounds host preparation work.
// In particular, an unchanged revision or association-only update must not
// reserve an ordinal, fetch a pack, import objects, or create staging refs.
[[nodiscard]] auto plan_epoch_preparation(
    const pull_request_authority& authority,
    const accepted_pull_request_revision& revision,
    const epoch_catalog_identity& catalog,
    const epoch_preparation_catalog_observation& observation) noexcept
    -> planned_epoch_preparation;

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
