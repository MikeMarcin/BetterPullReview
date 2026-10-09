// [[BPR-0970]]. API normalization: every reader of a GitHub or GitHub
// Enterprise response body or header, and the policy that inspects an
// outbound GraphQL document.
//
// Input: [selector] [parameter] then the payload. The read normalizer case
// replaces one of the ten recorded github.com contract responses
// (tests/fixtures/github/contracts/v1/github-com) with the payload, so the
// other nine stay valid and the normalizer reaches the replaced one. The
// paged read case answers the read's requests in order with the payload's
// parts. Nothing here opens a socket or holds a credential.

#include "bpr/application/review_mutation_coordinator.hpp"
#include "gh_cli_response.hpp"
#include "gh_cli_transport.hpp"
#include "github_capability_probe.hpp"
#include "github_device_authorization_contract.hpp"
#include "github_graphql_operation_policy.hpp"
#include "github_host_profile.hpp"
#include "github_json_preflight.hpp"
#include "github_mutation_contract.hpp"
#include "github_paged_read.hpp"
#include "github_publication_contract.hpp"
#include "github_read_contract.hpp"
#include "github_release_update.hpp"
#include "github_wire_transport.hpp"
#include "review_comment_permalink.hpp"

#include "fuzz_support.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	// --- Recorded contract responses -------------------------------------

	constexpr std::array recorded_files{
		"viewer.json",
		"repository.json",
		"pull_request.json",
		"files.json",
		"commits.json",
		"reviews.json",
		"review_threads.json",
		"issue_comments.json",
		"checks.json",
		"search.json",
	};

	auto read_text(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream stream{ aPath, std::ios::binary };
		fuzz::require(stream.good());
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto recorded_responses() -> const std::vector<operation_response>&
	{
		static const auto responses = [] {
			static_assert(recorded_files.size() == read_operation_catalog.size());
			std::vector<operation_response> result;
			result.reserve(recorded_files.size());
			const auto root = std::filesystem::path{ BPR_GITHUB_CONTRACT_FIXTURE_ROOT } / "github-com" / "responses";
			for (std::size_t index = 0U; index < recorded_files.size(); ++index) {
				result.push_back({
					.Operation = read_operation_catalog[index],
					.Response  = {
						.Status       = 200,
						.Headers      = { { .Name = "Content-Type", .Value = "application/json" } },
						.Body         = read_text(root / recorded_files[index]),
						.Completeness = page_completeness::complete,
					},
				});
			}
			return result;
		}();
		return responses;
	}

	auto capabilities() -> core::github_capabilities
	{
		return {
			.ReviewThreads       = core::github_capability_state::supported,
			.ViewedFileMutations = core::github_capability_state::unsupported,
			.ResolveThread       = core::github_capability_state::unsupported,
			.UnresolveThread     = core::github_capability_state::unsupported,
		};
	}

	auto rate_limits() -> application::github_rate_limit_state
	{
		const auto record = [](const application::github_rate_limit_resource aResource, const std::uint32_t aLimit) {
			return application::github_rate_limit_record{ .Resource = aResource, .Limit = aLimit, .Remaining = aLimit - 1U, .ResetUnixSeconds = 2'000'000'000 };
		};
		return {
			.Core    = record(application::github_rate_limit_resource::core, 5'000U),
			.Search  = record(application::github_rate_limit_resource::search, 30U),
			.Graphql = record(application::github_rate_limit_resource::graphql, 5'000U),
		};
	}

	auto normalization_context() -> github_read_normalization_context
	{
		return {
			.Server         = server_contract::github_com,
			.SessionRequest = {
				.Request            = core::request_id{ 1 },
				.ApplicationSession = core::application_session_id{ 7 },
				.Profile            = core::host_profile_id{ 11 },
				.IdentityGeneration = core::github_identity_generation{ 3 },
			},
			.PullRequestRequest = {
				.Request             = core::request_id{ 2 },
				.ApplicationSession  = core::application_session_id{ 7 },
				.Profile             = core::host_profile_id{ 11 },
				.Host                = core::git_host_id{ 22 },
				.Account             = core::github_account_id{ 1001 },
				.Repository          = { .Owner = "octo-org", .Name = "review-repository" },
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
			.RateLimits         = rate_limits(),
			.Limits             = { },
			.SearchTerms        = "is:open",
		};
	}

	auto read_normalizer(const std::string_view aBody, const std::uint8_t aParameter) -> void
	{
		auto       responses              = recorded_responses();
		const auto replaced               = aParameter % responses.size();
		responses[replaced].Response.Body = std::string{ aBody };
		if ((aParameter & 0x80U) != 0U) {
			responses[replaced].Response.Completeness = page_completeness::has_next_page;
		}
		const auto context    = normalization_context();
		const auto normalized = normalize_github_read(context, responses);
		const auto replay     = normalize_github_read(context, responses);
		fuzz::require(normalized.has_value() == replay.has_value());
		if (normalized) {
			// An accepted read is the read the request asked for.
			fuzz::require(*normalized == *replay);
			fuzz::require(normalized->PullRequest.Repository.Repository == context.Repository);
			fuzz::require(normalized->PullRequest.PullRequest.PullRequest == context.PullRequest);
			fuzz::require(normalized->PullRequest.Snapshot == context.Snapshot);
		}
	}

	// --- Headers, error bodies and the viewer document --------------------

	// Payload lines before the first blank line are `Name: value` headers; the
	// rest is the body. A payload that opens with `{` or `[` has no headers.
	auto wire_of(const std::string_view aPayload, const int aStatus) -> wire_response
	{
		wire_response response{ .Status = aStatus, .Completeness = page_completeness::complete };
		auto          remaining = aPayload;
		// A payload that opens with a JSON document is all body.
		while (!remaining.empty() && remaining.front() != '{' && remaining.front() != '[') {
			const auto end  = remaining.find('\n');
			const auto line = remaining.substr(0U, end);
			remaining       = end == std::string_view::npos ? std::string_view{ } : remaining.substr(end + 1U);
			if (line.empty()) {
				break;
			}
			const auto colon = line.find(':');
			auto       value = colon == std::string_view::npos ? std::string_view{ } : line.substr(colon + 1U);
			if (value.starts_with(' ')) {
				value.remove_prefix(1U);
			}
			response.Headers.push_back({ .Name = std::string{ line.substr(0U, colon) }, .Value = std::string{ value } });
			if (response.Headers.size() == 32U) {
				break;
			}
		}
		response.Body = std::string{ remaining };
		return response;
	}

	auto status_of(const std::uint8_t aParameter) -> int
	{
		constexpr std::array statuses{ 200, 201, 204, 304, 400, 401, 403, 404, 408, 422, 429, 500, 502, 503, 504, 302 };
		return statuses[aParameter % statuses.size()];
	}

	auto metadata(const std::string_view aPayload, const std::uint8_t aParameter) -> void
	{
		const auto response = wire_of(aPayload, status_of(aParameter));
		if (const auto parsed = parse_github_response_metadata(response)) {
			if (parsed->RateLimit) {
				fuzz::require(parsed->RateLimit->Remaining <= parsed->RateLimit->Limit);
			}
		}
		static_cast<void>(github_error_body_is_secondary_rate_limit(response.Body, 4096U));
		static_cast<void>(github_graphql_error_body_is_rate_limited(response.Body, 4096U));
		static_cast<void>(normalize_observed_github_viewer(response, (aParameter & 0x10U) != 0U ? server_contract::ghes_3_18 : server_contract::github_com));
	}

	// --- Device authorization, release, gh, permalinks ---------------------

	auto device(const std::string_view aPayload, const std::uint8_t aParameter) -> void
	{
		const auto response = wire_of(aPayload, status_of(aParameter));
		static_cast<void>(parse_device_code_response(response));
		static_cast<void>(parse_device_token_response(response));
		static_cast<void>(parse_device_refresh_response(response));
	}

	auto mutation_target() -> application::review_mutation_target_binding;

	auto other_readers(const std::string_view aPayload, const std::uint8_t aParameter) -> void
	{
		switch (aParameter % 5U) {
		case 0U:
			static_cast<void>(parse_release_update_metadata(aPayload));
			break;
		case 4U:
			// What `gh api --include` printed. An accepted reply keeps only the
			// headers provider metadata reads, and those parse on.
			if (const auto reply = parse_gh_cli_response(aPayload)) {
				fuzz::require(reply->Status >= 200 && reply->Status <= 599);
				for (const auto& header : reply->Headers) {
					fuzz::require(selected_response_header(lowercase_header(header.Name)));
				}
				static_cast<void>(parse_github_response_metadata(*reply));
			}
			break;
		case 1U:
			if (const auto accounts = parse_gh_cli_auth_status(aPayload)) {
				static_cast<void>(supported_gh_cli_auth_status_version(aPayload));
			}
			break;
		case 2U: {
			const tour_permalink_scope scope{ .Hostname = "github.com", .Repository = { .Owner = "octo-org", .Name = "review-repository" } };
			if (const auto destination = parse_tour_permalink(scope, aPayload)) {
				// What was read compiles back to an address that reads the same.
				if (const auto compiled = compile_tour_permalink(scope, *destination)) {
					fuzz::require(parse_tour_permalink(scope, *compiled) == destination);
				}
			}
			break;
		}
		default: {
			const application::review_permalink_source source{
				.Target     = mutation_target(),
				.Endpoint   = std::nullopt,
				.Comparison = { },
				.LineCount  = 64U,
			};
			static_cast<void>(parse_review_permalink_body(source, aPayload));
			const std::array<std::byte, 4> credential{ std::byte{ 't' }, std::byte{ 'o' }, std::byte{ 'k' }, std::byte{ 'n' } };
			static_cast<void>(sanitize_capability_error(aPayload, credential));
			break;
		}
		}
	}

	// --- Publication and review mutation responses -------------------------

	auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		std::fill_n(bytes.begin(), 20U, aSeed);
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	auto mutation_target() -> application::review_mutation_target_binding
	{
		return {
			.Semantic = {
				.Host         = core::git_host_id{ 11U },
				.Repository   = core::repository_id{ 12U },
				.PullRequest  = core::pull_request_id{ 13U },
				.ExpectedHead = commit(0x11U),
			},
			.Hostname                 = application::github_canonical_hostname{ "github.com" },
			.ServerContract           = application::github_server_contract::github_com_2022_11_28,
			.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
			.Number                   = core::pull_request_number{ 1U },
			.RepositoryDatabaseId     = 1'328'277'631U,
			.RepositoryNodeId         = "R_kgDOTyvofw",
			.PullRequestDatabaseId    = 4'246'446'933U,
			.PullRequestNodeId        = "PR_kwDOTyvof879G6NV",
			.BaseRepositoryDatabaseId = 1'328'277'631U,
			.BaseOid                  = commit(0x10U),
			.BaseRef                  = "main",
			.HeadRef                  = "codex/disposable-live-review-20260810",
			.Open                     = true,
		};
	}

	auto mutation_identity() -> application::review_mutation_acting_identity
	{
		return { .Account = core::github_account_id{ 21U }, .DatabaseId = 22U, .NodeId = "MDQ6VXNlcjIy", .Login = "fixture-reviewer" };
	}

	auto comment_request(const bool aReply) -> application::review_mutation_request
	{
		auto proposal = core::review_publication_proposal{
			.Draft       = core::review_draft_id{ 31U },
			.Anchor      = { .Comparison = fuzz::opaque<core::file_comparison_digest>(0x22U), .Side = core::diff_side::right, .Line = 12U },
			.Body        = "Please rename this.",
			.Disposition = core::review_disposition::comment,
		};
		if (aReply) {
			proposal.ReplyToThread = "PRRT_fixture_1";
		}
		return {
			.Proposal       = proposal,
			.Target         = mutation_target().Semantic,
			.ActingIdentity = mutation_identity().Account,
			.IdempotencyKey = application::idempotency_key_for(proposal, mutation_target().Semantic),
			.LiveBinding    = application::review_mutation_live_binding{
				.Target         = mutation_target(),
				.ActingIdentity = mutation_identity(),
				.Anchor         = { .Semantic = proposal.Anchor, .Path = "fixture/review.txt", .Commit = mutation_target().Semantic.ExpectedHead },
			},
		};
	}

	auto mutation(const std::string_view aPayload, const std::uint8_t aParameter) -> void
	{
		const review_mutation_wire_response                 response{ .Status = (aParameter & 0x80U) != 0U ? 201 : 200, .Headers = { }, .Body = std::string{ aPayload } };
		const review_submission_request                     submission{ .Target = mutation_target(), .ActingIdentity = mutation_identity(), .Disposition = core::review_disposition::comment, .Body = "Review summary." };
		const auto                                          reply = comment_request(true);
		const application::review_thread_resolution_request resolution{
			.Target         = mutation_target().Semantic,
			.ActingIdentity = mutation_identity().Account,
			.Thread         = "PRRT_fixture_1",
			.Resolve        = (aParameter & 0x40U) != 0U,
			.LiveBinding    = reply.LiveBinding,
		};
		switch (aParameter % 10U) {
		case 0U:
			static_cast<void>(parse_target_observation(response, mutation_target()));
			break;
		case 1U:
			static_cast<void>(parse_viewer_observation(response, mutation_identity()));
			break;
		case 2U:
			static_cast<void>(parse_review_comment_response(response, comment_request((aParameter & 0x40U) != 0U)));
			break;
		case 3U:
			static_cast<void>(parse_review_comment_observations(response, comment_request((aParameter & 0x40U) != 0U)));
			break;
		case 4U:
			static_cast<void>(parse_review_submission_response(response, submission));
			break;
		case 5U:
			static_cast<void>(parse_review_submission_observations(response, submission));
			break;
		case 6U:
			static_cast<void>(parse_review_submission_recovery_target(response, mutation_target()));
			break;
		case 7U:
			static_cast<void>(parse_thread_target_observation(response, resolution));
			break;
		case 8U:
			static_cast<void>(parse_thread_resolution_response(response, resolution));
			break;
		default: {
			// A request the compiler admits: a review or comment number only
			// where the operation names one, and a body only where it sends one.
			// Creating a pending review needs its lines and is left out.
			using operation = application::staged_review_operation;
			constexpr std::array ops{ operation::list_reviews, operation::read_review, operation::list_comments, operation::update_comment,
									  operation::update_summary, operation::submit_comment, operation::discard_pending, operation::update_description,
									  operation::list_issue_comments, operation::create_issue_comment, operation::update_issue_comment, operation::update_pending_comment };
			const auto           chosen = ops[(aParameter >> 4U) % ops.size()];
			const auto           review = chosen == operation::read_review || chosen == operation::list_comments || chosen == operation::update_summary
				|| chosen == operation::submit_comment || chosen == operation::discard_pending || chosen == operation::update_pending_comment;
			const auto comment = chosen == operation::update_comment || chosen == operation::update_issue_comment || chosen == operation::update_pending_comment;
			const auto reads   = chosen == operation::list_reviews || chosen == operation::read_review || chosen == operation::list_comments
				|| chosen == operation::list_issue_comments || chosen == operation::discard_pending;
			const application::staged_review_request staged{
				.Binding     = { .Target = mutation_target(), .ActingIdentity = mutation_identity() },
				.Operation   = chosen,
				.Review      = application::review_database_id{ review ? 45U : 0U },
				.Comment     = comment ? 44U : 0U,
				.CommentNode = chosen == operation::update_pending_comment ? std::string{ "PRRC_fuzz_44" } : std::string{ },
				.Page        = 1U,
				.Body        = reads ? std::string{ } : std::string{ "Review summary." },
				.Lines       = { },
			};
			fuzz::require(compile_staged_review_request(staged).has_value());
			static_cast<void>(parse_staged_review_response(response, staged));
			break;
		}
		}
	}

	auto publication(const std::string_view aPayload, const std::uint8_t aParameter) -> void
	{
		const publication_wire_response              response{ .Status = (aParameter & 0x80U) != 0U ? 201 : 200, .Headers = { }, .Body = std::string{ aPayload } };
		const application::github_publication_target target{
			.Host       = "github.com",
			.Account    = core::github_account_id{ 21U },
			.Login      = "fixture-reviewer",
			.Owner      = "MikeMarcin",
			.Repository = "BPR-Test",
		};
		const auto                     format = core::git_hash_algorithm::sha1;
		const auto                     head   = commit(0x11U);
		const auto                     tree   = *core::parse_git_tree_oid("3333333333333333333333333333333333333333");
		const auto                     blob   = core::parse_git_entry_oid("4444444444444444444444444444444444444444");
		const core::git_commit_content content{
			.Tree      = tree,
			.Parents   = { commit(0x10U) },
			.Author    = { .Name = "Author", .Email = "author@example.invalid", .Seconds = 1'700'000'000, .OffsetMinutes = 60 },
			.Committer = { .Name = "Author", .Email = "author@example.invalid", .Seconds = 1'700'000'000, .OffsetMinutes = 60 },
			.Message   = "Fix the retry loop\n",
		};
		switch (aParameter % 10U) {
		case 0U:
			static_cast<void>(parse_repository(response, target));
			break;
		case 1U:
			static_cast<void>(parse_branch(response, "main", format));
			break;
		case 2U:
			static_cast<void>(parse_commit(response, head));
			break;
		case 3U:
			static_cast<void>(parse_tree_presence(response, tree));
			break;
		case 4U:
			if (blob) {
				static_cast<void>(parse_blob_presence(response, *blob));
			}
			break;
		case 5U:
			static_cast<void>(parse_created_sha(response, format));
			break;
		case 6U:
			static_cast<void>(parse_created_commit(response, content, head));
			break;
		case 7U:
			static_cast<void>(parse_created_branch(response, "bpr/fix", head));
			break;
		case 8U:
			static_cast<void>(parse_pull_request(response, format));
			break;
		default:
			static_cast<void>(parse_pull_requests(response, format));
			break;
		}
	}

	// --- Capability probe ----------------------------------------------------

	auto capability(const std::string_view aPayload, const std::uint8_t aParameter) -> void
	{
		// Leading `Name: value` lines are the reply's rate-limit headers, as
		// the transport hands them over; the rest is the body.
		const auto                wire = wire_of(aPayload, status_of(aParameter));
		capability_probe_response response{
			.Status        = wire.Status,
			.Authenticated = (aParameter & 0x40U) == 0U,
			.ContentType   = "application/json; charset=utf-8",
			.Body          = wire.Body,
		};
		for (const auto& header : wire.Headers) {
			if (response.RateLimitHeaderCount == response.RateLimitHeaders.size()) {
				break;
			}
			response.RateLimitHeaders[response.RateLimitHeaderCount++] = { .Name = header.Name, .Value = header.Value };
		}
		const auto observation = normalize_capability_probe(response);
		fuzz::require(!observation.Complete || valid(observation));
		capability_probe_batch batch;
		if (batch.append(response, static_cast<std::uint8_t>(aParameter % 4U))) {
			static_cast<void>(batch.finish());
		}
	}

	// --- Paged read ----------------------------------------------------------

	auto host_profile() -> validated_github_host_profile
	{
		static const auto profile = [] {
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
			fuzz::require(result.has_value());
			return *result;
		}();
		return profile;
	}

	class fuzz_account_transport final : public application::github_account_transport_lease
	{
	};

	auto provider_request() -> application::github_pull_request_provider_request
	{
		const application::github_capability_authority_key authority{
			.Profile               = core::host_profile_id{ 11U },
			.ProfileRevision       = application::github_profile_revision{ 3U },
			.Host                  = core::git_host_id{ 22U },
			.Account               = core::github_account_id{ 1001U },
			.ServerContract        = application::github_server_contract::github_com_2022_11_28,
			.ProbeContractRevision = application::github_probe_contract_revision{ 1U },
		};
		std::array<application::github_capability_fact, application::github_capability_fact_count> aggregate{ };
		for (std::size_t index = 0U; index < aggregate.size(); ++index) {
			aggregate[index] = {
				.Capability = application::canonical_github_capabilities[index],
				.State      = index == 0U ? core::github_capability_state::supported : core::github_capability_state::unsupported,
				.Source     = application::github_capability_evidence_source::observed_contract,
			};
		}
		std::array<application::github_schema_capability_fact, application::github_schema_capability_fact_count> schema{ };
		for (std::size_t index = 0U; index < schema.size(); ++index) {
			schema[index] = {
				.Capability = application::canonical_github_schema_capabilities[index],
				.State      = index == 0U || index == 6U ? core::github_capability_state::supported : core::github_capability_state::unsupported,
				.Source     = application::github_capability_evidence_source::observed_contract,
			};
		}
		return {
			.Context = {
				.Request             = core::request_id{ 41U },
				.ApplicationSession  = core::application_session_id{ 7U },
				.Profile             = core::host_profile_id{ 11U },
				.Host                = core::git_host_id{ 22U },
				.Account             = core::github_account_id{ 1001U },
				.Repository          = { .Owner = "octo-org", .Name = "review-repository" },
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
				.Hostname              = application::github_canonical_hostname{ "github.com" },
				.ServerContract        = application::github_server_contract::github_com_2022_11_28,
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
				.Authority     = authority,
				.Facts         = aggregate,
				.SchemaFacts   = schema,
				.Capabilities  = capabilities(),
				.ReadShape     = { .ReviewThreads = true, .ViewedFileState = false, .ResolvePermission = false, .UnresolvePermission = true },
				.Revision      = core::capability_revision{ 44U },
				.ObservedAt    = application::github_freshness_epoch{ 100U },
				.ExpiresBefore = application::github_freshness_epoch{ 400U },
			},
			.AccountTransport = std::make_shared<fuzz_account_transport>(),
			.Attempt          = application::github_provider_attempt{ 1U },
		};
	}

	// The request a signed-out profile presents: public preview, no account,
	// an anonymous viewer, every capability unsupported and never probed.
	auto anonymous_request() -> application::github_pull_request_provider_request
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
		return request;
	}

	// Answers request n with part n of the payload, under ordinary REST
	// headers, and refuses once the parts run out. A part may open with
	// `Name: value` lines, which set headers; the pseudo-header
	// `Completeness: next` marks a page GitHub says continues.
	class part_transport final : public github_wire_transport_port
	{
	public:
		explicit part_transport(const std::string_view aPayload)
			: Remaining{ aPayload }
		{
		}

		[[nodiscard]] auto perform(const github_wire_request& aRequest, std::stop_token) -> github_wire_completion override
		{
			if (Remaining.empty() || ++Requests > 64U) {
				return github_wire_error{
					.Correlation = aRequest.Correlation,
					.Error       = { .Failure = application::github_provider_failure::network, .RetryAfterSeconds = std::nullopt, .RateLimits = { }, .RateLimitCause = std::nullopt },
				};
			}
			const auto at   = Remaining.find(fuzz::part_separator);
			auto       body = Remaining.substr(0U, at);
			Remaining       = at == std::string_view::npos ? std::string_view{ } : Remaining.substr(at + fuzz::part_separator.size());
			wire_response response{
				.Status  = 200,
				.Headers = {
					{ .Name = "Content-Type", .Value = "application/json; charset=utf-8" },
					{ .Name = "X-RateLimit-Limit", .Value = "5000" },
					{ .Name = "X-RateLimit-Remaining", .Value = "4999" },
					{ .Name = "X-RateLimit-Reset", .Value = "2000000000" },
					// The resource a real server names for the operation.
					{ .Name = "X-RateLimit-Resource", .Value = aRequest.Wire.Operation == read_operation::list_review_threads ? "graphql" : aRequest.Wire.Operation == read_operation::search_pull_requests || aRequest.Wire.Operation == read_operation::search_personal_pull_requests ? "search"
																																																																						: "core" },
				},
				.Body         = { },
				.Completeness = page_completeness::complete,
			};
			while (!body.empty() && body.front() != '{' && body.front() != '[') {
				const auto end   = body.find('\n');
				const auto line  = body.substr(0U, end);
				const auto colon = line.find(": ");
				if (colon == std::string_view::npos || end == std::string_view::npos) {
					break;
				}
				if (line.substr(0U, colon) == "Completeness") {
					response.Completeness = line.substr(colon + 2U) == "next" ? page_completeness::has_next_page : page_completeness::unknown;
				} else {
					// A named header replaces the default of the same name.
					const auto name     = std::string{ line.substr(0U, colon) };
					const auto existing = std::ranges::find(response.Headers, name, &http_header::Name);
					if (existing != response.Headers.end()) {
						existing->Value = std::string{ line.substr(colon + 2U) };
					} else {
						response.Headers.push_back({ .Name = name, .Value = std::string{ line.substr(colon + 2U) } });
					}
				}
				body.remove_prefix(end + 1U);
			}
			response.Body = std::string{ body };
			return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(response) };
		}

	private:
		std::string_view Remaining;
		std::size_t      Requests = 0U;
	};

	auto paged_read(const std::string_view aPayload, const std::uint8_t aParameter) -> void
	{
		part_transport transport{ aPayload };
		const auto     request = provider_request();
		switch (aParameter % 3U) {
		case 0U: {
			const auto anonymous = (aParameter & 0x80U) != 0U;
			const auto authority = anonymous ? github_provider_read_authority::anonymous_public : github_provider_read_authority::authenticated;
			const auto result    = perform_paged_github_read(transport, host_profile(), anonymous ? anonymous_request() : request, core::provider_snapshot_id{ 55U }, std::stop_token{ }, github_paged_read_limits{ }, authority);
			if (result) {
				fuzz::require(result->Snapshot == core::provider_snapshot_id{ 55U });
			}
			break;
		}
		case 1U:
			static_cast<void>(perform_paged_github_timeline_read(transport, host_profile(), request, core::repository_id{ 2001U }, std::stop_token{ }));
			break;
		default: {
			const application::personal_discovery_request discovery{
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
				.ProfileAuthority      = request.ProfileAuthority,
				.Viewer                = request.Viewer,
				.CapabilityEvidence    = request.CapabilityEvidence,
				.AccountTransport      = request.AccountTransport,
				.Reason                = (aParameter & 0x80U) == 0U ? application::personal_discovery_reason::authored : application::personal_discovery_reason::requested,
				.AuthenticationSession = std::nullopt,
			};
			static_cast<void>(perform_personal_discovery_read(transport, host_profile(), discovery, std::stop_token{ }));
			break;
		}
		}
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	fuzz::fuzz_input input{ aData, aSize };
	const auto       selector  = input.byte();
	const auto       parameter = input.byte();
	if (input.rest().size() > 262144U) {
		return 0;
	}
	const auto text = input.text();
	switch (selector % 10U) {
	case 0U: {
		const json_preflight_limits limits{ .Depth = 1U + (parameter % 32U), .Elements = 128U, .Values = 4096U };
		const auto                  first = json_preflight{ text, limits }.run();
		fuzz::require(first == json_preflight{ text, limits }.run());
		break;
	}
	case 1U: {
		const auto result = inspect_graphql_operation_policy(text);
		fuzz::require(result == inspect_graphql_operation_policy(text));
		if (text.size() > maximum_graphql_operation_document_bytes) {
			fuzz::require(result == graphql_operation_policy_result::response_limit);
		} else if (result == graphql_operation_policy_result::query_only && text.size() < maximum_graphql_operation_document_bytes - 32U) {
			// A query-only document never stays query-only with a mutation added.
			fuzz::require(inspect_graphql_operation_policy(std::string{ text } + "\nmutation Evil { change }\n") != graphql_operation_policy_result::query_only);
		}
		break;
	}
	case 2U:
		read_normalizer(text, parameter);
		break;
	case 3U:
		metadata(text, parameter);
		break;
	case 4U:
		device(text, parameter);
		break;
	case 5U:
		other_readers(text, parameter);
		break;
	case 6U:
		publication(text, parameter);
		break;
	case 7U:
		mutation(text, parameter);
		break;
	case 8U:
		capability(text, parameter);
		break;
	default:
		paged_read(text, parameter);
		break;
	}
	return 0;
}
