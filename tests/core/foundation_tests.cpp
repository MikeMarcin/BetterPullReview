#include "bpr/core/transition.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
using namespace bpr::core;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

auto initial_state() -> application_state
{
    return application_state{
        .Session = application_session_id{41},
        .NextRequest = request_id{1},
    };
}

auto first_refresh_context(const transition_output& output) -> request_context
{
    expect(output.Requests.size() == 1U, "refresh emits one request");
    const auto* request =
        std::get_if<refresh_foundation_request>(&output.Requests.front());
    expect(request != nullptr, "refresh request has the typed payload");
    return request->Context;
}

auto test_refresh_request() -> void
{
    auto state = initial_state();
    const auto output = transition(
        state,
        refresh_requested_intent{},
        foundation_observation{.GitHubRevision = 7, .LocalPrivateRevision = 91});

    const auto context = first_refresh_context(output);
    expect(context.Session == state.Session, "request carries session identity");
    expect(context.Request == request_id{1}, "request identity is deterministic");
    expect(
        context.Generation == request_generation{1},
        "refresh increments its generation");
    expect(
        state.PendingRefresh == request_id{1},
        "pending request identity is retained");
    expect(
        output.Changed
            == (changed_domain::commands | changed_domain::status),
        "refresh changes only command and status domains");
    expect(
        output.Commands.front().Enabled == false,
        "refresh command is disabled while pending");
}

auto test_github_is_shared_authority() -> void
{
    const foundation_observation observation{
        .GitHubRevision = 19,
        .LocalPrivateRevision = 999,
    };
    expect(
        resolve_shared_revision(observation) == 19,
        "local private state cannot override GitHub shared state");

    auto state = initial_state();
    const auto request_output =
        transition(state, refresh_requested_intent{}, observation);
    const auto context = first_refresh_context(request_output);
    const auto completion_output = transition(
        state,
        foundation_refreshed_event{
            .Context = context,
            .GitHubRevision = 23,
        },
        observation);

    expect(state.GitHubRevision == 23, "completion adopts GitHub revision");
    expect(!state.PendingRefresh.has_value(), "accepted request is cleared");
    expect(
        completion_output.Changed
            == (changed_domain::commands | changed_domain::status
                | changed_domain::authority),
        "accepted completion publishes bounded changed domains");
    expect(
        completion_output.Commands.front().Enabled,
        "refresh command is enabled after completion");
}

auto test_stale_completion_is_rejected() -> void
{
    auto state = initial_state();
    const auto request_output = transition(
        state,
        refresh_requested_intent{},
        foundation_observation{});
    const auto current = first_refresh_context(request_output);
    const auto stale = request_context{
        .Request = request_id{current.Request.value() + 10U},
        .Session = current.Session,
        .Generation = current.Generation,
    };

    const auto output = transition(
        state,
        foundation_refreshed_event{
            .Context = stale,
            .GitHubRevision = 50,
        },
        foundation_observation{});

    expect(state.GitHubRevision == 0, "stale completion cannot update state");
    expect(
        state.PendingRefresh == current.Request,
        "stale completion cannot clear current work");
    expect(output.Requests.empty(), "stale completion emits no host request");
    expect(
        output.Diagnostics.size() == 1U
            && output.Diagnostics.front().Code
                == reason_code::stale_completion,
        "stale completion emits a structured diagnostic");
}

auto assert_identity_is_rejected(const request_context& stale) -> void
{
    auto state = initial_state();
    const auto request_output = transition(
        state,
        refresh_requested_intent{},
        foundation_observation{});
    const auto current = first_refresh_context(request_output);

    const auto output = transition(
        state,
        foundation_refreshed_event{
            .Context = stale,
            .GitHubRevision = 50,
        },
        foundation_observation{});

    expect(state.GitHubRevision == 0, "mismatched identity cannot update state");
    expect(
        state.PendingRefresh == current.Request,
        "mismatched identity cannot clear current work");
    expect(
        output.Diagnostics.size() == 1U
            && output.Diagnostics.front().Code
                == reason_code::stale_completion,
        "mismatched identity reports stale completion");
}

auto test_every_async_identity_dimension_is_checked() -> void
{
    assert_identity_is_rejected(request_context{
        .Request = request_id{1},
        .Session = application_session_id{99},
        .Generation = request_generation{1},
    });
    assert_identity_is_rejected(request_context{
        .Request = request_id{1},
        .Session = application_session_id{41},
        .Generation = request_generation{99},
    });
}

auto test_duplicate_intent_is_rejected() -> void
{
    auto state = initial_state();
    static_cast<void>(
        transition(state, refresh_requested_intent{}, foundation_observation{}));
    const auto output =
        transition(state, refresh_requested_intent{}, foundation_observation{});

    expect(output.Requests.empty(), "duplicate refresh emits no second request");
    expect(
        output.Diagnostics.size() == 1U
            && output.Diagnostics.front().Code
                == reason_code::refresh_in_progress,
        "duplicate refresh reports semantic rejection");
}

auto test_authority_categories_are_explicit() -> void
{
    expect(
        name(authority_domain::github_shared) == "GitHub shared state",
        "GitHub shared authority has an explicit category");
    expect(
        name(authority_domain::local_private) == "Local private state",
        "local private authority has an explicit category");
    expect(
        name(authority_domain::derived_projection) == "Derived projection",
        "derived state has an explicit category");
}

auto test_replay_is_deterministic() -> void
{
    auto left = initial_state();
    auto right = initial_state();
    const foundation_observation observation{
        .GitHubRevision = 5,
        .LocalPrivateRevision = 8,
    };

    const auto left_request =
        transition(left, refresh_requested_intent{}, observation);
    const auto right_request =
        transition(right, refresh_requested_intent{}, observation);
    expect(left == right, "identical intent produces identical state");
    expect(
        left_request == right_request,
        "identical intent produces identical outputs");

    const auto context = first_refresh_context(left_request);
    const foundation_refreshed_event completion{
        .Context = context,
        .GitHubRevision = 6,
    };
    const auto left_completion = transition(left, completion, observation);
    const auto right_completion = transition(right, completion, observation);
    expect(left == right, "identical completion produces identical state");
    expect(
        left_completion == right_completion,
        "identical completion produces identical outputs");
}
}

auto main() -> int
{
    static_assert(sizeof(bpr::core::application_state) <= 64U);

    test_refresh_request();
    test_github_is_shared_authority();
    test_stale_completion_is_rejected();
    test_every_async_identity_dimension_is_checked();
    test_duplicate_intent_is_rejected();
    test_authority_categories_are_explicit();
    test_replay_is_deterministic();

    std::cout << "All deterministic core foundation tests passed\n";
    return EXIT_SUCCESS;
}
