#include "bpr/application/review_mutation_coordinator.hpp"
#include "github_mutation_contract.hpp"
#include "review_comment_permalink.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

	template <typename Oid>
	[[nodiscard]] auto oid(const std::uint8_t aSeed) -> Oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto workbench_authority()
		-> application::workbench_pull_request_authority
	{
		return {
			.ApplicationSession    = core::application_session_id{ 3U },
			.Profile               = core::host_profile_id{ 10U },
			.ProfileRevision       = core::github_profile_revision{ 20U },
			.Host                  = core::git_host_id{ 11U },
			.HostAuthority         = opaque<core::host_authority_fingerprint>(13U),
			.Account               = core::github_account_id{ 40U },
			.Viewer                = core::github_viewer_actor_id{ 50U },
			.Repository            = core::repository_id{ 12U },
			.PullRequest           = core::pull_request_id{ 13U },
			.Number                = core::pull_request_number{ 1U },
			.Snapshot              = core::provider_snapshot_id{ 60U },
			.IdentityGeneration    = core::github_identity_generation{ 70U },
			.PullRequestGeneration = core::request_generation{ 80U },
		};
	}

	[[nodiscard]] auto selected_diff_context()
		-> application::workbench_selected_diff_context
	{
		const auto workbench = workbench_authority();
		const auto authority = core::pull_request_authority{
			.Profile         = workbench.Profile,
			.ProfileRevision = workbench.ProfileRevision,
			.Host            = workbench.Host,
			.HostAuthority   = workbench.HostAuthority,
			.Account         = workbench.Account,
			.Viewer          = workbench.Viewer,
			.Repository      = workbench.Repository,
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = workbench.PullRequest,
			.Number          = workbench.Number,
			.Scope           = opaque<core::pull_request_authority_scope_id>(15U),
		};
		const auto options = core::normalize_diff_options(core::diff_options{ });
		expect(options.has_value(), "default options normalize");
		const auto digest = core::options_digest(*options);
		expect(digest.has_value(), "default options have a digest");
		return {
			.ApplicationSession = workbench.ApplicationSession,
			.Authority          = authority,
			.Comparison         = {
				.Left = {
					.Identity = {
						.Authority   = authority.Scope,
						.Incarnation = opaque<core::epoch_catalog_incarnation_id>(16U),
						.Kind        = core::comparison_anchor_kind::epoch_base,
						.Epoch       = core::epoch_ordinal{ 1 },
					},
					.Commit       = commit(0x10U),
					.Availability = core::anchor_availability::available,
				},
				.Right = {
					.Identity = {
						.Authority   = authority.Scope,
						.Incarnation = opaque<core::epoch_catalog_incarnation_id>(16U),
						.Kind        = core::comparison_anchor_kind::epoch_head,
						.Epoch       = core::epoch_ordinal{ 1 },
					},
					.Commit       = commit(0x11U),
					.Availability = core::anchor_availability::available,
				},
			},
			.ComparisonRevision = core::comparison_revision{ 1U },
			.Options            = *options,
			.OptionsDigest      = *digest,
			.Algorithm          = core::supported_diff_algorithm_revision,
		};
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

	[[nodiscard]] auto permalink_source(const std::string& aPath = "dir/review.cpp") -> review_permalink_source
	{
		const auto context = selected_diff_context();
		auto       request = application::workbench_selected_diff_request{
			.Context            = { .Request = core::request_id{ 1U }, .Session = core::application_session_id{ 3U }, .Generation = core::request_generation{ 8U } },
			.WorkbenchAuthority = workbench_authority(),
			.Diff               = context,
			.File               = { .File = opaque<core::file_identity>(44U), .PresentationPath = aPath, .CurrentPath = aPath, .Change = application::workbench_file_change_kind::modified, .ContentKind = application::workbench_file_content_kind::text },
		};
		const auto blob = oid<core::git_blob_oid>(50U);
		return {
			.Target     = target(),
			.Endpoint   = application::make_workbench_selected_diff_endpoint_for_testing(request, core::diff_side::right, blob, aPath),
			.Comparison = { .Authority = context.Authority, .Repository = context.Authority.Repository, .PullRequest = context.Authority.PullRequest, .LeftAnchor = context.Comparison.Left.Identity, .RightAnchor = context.Comparison.Right.Identity, .LeftBlob = oid<core::git_blob_oid>(40U), .RightBlob = blob, .File = request.File.File, .Options = context.OptionsDigest, .Algorithm = context.Algorithm },
			.LineCount  = 100U,
		};
	}

	[[nodiscard]] auto permalink_anchor(const review_permalink_source& aSource, const std::uint32_t aLine = 12U) -> core::source_line_ref
	{
		return { .Comparison = core::comparison_digest(aSource.Comparison), .Side = core::diff_side::right, .Line = aLine };
	}

	[[nodiscard]] auto publication_request(const review_permalink_source& aSource) -> application::review_mutation_request
	{
		const auto actor    = application::review_mutation_acting_identity{ .Account = workbench_authority().Account, .DatabaseId = 22U, .NodeId = "U_fixture", .Login = "fixture-reviewer" };
		const auto anchor   = permalink_anchor(aSource);
		const auto proposal = core::review_publication_proposal{ .Draft = core::review_draft_id{ 31U }, .Anchor = anchor, .Body = "Literal review prose.\nSecond line.", .Disposition = core::review_disposition::comment };
		return { .Proposal = proposal, .Target = aSource.Target.Semantic, .ActingIdentity = actor.Account, .IdempotencyKey = application::idempotency_key_for(proposal, aSource.Target.Semantic), .LiveBinding = application::review_mutation_live_binding{ .Target = aSource.Target, .ActingIdentity = actor, .Anchor = { .Semantic = anchor, .Path = aSource.Endpoint->path().generic_string(), .Commit = aSource.Target.Semantic.ExpectedHead } }, .Source = application::review_mutation_source_binding{ .Endpoint = aSource.Endpoint, .Comparison = aSource.Comparison, .LineCount = aSource.LineCount } };
	}

	[[nodiscard]] auto created_response(const review_mutation_wire_request& aWire) -> review_mutation_wire_response
	{
		auto body = aWire.Body;
		body.pop_back();
		body += R"(,"id":44,"node_id":"PRRC_fixture_44","user":{"id":22,"node_id":"U_fixture","login":"fixture-reviewer"}})";
		return { .Status = 201, .Headers = { { .Name = "Content-Type", .Value = "application/json" } }, .Body = std::move(body) };
	}

	void test_wire_representations_and_classifier()
	{
		for (const auto server : { application::github_server_contract::github_com_2022_11_28, application::github_server_contract::ghes_3_18_2022_11_28 }) {
			auto source                  = permalink_source("dir/review.md");
			source.Target.ServerContract = server;
			if (server == application::github_server_contract::ghes_3_18_2022_11_28) {
				source.Target.Hostname = application::github_canonical_hostname{ "reviews.example.test" };
			}
			auto       request = publication_request(source);
			const auto native  = compile_review_comment_request(request, server);
			expect(native && native->Body.find(R"("line":13)") != std::string::npos && native->Body.find("subject_type") == std::string::npos, "native wire keeps original line fields");
			const auto nativeReceipt = parse_review_comment_response(created_response(*native), request);
			expect(nativeReceipt && nativeReceipt->RemoteSubject == application::review_remote_comment_subject::line, "native response keeps remote line subject");
			request.Representation = core::review_publication_representation::file_comment_with_permalink;
			const auto file        = compile_review_comment_request(request, server);
			expect(file && validate_outbound_review_mutation_request(*file), "file wire compiles and canonically revalidates");
			expect(file->Body.find(R"("subject_type":"file")") != std::string::npos && file->Body.find(R"("line":)") == std::string::npos
					   && file->Body.find(R"("side":)") == std::string::npos && file->Body.find("start_line") == std::string::npos && file->Body.find("start_side") == std::string::npos,
				   "file request omits every line/range side field");
			const auto fileReceipt = parse_review_comment_response(created_response(*file), request);
			expect(fileReceipt && fileReceipt->RemoteSubject == application::review_remote_comment_subject::file
					   && fileReceipt->Receipt.Representation == request.Representation && fileReceipt->Body == compile_review_permalink_body(source, request.Proposal.Anchor, request.Proposal.Body),
				   "file response keeps honest subject and exact public body");
			expect(!parse_review_comment_response(created_response(*native), request), "line response cannot confirm a file write");
			auto wrong = created_response(*file);
			wrong.Body.replace(wrong.Body.find("U_fixture"), 9U, "U_foreign");
			expect(!parse_review_comment_response(wrong, request), "foreign actor refuses a file receipt");
			request.Source.reset();
			expect(!compile_review_comment_request(request, server), "file write requires exact retained source");

			// Synthetic protocol evidence only: no live host is claimed to emit this tuple.
			const auto contract  = off_hunk_unsupported_contract{ .Server = server, .Resource = "PullRequestReviewComment", .Field = "line", .Code = "fixture_off_hunk_unsupported", .Message = "Fixture: no comment created for off-hunk line" };
			const auto rejection = review_mutation_wire_response{ .Status = 422, .Headers = { { .Name = "Content-Type", .Value = "application/json" } }, .Body = R"({"message":"Validation Failed","errors":[{"resource":"PullRequestReviewComment","field":"line","code":"fixture_off_hunk_unsupported","message":"Fixture: no comment created for off-hunk line"}]})" };
			expect(definitive_off_hunk_unsupported(rejection, *native, contract), "exact synthetic operation contract classifies");
			for (const auto status : { 0, 200, 201, 401, 403, 404, 429, 500, 503 }) {
				auto changed   = rejection;
				changed.Status = status;
				expect(!definitive_off_hunk_unsupported(changed, *native, contract), "status counterexamples never classify");
			}
			for (const auto body : { R"({"message":"Validation Failed"})", R"({"message":"Spam detected","errors":[]})", R"({"message":"Validation Failed","errors":[]})", "{", R"({"message":"Validation Failed","message":"Validation Failed","errors":[]})" }) {
				auto changed = rejection;
				changed.Body = body;
				expect(!definitive_off_hunk_unsupported(changed, *native, contract), "generic, spam, malformed and duplicate JSON never classify");
			}
			auto changed = rejection;
			changed.Headers.push_back({ .Name = "Retry-After", .Value = "1" });
			expect(!definitive_off_hunk_unsupported(changed, *native, contract), "rate response never classifies");
			auto revision = contract;
			++revision.ClassifierRevision;
			expect(!definitive_off_hunk_unsupported(rejection, *native, revision), "different classifier revision refuses");
			revision             = contract;
			revision.ApiRevision = "unknown";
			expect(!definitive_off_hunk_unsupported(rejection, *native, revision), "different API revision refuses");
			expect(!definitive_off_hunk_unsupported(rejection, *file, contract), "file write cannot be native unsupported evidence");
		}
	}

	void test_canonical_permalink_round_trip_and_bounds()
	{
		const auto source = permalink_source("dir/sp ace-雪%?#.MD");
		const auto anchor = permalink_anchor(source);
		const auto prose  = std::string{ "  Preserve \tUnicode 雪 and punctuation.\n\nSecond paragraph.\r\n" };
		const auto body   = compile_review_permalink_body(source, anchor, prose);
		const auto prefix = std::string{ "https://github.com/MikeMarcin/BPR-Test/blob/" }
			+ std::string{ core::canonical_hex(source.Target.Semantic.ExpectedHead).view() }
			+ "/dir/sp%20ace-%E9%9B%AA%25%3F%23.MD?plain=1#L13\n\n";
		expect(body && *body == prefix + prose, "canonical Markdown URL escapes each path segment and preserves prose");
		const auto parsed = parse_review_permalink_body(source, *body);
		expect(parsed && parsed->Anchor == anchor && parsed->Prose == prose, "permalink rehydrates exact source and literal prose");
		const auto plain = permalink_source();
		const auto first = compile_review_permalink_body(plain, permalink_anchor(plain, 0U), "First");
		const auto last  = compile_review_permalink_body(plain, permalink_anchor(plain, 99U), "Last");
		expect(first && first->find("#L1\n\n") != std::string::npos && first->find('?') == std::string::npos,
			   "ordinary source has no query and first line is one-based");
		expect(last && parse_review_permalink_body(plain, *last)->Anchor.Line == 99U, "last source line remains in range");
		expect(!compile_review_permalink_body(plain, permalink_anchor(plain, 100U), "Outside"), "out-of-range source cannot compile");
		const auto headerBytes = first->size() - 5U;
		const auto exact       = compile_review_permalink_body(plain, permalink_anchor(plain, 0U), std::string(maximum_review_mutation_body_bytes - headerBytes, 'x'));
		expect(exact && exact->size() == maximum_review_mutation_body_bytes && parse_review_permalink_body(plain, *exact), "whole wire body admits exact size limit");
		expect(!compile_review_permalink_body(plain, permalink_anchor(plain, 0U), std::string(maximum_review_mutation_body_bytes - headerBytes + 1U, 'x')),
			   "permalink overhead counts toward the wire body limit");
		for (const auto& invalid : std::vector<std::string>{ "", " \t", "\nprose", std::string{ "bad\0body", 8U }, std::string{ "bad\xFF" }, *first }) {
			expect(!compile_review_permalink_body(plain, permalink_anchor(plain), invalid), "empty, invalid, or ambiguous prose is refused");
		}
		auto historical                         = plain;
		historical.Target.Semantic.ExpectedHead = commit(0x22U);
		expect(!compile_review_permalink_body(historical, permalink_anchor(historical), "Historical"), "historical retained endpoint grants no current-head write");
		expect(parse_review_permalink_body(historical, *first).has_value(), "historical comment can use its exact retained blob");
		for (const auto& path : { "README.markdown", "README.MDOWN", "README.mkd", "README.mkdn", "README.mdwn", "README.mdx", "README.litcoffee" }) {
			const auto markdown = permalink_source(path);
			const auto text     = compile_review_permalink_body(markdown, permalink_anchor(markdown), "Review");
			expect(text && text->find("?plain=1#L13") != std::string::npos, "Markdown renderer suffixes request plain source");
		}
		auto enterprise                  = plain;
		enterprise.Target.Hostname       = application::github_canonical_hostname{ "reviews.example.test" };
		enterprise.Target.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		const auto enterpriseBody        = compile_review_permalink_body(enterprise, permalink_anchor(enterprise), "Review");
		expect(enterpriseBody && enterpriseBody->starts_with("https://reviews.example.test/")
				   && parse_review_permalink_body(enterprise, *enterpriseBody),
			   "GHES uses its connected web origin, with no API route");
		expect(!parse_review_permalink_body(plain, *enterpriseBody), "GHES URL cannot cross to GitHub.com authority");
		auto sha256          = plain;
		auto endpointRequest = sha256.Endpoint->request();
		auto bytes           = std::array<std::uint8_t, 32U>{ };
		bytes.fill(0x33U);
		const auto sha256Commit                      = core::git_commit_oid{ core::git_hash_algorithm::sha256, bytes };
		const auto sha256Blob                        = core::git_blob_oid{ core::git_hash_algorithm::sha256, bytes };
		sha256.Target.Semantic.ExpectedHead          = sha256Commit;
		sha256.Target.BaseOid                        = sha256Commit;
		endpointRequest.Diff.Comparison.Left.Commit  = sha256Commit;
		endpointRequest.Diff.Comparison.Right.Commit = sha256Commit;
		sha256.Endpoint                              = application::make_workbench_selected_diff_endpoint_for_testing(endpointRequest, core::diff_side::right, sha256Blob, "dir/review.cpp");
		sha256.Comparison.LeftBlob                   = sha256Blob;
		sha256.Comparison.RightBlob                  = sha256Blob;
		const auto sha256Body                        = compile_review_permalink_body(sha256, permalink_anchor(sha256), "Review");
		expect(sha256Body && sha256Body->find(std::string{ core::canonical_hex(sha256Commit).view() }) != std::string::npos
				   && parse_review_permalink_body(sha256, *sha256Body),
			   "full SHA-256 material round-trips without hash-algorithm coercion");
	}

	void test_permalink_rejects_foreign_noncanonical_and_unavailable_sources()
	{
		const auto source    = permalink_source("dir/review.md");
		const auto canonical = *compile_review_permalink_body(source, permalink_anchor(source), "Literal prose");
		const auto replace   = [&](const std::string_view aFrom, const std::string_view aTo) {
			auto body = canonical;
			body.replace(body.find(aFrom), aFrom.size(), aTo);
			expect(!parse_review_permalink_body(source, body), "foreign, injected or noncanonical URL yields no anchor");
		};
		replace("https://", "http://");
		replace("github.com", "github.com.evil.test");
		replace("github.com", "user@github.com");
		replace("github.com", "github.com:443");
		replace("MikeMarcin", "AnotherOwner");
		replace("BPR-Test", "AnotherRepository");
		replace(core::canonical_hex(source.Target.Semantic.ExpectedHead).view(), "1111111");
		replace("dir/review", "dir%2Freview");
		replace("dir/review", "dir/../dir/review");
		replace("dir/review", "dir/%72eview");
		replace("?plain=1", "?plain=1&evil=1");
		replace("?plain=1", "");
		replace("#L13", "#L013");
		replace("#L13", "#L0");
		replace("#L13", "#L101");
		replace("#L13", "#L4294967296");
		replace("#L13", "#L13-L14");
		replace("#L13", "#L+13");
		replace("\n\n", "\r\n\r\n");
		replace("\n\n", "\n\n\n");
		expect(!parse_review_permalink_body(source, "<" + canonical), "angle-wrapped link is not the bare grammar");
		expect(!parse_review_permalink_body(source, canonical + "\n" + canonical), "second code link is ambiguous");
		expect(!parse_review_permalink_body(source, std::string(maximum_review_mutation_body_bytes + 1U, 'x')), "oversized body returns no anchor");
		for (const auto& path : std::vector<std::string>{ "/absolute.cpp", "../escape.cpp", "dir/./file.cpp", "dir/../file.cpp", "dir/back\\slash.cpp", "dir/control\n.cpp", std::string(4097U, 'x'), std::string{ "dir/bad\xFF" } }) {
			const auto invalid = permalink_source(path);
			expect(!compile_review_permalink_body(invalid, permalink_anchor(invalid), "Prose"), "unsafe source path cannot compile");
		}
		auto unavailable = source;
		unavailable.Endpoint.reset();
		expect(!parse_review_permalink_body(unavailable, canonical), "unavailable retained blob cannot invent an anchor");
		unavailable                      = source;
		unavailable.Comparison.RightBlob = oid<core::git_blob_oid>(51U);
		expect(!parse_review_permalink_body(unavailable, canonical), "mismatched blob cannot invent an anchor");
		unavailable           = source;
		unavailable.LineCount = 0U;
		expect(!parse_review_permalink_body(unavailable, canonical), "unavailable text lines cannot invent an anchor");
		unavailable                            = source;
		unavailable.Target.Semantic.Repository = core::repository_id{ 99U };
		expect(!parse_review_permalink_body(unavailable, canonical), "foreign source authority cannot invent an anchor");
	}
} // namespace

auto main() -> int
{
	test_canonical_permalink_round_trip_and_bounds();
	test_wire_representations_and_classifier();
	test_permalink_rejects_foreign_noncanonical_and_unavailable_sources();
	std::cout << "review-comment-permalink-tests:pass\n";
	return EXIT_SUCCESS;
}
