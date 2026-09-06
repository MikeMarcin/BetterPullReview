#include "canonical_json_expectations.hpp"
#include "fixture_json.hpp"
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
	using namespace bpr::tests;
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

	[[nodiscard]] auto submission() -> review_submission_request
	{
		return { .Target = target(), .ActingIdentity = identity(), .Disposition = core::review_disposition::comment, .Body = "Review summary." };
	}

	[[nodiscard]] auto review_body() -> std::string
	{
		return R"({"id":45,"node_id":"PRR_fixture_45","user":{"login":"fixture-reviewer","id":22,"node_id":"MDQ6VXNlcjIy"},"body":"Review summary.","state":"COMMENTED","html_url":"https://github.com/MikeMarcin/BPR-Test/pull/1#pullrequestreview-45","pull_request_url":"https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1","_links":{"html":{"href":"https://github.com/MikeMarcin/BPR-Test/pull/1#pullrequestreview-45"},"pull_request":{"href":"https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1"}},"submitted_at":"2026-09-05T12:34:56Z","commit_id":"1111111111111111111111111111111111111111"})";
	}

	[[nodiscard]] auto review_response(const fixture_object& aObject)
		-> review_mutation_wire_response
	{
		return { .Status = 200, .Body = fixture_json(aObject) };
	}

	void test_whole_review_dispositions_and_literal_summary()
	{
		struct review_case
		{
			core::review_disposition Disposition;
			std::string_view         Event;
			std::string              State;
		};
		const auto cases = std::array{
			review_case{ core::review_disposition::comment, "COMMENT", "COMMENTED" },
			review_case{ core::review_disposition::approve, "APPROVE", "APPROVED" },
			review_case{ core::review_disposition::request_changes, "REQUEST_CHANGES", "CHANGES_REQUESTED" },
		};
		for (const auto& entry : cases) {
			auto input          = submission();
			input.Disposition   = entry.Disposition;
			const auto compiled = compile_review_submission_request(input);
			expect(compiled.has_value(), "each whole-review disposition compiles");
			expect(compiled->Method == http_method::post
					   && compiled->Operation == review_mutation_operation::create_review
					   && compiled->Path == "/repos/MikeMarcin/BPR-Test/pulls/1/reviews"
					   && compiled->Headers == std::vector<http_header>{ { .Name = "Accept", .Value = "application/vnd.github+json" }, { .Name = "User-Agent", .Value = "BetterPullReview/0.1" }, { .Name = "X-GitHub-Api-Version", .Value = "2022-11-28" }, { .Name = "Content-Type", .Value = "application/json" } }
					   && compiled->Body == std::string{ R"({"body":"Review summary.","commit_id":"1111111111111111111111111111111111111111","event":")" } + std::string{ entry.Event } + "\"}",
				   "ordinary review wire contains only summary, exact head and explicit event");
			expect(validate_outbound_review_mutation_request(*compiled).has_value(), "review wire revalidates");
			auto object = parse_fixture_object(review_body());
			object.insert_or_assign("state", entry.State);
			const auto parsed = parse_review_submission_response(review_response(object), input);
			expect(parsed.has_value() && parsed->ReviewDatabaseId == 45U
					   && parsed->ReviewNodeId == "PRR_fixture_45" && parsed->Target == input.Target
					   && parsed->Author == input.ActingIdentity && parsed->Body == input.Body
					   && parsed->Disposition == entry.Disposition && parsed->SubmittedAt == "2026-09-05T12:34:56Z",
				   "submitted review preserves distinct review identity and exact expected facts");

			for (const auto& body : { std::string{ }, std::string{ " \t\r\n" }, std::string{ "\xE2\x80\x83" } }) {
				input.Body = body;
				object.insert_or_assign("body", body);
				const auto allowed = entry.Disposition == core::review_disposition::approve;
				expect(compile_review_submission_request(input).has_value() == allowed
						   && parse_review_submission_response(review_response(object), input).has_value() == allowed,
					   "only approval admits an empty or whitespace-only summary");
			}
		}
		auto input = submission();
		input.Body = "Literal \"summary\"\\\n\t\xE2\x9C\x93";
		input.Body.append("\0\x01", 2U);
		const auto compiled = compile_review_submission_request(input);
		expect(compiled.has_value(), "escaped and non-ASCII summary compiles");
		auto object = parse_fixture_object(review_body());
		object.insert_or_assign("body", input.Body);
		expect(parse_fixture_object(compiled->Body).at("body").get_string() == input.Body
				   && parse_review_submission_response(review_response(object), input).has_value(),
			   "summary bytes survive JSON escaping and response parsing");
		const auto lineComment = parse_review_comment_response({ .Status = 201, .Body = comment_body() }, request());
		expect(lineComment.has_value() && lineComment->Receipt.ServerCommentId == "PRRC_fixture_44"
				   && !parse_review_submission_response({ .Status = 200, .Body = comment_body() }, submission()).has_value()
				   && !parse_review_comment_response({ .Status = 201, .Body = review_body() }, request()).has_value(),
			   "whole reviews and line-comment receipts cannot substitute for one another");
	}

	void test_whole_review_request_rejection()
	{
		using change       = void (*)(review_submission_request&);
		const auto changes = std::array<change, 22>{
			[](auto& aInput) { aInput.Disposition                  = static_cast<core::review_disposition>(255); },
			[](auto& aInput) { aInput.Target.Semantic.Host         = { }; },
			[](auto& aInput) { aInput.Target.Semantic.Repository   = { }; },
			[](auto& aInput) { aInput.Target.Semantic.PullRequest  = { }; },
			[](auto& aInput) { aInput.Target.Semantic.ExpectedHead = { }; },
			[](auto& aInput) { aInput.Target.Hostname              = application::github_canonical_hostname{ "elsewhere.invalid" }; },
			[](auto& aInput) { aInput.Target.Repository.Owner.clear(); },
			[](auto& aInput) { aInput.Target.Number               = { }; },
			[](auto& aInput) { aInput.Target.RepositoryDatabaseId = 0U; },
			[](auto& aInput) { aInput.Target.RepositoryNodeId.clear(); },
			[](auto& aInput) { aInput.Target.PullRequestDatabaseId = 0U; },
			[](auto& aInput) { aInput.Target.PullRequestNodeId.clear(); },
			[](auto& aInput) { aInput.Target.BaseRepositoryDatabaseId = 0U; },
			[](auto& aInput) { aInput.Target.BaseOid                  = { }; },
			[](auto& aInput) { aInput.Target.BaseRef.clear(); },
			[](auto& aInput) { aInput.Target.HeadRef.clear(); },
			[](auto& aInput) { aInput.Target.Open               = false; },
			[](auto& aInput) { aInput.ActingIdentity.Account    = { }; },
			[](auto& aInput) { aInput.ActingIdentity.DatabaseId = 0U; },
			[](auto& aInput) { aInput.ActingIdentity.NodeId.clear(); },
			[](auto& aInput) { aInput.ActingIdentity.Login.clear(); },
			[](auto& aInput) { aInput.Body = "\xC3\x28"; },
		};
		for (const auto changeInput : changes) {
			auto input = submission();
			changeInput(input);
			expect(!compile_review_submission_request(input).has_value(), "incomplete or invalid review authority/body is refused");
		}
		auto input = submission();
		input.Body.assign(maximum_review_mutation_body_bytes + 1U, 'x');
		expect(!compile_review_submission_request(input).has_value(), "oversized summary is refused");
		input.Body.assign(maximum_review_mutation_body_bytes / 2U, '\n');
		input.Body += 'x';
		expect(!compile_review_submission_request(input).has_value(), "encoded JSON size is bounded independently");
		expect(!compile_review_submission_request(submission(), static_cast<application::github_server_contract>(255)).has_value()
				   && !compile_review_submission_request(submission(), application::github_server_contract::ghes_3_18_2022_11_28).has_value(),
			   "unknown and mismatched server contracts are refused");

		using wire_change      = void (*)(review_mutation_wire_request&);
		const auto wireChanges = std::array<wire_change, 9>{
			[](auto& aWire) { aWire.Method = http_method::get; },
			[](auto& aWire) { aWire.Path += "/elsewhere"; },
			[](auto& aWire) { aWire.Headers.front().Value = "text/plain"; },
			[](auto& aWire) { aWire.Body += ' '; },
			[](auto& aWire) { aWire.Operation = review_mutation_operation::create_review_comment; },
			[](auto& aWire) { aWire.Operation = static_cast<review_mutation_operation>(255); },
			[](auto& aWire) { aWire.Semantic  = request(); },
			[](auto& aWire) { aWire.Semantic  = target(); },
			[](auto& aWire) { aWire.Semantic  = identity(); },
		};
		for (const auto changeWire : wireChanges) {
			auto wire = *compile_review_submission_request(submission());
			changeWire(wire);
			expect(!validate_outbound_review_mutation_request(wire).has_value(), "review wire tampering fails canonical validation");
		}
		input                       = submission();
		input.Target.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		input.Target.Hostname       = application::github_canonical_hostname{ "ghe.example.com" };
		const auto ghes             = compile_review_submission_request(input, input.Target.ServerContract);
		expect(ghes.has_value() && ghes->Path == "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews"
				   && validate_outbound_review_mutation_request(*ghes).has_value(),
			   "exact GHES authority compiles");
		auto object = parse_fixture_object(review_body());
		expect(!parse_review_submission_response(review_response(object), input).has_value(), "GitHub.com response cannot satisfy GHES authority");
		object.insert_or_assign("pull_request_url", "https://ghe.example.com/api/v3/repos/MikeMarcin/BPR-Test/pulls/1");
		expect(parse_review_submission_response(review_response(object), input).has_value(), "exact GHES response authority is accepted");
	}

	void test_review_list_baseline_and_candidates()
	{
		const auto input    = submission();
		const auto compiled = compile_review_submission_observation_request(input);
		expect(compiled && compiled->Method == http_method::get && compiled->Path == "/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100" && compiled->Body.empty() && validate_outbound_review_mutation_request(*compiled), "review list GET compiles canonically");
		auto tampered      = *compiled;
		tampered.Operation = review_mutation_operation::create_review;
		expect(!validate_outbound_review_mutation_request(tampered), "review list semantic/operation mismatch is refused");
		auto object  = parse_fixture_object(review_body());
		auto pending = object;
		pending.insert_or_assign("id", 46);
		pending.insert_or_assign("state", "PENDING");
		pending.erase("submitted_at");
		auto dismissed = object;
		dismissed.insert_or_assign("id", 47);
		dismissed.insert_or_assign("state", "DISMISSED");
		auto other = object;
		other.insert_or_assign("id", 48);
		auto actor = other.at("user").get_object();
		actor.insert_or_assign("id", 23);
		other.insert_or_assign("user", actor);
		const auto body   = "[" + review_body() + "," + review_response(pending).Body + "," + review_response(dismissed).Body + "," + review_response(other).Body + "]";
		const auto parsed = parse_review_submission_observations({ .Status = 200, .Body = body }, input);
		expect(parsed && parsed->ReviewIds.size() == 4U && parsed->ExactReviews.size() == 1U && parsed->ExactReviews.front().ReviewDatabaseId == 45U, "baseline retains all reviews but only exact submitted reviews are candidates");
		for (const auto& malformed : std::array<std::string, 4>{ "[" + review_body() + "," + review_body() + "]", "[" + review_body() + "] {}", "[{}]", "[" + std::string{ "{\"id\":45.0000000000000001," } + review_body().substr(9U) + "]" }) {
			expect(!parse_review_submission_observations({ .Status = 200, .Body = malformed }, input), "malformed, fractional or duplicate review lists fail closed");
		}
		expect(!parse_review_submission_observations({ .Status = 200, .Headers = { { .Name = "LiNk", .Value = "next" } }, .Body = body }, input), "incomplete review pagination is refused");
		auto oversized = std::string{ "[" };
		for (auto index = 0; index < 101; ++index) {
			if (index) {
				oversized += ',';
			}
			object.insert_or_assign("id", index + 1);
			oversized += review_response(object).Body;
		}
		oversized += ']';
		expect(!parse_review_submission_observations({ .Status = 200, .Body = oversized }, input), "review list capacity refuses without truncating");
	}

	void test_whole_review_response_rejection()
	{
		auto lossyInput    = submission();
		lossyInput.Body    = "?";
		auto surrogateBody = review_body();
		surrogateBody.replace(surrogateBody.find("Review summary."), std::string_view{ "Review summary." }.size(), "\\ud800");
		expect(!parse_review_submission_response({ .Status = 200, .Body = surrogateBody }, lossyInput).has_value(),
			   "an unpaired JSON surrogate cannot normalize into the expected summary");
		for (const auto* integerText : { "22", "45" }) {
			auto       fractionalId = review_body();
			const auto field        = std::string{ "\"id\":" } + integerText;
			fractionalId.replace(fractionalId.find(field), field.size(), field + ".0000000000000001");
			expect(!parse_review_submission_response({ .Status = 200, .Body = fractionalId }, submission()).has_value(),
				   "fractional provider identity cannot round into an exact integer");
		}
		const auto original = parse_fixture_object(review_body());
		const auto fields   = std::array{
			"id", "node_id", "commit_id", "body", "state", "submitted_at", "pull_request_url", "user"
		};
		for (const auto* field : fields) {
			auto object = original;
			object.erase(field);
			expect(!parse_review_submission_response(review_response(object), submission()).has_value(), "missing review field is refused");
			object.insert_or_assign(field, nullptr);
			expect(!parse_review_submission_response(review_response(object), submission()).has_value(), "null review field is refused");
		}
		const auto mismatches = std::array<std::pair<std::string, fixture_value>, 19>{
			std::pair{ "id", fixture_value(0) },
			{ "id", -1 },
			{ "id", 1.5 },
			{ "node_id", "" },
			{ "node_id", std::string(257U, 'x') },
			{ "commit_id", "2222222222222222222222222222222222222222" },
			{ "commit_id", "invalid" },
			{ "body", "different summary" },
			{ "state", "APPROVED" },
			{ "state", "PENDING" },
			{ "submitted_at", "" },
			{ "submitted_at", "not a date" },
			{ "submitted_at", "2026-02-30T12:34:56Z" },
			{ "pull_request_url", "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/2" },
			{ "pull_request_url", "https://api.github.com/repos/MikeMarcin/Other/pulls/1" },
			{ "pull_request_url", "https://elsewhere.invalid/repos/MikeMarcin/BPR-Test/pulls/1" },
			{ "pull_request_url", "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1?other" },
			{ "pull_request_url", "https://github.com/MikeMarcin/BPR-Test/pull/1" },
			{ "user", fixture_object{ { "id", 23 }, { "node_id", "MDQ6VXNlcjIy" }, { "login", "fixture-reviewer" } } },
		};
		for (const auto& [field, value] : mismatches) {
			auto object = original;
			object.insert_or_assign(field, value);
			expect(!parse_review_submission_response(review_response(object), submission()).has_value(), "mismatched or malformed review facts are refused");
		}
		for (const auto* field : { "id", "node_id", "login" }) {
			auto object = original;
			auto author = object.at("user").get_object();
			author.insert_or_assign(field, "different");
			object.insert_or_assign("user", author);
			expect(!parse_review_submission_response(review_response(object), submission()).has_value(), "every author identity field must match");
		}
		for (const auto status : { 0, 201, 202, 301, 302, 307, 400, 422, 500 }) {
			expect(!parse_review_submission_response({ .Status = status, .Body = review_body() }, submission()).has_value(), "only status 200 creates a submitted review observation");
		}
		for (const auto* header : { "Location", "location", "lOcAtIoN" }) {
			expect(!parse_review_submission_response({ .Status = 200, .Headers = { { .Name = header, .Value = "https://elsewhere.invalid" } }, .Body = review_body() }, submission()).has_value(), "redirect headers fail closed regardless of case");
		}
		for (const auto& body : std::array<std::string, 6>{ "{", "[]", "{}", "{\"id\":45," + review_body().substr(1U),
															std::string(maximum_review_mutation_response_bytes + 1U, 'x'), "{\"extra\":\"\xC3\x28\"," + review_body().substr(1U) }) {
			expect(!parse_review_submission_response({ .Status = 200, .Body = body }, submission()).has_value(), "malformed, duplicate, invalid UTF-8 and oversized responses are refused");
		}
	}

	void test_whole_review_exact_integer_ids_and_extensions()
	{
		for (const auto id : { std::uint64_t{ 9'007'199'254'740'993U }, std::numeric_limits<std::uint64_t>::max() }) {
			auto input                      = submission();
			input.ActingIdentity.DatabaseId = id;
			auto body                       = review_body();
			body.replace(body.find("\"id\":45"), 7U, "\"id\":" + std::to_string(id));
			body.replace(body.find("\"id\":22"), 7U, "\"id\":" + std::to_string(id));
			const auto parsed = parse_review_submission_response({ .Status = 200, .Body = body }, input);
			expect(parsed.has_value() && parsed->ReviewDatabaseId == id && parsed->Author.DatabaseId == id,
				   "provider identities retain exact uint64 values above double precision");
			input.ActingIdentity.DatabaseId -= 1U;
			expect(!parse_review_submission_response({ .Status = 200, .Body = body }, input).has_value(),
				   "adjacent large author identities cannot round into the same account");
		}
		for (const auto* field : { "\"id\":45", "\"id\":22" }) {
			auto body = review_body();
			body.replace(body.find(field), std::string_view{ field }.size(), "\"id\":18446744073709551616");
			expect(!parse_review_submission_response({ .Status = 200, .Body = body }, submission()).has_value(),
				   "overflowing uint64 identities fail closed");
		}
		const auto extended = "{\"extension\":{\"ratio\":22.0000000000000001,\"values\":[true,null,\"ordinary\"]},"
			+ review_body().substr(1U);
		expect(parse_review_submission_response({ .Status = 200, .Body = extended }, submission()).has_value(),
			   "unknown valid ordinary GitHub fields can include decimal values");
		for (const auto* extension : { "\"\\ud800\"", "\"\\udc00\"", "[true,]", "{\"ratio\":1e}" }) {
			const auto body = std::string{ "{\"extension\":" } + extension + "," + review_body().substr(1U);
			expect(!parse_review_submission_response({ .Status = 200, .Body = body }, submission()).has_value(),
				   "skipped extension values still require valid JSON and Unicode");
		}
		expect(!parse_review_submission_response({ .Status = 200, .Body = review_body() + " {}" }, submission()).has_value(),
			   "full response consumption rejects a trailing document");
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

	[[nodiscard]] auto replace_wire_field(std::string aBody, const std::string_view aFrom, const std::string_view aTo) -> std::string
	{
		const auto position = aBody.find(aFrom);
		expect(position != std::string::npos, "raw JSON fixture contains selected field");
		aBody.replace(position, aFrom.size(), aTo);
		return aBody;
	}

	void test_legacy_exact_integer_json()
	{
		for (const auto* invalid : { "22.0000000000000001", "22.0", "22e-1", "22e1000", "-22", "0", "18446744073709551616", "null", "true", "\"22\"" }) {
			const auto field  = std::string{ "\"id\":" } + invalid;
			const auto viewer = replace_wire_field(viewer_body(), "\"id\":22", field);
			expect(!parse_viewer_observation({ .Status = 200, .Body = viewer }, identity()), std::string{ "viewer identities require exact positive integer tokens: " } + invalid);
			const auto comment = replace_wire_field(comment_body(), "\"id\":22", field);
			expect(!parse_review_comment_response({ .Status = 201, .Body = comment }, request()), "comment author cannot normalize into an integer");
			expect(!parse_review_comment_observations({ .Status = 200, .Body = "[" + comment + "]" }, request()), "reconciliation cannot normalize fractional author through a DOM round trip");
		}
		const auto exactExponent = replace_wire_field(viewer_body(), "\"id\":22", "\"id\":22e0");
		expect(parse_viewer_observation({ .Status = 200, .Body = exactExponent }, identity()).has_value(), "exact nonnegative exponent remains an integer identity without floating-point conversion");
		for (const auto* field : { "\"id\":4246446933", "\"id\":1328277631", "\"number\":1" }) {
			const auto body = replace_wire_field(target_body(target().Semantic.ExpectedHead), field, std::string{ field } + ".0000000000000001");
			expect(!parse_target_observation({ .Status = 200, .Body = body }, target()), "target identities and number reject fractional raw tokens");
		}
		for (const auto* field : { "\"id\":44", "\"line\":13" }) {
			const auto body = replace_wire_field(comment_body(), field, std::string{ field } + ".0000000000000001");
			expect(!parse_review_comment_response({ .Status = 201, .Body = body }, request()), "comment identity and line reject fractional raw tokens");
		}
		for (const auto id : { std::uint64_t{ 9'007'199'254'740'993U }, std::numeric_limits<std::uint64_t>::max() }) {
			auto actor        = identity();
			actor.DatabaseId  = id;
			const auto field  = "\"id\":" + std::to_string(id);
			const auto viewer = replace_wire_field(viewer_body(), "\"id\":22", field);
			expect(parse_viewer_observation({ .Status = 200, .Body = viewer }, actor).has_value(), "viewer retains exact uint64 identity");
			auto input                        = request();
			input.LiveBinding->ActingIdentity = actor;
			const auto comment                = replace_wire_field(replace_wire_field(comment_body(), "\"id\":44", field), "\"id\":22", field);
			const auto parsed                 = parse_review_comment_response({ .Status = 201, .Body = comment }, input);
			expect(parsed && parsed->DatabaseId == id && parsed->Author.DatabaseId == id, "comment and author retain exact uint64 identities");
			const auto reconciled = parse_review_comment_observations({ .Status = 200, .Body = "[" + comment + "]" }, input);
			expect(reconciled && reconciled->ExactReceipts.size() == 1U, "recovery sees the same exact large author identity");
			--input.LiveBinding->ActingIdentity.DatabaseId;
			const auto adjacent = parse_review_comment_observations({ .Status = 200, .Body = "[" + comment + "]" }, input);
			expect(adjacent && adjacent->ExactReceipts.empty(), "adjacent large identity never matches a recovery receipt");
		}
	}

	void test_legacy_json_validation_and_canonical_bytes()
	{
		for (const auto& body : { comment_body() + " {}", "{\"id\":44," + comment_body().substr(1U),
								  "{\"extra\":\"\\ud800\"," + comment_body().substr(1U), "{\"extra\":\"\\udc00\"," + comment_body().substr(1U),
								  "{\"extra\":\"\xC3\x28\"," + comment_body().substr(1U), replace_wire_field(comment_body(), "\"line\":13", "\"line\":null"),
								  replace_wire_field(comment_body(), "\"user\":{", "\"unknown_user\":{") }) {
			expect(!parse_review_comment_response({ .Status = 201, .Body = body }, request()), "legacy comment rejects missing, null, duplicate, trailing and malformed Unicode fields");
			expect(!parse_review_comment_observations({ .Status = 200, .Body = "[" + body + "]" }, request()), "comment lists retain the same full-input validation");
		}
		const auto extended = "{\"extension\":{\"ratio\":22.0000000000000001,\"values\":[true,null,\"\\ud83d\\ude00\"]}," + comment_body().substr(1U);
		expect(parse_review_comment_response({ .Status = 201, .Body = extended }, request()).has_value(), "valid unknown extension values remain accepted");
		auto input          = request();
		input.Proposal.Body = "Literal \\u000B \\u001F, escapes: \" \\ / café 😀 ";
		for (auto character = 0; character < 32; ++character) {
			input.Proposal.Body.push_back(static_cast<char>(character));
		}
		input.IdempotencyKey = application::idempotency_key_for(input.Proposal, input.Target);
		const auto compiled  = compile_review_comment_request(input);

		expect(compiled && compiled->Body == tests::canonical_json::line_comment, "literal canonical bytes preserve request fingerprints including every control escape and Unicode");
	}

	void test_historical_review_target_identity()
	{
		const auto response = review_mutation_wire_response{
			.Status = 200, .Body = target_body(commit(0x33U))
		};
		expect(parse_review_submission_recovery_target(response, target()).has_value(), "historical read accepts moved head for exact stable target");
		for (const auto& [from, to] : std::array<std::pair<std::string_view, std::string_view>, 9>{ { { "4246446933", "4246446934" }, { "4246446933", "4246446933.0" }, { "4246446933", "18446744073709551616" }, { "PR_kwDOTyvof879G6NV", "wrong-pr-node" }, { "1328277631", "1328277632" }, { "R_kgDOTyvofw", "wrong-repo-node" }, { "MikeMarcin/BPR-Test", "MikeMarcin/another-repo" }, { "\"number\":1", "\"number\":2" }, { "\"id\":4246446933", "\"unexpected\":4246446933" } } }) {
			auto       changed  = response;
			const auto position = changed.Body.find(from);
			expect(position != std::string::npos, "historical parser mutation locates expected field");
			changed.Body.replace(position, from.size(), to);
			expect(!parse_review_submission_recovery_target(changed, target()), "historical parser rejects wrong, missing and inexact stable identities");
		}
		auto duplicate = response;
		duplicate.Body.insert(1U, "\"id\":4246446933,");
		expect(!parse_review_submission_recovery_target(duplicate, target()), "historical parser rejects duplicate keys");
		auto redirected = response;
		redirected.Headers.push_back({ .Name = "Location", .Value = "https://elsewhere.invalid" });
		expect(!parse_review_submission_recovery_target(redirected, target()), "historical parser refuses redirected identity");
		auto oversized = response;
		oversized.Body.resize(maximum_review_mutation_response_bytes + 1U, ' ');
		expect(!parse_review_submission_recovery_target(oversized, target()), "historical target response remains bounded");
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
	const auto fixture = fixture_object{
		{ "fraction", 1.5 },
		{ "large", std::uint64_t{ 9'007'199'254'740'993U } },
		{ "negative", -1 },
		{ "null", nullptr },
	};
	expect(fixture_json(fixture)
			   == R"({"fraction":1.5,"large":9007199254740993,"negative":-1,"null":null})",
		   "semantic fixtures retain explicit null and signed, fractional and exact unsigned numbers");
	test_exact_wire_compilation_and_tamper_rejection();
	test_whole_review_dispositions_and_literal_summary();
	test_whole_review_request_rejection();
	test_whole_review_response_rejection();
	test_review_list_baseline_and_candidates();
	test_whole_review_exact_integer_ids_and_extensions();
	test_zero_based_semantic_lines_translate_to_one_based_rest_lines();
	test_exact_response_identity_and_bounds();
	test_historical_review_target_identity();
	test_legacy_exact_integer_json();
	test_legacy_json_validation_and_canonical_bytes();
	std::cout << "github-mutation-contract-tests:pass\n";
	return EXIT_SUCCESS;
}
