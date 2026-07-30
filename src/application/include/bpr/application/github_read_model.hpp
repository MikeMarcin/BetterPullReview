#pragma once

#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bpr::application
{
template<typename Tag>
class github_text_id
{
public:
    github_text_id() = default;
    explicit github_text_id(std::string value)
        : Value{std::move(value)}
    {
    }

    [[nodiscard]] auto value() const noexcept -> std::string_view
    {
        return Value;
    }

    auto operator<=>(const github_text_id&) const = default;

private:
    std::string Value;
};

struct github_node_id_tag;
struct github_actor_id_tag;
struct github_review_id_tag;
struct github_thread_id_tag;
struct github_comment_id_tag;
struct github_check_id_tag;
struct commit_oid_tag;

using github_node_id = github_text_id<github_node_id_tag>;
using github_actor_id = github_text_id<github_actor_id_tag>;
using github_review_id = github_text_id<github_review_id_tag>;
using github_thread_id = github_text_id<github_thread_id_tag>;
using github_comment_id = github_text_id<github_comment_id_tag>;
using github_check_id = github_text_id<github_check_id_tag>;
using commit_oid = github_text_id<commit_oid_tag>;

inline constexpr std::uint32_t maximum_github_line = 2'147'483'647U;

struct github_actor_record
{
    github_actor_id Actor;
    std::string Login;

    auto operator<=>(const github_actor_record&) const = default;
};

enum class github_pull_request_state : std::uint8_t
{
    open,
    closed,
    merged,
};

enum class github_file_change : std::uint8_t
{
    uninitialized,
    added,
    modified,
    removed,
    renamed,
    copied,
    type_changed,
    unchanged,
};

enum class github_file_content_kind : std::uint8_t
{
    unknown,
    text,
    binary,
};

enum class github_review_state : std::uint8_t
{
    uninitialized,
    pending,
    commented,
    approved,
    changes_requested,
    dismissed,
};

enum class github_review_thread_subject : std::uint8_t
{
    line,
    file,
};

enum class github_check_status : std::uint8_t
{
    uninitialized,
    queued,
    in_progress,
    completed,
    waiting,
    requested,
    pending,
};

enum class github_check_conclusion : std::uint8_t
{
    uninitialized,
    none,
    success,
    failure,
    neutral,
    cancelled,
    skipped,
    timed_out,
    action_required,
    stale,
};

struct github_viewer_record
{
    core::github_account_id Account;
    github_actor_id Node;
    std::string Login;
    std::string DisplayName;

    auto operator<=>(const github_viewer_record&) const = default;
};

struct github_repository_record
{
    core::repository_id Repository;
    github_node_id Node;
    std::string Owner;
    std::string Name;
    std::string DefaultBranch;
    bool Private = false;

    auto operator<=>(const github_repository_record&) const = default;
};

struct github_pull_request_record
{
    core::pull_request_id PullRequest;
    core::repository_id Repository;
    core::pull_request_number Number;
    github_node_id Node;
    std::string Title;
    std::string Body;
    github_pull_request_state State = github_pull_request_state::open;
    bool Draft = false;
    std::optional<github_actor_record> Author;
    commit_oid BaseOid;
    commit_oid HeadOid;
    std::string BaseRef;
    std::string HeadRef;

    auto operator<=>(const github_pull_request_record&) const = default;
};

struct github_file_record
{
    std::string Path;
    std::optional<std::string> PreviousPath;
    github_file_change Change = github_file_change::uninitialized;
    std::uint64_t Additions = 0;
    std::uint64_t Deletions = 0;
    github_file_content_kind ContentKind =
        github_file_content_kind::unknown;

    auto operator<=>(const github_file_record&) const = default;
};

struct github_commit_record
{
    commit_oid Oid;
    github_node_id Node;
    std::optional<github_actor_record> Author;
    std::string Headline;

    auto operator<=>(const github_commit_record&) const = default;
};

struct github_review_record
{
    github_review_id Review;
    std::optional<github_actor_record> Author;
    github_review_state State = github_review_state::uninitialized;
    std::optional<commit_oid> Commit;
    std::string Body;

    auto operator<=>(const github_review_record&) const = default;
};

struct github_review_comment_record
{
    github_comment_id Comment;
    std::optional<github_comment_id> ReplyTo;
    std::optional<github_actor_record> Author;
    std::string Body;
    std::string Path;
    std::optional<std::uint32_t> Line;
    bool Outdated = false;
    bool Deleted = false;

    auto operator<=>(const github_review_comment_record&) const = default;
};

struct github_review_thread_record
{
    github_thread_id Thread;
    github_review_thread_subject Subject =
        github_review_thread_subject::line;
    std::string Path;
    std::optional<std::uint32_t> Line;
    bool Resolved = false;
    bool Outdated = false;
    std::vector<github_review_comment_record> Comments;

    auto operator<=>(const github_review_thread_record&) const = default;
};

struct github_issue_comment_record
{
    github_comment_id Comment;
    std::optional<github_actor_record> Author;
    std::string Body;
    bool Deleted = false;

    auto operator<=>(const github_issue_comment_record&) const = default;
};

struct github_check_record
{
    github_check_id Check;
    std::string Name;
    github_check_status Status = github_check_status::uninitialized;
    github_check_conclusion Conclusion =
        github_check_conclusion::uninitialized;

    auto operator<=>(const github_check_record&) const = default;
};

struct github_search_result_record
{
    core::repository_id Repository;
    github_node_id PullRequestNode;
    core::pull_request_number Number;
    std::string Title;
    github_pull_request_state State = github_pull_request_state::open;
    bool Draft = false;

    auto operator<=>(const github_search_result_record&) const = default;
};

struct github_capability_evidence
{
    core::github_capabilities Capabilities;
    core::capability_revision Revision;
    std::uint32_t ContractVersion = 1;

    auto operator<=>(const github_capability_evidence&) const = default;
};

struct github_rate_limit_record
{
    std::uint64_t Limit = 0;
    std::uint64_t Remaining = 0;
    std::uint64_t ResetUnixSeconds = 0;
    bool SecondaryLimited = false;

    auto operator<=>(const github_rate_limit_record&) const = default;
};

struct github_session_read_model
{
    core::host_profile_id Profile;
    core::git_host_id Host;
    github_viewer_record Viewer;
    github_capability_evidence CapabilityEvidence;
    github_rate_limit_record RateLimit;

    auto operator<=>(const github_session_read_model&) const = default;
};

struct github_pull_request_snapshot
{
    core::host_profile_id Profile;
    core::git_host_id Host;
    core::github_account_id Account;
    github_actor_record Viewer;
    core::provider_snapshot_id Snapshot;
    github_repository_record Repository;
    github_pull_request_record PullRequest;
    std::vector<github_file_record> Files;
    std::vector<github_commit_record> Commits;
    std::vector<github_review_record> Reviews;
    std::vector<github_review_thread_record> ReviewThreads;
    std::vector<github_issue_comment_record> IssueComments;
    std::vector<github_check_record> Checks;
    std::vector<github_search_result_record> SearchResults;
    github_capability_evidence CapabilityEvidence;
    github_rate_limit_record RateLimit;

    auto operator<=>(const github_pull_request_snapshot&) const = default;
};

struct github_read_limits
{
    std::size_t MaximumIdentityBytes = 256U;
    std::size_t MaximumLoginBytes = 256U;
    std::size_t MaximumDisplayNameBytes = 1024U;
    std::size_t MaximumTitleBytes = 1024U;
    std::size_t MaximumBodyBytes = 256U * 1024U;
    std::size_t MaximumPathBytes = 4U * 1024U;
    std::size_t MaximumRefBytes = 256U;
    std::size_t MaximumHeadlineBytes = 1024U;
    std::size_t MaximumCheckNameBytes = 1024U;
    std::size_t MaximumFiles = 3'000U;
    std::size_t MaximumCommits = 250U;
    std::size_t MaximumReviews = 10'000U;
    std::size_t MaximumReviewThreads = 10'000U;
    std::size_t MaximumCommentsPerThread = 10'000U;
    std::size_t MaximumIssueComments = 10'000U;
    std::size_t MaximumTotalComments = 50'000U;
    std::size_t MaximumChecks = 10'000U;
    std::size_t MaximumSearchResults = 1'000U;
    std::size_t MaximumTotalRecords = 50'000U;
    std::size_t MaximumTotalTextBytes = 16U * 1024U * 1024U;

    auto operator<=>(const github_read_limits&) const = default;
};

enum class github_read_validation_error : std::uint8_t
{
    none,
    invalid_context,
    invalid_limits,
    missing_identity,
    identity_mismatch,
    missing_required_text,
    invalid_text,
    text_too_long,
    invalid_repository,
    invalid_pull_request_number,
    invalid_commit_oid,
    invalid_line,
    invalid_enum,
    invalid_capability_revision,
    invalid_rate_limit,
    collection_too_large,
    duplicate_identity,
    invalid_relationship,
    allocation_limit,
    total_text_too_large,
};

struct github_read_validation
{
    github_read_validation_error Error =
        github_read_validation_error::none;

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        return Error == github_read_validation_error::none;
    }

    auto operator<=>(const github_read_validation&) const = default;
};

namespace detail
{
class github_text_budget
{
public:
    explicit github_text_budget(const github_read_limits& limits)
        : Limits{limits}
    {
    }

    [[nodiscard]] auto consume(
        const std::string_view text,
        const std::size_t field_limit,
        const bool required = false) noexcept
        -> github_read_validation_error
    {
        if(required && text.empty())
        {
            return github_read_validation_error::missing_required_text;
        }
        if(text.find('\0') != std::string_view::npos)
        {
            return github_read_validation_error::invalid_text;
        }
        if(text.size() > field_limit)
        {
            return github_read_validation_error::text_too_long;
        }
        if(
            text.size() > Limits.MaximumTotalTextBytes
            || Total > Limits.MaximumTotalTextBytes - text.size())
        {
            return github_read_validation_error::total_text_too_large;
        }
        Total += text.size();
        return github_read_validation_error::none;
    }

private:
    const github_read_limits& Limits;
    std::size_t Total = 0;
};

[[nodiscard]] inline auto validation(
    const github_read_validation_error error) noexcept
    -> github_read_validation
{
    return github_read_validation{.Error = error};
}

[[nodiscard]] constexpr auto within_security_ceiling(
    const github_read_limits& limits) noexcept -> bool
{
    constexpr github_read_limits ceiling;
    return limits.MaximumIdentityBytes <= ceiling.MaximumIdentityBytes
        && limits.MaximumLoginBytes <= ceiling.MaximumLoginBytes
        && limits.MaximumDisplayNameBytes
            <= ceiling.MaximumDisplayNameBytes
        && limits.MaximumTitleBytes <= ceiling.MaximumTitleBytes
        && limits.MaximumBodyBytes <= ceiling.MaximumBodyBytes
        && limits.MaximumPathBytes <= ceiling.MaximumPathBytes
        && limits.MaximumRefBytes <= ceiling.MaximumRefBytes
        && limits.MaximumHeadlineBytes <= ceiling.MaximumHeadlineBytes
        && limits.MaximumCheckNameBytes <= ceiling.MaximumCheckNameBytes
        && limits.MaximumFiles <= ceiling.MaximumFiles
        && limits.MaximumCommits <= ceiling.MaximumCommits
        && limits.MaximumReviews <= ceiling.MaximumReviews
        && limits.MaximumReviewThreads <= ceiling.MaximumReviewThreads
        && limits.MaximumCommentsPerThread
            <= ceiling.MaximumCommentsPerThread
        && limits.MaximumIssueComments <= ceiling.MaximumIssueComments
        && limits.MaximumTotalComments <= ceiling.MaximumTotalComments
        && limits.MaximumChecks <= ceiling.MaximumChecks
        && limits.MaximumSearchResults <= ceiling.MaximumSearchResults
        && limits.MaximumTotalRecords <= ceiling.MaximumTotalRecords
        && limits.MaximumTotalTextBytes <= ceiling.MaximumTotalTextBytes;
}

[[nodiscard]] inline auto valid_context(
    const core::github_session_request_context& context) noexcept -> bool
{
    return context.Request.value() != 0U
        && context.ApplicationSession.value() != 0U
        && context.Profile.value() != 0U
        && context.IdentityGeneration.value() != 0U;
}

[[nodiscard]] inline auto valid_context(
    const core::pull_request_request_context& context) noexcept -> bool
{
    return context.Request.value() != 0U
        && context.ApplicationSession.value() != 0U
        && context.Profile.value() != 0U && context.Host.value() != 0U
        && context.Account.value() != 0U && core::valid(context.Repository)
        && context.Number.valid()
        && (
            !context.ExpectedRepository.has_value()
            || context.ExpectedRepository->value() != 0U)
        && (
            !context.ExpectedPullRequest.has_value()
            || context.ExpectedPullRequest->value() != 0U)
        && context.Capabilities.value() != 0U
        && context.IdentityGeneration.value() != 0U
        && context.Generation.value() != 0U;
}

[[nodiscard]] inline auto ascii_equal_fold(
    const std::string_view left,
    const std::string_view right) noexcept -> bool
{
    if(left.size() != right.size())
    {
        return false;
    }

    for(std::size_t index = 0; index < left.size(); ++index)
    {
        const auto fold = [](const char value) {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a')
                : value;
        };
        if(fold(left[index]) != fold(right[index]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline auto hexadecimal_oid(
    const std::string_view value) noexcept -> bool
{
    if(value.size() != 40U && value.size() != 64U)
    {
        return false;
    }
    for(const auto character : value)
    {
        const auto hexadecimal =
            (character >= '0' && character <= '9')
            || (character >= 'a' && character <= 'f')
            || (character >= 'A' && character <= 'F');
        if(!hexadecimal)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr auto valid_enum(
    const github_pull_request_state value) noexcept -> bool
{
    switch(value)
    {
    case github_pull_request_state::open:
    case github_pull_request_state::closed:
    case github_pull_request_state::merged:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_enum(
    const github_file_change value) noexcept -> bool
{
    switch(value)
    {
    case github_file_change::added:
    case github_file_change::modified:
    case github_file_change::removed:
    case github_file_change::renamed:
    case github_file_change::copied:
    case github_file_change::type_changed:
    case github_file_change::unchanged:
        return true;
    case github_file_change::uninitialized:
        return false;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_enum(
    const github_file_content_kind value) noexcept -> bool
{
    switch(value)
    {
    case github_file_content_kind::unknown:
    case github_file_content_kind::text:
    case github_file_content_kind::binary:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_enum(
    const github_review_state value) noexcept -> bool
{
    switch(value)
    {
    case github_review_state::pending:
    case github_review_state::commented:
    case github_review_state::approved:
    case github_review_state::changes_requested:
    case github_review_state::dismissed:
        return true;
    case github_review_state::uninitialized:
        return false;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_enum(
    const github_review_thread_subject value) noexcept -> bool
{
    switch(value)
    {
    case github_review_thread_subject::line:
    case github_review_thread_subject::file:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_enum(
    const github_check_status value) noexcept -> bool
{
    switch(value)
    {
    case github_check_status::queued:
    case github_check_status::in_progress:
    case github_check_status::completed:
    case github_check_status::waiting:
    case github_check_status::requested:
    case github_check_status::pending:
        return true;
    case github_check_status::uninitialized:
        return false;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_enum(
    const github_check_conclusion value) noexcept -> bool
{
    switch(value)
    {
    case github_check_conclusion::none:
    case github_check_conclusion::success:
    case github_check_conclusion::failure:
    case github_check_conclusion::neutral:
    case github_check_conclusion::cancelled:
    case github_check_conclusion::skipped:
    case github_check_conclusion::timed_out:
    case github_check_conclusion::action_required:
    case github_check_conclusion::stale:
        return true;
    case github_check_conclusion::uninitialized:
        return false;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_enum(
    const core::github_capability_state value) noexcept -> bool
{
    switch(value)
    {
    case core::github_capability_state::unknown:
    case core::github_capability_state::supported:
    case core::github_capability_state::unsupported:
        return true;
    }
    return false;
}

[[nodiscard]] inline auto consume_id(
    github_text_budget& budget,
    const std::string_view value,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    return budget.consume(value, limits.MaximumIdentityBytes, true);
}

[[nodiscard]] inline auto validate_author(
    const std::optional<github_actor_record>& author,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(!author.has_value())
    {
        return github_read_validation_error::none;
    }
    if(author->Actor.value().empty())
    {
        return github_read_validation_error::missing_identity;
    }
    if(const auto error =
           consume_id(budget, author->Actor.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        author->Login,
        limits.MaximumLoginBytes,
        true);
}

[[nodiscard]] inline auto validate_actor(
    const github_actor_record& actor,
    github_text_budget& budget,
    const github_read_limits& limits,
    const bool login_required) noexcept
    -> github_read_validation_error
{
    if(actor.Actor.value().empty())
    {
        return github_read_validation_error::missing_identity;
    }
    if(const auto error =
           consume_id(budget, actor.Actor.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        actor.Login,
        limits.MaximumLoginBytes,
        login_required);
}

[[nodiscard]] inline auto validate_viewer(
    const github_viewer_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(record.Account.value() == 0U)
    {
        return github_read_validation_error::missing_identity;
    }
    if(const auto error = consume_id(budget, record.Node.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           budget.consume(record.Login, limits.MaximumLoginBytes, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        record.DisplayName,
        limits.MaximumDisplayNameBytes);
}

[[nodiscard]] inline auto validate_repository(
    const github_repository_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(record.Repository.value() == 0U)
    {
        return github_read_validation_error::missing_identity;
    }
    if(const auto error = consume_id(budget, record.Node.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(!core::valid(
           core::repository_locator{
               .Owner = record.Owner,
               .Name = record.Name,
           }))
    {
        return github_read_validation_error::invalid_repository;
    }
    if(const auto error = budget.consume(
           record.Owner,
           core::maximum_repository_owner_length,
           true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error = budget.consume(
           record.Name,
           core::maximum_repository_name_length,
           true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        record.DefaultBranch,
        limits.MaximumRefBytes,
        true);
}

[[nodiscard]] inline auto validate_pull_request(
    const github_pull_request_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(
        record.PullRequest.value() == 0U
        || record.Repository.value() == 0U)
    {
        return github_read_validation_error::missing_identity;
    }
    if(!record.Number.valid())
    {
        return github_read_validation_error::invalid_pull_request_number;
    }
    if(!valid_enum(record.State))
    {
        return github_read_validation_error::invalid_enum;
    }
    if(const auto error = consume_id(budget, record.Node.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           validate_author(record.Author, budget, limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           budget.consume(record.Title, limits.MaximumTitleBytes, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           budget.consume(record.Body, limits.MaximumBodyBytes);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(
        !hexadecimal_oid(record.BaseOid.value())
        || !hexadecimal_oid(record.HeadOid.value()))
    {
        return github_read_validation_error::invalid_commit_oid;
    }
    if(const auto error =
           budget.consume(record.BaseOid.value(), 64U, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           budget.consume(record.HeadOid.value(), 64U, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           budget.consume(record.BaseRef, limits.MaximumRefBytes, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        record.HeadRef,
        limits.MaximumRefBytes,
        true);
}

[[nodiscard]] inline auto validate_file(
    const github_file_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(!valid_enum(record.Change) || !valid_enum(record.ContentKind))
    {
        return github_read_validation_error::invalid_enum;
    }
    if(
        record.PreviousPath.has_value()
        && record.Change != github_file_change::renamed
        && record.Change != github_file_change::copied)
    {
        return github_read_validation_error::invalid_relationship;
    }
    if(const auto error =
           budget.consume(record.Path, limits.MaximumPathBytes, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(record.PreviousPath.has_value())
    {
        return budget.consume(
            *record.PreviousPath,
            limits.MaximumPathBytes,
            true);
    }
    return github_read_validation_error::none;
}

[[nodiscard]] inline auto validate_commit(
    const github_commit_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(!hexadecimal_oid(record.Oid.value()))
    {
        return github_read_validation_error::invalid_commit_oid;
    }
    if(const auto error =
           budget.consume(record.Oid.value(), 64U, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error = consume_id(budget, record.Node.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           validate_author(record.Author, budget, limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        record.Headline,
        limits.MaximumHeadlineBytes);
}

[[nodiscard]] inline auto validate_review(
    const github_review_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(!valid_enum(record.State))
    {
        return github_read_validation_error::invalid_enum;
    }
    if(const auto error = consume_id(budget, record.Review.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           validate_author(record.Author, budget, limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(record.Commit.has_value())
    {
        if(!hexadecimal_oid(record.Commit->value()))
        {
            return github_read_validation_error::invalid_commit_oid;
        }
        if(const auto error =
               budget.consume(record.Commit->value(), 64U, true);
           error != github_read_validation_error::none)
        {
            return error;
        }
    }
    return budget.consume(record.Body, limits.MaximumBodyBytes);
}

[[nodiscard]] inline auto validate_review_comment(
    const github_review_comment_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(
        record.Line.has_value()
        && (
            *record.Line == 0U
            || *record.Line > maximum_github_line))
    {
        return github_read_validation_error::invalid_line;
    }
    if(const auto error = consume_id(budget, record.Comment.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           validate_author(record.Author, budget, limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(record.ReplyTo.has_value())
    {
        if(const auto error =
               consume_id(budget, record.ReplyTo->value(), limits);
           error != github_read_validation_error::none)
        {
            return error;
        }
    }
    if(const auto error =
           budget.consume(record.Body, limits.MaximumBodyBytes);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        record.Path,
        limits.MaximumPathBytes,
        true);
}

[[nodiscard]] inline auto validate_thread(
    const github_review_thread_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(record.Comments.size() > limits.MaximumCommentsPerThread)
    {
        return github_read_validation_error::collection_too_large;
    }
    if(!valid_enum(record.Subject))
    {
        return github_read_validation_error::invalid_enum;
    }
    if(
        (
            record.Subject == github_review_thread_subject::line
            && !record.Line.has_value())
        || (
            record.Subject == github_review_thread_subject::file
            && record.Line.has_value())
        || (
            record.Line.has_value()
            && (
                *record.Line == 0U
                || *record.Line > maximum_github_line)))
    {
        return github_read_validation_error::invalid_line;
    }
    if(const auto error = consume_id(budget, record.Thread.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           budget.consume(record.Path, limits.MaximumPathBytes, true);
       error != github_read_validation_error::none)
    {
        return error;
    }
    for(const auto& comment : record.Comments)
    {
        if(comment.Path != record.Path)
        {
            return github_read_validation_error::invalid_relationship;
        }
        if(
            (
                record.Subject == github_review_thread_subject::line
                && !comment.Line.has_value())
            || (
                record.Subject == github_review_thread_subject::file
                && comment.Line.has_value()))
        {
            return github_read_validation_error::invalid_line;
        }
        if(const auto error =
               validate_review_comment(comment, budget, limits);
           error != github_read_validation_error::none)
        {
            return error;
        }
    }
    return github_read_validation_error::none;
}

[[nodiscard]] inline auto validate_issue_comment(
    const github_issue_comment_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(const auto error = consume_id(budget, record.Comment.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    if(const auto error =
           validate_author(record.Author, budget, limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(record.Body, limits.MaximumBodyBytes);
}

[[nodiscard]] inline auto validate_check(
    const github_check_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(!valid_enum(record.Status) || !valid_enum(record.Conclusion))
    {
        return github_read_validation_error::invalid_enum;
    }
    if(
        (
            record.Status == github_check_status::completed
            && record.Conclusion == github_check_conclusion::none)
        || (
            record.Status != github_check_status::completed
            && record.Conclusion != github_check_conclusion::none))
    {
        return github_read_validation_error::invalid_enum;
    }
    if(const auto error = consume_id(budget, record.Check.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        record.Name,
        limits.MaximumCheckNameBytes,
        true);
}

[[nodiscard]] inline auto validate_search_result(
    const github_search_result_record& record,
    github_text_budget& budget,
    const github_read_limits& limits) noexcept
    -> github_read_validation_error
{
    if(record.Repository.value() == 0U)
    {
        return github_read_validation_error::missing_identity;
    }
    if(!record.Number.valid())
    {
        return github_read_validation_error::invalid_pull_request_number;
    }
    if(
        !valid_enum(record.State)
        || record.State != github_pull_request_state::open)
    {
        return github_read_validation_error::invalid_enum;
    }
    if(const auto error =
           consume_id(budget, record.PullRequestNode.value(), limits);
       error != github_read_validation_error::none)
    {
        return error;
    }
    return budget.consume(
        record.Title,
        limits.MaximumTitleBytes,
        true);
}

[[nodiscard]] inline auto validate_capabilities(
    const github_capability_evidence& evidence) noexcept
    -> github_read_validation_error
{
    if(evidence.Revision.value() == 0U || evidence.ContractVersion != 1U)
    {
        return github_read_validation_error::invalid_capability_revision;
    }
    if(
        !valid_enum(evidence.Capabilities.ReviewThreads)
        || !valid_enum(evidence.Capabilities.ViewedFileMutations)
        || !valid_enum(evidence.Capabilities.ResolveThread)
        || !valid_enum(evidence.Capabilities.UnresolveThread))
    {
        return github_read_validation_error::invalid_enum;
    }
    return github_read_validation_error::none;
}

[[nodiscard]] inline auto validate_rate_limit(
    const github_rate_limit_record& rate_limit) noexcept
    -> github_read_validation_error
{
    if(rate_limit.Remaining > rate_limit.Limit)
    {
        return github_read_validation_error::invalid_rate_limit;
    }
    return github_read_validation_error::none;
}

[[nodiscard]] inline auto validate_unique_identities(
    const github_pull_request_snapshot& snapshot)
    -> github_read_validation_error
{
    std::unordered_map<std::string_view, std::string_view> actors;
    actors.reserve(
        snapshot.Commits.size() + snapshot.Reviews.size()
        + snapshot.ReviewThreads.size() + snapshot.IssueComments.size()
        + 2U);
    actors.emplace(
        snapshot.Viewer.Actor.value(),
        snapshot.Viewer.Login);
    const auto observe_actor =
        [&actors](
            const std::optional<github_actor_record>& author)
        -> github_read_validation_error {
        if(!author.has_value())
        {
            return github_read_validation_error::none;
        }
        const auto [iterator, inserted] = actors.try_emplace(
            author->Actor.value(),
            author->Login);
        if(!inserted && iterator->second != author->Login)
        {
            return github_read_validation_error::invalid_relationship;
        }
        return github_read_validation_error::none;
    };
    if(const auto error = observe_actor(snapshot.PullRequest.Author);
       error != github_read_validation_error::none)
    {
        return error;
    }
    for(const auto& commit : snapshot.Commits)
    {
        if(const auto error = observe_actor(commit.Author);
           error != github_read_validation_error::none)
        {
            return error;
        }
    }
    for(const auto& review : snapshot.Reviews)
    {
        if(const auto error = observe_actor(review.Author);
           error != github_read_validation_error::none)
        {
            return error;
        }
    }
    for(const auto& thread : snapshot.ReviewThreads)
    {
        for(const auto& comment : thread.Comments)
        {
            if(const auto error = observe_actor(comment.Author);
               error != github_read_validation_error::none)
            {
                return error;
            }
        }
    }
    for(const auto& comment : snapshot.IssueComments)
    {
        if(const auto error = observe_actor(comment.Author);
           error != github_read_validation_error::none)
        {
            return error;
        }
    }

    std::unordered_set<std::string_view> entity_node_ids;
    entity_node_ids.reserve(
        snapshot.Commits.size() + snapshot.Reviews.size()
        + snapshot.ReviewThreads.size() + snapshot.IssueComments.size()
        + snapshot.Checks.size() + 2U);
    if(!entity_node_ids.emplace(snapshot.Repository.Node.value()).second
       || !entity_node_ids.emplace(snapshot.PullRequest.Node.value()).second)
    {
        return github_read_validation_error::duplicate_identity;
    }
    for(const auto& commit : snapshot.Commits)
    {
        if(!entity_node_ids.emplace(commit.Node.value()).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }

    std::unordered_set<std::string_view> paths;
    paths.reserve(snapshot.Files.size());
    for(const auto& file : snapshot.Files)
    {
        if(!paths.emplace(file.Path).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }

    std::unordered_set<std::string_view> commit_oids;
    commit_oids.reserve(snapshot.Commits.size());
    for(const auto& commit : snapshot.Commits)
    {
        if(!commit_oids.emplace(commit.Oid.value()).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }

    for(const auto& review : snapshot.Reviews)
    {
        if(!entity_node_ids.emplace(review.Review.value()).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }

    for(const auto& thread : snapshot.ReviewThreads)
    {
        if(!entity_node_ids.emplace(thread.Thread.value()).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
        std::unordered_set<std::string_view> thread_comment_ids;
        thread_comment_ids.reserve(thread.Comments.size());
        for(const auto& comment : thread.Comments)
        {
            if(
                comment.ReplyTo.has_value()
                && !thread_comment_ids.contains(
                    comment.ReplyTo->value()))
            {
                return github_read_validation_error::
                    invalid_relationship;
            }
            if(!entity_node_ids.emplace(comment.Comment.value()).second)
            {
                return github_read_validation_error::duplicate_identity;
            }
            thread_comment_ids.emplace(comment.Comment.value());
        }
    }
    for(const auto& comment : snapshot.IssueComments)
    {
        if(!entity_node_ids.emplace(comment.Comment.value()).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }

    for(const auto& check : snapshot.Checks)
    {
        if(!entity_node_ids.emplace(check.Check.value()).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }
    for(const auto& actor : actors)
    {
        if(!entity_node_ids.emplace(actor.first).second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }

    std::unordered_set<std::string_view> search_pull_requests;
    std::unordered_set<std::uint32_t> search_pull_request_numbers;
    search_pull_requests.reserve(snapshot.SearchResults.size());
    search_pull_request_numbers.reserve(snapshot.SearchResults.size());
    for(const auto& result : snapshot.SearchResults)
    {
        if(result.Repository != snapshot.Repository.Repository)
        {
            return github_read_validation_error::invalid_relationship;
        }
        const auto matches_snapshot_node =
            result.PullRequestNode == snapshot.PullRequest.Node;
        const auto matches_snapshot_number =
            result.Number == snapshot.PullRequest.Number;
        if(matches_snapshot_node != matches_snapshot_number)
        {
            return github_read_validation_error::invalid_relationship;
        }
        if(
            matches_snapshot_node
            && (
                result.Title != snapshot.PullRequest.Title
                || result.State != snapshot.PullRequest.State
                || result.Draft != snapshot.PullRequest.Draft))
        {
            return github_read_validation_error::invalid_relationship;
        }
        if(
            !matches_snapshot_node
            && !entity_node_ids
                    .emplace(result.PullRequestNode.value())
                    .second)
        {
            return github_read_validation_error::duplicate_identity;
        }
        if(
            !search_pull_requests
                    .emplace(result.PullRequestNode.value())
                    .second
            || !search_pull_request_numbers
                    .emplace(result.Number.value())
                    .second)
        {
            return github_read_validation_error::duplicate_identity;
        }
    }
    return github_read_validation_error::none;
}
}

[[nodiscard]] inline auto validate(
    const github_session_read_model& model,
    const core::github_session_request_context& context,
    const github_read_limits& limits = {}) noexcept
    -> github_read_validation
{
    if(!detail::within_security_ceiling(limits))
    {
        return detail::validation(
            github_read_validation_error::invalid_limits);
    }
    if(!detail::valid_context(context))
    {
        return detail::validation(
            github_read_validation_error::invalid_context);
    }
    if(
        model.Profile.value() == 0U || model.Host.value() == 0U
        || model.Viewer.Account.value() == 0U)
    {
        return detail::validation(
            github_read_validation_error::missing_identity);
    }
    if(model.Profile != context.Profile)
    {
        return detail::validation(
            github_read_validation_error::identity_mismatch);
    }

    detail::github_text_budget budget{limits};
    if(const auto error =
           detail::validate_viewer(model.Viewer, budget, limits);
       error != github_read_validation_error::none)
    {
        return detail::validation(error);
    }
    if(const auto error =
           detail::validate_capabilities(model.CapabilityEvidence);
       error != github_read_validation_error::none)
    {
        return detail::validation(error);
    }
    return detail::validation(
        detail::validate_rate_limit(model.RateLimit));
}

[[nodiscard]] inline auto validate(
    const github_pull_request_snapshot& snapshot,
    const core::pull_request_request_context& context,
    const github_read_limits& limits = {})
    -> github_read_validation
{
    if(!detail::within_security_ceiling(limits))
    {
        return detail::validation(
            github_read_validation_error::invalid_limits);
    }
    if(!detail::valid_context(context))
    {
        return detail::validation(
            github_read_validation_error::invalid_context);
    }
    if(
        snapshot.Profile.value() == 0U || snapshot.Host.value() == 0U
        || snapshot.Account.value() == 0U
        || snapshot.Snapshot.value() == 0U)
    {
        return detail::validation(
            github_read_validation_error::missing_identity);
    }
    if(
        snapshot.Profile != context.Profile
        || snapshot.Host != context.Host
        || snapshot.Account != context.Account
        || snapshot.PullRequest.Number != context.Number
        || snapshot.PullRequest.Repository
            != snapshot.Repository.Repository
        || !detail::ascii_equal_fold(
            snapshot.Repository.Owner,
            context.Repository.Owner)
        || !detail::ascii_equal_fold(
            snapshot.Repository.Name,
            context.Repository.Name)
        || (
            context.ExpectedRepository.has_value()
            && *context.ExpectedRepository
                != snapshot.Repository.Repository)
        || (
            context.ExpectedPullRequest.has_value()
            && *context.ExpectedPullRequest
                != snapshot.PullRequest.PullRequest)
        || snapshot.CapabilityEvidence.Revision
            != context.Capabilities)
    {
        return detail::validation(
            github_read_validation_error::identity_mismatch);
    }
    if(snapshot.Files.size() > limits.MaximumFiles
       || snapshot.Commits.size() > limits.MaximumCommits
       || snapshot.Reviews.size() > limits.MaximumReviews
       || snapshot.ReviewThreads.size() > limits.MaximumReviewThreads
       || snapshot.IssueComments.size() > limits.MaximumIssueComments
       || snapshot.Checks.size() > limits.MaximumChecks
       || snapshot.SearchResults.size() > limits.MaximumSearchResults)
    {
        return detail::validation(
            github_read_validation_error::collection_too_large);
    }

    auto total_comments = snapshot.IssueComments.size();
    if(total_comments > limits.MaximumTotalComments)
    {
        return detail::validation(
            github_read_validation_error::collection_too_large);
    }
    for(const auto& thread : snapshot.ReviewThreads)
    {
        if(
            thread.Comments.size()
                > limits.MaximumTotalComments - total_comments)
        {
            return detail::validation(
                github_read_validation_error::collection_too_large);
        }
        total_comments += thread.Comments.size();
    }

    auto total_records = snapshot.Files.size();
    const auto add_records = [&](const std::size_t count) {
        if(
            count > limits.MaximumTotalRecords
            || total_records > limits.MaximumTotalRecords - count)
        {
            return false;
        }
        total_records += count;
        return true;
    };
    if(
        !add_records(snapshot.Commits.size())
        || !add_records(snapshot.Reviews.size())
        || !add_records(snapshot.ReviewThreads.size())
        || !add_records(total_comments)
        || !add_records(snapshot.Checks.size())
        || !add_records(snapshot.SearchResults.size()))
    {
        return detail::validation(
            github_read_validation_error::collection_too_large);
    }

    detail::github_text_budget budget{limits};
    if(const auto error =
           detail::validate_actor(
               snapshot.Viewer,
               budget,
               limits,
               true);
       error != github_read_validation_error::none)
    {
        return detail::validation(error);
    }
    if(const auto error =
           detail::validate_repository(
               snapshot.Repository,
               budget,
               limits);
       error != github_read_validation_error::none)
    {
        return detail::validation(error);
    }
    if(const auto error =
           detail::validate_pull_request(
               snapshot.PullRequest,
               budget,
               limits);
       error != github_read_validation_error::none)
    {
        return detail::validation(error);
    }
    for(const auto& record : snapshot.Files)
    {
        if(const auto error =
               detail::validate_file(record, budget, limits);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    for(const auto& record : snapshot.Commits)
    {
        if(const auto error =
               detail::validate_commit(record, budget, limits);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    for(const auto& record : snapshot.Reviews)
    {
        if(const auto error =
               detail::validate_review(record, budget, limits);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    for(const auto& record : snapshot.ReviewThreads)
    {
        if(const auto error =
               detail::validate_thread(record, budget, limits);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    for(const auto& record : snapshot.IssueComments)
    {
        if(const auto error =
               detail::validate_issue_comment(record, budget, limits);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    for(const auto& record : snapshot.Checks)
    {
        if(const auto error =
               detail::validate_check(record, budget, limits);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    for(const auto& record : snapshot.SearchResults)
    {
        if(const auto error =
               detail::validate_search_result(record, budget, limits);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    try
    {
        if(const auto error =
               detail::validate_unique_identities(snapshot);
           error != github_read_validation_error::none)
        {
            return detail::validation(error);
        }
    }
    catch(const std::bad_alloc&)
    {
        return detail::validation(
            github_read_validation_error::allocation_limit);
    }
    if(const auto error =
           detail::validate_capabilities(snapshot.CapabilityEvidence);
       error != github_read_validation_error::none)
    {
        return detail::validation(error);
    }
    return detail::validation(
        detail::validate_rate_limit(snapshot.RateLimit));
}
}
