#pragma once

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <optional>
#include <variant>
#include <vector>

namespace bpr::core
{
struct diff_request_context
{
    request_id Request;
    application_session_id ApplicationSession;
    file_comparison_key Key;
    request_generation Generation;

    auto operator<=>(const diff_request_context&) const = default;
};

struct request_file_diff_intent
{
    file_comparison_key Key;
    diff_options Options;

    auto operator<=>(const request_file_diff_intent&) const = default;
};

struct cancel_file_diff_intent
{
    auto operator<=>(const cancel_file_diff_intent&) const = default;
};

struct invalidate_diff_authority_intent
{
    pull_request_authority Authority;

    auto operator<=>(const invalidate_diff_authority_intent&) const = default;
};

struct file_diff_computed_event
{
    diff_request_context Context;

    auto operator<=>(const file_diff_computed_event&) const = default;
};

struct file_diff_failed_event
{
    diff_request_context Context;
    diff_reason_code Reason = diff_reason_code::host_failure;

    auto operator<=>(const file_diff_failed_event&) const = default;
};

using diff_event = std::variant<request_file_diff_intent,
                                cancel_file_diff_intent,
                                invalidate_diff_authority_intent,
                                file_diff_computed_event,
                                file_diff_failed_event>;

struct compute_file_diff_request
{
    diff_request_context Context;
    diff_options Options;

    auto operator<=>(const compute_file_diff_request&) const = default;
};

struct cancel_file_diff_request
{
    diff_request_context Context;

    auto operator<=>(const cancel_file_diff_request&) const = default;
};

using diff_host_request =
    std::variant<compute_file_diff_request, cancel_file_diff_request>;

enum class diff_transition_reason : std::uint8_t
{
    none,
    invalid_request,
    unsupported_algorithm,
    request_identity_exhausted,
    stale_completion,
    invalid_failure,
};

struct accepted_file_diff_delta
{
    diff_request_context Context;

    auto operator<=>(const accepted_file_diff_delta&) const = default;
};

struct diff_terminal_delta
{
    diff_request_context Context;
    diff_reason_code Reason = diff_reason_code::none;

    auto operator<=>(const diff_terminal_delta&) const = default;
};

struct diff_transition_output
{
    std::vector<diff_host_request> Requests;
    std::optional<accepted_file_diff_delta> Accepted;
    std::optional<diff_terminal_delta> Terminal;
    diff_transition_reason Reason = diff_transition_reason::none;

    auto operator<=>(const diff_transition_output&) const = default;
};

struct diff_state
{
    application_session_id ApplicationSession;
    request_id NextRequest{1U};
    request_generation Generation;
    std::optional<diff_request_context> Pending;
    std::optional<diff_request_context> Current;
    diff_reason_code LastReason = diff_reason_code::none;

    auto operator<=>(const diff_state&) const = default;
};

[[nodiscard]] auto valid(const diff_request_context& context) noexcept -> bool;
[[nodiscard]] auto transition(diff_state& state, const diff_event& event)
    -> diff_transition_output;
} // namespace bpr::core
