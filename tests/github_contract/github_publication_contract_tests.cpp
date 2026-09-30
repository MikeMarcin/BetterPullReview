#include "github_publication_contract.hpp"

#include "bpr/core/git_objects.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief The GitHub publication catalog's REST contract and session
/// (RFC-BPR-0059, BPR-1175): every request is exact, every answer is admitted
/// only for what was asked, identities are the ones computed locally, and
/// the session refuses hosts, accounts and server contracts it is not bound
/// to before a request exists. Fixture exchanges only; nothing is sent.

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

	auto oid(const std::string_view aHex) -> core::git_entry_oid
	{
		const auto parsed = core::parse_git_entry_oid(aHex);
		expect(parsed.has_value(), "fixture identity");
		return *parsed;
	}

	// Identities produced by installed Git for the fixture below.
	constexpr std::string_view hello_blob  = "ce013625030ba8dba906f756967f9e9ca394464a";
	constexpr std::string_view parent_hex  = "a45fbcb979e80a11e91371a41a86ea707b552ee4";
	constexpr std::string_view tree_hex    = "b5358577b95747ac0e3ba9a7cc4612da70ebf3bc";
	constexpr std::string_view commit_hex  = "a18180d187116b193478f0fdf147af7d95bc1a32";
	constexpr std::string_view base_tree   = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
	constexpr std::string_view foreign_hex = "1111111111111111111111111111111111111111";

	auto target(const application::github_server_contract aServer = application::github_server_contract::github_com_2022_11_28) -> application::github_publication_target
	{
		return { .Host = "github.com", .Server = aServer, .Account = core::github_account_id{ 21U }, .Login = "fixture-reviewer", .Owner = "octo-org", .Repository = "review-repository" };
	}

	auto commit_content() -> core::git_commit_content
	{
		return {
			.Tree      = core::retag<core::git_tree_oid_kind>(oid(tree_hex)),
			.Parents   = { core::retag<core::git_commit_oid_kind>(oid(parent_hex)) },
			.Author    = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
			.Committer = { .Name = "Ada Lovelace", .Email = "ada@example.invalid", .Seconds = 1700000000 },
			.Message   = "Second\n",
		};
	}

	auto bytes(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	auto response(const int aStatus, std::string aBody, std::vector<http_header> aHeaders = { }) -> publication_wire_response
	{
		return { .Status = aStatus, .Headers = std::move(aHeaders), .Body = std::move(aBody) };
	}

	void compiles_exact_requests()
	{
		const auto repository = compile_observe_repository(target());
		expect(repository && repository->Method == http_method::get && repository->Path == "/repos/octo-org/review-repository" && repository->Body.empty(), "repository read");
		expect(std::ranges::find(repository->Headers, http_header{ .Name = "X-GitHub-Api-Version", .Value = "2022-11-28" }) != repository->Headers.end(), "the admitted API version");
		const auto branch = compile_observe_branch(target(), "bpr/harden parsing");
		expect(!branch, "a branch name with a space");
		const auto nested = compile_observe_branch(target(), "bpr/harden");
		expect(nested && nested->Path == "/repos/octo-org/review-repository/git/ref/heads/bpr/harden", "the exact reference, never a prefix listing");

		const auto blob = compile_create_blob(target(), bytes("hello\n"));
		expect(blob && blob->Method == http_method::post && blob->Path == "/repos/octo-org/review-repository/git/blobs" && blob->Body == R"({"content":"aGVsbG8K","encoding":"base64"})",
			   "blob bytes travel as base64, byte for byte");
		expect(!compile_create_blob(target(), std::vector<std::uint8_t>(application::maximum_github_publication_blob_bytes + 1U, 1U)), "over the upload bound");

		const auto tree = compile_create_tree(target(), core::retag<core::git_tree_oid_kind>(oid(base_tree)),
											  { { .Path = "bin/run.sh", .Mode = core::git_mode_executable, .Object = oid(hello_blob) }, { .Path = "link", .Mode = core::git_mode_symlink, .Object = oid(hello_blob) } },
											  { "old.cpp" });
		expect(tree && tree->Body == std::string{ R"({"base_tree":")" } + std::string{ base_tree } + R"(","tree":[{"path":"bin/run.sh","mode":"100755","type":"blob","sha":")" + std::string{ hello_blob } + R"("},{"path":"link","mode":"120000","type":"blob","sha":")" + std::string{ hello_blob } + R"("},{"path":"old.cpp","mode":"100644","type":"blob","sha":null}]})",
			   "the base tree, each change, and an explicit null for a deletion");
		expect(!compile_create_tree(target(), core::retag<core::git_tree_oid_kind>(oid(base_tree)), { { .Path = "vendor", .Mode = core::git_mode_gitlink, .Object = oid(hello_blob) } }, { }),
			   "no submodule entries");

		auto dated                 = commit_content();
		dated.Author.Seconds       = 1700000000;
		dated.Author.OffsetMinutes = 90;
		expect(publication_date(dated.Author) == "2023-11-14T23:43:20+01:30" && publication_date(commit_content().Committer) == "2023-11-14T22:13:20Z", "explicit, pinned dates");
		const auto commit = compile_create_commit(target(), commit_content());
		expect(commit && commit->Body == std::string{ R"({"message":"Second\n","tree":")" } + std::string{ tree_hex } + R"(","parents":[")" + std::string{ parent_hex } + R"("],"author":{"name":"Ada Lovelace","email":"ada@example.invalid","date":"2023-11-14T22:13:20Z"},"committer":{"name":"Ada Lovelace","email":"ada@example.invalid","date":"2023-11-14T22:13:20Z"}})",
			   "one parent, exact attribution, no trailer");
		auto orphan    = commit_content();
		orphan.Parents = { };
		expect(!compile_create_commit(target(), orphan), "exactly one parent");

		const auto ref = compile_create_branch(target(), "bpr/harden", core::retag<core::git_commit_oid_kind>(oid(commit_hex)));
		expect(ref && ref->Path == "/repos/octo-org/review-repository/git/refs" && ref->Body == std::string{ R"({"ref":"refs/heads/bpr/harden","sha":")" } + std::string{ commit_hex } + R"("})",
			   "a new reference, created, never updated");
		const auto pulls = compile_find_pull_requests(target(), "bpr/harden", "main");
		expect(pulls && pulls->Query == "state=all&head=octo-org%3Abpr%2Fharden&base=main&per_page=100", "open and closed matches for the exact head and base");
		const auto pull = compile_create_pull_request(target(), { .Title = "Harden parsing", .Body = "Two rounds.", .Head = "bpr/harden", .Base = "main", .Draft = true });
		expect(pull && pull->Body == R"({"title":"Harden parsing","body":"Two rounds.","head":"bpr/harden","base":"main","draft":true})", "the confirmed pull request text");

		for (const auto& compiled : { *repository, *nested, *blob, *tree, *commit, *ref, *pulls, *pull }) {
			expect(validate_outbound_publication_request(compiled), "compiled requests validate");
			auto tampered = compiled;
			tampered.Path += "/x";
			expect(!validate_outbound_publication_request(tampered), "a changed path is refused");
			tampered        = compiled;
			tampered.Method = compiled.Method == http_method::get ? http_method::post : http_method::get;
			expect(!validate_outbound_publication_request(tampered), "a changed method is refused");
		}
		auto elsewhere         = *blob;
		elsewhere.Target.Owner = "other-org";
		expect(!validate_outbound_publication_request(elsewhere), "a path outside the target repository");
		expect(!compile_observe_repository(target(application::github_server_contract::ghes_3_18_2022_11_28)), "GHES is not admitted for native publication yet");
		auto sha256 = commit_content();
		sha256.Tree = core::git_tree_oid{ core::git_hash_algorithm::sha256, oid(hello_blob).bytes() };
		expect(!compile_create_commit(target(), sha256), "SHA-256 objects are not sent to a SHA-1 provider");
	}

	void parses_only_what_was_asked()
	{
		const auto repository = parse_repository(response(200, R"({"name":"review-repository","full_name":"octo-org/review-repository","owner":{"login":"octo-org"},"archived":false,"default_branch":"main","permissions":{"admin":false,"push":true,"pull":true}})"), target());
		expect(repository && repository->CanPush && !repository->Archived && repository->DefaultBranch == "main", "push permission read");
		const auto readOnly = parse_repository(response(200, R"({"name":"review-repository","full_name":"octo-org/review-repository","owner":{"login":"octo-org"},"archived":false,"default_branch":"main"})"), target());
		expect(readOnly && !readOnly->CanPush, "no permissions means no push");
		const auto renamed = parse_repository(response(200, R"({"name":"renamed","full_name":"octo-org/renamed","owner":{"login":"octo-org"},"archived":false,"default_branch":"main"})"), target());
		expect(!renamed && renamed.error() == publication_contract_failure::identity_mismatch, "another repository is not this one");
		const auto duplicate = parse_repository(response(200, R"({"name":"review-repository","name":"x","full_name":"octo-org/review-repository","owner":{"login":"octo-org"},"archived":false,"default_branch":"main"})"), target());
		expect(!duplicate && duplicate.error() == publication_contract_failure::malformed_response, "a duplicate key is refused before decoding");

		const auto absent = parse_branch(response(404, R"({"message":"Not Found"})"), "bpr/harden", core::git_hash_algorithm::sha1);
		expect(absent && !*absent, "a missing branch is an observation");
		const auto head = parse_branch(response(200, std::string{ R"({"ref":"refs/heads/bpr/harden","object":{"sha":")" } + std::string{ commit_hex } + R"(","type":"commit"}})"), "bpr/harden", core::git_hash_algorithm::sha1);
		expect(head && *head && core::canonical_hex(**head).view() == commit_hex, "a branch head");
		const auto prefix = parse_branch(response(200, std::string{ R"({"ref":"refs/heads/bpr/harden-2","object":{"sha":")" } + std::string{ commit_hex } + R"(","type":"commit"}})"), "bpr/harden", core::git_hash_algorithm::sha1);
		expect(!prefix && prefix.error() == publication_contract_failure::identity_mismatch, "another reference is not this one");

		const auto commit  = core::retag<core::git_commit_oid_kind>(oid(commit_hex));
		const auto created = parse_created_commit(response(201, std::string{ R"({"sha":")" } + std::string{ commit_hex } + R"(","tree":{"sha":")" + std::string{ tree_hex } + R"("},"parents":[{"sha":")" + std::string{ parent_hex } + R"("}]})"), commit_content(), commit);
		expect(created.has_value(), "the server's commit is the local one");
		const auto rewritten = parse_created_commit(response(201, std::string{ R"({"sha":")" } + std::string{ foreign_hex } + R"(","tree":{"sha":")" + std::string{ tree_hex } + R"("},"parents":[{"sha":")" + std::string{ parent_hex } + R"("}]})"), commit_content(), commit);
		expect(!rewritten && rewritten.error() == publication_contract_failure::identity_mismatch, "another encoding is refused, never adopted");
		const auto observed = parse_commit(response(200, std::string{ R"({"sha":")" } + std::string{ commit_hex } + R"(","tree":{"sha":")" + std::string{ tree_hex } + R"("},"parents":[{"sha":")" + std::string{ parent_hex } + R"("}]})"), commit);
		expect(observed && *observed && (*observed)->Tree == commit_content().Tree && (*observed)->Parents == commit_content().Parents, "a commit read back");
		expect(parse_commit(response(404, "{}"), commit) && !*parse_commit(response(404, "{}"), commit), "an absent commit");

		const auto branch = parse_created_branch(response(201, std::string{ R"({"ref":"refs/heads/bpr/harden","object":{"sha":")" } + std::string{ commit_hex } + R"(","type":"commit"}})"), "bpr/harden", commit);
		expect(branch && *branch == application::github_branch_creation::created, "created");
		const auto exists = parse_created_branch(response(422, R"({"message":"Reference already exists","documentation_url":"https://docs.github.com/rest"})"), "bpr/harden", commit);
		expect(exists && *exists == application::github_branch_creation::exists, "an existing reference is reported, not overwritten");
		const auto other = parse_created_branch(response(422, R"({"message":"Validation Failed"})"), "bpr/harden", commit);
		expect(!other, "another refusal is not an existing branch");

		const auto sha = parse_created_sha(response(201, std::string{ R"({"sha":")" } + std::string{ hello_blob } + R"(","url":"https://api.github.com/x"})"), core::git_hash_algorithm::sha1);
		expect(sha && core::git_entry_oid{ core::git_hash_algorithm::sha1, *sha } == oid(hello_blob), "a created identity");
		expect(!parse_created_sha(response(201, R"({"sha":"not-hex"})"), core::git_hash_algorithm::sha1), "a malformed identity");

		const auto pull = std::string{ R"({"number":17,"node_id":"PR_17","html_url":"https://github.com/octo-org/review-repository/pull/17","state":"open","draft":false,"title":"Harden parsing","user":{"login":"fixture-reviewer"},"head":{"ref":"bpr/harden","sha":")" }
			+ std::string{ commit_hex } + R"(","repo":{"name":"review-repository","owner":{"login":"octo-org"}}},"base":{"ref":"main","sha":")" + std::string{ parent_hex } + R"("}})";
		const auto one = parse_pull_request(response(201, pull), core::git_hash_algorithm::sha1);
		expect(one && *one && (*one)->Number == 17U && (*one)->HeadRef == "bpr/harden" && (*one)->HeadOwner == "octo-org" && (*one)->BaseRef == "main", "a pull request");
		const auto many = parse_pull_requests(response(200, "[" + pull + "]"), core::git_hash_algorithm::sha1);
		expect(many && many->size() == 1U, "a listing");
		const auto paged = parse_pull_requests(response(200, "[" + pull + "]", { { .Name = "Link", .Value = R"(<https://api.github.com/x?page=2>; rel="next")" } }), core::git_hash_algorithm::sha1);
		expect(!paged, "an incomplete listing is refused, never read as all matches");
	}

	/// A transport that answers from a script and records every call.
	class scripted_transport final : public github_publication_wire_transport
	{
	public:
		std::deque<github_publication_wire_completion> Answers;
		std::vector<github_publication_wire_call>      Calls;

		auto perform_publication(const github_publication_wire_call& aCall, std::stop_token) -> github_publication_wire_completion override
		{
			Calls.push_back(aCall);
			expect(validate_outbound_publication_request(aCall.Wire), "the session sends only exact requests");
			expect(!Answers.empty(), "an answer is scripted");
			auto answer = std::move(Answers.front());
			Answers.pop_front();
			return answer;
		}
	};

	auto context() -> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 2U },
			.Authority          = { .Profile         = core::host_profile_id{ 3U },
									.ProfileRevision = application::github_profile_revision{ 4U },
									.Host            = core::git_host_id{ 5U },
									.Hostname        = application::github_canonical_hostname{ "github.com" },
									.Account         = core::github_account_id{ 21U },
									.ServerContract  = application::github_server_contract::github_com_2022_11_28 },
			.IdentityGeneration = core::github_identity_generation{ 6U },
		};
	}

	auto provider_error(const application::github_provider_failure aFailure, const std::uint16_t aStatus, const github_mutation_transmission aTransmission,
						const std::optional<std::uint32_t> aRetry = std::nullopt) -> github_publication_wire_completion
	{
		auto error                    = github_publication_wire_error{ .Transmission = aTransmission };
		error.Error.Failure           = aFailure;
		error.Error.HttpStatus        = aStatus;
		error.Error.RetryAfterSeconds = aRetry;
		return error;
	}

	void sessions_admit_one_authority()
	{
		auto transport = scripted_transport{ };
		auto session   = github_publication_session{ transport, context() };
		auto elsewhere = target();
		elsewhere.Host = "ghe.example.invalid";
		expect(session.observe_repository(elsewhere, { }).error().Failure == application::github_publication_failure::unsupported_host, "an unknown host");
		auto someone    = target();
		someone.Account = core::github_account_id{ 99U };
		expect(session.observe_repository(someone, { }).error().Failure == application::github_publication_failure::unsupported_host, "another account");
		expect(session.observe_repository(target(application::github_server_contract::ghes_3_18_2022_11_28), { }).error().Failure == application::github_publication_failure::unsupported_host,
			   "an unqualified server contract");
		expect(session.create_blob(target(), bytes("hello, world\n"), oid(hello_blob), { }).error().Failure == application::github_publication_failure::refused,
			   "bytes that are not the named object are never sent");
		expect(transport.Calls.empty(), "and none of those produced a request");

		transport.Answers.emplace_back(github_publication_wire_success{ .Response = response(201, std::string{ R"({"sha":")" } + std::string{ hello_blob } + R"("})") });
		expect(session.create_blob(target(), bytes("hello\n"), oid(hello_blob), { }).has_value(), "a blob created as computed");
		expect(transport.Calls.back().Context == context(), "under the session's own credential context");
		transport.Answers.emplace_back(github_publication_wire_success{ .Response = response(201, std::string{ R"({"sha":")" } + std::string{ foreign_hex } + R"("})") });
		const auto mismatch = session.create_blob(target(), bytes("hello\n"), oid(hello_blob), { });
		expect(!mismatch && mismatch.error().Failure == application::github_publication_failure::mismatch && mismatch.error().Transmission == application::github_publication_transmission::indeterminate,
			   "another identity is refused and marked as possibly stored");

		transport.Answers.emplace_back(provider_error(application::github_provider_failure::permission, 403U, github_mutation_transmission::indeterminate));
		const auto denied = session.create_branch(target(), "bpr/harden", core::retag<core::git_commit_oid_kind>(oid(commit_hex)), { });
		expect(!denied && denied.error().Failure == application::github_publication_failure::refused && denied.error().HttpStatus == 403U, "a permission refusal");
		transport.Answers.emplace_back(provider_error(application::github_provider_failure::rate_limit, 429U, github_mutation_transmission::indeterminate, 60U));
		const auto limited = session.create_commit(target(), commit_content(), core::retag<core::git_commit_oid_kind>(oid(commit_hex)), { });
		expect(!limited && limited.error().Failure == application::github_publication_failure::rate_limited && limited.error().RetryAfterSeconds == 60U, "a rate limit with its wait");
		transport.Answers.emplace_back(provider_error(application::github_provider_failure::unavailable, 502U, github_mutation_transmission::indeterminate));
		const auto lost = session.create_commit(target(), commit_content(), core::retag<core::git_commit_oid_kind>(oid(commit_hex)), { });
		expect(!lost && lost.error().Failure == application::github_publication_failure::unavailable && lost.error().Transmission == application::github_publication_transmission::indeterminate,
			   "a server error after sending is uncertain");
		transport.Answers.emplace_back(provider_error(application::github_provider_failure::network, 0U, github_mutation_transmission::definitely_not_sent));
		const auto unsent = session.create_commit(target(), commit_content(), core::retag<core::git_commit_oid_kind>(oid(commit_hex)), { });
		expect(!unsent && unsent.error().Transmission == application::github_publication_transmission::definitely_not_sent, "a request that never left");
		transport.Answers.emplace_back(github_publication_wire_success{ .Response = response(422, R"({"message":"Reference already exists"})") });
		const auto exists = session.create_branch(target(), "bpr/harden", core::retag<core::git_commit_oid_kind>(oid(commit_hex)), { });
		expect(exists && *exists == application::github_branch_creation::exists, "an existing branch is reported, never updated");
		transport.Answers.emplace_back(github_publication_wire_cancelled{ .Transmission = github_mutation_transmission::indeterminate });
		const auto cancelled = session.create_tree(target(), core::retag<core::git_tree_oid_kind>(oid(base_tree)), { { .Path = "a", .Mode = core::git_mode_regular, .Object = oid(hello_blob) } }, { },
												   core::retag<core::git_tree_oid_kind>(oid(tree_hex)), { });
		expect(!cancelled && cancelled.error().Failure == application::github_publication_failure::cancelled
				   && cancelled.error().Transmission == application::github_publication_transmission::indeterminate,
			   "cancelling after the request left does not claim it never happened");
		expect(std::ranges::none_of(transport.Calls, [](const github_publication_wire_call& aCall) { return aCall.Wire.Method != http_method::get && aCall.Wire.Method != http_method::post; }),
			   "no update, delete or force of any kind");
	}
}

auto main() -> int
{
	compiles_exact_requests();
	parses_only_what_was_asked();
	sessions_admit_one_authority();
	std::cout << "github-publication-contract:pass\n";
	return EXIT_SUCCESS;
}
