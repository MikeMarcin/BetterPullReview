#include "bpr/application/epoch_application_host.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using namespace bpr;

void expect(const bool condition, const std::string_view message)
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    bytes.front() = seed;
    bytes.back() = static_cast<std::uint8_t>(seed ^ 0xA5U);
    return Identity{bytes};
}

[[nodiscard]] constexpr auto entropy_bytes(const std::uint8_t seed)
    -> application::epoch_entropy_bytes
{
    auto bytes = application::epoch_entropy_bytes{};
    bytes.front() = seed;
    bytes.back() = static_cast<std::uint8_t>(seed ^ 0x5AU);
    return bytes;
}

template<typename Oid>
[[nodiscard]] constexpr auto oid(const std::uint8_t seed) -> Oid
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    for(std::size_t index = 0U; index < 20U; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return Oid{core::git_hash_algorithm::sha1, bytes};
}

[[nodiscard]] constexpr auto pack_oid(const std::uint8_t seed)
    -> application::git_pack_oid
{
    auto digest = std::array<std::uint8_t, 32U>{};
    digest.front() = seed;
    return {
        .Algorithm = core::git_hash_algorithm::sha1,
        .Digest = digest,
    };
}

[[nodiscard]] constexpr auto artifact(
    const application::git_artifact_role role,
    const std::uint8_t seed,
    const std::uint64_t bytes) -> application::git_artifact_fact
{
    auto identity = std::array<std::uint8_t, 32U>{};
    identity[0] = 1U;
    identity[1] = 1U;
    identity[2] = 1U;
    identity[8] = seed;
    return {
        .Role = role,
        .Identity = application::git_artifact_identity{identity},
        .Bytes = bytes,
    };
}

[[nodiscard]] auto bytes(const std::string_view value)
    -> std::vector<std::byte>
{
    return {
        reinterpret_cast<const std::byte*>(value.data()),
        reinterpret_cast<const std::byte*>(value.data() + value.size()),
    };
}

[[nodiscard]] auto packet(const std::string_view payload) -> std::string
{
    constexpr std::string_view digits = "0123456789abcdef";
    const auto total = payload.size() + 4U;
    auto header = std::array<char, 4U>{};
    auto remaining = total;
    for(auto index = header.size(); index-- > 0U;)
    {
        header[index] = digits[remaining & 0x0FU];
        remaining >>= 4U;
    }
    return std::string{header.data(), header.size()} + std::string{payload};
}

class queued_executor final
    : public application::decision_executor,
      public application::effect_executor
{
public:
    explicit queued_executor(const bool decision) noexcept
        : Decision{decision}
    {
    }

    void post(std::function<void()> task) override
    {
        Tasks.push_back(std::move(task));
    }

    [[nodiscard]] auto pending() const noexcept -> std::size_t
    {
        return Tasks.size();
    }

    void run(const std::size_t index)
    {
        expect(index < Tasks.size(), "executor index is in range");
        auto task = std::move(Tasks[index]);
        Tasks.erase(Tasks.begin() + static_cast<std::ptrdiff_t>(index));
        Running = true;
        task();
        Running = false;
    }

    void run_next()
    {
        run(0U);
    }

    [[nodiscard]] auto is_running() const noexcept -> bool
    {
        return Running;
    }

    [[nodiscard]] auto is_decision() const noexcept -> bool
    {
        return Decision;
    }

private:
    std::deque<std::function<void()>> Tasks;
    bool Decision = false;
    bool Running = false;
};

struct shared_state
{
    std::vector<std::string> Calls;
    application::epoch_catalog_binding Binding;
    application::epoch_catalog_snapshot Snapshot;
    std::deque<bool> SourceCollisions;
    std::deque<std::optional<application::epoch_catalog_failure>>
        CatalogOpenFailures;
    std::deque<application::epoch_catalog_snapshot> CatalogOpenSnapshots;
    std::optional<application::append_operation_record> Operation;
    std::optional<application::maintenance_operation_record> Maintenance;
    std::optional<application::git_retained_ref_pair> Retained;
    std::deque<application::epoch_recovery_page> RecoveryPages;
    std::vector<std::optional<core::epoch_operation_id>> RecoveryPageCursors;
    std::deque<application::epoch_catalog_result<
        application::epoch_history_page>> HistoryPages;
    std::vector<application::epoch_history_page_request> HistoryRequests;
    std::deque<application::git_cache_result<
        application::git_operation_recovery_observation>>
        RecoveryInspections;
    std::vector<application::git_operation_recovery_request>
        RecoveryInspectionRequests;
    std::vector<application::git_operation_recovery_request>
        RecoveryAbortRequests;
    std::vector<application::git_operation_recovery_request>
        RecoveryCompletionRequests;
    std::vector<application::git_cache_open_request> CacheOpenRequests;
    std::vector<application::exact_anchor_lookup_request> ExactAnchorRequests;
    std::vector<application::git_ref_prune_authorization> PruneRequests;
    std::vector<application::git_cache_delete_authorization> DeleteRequests;
    std::vector<application::begin_prune_request> BeginPruneRequests;
    std::vector<application::begin_cache_delete_request>
        BeginCacheDeleteRequests;
    std::deque<std::pair<std::string, application::epoch_catalog_failure>>
        MaintenanceCatalogFailures;
    std::deque<std::pair<std::string, application::git_cache_failure>>
        MaintenanceCacheFailures;
    bool PromoteAppliesThenFails = false;
    bool AssociationAppliesThenFails = false;
    bool CacheDeleteFinalizesWithoutChange = false;
    bool ThrowRecoveryAbort = false;
    std::function<void()> AfterCommitAuthorized;
    std::function<void()> AfterRefCommitInProgress;
    std::vector<std::uint64_t> AuthorityRevocations;
    std::vector<application::epoch_presentation_update> Updates;
    std::size_t CacheMutationCalls = 0U;
    std::size_t ActiveCacheSessions = 0U;
    std::size_t MaximumActiveCacheSessions = 0U;
    std::size_t TransportCalls = 0U;
    std::size_t ProjectionCalls = 0U;
    std::size_t ProjectionOutsideDecision = 0U;
    std::size_t ProjectionFailuresRemaining = 0U;
    std::size_t ProjectionDepth = 0U;
    std::size_t MaximumProjectionDepth = 0U;
    std::size_t ProjectionIngressCalls = 0U;
    std::function<void()> ProjectionIngress;
};

template <typename Failure>
[[nodiscard]] auto take_failure(
    std::deque<std::pair<std::string, Failure>>& failures,
    const std::string_view call) -> std::optional<Failure>
{
    if(failures.empty() || failures.front().first != call)
    {
        return std::nullopt;
    }
    const auto failure = failures.front().second;
    failures.pop_front();
    return failure;
}

[[nodiscard]] auto authority() -> core::pull_request_authority
{
    return {
        .Profile = core::host_profile_id{11U},
        .ProfileRevision = core::github_profile_revision{21U},
        .Host = core::git_host_id{31U},
        .HostAuthority = opaque<core::host_authority_fingerprint>(2U),
        .Account = core::github_account_id{41U},
        .Viewer = core::github_viewer_actor_id{51U},
        .Repository = core::repository_id{61U},
        .Cache = opaque<core::repository_cache_scope_id>(3U),
        .PullRequest = core::pull_request_id{71U},
        .Number = core::pull_request_number{81U},
        .Scope = opaque<core::pull_request_authority_scope_id>(4U),
    };
}

[[nodiscard]] auto catalog_identity(
    const std::int64_t revision = 0) -> core::epoch_catalog_identity
{
    return {
        .Incarnation = opaque<core::epoch_catalog_incarnation_id>(5U),
        .Revision = core::epoch_catalog_revision{revision},
    };
}

[[nodiscard]] auto summary_for(
    const core::accepted_pull_request_revision& revision,
    const core::epoch_catalog_identity catalog,
    const core::epoch_ordinal ordinal = core::epoch_ordinal{1})
    -> core::review_epoch_summary
{
    return {
        .Id =
            {
                .Authority = authority().Scope,
                .Incarnation = catalog.Incarnation,
                .Ordinal = ordinal,
            },
        .Source = revision.Source,
        .BaseBranch = revision.BaseBranch,
        .Base = revision.Base,
        .Head = revision.Head,
        .Material =
            core::epoch_material_identity{
                .BaseTree = oid<core::git_tree_oid>(60U),
                .HeadTree = oid<core::git_tree_oid>(70U),
                .PolicyRevision = core::comparison_policy_revision{1U},
            },
        .Cause =
            {
                .Origin = core::epoch_origin::initial_observation,
                .Base = core::epoch_base_transition::not_applicable,
                .Head = core::epoch_head_transition::not_applicable,
                .Material = core::epoch_material_relation::unknown,
                .Primary = core::epoch_primary_cause::initial_observation,
            },
        .ObservedAt = core::observed_utc_milliseconds{100U},
        .BaseAvailability = core::anchor_availability::available,
        .HeadAvailability = core::anchor_availability::available,
    };
}

[[nodiscard]] auto history_cursor_for(
    const core::epoch_catalog_identity catalog,
    const core::epoch_ordinal before) -> core::epoch_history_cursor
{
    return {
        .Authority = authority().Scope,
        .Incarnation = catalog.Incarnation,
        .Revision = catalog.Revision,
        .Before = before,
    };
}

class fake_entropy final : public application::epoch_entropy_port
{
public:
    explicit fake_entropy(shared_state& state) noexcept : State{state}
    {
    }

    [[nodiscard]] auto generate() noexcept
        -> std::optional<application::epoch_entropy_bytes> override
    {
        State.Calls.emplace_back("entropy");
        return entropy_bytes(Next++);
    }

private:
    shared_state& State;
    std::uint8_t Next = 0x20U;
};

class fake_clock final : public application::epoch_clock_port
{
public:
    explicit fake_clock(shared_state& state) noexcept : State{state}
    {
    }

    [[nodiscard]] auto observe_utc() noexcept
        -> std::optional<core::observed_utc_milliseconds> override
    {
        State.Calls.emplace_back("clock");
        return core::observed_utc_milliseconds{Next++};
    }

private:
    shared_state& State;
    std::int64_t Next = 1'000;
};

class fake_projection final : public application::epoch_projection_port
{
public:
    fake_projection(shared_state& state, queued_executor& decision) noexcept
        : State{state}, Decision{decision}
    {
    }

    void publish(const application::epoch_presentation_update& update) override
    {
        ++State.ProjectionCalls;
        if(!Decision.is_running())
        {
            ++State.ProjectionOutsideDecision;
        }
        ++State.ProjectionDepth;
        State.MaximumProjectionDepth = std::max(
            State.MaximumProjectionDepth, State.ProjectionDepth);
        struct depth_guard final
        {
            std::size_t& Depth;

            ~depth_guard()
            {
                --Depth;
            }
        } guard{State.ProjectionDepth};
        if(State.ProjectionFailuresRemaining != 0U)
        {
            --State.ProjectionFailuresRemaining;
            throw std::runtime_error{"injected projection failure"};
        }
        State.Updates.push_back(update);
        auto ingress = std::exchange(State.ProjectionIngress, {});
        if(ingress)
        {
            ++State.ProjectionIngressCalls;
            ingress();
        }
    }

private:
    shared_state& State;
    queued_executor& Decision;
};

class fake_catalog_session final : public application::epoch_catalog_session
{
public:
    explicit fake_catalog_session(shared_state& state) noexcept : State{state}
    {
    }

    [[nodiscard]] auto binding() const noexcept
        -> const application::epoch_catalog_binding& override
    {
        return State.Binding;
    }

    [[nodiscard]] auto snapshot(const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::epoch_catalog_snapshot> override
    {
        State.Calls.emplace_back("catalog.snapshot");
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::epoch_catalog_failure::cancelled};
        }
        if(const auto failure = take_failure(State.MaintenanceCatalogFailures,
               "catalog.snapshot"))
        {
            return std::unexpected{*failure};
        }
        return State.Snapshot;
    }

    [[nodiscard]] auto source_observation_exists(
        const core::epoch_source_observation_id,
        const std::stop_token stop)
        -> application::epoch_catalog_result<bool> override
    {
        State.Calls.emplace_back("catalog.source-probe");
        if(stop.stop_requested())
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        if(State.SourceCollisions.empty())
        {
            return false;
        }
        const auto collision = State.SourceCollisions.front();
        State.SourceCollisions.pop_front();
        return collision;
    }

    [[nodiscard]] auto canonical_review(
        const application::canonical_review_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            core::catalog_review_association> override
    {
        State.Calls.emplace_back("catalog.canonical-review");
        if(stop.stop_requested())
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        return core::catalog_review_association{
            .Association = request.ProposedAssociation,
            .Review = request.Candidate.Review,
            .Actor = request.Candidate.Actor,
            .Order = request.Candidate.Order,
            .State = request.Candidate.State,
            .Commit = request.Candidate.Commit,
            .Availability = core::anchor_availability::available,
        };
    }

    [[nodiscard]] auto exact_anchors(
        const application::exact_anchor_lookup_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::exact_anchor_lookup_result> override
    {
        State.Calls.emplace_back("catalog.exact-anchors");
        State.ExactAnchorRequests.push_back(request);
        if(stop.stop_requested())
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        if(const auto failure = take_failure(State.MaintenanceCatalogFailures,
               "catalog.exact-anchors"))
        {
            return std::unexpected{*failure};
        }
        auto result = application::exact_anchor_lookup_result{
            .Catalog = request.Catalog,
        };
        if(State.Snapshot.LatestLive)
        {
            const auto& epoch = *State.Snapshot.LatestLive;
            for(const auto& identity : request.Anchors)
            {
                if(identity.Authority != epoch.Id.Authority
                    || identity.Incarnation != epoch.Id.Incarnation
                    || identity.Epoch != epoch.Id.Ordinal)
                {
                    continue;
                }
                if(identity.Kind == core::comparison_anchor_kind::epoch_base)
                {
                    result.Anchors.push_back(core::comparison_anchor{
                        .Identity = identity,
                        .Commit = epoch.Base,
                        .Availability = epoch.BaseAvailability,
                    });
                }
                else if(identity.Kind
                    == core::comparison_anchor_kind::epoch_head)
                {
                    result.Anchors.push_back(core::comparison_anchor{
                        .Identity = identity,
                        .Commit = epoch.Head,
                        .Availability = epoch.HeadAvailability,
                    });
                }
            }
        }
        return result;
    }

    [[nodiscard]] auto history(
        const application::epoch_history_page_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::epoch_history_page> override
    {
        State.Calls.emplace_back("catalog.history");
        State.HistoryRequests.push_back(request);
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::epoch_catalog_failure::cancelled};
        }
        if(State.HistoryPages.empty())
        {
            return std::unexpected{
                application::epoch_catalog_failure::invalid_request};
        }
        auto page = std::move(State.HistoryPages.front());
        State.HistoryPages.pop_front();
        return page;
    }

    [[nodiscard]] auto mark_anchor_availability(
        const application::mark_anchor_availability_request&,
        const std::stop_token)
        -> application::epoch_catalog_result<
            application::catalog_commit_result> override
    {
        return std::unexpected{application::epoch_catalog_failure::invalid_request};
    }

    [[nodiscard]] auto reserve_append(
        const application::append_reservation_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::append_operation_record> override
    {
        State.Calls.emplace_back("catalog.reserve");
        if(stop.stop_requested())
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        const auto owner = State.Binding.Authority.Scope;
        const auto operation = request.Context.Operation;
        State.Operation = application::append_operation_record{
            .Context = request.Context,
            .ReservedOrdinal = request.ProposedOrdinal,
            .OperationRevision =
                application::epoch_catalog_operation_revision{1},
            .Phase = application::epoch_catalog_operation_phase::preparing,
            .StagingBase =
                {
                    .Authority = owner,
                    .Ordinal = request.ProposedOrdinal,
                    .Operation = operation,
                    .Staging = true,
                },
            .StagingHead =
                {
                    .Authority = owner,
                    .Ordinal = request.ProposedOrdinal,
                    .Operation = operation,
                    .Staging = true,
                    .Head = true,
                },
            .FinalBase =
                {
                    .Authority = owner,
                    .Ordinal = request.ProposedOrdinal,
                },
            .FinalHead =
                {
                    .Authority = owner,
                    .Ordinal = request.ProposedOrdinal,
                    .Head = true,
                },
        };
        return *State.Operation;
    }

    [[nodiscard]] auto record_spool(
        const application::record_spool_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::append_operation_record> override
    {
        State.Calls.emplace_back("catalog.spool-recorded");
        if(stop.stop_requested() || !State.Operation)
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        State.Operation->Spool = request.Spool;
        State.Operation->Preimport = request.Preimport;
        advance(application::epoch_catalog_operation_phase::spool_recorded);
        expect(
            application::valid(*State.Operation),
            "catalog fake returns a valid spool record");
        return *State.Operation;
    }

    [[nodiscard]] auto record_install(
        const application::record_install_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::append_operation_record> override
    {
        State.Calls.emplace_back("catalog.imported");
        if(stop.stop_requested() || !State.Operation)
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        State.Operation->Install = request.Install;
        advance(application::epoch_catalog_operation_phase::imported);
        return *State.Operation;
    }

    [[nodiscard]] auto record_candidate(
        const application::record_candidate_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::append_operation_record> override
    {
        State.Calls.emplace_back("catalog.commit-authorized");
        if(stop.stop_requested() || !State.Operation)
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        State.Operation->Candidate = request.Candidate;
        advance(application::epoch_catalog_operation_phase::commit_authorized);
        if(State.AfterCommitAuthorized)
        {
            auto callback = std::move(State.AfterCommitAuthorized);
            callback();
        }
        return *State.Operation;
    }

    [[nodiscard]] auto transition_operation(
        const application::operation_transition_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::epoch_recovery_operation> override
    {
        const auto call = "catalog.transition:" + phase_name(request.Next);
        State.Calls.push_back(call);
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::epoch_catalog_failure::cancelled};
        }
        if(const auto failure =
                take_failure(State.MaintenanceCatalogFailures, call))
        {
            return std::unexpected{*failure};
        }
        if(request.Kind != application::epoch_catalog_operation_kind::append)
        {
            if(!State.Maintenance)
            {
                return std::unexpected{
                    application::epoch_catalog_failure::phase_conflict};
            }
            expect(State.Maintenance->Phase == request.Expected
                    && State.Maintenance->Operation == request.Operation
                    && State.Maintenance->OperationRevision
                        == request.ExpectedOperationRevision,
                "catalog fake observes exact maintenance phase identity");
            if(request.Next
                == application::epoch_catalog_operation_phase::repair_required)
            {
                State.Maintenance->RepairFromPhase = State.Maintenance->Phase;
            }
            State.Maintenance->Phase = request.Next;
            State.Maintenance->OperationRevision =
                application::epoch_catalog_operation_revision{
                    State.Maintenance->OperationRevision.value() + 1};
            if(request.Next
                    == application::epoch_catalog_operation_phase::
                        ref_delete_in_progress
                || request.Next
                    == application::epoch_catalog_operation_phase::
                        quarantine_rename_in_progress)
            {
                if(State.AfterRefCommitInProgress)
                {
                    auto callback = std::move(State.AfterRefCommitInProgress);
                    callback();
                }
            }
            return application::epoch_recovery_operation{*State.Maintenance};
        }
        if(!State.Operation)
        {
            return std::unexpected{
                application::epoch_catalog_failure::phase_conflict};
        }
        if(State.Operation->Phase != request.Expected)
        {
            std::cerr << "catalog phase mismatch: actual="
                      << static_cast<int>(State.Operation->Phase)
                      << " expected=" << static_cast<int>(request.Expected)
                      << " next=" << static_cast<int>(request.Next) << '\n';
            expect(
                false,
                "catalog fake observes exact expected operation phase");
        }
        if(
            request.Next
            == application::epoch_catalog_operation_phase::repair_required)
        {
            State.Operation->RepairFromPhase = State.Operation->Phase;
        }
        advance(request.Next);
        if(
            request.Next
                == application::epoch_catalog_operation_phase::
                    ref_commit_in_progress
            && State.AfterRefCommitInProgress)
        {
            auto callback = std::move(State.AfterRefCommitInProgress);
            callback();
        }
        if(
            request.Next == application::epoch_catalog_operation_phase::aborted
            || request.Next
                == application::epoch_catalog_operation_phase::complete)
        {
            State.Operation->RecoveryInputCompacted = true;
            State.Operation->Spool.reset();
            State.Operation->Preimport.reset();
        }
        return application::epoch_recovery_operation{*State.Operation};
    }

    [[nodiscard]] auto finalize_append(
        const application::finalize_append_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::catalog_commit_result> override
    {
        State.Calls.emplace_back("catalog.finalize");
        if(stop.stop_requested() || !State.Operation)
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        advance(
            application::epoch_catalog_operation_phase::
                visible_cleanup_pending);
        const auto next = core::epoch_catalog_identity{
            .Incarnation = request.Catalog.Incarnation,
            .Revision = core::epoch_catalog_revision{
                request.Catalog.Revision.value() + 1},
        };
        State.Binding.Catalog = next;
        State.Snapshot.Binding.Catalog = next;
        State.Snapshot.LatestLive = request.Mutation.Epoch;
        State.Snapshot.NextOrdinal = core::epoch_ordinal{
            request.Mutation.Epoch.Id.Ordinal.value() + 1};
        return application::catalog_commit_result{
            .Catalog = next,
            .Changed = true,
            .InsertedEpoch = request.Mutation.Epoch.Id,
        };
    }

    [[nodiscard]] auto associate_review(
        const application::associate_review_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::catalog_commit_result> override
    {
        State.Calls.emplace_back("catalog.associate-review");
        if(stop.stop_requested())
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        const auto next = core::epoch_catalog_identity{
            .Incarnation = request.Catalog.Incarnation,
            .Revision = core::epoch_catalog_revision{
                request.Catalog.Revision.value() + 1},
        };
        State.Binding.Catalog = next;
        State.Snapshot.Binding.Catalog = next;
        State.Snapshot.CurrentViewerReview = request.Review;
        if(State.AssociationAppliesThenFails)
        {
            State.AssociationAppliesThenFails = false;
            return std::unexpected{
                application::epoch_catalog_failure::unavailable};
        }
        return application::catalog_commit_result{
            .Catalog = next,
            .Changed = true,
        };
    }

    [[nodiscard]] auto begin_prune(
        const application::begin_prune_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::maintenance_operation_record> override
    {
        State.Calls.emplace_back("catalog.begin-prune");
        State.BeginPruneRequests.push_back(request);
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::epoch_catalog_failure::cancelled};
        }
        if(const auto failure = take_failure(State.MaintenanceCatalogFailures,
               "catalog.begin-prune"))
        {
            return std::unexpected{*failure};
        }
        State.Maintenance = application::maintenance_operation_record{
            .Incarnation = request.Catalog.Incarnation,
            .Operation = request.Operation,
            .Kind = application::epoch_catalog_operation_kind::prune,
            .Phase = application::epoch_catalog_operation_phase::prune_intent,
            .OperationRevision =
                application::epoch_catalog_operation_revision{1},
            .Cache = State.Binding.Authority.Cache,
            .Epochs = request.Epochs,
        };
        return *State.Maintenance;
    }

    [[nodiscard]] auto finalize_prune(
        const application::finalize_prune_request& request,
        const std::stop_token)
        -> application::epoch_catalog_result<
            application::catalog_commit_result> override
    {
        State.Calls.emplace_back("catalog.finalize-prune");
        if(const auto failure = take_failure(State.MaintenanceCatalogFailures,
               "catalog.finalize-prune"))
        {
            return std::unexpected{*failure};
        }
        if(!State.Maintenance)
        {
            return std::unexpected{
                application::epoch_catalog_failure::phase_conflict};
        }
        expect(State.Maintenance->Phase == request.Transition.Expected
                && State.Maintenance->Operation == request.Transition.Operation,
            "prune finalization matches the sealed operation");
        State.Maintenance->Phase = request.Transition.Next;
        State.Maintenance->OperationRevision =
            application::epoch_catalog_operation_revision{
                State.Maintenance->OperationRevision.value() + 1};
        const auto next = core::epoch_catalog_identity{
            .Incarnation = State.Binding.Catalog.Incarnation,
            .Revision =
                core::epoch_catalog_revision{
                    State.Binding.Catalog.Revision.value() + 1},
        };
        State.Binding.Catalog = next;
        State.Snapshot.Binding.Catalog = next;
        if(State.Snapshot.LatestLive)
        {
            State.Snapshot.LatestLive->BaseAvailability =
                core::anchor_availability::pruned;
            State.Snapshot.LatestLive->HeadAvailability =
                core::anchor_availability::pruned;
        }
        return application::catalog_commit_result{
            .Catalog = next,
            .Changed = true,
        };
    }

    [[nodiscard]] auto begin_cache_delete(
        const application::begin_cache_delete_request& request,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::maintenance_operation_record> override
    {
        State.Calls.emplace_back("catalog.begin-cache-delete");
        State.BeginCacheDeleteRequests.push_back(request);
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::epoch_catalog_failure::cancelled};
        }
        if(const auto failure = take_failure(State.MaintenanceCatalogFailures,
               "catalog.begin-cache-delete"))
        {
            return std::unexpected{*failure};
        }
        State.Maintenance = application::maintenance_operation_record{
            .Incarnation = request.Impact.Incarnation,
            .Operation = request.Operation,
            .Kind = application::epoch_catalog_operation_kind::cache_delete,
            .Phase = application::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
            .OperationRevision =
                application::epoch_catalog_operation_revision{1},
            .Cache = request.Impact.Cache,
            .Scopes = request.Impact.Scopes,
        };
        return *State.Maintenance;
    }

    [[nodiscard]] auto cache_delete_impact(
        const core::repository_cache_scope_id cache,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::cache_delete_impact> override
    {
        State.Calls.emplace_back("catalog.cache-delete-impact");
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::epoch_catalog_failure::cancelled};
        }
        if(const auto failure = take_failure(State.MaintenanceCatalogFailures,
               "catalog.cache-delete-impact"))
        {
            return std::unexpected{*failure};
        }
        return application::cache_delete_impact{
            .Incarnation = State.Binding.Catalog.Incarnation,
            .Cache = cache,
            .Scopes =
                {
                    application::cache_delete_scope_fact{
                        .Authority = State.Binding.Authority.Scope,
                        .ExpectedRevision = State.Binding.Catalog.Revision,
                    },
                },
            .RetainedEpochCount = State.Snapshot.LatestLive ? 1U : 0U,
        };
    }

    [[nodiscard]] auto finalize_cache_delete(
        const application::finalize_cache_delete_request& request,
        const std::stop_token)
        -> application::epoch_catalog_result<void> override
    {
        State.Calls.emplace_back("catalog.finalize-cache-delete");
        if(const auto failure = take_failure(State.MaintenanceCatalogFailures,
               "catalog.finalize-cache-delete"))
        {
            return std::unexpected{*failure};
        }
        if(!State.Maintenance)
        {
            return std::unexpected{
                application::epoch_catalog_failure::phase_conflict};
        }
        expect(State.Maintenance->Phase == request.Transition.Expected
                && State.Maintenance->Operation == request.Transition.Operation,
            "cache-delete finalization matches the sealed operation");
        if(State.CacheDeleteFinalizesWithoutChange)
        {
            State.CacheDeleteFinalizesWithoutChange = false;
            return {};
        }
        State.Maintenance->Phase = request.Transition.Next;
        State.Maintenance->OperationRevision =
            application::epoch_catalog_operation_revision{
                State.Maintenance->OperationRevision.value() + 1};
        State.Binding.Catalog.Revision = core::epoch_catalog_revision{
            State.Binding.Catalog.Revision.value() + 1};
        State.Snapshot.Binding.Catalog = State.Binding.Catalog;
        if(State.Snapshot.LatestLive)
        {
            State.Snapshot.LatestLive->BaseAvailability =
                core::anchor_availability::cache_deleted;
            State.Snapshot.LatestLive->HeadAvailability =
                core::anchor_availability::cache_deleted;
        }
        return {};
    }

    [[nodiscard]] auto recovery_page(
        const std::optional<core::epoch_operation_id> after,
        const std::uint16_t,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            application::epoch_recovery_page> override
    {
        State.Calls.emplace_back("catalog.recovery-page");
        State.RecoveryPageCursors.push_back(after);
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::epoch_catalog_failure::cancelled};
        }
        if(State.RecoveryPages.empty())
        {
            return application::epoch_recovery_page{
                .Incarnation = State.Binding.Catalog.Incarnation,
            };
        }
        auto page = std::move(State.RecoveryPages.front());
        State.RecoveryPages.pop_front();
        return page;
    }

private:
    [[nodiscard]] static auto phase_name(
        const application::epoch_catalog_operation_phase phase)
        -> std::string
    {
        using enum application::epoch_catalog_operation_phase;
        switch(phase)
        {
        case prune_intent:
            return "prune-intent";
        case ref_delete_in_progress:
            return "ref-delete";
        case cache_delete_intent_sealed:
            return "cache-delete-sealed";
        case quarantine_rename_in_progress:
            return "quarantine-rename";
        case quarantined:
            return "quarantined";
        case catalog_tombstoned_cleanup_pending:
            return "catalog-tombstoned";
        case staging_ready:
            return "staging-ready";
        case ref_commit_in_progress:
            return "ponr";
        case refs_committed:
            return "refs-committed";
        case visible_cleanup_pending:
            return "visible";
        case complete:
            return "cleanup";
        case abort_requested:
            return "abort-requested";
        case aborted:
            return "aborted";
        case repair_required:
            return "repair-required";
        default:
            return "other";
        }
    }

    void advance(const application::epoch_catalog_operation_phase next)
    {
        expect(State.Operation.has_value(), "catalog operation exists");
        State.Operation->Phase = next;
        State.Operation->OperationRevision =
            application::epoch_catalog_operation_revision{
                State.Operation->OperationRevision.value() + 1};
    }

    shared_state& State;
};

class fake_catalog_port final : public application::epoch_catalog_port
{
public:
    explicit fake_catalog_port(shared_state& state) noexcept : State{state}
    {
    }

    [[nodiscard]] auto open(
        const application::epoch_catalog_open_request&,
        const std::stop_token stop)
        -> application::epoch_catalog_result<
            std::unique_ptr<application::epoch_catalog_session>> override
    {
        State.Calls.emplace_back("catalog.open");
        if(stop.stop_requested())
        {
            return std::unexpected{application::epoch_catalog_failure::cancelled};
        }
        if(!State.CatalogOpenFailures.empty())
        {
            const auto failure = State.CatalogOpenFailures.front();
            State.CatalogOpenFailures.pop_front();
            if(failure)
            {
                return std::unexpected{*failure};
            }
        }
        if(!State.CatalogOpenSnapshots.empty())
        {
            State.Snapshot =
                std::move(State.CatalogOpenSnapshots.front());
            State.CatalogOpenSnapshots.pop_front();
            State.Binding = State.Snapshot.Binding;
        }
        return std::make_unique<fake_catalog_session>(State);
    }

private:
    shared_state& State;
};

class fake_pack_sink final : public application::git_pack_stream_sink
{
public:
    fake_pack_sink(shared_state& state, const core::epoch_operation_id operation)
        : State{state}, Operation{operation}
    {
    }

    [[nodiscard]] auto append(
        const std::span<const std::byte> input,
        const std::stop_token stop)
        -> application::git_pack_stream_result<void> override
    {
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_pack_stream_failure::cancelled};
        }
        Received.insert(Received.end(), input.begin(), input.end());
        return {};
    }

    [[nodiscard]] auto finish(const std::stop_token stop)
        -> application::git_pack_stream_result<
            application::completed_git_pack_receipt> override
    {
        State.Calls.emplace_back("cache.sink-finish");
        if(stop.stop_requested())
        {
            return std::unexpected{
                application::git_pack_stream_failure::cancelled};
        }
        const auto pack = pack_oid(0x42U);
        auto token = std::array<std::uint8_t, 32U>{};
        token.front() = 0x71U;
        return application::completed_git_pack_receipt{
            .Operation = Operation,
            .ExpectedPack = pack,
            .ObservedPack = pack,
            .Spool = artifact(
                application::git_artifact_role::completed_spool,
                0x51U,
                Received.size()),
            .OpaqueToken = token,
        };
    }

    void abort() noexcept override
    {
        State.Calls.emplace_back("cache.sink-abort");
    }

private:
    shared_state& State;
    core::epoch_operation_id Operation;
    std::vector<std::byte> Received;
};

class fake_cache_session final : public application::git_cache_session
{
public:
    fake_cache_session(
        shared_state& state,
        application::git_cache_open_request request)
        : State{state}, Request{std::move(request)}
    {
        ++State.ActiveCacheSessions;
        State.MaximumActiveCacheSessions = std::max(
            State.MaximumActiveCacheSessions,
            State.ActiveCacheSessions);
        State.Calls.emplace_back(
            "cache.session-open:"
            + std::to_string(Request.Authority.PullRequest.value()));
    }

    ~fake_cache_session() override
    {
        State.Calls.emplace_back(
            "cache.session-close:"
            + std::to_string(Request.Authority.PullRequest.value()));
        expect(
            State.ActiveCacheSessions != 0U,
            "cache session accounting cannot underflow");
        --State.ActiveCacheSessions;
    }

    [[nodiscard]] auto request() const noexcept
        -> const application::git_cache_open_request& override
    {
        return Request;
    }

    [[nodiscard]] auto create_pack_sink(
        const core::epoch_operation_id operation,
        const std::uint64_t,
        const std::stop_token stop)
        -> application::git_cache_result<
            std::unique_ptr<application::git_pack_stream_sink>> override
    {
        State.Calls.emplace_back("cache.create-sink");
        ++State.CacheMutationCalls;
        if(stop.stop_requested())
        {
            return std::unexpected{application::git_cache_failure::cancelled};
        }
        return std::make_unique<fake_pack_sink>(State, operation);
    }

    [[nodiscard]] auto observe_preimport(
        const application::completed_git_pack_receipt& receipt,
        const std::stop_token stop)
        -> application::git_cache_result<
            application::git_preimport_observation> override
    {
        State.Calls.emplace_back("cache.observe-preimport");
        if(stop.stop_requested())
        {
            return std::unexpected{application::git_cache_failure::cancelled};
        }
        auto token = std::array<std::uint8_t, 32U>{};
        token.front() = 0x72U;
        return application::git_preimport_observation{
            .Operation = receipt.Operation,
            .ExpectedPack = receipt.ExpectedPack,
            .RepositoryBytes = 0U,
            .ExpectedPackPreexisting = false,
            .OpaqueToken = token,
        };
    }

    [[nodiscard]] auto prepare_revision(
        const application::git_prepare_revision_request& request,
        const std::stop_token stop)
        -> application::git_cache_result<
            application::git_prepared_revision> override
    {
        State.Calls.emplace_back("cache.prepare-revision");
        ++State.CacheMutationCalls;
        if(stop.stop_requested())
        {
            return std::unexpected{application::git_cache_failure::cancelled};
        }
        return application::git_prepared_revision{
            .Operation = request.Authorization.Operation,
            .Pack = request.Authorization.ExpectedPack,
            .InstalledPack = artifact(
                application::git_artifact_role::installed_pack,
                0x52U,
                8U),
            .InstalledIndex = artifact(
                application::git_artifact_role::installed_index,
                0x53U,
                4U),
            .Base = request.Base,
            .BaseTree = oid<core::git_tree_oid>(60U),
            .Head = request.Head,
            .HeadTree = oid<core::git_tree_oid>(70U),
            .HeadLineage = core::head_lineage::not_applicable,
            .ImportedBytes = 8U,
        };
    }

    [[nodiscard]] auto promote_retained(
        const application::git_prepared_revision&,
        const application::git_ref_promotion_authorization& authorization,
        const std::stop_token) -> application::git_cache_result<
            application::git_retained_ref_pair> override
    {
        State.Calls.emplace_back("cache.promote");
        ++State.CacheMutationCalls;
        State.Retained = application::git_retained_ref_pair{
            .Scope = authorization.Scope,
            .Ordinal = authorization.Ordinal,
            .Base = authorization.Base,
            .Head = authorization.Head,
        };
        if(State.PromoteAppliesThenFails)
        {
            State.PromoteAppliesThenFails = false;
            return std::unexpected{
                application::git_cache_failure::process_failure};
        }
        return *State.Retained;
    }

    [[nodiscard]] auto inspect_retained(
        const core::epoch_ordinal,
        const std::stop_token) -> application::git_cache_result<
            application::git_retained_ref_pair> override
    {
        State.Calls.emplace_back("cache.inspect-retained");
        if(!State.Retained)
        {
            return std::unexpected{application::git_cache_failure::ref_conflict};
        }
        return *State.Retained;
    }

    [[nodiscard]] auto inspect_commit_object(
        const core::git_commit_oid commit,
        const std::stop_token) -> application::git_cache_result<
            application::git_cache_commit_inspection> override
    {
        State.Calls.emplace_back("cache.inspect-commit");
        return application::git_cache_commit_inspection{
            .Commit = commit,
            .Tree = oid<core::git_tree_oid>(80U),
        };
    }

    [[nodiscard]] auto prune_retained(
        const application::git_ref_prune_authorization& authorization,
        const std::stop_token) -> application::git_cache_result<void> override
    {
        State.Calls.emplace_back("cache.prune");
        State.PruneRequests.push_back(authorization);
        ++State.CacheMutationCalls;
        if(const auto failure =
                take_failure(State.MaintenanceCacheFailures, "cache.prune"))
        {
            return std::unexpected{*failure};
        }
        State.Retained.reset();
        return {};
    }

    [[nodiscard]] auto abort_operation(
        const core::epoch_operation_id,
        const std::stop_token) -> application::git_cache_result<void> override
    {
        State.Calls.emplace_back("cache.abort");
        ++State.CacheMutationCalls;
        return {};
    }

    [[nodiscard]] auto inspect_operation_recovery(
        const application::git_operation_recovery_request& request,
        const std::stop_token) -> application::git_cache_result<
            application::git_operation_recovery_observation> override
    {
        State.Calls.emplace_back("cache.inspect-recovery");
        State.RecoveryInspectionRequests.push_back(request);
        if(State.RecoveryInspections.empty())
        {
            return std::unexpected{
                application::git_cache_failure::invalid_request};
        }
        auto result = std::move(State.RecoveryInspections.front());
        State.RecoveryInspections.pop_front();
        return result;
    }

    [[nodiscard]] auto abort_recovered_operation(
        const application::git_operation_recovery_request& request,
        const std::stop_token) -> application::git_cache_result<void> override
    {
        State.Calls.emplace_back("cache.abort-exact");
        State.RecoveryAbortRequests.push_back(request);
        ++State.CacheMutationCalls;
        if(State.ThrowRecoveryAbort)
        {
            throw std::runtime_error{"injected exact-abort failure"};
        }
        return {};
    }

    [[nodiscard]] auto complete_recovered_operation(
        const application::git_operation_recovery_request& request,
        const std::stop_token) -> application::git_cache_result<void> override
    {
        State.Calls.emplace_back("cache.complete-operation");
        State.RecoveryCompletionRequests.push_back(request);
        return {};
    }

private:
    shared_state& State;
    application::git_cache_open_request Request;
};

class fake_cache_port final : public application::git_cache_port
{
public:
    explicit fake_cache_port(shared_state& state) noexcept : State{state}
    {
    }

    [[nodiscard]] auto open(
        const application::git_cache_open_request& request,
        const std::stop_token stop) -> application::git_cache_result<
            std::unique_ptr<application::git_cache_session>> override
    {
        State.Calls.emplace_back("cache.open");
        State.CacheOpenRequests.push_back(request);
        if(stop.stop_requested())
        {
            return std::unexpected{application::git_cache_failure::cancelled};
        }
        return std::make_unique<fake_cache_session>(State, request);
    }

    [[nodiscard]] auto delete_cache(const application::git_cache_open_request&,
        const application::git_cache_delete_authorization& authorization,
        const std::stop_token) -> application::git_cache_result<void> override
    {
        State.Calls.emplace_back("cache.delete");
        State.DeleteRequests.push_back(authorization);
        ++State.CacheMutationCalls;
        expect(State.ActiveCacheSessions == 0U,
            "whole-cache deletion closes the live session before quarantine");
        if(const auto failure =
                take_failure(State.MaintenanceCacheFailures, "cache.delete"))
        {
            return std::unexpected{*failure};
        }
        return {};
    }

private:
    shared_state& State;
};

class fake_transport final : public application::git_smart_transport_port
{
public:
    fake_transport(
        shared_state& state,
        application::git_smart_request_context context)
        : State{state}, Context{std::move(context)}
    {
    }

    void revoke() noexcept override
    {
        Revoked = true;
    }

    [[nodiscard]] auto revoked() const noexcept -> bool override
    {
        return Revoked;
    }

    [[nodiscard]] auto discover(
        const application::git_smart_discovery_request& request,
        const std::stop_token stop)
        -> application::git_smart_transport_result<
            application::git_smart_discovery_response> override
    {
        State.Calls.emplace_back("transport.discover");
        ++State.TransportCalls;
        if(stop.stop_requested() || request.Context != Context)
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::cancelled,
            }};
        }
        return application::git_smart_discovery_response{
            .Body = bytes(
                packet("version 2\n") + packet("ls-refs=unborn\n")
                + packet("fetch=shallow wait-for-done\n")
                + packet("object-format=sha1\n")
                + packet("server-option\n") + "0000"),
        };
    }

    [[nodiscard]] auto ls_refs(
        const application::git_smart_ls_refs_request& request,
        const std::stop_token stop)
        -> application::git_smart_transport_result<
            application::git_smart_ls_refs_response> override
    {
        State.Calls.emplace_back("transport.ls-refs");
        ++State.TransportCalls;
        if(stop.stop_requested() || request.Context != Context)
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::cancelled,
            }};
        }
        const auto base =
            std::string{core::canonical_hex(Context.Revision.Base).view()};
        const auto head =
            std::string{core::canonical_hex(Context.Revision.Head).view()};
        return application::git_smart_ls_refs_response{
            .Body = bytes(
                packet(base + " refs/heads/main\n")
                + packet(
                    head + " refs/pull/"
                    + std::to_string(Context.Authority.Number.value())
                    + "/head\n")
                + "0000"),
        };
    }

    [[nodiscard]] auto fetch(
        const application::git_smart_fetch_request& request,
        application::git_smart_wire_consumer& consumer,
        const std::stop_token stop)
        -> application::git_smart_transport_result<void> override
    {
        State.Calls.emplace_back("transport.fetch");
        ++State.TransportCalls;
        if(stop.stop_requested() || request.Context != Context)
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::cancelled,
            }};
        }
        auto pack_data = std::string(1U, static_cast<char>(1));
        pack_data += "PACKdata";
        const auto body = bytes(
            packet("packfile\n") + packet(pack_data) + "0000");
        if(!consumer.consume(body))
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::
                    consumer_rejected,
            }};
        }
        return {};
    }

private:
    shared_state& State;
    application::git_smart_request_context Context;
    bool Revoked = false;
};

class fake_smart_authority final
    : public application::git_smart_authority_lease
{
public:
    fake_smart_authority(
        shared_state& state,
        application::git_smart_authority_binding binding,
        const std::uint64_t label)
        : State{state}, Binding{std::move(binding)}, Label{label}
    {
    }

    [[nodiscard]] auto binding() const noexcept
        -> const application::git_smart_authority_binding& override
    {
        return Binding;
    }

    [[nodiscard]] auto acquire(
        const application::git_smart_request_context& context)
        -> application::git_smart_transport_result<
            application::git_smart_transport> override
    {
        State.Calls.emplace_back("transport.acquire");
        if(Revoked)
        {
            return std::unexpected{application::git_smart_transport_error{
                .Failure = application::git_smart_transport_failure::cancelled,
            }};
        }
        return std::make_unique<fake_transport>(State, context);
    }

    void revoke() noexcept override
    {
        if(Revoked)
        {
            return;
        }
        Revoked = true;
        State.AuthorityRevocations.push_back(Label);
        State.Calls.emplace_back("transport.revoke-authority");
    }

    [[nodiscard]] auto revoked() const noexcept -> bool override
    {
        return Revoked;
    }

private:
    shared_state& State;
    application::git_smart_authority_binding Binding;
    std::uint64_t Label = 0U;
    bool Revoked = false;
};

[[nodiscard]] auto make_handoff(
    shared_state& state,
    const std::uint64_t snapshot = 1U,
    const std::uint8_t head = 20U,
    const bool smart = false,
    std::optional<core::current_viewer_review_candidate> review = std::nullopt)
    -> application::accepted_pull_request_handoff
{
    const auto base_ref =
        application::parse_git_base_branch_ref("refs/heads/main");
    expect(base_ref.has_value(), "fixture base ref is valid");
    auto handoff = application::accepted_pull_request_handoff{
        .ApplicationSession = core::application_session_id{91U},
        .Profile = core::host_profile_id{11U},
        .ProfileRevision = core::github_profile_revision{21U},
        .Host = core::git_host_id{31U},
        .HostAuthority = opaque<core::host_authority_fingerprint>(2U),
        .Hostname = application::github_canonical_hostname{"github.com"},
        .Account = core::github_account_id{41U},
        .Viewer = core::github_viewer_actor_id{51U},
        .Repository = core::repository_id{61U},
        .Locator = {.Owner = "octo-org", .Name = "review-repository"},
        .PullRequest = core::pull_request_id{71U},
        .Number = core::pull_request_number{81U},
        .Snapshot = core::provider_snapshot_id{snapshot},
        .IdentityGeneration = core::github_identity_generation{3U},
        .PullRequestGeneration = core::request_generation{snapshot + 3U},
        .Origin = core::pull_request_revision_origin::live,
        .ObjectFormat = core::git_hash_algorithm::sha1,
        .BaseBranch = opaque<core::base_branch_id>(30U),
        .BaseRef = *base_ref,
        .Base = oid<core::git_commit_oid>(10U),
        .Head = oid<core::git_commit_oid>(head),
        .CurrentViewerReview = std::move(review),
    };
    if(smart)
    {
        handoff.SmartGit = std::make_unique<fake_smart_authority>(
            state, application::authority_binding(handoff), snapshot);
    }
    return handoff;
}

[[nodiscard]] auto recovery_record(
    shared_state& state,
    const application::epoch_catalog_operation_phase phase,
    const std::uint8_t operation_seed = 0x31U)
    -> application::append_operation_record
{
    const auto handoff = make_handoff(state, 1U, 20U);
    const auto operation = opaque<core::epoch_operation_id>(operation_seed);
    const auto revision = core::accepted_pull_request_revision{
        .Source =
            {
                .Observation =
                    opaque<core::epoch_source_observation_id>(
                        static_cast<std::uint8_t>(operation_seed + 1U)),
                .Snapshot = handoff.Snapshot,
                .IdentityGeneration = handoff.IdentityGeneration,
                .PullRequestGeneration = handoff.PullRequestGeneration,
            },
        .Origin = handoff.Origin,
        .BaseBranch = handoff.BaseBranch,
        .Base = handoff.Base,
        .Head = handoff.Head,
    };
    const auto pack = pack_oid(0x42U);
    auto record = application::append_operation_record{
        .Context =
            {
                .ApplicationSession = handoff.ApplicationSession,
                .Request = core::epoch_request_id{101U},
                .Generation = core::epoch_generation{1U},
                .Operation = operation,
                .Revision = revision,
                .Catalog = state.Binding.Catalog,
            },
        .ReservedOrdinal = core::epoch_ordinal{1U},
        .OperationRevision =
            application::epoch_catalog_operation_revision{5},
        .Phase = phase,
        .StagingBase =
            {
                .Authority = state.Binding.Authority.Scope,
                .Ordinal = core::epoch_ordinal{1U},
                .Operation = operation,
                .Staging = true,
            },
        .StagingHead =
            {
                .Authority = state.Binding.Authority.Scope,
                .Ordinal = core::epoch_ordinal{1U},
                .Operation = operation,
                .Staging = true,
                .Head = true,
            },
        .FinalBase =
            {
                .Authority = state.Binding.Authority.Scope,
                .Ordinal = core::epoch_ordinal{1U},
            },
        .FinalHead =
            {
                .Authority = state.Binding.Authority.Scope,
                .Ordinal = core::epoch_ordinal{1U},
                .Head = true,
            },
        .Spool = application::durable_git_spool_fact{
            .Operation = operation,
            .ExpectedPack = pack,
            .ObservedPack = pack,
            .Spool = artifact(
                application::git_artifact_role::completed_spool,
                0x51U,
                8U),
        },
        .Preimport = application::durable_git_preimport_fact{
            .Operation = operation,
            .ExpectedPack = pack,
            .RepositoryBytes = 0U,
            .ExpectedPackPreexisting = false,
        },
        .Install = application::durable_git_install_fact{
            .Operation = operation,
            .Pack = pack,
            .InstalledPack = artifact(
                application::git_artifact_role::installed_pack,
                0x52U,
                8U),
            .InstalledIndex = artifact(
                application::git_artifact_role::installed_index,
                0x53U,
                4U),
            .Base = revision.Base,
            .BaseTree = oid<core::git_tree_oid>(60U),
            .Head = revision.Head,
            .HeadTree = oid<core::git_tree_oid>(70U),
            .HeadLineage = core::head_lineage::not_applicable,
        },
    };

    using enum application::epoch_catalog_operation_phase;
    if(
        phase == commit_authorized || phase == ref_commit_in_progress
        || phase == refs_committed || phase == visible_cleanup_pending)
    {
        record.Candidate = core::append_epoch_mutation{
            .Epoch = summary_for(
                revision, state.Binding.Catalog, record.ReservedOrdinal),
        };
    }
    expect(application::valid(record), "recovery operation fixture is valid");
    return record;
}

[[nodiscard]] auto recovery_observation(
    const application::append_operation_record& record,
    const application::git_recovery_ref_topology refs,
    const application::git_recovery_match staging,
    const application::git_recovery_match final)
    -> application::git_operation_recovery_observation
{
    return {
        .Operation = record.Context.Operation,
        .Spool = application::git_recovery_match::exact,
        .Installed = application::git_recovery_match::exact,
        .Staging = staging,
        .Final = final,
        .Objects = application::git_recovery_object_match::exact,
        .Refs = refs,
    };
}

void seed_recovery(
    shared_state& state,
    const application::append_operation_record& record)
{
    state.Operation = record;
    state.RecoveryPages.push_back(application::epoch_recovery_page{
        .Incarnation = state.Binding.Catalog.Incarnation,
        .Operations = {record},
    });
}

[[nodiscard]] auto maintenance_recovery_record(shared_state& state,
    const application::epoch_catalog_operation_kind kind,
    const application::epoch_catalog_operation_phase phase,
    const std::uint8_t operation_seed = 0x61U)
    -> application::maintenance_operation_record
{
    auto record = application::maintenance_operation_record{
        .Incarnation = state.Binding.Catalog.Incarnation,
        .Operation = opaque<core::epoch_operation_id>(operation_seed),
        .Kind = kind,
        .Phase = phase,
        .OperationRevision = application::epoch_catalog_operation_revision{2},
        .Cache = state.Binding.Authority.Cache,
    };
    if(kind == application::epoch_catalog_operation_kind::prune)
    {
        const auto epoch = core::review_epoch_id{
            .Authority = state.Binding.Authority.Scope,
            .Incarnation = state.Binding.Catalog.Incarnation,
            .Ordinal = core::epoch_ordinal{1},
        };
        record.Epochs.push_back(application::prune_epoch_fact{
            .Epoch = epoch,
            .Retained =
                application::git_retained_ref_pair{
                    .Scope = epoch.Authority,
                    .Ordinal = epoch.Ordinal,
                    .Base = oid<core::git_commit_oid>(10U),
                    .Head = oid<core::git_commit_oid>(20U),
                },
            .BaseRef =
                application::epoch_catalog_ref_identity{
                    .Authority = epoch.Authority,
                    .Ordinal = epoch.Ordinal,
                },
            .HeadRef =
                application::epoch_catalog_ref_identity{
                    .Authority = epoch.Authority,
                    .Ordinal = epoch.Ordinal,
                    .Head = true,
                },
        });
    }
    else
    {
        record.Scopes.push_back(application::cache_delete_scope_fact{
            .Authority = state.Binding.Authority.Scope,
            .ExpectedRevision = state.Binding.Catalog.Revision,
        });
    }
    return record;
}

void seed_maintenance_recovery(shared_state& state,
    const application::maintenance_operation_record& record)
{
    state.Maintenance = record;
    state.RecoveryPages.push_back(application::epoch_recovery_page{
        .Incarnation = state.Binding.Catalog.Incarnation,
        .Operations = {record},
    });
}

struct fixture
{
    shared_state State;
    fake_cache_port Caches{State};
    fake_catalog_port Catalogs{State};
    fake_entropy Entropy{State};
    fake_clock Clock{State};
    queued_executor Effects{false};
    queued_executor Decisions{true};
    fake_projection Projections{State, Decisions};
    std::unique_ptr<application::epoch_application_host> Host;

    fixture()
    {
        State.Binding = application::epoch_catalog_binding{
            .Authority = authority(),
            .Catalog = catalog_identity(),
            .ObjectFormat = core::git_hash_algorithm::sha1,
        };
        State.Snapshot = application::epoch_catalog_snapshot{
            .Binding = State.Binding,
            .NextOrdinal = core::epoch_ordinal{1},
        };
        Host = std::make_unique<application::epoch_application_host>(
            core::epoch_state{
                .ApplicationSession = core::application_session_id{91U},
            },
            Caches,
            Catalogs,
            Entropy,
            Clock,
            Effects,
            Decisions,
            Projections);
    }

    ~fixture()
    {
        Host->shutdown();
        while(Effects.pending() != 0U || Decisions.pending() != 0U)
        {
            if(Effects.pending() != 0U)
            {
                Effects.run_next();
            }
            if(Decisions.pending() != 0U)
            {
                Decisions.run_next();
            }
        }
    }
};

void run_effect_completion(fixture& value, const std::size_t index = 0U)
{
    value.Effects.run(index);
    expect(
        value.Decisions.pending() != 0U,
        "each completed worker posts one decision-lane terminal");
    value.Decisions.run_next();
}

void drive_through_preparation(fixture& value)
{
    run_effect_completion(value);
    run_effect_completion(value);
    run_effect_completion(value);
}

void drain(fixture& value)
{
    while(value.Effects.pending() != 0U || value.Decisions.pending() != 0U)
    {
        if(value.Effects.pending() != 0U)
        {
            value.Effects.run_next();
        }
        if(value.Decisions.pending() != 0U)
        {
            value.Decisions.run_next();
        }
    }
}

void establish_ready_comparison(fixture& value)
{
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    run_effect_completion(value);
    drain(value);
    expect(
        value.Host->state().AcceptedRevision.has_value()
            && value.Host->state().AcceptedRevision->Source.Snapshot
                == core::provider_snapshot_id{1U}
            && value.Host->state().AcceptedComparison.has_value(),
        "fixture establishes one active accepted comparison");
}

[[nodiscard]] auto matching_invalidation()
    -> application::pull_request_authority_invalidation
{
    const auto owner = authority();
    return {
        .ApplicationSession = core::application_session_id{91U},
        .Profile = owner.Profile,
        .ProfileRevision = owner.ProfileRevision,
        .Host = owner.Host,
        .HostAuthority = owner.HostAuthority,
        .Account = owner.Account,
        .Viewer = owner.Viewer,
        .Repository = owner.Repository,
        .PullRequest = owner.PullRequest,
        .IdentityGeneration = core::github_identity_generation{3U},
    };
}

void expect_in_order(
    const std::vector<std::string>& calls,
    const std::vector<std::string_view>& expected)
{
    auto cursor = calls.cbegin();
    for(const auto item : expected)
    {
        cursor = std::find(cursor, calls.cend(), item);
        expect(cursor != calls.cend(), item);
        ++cursor;
    }
}

void invalid_handoff_has_no_effects()
{
    fixture value;
    auto invalid = make_handoff(value.State);
    invalid.Repository = {};
    value.Host->accept(std::move(invalid));
    expect(
        value.Effects.pending() == 0U && value.Decisions.pending() == 0U
            && value.State.Calls.empty(),
        "invalid handoff has no entropy, catalog, cache, or transport effect");
}

void source_collision_retries_before_core_acceptance()
{
    fixture value;
    value.State.SourceCollisions = {true, false};
    value.Host->accept(make_handoff(value.State));
    expect(!value.Host->state().Authority, "core has not seen worker input");
    value.Effects.run_next();
    expect(
        !value.Host->state().Authority && value.State.ProjectionCalls == 0U,
        "source collision probing finishes before the core terminal");
    value.Decisions.run_next();
    expect(
        value.Host->state().Authority == authority()
            && value.Host->state().AcceptedRevision.has_value()
            && value.State.ProjectionCalls != 0U,
        "the second fresh source reaches core only on the decision lane");
    expect(
        std::ranges::count(value.State.Calls, "entropy") == 2
            && std::ranges::count(
                   value.State.Calls, "catalog.source-probe")
                == 2,
        "one collision consumes one fresh replacement observation");
}

void successful_append_preserves_exact_order()
{
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    expect(
        value.Host->state().PendingIngestion.has_value()
            && value.Host->state().PendingIngestion->Phase
                == core::epoch_ingestion_phase::committing,
        "prepared append waits for exact core commit authorization");
    run_effect_completion(value);
    drain(value);

    expect(
        value.Host->state().Catalog == catalog_identity(1)
            && !value.Host->state().PendingIngestion,
        "durable append advances the visible catalog exactly once");
    expect_in_order(
        value.State.Calls,
        {
            "catalog.reserve",
            "cache.create-sink",
            "transport.acquire",
            "transport.discover",
            "transport.ls-refs",
            "transport.fetch",
            "cache.sink-finish",
            "cache.observe-preimport",
            "catalog.spool-recorded",
            "cache.prepare-revision",
            "catalog.imported",
            "catalog.transition:staging-ready",
            "catalog.commit-authorized",
            "catalog.transition:ponr",
            "cache.promote",
            "catalog.transition:refs-committed",
            "cache.inspect-retained",
            "catalog.finalize",
            "catalog.transition:cleanup",
        });
    expect(
        value.State.ProjectionOutsideDecision == 0U,
        "worker effects never publish or mutate core outside decision lane");
}

void projection_failure_is_contained_and_decision_progress_drains()
{
    fixture value;
    value.State.ProjectionFailuresRemaining = 1U;
    value.Host->accept(make_handoff(value.State));

    run_effect_completion(value);
    expect(
        value.Host->projection_delivery_failed()
            && value.State.ProjectionFailuresRemaining == 0U
            && value.Host->state().AcceptedRevision.has_value()
            && value.Effects.pending() == 1U,
        "a throwing projection is contained after core acceptance and cannot "
        "suppress the next host request");

    drain(value);
    expect(
        value.State.ProjectionCalls > 1U && !value.State.Updates.empty()
            && value.State.ProjectionOutsideDecision == 0U,
        "decision-lane completions and later projections continue after a "
        "contained projection exception");

    value.Decisions.post([&value]
    {
        value.Host->shutdown();
    });
    value.Decisions.run_next();
    drain(value);
    expect(
        value.Host->is_closing() && value.Host->is_quiescent()
            && value.State.ProjectionOutsideDecision == 0U,
        "a contained projection exception cannot prevent serialized shutdown "
        "quiescence");
}

void reentrant_projection_ingress_is_deferred_without_recursion()
{
    fixture value;
    value.State.ProjectionIngress = [&value]
    {
        value.Host->dispatch(core::cancel_epoch_refresh_intent{});
    };
    value.Host->accept(make_handoff(value.State));

    run_effect_completion(value);
    expect(
        value.State.ProjectionIngressCalls == 1U
            && value.State.MaximumProjectionDepth == 1U
            && value.State.ProjectionDepth == 0U
            && value.Effects.pending() == 1U,
        "projection feedback queues semantic ingress without recursive "
        "publication or suppressing the accepted host request");

    drain(value);
    expect(
        value.Host->state().AcceptedRevision.has_value()
            && !value.Host->state().PendingIngestion
            && value.Host->state().Status == core::epoch_status::cancelled
            && value.State.ProjectionIngressCalls == 1U
            && value.State.MaximumProjectionDepth == 1U
            && value.State.ProjectionOutsideDecision == 0U,
        "deferred projection ingress reaches one serialized terminal without "
        "corrupting accepted authority state");

    value.Decisions.post([&value]
    {
        value.Host->shutdown();
    });
    value.Decisions.run_next();
    drain(value);
    expect(
        value.Host->is_quiescent()
            && value.State.MaximumProjectionDepth == 1U
            && value.State.ProjectionOutsideDecision == 0U,
        "reentrant projection ingress leaves no recursive or deferred work at "
        "shutdown");
}

void failed_same_authority_offer_preserves_active_comparison()
{
    fixture value;
    establish_ready_comparison(value);
    const auto before = value.Host->state();
    const auto projections_before = value.State.Updates.size();
    value.State.CatalogOpenFailures.push_back(
        application::epoch_catalog_failure::unavailable);

    value.Host->accept(make_handoff(value.State, 2U, 21U, true));
    run_effect_completion(value);

    expect(
        value.Host->state() == before
            && value.Host->state().AcceptedComparison.has_value(),
        "failed same-authority offer preserves active revision and pair");
    expect(
        value.State.Updates.size() == projections_before + 1U
            && value.State.Updates.back().Transition.has_value()
            && !value.State.Updates.back().Transition->Diagnostics.empty()
            && value.State.Updates.back().Transition->Diagnostics.back().Code
                == core::epoch_reason_code::unavailable,
        "current failed replacement reports one bounded diagnostic");
    expect(value.State.AuthorityRevocations == std::vector<std::uint64_t>{2U},
        "failed replacement revokes only its offered lease and preserves the "
        "active lease");
}

void successful_same_authority_offer_swaps_on_exact_terminal()
{
    fixture value;
    establish_ready_comparison(value);
    const auto accepted_pair = value.Host->state().AcceptedComparison;

    value.Host->accept(make_handoff(value.State, 2U, 21U, true));
    value.Effects.run_next();
    expect(
        value.Host->state().AcceptedRevision->Source.Snapshot
                == core::provider_snapshot_id{1U}
            && value.Host->state().AcceptedComparison == accepted_pair
            && value.State.AuthorityRevocations.empty(),
        "worker result alone cannot swap or revoke the active context");

    value.Decisions.run_next();
    expect(
        value.Host->state().AcceptedRevision->Source.Snapshot
                == core::provider_snapshot_id{2U}
            && value.Host->state().AcceptedComparison == accepted_pair
            && value.State.AuthorityRevocations
                == std::vector<std::uint64_t>{1U},
        "exact offer token swaps on decision lane and then revokes prior lease");
}

void historical_origin_reaches_authority_projection()
{
    fixture value;
    auto historical = make_handoff(value.State, 7U);
    historical.Origin =
        core::pull_request_revision_origin::explicit_historical_import;
    value.Host->accept(std::move(historical));
    run_effect_completion(value);

    const auto projected = std::ranges::any_of(
        value.State.Updates,
        [](const application::epoch_presentation_update& update)
        {
            return update.Transition && update.Transition->Authority
                && update.Transition->Authority->Revision
                && update.Transition->Authority->Revision->Origin
                    == core::pull_request_revision_origin::
                        explicit_historical_import;
        });
    expect(
        projected,
        "historical origin survives source minting and authority projection");
}

void superseded_offer_failure_emits_no_diagnostic()
{
    fixture value;
    establish_ready_comparison(value);
    const auto projections_before = value.State.Updates.size();
    value.State.CatalogOpenFailures.push_back(
        application::epoch_catalog_failure::unavailable);

    value.Host->accept(make_handoff(value.State, 2U, 21U, true));
    value.Host->accept(make_handoff(value.State, 3U, 22U, true));
    value.Effects.run_next();
    value.Decisions.run_next();

    expect(
        value.State.Updates.size() == projections_before
            && value.Host->state().AcceptedRevision->Source.Snapshot
                == core::provider_snapshot_id{1U}
            && value.Host->state().AcceptedComparison.has_value(),
        "superseded failed offer cannot diagnose or disturb active state");
}

void matching_invalidation_cancels_pending_offer_and_active_context()
{
    fixture value;
    establish_ready_comparison(value);
    const auto opens_before =
        std::ranges::count(value.State.Calls, "catalog.open");

    value.Host->accept(make_handoff(value.State, 2U, 21U, true));
    expect(value.Effects.pending() != 0U, "same-identity offer is pending");
    value.Host->invalidate(matching_invalidation());
    expect(
        !value.Host->state().Authority
            && !value.Host->state().AcceptedRevision
            && !value.Host->state().AcceptedComparison
            && value.State.AuthorityRevocations
                == std::vector<std::uint64_t>{1U},
        "matching invalidation immediately clears and revokes active context");

    drain(value);
    expect(
        !value.Host->state().Authority
            && std::ranges::count(value.State.Calls, "catalog.open")
                == opens_before,
        "cancelled matching offer never opens or resurrects its context");
}

void bounded_history_pages_project_incrementally()
{
    fixture value;
    establish_ready_comparison(value);
    const auto catalog = *value.Host->state().Catalog;
    const auto revision = *value.Host->state().AcceptedRevision;
    const auto continuation =
        history_cursor_for(catalog, core::epoch_ordinal{2});
    value.State.HistoryPages.push_back(application::epoch_history_page{
        .Authority = authority().Scope,
        .Catalog = catalog,
        .Epochs =
            {
                summary_for(revision, catalog, core::epoch_ordinal{3}),
                summary_for(revision, catalog, core::epoch_ordinal{2}),
            },
        .Continuation = continuation,
    });

    const auto updates_before = value.State.Updates.size();
    value.Host->dispatch(core::request_epoch_history_page_intent{
        .Catalog = catalog,
        .Limit = 2U,
    });
    expect(
        value.Effects.pending() == 1U
            && value.State.Updates.size() == updates_before + 1U
            && value.State.Updates.back().Catalog
            && value.State.Updates.back().Catalog->HistoryPage
            && value.State.Updates.back().Catalog->HistoryPage->Phase
                == core::epoch_history_page_phase::loading
            && !value.State.Updates.back().Catalog->Replace,
        "one history intent projects loading and schedules one bounded "
        "catalog effect");

    value.Host->dispatch(core::request_epoch_history_page_intent{
        .Catalog = catalog,
        .Limit = 2U,
    });
    expect(
        value.Effects.pending() == 1U,
        "a duplicate history intent cannot schedule a second worker");

    run_effect_completion(value);
    expect(
        value.State.HistoryRequests
                == std::vector{
                    application::epoch_history_page_request{
                        .Catalog = catalog,
                        .Limit = 2U,
                    },
                }
            && value.State.Updates.back().Catalog
            && !value.State.Updates.back().Catalog->Replace
            && value.State.Updates.back().Catalog->Changes.empty()
            && value.State.Updates.back().Catalog->HistoryPage
            && value.State.Updates.back().Catalog->HistoryPage->Phase
                == core::epoch_history_page_phase::loaded
            && value.State.Updates.back().Catalog->HistoryPage->Rows.size()
                == 2U
            && value.State.Updates.back()
                    .Catalog->HistoryPage->Rows[0U]
                    .Id.Ordinal
                == core::epoch_ordinal{3}
            && value.State.Updates.back().Catalog->HistoryPage->Continuation
                == continuation
            && value.Host->state().HistoryContinuation == continuation,
        "a loaded page emits sanitized stable rows without a model replacement");

    value.State.HistoryPages.push_back(application::epoch_history_page{
        .Authority = authority().Scope,
        .Catalog = catalog,
        .Epochs =
            {
                summary_for(revision, catalog, core::epoch_ordinal{1}),
            },
    });
    value.Host->dispatch(core::request_epoch_history_page_intent{
        .Catalog = catalog,
        .Cursor = continuation,
        .Limit = 2U,
    });
    run_effect_completion(value);
    expect(
        value.State.HistoryRequests.size() == 2U
            && value.State.HistoryRequests.back().Cursor == continuation
            && value.State.Updates.back().Catalog
            && value.State.Updates.back().Catalog->HistoryPage
            && value.State.Updates.back().Catalog->HistoryPage->Rows.size()
                == 1U
            && value.State.Updates.back()
                    .Catalog->HistoryPage->Rows.front()
                    .Id.Ordinal
                == core::epoch_ordinal{1}
            && !value.State.Updates.back().Catalog->HistoryPage->Continuation
            && !value.Host->state().HistoryContinuation,
        "the exact continuation appends a short terminal page without "
        "retaining complete history in core");
}

void empty_one_and_exact_bound_history_pages_reach_public_projection()
{
    {
        fixture value;
        establish_ready_comparison(value);
        const auto catalog = *value.Host->state().Catalog;
        value.State.HistoryPages.push_back(application::epoch_history_page{
            .Authority = authority().Scope,
            .Catalog = catalog,
        });
        value.Host->dispatch(core::request_epoch_history_page_intent{
            .Catalog = catalog,
            .Limit = 2U,
        });
        run_effect_completion(value);
        expect(
            value.State.Updates.back().Catalog
                && value.State.Updates.back().Catalog->HistoryPage
                && value.State.Updates.back().Catalog->HistoryPage->Rows.empty()
                && !value.State.Updates.back().Catalog->HistoryPage->Continuation,
            "an empty bounded catalog page reaches the incremental projection");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        const auto catalog = *value.Host->state().Catalog;
        const auto revision = *value.Host->state().AcceptedRevision;
        value.State.HistoryPages.push_back(application::epoch_history_page{
            .Authority = authority().Scope,
            .Catalog = catalog,
            .Epochs =
                {
                    summary_for(revision, catalog, core::epoch_ordinal{1}),
                },
        });
        value.Host->dispatch(core::request_epoch_history_page_intent{
            .Catalog = catalog,
            .Limit = 2U,
        });
        run_effect_completion(value);
        expect(
            value.State.Updates.back().Catalog
                && value.State.Updates.back().Catalog->HistoryPage
                && value.State.Updates.back().Catalog->HistoryPage->Rows.size()
                    == 1U
                && !value.State.Updates.back().Catalog->HistoryPage->Continuation,
            "a one-row page reaches the projection as a terminal page");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        const auto catalog = *value.Host->state().Catalog;
        const auto revision = *value.Host->state().AcceptedRevision;
        value.State.HistoryPages.push_back(application::epoch_history_page{
            .Authority = authority().Scope,
            .Catalog = catalog,
            .Epochs =
                {
                    summary_for(revision, catalog, core::epoch_ordinal{2}),
                    summary_for(revision, catalog, core::epoch_ordinal{1}),
                },
        });
        value.Host->dispatch(core::request_epoch_history_page_intent{
            .Catalog = catalog,
            .Limit = 2U,
        });
        run_effect_completion(value);
        expect(
            value.State.Updates.back().Catalog
                && value.State.Updates.back().Catalog->HistoryPage
                && value.State.Updates.back().Catalog->HistoryPage->Rows.size()
                    == 2U
                && !value.State.Updates.back()
                    .Catalog->HistoryPage->Continuation,
            "an exact-bound page remains terminal when the catalog advertises "
            "no continuation");
    }
}

void history_page_failures_and_catalog_revision_changes_are_typed()
{
    {
        fixture value;
        establish_ready_comparison(value);
        const auto catalog = *value.Host->state().Catalog;
        value.State.HistoryPages.emplace_back(std::unexpected{
            application::epoch_catalog_failure::unavailable});
        value.Host->dispatch(core::request_epoch_history_page_intent{
            .Catalog = catalog,
            .Limit = 4U,
        });
        run_effect_completion(value);
        expect(
            value.State.Updates.back().Catalog
                && value.State.Updates.back().Catalog->HistoryPage
                && value.State.Updates.back().Catalog->HistoryPage->Phase
                    == core::epoch_history_page_phase::failed
                && value.State.Updates.back().Catalog->HistoryPage->Reason
                    == core::epoch_reason_code::unavailable
                && !value.Host->state().PendingHistory,
            "a catalog paging failure reaches one typed retryable projection");

        value.State.HistoryPages.emplace_back(std::unexpected{
            application::epoch_catalog_failure::cancelled});
        value.Host->dispatch(core::request_epoch_history_page_intent{
            .Catalog = catalog,
            .Limit = 4U,
        });
        run_effect_completion(value);
        expect(
            value.State.Updates.back().Catalog
                && value.State.Updates.back().Catalog->HistoryPage
                && value.State.Updates.back().Catalog->HistoryPage->Phase
                    == core::epoch_history_page_phase::cancelled
                && value.State.Updates.back().Catalog->HistoryPage->Reason
                    == core::epoch_reason_code::cancelled
                && !value.Host->state().PendingHistory,
            "a catalog paging cancellation reaches one typed retryable projection");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        const auto requested_catalog = *value.Host->state().Catalog;
        value.Host->dispatch(core::request_epoch_history_page_intent{
            .Catalog = requested_catalog,
            .Limit = 4U,
        });
        const auto changed_catalog = core::epoch_catalog_identity{
            .Incarnation = requested_catalog.Incarnation,
            .Revision = core::epoch_catalog_revision{
                requested_catalog.Revision.value() + 1},
        };
        value.State.Binding.Catalog = changed_catalog;
        value.State.Snapshot.Binding.Catalog = changed_catalog;
        run_effect_completion(value);
        expect(
            std::ranges::count(value.State.Calls, "catalog.history") == 0
                && !value.Host->state().Catalog
                && value.Host->state().PendingIngestion
                && value.Host->state().PendingIngestion->Phase
                    == core::epoch_ingestion_phase::loading_catalog
                && value.State.Updates.back().Transition
                && std::ranges::any_of(
                    value.State.Updates.back().Transition->Diagnostics,
                    [](const core::epoch_diagnostic& diagnostic)
                    {
                        return diagnostic.Code
                            == core::epoch_reason_code::catalog_conflict;
                    }),
            "a changed live catalog binding rejects the page before SQLite "
            "access and starts an exact reload");
    }
}

void reordered_history_completion_cannot_cross_source_identity()
{
    fixture value;
    establish_ready_comparison(value);
    const auto catalog = *value.Host->state().Catalog;
    value.State.HistoryPages.push_back(application::epoch_history_page{
        .Authority = authority().Scope,
        .Catalog = catalog,
    });
    value.Host->dispatch(core::request_epoch_history_page_intent{
        .Catalog = catalog,
        .Limit = 4U,
    });
    value.Effects.run_next();
    expect(
        value.Decisions.pending() == 1U,
        "history worker completion waits on the serialized decision lane");

    value.Host->accept(make_handoff(value.State, 2U, 21U, true));
    value.Effects.run_next();
    expect(value.Decisions.pending() == 2U,
        "a replacement source and old history terminal can be "
        "deterministically reordered");
    value.Decisions.run(1U);
    expect(
        value.Host->state().AcceptedRevision
            && value.Host->state().AcceptedRevision->Source.Snapshot
                == core::provider_snapshot_id{2U}
            && !value.Host->state().PendingHistory,
        "the replacement source retires old paging before its terminal runs");
    const auto updates_before_stale = value.State.Updates.size();
    value.Decisions.run_next();
    expect(
        value.State.Updates.size() == updates_before_stale + 1U
            && !value.State.Updates.back().Catalog
            && value.State.Updates.back().Transition
            && value.State.Updates.back().Transition->Diagnostics
                == std::vector{
                    core::epoch_diagnostic{
                        .Code = core::epoch_reason_code::stale_completion,
                    },
                }
            && value.Host->state().AcceptedRevision->Source.Snapshot
                == core::provider_snapshot_id{2U},
         "the old loaded page becomes one stale diagnostic and cannot publish "
         "rows into the new source");
}

void unchanged_and_association_only_skip_git()
{
    {
        fixture value;
        auto accepted = make_handoff(value.State);
        auto revision = core::accepted_pull_request_revision{
            .Source =
                {
                    .Observation =
                        opaque<core::epoch_source_observation_id>(0x61U),
                    .Snapshot = accepted.Snapshot,
                    .IdentityGeneration = accepted.IdentityGeneration,
                    .PullRequestGeneration = accepted.PullRequestGeneration,
                },
            .Origin = accepted.Origin,
            .BaseBranch = accepted.BaseBranch,
            .Base = accepted.Base,
            .Head = accepted.Head,
        };
        value.State.Snapshot.LatestLive =
            summary_for(revision, value.State.Binding.Catalog);
        value.State.Snapshot.NextOrdinal = core::epoch_ordinal{2};
        value.Host->accept(std::move(accepted));
        drain(value);
        expect(
            value.State.CacheMutationCalls == 0U
                && value.State.TransportCalls == 0U
                && std::ranges::find(
                       value.State.Calls, "catalog.reserve")
                    == value.State.Calls.end(),
            "unchanged observation does not reserve, fetch, or mutate cache");
    }

    {
        fixture value;
        const auto review = core::current_viewer_review_candidate{
            .Review = core::github_review_id{90U},
            .Actor = authority().Viewer,
            .Order = core::github_review_order{1U},
            .State = core::github_review_state::approved,
            .Commit = oid<core::git_commit_oid>(20U),
        };
        auto accepted = make_handoff(value.State, 1U, 20U, false, review);
        auto revision = core::accepted_pull_request_revision{
            .Source =
                {
                    .Observation =
                        opaque<core::epoch_source_observation_id>(0x62U),
                    .Snapshot = accepted.Snapshot,
                    .IdentityGeneration = accepted.IdentityGeneration,
                    .PullRequestGeneration = accepted.PullRequestGeneration,
                },
            .Origin = accepted.Origin,
            .BaseBranch = accepted.BaseBranch,
            .Base = accepted.Base,
            .Head = accepted.Head,
            .CurrentViewerReview = review,
        };
        value.State.Snapshot.LatestLive =
            summary_for(revision, value.State.Binding.Catalog);
        value.State.Snapshot.NextOrdinal = core::epoch_ordinal{2};
        value.Host->accept(std::move(accepted));
        drain(value);
        expect(
            value.Host->state().Catalog == catalog_identity(1)
                && value.State.CacheMutationCalls == 0U
                && value.State.TransportCalls == 0U
                && std::ranges::find(
                       value.State.Calls, "catalog.associate-review")
                    != value.State.Calls.end(),
            "association-only update commits catalog identity without Git");
    }
}

void applied_association_error_reports_mandatory_success()
{
    fixture value;
    const auto review = core::current_viewer_review_candidate{
        .Review = core::github_review_id{90U},
        .Actor = authority().Viewer,
        .Order = core::github_review_order{1U},
        .State = core::github_review_state::approved,
        .Commit = oid<core::git_commit_oid>(20U),
    };
    auto accepted = make_handoff(value.State, 1U, 20U, false, review);
    auto revision = core::accepted_pull_request_revision{
        .Source =
            {
                .Observation =
                    opaque<core::epoch_source_observation_id>(0x63U),
                .Snapshot = accepted.Snapshot,
                .IdentityGeneration = accepted.IdentityGeneration,
                .PullRequestGeneration = accepted.PullRequestGeneration,
            },
        .Origin = accepted.Origin,
        .BaseBranch = accepted.BaseBranch,
        .Base = accepted.Base,
        .Head = accepted.Head,
        .CurrentViewerReview = review,
    };
    value.State.Snapshot.LatestLive =
        summary_for(revision, value.State.Binding.Catalog);
    value.State.Snapshot.NextOrdinal = core::epoch_ordinal{2};
    value.State.AssociationAppliesThenFails = true;

    value.Host->accept(std::move(accepted));
    drain(value);

    expect(
        value.Host->state().Catalog == catalog_identity(1)
            && value.Host->state().Status == core::epoch_status::ready
            && std::ranges::count(
                   value.State.Calls, "catalog.associate-review")
                == 1
            && value.State.CacheMutationCalls == 0U
            && value.State.TransportCalls == 0U,
        "an association committed before an adapter error is observed as "
        "mandatory success without Git work");
}

void stale_load_terminal_is_inert_under_reordering()
{
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U));
    run_effect_completion(value);
    expect(value.Effects.pending() == 1U, "A catalog load is pending");

    value.Host->accept(make_handoff(value.State, 2U));
    expect(
        value.Effects.pending() == 1U,
        "the cache actor retains one FIFO worker for A load then B open");
    value.Effects.run_next();
    expect(
        value.Decisions.pending() == 2U,
        "the actor emits A cancellation before the queued B offer terminal");
    value.Decisions.run_next();
    value.Decisions.run_next();
    value.Effects.run_next();
    while(value.Decisions.pending() != 0U)
    {
        value.Decisions.run_next();
    }
    expect(
        value.Host->state().AcceptedRevision->Source.Snapshot
                == core::provider_snapshot_id{2U}
            && value.Host->state().PendingIngestion->Phase
                == core::epoch_ingestion_phase::preparing,
        "A cancellation followed by B load leaves only B preparation active");
}

void stale_prepare_and_comparison_terminals_are_inert()
{
    {
        fixture value;
        value.Host->accept(make_handoff(value.State, 1U));
        run_effect_completion(value);
        run_effect_completion(value);
        expect(value.Effects.pending() == 1U, "A preparation is pending");
        value.Host->accept(make_handoff(value.State, 2U));
        expect(
            value.Effects.pending() == 1U,
            "B offer queues behind A preparation on the cache actor");
        value.Effects.run_next();
        expect(
            value.Decisions.pending() == 2U,
            "A preparation cancellation precedes B offer completion");
        value.Decisions.run_next();
        value.Decisions.run_next();
        value.Effects.run_next();
        while(value.Decisions.pending() != 0U)
        {
            value.Decisions.run_next();
        }
        expect(
            value.Host->state().AcceptedRevision->Source.Snapshot
                    == core::provider_snapshot_id{2U}
                && value.Host->state().PendingIngestion->Phase
                    == core::epoch_ingestion_phase::preparing,
            "FIFO A cancellation cannot clear subsequent B preparation");
    }

    {
        fixture value;
        value.Host->accept(make_handoff(value.State, 1U, 20U, true));
        drive_through_preparation(value);
        run_effect_completion(value);
        expect(value.Effects.pending() == 1U, "A comparison is pending");
        value.Host->accept(make_handoff(value.State, 2U, 21U));
        expect(
            value.Effects.pending() == 1U,
            "B offer queues behind A comparison on the cache actor");
        value.Effects.run_next();
        expect(
            value.Decisions.pending() == 2U,
            "A comparison cancellation precedes B offer completion");
        value.Decisions.run_next();
        value.Decisions.run_next();
        expect(
            value.Host->state().AcceptedRevision->Source.Snapshot
                    == core::provider_snapshot_id{2U}
                && !value.Host->state().AcceptedComparison,
            "FIFO A comparison cancellation cannot publish into B");
    }
}

void same_repository_actor_switches_exact_pr_cache_sessions_in_fifo_order()
{
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    expect(
        value.Effects.pending() == 1U
            && value.State.CacheOpenRequests.size() == 1U,
        "A retains one prepared commit and one exact cache session");

    auto second_authority = authority();
    second_authority.PullRequest = core::pull_request_id{72U};
    second_authority.Number = core::pull_request_number{82U};
    second_authority.Scope =
        opaque<core::pull_request_authority_scope_id>(0x44U);
    const auto second_catalog = core::epoch_catalog_identity{
        .Incarnation =
            opaque<core::epoch_catalog_incarnation_id>(0x45U),
        .Revision = core::epoch_catalog_revision{0},
    };
    value.State.CatalogOpenSnapshots.push_back(
        application::epoch_catalog_snapshot{
            .Binding =
                {
                    .Authority = second_authority,
                    .Catalog = second_catalog,
                    .ObjectFormat = core::git_hash_algorithm::sha1,
                },
            .NextOrdinal = core::epoch_ordinal{1U},
        });
    auto second = make_handoff(value.State, 2U, 21U);
    second.PullRequest = second_authority.PullRequest;
    second.Number = second_authority.Number;

    value.Host->accept(std::move(second));
    drain(value);

    expect_in_order(
        value.State.Calls,
        {
            "catalog.transition:aborted",
            "cache.session-close:71",
            "cache.session-open:72",
        });
    expect(
        value.State.CacheOpenRequests.size() == 2U
            && value.State.CacheOpenRequests[0U].Authority.PullRequest
                == core::pull_request_id{71U}
            && value.State.CacheOpenRequests[1U].Authority.PullRequest
                == core::pull_request_id{72U}
            && value.State.CacheOpenRequests[0U].Authority.Scope
                == authority().Scope
            && value.State.CacheOpenRequests[1U].Authority.Scope
                == second_authority.Scope
            && value.State.MaximumActiveCacheSessions == 1U
            && value.State.ActiveCacheSessions == 1U
            && value.Host->state().Authority == std::optional{second_authority},
        "one repository actor cleans A under A, closes it, then opens and "
        "uses the exact B PR session without overlap");
}

void cancellation_before_ponr_aborts_without_publication()
{
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    value.Host->dispatch(core::cancel_epoch_refresh_intent{});
    run_effect_completion(value);
    expect(
        std::ranges::find(value.State.Calls, "catalog.transition:ponr")
                == value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "cache.promote")
                == value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "catalog.finalize")
                == value.State.Calls.end()
            && std::ranges::find(
                   value.State.Calls, "catalog.transition:aborted")
                != value.State.Calls.end()
            && value.Host->state().Catalog == catalog_identity(),
        "pre-PONR cancellation aborts staging and leaves catalog invisible");
}

void stop_immediately_after_ponr_cannot_replace_committed_terminal()
{
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    std::mutex gate;
    std::condition_variable changed;
    auto reached_ponr = false;
    auto release_worker = false;
    auto stop_injected = false;
    value.State.AfterRefCommitInProgress = [&]
    {
        std::unique_lock lock{gate};
        reached_ponr = true;
        changed.notify_one();
        changed.wait(lock, [&release_worker]
        {
            return release_worker;
        });
    };

    std::thread worker{[&value]
    {
        value.Effects.run_next();
    }};
    {
        std::unique_lock lock{gate};
        changed.wait(lock, [&reached_ponr]
        {
            return reached_ponr;
        });
    }
    value.Decisions.post([&]
    {
        stop_injected = true;
        value.Host->dispatch(core::cancel_epoch_refresh_intent{});
    });
    value.Decisions.run_next();
    {
        const std::lock_guard lock{gate};
        release_worker = true;
    }
    changed.notify_one();
    worker.join();
    expect(
        value.Decisions.pending() == 1U,
        "the released post-PONR worker queues one serialized terminal");
    value.Decisions.run_next();
    drain(value);

    const auto committed_terminals = std::ranges::count_if(
        value.State.Updates,
        [](const application::epoch_presentation_update& update)
        {
            return update.Catalog
                && std::ranges::any_of(
                    update.Catalog->Changes,
                    [](const application::epoch_row_change_projection& change)
                    {
                        return change.Kind
                            == application::epoch_row_change_kind::insert;
                    });
        });
    expect(
        stop_injected
            && std::ranges::find(value.State.Calls, "cache.promote")
                != value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "catalog.finalize")
                != value.State.Calls.end()
            && value.Host->state().Catalog == catalog_identity(1)
            && committed_terminals == 1
            && value.State.ProjectionOutsideDecision == 0U,
        "a stop requested after durable PONR still promotes, finalizes, and "
        "publishes one committed terminal");
}

void applied_commit_authorization_error_aborts_exactly_before_ponr()
{
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    value.State.AfterCommitAuthorized = [&value]
    {
        expect(
            value.State.Operation.has_value(),
            "applied commit authorization leaves one durable operation");
        seed_recovery(value.State, *value.State.Operation);
        throw std::runtime_error{
            "injected applied commit-authorization error"};
    };

    run_effect_completion(value);

    expect(
        std::ranges::find(
            value.State.Calls, "catalog.transition:abort-requested")
                != value.State.Calls.end()
            && std::ranges::find(
                   value.State.Calls, "cache.abort-exact")
                != value.State.Calls.end()
            && std::ranges::find(
                   value.State.Calls, "catalog.transition:aborted")
                != value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "cache.promote")
                == value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "catalog.finalize")
                == value.State.Calls.end()
            && value.Host->state().Catalog == catalog_identity(),
        "an ambiguous commit-authorization result pages the exact durable "
        "operation and aborts while cancellation is still legal");
}

void applied_ponr_error_recovers_to_mandatory_success()
{
    using match = application::git_recovery_match;
    using refs = application::git_recovery_ref_topology;
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    value.State.AfterRefCommitInProgress = [&value]
    {
        expect(
            value.State.Operation.has_value(),
            "applied PONR leaves one durable operation");
        const auto durable = *value.State.Operation;
        seed_recovery(value.State, durable);
        value.State.RecoveryInspections.push_back(
            recovery_observation(
                durable, refs::staging, match::exact, match::absent));
        throw std::runtime_error{"injected applied-PONR error"};
    };

    run_effect_completion(value);
    drain(value);

    const auto committed_terminals = std::ranges::count_if(
        value.State.Updates,
        [](const application::epoch_presentation_update& update)
        {
            return update.Catalog
                && std::ranges::any_of(
                    update.Catalog->Changes,
                    [](const application::epoch_row_change_projection& change)
                    {
                        return change.Kind
                            == application::epoch_row_change_kind::insert;
                    });
        });
    expect(
        std::ranges::find(value.State.Calls, "catalog.recovery-page")
                != value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "cache.promote")
                != value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "catalog.finalize")
                != value.State.Calls.end()
            && value.Host->state().Catalog == catalog_identity(1)
            && committed_terminals == 1,
        "an applied-then-throw PONR transition resumes the exact durable "
        "operation and publishes mandatory success once");
}

void throwing_exact_abort_degrades_to_a_bounded_terminal()
{
    fixture value;
    value.Host->accept(make_handoff(value.State, 1U, 20U, true));
    drive_through_preparation(value);
    value.State.ThrowRecoveryAbort = true;

    value.Host->dispatch(core::cancel_epoch_refresh_intent{});
    drain(value);

    const auto diagnosed = std::ranges::any_of(
        value.State.Updates,
        [](const application::epoch_presentation_update& update)
        {
            return update.Transition
                && std::ranges::any_of(
                    update.Transition->Diagnostics,
                    [](const core::epoch_diagnostic& diagnostic)
                    {
                        return diagnostic.Code
                            == core::epoch_reason_code::repair_required;
                    });
        });
    expect(
        diagnosed
            && std::ranges::find(value.State.Calls, "cache.abort-exact")
                != value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "cache.promote")
                == value.State.Calls.end()
            && std::ranges::find(value.State.Calls, "catalog.finalize")
                == value.State.Calls.end()
            && value.Host->state().Catalog == catalog_identity(),
        "a throwing abort port cannot terminate or publish a pre-PONR append");
}

void staging_ready_recovery_journals_abort_before_cleanup_and_acceptance()
{
    fixture value;
    const auto record = recovery_record(
        value.State,
        application::epoch_catalog_operation_phase::staging_ready);
    seed_recovery(value.State, record);

    value.Host->accept(make_handoff(value.State));
    value.Effects.run_next();
    expect(
        !value.Host->state().Authority
            && value.State.ProjectionCalls == 0U,
        "restart recovery finishes before the offered revision reaches core");
    expect_in_order(
        value.State.Calls,
        {
            "catalog.recovery-page",
            "catalog.transition:abort-requested",
            "cache.abort-exact",
            "catalog.transition:aborted",
            "catalog.source-probe",
        });
    expect(
        value.State.Operation
            && value.State.Operation->Phase
                == application::epoch_catalog_operation_phase::aborted
            && value.State.Operation->RecoveryInputCompacted,
        "staging-ready recovery reaches the compacted aborted terminal");
    expect(
        value.State.RecoveryAbortRequests.size() == 1U
            && application::valid(value.State.RecoveryAbortRequests.front())
            && value.State.RecoveryAbortRequests.front().Operation
                == record.Context.Operation
            && value.State.RecoveryAbortRequests.front().Scope
                == value.State.Binding.Authority.Scope
            && value.State.RecoveryAbortRequests.front().Ordinal
                == record.ReservedOrdinal,
        "pre-PONR cleanup is bound to the exact durable operation and refs");

    value.Decisions.run_next();
    expect(
        value.Host->state().Authority == authority()
            && value.Host->state().AcceptedRevision.has_value(),
        "the offer reaches core only after exact recovery and source probing");
}

void post_ponr_recovery_resumes_every_durable_phase()
{
    using enum application::epoch_catalog_operation_phase;
    using match = application::git_recovery_match;
    using refs = application::git_recovery_ref_topology;
    struct recovery_case
    {
        application::epoch_catalog_operation_phase Phase;
        application::git_recovery_ref_topology Refs;
        application::git_recovery_match Staging;
        application::git_recovery_match Final;
        bool Promotes = false;
        bool Finalizes = false;
        bool AlreadyVisible = false;
        std::string_view Label;
    };
    const auto cases = std::array{
        recovery_case{
            .Phase = commit_authorized,
            .Refs = refs::staging,
            .Staging = match::exact,
            .Final = match::absent,
            .Promotes = true,
            .Finalizes = true,
            .Label = "commit-authorized recovery resumes promotion"},
        recovery_case{
            .Phase = ref_commit_in_progress,
            .Refs = refs::staging,
            .Staging = match::exact,
            .Final = match::absent,
            .Promotes = true,
            .Finalizes = true,
            .Label = "in-progress staging recovery resumes promotion"},
        recovery_case{
            .Phase = ref_commit_in_progress,
            .Refs = refs::final,
            .Staging = match::absent,
            .Final = match::exact,
            .Finalizes = true,
            .Label = "in-progress final refs resume catalog completion"},
        recovery_case{
            .Phase = refs_committed,
            .Refs = refs::final,
            .Staging = match::absent,
            .Final = match::exact,
            .Finalizes = true,
            .Label = "refs-committed recovery resumes visibility"},
        recovery_case{
            .Phase = visible_cleanup_pending,
            .Refs = refs::final,
            .Staging = match::exact,
            .Final = match::exact,
            .AlreadyVisible = true,
            .Label = "visible recovery removes exact staging residue"},
    };

    for(const auto& item : cases)
    {
        fixture value;
        const auto record = recovery_record(value.State, item.Phase);
        if(item.AlreadyVisible)
        {
            value.State.Binding.Catalog = catalog_identity(1);
            value.State.Snapshot.Binding.Catalog = catalog_identity(1);
            value.State.Snapshot.LatestLive = record.Candidate->Epoch;
            value.State.Snapshot.NextOrdinal = core::epoch_ordinal{2U};
        }
        seed_recovery(value.State, record);
        value.State.RecoveryInspections.push_back(
            recovery_observation(
                record, item.Refs, item.Staging, item.Final));
        if(!item.Promotes)
        {
            value.State.Retained = application::git_retained_ref_pair{
                .Scope = value.State.Binding.Authority.Scope,
                .Ordinal = record.ReservedOrdinal,
                .Base = record.Context.Revision.Base,
                .Head = record.Context.Revision.Head,
            };
        }

        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);

        const auto promoted = std::ranges::count(
            value.State.Calls, "cache.promote");
        const auto finalized = std::ranges::count(
            value.State.Calls, "catalog.finalize");
        expect(
            promoted == static_cast<std::ptrdiff_t>(item.Promotes),
            item.Label);
        expect(
            finalized == static_cast<std::ptrdiff_t>(item.Finalizes),
            item.Label);
        expect(
            value.State.Operation
                && value.State.Operation->Phase == complete
                && value.State.Operation->RecoveryInputCompacted
                && value.State.RecoveryCompletionRequests.size() == 1U
                && application::valid(
                    value.State.RecoveryCompletionRequests.front())
                && value.Host->state().AcceptedRevision.has_value(),
            item.Label);
        expect_in_order(
            value.State.Calls,
            {
                "catalog.recovery-page",
                "cache.inspect-recovery",
                "cache.inspect-retained",
                "cache.complete-operation",
                "catalog.transition:cleanup",
                "catalog.source-probe",
            });
    }
}

void promotion_error_is_resolved_by_exact_postcondition_inspection()
{
    using match = application::git_recovery_match;
    using refs = application::git_recovery_ref_topology;
    fixture value;
    const auto record = recovery_record(
        value.State,
        application::epoch_catalog_operation_phase::ref_commit_in_progress);
    seed_recovery(value.State, record);
    value.State.RecoveryInspections.push_back(
        recovery_observation(
            record, refs::staging, match::exact, match::absent));
    value.State.RecoveryInspections.push_back(
        recovery_observation(
            record, refs::final, match::absent, match::exact));
    value.State.PromoteAppliesThenFails = true;

    value.Host->accept(make_handoff(value.State));
    run_effect_completion(value);

    expect(
        std::ranges::count(value.State.Calls, "cache.inspect-recovery")
                == 2
            && std::ranges::count(value.State.Calls, "cache.promote") == 1
            && std::ranges::count(value.State.Calls, "catalog.finalize")
                == 1
            && value.State.Operation
            && value.State.Operation->Phase
                == application::epoch_catalog_operation_phase::complete
            && value.Host->state().AcceptedRevision.has_value(),
        "applied-then-error promotion resumes from exact final postcondition");
}

[[nodiscard]] auto has_repair_diagnostic(const shared_state& state) -> bool
{
    return std::ranges::any_of(
        state.Updates,
        [](const application::epoch_presentation_update& update)
        {
            return update.Transition
                && std::ranges::any_of(
                    update.Transition->Diagnostics,
                    [](const core::epoch_diagnostic& diagnostic)
                    {
                        return diagnostic.Code
                            == core::epoch_reason_code::repair_required;
                    });
        });
}

void recovery_identity_and_pagination_mismatches_fail_closed()
{
    using match = application::git_recovery_match;
    using refs = application::git_recovery_ref_topology;
    {
        fixture value;
        const auto record = recovery_record(
            value.State,
            application::epoch_catalog_operation_phase::commit_authorized);
        seed_recovery(value.State, record);
        auto wrong = recovery_observation(
            record, refs::staging, match::exact, match::absent);
        wrong.Operation = opaque<core::epoch_operation_id>(0x7FU);
        value.State.RecoveryInspections.push_back(wrong);

        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(
            !value.Host->state().Authority && has_repair_diagnostic(value.State)
                && std::ranges::find(
                       value.State.Calls,
                       "catalog.transition:repair-required")
                    != value.State.Calls.end()
                && std::ranges::find(
                       value.State.Calls, "catalog.source-probe")
                    == value.State.Calls.end(),
            "mismatched physical operation identity is journaled for repair "
            "before acceptance");
    }

    {
        fixture value;
        value.State.RecoveryPages.push_back(
            application::epoch_recovery_page{
                .Incarnation =
                    opaque<core::epoch_catalog_incarnation_id>(0x7EU),
            });
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(
            !value.Host->state().Authority && has_repair_diagnostic(value.State)
                && std::ranges::find(
                       value.State.Calls, "catalog.source-probe")
                    == value.State.Calls.end(),
            "mismatched recovery-page incarnation fails closed as repair");
    }

    {
        fixture value;
        const auto first = recovery_record(
            value.State,
            application::epoch_catalog_operation_phase::staging_ready,
            0x10U);
        const auto continuation = opaque<core::epoch_operation_id>(0x20U);
        const auto wrong_next = recovery_record(
            value.State,
            application::epoch_catalog_operation_phase::staging_ready,
            0x30U);
        value.State.Operation = first;
        value.State.RecoveryPages.push_back(
            application::epoch_recovery_page{
                .Incarnation = value.State.Binding.Catalog.Incarnation,
                .Operations = {first},
                .Continuation = continuation,
            });
        value.State.RecoveryPages.push_back(
            application::epoch_recovery_page{
                .Incarnation = value.State.Binding.Catalog.Incarnation,
                .Operations = {wrong_next},
            });

        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(
            !value.Host->state().Authority && has_repair_diagnostic(value.State)
                && value.State.RecoveryPageCursors
                    == std::vector<std::optional<core::epoch_operation_id>>{
                        std::nullopt,
                        continuation,
                    }
                && std::ranges::find(
                       value.State.Calls, "catalog.source-probe")
                    == value.State.Calls.end(),
            "pagination must resume at the exact advertised operation identity");
    }
}

void core_authorized_prune_composes_exact_durable_cleanup()
{
    fixture value;
    establish_ready_comparison(value);
    const auto epoch = value.State.Snapshot.LatestLive->Id;
    value.State.Calls.clear();
    value.Host->dispatch(core::prune_review_epochs_intent{
        .Catalog = *value.Host->state().Catalog,
        .Epochs = {epoch},
    });
    run_effect_completion(value);

    expect(value.State.BeginPruneRequests.size() == 1U
            && value.State.BeginPruneRequests.front().Epochs.size() == 1U
            && value.State.BeginPruneRequests.front().Epochs.front().Epoch
                == epoch
            && value.State.PruneRequests.size() == 1U
            && value.State.PruneRequests.front().Expected
                == value.State.BeginPruneRequests.front()
                    .Epochs.front()
                    .Retained
            && value.State.PruneRequests.front().DurableIntentToken
                == value.State.BeginPruneRequests.front().Operation.bytes()
            && value.Host->state().Catalog == catalog_identity(2)
            && !value.Host->state().PendingMaintenance,
        "the host seals and executes one exact core-authorized prune batch");
    expect_in_order(value.State.Calls,
        {
            "catalog.exact-anchors",
            "catalog.begin-prune",
            "catalog.transition:ref-delete",
            "cache.prune",
            "catalog.finalize-prune",
            "catalog.transition:cleanup",
        });
    expect(std::ranges::any_of(value.State.Updates,
               [epoch](const auto& update)
               {
                   return update.Catalog
                       && update.Catalog->AvailabilityChangedEpochs
                           == std::vector{epoch}
                       && update.Catalog->AvailabilityChangedTo
                           == core::anchor_availability::pruned
                       && !update.Catalog->Replace;
               }),
        "prune publishes an incremental stable-ID availability projection");
}

void core_authorized_cache_delete_closes_session_and_projects_bulk_change()
{
    fixture value;
    establish_ready_comparison(value);
    expect(value.State.ActiveCacheSessions == 1U,
        "the ready fixture owns one live cache session");
    value.State.Calls.clear();
    value.Host->dispatch(core::delete_repository_cache_intent{
        .Catalog = *value.Host->state().Catalog,
        .Cache = value.State.Binding.Authority.Cache,
    });
    run_effect_completion(value);

    expect(value.State.BeginCacheDeleteRequests.size() == 1U
            && value.State.BeginCacheDeleteRequests.front().Impact.Scopes
                == std::vector{application::cache_delete_scope_fact{
                    .Authority = value.State.Binding.Authority.Scope,
                    .ExpectedRevision = core::epoch_catalog_revision{1},
                }}
            && value.State.DeleteRequests.size() == 2U
            && value.State.DeleteRequests.front().DurableIntentToken
                == value.State.BeginCacheDeleteRequests.front()
                    .Operation.bytes()
            && value.State.DeleteRequests.back()
                == value.State.DeleteRequests.front()
            && value.State.ActiveCacheSessions == 0U
            && value.Host->state().Catalog == catalog_identity(2),
        "cache delete seals every affected scope, closes the handle, and uses "
        "one replay-stable authorization twice");
    expect_in_order(value.State.Calls,
        {
            "catalog.cache-delete-impact",
            "catalog.begin-cache-delete",
            "catalog.transition:quarantine-rename",
            "cache.session-close:71",
            "cache.delete",
            "catalog.transition:quarantined",
            "catalog.finalize-cache-delete",
            "cache.delete",
            "catalog.transition:cleanup",
        });
    expect(std::ranges::any_of(value.State.Updates,
               [](const auto& update)
               {
                   return update.Catalog
                       && update.Catalog->AllEpochAvailability
                       == core::anchor_availability::cache_deleted
                       && !update.Catalog->Replace;
               }),
        "cache deletion publishes one bulk incremental tombstone projection");
}

void maintenance_cancellation_obeys_the_durable_point_of_no_return()
{
    {
        fixture value;
        establish_ready_comparison(value);
        value.State.Calls.clear();
        value.Host->dispatch(core::delete_repository_cache_intent{
            .Catalog = *value.Host->state().Catalog,
            .Cache = value.State.Binding.Authority.Cache,
        });
        value.Host->dispatch(core::cancel_epoch_maintenance_intent{});
        run_effect_completion(value);
        expect(value.State.BeginCacheDeleteRequests.empty()
                && value.State.DeleteRequests.empty()
                && value.Host->state().Catalog == catalog_identity(1)
                && value.Host->state().Status == core::epoch_status::cancelled,
            "cancellation before durable sealing performs no destructive work");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        const auto epoch = value.State.Snapshot.LatestLive->Id;
        std::mutex gate;
        std::condition_variable changed;
        auto reached_point = false;
        auto release_worker = false;
        value.State.AfterRefCommitInProgress = [&]
        {
            std::unique_lock lock{gate};
            reached_point = true;
            changed.notify_one();
            changed.wait(lock, [&release_worker] { return release_worker; });
        };
        value.Host->dispatch(core::prune_review_epochs_intent{
            .Catalog = *value.Host->state().Catalog,
            .Epochs = {epoch},
        });
        std::thread worker{[&value] { value.Effects.run_next(); }};
        {
            std::unique_lock lock{gate};
            changed.wait(lock, [&reached_point] { return reached_point; });
        }
        value.Decisions.post([&]
            { value.Host->dispatch(core::cancel_epoch_maintenance_intent{}); });
        value.Decisions.run_next();
        {
            const std::lock_guard lock{gate};
            release_worker = true;
        }
        changed.notify_one();
        worker.join();
        value.Decisions.run_next();
        expect(value.State.PruneRequests.size() == 1U
                && value.Host->state().Catalog == catalog_identity(2)
                && !value.Host->state().PendingMaintenance,
            "post-point-of-no-return cancellation cannot replace durable prune "
            "success");
    }
}

void maintenance_restart_recovery_aborts_or_completes_by_phase()
{
    {
        fixture value;
        const auto record = maintenance_recovery_record(value.State,
            application::epoch_catalog_operation_kind::prune,
            application::epoch_catalog_operation_phase::prune_intent);
        seed_maintenance_recovery(value.State, record);
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(std::ranges::find(value.State.Calls,
                   "catalog.transition:abort-requested")
                    != value.State.Calls.end()
                && std::ranges::find(value.State.Calls,
                       "catalog.transition:aborted")
                    != value.State.Calls.end()
                && value.State.PruneRequests.empty()
                && value.Host->state().Authority.has_value(),
            "restart aborts a sealed pre-PONR maintenance intent before "
            "authority acceptance");
    }

    {
        fixture value;
        const auto record = maintenance_recovery_record(value.State,
            application::epoch_catalog_operation_kind::prune,
            application::epoch_catalog_operation_phase::ref_delete_in_progress);
        seed_maintenance_recovery(value.State, record);
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(value.State.PruneRequests.size() == 1U
                && value.State.Binding.Catalog == catalog_identity(1)
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::complete
                && value.Host->state().Authority.has_value(),
            "restart completes an exact post-PONR prune before authority "
            "acceptance");
    }

    {
        fixture value;
        const auto record = maintenance_recovery_record(value.State,
            application::epoch_catalog_operation_kind::cache_delete,
            application::epoch_catalog_operation_phase::
                quarantine_rename_in_progress);
        seed_maintenance_recovery(value.State, record);
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(value.State.DeleteRequests.size() == 2U
                && value.State.Binding.Catalog == catalog_identity(1)
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::complete
                && value.Host->state().Authority.has_value(),
            "restart completes quarantine, catalog tombstone, and exact "
            "cleanup "
            "before authority acceptance");
    }
}

void maintenance_restart_recovery_covers_every_durable_phase()
{
    using enum application::epoch_catalog_operation_phase;

    for(const auto phase : {prune_intent, abort_requested})
    {
        fixture value;
        seed_maintenance_recovery(value.State,
            maintenance_recovery_record(value.State,
                application::epoch_catalog_operation_kind::prune,
                phase));
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(value.State.Maintenance->Phase == aborted
                && value.State.PruneRequests.empty()
                && value.Host->state().Authority.has_value(),
            "every pre-PONR prune phase restarts by exact abort");
    }

    {
        fixture value;
        seed_maintenance_recovery(value.State,
            maintenance_recovery_record(value.State,
                application::epoch_catalog_operation_kind::prune,
                visible_cleanup_pending));
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(value.State.Maintenance->Phase == complete
                && value.State.PruneRequests.empty()
                && std::ranges::find(value.State.Calls,
                       "catalog.finalize-prune")
                    == value.State.Calls.end(),
            "restart after visible prune commit performs cleanup only");
    }

    for(const auto phase : {cache_delete_building_exact_set,
            cache_delete_intent_sealed,
            abort_requested})
    {
        fixture value;
        seed_maintenance_recovery(value.State,
            maintenance_recovery_record(value.State,
                application::epoch_catalog_operation_kind::cache_delete,
                phase));
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(value.State.Maintenance->Phase == aborted
                && value.State.DeleteRequests.empty()
                && value.Host->state().Authority.has_value(),
            "every pre-PONR cache-delete phase restarts by exact abort");
    }

    for(const auto [phase, expected_deletes] : std::array{
            std::pair{quarantined, std::size_t{1U}},
            std::pair{catalog_tombstoned_cleanup_pending, std::size_t{1U}},
        })
    {
        fixture value;
        seed_maintenance_recovery(value.State,
            maintenance_recovery_record(value.State,
                application::epoch_catalog_operation_kind::cache_delete,
                phase));
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(value.State.Maintenance->Phase == complete
                && value.State.DeleteRequests.size() == expected_deletes
                && value.Host->state().Authority.has_value(),
            "every post-quarantine cache-delete phase resumes exact cleanup");
    }

    for(const auto kind : {application::epoch_catalog_operation_kind::prune,
            application::epoch_catalog_operation_kind::cache_delete})
    {
        for(const auto phase : {complete, aborted})
        {
            fixture value;
            seed_maintenance_recovery(value.State,
                maintenance_recovery_record(value.State, kind, phase));
            value.Host->accept(make_handoff(value.State));
            run_effect_completion(value);
            expect(value.State.Maintenance->Phase == phase
                    && value.State.PruneRequests.empty()
                    && value.State.DeleteRequests.empty()
                    && value.Host->state().Authority.has_value(),
                "terminal maintenance recovery is an inert replay");
        }

        fixture value;
        auto repair =
            maintenance_recovery_record(value.State, kind, repair_required);
        repair.RepairFromPhase =
            kind == application::epoch_catalog_operation_kind::prune
            ? std::optional{ref_delete_in_progress}
            : std::optional{quarantine_rename_in_progress};
        seed_maintenance_recovery(value.State, repair);
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
        expect(!value.Host->state().Authority
                && has_repair_diagnostic(value.State)
                && value.State.PruneRequests.empty()
                && value.State.DeleteRequests.empty(),
            "repair-required maintenance blocks authority activation");
    }
}

void maintenance_faults_fail_closed_and_restart_from_durable_phase()
{
    const auto restart = [](fixture& value)
    {
        expect(value.State.Maintenance.has_value(),
            "fault leaves an exact durable maintenance record");
        seed_maintenance_recovery(value.State, *value.State.Maintenance);
        value.Host->accept(make_handoff(value.State));
        run_effect_completion(value);
    };

    {
        fixture value;
        establish_ready_comparison(value);
        const auto epoch = value.State.Snapshot.LatestLive->Id;
        value.State.MaintenanceCatalogFailures.emplace_back(
            "catalog.exact-anchors",
            application::epoch_catalog_failure::unavailable);
        value.Host->dispatch(core::prune_review_epochs_intent{
            .Catalog = *value.Host->state().Catalog,
            .Epochs = {epoch},
        });
        run_effect_completion(value);
        expect(!value.State.Maintenance && value.State.PruneRequests.empty()
                && value.Host->state().Catalog == catalog_identity(1)
                && value.Host->state().Status == core::epoch_status::failed,
            "pre-intent anchor lookup failure performs no destructive work");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        const auto epoch = value.State.Snapshot.LatestLive->Id;
        value.State.MaintenanceCatalogFailures.emplace_back(
            "catalog.transition:ref-delete",
            application::epoch_catalog_failure::unavailable);
        value.Host->dispatch(core::prune_review_epochs_intent{
            .Catalog = *value.Host->state().Catalog,
            .Epochs = {epoch},
        });
        run_effect_completion(value);
        expect(value.State.Maintenance
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::prune_intent
                && value.State.PruneRequests.empty(),
            "point-of-no-return transition failure preserves a sealed "
            "abortable "
            "prune intent");
        restart(value);
        expect(value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::aborted
                && value.State.PruneRequests.empty(),
            "restart aborts a prune whose point-of-no-return transition did "
            "not "
            "commit");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        const auto epoch = value.State.Snapshot.LatestLive->Id;
        value.State.MaintenanceCacheFailures.emplace_back("cache.prune",
            application::git_cache_failure::ref_conflict);
        value.Host->dispatch(core::prune_review_epochs_intent{
            .Catalog = *value.Host->state().Catalog,
            .Epochs = {epoch},
        });
        run_effect_completion(value);
        expect(value.State.Maintenance
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::
                        repair_required
                && has_repair_diagnostic(value.State),
            "ambiguous exact-ref failure is durably marked repair-required");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        const auto epoch = value.State.Snapshot.LatestLive->Id;
        value.State.MaintenanceCatalogFailures.emplace_back(
            "catalog.finalize-prune",
            application::epoch_catalog_failure::unavailable);
        value.Host->dispatch(core::prune_review_epochs_intent{
            .Catalog = *value.Host->state().Catalog,
            .Epochs = {epoch},
        });
        run_effect_completion(value);
        expect(value.State.Maintenance
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::
                        ref_delete_in_progress
                && value.State.PruneRequests.size() == 1U,
            "catalog failure after exact ref deletion retains the post-PONR "
            "phase");
        const auto authorization = value.State.PruneRequests.front();
        restart(value);
        expect(value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::complete
                && value.State.PruneRequests.size() == 2U
                && value.State.PruneRequests.back() == authorization
                && value.State.Binding.Catalog == catalog_identity(2),
            "restart replays the same prune authorization and completes "
            "visibility once");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        value.State.MaintenanceCacheFailures.emplace_back("cache.delete",
            application::git_cache_failure::process_failure);
        value.Host->dispatch(core::delete_repository_cache_intent{
            .Catalog = *value.Host->state().Catalog,
            .Cache = value.State.Binding.Authority.Cache,
        });
        run_effect_completion(value);
        expect(value.State.Maintenance
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::
                        quarantine_rename_in_progress
                && value.State.DeleteRequests.size() == 1U,
            "physical deletion failure retains the exact post-PONR quarantine "
            "phase");
        const auto authorization = value.State.DeleteRequests.front();
        restart(value);
        expect(value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::complete
                && value.State.DeleteRequests.size() == 3U
                && std::ranges::all_of(value.State.DeleteRequests,
                    [&authorization](const auto& request)
                    { return request == authorization; })
                && value.State.Binding.Catalog == catalog_identity(2),
            "restart replays one cache-delete authorization through "
            "quarantine, "
            "tombstone, and cleanup");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        value.State.CacheDeleteFinalizesWithoutChange = true;
        value.Host->dispatch(core::delete_repository_cache_intent{
            .Catalog = *value.Host->state().Catalog,
            .Cache = value.State.Binding.Authority.Cache,
        });
        run_effect_completion(value);
        expect(value.State.Maintenance
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::
                        repair_required
                && has_repair_diagnostic(value.State),
            "cache-delete success without the exact catalog revision "
            "postcondition "
            "is marked repair-required");
    }

    {
        fixture value;
        establish_ready_comparison(value);
        value.State.MaintenanceCatalogFailures.emplace_back(
            "catalog.transition:cleanup",
            application::epoch_catalog_failure::unavailable);
        value.Host->dispatch(core::delete_repository_cache_intent{
            .Catalog = *value.Host->state().Catalog,
            .Cache = value.State.Binding.Authority.Cache,
        });
        run_effect_completion(value);
        expect(value.State.Maintenance
                && value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::
                        catalog_tombstoned_cleanup_pending
                && value.State.Binding.Catalog == catalog_identity(2),
            "terminal bookkeeping failure preserves the already-visible cache "
            "tombstone phase");
        const auto authorization = value.State.DeleteRequests.front();
        restart(value);
        expect(value.State.Maintenance->Phase
                    == application::epoch_catalog_operation_phase::complete
                && std::ranges::all_of(value.State.DeleteRequests,
                    [&authorization](const auto& request)
                    { return request == authorization; })
                && value.State.Binding.Catalog == catalog_identity(2),
            "restart finishes bookkeeping without a second catalog revision");
    }
}

void shutdown_waits_for_worker_and_decision_quiescence()
{
    fixture value;
    value.Host->accept(make_handoff(value.State));
    value.Host->shutdown();
    expect(
        value.Host->is_closing() && !value.Host->is_quiescent(),
        "shutdown closes ingress while an accepted worker remains");
    value.Effects.run_next();
    expect(
        !value.Host->is_quiescent(),
        "worker completion is not quiescent before decision-lane retirement");
    value.Decisions.run_next();
    expect(
        value.Host->is_quiescent(),
        "shutdown becomes quiescent after terminal decision retirement");
}
} // namespace

int main()
{
    invalid_handoff_has_no_effects();
    source_collision_retries_before_core_acceptance();
    successful_append_preserves_exact_order();
    projection_failure_is_contained_and_decision_progress_drains();
    reentrant_projection_ingress_is_deferred_without_recursion();
    failed_same_authority_offer_preserves_active_comparison();
    successful_same_authority_offer_swaps_on_exact_terminal();
    historical_origin_reaches_authority_projection();
    superseded_offer_failure_emits_no_diagnostic();
    matching_invalidation_cancels_pending_offer_and_active_context();
    bounded_history_pages_project_incrementally();
    empty_one_and_exact_bound_history_pages_reach_public_projection();
    history_page_failures_and_catalog_revision_changes_are_typed();
    reordered_history_completion_cannot_cross_source_identity();
    unchanged_and_association_only_skip_git();
    applied_association_error_reports_mandatory_success();
    stale_load_terminal_is_inert_under_reordering();
    stale_prepare_and_comparison_terminals_are_inert();
    same_repository_actor_switches_exact_pr_cache_sessions_in_fifo_order();
    cancellation_before_ponr_aborts_without_publication();
    stop_immediately_after_ponr_cannot_replace_committed_terminal();
    applied_commit_authorization_error_aborts_exactly_before_ponr();
    applied_ponr_error_recovers_to_mandatory_success();
    throwing_exact_abort_degrades_to_a_bounded_terminal();
    staging_ready_recovery_journals_abort_before_cleanup_and_acceptance();
    post_ponr_recovery_resumes_every_durable_phase();
    promotion_error_is_resolved_by_exact_postcondition_inspection();
    recovery_identity_and_pagination_mismatches_fail_closed();
    core_authorized_prune_composes_exact_durable_cleanup();
    core_authorized_cache_delete_closes_session_and_projects_bulk_change();
    maintenance_cancellation_obeys_the_durable_point_of_no_return();
    maintenance_restart_recovery_aborts_or_completes_by_phase();
    maintenance_restart_recovery_covers_every_durable_phase();
    maintenance_faults_fail_closed_and_restart_from_durable_phase();
    shutdown_waits_for_worker_and_decision_quiescence();
    return EXIT_SUCCESS;
}
