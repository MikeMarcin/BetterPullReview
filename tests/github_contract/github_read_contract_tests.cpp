#include "github_graphql_operation_policy.hpp"
#include "github_read_contract.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

	[[nodiscard]] auto parse_object(const std::filesystem::path& aPath)
		-> QJsonObject
	{
		QJsonParseError error;
		const auto      bytes    = QByteArray::fromStdString(read_all(aPath));
		const auto      document = QJsonDocument::fromJson(bytes, &error);
		expect(
			error.error == QJsonParseError::NoError && document.isObject(),
			"fixture manifest is valid JSON");
		return document.object();
	}

	[[nodiscard]] auto utf8(const QString& aValue) -> std::string
	{
		return aValue.toUtf8().toStdString();
	}

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
		const auto manifest     = parse_object(root / "manifest.json");
		const auto interactions = manifest.value("interactions").toArray();
		expect(
			interactions.size()
				== static_cast<qsizetype>(read_operation_catalog.size()),
			"manifest has exactly ten interactions");

		loaded_recording result;
		result.Exchanges.reserve(read_operation_catalog.size());
		result.Responses.reserve(read_operation_catalog.size());
		for (qsizetype index = 0; index < interactions.size(); ++index) {
			const auto interaction     = interactions[index].toObject();
			const auto parsedOperation = read_operation_from_name(
				utf8(interaction.value("operation").toString()));
			expect(parsedOperation.has_value(), "manifest operation is known");
			const auto operation = *parsedOperation;
			expect(
				operation
					== read_operation_catalog[static_cast<std::size_t>(index)],
				"manifest operations use canonical order");

			std::vector<http_header> headers;
			const auto               headerObject = interaction.value("safe_headers").toObject();
			headers.reserve(static_cast<std::size_t>(headerObject.size()));
			for (
				auto iterator = headerObject.constBegin();
				iterator != headerObject.constEnd();
				++iterator) {
				headers.push_back(
					http_header{
						.Name  = utf8(iterator.key()),
						.Value = utf8(iterator.value().toString()),
					});
			}

			const auto method = parse_recorded_http_method(
				utf8(interaction.value("method").toString()));
			expect(
				method.has_value(),
				"manifest method is exactly GET or POST");
			const auto bodyValue    = interaction.value("body");
			const auto body         = bodyValue.isNull() ? std::string{ }
														 : utf8(bodyValue.toString());
			const auto responsePath = root
				/ utf8(interaction.value("response_file").toString());
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
						.Path      = utf8(interaction.value("path").toString()),
						.Query     = utf8(
							interaction.value("canonical_query")
								.toString()),
						.Headers = std::move(headers),
						.Body    = body,
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
					.Check      = application::github_check_id{ "CR_fixture_1" },
					.Name       = "Local verification",
					.Status     = application::github_check_status::completed,
					.Conclusion = application::github_check_conclusion::success,
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
			request.PullRequestNumber = 17;
			break;
		case read_operation::get_viewer_identity:
		case read_operation::get_repository:
		case read_operation::list_checks:
		case read_operation::search_pull_requests:
			break;
		}
		if (aOperation == read_operation::list_checks) {
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

	[[nodiscard]] auto graphql_query(const wire_request& aRequest) -> std::string
	{
		QJsonParseError error;
		const auto      document = QJsonDocument::fromJson(
			QByteArray::fromStdString(aRequest.Body),
			&error);
		expect(
			error.error == QJsonParseError::NoError && document.isObject()
				&& document.object().value("query").isString(),
			"compiled GraphQL body exposes one query string");
		return utf8(document.object().value("query").toString());
	}

	[[nodiscard]] auto with_graphql_query(
		wire_request           aRequest,
		const std::string_view aQuery) -> wire_request
	{
		QJsonParseError error;
		const auto      document = QJsonDocument::fromJson(
			QByteArray::fromStdString(aRequest.Body),
			&error);
		expect(
			error.error == QJsonParseError::NoError && document.isObject(),
			"hostile GraphQL base body is valid JSON");
		auto object = document.object();
		object.insert(
			"query",
			QString::fromUtf8(
				aQuery.data(),
				static_cast<qsizetype>(aQuery.size())));
		aRequest.Body = QJsonDocument{ object }.toJson(QJsonDocument::Compact).toStdString();
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

	auto test_read_only_catalog_and_outbound_validation() -> void
	{
		expect(
			read_operation_catalog.size() == 10U,
			"catalog has exactly ten reads");
		for (const auto operation : read_operation_catalog) {
			const auto roundTrip = read_operation_from_name(operation_name(operation));
			expect(
				roundTrip.has_value() && *roundTrip == operation,
				"every read name round-trips");
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

		for (const auto& [wire_state, normalized_state] : {
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
					+ std::string{ wire_state } + "\"");
			const auto normalized = normalize_github_read(
				normalization_context(server_contract::github_com),
				viewedRecording.Responses);
			expect(
				normalized.has_value()
					&& normalized->PullRequest.Files.front().ViewedState
						== normalized_state,
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
}

auto main() -> int
{
	test_manifest_catalog_and_normalization_are_one_contract();
	test_read_only_protocol_policy();
	test_read_only_catalog_and_outbound_validation();
	test_hostile_responses_fail_atomically();
	test_provider_valid_nullable_and_conditional_shapes();

	std::cout << "All GitHub normalized read-contract tests passed\n";
	return EXIT_SUCCESS;
}
