#include "bpr/core/transition.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <variant>

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

auto test_shared_private_and_derived_authorities_are_distinct() -> void
{
    expect(
        authority_domain::github_shared != authority_domain::local_private,
        "GitHub and local private state have distinct authority");
    expect(
        authority_domain::github_shared
            != authority_domain::derived_projection,
        "GitHub and derived state have distinct authority");
    expect(
        authority_domain::local_private
            != authority_domain::derived_projection,
        "local private and derived state have distinct authority");

    const foundation_observation disagreement{
        .GitHubRevision = 4,
        .LocalPrivateRevision = 400,
    };
    expect(
        resolve_shared_revision(disagreement) == 4,
        "GitHub observation wins every shared-state disagreement");
}

auto test_foundation_request_is_semantic() -> void
{
    application_state state{
        .Session = application_session_id{1},
        .NextRequest = request_id{1},
        .RefreshGeneration = {},
        .PendingRefresh = {},
        .GitHubRevision = 0U,
    };
    const auto output = transition(
        state,
        refresh_requested_intent{},
        foundation_observation{});
    expect(output.Requests.size() == 1U, "refresh emits one semantic request");
    expect(
        std::holds_alternative<refresh_foundation_request>(
            output.Requests.front()),
        "request contains no URL, SQL, process, or widget command");
}
}

auto main() -> int
{
    test_shared_private_and_derived_authorities_are_distinct();
    test_foundation_request_is_semantic();

    std::cout << "All product contract tests passed\n";
    return EXIT_SUCCESS;
}
