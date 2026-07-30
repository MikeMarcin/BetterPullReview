#include "github_projection_mapping.hpp"

#include <string_view>
#include <utility>

namespace bpr::application
{
namespace
{
[[nodiscard]] auto project(
    const github_rate_limit_resource resource) noexcept
    -> github_rate_limit_resource_projection
{
    switch(resource)
    {
    case github_rate_limit_resource::search:
        return github_rate_limit_resource_projection::search;
    case github_rate_limit_resource::graphql:
        return github_rate_limit_resource_projection::graphql;
    case github_rate_limit_resource::core:
    case github_rate_limit_resource::uninitialized:
        return github_rate_limit_resource_projection::core;
    }
    return github_rate_limit_resource_projection::core;
}

[[nodiscard]] auto project(const github_rate_limit_record& record) noexcept
    -> github_rate_limit_projection
{
    return {
        .Resource = project(record.Resource),
        .Limit = record.Limit,
        .Remaining = record.Remaining,
        .ResetUnixSeconds = record.ResetUnixSeconds,
        .SecondaryLimited = record.SecondaryLimited,
    };
}

[[nodiscard]] auto project(
    const std::optional<github_rate_limit_record>& record) noexcept
    -> std::optional<github_rate_limit_projection>
{
    if(!record.has_value())
    {
        return std::nullopt;
    }
    return project(*record);
}

[[nodiscard]] auto project(const github_rate_limit_state& limits) noexcept
    -> github_rate_limits_projection
{
    return {
        .Core = project(limits.Core),
        .Search = project(limits.Search),
        .Graphql = project(limits.Graphql),
    };
}

[[nodiscard]] auto project(
    const github_pull_request_state state) noexcept
    -> github_pull_request_state_projection
{
    switch(state)
    {
    case github_pull_request_state::closed:
        return github_pull_request_state_projection::closed;
    case github_pull_request_state::merged:
        return github_pull_request_state_projection::merged;
    case github_pull_request_state::open:
        return github_pull_request_state_projection::open;
    }
    return github_pull_request_state_projection::open;
}

[[nodiscard]] auto project(const github_review_state state) noexcept
    -> github_review_state_projection
{
    switch(state)
    {
    case github_review_state::pending:
        return github_review_state_projection::pending;
    case github_review_state::commented:
        return github_review_state_projection::commented;
    case github_review_state::approved:
        return github_review_state_projection::approved;
    case github_review_state::changes_requested:
        return github_review_state_projection::changes_requested;
    case github_review_state::dismissed:
        return github_review_state_projection::dismissed;
    case github_review_state::uninitialized:
        return github_review_state_projection::none;
    }
    return github_review_state_projection::none;
}

[[nodiscard]] auto prefixed_id(
    const std::string_view prefix,
    const std::string_view identity) -> std::string
{
    auto result = std::string{prefix};
    result.append(identity);
    return result;
}

[[nodiscard]] auto author_login(
    const std::optional<github_actor_record>& author) -> std::string
{
    return author.has_value() ? author->Login : std::string{};
}
}

auto project_github_transition(const core::github_transition_output& output)
    -> github_transition_projection
{
    return {
        .Changed = output.Changed,
        .Commands = output.Commands,
        .Status = output.Status,
        .Identity = output.Identity,
        .Capabilities = output.Capabilities,
        .PullRequest = output.PullRequest,
        .Diagnostics = output.Diagnostics,
    };
}

auto project_github_session(const github_session_read_model& session)
    -> github_session_projection
{
    return {
        .Profile = session.Profile,
        .Host = session.Host,
        .Account = session.Viewer.Account,
        .Login = session.Viewer.Login,
        .DisplayName = session.Viewer.DisplayName,
        .Capabilities = session.CapabilityEvidence.Capabilities,
        .CapabilityRevision = session.CapabilityEvidence.Revision,
        .RateLimits = project(session.RateLimits),
    };
}

auto project_github_pull_request(
    const github_pull_request_snapshot& snapshot)
    -> github_pull_request_projection
{
    std::vector<github_conversation_projection> conversations;
    conversations.reserve(
        snapshot.Reviews.size() + snapshot.ReviewThreads.size()
        + snapshot.IssueComments.size());

    for(const auto& review : snapshot.Reviews)
    {
        conversations.push_back(github_conversation_projection{
            .Id = prefixed_id("review:", review.Review.value()),
            .ParentId = std::nullopt,
            .Kind = github_conversation_kind::review,
            .AuthorLogin = author_login(review.Author),
            .Body = review.Body,
            .Path = {},
            .Line = std::nullopt,
            .ReviewState = project(review.State),
        });
    }

    for(const auto& thread : snapshot.ReviewThreads)
    {
        const auto thread_id =
            prefixed_id("review-thread:", thread.Thread.value());
        conversations.push_back(github_conversation_projection{
            .Id = thread_id,
            .ParentId = std::nullopt,
            .Kind = github_conversation_kind::review_thread,
            .AuthorLogin = {},
            .Body = {},
            .Path = thread.Path,
            .Line = thread.Line,
            .ReviewState = github_review_state_projection::none,
            .Resolved = thread.Resolved,
            .Outdated = thread.Outdated,
        });
        for(const auto& comment : thread.Comments)
        {
            conversations.push_back(github_conversation_projection{
                .Id =
                    prefixed_id("review-comment:", comment.Comment.value()),
                .ParentId = thread_id,
                .Kind = github_conversation_kind::review_comment,
                .AuthorLogin = author_login(comment.Author),
                .Body = comment.Body,
                .Path = comment.Path,
                .Line = comment.Line,
                .ReviewState = github_review_state_projection::none,
                .Resolved = thread.Resolved,
                .Outdated = comment.Outdated || thread.Outdated,
                .Deleted = comment.Deleted,
            });
        }
    }

    for(const auto& comment : snapshot.IssueComments)
    {
        conversations.push_back(github_conversation_projection{
            .Id = prefixed_id("issue-comment:", comment.Comment.value()),
            .ParentId = std::nullopt,
            .Kind = github_conversation_kind::issue_comment,
            .AuthorLogin = author_login(comment.Author),
            .Body = comment.Body,
            .Path = {},
            .Line = std::nullopt,
            .ReviewState = github_review_state_projection::none,
            .Deleted = comment.Deleted,
        });
    }

    return {
        .Profile = snapshot.Profile,
        .Host = snapshot.Host,
        .Account = snapshot.Account,
        .Snapshot = snapshot.Snapshot,
        .Repository = snapshot.Repository.Repository,
        .PullRequest = snapshot.PullRequest.PullRequest,
        .Owner = snapshot.Repository.Owner,
        .RepositoryName = snapshot.Repository.Name,
        .Number = snapshot.PullRequest.Number,
        .Title = snapshot.PullRequest.Title,
        .Body = snapshot.PullRequest.Body,
        .State = project(snapshot.PullRequest.State),
        .Draft = snapshot.PullRequest.Draft,
        .AuthorLogin = author_login(snapshot.PullRequest.Author),
        .BaseRef = snapshot.PullRequest.BaseRef,
        .HeadRef = snapshot.PullRequest.HeadRef,
        .BaseOid = std::string{snapshot.PullRequest.BaseOid.value()},
        .HeadOid = std::string{snapshot.PullRequest.HeadOid.value()},
        .FileCount = snapshot.Files.size(),
        .CommitCount = snapshot.Commits.size(),
        .ReviewCount = snapshot.Reviews.size(),
        .ReviewThreadCount = snapshot.ReviewThreads.size(),
        .IssueCommentCount = snapshot.IssueComments.size(),
        .ReviewThreadsAvailable =
            snapshot.ReviewThreadsAvailability
            == github_collection_availability::available,
        .Capabilities = snapshot.CapabilityEvidence.Capabilities,
        .CapabilityRevision = snapshot.CapabilityEvidence.Revision,
        .RateLimits = project(snapshot.RateLimits),
        .Conversations = std::move(conversations),
    };
}

auto project_github_host_diagnostic(
    const github_host_diagnostic& diagnostic)
    -> github_host_diagnostic_projection
{
    return {
        .Reason = diagnostic.Reason,
        .RetryAfterSeconds = diagnostic.RetryAfterSeconds,
        .RateLimits = project(diagnostic.RateLimits),
        .RateLimitCause = project(diagnostic.RateLimitCause),
    };
}
}
