#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/git_protocol_v2.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application
{
namespace
{
inline constexpr std::size_t maximum_identity_attempts = 32U;
inline constexpr std::size_t maximum_pending_preparations = 100U;
inline constexpr std::size_t maximum_recovery_operations = 1'000U;
inline constexpr std::size_t maximum_cache_actors = 1'000U;

struct cache_actor_key
{
    core::application_session_id ApplicationSession;
    core::host_profile_id Profile;
    core::github_profile_revision ProfileRevision;
    core::git_host_id Host;
    core::host_authority_fingerprint HostAuthority;
    core::github_account_id Account;
    core::repository_id Repository;
    core::git_hash_algorithm ObjectFormat =
        core::git_hash_algorithm::invalid;

    auto operator<=>(const cache_actor_key&) const = default;
};

[[nodiscard]] auto actor_key(
    const accepted_pull_request_handoff& handoff) noexcept -> cache_actor_key
{
    return {
        .ApplicationSession = handoff.ApplicationSession,
        .Profile = handoff.Profile,
        .ProfileRevision = handoff.ProfileRevision,
        .Host = handoff.Host,
        .HostAuthority = handoff.HostAuthority,
        .Account = handoff.Account,
        .Repository = handoff.Repository,
        .ObjectFormat = handoff.ObjectFormat,
    };
}

template<typename Identity>
[[nodiscard]] auto mint_identity(epoch_entropy_port& entropy)
    -> std::optional<Identity>
{
    const auto bytes = entropy.generate();
    if(!bytes || !application::valid(*bytes))
    {
        return std::nullopt;
    }
    return Identity{*bytes};
}

[[nodiscard]] auto catalog_failure(
    const epoch_catalog_failure failure) noexcept -> core::epoch_failure
{
    switch(failure)
    {
    case epoch_catalog_failure::cancelled:
        return core::epoch_failure::unavailable;
    case epoch_catalog_failure::quota_exceeded:
        return core::epoch_failure::quota_exceeded;
    case epoch_catalog_failure::ordinal_exhausted:
        return core::epoch_failure::ordinal_exhausted;
    case epoch_catalog_failure::revision_conflict:
    case epoch_catalog_failure::catalog_mismatch:
    case epoch_catalog_failure::authority_mismatch:
    case epoch_catalog_failure::operation_identity_conflict:
    case epoch_catalog_failure::source_observation_conflict:
    case epoch_catalog_failure::operation_conflict:
    case epoch_catalog_failure::phase_conflict:
    case epoch_catalog_failure::busy:
        return core::epoch_failure::catalog_conflict;
    case epoch_catalog_failure::schema_mismatch:
    case epoch_catalog_failure::integrity_failure:
    case epoch_catalog_failure::repair_required:
        return core::epoch_failure::repair_required;
    case epoch_catalog_failure::invalid_request:
        return core::epoch_failure::malformed_observation;
    case epoch_catalog_failure::invalid_path:
    case epoch_catalog_failure::invalid_permissions:
    case epoch_catalog_failure::unavailable:
    case epoch_catalog_failure::platform_unsupported:
        return core::epoch_failure::unavailable;
    }
    return core::epoch_failure::unavailable;
}

[[nodiscard]] auto cache_failure(const git_cache_failure failure) noexcept
    -> core::epoch_failure
{
    switch(failure)
    {
    case git_cache_failure::cancelled:
        return core::epoch_failure::unavailable;
    case git_cache_failure::quota_exceeded:
        return core::epoch_failure::quota_exceeded;
    case git_cache_failure::repair_required:
    case git_cache_failure::invalid_manifest:
    case git_cache_failure::invalid_repository:
    case git_cache_failure::inventory_mismatch:
    case git_cache_failure::authorization_mismatch:
    case git_cache_failure::object_mismatch:
    case git_cache_failure::ref_conflict:
        return core::epoch_failure::repair_required;
    case git_cache_failure::invalid_request:
    case git_cache_failure::invalid_authority:
        return core::epoch_failure::malformed_observation;
    case git_cache_failure::invalid_path:
    case git_cache_failure::lock_unavailable:
    case git_cache_failure::spool_failure:
    case git_cache_failure::process_failure:
        return core::epoch_failure::unavailable;
    }
    return core::epoch_failure::unavailable;
}

[[nodiscard]] auto catalog_key(
    const accepted_pull_request_handoff& handoff)
    -> epoch_catalog_authority_key
{
    return {
        .Profile = handoff.Profile,
        .ProfileRevision = handoff.ProfileRevision,
        .Host = handoff.Host,
        .HostAuthority = handoff.HostAuthority,
        .Account = handoff.Account,
        .Viewer = handoff.Viewer,
        .Repository = handoff.Repository,
        .PullRequest = handoff.PullRequest,
        .Number = handoff.Number,
        .ObjectFormat = handoff.ObjectFormat,
    };
}

[[nodiscard]] auto agrees(
    const core::pull_request_authority& authority,
    const accepted_pull_request_handoff& handoff) noexcept -> bool
{
    return authority.Profile == handoff.Profile
        && authority.ProfileRevision == handoff.ProfileRevision
        && authority.Host == handoff.Host
        && authority.HostAuthority == handoff.HostAuthority
        && authority.Account == handoff.Account
        && authority.Viewer == handoff.Viewer
        && authority.Repository == handoff.Repository
        && authority.PullRequest == handoff.PullRequest
        && authority.Number == handoff.Number;
}

[[nodiscard]] auto accepted_revision(
    const accepted_pull_request_handoff& handoff,
    const core::epoch_source_observation_id observation)
    -> core::accepted_pull_request_revision
{
    return {
        .Source =
            {
                .Observation = observation,
                .Snapshot = handoff.Snapshot,
                .IdentityGeneration = handoff.IdentityGeneration,
                .PullRequestGeneration = handoff.PullRequestGeneration,
            },
        .Origin = handoff.Origin,
        .BaseBranch = handoff.BaseBranch,
        .Base = handoff.Base,
        .Head = handoff.Head,
        .CurrentViewerReview = handoff.CurrentViewerReview,
    };
}

[[nodiscard]] auto invalidation_identity(
    const accepted_pull_request_handoff& handoff)
    -> pull_request_authority_invalidation
{
    return {
        .ApplicationSession = handoff.ApplicationSession,
        .Profile = handoff.Profile,
        .ProfileRevision = handoff.ProfileRevision,
        .Host = handoff.Host,
        .HostAuthority = handoff.HostAuthority,
        .Account = handoff.Account,
        .Viewer = handoff.Viewer,
        .Repository = handoff.Repository,
        .PullRequest = handoff.PullRequest,
        .IdentityGeneration = handoff.IdentityGeneration,
    };
}

[[nodiscard]] auto projected_revision(
    const core::accepted_pull_request_revision& revision)
    -> epoch_revision_projection
{
    return {
        .Snapshot = revision.Source.Snapshot,
        .IdentityGeneration = revision.Source.IdentityGeneration,
        .PullRequestGeneration = revision.Source.PullRequestGeneration,
        .Origin = revision.Origin,
        .BaseBranch = revision.BaseBranch,
        .Base = revision.Base,
        .Head = revision.Head,
        .CurrentViewerReview = revision.CurrentViewerReview,
    };
}

[[nodiscard]] auto projected_row(const core::review_epoch_summary& epoch)
    -> epoch_row_projection
{
    return {
        .Id = epoch.Id,
        .Snapshot = epoch.Source.Snapshot,
        .IdentityGeneration = epoch.Source.IdentityGeneration,
        .PullRequestGeneration = epoch.Source.PullRequestGeneration,
        .BaseBranch = epoch.BaseBranch,
        .Base = epoch.Base,
        .Head = epoch.Head,
        .Material = epoch.Material,
        .Cause = epoch.Cause,
        .ObservedAt = epoch.ObservedAt,
        .BaseAvailability = epoch.BaseAvailability,
        .HeadAvailability = epoch.HeadAvailability,
    };
}

[[nodiscard]] auto review_anchor(
    const core::pull_request_authority& authority,
    const core::epoch_catalog_identity catalog,
    const core::catalog_review_association& review)
    -> core::comparison_anchor
{
    return {
        .Identity =
            {
                .Authority = authority.Scope,
                .Incarnation = catalog.Incarnation,
                .Kind = core::comparison_anchor_kind::review_commit,
                .Review = review.Association,
            },
        .Commit = review.Commit,
        .Availability = review.Availability,
    };
}

[[nodiscard]] auto recovery_request(const append_operation_record& record)
    -> std::optional<git_operation_recovery_request>
{
    if(!application::valid(record))
    {
        return std::nullopt;
    }
    auto request = git_operation_recovery_request{
        .Operation = record.Context.Operation,
        .Scope = record.StagingBase.Authority,
        .Ordinal = record.ReservedOrdinal,
        .Base = record.Context.Revision.Base,
        .Head = record.Context.Revision.Head,
    };
    if(record.Spool && record.Preimport)
    {
        request.Spool = git_recovery_spool_expectation{
            .ExpectedPack = record.Spool->ExpectedPack,
            .CompletedSpool = record.Spool->Spool,
            .PreimportPacks = record.Preimport->Packs,
            .PreimportRepositoryBytes = record.Preimport->RepositoryBytes,
            .ExpectedPackPreexisting =
                record.Preimport->ExpectedPackPreexisting,
        };
    }
    if(record.Install)
    {
        request.Install = git_recovery_install_expectation{
            .Pack = record.Install->Pack,
            .InstalledPack = record.Install->InstalledPack,
            .InstalledIndex = record.Install->InstalledIndex,
            .BaseTree = record.Install->BaseTree,
            .HeadTree = record.Install->HeadTree,
        };
    }
    return application::valid(request) ? std::optional{std::move(request)}
                                       : std::nullopt;
}

[[nodiscard]] auto recovered_prepared_revision(
    const append_operation_record& record)
    -> std::optional<git_prepared_revision>
{
    if(!record.Install)
    {
        return std::nullopt;
    }
    auto prepared = git_prepared_revision{
        .Operation = record.Context.Operation,
        .Pack = record.Install->Pack,
        .InstalledPack = record.Install->InstalledPack,
        .InstalledIndex = record.Install->InstalledIndex,
        .Base = record.Install->Base,
        .BaseTree = record.Install->BaseTree,
        .Head = record.Install->Head,
        .HeadTree = record.Install->HeadTree,
        .HeadLineage = record.Install->HeadLineage,
        .ImportedBytes = record.Install->InstalledPack.Bytes,
    };
    return application::valid(prepared) ? std::optional{std::move(prepared)}
                                        : std::nullopt;
}

[[nodiscard]] auto expected_retained_pair(
    const append_operation_record& record) -> git_retained_ref_pair
{
    return {
        .Scope = record.FinalBase.Authority,
        .Ordinal = record.ReservedOrdinal,
        .Base = record.Context.Revision.Base,
        .Head = record.Context.Revision.Head,
    };
}

[[nodiscard]] auto same_append_identity(
    const append_operation_record& prior,
    const append_operation_record& next) noexcept -> bool
{
    return application::valid(next) && next.Context == prior.Context
        && next.ReservedOrdinal == prior.ReservedOrdinal
        && next.StagingBase == prior.StagingBase
        && next.StagingHead == prior.StagingHead
        && next.FinalBase == prior.FinalBase
        && next.FinalHead == prior.FinalHead;
}

[[nodiscard]] auto same_maintenance_identity(
    const maintenance_operation_record& prior,
    const maintenance_operation_record& next) noexcept -> bool
{
    return application::valid(next) && next.Incarnation == prior.Incarnation
        && next.Operation == prior.Operation && next.Kind == prior.Kind
        && next.Cache == prior.Cache && next.Epochs == prior.Epochs
        && next.Scopes == prior.Scopes;
}

[[nodiscard]] auto exact_maintenance_transition(
    const maintenance_operation_record& prior,
    const maintenance_operation_record& next,
    const epoch_catalog_operation_phase phase) noexcept -> bool
{
    return same_maintenance_identity(prior, next)
        && application::can_advance(prior.OperationRevision)
        && next.OperationRevision.value() == prior.OperationRevision.value() + 1
        && next.Phase == phase
        && (phase == epoch_catalog_operation_phase::repair_required
                ? next.RepairFromPhase == std::optional{prior.Phase}
                : !next.RepairFromPhase);
}

[[nodiscard]] auto exact_prune_intent(
    const maintenance_operation_record& record,
    const begin_prune_request& request,
    const core::repository_cache_scope_id cache) noexcept -> bool
{
    return application::valid(record)
        && record.Incarnation == request.Catalog.Incarnation
        && record.Operation == request.Operation
        && record.Kind == epoch_catalog_operation_kind::prune
        && record.Phase == epoch_catalog_operation_phase::prune_intent
        && record.OperationRevision == epoch_catalog_operation_revision{1}
        && record.Cache == cache && record.Epochs == request.Epochs
        && record.Scopes.empty() && !record.RepairFromPhase;
}

[[nodiscard]] auto exact_cache_delete_intent(
    const maintenance_operation_record& record,
    const begin_cache_delete_request& request) noexcept -> bool
{
    return application::valid(record)
        && record.Incarnation == request.Impact.Incarnation
        && record.Operation == request.Operation
        && record.Kind == epoch_catalog_operation_kind::cache_delete
        && record.Phase
            == epoch_catalog_operation_phase::cache_delete_intent_sealed
        && record.OperationRevision == epoch_catalog_operation_revision{1}
        && record.Cache == request.Impact.Cache && record.Epochs.empty()
        && record.Scopes == request.Impact.Scopes && !record.RepairFromPhase;
}

[[nodiscard]] auto recovery_operation_identity(
    const epoch_recovery_operation& operation) noexcept
    -> core::epoch_operation_id
{
    return std::visit(
        [](const auto& value)
        {
            using value_type = std::remove_cvref_t<decltype(value)>;
            if constexpr(
                std::is_same_v<value_type, append_operation_record>)
            {
                return value.Context.Operation;
            }
            else
            {
                return value.Operation;
            }
        },
        operation);
}

[[nodiscard]] auto exact_append_reservation(
    const append_operation_record& record,
    const append_reservation_request& request,
    const core::pull_request_authority_scope_id authority) noexcept -> bool
{
    return application::valid(record)
        && record.Context == request.Context
        && record.ReservedOrdinal == request.ProposedOrdinal
        && record.OperationRevision
            == epoch_catalog_operation_revision{1}
        && record.Phase == epoch_catalog_operation_phase::preparing
        && !record.RepairFromPhase && !record.RecoveryInputCompacted
        && !record.Spool && !record.Preimport && !record.Install
        && !record.Candidate
        && record.StagingBase
            == epoch_catalog_ref_identity{
                .Authority = authority,
                .Ordinal = request.ProposedOrdinal,
                .Operation = request.Context.Operation,
                .Staging = true,
            }
        && record.StagingHead
            == epoch_catalog_ref_identity{
                .Authority = authority,
                .Ordinal = request.ProposedOrdinal,
                .Operation = request.Context.Operation,
                .Staging = true,
                .Head = true,
            }
        && record.FinalBase
            == epoch_catalog_ref_identity{
                .Authority = authority,
                .Ordinal = request.ProposedOrdinal,
            }
        && record.FinalHead
            == epoch_catalog_ref_identity{
                .Authority = authority,
                .Ordinal = request.ProposedOrdinal,
                .Head = true,
            };
}

[[nodiscard]] auto exact_append_transition(
    const append_operation_record& prior,
    const append_operation_record& next,
    const epoch_catalog_operation_phase phase) noexcept -> bool
{
    if(
        !same_append_identity(prior, next)
        || !application::can_advance(prior.OperationRevision)
        || next.OperationRevision.value()
            != prior.OperationRevision.value() + 1
        || next.Phase != phase)
    {
        return false;
    }
    const auto terminal =
        phase == epoch_catalog_operation_phase::complete
        || phase == epoch_catalog_operation_phase::aborted;
    const auto exact_repair_origin =
        phase == epoch_catalog_operation_phase::repair_required
        ? next.RepairFromPhase == std::optional{prior.Phase}
        : !next.RepairFromPhase;
    return exact_repair_origin && next.Install == prior.Install
        && next.Candidate == prior.Candidate
        && (
            terminal
                ? next.RecoveryInputCompacted && !next.Spool
                    && !next.Preimport
                : !next.RecoveryInputCompacted
                    && next.Spool == prior.Spool
                    && next.Preimport == prior.Preimport);
}

[[nodiscard]] auto exact_spool_record(
    const append_operation_record& prior,
    const append_operation_record& next,
    const record_spool_request& request) noexcept -> bool
{
    return same_append_identity(prior, next)
        && next.Phase == epoch_catalog_operation_phase::spool_recorded
        && application::can_advance_nonterminal(prior.OperationRevision)
        && next.OperationRevision.value()
            == prior.OperationRevision.value() + 1
        && !next.RepairFromPhase && !next.RecoveryInputCompacted
        && next.Spool == std::optional{request.Spool}
        && next.Preimport == std::optional{request.Preimport}
        && next.Install == prior.Install
        && next.Candidate == prior.Candidate;
}

[[nodiscard]] auto exact_install_record(
    const append_operation_record& prior,
    const append_operation_record& next,
    const record_install_request& request) noexcept -> bool
{
    return same_append_identity(prior, next)
        && next.Phase == epoch_catalog_operation_phase::imported
        && application::can_advance_nonterminal(prior.OperationRevision)
        && next.OperationRevision.value()
            == prior.OperationRevision.value() + 1
        && !next.RepairFromPhase && !next.RecoveryInputCompacted
        && next.Spool == prior.Spool && next.Preimport == prior.Preimport
        && next.Install == std::optional{request.Install}
        && next.Candidate == prior.Candidate;
}

[[nodiscard]] auto exact_prepared_revision(
    const git_prepared_revision& prepared,
    const git_prepare_revision_request& request) noexcept -> bool
{
    return application::valid(prepared)
        && prepared.Operation == request.Authorization.Operation
        && prepared.Pack == request.Authorization.ExpectedPack
        && prepared.Base == request.Base && prepared.Head == request.Head
        && prepared.ImportedBytes == request.Receipt.Spool.Bytes
        && prepared.InstalledPack.Bytes == request.Receipt.Spool.Bytes;
}

[[nodiscard]] auto exact_candidate_record(
    const append_operation_record& prior,
    const append_operation_record& next,
    const record_candidate_request& request) noexcept -> bool
{
    return same_append_identity(prior, next)
        && next.Phase
            == epoch_catalog_operation_phase::commit_authorized
        && application::can_advance_nonterminal(prior.OperationRevision)
        && next.OperationRevision.value()
            == prior.OperationRevision.value() + 1
        && !next.RepairFromPhase && !next.RecoveryInputCompacted
        && next.Spool == prior.Spool && next.Preimport == prior.Preimport
        && next.Install == prior.Install
        && next.Candidate == std::optional{request.Candidate};
}

[[nodiscard]] auto exact_catalog_commit(
    const catalog_commit_result& result,
    const core::epoch_catalog_identity prior,
    const std::optional<core::review_epoch_id> inserted) noexcept -> bool
{
    return core::valid(result.Catalog) && result.Changed
        && prior.Revision.value() < core::maximum_epoch_catalog_revision
        && result.Catalog.Incarnation == prior.Incarnation
        && result.Catalog.Revision.value() == prior.Revision.value() + 1
        && result.InsertedEpoch == inserted;
}
} // namespace

struct epoch_application_host::implementation
{
    enum class prepared_kind : std::uint8_t
    {
        observation_only,
        append,
    };

    struct prepared_operation final
    {
        prepared_kind Kind = prepared_kind::observation_only;
        core::epoch_operation_id Operation;
        std::optional<append_operation_record> Record;
        std::optional<git_prepared_revision> Git;
    };

    // One cache-scoped storage actor is retained across revision and pull
    // request context replacement. Its FIFO serializes catalog/cache effects
    // that share the lifetime-exclusive cache session, while each context
    // remains independently discardable.
    struct authority_effect_actor final
    {
        std::mutex QueueMutex;
        std::deque<std::function<void()>> Queue;
        bool Scheduled = false;
        std::unique_ptr<git_cache_session> Cache;
    };

    struct authority_context final
    {
        authority_context(
            accepted_pull_request_handoff accepted,
            core::pull_request_authority authority,
            core::accepted_pull_request_revision revision,
            std::unique_ptr<epoch_catalog_session> catalog,
            std::shared_ptr<authority_effect_actor> actor)
            : Handoff{std::move(accepted)}
            , Authority{std::move(authority)}
            , Revision{std::move(revision)}
            , Catalog{std::move(catalog)}
            , Actor{std::move(actor)}
        {
            Preparations.reserve(maximum_pending_preparations);
        }

        [[nodiscard]] auto store(prepared_operation operation) -> bool
        {
            if(
                Preparations.size() >= maximum_pending_preparations
                || std::ranges::any_of(
                    Preparations,
                    [&](const auto& existing)
                    {
                        return existing.Operation == operation.Operation;
                    }))
            {
                return false;
            }
            Preparations.push_back(std::move(operation));
            return true;
        }

        [[nodiscard]] auto take(const core::epoch_operation_id operation)
            -> std::optional<prepared_operation>
        {
            const auto found = std::ranges::find(
                Preparations,
                operation,
                &prepared_operation::Operation);
            if(found == Preparations.end())
            {
                return std::nullopt;
            }
            auto result = std::move(*found);
            Preparations.erase(found);
            return result;
        }

        accepted_pull_request_handoff Handoff;
        core::pull_request_authority Authority;
        core::accepted_pull_request_revision Revision;
        std::unique_ptr<epoch_catalog_session> Catalog;
        std::shared_ptr<authority_effect_actor> Actor;
        std::vector<prepared_operation> Preparations;
    };

    struct job_control final
    {
        std::stop_source Stop;
        std::optional<core::epoch_request_id> Request;
        std::optional<core::epoch_generation> EpochGeneration;
        std::optional<core::comparison_generation> ComparisonGeneration;
        std::optional<std::uint64_t> OfferToken;
    };

    struct completion_gate final
    {
        std::recursive_mutex Barrier;
        std::atomic<implementation*> Host = nullptr;
        std::atomic_bool DeliveryFailed = false;
    };

    struct deferred_accept final
    {
        std::shared_ptr<accepted_pull_request_handoff> Handoff;
    };
    struct deferred_invalidate final
    {
        pull_request_authority_invalidation Invalidation;
    };
    struct deferred_dispatch final
    {
        epoch_intent Intent;
    };
    struct deferred_shutdown final
    {
    };
    using deferred_ingress = std::variant<
        deferred_accept,
        deferred_invalidate,
        deferred_dispatch,
        deferred_shutdown>;
    using worker_completion = std::function<void(implementation&)>;
    using worker_task =
        std::function<worker_completion(std::stop_token)>;

    implementation(
        core::epoch_state initial_state,
        git_cache_port& caches,
        epoch_catalog_port& catalogs,
        epoch_entropy_port& entropy,
        epoch_clock_port& clock,
        effect_executor& effects,
        decision_executor& decision_lane,
        epoch_projection_port& projections)
        : State{std::move(initial_state)}
        , Caches{caches}
        , Catalogs{catalogs}
        , Entropy{entropy}
        , Clock{clock}
        , Effects{effects}
        , DecisionLane{decision_lane}
        , Projections{projections}
        , CompletionGate{std::make_shared<completion_gate>()}
    {
        CompletionGate->Host.store(this, std::memory_order_release);
    }

    ~implementation()
    {
        const std::lock_guard barrier{CompletionGate->Barrier};
        CompletionGate->Host.store(nullptr, std::memory_order_release);
    }

    template<typename Action>
    void run_turn(Action&& action)
    {
        TurnActive = true;
        try
        {
            std::forward<Action>(action)();
            const auto count = DeferredIngress.size();
            for(std::size_t index = 0U; index < count; ++index)
            {
                auto ingress = std::move(DeferredIngress.front());
                DeferredIngress.pop_front();
                std::visit(
                    [this](auto&& value)
                    {
                        using value_type = std::decay_t<decltype(value)>;
                        if constexpr(
                            std::is_same_v<value_type, deferred_accept>)
                        {
                            accept_now(std::move(*value.Handoff));
                        }
                        else if constexpr(
                            std::is_same_v<value_type, deferred_invalidate>)
                        {
                            invalidate_now(value.Invalidation);
                        }
                        else if constexpr(
                            std::is_same_v<value_type, deferred_dispatch>)
                        {
                            dispatch_now(value.Intent);
                        }
                        else
                        {
                            shutdown_now();
                        }
                    },
                    std::move(ingress));
            }
            TurnActive = false;
            schedule_deferred_ingress();
        }
        catch(...)
        {
            DeferredIngress.clear();
            TurnActive = false;
            throw;
        }
    }

    void schedule_deferred_ingress() noexcept
    {
        if(DeferredIngress.empty() || DeferredIngressScheduled)
        {
            return;
        }
        DeferredIngressScheduled = true;
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        try
        {
            DecisionLane.post(
                [gate]
                {
                    const auto locked = gate.lock();
                    if(!locked)
                    {
                        return;
                    }
                    const std::lock_guard barrier{locked->Barrier};
                    auto* const host =
                        locked->Host.load(std::memory_order_acquire);
                    if(host == nullptr)
                    {
                        return;
                    }
                    host->DeferredIngressScheduled = false;
                    host->run_turn([] {});
                });
        }
        catch(...)
        {
            DeferredIngressScheduled = false;
            DeferredIngress.clear();
            ProjectionDeliveryFailed = true;
        }
    }

    static void enqueue_completion(
        const std::weak_ptr<completion_gate>& gate,
        decision_executor* const decision_lane,
        std::shared_ptr<job_control> control,
        worker_completion completion) noexcept
    {
        try
        {
            decision_lane->post(
                [gate,
                 control = std::move(control),
                 completion = std::move(completion)]() mutable
                {
                    const auto locked = gate.lock();
                    if(!locked)
                    {
                        return;
                    }
                    const std::lock_guard barrier{locked->Barrier};
                    auto* const host =
                        locked->Host.load(std::memory_order_acquire);
                    if(host == nullptr)
                    {
                        return;
                    }
                    host->run_turn(
                        [host, &control, &completion]
                        {
                            host->finish_job(control);
                            completion(*host);
                        });
                });
        }
        catch(...)
        {
            const auto locked = gate.lock();
            if(!locked)
            {
                return;
            }
            // A conforming decision lane never rejects. Preserve the live
            // job (and therefore non-quiescence) instead of retiring it or
            // mutating core-owned state from this worker thread.
            locked->DeliveryFailed.store(true, std::memory_order_release);
        }
    }

    static void drain_actor(
        const std::shared_ptr<authority_effect_actor>& actor) noexcept
    {
        while(true)
        {
            std::function<void()> work;
            {
                std::scoped_lock lock{actor->QueueMutex};
                if(actor->Queue.empty())
                {
                    actor->Scheduled = false;
                    return;
                }
                work = std::move(actor->Queue.front());
                actor->Queue.pop_front();
            }
            try
            {
                work();
            }
            catch(...)
            {
                // submit_effect workers contain their own typed exception
                // boundary. Keep the actor live if an allocator or foreign
                // callable violates that boundary unexpectedly.
            }
        }
    }

    void enqueue_actor_work(
        const std::shared_ptr<authority_effect_actor>& actor,
        std::function<void()> work)
    {
        bool schedule = false;
        {
            std::scoped_lock lock{actor->QueueMutex};
            actor->Queue.push_back(std::move(work));
            if(!actor->Scheduled)
            {
                actor->Scheduled = true;
                schedule = true;
            }
        }
        if(!schedule)
        {
            return;
        }
        try
        {
            Effects.post([actor]
            {
                drain_actor(actor);
            });
        }
        catch(...)
        {
            std::scoped_lock lock{actor->QueueMutex};
            actor->Scheduled = false;
            actor->Queue.clear();
            throw;
        }
    }

    auto submit_effect(
        worker_task task,
        worker_completion exception_completion,
        const std::optional<core::epoch_request_id> request = std::nullopt,
        const std::optional<core::epoch_generation> epoch_generation =
            std::nullopt,
        const std::optional<core::comparison_generation>
            comparison_generation = std::nullopt,
        const std::optional<std::uint64_t> offer_token = std::nullopt,
        std::shared_ptr<authority_effect_actor> actor = {}) -> bool
    {
        auto control = std::make_shared<job_control>();
        control->Request = request;
        control->EpochGeneration = epoch_generation;
        control->ComparisonGeneration = comparison_generation;
        control->OfferToken = offer_token;
        Jobs.push_back(control);
        const auto gate = std::weak_ptr<completion_gate>{CompletionGate};
        auto* const decision_lane = &DecisionLane;
        try
        {
            auto work = [gate,
                 decision_lane,
                 control,
                 exception_completion,
                 task = std::move(task)]() mutable
                {
                    auto completion = worker_completion{};
                    try
                    {
                        completion = task(control->Stop.get_token());
                    }
                    catch(...)
                    {
                        completion = std::move(exception_completion);
                        if(!completion)
                        {
                            completion = [](implementation& host)
                            {
                                host.publish_diagnostic(
                                    core::epoch_reason_code::unavailable);
                            };
                        }
                    }
                    enqueue_completion(
                        gate,
                        decision_lane,
                        std::move(control),
                        std::move(completion));
                };
            if(actor)
            {
                enqueue_actor_work(std::move(actor), std::move(work));
            }
            else
            {
                Effects.post(std::move(work));
            }
            return true;
        }
        catch(...)
        {
            finish_job(control);
            if(exception_completion)
            {
                exception_completion(*this);
            }
            else
            {
                publish_diagnostic(core::epoch_reason_code::unavailable);
            }
            return false;
        }
    }

    void finish_job(const std::shared_ptr<job_control>& control) noexcept
    {
        const auto match = std::ranges::find(Jobs, control);
        if(match != Jobs.end())
        {
            Jobs.erase(match);
        }
    }

    void cancel_all_jobs() noexcept
    {
        for(const auto& job : Jobs)
        {
            static_cast<void>(job->Stop.request_stop());
        }
    }

    [[nodiscard]] auto actors_idle() const noexcept -> bool
    {
        for(const auto& [key, actor] : CacheActors)
        {
            static_cast<void>(key);
            std::scoped_lock lock{actor->QueueMutex};
            if(actor->Scheduled || !actor->Queue.empty())
            {
                return false;
            }
        }
        return true;
    }

    void cancel_authority_jobs() noexcept
    {
        for(const auto& job : Jobs)
        {
            if(!job->OfferToken)
            {
                static_cast<void>(job->Stop.request_stop());
            }
        }
    }

    void cancel_offer_job(const std::uint64_t token) noexcept
    {
        for(const auto& job : Jobs)
        {
            if(job->OfferToken == token)
            {
                static_cast<void>(job->Stop.request_stop());
            }
        }
    }

    void cancel_work(const core::cancel_epoch_work_request& request) noexcept
    {
        for(const auto& job : Jobs)
        {
            if(job->Request != request.Request)
            {
                continue;
            }
            const bool generation_matches = std::visit(
                [&job](const auto generation)
                {
                    using generation_type =
                        std::remove_cvref_t<decltype(generation)>;
                    if constexpr(std::is_same_v<
                                     generation_type,
                                     core::epoch_generation>)
                    {
                        return job->EpochGeneration == generation;
                    }
                    else
                    {
                        return job->ComparisonGeneration == generation;
                    }
                },
                request.Generation);
            if(generation_matches)
            {
                static_cast<void>(job->Stop.request_stop());
            }
        }
    }

    [[nodiscard]] static auto advance_append(
        epoch_catalog_session& catalog_session,
        const append_operation_record& record,
        const core::epoch_catalog_identity catalog,
        const epoch_catalog_operation_phase next,
        const std::stop_token stop)
        -> epoch_catalog_result<append_operation_record>
    {
        const auto evidence = required_transition_evidence(
            epoch_catalog_operation_kind::append,
            record.Phase,
            next);
        if(!evidence)
        {
            return std::unexpected{epoch_catalog_failure::phase_conflict};
        }
        auto advanced = catalog_session.transition_operation(
            operation_transition_request{
                .Catalog = catalog,
                .Operation = record.Context.Operation,
                .Kind = epoch_catalog_operation_kind::append,
                .Expected = record.Phase,
                .Next = next,
                .Evidence = *evidence,
                .ExpectedOperationRevision = record.OperationRevision,
            },
            stop);
        if(!advanced)
        {
            return std::unexpected{advanced.error()};
        }
        const auto* append = std::get_if<append_operation_record>(&*advanced);
        if(
            append == nullptr
            || !exact_append_transition(record, *append, next))
        {
            return std::unexpected{epoch_catalog_failure::operation_conflict};
        }
        return *append;
    }

    [[nodiscard]] static auto advance_append(
        authority_context& context,
        const append_operation_record& record,
        const core::epoch_catalog_identity catalog,
        const epoch_catalog_operation_phase next,
        const std::stop_token stop)
        -> epoch_catalog_result<append_operation_record>
    {
        return advance_append(
            *context.Catalog, record, catalog, next, stop);
    }

    [[nodiscard]] static auto advance_maintenance(
        epoch_catalog_session& catalog,
        const maintenance_operation_record& record,
        const core::epoch_catalog_identity expected_catalog,
        const epoch_catalog_operation_phase next,
        const std::stop_token stop)
        -> epoch_catalog_result<maintenance_operation_record>
    {
        const auto evidence =
            required_transition_evidence(record.Kind, record.Phase, next);
        if(!evidence)
        {
            return std::unexpected{epoch_catalog_failure::phase_conflict};
        }
        auto advanced = catalog.transition_operation(
            operation_transition_request{
                .Catalog = expected_catalog,
                .Operation = record.Operation,
                .Kind = record.Kind,
                .Expected = record.Phase,
                .Next = next,
                .Evidence = *evidence,
                .ExpectedOperationRevision = record.OperationRevision,
            },
            stop);
        if(!advanced)
        {
            return std::unexpected{advanced.error()};
        }
        const auto* maintenance =
            std::get_if<maintenance_operation_record>(&*advanced);
        if(maintenance == nullptr
            || !exact_maintenance_transition(record, *maintenance, next))
        {
            return std::unexpected{epoch_catalog_failure::operation_conflict};
        }
        return *maintenance;
    }

    [[nodiscard]] static auto open_actor_cache(authority_effect_actor& actor,
        git_cache_port& caches,
        const git_cache_open_request& expected,
        const std::stop_token stop)
        -> git_cache_result<git_cache_session*>
    {
        if(actor.Cache && actor.Cache->request() != expected)
        {
            actor.Cache.reset();
        }
        if(!actor.Cache)
        {
            auto opened = caches.open(expected, stop);
            if(!opened)
            {
                return std::unexpected{opened.error()};
            }
            if(
                !*opened
                || !application::valid((*opened)->request())
                || (*opened)->request() != expected)
            {
                return std::unexpected{
                    git_cache_failure::repair_required};
            }
            actor.Cache = std::move(*opened);
        }
        return actor.Cache.get();
    }

    static void mark_append_repair(
        epoch_catalog_session& catalog,
        const append_operation_record& record)
    {
        if(record.Phase == epoch_catalog_operation_phase::repair_required)
        {
            return;
        }
        static_cast<void>(advance_append(
            catalog,
            record,
            catalog.binding().Catalog,
            epoch_catalog_operation_phase::repair_required,
            {}));
    }

    static void mark_maintenance_repair(epoch_catalog_session& catalog,
        const maintenance_operation_record& record)
    {
        if(record.Phase == epoch_catalog_operation_phase::repair_required)
        {
            return;
        }
        static_cast<void>(advance_maintenance(catalog,
            record,
            catalog.binding().Catalog,
            epoch_catalog_operation_phase::repair_required,
            {}));
    }

    [[nodiscard]] static auto recover_append(
        epoch_catalog_session& catalog,
        git_cache_session& cache,
        epoch_entropy_port& entropy,
        append_operation_record record,
        const std::stop_token stop)
        -> std::expected<void, core::epoch_failure>
    {
        const auto failed_cache = [&](const git_cache_failure failure)
            -> std::expected<void, core::epoch_failure>
        {
            const auto mapped = cache_failure(failure);
            if(mapped == core::epoch_failure::repair_required)
            {
                mark_append_repair(catalog, record);
            }
            return std::unexpected{mapped};
        };
        const auto request = recovery_request(record);
        if(!request)
        {
            mark_append_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(record.Phase == epoch_catalog_operation_phase::repair_required)
        {
            return std::unexpected{core::epoch_failure::repair_required};
        }

        const auto before_point_of_no_return =
            record.Phase == epoch_catalog_operation_phase::preparing
            || record.Phase
                == epoch_catalog_operation_phase::spool_recorded
            || record.Phase == epoch_catalog_operation_phase::imported
            || record.Phase
                == epoch_catalog_operation_phase::staging_ready
            || record.Phase
                == epoch_catalog_operation_phase::abort_requested;
        if(before_point_of_no_return)
        {
            if(
                record.Phase
                != epoch_catalog_operation_phase::abort_requested)
            {
                const auto requested = advance_append(
                    catalog,
                    record,
                    catalog.binding().Catalog,
                    epoch_catalog_operation_phase::abort_requested,
                    stop);
                if(!requested)
                {
                    return std::unexpected{
                        catalog_failure(requested.error())};
                }
                record = *requested;
            }
            auto cleanup =
                cache.abort_recovered_operation(*request, stop);
            if(!cleanup)
            {
                cleanup = cache.abort_recovered_operation(*request, stop);
            }
            if(!cleanup)
            {
                if(cleanup.error() == git_cache_failure::repair_required)
                {
                    mark_append_repair(catalog, record);
                }
                return failed_cache(cleanup.error());
            }
            const auto completed = advance_append(
                catalog,
                record,
                catalog.binding().Catalog,
                epoch_catalog_operation_phase::aborted,
                stop);
            if(!completed)
            {
                return std::unexpected{catalog_failure(completed.error())};
            }
            return {};
        }

        if(
            record.Phase
                != epoch_catalog_operation_phase::commit_authorized
            && record.Phase
                != epoch_catalog_operation_phase::ref_commit_in_progress
            && record.Phase
                != epoch_catalog_operation_phase::refs_committed
            && record.Phase
                != epoch_catalog_operation_phase::visible_cleanup_pending)
        {
            mark_append_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(!record.Install || !record.Candidate)
        {
            mark_append_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }

        const auto physical_stop =
            record.Phase
                    == epoch_catalog_operation_phase::commit_authorized
                ? stop
                : std::stop_token{};
        auto physical =
            cache.inspect_operation_recovery(*request, physical_stop);
        if(!physical)
        {
            return failed_cache(physical.error());
        }
        if(physical->Operation != request->Operation)
        {
            mark_append_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }
        const auto durable_physical_match =
            physical->Installed == git_recovery_match::exact
            && physical->Objects == git_recovery_object_match::exact
            && (
                physical->Spool == git_recovery_match::absent
                || physical->Spool == git_recovery_match::exact);
        if(!durable_physical_match)
        {
            mark_append_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }

        if(
            record.Phase
            == epoch_catalog_operation_phase::commit_authorized)
        {
            if(physical->Refs != git_recovery_ref_topology::staging)
            {
                mark_append_repair(catalog, record);
                return std::unexpected{core::epoch_failure::repair_required};
            }
            const auto started = advance_append(
                catalog,
                record,
                catalog.binding().Catalog,
                epoch_catalog_operation_phase::ref_commit_in_progress,
                stop);
            if(!started)
            {
                return std::unexpected{catalog_failure(started.error())};
            }
            record = *started;
        }

        const auto post_commit_stop = std::stop_token{};

        if(
            record.Phase
            == epoch_catalog_operation_phase::ref_commit_in_progress)
        {
            if(physical->Refs == git_recovery_ref_topology::staging)
            {
                const auto prepared = recovered_prepared_revision(record);
                const auto token = entropy.generate();
                if(!prepared || !token || !application::valid(*token))
                {
                    return std::unexpected{core::epoch_failure::unavailable};
                }
                const auto promoted = cache.promote_retained(
                    *prepared,
                    git_ref_promotion_authorization{
                        .CatalogIncarnation =
                            record.Context.Catalog.Incarnation,
                        .ExpectedCatalogRevision =
                            record.Context.Catalog.Revision,
                        .Operation = record.Context.Operation,
                        .Scope = record.FinalBase.Authority,
                        .Ordinal = record.ReservedOrdinal,
                        .Base = record.Context.Revision.Base,
                        .Head = record.Context.Revision.Head,
                        .DurableCommitToken = *token,
                    },
                    post_commit_stop);
                if(!promoted)
                {
                    auto after_promotion =
                        cache.inspect_operation_recovery(
                            *request, post_commit_stop);
                    if(!after_promotion)
                    {
                        return failed_cache(after_promotion.error());
                    }
                    if(after_promotion->Operation != request->Operation)
                    {
                        mark_append_repair(catalog, record);
                        return std::unexpected{
                            core::epoch_failure::repair_required};
                    }
                    const auto exact_after =
                        after_promotion->Installed
                            == git_recovery_match::exact
                        && after_promotion->Objects
                            == git_recovery_object_match::exact
                        && (
                            after_promotion->Spool
                                == git_recovery_match::absent
                            || after_promotion->Spool
                                == git_recovery_match::exact);
                    if(
                        exact_after
                        && after_promotion->Refs
                            == git_recovery_ref_topology::final)
                    {
                        physical = std::move(after_promotion);
                    }
                    else if(
                        exact_after
                        && after_promotion->Refs
                            == git_recovery_ref_topology::staging)
                    {
                        return std::unexpected{
                            core::epoch_failure::unavailable};
                    }
                    else
                    {
                        mark_append_repair(catalog, record);
                        return std::unexpected{
                            core::epoch_failure::repair_required};
                    }
                }
                else if(*promoted != expected_retained_pair(record))
                {
                    mark_append_repair(catalog, record);
                    return std::unexpected{
                        core::epoch_failure::repair_required};
                }
            }
            else if(physical->Refs != git_recovery_ref_topology::final)
            {
                mark_append_repair(catalog, record);
                return std::unexpected{core::epoch_failure::repair_required};
            }
            const auto committed = advance_append(
                catalog,
                record,
                catalog.binding().Catalog,
                epoch_catalog_operation_phase::refs_committed,
                post_commit_stop);
            if(!committed)
            {
                return std::unexpected{catalog_failure(committed.error())};
            }
            record = *committed;
        }
        else if(
            record.Phase
                == epoch_catalog_operation_phase::visible_cleanup_pending
            ? (
                  physical->Final != git_recovery_match::exact
                  || (
                      physical->Staging != git_recovery_match::absent
                      && physical->Staging
                          != git_recovery_match::exact))
            : physical->Refs != git_recovery_ref_topology::final)
        {
            mark_append_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }

        const auto retained = cache.inspect_retained(
            record.ReservedOrdinal, post_commit_stop);
        if(!retained)
        {
            return failed_cache(retained.error());
        }
        if(*retained != expected_retained_pair(record))
        {
            mark_append_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }

        auto cleanup_catalog = catalog.binding().Catalog;
        if(
            record.Phase == epoch_catalog_operation_phase::refs_committed)
        {
            if(catalog.binding().Catalog != record.Context.Catalog)
            {
                mark_append_repair(catalog, record);
                return std::unexpected{core::epoch_failure::repair_required};
            }
            const auto finalized = catalog.finalize_append(
                finalize_append_request{
                    .Catalog = record.Context.Catalog,
                    .Operation = record.Context.Operation,
                    .ExpectedOperationRevision = record.OperationRevision,
                    .Mutation = *record.Candidate,
                    .ObservedRefs = *retained,
                },
                post_commit_stop);
            if(!finalized)
            {
                return std::unexpected{catalog_failure(finalized.error())};
            }
            if(
                !exact_catalog_commit(
                    *finalized,
                    record.Context.Catalog,
                    record.Candidate->Epoch.Id)
                || catalog.binding().Catalog != finalized->Catalog)
            {
                mark_append_repair(catalog, record);
                return std::unexpected{
                    core::epoch_failure::repair_required};
            }
            cleanup_catalog = finalized->Catalog;
            record.Phase =
                epoch_catalog_operation_phase::visible_cleanup_pending;
            record.OperationRevision = epoch_catalog_operation_revision{
                record.OperationRevision.value() + 1};
        }

        auto cleaned = cache.complete_recovered_operation(
            *request, post_commit_stop);
        if(!cleaned)
        {
            const auto after_cleanup = cache.inspect_operation_recovery(
                *request, post_commit_stop);
            if(!after_cleanup)
            {
                return failed_cache(after_cleanup.error());
            }
            const auto exact_cleanup_state =
                after_cleanup->Operation == request->Operation
                && after_cleanup->Installed == git_recovery_match::exact
                && after_cleanup->Objects
                    == git_recovery_object_match::exact
                && after_cleanup->Final == git_recovery_match::exact
                && (
                    after_cleanup->Staging == git_recovery_match::absent
                    || after_cleanup->Staging == git_recovery_match::exact)
                && (
                    after_cleanup->Spool == git_recovery_match::absent
                    || after_cleanup->Spool == git_recovery_match::exact);
            if(!exact_cleanup_state)
            {
                mark_append_repair(catalog, record);
                return std::unexpected{
                    core::epoch_failure::repair_required};
            }
            cleaned = cache.complete_recovered_operation(
                *request, post_commit_stop);
            if(!cleaned)
            {
                return std::unexpected{core::epoch_failure::unavailable};
            }
        }
        const auto completed_result = advance_append(
            catalog,
            record,
            cleanup_catalog,
            epoch_catalog_operation_phase::complete,
            post_commit_stop);
        if(!completed_result)
        {
            return std::unexpected{
                catalog_failure(completed_result.error())};
        }
        return {};
    }

    [[nodiscard]] static auto recover_maintenance(
        epoch_catalog_session& catalog,
        git_cache_port& caches,
        const accepted_pull_request_handoff& handoff,
        std::unique_ptr<git_cache_session>& cache,
        maintenance_operation_record record,
        const std::stop_token stop)
        -> std::expected<core::epoch_catalog_identity, core::epoch_failure>
    {
        const auto fail_catalog = [&](const epoch_catalog_failure failure)
            -> std::expected<core::epoch_catalog_identity, core::epoch_failure>
        {
            const auto mapped = catalog_failure(failure);
            if(mapped == core::epoch_failure::repair_required)
            {
                mark_maintenance_repair(catalog, record);
            }
            return std::unexpected{mapped};
        };
        const auto fail_cache = [&](const git_cache_failure failure)
            -> std::expected<core::epoch_catalog_identity, core::epoch_failure>
        {
            const auto mapped = cache_failure(failure);
            if(mapped == core::epoch_failure::repair_required)
            {
                mark_maintenance_repair(catalog, record);
            }
            return std::unexpected{mapped};
        };
        if(!application::valid(record)
            || record.Incarnation != catalog.binding().Catalog.Incarnation
            || record.Cache != catalog.binding().Authority.Cache)
        {
            mark_maintenance_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(record.Phase == epoch_catalog_operation_phase::repair_required)
        {
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(record.Phase == epoch_catalog_operation_phase::complete
            || record.Phase == epoch_catalog_operation_phase::aborted)
        {
            return catalog.binding().Catalog;
        }

        const auto before_point_of_no_return =
            !point_of_no_cancellation(record.Phase);
        if(before_point_of_no_return)
        {
            if(record.Phase != epoch_catalog_operation_phase::abort_requested)
            {
                const auto requested = advance_maintenance(catalog,
                    record,
                    catalog.binding().Catalog,
                    epoch_catalog_operation_phase::abort_requested,
                    stop);
                if(!requested)
                {
                    return fail_catalog(requested.error());
                }
                record = *requested;
            }
            const auto aborted = advance_maintenance(catalog,
                record,
                catalog.binding().Catalog,
                epoch_catalog_operation_phase::aborted,
                stop);
            if(!aborted)
            {
                return fail_catalog(aborted.error());
            }
            return catalog.binding().Catalog;
        }

        const auto expected_cache = git_cache_open_request{
            .ApplicationSession = handoff.ApplicationSession,
            .Authority = catalog.binding().Authority,
            .ObjectFormat = handoff.ObjectFormat,
        };
        const auto durable_token = record.Operation.bytes();
        const auto post_point_stop = std::stop_token{};

        if(record.Kind == epoch_catalog_operation_kind::prune)
        {
            if(record.Phase
                == epoch_catalog_operation_phase::ref_delete_in_progress)
            {
                if(!cache)
                {
                    auto opened = caches.open(expected_cache, post_point_stop);
                    if(!opened)
                    {
                        return fail_cache(opened.error());
                    }
                    if(!*opened || !application::valid((*opened)->request())
                        || (*opened)->request() != expected_cache)
                    {
                        mark_maintenance_repair(catalog, record);
                        return std::unexpected{
                            core::epoch_failure::repair_required};
                    }
                    cache = std::move(*opened);
                }
                std::vector<git_retained_ref_pair> absent;
                absent.reserve(record.Epochs.size());
                for(const auto& fact : record.Epochs)
                {
                    const auto pruned = cache->prune_retained(
                        git_ref_prune_authorization{
                            .CatalogIncarnation = record.Incarnation,
                            .ExpectedCatalogRevision =
                                catalog.binding().Catalog.Revision,
                            .Expected = fact.Retained,
                            .DurableIntentToken = durable_token,
                        },
                        post_point_stop);
                    if(!pruned)
                    {
                        return fail_cache(pruned.error());
                    }
                    absent.push_back(fact.Retained);
                }
                const auto before = catalog.binding().Catalog;
                const auto finalized = catalog.finalize_prune(
                    finalize_prune_request{
                        .Transition =
                            operation_transition_request{
                                .Catalog = before,
                                .Operation = record.Operation,
                                .Kind = record.Kind,
                                .Expected = record.Phase,
                                .Next = epoch_catalog_operation_phase::
                                    visible_cleanup_pending,
                                .Evidence = epoch_catalog_transition_evidence::
                                    exact_refs_absent,
                                .ExpectedOperationRevision =
                                    record.OperationRevision,
                            },
                        .ObservedAbsent = std::move(absent),
                    },
                    post_point_stop);
                if(!finalized)
                {
                    return fail_catalog(finalized.error());
                }
                if(!exact_catalog_commit(*finalized, before, std::nullopt))
                {
                    mark_maintenance_repair(catalog, record);
                    return std::unexpected{
                        core::epoch_failure::repair_required};
                }
                record.Phase =
                    epoch_catalog_operation_phase::visible_cleanup_pending;
                record.OperationRevision = epoch_catalog_operation_revision{
                    record.OperationRevision.value() + 1};
            }
            if(record.Phase
                != epoch_catalog_operation_phase::visible_cleanup_pending)
            {
                mark_maintenance_repair(catalog, record);
                return std::unexpected{core::epoch_failure::repair_required};
            }
            const auto completed = advance_maintenance(catalog,
                record,
                catalog.binding().Catalog,
                epoch_catalog_operation_phase::complete,
                post_point_stop);
            if(!completed)
            {
                return fail_catalog(completed.error());
            }
            return catalog.binding().Catalog;
        }

        if(record.Kind != epoch_catalog_operation_kind::cache_delete)
        {
            mark_maintenance_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }
        const auto delete_physical_cache =
            [&]() -> std::expected<void, core::epoch_failure>
        {
            cache.reset();
            const auto deleted = caches.delete_cache(expected_cache,
                git_cache_delete_authorization{
                    .ApplicationSession = handoff.ApplicationSession,
                    .Cache = record.Cache,
                    .DurableIntentToken = durable_token,
                },
                post_point_stop);
            if(!deleted)
            {
                const auto mapped = cache_failure(deleted.error());
                if(mapped == core::epoch_failure::repair_required)
                {
                    mark_maintenance_repair(catalog, record);
                }
                return std::unexpected{mapped};
            }
            return {};
        };
        if(record.Phase
            == epoch_catalog_operation_phase::quarantine_rename_in_progress)
        {
            if(const auto deleted = delete_physical_cache(); !deleted)
            {
                return std::unexpected{deleted.error()};
            }
            const auto quarantined = advance_maintenance(catalog,
                record,
                catalog.binding().Catalog,
                epoch_catalog_operation_phase::quarantined,
                post_point_stop);
            if(!quarantined)
            {
                return fail_catalog(quarantined.error());
            }
            record = *quarantined;
        }
        if(record.Phase == epoch_catalog_operation_phase::quarantined)
        {
            const auto before = catalog.binding();
            const auto finalized = catalog.finalize_cache_delete(
                finalize_cache_delete_request{
                    .Transition =
                        operation_transition_request{
                            .Catalog = before.Catalog,
                            .Operation = record.Operation,
                            .Kind = record.Kind,
                            .Expected = record.Phase,
                            .Next = epoch_catalog_operation_phase::
                                catalog_tombstoned_cleanup_pending,
                            .Evidence = epoch_catalog_transition_evidence::
                                catalog_transaction,
                            .ExpectedOperationRevision =
                                record.OperationRevision,
                        },
                },
                post_point_stop);
            if(!finalized)
            {
                return fail_catalog(finalized.error());
            }
            const auto& after = catalog.binding();
            if(!application::valid(after) || after.Authority != before.Authority
                || after.Catalog.Incarnation != before.Catalog.Incarnation
                || before.Catalog.Revision.value()
                    >= core::maximum_epoch_catalog_revision
                || after.Catalog.Revision.value()
                    != before.Catalog.Revision.value() + 1)
            {
                mark_maintenance_repair(catalog, record);
                return std::unexpected{core::epoch_failure::repair_required};
            }
            record.Phase = epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending;
            record.OperationRevision = epoch_catalog_operation_revision{
                record.OperationRevision.value() + 1};
        }
        if(record.Phase
            != epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending)
        {
            mark_maintenance_repair(catalog, record);
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(const auto deleted = delete_physical_cache(); !deleted)
        {
            return std::unexpected{deleted.error()};
        }
        const auto completed = advance_maintenance(catalog,
            record,
            catalog.binding().Catalog,
            epoch_catalog_operation_phase::complete,
            post_point_stop);
        if(!completed)
        {
            return fail_catalog(completed.error());
        }
        return catalog.binding().Catalog;
    }

    [[nodiscard]] static auto recover_catalog(epoch_catalog_session& catalog,
        git_cache_port& caches,
        epoch_entropy_port& entropy,
        const accepted_pull_request_handoff& handoff,
        std::unique_ptr<git_cache_session>& cache,
        const std::stop_token stop)
        -> std::expected<void, core::epoch_failure>
    {
        std::optional<core::epoch_operation_id> after;
        std::size_t recovered_count = 0U;
        while(true)
        {
            const auto page = catalog.recovery_page(
                after, maximum_epoch_recovery_page_size, stop);
            if(!page)
            {
                return std::unexpected{catalog_failure(page.error())};
            }
            if(
                !application::valid(*page)
                || page->Incarnation
                    != catalog.binding().Catalog.Incarnation
                || (
                    page->Continuation
                    && page->Operations.empty())
                || (after && page->Operations.empty())
                || (
                    after && page->Continuation
                    && !(*after < *page->Continuation))
                || page->Operations.size()
                    > maximum_recovery_operations - recovered_count)
            {
                return std::unexpected{core::epoch_failure::repair_required};
            }
            if(after && !page->Operations.empty())
            {
                const auto first = recovery_operation_identity(
                    page->Operations.front());
                // SQLite's continuation is the first omitted identity, so
                // the next page includes an identity equal to `after`.
                if(first != *after)
                {
                    return std::unexpected{
                        core::epoch_failure::repair_required};
                }
            }
            recovered_count += page->Operations.size();
            for(const auto& operation : page->Operations)
            {
                const auto* append =
                    std::get_if<append_operation_record>(&operation);
                if(append == nullptr)
                {
                    const auto* maintenance =
                        std::get_if<maintenance_operation_record>(&operation);
                    if(maintenance == nullptr)
                    {
                        return std::unexpected{
                            core::epoch_failure::repair_required};
                    }
                    const auto recovered = recover_maintenance(catalog,
                        caches,
                        handoff,
                        cache,
                        *maintenance,
                        stop);
                    if(!recovered)
                    {
                        return std::unexpected{recovered.error()};
                    }
                    continue;
                }
                if(!cache)
                {
                    const auto expected = git_cache_open_request{
                        .ApplicationSession = handoff.ApplicationSession,
                        .Authority = catalog.binding().Authority,
                        .ObjectFormat = handoff.ObjectFormat,
                    };
                    auto opened = caches.open(expected, stop);
                    if(!opened)
                    {
                        return std::unexpected{cache_failure(opened.error())};
                    }
                    if(!*opened)
                    {
                        return std::unexpected{
                            core::epoch_failure::repair_required};
                    }
                    if(
                        !application::valid((*opened)->request())
                        || (*opened)->request() != expected)
                    {
                        return std::unexpected{
                            core::epoch_failure::repair_required};
                    }
                    cache = std::move(*opened);
                }
                const auto recovered = recover_append(
                    catalog, *cache, entropy, *append, stop);
                if(!recovered)
                {
                    return recovered;
                }
            }
            if(!page->Continuation)
            {
                return {};
            }
            after = page->Continuation;
        }
    }

    [[nodiscard]] static auto pending_append(
        epoch_catalog_session& catalog,
        const core::epoch_operation_id operation)
        -> std::expected<
            std::optional<append_operation_record>,
            core::epoch_failure>
    {
        auto page = catalog.recovery_page(operation, 1U, {});
        if(!page)
        {
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(
            !application::valid(*page)
            || page->Incarnation
                != catalog.binding().Catalog.Incarnation
            || page->Operations.size() > 1U
            || (page->Continuation && page->Operations.empty()))
        {
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(page->Operations.empty())
        {
            return std::optional<append_operation_record>{};
        }
        const auto* append =
            std::get_if<append_operation_record>(&page->Operations.front());
        const auto found = recovery_operation_identity(
            page->Operations.front());
        if(found < operation)
        {
            return std::unexpected{core::epoch_failure::repair_required};
        }
        if(found != operation)
        {
            return std::optional<append_operation_record>{};
        }
        if(append == nullptr)
        {
            return std::unexpected{core::epoch_failure::repair_required};
        }
        return std::optional{*append};
    }

    [[nodiscard]] static auto abort_preparation(
        authority_context& context,
        prepared_operation& operation) -> bool
    {
        if(operation.Kind == prepared_kind::observation_only)
        {
            return true;
        }
        if(!operation.Record)
        {
            return false;
        }
        auto record = *operation.Record;
        if(record.Phase == epoch_catalog_operation_phase::aborted
           || record.Phase == epoch_catalog_operation_phase::complete)
        {
            return true;
        }
        if(record.Phase == epoch_catalog_operation_phase::repair_required
           || point_of_no_cancellation(record.Phase))
        {
            return false;
        }
        const auto mark_repair = [&context, &record, &operation]()
        {
            const auto repaired = advance_append(
                context,
                record,
                record.Context.Catalog,
                epoch_catalog_operation_phase::repair_required,
                {});
            if(repaired)
            {
                record = *repaired;
                operation.Record = record;
            }
        };
        if(record.Phase != epoch_catalog_operation_phase::abort_requested)
        {
            const auto requested = advance_append(
                context,
                record,
                record.Context.Catalog,
                epoch_catalog_operation_phase::abort_requested,
                {});
            if(!requested)
            {
                mark_repair();
                return false;
            }
            record = *requested;
        }
        if(context.Actor->Cache)
        {
            const auto request = recovery_request(record);
            if(!request)
            {
                mark_repair();
                return false;
            }
            auto aborted =
                context.Actor->Cache->abort_recovered_operation(
                    *request, {});
            if(!aborted)
            {
                aborted = context.Actor->Cache->abort_recovered_operation(
                    *request, {});
            }
            if(!aborted)
            {
                mark_repair();
                return false;
            }
        }
        const auto completed = advance_append(
            context,
            record,
            record.Context.Catalog,
            epoch_catalog_operation_phase::aborted,
            {});
        if(!completed)
        {
            mark_repair();
            return false;
        }
        operation.Record = *completed;
        return true;
    }

    void discard_context_preparation(
        std::shared_ptr<authority_context> context,
        const std::optional<core::epoch_operation_id> expected = std::nullopt)
    {
        auto actor = context->Actor;
        auto* const caches = &Caches;
        submit_effect(
            [context = std::move(context), expected, caches](
                const std::stop_token stop)
                -> worker_completion
            {
                const auto requires_cache = std::ranges::any_of(
                    context->Preparations,
                    [&](const auto& operation)
                    {
                        return operation.Kind == prepared_kind::append
                            && (
                                !expected
                                || operation.Operation == *expected);
                    });
                if(requires_cache)
                {
                    const auto cache = open_actor_cache(
                        *context->Actor,
                        *caches,
                        git_cache_open_request{
                            .ApplicationSession =
                                context->Handoff.ApplicationSession,
                            .Authority = context->Authority,
                            .ObjectFormat = context->Handoff.ObjectFormat,
                        },
                        stop);
                    if(!cache)
                    {
                        return [](implementation& host)
                        {
                            host.publish_diagnostic(
                                core::epoch_reason_code::unavailable);
                        };
                    }
                }
                if(expected)
                {
                    auto operation = context->take(*expected);
                    if(operation)
                    {
                        static_cast<void>(
                            abort_preparation(*context, *operation));
                    }
                }
                else
                {
                    while(!context->Preparations.empty())
                    {
                        auto operation = context->take(
                            context->Preparations.back().Operation);
                        if(operation)
                        {
                            static_cast<void>(
                                abort_preparation(*context, *operation));
                        }
                    }
                }
                return [](implementation&) {};
            },
            [](implementation& host)
            {
                host.publish_diagnostic(
                    core::epoch_reason_code::unavailable);
            },
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::move(actor));
    }

    void publish_diagnostic(const core::epoch_reason_code reason) noexcept
    {
        auto update = epoch_presentation_update{};
        update.Transition = epoch_transition_projection{
            .Changed = core::epoch_changed_domain::diagnostics,
            .Diagnostics = {core::epoch_diagnostic{.Code = reason}},
        };
        publish(update);
    }

    void publish(const epoch_presentation_update& update) noexcept
    {
        try
        {
            Projections.publish(update);
        }
        catch(...)
        {
            ProjectionDeliveryFailed = true;
        }
    }

    void publish_transition(const core::epoch_transition_output& output)
    {
        if(output.Changed == core::epoch_changed_domain::none
           && output.Diagnostics.empty())
        {
            return;
        }
        epoch_presentation_update update;
        epoch_transition_projection transition{
            .Changed = output.Changed,
            .Selection = output.Selection,
            .Status = output.Status,
            .Commands = output.Commands,
            .Diagnostics = output.Diagnostics,
        };
        if(output.Authority)
        {
            transition.Authority = epoch_authority_projection{
                .Authority = output.Authority->Authority,
                .Generation = output.Authority->Generation,
            };
            if(output.Authority->Revision)
            {
                transition.Authority->Revision =
                    projected_revision(*output.Authority->Revision);
            }
        }
        if(output.Comparison)
        {
            auto reason = core::epoch_reason_code::none;
            if(!output.Comparison->Pair && !output.Diagnostics.empty())
            {
                reason = output.Diagnostics.back().Code;
            }
            transition.Comparison = epoch_comparison_projection{
                .Selection = output.Comparison->Selection,
                .Pair = output.Comparison->Pair,
                .Generation = State.ComparisonGeneration,
                .Revision = output.Comparison->Revision,
                .AvailableForDiff = output.Comparison->AvailableForDiff,
                .Reason = reason,
            };
        }
        update.Transition = std::move(transition);

        if(output.Catalog || output.AcceptedEpoch || output.History)
        {
            update.Catalog = epoch_catalog_projection{};
            if(output.Catalog)
            {
                update.Catalog->Catalog = output.Catalog->Catalog;
                update.Catalog->Replace =
                    !output.Catalog->Catalog.has_value();
                update.Catalog->AvailabilityChangedEpochs =
                    output.Catalog->AvailabilityChangedEpochs;
                update.Catalog->AvailabilityChangedTo =
                    output.Catalog->AvailabilityChangedTo;
                update.Catalog->AllEpochAvailability =
                    output.Catalog->AllEpochAvailability;
            }
            if(output.AcceptedEpoch)
            {
                update.Catalog->Catalog = core::epoch_catalog_identity{
                    .Incarnation = output.AcceptedEpoch->Epoch.Id.Incarnation,
                    .Revision = output.AcceptedEpoch->CatalogRevision,
                };
                update.Catalog->Changes.push_back(
                    epoch_row_change_projection{
                        .Kind = epoch_row_change_kind::insert,
                        .Id = output.AcceptedEpoch->Epoch.Id,
                        .Row = projected_row(output.AcceptedEpoch->Epoch),
                    });
            }
            if(output.History)
            {
                if(!output.Catalog)
                {
                    update.Catalog->Catalog = output.History->Catalog;
                }
                auto page = epoch_history_page_projection{
                    .Phase = output.History->Phase,
                    .Catalog = output.History->Catalog,
                    .RequestedCursor = output.History->RequestedCursor,
                    .Limit = output.History->Limit,
                    .Continuation = output.History->Continuation,
                    .Reason = output.History->Reason,
                };
                page.Rows.reserve(output.History->Epochs.size());
                for(const auto& epoch : output.History->Epochs)
                {
                    page.Rows.push_back(projected_row(epoch));
                }
                update.Catalog->HistoryPage = std::move(page);
            }
        }
        publish(update);
    }

    auto dispatch_core(
        const core::epoch_event& event,
        const std::function<void(const core::epoch_transition_output&)>&
            before_requests = {})
        -> core::epoch_transition_output
    {
        auto output = core::transition(State, event);
        publish_transition(output);
        if(before_requests)
        {
            before_requests(output);
        }
        for(const auto& request : output.Requests)
        {
            handle_request(request);
        }
        return output;
    }

    void handle_request(const core::epoch_host_request& request)
    {
        std::visit(
            [this](const auto& value)
            {
                using request_type = std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<
                                 request_type,
                                 core::invalidate_epoch_authority_request>)
                {
                    cancel_authority_jobs();
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      core::cancel_epoch_work_request>)
                {
                    cancel_work(value);
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      core::load_epoch_catalog_request>)
                {
                    start_catalog_load(value);
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      core::prepare_epoch_revision_request>)
                {
                    start_revision_preparation(value);
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      core::commit_epoch_catalog_request>)
                {
                    start_catalog_commit(value);
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      core::discard_epoch_preparation_request>)
                {
                    discard_preparation(value);
                }
                else if constexpr(std::is_same_v<
                                      request_type,
                                      core::load_epoch_history_page_request>)
                {
                    start_history_page(value);
                }
                else if constexpr(std::is_same_v<request_type,
                                      core::perform_epoch_maintenance_request>)
                {
                    start_epoch_maintenance(value);
                }
                else
                {
                    start_comparison_resolution(value);
                }
            },
            request);
    }

    [[nodiscard]] auto exact_context(
        const core::pull_request_authority& authority,
        const core::accepted_pull_request_revision& revision) const
        -> std::shared_ptr<authority_context>
    {
        if(Active && Active->Authority == authority
           && Active->Revision == revision)
        {
            return Active;
        }
        return {};
    }

    [[nodiscard]] auto actor_for(
        const accepted_pull_request_handoff& handoff)
        -> std::shared_ptr<authority_effect_actor>
    {
        const auto key = actor_key(handoff);
        const auto found = std::ranges::find(
            CacheActors, key, &decltype(CacheActors)::value_type::first);
        if(found != CacheActors.end())
        {
            return found->second;
        }
        if(CacheActors.size() >= maximum_cache_actors)
        {
            return {};
        }
        auto actor = std::make_shared<authority_effect_actor>();
        CacheActors.emplace_back(key, actor);
        return actor;
    }

    void accept(accepted_pull_request_handoff handoff)
    {
        if(Closing || !application::valid(handoff)
           || handoff.ApplicationSession != State.ApplicationSession)
        {
            return;
        }
        auto accepted =
            std::make_shared<accepted_pull_request_handoff>(
                std::move(handoff));
        if(TurnActive || DeferredIngressScheduled || !DeferredIngress.empty())
        {
            DeferredIngress.emplace_back(deferred_accept{std::move(accepted)});
            schedule_deferred_ingress();
            return;
        }
        run_turn([this, &accepted]
        {
            accept_now(std::move(*accepted));
        });
    }

    void accept_now(accepted_pull_request_handoff handoff)
    {
        if(Closing)
        {
            return;
        }
        ++OfferToken;
        if(PendingOfferToken)
        {
            cancel_offer_job(*PendingOfferToken);
        }
        auto actor = actor_for(handoff);
        if(!actor)
        {
            PendingAuthority.reset();
            PendingOfferToken.reset();
            if(handoff.SmartGit)
            {
                handoff.SmartGit->revoke();
            }
            publish_diagnostic(core::epoch_reason_code::quota_exceeded);
            return;
        }
        const bool preserves_active_authority =
            Active && agrees(Active->Authority, handoff);
        if(Active)
        {
            cancel_authority_jobs();
        }
        if(Active && !preserves_active_authority)
        {
            auto retired = std::move(Active);
            if(retired->Handoff.SmartGit)
            {
                retired->Handoff.SmartGit->revoke();
            }
            dispatch_core(core::pull_request_revision_invalidated_event{
                .ApplicationSession = State.ApplicationSession,
                .Authority = retired->Authority,
                .Revision = retired->Revision,
                .Generation = State.EpochGeneration,
            });
            discard_context_preparation(std::move(retired));
        }

        const auto token = OfferToken;
        auto offered = std::make_shared<accepted_pull_request_handoff>(
            std::move(handoff));
        PendingAuthority = invalidation_identity(*offered);
        PendingOfferToken = token;
        auto* const catalogs = &Catalogs;
        auto* const caches = &Caches;
        auto* const entropy = &Entropy;
        const auto submitted = submit_effect(
            [offered, actor, catalogs, caches, entropy, token](
                const std::stop_token stop)
                -> worker_completion
            {
                struct lease_guard final
                {
                    std::shared_ptr<accepted_pull_request_handoff> Offered;

                    ~lease_guard()
                    {
                        if(Offered && Offered->SmartGit)
                        {
                            Offered->SmartGit->revoke();
                        }
                    }
                } lease{offered};
                const auto diagnostic = [token](
                                            const core::epoch_reason_code reason)
                    -> worker_completion
                {
                    return [token, reason](implementation& host)
                    {
                        if(token != host.OfferToken)
                        {
                            return;
                        }
                        host.PendingAuthority.reset();
                        host.PendingOfferToken.reset();
                        host.publish_diagnostic(reason);
                    };
                };
                if(stop.stop_requested())
                {
                    return [](implementation&) {};
                }
                auto opened = catalogs->open(
                    epoch_catalog_open_request{
                        .ApplicationSession = offered->ApplicationSession,
                        .Authority = catalog_key(*offered),
                    },
                    stop);
                if(!opened)
                {
                    const auto reason = core::reason_for(
                        catalog_failure(opened.error()));
                    return diagnostic(reason);
                }
                if(!*opened)
                {
                    return diagnostic(
                        core::epoch_reason_code::repair_required);
                }

                const auto initial_binding = (*opened)->binding();
                if(!application::valid(initial_binding)
                   || initial_binding.ObjectFormat != offered->ObjectFormat
                   || !agrees(initial_binding.Authority, *offered))
                {
                    return diagnostic(core::epoch_reason_code::invalid_identity);
                }

                const auto expected_cache = git_cache_open_request{
                    .ApplicationSession = offered->ApplicationSession,
                    .Authority = initial_binding.Authority,
                    .ObjectFormat = offered->ObjectFormat,
                };
                if(actor->Cache && actor->Cache->request() != expected_cache)
                {
                    actor->Cache.reset();
                }
                const auto recovered = recover_catalog(
                    **opened,
                    *caches,
                    *entropy,
                    *offered,
                    actor->Cache,
                    stop);
                if(!recovered)
                {
                    return diagnostic(core::reason_for(recovered.error()));
                }

                std::optional<core::epoch_source_observation_id> source;
                for(std::size_t attempt = 0U;
                    attempt < maximum_identity_attempts;
                    ++attempt)
                {
                    source = mint_identity<core::epoch_source_observation_id>(
                        *entropy);
                    if(!source)
                    {
                        break;
                    }
                    const auto exists =
                        (*opened)->source_observation_exists(*source, stop);
                    if(!exists)
                    {
                        const auto reason = core::reason_for(
                            catalog_failure(exists.error()));
                        return diagnostic(reason);
                    }
                    if(!*exists)
                    {
                        break;
                    }
                    source.reset();
                }
                if(!source || stop.stop_requested())
                {
                    return diagnostic(core::epoch_reason_code::unavailable);
                }

                const auto binding = (*opened)->binding();
                if(!application::valid(binding)
                   || binding.ObjectFormat != offered->ObjectFormat
                   || !agrees(binding.Authority, *offered))
                {
                    return diagnostic(core::epoch_reason_code::invalid_identity);
                }
                auto revision = accepted_revision(*offered, *source);
                auto context = std::make_shared<authority_context>(
                    std::move(*offered),
                    binding.Authority,
                    revision,
                    std::move(*opened),
                    std::move(actor));
                return [context = std::move(context), token](implementation& host)
                {
                    if(host.Closing || token != host.OfferToken)
                    {
                        if(context->Handoff.SmartGit)
                        {
                            context->Handoff.SmartGit->revoke();
                        }
                        return;
                    }
                    host.PendingAuthority.reset();
                    host.PendingOfferToken.reset();
                    auto previous = host.Active;
                    host.Active = context;
                    bool accepted = false;
                    static_cast<void>(host.dispatch_core(
                        core::pull_request_revision_accepted_event{
                        .ApplicationSession = context->Handoff.ApplicationSession,
                        .Authority = context->Authority,
                        .Revision = context->Revision,
                    },
                        [&](const core::epoch_transition_output&)
                        {
                            accepted = host.State.Authority
                                && host.State.AcceptedRevision
                                && *host.State.Authority
                                    == context->Authority
                                && *host.State.AcceptedRevision
                                    == context->Revision;
                            if(accepted && previous)
                            {
                                if(previous->Handoff.SmartGit)
                                {
                                    previous->Handoff.SmartGit->revoke();
                                }
                                host.discard_context_preparation(
                                    std::move(previous));
                            }
                        }));
                    if(!accepted)
                    {
                        const auto previous_still_owned = previous
                            && host.State.Authority
                            && host.State.AcceptedRevision
                            && *host.State.Authority == previous->Authority
                            && *host.State.AcceptedRevision
                                == previous->Revision;
                        host.Active = previous_still_owned
                            ? std::move(previous)
                            : std::shared_ptr<authority_context>{};
                        if(context->Handoff.SmartGit)
                        {
                            context->Handoff.SmartGit->revoke();
                        }
                        if(previous && !previous_still_owned)
                        {
                            if(previous->Handoff.SmartGit)
                            {
                                previous->Handoff.SmartGit->revoke();
                            }
                            host.discard_context_preparation(
                                std::move(previous));
                        }
                        return;
                    }
                };
            },
            [token](implementation& host)
            {
                if(token == host.OfferToken)
                {
                    host.PendingAuthority.reset();
                    host.PendingOfferToken.reset();
                    host.publish_diagnostic(
                        core::epoch_reason_code::unavailable);
                }
            },
            std::nullopt,
            std::nullopt,
            std::nullopt,
            token,
            std::move(actor));
        if(!submitted && token == OfferToken)
        {
            PendingAuthority.reset();
            PendingOfferToken.reset();
            if(offered->SmartGit)
            {
                offered->SmartGit->revoke();
            }
        }
    }

    void invalidate(const pull_request_authority_invalidation invalidation)
    {
        if(Closing || !application::valid(invalidation))
        {
            return;
        }
        if(TurnActive || DeferredIngressScheduled || !DeferredIngress.empty())
        {
            DeferredIngress.emplace_back(deferred_invalidate{invalidation});
            schedule_deferred_ingress();
            return;
        }
        run_turn([this, &invalidation]
        {
            invalidate_now(invalidation);
        });
    }

    void invalidate_now(
        const pull_request_authority_invalidation& invalidation)
    {
        if(PendingAuthority == std::optional{invalidation})
        {
            ++OfferToken;
            if(PendingOfferToken)
            {
                cancel_offer_job(*PendingOfferToken);
            }
            PendingAuthority.reset();
            PendingOfferToken.reset();
        }
        if(!Active
           || invalidation.ApplicationSession
               != Active->Handoff.ApplicationSession
           || invalidation.Profile != Active->Authority.Profile
           || invalidation.ProfileRevision
               != Active->Authority.ProfileRevision
           || invalidation.Host != Active->Authority.Host
           || invalidation.HostAuthority
               != Active->Authority.HostAuthority
           || invalidation.Account != Active->Authority.Account
           || invalidation.Viewer != Active->Authority.Viewer
           || invalidation.Repository != Active->Authority.Repository
           || invalidation.PullRequest != Active->Authority.PullRequest
           || invalidation.IdentityGeneration
               != Active->Revision.Source.IdentityGeneration)
        {
            return;
        }
        cancel_authority_jobs();
        if(Active->Handoff.SmartGit)
        {
            Active->Handoff.SmartGit->revoke();
        }
        dispatch_core(core::pull_request_revision_invalidated_event{
            .ApplicationSession = invalidation.ApplicationSession,
            .Authority = Active->Authority,
            .Revision = Active->Revision,
            .Generation = State.EpochGeneration,
        });
        Active.reset();
    }

    void dispatch(const epoch_intent intent)
    {
        if(Closing)
        {
            return;
        }
        if(TurnActive || DeferredIngressScheduled || !DeferredIngress.empty())
        {
            DeferredIngress.emplace_back(deferred_dispatch{intent});
            schedule_deferred_ingress();
            return;
        }
        run_turn([this, &intent]
        {
            dispatch_now(intent);
        });
    }

    void dispatch_now(const epoch_intent& intent)
    {
        if(Closing)
        {
            return;
        }
        std::visit(
            [this](const auto& value)
            {
                dispatch_core(core::epoch_event{value});
            },
            intent);
    }

    void shutdown()
    {
        if(TurnActive)
        {
            DeferredIngress.emplace_back(deferred_shutdown{});
            return;
        }
        run_turn([this]
        {
            shutdown_now();
        });
    }

    void shutdown_now()
    {
        if(Closing)
        {
            return;
        }
        Closing = true;
        ++OfferToken;
        cancel_all_jobs();
        PendingAuthority.reset();
        PendingOfferToken.reset();
        if(Active)
        {
            if(Active->Handoff.SmartGit)
            {
                Active->Handoff.SmartGit->revoke();
            }
            dispatch_core(core::pull_request_revision_invalidated_event{
                .ApplicationSession = State.ApplicationSession,
                .Authority = Active->Authority,
                .Revision = Active->Revision,
                .Generation = State.EpochGeneration,
            });
            Active.reset();
        }
    }

    void start_catalog_load(const core::load_epoch_catalog_request& request)
    {
        auto context = exact_context(
            request.Context.Authority, request.Context.Revision);
        if(!context)
        {
            return;
        }
        auto actor = context->Actor;
        submit_effect(
            [context, request](const std::stop_token stop)
                -> worker_completion
            {
                const auto snapshot = context->Catalog->snapshot(stop);
                if(!snapshot)
                {
                    if(snapshot.error() == epoch_catalog_failure::cancelled)
                    {
                        return [request](implementation& host)
                        {
                            host.dispatch_core(
                                core::epoch_catalog_load_cancelled_event{
                                    .Context = request.Context,
                                });
                        };
                    }
                    const auto failure = catalog_failure(snapshot.error());
                    return [request, failure](implementation& host)
                    {
                        host.dispatch_core(core::epoch_catalog_load_failed_event{
                            .Context = request.Context,
                            .Failure = failure,
                        });
                        };
                }
                if(
                    !application::valid(snapshot->Binding)
                    || snapshot->Binding != context->Catalog->binding())
                {
                    return [request](implementation& host)
                    {
                        host.dispatch_core(core::epoch_catalog_load_failed_event{
                            .Context = request.Context,
                            .Failure = core::epoch_failure::repair_required,
                        });
                    };
                }
                const auto catalog = snapshot->Binding.Catalog;
                return [request, catalog](implementation& host)
                {
                    host.dispatch_core(core::epoch_catalog_loaded_event{
                        .Context = request.Context,
                        .Catalog = catalog,
                    });
                };
            },
            [request](implementation& host)
            {
                host.dispatch_core(core::epoch_catalog_load_failed_event{
                    .Context = request.Context,
                    .Failure = core::epoch_failure::unavailable,
                });
            },
            request.Context.Request,
            request.Context.Generation,
            std::nullopt,
            std::nullopt,
            std::move(actor));
    }

    void start_revision_preparation(
        const core::prepare_epoch_revision_request& request)
    {
        auto context = exact_context(
            request.Context.Authority, request.Context.Revision);
        if(!context)
        {
            return;
        }
        auto* const caches = &Caches;
        auto* const entropy = &Entropy;
        auto* const clock = &Clock;
        auto actor = context->Actor;
        submit_effect(
            [context, request, caches, entropy, clock](
                const std::stop_token stop) -> worker_completion
            {
                const auto cancelled = [request]() -> worker_completion
                {
                    return [request](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_revision_prepare_cancelled_event{
                                .Context = request.Context,
                            });
                    };
                };
                const auto failed = [request](const core::epoch_failure failure)
                    -> worker_completion
                {
                    return [request, failure](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_revision_prepare_failed_event{
                                .Context = request.Context,
                                .Failure = failure,
                        });
                    };
                };
                if(stop.stop_requested())
                {
                    return cancelled();
                }
                const auto snapshot = context->Catalog->snapshot(stop);
                if(!snapshot)
                {
                    return snapshot.error() == epoch_catalog_failure::cancelled
                        ? cancelled()
                        : failed(catalog_failure(snapshot.error()));
                }
                if(
                    !application::valid(snapshot->Binding)
                    || snapshot->Binding != context->Catalog->binding())
                {
                    return failed(core::epoch_failure::repair_required);
                }
                const auto expected_catalog = core::epoch_catalog_identity{
                    .Incarnation = request.Context.CatalogIncarnation,
                    .Revision = request.Context.ExpectedCatalogRevision,
                };
                if(snapshot->Binding.Catalog != expected_catalog)
                {
                    return failed(core::epoch_failure::catalog_conflict);
                }

                const auto effect = core::plan_epoch_preparation(
                    context->Authority,
                    request.Context.Revision,
                    expected_catalog,
                    core::epoch_preparation_catalog_observation{
                        .LatestLive = snapshot->LatestLive,
                        .RetentionRehydration =
                            snapshot->RetentionRehydration,
                    });
                if(effect.Rejection != core::epoch_reason_code::none)
                {
                    return failed(core::epoch_failure::malformed_observation);
                }

                const auto canonical_review =
                    [&](const std::optional<projected_append_head> projected)
                    -> std::expected<
                        std::optional<core::catalog_review_association>,
                        core::epoch_failure>
                {
                    if(!request.Context.Revision.CurrentViewerReview)
                    {
                        return std::nullopt;
                    }
                    for(std::size_t attempt = 0U;
                        attempt < maximum_identity_attempts;
                        ++attempt)
                    {
                        const auto association =
                            mint_identity<core::review_association_id>(
                                *entropy);
                        if(!association)
                        {
                            return std::unexpected{
                                core::epoch_failure::unavailable};
                        }
                        const auto review = context->Catalog->canonical_review(
                            canonical_review_request{
                                .Catalog = expected_catalog,
                                .Candidate = *request.Context.Revision.
                                    CurrentViewerReview,
                                .ProposedAssociation = *association,
                                .ProjectedHead = projected,
                            },
                            stop);
                        if(review)
                        {
                            return std::optional{*review};
                        }
                        if(review.error()
                           != epoch_catalog_failure::
                               operation_identity_conflict)
                        {
                            return std::unexpected{
                                catalog_failure(review.error())};
                        }
                    }
                    return std::unexpected{
                        core::epoch_failure::catalog_conflict};
                };

                const auto observed_time = [&]()
                    -> std::optional<core::observed_utc_milliseconds>
                {
                    const auto observed = clock->observe_utc();
                    return observed && core::valid(*observed)
                        ? observed
                        : std::nullopt;
                };

                if(effect.Effect == core::epoch_preparation_effect::none)
                {
                    const auto operation =
                        mint_identity<core::epoch_operation_id>(*entropy);
                    const auto review = canonical_review(std::nullopt);
                    const auto observed = observed_time();
                    if(!operation || !review || !observed)
                    {
                        return failed(
                            review ? core::epoch_failure::unavailable
                                   : review.error());
                    }
                    const auto prepared = prepared_operation{
                        .Kind = prepared_kind::observation_only,
                        .Operation = *operation,
                    };
                    if(!context->store(prepared))
                    {
                        return failed(core::epoch_failure::unavailable);
                    }
                    const auto event = core::epoch_revision_prepared_event{
                        .Context = request.Context,
                        .Operation = *operation,
                        .Observation =
                            {
                                .LatestLive = snapshot->LatestLive,
                                .CatalogCurrentViewerReview =
                                    snapshot->CurrentViewerReview,
                                .ProposedCurrentViewerReview = *review,
                                .ObservedAt = *observed,
                                .HeadLineage =
                                    core::head_lineage::not_applicable,
                                .RetentionRehydration =
                                    snapshot->RetentionRehydration,
                            },
                    };
                    return [context, event](implementation& host)
                    {
                        const auto output = host.dispatch_core(event);
                        const auto accepted = std::ranges::any_of(
                            output.Requests,
                            [](const auto& next)
                            {
                                return std::holds_alternative<
                                           core::commit_epoch_catalog_request>(
                                           next)
                                    || std::holds_alternative<
                                        core::discard_epoch_preparation_request>(
                                        next);
                            });
                        if(!accepted)
                        {
                            host.discard_context_preparation(
                                context, event.Operation);
                        }
                    };
                }

                if(!snapshot->NextOrdinal)
                {
                    return failed(core::epoch_failure::ordinal_exhausted);
                }

                auto prepared = prepared_operation{
                    .Kind = prepared_kind::append,
                };
                for(std::size_t attempt = 0U;
                    attempt < maximum_identity_attempts;
                    ++attempt)
                {
                    const auto operation =
                        mint_identity<core::epoch_operation_id>(*entropy);
                    if(!operation)
                    {
                        return failed(core::epoch_failure::unavailable);
                    }
                    const auto reservation = append_reservation_request{
                            .Context =
                                {
                                    .ApplicationSession =
                                        request.Context.ApplicationSession,
                                    .Request = request.Context.Request,
                                    .Generation = request.Context.Generation,
                                    .Operation = *operation,
                                    .Revision = request.Context.Revision,
                                    .Catalog = expected_catalog,
                                },
                            .ProposedOrdinal = *snapshot->NextOrdinal,
                        };
                    const auto reserved = context->Catalog->reserve_append(
                        reservation, stop);
                    if(reserved)
                    {
                        if(!exact_append_reservation(
                               *reserved,
                               reservation,
                               context->Authority.Scope))
                        {
                            return failed(
                                core::epoch_failure::repair_required);
                        }
                        prepared.Operation = *operation;
                        prepared.Record = *reserved;
                        break;
                    }
                    if(reserved.error()
                       == epoch_catalog_failure::operation_identity_conflict)
                    {
                        continue;
                    }
                    if(reserved.error()
                       == epoch_catalog_failure::source_observation_conflict)
                    {
                        std::optional<core::epoch_source_observation_id>
                            replacement;
                        for(std::size_t source_attempt = 0U;
                            source_attempt < maximum_identity_attempts;
                            ++source_attempt)
                        {
                            replacement = mint_identity<
                                core::epoch_source_observation_id>(*entropy);
                            if(!replacement)
                            {
                                break;
                            }
                            const auto exists = context->Catalog->
                                source_observation_exists(*replacement, stop);
                            if(!exists)
                            {
                                return failed(catalog_failure(exists.error()));
                            }
                            if(!*exists)
                            {
                                break;
                            }
                            replacement.reset();
                        }
                        if(!replacement)
                        {
                            return failed(core::epoch_failure::unavailable);
                        }
                        auto next_revision = context->Revision;
                        next_revision.Source.Observation = *replacement;
                        return [context,
                                prior = request.Context.Revision,
                                next_revision](implementation& host)
                        {
                            if(host.Active != context
                               || host.State.AcceptedRevision
                                      != std::optional{prior})
                            {
                                return;
                            }
                            context->Revision = next_revision;
                            host.dispatch_core(
                                core::pull_request_revision_accepted_event{
                                    .ApplicationSession =
                                        context->Handoff.ApplicationSession,
                                    .Authority = context->Authority,
                                    .Revision = next_revision,
                                });
                            if(host.State.AcceptedRevision
                               != std::optional{next_revision})
                            {
                                context->Revision = prior;
                            }
                        };
                    }
                    return reserved.error()
                            == epoch_catalog_failure::cancelled
                        ? cancelled()
                        : failed(catalog_failure(reserved.error()));
                }
                if(!prepared.Record)
                {
                    return failed(core::epoch_failure::catalog_conflict);
                }

                const auto fail_prepared =
                    [&](const core::epoch_failure failure)
                    -> worker_completion
                {
                    const auto safe = abort_preparation(*context, prepared);
                    return failed(
                        safe ? failure : core::epoch_failure::repair_required);
                };
                const auto cancel_prepared = [&]() -> worker_completion
                {
                    const auto safe = abort_preparation(*context, prepared);
                    return safe
                        ? cancelled()
                        : failed(core::epoch_failure::repair_required);
                };

                const auto expected_cache = git_cache_open_request{
                    .ApplicationSession =
                        request.Context.ApplicationSession,
                    .Authority = context->Authority,
                    .ObjectFormat = context->Handoff.ObjectFormat,
                };
                const auto cache = open_actor_cache(
                    *context->Actor, *caches, expected_cache, stop);
                if(!cache)
                {
                    return cache.error() == git_cache_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(cache_failure(cache.error()));
                }
                if(!context->Handoff.SmartGit
                   || context->Handoff.SmartGit->revoked())
                {
                    return fail_prepared(core::epoch_failure::unavailable);
                }
                auto sink = context->Actor->Cache->create_pack_sink(
                    prepared.Operation,
                    context->Actor->Cache->request().Limits.OperationPackBytes,
                    stop);
                if(!sink)
                {
                    return sink.error() == git_cache_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(cache_failure(sink.error()));
                }
                if(!*sink)
                {
                    return fail_prepared(
                        core::epoch_failure::repair_required);
                }
                const auto smart_context = git_smart_request_context{
                    .ApplicationSession = request.Context.ApplicationSession,
                    .Authority = context->Authority,
                    .Revision = request.Context.Revision,
                    .Repository = context->Handoff.Locator,
                    .BaseRef = context->Handoff.BaseRef,
                };
                auto transport =
                    context->Handoff.SmartGit->acquire(smart_context);
                if(!transport)
                {
                    (*sink)->abort();
                    return stop.stop_requested()
                            || transport.error().Failure
                                == git_smart_transport_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(core::epoch_failure::unavailable);
                }
                if(!*transport)
                {
                    (*sink)->abort();
                    return fail_prepared(
                        core::epoch_failure::repair_required);
                }
                std::vector<git_protocol_v2_have> haves;
                if(snapshot->LatestLive)
                {
                    const auto append_have = [&](
                                                 const core::git_commit_oid oid,
                                                 const core::anchor_availability
                                                     availability)
                    {
                        if(availability != core::anchor_availability::available
                           || std::ranges::any_of(
                               haves,
                               [&](const auto& have)
                               {
                                   return have.Commit == oid;
                               }))
                        {
                            return;
                        }
                        const auto inspected =
                            context->Actor->Cache->inspect_commit_object(
                                oid, stop);
                        if(
                            !inspected || !application::valid(*inspected)
                            || inspected->Commit != oid)
                        {
                            return;
                        }
                        haves.push_back(git_protocol_v2_have{
                            .Cache = context->Authority.Cache,
                            .Commit = oid,
                            .CatalogVisible = true,
                            .ObjectVerified = true,
                        });
                    };
                    append_have(
                        snapshot->LatestLive->Base,
                        snapshot->LatestLive->BaseAvailability);
                    append_have(
                        snapshot->LatestLive->Head,
                        snapshot->LatestLive->HeadAvailability);
                }
                const auto acquisition = acquire_git_protocol_v2_pack(
                    **transport,
                    smart_context,
                    haves,
                    **sink,
                    git_protocol_v2_fetch_limits{
                        .MaximumWireBytes =
                            maximum_git_smart_fetch_response_bytes,
                        .MaximumPackBytes = context->Actor->Cache->request().Limits.
                            OperationPackBytes,
                        .MaximumProgressBytes =
                            maximum_git_protocol_v2_progress_bytes,
                        .MaximumPackets =
                            maximum_git_protocol_v2_fetch_packets,
                    },
                    stop);
                if(!acquisition)
                {
                    (*transport)->revoke();
                    const auto was_cancelled = stop.stop_requested()
                        || (std::holds_alternative<git_smart_transport_error>(
                                acquisition.error())
                            && std::get<git_smart_transport_error>(
                                   acquisition.error())
                                       .Failure
                                == git_smart_transport_failure::cancelled);
                    return was_cancelled
                        ? cancel_prepared()
                        : fail_prepared(core::epoch_failure::unavailable);
                }
                const auto inventory = context->Actor->Cache->observe_preimport(
                    acquisition->Fetch.Pack, stop);
                if(!inventory)
                {
                    return inventory.error() == git_cache_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(cache_failure(inventory.error()));
                }
                const auto spool_request = record_spool_request{
                        .Catalog = expected_catalog,
                        .Operation = prepared.Operation,
                        .ExpectedOperationRevision =
                            prepared.Record->OperationRevision,
                        .Spool = durable_fact(acquisition->Fetch.Pack),
                        .Preimport = durable_fact(*inventory),
                    };
                auto record = context->Catalog->record_spool(
                    spool_request, stop);
                if(!record)
                {
                    return record.error() == epoch_catalog_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(catalog_failure(record.error()));
                }
                if(!exact_spool_record(
                       *prepared.Record, *record, spool_request))
                {
                    return fail_prepared(
                        core::epoch_failure::repair_required);
                }
                prepared.Record = *record;

                const auto durable_record_token = entropy->generate();
                if(!durable_record_token
                   || !application::valid(*durable_record_token))
                {
                    return fail_prepared(core::epoch_failure::unavailable);
                }
                std::optional<core::git_commit_oid> prior_head;
                if(snapshot->LatestLive)
                {
                    prior_head = snapshot->LatestLive->Head;
                }
                const auto git_request = git_prepare_revision_request{
                        .Receipt = acquisition->Fetch.Pack,
                        .Inventory = *inventory,
                        .Authorization =
                            {
                                .CatalogIncarnation =
                                    expected_catalog.Incarnation,
                                .ExpectedCatalogRevision =
                                    expected_catalog.Revision,
                                .Operation = prepared.Operation,
                                .ExpectedPack =
                                    acquisition->Fetch.Pack.ExpectedPack,
                                .ReceiptToken =
                                    acquisition->Fetch.Pack.OpaqueToken,
                                .InventoryToken = inventory->OpaqueToken,
                                .DurableRecordToken = *durable_record_token,
                            },
                        .Base = request.Context.Revision.Base,
                        .Head = request.Context.Revision.Head,
                        .PriorHead = prior_head,
                    };
                auto git = context->Actor->Cache->prepare_revision(
                    git_request, stop);
                if(!git)
                {
                    return git.error() == git_cache_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(cache_failure(git.error()));
                }
                if(!exact_prepared_revision(*git, git_request))
                {
                    return fail_prepared(
                        core::epoch_failure::repair_required);
                }
                if(
                    snapshot->LatestLive
                    && snapshot->LatestLive->Head
                        != request.Context.Revision.Head
                    && snapshot->LatestLive->HeadAvailability
                        != core::anchor_availability::available)
                {
                    git->HeadLineage = core::head_lineage::unknown;
                }
                prepared.Git = *git;
                const auto install_request = record_install_request{
                        .Catalog = expected_catalog,
                        .Operation = prepared.Operation,
                        .ExpectedOperationRevision =
                            prepared.Record->OperationRevision,
                        .Install = durable_fact(*git),
                    };
                record = context->Catalog->record_install(
                    install_request, stop);
                if(!record)
                {
                    return record.error() == epoch_catalog_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(catalog_failure(record.error()));
                }
                if(!exact_install_record(
                       *prepared.Record, *record, install_request))
                {
                    return fail_prepared(
                        core::epoch_failure::repair_required);
                }
                prepared.Record = *record;
                const auto staging = advance_append(
                    *context,
                    *prepared.Record,
                    expected_catalog,
                    epoch_catalog_operation_phase::staging_ready,
                    stop);
                if(!staging)
                {
                    return staging.error()
                            == epoch_catalog_failure::cancelled
                        ? cancel_prepared()
                        : fail_prepared(catalog_failure(staging.error()));
                }
                prepared.Record = *staging;

                const auto review = canonical_review(projected_append_head{
                    .Ordinal = prepared.Record->ReservedOrdinal,
                    .Head = request.Context.Revision.Head,
                });
                const auto observed = observed_time();
                if(!review || !observed)
                {
                    return fail_prepared(
                        review ? core::epoch_failure::unavailable
                               : review.error());
                }
                if(!context->store(prepared))
                {
                    const auto safe = abort_preparation(*context, prepared);
                    return failed(
                        safe ? core::epoch_failure::unavailable
                             : core::epoch_failure::repair_required);
                }
                const auto event = core::epoch_revision_prepared_event{
                    .Context = request.Context,
                    .Operation = prepared.Operation,
                    .Observation =
                        {
                            .LatestLive = snapshot->LatestLive,
                            .CatalogCurrentViewerReview =
                                snapshot->CurrentViewerReview,
                            .ProposedCurrentViewerReview = *review,
                            .ProposedOrdinal =
                                prepared.Record->ReservedOrdinal,
                            .ObservedAt = *observed,
                            .HeadLineage = git->HeadLineage,
                            .Material =
                                core::epoch_material_identity{
                                    .BaseTree = git->BaseTree,
                                    .HeadTree = git->HeadTree,
                                    .PolicyRevision =
                                        core::comparison_policy_revision{1U},
                                },
                            .RetentionRehydration =
                                snapshot->RetentionRehydration,
                        },
                };
                return [context, event](implementation& host)
                {
                    const auto output = host.dispatch_core(event);
                    const auto accepted = std::ranges::any_of(
                        output.Requests,
                        [](const auto& next)
                        {
                            return std::holds_alternative<
                                       core::commit_epoch_catalog_request>(next)
                                || std::holds_alternative<
                                    core::discard_epoch_preparation_request>(
                                    next);
                        });
                    if(!accepted)
                    {
                        host.discard_context_preparation(
                            context, event.Operation);
                    }
                };
            },
            [request](implementation& host)
            {
                host.dispatch_core(
                    core::epoch_revision_prepare_failed_event{
                        .Context = request.Context,
                        .Failure = core::epoch_failure::unavailable,
                    });
            },
            request.Context.Request,
            request.Context.Generation,
            std::nullopt,
            std::nullopt,
            std::move(actor));
    }

    void start_catalog_commit(const core::commit_epoch_catalog_request& request)
    {
        auto context = exact_context(
            request.Context.Authority, request.Context.Revision);
        if(!context)
        {
            return;
        }
        auto* const entropy = &Entropy;
        auto* const caches = &Caches;
        auto actor = context->Actor;
        submit_effect(
            [context, request, entropy, caches](const std::stop_token stop)
                -> worker_completion
            {
                const auto cancelled = [request](const core::epoch_commit_progress
                                                     progress)
                    -> worker_completion
                {
                    return [request, progress](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_catalog_commit_cancelled_event{
                                .Context = request.Context,
                                .Mutation = request.Mutation,
                                .Progress = progress,
                            });
                    };
                };
                const auto failed = [request](const core::epoch_failure failure)
                    -> worker_completion
                {
                    return [request, failure](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_catalog_commit_failed_event{
                                .Context = request.Context,
                                .Mutation = request.Mutation,
                                .Failure = failure,
                            });
                    };
                };
                const auto succeeded = [request](
                                           const core::epoch_catalog_revision
                                               revision)
                    -> worker_completion
                {
                    return [request, revision](implementation& host)
                    {
                        host.dispatch_core(core::epoch_catalog_committed_event{
                            .Context = request.Context,
                            .Mutation = request.Mutation,
                            .NewRevision = revision,
                        });
                    };
                };

                auto claimed = context->take(request.Context.Operation);
                if(!claimed)
                {
                    return failed(core::epoch_failure::catalog_conflict);
                }
                auto operation = std::move(*claimed);
                const auto expected_catalog = core::epoch_catalog_identity{
                    .Incarnation = request.Context.CatalogIncarnation,
                    .Revision = request.Context.ExpectedCatalogRevision,
                };
                const auto abort_and_fail =
                    [&](const core::epoch_failure failure)
                    -> worker_completion
                {
                    const auto safe = abort_preparation(*context, operation);
                    return failed(
                        safe ? failure : core::epoch_failure::repair_required);
                };
                const auto abort_and_cancel = [&]() -> worker_completion
                {
                    const auto safe = abort_preparation(*context, operation);
                    return safe
                        ? cancelled(
                              core::epoch_commit_progress::before_ref_commit)
                        : failed(core::epoch_failure::repair_required);
                };

                if(const auto* association =
                       std::get_if<core::associate_review_mutation>(
                           &request.Mutation))
                {
                    if(operation.Kind != prepared_kind::observation_only)
                    {
                        return abort_and_fail(
                            core::epoch_failure::malformed_observation);
                    }
                    if(stop.stop_requested())
                    {
                        return cancelled(
                            core::epoch_commit_progress::before_ref_commit);
                    }
                    const auto visible_association = [&]()
                        -> std::optional<core::epoch_catalog_revision>
                    {
                        try
                        {
                            const auto snapshot =
                                context->Catalog->snapshot({});
                            if(
                                !snapshot
                                || !application::valid(snapshot->Binding)
                                || snapshot->Binding
                                    != context->Catalog->binding()
                                || snapshot->Binding.Catalog.Incarnation
                                    != expected_catalog.Incarnation
                                || expected_catalog.Revision.value()
                                    >= core::maximum_epoch_catalog_revision
                                || snapshot->Binding.Catalog.Revision.value()
                                    != expected_catalog.Revision.value() + 1
                                || snapshot->CurrentViewerReview
                                    != std::optional{association->Review})
                            {
                                return std::nullopt;
                            }
                            return snapshot->Binding.Catalog.Revision;
                        }
                        catch(...)
                        {
                            return std::nullopt;
                        }
                    };
                    epoch_catalog_result<catalog_commit_result> committed =
                        std::unexpected{epoch_catalog_failure::unavailable};
                    try
                    {
                        committed = context->Catalog->associate_review(
                            associate_review_request{
                                .Catalog = expected_catalog,
                                .Review = association->Review,
                                .AcceptedSnapshot =
                                    request.Context.Revision.Source.Snapshot,
                            },
                            stop);
                    }
                    catch(...)
                    {
                        const auto visible = visible_association();
                        return visible ? succeeded(*visible)
                                       : failed(
                                           core::epoch_failure::
                                               repair_required);
                    }
                    if(!committed)
                    {
                        if(const auto visible = visible_association())
                        {
                            return succeeded(*visible);
                        }
                        return committed.error()
                                == epoch_catalog_failure::cancelled
                            ? cancelled(core::epoch_commit_progress::
                                  before_ref_commit)
                            : failed(catalog_failure(committed.error()));
                    }
                    if(
                        !exact_catalog_commit(
                            *committed, expected_catalog, std::nullopt)
                        || context->Catalog->binding().Catalog
                            != committed->Catalog)
                    {
                        const auto visible = visible_association();
                        return visible ? succeeded(*visible)
                                       : failed(
                                           core::epoch_failure::
                                               catalog_conflict);
                    }
                    return succeeded(committed->Catalog.Revision);
                }

                const auto* append =
                    std::get_if<core::append_epoch_mutation>(&request.Mutation);
                if(append == nullptr || operation.Kind != prepared_kind::append
                   || !operation.Record || !operation.Git
                   || operation.Record->Context.Catalog != expected_catalog
                   || operation.Record->ReservedOrdinal
                       != append->Epoch.Id.Ordinal
                   || operation.Git->Base != append->Epoch.Base
                   || operation.Git->Head != append->Epoch.Head)
                {
                    return abort_and_fail(
                        core::epoch_failure::malformed_observation);
                }
                const auto expected_cache = git_cache_open_request{
                    .ApplicationSession =
                        request.Context.ApplicationSession,
                    .Authority = context->Authority,
                    .ObjectFormat = context->Handoff.ObjectFormat,
                };
                const auto cache = open_actor_cache(
                    *context->Actor, *caches, expected_cache, stop);
                if(!cache)
                {
                    return abort_and_fail(cache_failure(cache.error()));
                }
                if(stop.stop_requested())
                {
                    return abort_and_cancel();
                }
                const auto durable_commit_token = entropy->generate();
                if(!durable_commit_token
                   || !application::valid(*durable_commit_token))
                {
                    return abort_and_fail(core::epoch_failure::unavailable);
                }

                const auto visible_append = [&]()
                    -> std::optional<core::epoch_catalog_revision>
                {
                    try
                    {
                        const auto snapshot = context->Catalog->snapshot({});
                        if(
                            !snapshot
                            || !application::valid(snapshot->Binding)
                            || snapshot->Binding
                                != context->Catalog->binding()
                            || snapshot->Binding.Catalog.Incarnation
                                != expected_catalog.Incarnation
                            || expected_catalog.Revision.value()
                                >= core::maximum_epoch_catalog_revision
                            || snapshot->Binding.Catalog.Revision.value()
                                != expected_catalog.Revision.value() + 1
                            || snapshot->LatestLive
                                != std::optional{append->Epoch})
                        {
                            return std::nullopt;
                        }
                        return snapshot->Binding.Catalog.Revision;
                    }
                    catch(...)
                    {
                        return std::nullopt;
                    }
                };
                const auto resolve_ambiguity = [&, context](
                    const core::epoch_failure fallback,
                    const bool cancelled_before_ponr) -> worker_completion
                {
                    if(const auto visible = visible_append())
                    {
                        return succeeded(*visible);
                    }
                    try
                    {
                        const auto pending = pending_append(
                            *context->Catalog, operation.Operation);
                        if(!pending)
                        {
                            return failed(
                                core::epoch_failure::repair_required);
                        }
                        if(!*pending)
                        {
                            if(const auto visible = visible_append())
                            {
                                return succeeded(*visible);
                            }
                            return cancelled_before_ponr
                                ? cancelled(core::epoch_commit_progress::
                                      before_ref_commit)
                                : failed(fallback);
                        }
                        const auto& durable = **pending;
                        if(
                            !same_append_identity(
                                *operation.Record, durable)
                            || durable.Spool != operation.Record->Spool
                            || durable.Preimport
                                != operation.Record->Preimport
                            || durable.Install != operation.Record->Install
                            || (
                                durable.Candidate
                                && durable.Candidate
                                    != std::optional{*append}))
                        {
                            return failed(
                                core::epoch_failure::repair_required);
                        }
                        operation.Record = durable;
                        if(!point_of_no_cancellation(durable.Phase))
                        {
                            const auto safe =
                                abort_preparation(*context, operation);
                            if(!safe)
                            {
                                return failed(
                                    core::epoch_failure::repair_required);
                            }
                            return cancelled_before_ponr
                                ? cancelled(core::epoch_commit_progress::
                                      before_ref_commit)
                                : failed(fallback);
                        }

                        const auto recovered = recover_append(
                            *context->Catalog,
                            **cache,
                            *entropy,
                            durable,
                            {});
                        if(const auto visible = visible_append())
                        {
                            return succeeded(*visible);
                        }
                        return recovered
                            ? failed(core::epoch_failure::repair_required)
                            : failed(recovered.error());
                    }
                    catch(...)
                    {
                        if(const auto visible = visible_append())
                        {
                            return succeeded(*visible);
                        }
                        return failed(
                            core::epoch_failure::repair_required);
                    }
                };

                const auto candidate_request = record_candidate_request{
                        .Catalog = expected_catalog,
                        .Operation = operation.Operation,
                        .ExpectedOperationRevision =
                            operation.Record->OperationRevision,
                        .Candidate = *append,
                    };
                epoch_catalog_result<append_operation_record> record =
                    std::unexpected{epoch_catalog_failure::unavailable};
                try
                {
                    record = context->Catalog->record_candidate(
                        candidate_request, stop);
                }
                catch(...)
                {
                    return resolve_ambiguity(
                        core::epoch_failure::repair_required, false);
                }
                if(!record)
                {
                    const auto was_cancelled =
                        record.error() == epoch_catalog_failure::cancelled;
                    return resolve_ambiguity(
                        was_cancelled
                            ? core::epoch_failure::unavailable
                            : catalog_failure(record.error()),
                        was_cancelled);
                }
                if(!exact_candidate_record(
                       *operation.Record, *record, candidate_request))
                {
                    return resolve_ambiguity(
                        core::epoch_failure::repair_required, false);
                }
                operation.Record = *record;
                if(stop.stop_requested())
                {
                    return abort_and_cancel();
                }
                epoch_catalog_result<append_operation_record>
                    point_of_no_return =
                        std::unexpected{
                            epoch_catalog_failure::unavailable};
                try
                {
                    point_of_no_return = advance_append(
                        *context,
                        *operation.Record,
                        expected_catalog,
                        epoch_catalog_operation_phase::
                            ref_commit_in_progress,
                        stop);
                }
                catch(...)
                {
                    return resolve_ambiguity(
                        core::epoch_failure::repair_required, false);
                }
                if(!point_of_no_return)
                {
                    const auto was_cancelled = point_of_no_return.error()
                        == epoch_catalog_failure::cancelled;
                    return resolve_ambiguity(
                        was_cancelled
                            ? core::epoch_failure::unavailable
                            : catalog_failure(
                                  point_of_no_return.error()),
                        was_cancelled);
                }
                operation.Record = *point_of_no_return;

                const auto resolve_recovery = [&]() -> worker_completion
                {
                    return resolve_ambiguity(
                        core::epoch_failure::repair_required, false);
                };

                try
                {
                auto retained = context->Actor->Cache->promote_retained(
                    *operation.Git,
                    git_ref_promotion_authorization{
                        .CatalogIncarnation = expected_catalog.Incarnation,
                        .ExpectedCatalogRevision = expected_catalog.Revision,
                        .Operation = operation.Operation,
                        .Scope = context->Authority.Scope,
                        .Ordinal = append->Epoch.Id.Ordinal,
                        .Base = append->Epoch.Base,
                        .Head = append->Epoch.Head,
                        .DurableCommitToken = *durable_commit_token,
                    },
                    {});
                if(!retained)
                {
                    return resolve_recovery();
                }
                if(*retained != expected_retained_pair(*operation.Record))
                {
                    return resolve_recovery();
                }
                record = advance_append(
                    *context,
                    *operation.Record,
                    expected_catalog,
                    epoch_catalog_operation_phase::refs_committed,
                    {});
                if(!record)
                {
                    return resolve_recovery();
                }
                operation.Record = *record;
                const auto inspected = context->Actor->Cache->inspect_retained(
                    append->Epoch.Id.Ordinal, {});
                if(!inspected)
                {
                    return resolve_recovery();
                }
                if(
                    *inspected
                    != expected_retained_pair(*operation.Record))
                {
                    return resolve_recovery();
                }
                const auto finalized = context->Catalog->finalize_append(
                    finalize_append_request{
                        .Catalog = expected_catalog,
                        .Operation = operation.Operation,
                        .ExpectedOperationRevision =
                            operation.Record->OperationRevision,
                        .Mutation = *append,
                        .ObservedRefs = *inspected,
                    },
                    {});
                if(!finalized)
                {
                    return resolve_recovery();
                }
                if(
                    !exact_catalog_commit(
                        *finalized,
                        expected_catalog,
                        append->Epoch.Id)
                    || context->Catalog->binding().Catalog
                        != finalized->Catalog)
                {
                    return resolve_recovery();
                }

                // Catalog visibility is the durable-success linearization
                // point. Cleanup failure remains recoverable and must not
                // replace this committed terminal with failure/cancellation.
                auto cleanup_record = *operation.Record;
                cleanup_record.Phase =
                    epoch_catalog_operation_phase::visible_cleanup_pending;
                cleanup_record.OperationRevision =
                    epoch_catalog_operation_revision{
                        cleanup_record.OperationRevision.value() + 1};
                const auto cleanup_request = recovery_request(cleanup_record);
                if(!cleanup_request)
                {
                    mark_append_repair(*context->Catalog, cleanup_record);
                }
                else
                {
                    const auto cleaned =
                        context->Actor->Cache->complete_recovered_operation(
                            *cleanup_request, {});
                    if(cleaned)
                    {
                        static_cast<void>(advance_append(
                            *context,
                            cleanup_record,
                            finalized->Catalog,
                            epoch_catalog_operation_phase::complete,
                            {}));
                    }
                    else if(
                        cache_failure(cleaned.error())
                        == core::epoch_failure::repair_required)
                    {
                        mark_append_repair(
                            *context->Catalog, cleanup_record);
                    }
                }
                return succeeded(finalized->Catalog.Revision);
                }
                catch(...)
                {
                    return resolve_recovery();
                }
            },
            [request](implementation& host)
            {
                host.dispatch_core(core::epoch_catalog_commit_failed_event{
                    .Context = request.Context,
                    .Mutation = request.Mutation,
                    .Failure = core::epoch_failure::repair_required,
                });
            },
            request.Context.Request,
            request.Context.Generation,
            std::nullopt,
            std::nullopt,
            std::move(actor));
    }

    void discard_preparation(
        const core::discard_epoch_preparation_request& request)
    {
        auto context = exact_context(
            request.Context.Authority, request.Context.Revision);
        if(!context)
        {
            return;
        }
        discard_context_preparation(
            std::move(context), request.Context.Operation);
    }

    void start_epoch_maintenance(
        const core::perform_epoch_maintenance_request& request)
    {
        auto context = Active;
        if(!context || context->Authority != request.Context.Authority
            || context->Revision.Source != request.Context.Source)
        {
            return;
        }
        auto* const caches = &Caches;
        auto* const entropy = &Entropy;
        auto actor = context->Actor;
        submit_effect(
            [context, request, caches, entropy](
                const std::stop_token stop) -> worker_completion
            {
                const auto completed =
                    [request](const core::epoch_catalog_identity catalog)
                    -> worker_completion
                {
                    return [request, catalog](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_maintenance_completed_event{
                                .Context = request.Context,
                                .NewCatalog = catalog,
                            });
                    };
                };
                const auto failed =
                    [request](
                        const core::epoch_failure failure) -> worker_completion
                {
                    return [request, failure](implementation& host)
                    {
                        host.dispatch_core(core::epoch_maintenance_failed_event{
                            .Context = request.Context,
                            .Failure = failure,
                        });
                    };
                };
                const auto cancelled = [request]() -> worker_completion
                {
                    return [request](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_maintenance_cancelled_event{
                                .Context = request.Context,
                            });
                    };
                };
                const auto& binding = context->Catalog->binding();
                if(!application::valid(binding)
                    || binding.Authority != request.Context.Authority
                    || binding.Catalog != request.Context.Catalog
                    || request.Context.Cache != binding.Authority.Cache
                    || request.Context.ApplicationSession
                        != context->Handoff.ApplicationSession
                    || request.Context.Generation.value() == 0U
                    || request.Context.Request.value() == 0U
                    || !request.Context.TargetDigest.valid())
                {
                    return failed(core::epoch_failure::catalog_conflict);
                }

                const auto snapshot = context->Catalog->snapshot(stop);
                if(!snapshot)
                {
                    return snapshot.error() == epoch_catalog_failure::cancelled
                        ? cancelled()
                        : failed(catalog_failure(snapshot.error()));
                }
                if(!application::valid(snapshot->Binding)
                    || snapshot->Binding != binding)
                {
                    return failed(core::epoch_failure::repair_required);
                }

                std::optional<maintenance_operation_record> operation;
                for(std::size_t attempt = 0U;
                    attempt < maximum_identity_attempts;
                    ++attempt)
                {
                    const auto identity =
                        mint_identity<core::epoch_operation_id>(*entropy);
                    if(!identity)
                    {
                        return failed(core::epoch_failure::unavailable);
                    }

                    if(request.Context.Kind
                        == core::epoch_maintenance_kind::prune_epochs)
                    {
                        if(request.Context.Epochs.empty()
                            || request.Context.Epochs.size()
                                > maximum_prune_epoch_count)
                        {
                            return failed(
                                core::epoch_failure::malformed_observation);
                        }
                        std::vector<core::comparison_anchor_identity>
                            identities;
                        identities.reserve(request.Context.Epochs.size() * 2U);
                        for(std::size_t index = 0U;
                            index < request.Context.Epochs.size();
                            ++index)
                        {
                            const auto& epoch = request.Context.Epochs[index];
                            if(!core::valid(epoch)
                                || epoch.Authority != binding.Authority.Scope
                                || epoch.Incarnation
                                    != binding.Catalog.Incarnation
                                || (index != 0U
                                    && !(request.Context.Epochs[index - 1U]
                                        < epoch)))
                            {
                                return failed(
                                    core::epoch_failure::malformed_observation);
                            }
                            identities.push_back(
                                core::comparison_anchor_identity{
                                    .Authority = epoch.Authority,
                                    .Incarnation = epoch.Incarnation,
                                    .Kind = core::comparison_anchor_kind::
                                        epoch_base,
                                    .Epoch = epoch.Ordinal,
                                });
                            identities.push_back(
                                core::comparison_anchor_identity{
                                    .Authority = epoch.Authority,
                                    .Incarnation = epoch.Incarnation,
                                    .Kind = core::comparison_anchor_kind::
                                        epoch_head,
                                    .Epoch = epoch.Ordinal,
                                });
                        }
                        std::ranges::sort(identities);
                        const auto anchors = context->Catalog->exact_anchors(
                            exact_anchor_lookup_request{
                                .Catalog = request.Context.Catalog,
                                .Anchors = identities,
                            },
                            stop);
                        if(!anchors)
                        {
                            return anchors.error()
                                    == epoch_catalog_failure::cancelled
                                ? cancelled()
                                : failed(catalog_failure(anchors.error()));
                        }
                        if(anchors->Catalog != request.Context.Catalog
                            || anchors->Anchors.size() != identities.size())
                        {
                            return failed(core::epoch_failure::repair_required);
                        }
                        std::vector<prune_epoch_fact> facts;
                        facts.reserve(request.Context.Epochs.size());
                        for(const auto& epoch : request.Context.Epochs)
                        {
                            const auto find = [&](const auto kind)
                                -> const core::comparison_anchor*
                            {
                                const auto identity =
                                    core::comparison_anchor_identity{
                                        .Authority = epoch.Authority,
                                        .Incarnation = epoch.Incarnation,
                                        .Kind = kind,
                                        .Epoch = epoch.Ordinal,
                                    };
                                const auto found =
                                    std::ranges::find(anchors->Anchors,
                                        identity,
                                        &core::comparison_anchor::Identity);
                                return found == anchors->Anchors.end()
                                    ? nullptr
                                    : &*found;
                            };
                            const auto* base =
                                find(core::comparison_anchor_kind::epoch_base);
                            const auto* head =
                                find(core::comparison_anchor_kind::epoch_head);
                            if(base == nullptr || head == nullptr
                                || !core::valid(*base) || !core::valid(*head)
                                || !valid_prune_anchor_availability(
                                    base->Availability)
                                || !valid_prune_anchor_availability(
                                    head->Availability)
                                || base->Commit.algorithm()
                                    != context->Handoff.ObjectFormat
                                || head->Commit.algorithm()
                                    != context->Handoff.ObjectFormat)
                            {
                                return failed(
                                    core::epoch_failure::repair_required);
                            }
                            facts.push_back(prune_epoch_fact{
                                .Epoch = epoch,
                                .Retained =
                                    git_retained_ref_pair{
                                        .Scope = epoch.Authority,
                                        .Ordinal = epoch.Ordinal,
                                        .Base = base->Commit,
                                        .Head = head->Commit,
                                    },
                                .BaseRef =
                                    epoch_catalog_ref_identity{
                                        .Authority = epoch.Authority,
                                        .Ordinal = epoch.Ordinal,
                                    },
                                .HeadRef =
                                    epoch_catalog_ref_identity{
                                        .Authority = epoch.Authority,
                                        .Ordinal = epoch.Ordinal,
                                        .Head = true,
                                    },
                                .ExpectedBaseAvailability = base->Availability,
                                .ExpectedHeadAvailability = head->Availability,
                            });
                        }
                        const auto begin = begin_prune_request{
                            .Catalog = request.Context.Catalog,
                            .Operation = *identity,
                            .Epochs = std::move(facts),
                        };
                        auto begun = context->Catalog->begin_prune(begin, stop);
                        if(begun)
                        {
                            if(!exact_prune_intent(*begun,
                                   begin,
                                   binding.Authority.Cache))
                            {
                                return failed(
                                    core::epoch_failure::repair_required);
                            }
                            operation = std::move(*begun);
                            break;
                        }
                        if(begun.error() == epoch_catalog_failure::cancelled)
                        {
                            return cancelled();
                        }
                        if(begun.error()
                                != epoch_catalog_failure::operation_conflict
                            && begun.error()
                                != epoch_catalog_failure::
                                    operation_identity_conflict)
                        {
                            return failed(catalog_failure(begun.error()));
                        }
                    }
                    else if(request.Context.Kind
                        == core::epoch_maintenance_kind::
                            delete_repository_cache)
                    {
                        if(!request.Context.Epochs.empty())
                        {
                            return failed(
                                core::epoch_failure::malformed_observation);
                        }
                        const auto impact =
                            context->Catalog->cache_delete_impact(
                                request.Context.Cache,
                                stop);
                        if(!impact)
                        {
                            return impact.error()
                                    == epoch_catalog_failure::cancelled
                                ? cancelled()
                                : failed(catalog_failure(impact.error()));
                        }
                        const auto current_scope =
                            std::ranges::find(impact->Scopes,
                                binding.Authority.Scope,
                                &cache_delete_scope_fact::Authority);
                        if(!application::valid(*impact)
                            || impact->Incarnation
                                != binding.Catalog.Incarnation
                            || impact->Cache != binding.Authority.Cache
                            || current_scope == impact->Scopes.end()
                            || current_scope->ExpectedRevision
                                != binding.Catalog.Revision)
                        {
                            return failed(core::epoch_failure::repair_required);
                        }
                        const auto begin = begin_cache_delete_request{
                            .Operation = *identity,
                            .Impact = *impact,
                        };
                        auto begun =
                            context->Catalog->begin_cache_delete(begin, stop);
                        if(begun)
                        {
                            if(!exact_cache_delete_intent(*begun, begin))
                            {
                                return failed(
                                    core::epoch_failure::repair_required);
                            }
                            operation = std::move(*begun);
                            break;
                        }
                        if(begun.error() == epoch_catalog_failure::cancelled)
                        {
                            return cancelled();
                        }
                        if(begun.error()
                                != epoch_catalog_failure::operation_conflict
                            && begun.error()
                                != epoch_catalog_failure::
                                    operation_identity_conflict)
                        {
                            return failed(catalog_failure(begun.error()));
                        }
                    }
                    else
                    {
                        return failed(
                            core::epoch_failure::malformed_observation);
                    }
                }
                if(!operation)
                {
                    return failed(core::epoch_failure::catalog_conflict);
                }

                if(stop.stop_requested())
                {
                    const auto aborted = recover_maintenance(*context->Catalog,
                        *caches,
                        context->Handoff,
                        context->Actor->Cache,
                        *operation,
                        {});
                    return aborted ? cancelled() : failed(aborted.error());
                }
                const auto next =
                    operation->Kind == epoch_catalog_operation_kind::prune
                    ? epoch_catalog_operation_phase::ref_delete_in_progress
                    : epoch_catalog_operation_phase::
                          quarantine_rename_in_progress;
                auto authorized = advance_maintenance(*context->Catalog,
                    *operation,
                    request.Context.Catalog,
                    next,
                    stop);
                if(!authorized)
                {
                    if(authorized.error() == epoch_catalog_failure::cancelled)
                    {
                        const auto aborted =
                            recover_maintenance(*context->Catalog,
                                *caches,
                                context->Handoff,
                                context->Actor->Cache,
                                *operation,
                                {});
                        return aborted ? cancelled() : failed(aborted.error());
                    }
                    return failed(catalog_failure(authorized.error()));
                }
                const auto recovered = recover_maintenance(*context->Catalog,
                    *caches,
                    context->Handoff,
                    context->Actor->Cache,
                    *authorized,
                    {});
                return recovered ? completed(*recovered)
                                 : failed(recovered.error());
            },
            [request](implementation& host)
            {
                host.dispatch_core(core::epoch_maintenance_failed_event{
                    .Context = request.Context,
                    .Failure = core::epoch_failure::unavailable,
                });
            },
            request.Context.Request,
            request.Context.Generation,
            std::nullopt,
            std::nullopt,
            std::move(actor));
    }

    void start_history_page(
        const core::load_epoch_history_page_request& request)
    {
        auto context = Active;
        if(!context || context->Authority != request.Context.Authority
           || context->Revision.Source != request.Context.Source)
        {
            return;
        }
        auto actor = context->Actor;
        submit_effect(
            [context, request](const std::stop_token stop)
                -> worker_completion
            {
                if(
                    !application::valid(context->Catalog->binding())
                    || context->Catalog->binding().Authority
                        != request.Context.Authority
                    || context->Catalog->binding().Catalog
                        != request.Context.Catalog)
                {
                    return [request](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_history_page_load_failed_event{
                                .Context = request.Context,
                                .Failure =
                                    core::epoch_failure::catalog_conflict,
                            });
                    };
                }
                auto page = context->Catalog->history(
                    epoch_history_page_request{
                        .Catalog = request.Context.Catalog,
                        .Cursor = request.Context.Cursor,
                        .Limit = request.Context.Limit,
                    },
                    stop);
                if(!page)
                {
                    if(page.error() == epoch_catalog_failure::cancelled)
                    {
                        return [request](implementation& host)
                        {
                            host.dispatch_core(
                                core::epoch_history_page_load_cancelled_event{
                                    .Context = request.Context,
                                });
                        };
                    }
                    const auto failure = catalog_failure(page.error());
                    return [request, failure](implementation& host)
                    {
                        host.dispatch_core(
                            core::epoch_history_page_load_failed_event{
                                .Context = request.Context,
                                .Failure = failure,
                            });
                    };
                }
                auto observation = core::epoch_history_page_observation{
                    .Authority = page->Authority,
                    .Catalog = page->Catalog,
                    .Epochs = std::move(page->Epochs),
                    .Continuation = page->Continuation,
                };
                return [request, observation = std::move(observation)](
                           implementation& host)
                {
                    host.dispatch_core(core::epoch_history_page_loaded_event{
                        .Context = request.Context,
                        .Page = observation,
                    });
                };
            },
            [request](implementation& host)
            {
                host.dispatch_core(
                    core::epoch_history_page_load_failed_event{
                        .Context = request.Context,
                        .Failure = core::epoch_failure::unavailable,
                    });
            },
            request.Context.Request,
            request.Context.Generation,
            std::nullopt,
            std::nullopt,
            std::move(actor));
    }

    void start_comparison_resolution(
        const core::resolve_comparison_request& request)
    {
        auto context = Active;
        if(!context || context->Authority != request.Context.Authority
           || context->Revision.Source != request.Context.Source)
        {
            return;
        }
        auto* const caches = &Caches;
        auto actor = context->Actor;
        submit_effect(
            [context, request, caches](const std::stop_token stop)
                -> worker_completion
            {
                const auto snapshot = context->Catalog->snapshot(stop);
                if(!snapshot)
                {
                    if(snapshot.error() == epoch_catalog_failure::cancelled)
                    {
                        return [request](implementation& host)
                        {
                            host.dispatch_core(
                                core::comparison_resolve_cancelled_event{
                                    .Context = request.Context,
                                });
                        };
                    }
                    const auto failure = catalog_failure(snapshot.error());
                    return [request, failure](implementation& host)
                    {
                        host.dispatch_core(core::comparison_resolve_failed_event{
                            .Context = request.Context,
                            .Failure = failure,
                        });
                    };
                }
                if(
                    !application::valid(snapshot->Binding)
                    || snapshot->Binding != context->Catalog->binding())
                {
                    return [request](implementation& host)
                    {
                        host.dispatch_core(
                            core::comparison_resolve_failed_event{
                                .Context = request.Context,
                                .Failure = core::epoch_failure::repair_required,
                            });
                    };
                }
                const auto expected_catalog = core::epoch_catalog_identity{
                    .Incarnation = request.Context.CatalogIncarnation,
                    .Revision = request.Context.CatalogRevision,
                };
                if(snapshot->Binding.Catalog != expected_catalog)
                {
                    return [request](implementation& host)
                    {
                        host.dispatch_core(core::comparison_resolve_failed_event{
                            .Context = request.Context,
                            .Failure = core::epoch_failure::catalog_conflict,
                        });
                    };
                }

                core::comparison_candidates candidates{
                    .Catalog = snapshot->Binding.Catalog,
                    .LatestLive = snapshot->LatestLive,
                    .PreviousDistinctHead = snapshot->PreviousDistinctHead,
                };
                if(snapshot->CurrentViewerReview)
                {
                    candidates.CurrentViewerReview = review_anchor(
                        context->Authority,
                        snapshot->Binding.Catalog,
                        *snapshot->CurrentViewerReview);
                }
                if(const auto* explicit_pair =
                       std::get_if<core::explicit_comparison_selection>(
                           &request.Context.Selection))
                {
                    std::vector<core::comparison_anchor_identity> identities{
                        explicit_pair->Left,
                        explicit_pair->Right,
                    };
                    std::ranges::sort(identities);
                    identities.erase(
                        std::unique(identities.begin(), identities.end()),
                        identities.end());
                    const auto exact = context->Catalog->exact_anchors(
                        exact_anchor_lookup_request{
                            .Catalog = snapshot->Binding.Catalog,
                            .Anchors = identities,
                        },
                        stop);
                    if(!exact)
                    {
                        if(exact.error() == epoch_catalog_failure::cancelled)
                        {
                            return [request](implementation& host)
                            {
                                host.dispatch_core(
                                    core::comparison_resolve_cancelled_event{
                                        .Context = request.Context,
                                    });
                            };
                        }
                        const auto failure = catalog_failure(exact.error());
                        return [request, failure](implementation& host)
                        {
                            host.dispatch_core(
                                core::comparison_resolve_failed_event{
                                    .Context = request.Context,
                                    .Failure = failure,
                                });
                        };
                    }
                    std::vector<core::comparison_anchor_identity>
                        returned_identities;
                    returned_identities.reserve(exact->Anchors.size());
                    for(const auto& anchor : exact->Anchors)
                    {
                        if(
                            !core::valid(anchor)
                            || !std::ranges::binary_search(
                                identities, anchor.Identity))
                        {
                            return [request](implementation& host)
                            {
                                host.dispatch_core(
                                    core::comparison_resolve_failed_event{
                                        .Context = request.Context,
                                        .Failure = core::epoch_failure::
                                            repair_required,
                                    });
                            };
                        }
                        returned_identities.push_back(anchor.Identity);
                    }
                    std::ranges::sort(returned_identities);
                    if(
                        exact->Catalog != snapshot->Binding.Catalog
                        || exact->Anchors.size() > identities.size()
                        || std::ranges::adjacent_find(returned_identities)
                            != returned_identities.end())
                    {
                        return [request](implementation& host)
                        {
                            host.dispatch_core(
                                core::comparison_resolve_failed_event{
                                    .Context = request.Context,
                                    .Failure = core::epoch_failure::
                                        repair_required,
                                });
                        };
                    }
                    const auto find_anchor = [&exact](const auto& identity)
                        -> std::optional<core::comparison_anchor>
                    {
                        const auto found = std::ranges::find(
                            exact->Anchors,
                            identity,
                            &core::comparison_anchor::Identity);
                        return found == exact->Anchors.end()
                            ? std::nullopt
                            : std::optional{*found};
                    };
                    candidates.ExplicitLeft =
                        find_anchor(explicit_pair->Left);
                    candidates.ExplicitRight =
                        find_anchor(explicit_pair->Right);
                }

                const auto expected_cache = git_cache_open_request{
                    .ApplicationSession =
                        request.Context.ApplicationSession,
                    .Authority = context->Authority,
                    .ObjectFormat = context->Handoff.ObjectFormat,
                };
                const auto opened_cache = open_actor_cache(
                    *context->Actor, *caches, expected_cache, stop);
                if(!opened_cache)
                {
                    const auto failure = cache_failure(opened_cache.error());
                    return [request, failure](implementation& host)
                    {
                        host.dispatch_core(
                            core::comparison_resolve_failed_event{
                                .Context = request.Context,
                                .Failure = failure,
                            });
                    };
                }
                std::vector<std::pair<core::git_commit_oid, bool>> verified;
                const auto is_verified = [&](const core::git_commit_oid commit)
                {
                    const auto prior = std::ranges::find_if(
                        verified,
                        [&](const auto& entry)
                        {
                            return entry.first == commit;
                        });
                    if(prior != verified.end())
                    {
                        return prior->second;
                    }
                    const auto inspected =
                        context->Actor->Cache->inspect_commit_object(
                            commit, stop);
                    const auto exact = inspected
                        && application::valid(*inspected)
                        && inspected->Commit == commit;
                    verified.emplace_back(commit, exact);
                    return exact;
                };
                const auto verify_epoch = [&](auto& epoch)
                {
                    if(
                        epoch.BaseAvailability
                            == core::anchor_availability::available
                        && !is_verified(epoch.Base))
                    {
                        epoch.BaseAvailability =
                            core::anchor_availability::unavailable;
                    }
                    if(
                        epoch.HeadAvailability
                            == core::anchor_availability::available
                        && !is_verified(epoch.Head))
                    {
                        epoch.HeadAvailability =
                            core::anchor_availability::unavailable;
                    }
                };
                const auto verify_anchor = [&](auto& anchor)
                {
                    if(
                        anchor
                        && anchor->Availability
                            == core::anchor_availability::available
                        && !is_verified(anchor->Commit))
                    {
                        anchor->Availability =
                            core::anchor_availability::unavailable;
                    }
                };
                if(candidates.LatestLive)
                {
                    verify_epoch(*candidates.LatestLive);
                }
                if(candidates.PreviousDistinctHead)
                {
                    verify_epoch(*candidates.PreviousDistinctHead);
                }
                verify_anchor(candidates.CurrentViewerReview);
                verify_anchor(candidates.ExplicitLeft);
                verify_anchor(candidates.ExplicitRight);
                return [request, candidates = std::move(candidates)](
                           implementation& host)
                {
                    host.dispatch_core(core::comparison_resolved_event{
                        .Context = request.Context,
                        .Candidates = candidates,
                    });
                };
            },
            [request](implementation& host)
            {
                host.dispatch_core(core::comparison_resolve_failed_event{
                    .Context = request.Context,
                    .Failure = core::epoch_failure::unavailable,
                });
            },
            request.Context.Request,
            std::nullopt,
            request.Context.Generation,
            std::nullopt,
            std::move(actor));
    }

    core::epoch_state State;
    git_cache_port& Caches;
    epoch_catalog_port& Catalogs;
    epoch_entropy_port& Entropy;
    epoch_clock_port& Clock;
    effect_executor& Effects;
    decision_executor& DecisionLane;
    epoch_projection_port& Projections;
    std::shared_ptr<completion_gate> CompletionGate;
    std::vector<
        std::pair<cache_actor_key, std::shared_ptr<authority_effect_actor>>>
        CacheActors;
    std::shared_ptr<authority_context> Active;
    std::optional<pull_request_authority_invalidation> PendingAuthority;
    std::optional<std::uint64_t> PendingOfferToken;
    std::vector<std::shared_ptr<job_control>> Jobs;
    std::deque<deferred_ingress> DeferredIngress;
    std::uint64_t OfferToken = 0U;
    bool TurnActive = false;
    bool DeferredIngressScheduled = false;
    bool Closing = false;
    bool ProjectionDeliveryFailed = false;
};

epoch_application_host::epoch_application_host(
    core::epoch_state initial_state,
    git_cache_port& caches,
    epoch_catalog_port& catalogs,
    epoch_entropy_port& entropy,
    epoch_clock_port& clock,
    effect_executor& effects,
    decision_executor& decision_lane,
    epoch_projection_port& projections)
    : Implementation{std::make_unique<implementation>(
          std::move(initial_state),
          caches,
          catalogs,
          entropy,
          clock,
          effects,
          decision_lane,
          projections)}
{
}

epoch_application_host::~epoch_application_host()
{
    shutdown();
}

void epoch_application_host::accept(accepted_pull_request_handoff handoff)
{
    Implementation->accept(std::move(handoff));
}

void epoch_application_host::invalidate(
    pull_request_authority_invalidation invalidation)
{
    Implementation->invalidate(std::move(invalidation));
}

void epoch_application_host::dispatch(epoch_intent intent)
{
    Implementation->dispatch(std::move(intent));
}

void epoch_application_host::shutdown()
{
    Implementation->shutdown();
}

auto epoch_application_host::state() const noexcept -> const core::epoch_state&
{
    return Implementation->State;
}

auto epoch_application_host::is_closing() const noexcept -> bool
{
    return Implementation->Closing;
}

auto epoch_application_host::is_quiescent() const noexcept -> bool
{
    return Implementation->Closing && Implementation->Jobs.empty()
        && !Implementation->TurnActive
        && Implementation->DeferredIngress.empty()
        && !Implementation->DeferredIngressScheduled
        && !Implementation->PendingOfferToken
        && Implementation->actors_idle();
}

auto epoch_application_host::projection_delivery_failed() const noexcept
    -> bool
{
    return Implementation->ProjectionDeliveryFailed
        || Implementation->CompletionGate->DeliveryFailed.load(
            std::memory_order_acquire);
}
} // namespace bpr::application
