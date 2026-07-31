#include "bpr/core/epoch_transition.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace bpr::core
{
namespace
{
enum class completion_match : std::uint8_t
{
    current,
    stale,
    identity_mismatch,
};

[[nodiscard]] constexpr auto valid_nonzero(const std::uint64_t value) noexcept
    -> bool
{
    return value != 0U;
}

[[nodiscard]] constexpr auto valid_availability(
    const anchor_availability availability) noexcept -> bool
{
    switch(availability)
    {
    case anchor_availability::available:
    case anchor_availability::unavailable:
    case anchor_availability::pruned:
    case anchor_availability::cache_deleted:
    case anchor_availability::corrupt:
    case anchor_availability::never_observed:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_review_state(
    const github_review_state state) noexcept -> bool
{
    switch(state)
    {
    case github_review_state::pending:
    case github_review_state::approved:
    case github_review_state::changes_requested:
    case github_review_state::commented:
    case github_review_state::dismissed:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto eligible_review_state(
    const github_review_state state) noexcept -> bool
{
    return valid_review_state(state) && state != github_review_state::pending;
}

[[nodiscard]] constexpr auto valid_lineage(const head_lineage lineage) noexcept
    -> bool
{
    return lineage == head_lineage::not_applicable
        || lineage == head_lineage::descendant
        || lineage == head_lineage::non_fast_forward
        || lineage == head_lineage::unknown;
}

[[nodiscard]] constexpr auto valid_alias(const named_comparison alias) noexcept
    -> bool
{
    return alias == named_comparison::whole_pr
        || alias == named_comparison::previous_upload_to_latest
        || alias == named_comparison::my_last_reviewed_to_latest;
}

[[nodiscard]] constexpr auto valid_side(const comparison_side side) noexcept
    -> bool
{
    return side == comparison_side::left || side == comparison_side::right;
}

[[nodiscard]] auto valid_catalog_review(
    const catalog_review_association& review) noexcept -> bool
{
    return review.Association.valid() && valid_nonzero(review.Review.value())
        && valid_nonzero(review.Actor.value()) && valid(review.Order)
        && eligible_review_state(review.State) && review.Commit.valid()
        && valid_availability(review.Availability);
}

[[nodiscard]] auto same_provider_review(
    const current_viewer_review_candidate& provider,
    const catalog_review_association& catalog_review) noexcept -> bool
{
    return provider.Review == catalog_review.Review
        && provider.Actor == catalog_review.Actor
        && provider.Order == catalog_review.Order
        && provider.State == catalog_review.State
        && provider.Commit == catalog_review.Commit;
}

[[nodiscard]] auto valid_material(
    const epoch_material_identity& material) noexcept -> bool
{
    return material.BaseTree.valid() && material.HeadTree.valid()
        && material.BaseTree.algorithm() == material.HeadTree.algorithm()
        && valid_nonzero(material.PolicyRevision.value());
}

[[nodiscard]] constexpr auto valid_cause(const epoch_cause& cause) noexcept
    -> bool
{
    const bool origin = cause.Origin == epoch_origin::initial_observation
        || cause.Origin == epoch_origin::live_observation
        || cause.Origin == epoch_origin::explicit_historical_import;
    const bool base = cause.Base == epoch_base_transition::not_applicable
        || cause.Base == epoch_base_transition::unchanged
        || cause.Base == epoch_base_transition::base_oid_changed
        || cause.Base == epoch_base_transition::base_branch_changed;
    const bool head = cause.Head == epoch_head_transition::not_applicable
        || cause.Head == epoch_head_transition::unchanged
        || cause.Head == epoch_head_transition::descendant
        || cause.Head == epoch_head_transition::non_fast_forward
        || cause.Head == epoch_head_transition::lineage_unknown;
    const bool material = cause.Material == epoch_material_relation::same
        || cause.Material == epoch_material_relation::different
        || cause.Material == epoch_material_relation::unknown;
    const bool primary =
        cause.Primary == epoch_primary_cause::initial_observation
        || cause.Primary == epoch_primary_cause::explicit_historical_import
        || cause.Primary == epoch_primary_cause::base_branch_change
        || cause.Primary == epoch_primary_cause::force_push_rebase
        || cause.Primary == epoch_primary_cause::author_upload
        || cause.Primary == epoch_primary_cause::head_change_lineage_unavailable
        || cause.Primary == epoch_primary_cause::base_update;
    if(!(origin && base && head && material && primary))
    {
        return false;
    }
    if(cause.Origin == epoch_origin::initial_observation)
    {
        return cause.Base == epoch_base_transition::not_applicable
            && cause.Head == epoch_head_transition::not_applicable
            && cause.Primary == epoch_primary_cause::initial_observation;
    }
    if(cause.Origin == epoch_origin::explicit_historical_import)
    {
        return cause.Base == epoch_base_transition::not_applicable
            && cause.Head == epoch_head_transition::not_applicable
            && cause.Primary == epoch_primary_cause::explicit_historical_import;
    }
    if(cause.Base == epoch_base_transition::not_applicable
       || cause.Head == epoch_head_transition::not_applicable)
    {
        return false;
    }
    if(cause.Base == epoch_base_transition::base_branch_changed)
    {
        return cause.Primary == epoch_primary_cause::base_branch_change;
    }
    if(cause.Head == epoch_head_transition::non_fast_forward)
    {
        return cause.Primary == epoch_primary_cause::force_push_rebase;
    }
    if(cause.Head == epoch_head_transition::descendant)
    {
        return cause.Primary == epoch_primary_cause::author_upload;
    }
    if(cause.Head == epoch_head_transition::lineage_unknown)
    {
        return cause.Primary
            == epoch_primary_cause::head_change_lineage_unavailable;
    }
    return cause.Head == epoch_head_transition::unchanged
        && cause.Base == epoch_base_transition::base_oid_changed
        && cause.Primary == epoch_primary_cause::base_update;
}

[[nodiscard]] constexpr auto live(const review_epoch_summary& epoch) noexcept
    -> bool
{
    return epoch.Cause.Origin != epoch_origin::explicit_historical_import;
}

[[nodiscard]] auto valid_summary(
    const pull_request_authority& authority,
    const epoch_catalog_identity& catalog,
    const review_epoch_summary& epoch) noexcept -> bool
{
    return valid(epoch.Id) && epoch.Id.Authority == authority.Scope
        && epoch.Id.Incarnation == catalog.Incarnation
        && valid_nonzero(epoch.Source.Snapshot.value())
        && valid_nonzero(epoch.Source.IdentityGeneration.value())
        && valid_nonzero(epoch.Source.PullRequestGeneration.value())
        && epoch.BaseBranch.valid() && epoch.Base.valid() && epoch.Head.valid()
        && epoch.Base.algorithm() == epoch.Head.algorithm()
        && (!epoch.Material.has_value()
            || (valid_material(*epoch.Material)
                && epoch.Material->BaseTree.algorithm()
                    == epoch.Base.algorithm()))
        && valid(epoch.ObservedAt) && valid_cause(epoch.Cause)
        && valid_availability(epoch.BaseAvailability)
        && valid_availability(epoch.HeadAvailability);
}

[[nodiscard]] auto epoch_anchor(
    const review_epoch_summary& epoch,
    const comparison_anchor_kind kind) noexcept -> comparison_anchor
{
    return comparison_anchor{
        .Identity =
            comparison_anchor_identity{
                .Authority = epoch.Id.Authority,
                .Incarnation = epoch.Id.Incarnation,
                .Kind = kind,
                .Epoch = epoch.Id.Ordinal,
            },
        .Commit = kind == comparison_anchor_kind::epoch_base ? epoch.Base
                                                             : epoch.Head,
        .Availability = kind == comparison_anchor_kind::epoch_base
            ? epoch.BaseAvailability
            : epoch.HeadAvailability,
    };
}

[[nodiscard]] auto reject(const epoch_reason_code reason)
    -> epoch_transition_output
{
    epoch_transition_output output;
    output.Changed = epoch_changed_domain::diagnostics;
    output.Diagnostics.push_back(epoch_diagnostic{.Code = reason});
    return output;
}

auto diagnose(epoch_transition_output& output, const epoch_reason_code reason)
    -> void
{
    output.Diagnostics.push_back(epoch_diagnostic{.Code = reason});
    output.Changed = output.Changed | epoch_changed_domain::diagnostics;
}

auto set_status(
    epoch_state& state,
    epoch_transition_output& output,
    const epoch_status status) -> void
{
    state.Status = status;
    output.Status = epoch_status_delta{.Status = status};
    output.Commands = epoch_command_delta{.Commands = epoch_commands(state)};
    output.Changed = output.Changed | epoch_changed_domain::status
        | epoch_changed_domain::commands;
}

[[nodiscard]] auto advanced(const epoch_generation generation) noexcept
    -> std::optional<epoch_generation>
{
    if(generation.value() == std::numeric_limits<std::uint64_t>::max())
    {
        return std::nullopt;
    }
    return epoch_generation{generation.value() + 1U};
}

[[nodiscard]] auto advanced(const comparison_generation generation) noexcept
    -> std::optional<comparison_generation>
{
    if(generation.value() == std::numeric_limits<std::uint64_t>::max())
    {
        return std::nullopt;
    }
    return comparison_generation{generation.value() + 1U};
}

[[nodiscard]] auto advanced(const comparison_revision revision) noexcept
    -> std::optional<comparison_revision>
{
    if(revision.value() >= maximum_comparison_revision)
    {
        return std::nullopt;
    }
    return comparison_revision{revision.value() + 1};
}

[[nodiscard]] auto advanced(const epoch_catalog_revision revision) noexcept
    -> std::optional<epoch_catalog_revision>
{
    if(revision.value() >= maximum_epoch_catalog_revision)
    {
        return std::nullopt;
    }
    return epoch_catalog_revision{revision.value() + 1};
}

[[nodiscard]] auto request_available(const epoch_state& state) noexcept -> bool
{
    return !state.RequestCounterExhausted
        && valid_nonzero(state.NextRequest.value());
}

[[nodiscard]] auto take_request(epoch_state& state) noexcept -> epoch_request_id
{
    const auto result = state.NextRequest;
    if(result.value() == std::numeric_limits<std::uint64_t>::max())
    {
        state.RequestCounterExhausted = true;
    }
    else
    {
        state.NextRequest = epoch_request_id{result.value() + 1U};
    }
    return result;
}

[[nodiscard]] auto current_load_context(
    const epoch_state& state, const epoch_request_id request)
    -> epoch_load_context
{
    return epoch_load_context{
        .Request = request,
        .ApplicationSession = state.ApplicationSession,
        .Authority = *state.Authority,
        .Revision = *state.AcceptedRevision,
        .Generation = state.EpochGeneration,
    };
}

[[nodiscard]] auto current_preparation_context(
    const epoch_state& state, const epoch_request_id request)
    -> epoch_preparation_context
{
    return epoch_preparation_context{
        .Request = request,
        .ApplicationSession = state.ApplicationSession,
        .Authority = *state.Authority,
        .Revision = *state.AcceptedRevision,
        .Generation = state.EpochGeneration,
        .CatalogIncarnation = state.Catalog->Incarnation,
        .ExpectedCatalogRevision = state.Catalog->Revision,
    };
}

[[nodiscard]] auto current_operation_context(
    const epoch_state& state, const pending_epoch_ingestion& pending)
    -> epoch_operation_context
{
    return epoch_operation_context{
        .Request = pending.Request,
        .ApplicationSession = state.ApplicationSession,
        .Authority = *state.Authority,
        .Revision = *state.AcceptedRevision,
        .Generation = state.EpochGeneration,
        .CatalogIncarnation = state.Catalog->Incarnation,
        .ExpectedCatalogRevision = state.Catalog->Revision,
        .Operation = pending.Operation,
    };
}

[[nodiscard]] auto current_comparison_context(
    const epoch_state& state, const epoch_request_id request)
    -> comparison_context
{
    return comparison_context{
        .Request = request,
        .ApplicationSession = state.ApplicationSession,
        .Authority = *state.Authority,
        .Source = state.AcceptedRevision->Source,
        .CatalogIncarnation = state.Catalog->Incarnation,
        .CatalogRevision = state.Catalog->Revision,
        .Generation = state.ComparisonGeneration,
        .Selection = state.RequestedSelection,
    };
}

template<typename Context>
[[nodiscard]] auto context_match(
    const std::optional<pending_epoch_ingestion>& pending,
    const epoch_ingestion_phase phase,
    const Context& actual,
    const Context& expected) noexcept -> completion_match
{
    if(!pending.has_value() || pending->Phase != phase)
    {
        return completion_match::stale;
    }
    if(actual.Request != pending->Request
       || actual.Generation != pending->Generation)
    {
        return completion_match::stale;
    }
    return actual == expected ? completion_match::current
                              : completion_match::identity_mismatch;
}

[[nodiscard]] auto comparison_context_match(
    const epoch_state& state, const comparison_context& actual)
    -> completion_match
{
    if(!state.PendingComparison.has_value())
    {
        return completion_match::stale;
    }
    if(actual.Request != state.PendingComparison->Request
       || actual.Generation != state.PendingComparison->Generation)
    {
        return completion_match::stale;
    }
    return actual
            == current_comparison_context(
                state, state.PendingComparison->Request)
        ? completion_match::current
        : completion_match::identity_mismatch;
}

[[nodiscard]] auto reject_match(const completion_match match)
    -> epoch_transition_output
{
    return reject(
        match == completion_match::identity_mismatch
            ? epoch_reason_code::completion_identity_mismatch
            : epoch_reason_code::stale_completion);
}

auto cancel_pending(const epoch_state& state, epoch_transition_output& output)
    -> void
{
    if(state.PendingIngestion.has_value())
    {
        output.Requests.emplace_back(
            cancel_epoch_work_request{
                .Request = state.PendingIngestion->Request,
                .Generation = state.PendingIngestion->Generation,
            });
    }
}

[[nodiscard]] auto clear_authority_fail_closed(
    epoch_state& state, const epoch_reason_code reason)
    -> epoch_transition_output
{
    epoch_transition_output output;
    cancel_pending(state, output);
    if(state.PendingComparison.has_value())
    {
        output.Requests.emplace_back(
            cancel_epoch_work_request{
                .Request = state.PendingComparison->Request,
                .Generation = state.PendingComparison->Generation,
            });
    }
    const auto previous = state.Authority;
    state.Authority.reset();
    state.AcceptedRevision.reset();
    state.Catalog.reset();
    state.RequestedSelection = named_selection(named_comparison::whole_pr);
    state.AcceptedComparison.reset();
    state.AcceptedComparisonRevision = comparison_revision{};
    state.PendingIngestion.reset();
    state.PendingComparison.reset();
    output.Requests.insert(
        output.Requests.begin(),
        invalidate_epoch_authority_request{
            .ApplicationSession = state.ApplicationSession,
            .PreviousAuthority = previous,
            .NewGeneration = state.EpochGeneration,
        });
    output.Authority =
        epoch_authority_delta{.Generation = state.EpochGeneration};
    output.Comparison = comparison_resolution_delta{
        .Selection = state.RequestedSelection,
        .Revision = state.AcceptedComparisonRevision,
    };
    output.Changed =
        epoch_changed_domain::authority | epoch_changed_domain::comparison;
    diagnose(output, reason);
    set_status(state, output, epoch_status::unavailable);
    return output;
}

[[nodiscard]] auto start_comparison(
    epoch_state& state,
    epoch_transition_output& output,
    const epoch_request_id request,
    const bool preserve_accepted_pair) -> bool
{
    const auto generation = advanced(state.ComparisonGeneration);
    if(!generation.has_value())
    {
        if(state.PendingComparison.has_value())
        {
            output.Requests.emplace_back(
                cancel_epoch_work_request{
                    .Request = state.PendingComparison->Request,
                    .Generation = state.PendingComparison->Generation,
                });
            state.PendingComparison.reset();
        }
        state.AcceptedComparison.reset();
        output.Comparison = comparison_resolution_delta{
            .Selection = state.RequestedSelection,
            .Revision = state.AcceptedComparisonRevision,
        };
        output.Changed = output.Changed | epoch_changed_domain::comparison;
        diagnose(output, epoch_reason_code::comparison_generation_exhausted);
        set_status(state, output, epoch_status::unavailable);
        return false;
    }

    if(state.PendingComparison.has_value())
    {
        output.Requests.emplace_back(
            cancel_epoch_work_request{
                .Request = state.PendingComparison->Request,
                .Generation = state.PendingComparison->Generation,
            });
    }

    state.ComparisonGeneration = *generation;
    state.PendingComparison = pending_comparison_resolution{
        .Request = request,
        .Generation = *generation,
    };
    if(!preserve_accepted_pair)
    {
        state.AcceptedComparison.reset();
    }
    output.Requests.emplace_back(
        resolve_comparison_request{
            .Context = current_comparison_context(state, request),
        });
    set_status(state, output, epoch_status::resolving);
    return true;
}

[[nodiscard]] auto next_catalog_revision(
    const epoch_catalog_revision revision,
    const epoch_catalog_revision actual) noexcept -> bool
{
    const auto expected = advanced(revision);
    return expected.has_value() && actual == *expected;
}

auto clear_ingestion(
    epoch_state& state,
    epoch_transition_output& output,
    const epoch_status status,
    const std::optional<epoch_reason_code> diagnostic = std::nullopt) -> void
{
    state.PendingIngestion.reset();
    if(diagnostic.has_value())
    {
        diagnose(output, *diagnostic);
    }
    set_status(state, output, status);
}

[[nodiscard]] auto operation_payload_matches(
    const epoch_state& state,
    const epoch_operation_context& context,
    const epoch_catalog_mutation& mutation) -> completion_match
{
    if(!state.PendingIngestion.has_value()
       || state.PendingIngestion->Phase != epoch_ingestion_phase::committing)
    {
        return completion_match::stale;
    }
    const auto& pending = *state.PendingIngestion;
    if(context.Request != pending.Request
       || context.Generation != pending.Generation)
    {
        return completion_match::stale;
    }
    if(context != current_operation_context(state, pending)
       || !pending.Mutation.has_value() || mutation != *pending.Mutation)
    {
        return completion_match::identity_mismatch;
    }
    return completion_match::current;
}

[[nodiscard]] auto select_pair(
    const pull_request_authority& authority,
    const comparison_selection& selection,
    const comparison_candidates& candidates) noexcept -> resolved_comparison
{
    if(candidates.LatestLive.has_value()
       && !valid_summary(authority, candidates.Catalog, *candidates.LatestLive))
    {
        return {.Reason = epoch_reason_code::invalid_observation};
    }
    const auto latest_valid =
        candidates.LatestLive.has_value() && live(*candidates.LatestLive);

    if(const auto* explicit_pair =
           std::get_if<explicit_comparison_selection>(&selection))
    {
        if(!candidates.ExplicitLeft.has_value()
           || !candidates.ExplicitRight.has_value())
        {
            return {.Reason = epoch_reason_code::unavailable};
        }
        if(candidates.ExplicitLeft->Identity != explicit_pair->Left
           || candidates.ExplicitRight->Identity != explicit_pair->Right
           || !valid(*candidates.ExplicitLeft)
           || !valid(*candidates.ExplicitRight))
        {
            return {.Reason = epoch_reason_code::invalid_selection};
        }
        return {
            .Pair =
                comparison_pair{
                    .Left = *candidates.ExplicitLeft,
                    .Right = *candidates.ExplicitRight,
                },
        };
    }

    const auto& named = std::get<named_comparison_selection>(selection);
    if(!valid_alias(named.Alias))
    {
        return {.Reason = epoch_reason_code::invalid_selection};
    }
    if(!latest_valid)
    {
        return {.Reason = epoch_reason_code::unavailable};
    }
    const auto latest_head = epoch_anchor(
        *candidates.LatestLive, comparison_anchor_kind::epoch_head);

    switch(named.Alias)
    {
    case named_comparison::whole_pr:
        return {
            .Pair =
                comparison_pair{
                    .Left = epoch_anchor(
                        *candidates.LatestLive,
                        comparison_anchor_kind::epoch_base),
                    .Right = latest_head,
                },
        };
    case named_comparison::previous_upload_to_latest:
        if(candidates.PreviousDistinctHead.has_value()
           && !valid_summary(
               authority, candidates.Catalog, *candidates.PreviousDistinctHead))
        {
            return {.Reason = epoch_reason_code::invalid_observation};
        }
        if(!candidates.PreviousDistinctHead.has_value()
           || !live(*candidates.PreviousDistinctHead)
           || candidates.PreviousDistinctHead->Id.Ordinal.value()
               >= candidates.LatestLive->Id.Ordinal.value()
           || candidates.PreviousDistinctHead->Head
               == candidates.LatestLive->Head)
        {
            return {.Reason = epoch_reason_code::unavailable};
        }
        return {
            .Pair =
                comparison_pair{
                    .Left = epoch_anchor(
                        *candidates.PreviousDistinctHead,
                        comparison_anchor_kind::epoch_head),
                    .Right = latest_head,
                },
        };
    case named_comparison::my_last_reviewed_to_latest:
        if(candidates.CurrentViewerReview.has_value()
           && (!valid(*candidates.CurrentViewerReview)
               || candidates.CurrentViewerReview->Identity.Kind
                   != comparison_anchor_kind::review_commit))
        {
            return {.Reason = epoch_reason_code::invalid_observation};
        }
        if(!candidates.CurrentViewerReview.has_value())
        {
            return {.Reason = epoch_reason_code::unavailable};
        }
        return {
            .Pair =
                comparison_pair{
                    .Left = *candidates.CurrentViewerReview,
                    .Right = latest_head,
                },
        };
    }
    return {.Reason = epoch_reason_code::invalid_selection};
}

[[nodiscard]] auto handle(
    epoch_state& state, const pull_request_revision_accepted_event& event)
    -> epoch_transition_output
{
    if(event.ApplicationSession != state.ApplicationSession
       || !valid(event.Authority) || !valid(event.Revision)
       || (event.Revision.CurrentViewerReview.has_value()
           && event.Revision.CurrentViewerReview->Actor
               != event.Authority.Viewer))
    {
        return reject(epoch_reason_code::invalid_revision);
    }
    if(state.Authority == event.Authority
       && state.AcceptedRevision == event.Revision)
    {
        return {};
    }

    const bool authority_changed =
        !state.Authority.has_value() || *state.Authority != event.Authority;
    const auto next_epoch_generation = advanced(state.EpochGeneration);
    if(!next_epoch_generation.has_value())
    {
        return state.Authority.has_value()
            ? clear_authority_fail_closed(
                  state, epoch_reason_code::epoch_generation_exhausted)
            : reject(epoch_reason_code::epoch_generation_exhausted);
    }
    std::optional<comparison_generation> next_comparison_generation;
    if(authority_changed)
    {
        next_comparison_generation = advanced(state.ComparisonGeneration);
        if(!next_comparison_generation.has_value())
        {
            return state.Authority.has_value()
                ? clear_authority_fail_closed(
                      state, epoch_reason_code::comparison_generation_exhausted)
                : reject(epoch_reason_code::comparison_generation_exhausted);
        }
    }
    if(!request_available(state))
    {
        return state.Authority.has_value()
            ? clear_authority_fail_closed(
                  state, epoch_reason_code::request_counter_exhausted)
            : reject(epoch_reason_code::request_counter_exhausted);
    }

    auto output = epoch_transition_output{};
    cancel_pending(state, output);
    if(state.PendingComparison.has_value())
    {
        output.Requests.emplace_back(
            cancel_epoch_work_request{
                .Request = state.PendingComparison->Request,
                .Generation = state.PendingComparison->Generation,
            });
    }

    const auto previous_authority = state.Authority;
    state.EpochGeneration = *next_epoch_generation;
    if(next_comparison_generation.has_value())
    {
        state.ComparisonGeneration = *next_comparison_generation;
    }
    state.Authority = event.Authority;
    state.AcceptedRevision = event.Revision;
    state.PendingIngestion.reset();
    state.PendingComparison.reset();

    if(authority_changed)
    {
        state.Catalog.reset();
        state.RequestedSelection = named_selection(named_comparison::whole_pr);
        state.AcceptedComparison.reset();
        state.AcceptedComparisonRevision = comparison_revision{};
        output.Requests.insert(
            output.Requests.begin(),
            invalidate_epoch_authority_request{
                .ApplicationSession = state.ApplicationSession,
                .PreviousAuthority = previous_authority,
                .NewGeneration = state.EpochGeneration,
            });
        output.Selection = comparison_selection_delta{
            .Selection = state.RequestedSelection,
        };
        output.Changed = output.Changed | epoch_changed_domain::selection;
    }

    const auto request = take_request(state);
    state.PendingIngestion = pending_epoch_ingestion{
        .Request = request,
        .Generation = state.EpochGeneration,
        .Phase = state.Catalog.has_value()
            ? epoch_ingestion_phase::preparing
            : epoch_ingestion_phase::loading_catalog,
    };

    output.Authority = epoch_authority_delta{
        .Authority = state.Authority,
        .Revision = state.AcceptedRevision,
        .Generation = state.EpochGeneration,
    };
    output.Changed = output.Changed | epoch_changed_domain::authority;

    if(state.Catalog.has_value())
    {
        output.Requests.emplace_back(
            prepare_epoch_revision_request{
                .Context = current_preparation_context(state, request),
            });
        set_status(state, output, epoch_status::preparing);
    }
    else
    {
        output.Requests.emplace_back(
            load_epoch_catalog_request{
                .Context = current_load_context(state, request),
            });
        set_status(state, output, epoch_status::loading_catalog);
    }
    return output;
}

[[nodiscard]] auto handle(epoch_state& state, const refresh_epochs_intent&)
    -> epoch_transition_output
{
    if(!state.Authority.has_value() || !state.AcceptedRevision.has_value())
    {
        return reject(epoch_reason_code::no_active_authority);
    }

    const auto next_generation = advanced(state.EpochGeneration);
    if(!next_generation.has_value())
    {
        return reject(epoch_reason_code::epoch_generation_exhausted);
    }
    if(!request_available(state))
    {
        return reject(epoch_reason_code::request_counter_exhausted);
    }

    auto output = epoch_transition_output{};
    cancel_pending(state, output);
    if(state.PendingComparison.has_value())
    {
        output.Requests.emplace_back(
            cancel_epoch_work_request{
                .Request = state.PendingComparison->Request,
                .Generation = state.PendingComparison->Generation,
            });
        state.PendingComparison.reset();
    }
    state.EpochGeneration = *next_generation;
    const auto request = take_request(state);
    state.PendingIngestion = pending_epoch_ingestion{
        .Request = request,
        .Generation = state.EpochGeneration,
        .Phase = state.Catalog.has_value()
            ? epoch_ingestion_phase::preparing
            : epoch_ingestion_phase::loading_catalog,
    };
    output.Authority = epoch_authority_delta{
        .Authority = state.Authority,
        .Revision = state.AcceptedRevision,
        .Generation = state.EpochGeneration,
    };
    output.Changed = epoch_changed_domain::authority;

    if(state.Catalog.has_value())
    {
        output.Requests.emplace_back(
            prepare_epoch_revision_request{
                .Context = current_preparation_context(state, request),
            });
        set_status(state, output, epoch_status::preparing);
    }
    else
    {
        output.Requests.emplace_back(
            load_epoch_catalog_request{
                .Context = current_load_context(state, request),
            });
        set_status(state, output, epoch_status::loading_catalog);
    }
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const pull_request_revision_invalidated_event& event)
    -> epoch_transition_output
{
    if(event.ApplicationSession != state.ApplicationSession
       || !state.Authority.has_value() || !state.AcceptedRevision.has_value()
       || *state.Authority != event.Authority
       || *state.AcceptedRevision != event.Revision
       || state.EpochGeneration != event.Generation)
    {
        return reject(epoch_reason_code::stale_completion);
    }

    auto output = epoch_transition_output{};
    cancel_pending(state, output);
    if(state.PendingComparison.has_value())
    {
        output.Requests.emplace_back(
            cancel_epoch_work_request{
                .Request = state.PendingComparison->Request,
                .Generation = state.PendingComparison->Generation,
            });
    }

    const auto previous = state.Authority;
    const auto next_epoch = advanced(state.EpochGeneration);
    const auto next_comparison = advanced(state.ComparisonGeneration);
    if(next_epoch.has_value())
    {
        state.EpochGeneration = *next_epoch;
    }
    else
    {
        diagnose(output, epoch_reason_code::epoch_generation_exhausted);
    }
    if(next_comparison.has_value())
    {
        state.ComparisonGeneration = *next_comparison;
    }
    else
    {
        diagnose(output, epoch_reason_code::comparison_generation_exhausted);
    }

    state.Authority.reset();
    state.AcceptedRevision.reset();
    state.Catalog.reset();
    state.RequestedSelection = named_selection(named_comparison::whole_pr);
    state.AcceptedComparison.reset();
    state.AcceptedComparisonRevision = comparison_revision{};
    state.PendingIngestion.reset();
    state.PendingComparison.reset();

    output.Requests.insert(
        output.Requests.begin(),
        invalidate_epoch_authority_request{
            .ApplicationSession = state.ApplicationSession,
            .PreviousAuthority = previous,
            .NewGeneration = state.EpochGeneration,
        });
    output.Authority = epoch_authority_delta{
        .Generation = state.EpochGeneration,
    };
    output.Changed = output.Changed | epoch_changed_domain::authority;
    set_status(state, output, epoch_status::inactive);
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_catalog_loaded_event& event)
    -> epoch_transition_output
{
    if(!state.PendingIngestion.has_value())
    {
        return reject(epoch_reason_code::stale_completion);
    }
    const auto expected =
        current_load_context(state, state.PendingIngestion->Request);
    const auto match = context_match(
        state.PendingIngestion,
        epoch_ingestion_phase::loading_catalog,
        event.Context,
        expected);
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    if(!valid(event.Catalog))
    {
        auto output = epoch_transition_output{};
        clear_ingestion(
            state,
            output,
            epoch_status::failed,
            epoch_reason_code::invalid_catalog);
        return output;
    }

    state.Catalog = event.Catalog;
    state.PendingIngestion->Phase = epoch_ingestion_phase::preparing;
    epoch_transition_output output;
    output.Catalog = epoch_catalog_delta{.Catalog = event.Catalog};
    output.Changed = epoch_changed_domain::catalog;
    output.Requests.emplace_back(
        prepare_epoch_revision_request{
            .Context = current_preparation_context(
                state, state.PendingIngestion->Request),
        });
    set_status(state, output, epoch_status::preparing);
    return output;
}

template<typename Event>
[[nodiscard]] auto finish_load(
    epoch_state& state,
    const Event& event,
    const epoch_status status,
    const epoch_reason_code reason) -> epoch_transition_output
{
    if(!state.PendingIngestion.has_value())
    {
        return reject(epoch_reason_code::stale_completion);
    }
    const auto match = context_match(
        state.PendingIngestion,
        epoch_ingestion_phase::loading_catalog,
        event.Context,
        current_load_context(state, state.PendingIngestion->Request));
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    auto output = epoch_transition_output{};
    clear_ingestion(state, output, status, reason);
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_catalog_load_failed_event& event)
    -> epoch_transition_output
{
    return finish_load(
        state, event, epoch_status::failed, reason_for(event.Failure));
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_catalog_load_cancelled_event& event)
    -> epoch_transition_output
{
    return finish_load(
        state, event, epoch_status::cancelled, epoch_reason_code::cancelled);
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_revision_prepared_event& event)
    -> epoch_transition_output
{
    if(!state.PendingIngestion.has_value())
    {
        return reject(epoch_reason_code::stale_completion);
    }
    const auto match = context_match(
        state.PendingIngestion,
        epoch_ingestion_phase::preparing,
        event.Context,
        current_preparation_context(state, state.PendingIngestion->Request));
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    if(!event.Operation.valid())
    {
        auto output = epoch_transition_output{};
        clear_ingestion(
            state,
            output,
            epoch_status::failed,
            epoch_reason_code::invalid_identity);
        return output;
    }

    const auto plan = plan_epoch_mutation(
        *state.Authority,
        *state.AcceptedRevision,
        *state.Catalog,
        event.Observation);
    state.PendingIngestion->Operation = event.Operation;
    const auto operation_context =
        current_operation_context(state, *state.PendingIngestion);
    if(plan.Rejection != epoch_reason_code::none)
    {
        auto output = epoch_transition_output{};
        output.Requests.emplace_back(
            discard_epoch_preparation_request{
                .Context = operation_context,
            });
        clear_ingestion(state, output, epoch_status::failed, plan.Rejection);
        return output;
    }

    epoch_transition_output output;
    const auto request = state.PendingIngestion->Request;
    if(!plan.Mutation.has_value())
    {
        output.Requests.emplace_back(
            discard_epoch_preparation_request{
                .Context = operation_context,
            });
        state.PendingIngestion.reset();
        static_cast<void>(start_comparison(state, output, request, true));
        return output;
    }

    state.PendingIngestion->Mutation = plan.Mutation;
    state.PendingIngestion->Phase = epoch_ingestion_phase::committing;
    output.Requests.emplace_back(
        commit_epoch_catalog_request{
            .Context =
                current_operation_context(state, *state.PendingIngestion),
            .Mutation = *plan.Mutation,
        });
    set_status(state, output, epoch_status::committing);
    return output;
}

template<typename Event>
[[nodiscard]] auto finish_preparation(
    epoch_state& state,
    const Event& event,
    const epoch_status status,
    const epoch_reason_code reason) -> epoch_transition_output
{
    if(!state.PendingIngestion.has_value())
    {
        return reject(epoch_reason_code::stale_completion);
    }
    const auto match = context_match(
        state.PendingIngestion,
        epoch_ingestion_phase::preparing,
        event.Context,
        current_preparation_context(state, state.PendingIngestion->Request));
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    auto output = epoch_transition_output{};
    clear_ingestion(state, output, status, reason);
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_revision_prepare_failed_event& event)
    -> epoch_transition_output
{
    return finish_preparation(
        state, event, epoch_status::failed, reason_for(event.Failure));
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_revision_prepare_cancelled_event& event)
    -> epoch_transition_output
{
    return finish_preparation(
        state, event, epoch_status::cancelled, epoch_reason_code::cancelled);
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_catalog_committed_event& event)
    -> epoch_transition_output
{
    const auto match =
        operation_payload_matches(state, event.Context, event.Mutation);
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    if(!next_catalog_revision(state.Catalog->Revision, event.NewRevision))
    {
        auto output = epoch_transition_output{};
        clear_ingestion(
            state,
            output,
            epoch_status::failed,
            epoch_reason_code::completion_identity_mismatch);
        return output;
    }

    const auto request = state.PendingIngestion->Request;
    const auto mutation = *state.PendingIngestion->Mutation;
    state.Catalog->Revision = event.NewRevision;
    state.PendingIngestion.reset();

    epoch_transition_output output;
    output.Catalog = epoch_catalog_delta{.Catalog = *state.Catalog};
    output.Changed = epoch_changed_domain::catalog;
    if(const auto* append = std::get_if<append_epoch_mutation>(&mutation))
    {
        output.AcceptedEpoch = accepted_epoch_delta{
            .Epoch = append->Epoch,
            .CatalogRevision = event.NewRevision,
        };
        output.Catalog->InsertedEpoch = append->Epoch.Id;
        if(append->Review.has_value())
        {
            output.Catalog->ChangedReviewAssociation =
                append->Review->Association;
        }
        output.Changed = output.Changed | epoch_changed_domain::epoch;
    }
    else
    {
        output.Catalog->ChangedReviewAssociation =
            std::get<associate_review_mutation>(mutation).Review.Association;
    }
    static_cast<void>(start_comparison(state, output, request, true));
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_catalog_commit_failed_event& event)
    -> epoch_transition_output
{
    const auto match =
        operation_payload_matches(state, event.Context, event.Mutation);
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    if(event.Failure == epoch_failure::catalog_conflict)
    {
        auto output = epoch_transition_output{};
        state.PendingIngestion.reset();
        if(state.PendingComparison.has_value())
        {
            output.Requests.emplace_back(
                cancel_epoch_work_request{
                    .Request = state.PendingComparison->Request,
                    .Generation = state.PendingComparison->Generation,
                });
            state.PendingComparison.reset();
        }
        state.Catalog.reset();
        diagnose(output, epoch_reason_code::catalog_conflict);

        const auto next_generation = advanced(state.EpochGeneration);
        if(!next_generation.has_value())
        {
            diagnose(output, epoch_reason_code::epoch_generation_exhausted);
            set_status(state, output, epoch_status::failed);
            return output;
        }
        if(!request_available(state))
        {
            diagnose(output, epoch_reason_code::request_counter_exhausted);
            set_status(state, output, epoch_status::failed);
            return output;
        }

        state.EpochGeneration = *next_generation;
        const auto request = take_request(state);
        state.PendingIngestion = pending_epoch_ingestion{
            .Request = request,
            .Generation = state.EpochGeneration,
            .Phase = epoch_ingestion_phase::loading_catalog,
        };
        output.Authority = epoch_authority_delta{
            .Authority = state.Authority,
            .Revision = state.AcceptedRevision,
            .Generation = state.EpochGeneration,
        };
        output.Changed = output.Changed | epoch_changed_domain::authority;
        output.Requests.emplace_back(
            load_epoch_catalog_request{
                .Context = current_load_context(state, request),
            });
        set_status(state, output, epoch_status::loading_catalog);
        return output;
    }
    auto output = epoch_transition_output{};
    clear_ingestion(
        state, output, epoch_status::failed, reason_for(event.Failure));
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const epoch_catalog_commit_cancelled_event& event)
    -> epoch_transition_output
{
    const auto match =
        operation_payload_matches(state, event.Context, event.Mutation);
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    switch(event.Progress)
    {
    case epoch_commit_progress::ref_commit_in_progress:
        return reject(epoch_reason_code::cancellation_after_point_of_no_return);
    case epoch_commit_progress::before_ref_commit:
        break;
    default:
        return reject(epoch_reason_code::invalid_observation);
    }

    auto output = epoch_transition_output{};
    clear_ingestion(
        state, output, epoch_status::cancelled, epoch_reason_code::cancelled);
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const comparison_resolved_event& event)
    -> epoch_transition_output
{
    const auto match = comparison_context_match(state, event.Context);
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    if(!valid(event.Candidates.Catalog)
       || event.Candidates.Catalog != *state.Catalog)
    {
        state.PendingComparison.reset();
        auto output = reject(epoch_reason_code::completion_identity_mismatch);
        set_status(state, output, epoch_status::failed);
        return output;
    }

    const auto resolution = resolve_comparison(
        *state.Authority, state.RequestedSelection, event.Candidates);
    if(resolution.Reason != epoch_reason_code::none
       && resolution.Reason != epoch_reason_code::unavailable)
    {
        auto output = epoch_transition_output{};
        state.PendingComparison.reset();
        diagnose(output, resolution.Reason);
        set_status(state, output, epoch_status::failed);
        return output;
    }

    const bool changed = state.AcceptedComparison != resolution.Pair;
    std::optional<comparison_revision> next_revision;
    if(changed)
    {
        next_revision = advanced(state.AcceptedComparisonRevision);
        if(!next_revision.has_value())
        {
            state.PendingComparison.reset();
            state.AcceptedComparison.reset();
            auto output =
                reject(epoch_reason_code::comparison_revision_exhausted);
            output.Comparison = comparison_resolution_delta{
                .Selection = state.RequestedSelection,
                .Revision = state.AcceptedComparisonRevision,
            };
            output.Changed = output.Changed | epoch_changed_domain::comparison;
            set_status(state, output, epoch_status::unavailable);
            return output;
        }
    }

    state.PendingComparison.reset();
    if(changed)
    {
        state.AcceptedComparison = resolution.Pair;
        state.AcceptedComparisonRevision = *next_revision;
    }

    epoch_transition_output output;
    output.Comparison = comparison_resolution_delta{
        .Selection = state.RequestedSelection,
        .Pair = resolution.Pair,
        .Revision = state.AcceptedComparisonRevision,
        .AvailableForDiff =
            resolution.Pair.has_value() && available_for_diff(*resolution.Pair),
    };
    output.Changed = epoch_changed_domain::comparison;
    set_status(
        state,
        output,
        output.Comparison->AvailableForDiff ? epoch_status::ready
                                            : epoch_status::unavailable);
    return output;
}

template<typename Event>
[[nodiscard]] auto finish_comparison(
    epoch_state& state,
    const Event& event,
    const epoch_status status,
    const epoch_reason_code reason) -> epoch_transition_output
{
    const auto match = comparison_context_match(state, event.Context);
    if(match != completion_match::current)
    {
        return reject_match(match);
    }
    state.PendingComparison.reset();
    auto output = reject(reason);
    set_status(state, output, status);
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const comparison_resolve_failed_event& event)
    -> epoch_transition_output
{
    return finish_comparison(
        state, event, epoch_status::failed, reason_for(event.Failure));
}

[[nodiscard]] auto handle(
    epoch_state& state, const comparison_resolve_cancelled_event& event)
    -> epoch_transition_output
{
    return finish_comparison(
        state, event, epoch_status::cancelled, epoch_reason_code::cancelled);
}

[[nodiscard]] auto start_selection(
    epoch_state& state, const comparison_selection& selection)
    -> epoch_transition_output
{
    if(!state.Authority.has_value() || !state.AcceptedRevision.has_value())
    {
        return reject(epoch_reason_code::no_active_authority);
    }
    if(!state.Catalog.has_value())
    {
        return reject(epoch_reason_code::no_catalog);
    }
    if(selection == state.RequestedSelection
       && state.AcceptedComparison.has_value()
       && !state.PendingComparison.has_value())
    {
        return {};
    }
    const auto generation = advanced(state.ComparisonGeneration);
    if(!generation.has_value())
    {
        return reject(epoch_reason_code::comparison_generation_exhausted);
    }
    if(!request_available(state))
    {
        return reject(epoch_reason_code::request_counter_exhausted);
    }

    epoch_transition_output output;
    if(state.PendingComparison.has_value())
    {
        output.Requests.emplace_back(
            cancel_epoch_work_request{
                .Request = state.PendingComparison->Request,
                .Generation = state.PendingComparison->Generation,
            });
    }
    const bool changed = selection != state.RequestedSelection;
    state.RequestedSelection = selection;
    state.ComparisonGeneration = *generation;
    const auto request = take_request(state);
    state.PendingComparison = pending_comparison_resolution{
        .Request = request,
        .Generation = *generation,
    };
    if(changed)
    {
        state.AcceptedComparison.reset();
        output.Selection = comparison_selection_delta{.Selection = selection};
        output.Changed = epoch_changed_domain::selection;
    }
    output.Requests.emplace_back(
        resolve_comparison_request{
            .Context = current_comparison_context(state, request),
        });
    set_status(state, output, epoch_status::resolving);
    return output;
}

[[nodiscard]] auto handle(
    epoch_state& state, const select_named_comparison_intent& intent)
    -> epoch_transition_output
{
    if(!valid_alias(intent.Alias))
    {
        return reject(epoch_reason_code::invalid_selection);
    }
    if(!state.Catalog.has_value()
       || intent.ExpectedCatalogRevision != state.Catalog->Revision)
    {
        return reject(epoch_reason_code::catalog_revision_mismatch);
    }
    return start_selection(state, named_selection(intent.Alias));
}

[[nodiscard]] auto anchor_belongs(
    const epoch_state& state, const comparison_anchor_identity& anchor) noexcept
    -> bool
{
    return state.Authority.has_value() && state.Catalog.has_value()
        && valid(anchor) && anchor.Authority == state.Authority->Scope
        && anchor.Incarnation == state.Catalog->Incarnation;
}

[[nodiscard]] auto handle(
    epoch_state& state, const select_comparison_anchors_intent& intent)
    -> epoch_transition_output
{
    if(!state.Catalog.has_value()
       || intent.ExpectedCatalogRevision != state.Catalog->Revision)
    {
        return reject(epoch_reason_code::catalog_revision_mismatch);
    }
    if(!anchor_belongs(state, intent.Left)
       || !anchor_belongs(state, intent.Right))
    {
        return reject(epoch_reason_code::invalid_selection);
    }
    return start_selection(
        state, explicit_selection(intent.Left, intent.Right));
}

[[nodiscard]] auto handle(
    epoch_state& state, const select_comparison_anchor_intent& intent)
    -> epoch_transition_output
{
    if(!valid_side(intent.Side))
    {
        return reject(epoch_reason_code::invalid_selection);
    }
    if(!state.Catalog.has_value()
       || intent.ExpectedCatalogRevision != state.Catalog->Revision)
    {
        return reject(epoch_reason_code::catalog_revision_mismatch);
    }
    if(!anchor_belongs(state, intent.Anchor))
    {
        return reject(epoch_reason_code::invalid_selection);
    }
    if(!state.AcceptedComparison.has_value())
    {
        return reject(epoch_reason_code::no_resolved_comparison);
    }

    auto left = state.AcceptedComparison->Left.Identity;
    auto right = state.AcceptedComparison->Right.Identity;
    if(intent.Side == comparison_side::left)
    {
        left = intent.Anchor;
    }
    else
    {
        right = intent.Anchor;
    }
    return start_selection(state, explicit_selection(left, right));
}

[[nodiscard]] auto handle(
    epoch_state& state, const cancel_epoch_refresh_intent&)
    -> epoch_transition_output
{
    if(!state.PendingIngestion.has_value())
    {
        return reject(epoch_reason_code::stale_completion);
    }
    if(state.PendingIngestion->CancellationRequested)
    {
        return {};
    }
    state.PendingIngestion->CancellationRequested = true;
    epoch_transition_output output;
    output.Requests.emplace_back(
        cancel_epoch_work_request{
            .Request = state.PendingIngestion->Request,
            .Generation = state.PendingIngestion->Generation,
        });
    output.Commands = epoch_command_delta{.Commands = epoch_commands(state)};
    output.Changed = epoch_changed_domain::commands;
    return output;
}

} // namespace

auto valid(const pull_request_authority& authority) noexcept -> bool
{
    return valid_nonzero(authority.Profile.value())
        && valid_nonzero(authority.ProfileRevision.value())
        && valid_nonzero(authority.Host.value())
        && authority.HostAuthority.valid()
        && valid_nonzero(authority.Account.value())
        && valid_nonzero(authority.Viewer.value())
        && valid_nonzero(authority.Repository.value())
        && authority.Cache.valid()
        && valid_nonzero(authority.PullRequest.value())
        && authority.Number.valid() && authority.Scope.valid();
}

auto valid(const accepted_pull_request_revision& revision) noexcept -> bool
{
    const bool valid_origin =
        revision.Origin == pull_request_revision_origin::live
        || revision.Origin
            == pull_request_revision_origin::explicit_historical_import;
    const bool valid_review = !revision.CurrentViewerReview.has_value()
        || (valid_nonzero(revision.CurrentViewerReview->Review.value())
            && valid_nonzero(revision.CurrentViewerReview->Actor.value())
            && valid(revision.CurrentViewerReview->Order)
            && eligible_review_state(revision.CurrentViewerReview->State)
            && revision.CurrentViewerReview->Commit.valid()
            && revision.CurrentViewerReview->Commit.algorithm()
                == revision.Base.algorithm());

    return valid_nonzero(revision.Source.Snapshot.value())
        && valid_nonzero(revision.Source.IdentityGeneration.value())
        && valid_nonzero(revision.Source.PullRequestGeneration.value())
        && valid_origin && revision.BaseBranch.valid() && revision.Base.valid()
        && revision.Head.valid()
        && revision.Base.algorithm() == revision.Head.algorithm()
        && valid_review;
}

auto valid(const epoch_catalog_identity& catalog) noexcept -> bool
{
    return catalog.Incarnation.valid() && valid(catalog.Revision);
}

auto valid(const review_epoch_id& epoch) noexcept -> bool
{
    return epoch.Authority.valid() && epoch.Incarnation.valid()
        && valid(epoch.Ordinal);
}

auto valid(const comparison_anchor_identity& anchor) noexcept -> bool
{
    if(!anchor.Authority.valid() || !anchor.Incarnation.valid())
    {
        return false;
    }
    switch(anchor.Kind)
    {
    case comparison_anchor_kind::epoch_base:
    case comparison_anchor_kind::epoch_head:
        return valid(anchor.Epoch) && !anchor.Review.valid();
    case comparison_anchor_kind::review_commit:
        return anchor.Epoch.value() == 0 && anchor.Review.valid();
    }
    return false;
}

auto valid(const comparison_anchor& anchor) noexcept -> bool
{
    return valid(anchor.Identity) && anchor.Commit.valid()
        && valid_availability(anchor.Availability);
}

auto available_for_diff(const comparison_pair& pair) noexcept -> bool
{
    return valid(pair.Left) && valid(pair.Right)
        && pair.Left.Identity.Authority == pair.Right.Identity.Authority
        && pair.Left.Identity.Incarnation == pair.Right.Identity.Incarnation
        && pair.Left.Commit.algorithm() == pair.Right.Commit.algorithm()
        && pair.Left.Availability == anchor_availability::available
        && pair.Right.Availability == anchor_availability::available;
}

auto epoch_commands(const epoch_state& state) noexcept
    -> std::array<epoch_command_projection, 3>
{
    const bool active =
        state.Authority.has_value() && state.AcceptedRevision.has_value();
    const bool catalog_ready = active && state.Catalog.has_value();
    epoch_reason_code refresh_reason = epoch_reason_code::no_active_authority;
    if(active)
    {
        if(state.PendingIngestion.has_value())
        {
            refresh_reason = epoch_reason_code::unavailable;
        }
        else if(!advanced(state.EpochGeneration).has_value())
        {
            refresh_reason = epoch_reason_code::epoch_generation_exhausted;
        }
        else if(!request_available(state))
        {
            refresh_reason = epoch_reason_code::request_counter_exhausted;
        }
        else
        {
            refresh_reason = epoch_reason_code::none;
        }
    }

    epoch_reason_code selection_reason = epoch_reason_code::no_active_authority;
    if(active)
    {
        if(!catalog_ready)
        {
            selection_reason = epoch_reason_code::no_catalog;
        }
        else if(!advanced(state.ComparisonGeneration).has_value())
        {
            selection_reason =
                epoch_reason_code::comparison_generation_exhausted;
        }
        else if(!request_available(state))
        {
            selection_reason = epoch_reason_code::request_counter_exhausted;
        }
        else
        {
            selection_reason = epoch_reason_code::none;
        }
    }

    const bool can_refresh = refresh_reason == epoch_reason_code::none;
    const bool can_select = selection_reason == epoch_reason_code::none;
    const bool can_cancel = state.PendingIngestion.has_value()
        && !state.PendingIngestion->CancellationRequested;

    return {
        epoch_command_projection{
            .Id = epoch_command_id::refresh_epochs,
            .Enabled = can_refresh,
            .DisabledReason = refresh_reason,
        },
        epoch_command_projection{
            .Id = epoch_command_id::select_comparison,
            .Enabled = can_select,
            .DisabledReason = selection_reason,
        },
        epoch_command_projection{
            .Id = epoch_command_id::cancel_epoch_refresh,
            .Enabled = can_cancel,
            .DisabledReason = can_cancel ? epoch_reason_code::none
                                         : epoch_reason_code::unavailable,
        },
    };
}

auto material_relation(
    const pull_request_authority& current_authority,
    const std::optional<epoch_material_identity>& current,
    const pull_request_authority& prior_authority,
    const std::optional<epoch_material_identity>& prior) noexcept
    -> epoch_material_relation
{
    if(current_authority != prior_authority || !current.has_value()
       || !prior.has_value() || !valid_material(*current)
       || !valid_material(*prior))
    {
        return epoch_material_relation::unknown;
    }
    return *current == *prior ? epoch_material_relation::same
                              : epoch_material_relation::different;
}

auto plan_epoch_mutation(
    const pull_request_authority& authority,
    const accepted_pull_request_revision& revision,
    const epoch_catalog_identity& catalog,
    const epoch_preparation_observation& observation) noexcept
    -> planned_epoch_mutation
{
    if(!valid(authority) || !valid(revision) || !valid(catalog)
       || (revision.CurrentViewerReview.has_value()
           && revision.CurrentViewerReview->Actor != authority.Viewer)
       || !valid_lineage(observation.HeadLineage)
       || (observation.CatalogCurrentViewerReview.has_value()
           && (!valid_catalog_review(*observation.CatalogCurrentViewerReview)
               || observation.CatalogCurrentViewerReview->Actor
                   != authority.Viewer))
       || (observation.ProposedCurrentViewerReview.has_value()
           && (!valid_catalog_review(*observation.ProposedCurrentViewerReview)
               || observation.ProposedCurrentViewerReview->Actor
                   != authority.Viewer)))
    {
        return {.Rejection = epoch_reason_code::invalid_observation};
    }
    if(!valid(observation.ObservedAt)
       || (observation.Material.has_value()
           && (!valid_material(*observation.Material)
               || observation.Material->BaseTree.algorithm()
                   != revision.Base.algorithm())))
    {
        return {.Rejection = epoch_reason_code::invalid_observation};
    }
    if(observation.LatestLive.has_value()
       && (!valid_summary(authority, catalog, *observation.LatestLive)
           || !live(*observation.LatestLive)))
    {
        return {.Rejection = epoch_reason_code::invalid_observation};
    }

    const bool historical = revision.Origin
        == pull_request_revision_origin::explicit_historical_import;
    const bool initial = !historical && !observation.LatestLive.has_value();
    bool branch_changed = false;
    bool base_changed = false;
    bool head_changed = false;
    epoch_material_relation relation = epoch_material_relation::unknown;
    if(observation.LatestLive.has_value())
    {
        branch_changed =
            revision.BaseBranch != observation.LatestLive->BaseBranch;
        base_changed = revision.Base != observation.LatestLive->Base;
        head_changed = revision.Head != observation.LatestLive->Head;
        relation = material_relation(
            authority,
            observation.Material,
            authority,
            observation.LatestLive->Material);
    }

    const bool revision_changed =
        historical || initial || branch_changed || base_changed || head_changed;
    if(revision_changed)
    {
        if(!observation.ProposedOrdinal.has_value()
           || !valid(*observation.ProposedOrdinal))
        {
            return {.Rejection = epoch_reason_code::invalid_observation};
        }
        if(observation.LatestLive.has_value()
           && observation.LatestLive->Id.Ordinal.value()
               >= maximum_epoch_ordinal)
        {
            return {
                .Rejection = epoch_reason_code::epoch_ordinal_exhausted,
            };
        }
        if(observation.LatestLive.has_value()
           && observation.ProposedOrdinal->value()
               <= observation.LatestLive->Id.Ordinal.value())
        {
            return {.Rejection = epoch_reason_code::invalid_observation};
        }
    }

    if((historical || initial)
       && observation.HeadLineage != head_lineage::not_applicable)
    {
        return {.Rejection = epoch_reason_code::invalid_observation};
    }
    if(!historical && !initial
       && ((head_changed
            && observation.HeadLineage == head_lineage::not_applicable)
           || (!head_changed
               && observation.HeadLineage != head_lineage::not_applicable)))
    {
        return {.Rejection = epoch_reason_code::invalid_observation};
    }

    if(revision.CurrentViewerReview.has_value())
    {
        if(!observation.ProposedCurrentViewerReview.has_value()
           || !same_provider_review(
               *revision.CurrentViewerReview,
               *observation.ProposedCurrentViewerReview))
        {
            return {.Rejection = epoch_reason_code::invalid_observation};
        }
    }
    else if(observation.ProposedCurrentViewerReview.has_value())
    {
        return {.Rejection = epoch_reason_code::invalid_observation};
    }

    if(!historical && !initial && !branch_changed && !base_changed
       && !head_changed)
    {
        if(!revision.CurrentViewerReview.has_value())
        {
            return {};
        }
        if(observation.ProposedCurrentViewerReview
           == observation.CatalogCurrentViewerReview)
        {
            return {};
        }
        if(catalog.Revision.value() >= maximum_epoch_catalog_revision)
        {
            return {
                .Rejection = epoch_reason_code::catalog_revision_exhausted,
            };
        }
        return {
            .Mutation =
                associate_review_mutation{
                    .Review = *observation.ProposedCurrentViewerReview,
                },
        };
    }

    if(catalog.Revision.value() >= maximum_epoch_catalog_revision)
    {
        return {
            .Rejection = epoch_reason_code::catalog_revision_exhausted,
        };
    }

    epoch_cause cause;
    cause.Material = relation;
    if(historical)
    {
        cause.Origin = epoch_origin::explicit_historical_import;
        cause.Base = epoch_base_transition::not_applicable;
        cause.Head = epoch_head_transition::not_applicable;
        cause.Primary = epoch_primary_cause::explicit_historical_import;
    }
    else if(initial)
    {
        cause.Origin = epoch_origin::initial_observation;
        cause.Base = epoch_base_transition::not_applicable;
        cause.Head = epoch_head_transition::not_applicable;
        cause.Primary = epoch_primary_cause::initial_observation;
    }
    else
    {
        cause.Origin = epoch_origin::live_observation;
        cause.Base = branch_changed
            ? epoch_base_transition::base_branch_changed
            : (base_changed ? epoch_base_transition::base_oid_changed
                            : epoch_base_transition::unchanged);
        if(!head_changed)
        {
            cause.Head = epoch_head_transition::unchanged;
        }
        else
        {
            switch(observation.HeadLineage)
            {
            case head_lineage::descendant:
                cause.Head = epoch_head_transition::descendant;
                break;
            case head_lineage::non_fast_forward:
                cause.Head = epoch_head_transition::non_fast_forward;
                break;
            case head_lineage::unknown:
                cause.Head = epoch_head_transition::lineage_unknown;
                break;
            case head_lineage::not_applicable:
                return {
                    .Rejection = epoch_reason_code::invalid_observation,
                };
            }
        }

        if(branch_changed)
        {
            cause.Primary = epoch_primary_cause::base_branch_change;
        }
        else if(cause.Head == epoch_head_transition::non_fast_forward)
        {
            cause.Primary = epoch_primary_cause::force_push_rebase;
        }
        else if(cause.Head == epoch_head_transition::descendant)
        {
            cause.Primary = epoch_primary_cause::author_upload;
        }
        else if(cause.Head == epoch_head_transition::lineage_unknown)
        {
            cause.Primary =
                epoch_primary_cause::head_change_lineage_unavailable;
        }
        else
        {
            cause.Primary = epoch_primary_cause::base_update;
        }
    }

    const review_epoch_summary epoch{
        .Id =
            review_epoch_id{
                .Authority = authority.Scope,
                .Incarnation = catalog.Incarnation,
                .Ordinal = *observation.ProposedOrdinal,
            },
        .Source = revision.Source,
        .BaseBranch = revision.BaseBranch,
        .Base = revision.Base,
        .Head = revision.Head,
        .Material = observation.Material,
        .Cause = cause,
        .ObservedAt = observation.ObservedAt,
        .BaseAvailability = anchor_availability::available,
        .HeadAvailability = anchor_availability::available,
    };
    return {
        .Mutation =
            append_epoch_mutation{
                .Epoch = epoch,
                .Review = observation.ProposedCurrentViewerReview,
            },
    };
}

auto resolve_comparison(
    const pull_request_authority& authority,
    const comparison_selection& selection,
    const comparison_candidates& candidates) noexcept -> resolved_comparison
{
    if(!valid(authority) || !valid(candidates.Catalog))
    {
        return {.Reason = epoch_reason_code::invalid_observation};
    }
    const auto resolution = select_pair(authority, selection, candidates);
    if(!resolution.Pair.has_value())
    {
        return resolution;
    }
    if(resolution.Pair->Left.Identity.Authority != authority.Scope
       || resolution.Pair->Right.Identity.Authority != authority.Scope
       || resolution.Pair->Left.Identity.Incarnation
           != candidates.Catalog.Incarnation
       || resolution.Pair->Right.Identity.Incarnation
           != candidates.Catalog.Incarnation
       || resolution.Pair->Left.Commit.algorithm()
           != resolution.Pair->Right.Commit.algorithm())
    {
        return {.Reason = epoch_reason_code::invalid_selection};
    }
    if(!available_for_diff(*resolution.Pair))
    {
        return {
            .Pair = resolution.Pair,
            .Reason = epoch_reason_code::unavailable,
        };
    }
    return resolution;
}

auto transition(epoch_state& state, const epoch_event& event)
    -> epoch_transition_output
{
    return std::visit(
        [&state](const auto& concrete) { return handle(state, concrete); },
        event);
}
} // namespace bpr::core
