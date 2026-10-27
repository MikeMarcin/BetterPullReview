#include "canonical_json_expectations.hpp"
#include "fixture_json.hpp"
#include "github_mutation_contract.hpp"
#include "github_review_list_shapes.hpp"

#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/description_edit.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

	// [[BPR-0879]]. The pull request a thread belongs to, spelled as the
	// server spells it: the designated one, and one that is not.
	[[nodiscard]] auto designated_pull_request_json() -> std::string
	{
		return { "\"pullRequest\":{\"id\":\"PR_kwDOTyvof879G6NV\","
				 "\"databaseId\":4246446933,\"number\":1,"
				 "\"repository\":{\"id\":\"R_kgDOTyvofw\",\"databaseId\":1328277631,"
				 "\"nameWithOwner\":\"MikeMarcin/BPR-Test\"}}" };
	}

	[[nodiscard]] auto foreign_pull_request_json() -> std::string
	{
		return { "\"pullRequest\":{\"id\":\"PR_kwDOTyvof879Gzzz\","
				 "\"databaseId\":4246446999,\"number\":7,"
				 "\"repository\":{\"id\":\"R_kgDOTyvofw\",\"databaseId\":1328277631,"
				 "\"nameWithOwner\":\"MikeMarcin/BPR-Test\"}}" };
	}

	// [[BPR-0657]]. A reply names its thread and goes by GraphQL with the
	// reserved ReplyToThread document; a new root keeps REST. The reply's
	// response is held to the same identity as a REST comment. Resolve and
	// unresolve compile to the reserved documents and accept only the exact
	// thread in the state asked for.
	//
	// [[BPR-0879]]. Every one of those documents also names the pull request
	// the thread belongs to, and the pre-flight read that precedes a write
	// refuses a thread in any other pull request before the write happens.
	void test_replies_and_thread_resolution_go_by_graphql_exactly()
	{
		auto reply                   = request();
		reply.Proposal.ReplyToThread = "PRRT_fixture_1";
		reply.IdempotencyKey         = application::idempotency_key_for(reply.Proposal, reply.Target);
		expect(reply.IdempotencyKey != request().IdempotencyKey,
			   "a reply and a root at the same line with the same body are different publications");
		const auto compiled = compile_review_comment_request(reply);
		expect(compiled.has_value(), "reply request compiles");
		const auto document = std::string{
			"mutation ReplyToThread($thread:ID!,$body:String!){"
			"addPullRequestReviewThreadReply(input:{pullRequestReviewThreadId:$thread,body:$body}){"
			"comment{id databaseId body path line commit{oid} author{login} viewerDidAuthor "
			"pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}"
		};
		expect(compiled->Operation == review_mutation_operation::reply_review_thread
				   && compiled->Method == http_method::post && compiled->Path == "/graphql"
				   && compiled->Headers.size() == 4U
				   && compiled->Body
					   == "{\"query\":\"" + document
						   + "\",\"variables\":{\"thread\":\"PRRT_fixture_1\",\"body\":\"Please rename this.\"}}",
			   "reply request is the reserved document with exact variables at the GraphQL root");
		expect(validate_outbound_review_mutation_request(*compiled).has_value(),
			   "reply request revalidates canonically");
		const auto ghes = compile_review_comment_request(
			reply, application::github_server_contract::ghes_3_18_2022_11_28);
		expect(ghes.has_value() && ghes->Path == "/api/graphql",
			   "GHES replies use the exact GraphQL root");
		for (const auto tamper : { 0, 1, 2 }) {
			auto changed = *compiled;
			switch (tamper) {
			case 0:
				changed.Operation = review_mutation_operation::create_review_comment;
				break;
			case 1:
				changed.Path = "/repos/MikeMarcin/BPR-Test/pulls/1/comments";
				break;
			case 2:
				changed.Body += ' ';
				break;
			default:
				std::abort();
			}
			expect(!validate_outbound_review_mutation_request(changed).has_value(),
				   "any reply wire tamper is rejected");
		}
		auto badThread                   = reply;
		badThread.Proposal.ReplyToThread = "PRRT_\"fixture";
		badThread.IdempotencyKey         = application::idempotency_key_for(badThread.Proposal, badThread.Target);
		expect(!compile_review_comment_request(badThread).has_value(),
			   "a thread identity outside the GraphQL token vocabulary is refused");

		const auto replyBodyIn = [](const std::string_view aLogin,
									const std::string_view aBody,
									const std::string&     aPullRequest) {
			return std::string{ "{\"data\":{\"addPullRequestReviewThreadReply\":{\"comment\":{"
								"\"id\":\"PRRC_reply_45\",\"databaseId\":45,\"body\":\"" }
			+ std::string{ aBody } + "\",\"path\":\"fixture/review.txt\",\"line\":13,\"commit\":{\"oid\":\""
				+ std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() }
			+ "\"},\"author\":{\"login\":\"" + std::string{ aLogin } + "\"},\"viewerDidAuthor\":true,"
				+ aPullRequest + "}}}}";
		};
		const auto replyBody = [&replyBodyIn](const std::string_view aLogin, const std::string_view aBody) {
			return replyBodyIn(aLogin, aBody, designated_pull_request_json());
		};
		const auto parsed = parse_review_comment_response(
			{ .Status = 200, .Body = replyBody("fixture-reviewer", "Please rename this.") }, reply);
		expect(parsed.has_value() && parsed->NodeId == "PRRC_reply_45" && parsed->DatabaseId == 45U
				   && parsed->Receipt.ServerCommentId == "PRRC_reply_45" && !parsed->Receipt.Duplicate
				   && parsed->Anchor == reply.LiveBinding->Anchor,
			   "an exact reply response yields the comment's node identity and the bound anchor");
		expect(!parse_review_comment_response(
					{ .Status = 201, .Body = replyBody("fixture-reviewer", "Please rename this.") }, reply)
					.has_value(),
			   "a reply is a 200 GraphQL envelope, never a 201 comment");
		const auto otherAuthor = parse_review_comment_response(
			{ .Status = 200, .Body = replyBody("someone-else", "Please rename this.") }, reply);
		expect(!otherAuthor.has_value() && otherAuthor.error() == review_mutation_contract_failure::identity_mismatch,
			   "a reply by another login is an identity mismatch");
		const auto otherBody = parse_review_comment_response(
			{ .Status = 200, .Body = replyBody("fixture-reviewer", "Something else.") }, reply);
		expect(!otherBody.has_value() && otherBody.error() == review_mutation_contract_failure::identity_mismatch,
			   "a reply with another body is an identity mismatch");
		expect(!parse_review_comment_response({ .Status = 200, .Body = "{\"data\":null}" }, reply).has_value(),
			   "a GraphQL error envelope is malformed for a reply");
		// [[BPR-0879]]. The reply's response binding was strong on the comment
		// and silent on the pull request; it is not silent now.
		const auto foreignReply = parse_review_comment_response(
			{ .Status = 200,
			  .Body   = replyBodyIn("fixture-reviewer", "Please rename this.", foreign_pull_request_json()) },
			reply);
		expect(!foreignReply.has_value()
				   && foreignReply.error() == review_mutation_contract_failure::identity_mismatch,
			   "a reply the server says landed in another pull request is an identity mismatch");

		const auto resolution = application::review_thread_resolution_request{
			.Target         = target().Semantic,
			.ActingIdentity = identity().Account,
			.Thread         = "PRRT_fixture_1",
			.Resolve        = true,
			.LiveBinding    = reply.LiveBinding,
		};
		const auto resolve = compile_thread_resolution_request(resolution);
		expect(resolve.has_value() && resolve->Operation == review_mutation_operation::resolve_review_thread
				   && resolve->Method == http_method::post && resolve->Path == "/graphql"
				   && resolve->Body
					   == "{\"query\":\"mutation ResolveThread($thread:ID!){resolveReviewThread(input:{threadId:$thread}){"
						  "thread{id isResolved pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}\","
						  "\"variables\":{\"thread\":\"PRRT_fixture_1\"}}",
			   "resolve compiles to the reserved document with the exact thread and its pull request");
		expect(validate_outbound_review_mutation_request(*resolve).has_value(), "resolve revalidates canonically");
		auto unresolution    = resolution;
		unresolution.Resolve = false;
		const auto unresolve = compile_thread_resolution_request(unresolution);
		expect(unresolve.has_value() && unresolve->Operation == review_mutation_operation::unresolve_review_thread
				   && unresolve->Body.find("unresolveReviewThread") != std::string::npos
				   && unresolve->Body.find("mutation UnresolveThread") != std::string::npos,
			   "unresolve compiles to its own reserved document");
		auto unbound               = resolution;
		unbound.LiveBinding        = std::nullopt;
		auto foreign               = resolution;
		foreign.Target.PullRequest = core::pull_request_id{ 99U };
		auto blankThread           = resolution;
		blankThread.Thread         = "";
		expect(!compile_thread_resolution_request(unbound).has_value()
				   && !compile_thread_resolution_request(foreign).has_value()
				   && !compile_thread_resolution_request(blankThread).has_value(),
			   "an unbound, foreign or nameless resolution is refused before the wire");

		// [[BPR-0879]]. Every resolution answer names its pull request, so the
		// same body is spelled once and varied only where a row varies it.
		const auto resolutionBody = [](const std::string_view aField,
									   const std::string_view aThread,
									   const bool             aResolved,
									   const std::string&     aPullRequest) {
			return std::string{ "{\"data\":{\"" } + std::string{ aField }
			+ "\":{\"thread\":{\"id\":\"" + std::string{ aThread } + "\",\"isResolved\":"
				+ (aResolved ? "true" : "false") + "," + aPullRequest + "}}}}";
		};
		const auto resolved = parse_thread_resolution_response(
			{ .Status = 200,
			  .Body   = resolutionBody("resolveReviewThread", "PRRT_fixture_1", true, designated_pull_request_json()) },
			resolution);
		expect(resolved.has_value() && resolved->Receipt.Thread == "PRRT_fixture_1" && resolved->Receipt.Resolved,
			   "an exact resolve response yields the resolved thread");
		const auto stillOpen = parse_thread_resolution_response(
			{ .Status = 200,
			  .Body   = resolutionBody("resolveReviewThread", "PRRT_fixture_1", false, designated_pull_request_json()) },
			resolution);
		expect(!stillOpen.has_value() && stillOpen.error() == review_mutation_contract_failure::identity_mismatch,
			   "a thread not in the state asked for is a mismatch, not a success");
		const auto otherThread = parse_thread_resolution_response(
			{ .Status = 200,
			  .Body   = resolutionBody("resolveReviewThread", "PRRT_fixture_2", true, designated_pull_request_json()) },
			resolution);
		expect(!otherThread.has_value() && otherThread.error() == review_mutation_contract_failure::identity_mismatch,
			   "another thread's state is a mismatch");
		// [[BPR-0879]]. The write's own answer, bound to the pull request.
		const auto foreignResolved = parse_thread_resolution_response(
			{ .Status = 200,
			  .Body   = resolutionBody("resolveReviewThread", "PRRT_fixture_1", true, foreign_pull_request_json()) },
			resolution);
		expect(!foreignResolved.has_value()
				   && foreignResolved.error() == review_mutation_contract_failure::identity_mismatch,
			   "a thread the server places in another pull request is an identity mismatch");
		const auto unresolved = parse_thread_resolution_response(
			{ .Status = 200,
			  .Body   = resolutionBody("unresolveReviewThread", "PRRT_fixture_1", false, designated_pull_request_json()) },
			unresolution);
		expect(unresolved.has_value() && !unresolved->Receipt.Resolved, "an exact unresolve response yields the open thread");
		expect(!parse_thread_resolution_response({ .Status = 401, .Body = "{}" }, resolution).has_value(),
			   "a non-200 resolve answer is an invalid status");

		// [[BPR-0879]]. The pre-flight read: the same authority, the same
		// guards, a query in place of the mutation, and an answer that admits
		// only a thread inside the designated pull request.
		const auto observation = compile_thread_target_observation_request(resolution);
		expect(observation.has_value()
				   && observation->Operation == review_mutation_operation::observe_review_thread
				   && observation->Method == http_method::post && observation->Path == "/graphql"
				   && observation->Body
					   == "{\"query\":\"query ObserveThread($thread:ID!){node(id:$thread){... on PullRequestReviewThread{"
						  "id isResolved pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}\","
						  "\"variables\":{\"thread\":\"PRRT_fixture_1\"}}",
			   "the thread observation compiles to a read document with the exact thread");
		expect(validate_outbound_review_mutation_request(*observation).has_value(),
			   "the thread observation revalidates canonically");
		expect(compile_thread_target_observation_request(
				   resolution, application::github_server_contract::ghes_3_18_2022_11_28)
					   ->Path
				   == "/api/graphql",
			   "GHES observes a thread at the exact GraphQL root");
		expect(!compile_thread_target_observation_request(unbound).has_value()
				   && !compile_thread_target_observation_request(foreign).has_value()
				   && !compile_thread_target_observation_request(blankThread).has_value(),
			   "an unbound, foreign or nameless thread is not observed either");
		auto sameDocumentAsUnresolve      = *observation;
		sameDocumentAsUnresolve.Operation = review_mutation_operation::unresolve_review_thread;
		expect(!validate_outbound_review_mutation_request(sameDocumentAsUnresolve).has_value(),
			   "a read document cannot pass itself off as the unresolve mutation");

		const auto nodeBody = [](const std::string_view aThread,
								 const bool             aResolved,
								 const std::string&     aPullRequest) {
			return std::string{ "{\"data\":{\"node\":{\"id\":\"" } + std::string{ aThread }
			+ "\",\"isResolved\":" + (aResolved ? "true" : "false") + "," + aPullRequest + "}}}";
		};
		const auto inside = parse_thread_target_observation(
			{ .Status = 200, .Body = nodeBody("PRRT_fixture_1", false, designated_pull_request_json()) },
			resolution);
		expect(inside.has_value() && inside->Thread == "PRRT_fixture_1" && !inside->Resolved,
			   "a thread inside the designated pull request is observed with its state");
		const auto outside = parse_thread_target_observation(
			{ .Status = 200, .Body = nodeBody("PRRT_fixture_1", false, foreign_pull_request_json()) },
			resolution);
		expect(!outside.has_value() && outside.error() == review_mutation_contract_failure::identity_mismatch,
			   "a thread in another pull request is refused before any write");
		const auto echoed = parse_thread_target_observation(
			{ .Status = 200, .Body = nodeBody("PRRT_fixture_2", false, designated_pull_request_json()) },
			resolution);
		expect(!echoed.has_value() && echoed.error() == review_mutation_contract_failure::identity_mismatch,
			   "an answer about another thread is refused");
		expect(!parse_thread_target_observation({ .Status = 200, .Body = "{\"data\":{\"node\":{}}}" }, resolution)
					.has_value(),
			   "a node that is not a review thread selects nothing and is malformed");
		const auto invisible = parse_thread_target_observation(
			{ .Status = 200, .Body = R"({"data":{"node":null}})" }, resolution);
		expect(!invisible && invisible.error() == review_mutation_contract_failure::target_unavailable,
			   "a valid null node names an unavailable target and refuses the write");
		for (const auto body : { "{", R"({"data":{}})", R"({"data":{"node":{}}})", R"({"data":{"node":false}})" }) {
			const auto malformed = parse_thread_target_observation({ .Status = 200, .Body = body }, resolution);
			expect(!malformed && malformed.error() == review_mutation_contract_failure::malformed_response,
				   "invalid JSON or a missing, mistyped or incomplete node is a malformed response");
		}
		expect(!parse_thread_target_observation({ .Status = 401, .Body = "{}" }, resolution).has_value()
				   && !parse_thread_target_observation(
						   { .Status = 200, .Body = nodeBody("PRRT_fixture_1", false, designated_pull_request_json()) },
						   unbound)
						   .has_value(),
			   "a non-200 answer and an unbound expectation are both refused");
	}

	struct request_shape
	{
		http_method      Method;
		std::string_view GithubPath;
		std::string_view GhesPath;
		std::string_view GraphqlOperation;
	};

	// [[BPR-0904]]. Independent server expectations, never derived from the
	// compiler or exchange table. With no default, a new enum member lacking
	// a pin is a build error under the test target's warnings-as-errors policy.
	[[nodiscard]] auto expected_request_shape(const review_mutation_operation aOperation)
		-> std::optional<request_shape>
	{
		using operation = review_mutation_operation;
		switch (aOperation) {
		case operation::staged_list_reviews:           return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100&page=1", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100&page=1", "" };
		case operation::staged_read_review:            return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45", "" };
		case operation::staged_list_comments:          return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45/comments?per_page=100&page=1", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45/comments?per_page=100&page=1", "" };
		case operation::staged_create_pending:         return request_shape{ http_method::post, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews", "" };
		case operation::staged_update_comment:         return request_shape{ http_method::patch, "/repos/MikeMarcin/BPR-Test/pulls/comments/44", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/comments/44", "" };
		case operation::staged_update_summary:         return request_shape{ http_method::put, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45", "" };
		case operation::staged_submit_comment:         return request_shape{ http_method::post, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45/events", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45/events", "" };
		case operation::staged_discard_pending:        return request_shape{ http_method::delete_, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews/45", "" };
		case operation::staged_update_description:     return request_shape{ http_method::patch, "/repos/MikeMarcin/BPR-Test/pulls/1", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1", "" };
		case operation::staged_list_issue_comments:    return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/issues/1/comments?per_page=100&page=1", "/api/v3/repos/MikeMarcin/BPR-Test/issues/1/comments?per_page=100&page=1", "" };
		case operation::staged_create_issue_comment:   return request_shape{ http_method::post, "/repos/MikeMarcin/BPR-Test/issues/1/comments", "/api/v3/repos/MikeMarcin/BPR-Test/issues/1/comments", "" };
		case operation::staged_update_issue_comment:   return request_shape{ http_method::patch, "/repos/MikeMarcin/BPR-Test/issues/comments/44", "/api/v3/repos/MikeMarcin/BPR-Test/issues/comments/44", "" };
		case operation::staged_update_pending_comment: return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation UpdateReviewComment" };
		case operation::observe_pull_request:
			return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/pulls/1", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1", "" };
		case operation::observe_merge_base:
			return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/compare/main...1111111111111111111111111111111111111111?page=2&per_page=1",
								  "/api/v3/repos/MikeMarcin/BPR-Test/compare/main...1111111111111111111111111111111111111111?page=2&per_page=1", "" };
		case operation::observe_recorded_merge_base:
			return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/compare/1010101010101010101010101010101010101010...1111111111111111111111111111111111111111?page=2&per_page=1",
								  "/api/v3/repos/MikeMarcin/BPR-Test/compare/1010101010101010101010101010101010101010...1111111111111111111111111111111111111111?page=2&per_page=1", "" };
		case operation::observe_base_ancestry:
			// Compiled from the ancestry binding, whose expected head is the tip.
			return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/compare/1010101010101010101010101010101010101010...2222222222222222222222222222222222222222?page=2&per_page=1",
								  "/api/v3/repos/MikeMarcin/BPR-Test/compare/1010101010101010101010101010101010101010...2222222222222222222222222222222222222222?page=2&per_page=1", "" };
		case operation::observe_viewer:
			return request_shape{ http_method::get, "/user", "/api/v3/user", "" };
		case operation::observe_server_meta:
			// [[BPR-1736]]. GitHub Enterprise Server only: GitHub.com has no version.
			return request_shape{ http_method::get, "", "/api/v3/meta", "" };
		case operation::observe_review_comments:
			return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/pulls/1/comments?per_page=100", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/comments?per_page=100", "" };
		case operation::create_review_comment:
			return request_shape{ http_method::post, "/repos/MikeMarcin/BPR-Test/pulls/1/comments", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/comments", "" };
		case operation::create_review:
			return request_shape{ http_method::post, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews", "" };
		case operation::observe_reviews:
			return request_shape{ http_method::get, "/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100", "/api/v3/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100", "" };
		case operation::reply_review_thread:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation ReplyToThread" };
		case operation::resolve_review_thread:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation ResolveThread" };
		case operation::unresolve_review_thread:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation UnresolveThread" };
		case operation::observe_review_thread:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "query ObserveThread" };
		case operation::observe_review_thread_comments:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "query ObserveThreadComments" };
		case operation::observe_reaction_subject:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "query ObserveReactionSubject" };
		case operation::add_reaction:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation AddReaction" };
		case operation::remove_reaction:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation RemoveReaction" };
		case operation::observe_pull_request_description:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "query ObserveDescription" };
		case operation::update_pull_request_description:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation UpdateDescription" };
		case operation::observe_comment_for_edit:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "query ObserveComment" };
		case operation::edit_review_comment:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation EditReviewComment" };
		case operation::edit_issue_comment:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation EditIssueComment" };
		case operation::edit_review_body:
			return request_shape{ http_method::post, "/graphql", "/api/graphql", "mutation EditReviewBody" };
		}
		return std::nullopt;
	}

	void expect_request_shape(const review_mutation_wire_request& aWire)
	{
		const auto expected = expected_request_shape(aWire.Operation);
		const auto label    = "operation " + std::to_string(static_cast<unsigned int>(aWire.Operation));
		expect(expected.has_value(), label + " has an independent request-shape pin");
		const auto path = aWire.Server == application::github_server_contract::github_com_2022_11_28
			? expected->GithubPath
			: expected->GhesPath;
		expect(aWire.Method == expected->Method && aWire.Path == path,
			   label + " compiles to its independent method and exact path");
		if (!expected->GraphqlOperation.empty()) {
			expect(aWire.Body.starts_with("{\"query\":\"" + std::string{ expected->GraphqlOperation } + "("),
				   label + " compiles to its independent GraphQL kind and operation name");
		}
	}

	void expect_every_shape_exercised(const std::vector<review_mutation_operation>& aExercised, const application::github_server_contract aServer)
	{
		// Enumerate the enum's entire underlying range, including holes. A pin
		// added to the exhaustive switch must also have a compiled fixture;
		// scripting an unused transport answer cannot satisfy this check.
		for (auto value = 0U; value <= std::numeric_limits<std::uint8_t>::max(); ++value) {
			const auto op    = static_cast<review_mutation_operation>(value);
			const auto count = std::ranges::count(aExercised, op);
			// [[BPR-1736]]. GitHub.com has no `GET /meta` to compile.
			const auto pinned = expected_request_shape(op).has_value() && !(op == review_mutation_operation::observe_server_meta && aServer == application::github_server_contract::github_com_2022_11_28);
			expect(count == (pinned ? 1 : 0),
				   "operation " + std::to_string(value) + " exercises its request-shape pin exactly once per server");
		}
	}

	auto staged_request(application::staged_review_operation aOperation, application::review_mutation_target_binding aTarget = target()) -> application::staged_review_request
	{
		using op = application::staged_review_operation;
		application::staged_review_request result{ .Binding = { aTarget, identity() }, .Operation = aOperation };
		if (aOperation == op::read_review || aOperation == op::list_comments || aOperation == op::update_summary || aOperation == op::submit_comment || aOperation == op::discard_pending) {
			result.Review = application::review_database_id{ 45 };
		}
		if (aOperation == op::update_comment || aOperation == op::update_issue_comment || aOperation == op::update_pending_comment) {
			result.Comment = 44;
		}
		if (aOperation == op::update_pending_comment) {
			result.Review      = application::review_database_id{ 45 };
			result.CommentNode = "PRRC_fixture_44";
		}
		if (aOperation == op::create_pending || aOperation == op::update_comment || aOperation == op::update_summary || aOperation == op::submit_comment || aOperation == op::update_description || aOperation == op::create_issue_comment || aOperation == op::update_issue_comment || aOperation == op::update_pending_comment) {
			result.Body = "Raw **tour** `$(literal)`";
		}
		if (aOperation == op::create_pending) {
			result.Lines.push_back({ .Source = { .Commit = aTarget.Semantic.ExpectedHead, .Path = "fixture/review.txt", .Side = core::diff_side::right, .FirstLine = 10, .LastLine = 12 }, .Body = "First stop" });
		}
		return result;
	}
	[[nodiscard]] auto compile_exchange_fixtures(const application::github_server_contract aServer)
	{
		auto serverTarget           = target();
		serverTarget.ServerContract = aServer;
		serverTarget.Hostname       = application::github_canonical_hostname{
			aServer == application::github_server_contract::github_com_2022_11_28 ? "github.com" : "ghe.example.com"
		};
		auto root                    = request();
		root.LiveBinding->Target     = serverTarget;
		auto reply                   = root;
		reply.Proposal.ReplyToThread = "PRRT_fixture_1";
		reply.IdempotencyKey         = application::idempotency_key_for(reply.Proposal, reply.Target);
		const auto resolution        = application::review_thread_resolution_request{
			.Target         = serverTarget.Semantic,
			.ActingIdentity = identity().Account,
			.Thread         = "PRRT_fixture_1",
			.Resolve        = true,
			.LiveBinding    = reply.LiveBinding,
		};
		auto unresolution    = resolution;
		unresolution.Resolve = false;
		const auto review    = review_submission_request{
			.Target         = serverTarget,
			.ActingIdentity = identity(),
			.Disposition    = core::review_disposition::comment,
			.Body           = "Looks good.",
		};
		const auto reaction = application::review_reaction_request{
			.Target         = serverTarget.Semantic,
			.ActingIdentity = identity().Account,
			.Subject        = "PRRC_fixture_1",
			.Content        = core::reaction_content::heart,
			.Add            = true,
			.LiveBinding    = reply.LiveBinding,
		};
		auto unreaction        = reaction;
		unreaction.Add         = false;
		const auto description = application::review_description_request{
			.Target         = serverTarget.Semantic,
			.ActingIdentity = identity().Account,
			.Original       = "Before.",
			.Body           = "After.",
			.LiveBinding    = reply.LiveBinding,
		};
		const auto commentEdit = application::review_comment_edit_request{
			.Target         = serverTarget.Semantic,
			.ActingIdentity = identity().Account,
			.Kind           = core::comment_edit_kind::review_comment,
			.Subject        = "PRRC_fixture_1",
			.Original       = "Before.",
			.Body           = "After.",
			.LiveBinding    = reply.LiveBinding,
		};
		auto issueEdit         = commentEdit;
		issueEdit.Kind         = core::comment_edit_kind::pull_request_comment;
		issueEdit.Subject      = "IC_fixture_1";
		auto reviewBodyEdit    = commentEdit;
		reviewBodyEdit.Kind    = core::comment_edit_kind::review_body;
		reviewBodyEdit.Subject = "PRR_fixture_1";
		return std::array{

			compile_target_observation_request(serverTarget, aServer),
			compile_viewer_observation_request(identity(), aServer),
			compile_review_comment_observation_request(root, aServer),
			compile_review_submission_observation_request(review, aServer),
			compile_review_comment_request(root, aServer),
			compile_review_submission_request(review, aServer),
			compile_review_comment_request(reply, aServer),
			compile_thread_resolution_request(resolution, aServer),
			compile_thread_resolution_request(unresolution, aServer),
			compile_thread_target_observation_request(resolution, aServer),
			compile_review_comment_observation_request(reply, aServer),
			compile_staged_review_request(staged_request(application::staged_review_operation::list_reviews, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::read_review, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::list_comments, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::create_pending, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::update_comment, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::update_summary, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::submit_comment, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::discard_pending, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::update_description, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::list_issue_comments, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::create_issue_comment, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::update_issue_comment, serverTarget)),
			compile_staged_review_request(staged_request(application::staged_review_operation::update_pending_comment, serverTarget)),
			compile_merge_base_observation_request(serverTarget, aServer),
			compile_merge_base_observation_request(serverTarget, aServer, merge_base_observation_basis::recorded_base),
			compile_merge_base_observation_request(merge_base_ancestry_binding(serverTarget, commit(0x22U)), aServer, merge_base_observation_basis::base_ancestry),
			compile_server_meta_request(serverTarget, aServer),
			compile_reaction_subject_observation_request(reaction, aServer),
			compile_reaction_request(reaction, aServer),
			compile_reaction_request(unreaction, aServer),
			compile_description_observation_request(description, aServer),
			compile_description_request(description, aServer),
			compile_comment_observation_request(commentEdit, aServer),
			compile_comment_edit_request(commentEdit, aServer),
			compile_comment_edit_request(issueEdit, aServer),
			compile_comment_edit_request(reviewBodyEdit, aServer),
		};
	}

	// [[BPR-1018]]. What each operation demands of a transport, row by row,
	// and that each operation's compiled request agrees with its own row: the
	// method it is sent with, and whether its path is the GraphQL root.
	void test_every_operation_names_its_exchange()
	{
		using operation = review_mutation_operation;
		struct row
		{
			operation   Operation;
			http_method Method;
			bool        Graphql;
			int         SuccessfulStatus;
			bool        Witnessed;
		};
		constexpr auto rows = std::array{
			row{ operation::observe_pull_request, http_method::get, false, 200, false },
			row{ operation::observe_viewer, http_method::get, false, 200, false },
			row{ operation::observe_review_comments, http_method::get, false, 200, false },
			row{ operation::observe_reviews, http_method::get, false, 200, false },
			row{ operation::create_review_comment, http_method::post, false, 201, true },
			row{ operation::create_review, http_method::post, false, 200, true },
			row{ operation::reply_review_thread, http_method::post, true, 200, true },
			row{ operation::resolve_review_thread, http_method::post, true, 200, false },
			row{ operation::unresolve_review_thread, http_method::post, true, 200, false },
			row{ operation::observe_review_thread, http_method::post, true, 200, false },
			row{ operation::observe_review_thread_comments, http_method::post, true, 200, false },
			row{ operation::staged_list_reviews, http_method::get, false, 200, false },
			row{ operation::staged_read_review, http_method::get, false, 200, false },
			row{ operation::staged_list_comments, http_method::get, false, 200, false },
			row{ operation::staged_create_pending, http_method::post, false, 200, true },
			row{ operation::staged_update_comment, http_method::patch, false, 200, true },
			row{ operation::staged_update_summary, http_method::put, false, 200, true },
			row{ operation::staged_submit_comment, http_method::post, false, 200, true },
			row{ operation::staged_discard_pending, http_method::delete_, false, 200, true },
			row{ operation::staged_update_description, http_method::patch, false, 200, true },
			row{ operation::staged_list_issue_comments, http_method::get, false, 200, false },
			row{ operation::staged_create_issue_comment, http_method::post, false, 201, true },
			row{ operation::staged_update_issue_comment, http_method::patch, false, 200, true },
			row{ operation::staged_update_pending_comment, http_method::post, true, 200, true },
			row{ operation::observe_merge_base, http_method::get, false, 200, false },
			row{ operation::observe_recorded_merge_base, http_method::get, false, 200, false },
			row{ operation::observe_base_ancestry, http_method::get, false, 200, false },
			row{ operation::observe_server_meta, http_method::get, false, 200, false },
			row{ operation::observe_reaction_subject, http_method::post, true, 200, false },
			row{ operation::add_reaction, http_method::post, true, 200, false },
			row{ operation::remove_reaction, http_method::post, true, 200, false },
			row{ operation::observe_pull_request_description, http_method::post, true, 200, false },
			row{ operation::update_pull_request_description, http_method::post, true, 200, false },
			row{ operation::observe_comment_for_edit, http_method::post, true, 200, false },
			row{ operation::edit_review_comment, http_method::post, true, 200, false },
			row{ operation::edit_issue_comment, http_method::post, true, 200, false },
			row{ operation::edit_review_body, http_method::post, true, 200, false },
		};

		for (const auto& [op, method, graphql, status, witnessed] : rows) {
			expect(review_mutation_exchange_for(op)
					   == review_mutation_exchange{
						   .Method           = method,
						   .Graphql          = graphql,
						   .SuccessfulStatus = status,
						   .Witnessed        = witnessed,
					   },
				   "operation " + std::to_string(static_cast<int>(op)) + " names its exchange");
		}
		// An operation appended to the enumeration has no row here, and no
		// exchange in the contract, until someone classifies it.
		expect(!review_mutation_exchange_for(static_cast<operation>(rows.size())).has_value(),
			   "a value past the classified operations has no exchange");

		for (const auto server : { application::github_server_contract::github_com_2022_11_28,
								   application::github_server_contract::ghes_3_18_2022_11_28 }) {
			const auto compiled = compile_exchange_fixtures(server);
			static_assert(compiled.size() == rows.size());
			auto exercised = std::vector<review_mutation_operation>{ };
			for (std::size_t index = 0U; index < rows.size(); ++index) {
				const auto& wire = compiled[index];
				if (rows[index].Operation == operation::observe_server_meta && server == application::github_server_contract::github_com_2022_11_28) {
					expect(!wire.has_value(), "GitHub.com has no server version to read");
					continue;
				}
				expect(wire.has_value() && wire->Operation == rows[index].Operation
						   && wire->Method == rows[index].Method
						   && (wire->Path == "/graphql" || wire->Path == "/api/graphql") == rows[index].Graphql,
					   "operation " + std::to_string(index) + " compiles to its exchange's method and root");
				expect_request_shape(*wire);
				exercised.push_back(wire->Operation);
			}
			expect_every_shape_exercised(exercised, server);
		}
	}

	/// [[BPR-1741]]. GitHub's eight groups, as `reactionGroups` always
	/// answers them: zero counts included, the viewer's thumbs up named.
	[[nodiscard]] auto all_reaction_groups_json(const bool aMine, const std::uint32_t aThumbs) -> std::string
	{
		auto groups = std::string{ "[" };
		for (const auto content : core::reaction_contents) {
			const auto thumbs = content == core::reaction_content::thumbs_up;
			const auto mine   = thumbs && aMine;
			groups += std::string{ groups.size() > 1U ? "," : "" } + "{\"content\":\"" + std::string{ core::reaction_graphql_name(content) } + "\",\"viewerHasReacted\":"
				+ (mine ? "true" : "false") + ",\"reactors\":{\"totalCount\":" + std::to_string(thumbs ? aThumbs : 0U) + ",\"nodes\":["
				+ (mine ? "{\"__typename\":\"User\",\"login\":\"fixture-reviewer\"}" : "") + "]}}";
		}
		return groups + "]";
	}

	[[nodiscard]] auto reaction_subject_body(const std::string_view aType, const std::string_view aId, const bool aMine, const std::uint32_t aThumbs, const std::string& aPullRequest)
		-> std::string
	{
		return "{\"__typename\":\"" + std::string{ aType } + "\",\"id\":\"" + std::string{ aId } + "\",\"viewerCanReact\":true,\"reactionGroups\":"
			+ all_reaction_groups_json(aMine, aThumbs) + "," + aPullRequest + "}";
	}

	// [[BPR-1741]]. A reaction goes by fixed GraphQL documents: the
	// read that binds the comment to the pull request, and the two writes,
	// each named so the read transport refuses it, each answer bound to the
	// pull request and the comment, and a write's answer in the state asked.
	void test_reactions_go_by_graphql_exactly()
	{
		const auto selection = std::string{
			"__typename id ... on Reactable{viewerCanReact reactionGroups{content viewerHasReacted reactors(first:5){totalCount "
			"nodes{__typename ... on User{login} ... on Bot{login} ... on Organization{login} ... on Mannequin{login}}}}} "
			"... on PullRequestReviewComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on IssueComment{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}} "
			"... on PullRequestReview{pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}"
		};
		const auto reaction = application::review_reaction_request{
			.Target         = target().Semantic,
			.ActingIdentity = identity().Account,
			.Subject        = "PRRC_fixture_1",
			.Content        = core::reaction_content::thumbs_up,
			.Add            = true,
			.LiveBinding    = request().LiveBinding,
		};
		auto unreaction = reaction;
		unreaction.Add  = false;
		const auto add  = compile_reaction_request(reaction);
		expect(add.has_value() && add->Operation == review_mutation_operation::add_reaction && add->Method == http_method::post && add->Path == "/graphql"
				   && add->Body
					   == "{\"query\":\"mutation AddReaction($subject:ID!,$content:ReactionContent!){addReaction(input:{subjectId:$subject,content:$content}){"
						  "reaction{content} subject{"
						   + selection + "}}}\",\"variables\":{\"subject\":\"PRRC_fixture_1\",\"content\":\"THUMBS_UP\"}}",
			   "adding a reaction compiles to the reserved document with the exact comment and content");
		const auto remove = compile_reaction_request(unreaction);
		expect(remove.has_value() && remove->Operation == review_mutation_operation::remove_reaction
				   && remove->Body.starts_with("{\"query\":\"mutation RemoveReaction($subject:ID!,$content:ReactionContent!){removeReaction(")
				   && remove->Body.ends_with("\"variables\":{\"subject\":\"PRRC_fixture_1\",\"content\":\"THUMBS_UP\"}}"),
			   "removing a reaction compiles to its own reserved document");
		const auto observe = compile_reaction_subject_observation_request(reaction);
		expect(observe.has_value() && observe->Operation == review_mutation_operation::observe_reaction_subject && observe->Path == "/graphql"
				   && observe->Body == "{\"query\":\"query ObserveReactionSubject($subject:ID!){node(id:$subject){" + selection + "}}\",\"variables\":{\"subject\":\"PRRC_fixture_1\"}}",
			   "the subject observation is a read document naming only the comment");
		for (const auto& wire : { *add, *remove, *observe }) {
			expect(validate_outbound_review_mutation_request(wire).has_value(), "each reaction request revalidates canonically");
		}
		for (const auto content : core::reaction_contents) {
			auto other    = reaction;
			other.Content = content;
			expect(compile_reaction_request(other)->Body.find("\"content\":\"" + std::string{ core::reaction_graphql_name(content) } + "\"") != std::string::npos,
				   "every content goes by GitHub's ReactionContent name");
		}
		auto spoofed      = *observe;
		spoofed.Operation = review_mutation_operation::add_reaction;
		auto swapped      = *add;
		swapped.Operation = review_mutation_operation::remove_reaction;
		auto rewritten    = *add;
		rewritten.Body.replace(rewritten.Body.find("THUMBS_UP"), 9U, "HEART");
		expect(!validate_outbound_review_mutation_request(spoofed).has_value() && !validate_outbound_review_mutation_request(swapped).has_value()
				   && !validate_outbound_review_mutation_request(rewritten).has_value(),
			   "a read passed off as a write, a write passed off as the other and a body changed in flight are refused");
		auto unbound               = reaction;
		unbound.LiveBinding        = std::nullopt;
		auto foreign               = reaction;
		foreign.Target.PullRequest = core::pull_request_id{ 99U };
		auto blank                 = reaction;
		blank.Subject              = "";
		auto quoted                = reaction;
		quoted.Subject             = "PRRC\"x";
		auto unknown               = reaction;
		unknown.Content            = static_cast<core::reaction_content>(9U);
		for (const auto& refused : { unbound, foreign, blank, quoted, unknown }) {
			expect(!compile_reaction_request(refused).has_value() && !compile_reaction_subject_observation_request(refused).has_value(),
				   "an unbound, foreign, nameless, unsafe or unknown reaction is refused before the wire");
		}
		auto enterprise                               = reaction;
		enterprise.LiveBinding->Target.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		enterprise.LiveBinding->Target.Hostname       = application::github_canonical_hostname{ "ghe.example.com" };
		expect(compile_reaction_request(enterprise, application::github_server_contract::ghes_3_18_2022_11_28)->Path == "/api/graphql"
				   && !compile_reaction_request(reaction, application::github_server_contract::ghes_3_18_2022_11_28).has_value(),
			   "GHES reacts at its GraphQL root, and a binding for another server is refused");

		// The answers.
		const auto observed = parse_reaction_subject_observation(
			{ .Status = 200, .Body = "{\"data\":{\"node\":" + reaction_subject_body("PullRequestReviewComment", "PRRC_fixture_1", false, 2U, designated_pull_request_json()) + "}}" }, reaction);
		expect(observed.has_value() && observed->Subject == "PRRC_fixture_1" && observed->Reactions.ViewerCanReact && observed->Reactions.Groups.size() == 1U
				   && observed->Reactions.Groups[0].Count == 2U && !observed->Reactions.Groups[0].ViewerReacted,
			   "a comment inside the designated pull request is observed with its reactions, zero groups dropped");
		const auto outside = parse_reaction_subject_observation(
			{ .Status = 200, .Body = "{\"data\":{\"node\":" + reaction_subject_body("IssueComment", "PRRC_fixture_1", false, 2U, foreign_pull_request_json()) + "}}" }, reaction);
		expect(!outside && outside.error() == review_mutation_contract_failure::identity_mismatch, "a comment of another pull request is an identity mismatch");
		const auto otherComment = parse_reaction_subject_observation(
			{ .Status = 200, .Body = "{\"data\":{\"node\":" + reaction_subject_body("IssueComment", "IC_other", false, 2U, designated_pull_request_json()) + "}}" }, reaction);
		expect(!otherComment && otherComment.error() == review_mutation_contract_failure::identity_mismatch, "another comment is an identity mismatch");
		const auto commit = parse_reaction_subject_observation(
			{ .Status = 200, .Body = "{\"data\":{\"node\":" + reaction_subject_body("Commit", "PRRC_fixture_1", false, 2U, designated_pull_request_json()) + "}}" }, reaction);
		expect(!commit && commit.error() == review_mutation_contract_failure::identity_mismatch, "a subject of another kind is an identity mismatch");
		expect(parse_reaction_subject_observation({ .Status = 200, .Body = R"({"data":{"node":null}})" }, reaction).error() == review_mutation_contract_failure::target_unavailable,
			   "a comment GitHub no longer shows is unavailable");
		expect(!parse_reaction_subject_observation({ .Status = 502, .Body = "{}" }, reaction).has_value(), "a non-200 answer is an invalid status");

		const auto written = parse_reaction_response(
			{ .Status = 200,
			  .Body   = "{\"data\":{\"addReaction\":{\"reaction\":{\"content\":\"THUMBS_UP\"},\"subject\":"
				  + reaction_subject_body("PullRequestReviewComment", "PRRC_fixture_1", true, 3U, designated_pull_request_json()) + "}}}" },
			reaction);
		expect(written.has_value() && written->Reactions.Groups[0].ViewerReacted && written->Reactions.Groups[0].Count == 3U
				   && written->Reactions.Groups[0].Reactors == std::vector<std::string>{ "fixture-reviewer" },
			   "an exact add answer yields the comment's reactions with mine");
		const auto notTaken = parse_reaction_response(
			{ .Status = 200,
			  .Body   = "{\"data\":{\"addReaction\":{\"reaction\":{\"content\":\"THUMBS_UP\"},\"subject\":"
				  + reaction_subject_body("PullRequestReviewComment", "PRRC_fixture_1", false, 2U, designated_pull_request_json()) + "}}}" },
			reaction);
		expect(!notTaken && notTaken.error() == review_mutation_contract_failure::identity_mismatch, "an answer not in the state asked is a mismatch, not a success");
		const auto otherContent = parse_reaction_response(
			{ .Status = 200,
			  .Body   = "{\"data\":{\"addReaction\":{\"reaction\":{\"content\":\"HEART\"},\"subject\":"
				  + reaction_subject_body("PullRequestReviewComment", "PRRC_fixture_1", true, 3U, designated_pull_request_json()) + "}}}" },
			reaction);
		expect(!otherContent.has_value(), "an answer about another content is refused");
		const auto removed = parse_reaction_response(
			{ .Status = 200,
			  .Body   = "{\"data\":{\"removeReaction\":{\"reaction\":{\"content\":\"THUMBS_UP\"},\"subject\":"
				  + reaction_subject_body("PullRequestReviewComment", "PRRC_fixture_1", false, 1U, designated_pull_request_json()) + "}}}" },
			unreaction);
		expect(removed.has_value() && !removed->Reactions.Groups[0].ViewerReacted && removed->Reactions.Groups[0].Count == 1U, "an exact remove answer yields the comment without mine");
		expect(!parse_reaction_response({ .Status = 200, .Body = R"({"data":{"addReaction":null},"errors":[{"type":"FORBIDDEN"}]})" }, reaction).has_value(),
			   "a GraphQL refusal is not a success");
	}

	/// [[BPR-1774]]. The designated pull request as a description answer
	/// spells it, with a body, GitHub's permission and its update time.
	[[nodiscard]] auto description_pull_request_json(const std::string_view aBody, const bool aCan, const std::string_view aId = "PR_kwDOTyvof879G6NV") -> std::string
	{
		return "{\"id\":\"" + std::string{ aId } + "\",\"databaseId\":4246446933,\"number\":1,\"body\":\"" + std::string{ aBody } + "\",\"viewerCanUpdate\":"
			+ (aCan ? "true" : "false")
			+ ",\"updatedAt\":\"2026-10-26T12:00:00Z\",\"repository\":{\"id\":\"R_kgDOTyvofw\",\"databaseId\":1328277631,\"nameWithOwner\":\"MikeMarcin/BPR-Test\"}}";
	}

	// [[BPR-1774]]. The description goes by two fixed GraphQL documents: the
	// read that binds the pull request and says its body and permission, and
	// the write. Both name the pull request by the node the accepted read
	// gave, revalidate canonically, and are held to the designated pull
	// request; the write's answer must carry the body asked.
	void test_the_description_goes_by_graphql_exactly()
	{
		const auto selection = std::string{ "id databaseId number body viewerCanUpdate updatedAt repository{id databaseId nameWithOwner}" };
		const auto edit      = application::review_description_request{
			.Target         = target().Semantic,
			.ActingIdentity = identity().Account,
			.Original       = "Before.\r\n",
			.Body           = "After \"quoted\".\r\n",
			.LiveBinding    = request().LiveBinding,
		};
		const auto write = compile_description_request(edit);
		expect(write.has_value() && write->Operation == review_mutation_operation::update_pull_request_description && write->Method == http_method::post && write->Path == "/graphql"
				   && write->Body
					   == "{\"query\":\"mutation UpdateDescription($pullRequest:ID!,$body:String!){updatePullRequest(input:{pullRequestId:$pullRequest,body:$body}){pullRequest{"
						   + selection + "}}}\",\"variables\":{\"pullRequest\":\"PR_kwDOTyvof879G6NV\",\"body\":\"After \\\"quoted\\\".\\r\\n\"}}",
			   "the description write is the reserved document naming the designated pull request and the exact body");
		const auto observe = compile_description_observation_request(edit);
		expect(observe.has_value() && observe->Operation == review_mutation_operation::observe_pull_request_description && observe->Path == "/graphql"
				   && observe->Body
					   == "{\"query\":\"query ObserveDescription($pullRequest:ID!){node(id:$pullRequest){... on PullRequest{" + selection
						   + "}}}\",\"variables\":{\"pullRequest\":\"PR_kwDOTyvof879G6NV\"}}",
			   "the observation is a read document naming only the pull request");
		expect(write->Body.find("Before.") == std::string::npos && observe->Body.find("After") == std::string::npos, "the original is never sent, and the read carries no body");
		for (const auto& wire : { *write, *observe }) {
			expect(validate_outbound_review_mutation_request(wire).has_value(), "each description request revalidates canonically");
		}
		auto spoofed      = *observe;
		spoofed.Operation = review_mutation_operation::update_pull_request_description;
		auto rewritten    = *write;
		rewritten.Body.replace(rewritten.Body.find("After"), 5U, "Other");
		auto retargeted = *write;
		retargeted.Body.replace(retargeted.Body.find("PR_kwDOTyvof879G6NV"), 19U, "PR_kwDOTyvof879Gzzz");
		expect(!validate_outbound_review_mutation_request(spoofed).has_value() && !validate_outbound_review_mutation_request(rewritten).has_value()
				   && !validate_outbound_review_mutation_request(retargeted).has_value(),
			   "a read passed off as the write, a body changed in flight and another pull request are refused");

		auto unbound               = edit;
		unbound.LiveBinding        = std::nullopt;
		auto foreign               = edit;
		foreign.Target.PullRequest = core::pull_request_id{ 99U };
		auto nodeless              = edit;
		nodeless.LiveBinding->Target.PullRequestNodeId.clear();
		auto actor           = edit;
		actor.ActingIdentity = core::github_account_id{ 99U };
		auto invalid         = edit;
		invalid.Body         = std::string{ "\xC3\x28" };
		for (const auto& refused : { unbound, foreign, nodeless, actor }) {
			expect(!compile_description_request(refused).has_value() && !compile_description_observation_request(refused).has_value(),
				   "an unbound, foreign, nodeless or other-actor edit is refused before the wire");
		}
		expect(!compile_description_request(invalid).has_value(), "text that is not UTF-8 is refused");
		auto longest = edit;
		longest.Body = std::string(core::maximum_description_characters, 'a');
		auto huge    = edit;
		huge.Body    = std::string(maximum_description_request_bytes, 'a');
		expect(compile_description_request(longest).has_value() && maximum_review_mutation_wire_request_bytes(review_mutation_operation::update_pull_request_description) == maximum_description_request_bytes,
			   "GitHub's 65,536 characters go in one write");
		expect(!compile_description_request(huge).has_value() && compile_description_observation_request(huge).has_value(),
			   "a body past the request ceiling is refused to write, and can still be observed");
		auto enterprise                               = edit;
		enterprise.LiveBinding->Target.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		enterprise.LiveBinding->Target.Hostname       = application::github_canonical_hostname{ "ghe.example.com" };
		expect(compile_description_request(enterprise, application::github_server_contract::ghes_3_18_2022_11_28)->Path == "/api/graphql"
				   && compile_description_observation_request(enterprise, application::github_server_contract::ghes_3_18_2022_11_28)->Path == "/api/graphql"
				   && !compile_description_request(edit, application::github_server_contract::ghes_3_18_2022_11_28).has_value(),
			   "GHES 3.18 edits at its GraphQL root, and a binding for another server is refused");

		// The answers.
		const auto observed = parse_description_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + description_pull_request_json("Before.\\r\\n", true) + "}}" }, edit);
		expect(observed.has_value() && observed->Body == "Before.\r\n" && observed->ViewerCanUpdate && observed->UpdatedAt == "2026-10-26T12:00:00Z",
			   "the designated pull request is observed with its exact body, permission and time");
		const auto denied = parse_description_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + description_pull_request_json("Before.", false) + "}}" }, edit);
		expect(denied.has_value() && !denied->ViewerCanUpdate, "GitHub's refusal of the viewer is read as such");
		const auto outside = parse_description_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + description_pull_request_json("Before.", true, "PR_kwDOTyvof879Gzzz") + "}}" }, edit);
		expect(!outside && outside.error() == review_mutation_contract_failure::identity_mismatch, "another pull request is an identity mismatch");
		expect(parse_description_observation({ .Status = 200, .Body = R"({"data":{"node":null}})" }, edit).error() == review_mutation_contract_failure::target_unavailable,
			   "a pull request GitHub no longer shows is unavailable");
		expect(parse_description_observation({ .Status = 502, .Body = "{}" }, edit).error() == review_mutation_contract_failure::invalid_status, "a non-200 answer is an invalid status");
		const auto written = parse_description_response(
			{ .Status = 200, .Body = "{\"data\":{\"updatePullRequest\":{\"pullRequest\":" + description_pull_request_json("After \\\"quoted\\\".\\r\\n", true) + "}}}" }, edit);
		expect(written.has_value() && written->Body == edit.Body, "an exact write answer yields the body asked");
		const auto other = parse_description_response({ .Status = 200, .Body = "{\"data\":{\"updatePullRequest\":{\"pullRequest\":" + description_pull_request_json("Before.", true) + "}}}" }, edit);
		expect(!other && other.error() == review_mutation_contract_failure::identity_mismatch, "an answer with another body is a mismatch, not a success");
		expect(!parse_description_response({ .Status = 200, .Body = R"({"data":{"updatePullRequest":null},"errors":[{"type":"FORBIDDEN"}]})" }, edit).has_value(),
			   "a GraphQL refusal is not a success");
		auto big         = edit;
		big.Body         = std::string(core::maximum_description_characters, 'b');
		const auto large = parse_description_response({ .Status = 200, .Body = "{\"data\":{\"updatePullRequest\":{\"pullRequest\":" + description_pull_request_json(big.Body, true) + "}}}" }, big);
		expect(large.has_value() && large->Body.size() == core::maximum_description_characters, "an answer carrying GitHub's longest body is read");
	}

	/// [[BPR-1775]]. A comment of one kind as an edit answer spells it.
	[[nodiscard]] auto comment_json(const std::string_view aTypename, const std::string_view aId, const std::string_view aBody, const bool aCan, const std::string_view aPullRequest = "PR_kwDOTyvof879G6NV", const bool aMine = true) -> std::string
	{
		return "{\"__typename\":\"" + std::string{ aTypename } + "\",\"id\":\"" + std::string{ aId } + "\",\"body\":\"" + std::string{ aBody } + "\",\"viewerCanUpdate\":" + (aCan ? "true" : "false")
			+ ",\"viewerDidAuthor\":" + (aMine ? "true" : "false") + ",\"updatedAt\":\"2026-10-27T12:00:00Z\",\"pullRequest\":{\"id\":\"" + std::string{ aPullRequest }
		+ "\",\"databaseId\":4246446933,\"number\":1,\"repository\":{\"id\":\"R_kgDOTyvofw\",\"databaseId\":1328277631,\"nameWithOwner\":\"MikeMarcin/BPR-Test\"}}}";
	}

	// [[BPR-1775]]. A published comment goes by one read and the update its
	// kind takes: each names the comment by its node, revalidates
	// canonically, and is held to the comment asked, of its kind, in the
	// designated pull request; the write's answer must carry the body asked.
	void test_a_published_comment_goes_by_graphql_exactly()
	{
		const auto pullRequest = std::string{ "pullRequest{id databaseId number repository{id databaseId nameWithOwner}}" };
		const auto fields      = "body viewerCanUpdate viewerDidAuthor updatedAt " + pullRequest;
		auto       edit        = application::review_comment_edit_request{
			.Target         = target().Semantic,
			.ActingIdentity = identity().Account,
			.Kind           = core::comment_edit_kind::review_comment,
			.Subject        = "PRRC_kwDOTyvof86abc",
			.Original       = "Before.\r\n",
			.Body           = "After \"quoted\".\r\n",
			.LiveBinding    = request().LiveBinding,
		};
		struct kind_row
		{
			core::comment_edit_kind   Kind;
			review_mutation_operation Operation;
			std::string_view          Document;
			std::string_view          Typename;
			std::string_view          Payload;
		};
		const auto kinds = std::array{
			kind_row{ core::comment_edit_kind::review_comment, review_mutation_operation::edit_review_comment,
					  "mutation EditReviewComment($subject:ID!,$body:String!){updatePullRequestReviewComment(input:{pullRequestReviewCommentId:$subject,body:$body}){pullRequestReviewComment{",
					  "PullRequestReviewComment", "{\"updatePullRequestReviewComment\":{\"pullRequestReviewComment\":" },
			kind_row{ core::comment_edit_kind::pull_request_comment, review_mutation_operation::edit_issue_comment,
					  "mutation EditIssueComment($subject:ID!,$body:String!){updateIssueComment(input:{id:$subject,body:$body}){issueComment{",
					  "IssueComment", "{\"updateIssueComment\":{\"issueComment\":" },
			kind_row{ core::comment_edit_kind::review_body, review_mutation_operation::edit_review_body,
					  "mutation EditReviewBody($subject:ID!,$body:String!){updatePullRequestReview(input:{pullRequestReviewId:$subject,body:$body}){pullRequestReview{",
					  "PullRequestReview", "{\"updatePullRequestReview\":{\"pullRequestReview\":" },
		};
		for (const auto& kind : kinds) {
			edit.Kind        = kind.Kind;
			const auto write = compile_comment_edit_request(edit);
			expect(write.has_value() && write->Operation == kind.Operation && write->Method == http_method::post && write->Path == "/graphql"
					   && write->Body
						   == "{\"query\":\"" + std::string{ kind.Document } + "__typename id " + fields
							   + "}}}\",\"variables\":{\"subject\":\"PRRC_kwDOTyvof86abc\",\"body\":\"After \\\"quoted\\\".\\r\\n\"}}",
				   "each kind's write is its reserved document naming the comment and the exact body");
			expect(write->Body.find("Before.") == std::string::npos, "the original is never sent");
			expect(validate_outbound_review_mutation_request(*write).has_value(), "each kind's write revalidates canonically");
			const auto ok      = comment_json(kind.Typename, "PRRC_kwDOTyvof86abc", "After \\\"quoted\\\".\\r\\n", true);
			const auto written = parse_comment_edit_response({ .Status = 200, .Body = "{\"data\":" + std::string{ kind.Payload } + ok + "}}}" }, edit);
			expect(written.has_value() && written->Body == edit.Body && written->UpdatedAt == "2026-10-27T12:00:00Z", "an exact answer of the kind yields the body asked");
			const auto stale = parse_comment_edit_response({ .Status = 200, .Body = "{\"data\":" + std::string{ kind.Payload } + comment_json(kind.Typename, "PRRC_kwDOTyvof86abc", "Before.", true) + "}}}" }, edit);
			expect(!stale && stale.error() == review_mutation_contract_failure::identity_mismatch, "an answer with another body is a mismatch, not a success");
			const auto observed = parse_comment_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + comment_json(kind.Typename, "PRRC_kwDOTyvof86abc", "Before.\\r\\n", false) + "}}" }, edit);
			expect(observed.has_value() && observed->Body == "Before.\r\n" && !observed->ViewerCanUpdate, "the comment is observed with its exact body and GitHub's permission");
		}
		edit.Kind          = core::comment_edit_kind::review_comment;
		const auto observe = compile_comment_observation_request(edit);
		expect(observe.has_value() && observe->Operation == review_mutation_operation::observe_comment_for_edit && observe->Path == "/graphql"
				   && observe->Body
					   == "{\"query\":\"query ObserveComment($subject:ID!){node(id:$subject){__typename id ... on PullRequestReviewComment{" + fields + "} ... on IssueComment{" + fields
						   + "} ... on PullRequestReview{" + fields + "}}}\",\"variables\":{\"subject\":\"PRRC_kwDOTyvof86abc\"}}",
			   "the observation is a read document naming only the comment");
		expect(validate_outbound_review_mutation_request(*observe).has_value(), "the observation revalidates canonically");
		auto spoofed      = *observe;
		spoofed.Operation = review_mutation_operation::edit_review_comment;
		auto rewritten    = *compile_comment_edit_request(edit);
		rewritten.Body.replace(rewritten.Body.find("After"), 5U, "Other");
		auto retargeted = *compile_comment_edit_request(edit);
		retargeted.Body.replace(retargeted.Body.find("PRRC_kwDOTyvof86abc"), 19U, "PRRC_kwDOTyvof86zzz");
		auto rekinded      = *compile_comment_edit_request(edit);
		rekinded.Operation = review_mutation_operation::edit_issue_comment;
		expect(!validate_outbound_review_mutation_request(spoofed).has_value() && !validate_outbound_review_mutation_request(rewritten).has_value()
				   && !validate_outbound_review_mutation_request(retargeted).has_value() && !validate_outbound_review_mutation_request(rekinded).has_value(),
			   "a read passed off as the write, a body changed in flight, another comment and another kind's update are refused");

		auto unbound               = edit;
		unbound.LiveBinding        = std::nullopt;
		auto foreign               = edit;
		foreign.Target.PullRequest = core::pull_request_id{ 99U };
		auto nodeless              = edit;
		nodeless.Subject.clear();
		auto quoted          = edit;
		quoted.Subject       = "PRRC\"x";
		auto actor           = edit;
		actor.ActingIdentity = core::github_account_id{ 99U };
		for (const auto& refused : { unbound, foreign, nodeless, quoted, actor }) {
			expect(!compile_comment_edit_request(refused).has_value() && !compile_comment_observation_request(refused).has_value(),
				   "an unbound, foreign, nodeless, malformed or other-actor edit is refused before the wire");
		}
		auto invalid = edit;
		invalid.Body = std::string{ "\xC3\x28" };
		expect(!compile_comment_edit_request(invalid).has_value(), "text that is not UTF-8 is refused");
		auto longest = edit;
		longest.Body = std::string(core::maximum_comment_characters, 'a');
		auto huge    = edit;
		huge.Body    = std::string(maximum_description_request_bytes, 'a');
		expect(compile_comment_edit_request(longest).has_value() && maximum_review_mutation_wire_request_bytes(review_mutation_operation::edit_review_comment) == maximum_description_request_bytes
				   && maximum_review_mutation_wire_response_bytes(review_mutation_operation::observe_comment_for_edit) == maximum_description_response_bytes,
			   "GitHub's 65,536 characters go in one write, and come back in one answer");
		expect(!compile_comment_edit_request(huge).has_value() && compile_comment_observation_request(huge).has_value(),
			   "a body past the request ceiling is refused to write, and can still be observed");
		auto enterprise                               = edit;
		enterprise.LiveBinding->Target.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		enterprise.LiveBinding->Target.Hostname       = application::github_canonical_hostname{ "ghe.example.com" };
		expect(compile_comment_edit_request(enterprise, application::github_server_contract::ghes_3_18_2022_11_28)->Path == "/api/graphql"
				   && compile_comment_observation_request(enterprise, application::github_server_contract::ghes_3_18_2022_11_28)->Path == "/api/graphql"
				   && !compile_comment_edit_request(edit, application::github_server_contract::ghes_3_18_2022_11_28).has_value(),
			   "GHES 3.18 edits at its GraphQL root, and a binding for another server is refused");

		// The answers are held to the comment, its kind and the pull request.
		const auto wrongKind = parse_comment_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + comment_json("IssueComment", "PRRC_kwDOTyvof86abc", "Before.", true) + "}}" }, edit);
		expect(!wrongKind && wrongKind.error() == review_mutation_contract_failure::identity_mismatch, "a node of another kind is a mismatch");
		const auto wrongNode = parse_comment_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + comment_json("PullRequestReviewComment", "PRRC_other", "Before.", true) + "}}" }, edit);
		expect(!wrongNode && wrongNode.error() == review_mutation_contract_failure::identity_mismatch, "another comment is a mismatch");
		const auto outside = parse_comment_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + comment_json("PullRequestReviewComment", "PRRC_kwDOTyvof86abc", "Before.", true, "PR_kwDOTyvof879Gzzz") + "}}" }, edit);
		expect(!outside && outside.error() == review_mutation_contract_failure::identity_mismatch, "a comment in another pull request is a mismatch");
		expect(parse_comment_observation({ .Status = 200, .Body = R"({"data":{"node":null}})" }, edit).error() == review_mutation_contract_failure::target_unavailable,
			   "a comment GitHub no longer shows is unavailable");
		const auto maintained = parse_comment_observation({ .Status = 200, .Body = "{\"data\":{\"node\":" + comment_json("PullRequestReviewComment", "PRRC_kwDOTyvof86abc", "Before.", true, "PR_kwDOTyvof879G6NV", false) + "}}" }, edit);
		expect(maintained.has_value() && !maintained->ViewerCanUpdate, "another account's comment a maintainer may update is held not permitted: only my own is edited");
		expect(parse_comment_observation({ .Status = 502, .Body = "{}" }, edit).error() == review_mutation_contract_failure::invalid_status, "a non-200 answer is an invalid status");
		expect(!parse_comment_edit_response({ .Status = 200, .Body = R"({"data":{"updatePullRequestReviewComment":null},"errors":[{"type":"FORBIDDEN"}]})" }, edit).has_value(),
			   "a GraphQL refusal is not a success");
		auto big         = edit;
		big.Body         = std::string(core::maximum_comment_characters, 'b');
		const auto large = parse_comment_edit_response({ .Status = 200, .Body = "{\"data\":{\"updatePullRequestReviewComment\":{\"pullRequestReviewComment\":" + comment_json("PullRequestReviewComment", "PRRC_kwDOTyvof86abc", big.Body, true) + "}}}" }, big);
		expect(large.has_value() && large->Body.size() == core::maximum_comment_characters, "an answer carrying GitHub's longest body is read");
	}

	[[nodiscard]] auto recorded_answer(const std::string_view aName) -> review_mutation_wire_response
	{
		auto file = std::ifstream{ std::string{ BPR_REACTIONS_FIXTURE_DIR } + "/" + std::string{ aName }, std::ios::binary };
		expect(file.good(), "the recorded answer opens");
		return { .Status = 200, .Body = std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } } };
	}

	// [[BPR-1741]]. GitHub's own answers to the product's documents,
	// recorded on BPR-Test #132 (tests/fixtures/github/reactions-v1), read by
	// the product's parsers as they read them live.
	void test_recorded_reaction_and_resolution_answers()
	{
		auto bound                     = target();
		bound.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" };
		bound.Number                   = core::pull_request_number{ 132U };
		bound.RepositoryDatabaseId     = 1'328'277'631U;
		bound.RepositoryNodeId         = "R_kgDOTyvofw";
		bound.PullRequestDatabaseId    = 4'974'995'320U;
		bound.PullRequestNodeId        = "PR_kwDOTyvof88AAAABKIhneA";
		bound.BaseRepositoryDatabaseId = 1'328'277'631U;
		auto live                      = request().LiveBinding;
		live->Target                   = bound;
		const auto reaction            = [&](const std::string& aSubject, const core::reaction_content aContent, const bool aAdd) {
			return application::review_reaction_request{ .Target = bound.Semantic, .ActingIdentity = identity().Account, .Subject = aSubject, .Content = aContent, .Add = aAdd, .LiveBinding = live };
		};
		const auto comment  = std::string{ "PRRC_kwDOTyvof88AAAABA4f-_w" };
		const auto observed = parse_reaction_subject_observation(recorded_answer("observe-review-comment.json"), reaction(comment, core::reaction_content::heart, true));
		expect(observed.has_value() && observed->Reactions.ViewerCanReact && observed->Reactions.Groups.size() == 1U
				   && observed->Reactions.Groups[0].Content == core::reaction_content::thumbs_up && observed->Reactions.Groups[0].Count == 1U
				   && !observed->Reactions.Groups[0].ViewerReacted && observed->Reactions.Groups[0].Reactors == std::vector<std::string>{ "MikeMarcinTest" },
			   "the recorded observation is the second identity's thumbs up, not mine");
		const auto heart = parse_reaction_response(recorded_answer("add-heart.json"), reaction(comment, core::reaction_content::heart, true));
		expect(heart.has_value() && heart->Reactions.Groups.size() == 2U && heart->Reactions.Groups[1].Content == core::reaction_content::heart
				   && heart->Reactions.Groups[1].ViewerReacted,
			   "the recorded add answer has my heart beside the thumbs up");
		const auto thumbs = parse_reaction_response(recorded_answer("add-thumbs-up.json"), reaction(comment, core::reaction_content::thumbs_up, true));
		expect(thumbs.has_value() && thumbs->Reactions.Groups[0].Count == 2U && thumbs->Reactions.Groups[0].ViewerReacted
				   && thumbs->Reactions.Groups[0].Reactors == std::vector<std::string>{ "MikeMarcinTest", "MikeMarcin" },
			   "two reactors on one reaction, mine among them, both named");
		const auto removed = parse_reaction_response(recorded_answer("remove-heart.json"), reaction(comment, core::reaction_content::heart, false));
		expect(removed.has_value() && core::find_reaction_group(removed->Reactions, core::reaction_content::heart) == nullptr, "the recorded remove answer has no heart left");
		expect(!parse_reaction_response(recorded_answer("remove-heart.json"), reaction(comment, core::reaction_content::heart, true)).has_value(),
			   "the remove answer is not an add's");
		const auto eyes = parse_reaction_response(recorded_answer("add-eyes-to-review.json"), reaction("PRR_kwDOTyvof88AAAABTxO_vw", core::reaction_content::eyes, true));
		expect(eyes.has_value() && eyes->Reactions.Groups.size() == 1U && eyes->Reactions.Groups[0].Content == core::reaction_content::eyes, "a review takes a reaction as a comment does");
		auto other         = reaction(comment, core::reaction_content::heart, true);
		other.LiveBinding  = request().LiveBinding;
		other.Target       = request().Target;
		const auto foreign = parse_reaction_response(recorded_answer("add-heart.json"), other);
		expect(!foreign && foreign.error() == review_mutation_contract_failure::identity_mismatch, "the recorded answer is refused for another pull request");

		const auto resolution = [&](const bool aResolve) {
			return application::review_thread_resolution_request{ .Target = bound.Semantic, .ActingIdentity = identity().Account, .Thread = "PRRT_kwDOTyvof86vZ2dW", .Resolve = aResolve, .LiveBinding = live };
		};
		const auto resolved = parse_thread_resolution_response(recorded_answer("resolve-thread.json"), resolution(true));
		const auto reopened = parse_thread_resolution_response(recorded_answer("unresolve-thread.json"), resolution(false));
		expect(resolved.has_value() && resolved->Receipt.Resolved && reopened.has_value() && !reopened->Receipt.Resolved,
			   "the recorded resolve and unresolve answers name the thread in the state asked");
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
			// Named: GCC 15.2 misreads a header list temporary inside a && chain as dangling (BPR-0816).
			const auto reviewHeaders = std::vector<http_header>{ { .Name = "Accept", .Value = "application/vnd.github+json" }, { .Name = "User-Agent", .Value = "BetterPullReview/0.1" }, { .Name = "X-GitHub-Api-Version", .Value = "2022-11-28" }, { .Name = "Content-Type", .Value = "application/json" } };
			expect(compiled->Method == http_method::post
					   && compiled->Operation == review_mutation_operation::create_review
					   && compiled->Path == "/repos/MikeMarcin/BPR-Test/pulls/1/reviews"
					   && compiled->Headers == reviewHeaders
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

		// [[BPR-1745]]. A line count is a 32-bit value, so the last
		// index a document can have is one below the 32-bit maximum; that
		// maximum is `core::thread_reply_line`, the line of a reply bound to
		// its thread, which no new comment takes.
		const auto maximumSemantic
			= request_at_line(std::numeric_limits<std::uint32_t>::max() - 1U);
		const auto maximumCompiled = compile_review_comment_request(maximumSemantic);
		expect(maximumCompiled.has_value()
				   && maximumCompiled->Body.find("\"line\":4294967295,")
					   != std::string::npos,
			   "the maximum semantic line translates without overflow");
		expect(parse_review_comment_response(
				   { .Status = 201, .Body = comment_body(4'294'967'295U) },
				   maximumSemantic)
				   .has_value(),
			   "the maximum representable semantic line round-trips");
		expect(!parse_review_comment_response(
					{ .Status = 201, .Body = comment_body(4'294'967'297U) },
					maximumSemantic)
					.has_value(),
			   "a REST line beyond the semantic range is refused");
		expect(!compile_review_comment_request(request_at_line(core::thread_reply_line)).has_value(),
			   "a new comment never takes the thread-bound reply's line");
	}

	[[nodiscard]] auto replace_wire_field(std::string aBody, const std::string_view aFrom, const std::string_view aTo) -> std::string
	{
		const auto position = aBody.find(aFrom);
		expect(position != std::string::npos, "raw JSON fixture contains selected field");
		aBody.replace(position, aFrom.size(), aTo);
		return aBody;
	}

	// [[BPR-0896]]. A reply's own path is the GraphQL root, so no REST
	// collection can be derived from it: its reconciliation observes the
	// thread it landed in. The document is spelled here in full so a changed
	// selection is a changed row.
	constexpr auto observe_thread_comments_document = std::string_view{
		"query ObserveThreadComments($thread:ID!){node(id:$thread){... on PullRequestReviewThread{"
		"id diffSide pullRequest{id databaseId number repository{id databaseId nameWithOwner}} "
		"comments(first:100){pageInfo{hasNextPage} nodes{id fullDatabaseId body path line commit{oid} "
		"viewerDidAuthor author{login ... on User{databaseId id}}}}}}}"
	};

	[[nodiscard]] auto reply_request() -> application::review_mutation_request
	{
		auto reply                   = request();
		reply.Proposal.ReplyToThread = "PRRT_fixture_1";
		reply.IdempotencyKey         = application::idempotency_key_for(reply.Proposal, reply.Target);
		return reply;
	}

	[[nodiscard]] auto head_commit_json() -> std::string
	{
		return "{\"oid\":\"" + std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() } + "\"}";
	}

	[[nodiscard]] auto our_author_json() -> std::string
	{
		return R"({"login":"fixture-reviewer","databaseId":22,"id":"MDQ6VXNlcjIy"})";
	}

	// One comment of the observed thread. The scalar arguments are raw JSON
	// tokens, so a row can spell `null`, a number or a string as it needs.
	[[nodiscard]] auto thread_comment_json(
		const std::string_view aId,
		const std::string_view aFullDatabaseId,
		const std::string_view aBody,
		const std::string_view aLine,
		const std::string_view aCommit,
		const bool             aViewerDidAuthor,
		const std::string_view aAuthor) -> std::string
	{
		return "{\"id\":\"" + std::string{ aId } + "\",\"fullDatabaseId\":" + std::string{ aFullDatabaseId }
		+ ",\"body\":\"" + std::string{ aBody } + "\",\"path\":\"fixture/review.txt\",\"line\":" + std::string{ aLine }
		+ ",\"commit\":" + std::string{ aCommit } + ",\"viewerDidAuthor\":" + (aViewerDidAuthor ? "true" : "false")
			+ ",\"author\":" + std::string{ aAuthor } + "}";
	}

	[[nodiscard]] auto exact_reply_comment_json() -> std::string
	{
		return thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "13", head_commit_json(), true, our_author_json());
	}

	[[nodiscard]] auto root_comment_json() -> std::string
	{
		return thread_comment_json("PRRC_fixture_40", "\"40\"", "Root comment.", "13", head_commit_json(), false,
								   R"({"login":"someone-else","databaseId":23,"id":"MDQ6VXNlcjIz"})");
	}

	// Ours, and outdated: a null line and a null commit.
	[[nodiscard]] auto outdated_reply_comment_json() -> std::string
	{
		return thread_comment_json("PRRC_fixture_42", "\"42\"", "Please rename this.", "null", "null", true, our_author_json());
	}

	// A Bot satisfies no `User` fragment: a login and nothing else.
	[[nodiscard]] auto bot_comment_json() -> std::string
	{
		return thread_comment_json("PRRC_fixture_43", "\"43\"", "Please rename this.", "13", head_commit_json(), false, R"({"login":"bot"})");
	}

	[[nodiscard]] auto thread_comments_body(
		const std::string_view          aThread,
		const std::string_view          aDiffSide,
		const bool                      aHasNextPage,
		const std::string&              aPullRequest,
		const std::vector<std::string>& aComments) -> std::string
	{
		auto nodes = std::string{ };
		for (const auto& comment : aComments) {
			if (!nodes.empty()) {
				nodes.push_back(',');
			}
			nodes += comment;
		}
		return "{\"data\":{\"node\":{\"id\":\"" + std::string{ aThread } + "\",\"diffSide\":\"" + std::string{ aDiffSide } + "\","
			+ aPullRequest + ",\"comments\":{\"pageInfo\":{\"hasNextPage\":" + (aHasNextPage ? "true" : "false")
			+ "},\"nodes\":[" + nodes + "]}}}}";
	}

	[[nodiscard]] auto designated_thread_body(const std::vector<std::string>& aComments) -> std::string
	{
		return thread_comments_body("PRRT_fixture_1", "RIGHT", false, designated_pull_request_json(), aComments);
	}

	void test_a_reply_is_reconciled_through_its_thread()
	{
		const auto reply       = reply_request();
		const auto observation = compile_review_comment_observation_request(reply);
		expect(observation.has_value()
				   && observation->Operation == review_mutation_operation::observe_review_thread_comments
				   && observation->Method == http_method::post && observation->Path == "/graphql"
				   && observation->Headers.size() == 4U
				   && observation->Body
					   == "{\"query\":\"" + std::string{ observe_thread_comments_document }
						   + "\",\"variables\":{\"thread\":\"PRRT_fixture_1\"}}",
			   "a reply's reconciliation is the ObserveThreadComments document at the GraphQL root with the exact thread");
		expect(validate_outbound_review_mutation_request(*observation).has_value(),
			   "the thread comments observation revalidates canonically");
		expect(compile_review_comment_observation_request(
				   reply, application::github_server_contract::ghes_3_18_2022_11_28)
					   ->Path
				   == "/api/graphql",
			   "GHES observes the thread's comments at the exact GraphQL root");
		const auto root = compile_review_comment_observation_request(request());
		expect(root.has_value() && root->Operation == review_mutation_operation::observe_review_comments
				   && root->Method == http_method::get
				   && root->Path == "/repos/MikeMarcin/BPR-Test/pulls/1/comments?per_page=100"
				   && root->Headers.size() == 3U && root->Body.empty(),
			   "a root comment's reconciliation is still the REST comment list");
		auto restDerived      = *observation;
		restDerived.Operation = review_mutation_operation::observe_review_comments;
		restDerived.Method    = http_method::get;
		restDerived.Path      = "/graphql?per_page=100";
		restDerived.Headers   = root->Headers;
		restDerived.Body.clear();
		auto asRest            = *observation;
		asRest.Operation       = review_mutation_operation::observe_review_comments;
		auto rootAsThread      = *root;
		rootAsThread.Operation = review_mutation_operation::observe_review_thread_comments;
		auto otherThread       = *observation;
		otherThread.Body       = replace_wire_field(otherThread.Body, "PRRT_fixture_1", "PRRT_fixture_2");
		expect(!validate_outbound_review_mutation_request(restDerived).has_value()
				   && !validate_outbound_review_mutation_request(asRest).has_value()
				   && !validate_outbound_review_mutation_request(rootAsThread).has_value()
				   && !validate_outbound_review_mutation_request(otherThread).has_value(),
			   "the REST-derived GET /graphql?per_page=100, a relabelled operation on either side and another thread's variable all fail canonical validation");
		// The read transport refuses the name as it refuses ObserveThread: as
		// an operation it does not know.
		expect(!read_operation_from_name("ObserveThreadComments").has_value()
				   && read_operation_from_name("ObserveThreadComments").error().Code
					   == contract_failure_code::unsupported_operation
				   && !read_operation_from_name("ObserveThread").has_value(),
			   "ObserveThreadComments is no read operation, exactly as ObserveThread is none");

		const auto parsed = parse_review_comment_observations(
			{ .Status = 200,
			  .Body   = designated_thread_body({ exact_reply_comment_json(), root_comment_json(), outdated_reply_comment_json(), bot_comment_json() }) },
			reply);
		expect(parsed.has_value() && parsed->Projection.size() == 4U
				   && parsed->Projection[0].ServerId == "PRRC_fixture_40" && parsed->Projection[0].Body == "Root comment."
				   && parsed->Projection[1].ServerId == "PRRC_fixture_42"
				   && parsed->Projection[2].ServerId == "PRRC_fixture_43"
				   && parsed->Projection[3].ServerId == "PRRC_reply_45" && parsed->Projection[3].Body == "Please rename this."
				   && parsed->ExactReceipts.size() == 1U
				   && parsed->ExactReceipts.front().ServerCommentId == "PRRC_reply_45"
				   && !parsed->ExactReceipts.front().Duplicate,
			   "every comment of the thread is projected in server-identity order and only our live reply is an exact receipt: the root by another user, our outdated one and the bot's are tolerated non-matches");
		const auto nonMatches = std::array<std::pair<std::string, std::string_view>, 8>{
			std::pair{ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "13", head_commit_json(), false, our_author_json()), "viewerDidAuthor false" },
			{ thread_comment_json("PRRC_reply_45", "\"45\"", "Something else.", "13", head_commit_json(), true, our_author_json()), "another body" },
			{ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "14", head_commit_json(), true, our_author_json()), "another line" },
			{ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "13", "{\"oid\":\"2222222222222222222222222222222222222222\"}", true, our_author_json()), "another commit" },
			{ thread_comment_json("PRRC_reply_45", "\"0\"", "Please rename this.", "13", head_commit_json(), true, our_author_json()), "a zero fullDatabaseId" },
			{ thread_comment_json("PRRC_reply_45", "null", "Please rename this.", "13", head_commit_json(), true, our_author_json()), "a null fullDatabaseId" },
			{ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "13", head_commit_json(), true, "null"), "a null author" },
			{ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "13", head_commit_json(), true, R"({"login":"fixture-reviewer","databaseId":23,"id":"MDQ6VXNlcjIy"})"), "another author identity" },
		};
		for (const auto& [comment, why] : nonMatches) {
			const auto tolerated = parse_review_comment_observations(
				{ .Status = 200, .Body = designated_thread_body({ root_comment_json(), comment }) }, reply);
			expect(tolerated.has_value() && tolerated->Projection.size() == 2U && tolerated->ExactReceipts.empty(),
				   "a comment that is not ours is projected and is no exact receipt: " + std::string{ why });
		}
		const auto leftThread = parse_review_comment_observations(
			{ .Status = 200,
			  .Body   = thread_comments_body("PRRT_fixture_1", "LEFT", false, designated_pull_request_json(), { exact_reply_comment_json() }) },
			reply);
		expect(leftThread.has_value() && leftThread->Projection.size() == 1U && leftThread->ExactReceipts.empty(),
			   "the side is the thread's diffSide, so our comment in a LEFT thread is no exact receipt for a RIGHT anchor");

		const auto invisible = parse_review_comment_observations(
			{ .Status = 200, .Body = R"({"data":{"node":null}})" }, reply);
		expect(!invisible && invisible.error() == review_mutation_contract_failure::target_unavailable,
			   "a reply cannot reconcile an invisible thread and preserves the target diagnostic");

		const auto exact     = exact_reply_comment_json();
		const auto malformed = std::array<std::pair<std::string, std::string_view>, 21>{
			std::pair{ std::string{ "{" }, "undecodable JSON" },
			{ "{\"data\":null}", "a GraphQL error envelope" },
			{ "{\"data\":{}}", "a missing node" },
			{ "{\"data\":{\"node\":{}}}", "a node that is not a review thread" },
			{ thread_comments_body("PRRT_fixture_1", "UP", false, designated_pull_request_json(), { exact }), "a diffSide outside LEFT and RIGHT" },
			{ replace_wire_field(designated_thread_body({ exact }), "\"diffSide\"", "\"side\""), "a missing diffSide" },
			{ designated_thread_body({ replace_wire_field(exact, "\"id\":\"PRRC_reply_45\"", "\"nodeId\":\"PRRC_reply_45\"") }), "a missing comment id" },
			{ designated_thread_body({ replace_wire_field(exact, "\"body\"", "\"text\"") }), "a missing body" },
			{ designated_thread_body({ replace_wire_field(exact, "\"path\"", "\"file\"") }), "a missing path" },
			{ designated_thread_body({ replace_wire_field(exact, "\"viewerDidAuthor\"", "\"mine\"") }), "a missing viewerDidAuthor" },
			{ designated_thread_body({ replace_wire_field(exact, "\"id\":\"PRRC_reply_45\"", "\"id\":\"\"") }), "an empty id" },
			{ designated_thread_body({ replace_wire_field(exact, "\"id\":\"PRRC_reply_45\"", "\"id\":\"" + std::string(257U, 'x') + "\"") }), "an unbounded id" },
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "\"abc\"", "Please rename this.", "13", head_commit_json(), true, our_author_json()) }), "a fullDatabaseId that is no integer" },
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "\"18446744073709551616\"", "Please rename this.", "13", head_commit_json(), true, our_author_json()) }), "an overflowing fullDatabaseId" },
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "\"045\"", "Please rename this.", "13", head_commit_json(), true, our_author_json()) }), "a non-canonical fullDatabaseId" },
			// BigInt is string-encoded on the wire because it may exceed a
			// 32-bit Int; a bare number is the spelling of `databaseId: Int`,
			// and is refused rather than mistaken for the full identity.
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "45", "Please rename this.", "13", head_commit_json(), true, our_author_json()) }), "a fullDatabaseId spelled as a JSON number" },
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "13", "{\"oid\":\"invalid\"}", true, our_author_json()) }), "a commit that is no oid" },
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "0", head_commit_json(), true, our_author_json()) }), "a zero line" },
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "4294967297", head_commit_json(), true, our_author_json()) }), "a line beyond the semantic range" },
			{ designated_thread_body({ thread_comment_json("PRRC_reply_45", "\"45\"", "Please rename this.", "13.5", head_commit_json(), true, our_author_json()) }), "a fractional line" },
			{ designated_thread_body({ replace_wire_field(exact, "Please rename this.", "\xC3\x28") }), "a non-UTF-8 body" },
		};
		for (const auto& [body, why] : malformed) {
			const auto refused = parse_review_comment_observations({ .Status = 200, .Body = body }, reply);
			expect(!refused.has_value() && refused.error() == review_mutation_contract_failure::malformed_response,
				   "a structurally invalid thread answer is malformed: " + std::string{ why });
		}
		auto crowded = std::vector<std::string>{ };
		for (auto index = 0; index < 101; ++index) {
			crowded.push_back(thread_comment_json("PRRC_fixture_" + std::to_string(100 + index), "\"" + std::to_string(100 + index) + "\"",
												  "Please rename this.", "13", head_commit_json(), true, our_author_json()));
		}
		const auto overflowing = parse_review_comment_observations({ .Status = 200, .Body = designated_thread_body(crowded) }, reply);
		expect(!overflowing.has_value() && overflowing.error() == review_mutation_contract_failure::malformed_response,
			   "more than 100 comments are refused without truncation");
		const auto paged = parse_review_comment_observations(
			{ .Status = 200,
			  .Body   = thread_comments_body("PRRT_fixture_1", "RIGHT", true, designated_pull_request_json(), { exact }) },
			reply);
		expect(!paged.has_value() && paged.error() == review_mutation_contract_failure::invalid_status,
			   "a thread with a further page is refused as a paginated REST list is");
		const auto echoed = parse_review_comment_observations(
			{ .Status = 200,
			  .Body   = thread_comments_body("PRRT_fixture_2", "RIGHT", false, designated_pull_request_json(), { exact }) },
			reply);
		const auto foreign = parse_review_comment_observations(
			{ .Status = 200,
			  .Body   = thread_comments_body("PRRT_fixture_1", "RIGHT", false, foreign_pull_request_json(), { exact }) },
			reply);
		const auto repeated = parse_review_comment_observations(
			{ .Status = 200, .Body = designated_thread_body({ exact, exact }) }, reply);
		expect(!echoed.has_value() && echoed.error() == review_mutation_contract_failure::identity_mismatch
				   && !foreign.has_value() && foreign.error() == review_mutation_contract_failure::identity_mismatch
				   && !repeated.has_value() && repeated.error() == review_mutation_contract_failure::identity_mismatch,
			   "an answer about another thread, a thread in another pull request, or a repeated comment identity is an identity mismatch");
		auto unbound = reply;
		unbound.LiveBinding.reset();
		const auto created = parse_review_comment_observations({ .Status = 201, .Body = designated_thread_body({ exact }) }, reply);
		expect(!created.has_value() && created.error() == review_mutation_contract_failure::invalid_status
				   && !parse_review_comment_observations({ .Status = 200, .Body = designated_thread_body({ exact }) }, unbound).has_value()
				   && !parse_review_comment_observations({ .Status = 200, .Body = designated_thread_body({ exact }) }, request()).has_value(),
			   "a non-200 answer, an unbound expectation and a thread answer offered to a root comment are all refused");
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
								  "{\"extra\":\"\xC3\x28\"," + comment_body().substr(1U),
								  replace_wire_field(comment_body(), "\"user\":{", "\"unknown_user\":{") }) {
			expect(!parse_review_comment_response({ .Status = 201, .Body = body }, request()), "legacy comment rejects missing, null, duplicate, trailing and malformed Unicode fields");
			expect(!parse_review_comment_observations({ .Status = 200, .Body = "[" + body + "]" }, request()), "comment lists retain the same full-input validation");
		}
		// RFC-BPR-0089 M4. An outdated comment (`line` null) is never a head
		// comment's receipt, but it is anyone's well-formed comment: a list that
		// holds one still reconciles, as `parse_review_comment_collection` reads
		// it. A comment written on an earlier commit is outdated from the start.
		const auto outdated = replace_wire_field(comment_body(), "\"line\":13", "\"line\":null");
		expect(!parse_review_comment_response({ .Status = 201, .Body = outdated }, request()), "an outdated answer is not a head comment's receipt");
		const auto listed = parse_review_comment_observations({ .Status = 200, .Body = "[" + outdated + "]" }, request());
		expect(listed && listed->ExactReceipts.empty() && listed->Projection.size() == 1U, "a list holding an outdated comment reconciles without it as a receipt");
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
	/// [[BPR-1560]]. The publication's merge-base observation: one GET of
	/// compare on the page with no file list, whose answer must echo the
	/// target's head and name a full merge base of its format.
	void test_merge_base_observation_contract()
	{
		const auto wire = compile_merge_base_observation_request(target(), application::github_server_contract::github_com_2022_11_28);
		expect(wire && wire->Method == http_method::get && wire->Operation == review_mutation_operation::observe_merge_base && validate_outbound_review_mutation_request(*wire),
			   "the merge-base observation is a canonical GET");
		const auto head     = std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() };
		const auto answered = [&](const std::string& aUrlHead, const std::string& aMergeBase) {
			return parse_merge_base_observation(
				{ .Status = 200, .Headers = { { .Name = "Content-Type", .Value = "application/json" } }, .Body = "{\"url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/compare/main..." + aUrlHead + "\",\"base_commit\":{\"sha\":\"" + std::string(40U, 'b') + "\"},\"merge_base_commit\":{\"sha\":\"" + aMergeBase + "\"},\"files\":[]}" },
				target());
		};
		const auto named = answered(head, std::string(40U, 'c'));
		expect(named && core::canonical_hex(named->MergeBase).view() == std::string(40U, 'c') && core::canonical_hex(named->BaseCommit).view() == std::string(40U, 'b') && !named->AheadBy,
			   "the merge base and the tip it was computed against are read from the answer");
		expect(!answered(std::string(40U, 'd'), std::string(40U, 'c')), "an answer about another head is refused");
		expect(!answered(head, "cccc"), "an abbreviated merge base is refused");
		expect(!answered(head, std::string(64U, 'c')), "a merge base of another format is refused");

		// [[BPR-1572]]. The recorded base against the head, and against the
		// base branch tip the first answer resolved (exact commits, through the
		// ancestry binding): each answer must name the recorded base as its
		// base commit, echo the head it was asked about, and carry a
		// non-negative ahead_by if any.
		const auto recordedBase = std::string{ core::canonical_hex(target().BaseOid).view() };
		const auto tip          = commit(0x7cU);
		for (const auto basis : { merge_base_observation_basis::recorded_base, merge_base_observation_basis::base_ancestry }) {
			const auto binding = basis == merge_base_observation_basis::recorded_base ? target() : merge_base_ancestry_binding(target(), tip);
			const auto asked   = compile_merge_base_observation_request(binding, application::github_server_contract::github_com_2022_11_28, basis);
			const auto echoed  = std::string{ core::canonical_hex(binding.Semantic.ExpectedHead).view() };
			auto       suffix  = std::string{ "/compare/" };
			suffix.append(recordedBase).append("...").append(echoed).append("?page=2&per_page=1");
			expect(asked && asked->Method == http_method::get && validate_outbound_review_mutation_request(*asked)
					   && asked->Path.ends_with(suffix)
					   && asked->Operation
						   == (basis == merge_base_observation_basis::recorded_base ? review_mutation_operation::observe_recorded_merge_base : review_mutation_operation::observe_base_ancestry),
				   "each observation is its own canonical GET of exact commits");
			auto relabelled      = *asked;
			relabelled.Operation = review_mutation_operation::observe_merge_base;
			expect(!validate_outbound_review_mutation_request(relabelled), "a pair cannot travel under another observation's name");
			const auto reply = [&](const std::string& aUrlHead, const std::string& aBase, const std::string_view aAhead = "") {
				auto body = std::string{ "{\"url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/compare/" };
				body.append(recordedBase).append("...").append(aUrlHead).append("\",").append(aAhead).append("\"base_commit\":{\"sha\":\"").append(aBase);
				body.append("\"},\"merge_base_commit\":{\"sha\":\"").append(40U, 'c').append("\"},\"files\":[]}");
				return parse_merge_base_observation({ .Status = 200, .Headers = { { .Name = "Content-Type", .Value = "application/json" } }, .Body = std::move(body) }, binding, basis);
			};
			expect(reply(echoed, recordedBase).has_value(), "the answer about the recorded base is read");
			const auto counted = reply(echoed, recordedBase, R"("ahead_by":3,)");
			expect(counted && counted->AheadBy == std::optional<std::uint64_t>{ 3U }, "ahead_by is read");
			expect(!reply(echoed, recordedBase, R"("ahead_by":-1,)"), "a negative ahead_by is refused");
			expect(!reply(echoed, recordedBase, R"("ahead_by":"3",)"), "a non-numeric ahead_by is refused");
			expect(!reply(echoed, std::string(40U, 'b')), "an answer about another base commit is refused");
			expect(!reply(std::string(40U, 'd'), recordedBase), "an answer about another head is refused");
		}
		expect(!parse_merge_base_observation({ .Status = 200, .Headers = { { .Name = "Content-Type", .Value = "application/json" } }, .Body = "{\"url\":\"https://api.github.com/repos/MikeMarcin/BPR-Test/compare/" + recordedBase + "...main\",\"base_commit\":{\"sha\":\"" + recordedBase + "\"},\"merge_base_commit\":{\"sha\":\"" + recordedBase + "\"}}" },
											 merge_base_ancestry_binding(target(), tip), merge_base_observation_basis::base_ancestry),
			   "an ancestry answer about the branch by name, not the tip resolved, is refused");

		// Whether LEFT lines are still numbered in the comparison base.
		auto       stacked        = target();
		const auto mergeBase      = commit(0x5aU);
		stacked.ComparisonBaseOid = mergeBase;
		const auto other          = commit(0x6bU);
		const auto observed       = [](const core::git_commit_oid& aMergeBase, const core::git_commit_oid& aBase, const std::optional<std::uint64_t> aAhead = std::nullopt) {
			return merge_base_observation{ .MergeBase = aMergeBase, .BaseCommit = aBase, .AheadBy = aAhead };
		};
		const auto branchSame  = observed(mergeBase, tip);
		const auto branchOther = observed(other, tip);
		const auto recorded    = observed(mergeBase, stacked.BaseOid, 2U);
		const auto ancestor    = observed(stacked.BaseOid, stacked.BaseOid);
		expect(left_lines_numbered_in_comparison_base(stacked, branchSame, std::nullopt, std::nullopt, std::nullopt), "the base branch naming the comparison base settles it");
		expect(left_lines_numbered_in_comparison_base(stacked, branchOther, recorded, ancestor, 2U),
			   "a base branch that only moved forward from the recorded base keeps GitHub numbering in the recorded base's merge base");
		expect(!left_lines_numbered_in_comparison_base(stacked, branchOther, recorded, observed(other, stacked.BaseOid), 2U),
			   "a base branch that no longer contains the recorded base moves the lines");
		expect(!left_lines_numbered_in_comparison_base(stacked, branchOther, observed(other, stacked.BaseOid, 2U), ancestor, 2U),
			   "a recorded base's merge base other than the comparison base moves the lines");
		expect(!left_lines_numbered_in_comparison_base(stacked, branchOther, recorded, ancestor, 3U), "a commit count other than the recorded base's moves the lines");
		expect(!left_lines_numbered_in_comparison_base(stacked, branchOther, recorded, ancestor, std::nullopt), "no commit count from GitHub moves the lines");
		expect(!left_lines_numbered_in_comparison_base(stacked, branchOther, observed(mergeBase, stacked.BaseOid), ancestor, 2U), "no ahead_by moves the lines");
		expect(!left_lines_numbered_in_comparison_base(stacked, branchOther, std::nullopt, std::nullopt, 2U), "no answer for the recorded base moves the lines");
	}
	void test_staged_batch_contract()
	{
		using op         = application::staged_review_operation;
		auto       input = staged_request(op::create_pending);
		const auto wire  = compile_staged_review_request(input);
		expect(wire && wire->Body == R"({"body":"Raw **tour** `$(literal)`","commit_id":"1111111111111111111111111111111111111111","comments":[{"path":"fixture/review.txt","body":"First stop","line":13,"side":"RIGHT","start_line":11,"start_side":"RIGHT"}]})", "pending batch exact literal bytes with no event, position, subject_type or per-comment review id");
		auto tampered = *wire;
		tampered.Body.insert(1, "\"event\":\"APPROVE\",");
		expect(!validate_outbound_review_mutation_request(tampered), "batch cannot acquire a submission event");
		input.Lines[0].Source.FirstLine = input.Lines[0].Source.LastLine;
		auto single                     = compile_staged_review_request(input);
		expect(single && single->Body.find("start_line") == std::string::npos, "single source line omits optional range fields");
		input.Lines[0].Source.Side = core::diff_side::left;
		expect(!compile_staged_review_request(input), "left subject cannot silently borrow right head identity");
		input.Lines[0].Source.Commit = target().BaseOid;
		expect(compile_staged_review_request(input).has_value(), "left source binds exact base while batch commit remains head");
		// [[BPR-1560]]. With a merge base apart from the recorded base, a LEFT
		// line is numbered in the merge base and only there.
		auto mergeBased                             = input;
		mergeBased.Binding.Target.ComparisonBaseOid = core::git_commit_oid{ core::git_hash_algorithm::sha1, std::array<std::uint8_t, 32>{ 0x5aU } };
		expect(!compile_staged_review_request(mergeBased), "a LEFT line on the recorded base is refused when GitHub numbers LEFT in the merge base");
		mergeBased.Lines[0].Source.Commit = mergeBased.Binding.Target.ComparisonBaseOid;
		expect(compile_staged_review_request(mergeBased).has_value(), "a LEFT line on the merge base binds while the batch commit remains head");
		input.Lines.resize(101, input.Lines.front());
		expect(!compile_staged_review_request(input), "batch never silently partitions provider comment bound");
		input                    = staged_request(op::create_pending);
		input.Lines.front().Body = std::string(maximum_review_mutation_body_bytes, 'x');
		expect(!compile_staged_review_request(input), "serialized overhead counts toward body bound");
		for (auto operation : { op::update_comment, op::update_summary, op::submit_comment, op::discard_pending, op::update_pending_comment }) {
			auto request    = staged_request(operation);
			request.Comment = 0;
			request.Review  = application::review_database_id{ };
			expect(!compile_staged_review_request(request), "object operation refuses missing exact identity");
		}
		const auto submit = compile_staged_review_request(staged_request(op::submit_comment));
		expect(submit && submit->Body == R"({"body":"Raw **tour** `$(literal)`","event":"COMMENT"})", "prepared review only submits COMMENT by exact returned review ID");
	}
	// [[BPR-1367]]. The pull request's review comment collection is read
	// page by page, at most `maximum_review_comment_collection_pages`, and
	// other reviewers' comments in it are structure only.
	/// [[BPR-1538]]. Pages as GitHub and GHES send them: complete comments,
	/// 100 to a page, the next page named by the repository's number.
	void test_real_collection_pages()
	{
		namespace shapes          = bpr::tests::github_shapes;
		auto enterprise           = target();
		enterprise.Hostname       = application::github_canonical_hostname{ "ghe.example.invalid" };
		enterprise.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		const auto author         = shapes::user{ .Id = 23U, .NodeId = "MDQ6VXNlcjIz", .Login = "another-reviewer" };
		const auto head           = std::string{ core::canonical_hex(target().Semantic.ExpectedHead).view() };
		for (const auto& [selected, server] : { std::pair{ target(), shapes::github_com() }, std::pair{ enterprise, shapes::enterprise("ghe.example.invalid") } }) {
			const auto pull  = shapes::pull{ .Server = server, .Owner = "MikeMarcin", .Name = "BPR-Test", .RepositoryId = selected.RepositoryDatabaseId, .Number = 1U };
			const auto label = std::string{ selected.Hostname.value() } + ": ";
			auto       body  = std::string{ "[" };
			for (auto index = 0U; index < 100U; ++index) {
				auto value           = shapes::comment{ };
				value.Id             = 1'000U + index;
				value.Review         = 500U;
				value.Path           = "src/many.txt";
				value.Body           = "An earlier comment on this line.";
				value.Author         = author;
				value.Commit         = head;
				value.OriginalCommit = head;
				value.Line           = index + 1U;
				value.OriginalLine   = index + 1U;
				if (index != 0U) {
					body += ',';
				}
				body += shapes::comment_json(pull, value);
			}
			body += ']';
			expect(body.size() > maximum_review_mutation_response_bytes, label + "a page of 100 complete comments is over the ordinary bound");
			auto       response = review_mutation_wire_response{ .Status = 200, .Headers = { { .Name = "Link", .Value = "<" + pull.comments_page(2U) + ">; rel=\"next\", <" + pull.comments_page(2U) + ">; rel=\"last\"" } }, .Body = body };
			const auto page     = parse_review_comment_collection(response, selected, 1U);
			expect(page && page->Comments.size() == 100U && page->HasNext, label + "a full page is read and its next page, named by the repository's number, is owed");
			expect(page && page->Comments.back().OriginalCommitId == head && page->Comments.back().OriginalLine == std::optional{ 99U } && !page->Comments.back().OriginalStartLine,
				   label + "where each comment was written is kept");
			response.Headers = { { .Name = "Link", .Value = "<" + pull.comments_page(1U) + ">; rel=\"prev\", <" + pull.comments_page(1U) + ">; rel=\"first\"" } };
			expect(parse_review_comment_collection(response, selected, 2U).has_value(), label + "the last page's links back are read");
			auto other         = pull;
			other.RepositoryId = 999U;
			response.Headers   = { { .Name = "Link", .Value = "<" + other.comments_page(2U) + ">; rel=\"next\"" } };
			expect(!parse_review_comment_collection(response, selected, 1U), label + "another repository's number is never followed");
			auto elsewhere   = pull;
			elsewhere.Server = shapes::enterprise("evil.invalid");
			response.Headers = { { .Name = "Link", .Value = "<" + elsewhere.comments_page(2U) + ">; rel=\"next\"" } };
			expect(!parse_review_comment_collection(response, selected, 1U), label + "nor this repository's number on another origin");
			response.Headers.clear();
			response.Body = std::string(maximum_review_mutation_list_response_bytes + 1U, ' ');
			expect(!parse_review_comment_collection(response, selected, 1U), label + "a page stays bounded");
		}
		expect(maximum_review_mutation_wire_response_bytes(review_mutation_operation::observe_review_comments) == maximum_review_mutation_list_response_bytes
				   && maximum_review_mutation_wire_response_bytes(review_mutation_operation::observe_reviews) == maximum_review_mutation_list_response_bytes
				   && maximum_review_mutation_wire_response_bytes(review_mutation_operation::create_review) == maximum_review_mutation_response_bytes,
			   "only the two lists may carry a larger answer");
	}

	void test_review_comment_collection_pages()
	{
		constexpr auto collection = "/repos/MikeMarcin/BPR-Test/pulls/1/comments";
		const auto     first      = compile_review_comment_collection_request(target(), target().ServerContract);
		expect(first && first->Path == std::string{ collection } + "?per_page=100&page=1" && validate_outbound_review_mutation_request(*first).has_value(), "the collection's first page is an exact request");
		const auto last = compile_review_comment_collection_request(target(), target().ServerContract, maximum_review_comment_collection_pages);
		expect(last && validate_outbound_review_mutation_request(*last).has_value(), "the last readable page revalidates");
		expect(!compile_review_comment_collection_request(target(), target().ServerContract, 0U) && !compile_review_comment_collection_request(target(), target().ServerContract, maximum_review_comment_collection_pages + 1U), "pages outside the bound are refused before the wire");
		auto tampered = *first;
		tampered.Path = std::string{ collection } + "?per_page=100&page=11";
		expect(!validate_outbound_review_mutation_request(tampered).has_value(), "a page past the bound is a wire tamper");
		tampered.Path = std::string{ collection } + "?per_page=100";
		expect(!validate_outbound_review_mutation_request(tampered).has_value(), "a collection request names its page");

		const auto other    = std::string{ R"({"id":9001,"node_id":"PRRC_other","pull_request_review_id":900,"path":"src/other.cpp","commit_id":"2222222222222222222222222222222222222222","body":"Other","user":{"id":23,"node_id":"other","login":"other"})" };
		const auto link     = [&](const std::uint32_t aPage) { return "<https://api.github.com" + std::string{ collection } + "?per_page=100&page=" + std::to_string(aPage) + ">; rel=\"next\""; };
		auto       response = review_mutation_wire_response{ .Status = 200, .Body = "[" + other + R"(,"subject_type":"file"},)" + R"({"id":9002,"node_id":"PRRC_other_2","pull_request_review_id":900,"path":"src/other.cpp","commit_id":"2222222222222222222222222222222222222222","body":"Other","user":{"id":23,"node_id":"other","login":"other"},"subject_type":"line","start_side":"LEFT","start_line":9,"side":"RIGHT","line":3}])" };
		response.Headers    = { { .Name = "Link", .Value = link(2U) } };
		const auto page     = parse_review_comment_collection(response, target(), 1U);
		expect(page && page->HasNext && page->Comments.size() == 2U && !page->Comments.front().OnLine && page->Comments.back().StartSide == core::diff_side::left, "a file comment and a range across sides are read, and the next page is owed");
		expect(!parse_review_comment_collection(response, target(), 2U), "a next link must name the following page");
		response.Headers = { { .Name = "Link", .Value = link(maximum_review_comment_collection_pages + 1U) } };
		expect(!parse_review_comment_collection(response, target(), maximum_review_comment_collection_pages), "a collection longer than the bound is refused, not read in part");
		response.Headers = { { .Name = "Link", .Value = "<https://evil.invalid" + std::string{ collection } + "?per_page=100&page=2>; rel=\"next\"" } };
		expect(!parse_review_comment_collection(response, target(), 1U), "a foreign continuation is never followed");
		test_real_collection_pages();
	}

	void test_staged_response_contract()
	{
		using op     = application::staged_review_operation;
		auto request = staged_request(op::list_reviews);
		auto object  = parse_fixture_object(review_body());
		object.insert_or_assign("state", "PENDING");
		object.insert_or_assign("submitted_at", nullptr);
		object.insert_or_assign("html_url", "https://github.com/MikeMarcin/BPR-Test/pull/1#pullrequestreview-45");
		auto response = review_mutation_wire_response{ .Status = 200, .Body = "[" + fixture_json(object) + "]" };
		auto observed = parse_staged_review_response(response, request);
		expect(observed && observed->Reviews.size() == 1 && observed->Reviews.front().State == application::staged_review_state::pending, "pending review is a distinct observed state");
		response.Headers.push_back({ .Name = "Link", .Value = "<https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100&page=2>; rel=\"next\", <https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100&page=2>; rel=\"last\"" });
		observed = parse_staged_review_response(response, request);
		expect(observed && observed->HasNext, "complete pagination obligation is retained");
		response.Headers.front().Value = "<https://evil.invalid/repos/MikeMarcin/BPR-Test/pulls/1/reviews?per_page=100&page=2>; rel=\"next\"";
		expect(!parse_staged_review_response(response, request), "foreign continuation is never followed");
		response.Headers.clear();
		response.Body = "[" + fixture_json(object) + "," + fixture_json(object) + "]";
		expect(!parse_staged_review_response(response, request), "duplicate remote identity invalidates complete page");
		constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
		object.insert_or_assign("id", maximum);
		object.insert_or_assign("html_url", "https://github.com/MikeMarcin/BPR-Test/pull/1#pullrequestreview-18446744073709551615");
		response.Body = "[" + fixture_json(object) + "]";
		observed      = parse_staged_review_response(response, request);
		expect(observed && observed->Reviews.front().Id.value() == maximum, "full decimal REST review ID survives normalization");
		for (int status : { 301, 401, 403, 422, 429, 503 }) {
			response.Status = status;
			expect(!parse_staged_review_response(response, request), "non-success cannot qualify fallback");
		}
		response.Status  = 200;
		response.Headers = { { .Name = "Location", .Value = "https://evil.invalid" } };
		expect(!parse_staged_review_response(response, request), "success carrying redirect metadata refused");
		response.Headers.clear();
		response.Body = std::string(maximum_review_mutation_response_bytes + 1, ' ');
		expect(!parse_staged_review_response(response, request), "wire response byte budget enforced");
		request      = staged_request(op::list_comments);
		auto comment = parse_fixture_object(comment_body());
		comment.insert_or_assign("pull_request_review_id", 45U);
		comment.insert_or_assign("pull_request_url", "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1");
		comment.insert_or_assign("html_url", "https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r44");
		comment.insert_or_assign("start_line", 11U);
		comment.insert_or_assign("start_side", "RIGHT");
		// [[BPR-1367]]. GitHub's list of one review's comments, pending or
		// submitted, has no line, side or subject (MikeMarcin/BPR-Test#45).
		auto perReview = comment;
		for (const auto* field : { "line", "side", "start_line", "start_side" }) {
			perReview.erase(field);
		}
		perReview.insert_or_assign("position", 3U);
		response.Body = "[" + fixture_json(perReview) + "]";
		observed      = parse_staged_review_response(response, request);
		expect(observed && observed->Comments.size() == 1 && !observed->Comments.front().Source && observed->Comments.front().Id == 44U, "a review's own comment list in GitHub's shape, without lines, is read");
		response.Body = "[" + fixture_json(comment) + "]";
		observed      = parse_staged_review_response(response, request);
		expect(observed && observed->Comments.size() == 1 && !observed->Comments.front().Source, "lines are never read from a review's own comment list");
		perReview.insert_or_assign("path", "");
		response.Body = "[" + fixture_json(perReview) + "]";
		expect(!parse_staged_review_response(response, request), "a listed comment without a path is refused");
		// The pull request's collection carries the lines.
		auto collected = comment;
		collected.insert_or_assign("subject_type", "line");
		response.Body = "[" + fixture_json(collected) + "]";
		auto page     = parse_review_comment_collection(response, target());
		expect(page && !page->HasNext && page->Comments.size() == 1, "the pull request's collection is read");
		auto mine = staged_review_comments(page->Comments, request);
		expect(mine && mine->size() == 1 && mine->front().Source && mine->front().Source->FirstLine == 10 && mine->front().Source->LastLine == 12 && mine->front().Source->Side == core::diff_side::right, "range from the collection preserves inclusive source identity");
		comment.insert_or_assign("pull_request_review_id", 46U);
		response.Body = "[" + fixture_json(comment) + "]";
		expect(!parse_staged_review_response(response, request), "comment from another review refuses membership");
		comment.insert_or_assign("pull_request_review_id", 45U);
		comment.insert_or_assign("html_url", "https://github.com/MikeMarcin/BPR-Test/pull/2#discussion_r44");
		response.Body = "[" + fixture_json(comment) + "]";
		expect(!parse_staged_review_response(response, request), "cross-PR returned web link refused");
		collected.insert_or_assign("html_url", "https://github.com/MikeMarcin/BPR-Test/pull/2#discussion_r44");
		response.Body = "[" + fixture_json(collected) + "]";
		page          = parse_review_comment_collection(response, target());
		expect(page && !staged_review_comments(page->Comments, request), "cross-PR web link of a tour comment in the collection refused");
		collected.insert_or_assign("html_url", "https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r44");
		// Outdated, GitHub nulls the lines and keeps the sides.
		collected.insert_or_assign("line", nullptr);
		collected.insert_or_assign("start_line", nullptr);
		response.Body = "[" + fixture_json(collected) + "]";
		page          = parse_review_comment_collection(response, target());
		expect(page && page->Comments.size() == 1 && !staged_review_comments(page->Comments, request), "an outdated comment is well formed in the collection but not a tour comment");
		test_review_comment_collection_pages();
	}

	// [[BPR-1368]]. A stop edited while its review is pending goes by the
	// fixed GraphQL mutation, addressed by the comment's node ID: GitHub's
	// REST edit answers 404 for it (MikeMarcin/BPR-Test#45). Its answer must
	// name the REST comment and review the tour holds, still pending.
	[[nodiscard]] auto pending_edit_answer(const std::string_view aNode, const std::string_view aDatabase, const std::string_view aReview, const std::string_view aState, const std::string_view aBody, const std::string_view aPullRequest) -> std::string
	{
		return std::string{ R"({"data":{"updatePullRequestReviewComment":{"pullRequestReviewComment":{"id":")" } + std::string{ aNode } + R"(","fullDatabaseId":")" + std::string{ aDatabase }
		+ R"(","url":"https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r44","body":)" + std::string{ aBody } + R"(,"state":")" + std::string{ aState }
		+ R"(","viewerDidAuthor":true,"author":{"login":"fixture-reviewer","databaseId":22,"id":"MDQ6VXNlcjIy"},"pullRequestReview":{"id":"PRR_45","fullDatabaseId":")" + std::string{ aReview }
		+ R"("},"pullRequest":{"id":")" + std::string{ aPullRequest } + R"(","databaseId":4246446933,"number":1,"repository":{"id":"R_kgDOTyvofw","databaseId":1328277631,"nameWithOwner":"MikeMarcin/BPR-Test"}}}}}})";
	}
	void test_a_pending_stop_is_edited_by_graphql()
	{
		using op           = application::staged_review_operation;
		const auto request = staged_request(op::update_pending_comment);
		const auto wire    = compile_staged_review_request(request);
		expect(wire && wire->Operation == review_mutation_operation::staged_update_pending_comment && wire->Method == http_method::post && wire->Path == "/graphql"
				   && wire->Body == R"({"query":"mutation UpdateReviewComment($comment:ID!,$body:String!){updatePullRequestReviewComment(input:{pullRequestReviewCommentId:$comment,body:$body}){pullRequestReviewComment{id fullDatabaseId url body state viewerDidAuthor author{login ... on User{databaseId id}} pullRequestReview{id fullDatabaseId} pullRequest{id databaseId number repository{id databaseId nameWithOwner}}}}}","variables":{"comment":"PRRC_fixture_44","body":"Raw **tour** `$(literal)`"}})"
				   && validate_outbound_review_mutation_request(*wire).has_value(),
			   "a pending stop edit is the fixed GraphQL mutation, by node ID, with the raw body as a variable");
		auto ghes             = target();
		ghes.ServerContract   = application::github_server_contract::ghes_3_18_2022_11_28;
		ghes.Hostname         = application::github_canonical_hostname{ "ghe.example.com" };
		const auto enterprise = compile_staged_review_request(staged_request(op::update_pending_comment, ghes));
		expect(enterprise && enterprise->Path == "/api/graphql", "GHES sends it to its own GraphQL root");
		auto tampered = *wire;
		tampered.Body.replace(tampered.Body.find("pullRequestReview{"), 18, "repository{id} pullRequestReview{");
		expect(!validate_outbound_review_mutation_request(tampered).has_value(), "an altered document is refused at the wire");
		tampered      = *wire;
		tampered.Path = "/repos/MikeMarcin/BPR-Test/pulls/comments/44";
		expect(!validate_outbound_review_mutation_request(tampered).has_value(), "the pending edit never takes the REST path");
		for (const auto* node : { "", "PRRC \"x", "PRRC\"x" }) {
			auto bad        = request;
			bad.CommentNode = node;
			expect(!compile_staged_review_request(bad), "a missing or unsafe node ID is refused");
		}
		auto noReview   = request;
		noReview.Review = application::review_database_id{ };
		expect(!compile_staged_review_request(noReview), "the pending edit names the review its answer must match");
		auto restWithNode        = staged_request(op::update_comment);
		restWithNode.CommentNode = "PRRC_fixture_44";
		expect(!compile_staged_review_request(restWithNode), "a node ID rides only the GraphQL edit");
		auto empty = request;
		empty.Body.clear();
		expect(!compile_staged_review_request(empty), "an empty body is refused");

		const auto body     = std::string{ R"("Raw **tour** `$(literal)`")" };
		auto       response = review_mutation_wire_response{ .Status = 200, .Body = pending_edit_answer("PRRC_fixture_44", "44", "45", "PENDING", body, "PR_kwDOTyvof879G6NV") };
		const auto observed = parse_staged_review_response(response, request);
		expect(observed && observed->Comments.size() == 1U && observed->Comments.front().Id == 44U && observed->Comments.front().NodeId == "PRRC_fixture_44" && observed->Comments.front().Review.value() == 45U
				   && observed->Comments.front().Body == request.Body && !observed->Comments.front().Source && observed->Comments.front().WebUrl == "https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r44",
			   "GitHub's answer names the comment, its review and its pull request");
		for (const auto& wrong : {
				 pending_edit_answer("PRRC_other", "44", "45", "PENDING", body, "PR_kwDOTyvof879G6NV"),
				 pending_edit_answer("PRRC_fixture_44", "43", "45", "PENDING", body, "PR_kwDOTyvof879G6NV"),
				 pending_edit_answer("PRRC_fixture_44", "44", "46", "PENDING", body, "PR_kwDOTyvof879G6NV"),
				 pending_edit_answer("PRRC_fixture_44", "44", "45", "SUBMITTED", body, "PR_kwDOTyvof879G6NV"),
				 pending_edit_answer("PRRC_fixture_44", "44", "45", "PENDING", R"("Website text")", "PR_kwDOTyvof879G6NV"),
				 pending_edit_answer("PRRC_fixture_44", "44", "45", "PENDING", body, "PR_other"),
			 }) {
			response.Body = wrong;
			expect(!parse_staged_review_response(response, request), "another comment, review, state, body or pull request is refused");
		}
		response.Body = R"({"data":{"updatePullRequestReviewComment":null},"errors":[{"type":"NOT_FOUND","message":"x"}]})";
		expect(!parse_staged_review_response(response, request), "a GraphQL error is not an edit");
		response.Body   = pending_edit_answer("PRRC_fixture_44", "44", "45", "PENDING", body, "PR_kwDOTyvof879G6NV");
		response.Status = 404;
		expect(!parse_staged_review_response(response, request), "only 200 is an answer");
	}

	// [[BPR-1212]]. A comment on several lines sends GitHub's multi-line
	// fields; one line sends exactly the bytes and key it always did.
	[[nodiscard]] auto range_request(const std::uint32_t aStart) -> application::review_mutation_request
	{
		auto value               = request();
		value.Proposal.StartLine = aStart;
		value.IdempotencyKey     = application::idempotency_key_for(value.Proposal, value.Target);
		return value;
	}

	void test_a_range_sends_githubs_multi_line_fields()
	{
		const auto single = request();
		expect(single.IdempotencyKey == "5871cf205188372ebc5fbfffcccda1351bef269f18041abe642b2f36fd03766c",
			   "a single line's idempotency key is the one sealed before ranges existed");
		const auto ranged = range_request(8U);
		expect(ranged.IdempotencyKey != single.IdempotencyKey && ranged.IdempotencyKey != range_request(9U).IdempotencyKey,
			   "a range's key differs from its last line's and from another range's");
		const auto compiled = compile_review_comment_request(ranged);
		expect(compiled.has_value()
				   && compiled->Body
					   == "{\"body\":\"Please rename this.\","
						  "\"commit_id\":\"1111111111111111111111111111111111111111\","
						  "\"line\":13,\"path\":\"fixture/review.txt\",\"side\":\"RIGHT\","
						  "\"start_line\":9,\"start_side\":\"RIGHT\"}",
			   "lines 9 to 13 send start_line 9 and start_side RIGHT after the single-line fields, in sorted key order");
		expect(validate_outbound_review_mutation_request(*compiled).has_value(), "the range request revalidates canonically");
		auto tampered = *compiled;
		tampered.Body.replace(tampered.Body.find("\"start_line\":9"), 14U, "\"start_line\":8");
		expect(!validate_outbound_review_mutation_request(tampered).has_value(), "a changed start line is a wire tamper");
		const auto plain = compile_review_comment_request(single);
		expect(plain && plain->Body.find("start_") == std::string::npos, "one line sends no multi-line field");

		for (const auto start : { 12U, 13U }) {
			expect(!compile_review_comment_request(range_request(start)).has_value(), "a start at or after the last line is refused before the wire");
		}
		auto reply                   = range_request(8U);
		reply.Proposal.ReplyToThread = "PRRT_kwDOFixture";
		reply.IdempotencyKey         = application::idempotency_key_for(reply.Proposal, reply.Target);
		expect(!compile_review_comment_request(reply).has_value(), "a reply never carries a range");
		auto file           = range_request(8U);
		file.Representation = core::review_publication_representation::file_comment_with_permalink;
		expect(!compile_review_comment_request(file).has_value(), "a range never falls back to a file comment with one line's link");

		const auto rangeResponse = [](const std::string_view aStart) {
			auto body = comment_body();
			body.insert(body.find(",\"body\""), std::string{ aStart });
			return body;
		};
		const auto exact = parse_review_comment_response({ .Status = 201, .Body = rangeResponse(",\"start_line\":9,\"start_side\":\"RIGHT\"") }, ranged);
		expect(exact.has_value() && exact->NodeId == "PRRC_fixture_44", "GitHub's answer with the range's start line is the range's receipt");
		for (const auto wrong : { std::string_view{ "" }, std::string_view{ ",\"start_line\":null,\"start_side\":null" }, std::string_view{ ",\"start_line\":8,\"start_side\":\"RIGHT\"" },
								  std::string_view{ ",\"start_line\":9,\"start_side\":\"LEFT\"" }, std::string_view{ ",\"start_line\":9" } }) {
			const auto refused = parse_review_comment_response({ .Status = 201, .Body = rangeResponse(wrong) }, ranged);
			expect(!refused.has_value() && refused.error() == review_mutation_contract_failure::identity_mismatch, "an answer without the exact start line and side is not the range");
		}
		expect(parse_review_comment_response({ .Status = 201, .Body = rangeResponse(",\"start_line\":null,\"start_side\":null") }, single).has_value(),
			   "a single line still takes GitHub's null multi-line fields");
		const auto stray = parse_review_comment_response({ .Status = 201, .Body = rangeResponse(",\"start_line\":9,\"start_side\":\"RIGHT\"") }, single);
		expect(!stray.has_value() && stray.error() == review_mutation_contract_failure::identity_mismatch, "a range ending at the line is not the single line's receipt");
	}

	// [[BPR-1212]] v002. A new comment on a base line is GitHub's `side: LEFT`,
	// with `start_side: LEFT` for a range, and GitHub's answer is its receipt
	// only on that side. It has no file-comment form.
	void test_a_base_line_sends_side_left()
	{
		auto base                              = range_request(8U);
		base.Proposal.Anchor.Side              = core::diff_side::left;
		base.LiveBinding->Anchor.Semantic.Side = core::diff_side::left;
		base.IdempotencyKey                    = application::idempotency_key_for(base.Proposal, base.Target);
		expect(base.IdempotencyKey != range_request(8U).IdempotencyKey, "a base-side range's key differs from the head-side one's");
		const auto compiled = compile_review_comment_request(base);
		expect(compiled.has_value()
				   && compiled->Body
					   == "{\"body\":\"Please rename this.\","
						  "\"commit_id\":\"1111111111111111111111111111111111111111\","
						  "\"line\":13,\"path\":\"fixture/review.txt\",\"side\":\"LEFT\","
						  "\"start_line\":9,\"start_side\":\"LEFT\"}",
			   "base lines 9 to 13 send side LEFT and start_side LEFT on the head's path and commit");
		expect(validate_outbound_review_mutation_request(*compiled).has_value(), "the base-side request revalidates canonically");
		auto file = base;
		file.Proposal.StartLine.reset();
		file.Representation = core::review_publication_representation::file_comment_with_permalink;
		file.IdempotencyKey = application::idempotency_key_for(file.Proposal, file.Target);
		expect(!compile_review_comment_request(file).has_value(), "a base line never falls back to a file comment");

		auto answer = comment_body();
		answer.replace(answer.find("\"side\":\"RIGHT\""), 14U, "\"side\":\"LEFT\"");
		answer.insert(answer.find(",\"body\""), ",\"start_line\":9,\"start_side\":\"LEFT\"");
		const auto exact = parse_review_comment_response({ .Status = 201, .Body = answer }, base);
		expect(exact.has_value() && exact->NodeId == "PRRC_fixture_44", "GitHub's answer on the left is the base-side comment's receipt");
		const auto right = parse_review_comment_response({ .Status = 201, .Body = answer }, range_request(8U));
		expect(!right.has_value(), "the same answer is not a head-side range's receipt");
	}

	/// RFC-BPR-0089 M4. A line the host placed from a commit-filtered diff:
	/// at the head, GitHub is told the head's line; on an earlier commit,
	/// that commit and its line. GitHub answers the earlier commit's comment
	/// outdated at once (BPR-Test #116: `line` null, `commit_id` kept), and it
	/// is ours by the commit and line it was written at. Anyone's outdated
	/// comment in the list is well formed and not ours.
	void test_a_placed_line_sends_its_commit_and_line()
	{
		auto moved                           = request();
		moved.LiveBinding->Anchor.RemoteLine = 14U;
		const auto head                      = compile_review_comment_request(moved);
		expect(head.has_value()
				   && head->Body
					   == "{\"body\":\"Please rename this.\","
						  "\"commit_id\":\"1111111111111111111111111111111111111111\","
						  "\"line\":15,\"path\":\"fixture/review.txt\",\"side\":\"RIGHT\"}",
			   "a line moved to the head sends the head's line, not the diff's");
		expect(parse_review_comment_response({ .Status = 201, .Body = comment_body(15U) }, moved).has_value(),
			   "GitHub's answer at the head's line is the moved comment's receipt");
		expect(!parse_review_comment_response({ .Status = 201, .Body = comment_body(13U) }, moved).has_value(),
			   "an answer at the diff's own line is not");

		const auto c1                          = commit(0x25U);
		auto       earlier                     = request();
		earlier.LiveBinding->Anchor.Commit     = c1;
		earlier.LiveBinding->Anchor.RemoteLine = 12U;
		expect(application::valid(*earlier.LiveBinding), "an earlier commit is a valid binding only as a placed line");
		auto unplaced                       = request();
		unplaced.LiveBinding->Anchor.Commit = c1;
		expect(!application::valid(*unplaced.LiveBinding) && !compile_review_comment_request(unplaced).has_value(),
			   "an earlier commit without a placement is refused before any write");
		auto onBase                              = earlier;
		onBase.Proposal.Anchor.Side              = core::diff_side::left;
		onBase.LiveBinding->Anchor.Semantic.Side = core::diff_side::left;
		onBase.IdempotencyKey                    = application::idempotency_key_for(onBase.Proposal, onBase.Target);
		expect(!application::valid(*onBase.LiveBinding), "a base line never goes on an earlier commit");
		const auto compiled = compile_review_comment_request(earlier);
		expect(compiled.has_value()
				   && compiled->Body
					   == "{\"body\":\"Please rename this.\","
						  "\"commit_id\":\"2525252525252525252525252525252525252525\","
						  "\"line\":13,\"path\":\"fixture/review.txt\",\"side\":\"RIGHT\"}",
			   "a line placed on C1 sends C1 and its line");
		const auto outdated = std::string{ "{\"id\":44,\"node_id\":\"PRRC_fixture_44\",\"path\":\"fixture/review.txt\","
										   "\"commit_id\":\"2525252525252525252525252525252525252525\","
										   "\"original_commit_id\":\"2525252525252525252525252525252525252525\","
										   "\"side\":\"RIGHT\",\"line\":null,\"original_line\":13,"
										   "\"body\":\"Please rename this.\",\"user\":{\"login\":\"fixture-reviewer\",\"id\":22,\"node_id\":\"MDQ6VXNlcjIy\"}}" };
		const auto exact    = parse_review_comment_response({ .Status = 201, .Body = outdated }, earlier);
		expect(exact.has_value() && exact->NodeId == "PRRC_fixture_44", "GitHub's outdated answer is the earlier commit's comment");
		const auto swapped = [](std::string aText, const std::string_view aFrom, const std::string_view aTo) {
			aText.replace(aText.find(aFrom), aFrom.size(), aTo);
			return aText;
		};
		const auto moving = swapped(swapped(outdated, "\"commit_id\":\"2525252525252525252525252525252525252525\"", "\"commit_id\":\"1111111111111111111111111111111111111111\""),
									"\"line\":null", "\"line\":20");
		expect(parse_review_comment_response({ .Status = 201, .Body = moving }, earlier).has_value(),
			   "and so is the same comment GitHub moved on to the head");
		const auto elsewhere = swapped(outdated, "\"original_line\":13", "\"original_line\":14");
		expect(!parse_review_comment_response({ .Status = 201, .Body = elsewhere }, earlier).has_value(), "one written at another line is not");
		const auto other = parse_review_comment_response({ .Status = 201, .Body = outdated }, request());
		expect(!other.has_value() && other.error() == review_mutation_contract_failure::identity_mismatch,
			   "an outdated comment is a well-formed comment that is not a head comment's receipt");
	}
} // namespace

namespace {
	/// [[BPR-1345]]. A review that carries pending drafts sends them as its
	/// `comments`, each with GitHub's path, line and side and, for a range,
	/// start_line and start_side, in sorted key order between the summary
	/// and the head; a summary-only review keeps its exact bytes. Comment
	/// with no summary compiles only with a comment; Request changes still
	/// needs one; every comment needs words and a path; the whole request
	/// stays inside the 64 KiB limit.
	void test_a_review_sends_its_pending_comments()
	{
		auto first          = core::review_submission_comment{ };
		first.Draft         = core::review_draft_id{ 7U };
		first.DraftRevision = 3U;
		first.Path          = "src/engine/renderer.cpp";
		first.Line          = 430U;
		first.Body          = "question: Where does 0.01 come from?";
		auto range          = first;
		range.Draft         = core::review_draft_id{ 8U };
		range.Line          = 435U;
		range.StartLine     = 433U;
		range.Body          = "nit: \"scale\" could be constexpr.";
		auto base           = first;
		base.Draft          = core::review_draft_id{ 9U };
		base.Side           = core::diff_side::left;
		base.Line           = 11U;
		base.Body           = "Was this ever reached?";

		auto input          = submission();
		input.Body          = { };
		input.Comments      = { first, range, base };
		const auto compiled = compile_review_submission_request(input);
		expect(compiled.has_value() && compiled->Operation == review_mutation_operation::create_review && compiled->Method == http_method::post
				   && compiled->Path == "/repos/MikeMarcin/BPR-Test/pulls/1/reviews",
			   "a review with comments is the same one POST to the reviews collection");
		expect(compiled->Body
				   == R"({"body":"","comments":[{"body":"question: Where does 0.01 come from?","line":431,"path":"src/engine/renderer.cpp","side":"RIGHT"},)"
					  R"({"body":"nit: \"scale\" could be constexpr.","line":436,"path":"src/engine/renderer.cpp","side":"RIGHT","start_line":434,"start_side":"RIGHT"},)"
					  R"({"body":"Was this ever reached?","line":12,"path":"src/engine/renderer.cpp","side":"LEFT"}],)"
					  R"("commit_id":"1111111111111111111111111111111111111111","event":"COMMENT"})",
			   "each pending draft is a review comment: one-based lines, both sides, a range's first line, sorted keys");
		expect(validate_outbound_review_mutation_request(*compiled).has_value(), "the review with comments revalidates against its semantic request");
		auto tampered = *compiled;
		tampered.Body.replace(tampered.Body.find("431"), 3U, "432");
		expect(!validate_outbound_review_mutation_request(tampered).has_value(), "a comment moved on the wire is refused");
		const auto observed = compile_review_submission_observation_request(input);
		expect(observed && observed->Method == http_method::get && observed->Body.empty(), "its observation reads the review list, with no body");

		auto plain = submission();
		expect(compile_review_submission_request(plain)->Body == R"({"body":"Review summary.","commit_id":"1111111111111111111111111111111111111111","event":"COMMENT"})",
			   "a summary-only review keeps its exact bytes");
		plain.Body = { };
		expect(!compile_review_submission_request(plain).has_value(), "Comment with no summary and no comment is refused, as GitHub refuses it");
		auto changes        = input;
		changes.Disposition = core::review_disposition::request_changes;
		expect(!compile_review_submission_request(changes).has_value(), "Request changes needs a summary, comments or not");
		changes.Body = "Please fix.";
		expect(compile_review_submission_request(changes).has_value(), "with one it compiles");
		auto blank             = input;
		blank.Comments[1].Body = " \n";
		expect(!compile_review_submission_request(blank).has_value(), "a comment with no words is refused");
		auto pathless             = input;
		pathless.Comments[0].Path = { };
		expect(!compile_review_submission_request(pathless).has_value(), "a comment with no path is refused");
		auto inverted                  = input;
		inverted.Comments[1].StartLine = 436U;
		expect(!compile_review_submission_request(inverted).has_value(), "a range that does not end below its start is refused");
		auto twice        = input;
		twice.Comments[1] = twice.Comments[0];
		expect(!compile_review_submission_request(twice).has_value(), "one draft rides once");
		auto large = input;
		for (auto& comment : large.Comments) {
			comment.Body = std::string(20U * 1024U, 'x');
		}
		expect(!compile_review_submission_request(large).has_value(), "a review that cannot fit one request is refused before any send");
	}
}

auto main() -> int
{
	test_a_review_sends_its_pending_comments();
	const auto fixture = fixture_object{
		{ "fraction", 1.5 },
		{ "large", std::uint64_t{ 9'007'199'254'740'993U } },
		{ "negative", -1 },
		{ "null", nullptr },
	};
	expect(fixture_json(fixture)
			   == R"({"fraction":1.5,"large":9007199254740993,"negative":-1,"null":null})",
		   "semantic fixtures retain explicit null and signed, fractional and exact unsigned numbers");
	test_staged_batch_contract();
	test_merge_base_observation_contract();
	test_staged_response_contract();
	test_a_pending_stop_is_edited_by_graphql();
	test_exact_wire_compilation_and_tamper_rejection();
	test_a_range_sends_githubs_multi_line_fields();
	test_a_base_line_sends_side_left();
	test_a_placed_line_sends_its_commit_and_line();
	test_replies_and_thread_resolution_go_by_graphql_exactly();
	test_every_operation_names_its_exchange();
	test_reactions_go_by_graphql_exactly();
	test_the_description_goes_by_graphql_exactly();
	test_a_published_comment_goes_by_graphql_exactly();
	test_recorded_reaction_and_resolution_answers();

	test_a_reply_is_reconciled_through_its_thread();
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
