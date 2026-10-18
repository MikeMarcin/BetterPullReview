#include "github_projection_mapping.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/pull_request_link_location.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
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

		[[nodiscard]] auto project(const github_diff_side aSide) noexcept
			-> std::optional<github_conversation_diff_side>
		{
			switch (aSide) {
			case github_diff_side::left:
				return github_conversation_diff_side::left;
			case github_diff_side::right:
				return github_conversation_diff_side::right;
			case github_diff_side::uninitialized:
				return std::nullopt;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto anchor_availability(
			const github_review_thread_record& aThread) noexcept
			-> github_conversation_anchor_availability
		{
			if (aThread.Subject == github_review_thread_subject::file) {
				return github_conversation_anchor_availability::file_level;
			}
			if (aThread.DiffSide == github_diff_side::uninitialized) {
				return github_conversation_anchor_availability::missing_side;
			}
			if (aThread.Outdated) {
				return github_conversation_anchor_availability::outdated_revision;
			}
			return aThread.Line.has_value()
				? github_conversation_anchor_availability::current
				: github_conversation_anchor_availability::missing_current_line;
		}

		using review_comments_by_id
			= std::unordered_map<std::string_view, const github_review_comment_record*>;

		/// \brief Resolves the parent identity of one review comment.
		///
		/// A reply's parent is the comment it replied to, so that the projected
		/// forest carries the shape GitHub reported rather than one flattened
		/// against the thread. Three inputs cannot produce that shape and each
		/// falls back to the thread root, because a projection that drops a
		/// comment is worse than one that under-nests it:
		///
		/// - no `ReplyTo`, which is what a thread's first comment carries;
		/// - a `ReplyTo` naming a comment outside this thread, which happens
		///   when the target was deleted or fell outside the fetched page;
		/// - a chain that never reaches a comment without a `ReplyTo`, which a
		///   cycle produces and which would otherwise leave every comment in
		///   the cycle descending from no root at all.
		[[nodiscard]] auto review_comment_parent(
			const github_review_comment_record& aComment,
			const review_comments_by_id&        aCommentsById,
			const std::string_view              aThreadId) -> std::string
		{
			auto threadRoot = std::string{ aThreadId };
			if (!aComment.ReplyTo.has_value()) {
				return threadRoot;
			}
			const auto target = aCommentsById.find(aComment.ReplyTo->value());
			if (target == aCommentsById.end()) {
				return threadRoot;
			}

			// Walk upward from the target. Every comment in the thread can be
			// visited at most once before a chain must have terminated, so a
			// walk that outlives that bound is in a cycle.
			const auto* walk = target->second;
			for (auto remaining = aCommentsById.size(); remaining != 0U; --remaining) {
				if (!walk->ReplyTo.has_value()) {
					return prefixed_id("review-comment:", aComment.ReplyTo->value());
				}
				const auto next = aCommentsById.find(walk->ReplyTo->value());
				if (next == aCommentsById.end()) {
					// The chain leaves the thread, so the comment it leaves
					// through takes the thread root and this one still descends
					// from it.
					return prefixed_id("review-comment:", aComment.ReplyTo->value());
				}
				walk = next->second;
			}
			return threadRoot;
		}
	}

	auto project_github_loading_overview(const github_pull_request_overview_observation& aObservation)
		-> std::optional<github_loading_overview_projection>
	{
		const auto& context    = aObservation.Context;
		const auto& repository = aObservation.Repository;
		const auto& pr         = aObservation.PullRequest;
		const auto  limits     = github_read_limits{ };
		auto        budget     = detail::github_text_budget{ limits };
		if (!detail::valid_context(context)
			|| detail::validate_repository(repository, budget, limits) != github_read_validation_error::none
			|| detail::validate_pull_request(pr, budget, limits) != github_read_validation_error::none
			|| pr.Number != context.Number || pr.Repository != repository.Repository
			|| !detail::ascii_equal_fold(repository.Owner, context.Repository.Owner)
			|| !detail::ascii_equal_fold(repository.Name, context.Repository.Name)
			|| (context.ExpectedRepository && *context.ExpectedRepository != repository.Repository)
			|| (context.ExpectedPullRequest && *context.ExpectedPullRequest != pr.PullRequest)
			|| !pr.ReportedCommitCount || *pr.ReportedCommitCount == 0U
			|| aObservation.ReportedFiles > limits.MaximumFiles) {
			return std::nullopt;
		}
		return github_loading_overview_projection{
			.Title           = pr.Title,
			.Body            = pr.Body,
			.AuthorLogin     = pr.Author ? pr.Author->Login : std::string{ },
			.BaseRef         = pr.BaseRef,
			.HeadRef         = pr.HeadRef,
			.ReportedFiles   = aObservation.ReportedFiles,
			.ReportedCommits = *pr.ReportedCommitCount,
			.State           = project(pr.State),
			.Draft           = pr.Draft,
		};
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
			.Refusal      = aOutput.Refusal,
			.Read         = aOutput.Read,
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

	auto project_github_session(
		const github_session_read_model&       aSession,
		std::optional<github_grant_projection> aGrant) -> github_session_projection
	{
		auto projection  = project_github_session(aSession);
		projection.Grant = std::move(aGrant);
		return projection;
	}

	auto pull_request_link_location_facts_of(const github_pull_request_snapshot& aSnapshot)
		-> pull_request_link_location_facts
	{
		// [[BPR-0987]]. The conversation identities are the ones
		// `project_github_pull_request` gives the same records below, so a
		// resolved location reveals exactly the row the window shows.
		auto facts = pull_request_link_location_facts{ };
		facts.ChangedPaths.reserve(aSnapshot.Files.size());
		for (const auto& file : aSnapshot.Files) {
			facts.ChangedPaths.push_back(file.Path);
		}
		for (const auto& review : aSnapshot.Reviews) {
			if (review.DatabaseId != 0U) {
				facts.Reviews.push_back({ .Identity = review.DatabaseId, .Conversation = prefixed_id("review:", review.Review.value()) });
			}
		}
		const auto rest = [&facts](const github_review_comment_record& aComment) {
			const auto text     = aComment.Comment.value();
			auto       identity = std::uint64_t{ };
			const auto parsed   = std::from_chars(text.data(), text.data() + text.size(), identity);
			if (!text.empty() && parsed.ec == std::errc{ } && parsed.ptr == text.data() + text.size() && identity != 0U) {
				facts.ReviewComments.push_back({ .Identity = identity, .Conversation = prefixed_id("public-preview-comment:", text) });
			}
		};
		for (const auto& group : aSnapshot.ReviewCommentGroups) {
			rest(group.Root);
			std::ranges::for_each(group.Replies, rest);
		}
		return facts;
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
			const auto threadId     = prefixed_id("review-thread:", thread.Thread.value());
			const auto side         = project(thread.DiffSide);
			const auto availability = anchor_availability(thread);
			// [[BPR-1564]]. The thread's lines are its first comment's.
			const auto startLine = thread.Comments.empty() ? std::nullopt : thread.Comments.front().Observation.StartLine;
			conversations.push_back(github_conversation_projection{
				.Id                 = threadId,
				.ParentId           = std::nullopt,
				.Kind               = github_conversation_kind::review_thread,
				.AuthorLogin        = { },
				.Body               = { },
				.Path               = thread.Path,
				.Line               = thread.Line,
				.OriginalLine       = thread.OriginalLine,
				.StartLine          = startLine,
				.DiffSide           = side,
				.AnchorAvailability = availability,
				.ReviewState        = github_review_state_projection::none,
				.Resolved           = thread.Resolved,
				.Outdated           = thread.Outdated,
				.RemoteSubject      = thread.Subject == github_review_thread_subject::file ? github_conversation_remote_subject::file : github_conversation_remote_subject::line,
			});
			auto commentsById = review_comments_by_id{ };
			commentsById.reserve(thread.Comments.size());
			for (const auto& comment : thread.Comments) {
				commentsById.try_emplace(comment.Comment.value(), &comment);
			}

			for (const auto& comment : thread.Comments) {
				conversations.push_back(github_conversation_projection{
					.Id = prefixed_id("review-comment:", comment.Comment.value()),
					.ParentId
					= review_comment_parent(comment, commentsById, threadId),
					.Kind               = github_conversation_kind::review_comment,
					.AuthorLogin        = author_login(comment.Author),
					.Body               = comment.Body,
					.Path               = thread.Path,
					.Line               = thread.Line,
					.OriginalLine       = thread.OriginalLine,
					.StartLine          = startLine,
					.DiffSide           = side,
					.AnchorAvailability = availability,
					.ReviewState        = github_review_state_projection::none,
					.Resolved           = thread.Resolved,
					.Outdated           = comment.Outdated || thread.Outdated,
					.Deleted            = comment.Deleted,
					.RemoteSubject      = thread.Subject == github_review_thread_subject::file ? github_conversation_remote_subject::file : github_conversation_remote_subject::line,
				});
			}
		}

		for (const auto& group : aSnapshot.ReviewCommentGroups) {
			const auto rootId = prefixed_id("public-preview-comment:", group.Root.Comment.value());
			const auto append = [&](const github_review_comment_record& aComment, std::optional<std::string> aParent) {
				conversations.push_back(github_conversation_projection{
					.Id                 = prefixed_id("public-preview-comment:", aComment.Comment.value()),
					.ParentId           = std::move(aParent),
					.Kind               = github_conversation_kind::review_comment,
					.AuthorLogin        = author_login(aComment.Author),
					.Body               = aComment.Body,
					.Path               = aComment.Path,
					.Line               = aComment.Line,
					.OriginalLine       = aComment.OriginalLine,
					.AnchorAvailability = github_conversation_anchor_availability::file_level,
					.Outdated           = aComment.Outdated,
					.Deleted            = aComment.Deleted,
				});
			};
			append(group.Root, std::nullopt);
			for (const auto& reply : group.Replies) {
				append(reply, rootId);
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
			.CommitCount            = aSnapshot.PullRequest.ReportedCommitCount.value_or(aSnapshot.Commits.size()),
			.ReturnedCommitCount    = aSnapshot.Commits.size(),
			.ReviewCount            = aSnapshot.Reviews.size(),
			.ReviewThreadCount      = aSnapshot.ReviewThreads.size(),
			.IssueCommentCount      = aSnapshot.IssueComments.size(),
			.ReviewThreadsAvailable = aSnapshot.ReviewThreadsAvailability
				== github_collection_availability::available,
			.Capabilities            = aSnapshot.CapabilityEvidence.Capabilities,
			.CapabilityRevision      = aSnapshot.CapabilityEvidence.Revision,
			.RateLimits              = project(aSnapshot.RateLimits),
			.Conversations           = std::move(conversations),
			.PublicPreview           = aSnapshot.Mode == core::github_access_mode::public_preview,
			.ReviewCommentsAvailable = aSnapshot.ReviewCommentsAvailability == github_collection_availability::available,
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
			.CapabilityDetail  = aDiagnostic.CapabilityDetail == github_capability_diagnostic::none ? github_capability_diagnostic::none : bounded_capability_diagnostic(aDiagnostic.CapabilityDetail),
			.AccountMismatch   = aDiagnostic.AccountMismatch,
		};
	}
}
