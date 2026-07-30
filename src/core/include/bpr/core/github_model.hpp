#pragma once

#include "bpr/core/model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::core
{
struct host_profile_id_tag;
struct git_host_id_tag;
struct github_account_id_tag;
struct repository_id_tag;
struct pull_request_id_tag;
struct provider_snapshot_id_tag;
struct capability_revision_tag;
struct github_identity_generation_tag;

using host_profile_id = strong_id<host_profile_id_tag>;
using git_host_id = strong_id<git_host_id_tag>;
using github_account_id = strong_id<github_account_id_tag>;
using repository_id = strong_id<repository_id_tag>;
using pull_request_id = strong_id<pull_request_id_tag>;
using provider_snapshot_id = strong_id<provider_snapshot_id_tag>;
using capability_revision = strong_id<capability_revision_tag>;
using github_identity_generation =
    strong_id<github_identity_generation_tag>;

class pull_request_number
{
public:
    constexpr pull_request_number() = default;
    explicit constexpr pull_request_number(const std::uint32_t value) noexcept
        : Value{value}
    {
    }

    [[nodiscard]] constexpr auto value() const noexcept -> std::uint32_t
    {
        return Value;
    }

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        return Value != 0U;
    }

    auto operator<=>(const pull_request_number&) const = default;

private:
    std::uint32_t Value = 0;
};

struct repository_locator
{
    std::string Owner;
    std::string Name;

    auto operator<=>(const repository_locator&) const = default;
};

inline constexpr std::size_t maximum_repository_owner_length = 100U;
inline constexpr std::size_t maximum_repository_name_length = 100U;

[[nodiscard]] auto valid(const repository_locator& locator) noexcept -> bool;

enum class github_capability_state : std::uint8_t
{
    unknown,
    supported,
    unsupported,
};

enum class github_capability : std::uint8_t
{
    review_threads,
    viewed_file_mutations,
    resolve_thread,
    unresolve_thread,
};

struct github_capabilities
{
    github_capability_state ReviewThreads =
        github_capability_state::unknown;
    github_capability_state ViewedFileMutations =
        github_capability_state::unknown;
    github_capability_state ResolveThread =
        github_capability_state::unknown;
    github_capability_state UnresolveThread =
        github_capability_state::unknown;

    auto operator<=>(const github_capabilities&) const = default;
};

[[nodiscard]] auto capability_state(
    const github_capabilities& capabilities,
    github_capability capability) noexcept -> github_capability_state;

[[nodiscard]] auto capability_supported(
    const github_capabilities& capabilities,
    github_capability capability) noexcept -> bool;

struct github_session_request_context
{
    request_id Request;
    application_session_id ApplicationSession;
    host_profile_id Profile;
    github_identity_generation IdentityGeneration;

    auto operator<=>(const github_session_request_context&) const = default;
};

struct pull_request_request_context
{
    request_id Request;
    application_session_id ApplicationSession;
    host_profile_id Profile;
    git_host_id Host;
    github_account_id Account;
    repository_locator Repository;
    pull_request_number Number;
    std::optional<repository_id> ExpectedRepository;
    std::optional<pull_request_id> ExpectedPullRequest;
    capability_revision Capabilities;
    github_identity_generation IdentityGeneration;
    request_generation Generation;

    auto operator<=>(const pull_request_request_context&) const = default;
};

struct activate_host_profile_intent
{
    host_profile_id Profile;

    auto operator<=>(const activate_host_profile_intent&) const = default;
};

struct open_pull_request_intent
{
    repository_locator Repository;
    pull_request_number Number;

    auto operator<=>(const open_pull_request_intent&) const = default;
};

struct refresh_pull_request_intent
{
    auto operator<=>(const refresh_pull_request_intent&) const = default;
};

enum class github_failure : std::uint8_t
{
    authentication,
    permission,
    network,
    api_version,
    capability,
    rate_limit,
    malformed_response,
    response_too_large,
    vault,
    certificate,
    redirect,
    timeout,
    cli_unavailable,
};

struct github_session_ready_event
{
    github_session_request_context Context;
    git_host_id Host;
    github_account_id Account;
    github_capabilities Capabilities;
    capability_revision CapabilityRevision;

    auto operator<=>(const github_session_ready_event&) const = default;
};

struct github_session_failed_event
{
    github_session_request_context Context;
    github_failure Failure = github_failure::network;

    auto operator<=>(const github_session_failed_event&) const = default;
};

struct github_session_cancelled_event
{
    github_session_request_context Context;

    auto operator<=>(const github_session_cancelled_event&) const = default;
};

struct pull_request_loaded_event
{
    pull_request_request_context Context;
    repository_id Repository;
    pull_request_id PullRequest;
    provider_snapshot_id Snapshot;

    auto operator<=>(const pull_request_loaded_event&) const = default;
};

struct pull_request_load_failed_event
{
    pull_request_request_context Context;
    github_failure Failure = github_failure::network;

    auto operator<=>(const pull_request_load_failed_event&) const = default;
};

struct pull_request_load_cancelled_event
{
    pull_request_request_context Context;

    auto operator<=>(const pull_request_load_cancelled_event&) const = default;
};

using github_event = std::variant<
    activate_host_profile_intent,
    open_pull_request_intent,
    refresh_pull_request_intent,
    github_session_ready_event,
    github_session_failed_event,
    github_session_cancelled_event,
    pull_request_loaded_event,
    pull_request_load_failed_event,
    pull_request_load_cancelled_event>;

struct github_identity_scope
{
    std::optional<host_profile_id> Profile;
    std::optional<git_host_id> Host;
    std::optional<github_account_id> Account;
    std::optional<repository_id> Repository;
    std::optional<pull_request_id> PullRequest;
    std::optional<provider_snapshot_id> Snapshot;

    auto operator<=>(const github_identity_scope&) const = default;
};

struct invalidate_github_identity_request
{
    application_session_id ApplicationSession;
    github_identity_generation NewGeneration;
    github_identity_scope Previous;

    auto operator<=>(const invalidate_github_identity_request&) const = default;
};

struct establish_github_session_request
{
    github_session_request_context Context;

    auto operator<=>(const establish_github_session_request&) const = default;
};

struct fetch_pull_request_request
{
    pull_request_request_context Context;

    auto operator<=>(const fetch_pull_request_request&) const = default;
};

using github_host_request = std::variant<
    invalidate_github_identity_request,
    establish_github_session_request,
    fetch_pull_request_request>;

enum class github_command_id : std::uint8_t
{
    activate_host_profile,
    open_pull_request,
    refresh_pull_request,
    show_review_threads,
};

enum class github_reason_code : std::uint8_t
{
    none,
    invalid_profile,
    session_in_progress,
    session_required,
    invalid_repository,
    invalid_pull_request_number,
    pull_request_load_in_progress,
    no_active_pull_request,
    capability_unknown,
    capability_unsupported,
    stale_completion,
    completion_identity_mismatch,
    authentication_failed,
    permission_denied,
    network_unavailable,
    api_version_unsupported,
    capability_unavailable,
    rate_limited,
    malformed_response,
    response_too_large,
    vault_unavailable,
    certificate_rejected,
    redirect_rejected,
    timed_out,
    cli_unavailable,
    cancelled,
};

[[nodiscard]] auto reason_for(github_failure failure) noexcept
    -> github_reason_code;

struct github_command_projection
{
    github_command_id Id = github_command_id::activate_host_profile;
    bool Enabled = true;
    github_reason_code DisabledReason = github_reason_code::none;

    auto operator<=>(const github_command_projection&) const = default;
};

enum class github_status : std::uint8_t
{
    no_profile,
    establishing_session,
    session_ready,
    session_failed,
    session_cancelled,
    loading_pull_request,
    pull_request_ready,
    pull_request_failed,
    pull_request_cancelled,
};

struct github_status_delta
{
    github_status Status = github_status::no_profile;

    auto operator<=>(const github_status_delta&) const = default;
};

struct github_diagnostic
{
    github_reason_code Code = github_reason_code::none;

    auto operator<=>(const github_diagnostic&) const = default;
};

struct github_identity_delta
{
    std::optional<host_profile_id> Profile;
    std::optional<git_host_id> Host;
    std::optional<github_account_id> Account;
    github_identity_generation Generation;

    auto operator<=>(const github_identity_delta&) const = default;
};

struct github_capability_delta
{
    github_capabilities Capabilities;
    capability_revision Revision;

    auto operator<=>(const github_capability_delta&) const = default;
};

struct github_pull_request_summary
{
    repository_locator Locator;
    pull_request_number Number;
    repository_id Repository;
    pull_request_id PullRequest;
    provider_snapshot_id Snapshot;

    auto operator<=>(const github_pull_request_summary&) const = default;
};

struct pull_request_summary_delta
{
    std::optional<github_pull_request_summary> Summary;

    auto operator<=>(const pull_request_summary_delta&) const = default;
};

struct accepted_snapshot_delta
{
    pull_request_request_context Context;
    repository_id Repository;
    pull_request_id PullRequest;
    provider_snapshot_id Snapshot;

    auto operator<=>(const accepted_snapshot_delta&) const = default;
};

enum class github_changed_domain : std::uint16_t
{
    none = 0,
    identity = 1U << 0U,
    capabilities = 1U << 1U,
    pull_request = 1U << 2U,
    commands = 1U << 3U,
    status = 1U << 4U,
    diagnostics = 1U << 5U,
};

[[nodiscard]] constexpr auto operator|(
    const github_changed_domain left,
    const github_changed_domain right) noexcept -> github_changed_domain
{
    return static_cast<github_changed_domain>(
        static_cast<std::uint16_t>(left)
        | static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr auto has_changed(
    const github_changed_domain value,
    const github_changed_domain expected) noexcept -> bool
{
    return (
        static_cast<std::uint16_t>(value)
        & static_cast<std::uint16_t>(expected))
        != 0U;
}

struct github_transition_output
{
    github_changed_domain Changed = github_changed_domain::none;
    std::vector<github_host_request> Requests;
    std::vector<github_command_projection> Commands;
    std::optional<github_status_delta> Status;
    std::optional<github_identity_delta> Identity;
    std::optional<github_capability_delta> Capabilities;
    std::optional<pull_request_summary_delta> PullRequest;
    std::optional<accepted_snapshot_delta> AcceptedSnapshot;
    std::vector<github_diagnostic> Diagnostics;

    auto operator<=>(const github_transition_output&) const = default;
};

struct github_state
{
    application_session_id ApplicationSession;
    request_id NextRequest{1};
    std::optional<host_profile_id> ActiveProfile;
    github_identity_generation IdentityGeneration;
    std::optional<github_session_request_context> PendingSession;
    std::optional<git_host_id> Host;
    std::optional<github_account_id> Account;
    github_capabilities Capabilities;
    capability_revision CapabilityRevision;
    request_generation PullRequestGeneration;
    std::optional<pull_request_request_context> PendingPullRequest;
    std::optional<repository_locator> ActiveLocator;
    std::optional<pull_request_number> ActiveNumber;
    std::optional<repository_id> Repository;
    std::optional<pull_request_id> PullRequest;
    std::optional<provider_snapshot_id> AcceptedSnapshot;

    auto operator<=>(const github_state&) const = default;
};
}
