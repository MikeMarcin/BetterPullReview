#include "bpr/core/transition.hpp"

#include <type_traits>

namespace bpr::core
{
namespace
{
auto refresh_command(const bool enabled, const reason_code reason)
    -> command_projection
{
    return command_projection{
        .Id = command_id::refresh,
        .Enabled = enabled,
        .Checked = false,
        .DisabledReason = reason,
    };
}

auto reject_stale_completion() -> transition_output
{
    transition_output output;
    output.Diagnostics.push_back(diagnostic{
        .Code = reason_code::stale_completion,
        .Detail = "Completion identity does not match the active refresh",
    });
    return output;
}

auto request_refresh(application_state& state) -> transition_output
{
    if(state.PendingRefresh.has_value())
    {
        transition_output output;
        output.Diagnostics.push_back(diagnostic{
            .Code = reason_code::refresh_in_progress,
            .Detail = "A foundation refresh is already in progress",
        });
        return output;
    }

    const auto request = state.NextRequest;
    state.NextRequest = request_id{request.value() + 1U};
    state.RefreshGeneration =
        request_generation{state.RefreshGeneration.value() + 1U};
    state.PendingRefresh = request;

    const request_context context{
        .Request = request,
        .Session = state.Session,
        .Generation = state.RefreshGeneration,
    };

    transition_output output;
    output.Changed = changed_domain::commands | changed_domain::status;
    output.Requests.push_back(refresh_foundation_request{.Context = context});
    output.Commands.push_back(
        refresh_command(false, reason_code::refresh_in_progress));
    output.Status = status_delta{.Message = "Refreshing foundation state"};
    return output;
}

auto complete_refresh(
    application_state& state,
    const foundation_refreshed_event& event) -> transition_output
{
    if(event.Context.Session != state.Session
       || event.Context.Generation != state.RefreshGeneration
       || !state.PendingRefresh.has_value()
       || event.Context.Request != *state.PendingRefresh)
    {
        return reject_stale_completion();
    }

    state.GitHubRevision = event.GitHubRevision;
    state.PendingRefresh.reset();

    transition_output output;
    output.Changed = changed_domain::commands | changed_domain::status
        | changed_domain::authority;
    output.Commands.push_back(refresh_command(true, reason_code::none));
    output.Status = status_delta{.Message = "Foundation state is current"};
    return output;
}
}

auto resolve_shared_revision(
    const foundation_observation& observation) noexcept -> std::uint64_t
{
    return observation.GitHubRevision;
}

auto transition(
    application_state& state,
    const application_event& event,
    const foundation_observation& observation) -> transition_output
{
    static_cast<void>(observation);

    return std::visit(
        [&state](const auto& typed_event) -> transition_output {
            using event_type = std::decay_t<decltype(typed_event)>;
            if constexpr(std::is_same_v<event_type, refresh_requested_intent>)
            {
                return request_refresh(state);
            }
            else
            {
                return complete_refresh(state, typed_event);
            }
        },
        event);
}
}
