#include "bpr/application/github_provider_port.hpp"

#include <concepts>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
	using namespace bpr;

	static_assert(std::is_abstract_v<application::github_provider_port>);
	static_assert(
		std::derived_from<
			application::github_provider_port,
			application::github_profile_port>);
	static_assert(
		std::derived_from<
			application::github_provider_port,
			application::github_viewer_port>);
	static_assert(
		std::derived_from<
			application::github_provider_port,
			application::github_capability_port>);
	static_assert(
		std::derived_from<
			application::github_provider_port,
			application::github_pull_request_port>);
	static_assert(
		std::variant_size_v<application::github_profile_completion> == 3U);
	static_assert(
		std::variant_size_v<application::github_viewer_completion> == 3U);
	static_assert(
		std::variant_size_v<application::github_capability_completion> == 3U);
	static_assert(
		std::variant_size_v<application::github_pull_request_completion> == 3U);
	static_assert(
		!std::is_convertible_v<std::string, application::github_node_id>);
	static_assert(
		!std::same_as<
			application::github_node_id,
			application::github_comment_id>);
	static_assert(
		!std::same_as<
			application::github_actor_id,
			application::github_node_id>);

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto expect_error(
		const application::github_read_validation       aActual,
		const application::github_read_validation_error aExpected,
		const std::string_view                          aMessage) -> void
	{
		expect(aActual.Error == aExpected, aMessage);
	}

	auto session_context() -> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 1 },
			.ApplicationSession = core::application_session_id{ 2 },
			.Profile            = core::host_profile_id{ 3 },
			.IdentityGeneration = core::github_identity_generation{ 4 },
		};
	}

	auto pull_request_context() -> core::pull_request_request_context
	{
		return {
			.Request             = core::request_id{ 11 },
			.ApplicationSession  = core::application_session_id{ 12 },
			.Profile             = core::host_profile_id{ 13 },
			.Host                = core::git_host_id{ 14 },
			.Account             = core::github_account_id{ 15 },
			.Repository          = { .Owner = "openai", .Name = "better-pull-review" },
			.Number              = core::pull_request_number{ 16 },
			.ExpectedRepository  = core::repository_id{ 17 },
			.ExpectedPullRequest = core::pull_request_id{ 18 },
			.Capabilities        = core::capability_revision{ 19 },
			.IdentityGeneration  = core::github_identity_generation{ 20 },
			.Generation          = core::request_generation{ 21 },
		};
	}

	auto capability_evidence(
		const core::capability_revision aRevision)
		-> application::github_capability_evidence
	{
		return {
			.Capabilities = {
				.ReviewThreads       = core::github_capability_state::supported,
				.ViewedFileMutations = core::github_capability_state::unsupported,
				.ResolveThread       = core::github_capability_state::supported,
				.UnresolveThread     = core::github_capability_state::unknown,
			},
			.Revision  = aRevision,
			.ReadShape = {
				.ReviewThreads       = true,
				.ViewedFileState     = true,
				.ResolvePermission   = true,
				.UnresolvePermission = true,
			},
			.ContractVersion = 1,
		};
	}

	[[nodiscard]] auto rate_limit(
		const application::github_rate_limit_resource aResource)
		-> application::github_rate_limit_record
	{
		return {
			.Resource         = aResource,
			.Limit            = 5'000U,
			.Remaining        = 4'999U,
			.ResetUnixSeconds = 2'000'000'000U,
			.SecondaryLimited = false,
		};
	}

	[[nodiscard]] auto rate_limits() -> application::github_rate_limit_state
	{
		auto core      = rate_limit(application::github_rate_limit_resource::core);
		core.Remaining = 4'900U;

		auto search             = rate_limit(application::github_rate_limit_resource::search);
		search.Limit            = 30U;
		search.Remaining        = 29U;
		search.ResetUnixSeconds = 2'000'000'100U;

		auto graphql             = rate_limit(application::github_rate_limit_resource::graphql);
		graphql.Remaining        = 4'998U;
		graphql.ResetUnixSeconds = 2'000'000'200U;

		return {
			.Core    = core,
			.Search  = search,
			.Graphql = graphql,
		};
	}

	auto valid_session() -> application::github_session_read_model
	{
		return {
			.Profile = core::host_profile_id{ 3 },
			.Host    = core::git_host_id{ 14 },
			.Viewer  = {
				.Account     = core::github_account_id{ 15 },
				.Node        = application::github_actor_id{ "reviewer-node" },
				.Login       = "reviewer",
				.DisplayName = "Review Engineer",
			},
			.CapabilityEvidence = capability_evidence(core::capability_revision{ 1 }),
			.RateLimits         = rate_limits(),
		};
	}

	auto valid_snapshot() -> application::github_pull_request_snapshot
	{
		const auto baseOid = application::commit_oid{
			"1111111111111111111111111111111111111111"
		};
		const auto headOid = application::commit_oid{
			"2222222222222222222222222222222222222222"
		};
		const auto commitOid = application::commit_oid{
			"3333333333333333333333333333333333333333"
		};

		return {
			.Profile = core::host_profile_id{ 13 },
			.Host    = core::git_host_id{ 14 },
			.Account = core::github_account_id{ 15 },
			.Viewer  = {
				.Actor = application::github_actor_id{ "reviewer-node" },
				.Login = "reviewer",
			},
			.Snapshot   = core::provider_snapshot_id{ 22 },
			.Repository = {
				.Repository    = core::repository_id{ 17 },
				.Node          = application::github_node_id{ "repository-node" },
				.Owner         = "openai",
				.Name          = "better-pull-review",
				.DefaultBranch = "main",
				.Private       = true,
			},
			.PullRequest = {
				.PullRequest = core::pull_request_id{ 18 },
				.Repository  = core::repository_id{ 17 },
				.Number      = core::pull_request_number{ 16 },
				.Node        = application::github_node_id{ "pull-request-node" },
				.Title       = "Normalize GitHub reads",
				.Body        = "Bounded synthetic pull request body.",
				.State       = application::github_pull_request_state::open,
				.Draft       = false,
				.Author      = application::github_actor_record{
					.Actor = application::github_actor_id{ "author-node" },
					.Login = "author",
				},
				.BaseOid = baseOid,
				.HeadOid = headOid,
				.BaseRef = "main",
				.HeadRef = "normalized-reads",
			},
			.Files = {
				{
					.Path         = "src/github.cpp",
					.PreviousPath = std::nullopt,
					.Change       = application::github_file_change::modified,
					.Additions    = 12,
					.Deletions    = 3,
					.ViewedState  = application::github_file_viewed_state::viewed,
				},
			},
			.Commits = {
				{
					.Oid    = commitOid,
					.Node   = application::github_node_id{ "commit-node" },
					.Author = application::github_actor_record{
						.Actor = application::github_actor_id{ "author-node" },
						.Login = "author",
					},
					.Headline = "Add normalized read model",
				},
			},
			.Reviews = {
				{
					.DatabaseId = 4'001U,
					.Review     = application::github_review_id{ "review-node" },
					.Author     = application::github_actor_record{
						.Actor = application::github_actor_id{ "reviewer-node" },
						.Login = "reviewer",
					},
					.State  = application::github_review_state::approved,
					.Commit = commitOid,
					.Body   = "Looks good.",
				},
			},
			.ReviewThreadsAvailability = application::github_collection_availability::available,
			.ReviewThreads             = {
				{
					.Thread                    = application::github_thread_id{ "thread-node" },
					.Subject                   = application::github_review_thread_subject::line,
					.Path                      = "src/github.cpp",
					.Line                      = 42,
					.DiffSide                  = application::github_diff_side::right,
					.Resolved                  = false,
					.Outdated                  = false,
					.ViewerResolvePermission   = application::github_permission_state::allowed,
					.ViewerUnresolvePermission = application::github_permission_state::denied,
					.Comments                  = {
						{
							.Comment = application::github_comment_id{ "review-comment-node" },
							.ReplyTo = std::nullopt,
							.Author  = application::github_actor_record{
								.Actor = application::github_actor_id{ "reviewer-node" },
								.Login = "reviewer",
							},
							.Body = "Please keep this bounded.",
							.Path = "src/github.cpp",
							.Line = 42,
						},
					},
				},
			},
			.IssueComments = {
				{
					.Comment = application::github_comment_id{ "issue-comment-node" },
					.Author  = application::github_actor_record{
						.Actor = application::github_actor_id{ "author-node" },
						.Login = "author",
					},
					.Body = "Ready for review.",
				},
			},
			.Checks = {
				{
					.Check      = application::github_check_id{ "check-node" },
					.Name       = "local-core-only",
					.Status     = application::github_check_status::completed,
					.Conclusion = application::github_check_conclusion::success,
				},
			},
			.SearchResults = {
				{
					.Repository      = core::repository_id{ 17 },
					.PullRequestNode = application::github_node_id{ "search-pr-node" },
					.Number          = core::pull_request_number{ 32 },
					.Title           = "Another result",
					.State           = application::github_pull_request_state::open,
				},
			},
			.SearchAvailability = application::github_search_availability::available,
			.CapabilityEvidence = capability_evidence(core::capability_revision{ 19 }),
			.RateLimits         = rate_limits(),
		};
	}

	auto test_relationship_diagnostics() -> void
	{
		using error  = application::github_read_validation_error;
		auto limited = valid_snapshot();
		limited.SearchResults.clear();
		limited.SearchAvailability = application::github_search_availability::result_limit;
		expect(application::validate(limited, pull_request_context()).valid(), "refused ancillary search does not invalidate exact PR data");
		limited.SearchAvailability = static_cast<application::github_search_availability>(255);
		expect(application::validate(limited, pull_request_context()).Error == error::invalid_enum, "unknown search availability is refused");
		const auto check = [](const auto& aMutate, const error aDetail) {
			auto snapshot = valid_snapshot();
			aMutate(snapshot);
			const auto result = application::validate(snapshot, pull_request_context());
			expect(!result.valid() && result.Error == error::invalid_relationship,
				   "relationship diagnostic preserves validation category and refusal");
			expect(result.Detail == aDetail,
				   "relationship diagnostic identifies the exact failed invariant");
		};
		check([](auto& aSnapshot) { aSnapshot.Files.front().PreviousPath = "old.cpp"; }, error::relationship_file_previous_path);
		check([](auto& aSnapshot) { aSnapshot.ReviewThreads.front().Comments.front().Path = "other.cpp"; }, error::relationship_thread_comment_path);
		check([](auto& aSnapshot) { aSnapshot.IssueComments.front().Author->Login = "other-login"; }, error::relationship_actor_login);
		check([](auto& aSnapshot) { aSnapshot.ReviewThreads.front().Comments.front().ReplyTo = application::github_comment_id{ "absent" }; }, error::relationship_thread_reply);
		check([](auto& aSnapshot) { aSnapshot.SearchResults.front().Repository = core::repository_id{ 99 }; }, error::relationship_search_repository);
		check([](auto& aSnapshot) { aSnapshot.SearchResults.front().Number = aSnapshot.PullRequest.Number; }, error::relationship_search_target_identity);
		check([](auto& aSnapshot) { aSnapshot.SearchResults.front().Number = aSnapshot.PullRequest.Number; aSnapshot.SearchResults.front().PullRequestNode = aSnapshot.PullRequest.Node; }, error::relationship_search_target_metadata);
		check([](auto& aSnapshot) { aSnapshot.ForcePushEvents.push_back({ }); }, error::relationship_timeline_availability);
		check([](auto& aSnapshot) { aSnapshot.TimelineAvailability = application::github_timeline_availability::available; aSnapshot.ForcePushEvents.push_back({ }); }, error::relationship_timeline_event);
		check([](auto& aSnapshot) { aSnapshot.ReviewCommentGroups.push_back({ }); }, error::relationship_review_comments_availability);
		check([](auto& aSnapshot) { aSnapshot.ReviewThreadsAvailability = application::github_collection_availability::unavailable; }, error::relationship_review_threads_capability);
		check([](auto& aSnapshot) { aSnapshot.Files.front().ViewedState = application::github_file_viewed_state::unavailable; }, error::relationship_file_viewed_capability);
		check([](auto& aSnapshot) { aSnapshot.ReviewThreads.front().ViewerResolvePermission = application::github_permission_state::unavailable; }, error::relationship_thread_permission_capability);
		check([](auto& aSnapshot) { aSnapshot.ReviewCommentsAvailability = application::github_collection_availability::available; auto group = application::github_review_comment_group_record{ .Root = aSnapshot.ReviewThreads.front().Comments.front() }; group.Root.ReplyTo = application::github_comment_id{ "absent" }; aSnapshot.ReviewCommentGroups.push_back(std::move(group)); }, error::relationship_review_group_root);
		check([](auto& aSnapshot) { aSnapshot.ReviewCommentsAvailability = application::github_collection_availability::available; auto group = application::github_review_comment_group_record{ .Root = aSnapshot.ReviewThreads.front().Comments.front() }; group.Replies.push_back(group.Root); aSnapshot.ReviewCommentGroups.push_back(std::move(group)); }, error::relationship_review_group_reply);
		check([](auto& aSnapshot) { aSnapshot.SearchAvailability = application::github_search_availability::result_limit; }, error::relationship_search_availability);
		using origin = application::github_actor_observation_origin;
		for (const auto login : { "AUTHOR", "other-login", "authør" }) {
			auto snapshot                                = valid_snapshot();
			snapshot.IssueComments.front().Author->Login = login;
			const auto result                            = application::validate(snapshot, pull_request_context());
			expect(result.Detail == error::relationship_actor_login && result.ActorConflict
					   && result.ActorConflict->First == origin::pull_request && result.ActorConflict->Conflicting == origin::issue_comment
					   && result.ActorConflict->AsciiCaseOnly == (std::string_view{ login } == "AUTHOR"),
				   "actor provenance distinguishes ASCII-only case from true disagreement without changing refusal");
		}
		expect(!application::validate(valid_snapshot(), pull_request_context()).ActorConflict, "valid snapshot carries no invented actor conflict");
		expect(application::validate(valid_snapshot(), pull_request_context()).Detail == error::none, "valid snapshot has no diagnostic failure");
	}

	auto test_valid_models_and_identity_correlation() -> void
	{
		const auto session        = valid_session();
		const auto sessionRequest = session_context();
		expect(
			application::validate(session, sessionRequest).valid(),
			"valid session read model is accepted");

		auto wrongSessionContext    = sessionRequest;
		wrongSessionContext.Profile = core::host_profile_id{ 99 };
		expect_error(
			application::validate(session, wrongSessionContext),
			application::github_read_validation_error::identity_mismatch,
			"session profile mismatch is rejected");

		const auto snapshot = valid_snapshot();
		const auto request  = pull_request_context();
		expect(
			application::validate(snapshot, request).valid(),
			"valid aggregate pull request snapshot is accepted");
		expect(
			snapshot.RateLimits.Core.has_value()
				&& snapshot.RateLimits.Search.has_value()
				&& snapshot.RateLimits.Graphql.has_value()
				&& snapshot.RateLimits.Core->Resource
					== application::github_rate_limit_resource::core
				&& snapshot.RateLimits.Core->Remaining == 4'900U
				&& snapshot.RateLimits.Search->Resource
					== application::github_rate_limit_resource::search
				&& snapshot.RateLimits.Search->Limit == 30U
				&& snapshot.RateLimits.Search->Remaining == 29U
				&& snapshot.RateLimits.Graphql->Resource
					== application::github_rate_limit_resource::graphql
				&& snapshot.RateLimits.Graphql->Remaining == 4'998U,
			"snapshot preserves distinct core, search, and GraphQL rate limits");

		auto mismatch    = snapshot;
		mismatch.Profile = core::host_profile_id{ 99 };
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"profile mismatch is rejected");
		mismatch      = snapshot;
		mismatch.Host = core::git_host_id{ 99 };
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"host mismatch is rejected");
		mismatch         = snapshot;
		mismatch.Account = core::github_account_id{ 99 };
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"account mismatch is rejected");
		mismatch                    = snapshot;
		mismatch.PullRequest.Number = core::pull_request_number{ 99 };
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"pull request number mismatch is rejected");
		mismatch                  = snapshot;
		mismatch.Repository.Owner = "different";
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"repository owner mismatch is rejected");
		mismatch                 = snapshot;
		mismatch.Repository.Name = "different";
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"repository name mismatch is rejected");
		mismatch                       = snapshot;
		mismatch.Repository.Repository = core::repository_id{ 99 };
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"expected repository mismatch is rejected");
		mismatch                         = snapshot;
		mismatch.PullRequest.PullRequest = core::pull_request_id{ 99 };
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"expected pull request mismatch is rejected");
		mismatch                             = snapshot;
		mismatch.CapabilityEvidence.Revision = core::capability_revision{ 99 };
		expect_error(
			application::validate(mismatch, request),
			application::github_read_validation_error::identity_mismatch,
			"capability revision mismatch is rejected");

		auto caseOnly             = snapshot;
		caseOnly.Repository.Owner = "OpenAI";
		caseOnly.Repository.Name  = "Better-Pull-Review";
		expect(
			application::validate(caseOnly, request).valid(),
			"GitHub repository locator correlation is ASCII case-insensitive");

		auto fileSubject                          = snapshot;
		fileSubject.ReviewThreads.front().Subject = application::github_review_thread_subject::file;
		fileSubject.ReviewThreads.front().Line.reset();
		fileSubject.ReviewThreads.front().Comments.front().Line.reset();
		expect(
			application::validate(fileSubject, request).valid(),
			"file-level review thread with no line is accepted");

		auto originalOnlyComment = snapshot;
		originalOnlyComment.ReviewThreads.front().Comments.front().Line.reset();
		originalOnlyComment.ReviewThreads.front().Comments.front().OriginalLine = 42U;
		expect(
			application::validate(originalOnlyComment, request).valid(),
			"line-level review comment may retain only its nullable original line");

		auto missingCommentLine = snapshot;
		missingCommentLine.ReviewThreads.front().Comments.front().Line.reset();
		missingCommentLine.ReviewThreads.front().Comments.front().OriginalLine.reset();
		expect(
			application::validate(missingCommentLine, request).valid(),
			"line-level reply may inherit the root anchor with no comment coordinate");

		auto unexpectedCommentOriginalLine = fileSubject;
		unexpectedCommentOriginalLine.ReviewThreads.front()
			.Comments.front()
			.OriginalLine = 42U;
		expect_error(
			application::validate(unexpectedCommentOriginalLine, request),
			application::github_read_validation_error::invalid_relationship,
			"file-level review comment rejects an original line");

		auto missingLine = snapshot;
		missingLine.ReviewThreads.front().Line.reset();
		missingLine.ReviewThreads.front().OriginalLine = 42U;
		expect_error(
			application::validate(missingLine, request),
			application::github_read_validation_error::invalid_line,
			"a current LINE thread cannot substitute originalLine for its missing current line");

		for (const bool outdated : { false, true }) {
			auto  unanchored = snapshot;
			auto& thread     = unanchored.ReviewThreads.front();
			thread.Line.reset();
			thread.OriginalLine.reset();
			thread.Outdated = outdated;
			expect(application::validate(unanchored, request).valid(),
				   "an exact LINE thread may have no current or original anchor");
		}

		auto unexpectedLine                       = fileSubject;
		unexpectedLine.ReviewThreads.front().Line = 42;
		expect_error(
			application::validate(unexpectedLine, request),
			application::github_read_validation_error::invalid_line,
			"file-level review thread rejects a line");

		auto maximumLine                                        = snapshot;
		maximumLine.ReviewThreads.front().Line                  = application::maximum_github_line;
		maximumLine.ReviewThreads.front().Comments.front().Line = application::maximum_github_line;
		expect(
			application::validate(maximumLine, request).valid(),
			"GraphQL line at signed Int maximum is accepted");

		auto excessiveLine                                        = snapshot;
		excessiveLine.ReviewThreads.front().Comments.front().Line = application::maximum_github_line + 1U;
		expect_error(
			application::validate(excessiveLine, request),
			application::github_read_validation_error::invalid_line,
			"GraphQL line above signed Int range is rejected");

		auto missingPreviousPath                 = snapshot;
		missingPreviousPath.Files.front().Change = application::github_file_change::renamed;
		expect(
			application::validate(missingPreviousPath, request).valid(),
			"renamed file may omit an unavailable previous path");

		auto copiedWithPreviousPath                       = snapshot;
		copiedWithPreviousPath.Files.front().Change       = application::github_file_change::copied;
		copiedWithPreviousPath.Files.front().PreviousPath = "src/copied_file.cpp";
		expect(
			application::validate(copiedWithPreviousPath, request).valid(),
			"copied file may retain its available source path");

		auto unchanged                 = snapshot;
		unchanged.Files.front().Change = application::github_file_change::unchanged;
		expect(
			application::validate(unchanged, request).valid(),
			"unchanged is a valid GitHub file status");

		auto unexpectedPreviousPath                       = snapshot;
		unexpectedPreviousPath.Files.front().PreviousPath = "src/old_file.cpp";
		expect_error(
			application::validate(unexpectedPreviousPath, request),
			application::github_read_validation_error::invalid_relationship,
			"non-renamed file rejects a previous path");

		auto mismatchedCommentPath = snapshot;
		mismatchedCommentPath.ReviewThreads.front()
			.Comments.front()
			.Path = "src/other.cpp";
		expect_error(
			application::validate(mismatchedCommentPath, request),
			application::github_read_validation_error::invalid_relationship,
			"review comment path must match its thread");
	}

	auto test_malformed_enums_fail_closed() -> void
	{
		expect(
			application::valid(
				application::github_provider_failure::malformed_response),
			"known provider failure is valid");
		expect(
			!application::valid(
				static_cast<application::github_provider_failure>(255)),
			"malformed provider failure is rejected");

		auto transportError = application::github_provider_error{
			.Failure                = application::github_provider_failure::permission,
			.HttpStatus             = 404U,
			.TransportFailureSource = application::diagnostic_transport_failure_source::http_response,
		};
		expect(application::valid(transportError), "HTTP denial carries bounded diagnostic facts");
		transportError.HttpStatus = 0U;
		expect(!application::valid(transportError), "HTTP source requires observed status");
		transportError.TransportFailureSource = application::diagnostic_transport_failure_source::network_reply;
		expect(application::valid(transportError), "network reply may fail without HTTP status");
		transportError.HttpStatus = 600U;
		expect(!application::valid(transportError), "invalid status is rejected");
		transportError.HttpStatus             = 0U;
		transportError.TransportFailureSource = static_cast<application::diagnostic_transport_failure_source>(255U);
		expect(!application::valid(transportError), "invalid transport source is rejected");

		const auto observedRateLimits = rate_limits();
		auto       coreExhausted      = *observedRateLimits.Core;
		coreExhausted.Remaining       = 0U;
		auto primaryLimited           = observedRateLimits;
		primaryLimited.Core           = coreExhausted;

		auto graphqlSecondary             = *observedRateLimits.Graphql;
		graphqlSecondary.SecondaryLimited = true;
		auto secondaryLimited             = observedRateLimits;
		secondaryLimited.Graphql          = graphqlSecondary;

		expect(
			application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = application::maximum_github_retry_after_seconds,
				.RateLimits        = primaryLimited,
				.RateLimitCause    = coreExhausted,
			}),
			"bounded primary limit names the exact exhausted resource");
		expect(
			application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::unavailable,
				.RetryAfterSeconds = 30U,
				.RateLimits        = observedRateLimits,
				.RateLimitCause    = std::nullopt,
			}),
			"unavailable failures may preserve all observed resource limits");
		expect(
			application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = 60U,
				.RateLimits        = secondaryLimited,
				.RateLimitCause    = graphqlSecondary,
			}),
			"secondary limit names the exact triggering GraphQL resource");
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = primaryLimited,
				.RateLimitCause    = std::nullopt,
			}),
			"rate-limit failures require an exact triggering record");
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::network,
				.RetryAfterSeconds = 1U,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			}),
			"retry metadata is rejected for an ineligible failure");
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = application::maximum_github_retry_after_seconds + 1U,
				.RateLimits        = primaryLimited,
				.RateLimitCause    = coreExhausted,
			}),
			"retry metadata above one day is rejected");
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::permission,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = secondaryLimited,
				.RateLimitCause    = graphqlSecondary,
			}),
			"a triggering record cannot accompany a non-rate-limit failure");
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = observedRateLimits,
				.RateLimitCause    = observedRateLimits.Core,
			}),
			"a non-exhausted primary record cannot trigger rate limiting");

		auto mismatchedCause = coreExhausted;
		++mismatchedCause.ResetUnixSeconds;
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = primaryLimited,
				.RateLimitCause    = mismatchedCause,
			}),
			"trigger metadata must equal its resource-keyed observation");

		auto wrongResourceSlot           = observedRateLimits;
		wrongResourceSlot.Core->Resource = application::github_rate_limit_resource::search;
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::unavailable,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = wrongResourceSlot,
				.RateLimitCause    = std::nullopt,
			}),
			"a rate-limit record cannot occupy another resource's slot");

		auto wrongSecondaryCause = secondaryLimited;
		wrongSecondaryCause.Core = coreExhausted;
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = wrongSecondaryCause,
				.RateLimitCause    = coreExhausted,
			}),
			"a secondary-limited resource must be the named trigger");

		auto multipleSecondaryCauses                   = secondaryLimited;
		multipleSecondaryCauses.Core->SecondaryLimited = true;
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = multipleSecondaryCauses,
				.RateLimitCause    = graphqlSecondary,
			}),
			"one failure cannot claim multiple secondary-limit triggers");

		auto malformedSecondary               = secondaryLimited;
		malformedSecondary.Graphql->Remaining = 0U;
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::rate_limit,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = malformedSecondary,
				.RateLimitCause    = malformedSecondary.Graphql,
			}),
			"secondary limiting remains distinct from primary exhaustion");

		auto malformedArithmetic              = observedRateLimits;
		malformedArithmetic.Search->Remaining = malformedArithmetic.Search->Limit + 1U;
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = application::github_provider_failure::unavailable,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = malformedArithmetic,
				.RateLimitCause    = std::nullopt,
			}),
			"provider errors reject malformed rate-limit arithmetic");
		expect(
			!application::valid(application::github_provider_error{
				.Failure           = static_cast<application::github_provider_failure>(255),
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			}),
			"a provider error rejects a malformed failure enum");

		const auto request         = pull_request_context();
		auto       snapshot        = valid_snapshot();
		snapshot.PullRequest.State = static_cast<application::github_pull_request_state>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed pull request state is rejected");

		snapshot                      = valid_snapshot();
		snapshot.Files.front().Change = static_cast<application::github_file_change>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed file change is rejected");

		snapshot                      = valid_snapshot();
		snapshot.Files.front().Change = application::github_file_change::uninitialized;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"uninitialized file change is rejected");

		snapshot                           = valid_snapshot();
		snapshot.Files.front().ContentKind = static_cast<application::github_file_content_kind>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed file content kind is rejected");

		snapshot                           = valid_snapshot();
		snapshot.Files.front().ViewedState = static_cast<application::github_file_viewed_state>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed viewed-file state is rejected");

		for (const auto viewedState : {
				 application::github_file_viewed_state::dismissed,
				 application::github_file_viewed_state::unviewed,
				 application::github_file_viewed_state::viewed,
			 }) {
			snapshot                           = valid_snapshot();
			snapshot.Files.front().ViewedState = viewedState;
			expect(
				application::validate(snapshot, request).valid(),
				"every known viewed-file state is accepted");
		}
		snapshot                                              = valid_snapshot();
		snapshot.CapabilityEvidence.ReadShape.ViewedFileState = false;
		snapshot.Files.front().ViewedState                    = application::github_file_viewed_state::unavailable;
		expect(
			application::validate(snapshot, request).valid(),
			"capability-omitted viewed state is explicitly unavailable");
		snapshot.Files.front().ViewedState = application::github_file_viewed_state::unviewed;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"an omitted viewed-state selection cannot become authoritative");

		snapshot                       = valid_snapshot();
		snapshot.Reviews.front().State = static_cast<application::github_review_state>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed review state is rejected");

		snapshot                       = valid_snapshot();
		snapshot.Reviews.front().State = application::github_review_state::uninitialized;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"uninitialized review state is rejected");

		snapshot                               = valid_snapshot();
		snapshot.ReviewThreads.front().Subject = static_cast<application::github_review_thread_subject>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed review thread subject is rejected");

		snapshot                                               = valid_snapshot();
		snapshot.ReviewThreads.front().ViewerResolvePermission = static_cast<application::github_permission_state>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed thread permission is rejected");

		snapshot                                                = valid_snapshot();
		snapshot.CapabilityEvidence.ReadShape.ResolvePermission = false;
		snapshot.ReviewThreads.front().ViewerResolvePermission  = application::github_permission_state::unavailable;
		snapshot.CapabilityEvidence.Capabilities.ResolveThread  = core::github_capability_state::unsupported;
		expect(
			application::validate(snapshot, request).valid(),
			"omitted resolve permission is distinct from denied");
		snapshot.ReviewThreads.front().ViewerResolvePermission = application::github_permission_state::denied;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"an omitted permission cannot be projected as denied");

		snapshot                           = valid_snapshot();
		snapshot.ReviewThreadsAvailability = application::github_collection_availability::unavailable;
		snapshot.ReviewThreads.clear();
		snapshot.CapabilityEvidence.ReadShape.ReviewThreads      = false;
		snapshot.CapabilityEvidence.Capabilities.ReviewThreads   = core::github_capability_state::unsupported;
		snapshot.CapabilityEvidence.Capabilities.ResolveThread   = core::github_capability_state::unsupported;
		snapshot.CapabilityEvidence.Capabilities.UnresolveThread = core::github_capability_state::unsupported;
		expect(
			application::validate(snapshot, request).valid(),
			"capability-omitted thread collection is explicitly unavailable");
		snapshot.ReviewThreadsAvailability = application::github_collection_availability::available;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"selection omission cannot become an authoritative empty collection");

		snapshot                       = valid_snapshot();
		snapshot.Checks.front().Status = static_cast<application::github_check_status>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed check status is rejected");

		snapshot                       = valid_snapshot();
		snapshot.Checks.front().Status = application::github_check_status::uninitialized;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"uninitialized check status is rejected");

		snapshot                           = valid_snapshot();
		snapshot.Checks.front().Conclusion = static_cast<application::github_check_conclusion>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed check conclusion is rejected");

		snapshot                           = valid_snapshot();
		snapshot.Checks.front().Conclusion = application::github_check_conclusion::uninitialized;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"uninitialized check conclusion is rejected");

		snapshot                       = valid_snapshot();
		snapshot.Checks.front().Status = application::github_check_status::in_progress;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"in-progress check cannot carry a completed conclusion");

		snapshot                           = valid_snapshot();
		snapshot.Checks.front().Conclusion = application::github_check_conclusion::none;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"completed check requires a conclusion");

		snapshot                             = valid_snapshot();
		snapshot.SearchResults.front().State = static_cast<application::github_pull_request_state>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed search result state is rejected");

		snapshot                             = valid_snapshot();
		snapshot.SearchResults.front().State = application::github_pull_request_state::closed;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"selected open-only search rejects a closed result");

		snapshot                                               = valid_snapshot();
		snapshot.CapabilityEvidence.Capabilities.ReviewThreads = static_cast<core::github_capability_state>(255);
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_enum,
			"malformed capability state is rejected");
	}

	auto test_default_collection_and_field_bounds() -> void
	{
		const application::github_read_limits defaults;
		expect(defaults.MaximumTitleBytes == 1'024U, "title limit is 1 KiB");
		expect(defaults.MaximumFiles == 3'000U, "file limit is 3,000");
		expect(
			defaults.MaximumCommits == 250U,
			"pull-request commit limit matches the provider endpoint ceiling");
		expect(
			defaults.MaximumReviews == 10'000U
				&& defaults.MaximumChecks == 10'000U,
			"general list limits are 10,000");
		expect(
			defaults.MaximumIssueComments == 10'000U,
			"issue comment limit is explicit");
		expect(
			defaults.MaximumSearchResults == 1'000U,
			"search result limit is 1,000");
		expect(
			defaults.MaximumTotalTextBytes == 16U * 1'024U * 1'024U,
			"normalized text limit is 16 MiB");

		const auto request  = pull_request_context();
		auto       snapshot = valid_snapshot();
		snapshot.PullRequest.Title.assign(defaults.MaximumTitleBytes, 't');
		expect(
			application::validate(snapshot, request).valid(),
			"title at default N is accepted");
		snapshot.PullRequest.Title.push_back('t');
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::text_too_long,
			"title at default N+1 is rejected");

		snapshot = valid_snapshot();
		snapshot.Files.clear();
		snapshot.Files.reserve(defaults.MaximumFiles + 1U);
		for (std::size_t index = 0; index < defaults.MaximumFiles; ++index) {
			snapshot.Files.push_back(application::github_file_record{
				.Path         = "file-" + std::to_string(index),
				.PreviousPath = std::nullopt,
				.Change       = application::github_file_change::modified,
				.ViewedState  = application::github_file_viewed_state::unviewed,
			});
		}
		expect(
			application::validate(snapshot, request).valid(),
			"files at default N are accepted");
		snapshot.Files.push_back(application::github_file_record{
			.Path         = "file-over-limit",
			.PreviousPath = std::nullopt,
			.Change       = application::github_file_change::modified,
			.ViewedState  = application::github_file_viewed_state::unviewed,
		});
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::collection_too_large,
			"files at default N+1 are rejected");

		snapshot = valid_snapshot();
		snapshot.SearchResults.clear();
		snapshot.SearchResults.reserve(defaults.MaximumSearchResults + 1U);
		for (std::size_t index = 0; index < defaults.MaximumSearchResults;
			 ++index) {
			snapshot.SearchResults.push_back(
				application::github_search_result_record{
					.Repository      = core::repository_id{ 17 },
					.PullRequestNode = application::github_node_id{
						"search-pr-" + std::to_string(index) },
					.Number = core::pull_request_number{ static_cast<std::uint32_t>(index + 1'000U) },
					.Title  = "result",
				});
		}
		expect(
			application::validate(snapshot, request).valid(),
			"search results at default N are accepted");
		snapshot.SearchResults.push_back(
			application::github_search_result_record{
				.Repository      = core::repository_id{ 20'000 },
				.PullRequestNode = application::github_node_id{ "search-over-limit" },
				.Number          = core::pull_request_number{ 20'000 },
				.Title           = "over limit",
			});
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::collection_too_large,
			"search results at default N+1 are rejected");
	}

	auto test_duplicate_identity_text_budget_and_rate_limit() -> void
	{
		const auto request  = pull_request_context();
		auto       snapshot = valid_snapshot();
		snapshot.Files.push_back(snapshot.Files.front());
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::duplicate_identity,
			"duplicate file path identity is rejected");

		snapshot             = valid_snapshot();
		auto duplicateCommit = snapshot.Commits.front();
		duplicateCommit.Node = application::github_node_id{ "different-commit-node" };
		snapshot.Commits.push_back(std::move(duplicateCommit));
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::duplicate_identity,
			"duplicate commit OID is rejected");

		snapshot                               = valid_snapshot();
		snapshot.IssueComments.front().Comment = snapshot.ReviewThreads.front().Comments.front().Comment;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::duplicate_identity,
			"duplicate comment identity across collections is rejected");

		snapshot                        = valid_snapshot();
		snapshot.Reviews.front().Review = application::github_review_id{
			std::string{ snapshot.Repository.Node.value() }
		};
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::duplicate_identity,
			"GitHub node identity cannot collide across entity kinds");

		snapshot                                     = valid_snapshot();
		snapshot.IssueComments.front().Author->Actor = application::github_actor_id{
			std::string{ snapshot.Repository.Node.value() }
		};
		snapshot.IssueComments.front().Author->Login = "repository-alias";
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::duplicate_identity,
			"actor node identity cannot collide with another entity kind");

		snapshot                                       = valid_snapshot();
		snapshot.SearchResults.front().PullRequestNode = snapshot.Commits.front().Node;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::duplicate_identity,
			"non-target search node cannot collide with another entity kind");

		snapshot                                                = valid_snapshot();
		snapshot.ReviewThreads.front().Comments.front().ReplyTo = application::github_comment_id{ "missing-parent" };
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"dangling review-comment reply is rejected");

		snapshot                                  = valid_snapshot();
		snapshot.SearchResults.front().Repository = core::repository_id{ 99 };
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"repo-scoped search result cannot cross repository identity");

		snapshot                                       = valid_snapshot();
		snapshot.SearchResults.front().PullRequestNode = snapshot.PullRequest.Node;
		snapshot.SearchResults.front().Number          = core::pull_request_number{ 17 };
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"target search node cannot carry another PR number");

		snapshot                                       = valid_snapshot();
		snapshot.SearchResults.front().PullRequestNode = application::github_node_id{ "different-pr-node" };
		snapshot.SearchResults.front().Number          = snapshot.PullRequest.Number;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"target search number cannot carry another PR node");

		snapshot                                       = valid_snapshot();
		snapshot.SearchResults.front().PullRequestNode = snapshot.PullRequest.Node;
		snapshot.SearchResults.front().Number          = snapshot.PullRequest.Number;
		snapshot.SearchResults.front().Title           = "contradictory title";
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"target search title must match pull-request metadata");

		snapshot                                       = valid_snapshot();
		snapshot.SearchResults.front().PullRequestNode = snapshot.PullRequest.Node;
		snapshot.SearchResults.front().Number          = snapshot.PullRequest.Number;
		snapshot.SearchResults.front().Draft           = !snapshot.PullRequest.Draft;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"target search draft state must match pull-request metadata");

		snapshot                                       = valid_snapshot();
		snapshot.SearchResults.front().PullRequestNode = application::github_node_id{ "search-pr-3002" };
		snapshot.SearchResults.front().Number          = core::pull_request_number{ 18 };
		auto duplicateSearchNumber                     = snapshot.SearchResults.front();
		duplicateSearchNumber.PullRequestNode          = application::github_node_id{ "search-pr-3003" };
		snapshot.SearchResults.push_back(std::move(duplicateSearchNumber));
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::duplicate_identity,
			"repository-scoped search numbers are unique");

		snapshot                                     = valid_snapshot();
		snapshot.IssueComments.front().Author->Login = "different-author";
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"same actor ID cannot carry conflicting nonempty logins");

		snapshot                               = valid_snapshot();
		snapshot.Reviews.front().Author->Login = "viewer-alias";
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_relationship,
			"viewer node and login seed snapshot actor correlation");

		snapshot = valid_snapshot();
		snapshot.IssueComments.front().Author->Login.clear();
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::missing_required_text,
			"present actor requires a display login");

		snapshot                                     = valid_snapshot();
		snapshot.IssueComments.front().Author->Actor = application::github_actor_id{ "Author-node" };
		snapshot.IssueComments.front().Author->Login = "different-author";
		expect(
			application::validate(snapshot, request).valid(),
			"actor IDs remain case-sensitive");

		snapshot = valid_snapshot();
		snapshot.PullRequest.Author.reset();
		snapshot.Commits.front().Author.reset();
		snapshot.Reviews.front().Author.reset();
		snapshot.ReviewThreads.front().Comments.front().Author.reset();
		snapshot.IssueComments.front().Author.reset();
		expect(
			application::validate(snapshot, request).valid(),
			"nullable authors remain valid across every normalized record");

		auto session                 = valid_session();
		session.Viewer.Node          = application::github_actor_id{ "n" };
		session.Viewer.Login         = "l";
		session.Viewer.DisplayName   = "123456";
		auto limits                  = application::github_read_limits{ };
		limits.MaximumTotalTextBytes = 8U;
		expect(
			application::validate(session, session_context(), limits).valid(),
			"total normalized text at N is accepted");
		session.Viewer.DisplayName.push_back('7');
		expect_error(
			application::validate(session, session_context(), limits),
			application::github_read_validation_error::total_text_too_large,
			"total normalized text at N+1 is rejected");

		snapshot                               = valid_snapshot();
		snapshot.RateLimits.Graphql->Remaining = snapshot.RateLimits.Graphql->Limit + 1U;
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::invalid_rate_limit,
			"malformed GraphQL rate-limit arithmetic is rejected");

		snapshot                    = valid_snapshot();
		snapshot.PullRequest.Author = application::github_actor_record{ };
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::missing_identity,
			"present author identity cannot be empty");

		snapshot = valid_snapshot();
		snapshot.PullRequest.Author->Login.assign(
			application::github_read_limits{ }.MaximumLoginBytes + 1U,
			'a');
		expect_error(
			application::validate(snapshot, request),
			application::github_read_validation_error::text_too_long,
			"actor login is bounded by the normalized login limit");

		auto relaxedLimits = application::github_read_limits{ };
		++relaxedLimits.MaximumBodyBytes;
		expect_error(
			application::validate(valid_snapshot(), request, relaxedLimits),
			application::github_read_validation_error::invalid_limits,
			"callers cannot relax a security ceiling");

		auto invalidText                       = valid_snapshot();
		invalidText.IssueComments.front().Body = "before";
		invalidText.IssueComments.front().Body.push_back('\0');
		invalidText.IssueComments.front().Body += "after";
		expect_error(
			application::validate(invalidText, request),
			application::github_read_validation_error::invalid_text,
			"embedded NUL text cannot cross the normalized boundary");
	}

	class compiling_provider final : public application::github_provider_port
	{
	public:
		void resolve_profile(
			application::github_profile_request            aRequest,
			application::github_profile_completion_handler aCompletion) override
		{
			LastProfile = aRequest;
			aCompletion(application::github_profile_cancelled{
				.Request = aRequest,
			});
		}

		void cancel_profile(application::github_profile_request aRequest) override
		{
			LastProfileCancellation = aRequest;
		}

		void observe_viewer(
			application::github_viewer_submission         aSubmission,
			application::github_viewer_completion_handler aCompletion) override
		{
			LastViewer = aSubmission.Request;
			aCompletion(application::github_viewer_cancelled{
				.Request = aSubmission.Request,
			});
		}

		void cancel_viewer(application::github_viewer_request aRequest) override
		{
			LastViewerCancellation = aRequest;
		}

		void probe_capabilities(
			application::github_capability_request            aRequest,
			application::github_capability_completion_handler aCompletion)
			override
		{
			LastCapability = aRequest;
			aCompletion(application::github_capability_cancelled{
				.Request = aRequest,
			});
		}

		void cancel_capabilities(
			application::github_capability_request aRequest) override
		{
			LastCapabilityCancellation = std::move(aRequest);
		}

		void fetch_pull_request(
			application::github_pull_request_provider_request   aRequest,
			application::github_pull_request_completion_handler aCompletion)
			override
		{
			LastPullRequest = aRequest;
			aCompletion(application::github_pull_request_cancelled{
				.Request = aRequest,
			});
		}

		void cancel_pull_request(
			application::github_pull_request_provider_request aRequest) override
		{
			LastPullRequestCancellation = std::move(aRequest);
		}

		application::github_profile_request               LastProfile;
		application::github_profile_request               LastProfileCancellation;
		application::github_viewer_request                LastViewer;
		application::github_viewer_request                LastViewerCancellation;
		application::github_capability_request            LastCapability;
		application::github_capability_request            LastCapabilityCancellation;
		application::github_pull_request_provider_request LastPullRequest;
		application::github_pull_request_provider_request
			LastPullRequestCancellation;
	};

	auto test_completion_contexts_are_exact() -> void
	{
		const application::github_profile_authority authority{
			.Profile               = core::host_profile_id{ 3 },
			.ProfileRevision       = application::github_profile_revision{ 4 },
			.Host                  = core::git_host_id{ 14 },
			.Hostname              = application::github_canonical_hostname{ "github.com" },
			.ServerContract        = application::github_server_contract::github_com_2022_11_28,
			.Authentication        = application::github_authentication_transport::github_cli,
			.ProbeContractRevision = application::github_probe_contract_revision{ 5 },
		};
		const application::github_profile_request profileRequest{
			.Context = session_context(),
			.Attempt = application::github_provider_attempt{ 1U },
		};
		const application::github_viewer_request viewerRequest{
			.Context   = session_context(),
			.Authority = authority,
			.Attempt   = application::github_provider_attempt{ 1U },
		};
		const application::github_capability_request capabilityRequest{
			.Context          = session_context(),
			.ProfileAuthority = authority,
			.Viewer           = valid_session().Viewer,
			.AccountTransport = { },
			.Attempt          = application::github_provider_attempt{ 1U },
		};
		const application::github_pull_request_provider_request pullRequest{
			.Context            = pull_request_context(),
			.ProfileAuthority   = authority,
			.Viewer             = valid_session().Viewer,
			.CapabilityEvidence = { },
			.AccountTransport   = { },
			.Attempt            = application::github_provider_attempt{ 1U },
		};
		compiling_provider provider;

		application::github_profile_completion profileCompletion;
		provider.resolve_profile(
			profileRequest,
			[&](auto aCompletion) {
				profileCompletion = std::move(aCompletion);
			});
		expect(
			std::get<application::github_profile_cancelled>(
				profileCompletion)
					.Request
				== profileRequest,
			"profile cancellation returns the exact phase request");

		application::github_pull_request_completion pullCompletion;
		provider.fetch_pull_request(
			pullRequest,
			[&](auto aCompletion) {
				pullCompletion = std::move(aCompletion);
			});
		expect(
			std::get<application::github_pull_request_cancelled>(
				pullCompletion)
					.Request
				== pullRequest,
			"pull request cancellation returns the exact request context");

		const auto success = application::github_pull_request_completion{
			application::github_pull_request_success{
				.Request  = pullRequest,
				.Snapshot = valid_snapshot(),
			}
		};
		expect(
			std::get<application::github_pull_request_success>(success).Request
				== pullRequest,
			"success alternative retains the exact request context");

		auto failureRateLimits = rate_limits();
		auto failureCause      = *failureRateLimits.Core;
		failureCause.Remaining = 0U;
		failureRateLimits.Core = failureCause;
		const auto failure     = application::github_pull_request_completion{
			application::github_pull_request_error{
				.Request = pullRequest,
				.Error   = {
					.Failure           = application::github_provider_failure::rate_limit,
					.RetryAfterSeconds = 30,
					.RateLimits        = failureRateLimits,
					.RateLimitCause    = failureCause,
				},
			}
		};
		const auto& pullRequestError = std::get<application::github_pull_request_error>(failure);
		expect(
			pullRequestError.Request == pullRequest
				&& application::valid(pullRequestError.Error),
			"failure alternative retains exact context and a valid trigger");

		provider.cancel_profile(profileRequest);
		provider.cancel_viewer(viewerRequest);
		provider.cancel_capabilities(capabilityRequest);
		provider.cancel_pull_request(pullRequest);
		expect(
			provider.LastProfileCancellation == profileRequest
				&& provider.LastViewerCancellation == viewerRequest
				&& provider.LastCapabilityCancellation == capabilityRequest
				&& provider.LastPullRequestCancellation == pullRequest,
			"every phase cancellation retains its complete typed identity");
	}
}

auto main() -> int
{
	test_relationship_diagnostics();
	test_valid_models_and_identity_correlation();
	test_malformed_enums_fail_closed();
	test_default_collection_and_field_bounds();
	test_duplicate_identity_text_budget_and_rate_limit();
	test_completion_contexts_are_exact();
	return EXIT_SUCCESS;
}
