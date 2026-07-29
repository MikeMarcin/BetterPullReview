#include "bpr/application/host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <cstdlib>
#include <iostream>
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

class fake_observations final
    : public application::foundation_observation_port
{
public:
    core::foundation_observation Current;

    [[nodiscard]] auto observe() const
        -> core::foundation_observation override
    {
        return Current;
    }
};

class fake_foundation final : public application::foundation_effect_port
{
public:
    struct pending_request
    {
        core::refresh_foundation_request Request;
        application::completion_callback Completion;
    };

    void submit(
        const core::refresh_foundation_request& request,
        application::completion_callback completion) override
    {
        Pending.push_back(pending_request{
            .Request = request,
            .Completion = std::move(completion),
        });
    }

    void cancel(const core::request_id request) override
    {
        Cancelled.push_back(request);
    }

    auto completion(std::size_t index) -> application::completion_callback
    {
        return Pending.at(index).Completion;
    }

    void complete(const std::size_t index, const std::uint64_t revision)
    {
        const auto request = Pending.at(index).Request;
        Pending.at(index).Completion(core::foundation_refreshed_event{
            .Context = request.Context,
            .GitHubRevision = revision,
        });
    }

    std::vector<pending_request> Pending;
    std::vector<core::request_id> Cancelled;
};

class recording_projections final : public application::projection_port
{
public:
    void publish(const core::transition_output& output) override
    {
        Published.push_back(output);
    }

    std::vector<core::transition_output> Published;
};

auto initial_state() -> core::application_state
{
    return core::application_state{
        .Session = core::application_session_id{71},
        .NextRequest = core::request_id{1},
    };
}

auto test_completion_is_serialized() -> void
{
    fake_observations observations;
    fake_foundation foundation;
    application::manual_executor lane;
    recording_projections projections;
    application::application_host host{
        initial_state(),
        observations,
        foundation,
        lane,
        projections,
    };

    host.dispatch(core::refresh_requested_intent{});
    expect(foundation.Pending.size() == 1U, "host executes one typed request");
    expect(
        projections.Published.size() == 1U,
        "host publishes the pending projection");
    expect(
        !projections.Published.front().Commands.front().Enabled,
        "pending projection disables refresh");

    foundation.complete(0, 12);
    expect(lane.pending() == 1U, "completion is posted to decision lane");
    expect(
        host.state().GitHubRevision == 0,
        "worker completion cannot mutate core directly");
    expect(
        projections.Published.size() == 1U,
        "worker completion cannot publish directly");

    lane.run_next();
    expect(
        host.state().GitHubRevision == 12,
        "serialized completion advances state");
    expect(
        projections.Published.size() == 2U,
        "serialized completion publishes its delta");
    expect(
        projections.Published.back().Commands.front().Enabled,
        "completion projection re-enables refresh");
}

auto test_duplicate_and_stale_work_are_policy_safe() -> void
{
    fake_observations observations;
    fake_foundation foundation;
    application::manual_executor lane;
    recording_projections projections;
    application::application_host host{
        initial_state(),
        observations,
        foundation,
        lane,
        projections,
    };

    host.dispatch(core::refresh_requested_intent{});
    host.dispatch(core::refresh_requested_intent{});
    expect(
        foundation.Pending.size() == 1U,
        "host does not duplicate core request policy");
    expect(
        projections.Published.back().Diagnostics.front().Code
            == core::reason_code::refresh_in_progress,
        "duplicate intent preserves core rejection");

    auto stale_completion = foundation.completion(0);
    foundation.complete(0, 20);
    lane.run_next();
    host.dispatch(core::refresh_requested_intent{});
    expect(foundation.Pending.size() == 2U, "second generation is scheduled");

    stale_completion(core::foundation_refreshed_event{
        .Context = foundation.Pending.front().Request.Context,
        .GitHubRevision = 99,
    });
    lane.run_next();
    expect(
        host.state().GitHubRevision == 20,
        "late earlier generation cannot overwrite current state");
    expect(
        projections.Published.back().Diagnostics.front().Code
            == core::reason_code::stale_completion,
        "late completion returns through typed diagnostic path");

    foundation.complete(1, 21);
    lane.run_next();
    expect(
        host.state().GitHubRevision == 21,
        "matching current generation completes normally");
}

auto test_shutdown_cancels_and_suppresses_completion() -> void
{
    fake_observations observations;
    fake_foundation foundation;
    application::manual_executor lane;
    recording_projections projections;
    application::application_host host{
        initial_state(),
        observations,
        foundation,
        lane,
        projections,
    };

    host.dispatch(core::refresh_requested_intent{});
    host.shutdown();
    expect(host.is_closing(), "shutdown closes host ingress");
    expect(
        foundation.Cancelled == std::vector{core::request_id{1}},
        "shutdown cancels pending work by identity");

    foundation.complete(0, 40);
    lane.run_next();
    expect(
        host.state().GitHubRevision == 0,
        "completion after shutdown is suppressed");
    expect(
        projections.Published.size() == 1U,
        "completion after shutdown cannot publish");

    host.dispatch(core::refresh_requested_intent{});
    expect(
        foundation.Pending.size() == 1U,
        "new ingress is rejected after shutdown");
}

auto test_manual_executor_reorders_deterministically() -> void
{
    application::manual_executor lane;
    std::vector<int> order;
    lane.post([&order] { order.push_back(1); });
    lane.post([&order] { order.push_back(2); });
    lane.post([&order] { order.push_back(3); });

    lane.run(2);
    lane.run(0);
    lane.run(0);

    expect(
        order == std::vector{3, 1, 2},
        "manual executor runs tasks in the selected order");
    expect(lane.pending() == 0U, "manual executor drains selected tasks");
}
}

auto main() -> int
{
    test_completion_is_serialized();
    test_duplicate_and_stale_work_are_policy_safe();
    test_shutdown_cancels_and_suppresses_completion();
    test_manual_executor_reorders_deterministically();

    std::cout << "All application host tests passed\n";
    return EXIT_SUCCESS;
}
