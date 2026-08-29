#include "github_mutation_contract.hpp"

#include "bpr/application/review_mutation_coordinator.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github::detail;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		std::fill_n(bytes.begin(), 20U, aSeed);
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto target() -> application::review_mutation_target_binding
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

	[[nodiscard]] auto identity() -> application::review_mutation_acting_identity
	{
		return {
			.Account    = core::github_account_id{ 21U },
			.DatabaseId = 22U,
			.NodeId     = "MDQ6VXNlcjIy",
			.Login      = "fixture-reviewer",
		};
	}

	[[nodiscard]] auto request() -> application::review_mutation_request
	{
		const auto proposal = core::review_publication_proposal{
			.Draft  = core::review_draft_id{ 31U },
			.Anchor = {
				.Comparison = opaque<core::file_comparison_digest>(0x22U),
				.Side       = core::diff_side::right,
				.Line       = 12U,
			},
			.Body        = "Please rename this.",
			.Disposition = core::review_disposition::comment,
		};
		auto value = application::review_mutation_request{
			.Proposal       = proposal,
			.Target         = target().Semantic,
			.ActingIdentity = identity().Account,
			.IdempotencyKey = application::idempotency_key_for(proposal, target().Semantic),
			.LiveBinding    = application::review_mutation_live_binding{
				.Target         = target(),
				.ActingIdentity = identity(),
				.Anchor         = {
					.Semantic = proposal.Anchor,
					.Path     = "fixture/review.txt",
					.Commit   = target().Semantic.ExpectedHead,
				},
			},
		};
		return value;
	}

	[[nodiscard]] auto request_at_line(const std::uint32_t aLine)
		-> application::review_mutation_request
	{
		auto value                              = request();
		value.Proposal.Anchor.Line              = aLine;
		value.LiveBinding->Anchor.Semantic.Line = aLine;
		value.IdempotencyKey                    = application::idempotency_key_for(
			value.Proposal, value.Target);
		return value;
	}

	[[nodiscard]] auto target_body(const core::git_commit_oid& aHead) -> std::string
	{
		return std::string{ "{\"id\":4246446933,\"node_id\":\"PR_kwDOTyvof879G6NV\","
							"\"number\":1,\"state\":\"open\",\"head\":{\"sha\":\"" }
		+ std::string{ core::canonical_hex(aHead).view() }
		+ "\",\"ref\":\"codex/disposable-live-review-20260810\"},"
		  "\"base\":{\"sha\":\""
			+ std::string{ core::canonical_hex(target().BaseOid).view() }
		+ "\",\"ref\":\"main\",\"repo\":{\"id\":1328277631,"
		  "\"node_id\":\"R_kgDOTyvofw\",\"full_name\":\"MikeMarcin/BPR-Test\"}}}";
	}

	[[nodiscard]] auto viewer_body() -> std::string
	{
		return "{\"login\":\"fixture-reviewer\",\"id\":22,\"node_id\":\"MDQ6VXNlcjIy\"}";
	}

	[[nodiscard]] auto comment_body(const std::uint64_t aGithubLine = 13U)
		-> std::string
	{
		return std::string{ "{\"id\":44,\"node_id\":\"PRRC_fixture_44\","
							"\"path\":\"fixture/review.txt\",\"commit_id\":\"" }
		+ std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() }
		+ "\",\"side\":\"RIGHT\",\"line\":" + std::to_string(aGithubLine) + ","
																			"\"body\":\"Please rename this.\",\"user\":{"
																			"\"login\":\"fixture-reviewer\",\"id\":22,"
																			"\"node_id\":\"MDQ6VXNlcjIy\"}}";
	}

	void test_exact_wire_compilation_and_tamper_rejection()
	{
		const auto observed = compile_target_observation_request(target());
		expect(observed.has_value(), "target request compiles");
		expect(observed->Method == http_method::get
				   && observed->Path == "/repos/MikeMarcin/BPR-Test/pulls/1"
				   && observed->Headers.size() == 3U && observed->Body.empty(),
			   "target request has exact method path headers and body");
		expect(validate_outbound_review_mutation_request(*observed).has_value(),
			   "target request revalidates canonically");

		const auto viewer = compile_viewer_observation_request(identity());
		expect(viewer.has_value() && viewer->Path == "/user",
			   "viewer request is exact");
		const auto ghesTarget = compile_target_observation_request(
			target(), application::github_server_contract::ghes_3_18_2022_11_28);
		const auto ghesViewer = compile_viewer_observation_request(
			identity(), application::github_server_contract::ghes_3_18_2022_11_28);
		expect(ghesTarget.has_value()
				   && ghesTarget->Path == "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1"
				   && ghesViewer.has_value() && ghesViewer->Path == "/api/v3/user",
			   "GHES mutation observations use the exact REST root");

		const auto comment = compile_review_comment_request(request());
		expect(comment.has_value(), "comment request compiles");
		expect(comment->Method == http_method::post
				   && comment->Path == "/repos/MikeMarcin/BPR-Test/pulls/1/comments"
				   && comment->Headers.size() == 4U
				   && comment->Body
					   == "{\"body\":\"Please rename this.\","
						  "\"commit_id\":\"1111111111111111111111111111111111111111\","
						  "\"line\":13,\"path\":\"fixture/review.txt\",\"side\":\"RIGHT\"}",
			   "comment request has exact ordinary GitHub fields and bytes");
		expect(validate_outbound_review_mutation_request(*comment).has_value(),
			   "comment request revalidates canonically");
		const auto ghesComment = compile_review_comment_request(
			request(), application::github_server_contract::ghes_3_18_2022_11_28);
		expect(ghesComment.has_value()
				   && ghesComment->Path
					   == "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/comments",
			   "GHES comment uses the exact REST root");

		for (const auto tamper : { 0, 1, 2, 3 }) {
			auto changed = *comment;
			switch (tamper) {
			case 0:
				changed.Method = http_method::get;
				break;
			case 1:
				changed.Path += "/elsewhere";
				break;
			case 2:
				changed.Headers.front().Value = "text/plain";
				break;
			case 3:
				changed.Body += ' ';
				break;
			default:
				std::abort();
			}
			expect(!validate_outbound_review_mutation_request(changed).has_value(),
				   "any wire tamper is rejected");
		}

		auto invalidUtf8          = request();
		invalidUtf8.Proposal.Body = std::string{ "invalid-\xC3\x28", 10U };
		expect(!compile_review_comment_request(invalidUtf8).has_value(),
			   "invalid UTF-8 cannot be normalized into a different comment");
	}

	void test_zero_based_semantic_lines_translate_to_one_based_rest_lines()
	{
		for (const auto semanticLine : std::array<std::uint32_t, 2>{ 0U, 1U }) {
			const auto value      = request_at_line(semanticLine);
			const auto githubLine = static_cast<std::uint64_t>(semanticLine) + 1U;
			const auto compiled   = compile_review_comment_request(value);
			expect(compiled.has_value()
					   && compiled->Body.find(
							  "\"line\":" + std::to_string(githubLine) + ",")
						   != std::string::npos,
				   "zero-based semantic line compiles to the next one-based REST line");

			const auto parsed = parse_review_comment_response(
				{ .Status = 201, .Body = comment_body(githubLine) }, value);
			expect(parsed.has_value()
					   && parsed->Anchor.Semantic.Line == semanticLine,
				   "one-based REST response line maps back to the semantic line");
		}

		const auto maximumSemantic
			= request_at_line(std::numeric_limits<std::uint32_t>::max());
		const auto maximumCompiled = compile_review_comment_request(maximumSemantic);
		expect(maximumCompiled.has_value()
				   && maximumCompiled->Body.find("\"line\":4294967296,")
					   != std::string::npos,
			   "the maximum semantic line translates without overflow");
		expect(parse_review_comment_response(
				   { .Status = 201, .Body = comment_body(4'294'967'296U) },
				   maximumSemantic)
				   .has_value(),
			   "the maximum representable semantic line round-trips");
		expect(!parse_review_comment_response(
					{ .Status = 201, .Body = comment_body(4'294'967'297U) },
					maximumSemantic)
					.has_value(),
			   "a REST line beyond the semantic range is refused");
	}

	void test_exact_response_identity_and_bounds()
	{
		const auto targetResponse = review_mutation_wire_response{
			.Status = 200,
			.Body   = target_body(target().Semantic.ExpectedHead),
		};
		const auto observed = parse_target_observation(targetResponse, target());
		expect(observed.has_value() && observed->Target == target(),
			   "target response preserves complete identity");

		auto movedResponse = targetResponse;
		movedResponse.Body = target_body(commit(0x33U));
		const auto moved   = parse_target_observation(movedResponse, target());
		expect(moved.has_value()
				   && moved->Target.Semantic.ExpectedHead == commit(0x33U),
			   "a moved head is reported rather than hidden");

		auto wrongRepository = targetResponse;
		wrongRepository.Body.replace(
			wrongRepository.Body.find("1328277631"), 10U, "1328277632");
		expect(!parse_target_observation(wrongRepository, target()).has_value(),
			   "repository identity mismatch is rejected");

		const auto viewer = parse_viewer_observation(
			{ .Status = 200, .Body = viewer_body() }, identity());
		expect(viewer.has_value() && viewer->Identity == identity(),
			   "viewer identity is exact");

		const auto comment = parse_review_comment_response(
			{ .Status = 201, .Body = comment_body() }, request());
		expect(comment.has_value()
				   && comment->Receipt.ServerCommentId == "PRRC_fixture_44"
				   && !comment->Receipt.Duplicate,
			   "comment receipt and source identity are exact");

		auto wrongAuthor = comment_body();
		wrongAuthor.replace(wrongAuthor.find("fixture-reviewer"), 16U, "different-author");
		expect(!parse_review_comment_response(
					{ .Status = 201, .Body = std::move(wrongAuthor) }, request())
					.has_value(),
			   "wrong author is rejected");

		auto oversized = review_mutation_wire_response{
			.Status = 200,
			.Body   = std::string(maximum_review_mutation_response_bytes + 1U, 'x'),
		};
		expect(!parse_target_observation(oversized, target()).has_value(),
			   "oversized response is rejected");
		expect(!parse_target_observation(
					{ .Status = 200, .Body = "{" }, target())
					.has_value(),
			   "malformed JSON is rejected");
		auto duplicateKey = targetResponse;
		duplicateKey.Body.insert(1U, "\"id\":4246446933,");
		expect(!parse_target_observation(duplicateKey, target()).has_value(),
			   "duplicate JSON keys are rejected before parser normalization");
		expect(!parse_target_observation(
					{ .Status = 302, .Headers = { { .Name = "Location", .Value = "https://elsewhere.invalid" } }, .Body = "{}" },
					target())
					.has_value(),
			   "redirect response is rejected");
	}
} // namespace

auto main() -> int
{
	test_exact_wire_compilation_and_tamper_rejection();
	test_zero_based_semantic_lines_translate_to_one_based_rest_lines();
	test_exact_response_identity_and_bounds();
	std::cout << "github-mutation-contract-tests:pass\n";
	return EXIT_SUCCESS;
}
