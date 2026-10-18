#include "canonical_json_expectations.hpp"
#include "github_graphql_operation_policy.hpp"
#include "github_read_contract.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github::detail;

	constexpr std::string_view head_oid = "2222222222222222222222222222222222222222";
	constexpr std::string_view base_oid = "1111111111111111111111111111111111111111";

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto read_all(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		expect(input.good(), "fixture file opens");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ },
		};
	}

	struct fixture_json_options : glz::opts
	{
		bool escape_control_characters    = true;
		bool validate_skipped             = true;
		bool validate_trailing_whitespace = true;
		constexpr fixture_json_options()
		{
			null_terminated       = false;
			error_on_unknown_keys = false;
			error_on_missing_keys = true;
		}
	};

	struct interaction_json
	{
		std::string                                     Operation;
		std::string                                     Method;
		std::string                                     Path;
		std::string                                     CanonicalQuery;
		std::map<std::string, std::string, std::less<>> SafeHeaders;
		std::optional<std::string>                      Body;
		std::string                                     ResponseFile;
		struct glaze
		{
			static constexpr auto value = glz::object("operation", &interaction_json::Operation, "method", &interaction_json::Method, "path", &interaction_json::Path, "canonical_query", &interaction_json::CanonicalQuery, "safe_headers", &interaction_json::SafeHeaders, "body", &interaction_json::Body, "response_file", &interaction_json::ResponseFile);
		};
	};
	struct recording_json
	{
		std::vector<interaction_json> Interactions;
		struct glaze
		{
			static constexpr auto value = glz::object("interactions", &recording_json::Interactions);
		};
	};

	struct loaded_recording
	{
		std::vector<recorded_exchange>  Exchanges;
		std::vector<operation_response> Responses;
	};

	[[nodiscard]] auto load_recording(
		const std::string_view aDirectory,
		const server_contract  aServer) -> loaded_recording
	{
		const auto root = std::filesystem::path{
			BPR_GITHUB_CONTRACT_FIXTURE_ROOT
		} / aDirectory;
		recording_json manifest;
		expect(!glz::read<fixture_json_options{ }>(manifest, read_all(root / "manifest.json")), "fixture manifest has its required typed JSON fields");
		const auto& interactions = manifest.Interactions;
		expect(
			interactions.size()
				== read_operation_catalog.size(),
			"manifest has exactly ten interactions");

		loaded_recording result;
		result.Exchanges.reserve(read_operation_catalog.size());
		result.Responses.reserve(read_operation_catalog.size());
		for (std::size_t index = 0U; index < interactions.size(); ++index) {
			const auto& interaction     = interactions[index];
			const auto  parsedOperation = read_operation_from_name(
				interaction.Operation);
			expect(parsedOperation.has_value(), "manifest operation is known");
			const auto operation = *parsedOperation;
			expect(
				operation
					== read_operation_catalog[static_cast<std::size_t>(index)],
				"manifest operations use canonical order");

			std::vector<http_header> headers;
			headers.reserve(interaction.SafeHeaders.size());
			for (const auto& [name, value] : interaction.SafeHeaders) {
				headers.push_back(http_header{ .Name = name, .Value = value });
			}

			const auto method = parse_recorded_http_method(
				interaction.Method);
			expect(
				method.has_value(),
				"manifest method is exactly GET or POST");
			const auto body         = interaction.Body.value_or(std::string{ });
			const auto responsePath = root
				/ interaction.ResponseFile;
			wire_response response{
				.Status  = 200,
				.Headers = {
					{
						.Name  = "Content-Type",
						.Value = "application/json",
					},
				},
				.Body         = read_all(responsePath),
				.Completeness = page_completeness::complete,
			};
			result.Exchanges.push_back(
				recorded_exchange{
					.Request = {
						.Semantic  = { },
						.Server    = aServer,
						.Operation = operation,
						.Method    = *method,
						.Path      = interaction.Path,
						.Query     = interaction.CanonicalQuery,
						.Headers   = std::move(headers),
						.Body      = body,
					},
					.Response = response,
				});
			result.Responses.push_back(
				operation_response{
					.Operation = operation,
					.Response  = std::move(response),
				});
		}
		return result;
	}

	[[nodiscard]] auto capabilities() -> core::github_capabilities
	{
		return {
			.ReviewThreads       = core::github_capability_state::supported,
			.ViewedFileMutations = core::github_capability_state::unsupported,
			.ResolveThread       = core::github_capability_state::unsupported,
			.UnresolveThread     = core::github_capability_state::unsupported,
		};
	}

	[[nodiscard]] auto canonical_rate_limits()
		-> application::github_rate_limit_state
	{
		return {
			.Core = application::github_rate_limit_record{
				.Resource         = application::github_rate_limit_resource::core,
				.Limit            = 5'000,
				.Remaining        = 4'999,
				.ResetUnixSeconds = 2'000'000'000,
			},
			.Search = application::github_rate_limit_record{
				.Resource         = application::github_rate_limit_resource::search,
				.Limit            = 30,
				.Remaining        = 29,
				.ResetUnixSeconds = 2'000'000'000,
			},
			.Graphql = application::github_rate_limit_record{
				.Resource         = application::github_rate_limit_resource::graphql,
				.Limit            = 5'000,
				.Remaining        = 4'999,
				.ResetUnixSeconds = 2'000'000'000,
			},
		};
	}

	[[nodiscard]] auto normalization_context(const server_contract aServer)
		-> github_read_normalization_context
	{
		return {
			.Server         = aServer,
			.SessionRequest = {
				.Request            = core::request_id{ 1 },
				.ApplicationSession = core::application_session_id{ 7 },
				.Profile            = core::host_profile_id{ 11 },
				.IdentityGeneration = core::github_identity_generation{ 3 },
			},
			.PullRequestRequest = {
				.Request            = core::request_id{ 2 },
				.ApplicationSession = core::application_session_id{ 7 },
				.Profile            = core::host_profile_id{ 11 },
				.Host               = core::git_host_id{ 22 },
				.Account            = core::github_account_id{ 1001 },
				.Repository         = {
					.Owner = "octo-org",
					.Name  = "review-repository",
				},
				.Number              = core::pull_request_number{ 17 },
				.ExpectedRepository  = core::repository_id{ 2001 },
				.ExpectedPullRequest = core::pull_request_id{ 3001 },
				.Capabilities        = core::capability_revision{ 44 },
				.IdentityGeneration  = core::github_identity_generation{ 3 },
				.Generation          = core::request_generation{ 1 },
			},
			.Profile            = core::host_profile_id{ 11 },
			.Host               = core::git_host_id{ 22 },
			.Account            = core::github_account_id{ 1001 },
			.Repository         = core::repository_id{ 2001 },
			.PullRequest        = core::pull_request_id{ 3001 },
			.Snapshot           = core::provider_snapshot_id{ 55 },
			.CapabilityRevision = core::capability_revision{ 44 },
			.Capabilities       = capabilities(),
			.RateLimits         = canonical_rate_limits(),
			.Limits             = { },
			.SearchTerms        = "is:open",
		};
	}

	[[nodiscard]] auto canonical_normalized_read() -> normalized_github_read
	{
		const application::github_capability_evidence evidence{
			.Capabilities = capabilities(),
			.Revision     = core::capability_revision{ 44 },
			.ReadShape    = {
				.ReviewThreads       = true,
				.ViewedFileState     = true,
				.ResolvePermission   = true,
				.UnresolvePermission = true,
			},
			.ContractVersion = 1,
		};
		const auto rateLimits = canonical_rate_limits();

		normalized_github_read result;
		result.Session = {
			.Profile = core::host_profile_id{ 11 },
			.Host    = core::git_host_id{ 22 },
			.Viewer  = {
				.Account     = core::github_account_id{ 1001 },
				.Node        = application::github_actor_id{ "U_fixture_viewer" },
				.Login       = "octo-reviewer",
				.DisplayName = "Octo Reviewer",
			},
			.CapabilityEvidence = evidence,
			.RateLimits         = rateLimits,
		};
		result.PullRequest = {
			.Profile = core::host_profile_id{ 11 },
			.Host    = core::git_host_id{ 22 },
			.Account = core::github_account_id{ 1001 },
			.Viewer  = {
				.Actor = application::github_actor_id{ "U_fixture_viewer" },
				.Login = "octo-reviewer",
			},
			.Snapshot   = core::provider_snapshot_id{ 55 },
			.Repository = {
				.Repository    = core::repository_id{ 2001 },
				.Node          = application::github_node_id{ "R_fixture_repository" },
				.Owner         = "octo-org",
				.Name          = "review-repository",
				.DefaultBranch = "main",
				.Private       = false,
			},
			.PullRequest = {
				.PullRequest = core::pull_request_id{ 3001 },
				.Repository  = core::repository_id{ 2001 },
				.Number      = core::pull_request_number{ 17 },
				.Node        = application::github_node_id{ "PR_fixture_17" },
				.Title       = "Keep review snapshots deterministic",
				.Body        = "This synthetic pull request exercises the "
							   "normalized read contract.",
				.State       = application::github_pull_request_state::open,
				.Draft       = false,
				.Author      = application::github_actor_record{
					.Actor = application::github_actor_id{ "U_fixture_author" },
					.Login = "octo-author",
				},
				.BaseOid = application::commit_oid{ std::string{ base_oid } },
				.HeadOid = application::commit_oid{ std::string{ head_oid } },
				.BaseRef = "main",
				.HeadRef = "fixture/deterministic-snapshots",
			},
			.Files = {
				{
					.Path         = "review.cpp",
					.PreviousPath = std::nullopt,
					.Change       = application::github_file_change::modified,
					.Additions    = 12,
					.Deletions    = 3,
					.ContentKind  = application::github_file_content_kind::unknown,
					.ViewedState  = application::github_file_viewed_state::viewed,
				},
			},
			.Commits = {
				{
					.Oid    = application::commit_oid{ std::string{ head_oid } },
					.Node   = application::github_node_id{ "C_fixture_head" },
					.Author = application::github_actor_record{
						.Actor = application::github_actor_id{ "U_fixture_author" },
						.Login = "octo-author",
					},
					.Headline = "Keep review snapshots deterministic",
				},
			},
			.Reviews = {
				{
					.DatabaseId = 4001,
					.Review     = application::github_review_id{ "PRR_fixture_1" },
					.Author     = application::github_actor_record{
						.Actor = application::github_actor_id{ "U_fixture_viewer" },
						.Login = "octo-reviewer",
					},
					.State  = application::github_review_state::changes_requested,
					.Commit = application::commit_oid{ std::string{ head_oid } },
					.Body   = "Please preserve snapshot identity across "
							  "refreshes.",
				},
			},
			.ReviewThreadsAvailability = application::github_collection_availability::available,
			.ReviewThreads             = {
				{
					.Thread                    = application::github_thread_id{ "PRRT_fixture_1" },
					.Subject                   = application::github_review_thread_subject::line,
					.Path                      = "review.cpp",
					.Line                      = 42,
					.OriginalLine              = 42,
					.DiffSide                  = application::github_diff_side::right,
					.Resolved                  = false,
					.Outdated                  = false,
					.ViewerResolvePermission   = application::github_permission_state::allowed,
					.ViewerUnresolvePermission = application::github_permission_state::denied,
					.Comments                  = {
						{
							.Comment = application::github_comment_id{ "PRRC_fixture_1" },
							.ReplyTo = std::nullopt,
							.Author  = application::github_actor_record{
								.Actor = application::github_actor_id{ "U_fixture_viewer" },
								.Login = "octo-reviewer",
							},
							.Body         = "Please preserve snapshot identity "
											"across refreshes.",
							.Path         = "review.cpp",
							.Line         = 42,
							.OriginalLine = 42,
							.Outdated     = false,
							.Deleted      = false,
						},
					},
				},
				{
					.Thread                    = application::github_thread_id{ "PRRT_fixture_3" },
					.Subject                   = application::github_review_thread_subject::line,
					.Path                      = "review.cpp",
					.Line                      = 60,
					.OriginalLine              = 60,
					.DiffSide                  = application::github_diff_side::right,
					.Resolved                  = false,
					.Outdated                  = false,
					.ViewerResolvePermission   = application::github_permission_state::allowed,
					.ViewerUnresolvePermission = application::github_permission_state::denied,
					.Comments                  = {
						{
							.Comment = application::github_comment_id{ "PRRC_fixture_4" },
							.ReplyTo = std::nullopt,
							.Author  = application::github_actor_record{
								.Actor = application::github_actor_id{ "U_fixture_viewer" },
								.Login = "octo-reviewer",
							},
							.Body         = "Keep the second current-head anchor exact.",
							.Path         = "review.cpp",
							.Line         = 60,
							.OriginalLine = 60,
							.Outdated     = false,
							.Deleted      = false,
						},
						{
							.Comment = application::github_comment_id{ "PRRC_fixture_5" },
							.ReplyTo = application::github_comment_id{ "PRRC_fixture_4" },
							.Author  = application::github_actor_record{
								.Actor = application::github_actor_id{ "U_fixture_author" },
								.Login = "octo-author",
							},
							.Body         = "The exact current-head follow-up is ready to "
											"verify.",
							.Path         = "review.cpp",
							.Line         = 60,
							.OriginalLine = 60,
							.Outdated     = false,
							.Deleted      = false,
						},
					},
				},
				{
					.Thread                    = application::github_thread_id{ "PRRT_fixture_2" },
					.Subject                   = application::github_review_thread_subject::line,
					.Path                      = "review.cpp",
					.Line                      = std::nullopt,
					.OriginalLine              = 84,
					.DiffSide                  = application::github_diff_side::left,
					.Resolved                  = false,
					.Outdated                  = true,
					.ViewerResolvePermission   = application::github_permission_state::denied,
					.ViewerUnresolvePermission = application::github_permission_state::denied,
					.Comments                  = {
						{
							.Comment = application::github_comment_id{ "PRRC_fixture_2" },
							.ReplyTo = std::nullopt,
							.Author  = application::github_actor_record{
								.Actor = application::github_actor_id{ "U_fixture_viewer" },
								.Login = "octo-reviewer",
							},
							.Body         = "Please keep the retained anchor visible.",
							.Path         = "review.cpp",
							.Line         = 84,
							.OriginalLine = 84,
							.Outdated     = true,
							.Deleted      = false,
						},
						{
							.Comment = application::github_comment_id{ "PRRC_fixture_3" },
							.ReplyTo = application::github_comment_id{ "PRRC_fixture_2" },
							.Author  = application::github_actor_record{
								.Actor = application::github_actor_id{ "U_fixture_author" },
								.Login = "octo-author",
							},
							.Body         = "Fixed in the current fixture head.",
							.Path         = "review.cpp",
							.Line         = 84,
							.OriginalLine = 84,
							.Outdated     = true,
							.Deleted      = false,
						},
					},
				},
				{
					.Thread                    = application::github_thread_id{ "PRRT_fixture_file" },
					.Subject                   = application::github_review_thread_subject::file,
					.Path                      = "review.cpp",
					.Line                      = std::nullopt,
					.OriginalLine              = std::nullopt,
					.DiffSide                  = application::github_diff_side::right,
					.Resolved                  = false,
					.Outdated                  = false,
					.ViewerResolvePermission   = application::github_permission_state::allowed,
					.ViewerUnresolvePermission = application::github_permission_state::denied,
					.Comments                  = {
						{
							.Comment = application::github_comment_id{ "PRRC_fixture_file" },
							.ReplyTo = std::nullopt,
							.Author  = application::github_actor_record{
								.Actor = application::github_actor_id{ "U_fixture_viewer" },
								.Login = "octo-reviewer",
							},
							.Body         = "Please review the file-level design.",
							.Path         = "review.cpp",
							.Line         = std::nullopt,
							.OriginalLine = std::nullopt,
							.Outdated     = false,
							.Deleted      = false,
						},
					},
				},
			},
			.IssueComments = {
				{
					.Comment = application::github_comment_id{ "IC_fixture_1" },
					.Author  = application::github_actor_record{
						.Actor = application::github_actor_id{ "U_fixture_author" },
						.Login = "octo-author",
					},
					.Body    = "Fixture follow-up: validation is complete.",
					.Deleted = false,
				},
			},
			.Checks = {
				{
					.Check       = application::github_check_id{ "CR_fixture_1" },
					.Name        = "Local verification",
					.Status      = application::github_check_status::completed,
					.Conclusion  = application::github_check_conclusion::success,
					.Producer    = { },
					.Description = { },
					.DetailsUrl  = { },
				},
			},
			.SearchResults = {
				{
					.Repository      = core::repository_id{ 2001 },
					.PullRequestNode = application::github_node_id{ "PR_fixture_17" },
					.Number          = core::pull_request_number{ 17 },
					.Title           = "Keep review snapshots deterministic",
					.State           = application::github_pull_request_state::open,
					.Draft           = false,
				},
			},
			.SearchAvailability = application::github_search_availability::available,
			.CapabilityEvidence = evidence,
			.RateLimits         = rateLimits,
		};
		return result;
	}

	[[nodiscard]] auto execute_recording(
		loaded_recording      aRecording,
		const server_contract aServer) -> normalized_github_read
	{
		const auto compiledGraphql = compile_read_request(
			semantic_read_request{
				.Server            = aServer,
				.Operation         = read_operation::list_review_threads,
				.Owner             = "octo-org",
				.Repository        = "review-repository",
				.PullRequestNumber = 17,
				.HeadOid           = { },
				.SearchTerms       = { },
				.Page              = 1,
				.GraphqlPage       = github_graphql_page_request{
					.Kind  = github_graphql_page_kind::initial,
					.Shape = {
						.ReviewThreads       = true,
						.ViewedFileState     = true,
						.ResolvePermission   = true,
						.UnresolvePermission = true,
					},
					.AfterCursor = { },
					.ThreadId    = { },
				},
			});
		expect(compiledGraphql.has_value(), "fixture GraphQL request compiles");
		expect(
			compiledGraphql->Body
				== aRecording.Exchanges[6].Request.Body,
			"compiled GraphQL body matches the manifest exactly");
		strict_recorded_transport transport{ std::move(aRecording.Exchanges) };
		const auto                result = perform_normalized_github_read(
			transport,
			normalization_context(aServer));
		if (!result.has_value()) {
			std::cerr << "normalization failure code: "
					  << static_cast<int>(result.error().Code) << '\n';
		}
		expect(result.has_value(), "recorded provider facade normalizes");
		expect(transport.consumed_all(), "all ten manifest reads are consumed");
		return *result;
	}

	auto test_manifest_catalog_and_normalization_are_one_contract() -> void
	{
		const auto cloud = execute_recording(
			load_recording("github-com", server_contract::github_com),
			server_contract::github_com);
		const auto enterprise = execute_recording(
			load_recording("ghes-3.18", server_contract::ghes_3_18),
			server_contract::ghes_3_18);
		const auto expected               = canonical_normalized_read();
		const auto hasExactReviewIdentity = [](const auto& aRead) {
			return aRead.PullRequest.Reviews.size() == 1U
				&& aRead.PullRequest.Reviews.front().DatabaseId == 4001U;
		};
		expect(
			hasExactReviewIdentity(cloud)
				&& hasExactReviewIdentity(enterprise)
				&& hasExactReviewIdentity(expected),
			"paired host recordings and independent facts retain review identity");
		expect(cloud == expected, "GitHub.com matches independent C++ facts");
		expect(enterprise == expected, "GHES 3.18 matches independent C++ facts");
		expect(cloud == enterprise, "paired hosts normalize identically");
	}

	[[nodiscard]] auto semantic(
		const read_operation  aOperation,
		const server_contract aServer = server_contract::github_com)
		-> semantic_read_request
	{
		semantic_read_request request{
			.Server            = aServer,
			.Operation         = aOperation,
			.Owner             = { },
			.Repository        = { },
			.PullRequestNumber = 0U,
			.HeadOid           = { },
			.SearchTerms       = { },
			.Page              = 1,
			.GraphqlPage       = std::nullopt,
		};
		if (aOperation != read_operation::get_viewer_identity) {
			request.Owner      = "octo-org";
			request.Repository = "review-repository";
		}
		switch (aOperation) {
		case read_operation::get_pull_request:
		case read_operation::list_pull_request_files:
		case read_operation::list_pull_request_commits:
		case read_operation::list_reviews:
		case read_operation::list_review_threads:
		case read_operation::list_issue_comments:
		case read_operation::list_pull_request_timeline:
		case read_operation::list_review_comments:
		case read_operation::list_review_requests:
		case read_operation::get_merge_assessment:
		case read_operation::list_requested_reviewers:
			request.PullRequestNumber = 17;
			break;
		case read_operation::get_viewer_identity:
		case read_operation::get_repository:
		case read_operation::list_commit_statuses:
		case read_operation::list_checks:
		case read_operation::search_pull_requests:
			break;
		case read_operation::list_branch_rules:
		case read_operation::get_branch_protection:
			break;
		case read_operation::search_personal_pull_requests:
			request.SearchTerms = "author:octo-reviewer";
			break;
		case read_operation::get_merge_base:
			request.BaseOid = "1111111111111111111111111111111111111111";
			break;
		case read_operation::resolve_body_images:
			request.PullRequestNumber = 17;
			request.Nodes             = { "PR_kwDOA", "IC_kwDOB" };
			break;
		case read_operation::get_team_membership:
			request.Organization = "octo-org";
			request.Team         = "reviewers";
			request.Login        = "octo-reviewer";
			break;
		}
		if (aOperation == read_operation::list_branch_rules || aOperation == read_operation::get_branch_protection) {
			request.BranchRef = "main";
		}
		if (aOperation == read_operation::list_checks || aOperation == read_operation::list_commit_statuses || aOperation == read_operation::get_merge_base) {
			request.HeadOid = std::string{ head_oid };
		}
		if (aOperation == read_operation::search_pull_requests) {
			request.SearchTerms = "is:open";
		}
		return request;
	}

	[[nodiscard]] auto paged_graphql_semantic(
		const github_graphql_page_kind       aKind,
		const std::uint32_t                  aPage,
		const application::github_read_shape aShape,
		std::string                          aAfterCursor = { },
		std::string                          aThreadId    = { }) -> semantic_read_request
	{
		auto request        = semantic(read_operation::list_review_threads);
		request.Page        = aPage;
		request.GraphqlPage = github_graphql_page_request{
			.Kind        = aKind,
			.Shape       = aShape,
			.AfterCursor = std::move(aAfterCursor),
			.ThreadId    = std::move(aThreadId),
		};
		return request;
	}

	[[nodiscard]] auto graphql_request_shapes()
		-> std::vector<semantic_read_request>
	{
		return {
			semantic(read_operation::list_review_threads),
			paged_graphql_semantic(
				github_graphql_page_kind::initial,
				1U,
				{
					.ReviewThreads       = true,
					.ViewedFileState     = true,
					.ResolvePermission   = true,
					.UnresolvePermission = true,
				}),
			paged_graphql_semantic(
				github_graphql_page_kind::files,
				2U,
				{
					.ReviewThreads       = false,
					.ViewedFileState     = true,
					.ResolvePermission   = false,
					.UnresolvePermission = false,
				},
				"fixture-files-cursor"),
			paged_graphql_semantic(
				github_graphql_page_kind::threads,
				2U,
				{
					.ReviewThreads       = true,
					.ViewedFileState     = false,
					.ResolvePermission   = true,
					.UnresolvePermission = true,
				},
				"fixture-threads-cursor"),
			paged_graphql_semantic(
				github_graphql_page_kind::comments,
				2U,
				{
					.ReviewThreads       = true,
					.ViewedFileState     = false,
					.ResolvePermission   = false,
					.UnresolvePermission = false,
				},
				"fixture-comments-cursor",
				"PRRT_fixture_thread"),
		};
	}

	struct query_json
	{
		std::string Query;
		struct glaze
		{
			static constexpr auto value = glz::object("query", &query_json::Query);
		};
	};
	[[nodiscard]] auto graphql_query(const wire_request& aRequest) -> std::string
	{
		query_json envelope;
		expect(!glz::read<fixture_json_options{ }>(envelope, aRequest.Body), "compiled GraphQL body exposes one query string");
		return envelope.Query;
	}
	[[nodiscard]] auto with_graphql_query(wire_request aRequest, const std::string_view aQuery) -> wire_request
	{
		std::map<std::string, glz::raw_json, std::less<>> envelope;
		expect(!glz::read<fixture_json_options{ }>(envelope, aRequest.Body), "hostile GraphQL base body is valid JSON");
		expect(!glz::write<fixture_json_options{ }>(aQuery, envelope["query"].str), "hostile query string serializes");
		expect(!glz::write<fixture_json_options{ }>(envelope, aRequest.Body), "hostile query envelope serializes");
		return aRequest;
	}

	auto expect_graphql_failure(
		const wire_request&         aRequest,
		const contract_failure_code aExpected,
		const std::string_view      aMessage) -> void
	{
		const auto result = validate_outbound_read_request(aRequest);
		expect(
			!result.has_value() && result.error().Code == aExpected,
			aMessage);
	}

	[[nodiscard]] auto nested_query(const std::size_t aDepth) -> std::string
	{
		auto query = std::string{ "query Deep" };
		for (std::size_t index = 0U; index < aDepth; ++index) {
			query += "{field";
		}
		query.append(aDepth, '}');
		return query;
	}

	[[nodiscard]] auto query_with_definitions(
		const std::size_t aDefinitions) -> std::string
	{
		auto query = std::string{ "query Safe{name}" };
		for (std::size_t index = 1U; index < aDefinitions; ++index) {
			query += " fragment F";
			query += std::to_string(index);
			query += " on Type{name}";
		}
		return query;
	}

	auto test_read_only_protocol_policy() -> void
	{
		const auto rest = compile_read_request(
			semantic(read_operation::get_repository));
		expect(rest.has_value(), "raw-method REST fixture compiles");

		constexpr std::array rawMethods{
			std::string_view{ "GET" },
			std::string_view{ "HEAD" },
			std::string_view{ "POST" },
			std::string_view{ "PUT" },
			std::string_view{ "PATCH" },
			std::string_view{ "DELETE" },
			std::string_view{ "OPTIONS" },
			std::string_view{ "TRACE" },
			std::string_view{ "CONNECT" },
			std::string_view{ "UNKNOWN" },
			std::string_view{ "get" },
			std::string_view{ "post" },
			std::string_view{ "PoSt" },
			std::string_view{ "" },
			std::string_view{ " GET" },
			std::string_view{ "GET " },
		};
		for (const auto rawMethod : rawMethods) {
			const auto parsed   = parse_recorded_http_method(rawMethod);
			auto       accepted = false;
			if (parsed.has_value()) {
				auto candidate   = *rest;
				candidate.Method = *parsed;
				accepted         = validate_outbound_read_request(candidate).has_value();
			}
			expect(
				accepted == (rawMethod == "GET"),
				"raw REST sink accepts only exact GET");
			expect(
				parsed.has_value()
					== (rawMethod == "GET" || rawMethod == "POST"),
				"recording method parser accepts only exact GET or POST");
		}
		expect(
			parse_recorded_http_method("GET") == http_method::get
				&& parse_recorded_http_method("POST") == http_method::post,
			"recording methods retain their exact typed meaning");

		using policy = graphql_operation_policy_result;
		expect(
			inspect_graphql_operation_policy(
				"# mutation is comment text\n"
				"query mutation($value:String=\"mutation\"){"
				"mutationType probe(value:\"mutation\"){name}} "
				"fragment MutationFields on __Type{name}")
				== policy::query_only,
			"comments, strings, field names, operation names, and fragments do "
			"not create write operations");
		expect(
			inspect_graphql_operation_policy(
				"query Safe($value:String=\"\"\"mutation\"\"\"){"
				"probe(value:$value){name}}")
				== policy::query_only,
			"block strings containing mutation remain query data");
		expect(
			inspect_graphql_operation_policy(
				"query Safe{probe(integer:-17,zero:0,float:1.25,"
				"exponent:-2.5e+3){name}}")
				== policy::query_only,
			"valid integer, float, and exponent values remain query data");
		expect(
			inspect_graphql_operation_policy(
				"mutation Unsafe{markFileAsViewed(input:{}){clientMutationId}}")
					== policy::write_operation
				&& inspect_graphql_operation_policy(
					   "subscription Unsafe{viewer{login}}")
					== policy::write_operation
				&& inspect_graphql_operation_policy(
					   "query Safe{viewer{login}} "
					   "mutation Hidden{__typename}")
					== policy::write_operation,
			"top-level mutation, subscription, and hidden writes are rejected");
		expect(
			inspect_graphql_operation_policy(
				"query First{viewer{login}} query Second{__typename}")
				== policy::multiple_operations,
			"multiple read operations fail closed without operation selection");
		expect(
			inspect_graphql_operation_policy("query Broken{viewer{login}")
					== policy::malformed
				&& inspect_graphql_operation_policy(
					   "query Broken{probe(value:\"unterminated){name}}")
					== policy::malformed
				&& inspect_graphql_operation_policy(
					   "query Safe{viewer{login}} trailing")
					== policy::malformed,
			"unbalanced, unterminated, and trailing definitions fail closed");
		constexpr std::array malformedNumericQueries{
			std::string_view{ "query Broken{probe(value:-){name}}" },
			std::string_view{ "query Broken{probe(value:01){name}}" },
			std::string_view{ "query Broken{probe(value:1.){name}}" },
			std::string_view{ "query Broken{probe(value:1e+){name}}" },
			std::string_view{ "query Broken{probe(value:1name){name}}" },
		};
		for (const auto query : malformedNumericQueries) {
			expect(
				inspect_graphql_operation_policy(query) == policy::malformed,
				"malformed GraphQL numeric syntax fails closed");
		}

		auto oversizedQuery = std::string{ "query Oversized{" };
		oversizedQuery.append(
			maximum_graphql_operation_document_bytes,
			'a');
		oversizedQuery.push_back('}');
		expect(
			inspect_graphql_operation_policy(oversizedQuery)
					== policy::response_limit
				&& inspect_graphql_operation_policy(
					   nested_query(maximum_graphql_operation_nesting))
					== policy::query_only
				&& inspect_graphql_operation_policy(
					   nested_query(maximum_graphql_operation_nesting + 1U))
					== policy::response_limit
				&& inspect_graphql_operation_policy(
					   query_with_definitions(
						   maximum_graphql_document_definitions))
					== policy::query_only
				&& inspect_graphql_operation_policy(
					   query_with_definitions(
						   maximum_graphql_document_definitions + 1U))
					== policy::response_limit,
			"GraphQL operation policy bounds bytes, nesting, and definitions");

		for (const auto& semanticRequest : graphql_request_shapes()) {
			const auto compiled = compile_read_request(semanticRequest);
			expect(
				compiled.has_value(),
				"every legacy and paged GraphQL request shape compiles");
			const auto query = graphql_query(*compiled);
			expect(
				inspect_graphql_operation_policy(query)
					== policy::query_only,
				"every compiled GraphQL request shape is exactly one query");

			auto alteredRead = query;
			expect(
				!alteredRead.empty() && alteredRead.back() == '}',
				"compiled GraphQL query has an outer selection terminator");
			alteredRead.insert(
				alteredRead.size() - 1U,
				" __typename");
			expect(
				inspect_graphql_operation_policy(alteredRead)
					== policy::query_only,
				"noncanonical read remains exactly one scanner-approved query");
			expect_graphql_failure(
				with_graphql_query(*compiled, alteredRead),
				contract_failure_code::unexpected_body,
				"canonical body equality rejects a scanner-approved altered "
				"selection");

			auto mutation = query;
			mutation.replace(0U, std::string_view{ "query" }.size(), "mutation");
			expect_graphql_failure(
				with_graphql_query(*compiled, mutation),
				contract_failure_code::mutation_operation,
				"every GraphQL request shape rejects a mutation operation");

			auto subscription = query;
			subscription.replace(
				0U,
				std::string_view{ "query" }.size(),
				"subscription");
			expect_graphql_failure(
				with_graphql_query(*compiled, subscription),
				contract_failure_code::mutation_operation,
				"every GraphQL request shape rejects a subscription operation");
			expect_graphql_failure(
				with_graphql_query(
					*compiled,
					query + " mutation Hidden{__typename}"),
				contract_failure_code::mutation_operation,
				"every GraphQL request shape rejects a hidden mutation");
			expect_graphql_failure(
				with_graphql_query(
					*compiled,
					query + " query Duplicate{__typename}"),
				contract_failure_code::unexpected_body,
				"every GraphQL request shape rejects multiple operations");

			auto malformed = query;
			malformed.pop_back();
			expect_graphql_failure(
				with_graphql_query(*compiled, malformed),
				contract_failure_code::unexpected_body,
				"every GraphQL request shape rejects malformed structure");
		}

		const auto legacy = compile_read_request(
			semantic(read_operation::list_review_threads));
		expect(legacy.has_value(), "oversized GraphQL base request compiles");
		expect_graphql_failure(
			with_graphql_query(*legacy, oversizedQuery),
			contract_failure_code::response_limit,
			"outbound GraphQL operation policy exposes its document bound");
	}

	auto test_glaze_read_request_bytes_and_raw_validation() -> void
	{
		std::string printableToken;
		for (char value = '!'; value <= '~'; ++value) {
			printableToken += value;
		}
		for (const auto server : { server_contract::github_com, server_contract::ghes_3_18 }) {
			std::size_t shapeIndex = 0U;
			for (auto request : graphql_request_shapes()) {
				request.Server            = server;
				request.PullRequestNumber = core::maximum_pull_request_number;
				if (request.GraphqlPage && request.GraphqlPage->Kind != github_graphql_page_kind::initial) {
					request.GraphqlPage->AfterCursor = printableToken;
					if (request.GraphqlPage->Kind == github_graphql_page_kind::comments) {
						request.GraphqlPage->ThreadId = printableToken;
					}
				}
				const auto compiled = compile_read_request(request);
				expect(compiled.has_value(), "bounded printable tokens and maximum PR number compile for every request shape and host");
				const auto expected = tests::canonical_json::read_requests.at(shapeIndex++);
				expect(compiled->Body == expected, "Glaze preserves canonical member order, optional omissions, integer bytes and printable token escaping");
			}
		}
		const auto compiled = compile_read_request(semantic(read_operation::list_review_threads));
		expect(compiled.has_value(), "raw request validation base compiles");
		for (const auto body : {
				 R"({"query":"query Safe{viewer{login}}","variables":{"owner":"org","name":"repo","number":17.0000000000000001}})",
				 R"({"query":"query Safe{viewer{login}}","variables":{"owner":"org","name":"repo","number":4294967296}})",
				 R"({"query":"query Safe{viewer{login}}","variables":{"owner":"org","name":"repo","number":17,"extra":"\ud800"}})",
				 R"({"query":"query Safe{viewer{login}}","variables":{"owner":"org","name":"repo","number":17,"extra":"\udc00"}})",
				 "{\"query\":\"query Safe{viewer{login}}\",\"variables\":{\"extra\":\"\xc3\x28\"}}",
				 R"({"query":"query Safe{viewer{login}}","variables":null})",
				 R"({"query":"query Safe{viewer{login}}"})",
				 R"({"variables":{}})",
				 R"({"query":"query Safe{viewer{login}}","variables":{}} {})",
				 R"({"query":"query Safe{viewer{login}}","query":"query Duplicate{viewer{login}}","variables":{}})" }) {
			auto hostile = *compiled;
			hostile.Body = body;
			expect_graphql_failure(hostile, contract_failure_code::unexpected_body, "invalid raw request JSON cannot cross outbound admission");
		}
	}

	auto test_read_only_catalog_and_outbound_validation() -> void
	{
		expect(
			read_operation_catalog.size() == 10U,
			"catalog has exactly ten reads");
		// The recorded manifest catalogue and the set of compiled operations
		// are different things and this is where they are kept apart: the
		// eleventh operation exists, routes, and names itself, and it is
		// deliberately outside the ten-interaction recording so the
		// authenticated wire shape is unchanged by its arrival.
		expect(
			all_read_operations.size() == 22U
				&& std::ranges::find(read_operation_catalog,
									 read_operation::list_review_comments)
					== read_operation_catalog.end()
				&& std::ranges::find(all_read_operations,
									 read_operation::list_review_comments)
					!= all_read_operations.end()
				&& std::ranges::find(read_operation_catalog, read_operation::get_merge_base) == read_operation_catalog.end(),
			"the compiled set holds twenty-two reads and the recording holds ten");
		for (const auto operation : all_read_operations) {
			const auto roundTrip = read_operation_from_name(operation_name(operation));
			expect(
				roundTrip.has_value() && *roundTrip == operation,
				"every read name round-trips");
		}
		{
			auto semanticComments = semantic(read_operation::list_review_comments);
			semanticComments.Page = 2U;
			const auto compiled   = compile_read_request(semanticComments);
			expect(
				compiled.has_value()
					&& compiled->Path
						== "/repos/octo-org/review-repository/pulls/17/comments"
					&& compiled->Query == "page=2&per_page=100"
					&& compiled->Method == http_method::get
					&& compiled->Body.empty(),
				"the inline review-comment read is a paged REST GET on the "
				"pull-request comment path");
			const auto issue = compile_read_request(
				semantic(read_operation::list_issue_comments));
			expect(
				issue.has_value()
					&& issue->Path
						== "/repos/octo-org/review-repository/issues/17/comments",
				"the issue-level conversation keeps its own distinct route");
		}
		{
			const auto anonymous = anonymous_read_admission(
				application::github_viewer_record{
					.Account     = core::github_account_id{ 1001U },
					.Node        = application::github_actor_id{ "U_viewer" },
					.Login       = "octo-reviewer",
					.DisplayName = "Octo Reviewer",
				});
			expect(
				anonymous.Operations.size() == 9U
					&& anonymous.Shape == application::github_read_shape{ }
					&& anonymous.Viewer.has_value()
					&& std::ranges::none_of(
						anonymous.Operations,
						[](const read_operation aOperation) {
							return aOperation
								== read_operation::get_viewer_identity
								|| aOperation
								== read_operation::list_review_threads;
						}),
				"the signed-out admission is nine operations with an all-false "
				"shape and no viewer operation");
			expect(
				authenticated_read_admission().Operations.size() == 10U
					&& authenticated_read_admission().Shape.ReviewThreads
					&& !authenticated_read_admission().Viewer.has_value(),
				"the default admission is still today's recorded behaviour");
		}
		for (const auto mutation :
			 {
				 "MarkFileViewed",
				 "MarkFileUnviewed",
				 "CreateReview",
				 "SubmitReview",
				 "CreateThread",
				 "ReplyToThread",
				 "ResolveThread",
				 "UnresolveThread",
				 "UpdateReviewComment",
			 }) {
			const auto result = read_operation_from_name(mutation);
			expect(
				!result.has_value()
					&& result.error().Code
						== contract_failure_code::mutation_operation,
				"mutation operation is explicitly rejected");
		}

		const auto rest = compile_read_request(
			semantic(read_operation::get_repository));
		expect(rest.has_value(), "REST request compiles");
		expect(
			rest->Headers
				== std::vector<http_header>{
					{ "Accept", "application/vnd.github+json" },
					{ "User-Agent", "BetterPullReview/0.1" },
					{ "X-GitHub-Api-Version", "2022-11-28" },
				},
			"REST request uses exact safe headers");

		const auto graphql = compile_read_request(
			semantic(read_operation::list_review_threads));
		expect(graphql.has_value(), "GraphQL request compiles");
		expect(
			graphql->Headers
				== std::vector<http_header>{
					{ "Accept", "application/json" },
					{ "Content-Type", "application/json" },
					{ "User-Agent", "BetterPullReview/0.1" },
				},
			"GraphQL does not claim REST versioning");

		auto hostile = *rest;
		hostile.Path = "/user";
		expect(
			!validate_outbound_read_request(hostile).has_value(),
			"operation-specific path drift is rejected");
		hostile       = *rest;
		hostile.Query = "page=2";
		expect(
			!validate_outbound_read_request(hostile).has_value(),
			"unexpected query is rejected");
		hostile = *rest;
		hostile.Headers.push_back({ "X-Injected", "value" });
		expect(
			!validate_outbound_read_request(hostile).has_value(),
			"extra header is rejected");
		hostile        = *rest;
		hostile.Method = http_method::post;
		expect(
			!validate_outbound_read_request(hostile).has_value(),
			"REST write method is rejected");

		auto injectedSearch        = semantic(read_operation::search_pull_requests);
		injectedSearch.SearchTerms = "repo:other is:issue";
		expect(
			!compile_read_request(injectedSearch).has_value(),
			"structured search cannot inject repository or item kind");

		auto oversizedViewer = semantic(read_operation::get_viewer_identity);
		oversizedViewer.Owner.assign(1'000'000U, 'a');
		expect(
			!compile_read_request(oversizedViewer).has_value(),
			"irrelevant semantic fields are bounded before copying");

		auto aliasedRepository = semantic(read_operation::get_repository);
		aliasedRepository.Page = 2;
		expect(
			!compile_read_request(aliasedRepository).has_value(),
			"nonpaged REST reads have exactly one semantic page identity");

		auto aliasedGraphql = semantic(read_operation::list_review_threads);
		aliasedGraphql.Page = 2;
		expect(
			!compile_read_request(aliasedGraphql).has_value(),
			"uncursored GraphQL reads cannot alias page one");

		auto irrelevantHead    = semantic(read_operation::get_pull_request);
		irrelevantHead.HeadOid = std::string{ head_oid };
		expect(
			!compile_read_request(irrelevantHead).has_value(),
			"irrelevant commit identity cannot alias a PR metadata read");

		auto excessiveSearchPage = semantic(read_operation::search_pull_requests);
		excessiveSearchPage.Page = 11;
		expect(
			!compile_read_request(excessiveSearchPage).has_value(),
			"search page identity stops at GitHub's one-thousand-result cap");

		auto excessiveCommitPage = semantic(read_operation::list_pull_request_commits);
		excessiveCommitPage.Page = 4;
		expect(
			!compile_read_request(excessiveCommitPage).has_value(),
			"commit page identity stops at GitHub's 250-commit cap");

		auto maximumGraphqlNumber              = semantic(read_operation::get_pull_request);
		maximumGraphqlNumber.PullRequestNumber = core::maximum_pull_request_number;
		expect(
			compile_read_request(maximumGraphqlNumber).has_value(),
			"GraphQL-compatible PR number at N compiles");
		maximumGraphqlNumber.PullRequestNumber = core::maximum_pull_request_number + 1U;
		expect(
			!compile_read_request(maximumGraphqlNumber).has_value(),
			"PR number above GraphQL Int range is rejected");
	}

	auto replace_once(
		std::string&           aText,
		const std::string_view aFrom,
		const std::string_view aTo) -> void
	{
		const auto position = aText.find(aFrom);
		expect(position != std::string::npos, "hostile replacement marker exists");
		aText.replace(position, aFrom.size(), aTo);
	}

	auto mark_graphql_page_incomplete(
		std::string&           aBody,
		const std::string_view aCursor) -> void
	{
		const auto cursorPosition = aBody.find(aCursor);
		expect(
			cursorPosition != std::string::npos,
			"targeted GraphQL cursor exists");
		const auto marker         = std::string_view{ "\"hasNextPage\": false" };
		const auto markerPosition = aBody.rfind(marker, cursorPosition);
		expect(
			markerPosition != std::string::npos,
			"targeted GraphQL page marker exists");
		aBody.replace(
			markerPosition,
			marker.size(),
			"\"hasNextPage\": true");
	}

	auto expect_atomic_failure(
		const github_read_normalization_context& aContext,
		const std::vector<operation_response>&   aResponses,
		const std::string_view                   aMessage) -> void
	{
		const auto result = normalize_github_read(aContext, aResponses);
		expect(!result.has_value(), aMessage);
	}

	auto test_hostile_responses_fail_atomically() -> void
	{
		auto       recording = load_recording("github-com", server_contract::github_com);
		const auto context   = normalization_context(server_contract::github_com);

		auto responses = recording.Responses;
		replace_once(
			responses[0].Response.Body,
			"\"id\": 1001,",
			"\"id\": 1001,\n  \"id\": 1001,");
		expect_atomic_failure(
			context,
			responses,
			"duplicate JSON member yields no candidate snapshot");

		responses = recording.Responses;
		replace_once(
			responses[2].Response.Body,
			"\"id\": 3001,",
			"\"id\": 9999,");
		expect_atomic_failure(
			context,
			responses,
			"cross-response pull-request identity mismatch is atomic");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"id\": \"R_fixture_repository\"",
			"\"id\": \"R_replayed_repository\"");
		expect_atomic_failure(
			context,
			responses,
			"GraphQL repository identity is bound to the REST repository");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"id\": \"PR_fixture_17\"",
			"\"id\": \"PR_replayed_18\"");
		expect_atomic_failure(
			context,
			responses,
			"GraphQL pull-request identity is bound to REST metadata");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"headRefOid\": \"2222222222222222222222222222222222222222\"",
			"\"headRefOid\": \"3333333333333333333333333333333333333333\"");
		expect_atomic_failure(
			context,
			responses,
			"force-pushed GraphQL coordinates cannot cross the REST head boundary");

		responses = recording.Responses;
		replace_once(
			responses[9].Response.Body,
			"\"number\": 17",
			"\"number\": 18");
		expect_atomic_failure(
			context,
			responses,
			"search node and repository-scoped PR number cannot disagree");

		responses = recording.Responses;
		replace_once(
			responses[9].Response.Body,
			"\"title\": \"Keep review snapshots deterministic\"",
			"\"title\": \"Contradictory search title\"");
		expect_atomic_failure(
			context,
			responses,
			"search metadata cannot contradict the same pull request");

		responses = recording.Responses;
		replace_once(
			responses[9].Response.Body,
			"\"draft\": false",
			"\"draft\": true");
		expect_atomic_failure(
			context,
			responses,
			"search draft state cannot contradict the same pull request");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"login\": \"octo-reviewer\"",
			"\"login\": \"identity-conflict\"");
		expect_atomic_failure(
			context,
			responses,
			"one actor ID cannot carry conflicting logins");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"bodyText\": \"Please preserve snapshot identity across "
			"refreshes.\",\n"
			"                    \"path\": \"review.cpp\"",
			"\"bodyText\": \"Please preserve snapshot identity across "
			"refreshes.\",\n"
			"                    \"path\": \"src/core/other.cpp\"");
		expect_atomic_failure(
			context,
			responses,
			"review comment path must match its thread");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"path\": \"review.cpp\",\n"
			"              \"viewerViewedState\": \"VIEWED\"",
			"\"path\": \"src/core/other.cpp\",\n"
			"              \"viewerViewedState\": \"VIEWED\"");
		expect_atomic_failure(
			context,
			responses,
			"GraphQL viewed state must correlate to one REST file path");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"viewerViewedState\": \"VIEWED\"",
			"\"viewerViewedState\": \"UNKNOWN\"");
		expect_atomic_failure(
			context,
			responses,
			"unknown viewed-file state is rejected");

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"viewerCanResolve\": true",
			"\"viewerCanResolve\": null");
		expect_atomic_failure(
			context,
			responses,
			"non-boolean per-thread resolve availability is rejected");

		for (const auto& [replacement, message] : {
				 std::pair{ std::string_view{ "" },
							std::string_view{ "missing thread diff side is rejected" } },
				 std::pair{ std::string_view{ "\"diffSide\": null,\n              " },
							std::string_view{ "null thread diff side is rejected" } },
				 std::pair{ std::string_view{ "\"diffSide\": 7,\n              " },
							std::string_view{ "wrong-shaped thread diff side is rejected" } },
				 std::pair{ std::string_view{ "\"diffSide\": \"UNCHANGED\",\n              " },
							std::string_view{ "unknown thread diff side is rejected" } },
			 }) {
			responses = recording.Responses;
			replace_once(
				responses[6].Response.Body,
				"\"diffSide\": \"RIGHT\",\n              ",
				replacement);
			expect_atomic_failure(context, responses, message);
		}

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"line\": 42,\n              \"originalLine\": 42,",
			"\"line\": null,\n              \"originalLine\": 42,");
		expect_atomic_failure(
			context,
			responses,
			"a current line thread cannot substitute originalLine for its missing current coordinate");

		for (const bool outdated : { false, true }) {
			responses = recording.Responses;
			replace_once(
				responses[6].Response.Body,
				"\"line\": 42,\n              \"originalLine\": 42,",
				"\"line\": null,\n              \"originalLine\": null,");
			if (outdated) {
				replace_once(responses[6].Response.Body,
							 "\"isOutdated\": false,",
							 "\"isOutdated\": true,");
			}
			const auto unanchored = normalize_github_read(context, responses);
			expect(unanchored.has_value()
					   && unanchored->PullRequest.ReviewThreads.size() == 4U
					   && unanchored->PullRequest.ReviewThreads.front().Subject
						   == application::github_review_thread_subject::line
					   && !unanchored->PullRequest.ReviewThreads.front().Line
					   && !unanchored->PullRequest.ReviewThreads.front().OriginalLine,
				   "the fixture normalizer preserves a LINE thread with no reported anchor");
		}

		responses = recording.Responses;
		replace_once(
			responses[9].Response.Body,
			"\"state\": \"open\"",
			"\"state\": \"closed\"");
		expect_atomic_failure(
			context,
			responses,
			"open-only search cannot normalize a closed result");

		responses = recording.Responses;
		replace_once(
			responses[9].Response.Body,
			"\"state\": \"open\"",
			"\"state\": \"unknown-provider-state\"");
		expect_atomic_failure(
			context,
			responses,
			"unknown provider enum is rejected");

		for (const auto& [cursor, description] : {
				 std::pair{
					 std::string_view{ "Y3Vyc29yOlBSQ0ZfZml4dHVyZV8x" },
					 std::string_view{ "viewed-file" },
				 },
				 std::pair{
					 std::string_view{ "Y3Vyc29yOlBSUkNfZml4dHVyZV8x" },
					 std::string_view{ "thread-comment" },
				 },
				 std::pair{
					 std::string_view{ "Y3Vyc29yOlBSUlRfZml4dHVyZV8x" },
					 std::string_view{ "review-thread" },
				 },
			 }) {
			responses = recording.Responses;
			mark_graphql_page_incomplete(
				responses[6].Response.Body,
				cursor);
			const auto strict = validate_strict_fixture_response(
				read_operation::list_review_threads,
				responses[6].Response);
			expect(
				!strict.has_value()
					&& strict.error().Code
						== contract_failure_code::response_limit,
				"each incomplete GraphQL connection is classified as a limit");
			expect_atomic_failure(
				context,
				responses,
				std::string{ "incomplete " } + std::string{ description }
					+ " GraphQL page cannot become a snapshot");
		}

		responses = recording.Responses;
		replace_once(
			responses[6].Response.Body,
			"\"line\": 42",
			"\"line\": 2147483648");
		expect_atomic_failure(
			context,
			responses,
			"GraphQL line above signed Int range is rejected");

		responses               = recording.Responses;
		auto&      fileBody     = responses[3].Response.Body;
		const auto originalFile = fileBody.substr(
			fileBody.find('{'),
			fileBody.rfind('}') - fileBody.find('{') + 1U);
		fileBody = "[";
		for (std::size_t index = 0; index < 101U; ++index) {
			if (index != 0U) {
				fileBody += ',';
			}
			fileBody += originalFile;
		}
		fileBody += ']';
		expect_atomic_failure(
			context,
			responses,
			"page at N+1 is rejected before DOM allocation");

		responses                          = recording.Responses;
		responses[3].Response.Completeness = page_completeness::has_next_page;
		expect_atomic_failure(
			context,
			responses,
			"transport-declared REST continuation cannot become a snapshot");

		responses                          = recording.Responses;
		responses[3].Response.Completeness = page_completeness::unknown;
		expect_atomic_failure(
			context,
			responses,
			"response with unknown page completeness fails closed");

		responses                          = recording.Responses;
		responses[3].Response.Completeness = static_cast<page_completeness>(255);
		expect_atomic_failure(
			context,
			responses,
			"malformed page completeness fails closed");

		responses = recording.Responses;
		replace_once(
			responses[5].Response.Body,
			"\"node_id\": \"PRR_fixture_1\"",
			"\"node_id\": \"R_fixture_repository\"");
		expect_atomic_failure(
			context,
			responses,
			"GitHub node identity cannot collide across entity kinds");

		auto mismatchedContext    = context;
		mismatchedContext.Account = core::github_account_id{ 9999 };
		expect_atomic_failure(
			mismatchedContext,
			recording.Responses,
			"redundant authority mismatch cannot launder provider data");

		auto aliasedSearchContext        = context;
		aliasedSearchContext.SearchTerms = "is:closed";
		expect_atomic_failure(
			aliasedSearchContext,
			recording.Responses,
			"normalization context cannot alias the selected open search");

		responses = recording.Responses;
		responses.push_back(responses.front());
		expect_atomic_failure(
			context,
			responses,
			"extra operation response is rejected");
	}

	auto test_provider_valid_nullable_and_conditional_shapes() -> void
	{
		auto recording = load_recording("github-com", server_contract::github_com);

		expect(
			validate_strict_fixture_response(
				read_operation::list_review_threads,
				recording.Responses[6].Response)
				.has_value(),
			"nonempty final GraphQL pages retain non-null cursors");

		wire_response emptyThreads{
			.Status       = 200,
			.Headers      = { { "Content-Type", "application/json" } },
			.Body         = R"({"data":{"repository":{"id":"R_fixture_repository","pullRequest":{"id":"PR_fixture_17","headRefOid":"2222222222222222222222222222222222222222","files":{"totalCount":0,"nodes":[],"pageInfo":{"hasNextPage":false,"endCursor":null}},"reviewThreads":{"totalCount":0,"nodes":[],"pageInfo":{"hasNextPage":false,"endCursor":null}}}}}})",
			.Completeness = page_completeness::complete,
		};
		expect(
			validate_strict_fixture_response(
				read_operation::list_review_threads,
				emptyThreads)
				.has_value(),
			"empty final GraphQL page has a null cursor");

		auto missingFinalCursor = recording.Responses[6].Response;
		replace_once(
			missingFinalCursor.Body,
			"\"endCursor\": \"Y3Vyc29yOlBSUkNfZml4dHVyZV8x\"",
			"\"endCursor\": null");
		expect(
			!validate_strict_fixture_response(
				 read_operation::list_review_threads,
				 missingFinalCursor)
				 .has_value(),
			"nonempty final GraphQL page rejects a missing cursor");

		auto viewer = recording.Responses[0].Response;
		replace_once(
			viewer.Body,
			"\"name\": \"Octo Reviewer\"",
			"\"name\": null");
		expect(
			validate_strict_fixture_response(
				read_operation::get_viewer_identity,
				viewer)
				.has_value(),
			"nullable viewer display name is valid");

		auto pullRequest = recording.Responses[2].Response;
		replace_once(
			pullRequest.Body,
			"\"body\": \"This synthetic pull request exercises the "
			"normalized read contract.\"",
			"\"body\": null");
		expect(
			validate_strict_fixture_response(
				read_operation::get_pull_request,
				pullRequest)
				.has_value(),
			"nullable PR body is valid");

		auto       forkPullRequest = recording.Responses[2].Response;
		const auto headPosition    = forkPullRequest.Body.find("\"head\"");
		expect(headPosition != std::string::npos, "head fixture exists");
		auto repositoryIdPosition = forkPullRequest.Body.find("\"id\": 2001", headPosition);
		expect(
			repositoryIdPosition != std::string::npos,
			"head repository ID exists");
		forkPullRequest.Body.replace(
			repositoryIdPosition,
			std::string_view{ "\"id\": 2001" }.size(),
			"\"id\": 9002");
		auto fullNamePosition = forkPullRequest.Body.find(
			"octo-org/review-repository",
			headPosition);
		expect(
			fullNamePosition != std::string::npos,
			"head repository name exists");
		forkPullRequest.Body.replace(
			fullNamePosition,
			std::string_view{ "octo-org/review-repository" }.size(),
			"octo-fork/review-repository");
		expect(
			validate_strict_fixture_response(
				read_operation::get_pull_request,
				forkPullRequest)
				.has_value(),
			"fork head repository need not equal the base repository");
		recording.Responses[2].Response = std::move(forkPullRequest);
		expect(
			normalize_github_read(
				normalization_context(server_contract::github_com),
				recording.Responses)
				.has_value(),
			"fork head cannot be rejected by base authority correlation");

		auto review = recording.Responses[5].Response;
		replace_once(
			review.Body,
			"\"state\": \"CHANGES_REQUESTED\"",
			"\"state\": \"PENDING\"");
		expect(
			validate_strict_fixture_response(
				read_operation::list_reviews,
				review)
				.has_value(),
			"pending review is a valid provider state");

		for (const auto& [wireState, normalizedState] : {
				 std::pair{
					 std::string_view{ "DISMISSED" },
					 application::github_file_viewed_state::dismissed,
				 },
				 std::pair{
					 std::string_view{ "UNVIEWED" },
					 application::github_file_viewed_state::unviewed,
				 },
				 std::pair{
					 std::string_view{ "VIEWED" },
					 application::github_file_viewed_state::viewed,
				 },
			 }) {
			auto viewedRecording = recording;
			replace_once(
				viewedRecording.Responses[6].Response.Body,
				"\"viewerViewedState\": \"VIEWED\"",
				std::string{ "\"viewerViewedState\": \"" }
					+ std::string{ wireState } + "\"");
			const auto normalized = normalize_github_read(
				normalization_context(server_contract::github_com),
				viewedRecording.Responses);
			expect(
				normalized.has_value()
					&& normalized->PullRequest.Files.front().ViewedState
						== normalizedState,
				"every selected viewed-file state normalizes exactly");
		}

		auto targetAvailability = recording;
		replace_once(
			targetAvailability.Responses[6].Response.Body,
			"\"viewerCanResolve\": true",
			"\"viewerCanResolve\": false");
		replace_once(
			targetAvailability.Responses[6].Response.Body,
			"\"viewerCanUnresolve\": false",
			"\"viewerCanUnresolve\": true");
		const auto normalizedAvailability = normalize_github_read(
			normalization_context(server_contract::github_com),
			targetAvailability.Responses);
		expect(
			normalizedAvailability.has_value()
				&& normalizedAvailability->PullRequest.ReviewThreads.front()
						.ViewerResolvePermission
					== application::github_permission_state::denied
				&& normalizedAvailability->PullRequest.ReviewThreads.front()
						.ViewerUnresolvePermission
					== application::github_permission_state::allowed
				&& normalizedAvailability->PullRequest.CapabilityEvidence
						.Capabilities
					== capabilities(),
			"per-thread availability remains distinct from host capability");

		auto check = recording.Responses[8].Response;
		replace_once(
			check.Body,
			"\"status\": \"completed\"",
			"\"status\": \"queued\"");
		replace_once(
			check.Body,
			"\"conclusion\": \"success\"",
			"\"conclusion\": null");
		expect(
			validate_strict_fixture_response(
				read_operation::list_checks,
				check)
				.has_value(),
			"queued check has nullable completion facts");

		auto file = recording.Responses[3].Response;
		replace_once(
			file.Body,
			"\"status\": \"modified\"",
			"\"status\": \"renamed\",\n"
			"    \"previous_filename\": \"src/core/old_snapshot.cpp\"");
		expect(
			validate_strict_fixture_response(
				read_operation::list_pull_request_files,
				file)
				.has_value(),
			"renamed file accepts an available previous path");

		auto copiedFile = recording.Responses[3].Response;
		replace_once(
			copiedFile.Body,
			"\"status\": \"modified\"",
			"\"status\": \"copied\",\n"
			"    \"previous_filename\": \"src/core/source_snapshot.cpp\"");
		expect(
			validate_strict_fixture_response(
				read_operation::list_pull_request_files,
				copiedFile)
				.has_value(),
			"copied file accepts an available source path");

		auto unchangedFile = recording.Responses[3].Response;
		replace_once(
			unchangedFile.Body,
			"\"status\": \"modified\"",
			"\"status\": \"unchanged\"");
		expect(
			validate_strict_fixture_response(
				read_operation::list_pull_request_files,
				unchangedFile)
				.has_value(),
			"unchanged is a valid GitHub file status");

		// [[BPR-0892]]. GitHub sends `sha`, `changes` and `patch` with every
		// file, and a recorded page may keep them. Each is checked, and nothing
		// else is admitted.
		const auto withGitHubKeys = [&recording](const std::string_view aKeys) {
			auto response = recording.Responses[3].Response;
			replace_once(response.Body, "\"status\": \"modified\"", std::string{ "\"status\": \"modified\",\n" } + std::string{ aKeys });
			return validate_strict_fixture_response(read_operation::list_pull_request_files, response).has_value();
		};
		expect(withGitHubKeys("    \"changes\": 15,\n    \"patch\": \"@@ -1,3 +1,12 @@\\n-old\\n+new\",\n    \"sha\": \"0123456789abcdef0123456789abcdef01234567\""),
			   "a file carrying GitHub's sha, changes and patch is a valid fixture");
		expect(withGitHubKeys("    \"changes\": 15,\n    \"sha\": \"0123456789abcdef0123456789abcdef01234567\""),
			   "a file GitHub sent without a patch is a valid fixture");
		for (const auto& [keys, refusal] : std::array<std::pair<std::string_view, std::string_view>, 5U>{ {
				 { "    \"changes\": 14", "changes that are not additions plus deletions are refused" },
				 { "    \"changes\": \"15\"", "changes that are not an integer are refused" },
				 { "    \"patch\": 7", "a patch that is not a string is refused" },
				 { "    \"sha\": \"not-an-object-id\"", "a sha that is not an object id is refused" },
				 { "    \"blob_url\": \"https://github.com/octo-org/review-repository/blob/head/review.cpp\"", "a key GitHub sends that no fixture may keep is still refused" },
			 } }) {
			expect(!withGitHubKeys(keys), refusal);
		}

		auto nullableCommitRecording = load_recording("github-com", server_contract::github_com);
		replace_once(
			nullableCommitRecording.Responses[5].Response.Body,
			"\"commit_id\": \"2222222222222222222222222222222222222222\"",
			"\"commit_id\": null");
		const auto nullableCommit = normalize_github_read(
			normalization_context(server_contract::github_com),
			nullableCommitRecording.Responses);
		expect(
			nullableCommit.has_value()
				&& !nullableCommit->PullRequest.Reviews.front()
						.Commit.has_value(),
			"a review whose commit was deleted keeps a nullable commit");

		auto emptyMessageRecording = load_recording("github-com", server_contract::github_com);
		replace_once(
			emptyMessageRecording.Responses[4].Response.Body,
			"\"message\": \"Keep review snapshots deterministic\"",
			"\"message\": \"\"");
		const auto emptyMessage = normalize_github_read(
			normalization_context(server_contract::github_com),
			emptyMessageRecording.Responses);
		expect(
			emptyMessage.has_value()
				&& emptyMessage->PullRequest.Commits.front()
					   .Headline.empty(),
			"Git commits may have an empty message");

		auto  fileThreadRecording = load_recording("github-com", server_contract::github_com);
		auto& fileThread          = fileThreadRecording.Responses[6].Response.Body;
		replace_once(
			fileThread,
			"\"subjectType\": \"LINE\"",
			"\"subjectType\": \"FILE\"");
		replace_once(fileThread, "\"line\": 42", "\"line\": null");
		replace_once(
			fileThread,
			"\"originalLine\": 42",
			"\"originalLine\": null");
		replace_once(fileThread, "\"line\": 42", "\"line\": null");
		replace_once(
			fileThread,
			"\"originalLine\": 42",
			"\"originalLine\": null");
		const auto normalizedFileThread = normalize_github_read(
			normalization_context(server_contract::github_com),
			fileThreadRecording.Responses);
		expect(
			normalizedFileThread.has_value()
				&& normalizedFileThread->PullRequest.ReviewThreads.front()
						.Subject
					== application::github_review_thread_subject::file
				&& !normalizedFileThread->PullRequest.ReviewThreads.front()
						.Line.has_value()
				&& !normalizedFileThread->PullRequest.ReviewThreads.front()
						.Comments.front()
						.Line.has_value(),
			"file-level thread and comment normalize without line numbers");

		auto  placeholderRecording = fileThreadRecording;
		auto& placeholderThread    = placeholderRecording.Responses[6].Response.Body;
		replace_once(placeholderThread, "\"line\": null", "\"line\": 1");
		replace_once(placeholderThread, "\"originalLine\": null", "\"originalLine\": 1");
		const auto placeholder = normalize_github_read(
			normalization_context(server_contract::github_com), placeholderRecording.Responses);
		expect(placeholder.has_value()
				   && placeholder->PullRequest.ReviewThreads.front().Subject == application::github_review_thread_subject::file
				   && !placeholder->PullRequest.ReviewThreads.front().Line
				   && !placeholder->PullRequest.ReviewThreads.front().OriginalLine
				   && !placeholder->PullRequest.ReviewThreads.front().Comments.front().Line,
			   "GitHub file-thread placeholder positions never become semantic coordinates");
		for (const auto& [before, after] : std::array<std::pair<std::string_view, std::string_view>, 6U>{ {
				 { "\"line\": 1", "\"line\": 2" },
				 { "\"line\": 1", "\"line\": null" },
				 { "\"originalLine\": 1", "\"originalLine\": 2" },
				 { "\"originalLine\": 1", "\"originalLine\": null" },
				 { "\"diffSide\": \"RIGHT\"", "\"diffSide\": \"LEFT\"" },
				 { "\"line\": null", "\"line\": 1" },
			 } }) {
			auto invalid = placeholderRecording;
			replace_once(invalid.Responses[6].Response.Body, before, after);
			expect(!normalize_github_read(normalization_context(server_contract::github_com), invalid.Responses),
				   "file placeholder acceptance refuses changed thread positions, side and comment coordinates");
		}

		auto noCoordinateReplyRecording = load_recording("github-com", server_contract::github_com);
		replace_once(
			noCoordinateReplyRecording.Responses[6].Response.Body,
			"\"bodyText\": \"Please preserve snapshot identity across refreshes.\",\n"
			"                    \"path\": \"review.cpp\",\n"
			"                    \"line\": 42,\n"
			"                    \"originalLine\": 42",
			"\"bodyText\": \"Please preserve snapshot identity across refreshes.\",\n"
			"                    \"path\": \"review.cpp\",\n"
			"                    \"line\": null,\n"
			"                    \"originalLine\": null");
		const auto noCoordinateReply = normalize_github_read(
			normalization_context(server_contract::github_com),
			noCoordinateReplyRecording.Responses);
		expect(
			noCoordinateReply.has_value()
				&& noCoordinateReply->PullRequest.ReviewThreads.front().Line
					== 42U
				&& !noCoordinateReply->PullRequest.ReviewThreads.front()
						.Comments.front()
						.Line.has_value()
				&& !noCoordinateReply->PullRequest.ReviewThreads.front()
						.Comments.front()
						.OriginalLine.has_value(),
			"a line-level reply without coordinates inherits the validated root anchor");

		auto nullAuthorRecording = load_recording("github-com", server_contract::github_com);
		replace_once(
			nullAuthorRecording.Responses[6].Response.Body,
			"\"author\": {\n"
			"                      \"login\": \"octo-reviewer\",\n"
			"                      \"id\": \"U_fixture_viewer\"\n"
			"                    }",
			"\"author\": null");
		const auto nullAuthor = normalize_github_read(
			normalization_context(server_contract::github_com),
			nullAuthorRecording.Responses);
		expect(
			nullAuthor.has_value()
				&& !nullAuthor->PullRequest.ReviewThreads.front()
						.Comments.front()
						.Author.has_value(),
			"deleted review-comment author remains nullable");

		auto botAuthorRecording = load_recording("github-com", server_contract::github_com);
		replace_once(
			botAuthorRecording.Responses[6].Response.Body,
			"\"login\": \"octo-reviewer\"",
			"\"login\": \"fixture-bot[bot]\"");
		replace_once(
			botAuthorRecording.Responses[6].Response.Body,
			"\"id\": \"U_fixture_viewer\"",
			"\"id\": \"BOT_fixture_ci\"");
		const auto botAuthor = normalize_github_read(
			normalization_context(server_contract::github_com),
			botAuthorRecording.Responses);
		expect(
			botAuthor.has_value()
				&& botAuthor->PullRequest.ReviewThreads.front()
					   .Comments.front()
					   .Author.has_value()
				&& botAuthor->PullRequest.ReviewThreads.front()
						.Comments.front()
						.Author->Actor.value()
					== "BOT_fixture_ci",
			"non-User Node actors retain stable identity");
	}
	auto test_glaze_normalization_keeps_raw_numbers_exact() -> void
	{
		struct numeric_case
		{
			std::size_t      Response;
			std::string_view Before;
			std::string_view After;
		};
		constexpr std::array cases{
			numeric_case{ 0U, "\"id\": 1001", "\"id\": 1001.0000000000000001" },
			numeric_case{ 1U, "\"id\": 2001", "\"id\": 2001.0000000000000001" },
			numeric_case{ 5U, "\"id\": 4001", "\"id\": 4001.0000000000000001" },
			numeric_case{ 3U, "\"additions\": 12", "\"additions\": 12.0000000000000001" },
			numeric_case{ 6U, "\"line\": 42", "\"line\": 42.0000000000000001" },
			numeric_case{ 0U, "\"id\": 1001", "\"id\": 18446744073709551616" },
		};
		for (const auto& example : cases) {
			auto recording = load_recording("github-com", server_contract::github_com);
			replace_once(recording.Responses[example.Response].Response.Body, example.Before, example.After);
			const auto result = normalize_github_read(normalization_context(server_contract::github_com), recording.Responses);
			expect(!result.has_value(), "raw fractional identities/counts/lines and uint64 overflow never yield a normalized snapshot");
		}
		for (const auto number : { std::uint64_t{ 1001U }, std::uint64_t{ 9007199254740991ULL } }) {
			auto recording = load_recording("github-com", server_contract::github_com);
			replace_once(recording.Responses.front().Response.Body, "\"id\": 1001", "\"id\": " + std::to_string(number) + "e0");
			auto context    = normalization_context(server_contract::github_com);
			context.Account = core::github_account_id{ number };
			// Keep the pull-request context bound to the same viewer.
			context.PullRequestRequest.Account = context.Account;
			const auto result                  = normalize_github_read(context, recording.Responses);
			expect(result.has_value() && result->Session.Viewer.Account == context.Account, "bounded exact integer exponent spelling retains account identity");
		}
	}

	auto test_glaze_anonymous_comment_normalization() -> void
	{
		auto recording = load_recording("github-com", server_contract::github_com);
		std::erase_if(recording.Responses, [](const auto& response) {
			return response.Operation == read_operation::get_viewer_identity || response.Operation == read_operation::list_review_threads;
		});
		const auto inlineResponse = recording.Responses.size();
		recording.Responses.push_back({ .Operation = read_operation::list_review_comments,
										.Response  = { .Status = 200, .Headers = { { "Content-Type", "application/json" } }, .Body = R"([
				{"id":501,"body":"root","path":"review.cpp","user":null,"line":42,"original_line":42},
				{"id":502,"in_reply_to_id":501,"body":"reply","path":"review.cpp","user":null,"line":42,"original_line":42,"position":null}
			])",
													   .Completeness = page_completeness::complete } });
		const auto viewer                  = canonical_normalized_read().Session.Viewer;
		const auto admission               = anonymous_read_admission(viewer);
		auto       context                 = normalization_context(server_contract::github_com);
		context.Capabilities.ReviewThreads = core::github_capability_state::unsupported;
		const auto result                  = normalize_github_read(context, recording.Responses, admission);
		expect(result.has_value(), "anonymous admission normalizes the nine REST responses with the supplied viewer");
		const auto& snapshot = result->PullRequest;
		expect(result->Session.Viewer == viewer && snapshot.ReviewThreads.empty()
				   && snapshot.ReviewThreadsAvailability == application::github_collection_availability::unavailable
				   && snapshot.ReviewCommentGroups.size() == 1U,
			   "anonymous normalization preserves viewer and REST comment groups without GraphQL threads");
		const auto& group = snapshot.ReviewCommentGroups.front();
		expect(group.Root.Comment.value() == "501" && !group.Root.Author && group.Root.Line == 42U && !group.Root.Outdated
				   && group.Replies.size() == 1U && group.Replies.front().Comment.value() == "502"
				   && group.Replies.front().ReplyTo == group.Root.Comment && group.Replies.front().Outdated,
			   "REST identities/replies and missing versus null position remain exact");
		for (const auto replacement : { std::string_view{ "\"id\":501.0000000000000001" }, std::string_view{ "\"id\":18446744073709551616" } }) {
			auto hostile = recording.Responses;
			replace_once(hostile[inlineResponse].Response.Body, "\"id\":501", replacement);
			expect(!normalize_github_read(context, hostile, admission), "fractional or overflowing REST comment identity fails atomically");
		}
		auto nullLine = recording.Responses;
		replace_once(nullLine[inlineResponse].Response.Body, "\"line\":42,", "\"line\":null,\"position\":{\"synthetic\":true},");
		const auto nullable = normalize_github_read(context, nullLine, admission);
		expect(nullable && !nullable->PullRequest.ReviewCommentGroups.front().Root.Line
				   && !nullable->PullRequest.ReviewCommentGroups.front().Root.Outdated,
			   "explicit null line stays absent and opaque non-null position keeps its nullness-only meaning");
		auto missingLine = recording.Responses;
		replace_once(missingLine[inlineResponse].Response.Body, "\"line\":42,", "");
		expect(!normalize_github_read(context, missingLine, admission), "missing line retains the prior invalid-coordinate refusal");
	}

	auto test_glaze_strict_response_identity_and_syntax() -> void
	{
		for (const auto identity : { std::uint64_t{ 9007199254740993ULL }, std::uint64_t{ 18446744073709551615ULL } }) {
			auto       recording = load_recording("github-com", server_contract::github_com);
			const auto encoded   = std::to_string(identity);
			replace_once(recording.Responses[0].Response.Body, "\"id\": 1001", "\"id\": " + encoded);
			replace_once(recording.Responses[1].Response.Body, "\"id\": 2001", "\"id\": " + encoded);
			replace_once(recording.Responses[2].Response.Body, "\"id\": 3001", "\"id\": " + encoded);
			replace_once(recording.Responses[2].Response.Body, "\"id\": 2001", "\"id\": " + encoded);
			replace_once(recording.Responses[2].Response.Body, "\"id\": 2001", "\"id\": " + encoded);
			replace_once(recording.Responses[5].Response.Body, "\"id\": 4001", "\"id\": " + encoded);
			auto context                                   = normalization_context(server_contract::github_com);
			context.Account                                = core::github_account_id{ identity };
			context.Repository                             = core::repository_id{ identity };
			context.PullRequest                            = core::pull_request_id{ identity };
			context.PullRequestRequest.Account             = context.Account;
			context.PullRequestRequest.ExpectedRepository  = context.Repository;
			context.PullRequestRequest.ExpectedPullRequest = context.PullRequest;
			const auto result                              = normalize_github_read(context, recording.Responses);
			expect(result && result->Session.Viewer.Account.value() == identity
					   && result->PullRequest.Repository.Repository.value() == identity
					   && result->PullRequest.PullRequest.PullRequest.value() == identity
					   && result->PullRequest.Reviews.front().DatabaseId == identity,
				   "strict validation and typed normalization retain IDs above 2^53 through UINT64_MAX");
			context.Account                    = core::github_account_id{ identity - 1U };
			context.PullRequestRequest.Account = context.Account;
			const auto mismatch                = normalize_github_read(context, recording.Responses);
			expect(!mismatch && mismatch.error().Code == contract_failure_code::identity_mismatch,
				   "adjacent large identities cannot satisfy the wrong viewer binding");
		}
		auto recording = load_recording("github-com", server_contract::github_com);
		for (const auto body : { R"(null)", R"(17)", R"("scalar")", R"({"id":1e400})", R"({"unknown":"\ud800"})", R"({"unknown":"\udc00"})", "{\"unknown\":\"\xc3\x28\"}", "{} {}" }) {
			auto response      = recording.Responses.front().Response;
			response.Body      = body;
			const auto invalid = validate_strict_fixture_response(read_operation::get_viewer_identity, response);
			expect(!invalid && invalid.error().Code == contract_failure_code::malformed_response,
				   "scalar roots, nonfinite numbers, malformed Unicode and trailing documents keep the syntax failure category");
		}
		for (auto& response : recording.Responses) {
			while (!response.Response.Body.empty() && (response.Response.Body.back() == '\n' || response.Response.Body.back() == '\r')) {
				response.Response.Body.pop_back();
			}
		}
		const auto eofResult = normalize_github_read(normalization_context(server_contract::github_com), recording.Responses);
		expect(eofResult && *eofResult == canonical_normalized_read(), "object and array responses ending exactly at EOF retain complete normalized facts");
		auto&                  graphql  = recording.Responses[6].Response.Body;
		const std::string_view path     = "\"path\": \"review.cpp\"";
		const auto             lastPath = graphql.rfind(path);
		expect(lastPath != std::string::npos, "escaped-path fixture selects a comment path");
		graphql.replace(lastPath, path.size(), R"("path": "\u0072eview.cpp")");
		const auto escaped = normalize_github_read(normalization_context(server_contract::github_com), recording.Responses);
		expect(escaped && *escaped == canonical_normalized_read(), "thread/comment path equality compares decoded strings rather than JSON spelling");
	}

}

namespace {
	/// [[BPR-0988]]. The two discovery reads compile to exactly one request
	/// shape each, on both server contracts, and nothing else passes.
	auto test_personal_discovery_requests() -> void
	{
		for (const auto server : { server_contract::github_com, server_contract::ghes_3_18 }) {
			const auto prefix   = std::string{ server == server_contract::github_com ? "" : "/api/v3" };
			auto       search   = semantic(read_operation::search_personal_pull_requests, server);
			search.SearchTerms  = "author:octo-reviewer";
			const auto authored = compile_read_request(search);
			expect(authored.has_value() && authored->Method == http_method::get && authored->Body.empty()
					   && authored->Path == prefix + "/search/issues"
					   && authored->Query == "page=1&per_page=100&q=is%3Apr%20is%3Aopen%20author%3Aocto-reviewer%20repo%3Aocto-org%2Freview-repository"
					   && validate_outbound_read_request(*authored).has_value(),
				   "an authored search is one REST GET scoped to the repository and the login");
			search.SearchTerms   = "review-requested:octo_reviewer-2";
			const auto requested = compile_read_request(search);
			expect(requested.has_value()
					   && requested->Query == "page=1&per_page=100&q=is%3Apr%20is%3Aopen%20review-requested%3Aocto_reviewer-2%20repo%3Aocto-org%2Freview-repository",
				   "a requested search uses the documented review-requested qualifier");
			auto reviewers              = semantic(read_operation::list_requested_reviewers, server);
			reviewers.PullRequestNumber = 17U;
			const auto detail           = compile_read_request(reviewers);
			expect(detail.has_value() && detail->Path == prefix + "/repos/octo-org/review-repository/pulls/17/requested_reviewers"
					   && detail->Query.empty() && detail->Method == http_method::get && validate_outbound_read_request(*detail).has_value(),
				   "the requested reviewers read is one REST GET of one pull request");
		}
		for (const auto* terms : { "", "is:open", "author:", "author:-octo", "author:octo reviewer", "author:octo repo:other/repo", "author:@me",
								   "reviewed-by:octo", "user-review-requested:octo", "team-review-requested:org/team", "author:octo%20x", "author:octo\n",
								   "review-requested:", "AUTHOR:octo" }) {
			auto search        = semantic(read_operation::search_personal_pull_requests);
			search.SearchTerms = terms;
			expect(!compile_read_request(search).has_value(), "any other search terms, qualifier or login shape is refused");
		}
		{
			auto search        = semantic(read_operation::search_personal_pull_requests);
			search.SearchTerms = "author:" + std::string(65U, 'a');
			expect(!compile_read_request(search).has_value(), "an unbounded login is refused");
			search.SearchTerms = "author:octo";
			search.Page        = 2U;
			expect(!compile_read_request(search).has_value(), "discovery reads one page only");
			search.Page              = 1U;
			search.PullRequestNumber = 17U;
			expect(!compile_read_request(search).has_value(), "a search names no pull request");
			auto reviewers              = semantic(read_operation::list_requested_reviewers);
			reviewers.PullRequestNumber = 0U;
			expect(!compile_read_request(reviewers).has_value(), "requested reviewers name a pull request");
			reviewers.PullRequestNumber = 17U;
			reviewers.SearchTerms       = "author:octo";
			expect(!compile_read_request(reviewers).has_value(), "requested reviewers carry no search terms");
			auto forged        = semantic(read_operation::search_personal_pull_requests);
			forged.SearchTerms = "author:octo";
			auto wire          = compile_read_request(forged);
			expect(wire.has_value(), "canonical personal search compiles");
			wire->Query += "&sort=updated";
			expect(!validate_outbound_read_request(*wire).has_value(), "an outbound search with extra query parameters is refused");
			expect(read_operation_from_name("SearchPersonalPullRequests") == read_operation::search_personal_pull_requests
					   && read_operation_from_name("ListRequestedReviewers") == read_operation::list_requested_reviewers,
				   "the discovery reads name themselves");
			expect(std::ranges::find(read_operation_catalog, read_operation::search_personal_pull_requests) == read_operation_catalog.end()
					   && std::ranges::find(read_operation_catalog, read_operation::list_requested_reviewers) == read_operation_catalog.end(),
				   "the recorded snapshot manifest does not grow with discovery");
		}
	}
}

auto main() -> int
{
	test_glaze_strict_response_identity_and_syntax();
	test_glaze_anonymous_comment_normalization();
	test_glaze_normalization_keeps_raw_numbers_exact();
	test_manifest_catalog_and_normalization_are_one_contract();
	test_read_only_protocol_policy();
	test_glaze_read_request_bytes_and_raw_validation();
	test_read_only_catalog_and_outbound_validation();
	test_hostile_responses_fail_atomically();
	test_provider_valid_nullable_and_conditional_shapes();
	test_personal_discovery_requests();

	std::cout << "All GitHub normalized read-contract tests passed\n";
	return EXIT_SUCCESS;
}
