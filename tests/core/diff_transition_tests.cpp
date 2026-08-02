#include "bpr/core/diff_transition.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
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

template<typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    std::array<std::uint8_t, 32U> bytes{};
    bytes[0] = seed;
    bytes[31] = static_cast<std::uint8_t>(seed ^ 0xA5U);
    return Identity{bytes};
}

[[nodiscard]] constexpr auto blob(const std::uint8_t seed) -> git_blob_oid
{
    std::array<std::uint8_t, 32U> bytes{};
    for(std::size_t index = 0U; index < 20U; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return git_blob_oid{git_hash_algorithm::sha1, bytes};
}

[[nodiscard]] auto key(const std::uint8_t file = 1U,
                       const diff_algorithm_revision algorithm =
                           supported_diff_algorithm_revision)
    -> file_comparison_key
{
    const auto options = diff_options{};
    const auto digest = options_digest(options);
    expect(digest.has_value(), "default options digest");
    const auto scope = opaque<pull_request_authority_scope_id>(2U);
    const auto incarnation = opaque<epoch_catalog_incarnation_id>(3U);
    const auto authority = pull_request_authority{
        .Profile = host_profile_id{1U},
        .ProfileRevision = github_profile_revision{1U},
        .Host = git_host_id{1U},
        .HostAuthority = opaque<host_authority_fingerprint>(4U),
        .Account = github_account_id{1U},
        .Viewer = github_viewer_actor_id{1U},
        .Repository = repository_id{1U},
        .Cache = opaque<repository_cache_scope_id>(5U),
        .PullRequest = pull_request_id{1U},
        .Number = pull_request_number{1U},
        .Scope = scope,
    };
    return file_comparison_key{
        .Authority = authority,
        .Repository = authority.Repository,
        .PullRequest = authority.PullRequest,
        .LeftAnchor = comparison_anchor_identity{
            .Authority = scope,
            .Incarnation = incarnation,
            .Kind = comparison_anchor_kind::epoch_base,
            .Epoch = epoch_ordinal{1},
        },
        .RightAnchor = comparison_anchor_identity{
            .Authority = scope,
            .Incarnation = incarnation,
            .Kind = comparison_anchor_kind::epoch_head,
            .Epoch = epoch_ordinal{2},
        },
        .LeftBlob = blob(10U),
        .RightBlob = blob(40U),
        .File = opaque<file_identity>(file),
        .Options = *digest,
        .Algorithm = algorithm,
    };
}

[[nodiscard]] auto state() -> diff_state
{
    return diff_state{.ApplicationSession = application_session_id{7U}};
}

auto test_exact_generation_and_acceptance() -> void
{
    auto value = state();
    const auto first = transition(
        value, request_file_diff_intent{.Key = key(), .Options = {}});
    expect(first.Requests.size() == 1U && value.Pending.has_value(),
           "first request becomes exact pending context");
    const auto first_request =
        std::get<compute_file_diff_request>(first.Requests.front());
    expect(first_request.Context.Request == request_id{1U}
               && first_request.Context.Generation == request_generation{1U}
               && valid(first_request.Context),
           "request and generation are nonzero and monotonic");

    const auto second = transition(
        value, request_file_diff_intent{.Key = key(2U), .Options = {}});
    expect(second.Requests.size() == 2U
               && std::holds_alternative<cancel_file_diff_request>(
                   second.Requests.front()),
           "supersession cancels the exact earlier context");
    const auto second_context =
        std::get<compute_file_diff_request>(second.Requests.back()).Context;
    expect(second_context.Request == request_id{2U}
               && second_context.Generation == request_generation{2U},
           "supersession advances both identities");

    const auto stale = transition(
        value, file_diff_computed_event{first_request.Context});
    expect(stale.Reason == diff_transition_reason::stale_completion
               && !stale.Accepted.has_value() && value.Pending.has_value(),
           "reordered old completion is rejected without state change");
    const auto accepted = transition(
        value, file_diff_computed_event{second_context});
    expect(accepted.Accepted.has_value() && accepted.Terminal.has_value()
               && value.Current == second_context && !value.Pending,
           "only the exact current completion is accepted");
    const auto duplicate = transition(
        value, file_diff_computed_event{second_context});
    expect(duplicate.Reason == diff_transition_reason::stale_completion,
           "duplicate completion is stale");
}

auto test_failure_cancel_and_invalidation() -> void
{
    auto value = state();
    auto output = transition(
        value, request_file_diff_intent{.Key = key(), .Options = {}});
    const auto context =
        std::get<compute_file_diff_request>(output.Requests.back()).Context;
    output = transition(value,
                        file_diff_failed_event{
                            .Context = context,
                            .Reason = diff_reason_code::decode_failure,
                        });
    expect(output.Terminal.has_value()
               && output.Terminal->Reason == diff_reason_code::decode_failure
               && value.LastReason == diff_reason_code::decode_failure,
           "matching typed failure terminates without replacing current");

    output = transition(
        value, request_file_diff_intent{.Key = key(), .Options = {}});
    output = transition(value, cancel_file_diff_intent{});
    expect(output.Requests.size() == 1U && output.Terminal.has_value()
               && output.Terminal->Reason == diff_reason_code::cancelled
               && !value.Pending,
           "explicit cancellation revokes pending context");

    output = transition(
        value, request_file_diff_intent{.Key = key(), .Options = {}});
    const auto authority = key().Authority;
    output = transition(
        value, invalidate_diff_authority_intent{.Authority = authority});
    expect(output.Requests.size() == 1U && !value.Pending,
           "authority invalidation cancels matching pending work");
}

auto test_invalid_and_unsupported_requests() -> void
{
    auto value = state();
    auto unsupported = key(1U, diff_algorithm_revision{2U});
    auto output = transition(
        value,
        request_file_diff_intent{.Key = unsupported, .Options = {}});
    expect(output.Reason == diff_transition_reason::unsupported_algorithm
               && output.Requests.empty(),
           "caller cannot select an unstamped algorithm revision");

    auto mismatched = key();
    auto options = diff_options{};
    options.ContextLines = 9U;
    output = transition(
        value,
        request_file_diff_intent{.Key = mismatched, .Options = options});
    expect(output.Reason == diff_transition_reason::invalid_request
               && output.Requests.empty(),
           "options must match the exact key digest");

    output = transition(value,
                        file_diff_failed_event{
                            .Context = {},
                            .Reason = diff_reason_code::none,
                        });
    expect(output.Reason == diff_transition_reason::invalid_failure,
           "success cannot enter through the failure path");

    for(const auto unsupported_replacement : {false, true})
    {
        auto replacement_state = state();
        const auto pending = transition(
            replacement_state,
            request_file_diff_intent{.Key = key(), .Options = {}});
        const auto pending_context =
            std::get<compute_file_diff_request>(pending.Requests.back())
                .Context;
        auto replacement_key = key();
        auto replacement_options = diff_options{};
        if(unsupported_replacement)
        {
            replacement_key.Algorithm = diff_algorithm_revision{2U};
        }
        else
        {
            replacement_options.ContextLines = 9U;
        }
        output = transition(
            replacement_state,
            request_file_diff_intent{
                .Key = replacement_key,
                .Options = replacement_options,
            });
        expect(output.Requests.size() == 1U
                   && std::get<cancel_file_diff_request>(
                          output.Requests.front())
                          .Context
                       == pending_context
                   && !replacement_state.Pending.has_value(),
               "a newer invalid request revokes older pending work");
        const auto stale = transition(
            replacement_state, file_diff_computed_event{pending_context});
        expect(stale.Reason == diff_transition_reason::stale_completion,
               "revoked work cannot publish after malformed replacement");
    }
}
} // namespace

int main()
{
    test_exact_generation_and_acceptance();
    test_failure_cancel_and_invalidation();
    test_invalid_and_unsupported_requests();
    return EXIT_SUCCESS;
}
