#include "github_paged_read.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	constexpr std::string_view base_oid = "1111111111111111111111111111111111111111";
	constexpr std::string_view head_oid = "2222222222222222222222222222222222222222";

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto json(const QJsonObject& aValue) -> std::string
	{
		return QJsonDocument{ aValue }.toJson(QJsonDocument::Compact).toStdString();
	}

	[[nodiscard]] auto json(const QJsonArray& aValue) -> std::string
	{
		return QJsonDocument{ aValue }.toJson(QJsonDocument::Compact).toStdString();
	}

	auto fill_to_member_count(QJsonObject&    aObject,
							  const QString&  aPrefix,
							  const qsizetype aTarget) -> void
	{
		qsizetype index = 0;
		while (aObject.size() < aTarget) {
			aObject.insert(QString{ "%1_%2" }.arg(aPrefix).arg(index),
						   QString{ "ignored raw GitHub field %1" }.arg(index));
			++index;
		}
	}

	[[nodiscard]] auto
	response_headers(std::optional<std::string> aLink = std::nullopt)
		-> std::vector<http_header>
	{
		std::vector<http_header> result{
			{
				.Name  = "Content-Type",
				.Value = "application/json; charset=utf-8",
			},
		};
		if (aLink.has_value()) {
			result.push_back({
				.Name  = "Link",
				.Value = std::move(*aLink),
			});
		}
		result.push_back({
			.Name  = "X-RateLimit-Limit",
			.Value = "5000",
		});
		result.push_back({
			.Name  = "X-RateLimit-Remaining",
			.Value = "4999",
		});
		result.push_back({
			.Name  = "X-RateLimit-Reset",
			.Value = "2000000000",
		});
		result.push_back({
			.Name  = "X-RateLimit-Resource",
			.Value = "core",
		});
		return result;
	}

	[[nodiscard]] auto success_response(
		std::string                aBody,
		std::optional<std::string> aLink         = std::nullopt,
		const page_completeness    aCompleteness = page_completeness::complete)
		-> wire_response
	{
		return {
			.Status       = 200,
			.Headers      = response_headers(std::move(aLink)),
			.Body         = std::move(aBody),
			.Completeness = aCompleteness,
		};
	}

	[[nodiscard]] auto profile() -> validated_github_host_profile
	{
		const auto result = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 22U },
			.Revision       = application::github_profile_revision{ 3U },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		});
		expect(result.has_value(), "test profile validates");
		return *result;
	}

	[[nodiscard]] auto enterprise_profile() -> validated_github_host_profile
	{
		const auto result = validate_github_host_profile(github_host_profile_candidate{
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 22U },
			.Revision       = application::github_profile_revision{ 3U },
			.Server         = application::github_server_contract::ghes_3_18_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::explicit_user,
			.Hostname       = "ghe.example.invalid",
			.RestRoot       = "https://ghe.example.invalid/api/v3",
			.GraphqlRoot    = "https://ghe.example.invalid/api/graphql",
		});
		expect(result.has_value(), "enterprise test profile validates");
		return *result;
	}

	class test_account_transport final
		: public application::github_account_transport_lease
	{
	};

	[[nodiscard]] auto
	provider_request(const application::github_server_contract aServer = application::github_server_contract::github_com_2022_11_28)
		-> application::github_pull_request_provider_request
	{
		const application::github_capability_authority_key authority{
			.Profile               = core::host_profile_id{ 11U },
			.ProfileRevision       = application::github_profile_revision{ 3U },
			.Host                  = core::git_host_id{ 22U },
			.Account               = core::github_account_id{ 1001U },
			.ServerContract        = aServer,
			.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
		};
		std::array<application::github_capability_fact,
				   application::github_capability_fact_count>
						 aggregate{ };
		const std::array aggregateStates{
			core::github_capability_state::supported,
			core::github_capability_state::unsupported,
			core::github_capability_state::unsupported,
			core::github_capability_state::unsupported,
		};
		for (std::size_t index = 0U; index < aggregate.size(); ++index) {
			aggregate[index] = {
				.Capability = application::canonical_github_capabilities[index],
				.State      = aggregateStates[index],
				.Source     = application::github_capability_evidence_source::
					observed_contract,
			};
		}
		std::array<application::github_schema_capability_fact,
				   application::github_schema_capability_fact_count>
						 schema{ };
		const std::array schemaStates{
			core::github_capability_state::supported,
			core::github_capability_state::unsupported,
			core::github_capability_state::unsupported,
			core::github_capability_state::unsupported,
			core::github_capability_state::unsupported,
			core::github_capability_state::unsupported,
			core::github_capability_state::supported,
			core::github_capability_state::unsupported,
		};
		for (std::size_t index = 0U; index < schema.size(); ++index) {
			schema[index] = {
				.Capability = application::canonical_github_schema_capabilities[index],
				.State      = schemaStates[index],
				.Source     = application::github_capability_evidence_source::
					observed_contract,
			};
		}

		return {
			.Context = {
				.Request            = core::request_id{ 41U },
				.ApplicationSession = core::application_session_id{ 7U },
				.Profile            = core::host_profile_id{ 11U },
				.Host               = core::git_host_id{ 22U },
				.Account            = core::github_account_id{ 1001U },
				.Repository         = {
					.Owner = "octo-org",
					.Name  = "review-repository",
				},
				.Number              = core::pull_request_number{ 17U },
				.ExpectedRepository  = std::nullopt,
				.ExpectedPullRequest = std::nullopt,
				.Capabilities        = core::capability_revision{ 44U },
				.IdentityGeneration  = core::github_identity_generation{ 3U },
				.Generation          = core::request_generation{ 9U },
			},
			.ProfileAuthority = {
				.Profile               = core::host_profile_id{ 11U },
				.ProfileRevision       = application::github_profile_revision{ 3U },
				.Host                  = core::git_host_id{ 22U },
				.Hostname              = application::github_canonical_hostname{ aServer == application::github_server_contract::github_com_2022_11_28 ? "github.com" : "ghe.example.invalid" },
				.ServerContract        = aServer,
				.Authentication        = application::github_authentication_transport::native_https,
				.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
			},
			.Viewer = {
				.Account     = core::github_account_id{ 1001U },
				.Node        = application::github_actor_id{ "U_viewer" },
				.Login       = "octo-reviewer",
				.DisplayName = "Octo Reviewer",
			},
			.CapabilityEvidence = {
				.Authority    = authority,
				.Facts        = aggregate,
				.SchemaFacts  = schema,
				.Capabilities = {
					.ReviewThreads       = core::github_capability_state::supported,
					.ViewedFileMutations = core::github_capability_state::unsupported,
					.ResolveThread       = core::github_capability_state::unsupported,
					.UnresolveThread     = core::github_capability_state::unsupported,
				},
				.ReadShape = {
					.ReviewThreads       = true,
					.ViewedFileState     = false,
					.ResolvePermission   = false,
					.UnresolvePermission = true,
				},
				.Revision      = core::capability_revision{ 44U },
				.ObservedAt    = application::github_freshness_epoch{ 100U },
				.ExpiresBefore = application::github_freshness_epoch{ 400U },
			},
			.AccountTransport = std::make_shared<test_account_transport>(),
			.Attempt          = application::github_provider_attempt{ 1U },
		};
	}

	[[nodiscard]] auto rest_actor(const std::string_view aNode,
								  const std::string_view aLogin) -> QJsonObject
	{
		return {
			{ "avatar_url", "https://avatars.githubusercontent.com/u/123" },
			{ "id", 123 },
			{ "login", QString::fromUtf8(aLogin) },
			{ "node_id", QString::fromUtf8(aNode) },
			{ "type", "User" },
		};
	}

	[[nodiscard]] auto comment_node(const std::size_t aIndex) -> QJsonObject
	{
		return {
			{ "author", QJsonValue::Null },
			{ "bodyText", QString{ "comment %1" }.arg(aIndex) },
			{ "id", QString{ "C_%1" }.arg(aIndex) },
			{ "line", 42 },
			{ "originalLine", QJsonValue::Null },
			{ "outdated", false },
			{ "path", "src/file0.cpp" },
			{ "replyTo", QJsonValue::Null },
		};
	}

	[[nodiscard]] auto page_info(const bool                            aHasNext,
								 const std::optional<std::string_view> aCursor)
		-> QJsonObject
	{
		return {
			{
				"endCursor",
				aCursor.has_value() ? QJsonValue{ QString::fromUtf8(*aCursor) }
									: QJsonValue{ QJsonValue::Null },
			},
			{ "hasNextPage", aHasNext },
		};
	}

	[[nodiscard]] auto connection(const std::uint64_t                   aTotal,
								  QJsonArray                            aNodes,
								  const bool                            aHasNext,
								  const std::optional<std::string_view> aCursor)
		-> QJsonObject
	{
		return {
			{ "nodes", std::move(aNodes) },
			{ "pageInfo", page_info(aHasNext, aCursor) },
			{ "totalCount", static_cast<qint64>(aTotal) },
		};
	}

	enum class fixture_scenario : std::uint8_t {
		success,
		raw_object_ceiling_exceeded,
		raw_depth_ceiling_exceeded,
		aggregate_byte_ceiling,
		large_ignored_patch,
		selected_title_over_limit,
		selected_page_records_exceeded,
		page_first_query,
		numeric_repository_link,
		cross_authority_link,
		wrong_numeric_repository_link,
		path_drift_link,
		fragment_link,
		userinfo_link,
		port_link,
		duplicate_link_query,
		extra_link_query,
		encoded_link_query,
		search_plus_link,
		search_query_semantic_drift,
		search_query_double_encoded,
		search_query_double_plus,
		unknown_completeness,
		missing_cursor,
		repeated_cursor,
		truncated_total,
		thread_side_missing,
		thread_side_null,
		thread_side_wrong,
		thread_side_unknown,
		current_thread_original_line_only,
		line_comment_original_line_only,
		line_comment_no_coordinates,
		file_comment_original_line,
		paged_line_comment_original_line_only,
		paged_line_comment_no_coordinates,
		paged_file_comment_original_line,
		graphql_head_moved,
		graphql_extensions,
		graphql_errors_type_rate_limited,
		graphql_partial_errors_extension_type,
		graphql_partial_errors_extension_code,
		graphql_primary_rate_metadata,
		graphql_malformed_errors,
		graphql_unknown_envelope_field,
		rest_secondary_no_retry,
		rest_secondary_with_retry,
		rest_secondary_primary_metadata,
		rest_secondary_duplicate_message,
		rest_generic_403,
		rest_429,
		rest_408,
		rest_503_retry,
		rest_504,
		network_failure,
		wrong_resource_wire_error,
		late_search_wire_failure,
		repository_missing_required,
		repository_wrong_required,
		pull_request_missing_required,
		pull_request_wrong_required,
		actor_missing_required,
		actor_wrong_required,
		file_missing_required,
		file_wrong_required,
		commit_missing_required,
		commit_wrong_required,
		review_missing_required,
		review_wrong_required,
		issue_comment_missing_required,
		issue_comment_wrong_required,
		check_missing_required,
		check_wrong_required,
		check_envelope_missing_required,
		check_envelope_wrong_required,
		search_missing_required,
		search_wrong_required,
		search_wrong_authority,
		search_wrong_owner,
		search_wrong_repository,
		search_wrong_number_url,
		search_missing_pull_request,
		search_wrong_pull_request_url,
		search_canonical_repository_casing,
		search_envelope_missing_required,
		search_envelope_wrong_required,
		pull_request_missing_file_total,
		pull_request_wrong_file_total,
		pull_request_missing_commit_total,
		pull_request_wrong_commit_total,
		file_count_mismatch,
		commit_count_mismatch,
		file_count_above_cap,
		commit_count_above_cap,
		collection_cap_exact,
	};

	class paged_fixture_transport final : public github_wire_transport_port
	{
	public:
		explicit paged_fixture_transport(const fixture_scenario aScenario,
										 std::string            aAuthority = "api.github.com")
			: Scenario{ aScenario }
			, Authority{ std::move(aAuthority) }
		{
		}

		[[nodiscard]] auto perform(const github_wire_request& aRequest,
								   std::stop_token)
			-> github_wire_completion override
		{
			Requests.push_back(aRequest);
			if (Scenario == fixture_scenario::network_failure
				&& Requests.size() == 1U) {
				return github_wire_error{
					.Correlation = aRequest.Correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				};
			}
			if (Scenario == fixture_scenario::wrong_resource_wire_error
				&& Requests.size() == 1U) {
				return github_wire_error{
					.Correlation = aRequest.Correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = {
							.Core   = std::nullopt,
							.Search = application::github_rate_limit_record{
								.Resource = application::
									github_rate_limit_resource::search,
								.Limit            = 30U,
								.Remaining        = 29U,
								.ResetUnixSeconds = 2'000'000'000U,
							},
							.Graphql = std::nullopt,
						},
						.RateLimitCause = std::nullopt,
					},
				};
			}
			if (Scenario == fixture_scenario::late_search_wire_failure
				&& aRequest.Wire.Operation == read_operation::search_pull_requests) {
				return github_wire_error{
					.Correlation = aRequest.Correlation,
					.Error       = {
						.Failure           = application::github_provider_failure::network,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = {
							.Core   = std::nullopt,
							.Search = application::github_rate_limit_record{
								.Resource = application::
									github_rate_limit_resource::search,
								.Limit            = 30U,
								.Remaining        = 17U,
								.ResetUnixSeconds = 2'000'000'001U,
							},
							.Graphql = std::nullopt,
						},
						.RateLimitCause = std::nullopt,
					},
				};
			}
			return github_wire_success{
				.Correlation = aRequest.Correlation,
				.Response    = response_for(aRequest),
			};
		}

		std::vector<github_wire_request> Requests;
		// The signed-out inline review-comment listing the fixture serves.
		// Each RFC-BPR-0017 case is a specific promoted or synthetic payload,
		// so the pages are handed in rather than generated.
		std::vector<std::string> ReviewCommentPages;
		int                      ReviewCommentStatus = 200;
		std::vector<http_header> ReviewCommentHeaders;

	private:
		[[nodiscard]] auto advertised_file_count() const noexcept -> std::uint32_t
		{
			if (Scenario == fixture_scenario::collection_cap_exact) {
				return 3'000U;
			}
			if (Scenario == fixture_scenario::file_count_mismatch) {
				return 102U;
			}
			if (Scenario == fixture_scenario::file_count_above_cap) {
				return 3'001U;
			}
			return 101U;
		}

		[[nodiscard]] auto advertised_commit_count() const noexcept -> std::uint32_t
		{
			if (Scenario == fixture_scenario::collection_cap_exact) {
				return 250U;
			}
			if (Scenario == fixture_scenario::commit_count_mismatch) {
				return 2U;
			}
			if (Scenario == fixture_scenario::commit_count_above_cap) {
				return 251U;
			}
			return 1U;
		}

		[[nodiscard]] auto repository_response() const -> wire_response
		{
			int                        operationalStatus = 0;
			std::string                operationalMessage;
			std::optional<std::string> retryAfter;
			switch (Scenario) {
			case fixture_scenario::rest_secondary_no_retry:
				operationalStatus  = 403;
				operationalMessage = "You have exceeded a SECONDARY RATE LIMIT. Please wait.";
				break;
			case fixture_scenario::rest_secondary_with_retry:
				operationalStatus  = 403;
				operationalMessage = "secondary rate limit";
				retryAfter         = "17";
				break;
			case fixture_scenario::rest_secondary_primary_metadata:
				operationalStatus  = 403;
				operationalMessage = "secondary rate limit";
				break;
			case fixture_scenario::rest_secondary_duplicate_message:
				operationalStatus = 403;
				break;
			case fixture_scenario::rest_generic_403:
				operationalStatus  = 403;
				operationalMessage = "API rate limit policy information";
				break;
			case fixture_scenario::rest_429:
				operationalStatus  = 429;
				operationalMessage = "Too many requests";
				break;
			case fixture_scenario::rest_408:
				operationalStatus  = 408;
				operationalMessage = "Request timed out";
				break;
			case fixture_scenario::rest_503_retry:
				operationalStatus  = 503;
				operationalMessage = "Temporarily unavailable";
				retryAfter         = "9";
				break;
			case fixture_scenario::rest_504:
				operationalStatus  = 504;
				operationalMessage = "Gateway timeout";
				break;
			default:
				break;
			}
			if (operationalStatus != 0) {
				auto response = success_response(
					Scenario == fixture_scenario::rest_secondary_duplicate_message
						? R"({"message":"secondary rate limit","message":"permission"})"
						: json(QJsonObject{
							  { "message",
								QString::fromStdString(operationalMessage) },
						  }));
				response.Status = operationalStatus;
				if (retryAfter.has_value()) {
					response.Headers.push_back({
						.Name  = "Retry-After",
						.Value = *retryAfter,
					});
				}
				if (Scenario == fixture_scenario::rest_secondary_primary_metadata) {
					for (auto& header : response.Headers) {
						if (header.Name == "X-RateLimit-Remaining") {
							header.Value = "0";
						}
					}
				}
				return response;
			}
			if (Scenario == fixture_scenario::raw_depth_ceiling_exceeded) {
				QJsonValue nested = QJsonValue::Null;
				for (std::size_t depth = 0U; depth < 33U; ++depth) {
					nested = QJsonObject{ { "nested", nested } };
				}
				return success_response(json(nested.toObject()));
			}
			const auto canonicalCasing = Scenario == fixture_scenario::search_canonical_repository_casing;
			const auto ownerName       = canonicalCasing ? std::string{ "Octo-Org" }
														 : std::string{ "octo-org" };
			const auto repositoryName  = canonicalCasing
				? std::string{ "Review-Repository" }
				: std::string{ "review-repository" };
			auto       repository      = QJsonObject{
				{ "archive_url",
				  "https://api.github.com/repos/octo-org/review-repository/"
				  "{archive_format}{/ref}" },
				{ "default_branch", "main" },
				{ "full_name",
				  QString::fromStdString(ownerName + "/" + repositoryName) },
				{ "html_url", "https://github.com/octo-org/review-repository" },
				{ "id", 2001 },
				{ "name", QString::fromStdString(repositoryName) },
				{ "node_id", "R_repository" },
				{ "owner", rest_actor("O_organization", ownerName) },
				{ "private", false },
				{ "visibility", "private" },
			};
			QJsonArray ignoredTopics;
			for (std::size_t index = 0U; index < 101U; ++index) {
				ignoredTopics.push_back(QString{ "topic-%1" }.arg(index));
			}
			repository.insert("ignored_topics", ignoredTopics);
			if (Scenario == fixture_scenario::aggregate_byte_ceiling) {
				repository.insert("aggregate_padding",
								  QString{ 70'000U, QLatin1Char{ 'r' } });
			}
			if (Scenario == fixture_scenario::repository_missing_required) {
				repository.remove("default_branch");
			} else if (Scenario == fixture_scenario::repository_wrong_required) {
				repository.insert("id", "not-an-integer");
			}
			fill_to_member_count(
				repository,
				"repository_surplus",
				Scenario == fixture_scenario::raw_object_ceiling_exceeded ? 257
																		  : 86);
			return success_response(json(repository));
		}

		[[nodiscard]] auto pull_request_response() const -> wire_response
		{
			auto baseRepository = QJsonObject{
				{ "full_name", "octo-org/review-repository" },
				{ "html_url", "https://github.com/octo-org/review-repository" },
				{ "id", 2001 },
				{ "node_id", "R_repository" },
			};
			fill_to_member_count(baseRepository, "base_repo_surplus", 81);
			auto headRepository = baseRepository;
			fill_to_member_count(headRepository, "head_repo_surplus", 81);
			const auto base = QJsonObject{
				{ "label", "octo-org:main" },
				{ "ref", "main" },
				{ "repo", baseRepository },
				{ "sha", QString::fromUtf8(base_oid) },
				{ "user", rest_actor("O_organization", "octo-org") },
			};
			const auto head = QJsonObject{
				{ "label", "octo-org:feature/paged-read" },
				{ "ref", "feature/paged-read" },
				{ "repo", headRepository },
				{ "sha", QString::fromUtf8(head_oid) },
				{ "user", rest_actor("O_organization", "octo-org") },
			};
			auto author = rest_actor("U_author", "octo-author");
			if (Scenario == fixture_scenario::actor_missing_required) {
				author.remove("node_id");
			} else if (Scenario == fixture_scenario::actor_wrong_required) {
				author.insert("login", 123);
			}
			auto pullRequest = QJsonObject{
				{ "base", base },
				{ "body", "Synthetic body" },
				{ "changed_files", static_cast<qint64>(advertised_file_count()) },
				{ "comments", 1 },
				{ "commits", static_cast<qint64>(advertised_commit_count()) },
				{ "created_at", "2026-07-30T12:00:00Z" },
				{ "draft", false },
				{ "head", head },
				{ "html_url",
				  "https://github.com/octo-org/review-repository/pull/17" },
				{ "id", 3001 },
				{ "locked", false },
				{ "merged", false },
				{ "node_id", "PR_17" },
				{ "number", 17 },
				{ "state", "open" },
				{ "title", "Exhaust every page" },
				{ "updated_at", "2026-07-30T12:30:00Z" },
				{ "user", author },
			};
			if (Scenario == fixture_scenario::aggregate_byte_ceiling) {
				pullRequest.insert("aggregate_padding",
								   QString{ 70'000U, QLatin1Char{ 'p' } });
			}
			if (Scenario == fixture_scenario::pull_request_missing_required) {
				pullRequest.remove("title");
			} else if (Scenario == fixture_scenario::pull_request_wrong_required) {
				pullRequest.insert("draft", "false");
			} else if (Scenario == fixture_scenario::pull_request_missing_file_total) {
				pullRequest.remove("changed_files");
			} else if (Scenario == fixture_scenario::pull_request_wrong_file_total) {
				pullRequest.insert("changed_files", "101");
			} else if (Scenario == fixture_scenario::pull_request_missing_commit_total) {
				pullRequest.remove("commits");
			} else if (Scenario == fixture_scenario::pull_request_wrong_commit_total) {
				pullRequest.insert("commits", "1");
			} else if (Scenario == fixture_scenario::selected_title_over_limit) {
				pullRequest.insert("title", QString{ 1'025U, QLatin1Char{ 't' } });
			}
			return success_response(json(pullRequest));
		}

		[[nodiscard]] auto files_response(const github_wire_request& aRequest) const
			-> wire_response
		{
			QJsonArray files;
			const auto page  = aRequest.Wire.Semantic.Page;
			const auto total = Scenario == fixture_scenario::collection_cap_exact
				? std::size_t{ 3'000U }
				: std::size_t{ 101U };
			const auto begin = (static_cast<std::size_t>(page) - 1U) * std::size_t{ 100U };
			const auto end   = Scenario == fixture_scenario::selected_page_records_exceeded
					&& page == 1U
				? total
				: std::min(begin + std::size_t{ 100U }, total);
			for (auto index = begin; index < end; ++index) {
				auto file = QJsonObject{
					{ "additions", 1 },
					{ "blob_url",
					  QString{ "https://github.com/octo-org/review-repository/"
							   "blob/head/src/file%1.cpp" }
						  .arg(index) },
					{ "changes", 1 },
					{ "contents_url",
					  QString{ "https://api.github.com/repos/octo-org/"
							   "review-repository/contents/src/file%1.cpp" }
						  .arg(index) },
					{ "deletions", 0 },
					{
						"filename",
						QString{ "src/file%1.cpp" }.arg(index),
					},
					{ "patch", "@@ -1 +1 @@" },
					{ "raw_url",
					  QString{ "https://github.com/octo-org/review-repository/"
							   "raw/head/src/file%1.cpp" }
						  .arg(index) },
					{ "sha", QString::fromUtf8(head_oid) },
					{ "status", "modified" },
				};
				if (index == 0U
					&& Scenario == fixture_scenario::file_missing_required) {
					file.remove("filename");
				} else if (index == 0U
						   && Scenario == fixture_scenario::file_wrong_required) {
					file.insert("additions", "one");
				} else if (index == 0U
						   && Scenario == fixture_scenario::large_ignored_patch) {
					file.insert("patch", QString{ 500U * 1'024U, QLatin1Char{ 'p' } });
				}
				files.push_back(std::move(file));
			}
			const auto hasNext = end < total;
			if (!hasNext) {
				const auto completeness = Scenario == fixture_scenario::unknown_completeness
					? page_completeness::unknown
					: page_completeness::complete;
				return success_response(json(files), std::nullopt, completeness);
			}
			auto authority = "https://" + Authority;
			if (Scenario == fixture_scenario::cross_authority_link) {
				authority = "https://evil.example.invalid";
			} else if (Scenario == fixture_scenario::userinfo_link) {
				authority = "https://octo@api.github.com";
			} else if (Scenario == fixture_scenario::port_link) {
				authority = "https://api.github.com:443";
			}
			auto path = aRequest.Wire.Path;
			if (Scenario == fixture_scenario::numeric_repository_link
				|| Scenario == fixture_scenario::wrong_numeric_repository_link) {
				const auto named    = std::string{ "/repos/octo-org/review-repository" };
				const auto position = path.find(named);
				expect(position != std::string::npos,
					   "fixture canonical repository path is present");
				const auto id = Scenario == fixture_scenario::numeric_repository_link ? "2001"
																					  : "9999";
				path.replace(
					position, named.size(), std::string{ "/repositories/" } + id);
			} else if (Scenario == fixture_scenario::path_drift_link) {
				path += "/drift";
			}
			auto query = Scenario == fixture_scenario::page_first_query
				? "page=" + std::to_string(page + 1U) + "&per_page=100"
				: "per_page=100&page=" + std::to_string(page + 1U);
			if (Scenario == fixture_scenario::duplicate_link_query) {
				query += "&page=" + std::to_string(page + 1U);
			} else if (Scenario == fixture_scenario::extra_link_query) {
				query += "&unexpected=1";
			} else if (Scenario == fixture_scenario::encoded_link_query) {
				query = "%70age=" + std::to_string(page + 1U) + "&per_page=100";
			}
			if (Scenario == fixture_scenario::fragment_link) {
				query += "#fragment";
			}
			const auto link = "<" + authority + path + "?" + query + ">; rel=\"next\"";
			return success_response(
				json(files), link, page_completeness::has_next_page);
		}

		[[nodiscard]] auto
		commits_response(const github_wire_request& aRequest) const -> wire_response
		{
			const auto total = Scenario == fixture_scenario::collection_cap_exact
				? std::size_t{ 250U }
				: std::size_t{ 1U };
			const auto page  = aRequest.Wire.Semantic.Page;
			const auto begin = (static_cast<std::size_t>(page) - 1U) * std::size_t{ 100U };
			const auto end   = std::min(begin + std::size_t{ 100U }, total);
			QJsonArray commits;
			for (auto index = begin; index < end; ++index) {
				const auto numeric = std::to_string(index + 1U);
				auto       oid     = std::string(40U, '0');
				oid.replace(oid.size() - numeric.size(), numeric.size(), numeric);
				if (index + 1U == total) {
					oid = head_oid;
				}
				auto commitDetails = QJsonObject{
					{
						"author",
						QJsonObject{
							{ "date", "2026-07-30T12:00:00Z" },
							{ "email", "author@example.invalid" },
							{ "name", "Octo Author" },
						},
					},
					{ "comment_count", 0 },
					{ "message",
					  index == 0U ? QString{ "Paged read\n\nBody" }
								  : QString{ "Paged read %1\n\nBody" }.arg(index) },
					{
						"tree",
						QJsonObject{
							{ "sha", QString::fromUtf8(base_oid) },
							{ "url", "https://api.github.com/tree" },
						},
					},
					{ "url", "https://api.github.com/commit" },
				};
				auto commit = QJsonObject{
					{ "author", rest_actor("U_commit_author", "octo-commit-author") },
					{ "comments_url", "https://api.github.com/commit/comments" },
					{ "commit", commitDetails },
					{ "html_url", "https://github.com/commit" },
					{ "node_id", QString{ "COMMIT_%1" }.arg(index) },
					{ "sha", QString::fromStdString(oid) },
					{ "url", "https://api.github.com/commit" },
				};
				if (index == 0U
					&& Scenario == fixture_scenario::commit_missing_required) {
					commit.remove("commit");
				} else if (index == 0U
						   && Scenario == fixture_scenario::commit_wrong_required) {
					commit.insert("sha", "not-a-commit-oid");
				}
				commits.push_back(std::move(commit));
			}
			if (end == total) {
				return success_response(json(commits));
			}
			const auto link = "<https://" + Authority + aRequest.Wire.Path
				+ "?per_page=100&page=" + std::to_string(page + 1U)
				+ ">; rel=\"next\"";
			return success_response(
				json(commits), link, page_completeness::has_next_page);
		}

		[[nodiscard]] auto reviews_response() const -> wire_response
		{
			auto review = QJsonObject{
				{ "author_association", "COLLABORATOR" },
				{ "body", "Looks good" },
				{ "commit_id", QString::fromUtf8(head_oid) },
				{ "html_url",
				  "https://github.com/octo-org/review-repository/pull/17#review" },
				{ "id", 4001 },
				{ "node_id", "R_review" },
				{ "state", "APPROVED" },
				{ "submitted_at", "2026-07-30T13:00:00Z" },
				{ "user", rest_actor("U_reviewer", "octo-reviewer-2") },
			};
			if (Scenario == fixture_scenario::review_missing_required) {
				review.remove("state");
			} else if (Scenario == fixture_scenario::review_wrong_required) {
				review.insert("body", 42);
			}
			return success_response(json(QJsonArray{ review }));
		}

		[[nodiscard]] auto issue_comments_response() const -> wire_response
		{
			auto comment = QJsonObject{
				{ "author_association", "MEMBER" },
				{ "body", "Tracking comment" },
				{ "created_at", "2026-07-30T13:30:00Z" },
				{ "html_url",
				  "https://github.com/octo-org/review-repository/issues/17#comment" },
				{ "id", 5001 },
				{ "issue_url",
				  "https://api.github.com/repos/octo-org/review-repository/"
				  "issues/17" },
				{ "node_id", "IC_comment" },
				{ "updated_at", "2026-07-30T13:30:00Z" },
				{ "user", rest_actor("U_commenter", "octo-commenter") },
			};
			if (Scenario == fixture_scenario::issue_comment_missing_required) {
				comment.remove("body");
			} else if (Scenario == fixture_scenario::issue_comment_wrong_required) {
				comment.insert("user", "not-an-actor");
			}
			return success_response(json(QJsonArray{ comment }));
		}

		[[nodiscard]] auto checks_response() const -> wire_response
		{
			auto check = QJsonObject{
				{ "completed_at", "2026-07-30T13:45:00Z" },
				{ "conclusion", "success" },
				{ "details_url", "https://checks.example.invalid/6001" },
				{ "head_sha", QString::fromUtf8(head_oid) },
				{ "html_url", "https://api.github.com/check-runs/6001" },
				{ "id", 6001 },
				{ "name", "native-build" },
				{ "node_id", "CHK_native" },
				{ "started_at", "2026-07-30T13:40:00Z" },
				{ "status", "completed" },
			};
			if (Scenario == fixture_scenario::check_missing_required) {
				check.remove("status");
			} else if (Scenario == fixture_scenario::check_wrong_required) {
				check.insert("head_sha", QString::fromUtf8(base_oid));
			}
			auto envelope = QJsonObject{
				{ "check_runs", QJsonArray{ check } },
				{ "total_count", 1 },
				{ "url",
				  "https://api.github.com/repos/octo-org/review-repository/"
				  "commits/head/check-runs" },
			};
			if (Scenario == fixture_scenario::check_envelope_missing_required) {
				envelope.remove("total_count");
			} else if (Scenario == fixture_scenario::check_envelope_wrong_required) {
				envelope.insert("check_runs", "not-an-array");
			}
			return success_response(json(envelope));
		}

		[[nodiscard]] auto search_response(const github_wire_request& aRequest) const
			-> wire_response
		{
			const auto paged = Scenario == fixture_scenario::search_plus_link
				|| Scenario == fixture_scenario::search_query_semantic_drift
				|| Scenario == fixture_scenario::search_query_double_encoded
				|| Scenario == fixture_scenario::search_query_double_plus;
			const auto total           = paged ? std::size_t{ 101U } : std::size_t{ 1U };
			const auto page            = aRequest.Wire.Semantic.Page;
			const auto begin           = (static_cast<std::size_t>(page) - 1U) * std::size_t{ 100U };
			const auto end             = std::min(begin + std::size_t{ 100U }, total);
			const auto canonicalCasing = Scenario == fixture_scenario::search_canonical_repository_casing;
			const auto owner           = canonicalCasing ? std::string{ "Octo-Org" }
														 : std::string{ "octo-org" };
			const auto repository      = canonicalCasing
				? std::string{ "Review-Repository" }
				: std::string{ "review-repository" };
			auto       repositoryUrl   = std::string{ "https://" };
			if (Authority == "api.github.com") {
				repositoryUrl.append("api.github.com/repos/");
			} else {
				repositoryUrl.append(Authority).append("/api/v3/repos/");
			}
			repositoryUrl.append(owner).append("/").append(repository);
			const auto webAuthority = Authority == "api.github.com"
				? std::string{ "github.com" }
				: Authority;
			QJsonArray items;
			for (auto index = begin; index < end; ++index) {
				const auto number  = 18U + index;
				auto       itemUrl = std::string{ "https://" };
				itemUrl.append(webAuthority)
					.append("/")
					.append(owner)
					.append("/")
					.append(repository)
					.append("/pull/")
					.append(std::to_string(number));
				auto item = QJsonObject{
					{ "comments", 0 },
					{ "draft", false },
					{ "html_url",
					  QString::fromStdString(itemUrl) },
					{ "id", static_cast<qint64>(7'001U + index) },
					{ "node_id", QString{ "PR_%1" }.arg(number) },
					{ "number", static_cast<qint64>(number) },
					{
						"pull_request",
						QJsonObject{
							{ "diff_url", "https://github.com/pull.diff" },
							{ "html_url", "https://github.com/pull" },
							{ "patch_url", "https://github.com/pull.patch" },
							{ "url",
							  QString::fromStdString(repositoryUrl + "/pulls/"
													 + std::to_string(number)) },
						},
					},
					{ "repository_url", QString::fromStdString(repositoryUrl) },
					{ "state", "open" },
					{ "title", QString{ "Open pull request %1" }.arg(number) },
					{ "url", "https://api.github.com/issues" },
				};
				if (index == 0U
					&& Scenario == fixture_scenario::search_missing_required) {
					item.remove("number");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_required) {
					item.insert("draft", "false");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_authority) {
					item.insert("repository_url",
								"https://attacker.invalid/repos/octo-org/"
								"review-repository");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_owner) {
					item.insert(
						"repository_url",
						"https://api.github.com/repos/other/review-repository");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_repository) {
					item.insert("repository_url",
								"https://api.github.com/repos/octo-org/other");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_number_url) {
					item.insert(
						"html_url",
						"https://github.com/octo-org/review-repository/pull/999");
				} else if (index == 0U
						   && Scenario
							   == fixture_scenario::search_missing_pull_request) {
					item.remove("pull_request");
				} else if (index == 0U
						   && Scenario
							   == fixture_scenario::search_wrong_pull_request_url) {
					auto pullRequest = item.value("pull_request").toObject();
					pullRequest.insert("url",
									   "https://api.github.com/repos/octo-org/"
									   "review-repository/pulls/999");
					item.insert("pull_request", pullRequest);
				}
				items.push_back(std::move(item));
			}
			auto envelope = QJsonObject{
				{ "incomplete_results", false },
				{ "items", items },
				{ "repository_selection", "selected" },
				{ "total_count", static_cast<qint64>(total) },
			};
			if (Scenario == fixture_scenario::search_envelope_missing_required) {
				envelope.remove("incomplete_results");
			} else if (Scenario == fixture_scenario::search_envelope_wrong_required) {
				envelope.insert("items", QJsonObject{ });
			}
			if (end == total) {
				return success_response(json(envelope));
			}

			const auto marker = aRequest.Wire.Query.find("&q=");
			expect(marker != std::string::npos,
				   "search fixture has a canonical q parameter");
			auto        queryValue = aRequest.Wire.Query.substr(marker + 3U);
			std::size_t space      = 0U;
			while ((space = queryValue.find("%20", space)) != std::string::npos) {
				queryValue.replace(space, 3U, "+");
				++space;
			}
			if (Scenario == fixture_scenario::search_query_semantic_drift) {
				const auto state = queryValue.find("is%3Aopen");
				expect(state != std::string::npos,
					   "search fixture has canonical open state");
				queryValue.replace(
					state, std::string_view{ "is%3Aopen" }.size(), "is%3Aclosed");
			} else if (Scenario == fixture_scenario::search_query_double_encoded) {
				const auto colon = queryValue.find("%3A");
				expect(colon != std::string::npos,
					   "search fixture has canonical encoded colon");
				queryValue.replace(colon, 3U, "%253A");
			} else if (Scenario == fixture_scenario::search_query_double_plus) {
				const auto plus = queryValue.find('+');
				expect(plus != std::string::npos,
					   "search fixture has canonical form space");
				queryValue.insert(plus, "+");
			}
			const auto link = "<https://" + Authority + aRequest.Wire.Path
				+ "?per_page=100&page=" + std::to_string(page + 1U)
				+ "&q=" + queryValue + ">; rel=\"next\"";
			return success_response(
				json(envelope), link, page_completeness::has_next_page);
		}

		[[nodiscard]] auto graphql_initial_response() const -> wire_response
		{
			std::uint64_t total = 101U;
			if (Scenario == fixture_scenario::repeated_cursor) {
				total = 201U;
			} else if (Scenario == fixture_scenario::truncated_total) {
				total = 102U;
			}
			QJsonArray comments;
			for (std::size_t index = 0U; index < 100U; ++index) {
				auto comment = comment_node(index);
				if ((Scenario == fixture_scenario::line_comment_original_line_only
					 || Scenario == fixture_scenario::line_comment_no_coordinates)
					&& index == 0U) {
					comment.insert("line", QJsonValue::Null);
					comment.insert(
						"originalLine",
						Scenario == fixture_scenario::line_comment_no_coordinates
							? QJsonValue{ QJsonValue::Null }
							: QJsonValue{ 42 });
				} else if (Scenario == fixture_scenario::file_comment_original_line) {
					comment.insert("line", QJsonValue::Null);
					comment.insert(
						"originalLine", index == 0U ? QJsonValue{ 42 } : QJsonValue::Null);
				} else if (Scenario
						   == fixture_scenario::paged_file_comment_original_line) {
					comment.insert("line", QJsonValue::Null);
					comment.insert("originalLine", QJsonValue::Null);
				}
				comments.push_back(std::move(comment));
			}
			const auto missing = Scenario == fixture_scenario::missing_cursor;
			auto       thread  = QJsonObject{
				{
					"comments",
					connection(total,
							   std::move(comments),
							   true,
							   missing
								   ? std::nullopt
								   : std::optional<std::string_view>{ "CURSOR_1" }),
				},
				{ "id", "T_1" },
				{ "isOutdated", false },
				{ "isResolved", false },
				{ "line", 42 },
				{ "originalLine", 41 },
				{ "path", "src/file0.cpp" },
				{ "diffSide", "RIGHT" },
				{ "subjectType", "LINE" },
				{ "viewerCanUnresolve", true },
			};
			if (Scenario == fixture_scenario::thread_side_missing) {
				thread.remove("diffSide");
			} else if (Scenario == fixture_scenario::thread_side_null) {
				thread.insert("diffSide", QJsonValue::Null);
			} else if (Scenario == fixture_scenario::thread_side_wrong) {
				thread.insert("diffSide", 7);
			} else if (Scenario == fixture_scenario::thread_side_unknown) {
				thread.insert("diffSide", "UNCHANGED");
			} else if (Scenario
					   == fixture_scenario::current_thread_original_line_only) {
				thread.insert("line", QJsonValue::Null);
			} else if (Scenario == fixture_scenario::file_comment_original_line
					   || Scenario
						   == fixture_scenario::paged_file_comment_original_line) {
				thread.insert("subjectType", "FILE");
				thread.insert("line", QJsonValue::Null);
				thread.insert("originalLine", QJsonValue::Null);
			}
			const auto pullRequest = QJsonObject{
				{ "headRefOid",
				  Scenario == fixture_scenario::graphql_head_moved
					  ? QString::fromUtf8(base_oid)
					  : QString::fromUtf8(head_oid) },
				{ "id", "PR_17" },
				{
					"reviewThreads",
					connection(1U,
							   QJsonArray{ thread },
							   false,
							   std::optional<std::string_view>{ "THREAD_END" }),
				},
			};
			auto envelope = QJsonObject{
				{
					"data",
					QJsonObject{
						{
							"repository",
							QJsonObject{
								{ "id", "R_repository" },
								{ "pullRequest", pullRequest },
							},
						},
					},
				},
			};
			if (Scenario == fixture_scenario::graphql_extensions) {
				envelope.insert(
					"extensions",
					QJsonObject{ { "requestId", "graphql-initial-request" } });
			} else if (Scenario == fixture_scenario::graphql_errors_type_rate_limited) {
				envelope = QJsonObject{
					{
						"errors",
						QJsonArray{
							QJsonObject{
								{ "message", "secondary limit" },
								{ "type", "RATE_LIMITED" },
							},
						},
					},
				};
			} else if (Scenario
					   == fixture_scenario::graphql_partial_errors_extension_type) {
				envelope.insert("errors",
								QJsonArray{
									QJsonObject{
										{
											"extensions",
											QJsonObject{ { "type", "RATE_LIMITED" } },
										},
										{ "message", "secondary limit" },
									},
								});
			} else if (Scenario
					   == fixture_scenario::graphql_partial_errors_extension_code) {
				envelope.insert("errors",
								QJsonArray{
									QJsonObject{
										{
											"extensions",
											QJsonObject{ { "code", "RATE_LIMITED" } },
										},
										{ "message", "secondary limit" },
									},
								});
			} else if (Scenario == fixture_scenario::graphql_primary_rate_metadata) {
				envelope.insert("errors",
								QJsonArray{
									QJsonObject{
										{ "message", "primary quota exhausted" },
										{ "type", "RATE_LIMITED" },
									},
								});
			} else if (Scenario == fixture_scenario::graphql_malformed_errors) {
				envelope.insert("errors", QJsonObject{ { "type", "RATE_LIMITED" } });
			} else if (Scenario == fixture_scenario::graphql_unknown_envelope_field) {
				envelope.insert("trace", "not-selected");
			}
			auto response = success_response(json(envelope));
			if (Scenario == fixture_scenario::graphql_primary_rate_metadata) {
				for (auto& header : response.Headers) {
					if (header.Name == "X-RateLimit-Remaining") {
						header.Value = "0";
					}
				}
			}
			return response;
		}

		[[nodiscard]] auto graphql_comments_response() const -> wire_response
		{
			const auto repeat = Scenario == fixture_scenario::repeated_cursor;
			QJsonArray comments;
			const auto count = repeat ? 100U : 1U;
			for (std::size_t index = 0U; index < count; ++index) {
				auto comment = comment_node(100U + index);
				if (index == 0U
					&& (Scenario
							== fixture_scenario::paged_line_comment_original_line_only
						|| Scenario == fixture_scenario::paged_line_comment_no_coordinates
						|| Scenario
							== fixture_scenario::paged_file_comment_original_line)) {
					comment.insert("line", QJsonValue::Null);
					comment.insert(
						"originalLine",
						Scenario == fixture_scenario::paged_line_comment_no_coordinates
							? QJsonValue{ QJsonValue::Null }
							: QJsonValue{ 42 });
				}
				comments.push_back(std::move(comment));
			}
			const auto total             = repeat
				? 201U
				: (Scenario == fixture_scenario::truncated_total ? 102U : 101U);
			const auto commentConnection = connection(total,
													  std::move(comments),
													  repeat,
													  repeat ? std::optional<std::string_view>{ "CURSOR_1" }
															 : std::optional<std::string_view>{ "COMMENT_END" });
			auto       envelope          = QJsonObject{
				{
					"data",
					QJsonObject{
						{
							"node",
							QJsonObject{
								{ "comments", commentConnection },
								{ "id", "T_1" },
							},
						},
						{
							"repository",
							QJsonObject{
								{ "id", "R_repository" },
								{
									"pullRequest",
									QJsonObject{
										{ "headRefOid", QString::fromUtf8(head_oid) },
										{ "id", "PR_17" },
									},
								},
							},
						},
					},
				},
			};
			if (Scenario == fixture_scenario::graphql_extensions) {
				envelope.insert(
					"extensions",
					QJsonObject{ { "requestId", "graphql-comments-request" } });
			}
			return success_response(json(envelope));
		}

		/// Serves the signed-out inline review-comment listing.
		///
		/// The pages are supplied by the test rather than generated here
		/// because each case is a specific promoted or synthetic payload, and
		/// the point of most of them is a linkage the generator would have to
		/// be taught to break. A page other than the last carries a `Link`
		/// header so exhaustion is driven by the header the real service
		/// sends, never by the page count the test knows.
		[[nodiscard]] auto review_comments_response(
			const github_wire_request& aRequest) const -> wire_response
		{
			if (ReviewCommentStatus != 200) {
				return {
					.Status       = ReviewCommentStatus,
					.Headers      = ReviewCommentHeaders,
					.Body         = ReviewCommentPages.empty()
						? std::string{ "{}" }
						: ReviewCommentPages.front(),
					.Completeness = page_completeness::complete,
				};
			}
			const auto page  = aRequest.Wire.Semantic.Page;
			const auto index = static_cast<std::size_t>(page) - 1U;
			if (index >= ReviewCommentPages.size()) {
				return success_response(json(QJsonArray{ }));
			}
			if (index + 1U == ReviewCommentPages.size()) {
				return success_response(ReviewCommentPages[index]);
			}
			const auto link = "<https://" + Authority + aRequest.Wire.Path
				+ "?per_page=100&page=" + std::to_string(page + 1U)
				+ ">; rel=\"next\"";
			return success_response(ReviewCommentPages[index],
									link,
									page_completeness::has_next_page);
		}

		[[nodiscard]] auto response_for(const github_wire_request& aRequest) const
			-> wire_response
		{
			auto response = [&]() -> wire_response {
				switch (aRequest.Wire.Operation) {
				case read_operation::get_repository:
					return repository_response();
				case read_operation::get_pull_request:
					return pull_request_response();
				case read_operation::list_pull_request_files:
					return files_response(aRequest);
				case read_operation::list_pull_request_commits:
					return commits_response(aRequest);
				case read_operation::list_reviews:
					return reviews_response();
				case read_operation::list_issue_comments:
					return issue_comments_response();
				case read_operation::list_review_threads:
					return aRequest.Wire.Semantic.GraphqlPage->Kind
							== github_graphql_page_kind::initial
						? graphql_initial_response()
						: graphql_comments_response();
				case read_operation::list_checks:
					return checks_response();
				case read_operation::search_pull_requests:
					return search_response(aRequest);
				case read_operation::list_review_comments:
					return review_comments_response(aRequest);
				case read_operation::get_viewer_identity:
					break;
				}
				return success_response(json(QJsonObject{ }));
			}();
			const auto resource = aRequest.Wire.Operation == read_operation::list_review_threads
				? "graphql"
				: (aRequest.Wire.Operation
						   == read_operation::search_pull_requests
					   ? "search"
					   : "core");
			for (auto& header : response.Headers) {
				if (header.Name == "X-RateLimit-Resource") {
					header.Value = resource;
				}
			}
			return response;
		}

		fixture_scenario Scenario = fixture_scenario::success;
		std::string      Authority;
	};

	[[nodiscard]] auto provider_failure_of(const github_paged_read_result& aResult)
		-> std::optional<application::github_provider_failure>
	{
		if (aResult.has_value()) {
			return std::nullopt;
		}
		const auto* error = std::get_if<application::github_provider_error>(&aResult.error());
		return error == nullptr ? std::nullopt : std::optional{ error->Failure };
	}

	[[nodiscard]] auto provider_error_of(const github_paged_read_result& aResult)
		-> std::optional<application::github_provider_error>
	{
		if (aResult.has_value()) {
			return std::nullopt;
		}
		const auto* error = std::get_if<application::github_provider_error>(&aResult.error());
		return error == nullptr ? std::nullopt : std::optional{ *error };
	}

	auto test_metadata_and_viewer() -> void
	{
		const auto response = success_response(json(QJsonObject{
			{ "avatar_url", "https://avatars.githubusercontent.com/u/1001" },
			{ "bio", "Reviews pull requests" },
			{ "created_at", "2011-01-25T18:44:36Z" },
			{ "email", QJsonValue::Null },
			{ "followers", 20 },
			{ "following", 0 },
			{ "html_url", "https://github.com/octo-reviewer" },
			{ "id", 1001 },
			{ "login", "octo-reviewer" },
			{ "name", "Octo Reviewer" },
			{ "node_id", "U_viewer" },
			{ "public_repos", 2 },
			{ "type", "User" },
		}));
		const auto metadata = parse_github_response_metadata(response);
		expect(metadata.has_value() && metadata->RateLimit.has_value()
				   && metadata->RateLimit->Resource
					   == application::github_rate_limit_resource::core
				   && metadata->RateLimit->Limit == 5000U
				   && metadata->RateLimit->Remaining == 4999U
				   && metadata->RateLimit->ResetUnixSeconds == 2000000000U
				   && !metadata->Link.has_value(),
			   "strict metadata derives exact rate facts");
		const auto viewer = normalize_observed_github_viewer(response);
		expect(viewer.has_value()
				   && viewer->Viewer.Account == core::github_account_id{ 1001U }
				   && viewer->Viewer.Node
					   == application::github_actor_id{ "U_viewer" }
				   && viewer->Viewer.Login == "octo-reviewer"
				   && viewer->Viewer.DisplayName == "Octo Reviewer"
				   && viewer->RateLimits.Core == metadata->RateLimit,
			   "viewer identity and account derive only from raw response");

		auto duplicate = response;
		duplicate.Headers.push_back({
			.Name  = "x-ratelimit-limit",
			.Value = "5000",
		});
		expect(!parse_github_response_metadata(duplicate).has_value(),
			   "case-folded duplicate metadata is rejected");

		auto unknown = response;
		unknown.Headers.push_back({
			.Name  = "Server",
			.Value = "not-allowlisted",
		});
		expect(!parse_github_response_metadata(unknown).has_value(),
			   "unknown response metadata is rejected");

		auto invalidOnly = wire_response{
			.Status  = 200,
			.Headers = {
				{
					.Name  = "Content-Type",
					.Value = "application/json",
				},
				{
					.Name  = "X-RateLimit-Limit",
					.Value = "not-a-number",
				},
			},
			.Body         = "{}",
			.Completeness = page_completeness::complete,
		};
		expect(!parse_github_response_metadata(invalidOnly).has_value(),
			   "present invalid-only rate metadata cannot collapse to absent");
		invalidOnly.Headers[1U] = {
			.Name  = "X-RateLimit-Resource",
			.Value = "unknown",
		};
		expect(!parse_github_response_metadata(invalidOnly).has_value(),
			   "present unknown-only rate resource cannot collapse to absent");
		invalidOnly.Headers[1U] = {
			.Name  = "Retry-After",
			.Value = "later",
		};
		expect(!parse_github_response_metadata(invalidOnly).has_value(),
			   "present invalid Retry-After cannot collapse to absent");

		auto enterpriseWithoutRate = response;
		std::erase_if(enterpriseWithoutRate.Headers,
					  [](const auto& aHeader) { return aHeader.Name.starts_with("X-RateLimit-"); });
		const auto enterpriseViewer = normalize_observed_github_viewer(
			enterpriseWithoutRate, server_contract::ghes_3_18);
		expect(enterpriseViewer.has_value()
				   && !application::has_observations(enterpriseViewer->RateLimits),
			   "GHES may explicitly omit its disabled rate-limit tuple");
		expect(!normalize_observed_github_viewer(enterpriseWithoutRate,
												 server_contract::github_com)
					.has_value(),
			   "github.com viewer responses require a complete rate-limit tuple");

		auto wrongResource                 = response;
		wrongResource.Headers.back().Value = "search";
		expect(!normalize_observed_github_viewer(wrongResource).has_value(),
			   "viewer response cannot inject a non-core rate-limit bucket");

		auto injected                  = response;
		injected.Headers.front().Value = "application/json\r\nInjected: yes";
		expect(!parse_github_response_metadata(injected).has_value(),
			   "metadata injection is rejected");

		auto missing = response;
		missing.Body = json(QJsonObject{
			{ "id", 1001 },
			{ "login", "octo-reviewer" },
			{ "name", "Octo Reviewer" },
		});
		expect(!normalize_observed_github_viewer(missing).has_value(),
			   "viewer normalization rejects a missing required field");

		auto wrong = response;
		wrong.Body = json(QJsonObject{
			{ "id", "1001" },
			{ "login", "octo-reviewer" },
			{ "name", "Octo Reviewer" },
			{ "node_id", "U_viewer" },
		});
		expect(!normalize_observed_github_viewer(wrong).has_value(),
			   "viewer normalization rejects a wrong required field type");

		auto duplicateJson = response;
		duplicateJson.Body = R"({"id":1001,"id":1002,"login":"octo-reviewer","name":"Octo Reviewer","node_id":"U_viewer"})";
		expect(!normalize_observed_github_viewer(duplicateJson).has_value(),
			   "surplus-field projection retains duplicate-key preflight");

		auto secondary             = response;
		secondary.Status           = 403;
		secondary.Headers[2].Value = "42";
		secondary.Body             = R"({"message":"You have exceeded a secondary rate limit. Please wait."})";
		const auto limited         = normalize_observed_github_viewer(secondary);
		expect(!limited.has_value()
				   && limited.error().Failure
					   == application::github_provider_failure::rate_limit
				   && limited.error().RetryAfterSeconds
					   == std::optional<std::uint32_t>{ 60U }
				   && limited.error().RateLimitCause.has_value()
				   && limited.error().RateLimitCause->Resource
					   == application::github_rate_limit_resource::core
				   && limited.error().RateLimitCause->SecondaryLimited
				   && limited.error().RateLimits.Core
					   == limited.error().RateLimitCause,
			   "viewer secondary-limit body is typed with bounded fallback");

		secondary.Body              = R"({"message":"You have exceeded a secondary rate limit.","message":"forged"})";
		const auto duplicateMessage = normalize_observed_github_viewer(secondary);
		expect(!duplicateMessage.has_value()
				   && duplicateMessage.error().Failure
					   == application::github_provider_failure::permission
				   && !duplicateMessage.error().RetryAfterSeconds.has_value(),
			   "duplicate viewer error keys cannot claim secondary throttling");
	}

	auto test_capability_shaped_query() -> void
	{
		const auto request = compile_read_request(semantic_read_request{
			.Server            = server_contract::github_com,
			.Operation         = read_operation::list_review_threads,
			.Owner             = "octo-org",
			.Repository        = "review-repository",
			.PullRequestNumber = 17U,
			.HeadOid           = { },
			.SearchTerms       = { },
			.Page              = 1U,
			.GraphqlPage       = github_graphql_page_request{
				.Kind  = github_graphql_page_kind::initial,
				.Shape = {
					.ReviewThreads       = true,
					.ViewedFileState     = false,
					.ResolvePermission   = false,
					.UnresolvePermission = true,
				},
				.AfterCursor = { },
				.ThreadId    = { },
			},
		});
		expect(request.has_value(), "capability-shaped query compiles");
		expect(request->Body.find("reviewThreads") != std::string::npos
				   && request->Body.find("headRefOid") != std::string::npos
				   && request->Body.find("viewerCanUnresolve") != std::string::npos
				   && request->Body.find("viewerCanResolve") == std::string::npos
				   && request->Body.find("viewerViewedState") == std::string::npos
				   && request->Body.find("files(") == std::string::npos,
			   "unsupported GraphQL fields are absent from canonical bytes");

		auto invalid = request->Semantic;
		invalid.Page = 2U;
		expect(!compile_read_request(invalid).has_value(),
			   "initial GraphQL page is sequence one only");
	}

	auto test_graphql_head_is_bound_to_rest_snapshot() -> void
	{
		auto       transport = paged_fixture_transport{ fixture_scenario::graphql_head_moved };
		const auto result    = perform_paged_github_read(transport,
														 profile(),
														 provider_request(),
														 core::provider_snapshot_id{ 55U },
														 std::stop_token{ });
		expect(
			!result.has_value()
				&& provider_failure_of(result)
					== application::github_provider_failure::malformed_response
				&& !transport.Requests.empty()
				&& transport.Requests.back().Wire.Operation
					== read_operation::list_review_threads,
			"a force-pushed GraphQL thread page cannot attach head-B line numbers "
			"to the REST snapshot retained for head A");
	}

	auto test_successful_exhaustive_read() -> void
	{
		auto       transport = paged_fixture_transport{ fixture_scenario::success };
		const auto result    = perform_paged_github_read(transport,
														 profile(),
														 provider_request(),
														 core::provider_snapshot_id{ 55U },
														 std::stop_token{ });
		expect(result.has_value(), "all REST and GraphQL pages normalize");
		expect(result->Repository.Repository == core::repository_id{ 2001U }
				   && result->PullRequest.PullRequest
					   == core::pull_request_id{ 3001U }
				   && result->PullRequest.HeadOid
					   == application::commit_oid{ std::string{ head_oid } },
			   "repository, pull request, and head derive from raw payloads");
		expect(result->Files.size() == 101U
				   && result->Files.front().ViewedState
					   == application::github_file_viewed_state::unavailable,
			   "REST Link pages exhaust and omitted viewed state is unavailable");
		expect(
			result->ReviewThreadsAvailability
					== application::github_collection_availability::available
				&& result->ReviewThreads.size() == 1U
				&& result->ReviewThreads.front().Comments.size() == 101U
				&& result->ReviewThreads.front().ViewerResolvePermission
					== application::github_permission_state::unavailable
				&& result->ReviewThreads.front().ViewerUnresolvePermission
					== application::github_permission_state::allowed,
			"nested comments exhaust and omitted permissions remain unavailable");
		expect(result->ReviewThreads.front().Line == 42U
				   && result->ReviewThreads.front().OriginalLine == 41U
				   && result->ReviewThreads.front().DiffSide
					   == application::github_diff_side::right,
			   "current and original line remain distinct beside the exact diff side");
		auto originalOnlyTransport = paged_fixture_transport{
			fixture_scenario::line_comment_original_line_only
		};
		const auto originalOnlyResult = perform_paged_github_read(
			originalOnlyTransport,
			profile(),
			provider_request(),
			core::provider_snapshot_id{ 56U },
			std::stop_token{ });
		expect(originalOnlyResult.has_value()
				   && !originalOnlyResult->ReviewThreads.front().Comments.front().Line.has_value()
				   && originalOnlyResult->ReviewThreads.front().Comments.front().OriginalLine
					   == 42U,
			   "a line-thread comment with only its nullable original line matches the strict contract");
		auto noCoordinateTransport = paged_fixture_transport{
			fixture_scenario::line_comment_no_coordinates
		};
		const auto noCoordinateResult = perform_paged_github_read(
			noCoordinateTransport,
			profile(),
			provider_request(),
			core::provider_snapshot_id{ 57U },
			std::stop_token{ });
		expect(noCoordinateResult.has_value()
				   && !noCoordinateResult->ReviewThreads.front()
						   .Comments.front()
						   .Line.has_value()
				   && !noCoordinateResult->ReviewThreads.front()
						   .Comments.front()
						   .OriginalLine.has_value(),
			   "a LINE reply may omit both nullable coordinates and inherit the root anchor");
		auto pagedOriginalOnlyTransport = paged_fixture_transport{
			fixture_scenario::paged_line_comment_original_line_only
		};
		const auto pagedOriginalOnlyResult = perform_paged_github_read(
			pagedOriginalOnlyTransport,
			profile(),
			provider_request(),
			core::provider_snapshot_id{ 57U },
			std::stop_token{ });
		expect(pagedOriginalOnlyResult.has_value()
				   && !pagedOriginalOnlyResult->ReviewThreads.front()
						   .Comments.at(100U)
						   .Line.has_value()
				   && pagedOriginalOnlyResult->ReviewThreads.front()
						   .Comments.at(100U)
						   .OriginalLine
					   == 42U,
			   "a nested LINE comment page accepts originalLine-only coordinates");
		auto pagedNoCoordinateTransport = paged_fixture_transport{
			fixture_scenario::paged_line_comment_no_coordinates
		};
		const auto pagedNoCoordinateResult = perform_paged_github_read(
			pagedNoCoordinateTransport,
			profile(),
			provider_request(),
			core::provider_snapshot_id{ 58U },
			std::stop_token{ });
		expect(pagedNoCoordinateResult.has_value()
				   && !pagedNoCoordinateResult->ReviewThreads.front()
						   .Comments.at(100U)
						   .Line.has_value()
				   && !pagedNoCoordinateResult->ReviewThreads.front()
						   .Comments.at(100U)
						   .OriginalLine.has_value(),
			   "a paged LINE reply may omit both coordinates and inherit the root anchor");
		expect(result->Commits.size() == 1U
				   && result->Commits.front().Headline == "Paged read"
				   && result->Reviews.size() == 1U
				   && result->Reviews.front().Review
					   == application::github_review_id{ "R_review" }
				   && result->IssueComments.size() == 1U
				   && result->IssueComments.front().Comment
					   == application::github_comment_id{ "IC_comment" }
				   && result->Checks.size() == 1U
				   && result->Checks.front().Check
					   == application::github_check_id{ "CHK_native" }
				   && result->SearchResults.size() == 1U
				   && result->SearchResults.front().Number
					   == core::pull_request_number{ 18U },
			   "86-key repositories, 81-key nested PR repositories, ignored "
			   "101-element arrays, and other REST supersets project only portable "
			   "records");
		expect(result->RateLimits.Core.has_value()
				   && result->RateLimits.Core->Remaining == 4999U
				   && result->RateLimits.Search.has_value()
				   && result->RateLimits.Search->Remaining == 4999U
				   && result->RateLimits.Graphql.has_value()
				   && result->RateLimits.Graphql->Remaining == 4999U
				   && transport.Requests.size() == 11U
				   && std::ranges::none_of(
					   transport.Requests,
					   [](const github_wire_request& aRequest) {
						   return aRequest.Wire.Operation
							   == read_operation::get_viewer_identity;
					   }),
			   "snapshot resource rates are observed and pull read does not repeat "
			   "viewer lookup");

		auto       numericTransport = paged_fixture_transport{ fixture_scenario::numeric_repository_link };
		const auto numericResult    = perform_paged_github_read(numericTransport,
																profile(),
																provider_request(),
																core::provider_snapshot_id{ 56U },
																std::stop_token{ });
		expect(
			numericResult.has_value()
				&& std::ranges::any_of(
					numericTransport.Requests,
					[](const github_wire_request& aRequest) {
						return aRequest.Wire.Operation
							== read_operation::list_pull_request_files
							&& aRequest.Wire.Semantic.Page == 2U
							&& aRequest.Wire.Path
							== "/repos/octo-org/review-repository/"
							   "pulls/17/files";
					}),
			"same-authority numeric repository Links are accepted while the next "
			"request remains canonical");

		auto       pageFirstTransport = paged_fixture_transport{ fixture_scenario::page_first_query };
		const auto pageFirstResult    = perform_paged_github_read(pageFirstTransport,
																  profile(),
																  provider_request(),
																  core::provider_snapshot_id{ 57U },
																  std::stop_token{ });
		expect(pageFirstResult.has_value(),
			   "Link query parsing accepts page before per_page as well as the "
			   "reversed GitHub order");

		auto enterpriseTransport = paged_fixture_transport{
			fixture_scenario::success, "ghe.example.invalid"
		};
		const auto enterpriseResult = perform_paged_github_read(
			enterpriseTransport,
			enterprise_profile(),
			provider_request(
				application::github_server_contract::ghes_3_18_2022_11_28),
			core::provider_snapshot_id{ 56U },
			std::stop_token{ });
		expect(
			enterpriseResult.has_value()
				&& enterpriseTransport.Requests.front().Wire.Path.starts_with(
					"/api/v3/")
				&& std::ranges::any_of(
					enterpriseTransport.Requests,
					[](const github_wire_request& aRequest) { return aRequest.Wire.Path == "/api/graphql"; }),
			"GHES exhausts the same production pipeline under its exact namespace");
	}

	auto test_raw_structural_limits() -> void
	{
		auto       largePatch       = paged_fixture_transport{ fixture_scenario::large_ignored_patch };
		const auto largePatchResult = perform_paged_github_read(largePatch,
																profile(),
																provider_request(),
																core::provider_snapshot_id{ 59U },
																std::stop_token{ });
		expect(largePatchResult.has_value()
				   && largePatchResult->Files.size() == 101U,
			   "ignored 500 KiB raw patches remain bounded by page bytes rather "
			   "than normalized text limits");

		for (const auto scenario : {
				 fixture_scenario::raw_object_ceiling_exceeded,
				 fixture_scenario::raw_depth_ceiling_exceeded,
			 }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 60U },
															 std::stop_token{ });
			expect(
				!result.has_value()
					&& provider_failure_of(result)
						== application::github_provider_failure::response_limit,
				"raw object breadth and depth remain bounded");
		}

		auto pageLimits                          = github_paged_read_limits{ };
		pageLimits.MaximumPageBytes              = 512U;
		pageLimits.MaximumAggregateResponseBytes = 1'024U;
		auto       oversizedPage                 = paged_fixture_transport{ fixture_scenario::success };
		const auto pageResult                    = perform_paged_github_read(oversizedPage,
																			 profile(),
																			 provider_request(),
																			 core::provider_snapshot_id{ 61U },
																			 std::stop_token{ },
																			 pageLimits);
		expect(!pageResult.has_value()
				   && provider_failure_of(pageResult)
					   == application::github_provider_failure::response_limit
				   && oversizedPage.Requests.size() == 1U,
			   "raw body bytes remain bounded before JSON projection");

		auto aggregateLimits                          = github_paged_read_limits{ };
		aggregateLimits.MaximumPageBytes              = 128U * 1'024U;
		aggregateLimits.MaximumAggregateResponseBytes = 128U * 1'024U;
		auto       aggregate                          = paged_fixture_transport{ fixture_scenario::aggregate_byte_ceiling };
		const auto aggregateResult                    = perform_paged_github_read(aggregate,
																				  profile(),
																				  provider_request(),
																				  core::provider_snapshot_id{ 62U },
																				  std::stop_token{ },
																				  aggregateLimits);
		expect(!aggregateResult.has_value()
				   && provider_failure_of(aggregateResult)
					   == application::github_provider_failure::response_limit
				   && aggregate.Requests.size() == 2U,
			   "aggregate bytes remain bounded even when each page is admissible");

		for (const auto scenario : {
				 fixture_scenario::selected_title_over_limit,
				 fixture_scenario::selected_page_records_exceeded,
			 }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 68U },
															 std::stop_token{ });
			expect(
				!result.has_value()
					&& provider_failure_of(result)
						== application::github_provider_failure::response_limit,
				"selected text and page-record limits remain strict after raw "
				"structural limits are widened");
		}
	}

	auto test_search_form_links() -> void
	{
		auto       githubForm = paged_fixture_transport{ fixture_scenario::search_plus_link };
		const auto accepted   = perform_paged_github_read(githubForm,
														  profile(),
														  provider_request(),
														  core::provider_snapshot_id{ 63U },
														  std::stop_token{ });
		expect(accepted.has_value() && accepted->SearchResults.size() == 101U,
			   "Search Links accept GitHub's exact plus-for-space q form");

		for (const auto scenario : {
				 fixture_scenario::search_query_semantic_drift,
				 fixture_scenario::search_query_double_encoded,
				 fixture_scenario::search_query_double_plus,
			 }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 64U },
															 std::stop_token{ });
			expect(!result.has_value()
					   && provider_failure_of(result)
						   == application::github_provider_failure::
							   malformed_response,
				   "altered and noncanonical Search q forms fail closed");
		}
	}

	auto test_graphql_error_envelopes() -> void
	{
		auto       extensions      = paged_fixture_transport{ fixture_scenario::graphql_extensions };
		const auto extensionResult = perform_paged_github_read(extensions,
															   profile(),
															   provider_request(),
															   core::provider_snapshot_id{ 69U },
															   std::stop_token{ });
		expect(extensionResult.has_value(),
			   "GraphQL data accepts only the selected optional extensions "
			   "envelope");

		for (const auto scenario : {
				 fixture_scenario::graphql_errors_type_rate_limited,
				 fixture_scenario::graphql_partial_errors_extension_type,
				 fixture_scenario::graphql_partial_errors_extension_code,
			 }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 70U },
															 std::stop_token{ });
			const auto error     = provider_error_of(result);
			expect(error.has_value()
					   && error->Failure
						   == application::github_provider_failure::rate_limit
					   && error->RetryAfterSeconds
						   == std::optional<std::uint32_t>{ 60U }
					   && error->RateLimitCause.has_value()
					   && error->RateLimitCause->Resource
						   == application::github_rate_limit_resource::graphql
					   && error->RateLimitCause->SecondaryLimited
					   && error->RateLimits.Core.has_value()
					   && error->RateLimits.Graphql == error->RateLimitCause,
				   "GraphQL RATE_LIMITED markers fail atomically with a bounded "
				   "secondary-rate fallback");
		}

		auto primary = paged_fixture_transport{
			fixture_scenario::graphql_primary_rate_metadata
		};
		const auto primaryResult = perform_paged_github_read(primary,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 71U },
															 std::stop_token{ });
		const auto primaryError  = provider_error_of(primaryResult);
		expect(primaryError.has_value()
				   && primaryError->Failure
					   == application::github_provider_failure::rate_limit
				   && !primaryError->RetryAfterSeconds.has_value()
				   && primaryError->RateLimitCause.has_value()
				   && primaryError->RateLimitCause->Resource
					   == application::github_rate_limit_resource::graphql
				   && !primaryError->RateLimitCause->SecondaryLimited,
			   "GraphQL errors plus exhausted primary metadata are typed as a "
			   "primary rate limit");

		for (const auto scenario : {
				 fixture_scenario::graphql_malformed_errors,
				 fixture_scenario::graphql_unknown_envelope_field,
			 }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 72U },
															 std::stop_token{ });
			expect(!result.has_value()
					   && provider_failure_of(result)
						   == application::github_provider_failure::
							   malformed_response,
				   "malformed errors and unselected GraphQL envelope fields fail "
				   "closed");
		}
	}

	auto test_raw_status_classification() -> void
	{
		const auto verify =
			[](const fixture_scenario                     aScenario,
			   const application::github_provider_failure aExpectedFailure,
			   const std::optional<std::uint32_t>         aExpectedRetry) {
				auto       transport = paged_fixture_transport{ aScenario };
				const auto result    = perform_paged_github_read(transport,
																 profile(),
																 provider_request(),
																 core::provider_snapshot_id{ 73U },
																 std::stop_token{ });
				const auto error     = provider_error_of(result);
				expect(error.has_value() && error->Failure == aExpectedFailure
						   && error->RetryAfterSeconds == aExpectedRetry
						   && transport.Requests.size() == 1U,
					   "raw HTTP status classification preserves exact typed failure "
					   "and bounded delay");
			};

		verify(fixture_scenario::rest_secondary_no_retry,
			   application::github_provider_failure::rate_limit,
			   60U);
		verify(fixture_scenario::rest_secondary_with_retry,
			   application::github_provider_failure::rate_limit,
			   17U);
		verify(fixture_scenario::rest_secondary_primary_metadata,
			   application::github_provider_failure::rate_limit,
			   std::nullopt);
		verify(fixture_scenario::rest_secondary_duplicate_message,
			   application::github_provider_failure::permission,
			   std::nullopt);
		verify(fixture_scenario::rest_generic_403,
			   application::github_provider_failure::permission,
			   std::nullopt);
		verify(fixture_scenario::rest_429,
			   application::github_provider_failure::rate_limit,
			   60U);
		verify(fixture_scenario::rest_408,
			   application::github_provider_failure::timeout,
			   std::nullopt);
		verify(fixture_scenario::rest_503_retry,
			   application::github_provider_failure::unavailable,
			   9U);
		verify(fixture_scenario::rest_504,
			   application::github_provider_failure::timeout,
			   std::nullopt);
	}

	auto test_collection_count_contract() -> void
	{
		auto       exact    = paged_fixture_transport{ fixture_scenario::collection_cap_exact };
		const auto accepted = perform_paged_github_read(exact,
														profile(),
														provider_request(),
														core::provider_snapshot_id{ 65U },
														std::stop_token{ });
		expect(accepted.has_value() && accepted->Files.size() == 3'000U
				   && accepted->Commits.size() == 250U
				   && !accepted->CapabilityEvidence.ReadShape.ViewedFileState,
			   "advertised file and commit counts remain exact at REST caps even "
			   "when GraphQL viewed state is unavailable");

		for (const auto scenario : {
				 fixture_scenario::file_count_mismatch,
				 fixture_scenario::commit_count_mismatch,
			 }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 66U },
															 std::stop_token{ });
			expect(!result.has_value()
					   && provider_failure_of(result)
						   == application::github_provider_failure::
							   malformed_response,
				   "terminal REST collections must equal advertised PR counts");
		}

		for (const auto scenario : {
				 fixture_scenario::file_count_above_cap,
				 fixture_scenario::commit_count_above_cap,
			 }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 67U },
															 std::stop_token{ });
			expect(
				!result.has_value()
					&& provider_failure_of(result)
						== application::github_provider_failure::response_limit
					&& transport.Requests.size() == 2U,
				"advertised collection counts above GitHub security caps fail "
				"before capped list endpoints are trusted");
		}
	}

	auto test_atomic_failures() -> void
	{
		const auto malformed = {
			fixture_scenario::cross_authority_link,
			fixture_scenario::wrong_numeric_repository_link,
			fixture_scenario::path_drift_link,
			fixture_scenario::fragment_link,
			fixture_scenario::userinfo_link,
			fixture_scenario::port_link,
			fixture_scenario::duplicate_link_query,
			fixture_scenario::extra_link_query,
			fixture_scenario::encoded_link_query,
			fixture_scenario::unknown_completeness,
			fixture_scenario::missing_cursor,
			fixture_scenario::repeated_cursor,
			fixture_scenario::truncated_total,
			fixture_scenario::thread_side_missing,
			fixture_scenario::thread_side_null,
			fixture_scenario::thread_side_wrong,
			fixture_scenario::thread_side_unknown,
			fixture_scenario::current_thread_original_line_only,
			fixture_scenario::file_comment_original_line,
			fixture_scenario::paged_file_comment_original_line,
		};
		for (const auto scenario : malformed) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 55U },
															 std::stop_token{ });
			expect(
				!result.has_value()
					&& provider_failure_of(result)
						== application::github_provider_failure::
							malformed_response,
				"hostile pagination fails without publishing a partial snapshot");
		}

		auto       once    = paged_fixture_transport{ fixture_scenario::network_failure };
		const auto network = perform_paged_github_read(once,
													   profile(),
													   provider_request(),
													   core::provider_snapshot_id{ 55U },
													   std::stop_token{ });
		expect(!network.has_value()
				   && provider_failure_of(network)
					   == application::github_provider_failure::network
				   && once.Requests.size() == 1U,
			   "first page failure returns directly without an internal retry");

		auto       wrongResource    = paged_fixture_transport{ fixture_scenario::wrong_resource_wire_error };
		const auto rejectedResource = perform_paged_github_read(wrongResource,
																profile(),
																provider_request(),
																core::provider_snapshot_id{ 55U },
																std::stop_token{ });
		expect(
			!rejectedResource.has_value()
				&& provider_failure_of(rejectedResource)
					== application::github_provider_failure::malformed_response
				&& wrongResource.Requests.size() == 1U,
			"wire errors cannot inject a rate bucket unrelated to the operation");

		auto       lateWire    = paged_fixture_transport{ fixture_scenario::late_search_wire_failure };
		const auto lateFailure = perform_paged_github_read(lateWire,
														   profile(),
														   provider_request(),
														   core::provider_snapshot_id{ 55U },
														   std::stop_token{ });
		const auto lateError   = provider_error_of(lateFailure);
		expect(lateError.has_value()
				   && lateError->Failure
					   == application::github_provider_failure::network
				   && lateError->RateLimits.Core.has_value()
				   && lateError->RateLimits.Graphql.has_value()
				   && lateError->RateLimits.Search.has_value()
				   && lateError->RateLimits.Search->Remaining == 17U,
			   "late exact wire errors preserve every accumulated resource bucket");

		const auto malformedRest = {
			fixture_scenario::repository_missing_required,
			fixture_scenario::repository_wrong_required,
			fixture_scenario::pull_request_missing_required,
			fixture_scenario::pull_request_wrong_required,
			fixture_scenario::pull_request_missing_file_total,
			fixture_scenario::pull_request_wrong_file_total,
			fixture_scenario::pull_request_missing_commit_total,
			fixture_scenario::pull_request_wrong_commit_total,
			fixture_scenario::actor_missing_required,
			fixture_scenario::actor_wrong_required,
			fixture_scenario::file_missing_required,
			fixture_scenario::file_wrong_required,
			fixture_scenario::commit_missing_required,
			fixture_scenario::commit_wrong_required,
			fixture_scenario::review_missing_required,
			fixture_scenario::review_wrong_required,
			fixture_scenario::issue_comment_missing_required,
			fixture_scenario::issue_comment_wrong_required,
			fixture_scenario::check_missing_required,
			fixture_scenario::check_wrong_required,
			fixture_scenario::check_envelope_missing_required,
			fixture_scenario::check_envelope_wrong_required,
			fixture_scenario::search_missing_required,
			fixture_scenario::search_wrong_required,
			fixture_scenario::search_wrong_authority,
			fixture_scenario::search_wrong_owner,
			fixture_scenario::search_wrong_repository,
			fixture_scenario::search_wrong_number_url,
			fixture_scenario::search_missing_pull_request,
			fixture_scenario::search_wrong_pull_request_url,
			fixture_scenario::search_envelope_missing_required,
			fixture_scenario::search_envelope_wrong_required,
		};
		for (const auto scenario : malformedRest) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 55U },
															 std::stop_token{ });
			expect(!result.has_value()
					   && provider_failure_of(result)
						   == application::github_provider_failure::
							   malformed_response,
				   "each REST parser rejects missing and wrong required fields");
		}
		auto       lateLocal        = paged_fixture_transport{ fixture_scenario::search_wrong_authority };
		const auto lateLocalFailure = perform_paged_github_read(lateLocal,
																profile(),
																provider_request(),
																core::provider_snapshot_id{ 55U },
																std::stop_token{ });
		const auto lateLocalError   = provider_error_of(lateLocalFailure);
		expect(
			lateLocalError.has_value()
				&& lateLocalError->RateLimits.Core.has_value()
				&& lateLocalError->RateLimits.Graphql.has_value()
				&& lateLocalError->RateLimits.Search.has_value(),
			"late local parse failures retain accumulated resource observations");

		auto canonicalCasing = paged_fixture_transport{
			fixture_scenario::search_canonical_repository_casing
		};
		const auto canonicalCasingResult = perform_paged_github_read(canonicalCasing,
																	 profile(),
																	 provider_request(),
																	 core::provider_snapshot_id{ 55U },
																	 std::stop_token{ });
		expect(canonicalCasingResult.has_value()
				   && canonicalCasingResult->Repository.Owner == "Octo-Org"
				   && canonicalCasingResult->Repository.Name
					   == "Review-Repository"
				   && canonicalCasingResult->SearchResults.size() == 1U,
			   "search URLs bind to server-canonical repository casing");

		auto launderedRequest                                         = provider_request();
		launderedRequest.CapabilityEvidence.ReadShape.ViewedFileState = true;
		auto       laundered                                          = paged_fixture_transport{ fixture_scenario::success };
		const auto rejected                                           = perform_paged_github_read(laundered,
																								  profile(),
																								  launderedRequest,
																								  core::provider_snapshot_id{ 55U },
																								  std::stop_token{ });
		expect(!rejected.has_value() && laundered.Requests.empty(),
			   "caller cannot launder a fabricated capability-shaped read");
	}

	// --- RFC-BPR-0017: the signed-out inline review conversation ----------

	[[nodiscard]] auto anonymous_fixture_bytes(const std::string_view aRelative)
		-> std::string
	{
		const auto path = std::filesystem::path{ BPR_RFC0017_ANONYMOUS_FIXTURE_ROOT }
			/ aRelative;
		std::ifstream stream{ path, std::ios::binary };
		expect(stream.good(), "an RFC-BPR-0017 fixture is readable");
		return std::string{ std::istreambuf_iterator<char>{ stream },
							std::istreambuf_iterator<char>{ } };
	}

	/// Builds the request a signed-out profile presents.
	///
	/// Every capability fact is unsupported, so the projected read shape is
	/// all-false and the projection helper stops before a GraphQL body
	/// exists. No account transport is attached, because an anonymous
	/// authority holds no lease to present.
	[[nodiscard]] auto anonymous_provider_request()
		-> application::github_pull_request_provider_request
	{
		auto request = provider_request();
		for (auto& fact : request.CapabilityEvidence.Facts) {
			fact.State = core::github_capability_state::unsupported;
		}
		for (auto& fact : request.CapabilityEvidence.SchemaFacts) {
			fact.State = core::github_capability_state::unsupported;
		}
		const auto observation = application::github_capability_probe_observation{
			.Authority   = request.CapabilityEvidence.Authority,
			.ObservedAt  = request.CapabilityEvidence.ObservedAt,
			.Facts       = request.CapabilityEvidence.Facts,
			.SchemaFacts = request.CapabilityEvidence.SchemaFacts,
		};
		request.CapabilityEvidence.Capabilities
			= application::projected_capabilities(observation);
		request.CapabilityEvidence.ReadShape
			= application::projected_read_shape(observation);
		request.AccountTransport = nullptr;
		expect(request.CapabilityEvidence.ReadShape
				   == application::github_read_shape{ },
			   "a signed-out profile carries an all-false read shape");
		return request;
	}

	[[nodiscard]] auto anonymous_read(paged_fixture_transport& aTransport)
		-> github_paged_read_result
	{
		return perform_paged_github_read(
			aTransport,
			profile(),
			anonymous_provider_request(),
			core::provider_snapshot_id{ 55U },
			std::stop_token{ },
			github_paged_read_limits{ },
			github_provider_read_authority::anonymous_public);
	}

	[[nodiscard]] auto anonymous_read_of(
		const std::initializer_list<std::string_view> aPages)
		-> github_paged_read_result
	{
		auto transport = paged_fixture_transport{ fixture_scenario::success };
		for (const auto page : aPages) {
			transport.ReviewCommentPages.push_back(anonymous_fixture_bytes(page));
		}
		return anonymous_read(transport);
	}

	auto test_anonymous_review_comments_group_by_linkage() -> void
	{
		auto transport = paged_fixture_transport{ fixture_scenario::success };
		transport.ReviewCommentPages.push_back(
			anonymous_fixture_bytes("synthetic/review-comments-linked.json"));
		const auto result = anonymous_read(transport);
		expect(result.has_value(), "a signed-out read normalizes");
		expect(
			result->ReviewCommentsAvailability
					== application::github_collection_availability::available
				&& result->ReviewCommentGroups.size() == 2U,
			"the flat listing becomes one group per root");
		const auto& first  = result->ReviewCommentGroups.front();
		const auto& second = result->ReviewCommentGroups.back();
		expect(
			first.Root.Comment == application::github_comment_id{ "101" }
				&& first.Replies.size() == 2U
				&& first.Replies.front().Comment
					== application::github_comment_id{ "103" }
				&& first.Replies.back().Comment
					== application::github_comment_id{ "104" },
			"replies join the root their linkage names, in server order");
		expect(
			second.Root.Comment == application::github_comment_id{ "102" }
				&& second.Replies.empty(),
			"a root that sits between two replies to another root keeps none of "
			"them: position decides no membership");
		expect(
			!first.Root.ReplyTo.has_value()
				&& first.Replies.front().ReplyTo
					== std::optional{ application::github_comment_id{ "101" } },
			"the linkage that produced the grouping is retained, not consumed");
		expect(
			result->ReviewThreadsAvailability
					== application::github_collection_availability::unavailable
				&& result->ReviewThreads.empty(),
			"the REST conversation never reaches the GraphQL thread collection");
		expect(
			std::ranges::all_of(
				result->Files,
				[](const application::github_file_record& aFile) {
					return aFile.ViewedState
						== application::github_file_viewed_state::unavailable;
				}),
			"viewed-file state stays unavailable on a signed-out read");
		expect(
			std::ranges::none_of(
				transport.Requests,
				[](const github_wire_request& aRequest) {
					return aRequest.Wire.Operation
						== read_operation::list_review_threads;
				}),
			"no GraphQL request is constructed for a signed-out read");
		expect(
			std::ranges::any_of(
				transport.Requests,
				[](const github_wire_request& aRequest) {
					return aRequest.Wire.Operation
						== read_operation::list_review_comments
						&& aRequest.Wire.Path
						== "/repos/octo-org/review-repository/pulls/17/comments"
						&& aRequest.Wire.Method == http_method::get;
				}),
			"the signed-out read issues the REST review-comment route");
	}

	auto test_anonymous_review_comments_exhaust_their_pages() -> void
	{
		auto transport = paged_fixture_transport{ fixture_scenario::success };
		transport.ReviewCommentPages.push_back(
			anonymous_fixture_bytes("synthetic/review-comments-page-1.json"));
		transport.ReviewCommentPages.push_back(
			anonymous_fixture_bytes("synthetic/review-comments-page-2.json"));
		const auto result = anonymous_read(transport);
		expect(result.has_value() && result->ReviewCommentGroups.size() == 100U,
			   "both pages contribute to the same conversation");
		expect(
			result->ReviewCommentGroups.front().Replies.size() == 1U
				&& result->ReviewCommentGroups.front().Replies.front().Comment
					== application::github_comment_id{ "203" }
				&& result->ReviewCommentGroups[1U].Replies.size() == 1U
				&& result->ReviewCommentGroups[1U].Replies.front().Comment
					== application::github_comment_id{ "204" }
				&& result->ReviewCommentGroups.back().Replies.empty(),
			"a reply on the second page still joins its root from the first");
		auto pages = std::size_t{ 0U };
		for (const auto& request : transport.Requests) {
			if (request.Wire.Operation == read_operation::list_review_comments) {
				++pages;
			}
		}
		expect(pages == 2U,
			   "the listing is exhausted by its Link header and stops there");
	}

	auto test_anonymous_observed_empty_is_not_unavailable() -> void
	{
		const auto observed = anonymous_read_of(
			{ "synthetic/review-comments-observed-empty.json" });
		expect(
			observed.has_value()
				&& observed->ReviewCommentsAvailability
					== application::github_collection_availability::available
				&& observed->ReviewCommentGroups.empty(),
			"a conversation that was read and holds nothing is observed, "
			"not unavailable");

		auto       authenticated = paged_fixture_transport{ fixture_scenario::success };
		const auto credentialed  = perform_paged_github_read(
			authenticated,
			profile(),
			provider_request(),
			core::provider_snapshot_id{ 55U },
			std::stop_token{ });
		expect(
			credentialed.has_value()
				&& credentialed->ReviewCommentsAvailability
					== application::github_collection_availability::unavailable
				&& credentialed->ReviewCommentGroups.empty(),
			"a collection the authenticated read never asks for is unavailable");
		expect(
			std::ranges::none_of(
				authenticated.Requests,
				[](const github_wire_request& aRequest) {
					return aRequest.Wire.Operation
						== read_operation::list_review_comments;
				}),
			"the authenticated read issues no REST review-comment request");
	}

	auto test_anonymous_review_comment_linkage_violations_are_refused() -> void
	{
		constexpr std::array<std::string_view, 7> hostile{
			"synthetic/review-comments-self-reply.json",
			"synthetic/review-comments-cyclic.json",
			"synthetic/review-comments-missing-parent.json",
			"synthetic/review-comments-reply-to-reply.json",
			"synthetic/review-comments-duplicate-id.json",
			"synthetic/review-comments-malformed-linkage.json",
			"synthetic/review-comments-missing-position.json",
		};
		for (const auto fixture : hostile) {
			auto transport = paged_fixture_transport{ fixture_scenario::success };
			transport.ReviewCommentPages.push_back(anonymous_fixture_bytes(fixture));
			const auto result = anonymous_read(transport);
			expect(
				!result.has_value()
					&& provider_failure_of(result)
						== application::github_provider_failure::malformed_response,
				"a conversation whose linkage cannot be trusted is refused "
				"rather than repaired");
		}
	}

	auto test_anonymous_review_comment_anchor_is_read_not_defaulted() -> void
	{
		const auto result = anonymous_read_of(
			{ "synthetic/review-comments-outdated.json" });
		expect(
			result.has_value() && result->ReviewCommentGroups.size() == 1U
				&& result->ReviewCommentGroups.front().Root.Outdated
				&& !result->ReviewCommentGroups.front().Root.Line.has_value(),
			"a null position is read as an outdated anchor rather than "
			"defaulted to a live one");
	}

	auto test_anonymous_review_comments_absent_is_not_disclosed() -> void
	{
		auto transport                 = paged_fixture_transport{ fixture_scenario::success };
		transport.ReviewCommentStatus  = 404;
		transport.ReviewCommentHeaders = response_headers(std::nullopt);
		transport.ReviewCommentPages.push_back(
			anonymous_fixture_bytes("captures/absent-not-disclosed.json"));
		const auto result = anonymous_read(transport);
		expect(
			!result.has_value()
				&& provider_failure_of(result)
					== application::github_provider_failure::permission,
			"a 404 is reported as permission, which asserts neither existence "
			"nor absence");
	}

	auto test_anonymous_review_comments_rate_limit_is_typed() -> void
	{
		auto transport                = paged_fixture_transport{ fixture_scenario::success };
		transport.ReviewCommentStatus = 403;
		transport.ReviewCommentHeaders
			= { { .Name = "Content-Type", .Value = "application/json" },
				{ .Name = "X-RateLimit-Limit", .Value = "60" },
				{ .Name = "X-RateLimit-Remaining", .Value = "0" },
				{ .Name = "X-RateLimit-Reset", .Value = "1787546881" },
				{ .Name = "X-RateLimit-Resource", .Value = "core" } };
		transport.ReviewCommentPages.push_back(
			anonymous_fixture_bytes("captures/rate-exhaustion.json"));
		const auto result = anonymous_read(transport);
		const auto error  = provider_error_of(result);
		expect(
			error.has_value()
				&& error->Failure
					== application::github_provider_failure::rate_limit,
			"an exhausted budget is a rate-limit diagnostic, not a network or "
			"authentication failure");
		expect(
			error->RateLimitCause.has_value()
				&& error->RateLimitCause->Limit == 60U
				&& error->RateLimitCause->ResetUnixSeconds == 1'787'546'881U
				&& error->RateLimitCause->Remaining == 0U
				&& !error->RateLimitCause->SecondaryLimited,
			"the refusal names the limit and the reset the caller must wait for");
		expect(!error->RetryAfterSeconds.has_value(),
			   "a primary budget refusal offers no retry");
		auto attempts = std::size_t{ 0U };
		for (const auto& request : transport.Requests) {
			if (request.Wire.Operation == read_operation::list_review_comments) {
				++attempts;
			}
		}
		expect(attempts == 1U, "nothing retries past the limit");
	}

	auto test_anonymous_review_comments_oversized_body_is_refused() -> void
	{
		auto transport = paged_fixture_transport{ fixture_scenario::success };
		auto oversized = std::string{ "[{\"id\":1,\"path\":\"README\",\"position\":1,"
									  "\"user\":null,\"body\":\"" };
		oversized.append(std::size_t{ 5U } * 1024U * 1024U, 'x');
		oversized.append("\"}]");
		transport.ReviewCommentPages.push_back(std::move(oversized));
		const auto result = anonymous_read(transport);
		expect(
			!result.has_value()
				&& provider_failure_of(result)
					== application::github_provider_failure::response_limit,
			"an oversized listing is refused by the response limits");
	}
}

auto main() -> int
{
	test_metadata_and_viewer();
	test_capability_shaped_query();
	test_successful_exhaustive_read();
	test_graphql_head_is_bound_to_rest_snapshot();
	test_raw_structural_limits();
	test_search_form_links();
	test_graphql_error_envelopes();
	test_raw_status_classification();
	test_collection_count_contract();
	test_atomic_failures();
	test_anonymous_review_comments_group_by_linkage();
	test_anonymous_review_comments_exhaust_their_pages();
	test_anonymous_observed_empty_is_not_unavailable();
	test_anonymous_review_comment_linkage_violations_are_refused();
	test_anonymous_review_comment_anchor_is_read_not_defaulted();
	test_anonymous_review_comments_absent_is_not_disclosed();
	test_anonymous_review_comments_rate_limit_is_typed();
	test_anonymous_review_comments_oversized_body_is_refused();
	return EXIT_SUCCESS;
}
