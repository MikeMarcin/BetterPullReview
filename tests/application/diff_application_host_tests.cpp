#include "bpr/application/diff_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace bpr;

auto expect(const bool condition, const std::string_view message) -> void
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
    std::array<std::uint8_t, 32U> bytes{};
    bytes[0] = seed;
    bytes[31] = static_cast<std::uint8_t>(seed ^ 0xA5U);
    return Identity{bytes};
}

[[nodiscard]] constexpr auto blob(const std::uint8_t seed)
    -> core::git_blob_oid
{
    std::array<std::uint8_t, 32U> bytes{};
    for(std::size_t index = 0U; index < 20U; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return core::git_blob_oid{core::git_hash_algorithm::sha1, bytes};
}

[[nodiscard]] auto bytes(const std::string_view text)
    -> std::vector<std::uint8_t>
{
    return {text.begin(), text.end()};
}

[[nodiscard]] auto comparison_key(
    const core::diff_options& options = {},
    const std::uint8_t file_seed = 31U,
    const bool has_left = true,
    const bool has_right = true) -> core::file_comparison_key
{
    const auto digest = core::options_digest(options);
    expect(digest.has_value(), "test options normalize");
    const auto scope = opaque<core::pull_request_authority_scope_id>(11U);
    const auto incarnation = opaque<core::epoch_catalog_incarnation_id>(12U);
    const auto authority = core::pull_request_authority{
        .Profile = core::host_profile_id{10U},
        .ProfileRevision = core::github_profile_revision{20U},
        .Host = core::git_host_id{30U},
        .HostAuthority = opaque<core::host_authority_fingerprint>(13U),
        .Account = core::github_account_id{40U},
        .Viewer = core::github_viewer_actor_id{50U},
        .Repository = core::repository_id{101U},
        .Cache = opaque<core::repository_cache_scope_id>(14U),
        .PullRequest = core::pull_request_id{202U},
        .Number = core::pull_request_number{17U},
        .Scope = scope,
    };
    return core::file_comparison_key{
        .Authority = authority,
        .Repository = authority.Repository,
        .PullRequest = authority.PullRequest,
        .LeftAnchor = core::comparison_anchor_identity{
            .Authority = scope,
            .Incarnation = incarnation,
            .Kind = core::comparison_anchor_kind::epoch_base,
            .Epoch = core::epoch_ordinal{1},
        },
        .RightAnchor = core::comparison_anchor_identity{
            .Authority = scope,
            .Incarnation = incarnation,
            .Kind = core::comparison_anchor_kind::epoch_head,
            .Epoch = core::epoch_ordinal{2},
        },
        .LeftBlob = has_left ? std::optional{blob(21U)} : std::nullopt,
        .RightBlob = has_right ? std::optional{blob(51U)} : std::nullopt,
        .File = opaque<core::file_identity>(file_seed),
        .Options = *digest,
        .Algorithm = core::supported_diff_algorithm_revision,
    };
}

class fake_blobs final : public application::diff_blob_port
{
public:
    [[nodiscard]] auto materialize(
        const application::diff_blob_request& request,
        const std::stop_token stop)
        -> application::diff_blob_observation override
    {
        Requests.push_back(request);
        StopObserved.push_back(stop.stop_requested());
        if(Throw)
        {
            throw std::runtime_error{"blob failure"};
        }
        auto observation = application::diff_blob_observation{
            .Context = request.Context,
            .Side = request.Side,
            .Blob = request.Blob,
            .Status = application::diff_blob_status::available,
        };
        if(DeclaredBlobBytes.has_value()
           && *DeclaredBlobBytes > request.MaximumBytes)
        {
            RefusedBeforeAllocation = true;
            observation.Status =
                application::diff_blob_status::materialization_limit;
            observation.Bytes.clear();
            return observation;
        }
        observation.Bytes = request.Side == core::diff_side::left ? LeftBytes
                                                                   : RightBytes;
        if(Status.has_value()
           && (!StatusSide.has_value() || *StatusSide == request.Side))
        {
            observation.Status = *Status;
            if(observation.Status != application::diff_blob_status::available)
            {
                observation.Bytes.clear();
            }
        }
        if(MismatchContext)
        {
            observation.Context.Generation = core::request_generation{999U};
        }
        if(MismatchSide)
        {
            observation.Side = request.Side == core::diff_side::left
                ? core::diff_side::right
                : core::diff_side::left;
        }
        if(MismatchBlob)
        {
            observation.Blob = blob(99U);
        }
        return observation;
    }

    void reset_behavior()
    {
        Status.reset();
        StatusSide.reset();
        Throw = false;
        MismatchContext = false;
        MismatchSide = false;
        MismatchBlob = false;
        DeclaredBlobBytes.reset();
        RefusedBeforeAllocation = false;
        LeftBytes = bytes("left\ncommon\n");
        RightBytes = bytes("right\ncommon\n");
    }

    std::vector<application::diff_blob_request> Requests;
    std::vector<bool> StopObserved;
    std::vector<std::uint8_t> LeftBytes = bytes("left\ncommon\n");
    std::vector<std::uint8_t> RightBytes = bytes("right\ncommon\n");
    std::optional<application::diff_blob_status> Status;
    std::optional<core::diff_side> StatusSide;
    bool Throw = false;
    bool MismatchContext = false;
    bool MismatchSide = false;
    bool MismatchBlob = false;
    std::optional<std::uint64_t> DeclaredBlobBytes;
    bool RefusedBeforeAllocation = false;
};

class recording_projection final : public application::diff_projection_port
{
public:
    struct result_record
    {
        core::diff_request_context Context;
        core::computed_file_diff Result;
        bool CacheHit = false;
    };

    void publish(const application::diff_publication& publication) override
    {
        if(Throw)
        {
            throw std::runtime_error{"projection failure"};
        }
        begin_delivery();
        expect(publication.Result != nullptr, "success has immutable result");
        Results.push_back(result_record{
            .Context = publication.Context,
            .Result = *publication.Result,
            .CacheHit = publication.CacheHit,
        });
        run_callback(OnPublish);
        end_delivery();
    }

    void fail(const application::diff_failure_publication& failure) override
    {
        if(Throw)
        {
            throw std::runtime_error{"projection failure"};
        }
        begin_delivery();
        Failures.push_back(failure);
        run_callback(OnFail);
        end_delivery();
    }

    void invalidate(
        const core::pull_request_authority& authority) override
    {
        if(Throw)
        {
            throw std::runtime_error{"projection failure"};
        }
        begin_delivery();
        Invalidated.push_back(authority);
        run_callback(OnInvalidate);
        end_delivery();
    }

    void begin_delivery()
    {
        NestedDelivery = NestedDelivery || Delivering;
        Delivering = true;
    }

    void end_delivery()
    {
        Delivering = false;
    }

    static void run_callback(std::function<void()>& callback)
    {
        auto current = std::move(callback);
        callback = {};
        if(current)
        {
            current();
        }
    }

    std::vector<result_record> Results;
    std::vector<application::diff_failure_publication> Failures;
    std::vector<core::pull_request_authority> Invalidated;
    bool Throw = false;
    bool Delivering = false;
    bool NestedDelivery = false;
    std::function<void()> OnPublish;
    std::function<void()> OnFail;
    std::function<void()> OnInvalidate;
};

class throwing_effect_executor final : public application::effect_executor
{
public:
    void post(std::function<void()>) override
    {
        throw std::runtime_error{"effect submission failure"};
    }
};

struct fixture
{
    explicit fixture(
        const application::diff_host_limits limits = {},
        const core::diff_compute_limits compute_limits = {})
        : Host{std::make_unique<application::diff_application_host>(
              core::diff_state{
                  .ApplicationSession = core::application_session_id{71U},
              },
              Blobs,
              Effects,
              Decisions,
              Projections,
              limits,
              compute_limits)}
    {
    }

    ~fixture()
    {
        Host->shutdown();
        Effects.drain();
        Decisions.drain();
    }

    void request(const core::file_comparison_key& key,
                 const core::diff_options& options = {})
    {
        Host->request(key, options);
    }

    void run_effect_and_decision()
    {
        expect(Effects.pending() == 1U, "one effect is pending");
        Effects.run_next();
        expect(Decisions.pending() == 1U,
               "effect posts one serialized terminal");
        Decisions.run_next();
    }

    fake_blobs Blobs;
    application::manual_executor Effects;
    application::manual_executor Decisions;
    recording_projection Projections;
    std::unique_ptr<application::diff_application_host> Host;
};

auto test_worker_and_cache_lanes() -> void
{
    fixture test;
    const auto key = comparison_key();
    test.request(key);
    expect(test.Effects.pending() == 1U && test.Decisions.pending() == 0U
               && test.Projections.Results.empty(),
           "request schedules only the effect lane");
    test.Effects.run_next();
    expect(test.Decisions.pending() == 1U
               && test.Projections.Results.empty()
               && !test.Host->state().Current.has_value(),
           "worker cannot accept or publish off decision lane");
    expect(!test.Blobs.Requests.empty()
               && test.Blobs.Requests.front().MaximumBytes
                   == core::diff_compute_limits{}.MaximumBytesPerSide,
           "blob acquisition receives the exact configured byte ceiling");
    test.Decisions.run_next();
    expect(test.Projections.Results.size() == 1U
               && !test.Projections.Results.back().CacheHit
               && test.Host->cache_entries() == 1U
               && test.Host->cache_bytes() > 0U,
           "validated accepted result publishes and enters bounded cache");
    const auto calls = test.Blobs.Requests.size();

    test.request(key);
    expect(test.Effects.pending() == 0U && test.Decisions.pending() == 1U
               && test.Projections.Results.size() == 1U,
           "cache hit still traverses serialized decision lane");
    test.Decisions.run_next();
    expect(test.Projections.Results.size() == 2U
               && test.Projections.Results.back().CacheHit
               && test.Blobs.Requests.size() == calls,
           "exact cache hit performs no blob acquisition");
}

auto complete_and_count_miss(fixture& test,
                             const core::file_comparison_key& key,
                             const core::diff_options& options,
                             std::size_t& calls) -> void
{
    test.request(key, options);
    expect(test.Effects.pending() == 1U, "identity change misses cache");
    test.run_effect_and_decision();
    expect(test.Blobs.Requests.size() > calls,
           "cache collision requires exact blob materialization");
    calls = test.Blobs.Requests.size();
}

auto test_exact_cache_key_collisions() -> void
{
    fixture test;
    const auto base = comparison_key();
    test.request(base);
    test.run_effect_and_decision();
    auto calls = test.Blobs.Requests.size();

    auto authority = base;
    authority.Authority.Account = core::github_account_id{41U};
    complete_and_count_miss(test, authority, {}, calls);

    auto repository = base;
    repository.Repository = core::repository_id{102U};
    repository.Authority.Repository = repository.Repository;
    complete_and_count_miss(test, repository, {}, calls);

    auto pull_request = base;
    pull_request.PullRequest = core::pull_request_id{203U};
    pull_request.Authority.PullRequest = pull_request.PullRequest;
    pull_request.Authority.Number = core::pull_request_number{18U};
    complete_and_count_miss(test, pull_request, {}, calls);

    auto anchors = base;
    anchors.RightAnchor.Epoch = core::epoch_ordinal{3};
    complete_and_count_miss(test, anchors, {}, calls);

    auto file = base;
    file.File = opaque<core::file_identity>(32U);
    complete_and_count_miss(test, file, {}, calls);

    auto blobs = base;
    blobs.LeftBlob = blob(22U);
    complete_and_count_miss(test, blobs, {}, calls);

    auto options = core::diff_options{};
    options.Whitespace = core::whitespace_policy::ignore_trailing;
    auto option_key = comparison_key(options);
    complete_and_count_miss(test, option_key, options, calls);

    auto unsupported = base;
    unsupported.Algorithm = core::diff_algorithm_revision{2U};
    test.request(unsupported);
    expect(test.Effects.pending() == 0U
               && test.Decisions.pending() == 0U
               && test.Host->state().LastReason
                   == core::diff_reason_code::host_failure,
           "unsupported algorithm revision cannot cross into cache or worker");
}

auto test_reordered_generation_and_stale_completion() -> void
{
    fixture test;
    const auto key = comparison_key();
    test.request(key);
    test.request(key);
    expect(test.Effects.pending() == 2U && test.Host->outstanding_jobs() == 2U,
           "same-key supersession retains bounded in-flight terminals");

    test.Effects.run(1U);
    test.Effects.run(0U);
    expect(test.Decisions.pending() == 2U,
           "manual effect lane can reverse generation completion order");
    test.Decisions.run(1U);
    expect(test.Projections.Results.empty(),
           "cancelled older generation cannot publish when delivered first");
    test.Decisions.run(0U);
    expect(test.Projections.Results.size() == 1U
               && test.Projections.Failures.empty()
               && test.Host->outstanding_jobs() == 0U,
           "only exact current generation publishes after reordering");
}

auto test_invalid_replacement_supersedes_pending_work() -> void
{
    for(const auto unsupported_replacement : {false, true})
    {
        fixture test;
        const auto base = comparison_key();
        test.request(base);
        auto replacement = base;
        auto options = core::diff_options{};
        if(unsupported_replacement)
        {
            replacement.Algorithm = core::diff_algorithm_revision{2U};
        }
        else
        {
            options.ContextLines = 9U;
        }
        test.request(replacement, options);
        expect(!test.Host->state().Pending.has_value(),
               "invalid replacement revokes the older pending context");
        test.run_effect_and_decision();
        expect(test.Projections.Results.empty()
                   && test.Projections.Failures.empty(),
               "revoked work cannot publish after invalid replacement");
    }
}

auto test_reordered_exact_identity_boundaries() -> void
{
    const auto base = comparison_key();
    auto changed = std::vector<
        std::pair<core::file_comparison_key, core::diff_options>>{};

    auto authority = base;
    authority.Authority.Account = core::github_account_id{41U};
    changed.emplace_back(authority, core::diff_options{});
    auto repository = base;
    repository.Repository = core::repository_id{102U};
    repository.Authority.Repository = repository.Repository;
    changed.emplace_back(repository, core::diff_options{});
    auto pull_request = base;
    pull_request.PullRequest = core::pull_request_id{203U};
    pull_request.Authority.PullRequest = pull_request.PullRequest;
    pull_request.Authority.Number = core::pull_request_number{18U};
    changed.emplace_back(pull_request, core::diff_options{});
    auto epoch = base;
    epoch.RightAnchor.Epoch = core::epoch_ordinal{3};
    changed.emplace_back(epoch, core::diff_options{});
    auto file = base;
    file.File = opaque<core::file_identity>(32U);
    changed.emplace_back(file, core::diff_options{});
    auto changed_blob = base;
    changed_blob.RightBlob = blob(52U);
    changed.emplace_back(changed_blob, core::diff_options{});
    auto options = core::diff_options{};
    options.LineEndings = core::line_ending_policy::normalize;
    changed.emplace_back(comparison_key(options), options);

    for(const auto& [new_key, new_options] : changed)
    {
        fixture test;
        test.request(base);
        test.Effects.run_next();
        test.request(new_key, new_options);
        test.Effects.run_next();
        expect(test.Decisions.pending() == 2U,
               "two exact identity terminals can be staged");
        test.Decisions.run_next();
        expect(test.Projections.Results.empty(),
               "old authority/repository/PR/epoch/file/blob/options result is stale");
        test.Decisions.run_next();
        expect(test.Projections.Results.size() == 1U
                   && test.Projections.Results.back().Result.Alignment.Key
                       == new_key,
               "only the complete current identity publishes");
    }

    fixture cache_test;
    cache_test.request(base);
    cache_test.run_effect_and_decision();
    cache_test.request(base);
    const auto miss = comparison_key({}, 32U);
    cache_test.request(miss);
    cache_test.Effects.run_next();
    expect(cache_test.Decisions.pending() == 2U,
           "cache hit and cache miss both stage serialized terminals");
    cache_test.Decisions.run_next();
    expect(cache_test.Projections.Results.size() == 1U,
           "superseded cache hit cannot bypass core stale rejection");
    cache_test.Decisions.run_next();
    expect(cache_test.Projections.Results.size() == 2U
               && !cache_test.Projections.Results.back().CacheHit
               && cache_test.Projections.Results.back().Result.Alignment.Key
                   == miss,
           "current cache miss publishes after reordered stale hit");
}

auto test_cancellation_and_shutdown_stages() -> void
{
    {
        fixture test;
        test.request(comparison_key());
        test.Host->cancel();
        expect(test.Projections.Failures.size() == 1U
                   && test.Projections.Failures.back().Reason
                       == core::diff_reason_code::cancelled,
               "explicit cancellation is a typed current terminal");
        test.run_effect_and_decision();
        expect(test.Blobs.Requests.empty()
                   && test.Projections.Failures.size() == 1U,
               "stop is observed before blob acquisition and late terminal is stale");
    }
    {
        fixture test;
        test.Blobs.Status = application::diff_blob_status::cancelled;
        test.Blobs.StatusSide = core::diff_side::right;
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Blobs.Requests.size() == 2U
                   && test.Projections.Failures.size() == 1U
                   && test.Projections.Failures.back().Reason
                       == core::diff_reason_code::cancelled,
               "cancellation between side acquisitions suppresses computation");
    }
    {
        fixture test;
        test.request(comparison_key());
        test.Effects.run_next();
        test.Host->cancel();
        test.Decisions.run_next();
        expect(test.Projections.Results.empty()
                   && test.Projections.Failures.size() == 1U,
               "cancellation after computation suppresses staged publication");
    }
    {
        fixture test;
        test.request(comparison_key());
        test.Effects.run_next();
        test.Host->shutdown();
        expect(!test.Host->is_quiescent(),
               "shutdown waits for already posted decision terminal");
        test.Decisions.run_next();
        expect(test.Host->is_quiescent()
                   && test.Projections.Results.empty()
                   && test.Projections.Failures.empty(),
               "shutdown drains without publishing staged work");
    }
}

auto test_job_and_cache_bounds() -> void
{
    {
        auto threw = false;
        try
        {
            fixture test{application::diff_host_limits{
                .OutstandingJobs = 0U,
                .CacheEntries = 1U,
                .CacheBytes = 1U,
            }};
        }
        catch(const std::invalid_argument&)
        {
            threw = true;
        }
        expect(threw, "invalid configured bounds fail instead of expanding");
    }
    {
        auto threw = false;
        try
        {
            auto compute_limits = core::diff_compute_limits{};
            ++compute_limits.MaximumBytesPerSide;
            fixture test{{}, compute_limits};
        }
        catch(const std::invalid_argument&)
        {
            threw = true;
        }
        expect(threw,
               "host rejects compute ceilings above the product maximum");
    }
    {
        fixture test{application::diff_host_limits{
            .OutstandingJobs = 1U,
            .CacheEntries = 2U,
            .CacheBytes = 1024U * 1024U,
        }};
        test.request(comparison_key({}, 31U));
        test.request(comparison_key({}, 32U));
        expect(test.Host->outstanding_jobs() == 1U
                   && test.Decisions.pending() == 0U,
               "outstanding work bound never over-enqueues a terminal");
        expect(test.Projections.Failures.size() == 1U
                   && test.Projections.Failures.back().Reason
                       == core::diff_reason_code::compute_limit,
               "job exhaustion terminates on the serialized caller lane");
        test.Effects.run_next();
        test.Decisions.run_next();
    }
    {
        fixture test{application::diff_host_limits{
            .OutstandingJobs = 2U,
            .CacheEntries = 1U,
            .CacheBytes = 1024U * 1024U,
        }};
        const auto first = comparison_key({}, 31U);
        const auto second = comparison_key({}, 32U);
        test.request(first);
        test.run_effect_and_decision();
        test.request(second);
        test.run_effect_and_decision();
        expect(test.Host->cache_entries() == 1U,
               "cache entry capacity evicts deterministically");
        const auto calls = test.Blobs.Requests.size();
        test.request(first);
        expect(test.Effects.pending() == 1U,
               "evicted exact key misses rather than path-colliding");
        test.run_effect_and_decision();
        expect(test.Blobs.Requests.size() > calls,
               "evicted result is rematerialized");
    }
    {
        fixture test{application::diff_host_limits{
            .OutstandingJobs = 1U,
            .CacheEntries = 1U,
            .CacheBytes = 1U,
        }};
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Host->cache_entries() == 0U
                   && test.Host->cache_bytes() == 0U,
               "oversized cache value is published but never retained");
    }
}

auto test_terminal_queue_is_bounded() -> void
{
    fixture test{application::diff_host_limits{
        .OutstandingJobs = 1U,
        .CacheEntries = 1U,
        .CacheBytes = 1024U * 1024U,
    }};
    const auto key = comparison_key();
    test.request(key);
    test.run_effect_and_decision();
    for(auto index = 0U; index < 100U; ++index)
    {
        test.request(key);
        expect(test.Host->outstanding_jobs() <= 1U
                   && test.Decisions.pending() <= 1U,
               "cache terminals and failures share the outstanding bound");
    }
    test.Decisions.drain();
    expect(test.Host->outstanding_jobs() == 0U,
           "bounded terminal queue drains exactly");
}

auto test_absent_and_empty_sides() -> void
{
    fixture test;
    test.Blobs.RightBytes.clear();
    const auto key = comparison_key({}, 31U, false, true);
    test.request(key);
    test.run_effect_and_decision();
    expect(test.Blobs.Requests.size() == 1U
               && test.Blobs.Requests.front().Side == core::diff_side::right,
           "absent left side performs no fake blob lookup");
    expect(test.Projections.Results.size() == 1U
               && !test.Projections.Results.front().Result.LeftDocument
               && test.Projections.Results.front().Result.RightDocument
                   .has_value()
               && test.Projections.Results.front()
                      .Result.RightDocument->Bytes.empty(),
           "absent side and present empty blob remain distinguishable");
}

auto expect_failure(application::diff_blob_status status,
                    core::diff_reason_code reason,
                    const std::string_view message) -> void
{
    fixture test;
    test.Blobs.Status = status;
    test.request(comparison_key());
    test.run_effect_and_decision();
    expect(test.Projections.Failures.size() == 1U
               && test.Projections.Failures.back().Reason == reason
               && test.Projections.Results.empty(),
           message);
}

auto test_typed_failures_and_exact_echo() -> void
{
    expect_failure(application::diff_blob_status::missing,
                   core::diff_reason_code::missing_left_blob,
                   "missing expected blob is side-specific");
    {
        fixture test;
        test.Blobs.Status = application::diff_blob_status::missing;
        test.Blobs.StatusSide = core::diff_side::right;
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Projections.Failures.back().Reason
                   == core::diff_reason_code::missing_right_blob,
               "missing right blob retains right-side reason identity");
    }
    expect_failure(application::diff_blob_status::binary,
                   core::diff_reason_code::binary_file,
                   "blob binary policy is typed");
    expect_failure(application::diff_blob_status::decode_failure,
                   core::diff_reason_code::decode_failure,
                   "decoder failure is typed");
    expect_failure(application::diff_blob_status::materialization_limit,
                   core::diff_reason_code::materialization_limit,
                   "materialization limit is typed");
    expect_failure(application::diff_blob_status::generated_file_unsupported,
                   core::diff_reason_code::generated_file_unsupported,
                   "generated policy is typed");
    expect_failure(application::diff_blob_status::failure,
                   core::diff_reason_code::host_failure,
                   "blob host failure is typed");

    {
        fixture test;
        test.Blobs.Throw = true;
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Projections.Failures.back().Reason
                   == core::diff_reason_code::host_failure,
               "worker exceptions are contained");
    }
    {
        fake_blobs blobs;
        throwing_effect_executor effects;
        application::manual_executor decisions;
        recording_projection projections;
        application::diff_application_host host{
            core::diff_state{
                .ApplicationSession = core::application_session_id{72U},
            },
            blobs,
            effects,
            decisions,
            projections,
        };
        host.request(comparison_key(), {});
        expect(decisions.pending() == 1U && host.outstanding_jobs() == 1U,
               "effect submission exception becomes a bounded terminal");
        decisions.run_next();
        expect(projections.Failures.size() == 1U
                   && projections.Failures.back().Reason
                       == core::diff_reason_code::host_failure
                   && host.outstanding_jobs() == 0U,
               "effect submission exception is contained on decision lane");
        host.shutdown();
    }
    for(const auto mismatch : {0U, 1U, 2U})
    {
        fixture test;
        test.Blobs.MismatchContext = mismatch == 0U;
        test.Blobs.MismatchSide = mismatch == 1U;
        test.Blobs.MismatchBlob = mismatch == 2U;
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Projections.Failures.back().Reason
                   == core::diff_reason_code::host_failure,
               "context, side, and blob echoes are exact");
    }
    {
        auto limits = core::diff_compute_limits{};
        limits.MaximumBytesPerSide = 4U;
        fixture test{{}, limits};
        test.Blobs.DeclaredBlobBytes = 5U;
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Blobs.RefusedBeforeAllocation
                   && test.Blobs.Requests.front().MaximumBytes == 4U
                   && test.Projections.Failures.back().Reason
                       == core::diff_reason_code::materialization_limit,
               "port refuses oversized material before allocating bytes");
    }
    {
        fixture test;
        test.Blobs.LeftBytes = {0U};
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Projections.Failures.empty()
                   && test.Projections.Results.size() == 1U
                   && test.Projections.Results.back()
                          .Result.Alignment.Content
                       == core::diff_content_state::binary
                   && test.Projections.Results.back()
                          .Result.Alignment.Diagnostic.Code
                       == core::diff_reason_code::binary_file
                   && test.Projections.Results.back()
                          .Result.Alignment.Segments.empty(),
               "validated binary result publishes its honest unavailable semantics");
    }
    {
        fixture test;
        test.Blobs.LeftBytes = {0xC3U, 0x28U};
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Projections.Failures.empty()
                   && test.Projections.Results.size() == 1U
                   && test.Projections.Results.back()
                          .Result.Alignment.Content
                       == core::diff_content_state::unavailable
                   && test.Projections.Results.back()
                          .Result.Alignment.Diagnostic.Code
                       == core::diff_reason_code::decode_failure,
               "validated decode result publishes its honest unavailable semantics");
    }
    {
        auto limits = core::diff_compute_limits{};
        limits.MaximumBytesPerSide = 1U;
        fixture test{{}, limits};
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Projections.Failures.back().Reason
                   == core::diff_reason_code::materialization_limit,
               "compute byte limit cannot publish partial output");
    }
    {
        auto limits = core::diff_compute_limits{};
        limits.MaximumMatrixCells = 1U;
        fixture test{{}, limits};
        test.request(comparison_key());
        test.run_effect_and_decision();
        expect(test.Projections.Failures.empty()
                   && test.Projections.Results.size() == 1U
                   && test.Projections.Results.back()
                          .Result.Alignment.Content
                       == core::diff_content_state::unavailable
                   && test.Projections.Results.back()
                          .Result.Alignment.Diagnostic.Code
                       == core::diff_reason_code::compute_limit,
               "compute work limit publishes validated unavailable semantics");
    }
    {
        fixture test;
        auto options = core::diff_options{};
        options.ContextLines = 9U;
        test.request(comparison_key(), options);
        expect(test.Effects.pending() == 0U
                   && test.Decisions.pending() == 0U
                   && test.Host->state().LastReason
                       == core::diff_reason_code::invalid_alignment,
               "key/options mismatch is rejected before acquisition");
    }
}

auto test_invalidation_and_projection_exception() -> void
{
    fixture test;
    const auto key = comparison_key();
    test.request(key);
    test.run_effect_and_decision();
    expect(test.Host->cache_entries() == 1U, "fixture has cached authority");
    test.Host->invalidate(key.Authority);
    expect(test.Host->cache_entries() == 0U
               && test.Host->cache_bytes() == 0U
               && !test.Host->state().Current.has_value()
               && test.Projections.Invalidated ==
                   std::vector{key.Authority},
           "authority invalidation clears semantic current and exact cache");
    test.request(key);
    expect(test.Effects.pending() == 1U,
           "invalidated authority cannot reuse derived cache");
    test.run_effect_and_decision();

    test.Projections.Throw = true;
    test.request(comparison_key({}, 32U));
    test.run_effect_and_decision();
    expect(test.Host->projection_delivery_failed(),
           "projection exception is contained and recorded");
}

auto test_projection_reentry_is_deferred() -> void
{
    fixture test;
    const auto key = comparison_key();
    auto request_was_deferred = false;
    test.Projections.OnPublish = [&test, &key, &request_was_deferred] {
        test.Host->request(key, {});
        request_was_deferred = !test.Host->state().Pending.has_value();
        test.Host->invalidate(key.Authority);
    };
    test.request(key);
    test.run_effect_and_decision();
    expect(request_was_deferred && !test.Projections.NestedDelivery
               && test.Projections.Invalidated == std::vector{key.Authority}
               && test.Host->cache_entries() == 0U
               && test.Decisions.pending() == 1U,
           "projection feedback waits until the active delivery returns");
    test.Decisions.run_next();
    expect(test.Projections.Results.size() == 1U
               && test.Projections.Failures.empty(),
           "deferred invalidation makes the queued cache terminal stale");
}

auto test_persistent_projection_feedback_yields_between_turns() -> void
{
    fixture test;
    const auto authority = comparison_key().Authority;
    auto recurring = std::function<void()>{};
    recurring = [&test, &authority, &recurring] {
        test.Projections.OnInvalidate = recurring;
        test.Host->invalidate(authority);
    };
    test.Projections.OnPublish = [&test, &authority, &recurring] {
        test.Projections.OnInvalidate = recurring;
        test.Host->invalidate(authority);
    };
    test.request(comparison_key());
    test.run_effect_and_decision();
    expect(test.Projections.Invalidated.size() == 1U
               && test.Decisions.pending() == 1U
               && !test.Projections.NestedDelivery,
           "feedback created during the snapshot yields to another decision turn");
    test.Decisions.run_next();
    expect(test.Projections.Invalidated.size() == 2U
               && test.Decisions.pending() == 1U,
           "persistent feedback remains one bounded scheduled turn");
    test.Projections.OnInvalidate = {};
    test.Decisions.run_next();
    expect(test.Projections.Invalidated.size() == 3U
               && test.Decisions.pending() == 0U,
           "feedback chain stops without stranded deferred ingress");
}

auto test_pending_effect_is_safe_after_host_destruction() -> void
{
    fake_blobs blobs;
    application::manual_executor effects;
    application::manual_executor decisions;
    recording_projection projections;
    {
        application::diff_application_host host{
            core::diff_state{
                .ApplicationSession = core::application_session_id{73U},
            },
            blobs,
            effects,
            decisions,
            projections,
        };
        host.request(comparison_key(), {});
        expect(effects.pending() == 1U,
               "fixture destroys host with one pending effect");
    }
    effects.drain();
    decisions.drain();
    expect(projections.Results.empty() && projections.Failures.empty(),
           "expired completion gate cannot target destroyed host state");
}
} // namespace

int main()
{
    test_worker_and_cache_lanes();
    test_exact_cache_key_collisions();
    test_reordered_generation_and_stale_completion();
    test_invalid_replacement_supersedes_pending_work();
    test_reordered_exact_identity_boundaries();
    test_cancellation_and_shutdown_stages();
    test_job_and_cache_bounds();
    test_terminal_queue_is_bounded();
    test_absent_and_empty_sides();
    test_typed_failures_and_exact_echo();
    test_invalidation_and_projection_exception();
    test_projection_reentry_is_deferred();
    test_persistent_projection_feedback_yields_between_turns();
    test_pending_effect_is_safe_after_host_destruction();
    return EXIT_SUCCESS;
}
