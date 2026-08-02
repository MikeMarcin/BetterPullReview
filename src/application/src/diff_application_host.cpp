#include "bpr/application/diff_application_host.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application
{
namespace
{
struct staged_diff_completion
{
    core::diff_request_context Context;
    core::diff_options Options;
    core::diff_reason_code Reason = core::diff_reason_code::host_failure;
    std::shared_ptr<const core::computed_file_diff> Result;
    bool CacheHit = false;
};

struct diff_job
{
    core::compute_file_diff_request Request;
    std::stop_source Stop;
};

struct deferred_diff_request
{
    core::file_comparison_key Key;
    core::diff_options Options;
};

struct deferred_diff_cancel
{
};

struct deferred_diff_invalidation
{
    core::pull_request_authority Authority;
};

struct deferred_diff_shutdown
{
};

using deferred_diff_ingress =
    std::variant<deferred_diff_request,
                 deferred_diff_cancel,
                 deferred_diff_invalidation,
                 deferred_diff_shutdown>;

[[nodiscard]] auto validated_limits(const diff_host_limits limits)
    -> diff_host_limits
{
    if(!valid(limits))
    {
        throw std::invalid_argument{"Invalid diff host limits"};
    }
    return limits;
}

[[nodiscard]] auto validated_compute_limits(
    const core::diff_compute_limits limits) -> core::diff_compute_limits
{
    constexpr auto maximum = core::diff_compute_limits{};
    if(limits.MaximumBytesPerSide == 0U
       || limits.MaximumBytesPerSide > maximum.MaximumBytesPerSide
       || limits.MaximumLinesPerSide == 0U
       || limits.MaximumLinesPerSide > maximum.MaximumLinesPerSide
       || limits.MaximumMatrixCells == 0U
       || limits.MaximumMatrixCells > maximum.MaximumMatrixCells
       || limits.MaximumSegments == 0U
       || limits.MaximumSegments > maximum.MaximumSegments)
    {
        throw std::invalid_argument{"Invalid diff compute limits"};
    }
    return limits;
}

[[nodiscard]] constexpr auto reason_for_blob_status(
    const diff_blob_status status,
    const core::diff_side side) noexcept -> core::diff_reason_code
{
    switch(status)
    {
    case diff_blob_status::available:
        return core::diff_reason_code::none;
    case diff_blob_status::missing:
        return side == core::diff_side::left
            ? core::diff_reason_code::missing_left_blob
            : core::diff_reason_code::missing_right_blob;
    case diff_blob_status::binary:
        return core::diff_reason_code::binary_file;
    case diff_blob_status::decode_failure:
        return core::diff_reason_code::decode_failure;
    case diff_blob_status::materialization_limit:
        return core::diff_reason_code::materialization_limit;
    case diff_blob_status::generated_file_unsupported:
        return core::diff_reason_code::generated_file_unsupported;
    case diff_blob_status::cancelled:
        return core::diff_reason_code::cancelled;
    case diff_blob_status::failure:
        return core::diff_reason_code::host_failure;
    }
    return core::diff_reason_code::host_failure;
}

[[nodiscard]] auto exact_blob_observation(
    const diff_blob_request& request,
    const diff_blob_observation& observation) noexcept -> bool
{
    return observation.Context == request.Context
        && observation.Side == request.Side
        && observation.Blob == request.Blob
        && (observation.Status == diff_blob_status::available
            || observation.Bytes.empty());
}

[[nodiscard]] auto validate_staged(
    const core::compute_file_diff_request& request,
    const core::computed_file_diff& result) noexcept -> bool
{
    const auto digest = core::options_digest(request.Options);
    return core::valid(request.Context)
        && request.Context.Key.Algorithm
            == core::supported_diff_algorithm_revision
        && digest.has_value() && *digest == request.Context.Key.Options
        && result.Alignment.Key == request.Context.Key
        && result.Alignment.Options == request.Options
        && result.Alignment.Digest
            == core::comparison_digest(request.Context.Key)
        && core::validate_computed_file_diff(result).valid();
}

[[nodiscard]] auto checked_add(std::uint64_t& total,
                               const std::uint64_t amount) noexcept -> bool
{
    if(amount > std::numeric_limits<std::uint64_t>::max() - total)
    {
        return false;
    }
    total += amount;
    return true;
}

template<typename Value>
[[nodiscard]] auto add_vector_bytes(
    std::uint64_t& total, const std::vector<Value>& values) noexcept -> bool
{
    const auto count = static_cast<std::uint64_t>(values.capacity());
    if(count > std::numeric_limits<std::uint64_t>::max() / sizeof(Value))
    {
        return false;
    }
    return checked_add(total, count * sizeof(Value));
}

[[nodiscard]] auto retained_result_bytes(
    const core::computed_file_diff& result) noexcept
    -> std::optional<std::uint64_t>
{
    auto total = static_cast<std::uint64_t>(sizeof(result));
    if(!checked_add(total, sizeof(core::file_comparison_key))
       || !checked_add(
           total,
           sizeof(std::shared_ptr<const core::computed_file_diff>))
       || !checked_add(total, sizeof(std::uint64_t))
       || !add_vector_bytes(total, result.Alignment.Segments)
       || !add_vector_bytes(total, result.Alignment.Pairs)
       || !add_vector_bytes(total, result.Alignment.Groups)
       || !add_vector_bytes(total, result.Alignment.Moves)
       || !add_vector_bytes(total, result.Intraline))
    {
        return std::nullopt;
    }
    const auto add_document = [&total](
                                  const std::optional<
                                      core::decoded_text_document>& document) {
        return !document.has_value()
            || (add_vector_bytes(total, document->Bytes)
                && add_vector_bytes(total, document->Lines));
    };
    if(!add_document(result.LeftDocument)
       || !add_document(result.RightDocument))
    {
        return std::nullopt;
    }
    return total;
}
} // namespace

struct diff_application_host::implementation
{
    struct completion_gate
    {
        std::atomic<implementation*> Host = nullptr;
    };

    struct cache_entry
    {
        core::file_comparison_key Key;
        std::shared_ptr<const core::computed_file_diff> Result;
        std::uint64_t Bytes = 0U;
    };

    implementation(core::diff_state initial_state,
                   diff_blob_port& blobs,
                   effect_executor& effects,
                   decision_executor& decision_lane,
                   diff_projection_port& projections,
                   const diff_host_limits limits,
                   const core::diff_compute_limits compute_limits)
        : State{std::move(initial_state)}
        , Blobs{blobs}
        , Effects{effects}
        , DecisionLane{decision_lane}
        , Projections{projections}
        , Limits{validated_limits(limits)}
        , ComputeLimits{validated_compute_limits(compute_limits)}
        , Gate{std::make_shared<completion_gate>()}
    {
        Gate->Host.store(this, std::memory_order_release);
    }

    ~implementation()
    {
        Gate->Host.store(nullptr, std::memory_order_release);
    }

    void dispatch(const core::diff_event& event)
    {
        const auto output = core::transition(State, event);
        for(const auto& request : output.Requests)
        {
            std::visit(
                [this](const auto& typed_request) {
                    using request_type =
                        std::decay_t<decltype(typed_request)>;
                    if constexpr(std::is_same_v<
                                     request_type,
                                     core::compute_file_diff_request>)
                    {
                        execute(typed_request);
                    }
                    else
                    {
                        stop(typed_request.Context);
                    }
                },
                request);
        }
        if(output.Terminal.has_value()
           && output.Terminal->Reason != core::diff_reason_code::none
           && !Closing)
        {
            publish_failure(output.Terminal->Context,
                            output.Terminal->Reason);
        }
    }

    void request(core::file_comparison_key key, core::diff_options options)
    {
        enqueue(deferred_diff_request{
            .Key = std::move(key),
            .Options = options,
        });
    }

    void request_now(core::file_comparison_key key,
                     const core::diff_options options)
    {
        if(Closing)
        {
            return;
        }
        dispatch(core::request_file_diff_intent{
            .Key = std::move(key),
            .Options = options,
        });
    }

    void cancel()
    {
        enqueue(deferred_diff_cancel{});
    }

    void cancel_now()
    {
        if(!Closing)
        {
            dispatch(core::cancel_file_diff_intent{});
        }
    }

    void invalidate(const core::pull_request_authority& authority)
    {
        enqueue(deferred_diff_invalidation{authority});
    }

    void invalidate_now(const core::pull_request_authority& authority)
    {
        if(Closing)
        {
            return;
        }
        dispatch(core::invalidate_diff_authority_intent{authority});
        for(const auto& job : Jobs)
        {
            if(job->Request.Context.Key.Authority == authority)
            {
                job->Stop.request_stop();
            }
        }
        auto current = Cache.begin();
        while(current != Cache.end())
        {
            if(current->Key.Authority != authority)
            {
                ++current;
                continue;
            }
            CachedBytes -= current->Bytes;
            current = Cache.erase(current);
        }
        deliver_projection(
            [this, &authority] { Projections.invalidate(authority); });
    }

    void shutdown()
    {
        enqueue(deferred_diff_shutdown{});
    }

    void shutdown_now()
    {
        if(Closing)
        {
            return;
        }
        Closing = true;
        const auto output =
            core::transition(State, core::cancel_file_diff_intent{});
        for(const auto& request : output.Requests)
        {
            if(const auto* cancel =
                   std::get_if<core::cancel_file_diff_request>(&request))
            {
                stop(cancel->Context);
            }
        }
        for(const auto& job : Jobs)
        {
            job->Stop.request_stop();
        }
        Cache.clear();
        CachedBytes = 0U;
    }

    void enqueue(deferred_diff_ingress ingress)
    {
        if(TurnActive)
        {
            DeferredIngress.push_back(std::move(ingress));
            return;
        }
        if(DeferredIngressScheduled || !DeferredIngress.empty())
        {
            DeferredIngress.push_back(std::move(ingress));
            schedule_deferred_ingress();
            return;
        }
        run_turn(
            [this, ingress = std::move(ingress)]() mutable {
                process(std::move(ingress));
            });
    }

    void process(deferred_diff_ingress ingress)
    {
        std::visit(
            [this](auto&& typed_ingress) {
                using ingress_type = std::decay_t<decltype(typed_ingress)>;
                if constexpr(std::is_same_v<ingress_type,
                                            deferred_diff_request>)
                {
                    request_now(std::move(typed_ingress.Key),
                                typed_ingress.Options);
                }
                else if constexpr(std::is_same_v<ingress_type,
                                                 deferred_diff_cancel>)
                {
                    cancel_now();
                }
                else if constexpr(std::is_same_v<
                                      ingress_type,
                                      deferred_diff_invalidation>)
                {
                    invalidate_now(typed_ingress.Authority);
                }
                else
                {
                    shutdown_now();
                }
            },
            std::move(ingress));
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
                process(std::move(ingress));
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
        const auto gate = std::weak_ptr<completion_gate>{Gate};
        try
        {
            DecisionLane.post([gate] {
                const auto locked = gate.lock();
                if(!locked)
                {
                    return;
                }
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

    template<typename Callback>
    void deliver_projection(Callback&& callback)
    {
        try
        {
            std::forward<Callback>(callback)();
        }
        catch(...)
        {
            ProjectionDeliveryFailed = true;
        }
    }

    void accept_completion(staged_diff_completion completion)
    {
        if(TurnActive)
        {
            finish(std::move(completion));
            return;
        }
        run_turn(
            [this, completion = std::move(completion)]() mutable {
                finish(std::move(completion));
            });
    }

    void execute(const core::compute_file_diff_request& request)
    {
        if(pending_work() >= Limits.OutstandingJobs)
        {
            finish(staged_diff_completion{
                .Context = request.Context,
                .Options = request.Options,
                .Reason = core::diff_reason_code::compute_limit,
            });
            return;
        }
        if(const auto cached = lookup(request); cached != nullptr)
        {
            post_local(staged_diff_completion{
                .Context = request.Context,
                .Options = request.Options,
                .Reason = core::diff_reason_code::none,
                .Result = std::move(cached),
                .CacheHit = true,
            });
            return;
        }
        auto job = std::shared_ptr<diff_job>{};
        try
        {
            job = std::make_shared<diff_job>();
            job->Request = request;
            Jobs.push_back(job);
        }
        catch(...)
        {
            finish(staged_diff_completion{
                .Context = request.Context,
                .Options = request.Options,
                .Reason = core::diff_reason_code::host_failure,
            });
            return;
        }
        const auto gate = std::weak_ptr<completion_gate>{Gate};
        auto* const blobs = &Blobs;
        auto* const lane = &DecisionLane;
        const auto compute_limits = ComputeLimits;
        try
        {
            Effects.post([job, gate, blobs, lane, compute_limits] {
                auto completion = perform(job, *blobs, compute_limits);
                lane->post([gate, completion = std::move(completion)]() mutable {
                    const auto locked = gate.lock();
                    if(!locked)
                    {
                        return;
                    }
                    auto* const host =
                        locked->Host.load(std::memory_order_acquire);
                    if(host != nullptr)
                    {
                        host->accept_completion(std::move(completion));
                    }
                });
            });
        }
        catch(...)
        {
            erase_job(request.Context);
            post_local(staged_diff_completion{
                .Context = request.Context,
                .Options = request.Options,
                .Reason = core::diff_reason_code::host_failure,
            });
        }
    }

    [[nodiscard]] static auto perform(
        const std::shared_ptr<diff_job>& job,
        diff_blob_port& blobs,
        const core::diff_compute_limits& limits) -> staged_diff_completion
    {
        auto completion = staged_diff_completion{
            .Context = job->Request.Context,
            .Options = job->Request.Options,
            .Reason = core::diff_reason_code::host_failure,
        };
        try
        {
            const auto stop = job->Stop.get_token();
            if(stop.stop_requested())
            {
                completion.Reason = core::diff_reason_code::cancelled;
                return completion;
            }
            auto left = std::optional<std::vector<std::uint8_t>>{};
            auto right = std::optional<std::vector<std::uint8_t>>{};
            if(job->Request.Context.Key.LeftBlob.has_value())
            {
                const auto request = diff_blob_request{
                    .Context = job->Request.Context,
                    .Side = core::diff_side::left,
                    .Blob = *job->Request.Context.Key.LeftBlob,
                    .MaximumBytes = limits.MaximumBytesPerSide,
                };
                auto observation = blobs.materialize(request, stop);
                if(!exact_blob_observation(request, observation))
                {
                    return completion;
                }
                completion.Reason =
                    reason_for_blob_status(observation.Status, request.Side);
                if(completion.Reason != core::diff_reason_code::none)
                {
                    return completion;
                }
                if(std::cmp_greater(observation.Bytes.size(),
                                    request.MaximumBytes))
                {
                    completion.Reason =
                        core::diff_reason_code::materialization_limit;
                    return completion;
                }
                left = std::move(observation.Bytes);
            }
            if(stop.stop_requested())
            {
                completion.Reason = core::diff_reason_code::cancelled;
                return completion;
            }
            if(job->Request.Context.Key.RightBlob.has_value())
            {
                const auto request = diff_blob_request{
                    .Context = job->Request.Context,
                    .Side = core::diff_side::right,
                    .Blob = *job->Request.Context.Key.RightBlob,
                    .MaximumBytes = limits.MaximumBytesPerSide,
                };
                auto observation = blobs.materialize(request, stop);
                if(!exact_blob_observation(request, observation))
                {
                    return completion;
                }
                completion.Reason =
                    reason_for_blob_status(observation.Status, request.Side);
                if(completion.Reason != core::diff_reason_code::none)
                {
                    return completion;
                }
                if(std::cmp_greater(observation.Bytes.size(),
                                    request.MaximumBytes))
                {
                    completion.Reason =
                        core::diff_reason_code::materialization_limit;
                    return completion;
                }
                right = std::move(observation.Bytes);
            }
            if(stop.stop_requested())
            {
                completion.Reason = core::diff_reason_code::cancelled;
                return completion;
            }
            const auto input = core::diff_compute_input{
                .Key = job->Request.Context.Key,
                .Options = job->Request.Options,
                .LeftBytes =
                    left.has_value()
                        ? std::optional{std::span<const std::uint8_t>{*left}}
                        : std::nullopt,
                .RightBytes =
                    right.has_value()
                        ? std::optional{std::span<const std::uint8_t>{*right}}
                        : std::nullopt,
                .Limits = limits,
            };
            auto outcome = core::compute_file_diff(input);
            if(stop.stop_requested())
            {
                completion.Reason = core::diff_reason_code::cancelled;
                return completion;
            }
            if(!outcome.Result.has_value())
            {
                completion.Reason =
                    outcome.Reason == core::diff_reason_code::none
                    ? core::diff_reason_code::invalid_alignment
                    : outcome.Reason;
                return completion;
            }
            if(!validate_staged(job->Request, *outcome.Result))
            {
                completion.Reason = core::diff_reason_code::invalid_alignment;
                return completion;
            }
            completion.Result = std::make_shared<const core::computed_file_diff>(
                std::move(*outcome.Result));
            completion.Reason = core::diff_reason_code::none;
            return completion;
        }
        catch(...)
        {
            completion.Result.reset();
            completion.Reason = core::diff_reason_code::host_failure;
            return completion;
        }
    }

    void post_local(staged_diff_completion completion)
    {
        ++PendingDecisionCompletions;
        const auto gate = std::weak_ptr<completion_gate>{Gate};
        DecisionLane.post([gate, completion = std::move(completion)]() mutable {
            const auto locked = gate.lock();
            if(!locked)
            {
                return;
            }
            auto* const host =
                locked->Host.load(std::memory_order_acquire);
            if(host != nullptr)
            {
                --host->PendingDecisionCompletions;
                host->accept_completion(std::move(completion));
            }
        });
    }

    void finish(staged_diff_completion completion)
    {
        erase_job(completion.Context);
        if(completion.Reason == core::diff_reason_code::none)
        {
            if(!completion.Result
               || !validate_staged(
                   core::compute_file_diff_request{
                       .Context = completion.Context,
                       .Options = completion.Options,
                   },
                   *completion.Result))
            {
                completion.Result.reset();
                completion.Reason = core::diff_reason_code::invalid_alignment;
            }
        }

        const auto output = completion.Reason == core::diff_reason_code::none
            ? core::transition(State,
                               core::file_diff_computed_event{
                                   .Context = completion.Context,
                               })
            : core::transition(State,
                               core::file_diff_failed_event{
                                   .Context = completion.Context,
                                   .Reason = completion.Reason,
                               });
        if(Closing || !output.Terminal.has_value())
        {
            return;
        }
        if(output.Accepted.has_value() && completion.Result)
        {
            try
            {
                store(completion.Result);
            }
            catch(...)
            {
                Cache.clear();
                CachedBytes = 0U;
            }
            deliver_projection([this, &completion] {
                Projections.publish(diff_publication{
                    .Context = completion.Context,
                    .Result = completion.Result,
                    .CacheHit = completion.CacheHit,
                });
            });
            return;
        }
        publish_failure(output.Terminal->Context, output.Terminal->Reason);
    }

    void publish_failure(const core::diff_request_context& context,
                         const core::diff_reason_code reason)
    {
        deliver_projection([this, &context, reason] {
            Projections.fail(diff_failure_publication{
                .Context = context,
                .Reason = reason,
            });
        });
    }

    void stop(const core::diff_request_context& context) noexcept
    {
        const auto found = std::ranges::find_if(
            Jobs, [&context](const auto& job) {
                return job->Request.Context == context;
            });
        if(found != Jobs.end())
        {
            (*found)->Stop.request_stop();
        }
    }

    [[nodiscard]] auto pending_work() const noexcept -> std::size_t
    {
        return Jobs.size() + PendingDecisionCompletions
            + DeferredIngress.size();
    }

    void erase_job(const core::diff_request_context& context)
    {
        const auto found = std::ranges::find_if(
            Jobs, [&context](const auto& job) {
                return job->Request.Context == context;
            });
        if(found != Jobs.end())
        {
            Jobs.erase(found);
        }
    }

    [[nodiscard]] auto lookup(const core::compute_file_diff_request& request)
        -> std::shared_ptr<const core::computed_file_diff>
    {
        const auto found = std::ranges::find_if(
            Cache, [&request](const cache_entry& entry) {
                return entry.Key == request.Context.Key;
            });
        if(found == Cache.end())
        {
            return {};
        }
        if(!validate_staged(request, *found->Result))
        {
            CachedBytes -= found->Bytes;
            Cache.erase(found);
            return {};
        }
        return found->Result;
    }

    void store(const std::shared_ptr<const core::computed_file_diff>& result)
    {
        const auto bytes = retained_result_bytes(*result);
        if(!bytes.has_value() || *bytes > Limits.CacheBytes)
        {
            return;
        }
        const auto duplicate = std::ranges::find_if(
            Cache, [&result](const cache_entry& entry) {
                return entry.Key == result->Alignment.Key;
            });
        if(duplicate != Cache.end())
        {
            CachedBytes -= duplicate->Bytes;
            Cache.erase(duplicate);
        }
        while(!Cache.empty()
              && (Cache.size() >= Limits.CacheEntries
                  || CachedBytes > Limits.CacheBytes - *bytes))
        {
            CachedBytes -= Cache.front().Bytes;
            Cache.erase(Cache.begin());
        }
        Cache.push_back(cache_entry{
            .Key = result->Alignment.Key,
            .Result = result,
            .Bytes = *bytes,
        });
        CachedBytes += *bytes;
    }

    core::diff_state State;
    diff_blob_port& Blobs;
    effect_executor& Effects;
    decision_executor& DecisionLane;
    diff_projection_port& Projections;
    diff_host_limits Limits;
    core::diff_compute_limits ComputeLimits;
    std::shared_ptr<completion_gate> Gate;
    std::vector<std::shared_ptr<diff_job>> Jobs;
    std::vector<cache_entry> Cache;
    std::uint64_t CachedBytes = 0U;
    std::size_t PendingDecisionCompletions = 0U;
    std::deque<deferred_diff_ingress> DeferredIngress;
    bool Closing = false;
    bool ProjectionDeliveryFailed = false;
    bool TurnActive = false;
    bool DeferredIngressScheduled = false;
};

diff_application_host::diff_application_host(
    core::diff_state initial_state,
    diff_blob_port& blobs,
    effect_executor& effects,
    decision_executor& decision_lane,
    diff_projection_port& projections,
    const diff_host_limits limits,
    const core::diff_compute_limits compute_limits)
    : Implementation{std::make_unique<implementation>(
          std::move(initial_state),
          blobs,
          effects,
          decision_lane,
          projections,
          limits,
          compute_limits)}
{
}

diff_application_host::~diff_application_host()
{
    Implementation->shutdown_now();
}

void diff_application_host::request(core::file_comparison_key key,
                                    core::diff_options options)
{
    Implementation->request(std::move(key), options);
}

void diff_application_host::cancel()
{
    Implementation->cancel();
}

void diff_application_host::invalidate(
    const core::pull_request_authority& authority)
{
    Implementation->invalidate(authority);
}

void diff_application_host::shutdown()
{
    Implementation->shutdown();
}

auto diff_application_host::state() const noexcept -> const core::diff_state&
{
    return Implementation->State;
}

auto diff_application_host::is_closing() const noexcept -> bool
{
    return Implementation->Closing;
}

auto diff_application_host::is_quiescent() const noexcept -> bool
{
    return Implementation->Closing && Implementation->Jobs.empty()
        && Implementation->PendingDecisionCompletions == 0U
        && Implementation->DeferredIngress.empty()
        && !Implementation->DeferredIngressScheduled
        && !Implementation->TurnActive;
}

auto diff_application_host::outstanding_jobs() const noexcept -> std::size_t
{
    return Implementation->pending_work();
}

auto diff_application_host::cache_entries() const noexcept -> std::size_t
{
    return Implementation->Cache.size();
}

auto diff_application_host::cache_bytes() const noexcept -> std::uint64_t
{
    return Implementation->CachedBytes;
}

auto diff_application_host::projection_delivery_failed() const noexcept
    -> bool
{
    return Implementation->ProjectionDeliveryFailed;
}
} // namespace bpr::application
