#include "github_projection_mapping.hpp"

#include <string_view>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto project(
			const github_rate_limit_resource aResource) noexcept
			-> github_rate_limit_resource_projection
		{
			switch (aResource) {
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

		[[nodiscard]] auto project(const github_rate_limit_record& aRecord) noexcept
			-> github_rate_limit_projection
		{
			return {
				.Resource         = project(aRecord.Resource),
				.Limit            = aRecord.Limit,
				.Remaining        = aRecord.Remaining,
				.ResetUnixSeconds = aRecord.ResetUnixSeconds,
				.SecondaryLimited = aRecord.SecondaryLimited,
			};
		}

		[[nodiscard]] auto project(
			const std::optional<github_rate_limit_record>& aRecord) noexcept
			-> std::optional<github_rate_limit_projection>
		{
			if (!aRecord.has_value()) {
				return std::nullopt;
			}
			return project(*aRecord);
		}

		[[nodiscard]] auto project(const github_rate_limit_state& aLimits) noexcept
			-> github_rate_limits_projection
		{
			return {
				.Core    = project(aLimits.Core),
				.Search  = project(aLimits.Search),
				.Graphql = project(aLimits.Graphql),
			};
		}

		[[nodiscard]] auto project(
			const github_pull_request_state aState) noexcept
			-> github_pull_request_state_projection
		{
			switch (aState) {
			case github_pull_request_state::closed:
				return github_pull_request_state_projection::closed;
			case github_pull_request_state::merged:
				return github_pull_request_state_projection::merged;
			case github_pull_request_state::open:
				return github_pull_request_state_projection::open;
			}
			return github_pull_request_state_projection::open;
		}

		[[nodiscard]] auto project(const github_review_state aState) noexcept
			-> github_review_state_projection
		{
			switch (aState) {
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
			const std::string_view aPrefix,
			const std::string_view aIdentity) -> std::string
		{
			auto result = std::string{ aPrefix };
			result.append(aIdentity);
			return result;
		}

		[[nodiscard]] auto author_login(
			const std::optional<github_actor_record>& aAuthor) -> std::string
		{
			return aAuthor.has_value() ? aAuthor->Login : std::string{ };
		}
	}

	auto project_github_transition(const core::github_transition_output& aOutput)
		-> github_transition_projection
	{
		return {
			.Changed      = aOutput.Changed,
			.Commands     = aOutput.Commands,
			.Status       = aOutput.Status,
			.Identity     = aOutput.Identity,
			.Capabilities = aOutput.Capabilities,
			.PullRequest  = aOutput.PullRequest,
			.Diagnostics  = aOutput.Diagnostics,
		};
	}

	auto project_github_session(const github_session_read_model& aSession)
		-> github_session_projection
	{
		return {
			.Profile            = aSession.Profile,
			.Host               = aSession.Host,
			.Account            = aSession.Viewer.Account,
			.Login              = aSession.Viewer.Login,
			.DisplayName        = aSession.Viewer.DisplayName,
			.Capabilities       = aSession.CapabilityEvidence.Capabilities,
			.CapabilityRevision = aSession.CapabilityEvidence.Revision,
			.RateLimits         = project(aSession.RateLimits),
		};
	}

	auto project_github_pull_request(
		const github_pull_request_snapshot& aSnapshot)
		-> github_pull_request_projection
	{
		std::vector<github_conversation_projection> conversations;
		conversations.reserve(
			aSnapshot.Reviews.size() + aSnapshot.ReviewThreads.size()
			+ aSnapshot.IssueComments.size());

		for (const auto& review : aSnapshot.Reviews) {
			conversations.push_back(github_conversation_projection{
				.Id          = prefixed_id("review:", review.Review.value()),
				.ParentId    = std::nullopt,
				.Kind        = github_conversation_kind::review,
				.AuthorLogin = author_login(review.Author),
				.Body        = review.Body,
				.Path        = { },
				.Line        = std::nullopt,
				.ReviewState = project(review.State),
			});
		}

		for (const auto& thread : aSnapshot.ReviewThreads) {
			const auto threadId = prefixed_id("review-thread:", thread.Thread.value());
			conversations.push_back(github_conversation_projection{
				.Id          = threadId,
				.ParentId    = std::nullopt,
				.Kind        = github_conversation_kind::review_thread,
				.AuthorLogin = { },
				.Body        = { },
				.Path        = thread.Path,
				.Line        = thread.Line,
				.ReviewState = github_review_state_projection::none,
				.Resolved    = thread.Resolved,
				.Outdated    = thread.Outdated,
			});
			for (const auto& comment : thread.Comments) {
				conversations.push_back(github_conversation_projection{
					.Id          = prefixed_id("review-comment:", comment.Comment.value()),
					.ParentId    = threadId,
					.Kind        = github_conversation_kind::review_comment,
					.AuthorLogin = author_login(comment.Author),
					.Body        = comment.Body,
					.Path        = comment.Path,
					.Line        = comment.Line,
					.ReviewState = github_review_state_projection::none,
					.Resolved    = thread.Resolved,
					.Outdated    = comment.Outdated || thread.Outdated,
					.Deleted     = comment.Deleted,
				});
			}
		}

		for (const auto& comment : aSnapshot.IssueComments) {
			conversations.push_back(github_conversation_projection{
				.Id          = prefixed_id("issue-comment:", comment.Comment.value()),
				.ParentId    = std::nullopt,
				.Kind        = github_conversation_kind::issue_comment,
				.AuthorLogin = author_login(comment.Author),
				.Body        = comment.Body,
				.Path        = { },
				.Line        = std::nullopt,
				.ReviewState = github_review_state_projection::none,
				.Deleted     = comment.Deleted,
			});
		}

		return {
			.Profile                = aSnapshot.Profile,
			.Host                   = aSnapshot.Host,
			.Account                = aSnapshot.Account,
			.Snapshot               = aSnapshot.Snapshot,
			.Repository             = aSnapshot.Repository.Repository,
			.PullRequest            = aSnapshot.PullRequest.PullRequest,
			.Owner                  = aSnapshot.Repository.Owner,
			.RepositoryName         = aSnapshot.Repository.Name,
			.Number                 = aSnapshot.PullRequest.Number,
			.Title                  = aSnapshot.PullRequest.Title,
			.Body                   = aSnapshot.PullRequest.Body,
			.State                  = project(aSnapshot.PullRequest.State),
			.Draft                  = aSnapshot.PullRequest.Draft,
			.AuthorLogin            = author_login(aSnapshot.PullRequest.Author),
			.BaseRef                = aSnapshot.PullRequest.BaseRef,
			.HeadRef                = aSnapshot.PullRequest.HeadRef,
			.BaseOid                = std::string{ aSnapshot.PullRequest.BaseOid.value() },
			.HeadOid                = std::string{ aSnapshot.PullRequest.HeadOid.value() },
			.FileCount              = aSnapshot.Files.size(),
			.CommitCount            = aSnapshot.Commits.size(),
			.ReviewCount            = aSnapshot.Reviews.size(),
			.ReviewThreadCount      = aSnapshot.ReviewThreads.size(),
			.IssueCommentCount      = aSnapshot.IssueComments.size(),
			.ReviewThreadsAvailable = aSnapshot.ReviewThreadsAvailability
				== github_collection_availability::available,
			.Capabilities       = aSnapshot.CapabilityEvidence.Capabilities,
			.CapabilityRevision = aSnapshot.CapabilityEvidence.Revision,
			.RateLimits         = project(aSnapshot.RateLimits),
			.Conversations      = std::move(conversations),
		};
	}

	auto project_github_host_diagnostic(
		const github_host_diagnostic& aDiagnostic)
		-> github_host_diagnostic_projection
	{
		return {
			.Reason            = aDiagnostic.Reason,
			.RetryAfterSeconds = aDiagnostic.RetryAfterSeconds,
			.RateLimits        = project(aDiagnostic.RateLimits),
			.RateLimitCause    = project(aDiagnostic.RateLimitCause),
		};
	}
}
