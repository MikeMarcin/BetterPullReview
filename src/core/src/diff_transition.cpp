#include "bpr/core/diff_transition.hpp"

#include <limits>
#include <type_traits>
#include <utility>

namespace bpr::core
{
namespace
{
[[nodiscard]] constexpr auto valid_failure(const diff_reason_code reason)
    noexcept -> bool
{
    return reason != diff_reason_code::none
        && reason != diff_reason_code::stale;
}

[[nodiscard]] auto request(diff_state& state,
                           const request_file_diff_intent& intent)
    -> diff_transition_output
{
    auto output = diff_transition_output{};
    if(state.Pending.has_value())
    {
        output.Requests.push_back(cancel_file_diff_request{*state.Pending});
        state.Pending.reset();
    }
    const auto digest = options_digest(intent.Options);
    if(!valid(intent.Key) || !digest.has_value()
       || *digest != intent.Key.Options)
    {
        output.Reason = diff_transition_reason::invalid_request;
        state.LastReason = diff_reason_code::invalid_alignment;
        return output;
    }
    if(intent.Key.Algorithm != supported_diff_algorithm_revision)
    {
        output.Reason = diff_transition_reason::unsupported_algorithm;
        state.LastReason = diff_reason_code::host_failure;
        return output;
    }
    if(state.ApplicationSession.value() == 0U
       || state.NextRequest.value() == 0U
       || state.NextRequest.value()
           == std::numeric_limits<std::uint64_t>::max()
       || state.Generation.value()
           == std::numeric_limits<std::uint64_t>::max())
    {
        output.Reason = diff_transition_reason::request_identity_exhausted;
        state.LastReason = diff_reason_code::host_failure;
        return output;
    }

    state.Generation = request_generation{state.Generation.value() + 1U};
    const auto context = diff_request_context{
        .Request = state.NextRequest,
        .ApplicationSession = state.ApplicationSession,
        .Key = intent.Key,
        .Generation = state.Generation,
    };
    state.NextRequest = request_id{state.NextRequest.value() + 1U};
    state.Pending = context;
    state.LastReason = diff_reason_code::none;
    output.Requests.push_back(
        compute_file_diff_request{.Context = context, .Options = intent.Options});
    return output;
}

[[nodiscard]] auto cancel(diff_state& state) -> diff_transition_output
{
    auto output = diff_transition_output{};
    if(!state.Pending.has_value())
    {
        return output;
    }
    const auto context = *state.Pending;
    output.Requests.push_back(cancel_file_diff_request{context});
    output.Terminal = diff_terminal_delta{
        .Context = context,
        .Reason = diff_reason_code::cancelled,
    };
    state.Pending.reset();
    state.LastReason = diff_reason_code::cancelled;
    return output;
}

[[nodiscard]] auto invalidate(
    diff_state& state, const invalidate_diff_authority_intent& intent)
    -> diff_transition_output
{
    auto output = diff_transition_output{};
    if(state.Pending.has_value()
       && state.Pending->Key.Authority == intent.Authority)
    {
        output.Requests.push_back(cancel_file_diff_request{*state.Pending});
        state.Pending.reset();
    }
    if(state.Current.has_value()
       && state.Current->Key.Authority == intent.Authority)
    {
        state.Current.reset();
    }
    return output;
}

[[nodiscard]] auto complete(diff_state& state,
                            const file_diff_computed_event& event)
    -> diff_transition_output
{
    auto output = diff_transition_output{};
    if(!state.Pending.has_value() || *state.Pending != event.Context)
    {
        output.Reason = diff_transition_reason::stale_completion;
        return output;
    }
    state.Pending.reset();
    state.Current = event.Context;
    state.LastReason = diff_reason_code::none;
    output.Accepted = accepted_file_diff_delta{event.Context};
    output.Terminal = diff_terminal_delta{
        .Context = event.Context,
        .Reason = diff_reason_code::none,
    };
    return output;
}

[[nodiscard]] auto fail(diff_state& state,
                        const file_diff_failed_event& event)
    -> diff_transition_output
{
    auto output = diff_transition_output{};
    if(!valid_failure(event.Reason))
    {
        output.Reason = diff_transition_reason::invalid_failure;
        return output;
    }
    if(!state.Pending.has_value() || *state.Pending != event.Context)
    {
        output.Reason = diff_transition_reason::stale_completion;
        return output;
    }
    state.Pending.reset();
    state.LastReason = event.Reason;
    output.Terminal = diff_terminal_delta{
        .Context = event.Context,
        .Reason = event.Reason,
    };
    return output;
}
} // namespace

auto valid(const diff_request_context& context) noexcept -> bool
{
    return context.Request.value() != 0U
        && context.ApplicationSession.value() != 0U
        && context.Generation.value() != 0U && valid(context.Key);
}

auto transition(diff_state& state, const diff_event& event)
    -> diff_transition_output
{
    return std::visit(
        [&state](const auto& typed_event) -> diff_transition_output {
            using event_type = std::decay_t<decltype(typed_event)>;
            if constexpr(std::is_same_v<event_type,
                                        request_file_diff_intent>)
            {
                return request(state, typed_event);
            }
            else if constexpr(std::is_same_v<event_type,
                                             cancel_file_diff_intent>)
            {
                return cancel(state);
            }
            else if constexpr(std::is_same_v<
                                  event_type,
                                  invalidate_diff_authority_intent>)
            {
                return invalidate(state, typed_event);
            }
            else if constexpr(std::is_same_v<event_type,
                                             file_diff_computed_event>)
            {
                return complete(state, typed_event);
            }
            else
            {
                return fail(state, typed_event);
            }
        },
        event);
}
} // namespace bpr::core
