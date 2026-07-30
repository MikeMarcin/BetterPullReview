#include "bpr/core/github_transition.hpp"

#include <type_traits>
#include <utility>

namespace bpr::core
{
namespace
{
[[nodiscard]] auto next_request(github_state& state) -> request_id
{
    const auto request = state.NextRequest;
    state.NextRequest = request_id{request.value() + 1U};
    return request;
}

[[nodiscard]] auto identity_scope(const github_state& state)
    -> github_identity_scope
{
    return github_identity_scope{
        .Profile = state.ActiveProfile,
        .Host = state.Host,
        .Account = state.Account,
        .Repository = state.Repository,
        .PullRequest = state.PullRequest,
        .Snapshot = state.AcceptedSnapshot,
    };
}

[[nodiscard]] auto identity_delta(const github_state& state)
    -> github_identity_delta
{
    return github_identity_delta{
        .Profile = state.ActiveProfile,
        .Host = state.Host,
        .Account = state.Account,
        .Generation = state.IdentityGeneration,
    };
}

[[nodiscard]] auto capability_delta(const github_state& state)
    -> github_capability_delta
{
    return github_capability_delta{
        .Capabilities = state.Capabilities,
        .Revision = state.CapabilityRevision,
    };
}

[[nodiscard]] auto summary(const github_state& state)
    -> std::optional<github_pull_request_summary>
{
    if(
        !state.ActiveLocator.has_value() || !state.ActiveNumber.has_value()
        || !state.Repository.has_value() || !state.PullRequest.has_value()
        || !state.AcceptedSnapshot.has_value())
    {
        return std::nullopt;
    }

    return github_pull_request_summary{
        .Locator = *state.ActiveLocator,
        .Number = *state.ActiveNumber,
        .Repository = *state.Repository,
        .PullRequest = *state.PullRequest,
        .Snapshot = *state.AcceptedSnapshot,
    };
}

[[nodiscard]] auto command(
    const github_command_id id,
    const bool enabled,
    const github_reason_code reason = github_reason_code::none)
    -> github_command_projection
{
    return github_command_projection{
        .Id = id,
        .Enabled = enabled,
        .DisabledReason = enabled ? github_reason_code::none : reason,
    };
}

[[nodiscard]] auto reject(const github_reason_code reason)
    -> github_transition_output
{
    github_transition_output output;
    output.Changed = github_changed_domain::diagnostics;
    output.Diagnostics.push_back(github_diagnostic{.Code = reason});
    return output;
}

auto publish_commands(
    const github_state& state,
    github_transition_output& output) -> void
{
    output.Commands = github_commands(state);
    output.Changed = output.Changed | github_changed_domain::commands;
}

[[nodiscard]] auto current_session(
    const github_state& state,
    const github_session_request_context& context) noexcept -> bool
{
    return state.PendingSession.has_value()
        && *state.PendingSession == context
        && state.ActiveProfile == context.Profile
        && state.ApplicationSession == context.ApplicationSession
        && state.IdentityGeneration == context.IdentityGeneration;
}

[[nodiscard]] auto current_pull_request(
    const github_state& state,
    const pull_request_request_context& context) noexcept -> bool
{
    return state.PendingPullRequest.has_value()
        && *state.PendingPullRequest == context
        && state.ActiveProfile == context.Profile && state.Host == context.Host
        && state.Account == context.Account
        && state.ApplicationSession == context.ApplicationSession
        && state.IdentityGeneration == context.IdentityGeneration
        && state.PullRequestGeneration == context.Generation
        && state.CapabilityRevision == context.Capabilities
        && state.ActiveLocator == context.Repository
        && state.ActiveNumber == context.Number;
}

[[nodiscard]] constexpr auto valid_capability_state(
    const github_capability_state state) noexcept -> bool
{
    return state == github_capability_state::unknown
        || state == github_capability_state::supported
        || state == github_capability_state::unsupported;
}

[[nodiscard]] constexpr auto valid_capabilities(
    const github_capabilities& capabilities) noexcept -> bool
{
    return valid_capability_state(capabilities.ReviewThreads)
        && valid_capability_state(capabilities.ViewedFileMutations)
        && valid_capability_state(capabilities.ResolveThread)
        && valid_capability_state(capabilities.UnresolveThread);
}

[[nodiscard]] auto activate_profile(
    github_state& state,
    const activate_host_profile_intent& intent) -> github_transition_output
{
    if(intent.Profile.value() == 0U)
    {
        return reject(github_reason_code::invalid_profile);
    }

    const auto previous = identity_scope(state);
    state.IdentityGeneration = github_identity_generation{
        state.IdentityGeneration.value() + 1U};
    state.ActiveProfile = intent.Profile;
    state.PendingSession.reset();
    state.Host.reset();
    state.Account.reset();
    state.Capabilities = {};
    state.CapabilityRevision = {};
    state.PendingPullRequest.reset();
    state.ActiveLocator.reset();
    state.ActiveNumber.reset();
    state.Repository.reset();
    state.PullRequest.reset();
    state.AcceptedSnapshot.reset();

    const github_session_request_context context{
        .Request = next_request(state),
        .ApplicationSession = state.ApplicationSession,
        .Profile = intent.Profile,
        .IdentityGeneration = state.IdentityGeneration,
    };
    state.PendingSession = context;

    github_transition_output output;
    output.Changed = github_changed_domain::identity
        | github_changed_domain::capabilities
        | github_changed_domain::pull_request
        | github_changed_domain::status;
    output.Requests.push_back(invalidate_github_identity_request{
        .ApplicationSession = state.ApplicationSession,
        .NewGeneration = state.IdentityGeneration,
        .Previous = previous,
    });
    output.Requests.push_back(
        establish_github_session_request{.Context = context});
    output.Identity = identity_delta(state);
    output.Capabilities = capability_delta(state);
    output.PullRequest = pull_request_summary_delta{.Summary = std::nullopt};
    output.Status =
        github_status_delta{.Status = github_status::establishing_session};
    publish_commands(state, output);
    return output;
}

[[nodiscard]] auto session_ready(
    github_state& state,
    const github_session_ready_event& event) -> github_transition_output
{
    if(!current_session(state, event.Context))
    {
        return reject(github_reason_code::stale_completion);
    }

    state.PendingSession.reset();
    if(
        event.Host.value() == 0U || event.Account.value() == 0U
        || event.CapabilityRevision.value() == 0U)
    {
        auto output =
            reject(github_reason_code::completion_identity_mismatch);
        output.Status =
            github_status_delta{.Status = github_status::session_failed};
        output.Changed = output.Changed | github_changed_domain::status;
        publish_commands(state, output);
        return output;
    }
    if(!valid_capabilities(event.Capabilities))
    {
        auto output = reject(github_reason_code::malformed_response);
        output.Status =
            github_status_delta{.Status = github_status::session_failed};
        output.Changed = output.Changed | github_changed_domain::status;
        publish_commands(state, output);
        return output;
    }

    state.Host = event.Host;
    state.Account = event.Account;
    state.Capabilities = event.Capabilities;
    state.CapabilityRevision = event.CapabilityRevision;

    github_transition_output output;
    output.Changed = github_changed_domain::identity
        | github_changed_domain::capabilities
        | github_changed_domain::status;
    output.Identity = identity_delta(state);
    output.Capabilities = capability_delta(state);
    output.Status = github_status_delta{.Status = github_status::session_ready};
    publish_commands(state, output);
    return output;
}

[[nodiscard]] auto session_failed(
    github_state& state,
    const github_session_request_context& context,
    const github_failure failure) -> github_transition_output
{
    if(!current_session(state, context))
    {
        return reject(github_reason_code::stale_completion);
    }

    state.PendingSession.reset();
    auto output = reject(reason_for(failure));
    output.Status =
        github_status_delta{.Status = github_status::session_failed};
    output.Changed = output.Changed | github_changed_domain::status;
    publish_commands(state, output);
    return output;
}

[[nodiscard]] auto session_cancelled(
    github_state& state,
    const github_session_request_context& context) -> github_transition_output
{
    if(!current_session(state, context))
    {
        return reject(github_reason_code::stale_completion);
    }

    state.PendingSession.reset();
    auto output = reject(github_reason_code::cancelled);
    output.Status =
        github_status_delta{.Status = github_status::session_cancelled};
    output.Changed = output.Changed | github_changed_domain::status;
    publish_commands(state, output);
    return output;
}

[[nodiscard]] auto start_pull_request(
    github_state& state,
    repository_locator locator,
    const pull_request_number number,
    const bool refresh) -> github_transition_output
{
    if(state.PendingSession.has_value())
    {
        return reject(github_reason_code::session_in_progress);
    }
    if(
        !state.ActiveProfile.has_value() || !state.Host.has_value()
        || !state.Account.has_value())
    {
        return reject(github_reason_code::session_required);
    }
    if(state.PendingPullRequest.has_value())
    {
        return reject(github_reason_code::pull_request_load_in_progress);
    }
    if(!valid(locator))
    {
        return reject(github_reason_code::invalid_repository);
    }
    if(!number.valid())
    {
        return reject(github_reason_code::invalid_pull_request_number);
    }
    if(
        refresh
        && (
            !state.Repository.has_value() || !state.PullRequest.has_value()
            || !state.AcceptedSnapshot.has_value()))
    {
        return reject(github_reason_code::no_active_pull_request);
    }

    const auto expected_repository =
        refresh ? state.Repository : std::optional<repository_id>{};
    const auto expected_pull_request =
        refresh ? state.PullRequest : std::optional<pull_request_id>{};

    if(!refresh)
    {
        state.ActiveLocator = locator;
        state.ActiveNumber = number;
        state.Repository.reset();
        state.PullRequest.reset();
        state.AcceptedSnapshot.reset();
    }

    state.PullRequestGeneration = request_generation{
        state.PullRequestGeneration.value() + 1U};
    pull_request_request_context context{
        .Request = next_request(state),
        .ApplicationSession = state.ApplicationSession,
        .Profile = *state.ActiveProfile,
        .Host = *state.Host,
        .Account = *state.Account,
        .Repository = std::move(locator),
        .Number = number,
        .ExpectedRepository = expected_repository,
        .ExpectedPullRequest = expected_pull_request,
        .Capabilities = state.CapabilityRevision,
        .IdentityGeneration = state.IdentityGeneration,
        .Generation = state.PullRequestGeneration,
    };
    state.PendingPullRequest = context;

    github_transition_output output;
    output.Changed =
        github_changed_domain::status | github_changed_domain::pull_request;
    output.Requests.push_back(fetch_pull_request_request{.Context = context});
    output.Status =
        github_status_delta{.Status = github_status::loading_pull_request};
    if(!refresh)
    {
        output.PullRequest =
            pull_request_summary_delta{.Summary = std::nullopt};
    }
    publish_commands(state, output);
    return output;
}

[[nodiscard]] auto open_pull_request(
    github_state& state,
    const open_pull_request_intent& intent) -> github_transition_output
{
    return start_pull_request(
        state,
        intent.Repository,
        intent.Number,
        false);
}

[[nodiscard]] auto refresh_pull_request(github_state& state)
    -> github_transition_output
{
    if(!state.ActiveLocator.has_value() || !state.ActiveNumber.has_value())
    {
        return reject(github_reason_code::no_active_pull_request);
    }

    return start_pull_request(
        state,
        *state.ActiveLocator,
        *state.ActiveNumber,
        true);
}

[[nodiscard]] auto pull_request_loaded(
    github_state& state,
    const pull_request_loaded_event& event) -> github_transition_output
{
    if(!current_pull_request(state, event.Context))
    {
        return reject(github_reason_code::stale_completion);
    }

    state.PendingPullRequest.reset();
    const auto valid_identity =
        event.Repository.value() != 0U && event.PullRequest.value() != 0U
        && event.Snapshot.value() != 0U
        && (
            !event.Context.ExpectedRepository.has_value()
            || *event.Context.ExpectedRepository == event.Repository)
        && (
            !event.Context.ExpectedPullRequest.has_value()
            || *event.Context.ExpectedPullRequest == event.PullRequest);
    if(!valid_identity)
    {
        auto output =
            reject(github_reason_code::completion_identity_mismatch);
        output.Status =
            github_status_delta{.Status = github_status::pull_request_failed};
        output.Changed = output.Changed | github_changed_domain::status;
        publish_commands(state, output);
        return output;
    }

    state.Repository = event.Repository;
    state.PullRequest = event.PullRequest;
    state.AcceptedSnapshot = event.Snapshot;

    github_transition_output output;
    output.Changed = github_changed_domain::pull_request
        | github_changed_domain::status;
    output.AcceptedSnapshot = accepted_snapshot_delta{
        .Context = event.Context,
        .Repository = event.Repository,
        .PullRequest = event.PullRequest,
        .Snapshot = event.Snapshot,
    };
    output.PullRequest =
        pull_request_summary_delta{.Summary = summary(state)};
    output.Status =
        github_status_delta{.Status = github_status::pull_request_ready};
    publish_commands(state, output);
    return output;
}

[[nodiscard]] auto pull_request_failed(
    github_state& state,
    const pull_request_request_context& context,
    const github_failure failure) -> github_transition_output
{
    if(!current_pull_request(state, context))
    {
        return reject(github_reason_code::stale_completion);
    }

    state.PendingPullRequest.reset();
    auto output = reject(reason_for(failure));
    output.Status =
        github_status_delta{.Status = github_status::pull_request_failed};
    output.Changed = output.Changed | github_changed_domain::status;
    publish_commands(state, output);
    return output;
}

[[nodiscard]] auto pull_request_cancelled(
    github_state& state,
    const pull_request_request_context& context) -> github_transition_output
{
    if(!current_pull_request(state, context))
    {
        return reject(github_reason_code::stale_completion);
    }

    state.PendingPullRequest.reset();
    auto output = reject(github_reason_code::cancelled);
    output.Status =
        github_status_delta{.Status = github_status::pull_request_cancelled};
    output.Changed = output.Changed | github_changed_domain::status;
    publish_commands(state, output);
    return output;
}
}

auto github_commands(const github_state& state)
    -> std::vector<github_command_projection>
{
    std::vector<github_command_projection> commands;
    commands.reserve(4U);
    commands.push_back(
        command(github_command_id::activate_host_profile, true));

    if(state.PendingSession.has_value())
    {
        commands.push_back(command(
            github_command_id::open_pull_request,
            false,
            github_reason_code::session_in_progress));
    }
    else if(!state.Host.has_value() || !state.Account.has_value())
    {
        commands.push_back(command(
            github_command_id::open_pull_request,
            false,
            github_reason_code::session_required));
    }
    else if(state.PendingPullRequest.has_value())
    {
        commands.push_back(command(
            github_command_id::open_pull_request,
            false,
            github_reason_code::pull_request_load_in_progress));
    }
    else
    {
        commands.push_back(
            command(github_command_id::open_pull_request, true));
    }

    if(state.PendingPullRequest.has_value())
    {
        commands.push_back(command(
            github_command_id::refresh_pull_request,
            false,
            github_reason_code::pull_request_load_in_progress));
    }
    else if(!state.AcceptedSnapshot.has_value())
    {
        commands.push_back(command(
            github_command_id::refresh_pull_request,
            false,
            github_reason_code::no_active_pull_request));
    }
    else
    {
        commands.push_back(
            command(github_command_id::refresh_pull_request, true));
    }

    if(!state.AcceptedSnapshot.has_value())
    {
        commands.push_back(command(
            github_command_id::show_review_threads,
            false,
            github_reason_code::no_active_pull_request));
    }
    else if(
        state.Capabilities.ReviewThreads
        == github_capability_state::unknown)
    {
        commands.push_back(command(
            github_command_id::show_review_threads,
            false,
            github_reason_code::capability_unknown));
    }
    else if(
        state.Capabilities.ReviewThreads
        == github_capability_state::unsupported)
    {
        commands.push_back(command(
            github_command_id::show_review_threads,
            false,
            github_reason_code::capability_unsupported));
    }
    else if(
        state.Capabilities.ReviewThreads
        == github_capability_state::supported)
    {
        commands.push_back(
            command(github_command_id::show_review_threads, true));
    }
    else
    {
        commands.push_back(command(
            github_command_id::show_review_threads,
            false,
            github_reason_code::capability_unknown));
    }

    return commands;
}

auto transition(
    github_state& state,
    const github_event& event) -> github_transition_output
{
    return std::visit(
        [&state](const auto& typed_event) -> github_transition_output {
            using event_type = std::decay_t<decltype(typed_event)>;
            if constexpr(
                std::is_same_v<
                    event_type,
                    activate_host_profile_intent>)
            {
                return activate_profile(state, typed_event);
            }
            else if constexpr(
                std::is_same_v<event_type, open_pull_request_intent>)
            {
                return open_pull_request(state, typed_event);
            }
            else if constexpr(
                std::is_same_v<event_type, refresh_pull_request_intent>)
            {
                return refresh_pull_request(state);
            }
            else if constexpr(
                std::is_same_v<event_type, github_session_ready_event>)
            {
                return session_ready(state, typed_event);
            }
            else if constexpr(
                std::is_same_v<event_type, github_session_failed_event>)
            {
                return session_failed(
                    state,
                    typed_event.Context,
                    typed_event.Failure);
            }
            else if constexpr(
                std::is_same_v<event_type, github_session_cancelled_event>)
            {
                return session_cancelled(state, typed_event.Context);
            }
            else if constexpr(
                std::is_same_v<event_type, pull_request_loaded_event>)
            {
                return pull_request_loaded(state, typed_event);
            }
            else if constexpr(
                std::is_same_v<
                    event_type,
                    pull_request_load_failed_event>)
            {
                return pull_request_failed(
                    state,
                    typed_event.Context,
                    typed_event.Failure);
            }
            else
            {
                return pull_request_cancelled(state, typed_event.Context);
            }
        },
        event);
}
}
