#include "../../src/application/src/github_projection_mapping.hpp"
#include "../support/review_tour_fixture.hpp"
#include "bpr/application/review_queue_facts.hpp"
#include "bpr/application/review_tour_import.hpp"
#include "fixture_json.hpp"
#include "github_anonymous_read_admission.hpp"
#include "github_http_helpers.hpp"
#include "github_paged_read.hpp"
#include "github_wire_request_validation.hpp"

#include "bpr/core/anonymous_read_policy.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::tests;
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

	auto fill_to_member_count(fixture_object& aObject, const std::string& aPrefix,
							  const std::size_t aTarget) -> void
	{
		std::size_t index = 0;
		while (aObject.size() < aTarget) {
			aObject.insert_or_assign(aPrefix + "_" + std::to_string(index),
									 "ignored raw GitHub field " + std::to_string(index));
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
								  const std::string_view aLogin) -> fixture_object
	{
		return {
			{ "avatar_url", "https://avatars.githubusercontent.com/u/123" },
			{ "id", 123 },
			{ "login", std::string{ aLogin } },
			{ "node_id", std::string{ aNode } },
			{ "type", "User" },
		};
	}

	[[nodiscard]] auto comment_node(const std::size_t aIndex) -> fixture_object
	{
		return {
			{ "author", nullptr },
			{ "bodyText", std::string{ "comment " } + std::to_string(aIndex) },
			{ "id", std::string{ "C_" } + std::to_string(aIndex) },
			{ "line", 42 },
			{ "originalLine", nullptr },
			{ "outdated", false },
			{ "path", "src/file0.cpp" },
			{ "replyTo", nullptr },
		};
	}

	[[nodiscard]] auto page_info(const bool                            aHasNext,
								 const std::optional<std::string_view> aCursor)
		-> fixture_object
	{
		return {
			{
				"endCursor",
				aCursor.has_value() ? fixture_value(std::string{ *aCursor })
									: fixture_value(nullptr),
			},
			{ "hasNextPage", aHasNext },
		};
	}

	[[nodiscard]] auto connection(const std::uint64_t                   aTotal,
								  fixture_array                         aNodes,
								  const bool                            aHasNext,
								  const std::optional<std::string_view> aCursor)
		-> fixture_object
	{
		return {
			{ "nodes", std::move(aNodes) },
			{ "pageInfo", page_info(aHasNext, aCursor) },
			{ "totalCount", static_cast<std::uint64_t>(aTotal) },
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
		current_thread_no_coordinates,
		outdated_thread_no_coordinates,
		line_comment_original_line_only,
		line_comment_no_coordinates,
		file_comment_original_line,
		file_thread_placeholder,
		file_thread_wrong_line,
		file_thread_wrong_original_line,
		file_thread_wrong_side,
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
		commit_count_above_cap_short,
		commit_count_above_cap_overlong,
		commit_count_above_cap_wrong_head,
		collection_cap_exact,
		measured_largest_files_page,
		measured_file_cap_pages,
	};

	// [[BPR-0892]]. Page shapes measured from real pull requests on
	// 2026-09-23. microsoft/TypeScript#51387 (656 files, +282,700 -284,842)
	// gave the largest page: 1,176,273 bytes, 1,048,253 of them patch, the
	// largest patch 100,071 bytes, and 61 of its 100 files sent with no patch.
	// NixOS/nixpkgs#322537 (21,294 files) stops at GitHub's 3,000-file limit:
	// 30 pages, 12,665,213 bytes in all, the largest page 723,104 bytes.
	inline constexpr std::size_t measured_largest_page_bytes     = 1'176'273U;
	inline constexpr std::size_t measured_largest_patch_bytes    = 100'071U;
	inline constexpr std::size_t measured_files_without_patch    = 61U;
	inline constexpr std::size_t measured_cap_largest_page_bytes = 723'104U;
	inline constexpr std::size_t measured_cap_total_bytes        = 12'665'213U;

	// Diff-shaped patch text of `aBytes` characters. Its newlines escape to two
	// bytes each, so the served JSON is somewhat larger.
	[[nodiscard]] auto measured_patch(const std::size_t aBytes) -> std::string
	{
		constexpr std::string_view line  = "+    const auto value = compute(input, 42);\n";
		std::string                patch = "@@ -1,400 +1,400 @@\n";
		while (patch.size() + line.size() <= aBytes) {
			patch += line;
		}
		patch.append(aBytes - patch.size(), 'x');
		return patch;
	}

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
			auto response = response_for(aRequest);
			if (RewriteResponse) {
				RewriteResponse(aRequest, response);
			}
			return github_wire_success{
				.Correlation = aRequest.Correlation,
				.Response    = std::move(response),
			};
		}

		std::vector<github_wire_request> Requests;
		// Alter literal wire bytes after serialization so rounding or parser
		// normalization in fixture construction cannot hide boundary failures.
		std::function<void(const github_wire_request&, wire_response&)> RewriteResponse;
		// The signed-out inline review-comment listing the fixture serves.
		// Each RFC-BPR-0017 case is a specific promoted or synthetic payload,
		// so the pages are handed in rather than generated.
		std::vector<std::string> ReviewCommentPages;
		int                      ReviewCommentStatus = 200;
		std::vector<http_header> ReviewCommentHeaders;
		// The raw size of each files page served, for the measured scenarios.
		mutable std::vector<std::size_t> FilesPageBytes;

	private:
		[[nodiscard]] auto advertised_file_count() const noexcept -> std::uint32_t
		{
			if (Scenario == fixture_scenario::collection_cap_exact
				|| Scenario == fixture_scenario::measured_file_cap_pages) {
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
			if (Scenario == fixture_scenario::commit_count_above_cap
				|| Scenario == fixture_scenario::commit_count_above_cap_short
				|| Scenario == fixture_scenario::commit_count_above_cap_overlong
				|| Scenario == fixture_scenario::commit_count_above_cap_wrong_head) {
				return 9'311U;
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
						: fixture_json(fixture_object{
							  { "message",
								std::string(operationalMessage) },
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
				fixture_value nested = nullptr;
				for (std::size_t depth = 0U; depth < 33U; ++depth) {
					nested = fixture_object{ { "nested", nested } };
				}
				return success_response(fixture_json(nested.get_object()));
			}
			const auto canonicalCasing = Scenario == fixture_scenario::search_canonical_repository_casing;
			const auto ownerName       = canonicalCasing ? std::string{ "Octo-Org" }
														 : std::string{ "octo-org" };
			const auto repositoryName  = canonicalCasing
				? std::string{ "Review-Repository" }
				: std::string{ "review-repository" };
			auto       repository      = fixture_object{
				{ "archive_url",
				  "https://api.github.com/repos/octo-org/review-repository/"
				  "{archive_format}{/ref}" },
				{ "default_branch", "main" },
				{ "full_name",
				  std::string(ownerName + "/" + repositoryName) },
				{ "html_url", "https://github.com/octo-org/review-repository" },
				{ "id", 2001 },
				{ "name", std::string(repositoryName) },
				{ "node_id", "R_repository" },
				{ "owner", rest_actor("O_organization", ownerName) },
				{ "private", false },
				{ "visibility", "private" },
			};
			fixture_array ignoredTopics;
			for (std::size_t index = 0U; index < 101U; ++index) {
				ignoredTopics.push_back(std::string{ "topic-" } + std::to_string(index));
			}
			repository.insert_or_assign("ignored_topics", ignoredTopics);
			if (Scenario == fixture_scenario::aggregate_byte_ceiling) {
				repository.insert_or_assign("aggregate_padding",
											std::string(70'000U, 'r'));
			}
			if (Scenario == fixture_scenario::repository_missing_required) {
				repository.erase("default_branch");
			} else if (Scenario == fixture_scenario::repository_wrong_required) {
				repository.insert_or_assign("id", "not-an-integer");
			}
			fill_to_member_count(
				repository,
				"repository_surplus",
				Scenario == fixture_scenario::raw_object_ceiling_exceeded ? 257
																		  : 86);
			return success_response(fixture_json(repository));
		}

		[[nodiscard]] auto pull_request_response() const -> wire_response
		{
			auto baseRepository = fixture_object{
				{ "full_name", "octo-org/review-repository" },
				{ "html_url", "https://github.com/octo-org/review-repository" },
				{ "id", 2001 },
				{ "node_id", "R_repository" },
			};
			fill_to_member_count(baseRepository, "base_repo_surplus", 81);
			auto headRepository = baseRepository;
			fill_to_member_count(headRepository, "head_repo_surplus", 81);
			const auto base = fixture_object{
				{ "label", "octo-org:main" },
				{ "ref", "main" },
				{ "repo", baseRepository },
				{ "sha", std::string{ base_oid } },
				{ "user", rest_actor("O_organization", "octo-org") },
			};
			const auto head = fixture_object{
				{ "label", "octo-org:feature/paged-read" },
				{ "ref", "feature/paged-read" },
				{ "repo", headRepository },
				{ "sha", std::string{ head_oid } },
				{ "user", rest_actor("O_organization", "octo-org") },
			};
			auto author = rest_actor("U_author", "octo-author");
			if (Scenario == fixture_scenario::actor_missing_required) {
				author.erase("node_id");
			} else if (Scenario == fixture_scenario::actor_wrong_required) {
				author.insert_or_assign("login", 123);
			}
			auto pullRequest = fixture_object{
				{ "base", base },
				{ "body", "Synthetic body" },
				{ "changed_files", static_cast<std::uint64_t>(advertised_file_count()) },
				{ "comments", 1 },
				{ "commits", static_cast<std::uint64_t>(advertised_commit_count()) },
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
				pullRequest.insert_or_assign("aggregate_padding",
											 std::string(70'000U, 'p'));
			}
			if (Scenario == fixture_scenario::pull_request_missing_required) {
				pullRequest.erase("title");
			} else if (Scenario == fixture_scenario::pull_request_wrong_required) {
				pullRequest.insert_or_assign("draft", "false");
			} else if (Scenario == fixture_scenario::pull_request_missing_file_total) {
				pullRequest.erase("changed_files");
			} else if (Scenario == fixture_scenario::pull_request_wrong_file_total) {
				pullRequest.insert_or_assign("changed_files", "101");
			} else if (Scenario == fixture_scenario::pull_request_missing_commit_total) {
				pullRequest.erase("commits");
			} else if (Scenario == fixture_scenario::pull_request_wrong_commit_total) {
				pullRequest.insert_or_assign("commits", "1");
			} else if (Scenario == fixture_scenario::selected_title_over_limit) {
				pullRequest.insert_or_assign("title", std::string(1'025U, 't'));
			}
			return success_response(fixture_json(pullRequest));
		}

		[[nodiscard]] auto files_response(const github_wire_request& aRequest) const
			-> wire_response
		{
			fixture_array files;
			const auto    page  = aRequest.Wire.Semantic.Page;
			const auto    total = Scenario == fixture_scenario::collection_cap_exact
					|| Scenario == fixture_scenario::measured_file_cap_pages
				? std::size_t{ 3'000U }
				: std::size_t{ 101U };
			const auto    begin = (static_cast<std::size_t>(page) - 1U) * std::size_t{ 100U };
			const auto    end   = Scenario == fixture_scenario::selected_page_records_exceeded
					&& page == 1U
				? total
				: std::min(begin + std::size_t{ 100U }, total);
			for (auto index = begin; index < end; ++index) {
				auto file = fixture_object{
					{ "additions", 1 },
					{ "blob_url",
					  std::string{ "https://github.com/octo-org/review-repository/blob/head/src/file" } + std::to_string(index) + ".cpp" },
					{ "changes", 1 },
					{ "contents_url",
					  std::string{ "https://api.github.com/repos/octo-org/review-repository/contents/src/file" } + std::to_string(index) + ".cpp" },
					{ "deletions", 0 },
					{
						"filename",
						std::string{ "src/file" } + std::to_string(index) + ".cpp",
					},
					{ "patch", "@@ -1 +1 @@" },
					{ "raw_url",
					  std::string{ "https://github.com/octo-org/review-repository/raw/head/src/file" } + std::to_string(index) + ".cpp" },
					{ "sha", std::string{ head_oid } },
					{ "status", "modified" },
				};
				if (index == 0U
					&& Scenario == fixture_scenario::file_missing_required) {
					file.erase("filename");
				} else if (index == 0U
						   && Scenario == fixture_scenario::file_wrong_required) {
					file.insert_or_assign("additions", "one");
				} else if (index == 0U
						   && Scenario == fixture_scenario::large_ignored_patch) {
					file.insert_or_assign("patch", std::string(500U * 1'024U, 'p'));
				} else if (Scenario == fixture_scenario::measured_largest_files_page && page == 1U) {
					// 39 patches: the largest measured one, then the rest of
					// the measured page's bytes spread evenly, so this page is
					// at least as large as that one. 61 files carry none.
					const auto slot    = index - begin;
					const auto patched = std::size_t{ 100U } - measured_files_without_patch;
					if (slot >= patched) {
						file.erase("patch");
					} else {
						const auto rest = measured_largest_page_bytes - measured_largest_patch_bytes;
						file.insert_or_assign("patch", measured_patch(slot == 0U ? measured_largest_patch_bytes : rest / (patched - 1U)));
					}
				} else if (Scenario == fixture_scenario::measured_file_cap_pages) {
					// Every page as heavy as the heaviest measured one.
					file.insert_or_assign("patch", measured_patch(measured_cap_largest_page_bytes / 100U));
				}
				files.emplace_back(file);
			}
			if (Scenario == fixture_scenario::measured_largest_files_page
				|| Scenario == fixture_scenario::measured_file_cap_pages) {
				const auto bytes = fixture_json(files).size();
				FilesPageBytes.push_back(bytes);
			}
			const auto hasNext = end < total;
			if (!hasNext) {
				const auto completeness = Scenario == fixture_scenario::unknown_completeness
					? page_completeness::unknown
					: page_completeness::complete;
				return success_response(fixture_json(files), std::nullopt, completeness);
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
				fixture_json(files), link, page_completeness::has_next_page);
		}

		[[nodiscard]] auto
		commits_response(const github_wire_request& aRequest) const -> wire_response
		{
			const auto    total = Scenario == fixture_scenario::collection_cap_exact
					|| Scenario == fixture_scenario::commit_count_above_cap
					|| Scenario == fixture_scenario::commit_count_above_cap_wrong_head
				? std::size_t{ 250U }
				: Scenario == fixture_scenario::commit_count_above_cap_overlong
				? std::size_t{ 251U }
				: std::size_t{ 1U };
			const auto    page  = aRequest.Wire.Semantic.Page;
			const auto    begin = (static_cast<std::size_t>(page) - 1U) * std::size_t{ 100U };
			const auto    end   = std::min(begin + std::size_t{ 100U }, total);
			fixture_array commits;
			for (auto index = begin; index < end; ++index) {
				const auto numeric = std::to_string(index + 1U);
				auto       oid     = std::string(40U, '0');
				oid.replace(oid.size() - numeric.size(), numeric.size(), numeric);
				if (index + 1U == total
					&& Scenario != fixture_scenario::commit_count_above_cap_wrong_head) {
					oid = head_oid;
				}
				auto commitDetails = fixture_object{
					{
						"author",
						fixture_object{
							{ "date", "2026-07-30T12:00:00Z" },
							{ "email", "author@example.invalid" },
							{ "name", "Octo Author" },
						},
					},
					{ "comment_count", 0 },
					{ "message",
					  index == 0U ? std::string{ "Paged read\n\nBody" }
								  : std::string{ "Paged read " } + std::to_string(index) + "\n\nBody" },
					{
						"tree",
						fixture_object{
							{ "sha", std::string{ base_oid } },
							{ "url", "https://api.github.com/tree" },
						},
					},
					{ "url", "https://api.github.com/commit" },
				};
				auto commit = fixture_object{
					{ "author", rest_actor("U_commit_author", "octo-commit-author") },
					{ "comments_url", "https://api.github.com/commit/comments" },
					{ "commit", commitDetails },
					{ "html_url", "https://github.com/commit" },
					{ "node_id", std::string{ "COMMIT_" } + std::to_string(index) },
					{ "sha", std::string(oid) },
					{ "url", "https://api.github.com/commit" },
				};
				if (index == 0U
					&& Scenario == fixture_scenario::commit_missing_required) {
					commit.erase("commit");
				} else if (index == 0U
						   && Scenario == fixture_scenario::commit_wrong_required) {
					commit.insert_or_assign("sha", "not-a-commit-oid");
				}
				commits.emplace_back(commit);
			}
			if (end == total) {
				return success_response(fixture_json(commits));
			}
			const auto link = "<https://" + Authority + aRequest.Wire.Path
				+ "?per_page=100&page=" + std::to_string(page + 1U)
				+ ">; rel=\"next\"";
			return success_response(
				fixture_json(commits), link, page_completeness::has_next_page);
		}

		[[nodiscard]] auto reviews_response() const -> wire_response
		{
			auto review = fixture_object{
				{ "author_association", "COLLABORATOR" },
				{ "body", "Looks good" },
				{ "commit_id", std::string{ head_oid } },
				{ "html_url",
				  "https://github.com/octo-org/review-repository/pull/17#review" },
				{ "id", 4001 },
				{ "node_id", "R_review" },
				{ "state", "APPROVED" },
				{ "submitted_at", "2026-07-30T13:00:00Z" },
				{ "user", rest_actor("U_reviewer", "octo-reviewer-2") },
			};
			if (Scenario == fixture_scenario::review_missing_required) {
				review.erase("state");
			} else if (Scenario == fixture_scenario::review_wrong_required) {
				review.insert_or_assign("body", 42);
			}
			return success_response(fixture_json(fixture_array{ review }));
		}

		[[nodiscard]] auto issue_comments_response() const -> wire_response
		{
			auto comment = fixture_object{
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
				comment.erase("body");
			} else if (Scenario == fixture_scenario::issue_comment_wrong_required) {
				comment.insert_or_assign("user", "not-an-actor");
			}
			return success_response(fixture_json(fixture_array{ comment }));
		}

		[[nodiscard]] auto checks_response() const -> wire_response
		{
			auto check = fixture_object{
				{ "completed_at", "2026-07-30T13:45:00Z" },
				{ "conclusion", "success" },
				{ "details_url", "https://checks.example.invalid/6001" },
				{ "head_sha", std::string{ head_oid } },
				{ "html_url", "https://api.github.com/check-runs/6001" },
				{ "id", 6001 },
				{ "name", "native-build" },
				{ "node_id", "CHK_native" },
				{ "started_at", "2026-07-30T13:40:00Z" },
				{ "status", "completed" },
			};
			if (Scenario == fixture_scenario::check_missing_required) {
				check.erase("status");
			} else if (Scenario == fixture_scenario::check_wrong_required) {
				check.insert_or_assign("head_sha", std::string{ base_oid });
			}
			auto envelope = fixture_object{
				{ "check_runs", fixture_array{ check } },
				{ "total_count", 1 },
				{ "url",
				  "https://api.github.com/repos/octo-org/review-repository/"
				  "commits/head/check-runs" },
			};
			if (Scenario == fixture_scenario::check_envelope_missing_required) {
				envelope.erase("total_count");
			} else if (Scenario == fixture_scenario::check_envelope_wrong_required) {
				envelope.insert_or_assign("check_runs", "not-an-array");
			}
			return success_response(fixture_json(envelope));
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
			const auto    webAuthority = Authority == "api.github.com"
				? std::string{ "github.com" }
				: Authority;
			fixture_array items;
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
				auto item = fixture_object{
					{ "comments", 0 },
					{ "draft", false },
					{ "html_url",
					  std::string(itemUrl) },
					{ "id", static_cast<std::uint64_t>(7'001U + index) },
					{ "node_id", std::string{ "PR_" } + std::to_string(number) },
					{ "number", static_cast<std::uint64_t>(number) },
					{
						"pull_request",
						fixture_object{
							{ "diff_url", "https://github.com/pull.diff" },
							{ "html_url", "https://github.com/pull" },
							{ "patch_url", "https://github.com/pull.patch" },
							{ "url",
							  std::string(repositoryUrl + "/pulls/"
										  + std::to_string(number)) },
						},
					},
					{ "repository_url", std::string(repositoryUrl) },
					{ "state", "open" },
					{ "title", std::string{ "Open pull request " } + std::to_string(number) },
					{ "url", "https://api.github.com/issues" },
				};
				if (index == 0U
					&& Scenario == fixture_scenario::search_missing_required) {
					item.erase("number");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_required) {
					item.insert_or_assign("draft", "false");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_authority) {
					item.insert_or_assign("repository_url",
										  "https://attacker.invalid/repos/octo-org/"
										  "review-repository");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_owner) {
					item.insert_or_assign(
						"repository_url",
						"https://api.github.com/repos/other/review-repository");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_repository) {
					item.insert_or_assign("repository_url",
										  "https://api.github.com/repos/octo-org/other");
				} else if (index == 0U
						   && Scenario == fixture_scenario::search_wrong_number_url) {
					item.insert_or_assign(
						"html_url",
						"https://github.com/octo-org/review-repository/pull/999");
				} else if (index == 0U
						   && Scenario
							   == fixture_scenario::search_missing_pull_request) {
					item.erase("pull_request");
				} else if (index == 0U
						   && Scenario
							   == fixture_scenario::search_wrong_pull_request_url) {
					auto pullRequest = item.at("pull_request").get_object();
					pullRequest.insert_or_assign("url",
												 "https://api.github.com/repos/octo-org/"
												 "review-repository/pulls/999");
					item.insert_or_assign("pull_request", pullRequest);
				}
				items.emplace_back(item);
			}
			auto envelope = fixture_object{
				{ "incomplete_results", false },
				{ "items", items },
				{ "repository_selection", "selected" },
				{ "total_count", static_cast<std::uint64_t>(total) },
			};
			if (Scenario == fixture_scenario::search_envelope_missing_required) {
				envelope.erase("incomplete_results");
			} else if (Scenario == fixture_scenario::search_envelope_wrong_required) {
				envelope.insert_or_assign("items", fixture_object{ });
			}
			if (end == total) {
				return success_response(fixture_json(envelope));
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
				fixture_json(envelope), link, page_completeness::has_next_page);
		}

		[[nodiscard]] auto graphql_initial_response() const -> wire_response
		{
			std::uint64_t total = 101U;
			if (Scenario == fixture_scenario::repeated_cursor) {
				total = 201U;
			} else if (Scenario == fixture_scenario::truncated_total) {
				total = 102U;
			}
			fixture_array comments;
			for (std::size_t index = 0U; index < 100U; ++index) {
				auto comment = comment_node(index);
				if ((Scenario == fixture_scenario::line_comment_original_line_only
					 || Scenario == fixture_scenario::line_comment_no_coordinates)
					&& index == 0U) {
					comment.insert_or_assign("line", nullptr);
					comment.insert_or_assign(
						"originalLine",
						Scenario == fixture_scenario::line_comment_no_coordinates
							? fixture_value(nullptr)
							: fixture_value(42));
				} else if (Scenario == fixture_scenario::file_comment_original_line) {
					comment.insert_or_assign("line", nullptr);
					comment.insert_or_assign(
						"originalLine", index == 0U ? fixture_value(42) : nullptr);
				} else if (Scenario == fixture_scenario::paged_file_comment_original_line
						   || Scenario == fixture_scenario::file_thread_placeholder
						   || Scenario == fixture_scenario::file_thread_wrong_line
						   || Scenario == fixture_scenario::file_thread_wrong_original_line
						   || Scenario == fixture_scenario::file_thread_wrong_side) {
					comment.insert_or_assign("line", nullptr);
					comment.insert_or_assign("originalLine", nullptr);
				}
				comments.emplace_back(comment);
			}
			const auto missing = Scenario == fixture_scenario::missing_cursor;
			auto       thread  = fixture_object{
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
				thread.erase("diffSide");
			} else if (Scenario == fixture_scenario::thread_side_null) {
				thread.insert_or_assign("diffSide", nullptr);
			} else if (Scenario == fixture_scenario::thread_side_wrong) {
				thread.insert_or_assign("diffSide", 7);
			} else if (Scenario == fixture_scenario::thread_side_unknown) {
				thread.insert_or_assign("diffSide", "UNCHANGED");
			} else if (Scenario
					   == fixture_scenario::current_thread_original_line_only) {
				thread.insert_or_assign("line", nullptr);
			} else if (Scenario == fixture_scenario::current_thread_no_coordinates
					   || Scenario == fixture_scenario::outdated_thread_no_coordinates) {
				thread.insert_or_assign("line", nullptr);
				thread.insert_or_assign("originalLine", nullptr);
				thread.insert_or_assign(
					"isOutdated",
					Scenario == fixture_scenario::outdated_thread_no_coordinates);
			} else if (Scenario == fixture_scenario::file_comment_original_line
					   || Scenario
						   == fixture_scenario::paged_file_comment_original_line) {
				thread.insert_or_assign("subjectType", "FILE");
				thread.insert_or_assign("line", nullptr);
				thread.insert_or_assign("originalLine", nullptr);
			}
			if (Scenario == fixture_scenario::file_thread_placeholder
				|| Scenario == fixture_scenario::file_thread_wrong_line
				|| Scenario == fixture_scenario::file_thread_wrong_original_line
				|| Scenario == fixture_scenario::file_thread_wrong_side) {
				thread.insert_or_assign("subjectType", "FILE");
				thread.insert_or_assign("line", Scenario == fixture_scenario::file_thread_wrong_line ? 2 : 1);
				thread.insert_or_assign("originalLine", Scenario == fixture_scenario::file_thread_wrong_original_line ? 2 : 1);
				thread.insert_or_assign("diffSide", Scenario == fixture_scenario::file_thread_wrong_side ? "LEFT" : "RIGHT");
			}

			const auto pullRequest = fixture_object{
				{ "headRefOid",
				  Scenario == fixture_scenario::graphql_head_moved
					  ? std::string{ base_oid }
					  : std::string{ head_oid } },
				{ "id", "PR_17" },
				{
					"reviewThreads",
					connection(1U,
							   fixture_array{ thread },
							   false,
							   std::optional<std::string_view>{ "THREAD_END" }),
				},
			};
			auto envelope = fixture_object{
				{
					"data",
					fixture_object{
						{
							"repository",
							fixture_object{
								{ "id", "R_repository" },
								{ "pullRequest", pullRequest },
							},
						},
					},
				},
			};
			if (Scenario == fixture_scenario::graphql_extensions) {
				envelope.insert_or_assign(
					"extensions",
					fixture_object{ { "requestId", "graphql-initial-request" } });
			} else if (Scenario == fixture_scenario::graphql_errors_type_rate_limited) {
				envelope = fixture_object{
					{
						"errors",
						fixture_array{
							fixture_object{
								{ "message", "secondary limit" },
								{ "type", "RATE_LIMITED" },
							},
						},
					},
				};
			} else if (Scenario
					   == fixture_scenario::graphql_partial_errors_extension_type) {
				envelope.insert_or_assign("errors",
										  fixture_array{
											  fixture_object{
												  {
													  "extensions",
													  fixture_object{ { "type", "RATE_LIMITED" } },
												  },
												  { "message", "secondary limit" },
											  },
										  });
			} else if (Scenario
					   == fixture_scenario::graphql_partial_errors_extension_code) {
				envelope.insert_or_assign("errors",
										  fixture_array{
											  fixture_object{
												  {
													  "extensions",
													  fixture_object{ { "code", "RATE_LIMITED" } },
												  },
												  { "message", "secondary limit" },
											  },
										  });
			} else if (Scenario == fixture_scenario::graphql_primary_rate_metadata) {
				envelope.insert_or_assign("errors",
										  fixture_array{
											  fixture_object{
												  { "message", "primary quota exhausted" },
												  { "type", "RATE_LIMITED" },
											  },
										  });
			} else if (Scenario == fixture_scenario::graphql_malformed_errors) {
				envelope.insert_or_assign("errors", fixture_object{ { "type", "RATE_LIMITED" } });
			} else if (Scenario == fixture_scenario::graphql_unknown_envelope_field) {
				envelope.insert_or_assign("trace", "not-selected");
			}
			auto response = success_response(fixture_json(envelope));
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
			const auto    repeat = Scenario == fixture_scenario::repeated_cursor;
			fixture_array comments;
			const auto    count = repeat ? 100U : 1U;
			for (std::size_t index = 0U; index < count; ++index) {
				auto comment = comment_node(100U + index);
				if (index == 0U
					&& (Scenario
							== fixture_scenario::paged_line_comment_original_line_only
						|| Scenario == fixture_scenario::paged_line_comment_no_coordinates
						|| Scenario
							== fixture_scenario::paged_file_comment_original_line)) {
					comment.insert_or_assign("line", nullptr);
					comment.insert_or_assign(
						"originalLine",
						Scenario == fixture_scenario::paged_line_comment_no_coordinates
							? fixture_value(nullptr)
							: fixture_value(42));
				}
				if (Scenario == fixture_scenario::file_thread_placeholder) {
					comment.insert_or_assign("line", nullptr);
					comment.insert_or_assign("originalLine", nullptr);
				}

				comments.emplace_back(comment);
			}
			const auto total             = repeat
				? 201U
				: (Scenario == fixture_scenario::truncated_total ? 102U : 101U);
			const auto commentConnection = connection(total,
													  std::move(comments),
													  repeat,
													  repeat ? std::optional<std::string_view>{ "CURSOR_1" }
															 : std::optional<std::string_view>{ "COMMENT_END" });
			auto       envelope          = fixture_object{
				{
					"data",
					fixture_object{
						{
							"node",
							fixture_object{
								{ "comments", commentConnection },
								{ "id", "T_1" },
							},
						},
						{
							"repository",
							fixture_object{
								{ "id", "R_repository" },
								{
									"pullRequest",
									fixture_object{
										{ "headRefOid", std::string{ head_oid } },
										{ "id", "PR_17" },
									},
								},
							},
						},
					},
				},
			};
			if (Scenario == fixture_scenario::graphql_extensions) {
				envelope.insert_or_assign(
					"extensions",
					fixture_object{ { "requestId", "graphql-comments-request" } });
			}
			return success_response(fixture_json(envelope));
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
				return success_response(fixture_json(fixture_array{ }));
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
				case read_operation::list_commit_statuses: return success_response("[]");
				case read_operation::list_checks:
					return checks_response();
				case read_operation::search_pull_requests:
					return search_response(aRequest);
				case read_operation::list_review_comments:
					return review_comments_response(aRequest);
				case read_operation::list_review_requests:
				case read_operation::get_merge_assessment:
				case read_operation::list_branch_rules:
				case read_operation::get_branch_protection:
				case read_operation::list_pull_request_timeline:
				case read_operation::get_viewer_identity:
				case read_operation::search_personal_pull_requests:
				case read_operation::list_requested_reviewers:
				case read_operation::get_merge_base:
					break;
				}
				return success_response(fixture_json(fixture_object{ }));
			}();
			const auto resource = graphql_read(aRequest.Wire.Operation)
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
		const auto response = success_response(fixture_json(fixture_object{
			{ "avatar_url", "https://avatars.githubusercontent.com/u/1001" },
			{ "bio", "Reviews pull requests" },
			{ "created_at", "2011-01-25T18:44:36Z" },
			{ "email", nullptr },
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

		// [[BPR-1439]]. A NUL in a header name or value is refused; the old
		// `"\r\n\0"` set ended at the NUL and let one through.
		auto nulValue                  = response;
		nulValue.Headers.front().Value = std::string{ "application/json\0x", 18U };
		expect(!parse_github_response_metadata(nulValue).has_value(), "a header value with a NUL is refused");
		auto nulName                 = response;
		nulName.Headers.front().Name = std::string{ "Content-Type\0", 13U };
		expect(!parse_github_response_metadata(nulName).has_value(), "a header name with a NUL is refused");

		auto missing = response;
		missing.Body = fixture_json(fixture_object{
			{ "id", 1001 },
			{ "login", "octo-reviewer" },
			{ "name", "Octo Reviewer" },
		});
		expect(!normalize_observed_github_viewer(missing).has_value(),
			   "viewer normalization rejects a missing required field");

		auto wrong = response;
		wrong.Body = fixture_json(fixture_object{
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

	auto replace_raw(std::string& aBody, const std::string_view aBefore,
					 const std::string_view aAfter) -> std::size_t
	{
		std::size_t count    = 0U;
		std::size_t position = 0U;
		while ((position = aBody.find(aBefore, position)) != std::string::npos) {
			aBody.replace(position, aBefore.size(), aAfter);
			position += aAfter.size();
			++count;
		}
		return count;
	}

	auto test_glaze_viewer_numbers_and_syntax() -> void
	{
		for (const auto id : { std::uint64_t{ 9'007'199'254'740'993U }, std::numeric_limits<std::uint64_t>::max() }) {
			const auto response = success_response("{\"id\":" + std::to_string(id)
												   + R"(,"login":"octo-reviewer","name":null,"node_id":"U_viewer"})");
			const auto viewer   = normalize_observed_github_viewer(response);
			expect(viewer.has_value() && viewer->Viewer.Account.value() == id
					   && viewer->Viewer.DisplayName.empty() && viewer->RateLimits.Core.has_value(),
				   "viewer retains exact full-width identity, explicit null name and rate tuple");
		}
		for (const auto token : { "1001.00000000000000000001", "18446744073709551616", "-1", "1e-20", "null", "true", "\"1001\"" }) {
			const auto response = success_response(std::string{ "{\"id\":" } + token
												   + R"(,"login":"octo-reviewer","name":null,"node_id":"U_viewer"})");
			const auto result   = normalize_observed_github_viewer(response);
			expect(!result.has_value() && result.error().Failure == application::github_provider_failure::malformed_response
					   && result.error().RateLimits.Core.has_value(),
				   "viewer refuses fractional, overflowing and mistyped identities with observed metadata");
		}
		const auto exactExponent = normalize_observed_github_viewer(success_response(
			R"({"id":1001e0,"login":"octo-reviewer","name":"Caf\u00e9","node_id":"U_viewer"})"));
		expect(exactExponent.has_value() && exactExponent->Viewer.Account.value() == 1001U
				   && exactExponent->Viewer.DisplayName == "Café",
			   "exact integer exponent and escaped Unicode retain their meaning");
		const auto body = std::string{ R"({"id":1001,"login":"octo-reviewer","name":null,"node_id":"U_viewer"})" };
		for (const auto& malformed : std::vector<std::string>{
				 "null", "17", "\"viewer\"", body + "{}", body + "garbage",
				 body.substr(0U, body.size() - 1U) + R"(,"unknown":1e400})",
				 body.substr(0U, body.size() - 1U) + R"(,"unknown":"\ud800"})",
				 body.substr(0U, body.size() - 1U) + ",\"unknown\":\"" + std::string(1U, static_cast<char>(0xff)) + "\"}",
				 body.substr(0U, body.size() - 1U) + R"(,"id":1001})" }) {
			const auto result = normalize_observed_github_viewer(success_response(malformed));
			expect(!result.has_value() && result.error().Failure == application::github_provider_failure::malformed_response,
				   "whole viewer response is validated including unknown values, duplicate keys and trailing input");
		}
	}

	auto test_glaze_paged_identity_and_lifetime() -> void
	{
		for (const auto id : { std::uint64_t{ 9'007'199'254'740'993U }, std::numeric_limits<std::uint64_t>::max() }) {
			auto request                        = provider_request();
			request.Context.ExpectedRepository  = core::repository_id{ id };
			request.Context.ExpectedPullRequest = core::pull_request_id{ id };
			const auto rewrite                  = [id](const github_wire_request& aRequest, wire_response& aResponse) {
				const auto field = "\"id\":" + std::to_string(id);
				if (aRequest.Wire.Operation == read_operation::get_repository) {
					expect(replace_raw(aResponse.Body, "\"id\":2001", field) == 1U, "repository raw numeric fixture selected");
				} else if (aRequest.Wire.Operation == read_operation::get_pull_request) {
					expect(replace_raw(aResponse.Body, "\"id\":2001", field) == 2U
							   && replace_raw(aResponse.Body, "\"id\":3001", field) == 1U,
						   "PR and both repository reference IDs selected");
				} else if (aRequest.Wire.Operation == read_operation::list_reviews) {
					expect(replace_raw(aResponse.Body, "\"id\":4001", field) == 1U, "review raw numeric fixture selected");
				}
			};
			// The transport and every temporary completion are gone before facts
			// from both REST pages and both nested GraphQL pages are examined.
			const auto result = [&] {
				auto transport            = paged_fixture_transport{ fixture_scenario::success };
				transport.RewriteResponse = rewrite;
				return perform_paged_github_read(transport, profile(), request,
												 core::provider_snapshot_id{ 55U }, std::stop_token{ });
			}();
			expect(result.has_value() && result->Repository.Repository.value() == id
					   && result->PullRequest.PullRequest.value() == id
					   && result->Reviews.size() == 1U && result->Reviews.front().DatabaseId == id,
				   "complete production paging retains exact full-width repository, PR and review identities");
			expect(result->Files.size() == 101U && result->ReviewThreads.size() == 1U
					   && result->ReviewThreads.front().Comments.size() == 101U,
				   "both collection continuations were exhausted");
			for (std::size_t index = 0U; index < 101U; ++index) {
				expect(result->Files[index].Path == "src/file" + std::to_string(index) + ".cpp"
						   && result->ReviewThreads.front().Comments[index].Body == "comment " + std::to_string(index)
						   && result->ReviewThreads.front().Comments[index].Comment.value() == "C_" + std::to_string(index),
					   "all decoded page identities and text survive completion and transport destruction");
			}
			for (const auto wrongRepository : { true, false }) {
				auto wrongRequest = request;
				if (wrongRepository) {
					wrongRequest.Context.ExpectedRepository = core::repository_id{ id - 1U };
				} else {
					wrongRequest.Context.ExpectedPullRequest = core::pull_request_id{ id - 1U };
				}
				auto transport            = paged_fixture_transport{ fixture_scenario::success };
				transport.RewriteResponse = rewrite;
				const auto wrong          = perform_paged_github_read(transport, profile(), wrongRequest,
																	  core::provider_snapshot_id{ 56U }, std::stop_token{ });
				expect(!wrong.has_value() && provider_failure_of(wrong) == application::github_provider_failure::malformed_response,
					   "adjacent full-width authority mismatch cannot yield a snapshot");
			}
		}
	}

	auto test_glaze_paged_raw_refusals() -> void
	{
		struct raw_case
		{
			read_operation   Operation;
			std::string_view Before;
			std::string_view After;
		};
		for (const auto& test : {
				 raw_case{ read_operation::get_repository, "\"id\":2001", "\"id\":2001.00000000000000001" },
				 raw_case{ read_operation::get_pull_request, "\"id\":3001", "\"id\":18446744073709551616" },
				 raw_case{ read_operation::list_reviews, "\"id\":4001", "\"id\":4001.00000000000000001" },
				 raw_case{ read_operation::list_pull_request_files, "\"additions\":1", "\"additions\":1.00000000000000001" },
				 raw_case{ read_operation::list_review_threads, "\"line\":42", "\"line\":42.00000000000000001" },
				 raw_case{ read_operation::list_review_threads, "\"totalCount\":101", "\"totalCount\":101.00000000000000001" } }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			bool selected             = false;
			transport.RewriteResponse = [&](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation == test.Operation) {
					selected = replace_raw(aResponse.Body, test.Before, test.After) != 0U;
					expect(selected, "raw numeric refusal fixture selected");
				}
			};
			const auto result = perform_paged_github_read(transport, profile(), provider_request(),
														  core::provider_snapshot_id{ 57U }, std::stop_token{ });
			expect(selected && !result.has_value()
					   && provider_failure_of(result) == application::github_provider_failure::malformed_response,
				   "fractional or overflowing page identities, counts and coordinates fail atomically");
		}
		for (const auto& suffix : std::vector<std::string>{ "{}", "garbage", std::string(1U, static_cast<char>(0xff)) }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			bool selected             = false;
			transport.RewriteResponse = [&](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation == read_operation::list_review_threads && aRequest.Wire.Semantic.Page == 2U) {
					selected = true;
					aResponse.Body += suffix;
				}
			};
			const auto result = perform_paged_github_read(transport, profile(), provider_request(),
														  core::provider_snapshot_id{ 58U }, std::stop_token{ });
			expect(selected && !result.has_value()
					   && provider_failure_of(result) == application::github_provider_failure::malformed_response,
				   "malformed continuation tail cannot return the earlier partial snapshot");
		}
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
		auto       fileTransport = paged_fixture_transport{ fixture_scenario::file_thread_placeholder };
		const auto fileResult    = perform_paged_github_read(fileTransport, profile(), provider_request(),
															 core::provider_snapshot_id{ 56U }, std::stop_token{ });
		expect(fileResult.has_value()
				   && fileResult->ReviewThreads.front().Subject == application::github_review_thread_subject::file
				   && !fileResult->ReviewThreads.front().Line
				   && !fileResult->ReviewThreads.front().OriginalLine
				   && fileResult->ReviewThreads.front().Comments.size() == 101U
				   && std::ranges::all_of(fileResult->ReviewThreads.front().Comments, [](const auto& comment) {
						  return !comment.Line && !comment.OriginalLine;
					  }),
			   "a paged FILE thread discards its synthetic first-line position and preserves all comments");

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
		for (const auto scenario : { fixture_scenario::current_thread_no_coordinates,
									 fixture_scenario::outdated_thread_no_coordinates }) {
			auto       unanchoredTransport = paged_fixture_transport{ scenario };
			const auto unanchored          = perform_paged_github_read(
				unanchoredTransport,
				profile(),
				provider_request(),
				core::provider_snapshot_id{ 57U },
				std::stop_token{ });
			expect(unanchored.has_value()
					   && unanchored->ReviewThreads.size() == 1U
					   && unanchored->ReviewThreads.front().Thread.value() == "T_1"
					   && unanchored->ReviewThreads.front().Subject
						   == application::github_review_thread_subject::line
					   && unanchored->ReviewThreads.front().Path == "src/file0.cpp"
					   && unanchored->ReviewThreads.front().Comments.size() == 101U
					   && unanchored->ReviewThreads.front().Comments.front().Comment.value() == "C_0"
					   && unanchored->ReviewThreads.front().Comments.back().Comment.value() == "C_100"
					   && unanchored->ReviewThreads.front().Comments.back().Body == "comment 100"
					   && unanchored->ReviewThreads.front().DiffSide
						   == application::github_diff_side::right
					   && !unanchored->ReviewThreads.front().Line
					   && !unanchored->ReviewThreads.front().OriginalLine
					   && unanchored->ReviewThreads.front().Outdated
						   == (scenario == fixture_scenario::outdated_thread_no_coordinates),
				   "nullable root coordinates preserve the exact remote LINE thread without a guessed anchor");
		}
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
				   && transport.Requests.size() == 17U
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

	// [[BPR-0892]]. The page and aggregate ceilings against the largest real
	// shapes measured, with the production limits. GitHub sends `patch` on
	// every page, and the product must load a pull request of that shape.
	auto test_measured_files_page_shapes() -> void
	{
		auto       largest = paged_fixture_transport{ fixture_scenario::measured_largest_files_page };
		const auto loaded  = perform_paged_github_read(largest,
													   profile(),
													   provider_request(),
													   core::provider_snapshot_id{ 70U },
													   std::stop_token{ });
		expect(!largest.FilesPageBytes.empty() && largest.FilesPageBytes.front() >= measured_largest_page_bytes,
			   "the served first page is at least as large as the largest measured files page");
		expect(loaded.has_value() && loaded->Files.size() == 101U,
			   "a pull request whose files page is as large as the largest measured one loads under the production page limit");

		auto       capped    = paged_fixture_transport{ fixture_scenario::measured_file_cap_pages };
		const auto collected = perform_paged_github_read(capped,
														 profile(),
														 provider_request(),
														 core::provider_snapshot_id{ 71U },
														 std::stop_token{ });
		const auto served    = std::accumulate(capped.FilesPageBytes.begin(), capped.FilesPageBytes.end(), std::size_t{ 0U });
		expect(capped.FilesPageBytes.size() == 30U && served >= measured_cap_total_bytes
				   && std::ranges::all_of(capped.FilesPageBytes, [](const std::size_t aBytes) { return aBytes >= measured_cap_largest_page_bytes; }),
			   "the served 3,000 files come in 30 pages, each at least as large as the largest measured one");
		expect(collected.has_value() && collected->Files.size() == 3'000U,
			   "a pull request at GitHub's 3,000-file limit, every page as heavy as the heaviest measured, loads under the production aggregate limit");
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

	auto test_search_result_limit_preserves_exact_pull_request() -> void
	{
		for (const bool malformed : { false, true }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::search_plus_link };
			transport.RewriteResponse = [malformed](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation != read_operation::search_pull_requests) {
					return;
				}
				auto document = parse_fixture_object(aResponse.Body);
				document.insert_or_assign("total_count", std::uint64_t{ 2104U });
				if (malformed) {
					document.insert_or_assign("incomplete_results", true);
				}
				aResponse.Body = fixture_json(document);
			};
			const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 56U }, { });
			if (malformed) {
				expect(!result && provider_failure_of(result) == application::github_provider_failure::malformed_response,
					   "excess search total never hides a malformed search envelope");
				continue;
			}
			expect(result && application::validate(*result, provider_request().Context).valid()
					   && result->SearchAvailability == application::github_search_availability::result_limit
					   && result->SearchResults.empty() && result->Files.size() == 101U
					   && result->PullRequest.HeadOid.value() == head_oid && result->PullRequest.BaseOid.value() == base_oid,
				   "2104 search hits refuse all search rows while preserving validated exact PR files and diff endpoints");
			expect(std::ranges::count_if(transport.Requests, [](const auto& aRequest) {
					   return aRequest.Wire.Operation == read_operation::search_pull_requests;
				   }) == 1,
				   "result-limit search does not fetch or retain a truncated continuation");
		}
		auto malformedItem            = paged_fixture_transport{ fixture_scenario::search_wrong_authority };
		malformedItem.RewriteResponse = [](const github_wire_request& aRequest, wire_response& aResponse) {
			if (aRequest.Wire.Operation == read_operation::search_pull_requests) {
				auto document = parse_fixture_object(aResponse.Body);
				document.insert_or_assign("total_count", std::uint64_t{ 2104U });
				aResponse.Body = fixture_json(document);
			}
		};
		const auto malformed = perform_paged_github_read(malformedItem, profile(), provider_request(), core::provider_snapshot_id{ 57U }, { });
		expect(!malformed && provider_failure_of(malformed) == application::github_provider_failure::malformed_response,
			   "excess search total never hides cross-authority search records");
	}

	auto test_limited_search_still_validates_received_records() -> void
	{
		enum class fault { none,
						   duplicate,
						   empty_terminal_page,
						   foreign_entity,
						   target_identity,
						   oversized_title };
		for (const auto selected : { fault::none, fault::duplicate, fault::empty_terminal_page, fault::foreign_entity, fault::target_identity, fault::oversized_title }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::search_plus_link };
			transport.RewriteResponse = [selected](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation != read_operation::search_pull_requests) {
					return;
				}
				auto document = parse_fixture_object(aResponse.Body);
				document.insert_or_assign("total_count", std::uint64_t{ 2104U });
				auto& items = document.at("items").get_array();
				switch (selected) {
				case fault::none:      break;
				case fault::duplicate: items.at(1U) = items.front(); break;
				case fault::empty_terminal_page:
					items.clear();
					std::erase_if(aResponse.Headers, [](const auto& aHeader) { return aHeader.Name == "Link"; });
					aResponse.Completeness = page_completeness::complete;
					break;
				case fault::foreign_entity:  items.front().get_object().insert_or_assign("node_id", "R_repository"); break;
				case fault::target_identity: items.front().get_object().insert_or_assign("node_id", "PR_17"); break;
				case fault::oversized_title: items.front().get_object().insert_or_assign("title", std::string(1025U, 'x')); break;
				}
				aResponse.Body = fixture_json(document);
			};
			const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 58U }, { });
			if (selected == fault::none) {
				expect(result && result->SearchAvailability == application::github_search_availability::result_limit
						   && result->SearchResults.empty() && result->Files.size() == 101U,
					   "valid adjacent oversized search preserves exact PR and refuses all search rows");
			} else {
				const auto expected = selected == fault::oversized_title ? application::github_provider_failure::response_limit : application::github_provider_failure::malformed_response;
				expect(!result && provider_failure_of(result) == expected,
					   "oversized search still refuses invalid received identities, fields and terminal-page claims");
			}
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
		expect(accepted.has_value() && accepted->SearchAvailability == application::github_search_availability::available && accepted->SearchResults.size() == 101U,
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
				   && accepted->PullRequest.ReportedCommitCount == 250U
				   && !accepted->CapabilityEvidence.ReadShape.ViewedFileState,
			   "advertised file and commit counts remain exact at REST caps even "
			   "when GraphQL viewed state is unavailable");

		auto       capped = paged_fixture_transport{ fixture_scenario::commit_count_above_cap };
		const auto large  = perform_paged_github_read(capped,
													  profile(),
													  provider_request(),
													  core::provider_snapshot_id{ 68U },
													  std::stop_token{ });
		expect(large.has_value() && large->PullRequest.ReportedCommitCount == 9'311U
				   && large->Commits.size() == 250U
				   && large->Commits.back().Oid == large->PullRequest.HeadOid,
			   "large metadata total preserves only the exact bounded returned commit slice");
		const auto projected = application::project_github_pull_request(*large);
		expect(projected.CommitCount == 9'311U && projected.ReturnedCommitCount == 250U,
			   "accepted capped snapshot projects the reported total and returned slice separately");

		for (const auto scenario : {
				 fixture_scenario::file_count_mismatch,
				 fixture_scenario::commit_count_mismatch,
				 fixture_scenario::commit_count_above_cap_short,
				 fixture_scenario::commit_count_above_cap_wrong_head,
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
		{
			auto limits                   = github_paged_read_limits{ };
			limits.Records.MaximumCommits = 1U;
			auto       transport          = paged_fixture_transport{ fixture_scenario::commit_count_mismatch };
			const auto result             = perform_paged_github_read(transport,
																	  profile(),
																	  provider_request(),
																	  core::provider_snapshot_id{ 70U },
																	  std::stop_token{ },
																	  limits);
			expect(!result.has_value()
					   && provider_failure_of(result) == application::github_provider_failure::malformed_response,
				   "a tightened local budget cannot turn an incomplete endpoint into success");
		}

		{
			auto       transport = paged_fixture_transport{ fixture_scenario::file_count_above_cap };
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
				"advertised file count above the security cap fails before file paging");
		}
		{
			auto       transport = paged_fixture_transport{ fixture_scenario::commit_count_above_cap_overlong };
			const auto result    = perform_paged_github_read(transport,
															 profile(),
															 provider_request(),
															 core::provider_snapshot_id{ 69U },
															 std::stop_token{ });
			expect(!result.has_value()
					   && provider_failure_of(result) == application::github_provider_failure::response_limit
					   && transport.Requests.size() > 2U,
				   "an overlong returned commit collection fails without publishing a snapshot");
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
			fixture_scenario::file_thread_wrong_line,
			fixture_scenario::file_thread_wrong_original_line,
			fixture_scenario::file_thread_wrong_side,
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
	/// The identity `signed_out_read_identity` admits, term by term: the
	/// public-preview mode on the request and its context, an anonymous
	/// viewer, a zero account everywhere, every capability unsupported so
	/// the read shape is all-false and the projection helper stops before a
	/// GraphQL body exists, and evidence that was never probed — both
	/// freshness epochs zero — because a signed-out session synthesises its
	/// entry rather than observing one. No account transport is attached,
	/// because an anonymous authority holds no lease to present.
	[[nodiscard]] auto anonymous_provider_request()
		-> application::github_pull_request_provider_request
	{
		auto request               = provider_request();
		request.Mode               = core::github_access_mode::public_preview;
		request.Context.Mode       = core::github_access_mode::public_preview;
		request.Context.Account    = core::github_account_id{ };
		request.Viewer             = application::github_viewer_record{ };
		request.AccountTransport   = nullptr;
		auto& evidence             = request.CapabilityEvidence;
		evidence.Authority.Account = core::github_account_id{ };
		for (auto& fact : evidence.Facts) {
			fact.State = core::github_capability_state::unsupported;
		}
		for (auto& fact : evidence.SchemaFacts) {
			fact.State = core::github_capability_state::unsupported;
		}
		evidence.Capabilities  = core::anonymous_capabilities();
		evidence.ReadShape     = application::github_read_shape{ };
		evidence.ObservedAt    = application::github_freshness_epoch{ };
		evidence.ExpiresBefore = application::github_freshness_epoch{ };
		expect(application::anonymous(request.Viewer)
				   && application::reads_nothing(evidence.ReadShape)
				   && signed_out_read_identity(request),
			   "a signed-out profile presents the signed-out identity");
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

	auto test_glaze_anonymous_exact_comment_linkage() -> void
	{
		for (const auto id : { std::uint64_t{ 9'007'199'254'740'993U }, std::numeric_limits<std::uint64_t>::max() }) {
			auto       body  = anonymous_fixture_bytes("synthetic/review-comments-linked.json");
			const auto rawId = std::to_string(id);
			expect(replace_raw(body, "\"id\": 101,", "\"id\": " + rawId + ",") == 1U
					   && replace_raw(body, "\"in_reply_to_id\": 101", "\"in_reply_to_id\": " + rawId) == 2U,
				   "full-width root and both exact reply links selected");
			auto transport = paged_fixture_transport{ fixture_scenario::success };
			transport.ReviewCommentPages.push_back(body);
			const auto result = anonymous_read(transport);
			expect(result.has_value() && result->ReviewCommentGroups.size() == 2U
					   && result->ReviewCommentGroups.front().Root.Comment.value() == rawId
					   && result->ReviewCommentGroups.front().Replies.size() == 2U
					   && result->ReviewCommentGroups.front().Replies.front().ReplyTo
						   == std::optional{ application::github_comment_id{ rawId } },
				   "anonymous root identity and reply grouping remain exact at full uint64 width");
			expect(replace_raw(body, "\"in_reply_to_id\": " + rawId,
							   "\"in_reply_to_id\": " + std::to_string(id - 1U))
					   == 2U,
				   "adjacent wrong anonymous reply links selected");
			auto wrong = paged_fixture_transport{ fixture_scenario::success };
			wrong.ReviewCommentPages.push_back(body);
			const auto rejected = anonymous_read(wrong);
			expect(!rejected.has_value() && provider_failure_of(rejected) == application::github_provider_failure::malformed_response,
				   "adjacent wrong root identity refuses the complete anonymous snapshot");
		}
		for (const auto field : { "\"id\": 101", "\"in_reply_to_id\": 101" }) {
			auto body = anonymous_fixture_bytes("synthetic/review-comments-linked.json");
			expect(replace_raw(body, field, std::string{ field } + ".00000000000000001") != 0U,
				   "fractional anonymous identity fixture selected");
			auto transport = paged_fixture_transport{ fixture_scenario::success };
			transport.ReviewCommentPages.push_back(body);
			const auto result = anonymous_read(transport);
			expect(!result.has_value() && provider_failure_of(result) == application::github_provider_failure::malformed_response,
				   "fractional root or reply identity cannot round into a valid anonymous group");
		}
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
	class timeline_fixture_transport final : public github_wire_transport_port
	{
	public:
		std::vector<wire_response>       Responses;
		std::vector<github_wire_request> Requests;

		auto perform(const github_wire_request& aRequest, std::stop_token) -> github_wire_completion override
		{
			Requests.push_back(aRequest);
			expect(aRequest.Wire.Operation == read_operation::list_pull_request_timeline
					   && aRequest.Wire.Method == http_method::get
					   && aRequest.Wire.Path == "/repos/octo-org/review-repository/issues/17/timeline"
					   && aRequest.Wire.Query == "page=" + std::to_string(Requests.size()) + "&per_page=100",
				   "timeline compiles its exact anonymous REST request");
			expect(Requests.size() <= Responses.size(), "timeline never issues an unprovided page");
			return github_wire_success{ .Correlation = aRequest.Correlation, .Response = Responses[Requests.size() - 1U] };
		}
	};

	[[nodiscard]] auto timeline_event(const std::uint64_t aId, const std::string_view aTimestamp = "2026-08-01T16:45:08Z") -> std::string
	{
		return "{\"event\":\"head_ref_force_pushed\",\"id\":" + std::to_string(aId)
			+ ",\"commit_id\":\"" + std::string{ head_oid } + "\",\"created_at\":\"" + std::string{ aTimestamp } + "\"}";
	}

	[[nodiscard]] auto timeline_full_page(const std::uint64_t aId) -> std::string
	{
		auto result = "[" + timeline_event(aId);
		for (std::size_t index = 1U; index < 100U; ++index) {
			result += ",{\"event\":\"committed\"}";
		}
		return result + "]";
	}

	[[nodiscard]] auto timeline_next(const std::uint32_t aPage, const std::string_view aAuthority = "api.github.com") -> std::string
	{
		return "<https://" + std::string{ aAuthority } + "/repos/octo-org/review-repository/issues/17/timeline?page=" + std::to_string(aPage) + "&per_page=100>; rel=\"next\"";
	}

	[[nodiscard]] auto read_timeline(timeline_fixture_transport& aTransport, const github_paged_read_limits& aLimits = { }) -> github_timeline_read_result
	{
		return perform_paged_github_timeline_read(aTransport, profile(), anonymous_provider_request(), core::repository_id{ 101U }, { }, aLimits, github_provider_read_authority::anonymous_public);
	}

	class recording_response_observer final : public github_paged_response_observer
	{
	public:
		std::optional<read_operation>                            LastOperation;
		std::optional<application::github_read_validation_error> ValidationError;
		std::optional<application::github_read_validation_error> RelationshipDetail;
		std::optional<application::github_actor_login_conflict>  ActorConflict;
		void                                                     request_started(const read_operation aOperation) noexcept override { LastOperation = aOperation; }
		void                                                     validation_failed(const application::github_read_validation aValidation) noexcept override
		{
			ValidationError    = aValidation.Error;
			RelationshipDetail = aValidation.Detail;
			ActorConflict      = aValidation.ActorConflict;
		}
		std::array<std::pair<read_operation, int>, 32U>                      Responses{ };
		std::size_t                                                          Count = 0U;
		std::optional<application::github_pull_request_overview_observation> Overview;
		std::size_t                                                          OverviewPageCount = 0U;
		void                                                                 overview_observed(application::github_pull_request_overview_observation aObservation) noexcept override
		{
			expect(!Overview, "metadata is observed once");
			Overview          = std::move(aObservation);
			OverviewPageCount = Count;
		}
		void observe(const read_operation aOperation, const int aStatus) noexcept override
		{
			expect(Count < Responses.size(), "response observation stays bounded");
			Responses[Count++] = { aOperation, aStatus };
		}
	};

	auto test_ghes_actor_conflict_recreated_read() -> void
	{
		using origin = application::github_actor_observation_origin;
		for (const auto login : { "OCTO-REVIEWER", "unrelated-login" }) {
			for (unsigned restart = 0U; restart < 2U; ++restart) {
				auto transport            = paged_fixture_transport{ fixture_scenario::success, "ghe.example.invalid" };
				transport.RewriteResponse = [login](const github_wire_request& aRequest, wire_response& aResponse) {
					if (aRequest.Wire.Operation == read_operation::list_reviews) {
						for (const auto& [from, to] : std::array<std::pair<std::string_view, std::string_view>, 2U>{ { { "U_reviewer", "U_viewer" }, { "octo-reviewer-2", login } } }) {
							const auto position = aResponse.Body.find(from);
							expect(position != std::string::npos, "attributable review actor fixture field exists");
							aResponse.Body.replace(position, from.size(), to);
						}
					}
				};
				recording_response_observer observer;
				const auto                  result = perform_paged_github_read(transport, enterprise_profile(), provider_request(application::github_server_contract::ghes_3_18_2022_11_28),
																			   core::provider_snapshot_id{ 55U + restart }, { }, { }, github_provider_read_authority::authenticated, &observer);
				expect(!result && observer.LastOperation == read_operation::list_commit_statuses && observer.ActorConflict
						   && observer.ActorConflict->First == origin::viewer && observer.ActorConflict->Conflicting == origin::review
						   && observer.ActorConflict->AsciiCaseOnly == (std::string_view{ login } == "OCTO-REVIEWER"),
					   "GHES case-only and different conflicts stay refusing with actual review provenance across recreated reads");
				transport.RewriteResponse = { };
				expect(perform_paged_github_read(transport, enterprise_profile(), provider_request(application::github_server_contract::ghes_3_18_2022_11_28), core::provider_snapshot_id{ 60U + restart }, { }).has_value(),
					   "corrected synthetic payload recovers on retry without retaining the refusal");
			}
		}
	}

	/// \brief [[BPR-1321]] One synthetic GitHub App actor observed by REST and GraphQL.
	struct bot_actor_observation
	{
		std::string_view RestType;
		std::string_view RestLogin;
		std::string_view GraphqlActor;
	};

	/// \brief Runs the composed GHES read with the review in REST and the first
	/// thread comment in GraphQL authored by the same `BOT_review_app` node.
	auto read_bot_actor(const bot_actor_observation& aObservation, const std::uint64_t aSnapshot,
						recording_response_observer* aObserver = nullptr) -> github_paged_read_result
	{
		auto transport            = paged_fixture_transport{ fixture_scenario::success, "ghe.example.invalid" };
		transport.RewriteResponse = [aObservation](const github_wire_request& aRequest, wire_response& aResponse) {
			if (aRequest.Wire.Operation == read_operation::list_reviews) {
				const auto rest = std::string{ R"("login":")" } + std::string{ aObservation.RestLogin } + R"(","node_id":"BOT_review_app")"
					+ (aObservation.RestType.empty() ? std::string{ } : R"(,"type":")" + std::string{ aObservation.RestType } + "\"");
				expect(replace_raw(aResponse.Body, R"("login":"octo-reviewer-2","node_id":"U_reviewer","type":"User")", rest) == 1U,
					   "review REST actor fixture field selected");
			}
			if (aRequest.Wire.Operation == read_operation::list_review_threads) {
				const auto position = aResponse.Body.find(R"("author":null)");
				if (position != std::string::npos) {
					aResponse.Body.replace(position, std::string_view{ R"("author":null)" }.size(),
										   std::string{ R"("author":)" } + std::string{ aObservation.GraphqlActor });
				}
			}
		};
		return perform_paged_github_read(transport, enterprise_profile(), provider_request(application::github_server_contract::ghes_3_18_2022_11_28),
										 core::provider_snapshot_id{ aSnapshot }, { }, { }, github_provider_read_authority::authenticated, aObserver);
	}

	auto test_ghes_bot_actor_representation() -> void
	{
		using origin = application::github_actor_observation_origin;
		// GitHub states a GitHub App actor as REST `"type":"Bot"` with login
		// `<name>[bot]` and as GraphQL `Bot` with login `<name>`, on one node.
		for (const auto graphql : { std::string_view{ R"({"__typename":"Bot","id":"BOT_review_app","login":"review-app"})" },
									std::string_view{ R"({"__typename":"Bot","id":"BOT_review_app","login":"review-app[bot]"})" } }) {
			for (unsigned restart = 0U; restart < 2U; ++restart) {
				const auto result       = read_bot_actor({ "Bot", "review-app[bot]", graphql }, 70U + restart);
				const auto threadAuthor = [&]() -> std::optional<application::github_actor_record> {
					for (const auto& thread : result->ReviewThreads) {
						for (const auto& comment : thread.Comments) {
							if (comment.Author) {
								return comment.Author;
							}
						}
					}
					return std::nullopt;
				};
				expect(result.has_value() && result->Reviews.size() == 1U && result->Reviews.front().Author
						   && result->Reviews.front().Author->Login == "review-app[bot]"
						   && result->Reviews.front().Author->Kind == application::github_actor_kind::bot
						   && threadAuthor() && threadAuthor()->Login == "review-app[bot]"
						   && threadAuthor()->Kind == application::github_actor_kind::bot
						   && threadAuthor()->Actor == result->Reviews.front().Author->Actor,
					   "a typed bot on both sides loads through the composed GHES read in the REST login form, on every recreated read");
			}
		}

		struct refused_case
		{
			bot_actor_observation Observation;
			bool                  AsciiCaseOnly;
			std::string_view      Message;
		};
		for (const auto& refused : std::array{
				 refused_case{ { "Bot", "review-app[bot]", R"({"__typename":"Bot","id":"BOT_review_app","login":"other-app"})" }, false, "a different bot login for the same node still refuses" },
				 refused_case{ { "Bot", "review-app[bot]", R"({"__typename":"Bot","id":"BOT_review_app","login":"Review-App"})" }, true, "a case-only bot login difference still refuses" },
				 refused_case{ { "User", "review-app[bot]", R"({"__typename":"Bot","id":"BOT_review_app","login":"review-app"})" }, false, "a REST User carrying a bot-form login is not matched by a GraphQL Bot" },
				 refused_case{ { "Bot", "review-app[bot]", R"({"__typename":"User","id":"BOT_review_app","login":"review-app"})" }, false, "a REST Bot is not matched by a GraphQL User of the bare name" },
				 refused_case{ { "Bot", "review-app[bot]", R"({"id":"BOT_review_app","login":"review-app"})" }, false, "without GraphQL type evidence nothing is canonicalized" },
				 refused_case{ { "", "review-app[bot]", R"({"__typename":"Bot","id":"BOT_review_app","login":"review-app"})" }, false, "without REST type evidence a GraphQL Bot does not match" },
				 refused_case{ { "User", "review-app", R"({"__typename":"User","id":"BOT_review_app","login":"review-app-2"})" }, false, "ordinary user conflicts are unchanged" },
			 }) {
			recording_response_observer observer;
			const auto                  result = read_bot_actor(refused.Observation, 80U, &observer);
			expect(!result && provider_failure_of(result) == application::github_provider_failure::malformed_response
					   && observer.LastOperation == read_operation::list_commit_statuses
					   && observer.RelationshipDetail == application::github_read_validation_error::relationship_actor_login
					   && observer.ActorConflict && observer.ActorConflict->First == origin::review
					   && observer.ActorConflict->Conflicting == origin::thread_comment
					   && observer.ActorConflict->AsciiCaseOnly == refused.AsciiCaseOnly,
				   refused.Message);
		}
		recording_response_observer malformed;
		expect(!read_bot_actor({ "Bot", "review-app[bot]", R"({"__typename":7,"id":"BOT_review_app","login":"review-app"})" }, 90U, &malformed)
				   && !malformed.ActorConflict,
			   "a non-string GraphQL type is a malformed actor, not type evidence");
	}

	auto test_paged_response_observation() -> void
	{
		for (const int status : { 200, 503, 0, -1 }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			transport.RewriteResponse = [status](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation != read_operation::search_pull_requests) {
					return;
				}
				if (status == 503) {
					aResponse.Status = status;
				}
				if (status == 0) {
					aResponse.Headers.push_back({ "x-ratelimit-remaining", "invalid" });
				}
				if (status == -1) {
					aResponse.Body.assign(4U * 1024U * 1024U + 1U, 'x');
				}
			};
			recording_response_observer observer;
			const auto                  result = perform_paged_github_read(transport, profile(), provider_request(),
																		   core::provider_snapshot_id{ 55U }, { }, { }, github_provider_read_authority::authenticated, &observer);
			expect(result.has_value() == (status == 200), "response observer preserves success and final-operation refusal");
			expect(observer.Overview && observer.OverviewPageCount == 2U
					   && observer.Overview->Context == provider_request().Context
					   && !observer.Overview->PullRequest.Title.empty(),
				   "validated useful metadata arrives after two responses, before any catalogue or ancillary page");
			expect(observer.Count + (status == 200 ? 5U : 0U) == transport.Requests.size() && observer.Count == (status == 200 ? 12U : 10U), "every page produces exactly one observation; failed mandatory search precedes optional results");
			for (std::size_t index = 0U; index < observer.Count; ++index) {
				const auto expectedStatus = status == 503 && index + 1U == observer.Count ? 503 : 200;
				expect(observer.Responses[index] == std::pair{ transport.Requests[index].Correlation.Operation, expectedStatus },
					   "response callback preserves request order and HTTP status, including repeated pages");
			}
			expect(observer.Responses[observer.Count - 1U].first == (status == 200 ? read_operation::list_commit_statuses : read_operation::search_pull_requests),
				   "last attempted operation remains observable before status, metadata, and byte validation");
			if (!result) {
				const auto expectedFailure = status == 503 ? application::github_provider_failure::unavailable
					: status == 0                          ? application::github_provider_failure::malformed_response
														   : application::github_provider_failure::response_limit;
				expect(provider_failure_of(result) == expectedFailure, "observer leaves final failure class unchanged");
			}
		}
		auto                        transport = paged_fixture_transport{ fixture_scenario::network_failure };
		recording_response_observer observer;
		const auto                  network = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55U },
																		{ }, { }, github_provider_read_authority::authenticated, &observer);
		expect(!network && observer.Count == 0U, "transport failure invents no HTTP response");
		expect(observer.LastOperation == read_operation::get_repository, "initial network failure retains attempted operation");
		auto                        lateTransport = paged_fixture_transport{ fixture_scenario::late_search_wire_failure };
		recording_response_observer lateObserver;
		const auto                  late = perform_paged_github_read(lateTransport, profile(), provider_request(), core::provider_snapshot_id{ 55U },
																	 { }, { }, github_provider_read_authority::authenticated, &lateObserver);
		expect(!late && lateObserver.LastOperation == read_operation::search_pull_requests
				   && lateObserver.Count + 1U == lateTransport.Requests.size(),
			   "later wire failure retains attempted search without inventing a response");
		auto invalidTransport            = paged_fixture_transport{ fixture_scenario::success };
		invalidTransport.RewriteResponse = [](const github_wire_request& aRequest, wire_response& aResponse) {
			if (aRequest.Wire.Operation == read_operation::list_reviews) {
				const auto position = aResponse.Body.find("R_review");
				expect(position != std::string::npos, "review fixture has node identity");
				aResponse.Body.replace(position, std::string_view{ "R_review" }.size(), "R_repository");
			}
		};
		recording_response_observer invalidObserver;
		const auto                  invalid = perform_paged_github_read(invalidTransport, profile(), provider_request(), core::provider_snapshot_id{ 55U },
																		{ }, { }, github_provider_read_authority::authenticated, &invalidObserver);
		expect(!invalid && invalidObserver.Count == 12U
				   && invalidObserver.ValidationError == application::github_read_validation_error::duplicate_identity
				   && provider_failure_of(invalid) == application::github_provider_failure::malformed_response,
			   "real final snapshot validation preserves duplicate identity detail and mapped refusal");
		auto relationshipTransport            = paged_fixture_transport{ fixture_scenario::success };
		relationshipTransport.RewriteResponse = [](const github_wire_request& aRequest, wire_response& aResponse) {
			if (aRequest.Wire.Operation == read_operation::list_reviews) {
				const auto position = aResponse.Body.find("U_reviewer");
				expect(position != std::string::npos, "review fixture has actor identity");
				aResponse.Body.replace(position, std::string_view{ "U_reviewer" }.size(), "U_viewer");
			}
		};
		recording_response_observer relationshipObserver;
		const auto                  relationship = perform_paged_github_read(relationshipTransport, profile(), provider_request(), core::provider_snapshot_id{ 55U },
																			 { }, { }, github_provider_read_authority::authenticated, &relationshipObserver);
		expect(!relationship && relationshipObserver.LastOperation == read_operation::list_commit_statuses
				   && relationshipObserver.ValidationError == application::github_read_validation_error::invalid_relationship
				   && relationshipObserver.RelationshipDetail == application::github_read_validation_error::relationship_actor_login
				   && provider_failure_of(relationship) == application::github_provider_failure::malformed_response,
			   "optional result pages do not obscure an earlier review actor relationship failure");
		auto timeline                    = timeline_fixture_transport{ };
		timeline.Responses               = { success_response(timeline_full_page(1U), timeline_next(2U), page_completeness::has_next_page), success_response("[]") };
		timeline.Responses.back().Status = 503;
		const auto history               = perform_paged_github_timeline_read(timeline, profile(), anonymous_provider_request(), core::repository_id{ 101U },
																			  { }, { }, github_provider_read_authority::anonymous_public, &observer);
		expect(!history && observer.Count == 2U
				   && observer.Responses[0] == std::pair{ read_operation::list_pull_request_timeline, 200 }
				   && observer.Responses[1] == std::pair{ read_operation::list_pull_request_timeline, 503 },
			   "timeline observer includes the failed final page in order");
	}

	auto test_timeline_read() -> void
	{
		auto transport      = timeline_fixture_transport{ };
		transport.Responses = { success_response("[" + timeline_event(9007199254740993ULL) + "]") };
		const auto one      = read_timeline(transport);
		expect(one && one->ForcePushEvents.size() == 1U
				   && one->ForcePushEvents.front().EventId == 9007199254740993ULL
				   && one->ForcePushEvents.front().AfterHeadOid == head_oid
				   && one->ForcePushEvents.front().CreatedAtUnixMilliseconds == 1785602708000LL
				   && one->RateLimits.Core.has_value(),
			   "timeline preserves exact numeric id, after head and UTC event time");
		transport.Requests.clear();
		transport.Responses = { success_response(timeline_full_page(1U), timeline_next(2U), page_completeness::has_next_page), success_response("[" + timeline_event(2U) + "]") };
		const auto multiple = read_timeline(transport);
		expect(multiple && multiple->ForcePushEvents.size() == 2U && transport.Requests.size() == 2U,
			   "timeline exhausts multiple pages and ignores surviving commit entries");
		transport.Requests.clear();
		transport.Responses = { success_response("[]") };
		const auto empty    = read_timeline(transport);
		expect(empty && empty->ForcePushEvents.empty(), "empty exhausted timeline is observed");
		transport.Requests.clear();
		transport.Responses     = { success_response(R"([{"event":"committed","sha":"1111111111111111111111111111111111111111"},{"event":"committed","sha":"2222222222222222222222222222222222222222"},{"event":"committed","sha":"3333333333333333333333333333333333333333"}])") };
		const auto ordinaryPush = read_timeline(transport);
		expect(ordinaryPush && ordinaryPush->ForcePushEvents.empty() && transport.Requests.size() == 1U,
			   "three surviving commits in an ordinary push supply no invented push occurrences or boundaries");
	}

	auto test_timeline_atomic_failures() -> void
	{
		for (const auto invalidTime : { "2026-02-30T16:45:08Z", "2026-08-01T24:45:08Z", "2026-08-01T16:45:08+00:00", "garbage" }) {
			auto transport      = timeline_fixture_transport{ };
			transport.Responses = { success_response(timeline_full_page(1U), timeline_next(2U), page_completeness::has_next_page), success_response("[" + timeline_event(2U, invalidTime) + "]") };
			expect(!read_timeline(transport), "malformed later event fails atomically");
		}
		for (const auto body : { "[{\"event\":\"head_ref_force_pushed\",\"id\":1,\"commit_id\":null,\"created_at\":\"2026-08-01T16:45:08Z\"}]", "[{\"event\":\"head_ref_force_pushed\",\"id\":1.5}]" }) {
			auto transport      = timeline_fixture_transport{ };
			transport.Responses = { success_response(body) };
			expect(!read_timeline(transport), "missing after head and fractional ids are refused");
		}
		auto transport      = timeline_fixture_transport{ };
		transport.Responses = { success_response(timeline_full_page(1U), timeline_next(2U, "other.invalid"), page_completeness::has_next_page) };
		expect(!read_timeline(transport) && transport.Requests.size() == 1U, "cross-authority next link is refused before transport");
		transport.Requests.clear();
		transport.Responses = { success_response(timeline_full_page(1U), timeline_next(2U), page_completeness::has_next_page), success_response("[" + timeline_event(1U) + "]") };
		expect(!read_timeline(transport), "duplicate event identity across pages fails atomically");
		transport.Requests.clear();
		transport.Responses                = { success_response("{}") };
		transport.Responses.front().Status = 403;
		const auto permission              = read_timeline(transport);
		expect(!permission && std::get<application::github_provider_error>(permission.error()).Failure == application::github_provider_failure::permission,
			   "server refusal is typed and yields no timeline rows");
		transport.Requests.clear();
		transport.Responses         = { success_response(timeline_full_page(1U), timeline_next(2U), page_completeness::has_next_page) };
		auto byteLimits             = github_paged_read_limits{ };
		byteLimits.MaximumPageBytes = 1U;
		expect(!read_timeline(transport, byteLimits) && transport.Requests.size() == 1U, "timeline respects the caller page byte limit");
		transport.Requests.clear();
		transport.Responses.clear();
		for (std::uint32_t page = 1U; page <= 10U; ++page) {
			transport.Responses.push_back(success_response(timeline_full_page(page), timeline_next(page + 1U), page_completeness::has_next_page));
		}
		const auto capped = read_timeline(transport);
		expect(!capped && std::get<application::github_provider_error>(capped.error()).Failure == application::github_provider_failure::response_limit
				   && transport.Requests.size() == 10U,
			   "ten-page cap counts ignored entries and returns no partial history");
		transport.Requests.clear();
		auto limits              = github_paged_read_limits{ };
		limits.MaximumTotalPages = 1U;
		expect(!read_timeline(transport, limits) && transport.Requests.size() == 1U, "caller can tighten timeline page budget");
		transport.Requests.clear();
		const auto refused = perform_paged_github_timeline_read(transport, enterprise_profile(), anonymous_provider_request(), core::repository_id{ 101U }, { }, { }, github_provider_read_authority::anonymous_public);
		expect(!refused && transport.Requests.empty(), "GHES anonymous timeline is refused before any request");
		auto request                       = anonymous_provider_request();
		request.Context.ExpectedRepository = core::repository_id{ 102U };
		const auto mismatch                = perform_paged_github_timeline_read(transport, profile(), request, core::repository_id{ 101U }, { }, { }, github_provider_read_authority::anonymous_public);
		expect(!mismatch && transport.Requests.empty(), "mismatched accepted repository is refused before any request");
		std::stop_source stop;
		stop.request_stop();
		const auto cancelled = perform_paged_github_timeline_read(transport, profile(), anonymous_provider_request(), core::repository_id{ 101U }, stop.get_token(), { }, github_provider_read_authority::anonymous_public);
		expect(!cancelled && std::holds_alternative<github_paged_read_cancelled>(cancelled.error()) && transport.Requests.empty(), "cancelled timeline sends no request");
	}

	/// [[BPR-1560]]. One scripted answer per request; records what was asked.
	class merge_base_fixture_transport final : public github_wire_transport_port
	{
	public:
		std::vector<wire_response>       Responses;
		std::vector<github_wire_request> Requests;

		auto perform(const github_wire_request& aRequest, std::stop_token) -> github_wire_completion override
		{
			Requests.push_back(aRequest);
			expect(Requests.size() <= Responses.size(), "the merge-base read never issues an unprovided request");
			return github_wire_success{ .Correlation = aRequest.Correlation, .Response = Responses[Requests.size() - 1U] };
		}
	};

	[[nodiscard]] auto compare_body(const std::string_view aBase, const std::string_view aMergeBase) -> std::string
	{
		return R"({"status":"diverged","total_commits":3,"base_commit":{"sha":")" + std::string{ aBase } + R"("},"merge_base_commit":{"sha":")"
			+ std::string{ aMergeBase } + R"("},"commits":[],"files":[]})";
	}

	auto test_merge_base_read() -> void
	{
		constexpr auto tip       = std::string_view{ "3333333333333333333333333333333333333333" };
		constexpr auto mergeBase = std::string_view{ "4444444444444444444444444444444444444444" };
		constexpr auto recorded  = std::string_view{ "1111111111111111111111111111111111111111" };
		const auto     branch    = github_merge_base_basis{ .BaseRef = "release/2.0", .BaseOid = std::string{ recorded } };
		const auto     exact     = github_merge_base_basis{ .BaseOid = std::string{ recorded } };
		const auto     read      = [](merge_base_fixture_transport& aTransport, const github_merge_base_basis& aBasis, const validated_github_host_profile& aProfile,
									  const application::github_pull_request_provider_request& aRequest, const github_provider_read_authority aAuthority = github_provider_read_authority::authenticated) {
			return perform_github_merge_base_read(aTransport, aProfile, aRequest, core::repository_id{ 101U }, aBasis, head_oid, { }, { }, aAuthority);
		};

		// An open pull request: its head against its base branch, by name.
		auto transport = merge_base_fixture_transport{ };
		transport.Responses.push_back(success_response(compare_body(tip, mergeBase)));
		const auto named = read(transport, branch, profile(), provider_request());
		expect(named && named->MergeBaseOid == mergeBase && named->BaseCommitOid == tip, "the answer names the merge base and the tip it was computed against");
		expect(transport.Requests.size() == 1U && transport.Requests[0].Wire.Operation == read_operation::get_merge_base
				   && transport.Requests[0].Wire.Method == http_method::get
				   && transport.Requests[0].Wire.Path == "/repos/octo-org/review-repository/compare/release%2F2.0..." + std::string{ head_oid }
				   && transport.Requests[0].Wire.Query == "page=2&per_page=1" && transport.Requests[0].Wire.Body.empty(),
			   "one REST GET of compare, the branch encoded, on the page that carries no file list");

		// A closed pull request: an exact pair, and the answer must be about it.
		auto closed = merge_base_fixture_transport{ };
		closed.Responses.push_back(success_response(compare_body(recorded, mergeBase)));
		const auto pinned = read(closed, exact, profile(), provider_request());
		expect(pinned && pinned->MergeBaseOid == mergeBase && pinned->BaseCommitOid == recorded
				   && closed.Requests[0].Wire.Path == "/repos/octo-org/review-repository/compare/" + std::string{ recorded } + "..." + std::string{ head_oid },
			   "an exact pair is asked by its commits");
		auto other = merge_base_fixture_transport{ };
		other.Responses.push_back(success_response(compare_body(tip, mergeBase)));
		expect(!read(other, exact, profile(), provider_request()), "an answer about another base commit is refused");

		const auto refused = [&](std::string aBody, const int aStatus, const char* aMessage) {
			auto scripted   = merge_base_fixture_transport{ };
			auto response   = success_response(std::move(aBody));
			response.Status = aStatus;
			scripted.Responses.push_back(std::move(response));
			const auto result = read(scripted, branch, profile(), provider_request());
			expect(!result && std::holds_alternative<application::github_provider_error>(result.error()) && scripted.Requests.size() == 1U, aMessage);
		};
		refused(R"({"base_commit":{"sha":"3333333333333333333333333333333333333333"}})", 200, "an answer without a merge base is refused");
		refused(compare_body(tip, "44444444"), 200, "an abbreviated merge base is refused");
		refused(compare_body(tip, "4444444444444444444444444444444444444444444444444444444444444444"), 200, "a merge base of another object format is refused");
		refused("[]", 200, "a non-object answer is refused");
		refused(R"({"message":"Not Found"})", 404, "no common ancestor (not found) leaves no merge base");
		refused(R"({"message":"Server Error"})", 500, "a server error leaves no merge base");
		refused(R"({"message":"timeout"})", 504, "a diff timeout leaves no merge base");

		// GHES serves the same read under its API root.
		auto enterprise = merge_base_fixture_transport{ };
		enterprise.Responses.push_back(success_response(compare_body(tip, mergeBase)));
		const auto ghes = read(enterprise, branch, enterprise_profile(), provider_request(application::github_server_contract::ghes_3_18_2022_11_28));
		expect(ghes && ghes->MergeBaseOid == mergeBase && enterprise.Requests.size() == 1U
				   && enterprise.Requests[0].Wire.Path == "/api/v3/repos/octo-org/review-repository/compare/release%2F2.0..." + std::string{ head_oid },
			   "GHES 3.18 asks compare under /api/v3");

		// Signed out: admitted on github.com, refused on GHES before any request.
		auto signedOut = merge_base_fixture_transport{ };
		signedOut.Responses.push_back(success_response(compare_body(tip, mergeBase)));
		expect(read(signedOut, branch, profile(), anonymous_provider_request(), github_provider_read_authority::anonymous_public).has_value(),
			   "a public pull request's merge base is read signed out");
		auto refusedSignedOut = merge_base_fixture_transport{ };
		expect(!read(refusedSignedOut, branch, enterprise_profile(), anonymous_provider_request(), github_provider_read_authority::anonymous_public)
				   && refusedSignedOut.Requests.empty(),
			   "a signed-out GHES read sends nothing");
	}

	// [[BPR-0988]]. Personal discovery over a scripted wire.
	[[nodiscard]] auto discovery_request(const application::personal_discovery_reason aReason,
										 const application::github_server_contract    aServer = application::github_server_contract::github_com_2022_11_28)
		-> application::personal_discovery_request
	{
		const auto base = provider_request(aServer);
		return {
			.Context = {
				.Request            = core::request_id{ 41U },
				.ApplicationSession = core::application_session_id{ 7U },
				.Profile            = core::host_profile_id{ 11U },
				.Host               = core::git_host_id{ 22U },
				.Account            = core::github_account_id{ 1001U },
				.Repository         = { .Owner = "octo-org", .Name = "review-repository" },
				.Capabilities       = core::capability_revision{ 44U },
				.IdentityGeneration = core::github_identity_generation{ 3U },
			},
			.ProfileAuthority   = base.ProfileAuthority,
			.Viewer             = base.Viewer,
			.CapabilityEvidence = base.CapabilityEvidence,
			.AccountTransport   = base.AccountTransport,
			.Reason             = aReason,
			.Number             = aReason == application::personal_discovery_reason::retained ? std::optional{ core::pull_request_number{ 17U } } : std::nullopt,
		};
	}

	[[nodiscard]] auto discovery_item(const std::uint32_t aNumber, const std::string_view aLogin, const std::string_view aRoot = "https://api.github.com") -> std::string
	{
		const auto number = std::to_string(aNumber);
		return "{\"number\":" + number + ",\"title\":\"Pull request " + number + "\",\"state\":\"open\",\"draft\":false,"
			+ "\"pull_request\":{\"url\":\"" + std::string{ aRoot } + "/repos/octo-org/review-repository/pulls/" + number + "\"},"
			+ "\"repository_url\":\"" + std::string{ aRoot } + "/repos/Octo-Org/Review-Repository\","
			+ "\"user\":{\"login\":\"" + std::string{ aLogin } + "\",\"node_id\":\"U_" + std::string{ aLogin } + "\"}}";
	}

	[[nodiscard]] auto discovery_search(const std::vector<std::string>& aItems, const std::uint64_t aTotal, const bool aIncomplete = false,
										std::optional<std::string> aLink = std::nullopt) -> wire_response
	{
		auto body = "{\"total_count\":" + std::to_string(aTotal) + ",\"incomplete_results\":" + (aIncomplete ? "true" : "false") + ",\"items\":[";
		for (std::size_t index = 0U; index < aItems.size(); ++index) {
			body += (index == 0U ? "" : ",") + aItems[index];
		}
		body += "]}";
		const auto completeness = aLink ? page_completeness::has_next_page : page_completeness::complete;
		auto       response     = success_response(std::move(body), std::move(aLink), completeness);
		for (auto& header : response.Headers) {
			if (header.Name == "X-RateLimit-Resource") {
				header.Value = "search";
			} else if (header.Name == "X-RateLimit-Limit") {
				header.Value = "30";
			} else if (header.Name == "X-RateLimit-Remaining") {
				header.Value = "29";
			}
		}
		return response;
	}

	[[nodiscard]] auto requested_reviewers_response(const std::vector<std::string_view>& aUsers, const std::size_t aTeams) -> wire_response
	{
		auto body = std::string{ "{\"users\":[" };
		for (std::size_t index = 0U; index < aUsers.size(); ++index) {
			body += (index == 0U ? "{\"login\":\"" : ",{\"login\":\"") + std::string{ aUsers[index] } + "\",\"node_id\":\"U_x\"}";
		}
		body += "],\"teams\":[";
		for (std::size_t index = 0U; index < aTeams; ++index) {
			body += (index == 0U ? "" : ",") + std::string{ "{\"slug\":\"team\",\"node_id\":\"T_x\"}" };
		}
		body += "]}";
		return success_response(std::move(body));
	}

	class discovery_fixture_transport final : public github_wire_transport_port
	{
	public:
		explicit discovery_fixture_transport(const bool aEnterprise = false)
			: Enterprise{ aEnterprise }
		{ }
		bool                                                     Enterprise = false;
		std::vector<github_wire_request>                         Requests;
		std::function<wire_response(const github_wire_request&)> Search;
		std::function<wire_response(std::uint32_t)>              Reviewers;
		std::function<wire_response(const github_wire_request&)> Metadata;
		std::function<wire_response(const github_wire_request&)> Reviews;

		auto perform(const github_wire_request& aRequest, std::stop_token) -> github_wire_completion override
		{
			Requests.push_back(aRequest);
			expect(validate_github_wire_request(Enterprise ? enterprise_profile() : profile(), aRequest).has_value(), "every discovery request passes production wire validation");
			const auto& semantic = aRequest.Wire.Semantic;
			expect(semantic.Owner == "octo-org" && semantic.Repository == "review-repository", "discovery reads only the named repository");
			if (aRequest.Wire.Operation == read_operation::search_personal_pull_requests) {
				expect(std::holds_alternative<core::repository_request_context>(aRequest.Correlation.Parent), "the search is scoped to the repository");
				return github_wire_success{ .Correlation = aRequest.Correlation, .Response = Search(aRequest) };
			}
			if (aRequest.Wire.Operation == read_operation::get_pull_request || aRequest.Wire.Operation == read_operation::list_reviews) {
				expect(std::get<core::pull_request_request_context>(aRequest.Correlation.Parent).Number.value() == semantic.PullRequestNumber,
					   "retained metadata stays scoped to the exact enrolled PR");
				if (aRequest.Wire.Operation == read_operation::get_pull_request && !Metadata) {
					return github_wire_error{ .Correlation = aRequest.Correlation, .Error = application::github_provider_error{ .Failure = application::github_provider_failure::unavailable } };
				}
				return github_wire_success{ .Correlation = aRequest.Correlation,
											.Response    = aRequest.Wire.Operation == read_operation::get_pull_request ? Metadata(aRequest) : Reviews(aRequest) };
			}
			expect(aRequest.Wire.Operation == read_operation::list_requested_reviewers
					   && std::get<core::pull_request_request_context>(aRequest.Correlation.Parent).Number.value() == semantic.PullRequestNumber,
				   "discovery issues only the search and each row's requested reviewers");
			return github_wire_success{ .Correlation = aRequest.Correlation, .Response = Reviewers(semantic.PullRequestNumber) };
		}

		[[nodiscard]] auto count(const read_operation aOperation) const -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(Requests, [aOperation](const github_wire_request& aRequest) { return aRequest.Wire.Operation == aOperation; }));
		}
	};

	[[nodiscard]] auto discovery_failure(const github_personal_discovery_read_result& aResult) -> std::optional<application::github_provider_error>
	{
		if (aResult.has_value() || !std::holds_alternative<application::github_provider_error>(aResult.error())) {
			return std::nullopt;
		}
		return std::get<application::github_provider_error>(aResult.error());
	}

	auto test_personal_discovery_read() -> void
	{
		using reason = application::personal_discovery_reason;
		using full   = application::personal_discovery_completeness;
		{
			auto transport   = discovery_fixture_transport{ };
			transport.Search = [](const github_wire_request& aRequest) {
				expect(aRequest.Wire.Path == "/search/issues"
						   && aRequest.Wire.Query == "page=1&per_page=100&q=is%3Apr%20is%3Aopen%20author%3Aocto-reviewer%20repo%3Aocto-org%2Freview-repository",
					   "the authored search is exact");
				return discovery_search({ discovery_item(5U, "octo-reviewer"), discovery_item(6U, "Octo-Reviewer"), discovery_item(7U, "someone-else") }, 3U);
			};
			transport.Reviewers = [](const std::uint32_t aNumber) {
				return aNumber == 5U ? requested_reviewers_response({ "alice", "bob" }, 1U) : requested_reviewers_response({ }, 0U);
			};
			const auto result = perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), { });
			expect(result.has_value() && result->Completeness == full::complete && result->Rows.size() == 2U
					   && result->Rows[0].Number.value() == 5U && result->Rows[0].Detailed && result->Rows[0].ReviewersAwaited == 3U
					   && result->Rows[1].Number.value() == 6U && result->Rows[1].Detailed && result->Rows[1].ReviewersAwaited == 0U
					   && result->Rows[0].Title == "Pull request 5" && result->Rows[0].Author == "octo-reviewer",
				   "authored rows are the viewer's own, with the reviewers each awaits");
			expect(transport.count(read_operation::search_personal_pull_requests) == 1U && transport.count(read_operation::list_requested_reviewers) == 2U
					   && transport.Requests.size() == 5U,
				   "one search, bounded reviewer and optional Home-summary reads; another actor's row costs nothing");
			expect(result->RateLimits.Search.has_value() && result->RateLimits.Core.has_value(), "search and core rate observations stay separate");
		}
		{
			auto transport      = discovery_fixture_transport{ };
			transport.Search    = [](const auto&) { return discovery_search({ discovery_item(5, "octo-reviewer") }, 1); };
			transport.Reviewers = [](std::uint32_t) { return requested_reviewers_response({ }, 0); };
			transport.Metadata  = [](const auto&) { return success_response(std::string(4U * 1024U * 1024U + 1U, 'x')); };
			const auto result   = perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), { });
			expect(result && result->Rows.size() == 1 && result->Rows.front().Waiting.Requests == 0 && result->Completeness == full::detail_limit,
				   "oversized added metadata leaves complete REST facts usable and explicitly marks discovery partial");
		}

		{
			auto transport   = discovery_fixture_transport{ };
			transport.Search = [](const github_wire_request& aRequest) {
				expect(aRequest.Wire.Query.find("review-requested%3Aocto-reviewer") != std::string::npos, "the requested search names the viewer");
				return discovery_search({ discovery_item(8U, "author-a"), discovery_item(9U, "author-b"), discovery_item(10U, "author-c") }, 3U);
			};
			transport.Reviewers = [](const std::uint32_t aNumber) {
				return aNumber == 8U ? requested_reviewers_response({ "OCTO-REVIEWER" }, 0U)
					: aNumber == 9U  ? requested_reviewers_response({ }, 1U)
									 : requested_reviewers_response({ "someone" }, 0U);
			};
			const auto result = perform_personal_discovery_read(transport, profile(), discovery_request(reason::requested), { });
			expect(result.has_value() && result->Completeness == full::complete && result->Rows.size() == 1U && result->Rows[0].Number.value() == 8U
					   && result->Rows[0].Detailed && result->Rows[0].Author == "author-a",
				   "only a request that names the viewer as a user is kept; a team-only request is not the viewer's");
		}
		{
			auto transport      = discovery_fixture_transport{ };
			transport.Search    = [](const github_wire_request&) { return discovery_search({ discovery_item(5U, "octo-reviewer") }, 1U, true); };
			transport.Reviewers = [](std::uint32_t) { return requested_reviewers_response({ }, 0U); };
			const auto result   = perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), { });
			expect(result.has_value() && result->Completeness == full::incomplete_results && result->Rows.size() == 1U,
				   "an incomplete GitHub search keeps its rows and is never complete");
		}
		{
			auto items = std::vector<std::string>{ };
			for (std::uint32_t number = 1U; number <= 100U; ++number) {
				items.push_back(discovery_item(number, "octo-reviewer"));
			}
			auto transport   = discovery_fixture_transport{ };
			transport.Search = [&items](const github_wire_request&) {
				return discovery_search(items, 250U, false,
										"<https://api.github.com/search/issues?page=2&per_page=100&q=is%3Apr+is%3Aopen+author%3Aocto-reviewer+repo%3Aocto-org%2Freview-repository>; rel=\"next\"");
			};
			transport.Reviewers = [](std::uint32_t) { return requested_reviewers_response({ "x" }, 0U); };
			const auto result   = perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), { });
			const auto detailed = result ? std::ranges::count_if(result->Rows, &application::personal_discovery_row::Detailed) : 0;
			expect(result.has_value() && result->Completeness == full::result_limit && result->Rows.size() == 100U
					   && detailed == static_cast<std::ptrdiff_t>(application::maximum_personal_discovery_details)
					   && transport.count(read_operation::search_personal_pull_requests) == 1U
					   && transport.count(read_operation::list_requested_reviewers) == application::maximum_personal_discovery_details,
				   "a longer match is one page, partial, with bounded detail reads and unknown detail beyond them");
		}
		{
			auto items = std::vector<std::string>{ };
			for (std::uint32_t number = 1U; number <= 31U; ++number) {
				items.push_back(discovery_item(number, "author"));
			}
			auto transport      = discovery_fixture_transport{ };
			transport.Search    = [&items](const github_wire_request&) { return discovery_search(items, 31U); };
			transport.Reviewers = [](std::uint32_t) { return requested_reviewers_response({ "octo-reviewer" }, 0U); };
			const auto result   = perform_personal_discovery_read(transport, profile(), discovery_request(reason::requested), { });
			expect(result.has_value() && result->Completeness == full::detail_limit && result->Rows.size() == application::maximum_personal_discovery_details,
				   "requested rows beyond the detail bound are unconfirmed, dropped and make the answer partial");
		}
		{
			auto transport   = discovery_fixture_transport{ true };
			transport.Search = [](const github_wire_request& aRequest) {
				expect(aRequest.Wire.Path == "/api/v3/search/issues", "the GHES search uses the enterprise REST root");
				return discovery_search({ discovery_item(5U, "octo-reviewer", "https://ghe.example.invalid/api/v3") }, 1U);
			};
			transport.Reviewers = [](std::uint32_t) { return requested_reviewers_response({ }, 2U); };
			const auto result   = perform_personal_discovery_read(transport, enterprise_profile(), discovery_request(reason::authored, application::github_server_contract::ghes_3_18_2022_11_28), { });
			expect(result.has_value() && result->Rows.size() == 1U && result->Rows[0].ReviewersAwaited == 2U
					   && std::ranges::any_of(transport.Requests, [](const auto& aRequest) { return aRequest.Wire.Path == "/api/v3/repos/octo-org/review-repository/pulls/5/requested_reviewers"; }),
				   "the same bounded personal and optional metadata reads on a GHES 3.18 fixture contract");
		}
		const auto failing = [](const int aStatus, const std::string_view aRemaining, const bool aDetail, std::string aBody = "{\"message\":\"no\"}") {
			auto       transport = discovery_fixture_transport{ };
			const auto fail      = [aStatus, aRemaining, aBody](wire_response aResponse) {
				aResponse.Status = aStatus;
				aResponse.Body   = aBody;
				for (auto& header : aResponse.Headers) {
					if (header.Name == "X-RateLimit-Remaining") {
						header.Value = aRemaining;
					}
				}
				return aResponse;
			};
			transport.Search = [&](const github_wire_request&) {
				auto response = discovery_search({ discovery_item(5U, "octo-reviewer") }, 1U);
				return aDetail ? response : fail(std::move(response));
			};
			transport.Reviewers = [&](std::uint32_t) {
				auto response = requested_reviewers_response({ }, 0U);
				return aDetail ? fail(std::move(response)) : response;
			};
			return discovery_failure(perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), { }));
		};
		const auto primary = failing(403, "0", false);
		expect(primary && primary->Failure == application::github_provider_failure::rate_limit && primary->RateLimitCause
				   && primary->RateLimitCause->Resource == application::github_rate_limit_resource::search && !primary->RateLimitCause->SecondaryLimited,
			   "a spent search allowance is the primary search limit");
		const auto secondary = failing(403, "20", false, "{\"message\":\"You have exceeded a secondary rate limit. Please wait a few minutes before you try again.\"}");
		expect(secondary && secondary->Failure == application::github_provider_failure::rate_limit && secondary->RateLimitCause
				   && secondary->RateLimitCause->SecondaryLimited && secondary->RetryAfterSeconds.has_value(),
			   "a secondary limit is its own kind, with a retry");
		const auto authentication = failing(401, "29", false);
		expect(authentication && authentication->Failure == application::github_provider_failure::authentication, "401 is an authentication failure");
		const auto removed = failing(422, "29", false);
		expect(removed && removed->Failure == application::github_provider_failure::permission, "a refused repository qualifier is lost access");
		const auto forbidden = failing(403, "29", false);
		expect(forbidden && forbidden->Failure == application::github_provider_failure::permission, "403 is lost access");
		const auto detail = failing(404, "4999", true);
		expect(detail && detail->Failure == application::github_provider_failure::permission, "a failed detail read fails the whole answer");
		{
			auto transport      = discovery_fixture_transport{ };
			transport.Search    = [](const github_wire_request&) { return discovery_search({ discovery_item(5U, "octo-reviewer") }, 1U); };
			transport.Reviewers = [](std::uint32_t) { return requested_reviewers_response({ }, 0U); };
			std::stop_source stop;
			stop.request_stop();
			const auto cancelled = perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), stop.get_token());
			expect(!cancelled && std::holds_alternative<github_paged_read_cancelled>(cancelled.error()) && transport.Requests.empty(), "a cancelled discovery sends nothing");
			auto foreign           = discovery_request(reason::authored);
			foreign.Viewer.Account = core::github_account_id{ 1002U };
			const auto refused     = perform_personal_discovery_read(transport, profile(), foreign, { });
			expect(!refused && transport.Requests.empty(), "a viewer of another account is refused before any request");
			auto noLease             = discovery_request(reason::requested);
			noLease.AccountTransport = nullptr;
			expect(!perform_personal_discovery_read(transport, profile(), noLease, { }) && transport.Requests.empty(), "no account lease, no request");
			auto hostile         = discovery_request(reason::authored);
			hostile.Viewer.Login = "octo repo:other/secret";
			expect(!perform_personal_discovery_read(transport, profile(), hostile, { }) && transport.Requests.empty(), "a login cannot smuggle a qualifier");
		}
		{
			auto transport      = discovery_fixture_transport{ };
			transport.Search    = [](const github_wire_request&) { return discovery_search({ discovery_item(5U, "octo-reviewer"), discovery_item(5U, "octo-reviewer") }, 2U); };
			transport.Reviewers = [](std::uint32_t) { return requested_reviewers_response({ }, 0U); };
			expect(!perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), { }), "a duplicated pull request is refused");
			transport.Requests.clear();
			transport.Search = [](const github_wire_request&) {
				return discovery_search({ "{\"number\":5,\"title\":\"x\",\"state\":\"open\",\"pull_request\":{},\"repository_url\":\"https://api.github.com/repos/octo-org/other\",\"user\":null}" }, 1U);
			};
			expect(!perform_personal_discovery_read(transport, profile(), discovery_request(reason::authored), { }), "a row of another repository is refused");
		}
		{
			// The wire refuses a discovery request under any other parent.
			auto       semantic = semantic_read_request{ .Server = server_contract::github_com, .Operation = read_operation::search_personal_pull_requests, .Owner = "octo-org", .Repository = "review-repository", .SearchTerms = "author:octo-reviewer" };
			const auto wire     = compile_read_request(semantic);
			const auto base     = discovery_request(reason::authored);
			auto       valid    = github_wire_request{
				.Correlation = { .Parent = base.Context, .ProfileRevision = profile().revision(), .Host = profile().host(), .Operation = read_operation::search_personal_pull_requests, .Page = 1U },
				.Wire        = *wire,
			};
			expect(validate_github_wire_request(profile(), valid).has_value(), "the repository parent admits its own search");
			auto underPullRequest               = valid;
			underPullRequest.Correlation.Parent = provider_request().Context;
			expect(!validate_github_wire_request(profile(), underPullRequest).has_value(), "a pull request parent cannot issue a personal search");
			auto otherRepository                                                                           = valid;
			std::get<core::repository_request_context>(otherRepository.Correlation.Parent).Repository.Name = "other";
			expect(!validate_github_wire_request(profile(), otherRepository).has_value(), "a repository parent cannot search another repository");
			auto reviewers                      = semantic_read_request{ .Server = server_contract::github_com, .Operation = read_operation::list_requested_reviewers, .Owner = "octo-org", .Repository = "review-repository", .PullRequestNumber = 17U };
			auto reviewersWire                  = valid;
			reviewersWire.Wire                  = *compile_read_request(reviewers);
			reviewersWire.Correlation.Operation = read_operation::list_requested_reviewers;
			expect(!validate_github_wire_request(profile(), reviewersWire).has_value(), "a repository parent admits no other operation");
			reviewersWire.Correlation.Parent = provider_request().Context;
			expect(validate_github_wire_request(profile(), reviewersWire).has_value(), "requested reviewers are read under their pull request");
		}
	}

	[[nodiscard]] auto retained_metadata(const std::string_view aRoot = "https://api.github.com") -> fixture_object
	{
		return {
			{ "number", 17 },
			{ "title", "Fix the queue before opening" },
			{ "state", "open" },
			{ "url", std::string{ aRoot } + "/repos/octo-org/review-repository/pulls/17" },
			{ "user", rest_actor("U_author", "author") },
			{ "head", fixture_object{ { "sha", std::string{ head_oid } } } },
			{ "base", fixture_object{ { "repo", fixture_object{ { "id", 101U }, { "full_name", "octo-org/review-repository" } } } } },
			{ "requested_reviewers", fixture_array{ rest_actor("U_other", "other") } },
			{ "requested_teams", fixture_array{ fixture_object{ { "slug", "team" } } } },
		};
	}

	[[nodiscard]] auto retained_review(const std::uint64_t aId, const std::string_view aState = "APPROVED") -> fixture_object
	{
		return {
			{ "id", aId },
			{ "node_id", "R_" + std::to_string(aId) },
			{ "user", rest_actor("U_viewer", "octo-reviewer") },
			{ "state", std::string{ aState } },
			{ "commit_id", std::string{ head_oid } },
			{ "body", "Review prose does not enter Home." },
		};
	}

	void test_home_draft_metadata_is_an_optional_wire_fact()
	{
		for (const auto mode : { 0, 1, 2, 3 }) {
			auto transport      = discovery_fixture_transport{ };
			transport.Reviewers = [](const std::uint32_t) { return requested_reviewers_response({ }, 0U); };
			transport.Reviews   = [](const auto&) { return success_response(fixture_json(fixture_array{ })); };
			transport.Search    = [mode](const auto&) {
				auto       item = discovery_item(17U, "octo-reviewer");
				const auto at   = item.find("\"draft\":false,");
				expect(at != std::string::npos, "draft fixture field exists");
				item.replace(at, 14U, mode == 0 ? "" : mode == 1 ? "\"draft\":false,"
								 : mode == 2                     ? "\"draft\":true,"
																 : "\"draft\":1,");
				return discovery_search({ item }, 1U);
			};
			transport.Metadata = [mode](const auto&) {
				auto metadata = retained_metadata();
				if (mode == 1 || mode == 2) {
					metadata.insert_or_assign("draft", mode == 2);
				}
				if (mode == 3) {
					metadata.insert_or_assign("draft", "true");
				}
				return success_response(fixture_json(metadata));
			};
			for (const auto reason : { application::personal_discovery_reason::authored, application::personal_discovery_reason::retained }) {
				const auto result = perform_personal_discovery_read(transport, profile(), discovery_request(reason), { });
				if (mode == 3) {
					expect(discovery_failure(result) && discovery_failure(result)->Failure == application::github_provider_failure::malformed_response, "malformed status cannot be accepted");
				} else {
					expect(result && result->Rows.size() == 1U && result->Rows.front().Draft == (mode == 0 ? std::nullopt : std::optional<bool>{ mode == 2 }), "missing draft stays unknown; observed booleans stay exact");
				}
			}
		}
	}

	/// [[BPR-1407]]. Age, size and comment count come from the pull request
	/// read Home already makes. Absent is unknown; a retained read refuses a
	/// malformed one, and a discovered row's optional summary leaves it unknown.
	void test_home_change_facts_are_read_with_the_pull_request()
	{
		using reason = application::personal_discovery_reason;
		for (const auto mode : { 0, 1, 2, 3, 4 }) {
			auto transport      = discovery_fixture_transport{ };
			transport.Reviewers = [](const std::uint32_t) { return requested_reviewers_response({ }, 0U); };
			transport.Reviews   = [](const auto&) { return success_response(fixture_json(fixture_array{ })); };
			transport.Search    = [](const auto&) { return discovery_search({ discovery_item(17U, "octo-reviewer") }, 1U); };
			transport.Metadata  = [mode](const auto&) {
				auto metadata = retained_metadata();
				if (mode != 0) {
					metadata.insert_or_assign("created_at", mode == 3 ? "last Tuesday" : "2026-10-06T11:15:00Z");
					metadata.insert_or_assign("additions", 236);
					metadata.insert_or_assign("deletions", mode == 4 ? -41 : 41);
					metadata.insert_or_assign("comments", 1);
				}
				if (mode == 1 || mode == 3 || mode == 4) {
					metadata.insert_or_assign("review_comments", 2);
				}
				return success_response(fixture_json(metadata));
			};
			for (const auto why : { reason::authored, reason::retained }) {
				const auto result = perform_personal_discovery_read(transport, profile(), discovery_request(why), { });
				const auto change = result && result->Rows.size() == 1U ? std::optional{ result->Rows.front().Change } : std::nullopt;
				if (mode == 0) {
					expect(change == core::queue_change_facts{ }, "a pull request without the fields leaves every change fact unknown");
				} else if (mode == 1) {
					expect(change == core::queue_change_facts{ .OpenedAt = 1'791'285'300, .Additions = 236U, .Deletions = 41U, .Comments = 3U }, "opening, lines and conversation plus inline comments are read");
				} else if (mode == 2) {
					expect(change && change->Additions == 236U && !change->Comments, "a total needs both comment counts");
				} else if (why == reason::retained) {
					expect(discovery_failure(result) && discovery_failure(result)->Failure == application::github_provider_failure::malformed_response, "a retained read refuses a malformed change fact");
				} else {
					expect(change == core::queue_change_facts{ }, "a discovered row's optional summary leaves a malformed change unknown");
				}
			}
			expect(transport.count(read_operation::get_pull_request) == 2U, "no request is added for the change facts");
		}
	}

	void test_retained_home_metadata_read()
	{
		using reason = application::personal_discovery_reason;
		for (const auto enterprise : { false, true }) {
			auto       transport = discovery_fixture_transport{ enterprise };
			const auto root      = enterprise ? "https://ghe.example.invalid/api/v3" : "https://api.github.com";
			transport.Metadata   = [root](const auto&) { return success_response(fixture_json(retained_metadata(root))); };
			transport.Reviews    = [root](const github_wire_request& aRequest) {
				if (aRequest.Wire.Semantic.Page == 1U) {
					auto reviews = fixture_array{ };
					for (std::uint64_t id = 1U; id <= 100U; ++id) {
						reviews.push_back(retained_review(id, "PENDING"));
					}
					return success_response(fixture_json(reviews), "<" + std::string{ root } + "/repos/octo-org/review-repository/pulls/17/reviews?page=2&per_page=100>; rel=\"next\"", page_completeness::has_next_page);
				}
				return success_response(fixture_json(fixture_array{ retained_review(101U) }));
			};
			const auto server             = enterprise ? application::github_server_contract::ghes_3_18_2022_11_28 : application::github_server_contract::github_com_2022_11_28;
			auto       request            = discovery_request(reason::retained, server);
			request.AuthenticationSession = core::application_session_id{ 7U };
			const auto result             = perform_personal_discovery_read(transport, enterprise ? enterprise_profile() : profile(), request, { });
			expect(result && result->Rows.size() == 1U && result->Rows.front().Title == "Fix the queue before opening"
					   && result->Rows.front().HeadOid == application::commit_oid{ std::string{ head_oid } }
					   && result->Rows.front().ReviewersAwaited == 2U && result->Rows.front().Waiting.Requests == 2U && result->Rows.front().Reviews.size() == 101U
					   && result->Rows.front().Reviews.back().State == application::github_review_state::approved,
				   "retained Home read carries title, current head and complete ordered review evidence");
			expect(transport.Requests.size() == 3U && transport.count(read_operation::get_pull_request) == 1U
					   && transport.count(read_operation::list_reviews) == 2U && transport.count(read_operation::search_personal_pull_requests) == 0U,
				   "metadata reads only the enrolled PR and review summaries; no files, commits, diff or conversations");
		}
		{
			auto transport     = discovery_fixture_transport{ };
			transport.Metadata = [](const auto&) {
				auto metadata = retained_metadata();
				metadata.insert_or_assign("node_id", "PR_17");
				metadata.insert_or_assign("base", fixture_object{ { "sha", std::string{ base_oid } }, { "ref", "main" }, { "repo", fixture_object{ { "id", 37 }, { "node_id", "R_repository" }, { "full_name", "octo-org/review-repository" } } } });
				return success_response(fixture_json(metadata));
			};
			transport.Reviews        = [](const auto&) { return success_response("[]"); };
			auto limits              = github_paged_read_limits{ };
			limits.MaximumTotalPages = 2;
			const auto result        = perform_personal_discovery_read(transport, profile(), discovery_request(reason::retained), { }, limits);
			expect(result && result->Rows.size() == 1 && result->Rows.front().WaitingLimited && result->Rows.front().Waiting.Requests == 2 && transport.Requests.size() == 2,
				   "retained optional summary exhaustion is explicit and sends no extra page; complete metadata request count remains usable");
		}
		for (const auto violation : { 0, 1, 2, 3 }) {
			auto transport     = discovery_fixture_transport{ };
			transport.Metadata = [violation](const auto&) {
				auto metadata = retained_metadata();
				if (violation == 0) {
					metadata["number"] = 18;
				}
				if (violation == 1) {
					metadata["url"] = "https://api.github.com/repos/other/repository/pulls/17";
				}
				if (violation == 2) {
					metadata["head"] = fixture_object{ { "sha", "invalid" } };
				}
				return success_response(fixture_json(metadata));
			};
			transport.Reviews = [](const auto&) { return success_response(fixture_json(fixture_array{ retained_review(1U), retained_review(1U) })); };
			const auto result = perform_personal_discovery_read(transport, profile(), discovery_request(reason::retained), { });
			expect(discovery_failure(result) && discovery_failure(result)->Failure == application::github_provider_failure::malformed_response,
				   "wrong PR, repository, head or duplicate review fails the metadata answer atomically");
		}
		{
			auto transport     = discovery_fixture_transport{ };
			transport.Metadata = [](const auto&) { return success_response(fixture_json(retained_metadata())); };
			transport.Reviews  = [](const github_wire_request& aRequest) {
				auto reviews = fixture_array{ };
				for (std::uint64_t index = 0U; index < 100U; ++index) {
					reviews.push_back(retained_review((aRequest.Wire.Semantic.Page - 1U) * 100U + index + 1U));
				}
				return success_response(fixture_json(reviews), "<https://api.github.com/repos/octo-org/review-repository/pulls/17/reviews?page=" + std::to_string(aRequest.Wire.Semantic.Page + 1U) + "&per_page=100>; rel=\"next\"", page_completeness::has_next_page);
			};
			const auto result = perform_personal_discovery_read(transport, profile(), discovery_request(reason::retained), { });
			expect(discovery_failure(result) && discovery_failure(result)->Failure == application::github_provider_failure::response_limit
					   && transport.Requests.size() == 5U,
				   "a retained read stops after four review pages and never publishes truncated review state");
		}
		{
			auto transport = discovery_fixture_transport{ };
			auto stop      = std::stop_source{ };
			stop.request_stop();
			const auto result = perform_personal_discovery_read(transport, profile(), discovery_request(reason::retained), stop.get_token());
			expect(!result && std::holds_alternative<github_paged_read_cancelled>(result.error()) && transport.Requests.empty(), "cancelled retained read sends nothing");
			auto request   = discovery_request(reason::requested);
			request.Number = core::pull_request_number{ 17U };
			expect(!perform_personal_discovery_read(transport, profile(), request, { }) && transport.Requests.empty(), "a search cannot carry a retained PR number");
		}
	}

	void test_raw_tour_import()
	{
		const std::string prefix    = "https://github.com/octo-org/review-repository/pull/17";
		auto              transport = paged_fixture_transport{ fixture_scenario::success };
		transport.RewriteResponse   = [&](const github_wire_request& aRequest, wire_response& aResponse) {
			if (aRequest.Wire.Operation == read_operation::list_reviews) {
				const auto body = "1. [same](" + prefix + "#discussion_r101)\r\n1. [same](" + prefix + "#discussion_r18446744073709551615)\r\n";
				aResponse.Body  = fixture_json(fixture_array{ fixture_object{
					{ "body", body }, { "commit_id", std::string{ head_oid } }, { "id", 4001 }, { "node_id", "R_review" }, { "state", "COMMENTED" }, { "html_url", prefix + "#pullrequestreview-4001" }, { "user", rest_actor("U_viewer", "octo-reviewer") } } });
			}
			if (aRequest.Wire.Operation == read_operation::list_review_threads) {
				expect(aRequest.Wire.Body.find("body url updatedAt fullDatabaseId") != std::string::npos, "production query selects raw body and real identity");
				for (std::size_t index = 0; index < 101; ++index) {
					const auto id       = index == 0 ? "18446744073709551615" : std::to_string(index + 1);
					const auto raw      = "### Author's tour · 9 — same\r\n\r\n**raw** " + std::to_string(index) + "\r\n\r\n[Tour overview](" + prefix + "#pullrequestreview-4001)\r\n";
					const auto existing = "\"bodyText\":\"comment " + std::to_string(index) + "\"";
					const auto added    = existing + ",\"body\":" + fixture_json(raw) + ",\"url\":" + fixture_json(prefix + "#discussion_r" + id)
						+ ",\"fullDatabaseId\":\"" + id + "\",\"updatedAt\":\"2026-09-26T00:00:00Z\",\"pullRequestReview\":{\"id\":\"R_review\"}";
					replace_raw(aResponse.Body, existing, added);
				}
				replace_raw(aResponse.Body, "\"author\":null", "\"author\":{\"id\":\"U_viewer\",\"login\":\"octo-reviewer\"}");
			}
		};
		const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { });
		expect(result.has_value(), "raw tour pages accepted");
		const auto& comments = result->ReviewThreads.front().Comments;
		expect(comments.size() == 101 && comments[0].Body == "comment 0" && comments[100].Body == "comment 100", "all pages exhausted and existing display prose unchanged");
		expect(comments[0].Observation.DatabaseId == std::numeric_limits<std::uint64_t>::max() && comments[100].Observation.RawBody->find("**raw** 100\r\n") != std::string::npos, "full uint64 and raw CRLF Markdown survive pages");
		auto seed              = tests::review_tour_fixture();
		seed.Owner.Host        = result->Host;
		seed.Owner.Account     = result->Account;
		seed.Owner.Repository  = result->Repository.Repository;
		seed.Owner.PullRequest = result->PullRequest.PullRequest;
		seed.Stops.clear();
		seed.LastStopId = 0;
		seed.Problem.clear();
		seed.Solution.clear();
		seed.ProblemBaseline.reset();
		seed.SolutionBaseline.reset();
		const auto calls    = transport.Requests.size();
		const auto imported = application::observe_review_tours(seed, *result, "github.com");
		expect(imported.size() == 1 && imported[0].Draft.Stops.size() == 2 && !imported[0].Draft.ReadOnly, "real paged snapshot cold imports without private IDs or AI");
		expect(imported[0].Draft.Stops[0].Remote->DatabaseId == 101 && imported[0].Draft.Stops[1].Remote->DatabaseId == std::numeric_limits<std::uint64_t>::max(), "index order crosses comment pages and server order");
		auto foreign                                              = *result;
		foreign.ReviewThreads[0].Comments[100].Observation.WebUrl = "https://evil.invalid/steal";
		expect(application::observe_review_tours(seed, foreign, "github.com")[0].Draft.ReadOnly && transport.Requests.size() == calls, "foreign links never fetch or forward credentials");
		auto malformed            = paged_fixture_transport{ fixture_scenario::success };
		malformed.RewriteResponse = [](const github_wire_request& aRequest, wire_response& aResponse) {if(aRequest.Wire.Operation==read_operation::list_review_threads){replace_raw(aResponse.Body,"\"bodyText\":\"comment 0\"","\"bodyText\":\"comment 0\",\"body\":42");} };
		expect(!perform_paged_github_read(malformed, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { }), "malformed raw data fails atomically");
	}
}

namespace {
	void test_optional_checks_cannot_spend_mandatory_read_budget()
	{
		for (const auto bytes : { false, true }) {
			auto transport                       = paged_fixture_transport{ fixture_scenario::success };
			auto limits                          = github_paged_read_limits{ };
			limits.MaximumTotalPages             = bytes ? 256U : 12U;
			limits.MaximumAggregateResponseBytes = bytes ? 128U * 1024U : limits.MaximumAggregateResponseBytes;
			limits.MaximumPageBytes              = std::min(limits.MaximumPageBytes, limits.MaximumAggregateResponseBytes);
			auto servedBytes                     = std::size_t{ 0 };
			transport.RewriteResponse            = [&](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation == read_operation::list_commit_statuses) {
					if (bytes) {
						aResponse.Body = "[]";
						expect(servedBytes + aResponse.Body.size() + 1 < limits.MaximumAggregateResponseBytes, "byte probe has room for a valid status page");
						aResponse.Body.resize(limits.MaximumAggregateResponseBytes - servedBytes - 1, ' ');
					} else {
						auto statuses = fixture_array{ };
						for (unsigned index = 0; index < 100; ++index) {
							const auto identity = std::to_string((aRequest.Wire.Semantic.Page - 1U) * 100U + index);
							statuses.push_back(fixture_object{ { "node_id", "status-" + identity }, { "context", "context-" + identity }, { "state", "success" }, { "description", nullptr }, { "target_url", nullptr } });
						}
						aResponse.Body = fixture_json(statuses);
						aResponse.Headers.push_back({ "Link", "<https://api.github.com/repos/octo-org/review-repository/commits/" + std::string{ head_oid } + "/statuses?per_page=100&page=" + std::to_string(aRequest.Wire.Semantic.Page + 1U) + ">; rel=\"next\"" });
						aResponse.Completeness = page_completeness::has_next_page;
					}
				}
				servedBytes += aResponse.Body.size();
			};
			const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { }, limits);
			expect(result && !result->Files.empty() && result->SearchAvailability == application::github_search_availability::available, "optional status page/byte exhaustion cannot erase mandatory readable PR and search facts");
			if (!bytes) {
				expect(result->StatusesAvailability == application::github_collection_availability::unavailable && result->Checks.size() == 1, "unfinished bounded status history stays unavailable while known check runs survive");
			}
		}
	}

	void test_current_check_details()
	{
		for (const auto limitedStatuses : { false, true }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			transport.RewriteResponse = [limitedStatuses](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation != read_operation::list_commit_statuses) {
					return;
				}
				aResponse.Body = R"([{"node_id":"new","context":"c","state":"success","description":null,"target_url":null},{"node_id":"old","context":"c","state":"failure","description":null,"target_url":null}])";
				if (!limitedStatuses) {
					aResponse.Headers.push_back({ "Link", "<https://foreign.invalid/steal>; rel=\"next\"" });
					aResponse.Completeness = page_completeness::has_next_page;
				}
			};
			auto limits = github_paged_read_limits{ };
			if (limitedStatuses) {
				limits.Records.MaximumChecks = 1U;
			}
			const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { }, limits);
			expect(result && result->Checks.size() == 1 && result->StatusesAvailability == application::github_collection_availability::unavailable && !result->Files.empty(), "limited or hostile status pagination preserves known checks as explicitly partial and keeps PR content");
		}
		for (const auto enterprise : { false, true }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success, enterprise ? "ghe.example.invalid" : "api.github.com" };
			transport.RewriteResponse = [](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation == read_operation::list_checks) {
					replace_raw(aResponse.Body, "\"name\":\"native-build\"", "\"name\":\"Branch Freshness\",\"app\":{\"slug\":\"freshness-app\"},\"output\":{\"title\":\"Branch behind base\",\"summary\":\"Update the branch before merging.\"}");
					replace_raw(aResponse.Body, "\"conclusion\":\"success\"", "\"conclusion\":\"failure\"");
				} else if (aRequest.Wire.Operation == read_operation::list_commit_statuses) {
					aResponse.Body = R"([{"node_id":"current","context":"Branch Freshness","state":"success","description":"Current attempt succeeded","target_url":"https://ci.example.invalid/result","creator":{"login":"status-service"}},{"node_id":"old","context":"Branch Freshness","state":"failure","description":"Old failed attempt","target_url":null}])";
				}
			};
			const auto result = perform_paged_github_read(transport, enterprise ? enterprise_profile() : profile(), provider_request(enterprise ? application::github_server_contract::ghes_3_18_2022_11_28 : application::github_server_contract::github_com_2022_11_28), core::provider_snapshot_id{ 55 }, { });
			expect(result && result->Checks.size() == 2 && result->StatusesAvailability == application::github_collection_availability::available, "native contracts retain one latest status and the distinct same-name check run");
			expect(result->Checks[0].Producer == "freshness-app" && result->Checks[0].Description == "Branch behind base\nUpdate the branch before merging." && result->Checks[0].DetailsUrl == "https://checks.example.invalid/6001", "bounded check explanation and destination survive the provider");
			expect(result->Checks[1].Source == application::github_check_source::commit_status && result->Checks[1].Conclusion == application::github_check_conclusion::success && result->Checks[1].EvaluatedOid.value() == head_oid, "old failed attempts and different result sources never contaminate current outcomes");
			for (const auto& request : transport.Requests) {
				if (request.Wire.Operation == read_operation::list_commit_statuses) {
					expect(request.Wire.Semantic.HeadOid == head_oid && request.Wire.Path.ends_with("/commits/" + std::string{ head_oid } + "/statuses"), "status read binds exact accepted head in closed REST route");
				}
			}
		}
		for (const auto scenario : { fixture_scenario::check_missing_required, fixture_scenario::check_wrong_required, fixture_scenario::check_envelope_missing_required, fixture_scenario::check_envelope_wrong_required }) {
			auto       transport = paged_fixture_transport{ scenario };
			const auto result    = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { });
			expect(result && result->Checks.empty() && result->ChecksAvailability == application::github_collection_availability::unavailable && !result->Files.empty(), "wrong-head or malformed optional check collection is unavailable without hiding readable PR content");
		}
		for (const auto bad : { "not-json", "[{\"node_id\":\"s\",\"context\":\"c\",\"state\":\"fake\"}]" }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			transport.RewriteResponse = [bad](const github_wire_request& aRequest, wire_response& aResponse) {if(aRequest.Wire.Operation==read_operation::list_commit_statuses){aResponse.Body=bad;} };
			const auto result         = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { });
			expect(result && result->Checks.size() == 1 && result->StatusesAvailability == application::github_collection_availability::unavailable, "optional status refusal retains readable known check runs and explicit partial availability");
		}
	}
	void test_review_requirements()
	{
		using availability = application::github_observation_availability;
		for (const auto enterprise : { false, true }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success, enterprise ? "ghe.example.invalid" : "api.github.com" };
			transport.RewriteResponse = [&](const github_wire_request& aRequest, wire_response& aResponse) {
				auto pull = fixture_object{ { "id", "PR_17" }, { "headRefOid", std::string{ head_oid } }, { "baseRefOid", std::string{ base_oid } }, { "baseRefName", "main" } };
				if (aRequest.Wire.Operation == read_operation::get_merge_assessment) {
					pull.insert_or_assign("mergeStateStatus", "BLOCKED");
					pull.insert_or_assign("reviewDecision", "REVIEW_REQUIRED");
					pull.insert_or_assign("autoMergeRequest", nullptr);
				} else if (aRequest.Wire.Operation == read_operation::list_review_requests) {
					auto nodes = fixture_array{ };
					for (int index = 0; index < 3; ++index) {
						nodes.push_back(fixture_object{ { "id", "RR_" + std::to_string(index) }, { "asCodeOwner", true }, { "requestedReviewer", fixture_object{ { "__typename", "Team" }, { "id", "TEAM_" + std::to_string(index) }, { "name", "Owner team " + std::to_string(index) }, { "slug", "team-" + std::to_string(index) }, { "organization", fixture_object{ { "login", "octo-org" } } } } } });
					}
					nodes.push_back(fixture_object{ { "id", "RR_user" }, { "asCodeOwner", false }, { "requestedReviewer", fixture_object{ { "__typename", "User" }, { "id", "U_requested" }, { "login", "first-time-reviewer" } } } });
					nodes.push_back(fixture_object{ { "id", "RR_hidden" }, { "asCodeOwner", true }, { "requestedReviewer", nullptr } });
					pull.insert_or_assign("reviewRequests", connection(5U, nodes, false, std::optional<std::string_view>{ "END" }));
				} else if (aRequest.Wire.Operation == read_operation::list_branch_rules) {
					expect(aRequest.Wire.Path.ends_with("/rules/branches/main"), "server selects active applicable target rules; no repository ruleset enumeration");
					aResponse.Body = R"([{"type":"pull_request","ruleset_source_type":"Organization","ruleset_source":"octo-org","ruleset_id":42,"parameters":{"required_approving_review_count":2,"require_code_owner_review":true,"require_last_push_approval":true,"dismiss_stale_reviews_on_push":true,"required_review_thread_resolution":true}},{"type":"required_status_checks","ruleset_source_type":"Repository","ruleset_source":"octo-org/review-repository","ruleset_id":43,"parameters":{"strict_required_status_checks_policy":true,"required_status_checks":[{"context":"Branch Freshness","integration_id":null},{"context":"Missing CI","integration_id":17}]}}])";
					return;
				} else if (aRequest.Wire.Operation == read_operation::get_branch_protection) {
					aResponse.Body = R"({"url":"https://api.github.com/repos/octo-org/review-repository/branches/main/protection","required_pull_request_reviews":{"required_approving_review_count":1,"require_code_owner_reviews":false,"dismiss_stale_reviews":false,"require_last_push_approval":false},"required_status_checks":{"strict":false,"checks":[{"context":"native-ci","app_id":null}]},"required_conversation_resolution":{"enabled":true}})";
					return;
				} else {
					return;
				}
				aResponse.Body = fixture_json(fixture_object{ { "data", fixture_object{ { "repository", fixture_object{ { "id", "R_repository" }, { "pullRequest", pull } } } } } });
			};
			const auto result = perform_paged_github_read(transport, enterprise ? enterprise_profile() : profile(), provider_request(enterprise ? application::github_server_contract::ghes_3_18_2022_11_28 : application::github_server_contract::github_com_2022_11_28), core::provider_snapshot_id{ 55 }, { }, { }, github_provider_read_authority::authenticated, nullptr);
			expect(result && result->ReviewRequestsAvailability == availability::available && result->ReviewRequests.size() == 5, "all requested users, three named code-owner teams and hidden identity survive public provider facade on both hosts");
			expect(result->ReviewRequests.front().Organization == "octo-org" && *result->ReviewRequests.front().AsCodeOwner && result->ReviewRequests.back().Kind == application::github_requested_actor_kind::unavailable, "teams stay qualified and hidden actor remains an explicit record");
			expect(result->Merge.Assessment == application::github_merge_assessment::blocked && result->Merge.Review == application::github_review_decision::required && result->Merge.AutoMerge == false, "aggregate assessment and observed absent auto-merge are distinct");
			expect(result->BranchPolicy.Rules.size() == 2 && result->BranchPolicy.Protection.size() == 3 && result->BranchPolicy.Rules.front().ApprovalMinimum == 2 && result->BranchPolicy.Rules.back().Checks.back().AppId == 17, "inherited active requirements, configured minimum, classic protection and app-qualified missing check survive separately");
			const auto goodRewrite = transport.RewriteResponse;
			for (const auto defect : { 0, 1, 2, 3, 4 }) {
				transport.RewriteResponse = [&, defect](const github_wire_request& aRequest, wire_response& aResponse) {
					goodRewrite(aRequest, aResponse);
					if (aRequest.Wire.Operation == read_operation::list_review_requests) {
						if (defect == 0) {
							replace_raw(aResponse.Body, "\"totalCount\":5", "\"totalCount\":6");
						}
						if (defect == 1) {
							replace_raw(aResponse.Body, std::string{ head_oid }, std::string{ base_oid });
						}
						if (defect == 2) {
							aResponse.Status = 403;
							aResponse.Body   = "{}";
						}
						if (defect == 3) {
							replace_raw(aResponse.Body, "\"baseRefName\":\"main\"", "\"baseRefName\":\"foreign\"");
						}
						if (defect == 4) {
							aResponse.Body = R"({"errors":[{"type":"undefinedField","message":"unsupported field"}]})";
						}
					}
				};
				const auto refused = perform_paged_github_read(transport, enterprise ? enterprise_profile() : profile(), provider_request(enterprise ? application::github_server_contract::ghes_3_18_2022_11_28 : application::github_server_contract::github_com_2022_11_28), core::provider_snapshot_id{ 56 }, { });
				expect(refused && refused->ReviewRequests.empty() && refused->ReviewRequestsAvailability != availability::available && refused->Merge.Assessment == application::github_merge_assessment::blocked && !refused->Files.empty(), "partial, moved, denied and unsupported rosters never erase a known merge block or readable PR");
			}
		}
	}
	void test_review_request_pagination()
	{
		for (const auto repeats : { false, true }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			transport.RewriteResponse = [&](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation != read_operation::list_review_requests) {
					return;
				}
				const auto next  = aRequest.Wire.Semantic.Page == 1U || repeats;
				auto       nodes = fixture_array{ };
				for (unsigned index = 0; index < (next ? 100U : 1U); ++index) {
					const auto id = std::to_string(index + (aRequest.Wire.Semantic.Page - 1U) * 100U);
					nodes.push_back(fixture_object{ { "id", "request-" + id }, { "asCodeOwner", false }, { "requestedReviewer", fixture_object{ { "__typename", "User" }, { "id", "actor-" + id }, { "login", "user-" + id } } } });
				}
				if (aRequest.Wire.Semantic.Page == 2U) {
					expect(aRequest.Wire.Semantic.ReviewCursor == "NEXT", "exact continuation reaches compiled read");
				}
				const auto pull = fixture_object{ { "id", "PR_17" }, { "headRefOid", std::string{ head_oid } }, { "baseRefOid", std::string{ base_oid } }, { "baseRefName", "main" }, { "reviewRequests", connection(repeats ? 300U : 101U, nodes, next, std::optional<std::string_view>{ next ? "NEXT" : "END" }) } };
				aResponse.Body  = fixture_json(fixture_object{ { "data", fixture_object{ { "repository", fixture_object{ { "id", "R_repository" }, { "pullRequest", pull } } } } } });
			};
			const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { });
			expect(result && (repeats ? result->ReviewRequests.empty() && result->ReviewRequestsAvailability == application::github_observation_availability::incomplete : result->ReviewRequests.size() == 101 && result->ReviewRequestsAvailability == application::github_observation_availability::available), "bounded connection exhausts all requests; repeated cursor discards partial roster");
		}
	}

	void test_review_request_actor_fallback()
	{
		for (const auto empty : { false, true }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			transport.RewriteResponse = [empty](const github_wire_request& aRequest, wire_response& aResponse) {
				if (aRequest.Wire.Operation == read_operation::list_review_requests) {
					aResponse.Status = 403;
					aResponse.Body   = "{}";
				} else if (aRequest.Wire.Operation == read_operation::list_requested_reviewers) {
					auto users = fixture_array{ };
					if (!empty) {
						users.push_back(rest_actor("U_new", "new-reviewer"));
					}
					aResponse.Body = fixture_json(fixture_object{ { "users", users }, { "teams", fixture_array{ } } });
				}
			};
			const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { });
			expect(result && result->ReviewRequestActorsObserved && !result->ReviewRequestsStale && result->ReviewRequestsAvailability == application::github_observation_availability::permission && result->ReviewRequests.size() == (empty ? 0U : 1U), "complete REST fallback observes current actors, including zero, without claiming code-owner origin availability");
			const auto waiting = application::queue_waiting_for(*result);
			expect(waiting.Requests == (empty ? 0U : 1U) && !waiting.RequestsStale && waiting.NamesKnown && waiting.CodeOwners.empty() && waiting.Reviewers.size() == (empty ? 0U : 1U), "current fallback actors reach Home as ordinary reviewers and complete zero clears waiting requests");
		}
	}

	void test_optional_explanations_preserve_account_authority()
	{
		for (const auto operation : { read_operation::get_merge_assessment, read_operation::list_review_requests, read_operation::list_branch_rules, read_operation::get_branch_protection, read_operation::list_checks, read_operation::list_commit_statuses }) {
			auto transport            = paged_fixture_transport{ fixture_scenario::success };
			transport.RewriteResponse = [operation](const github_wire_request& aRequest, wire_response& aResponse) {if(aRequest.Wire.Operation == operation){aResponse.Status = 401;aResponse.Body = "{}";} };
			const auto result         = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { });
			expect(!result && std::get<application::github_provider_error>(result.error()).Failure == application::github_provider_failure::authentication, "optional detail cannot downgrade lost authentication into a successful snapshot");
		}
		class account_loss_transport final : public github_wire_transport_port
		{
		public:
			auto perform(const github_wire_request& aRequest, std::stop_token aStop) -> github_wire_completion override
			{
				if (aRequest.Wire.Operation == read_operation::get_merge_assessment) {
					auto error            = application::github_provider_error{ };
					error.Failure         = application::github_provider_failure::authentication;
					error.AccountMismatch = true;
					return github_wire_error{ .Correlation = aRequest.Correlation, .Error = error };
				}
				return Fixture.perform(aRequest, aStop);
			}

		private:
			paged_fixture_transport Fixture{ fixture_scenario::success };
		} transport;
		const auto result = perform_paged_github_read(transport, profile(), provider_request(), core::provider_snapshot_id{ 55 }, { });
		expect(!result && std::get<application::github_provider_error>(result.error()).AccountMismatch, "the CLI lease's account-mismatch terminal propagates through optional requirements unchanged");
	}

	/// [[BPR-1439]]. The shared tokenizer, media-type predicate and encoder
	/// every GitHub reader uses; each reader's own policy is tested with it.
	void test_shared_http_helpers()
	{
		const auto links = parse_link_header(R"(<https://a.invalid/x?page=2>; rel="next last"; title=t , <https://a.invalid/y,z>;rel=prev)");
		expect(links && links->size() == 2U && links->front().Target == "https://a.invalid/x?page=2" && links->front().Parameters.size() == 2U
				   && links->front().Parameters.front().Quoted && links->back().Target == "https://a.invalid/y,z" && !links->back().Parameters.front().Quoted,
			   "the tokenizer reads quoted and token values, other parameters and a comma inside a target");
		const auto relations = links ? link_relations(links->front()) : std::nullopt;
		expect(relations && relations->size() == 2U && relations->front() == "next" && relations->back() == "last", "a multi-relation value splits on its spaces");
		for (const auto* malformed : { "", "<>; rel=next", "<a b>; rel=next", "<a>; rel=", R"(<a>; rel="next)", "<a>; rel=next,", "<a> rel=next", R"(<a>; rel="ne\xt")" }) {
			expect(!parse_link_header(malformed), "a malformed Link value is refused");
		}
		const auto twoRel = parse_link_header(R"(<a>; rel=next; rel=prev)");
		const auto spaced = parse_link_header(R"(<a>; rel="next  last")");
		expect(twoRel && !link_relations(twoRel->front()) && spaced && !link_relations(spaced->front()), "two rel parameters or an empty relation are refused");
		expect(link_completeness(std::nullopt) == page_completeness::complete && link_completeness(std::string{ "<a>; rel=next" }) == page_completeness::has_next_page
				   && link_completeness(std::string{ R"(<a>; rel="prev first")" }) == page_completeness::complete && !link_completeness(std::string{ "<a>; rel=self" })
				   && !link_completeness(std::string{ "<a>; title=x" }),
			   "completeness needs every link to carry only pagination relations");

		for (const auto* accepted : { "application/json", "Application/JSON", " application/json ; charset=UTF-8 ", "application/json;charset=utf-8" }) {
			expect(json_media_type(accepted), "a JSON media type is accepted");
		}
		for (const auto* refused : { "application/jsonx", "application/json; charset=latin1", "application/json;", "application/json; charset=utf-8; q=1", "text/json", "" }) {
			expect(!json_media_type(refused), "anything else is refused");
		}
		expect(!json_media_type("application/json;" + std::string(300U, ' ')), "the media type is bounded");

		expect(percent_encode("octo-org:bpr/harden ~._") == "octo-org%3Abpr%2Fharden%20~._", "unreserved bytes stay and the rest are uppercase escapes");
		expect(percent_encode("bpr/harden v2", "/") == "bpr/harden%20v2", "a kept byte stays");
		expect(percent_encode(std::string{ "\xC3\x28\0", 3U }) == "%C3%28%00", "invalid UTF-8 and NUL are encoded byte for byte");
		expect(percent_encode("\xC3\xA9") == "%C3%A9", "UTF-8 is encoded byte for byte");
	}
}

auto main() -> int
{
	test_shared_http_helpers();
	test_optional_explanations_preserve_account_authority();
	test_review_request_pagination();
	test_review_request_actor_fallback();
	test_review_requirements();
	test_current_check_details();
	test_optional_checks_cannot_spend_mandatory_read_budget();
	test_raw_tour_import();
	test_ghes_actor_conflict_recreated_read();
	test_ghes_bot_actor_representation();
	test_paged_response_observation();
	test_timeline_read();
	test_timeline_atomic_failures();
	test_merge_base_read();
	test_metadata_and_viewer();
	test_glaze_viewer_numbers_and_syntax();
	test_glaze_paged_identity_and_lifetime();
	test_glaze_paged_raw_refusals();
	test_capability_shaped_query();
	test_successful_exhaustive_read();
	test_graphql_head_is_bound_to_rest_snapshot();
	test_measured_files_page_shapes();
	test_raw_structural_limits();
	test_search_result_limit_preserves_exact_pull_request();
	test_limited_search_still_validates_received_records();
	test_search_form_links();
	test_graphql_error_envelopes();
	test_raw_status_classification();
	test_collection_count_contract();
	test_atomic_failures();
	test_glaze_anonymous_exact_comment_linkage();
	test_anonymous_review_comments_group_by_linkage();
	test_anonymous_review_comments_exhaust_their_pages();
	test_anonymous_observed_empty_is_not_unavailable();
	test_anonymous_review_comment_linkage_violations_are_refused();
	test_anonymous_review_comment_anchor_is_read_not_defaulted();
	test_anonymous_review_comments_absent_is_not_disclosed();
	test_anonymous_review_comments_rate_limit_is_typed();
	test_anonymous_review_comments_oversized_body_is_refused();
	test_personal_discovery_read();
	test_home_draft_metadata_is_an_optional_wire_fact();
	test_home_change_facts_are_read_with_the_pull_request();
	test_retained_home_metadata_read();
	return EXIT_SUCCESS;
}
