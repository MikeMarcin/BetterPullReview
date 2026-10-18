#include "../sqlite_contract/scratch_root.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/application/private_publication_application_host.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"
#include "cli_account_wrapper_support.hpp"
#include "fixture_json.hpp"
#include "github_mutation_contract.hpp"
#include "product_diff.hpp"
#include "review_comment_permalink.hpp"
#include <sqlite3.h>
#include <stdexcept>

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
			.Endpoint   = application::make_workbench_selected_diff_endpoint_for_testing(request, core::diff_side::right, blob, std::filesystem::path{ std::u8string{ aPath.begin(), aPath.end() } }),
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
		if (aWire.Body.find(R"("subject_type":"file")") != std::string::npos) {
			body += R"(,"line":1,"original_line":1,"side":"RIGHT","start_line":null,"start_side":null)";
		}
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
			auto       absentPosition = created_response(*file);
			const auto placeholder    = std::string{ R"(,"line":1,"original_line":1,"side":"RIGHT","start_line":null,"start_side":null)" };
			absentPosition.Body.erase(absentPosition.Body.find(placeholder), placeholder.size());
			expect(parse_review_comment_response(absentPosition, request).has_value(), "absent file position metadata remains supported");
			expect(fileReceipt->Anchor.Semantic == request.Proposal.Anchor && fileReceipt->Anchor.Semantic.Line == 12U,
				   "synthetic line1 never replaces the exact semantic permalink anchor");
			for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
					 { R"("line":1)", R"("line":0)" }, { R"("line":1)", R"("line":2)" }, { R"("original_line":1)", R"("original_line":2)" }, { R"("original_line":1)", R"("original_line":null)" }, { R"("side":"RIGHT")", R"("side":"LEFT")" }, { R"("start_line":null)", R"("start_line":1)" }, { R"("start_side":null)", R"("start_side":"RIGHT")" }, { R"("subject_type":"file")", R"("subject_type":"line")" }, { R"("subject_type":"file")", R"("subject_type":"file","in_reply_to_id":99)" } }) {
				auto changed = created_response(*file);
				changed.Body.replace(changed.Body.find(from), from.size(), to);
				expect(!parse_review_comment_response(changed, request), "noncanonical file positions, ranges, replies and subjects cannot confirm fallback");
			}

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
		auto unsafe = std::vector<std::string>{ "/absolute.cpp", "../escape.cpp", "dir/./file.cpp", "dir/../file.cpp", "dir/back\\slash.cpp", "dir/control\n.cpp", std::string(4097U, 'x') };
#if !defined(_WIN32)
		// A Windows path is UTF-16, so bytes that are not UTF-8 cannot become one.
		unsafe.emplace_back("dir/bad\xFF");
#endif
		for (const auto& path : unsafe) {
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
	using journal_state    = core::review_publication_state;
	using journal_record   = application::review_publication_recovery_record;
	using recovery_failure = application::review_mutation_recovery_failure;
	class recording_journal final : public application::review_mutation_recovery_port
	{
	public:
		explicit recording_journal(std::unique_ptr<application::review_mutation_recovery_port> aInner)
			: Inner{ std::move(aInner) }
		{ }
		auto load(const std::string& aKey) -> std::expected<std::optional<application::review_mutation_recovery_record>, recovery_failure> override { return Inner->load(aKey); }
		auto admit(const application::review_mutation_recovery_record& aRecord) -> std::expected<application::review_mutation_recovery_admission, recovery_failure> override { return Inner->admit(aRecord); }
		auto mark_possibly_transmitted(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<application::review_mutation_recovery_record, recovery_failure> override { return Inner->mark_possibly_transmitted(aKey, aFingerprint); }
		auto resolve(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const application::review_mutation_receipt& aReceipt) -> std::expected<application::review_mutation_recovery_record, recovery_failure> override { return Inner->resolve(aKey, aFingerprint, aReceipt); }
		auto load_publication(const std::string& aKey) -> std::expected<std::optional<journal_record>, recovery_failure> override { return Inner->load_publication(aKey); }
		auto advance_publication(const std::optional<journal_record>& aExpected, const journal_record& aNext) -> std::expected<journal_record, recovery_failure> override
		{
			if (InterruptBefore == aNext.State) {
				Interrupted = true;
				throw std::runtime_error{ "simulated process interruption before durable commit" };
			}
			auto saved = Inner->advance_publication(aExpected, aNext);
			if (saved) {
				Saved.push_back(*saved);
				if (InterruptAfter == saved->State) {
					Interrupted = true;
					throw std::runtime_error{ "simulated process interruption after durable commit" };
				}
			}
			return saved;
		}
		std::unique_ptr<application::review_mutation_recovery_port> Inner;
		std::vector<journal_record>                                 Saved;
		std::optional<journal_state>                                InterruptAfter;
		std::optional<journal_state>                                InterruptBefore;
		bool                                                        Interrupted = false;
	};
	class publication_lease final : public application::review_mutation_transport_lease
	{
	public:
	};
	[[nodiscard]] auto fixture_contract(const application::github_server_contract aServer) -> off_hunk_unsupported_contract
	{
		return { .Server = aServer, .Resource = "PullRequestReviewComment", .Field = "line", .Code = "fixture_off_hunk_unsupported", .Message = "Fixture: no comment created for off-hunk line" };
	}
	[[nodiscard]] auto fixture_rejection() -> review_mutation_wire_response
	{
		return { .Status = 422, .Headers = { { .Name = "Content-Type", .Value = "application/json" } }, .Body = R"({"message":"Validation Failed","errors":[{"resource":"PullRequestReviewComment","field":"line","code":"fixture_off_hunk_unsupported","message":"Fixture: no comment created for off-hunk line"}]})" };
	}
	[[nodiscard]] auto recorded_rejection() -> review_mutation_wire_response
	{
		return { .Status = 422, .Headers = { { "Content-Type", "application/json; charset=utf-8" }, { "X-RateLimit-Limit", "5000" }, { "X-RateLimit-Remaining", "4990" }, { "X-RateLimit-Reset", "2000000000" }, { "X-RateLimit-Resource", "core" } }, .Body = R"({"message":"Validation Failed","documentation_url":"https://docs.github.com/rest/pulls/comments#create-a-review-comment-for-a-pull-request","status":"422","errors":[{"resource":"PullRequestReviewComment","field":"pull_request_review_thread.line","code":"custom","message":"could not be resolved"}]})" };
	}
	[[nodiscard]] auto recorded_request() -> application::review_mutation_request
	{
		auto source                                    = permalink_source("off_hunk_fixture.md");
		source.Target.Number                           = core::pull_request_number{ 2U };
		source.Target.PullRequestDatabaseId            = 4'650'736'621U;
		source.Target.PullRequestNodeId                = "PR_kwDOTyvof88AAAABFTSb7Q";
		source.Target.BaseRef                          = "codex/rfc0022-fixture-base-20260926";
		source.Target.HeadRef                          = "codex/rfc0022-live-comments-20260926";
		source.Target.BaseOid                          = *core::parse_git_commit_oid("a939a85a5525994fe9e9cb4c973ad9362ed427c7");
		source.Target.Semantic.ExpectedHead            = *core::parse_git_commit_oid("61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a");
		auto endpoint                                  = source.Endpoint->request();
		endpoint.WorkbenchAuthority.Number             = source.Target.Number;
		endpoint.Diff.Authority.Number                 = source.Target.Number;
		endpoint.Diff.Comparison.Left.Commit           = source.Target.BaseOid;
		endpoint.Diff.Comparison.Right.Commit          = source.Target.Semantic.ExpectedHead;
		const auto blob                                = *core::parse_git_blob_oid("828d5c87c20a0f9d74c498e04c36b32d2948b7c5");
		source.Endpoint                                = application::make_workbench_selected_diff_endpoint_for_testing(endpoint, core::diff_side::right, blob, "off_hunk_fixture.md");
		source.Comparison.Authority                    = endpoint.Diff.Authority;
		source.Comparison.RightBlob                    = blob;
		source.LineCount                               = 80U;
		auto request                                   = publication_request(source);
		request.LiveBinding->ActingIdentity.DatabaseId = 7'605'339U;
		request.LiveBinding->ActingIdentity.NodeId     = "MDQ6VXNlcjc2MDUzMzk=";
		request.LiveBinding->ActingIdentity.Login      = "MikeMarcin";
		request.Proposal.Anchor.Line                   = 61U;
		request.LiveBinding->Anchor.Semantic           = request.Proposal.Anchor;
		request.IdempotencyKey                         = application::idempotency_key_for(request.Proposal, request.Target);
		return request;
	}

	class publication_transport final : public github_mutation_wire_transport
	{
	public:
		auto perform_review_mutation(const github_mutation_wire_request& aRequest, const std::stop_token aStop, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			boundary();
			using operation  = review_mutation_operation;
			const auto& wire = aRequest.Wire;
			expect(validate_outbound_review_mutation_request(wire).has_value(), "session emits canonical request");
			Operations.push_back(wire.Operation);
			auto response = review_mutation_wire_response{ .Status = 200, .Headers = { { .Name = "Content-Type", .Value = "application/json" } } };
			// [[BPR-1572]]. A stacked course: the base branch names another merge
			// base than the comparison base, the recorded base's is the
			// comparison base and the branch only moved forward. `ResetAtTarget`:
			// from that pull request read on, GitHub names the branch's tip as
			// the recorded base.
			if (Stacked && (wire.Operation == operation::observe_merge_base || wire.Operation == operation::observe_recorded_merge_base || wire.Operation == operation::observe_base_ancestry)) {
				const auto recorded  = std::string{ "1010101010101010101010101010101010101010" };
				const auto head      = std::string(40U, '1');
				const auto tip       = std::string(40U, 'b');
				const auto pair      = wire.Operation == operation::observe_merge_base ? "main..." + head : wire.Operation == operation::observe_recorded_merge_base ? recorded + "..." + head
																																									 : recorded + "..." + tip;
				const auto mergeBase = wire.Operation == operation::observe_merge_base ? std::string(40U, 'c') : wire.Operation == operation::observe_recorded_merge_base ? std::string{ "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a" }
																																										  : recorded;
				response.Body        = R"({"url":"https://api.github.com/repos/MikeMarcin/BPR-Test/compare/)" + pair + R"(",)" + (wire.Operation == operation::observe_recorded_merge_base ? std::string{ R"("ahead_by":2,)" } : std::string{ })
					+ R"("base_commit":{"sha":")" + (wire.Operation == operation::observe_merge_base ? tip : recorded) + R"("},"merge_base_commit":{"sha":")" + mergeBase + R"("}})";
				boundary();
				return github_mutation_wire_success{ .Response = std::move(response) };
			}
			if (wire.Operation == operation::observe_viewer) {
				response.Body = WrongActor ? R"({"id":23,"node_id":"U_foreign","login":"foreign"})" : R"({"id":22,"node_id":"U_fixture","login":"fixture-reviewer"})";
				if (RecordedIdentity && !WrongActor) {
					response.Body = R"({"id":7605339,"node_id":"MDQ6VXNlcjc2MDUzMzk=","login":"MikeMarcin"})";
				}
			} else if (wire.Operation == operation::observe_pull_request) {
				++Targets;
				response.Body = R"({"id":4246446933,"node_id":"PR_kwDOTyvof879G6NV","number":1,"state":"open","base":{"sha":"1010101010101010101010101010101010101010","ref":"main","repo":{"id":1328277631,"node_id":"R_kgDOTyvofw","full_name":"MikeMarcin/BPR-Test"}},"head":{"sha":")";
				response.Body += (StaleAtTarget != 0U && Targets >= StaleAtTarget) ? "1212121212121212121212121212121212121212" : "1111111111111111111111111111111111111111";
				response.Body += R"(","ref":"codex/disposable-live-review-20260810"}})";
				if (Stacked) {
					response.Body.insert(response.Body.find(R"("base")"), R"("commits":2,)");
					if (ResetAtTarget != 0U && Targets >= ResetAtTarget) {
						response.Body.replace(response.Body.find("1010101010101010101010101010101010101010"), 40U, std::string(40U, 'b'));
					}
				}
				if (RecordedIdentity && (StaleAtTarget == 0U || Targets < StaleAtTarget)) {
					response.Body = R"({"id":4650736621,"node_id":"PR_kwDOTyvof88AAAABFTSb7Q","number":2,"state":"open","base":{"sha":"a939a85a5525994fe9e9cb4c973ad9362ed427c7","ref":"codex/rfc0022-fixture-base-20260926","repo":{"id":1328277631,"node_id":"R_kgDOTyvofw","full_name":"MikeMarcin/BPR-Test"}},"head":{"sha":"61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a","ref":"codex/rfc0022-live-comments-20260926"}})";
				}
			} else if (wire.Operation == operation::observe_review_comments) {
				if (HideAfterPost && !Comments.empty()) {
					return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable };
				}
				response.Body = "[";
				for (const auto& item : Comments) {
					if (response.Body.size() > 1U) {
						response.Body += ',';
					}
					response.Body += item;
				}
				response.Body += ']';
			} else {
				expect(wire.Operation == operation::create_review_comment, "only root write in publication fixture");
				expect(!aStop.stop_requested(), "cancellation checked before POST");
				// The viewer, then the exact target, immediately precede each POST;
				// a LEFT line's merge-base reads ([[BPR-1572]]) sit between the
				// target and the target read again, which is last.
				const auto viewer = std::ranges::find(Operations.rbegin(), Operations.rend(), operation::observe_viewer);
				expect(Operations.size() >= 3U && Operations[Operations.size() - 2U] == operation::observe_pull_request && viewer != Operations.rend()
						   && std::all_of(Operations.rbegin() + 1, viewer, [](const operation aRead) {
								  return aRead == operation::observe_pull_request || aRead == operation::observe_merge_base || aRead == operation::observe_recorded_merge_base || aRead == operation::observe_base_ancestry;
							  }),
					   "viewer then exact target immediately precede each POST");
				boundary();
				if (!aWitness || !aWitness()) {
					return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable };
				}
				boundary();
				expect(!aWitness(), "transmission witness cannot be consumed twice");
				Posts.push_back(wire);
				const bool file = std::get<application::review_mutation_request>(wire.Semantic).Representation == core::review_publication_representation::file_comment_with_permalink;
				if ((!file && RejectNative) || GenericStatus != 0) {
					response = Rejection.value_or(fixture_rejection());
					if (GenericStatus != 0) {
						response.Status = GenericStatus;
						response.Body   = R"({"message":"Validation Failed"})";
					}
				} else {
					response = created_response(wire);
					if (RecordedIdentity) {
						const auto actor = response.Body.find(R"("user":)");
						response.Body.replace(actor, response.Body.size() - actor, R"("user":{"id":7605339,"node_id":"MDQ6VXNlcjc2MDUzMzk=","login":"MikeMarcin"}})");
					}
					response.Body.replace(response.Body.find("PRRC_fixture_44"), 15U, "PRRC_fixture_" + std::to_string(Posts.size() + 100U));
					if (Publish) {
						Comments.push_back(response.Body);
						if (AfterPost) {
							AfterPost();
						}
					}
				}
				if (CancelAfterPost) {
					CancelAfterPost->request_stop();
				}
				if (LoseResponse) {
					return github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate };
				}
			}
			boundary();
			return github_mutation_wire_success{ .Response = std::move(response) };
		}
		void boundary()
		{
			++Boundaries;
			if (InterruptAt == Boundaries) {
				InterruptAt.reset();
				throw std::runtime_error{ "simulated process interruption at wire boundary" };
			}
		}
		std::size_t                                  Boundaries = 0U;
		std::optional<std::size_t>                   InterruptAt;
		std::vector<review_mutation_operation>       Operations;
		std::vector<review_mutation_wire_request>    Posts;
		std::vector<std::string>                     Comments;
		bool                                         RecordedIdentity = false;
		std::optional<review_mutation_wire_response> Rejection;
		bool                                         RejectNative    = false;
		bool                                         LoseResponse    = false;
		bool                                         HideAfterPost   = false;
		bool                                         Publish         = true;
		bool                                         WrongActor      = false;
		unsigned                                     Targets         = 0U;
		unsigned                                     StaleAtTarget   = 0U;
		bool                                         Stacked         = false;
		unsigned                                     ResetAtTarget   = 0U;
		int                                          GenericStatus   = 0;
		std::stop_source*                            CancelAfterPost = nullptr;
		std::function<void()>                        AfterPost;
	};
	[[nodiscard]] auto session_configuration(const application::review_mutation_request& aRequest) -> live_review_mutation_configuration
	{
		return { .CredentialContext = { .Request = core::request_id{ 1 }, .ApplicationSession = core::application_session_id{ 3 }, .Authority = { .Profile = core::host_profile_id{ 10 }, .ProfileRevision = application::github_profile_revision{ 20 }, .Host = aRequest.Target.Host, .Hostname = aRequest.LiveBinding->Target.Hostname, .Account = aRequest.ActingIdentity, .ServerContract = aRequest.LiveBinding->Target.ServerContract }, .IdentityGeneration = core::github_identity_generation{ 70 } }, .Target = aRequest.LiveBinding->Target, .ActingIdentity = aRequest.LiveBinding->ActingIdentity };
	}
	[[nodiscard]] auto open_journal(const std::filesystem::path& aRoot) -> std::unique_ptr<recording_journal>
	{
		auto store = adapters::sqlite::make_review_mutation_recovery_store({ .StorageRoot = aRoot, .Host = "github.com", .Account = "fixture-reviewer", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
		expect(store.has_value(), "publication SQLite journal opens");
		return std::make_unique<recording_journal>(std::move(*store));
	}
	[[nodiscard]] auto recorded_configuration(const application::review_mutation_request& aRequest) -> live_review_mutation_configuration
	{
		auto config                                   = session_configuration(aRequest);
		config.CredentialContext.Authority.BprProfile = core::bpr_profile_id{ 2U };
		return config;
	}
	void test_recorded_contract_scope_and_protocol()
	{
		const auto request  = recorded_request();
		const auto config   = recorded_configuration(request);
		const auto contract = recorded_off_hunk_contract(config);
		const auto wire     = compile_review_comment_request(request);
		expect(contract && wire && definitive_off_hunk_unsupported(recorded_rejection(), *wire, *contract), "owner-approved exact shape and immutable source classify");
		for (unsigned change = 0; change < 10U; ++change) {
			auto other = config;
			switch (change) {
			case 0:  other.CredentialContext.Authority.BprProfile = core::bpr_profile_id{ 1U }; break;
			case 1:  other.Target.Number = core::pull_request_number{ 3U }; break;
			case 2:  other.Target.RepositoryDatabaseId++; break;
			case 3:  other.Target.PullRequestNodeId += "other"; break;
			case 4:  other.Target.BaseOid = commit(0x12U); break;
			case 5:  other.Target.Semantic.ExpectedHead = commit(0x12U); break;
			case 6:  other.ActingIdentity.DatabaseId++; break;
			case 7:  other.ActingIdentity.Login = "other"; break;
			case 8:  other.Target.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28; break;
			default: other.Target.Open = false; break;
			}
			expect(!recorded_off_hunk_contract(other), "foreign profile/PR/repository/revision/actor/server has no recorded contract");
		}
		for (unsigned change = 0; change < 7U; ++change) {
			auto other = request;
			switch (change) {
			case 0:  other.Source->LineCount = 81U; break;
			case 1:  other.Source->Comparison.RightBlob = oid<core::git_blob_oid>(51U); break;
			case 2:  other.Source->Endpoint.reset(); break;
			case 3:  other.Source->Placement = core::review_placement::legacy_line; break;
			case 4:  other.LiveBinding->Anchor.Semantic.Line = other.Proposal.Anchor.Line = 9U; break;
			case 5:  other.LiveBinding->Anchor.Semantic.Line = other.Proposal.Anchor.Line = 80U; break;
			default: other.LiveBinding->Anchor.Path = "other.md"; break;
			}
			const auto candidate = compile_review_comment_request(other);
			expect(!candidate || !definitive_off_hunk_unsupported(recorded_rejection(), *candidate, *contract), "missing/stale/hunk/out-of-range source cannot borrow recorded patch proof");
		}
		auto       refusals = std::vector<review_mutation_wire_response>{ };
		const auto replace  = [&refusals](const std::string_view aFrom, const std::string_view aTo) {
			auto response = recorded_rejection();
			response.Body.replace(response.Body.find(aFrom), aFrom.size(), aTo);
			refusals.push_back(std::move(response));
		};
		replace("could not be resolved", "could not be resolved.");
		replace("pull_request_review_thread.line", "line");
		replace("PullRequestReviewComment", "PullRequest");
		replace("custom", "invalid");
		replace("Validation Failed", "Spam detected");
		replace(R"("status":"422")", R"("status":422)");
		replace(R"("status":"422")", R"("status":null)");
		replace(R"("status":"422",)", "");
		replace("https://docs.github.com/", "https://example.test/");
		replace(R"("status":"422")", R"("status":"422","status":"422")");
		replace(R"("status":"422")", R"("status":"422","extra":true)");
		replace(R"("code":"custom")", R"("code":"custom","extra":true)");
		replace(R"("code":"custom")", R"("code":"custom","code":"custom")");
		replace("}]}", "},{}]}");
		for (const auto& header : std::vector<http_header>{ { "Retry-After", "1" }, { "lOcAtIoN", "https://github.com/" }, { "x-ratelimit-remaining", "4990" }, { "Content-Encoding", "gzip" } }) {
			auto response = recorded_rejection();
			response.Headers.push_back(header);
			refusals.push_back(std::move(response));
		}
		for (const auto& [index, value] : std::vector<std::pair<std::size_t, std::string>>{ { 0U, "text/plain" }, { 1U, "invalid" }, { 2U, "0" }, { 2U, "5001" }, { 3U, "0" }, { 4U, "search" } }) {
			auto response                 = recorded_rejection();
			response.Headers[index].Value = value;
			refusals.push_back(std::move(response));
		}
		for (const auto status : { 201, 403, 429, 500 }) {
			auto response   = recorded_rejection();
			response.Status = status;
			refusals.push_back(std::move(response));
		}
		auto missing = recorded_rejection();
		missing.Headers.pop_back();
		refusals.push_back(std::move(missing));
		for (const auto& response : refusals) {
			expect(!definitive_off_hunk_unsupported(response, *wire, *contract), "recorded shape and metadata counterexample refuses");
			auto scratch               = tests::sqlite_contract::scratch_root{ "recorded-refusal" };
			auto transport             = publication_transport{ };
			transport.RecordedIdentity = true;
			transport.RejectNative     = true;
			transport.Rejection        = response;
			{
				auto session = review_mutation_session{ transport, config, std::make_shared<publication_lease>(), open_journal(scratch.path()) };
				static_cast<void>(session.submit(request));
			}
			auto session = review_mutation_session{ transport, config, std::make_shared<publication_lease>(), open_journal(scratch.path()) };
			static_cast<void>(session.submit(request));
			expect(transport.Posts.size() == 1U, "nearby error and restart never authorize a fallback or retry POST");
		}
		auto scratch               = tests::sqlite_contract::scratch_root{ "recorded-success" };
		auto transport             = publication_transport{ };
		transport.RecordedIdentity = true;
		transport.RejectNative     = true;
		transport.Rejection        = recorded_rejection();
		auto        journal        = open_journal(scratch.path());
		const auto* observed       = journal.get();
		auto        session        = review_mutation_session{ transport, config, std::make_shared<publication_lease>(), std::move(journal) };
		const auto  result         = session.submit(request);
		expect(result && result->Representation == core::review_publication_representation::file_comment_with_permalink && transport.Posts.size() == 2U && transport.Comments.size() == 1U, "default production protocol negotiates native rejection then exactly one file comment");
		expect(observed->Saved.back().State == journal_state::succeeded_fallback && observed->Saved.back().Authorization.Source == application::review_fallback_authorization_source::native_rejection, "fallback uses the atomic native rejection journal authority");
		expect(session.submit(request)->Duplicate && transport.Posts.size() == 2U, "completed recorded publication replay has no POST");
		// An earlier build with no selected contract left a transmission witness.
		// Loading the very same logical request under rev2 must remain GET-only.
		auto oldRoot             = tests::sqlite_contract::scratch_root{ "recorded-old-witness" };
		auto oldWire             = publication_transport{ };
		oldWire.RecordedIdentity = true;
		oldWire.RejectNative     = true;
		oldWire.Rejection        = recorded_rejection();
		{
			auto oldSession = review_mutation_session{ oldWire, config, std::make_shared<publication_lease>(), open_journal(oldRoot.path()), off_hunk_unsupported_contract{ } };
			expect(!oldSession.submit(request), "old disabled classifier retains unresolved witness");
		}
		auto newSession = review_mutation_session{ oldWire, config, std::make_shared<publication_lease>(), open_journal(oldRoot.path()) };
		expect(!newSession.submit(request) && oldWire.Posts.size() == 1U, "new recorded contract cannot retroactively reject an old native attempt");
		std::cout << "recorded-contract-refusal-courses:" << refusals.size() << '\n';
	}

	void next_draft(application::review_mutation_request& aRequest)
	{
		aRequest.Proposal.Draft = core::review_draft_id{ aRequest.Proposal.Draft.value() + 1U };
		aRequest.IdempotencyKey = application::idempotency_key_for(aRequest.Proposal, aRequest.Target);
	}
	void test_session_negotiation_and_cache()
	{
		for (const auto server : { application::github_server_contract::github_com_2022_11_28, application::github_server_contract::ghes_3_18_2022_11_28 }) {
			auto source                  = permalink_source();
			source.Target.ServerContract = server;
			if (server == application::github_server_contract::ghes_3_18_2022_11_28) {
				source.Target.Hostname = application::github_canonical_hostname{ "reviews.example.test" };
			}
			auto request              = publication_request(source);
			request.Source->Placement = core::review_placement::off_hunk;
			auto  scratch             = tests::sqlite_contract::scratch_root{ "publication" };
			auto  journal             = open_journal(scratch.path());
			auto* observed            = journal.get();
			auto  transport           = publication_transport{ };
			transport.RejectNative    = true;
			auto       session        = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), std::move(journal), fixture_contract(server) };
			const auto first          = session.submit_and_reconcile(request);
			expect(first.Result && first.Result->Representation == core::review_publication_representation::file_comment_with_permalink && transport.Posts.size() == 2U, "exact synthetic rejection negotiates native then file");
			expect(transport.Targets == 4U && first.Reconciliation && first.Reconciliation->size() == 1U, "each child checks target twice and refreshes server");
			expect(observed->Saved.size() == 6U && observed->Saved[2].State == journal_state::native_definitively_unsupported && !observed->Saved[2].Fallback, "rejection is durable before fallback exists");
			expect(observed->Saved.back().Native && observed->Saved.back().Fallback && observed->Saved.back().Native->IdempotencyKey != observed->Saved.back().Fallback->IdempotencyKey, "one logical parent has distinct immutable children");
			const auto duplicate = session.submit(request);
			expect(duplicate && duplicate->Duplicate && transport.Posts.size() == 2U, "same gesture reuses durable receipt without POST");
			next_draft(request);
			const auto cached = session.submit(request);
			expect(cached && transport.Posts.size() == 3U && observed->Saved[6].State == journal_state::unsupported_evidence_admitted && !observed->Saved[6].Native, "fresh exact-anchor evidence admits fallback without invented native rejection");
			request.Source->Placement = core::review_placement::legacy_line;
			next_draft(request);
			expect(!session.submit(request) && transport.Posts.size() == 4U && transport.Posts.back().Body.find("subject_type") == std::string::npos, "legacy line stays native despite cached unsupported");
			request.Source->Placement = core::review_placement::off_hunk;
			++request.Proposal.Anchor.Line;
			request.LiveBinding->Anchor.Semantic = request.Proposal.Anchor;
			next_draft(request);
			expect(session.submit(request).has_value() && transport.Posts.size() == 6U, "a different anchor cannot borrow cached unsupported evidence");
		}
	}
	/// [[BPR-1572]]. A LEFT comment through the source-backed publication
	/// child on a stacked pull request: admission observes the target, the
	/// baseline comment read follows, then the child observes it again, last,
	/// before its POST. GitHub replacing the recorded base with the branch's
	/// tip during that last observation's merge-base reads (the target read
	/// again names the tip), or during the baseline read, sends nothing; with
	/// no reset the comment is sent.
	void test_left_source_backed_child_on_a_stacked_pull_request()
	{
		for (const unsigned resetAt : { 0U, 2U, 3U, 4U }) {
			auto source                          = permalink_source();
			source.Target.ComparisonBaseOid      = commit(0x5aU);
			auto endpoint                        = source.Endpoint->request();
			endpoint.Diff.Comparison.Left.Commit = commit(0x5aU);
			source.Endpoint                      = application::make_workbench_selected_diff_endpoint_for_testing(endpoint, core::diff_side::left, oid<core::git_blob_oid>(40U), "dir/review.cpp");
			auto request                         = publication_request(source);
			request.Proposal.Anchor.Side         = core::diff_side::left;
			request.LiveBinding->Anchor.Semantic = request.Proposal.Anchor;
			request.IdempotencyKey               = application::idempotency_key_for(request.Proposal, request.Target);
			auto scratch                         = tests::sqlite_contract::scratch_root{ "stacked-publication" };
			auto transport                       = publication_transport{ };
			transport.Stacked                    = true;
			transport.ResetAtTarget              = resetAt;
			auto       session                   = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()) };
			const auto result                    = session.submit_and_reconcile(request);
			const auto ancestries                = std::ranges::count(transport.Operations, review_mutation_operation::observe_base_ancestry);
			if (resetAt == 0U) {
				expect(result.Result && transport.Posts.size() == 1U && ancestries == 2 && transport.Targets >= 4U,
					   "a stacked LEFT comment through the source-backed child is sent after two complete observations");
			} else {
				expect(!result.Result && transport.Posts.empty(), "a recorded base replaced before the child's last read sends nothing");
			}
		}
	}
	/// [[BPR-1572]]. The same comment through the production GitHub CLI
	/// account wrapper: after the child's last read of the pull request (its
	/// fourth), the wrapper checks the account and reads the pull request once
	/// more, which now names another recorded base. The comment is refused
	/// unsent, and the session says why: `target_moved`, no attempt.
	void test_left_source_backed_child_through_the_cli_account_wrapper()
	{
		auto source                          = permalink_source();
		source.Target.ComparisonBaseOid      = commit(0x5aU);
		auto endpoint                        = source.Endpoint->request();
		endpoint.Diff.Comparison.Left.Commit = commit(0x5aU);
		source.Endpoint                      = application::make_workbench_selected_diff_endpoint_for_testing(endpoint, core::diff_side::left, oid<core::git_blob_oid>(40U), "dir/review.cpp");
		auto request                         = publication_request(source);
		request.Proposal.Anchor.Side         = core::diff_side::left;
		request.LiveBinding->Anchor.Semantic = request.Proposal.Anchor;
		request.IdempotencyKey               = application::idempotency_key_for(request.Proposal, request.Target);
		auto scratch                         = tests::sqlite_contract::scratch_root{ "stacked-cli-publication" };
		auto transport                       = publication_transport{ };
		transport.Stacked                    = true;
		transport.ResetAtTarget              = 5U;
		const auto configuration             = session_configuration(request);
		auto       cli                       = tests::github_contract::make_cli_account_wrapper(transport, configuration.CredentialContext, "fixture-reviewer");
		expect(cli.Transport != nullptr, "the production wrapper is built for the fixture account");
		auto       session = review_mutation_session{ *cli.Transport, configuration, std::make_shared<publication_lease>(), open_journal(scratch.path()) };
		const auto result  = session.submit_and_reconcile(request);
		expect(!result.Result && result.Result.error() == application::review_mutation_failure::target_moved && !result.MutationAttempted && transport.Posts.empty()
				   && transport.Targets == 5U && cli.Inner->AccountReads > 0U,
			   "through the CLI account wrapper a recorded base replaced before the write refuses the source-backed comment unsent and says why");
	}
	class publication_log final : public application::diagnostic_log_port
	{
	public:
		std::vector<application::diagnostic_event> Events;
		auto                                       record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			try {
				Events.push_back(aEvent);
			} catch (...) {
				return application::diagnostic_log_status::dropped;
			}
			return application::diagnostic_log_status::available;
		}
		auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
		auto context_generation() const noexcept -> std::uint64_t override { return 7U; }
	};
	void test_publication_diagnostic_privacy_and_authority()
	{
		auto request              = publication_request(permalink_source());
		request.Source->Placement = core::review_placement::off_hunk;
		const auto wire           = *compile_review_comment_request(request, request.LiveBinding->Target.ServerContract);
		auto       rejection      = fixture_rejection();
		rejection.Body            = R"({"message":"Validation Failed","errors":[{"resource":"PullRequestReviewComment","field":"line","code":"invalid","message":"line must be part of the diff"}]})";
		const auto event          = review_comment_diagnostic(wire, github_mutation_wire_success{ rejection });
		expect(event.Operation == application::diagnostic_operation::review_comment && event.HttpStatus == 422U && event.Summary.text().find("resource=PullRequestReviewComment field=line code=invalid message=line must be part of the diff") != std::string_view::npos, "closed rejection fields survive as observation only");
		for (const auto message : { "could not be resolved", "could not be resolved.", "must be part of the diff", "is not part of the diff.", "pull_request_review_thread.line must be part of the diff", "`pull_request_review_thread.line` must be part of the diff.", "'pull_request_review_thread.line' is not part of the diff" }) {
			rejection.Body      = std::string{ R"({"message":"Validation Failed","errors":[{"resource":"PullRequestReviewComment","field":"pull_request_review_thread.line","code":"custom","message":")" } + message + R"("}],"documentation_url":"https://docs.github.com/rest/pulls/comments#create-a-review-comment-for-a-pull-request","status":"422"})";
			const auto observed = review_comment_diagnostic(wire, github_mutation_wire_success{ rejection });
			expect(observed.Summary.text().find(std::string{ "message=" } + message) != std::string_view::npos && observed.Summary.text().find("field=pull_request_review_thread.line code=custom") != std::string_view::npos && observed.Summary.text().find("envelope_unknown=0") != std::string_view::npos, "exact recognized spelling survives without granting authority");
		}
		for (const auto& [member, expected] : std::array<std::pair<std::string_view, std::string_view>, 5>{ { { "", "absent" }, { R"("field":null,)", "null" }, { R"("field":"",)", "empty" }, { R"("field":42,)", "non_string" }, { R"("field":"unknown",)", "omitted" } } }) {
			rejection.Body      = std::string{ R"({"message":"Validation Failed","errors":[{)" } + std::string{ member } + R"("code":"custom","private_name":"private_value"}],"private_name":"private_value"})";
			const auto observed = review_comment_diagnostic(wire, github_mutation_wire_success{ rejection });
			expect(observed.Summary.text().find(std::string{ "field=" } + std::string{ expected }) != std::string_view::npos && observed.Summary.text().find("envelope_unknown=1") != std::string_view::npos && observed.Summary.text().find("error_unknown=1") != std::string_view::npos && observed.Summary.text().find("private") == std::string_view::npos, "missing, null, empty, unknown and extra members remain distinct without disclosing content");
		}
		for (const auto body : { R"({"errors":null})", R"({"errors":{}})", R"({})", R"({"errors":[{},{}]})", R"({"errors":[],"errors":[]})" }) {
			rejection.Body      = body;
			const auto observed = review_comment_diagnostic(wire, github_mutation_wire_success{ rejection });
			expect(observed.Summary.State == application::diagnostic_summary_state::sanitized && observed.Summary.text().find("errors=1") == std::string_view::npos, "invalid or absent error envelope cannot masquerade as one exact error");
		}

		for (const auto field : { "message", "resource", "field", "code" }) {
			rejection.Body  = std::string{ R"({"message":"Validation Failed","errors":[{")" } + field + R"(":"private_repository/secret_source ghp_SYNTHETIC_CANARY https://private.test/ 127.0.0.1"}]})";
			const auto safe = review_comment_diagnostic(wire, github_mutation_wire_success{ rejection });
			expect(safe.Summary.text().find("CANARY") == std::string_view::npos && safe.Summary.text().find("private") == std::string_view::npos && safe.Summary.text().find("127.0.0.1") == std::string_view::npos, "unknown reflected prose never enters diagnostics");
		}
		rejection.Body = std::string(4097U, 'x');
		expect(review_comment_diagnostic(wire, github_mutation_wire_success{ rejection }).Summary.text().find("omitted_invalid_or_limit") != std::string_view::npos, "oversize body is not parsed or retained");
		const auto failure = review_comment_diagnostic(wire, github_mutation_wire_error{ .Failure = application::review_mutation_failure::unavailable, .Transmission = github_mutation_transmission::indeterminate, .HttpStatus = 503, .TransportFailureSource = application::diagnostic_transport_failure_source::http_response });
		expect(failure.HttpStatus == 503U && failure.TransportFailureSource == application::diagnostic_transport_failure_source::http_response && failure.Summary.text().find("transmission=indeterminate") != std::string_view::npos, "typed HTTP failure metadata survives mutation mapping");
		auto scratch           = tests::sqlite_contract::scratch_root{ "publication-diagnostic" };
		auto transport         = publication_transport{ };
		transport.RejectNative = true;
		auto log               = std::make_shared<publication_log>();
		auto session           = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()), std::nullopt, nullptr, log };
		expect(!session.submit(request) && transport.Posts.size() == 1U && log->Events.size() == 1U, "diagnostic session does not activate synthetic rejection or fallback");
		expect(log->Events.front().HttpStatus == 422U && log->Events.front().ContextGeneration == 7U, "real session emits one scoped POST observation");
		static_cast<void>(session.submit(request));
		expect(transport.Posts.size() == 1U && log->Events.size() == 1U, "recovery stays GET-only and creates no synthetic POST diagnostic");
	}

	void test_session_ambiguity_and_refusals()
	{
		for (unsigned course = 0U; course < 13U; ++course) {
			auto request               = publication_request(permalink_source());
			request.Source->Placement  = core::review_placement::off_hunk;
			auto             scratch   = tests::sqlite_contract::scratch_root{ "publication" };
			auto             journal   = open_journal(scratch.path());
			auto             transport = publication_transport{ };
			auto             lease     = std::make_shared<publication_lease>();
			std::stop_source stop;
			if (course == 0U) {
				transport.RejectNative = true;
			} // production default has no synthetic authority
			if (course >= 1U && course <= 4U) {
				transport.GenericStatus = std::array{ 422, 403, 429, 503 }[course - 1U];
			}
			if (course == 5U || course == 6U) {
				transport.LoseResponse = true;
				transport.Publish      = course == 6U;
			}
			if (course == 7U) {
				transport.StaleAtTarget = 2U;
			}
			if (course == 8U) {
				transport.WrongActor = true;
			}
			if (course == 9U) {
				lease->revoke();
			}
			if (course == 10U) {
				transport.RejectNative = true;
				request.Proposal.Body  = "```suggestion\nreplacement\n```";
				next_draft(request);
			}
			if (course == 11U) {
				transport.CancelAfterPost = &stop;
				transport.LoseResponse    = true;
			}
			if (course == 12U) {
				request.Source->LineCount = 1U;
			}
			auto       session = review_mutation_session{ transport, session_configuration(request), lease, std::move(journal), course == 0U ? std::nullopt : std::optional{ fixture_contract(request.LiveBinding->Target.ServerContract) } };
			const auto result  = session.submit_and_reconcile(request, stop.get_token());
			const bool success = course == 6U || course == 11U;
			expect(result.Result.has_value() == success, "only exact new native evidence resolves an ambiguous write");
			expect(transport.Posts.size() == ((course >= 7U && course <= 9U) || course == 12U ? 0U : 1U), "generic status/loss/incompatible body never authorizes fallback");
			if (course == 10U) {
				expect(result.Result.error() == application::review_mutation_failure::incompatible_body, "suggestion body diagnosed explicitly");
			}
			const auto writes         = transport.Posts.size();
			transport.CancelAfterPost = nullptr;
			static_cast<void>(session.submit(request));
			expect(transport.Posts.size() == writes, "reentry never blindly resubmits native");
		}
	}
	void test_session_restart_states()
	{
		for (const auto interrupted : { journal_state::native_prepared, journal_state::native_possibly_transmitted, journal_state::native_definitively_unsupported, journal_state::fallback_prepared, journal_state::fallback_possibly_transmitted, journal_state::succeeded_fallback }) {
			auto request              = publication_request(permalink_source());
			request.Source->Placement = core::review_placement::off_hunk;
			auto scratch              = tests::sqlite_contract::scratch_root{ "publication" };
			auto transport            = publication_transport{ };
			transport.RejectNative    = true;
			{
				auto journal            = open_journal(scratch.path());
				journal->InterruptAfter = interrupted;
				auto session            = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), std::move(journal), fixture_contract(request.LiveBinding->Target.ServerContract) };
				expect(!session.submit(request), "interruption reports unresolved to caller");
			}
			const auto before    = transport.Posts.size();
			auto       session   = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()), fixture_contract(request.LiveBinding->Target.ServerContract) };
			const auto recovered = session.submit(request);
			const bool rejected  = interrupted == journal_state::native_definitively_unsupported;
			expect(transport.Posts.size() == before + (rejected ? 1U : 0U), "only durable exact rejection permits a new fallback after restart");
			expect(recovered.has_value() == (rejected || interrupted == journal_state::succeeded_fallback), "prepared and uncertain child restarts do not infer success");
		}
		// Lost response is resolved by a later process through the exact server identity.
		auto request              = publication_request(permalink_source());
		request.Source->Placement = core::review_placement::off_hunk;
		auto scratch              = tests::sqlite_contract::scratch_root{ "publication" };
		auto transport            = publication_transport{ };
		transport.LoseResponse    = true;
		transport.HideAfterPost   = true;
		{
			auto session = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()) };
			expect(!session.submit(request), "lost response and hidden refresh retain unresolved journal");
		}
		transport.HideAfterPost = false;
		auto       session      = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()) };
		const auto recovered    = session.submit(request);
		expect(recovered && recovered->Duplicate && transport.Posts.size() == 1U, "restart reconciles exact native receipt with zero POST");
	}

	void test_interruption_boundaries()
	{
		std::size_t courses = 0U;
		for (const bool fallback : { false, true }) {
			auto request              = publication_request(permalink_source());
			request.Source->Placement = core::review_placement::off_hunk;
			const auto configuration  = session_configuration(request);
			const auto contract       = fixture_contract(request.LiveBinding->Target.ServerContract);
			// Count the actual protocol course, then interrupt on both sides of
			// every read/response and every transmission witness in that course.
			auto baselineRoot         = tests::sqlite_contract::scratch_root{ "boundary-baseline" };
			auto baselineWire         = publication_transport{ };
			baselineWire.RejectNative = fallback;
			{
				auto session = review_mutation_session{ baselineWire, configuration, std::make_shared<publication_lease>(), open_journal(baselineRoot.path()), contract };
				expect(session.submit_and_reconcile(request).Result.has_value(), "boundary census follows successful native or fallback course");
			}
			const auto resume = [&](publication_transport& aWire, const std::filesystem::path& aRoot) {
				for (unsigned restart = 0U; restart < 3U; ++restart) {
					auto session = review_mutation_session{ aWire, configuration, std::make_shared<publication_lease>(), open_journal(aRoot), contract };
					static_cast<void>(session.submit_and_reconcile(request));
					expect(aWire.Comments.size() <= 1U, "interruption and repeated restart leave at most one remote candidate");
					const auto files = std::ranges::count_if(aWire.Posts, [](const auto& aPost) {
						return std::get<application::review_mutation_request>(aPost.Semantic).Representation == core::review_publication_representation::file_comment_with_permalink;
					});
					expect(files <= 1 && aWire.Posts.size() - static_cast<std::size_t>(files) <= 1U, "neither child is blindly retransmitted after restart");
				}
			};
			for (std::size_t point = 1U; point <= baselineWire.Boundaries; ++point) {
				auto scratch      = tests::sqlite_contract::scratch_root{ "wire-boundary" };
				auto wire         = publication_transport{ };
				wire.RejectNative = fallback;
				wire.InterruptAt  = point;
				{
					auto session = review_mutation_session{ wire, configuration, std::make_shared<publication_lease>(), open_journal(scratch.path()), contract };
					static_cast<void>(session.submit_and_reconcile(request));
				}
				expect(!wire.InterruptAt, "every counted wire interruption was reached");
				resume(wire, scratch.path());
				++courses;
			}
			const auto states = fallback
				? std::vector{ journal_state::native_prepared, journal_state::native_possibly_transmitted, journal_state::native_definitively_unsupported, journal_state::fallback_prepared, journal_state::fallback_possibly_transmitted, journal_state::succeeded_fallback }
				: std::vector{ journal_state::native_prepared, journal_state::native_possibly_transmitted, journal_state::succeeded_native };
			for (const auto state : states) {
				for (const bool before : { false, true }) {
					auto scratch      = tests::sqlite_contract::scratch_root{ "journal-boundary" };
					auto wire         = publication_transport{ };
					wire.RejectNative = fallback;
					{
						auto journal                                                  = open_journal(scratch.path());
						(before ? journal->InterruptBefore : journal->InterruptAfter) = state;
						auto* observed                                                = journal.get();
						auto  session                                                 = review_mutation_session{ wire, configuration, std::make_shared<publication_lease>(), std::move(journal), contract };
						static_cast<void>(session.submit_and_reconcile(request));
						expect(observed->Interrupted, "every before/after journal interruption was reached");
					}
					resume(wire, scratch.path());
					++courses;
				}
			}
		}
		for (const bool before : { false, true }) {
			auto request              = publication_request(permalink_source());
			request.Source->Placement = core::review_placement::off_hunk;
			auto scratch              = tests::sqlite_contract::scratch_root{ "cached-boundary" };
			auto wire                 = publication_transport{ };
			wire.RejectNative         = true;
			{
				auto  journal  = open_journal(scratch.path());
				auto* observed = journal.get();
				auto  session  = review_mutation_session{ wire, session_configuration(request), std::make_shared<publication_lease>(), std::move(journal), fixture_contract(request.LiveBinding->Target.ServerContract) };
				expect(session.submit(request).has_value(), "first logical publication establishes scoped cache evidence");
				next_draft(request);
				(before ? observed->InterruptBefore : observed->InterruptAfter) = journal_state::unsupported_evidence_admitted;
				static_cast<void>(session.submit(request));
				expect(observed->Interrupted && wire.Comments.size() == 1U, "cached admission boundary is reached before any new write");
			}
			for (unsigned restart = 0U; restart < 3U; ++restart) {
				auto session = review_mutation_session{ wire, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()), fixture_contract(request.LiveBinding->Target.ServerContract) };
				expect(session.submit(request).has_value() && wire.Comments.size() == 2U, "cached admission restart leaves exactly one candidate for each logical publication");
			}
			++courses;
		}
		std::cout << "publication-interruption-courses:" << courses << '\n';
	}

	void test_evidence_admission_restart_and_legacy_aliases()
	{
		auto request              = publication_request(permalink_source());
		request.Source->Placement = core::review_placement::off_hunk;
		auto scratch              = tests::sqlite_contract::scratch_root{ "publication-evidence" };
		auto transport            = publication_transport{ };
		transport.RejectNative    = true;
		{
			auto  journal  = open_journal(scratch.path());
			auto* observed = journal.get();
			auto  session  = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), std::move(journal), fixture_contract(request.LiveBinding->Target.ServerContract) };
			expect(session.submit(request).has_value(), "first exact rejection seeds only this session's evidence");
			next_draft(request);
			observed->InterruptAfter = journal_state::unsupported_evidence_admitted;
			expect(!session.submit(request) && transport.Posts.size() == 2U && !observed->Saved.back().Native, "interrupted cached admission durably records permission without native child");
		}
		{
			auto session = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()) };
			expect(session.submit(request).has_value() && transport.Posts.size() == 3U, "durable evidence admission permits one fallback across restart");
			next_draft(request);
			expect(!session.submit(request) && transport.Posts.size() == 4U && transport.Posts.back().Body.find("subject_type") == std::string::npos, "a new session has no cached evidence for a fresh logical attempt");
		}
		for (const bool legacyFirst : { false, true }) {
			auto aliasRoot          = tests::sqlite_contract::scratch_root{ "publication-alias" };
			auto fresh              = publication_request(permalink_source());
			fresh.Source->Placement = core::review_placement::off_hunk;
			const auto source       = fresh.Source;
			auto       wire         = publication_transport{ };
			if (legacyFirst) {
				fresh.Source.reset();
			}
			{
				auto session = review_mutation_session{ wire, session_configuration(fresh), std::make_shared<publication_lease>(), open_journal(aliasRoot.path()) };
				expect(session.submit(fresh).has_value(), "first representation of identity succeeds");
			}
			fresh.Source       = legacyFirst ? source : std::nullopt;
			auto       session = review_mutation_session{ wire, session_configuration(fresh), std::make_shared<publication_lease>(), open_journal(aliasRoot.path()) };
			const auto next    = session.submit(fresh);
			expect(wire.Posts.size() == 1U && (legacyFirst ? next.has_value() && next->Duplicate : !next), "legacy/logical alias cannot become a fresh POST in either direction after restart");
		}
	}

	std::size_t        placementDecodes = 0U;
	[[nodiscard]] auto counted_decode(const review_permalink_source& aSource, const std::string_view aBody)
		-> std::optional<parsed_review_permalink>
	{
		++placementDecodes;
		return parse_review_permalink_body(aSource, aBody);
	}

	void test_server_truth_placement()
	{
		const auto  source    = permalink_source();
		const auto& authority = source.Endpoint->request().WorkbenchAuthority;
		const auto  path      = source.Endpoint->path().generic_string();
		const auto  body      = *compile_review_permalink_body(source, permalink_anchor(source), "Root prose.\nSecond line.");
		auto        snapshot  = application::github_pull_request_snapshot{
			.Profile = authority.Profile, .Host = authority.Host, .Account = authority.Account, .Repository = { .Repository = authority.Repository, .Owner = source.Target.Repository.Owner, .Name = source.Target.Repository.Name }, .PullRequest = { .PullRequest = authority.PullRequest, .Number = source.Target.Number, .HeadOid = application::commit_oid{ std::string{ core::canonical_hex(source.Target.Semantic.ExpectedHead).view() } } }, .Files = { { .Path = path } }, .ReviewThreadsAvailability = application::github_collection_availability::available, .ReviewThreads = { { .Thread = application::github_thread_id{ "T_file" }, .Subject = application::github_review_thread_subject::file, .Path = path, .Comments = { { .Comment = application::github_comment_id{ "C_root" }, .Body = body, .Path = path }, { .Comment = application::github_comment_id{ "C_reply" }, .ReplyTo = application::github_comment_id{ "C_root" }, .Body = "https://foreign.example/elsewhere/blob/main/file#L90\n\nReply prose", .Path = path } } } }
		};
		auto& raw       = snapshot.ReviewThreads.front().Comments.front().Observation.RawBody;
		raw             = body;
		auto       host = application::review_comment_placement{ counted_decode };
		const auto node = [](const auto& aProjection, const std::string_view aIdentity) -> const auto& {
			const auto found = std::ranges::find(aProjection.Conversations, aIdentity, &application::github_conversation_projection::Id);
			expect(found != aProjection.Conversations.end(), "canonical conversation identity survives projection");
			return *found;
		};
		auto projected = host.project(snapshot, source);
		expect(node(projected, "review-thread:T_file").RemoteSubject == application::github_conversation_remote_subject::file
				   && node(projected, "review-thread:T_file").SemanticAnchor == permalink_anchor(source),
			   "file remote subject and semantic source anchor coexist");
		expect(node(projected, "review-comment:C_root").Body == body
				   && node(projected, "review-comment:C_root").ReviewerProse == "Root prose.\nSecond line.",
			   "raw GitHub body and byte-preserved prose coexist");
		expect(node(projected, "review-comment:C_reply").SemanticAnchor == permalink_anchor(source)
				   && !node(projected, "review-comment:C_reply").ReviewerProse,
			   "reply inherits root and never parses its own foreign link");
		snapshot.ReviewThreads.front().Comments.front().Body = "Rendered display without the source URL.";
		projected                                            = host.project(snapshot, source);
		expect(node(projected, "review-comment:C_root").Body == "Rendered display without the source URL."
				   && node(projected, "review-comment:C_root").ReviewerProse == "Root prose.\nSecond line.",
			   "raw Markdown drives placement while bodyText remains display text");
		const auto decoded = placementDecodes;
		projected          = host.project(snapshot, source);
		expect(placementDecodes == decoded, "unchanged root is not decoded again");
		snapshot.ReviewThreads.front().Resolved = true;
		projected                               = host.project(snapshot, source);
		expect(node(projected, "review-comment:C_reply").Resolved && placementDecodes == decoded, "resolution is current server truth without root reparse");
		raw       = *compile_review_permalink_body(source, permalink_anchor(source, 60U), "Edited prose.");
		projected = host.project(snapshot, source);
		expect(node(projected, "review-comment:C_root").ReviewerProse == "Edited prose."
				   && node(projected, "review-thread:T_file").SemanticAnchor == permalink_anchor(source, 60U),
			   "external valid link and prose edits remap the same canonical thread");
		for (const auto& invalid : { std::string{ "No link now." }, body + "\nhttps://github.com/other/repo/blob/main/file#L1", std::string{ "https://foreign.example/path#L13\n\nProse" } }) {
			raw       = invalid;
			projected = host.project(snapshot, source);
			expect(!node(projected, "review-thread:T_file").SemanticAnchor
					   && !node(projected, "review-comment:C_root").ReviewerProse
					   && !node(projected, "review-comment:C_reply").Line
					   && node(projected, "review-thread:T_file").AnchorAvailability == application::github_conversation_anchor_availability::file_level,
				   "removed, ambiguous or foreign root degrades; cached placement never returns");
		}
		raw       = body;
		projected = host.project(snapshot, source);
		raw.reset();
		snapshot.ReviewThreads.front().Comments.front().Body = body;
		projected                                            = host.project(snapshot, source);
		expect(!node(projected, "review-thread:T_file").SemanticAnchor,
			   "missing raw Markdown cannot reuse an anchor or infer one from rendered display");
		auto closedSource        = source;
		closedSource.Target.Open = false;
		expect(parse_review_permalink_body(closedSource, body).has_value()
				   && !compile_review_permalink_body(closedSource, permalink_anchor(source), "Cannot publish"),
			   "closed PR content can rehydrate without opening mutation authority");
		raw        = body;
		auto fresh = application::review_comment_placement{ counted_decode };
		projected  = fresh.project(snapshot, source);
		expect(node(projected, "review-thread:T_file").SemanticAnchor == permalink_anchor(source), "fresh host without database reconstructs only ordinary public content");
		projected = host.project(snapshot, std::nullopt);
		expect(!node(projected, "review-thread:T_file").SemanticAnchor, "unavailable retained commit cannot place a file comment");
		auto foreign                   = source;
		foreign.Target.Repository.Name = "other";
		projected                      = host.project(snapshot, foreign);
		expect(!node(projected, "review-thread:T_file").SemanticAnchor, "foreign source authority cannot place");
		snapshot.Files.clear();
		projected = host.project(snapshot, source);
		expect(!node(projected, "review-thread:T_file").SemanticAnchor, "file absent from changed catalogue cannot place");
		snapshot.Files.push_back({ .Path = path });
		snapshot.ReviewThreads.front().Comments.front().Deleted = true;
		projected                                               = host.project(snapshot, source);
		expect(!node(projected, "review-thread:T_file").SemanticAnchor, "deleted root cannot retain placement through a reply");
		snapshot.ReviewThreads.clear();
		projected = host.project(snapshot, source);
		expect(projected.Conversations.empty(), "server deletion removes the shared conversation");
		snapshot.ReviewThreads.push_back({ .Thread = application::github_thread_id{ "T_native" }, .Path = path, .Line = 4U, .DiffSide = application::github_diff_side::right });
		projected = host.project(snapshot, source);
		expect(node(projected, "review-thread:T_native").RemoteSubject == application::github_conversation_remote_subject::line
				   && node(projected, "review-thread:T_native").Line == 4U && !node(projected, "review-thread:T_native").SemanticAnchor,
			   "ordinary native mapping remains distinct; general historical identity is not inferred");
	}

} // namespace

void tour_ranges()
{
	using namespace bpr;
	using namespace bpr::adapters::github::detail;
	const auto scope  = tour_permalink_scope{ "github.com", { "octo-org", "review-repository" } };
	auto       source = core::tour_code_destination{ .Commit = *core::parse_git_commit_oid("1111111111111111111111111111111111111111"), .Path = "old/Unicode α #%.md", .Side = core::diff_side::left, .FirstLine = 1, .LastLine = 3 };
	const auto link   = compile_tour_permalink(scope, source);
	expect(link && link->ends_with("old/Unicode%20%CE%B1%20%23%25.md?plain=1#L2-L4"), "range/historical/plain-view shares canonical RFC0022 path escaping");
	const auto parsed = parse_tour_permalink(scope, *link + "\r\n\r\n### Author's tour · 1 — Why\r\n");
	expect(parsed && parsed->Commit == source.Commit && parsed->Path == source.Path && parsed->FirstLine == 1 && parsed->LastLine == 3, "cold body retains historical range without retained comparison fabrication");
	auto foreign            = scope;
	foreign.Repository.Name = "other";
	expect(!parse_tour_permalink(foreign, *link + "\n\nbody"), "cross-repository source rejected");
	auto altered = *link;
	altered.replace(altered.find("#L2-L4"), 7, "#L4-L2");
	expect(!parse_tour_permalink(scope, altered + "\n\nbody"), "reversed range refused");
	altered = *link;
	altered.replace(altered.find("?plain=1"), 8, "");
	expect(!parse_tour_permalink(scope, altered + "\n\nbody"), "Markdown rendered page is not canonical source");
	altered = *link;
	altered.replace(altered.find("1111111111111111111111111111111111111111"), 40, "main");
	expect(!parse_tour_permalink(scope, altered + "\n\nbody"), "branch links never stand for immutable source");
	source.Path      = "entirely-unchanged.cpp";
	source.FirstLine = source.LastLine = 0;
	expect(compile_tour_permalink(scope, source)->ends_with("#L1"), "unchanged and single-line destinations retain exact source");
}

namespace {
	struct lineage_fault final : application::review_publication_lineage_port
	{
		application::private_publication_application_host& Real;
		bool                                               FailPrepare = false;
		explicit lineage_fault(application::private_publication_application_host& aReal)
			: Real(aReal)
		{ }
		auto prepare(const application::review_mutation_request& r, std::string_view key, std::string_view body) -> bool override { return !FailPrepare && Real.prepare(r, key, body); }
		auto complete(const application::review_mutation_request& r, std::string_view key, std::string_view body, const application::review_mutation_receipt& receipt) -> bool override { return Real.complete(r, key, body, receipt); }
	};
	struct session_adapter final : application::review_mutation_port
	{
		review_mutation_session& Session;
		explicit session_adapter(review_mutation_session& aSession)
			: Session(aSession)
		{ }
		auto observe_target() -> std::expected<core::review_mutation_target, application::review_mutation_failure> override { return Session.observe_target(); }
		auto submit(const application::review_mutation_request& r) -> std::expected<application::review_mutation_receipt, application::review_mutation_failure> override { return Session.submit(r); }
		auto submit_and_reconcile(const application::review_mutation_request& r, std::stop_token stop) -> application::review_mutation_submission override { return Session.submit_and_reconcile(r, stop); }
		auto capabilities() const -> application::review_provider_capabilities override { return { }; }
	};
	auto lineage_writer(const std::filesystem::path& aRoot) -> sqlite3*
	{
		std::filesystem::path path;
		for (const auto& file : std::filesystem::recursive_directory_iterator(aRoot)) {
			if (file.path().filename() == "review-ledger.sqlite3") {
				path = file.path();
			}
		}
		expect(!path.empty(), "fault injector finds only fixture ledger");
		sqlite3* db = nullptr;
		expect(sqlite3_open(path.string().c_str(), &db) == SQLITE_OK, "fixture SQLite fault injection opens");
		expect(sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture peer holds SQLite writer exclusion");
		return db;
	}
	void private_composition_publication()
	{
		using namespace application;
		for (unsigned mode = 0; mode < 12; ++mode) {
			auto scratch              = tests::sqlite_contract::scratch_root{ "private-publication" };
			auto source               = permalink_source();
			auto request              = publication_request(source);
			request.Source->Placement = core::review_placement::off_hunk;
			adapters::sqlite::private_review_configuration config{ { scratch.path(), "github.com", "fixture-reviewer", "MikeMarcin/BPR-Test", 1 }, core::bpr_profile_id{ 1 }, request.Target.Host, request.ActingIdentity, request.Target.Repository, request.Target.PullRequest };
			auto                                           store   = std::move(*adapters::sqlite::make_private_review_store(config));
			auto                                           binding = core::private_review_binding{ .Scope = store->frontier()->Scope, .HostAuthority = source.Comparison.Authority.HostAuthority, .Revision = { request.Target.Host, request.Target.Repository, request.Target.PullRequest, source.Target.BaseOid, request.Target.ExpectedHead, source.Comparison.LeftAnchor.Epoch, source.Comparison.RightAnchor.Epoch }, .LeftAnchor = source.Comparison.LeftAnchor, .RightAnchor = source.Comparison.RightAnchor, .Comparison = request.Proposal.Anchor.Comparison };
			std::string                                    text;
			for (unsigned i = 0; i < 100; ++i) {
				text += "line\n";
			}
			auto bytes    = std::span{ reinterpret_cast<const std::uint8_t*>(text.data()), text.size() };
			auto computed = bpr::tests::product_file_diff({ .Key = source.Comparison, .Options = { }, .LeftBytes = bytes, .RightBytes = bytes });
			expect(computed.Result.has_value(), "real diff material for composition/publication");
			auto index = *core::build_diff_semantic_index(std::make_shared<const core::computed_file_diff>(std::move(*computed.Result)));
			auto note  = *core::create_private_thread(core::private_review_thread_id{ 1 }, { binding, core::private_pr_subject{ } }, { core::private_review_turn_id{ 2 }, core::private_review_turn_revision_id{ 3 }, core::private_vertex_order{ 3 }, core::private_reviewer_actor{ }, "private reasoning stays local" });
			expect(store->commit({ .ExpectedNextIdentity = 1, .Threads = { { note.Thread, 0 } }, .Revisions = { note.Revision } }).has_value(), "persist source before composition");
			auto                                ref = core::source_of(note.Revision);
			review_composition_application_host compositions(*store);
			auto                                workspace = compositions.create({ binding, core::review_composition_mode::reviewer_authored, { { ref, ref.Key, false } }, core::private_composition_target{ binding, request.Proposal.Anchor }, request.Proposal.Body });
			expect(workspace.has_value(), "create real composition before publish");
			std::array selection{ review_composition_acceptance{ workspace->State.Id, workspace->Version, *workspace->State.Candidate } };
			auto       accepted = compositions.accept(selection, binding, index, core::review_draft_scope_id{ 1 });
			expect(accepted.has_value(), "composition acceptance creates durable draft");
			auto draft = accepted->records().front().Draft;
			expect(store->find_publication()->Records.empty(), "acceptance alone creates no publication");
			auto publishing = compositions.change_draft({ { draft.Draft.Id, core::review_draft_command_kind::begin_publish, { } }, draft.Revision, false, { }, core::review_draft_scope_id{ 1 } });
			expect(publishing.has_value(), "separate explicit begin-publish commits existing reducer state");
			draft = publishing->records().front().Draft;
			private_publication_application_host lineage(*store);
			request.Proposal       = *lineage.proposal(draft.Draft.Id, draft.Revision);
			request.IdempotencyKey = idempotency_key_for(request.Proposal, request.Target);
			auto legacy            = request.Proposal;
			legacy.Durable.reset();
			expect(idempotency_key_for(legacy, request.Target) != request.IdempotencyKey, "durable idempotency domain separates legacy identity");
			auto foreign = request.Proposal;
			++foreign.Durable->Incarnation[0];
			expect(idempotency_key_for(foreign, request.Target) != request.IdempotencyKey, "recreated ledger cannot collide with same draft bytes");
			lineage_fault fault(lineage);
			fault.FailPrepare       = mode == 4;
			auto transport          = publication_transport{ };
			transport.RejectNative  = mode == 1 || mode == 11;
			transport.HideAfterPost = mode == 3 || mode >= 8;
			transport.WrongActor    = mode == 5;
			sqlite3* heldWriter     = nullptr;
			if (mode == 2) {
				transport.AfterPost = [&] { heldWriter = lineage_writer(scratch.path()); };
			}
			if (mode == 7) {
				transport.AfterPost = [&] {
					auto db = lineage_writer(scratch.path());
					expect(sqlite3_exec(db, "UPDATE private_scope_state SET payload_bytes=67108864; COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK, "simulate quota occupied after reservation");
					sqlite3_close(db);
				};
			}
			auto                        session = review_mutation_session{ transport, session_configuration(request), std::make_shared<publication_lease>(), open_journal(scratch.path()), fixture_contract(source.Target.ServerContract), &fault };
			session_adapter             adapter(session);
			review_mutation_coordinator coordinator(adapter, request.LiveBinding->Target, request.LiveBinding->ActingIdentity);
			std::stop_source            stop;
			if (mode == 6) {
				stop.request_stop();
			}
			auto result = coordinator.publish_attempt(request.Proposal, request.LiveBinding->Anchor, false, stop.get_token(), request.Source);
			auto rows   = store->find_publication();
			expect(rows.has_value(), "local publication joins readable");
			if (mode >= 4 && mode <= 6) {
				expect(!result.Result && transport.Posts.empty() && rows->Records.empty(), "prepare failure wrong actor and cancellation send nothing and record no success");
				continue;
			}
			expect(result.Result && rows->Records.size() == 1 && transport.Comments.size() == 1, "one logical publication and one shared comment");
			auto row = rows->Records.front();
			expect(row.Draft == draft.Draft.Id && row.DraftRevision == draft.Revision && row.Composition == draft.Composition && row.IdempotencyKey == request.IdempotencyKey && row.ActorNode == "U_fixture" && row.ActorDatabaseId == 22 && row.ActorLogin == "fixture-reviewer", "lineage freezes exact draft/composition actor and idempotency");
			expect(row.ApprovedProseDigest == core::private_digest(request.Proposal.Body), "approved prose digest retained independently");
			if (mode == 2 || mode == 3 || mode >= 8) {
				expect(!row.ServerComment && result.Lineage == review_lineage_state::pending && store->load_draft(draft.Draft.Id)->Draft.Draft.State == core::review_draft_state::publishing, "remote success and refused local write or refresh remains pending");
			} else {
				expect(row.ServerComment && result.Lineage == review_lineage_state::recorded, "successful receipt commits even when later writes occupy the remaining quota");
			}
			const auto posts = transport.Posts.size();
			if (mode == 2) {
				expect(sqlite3_exec(heldWriter, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK, "release competing fixture writer");
				sqlite3_close(heldWriter);
				transport.AfterPost = { };
			}
			transport.HideAfterPost                       = false;
			auto                                 reopened = std::move(*adapters::sqlite::make_private_review_store(config));
			private_publication_application_host recoveredLineage(*reopened);
			auto                                 current = session_configuration(request);
			if (mode == 3 || mode == 8) {
				transport.StaleAtTarget              = 1;
				current.Target.Semantic.ExpectedHead = *core::parse_git_commit_oid("1212121212121212121212121212121212121212");
				auto&      comment                   = transport.Comments.front();
				const auto line                      = "\"line\":" + std::to_string(request.Proposal.Anchor.Line + 1U);
				const auto at                        = comment.find(line);
				expect(at != std::string::npos, "native fixture exposes original line before head movement");
				comment.replace(at, line.size(), mode == 3 ? "\"line\":null" : "\"line\":" + std::to_string(request.Proposal.Anchor.Line + 2U));
				if (mode == 8) {
					const std::string originalCommit = "\"commit_id\":\"1111111111111111111111111111111111111111\"";
					const auto        commit         = comment.find(originalCommit);
					expect(commit != std::string::npos, "native fixture exposes original commit before head movement");
					comment.replace(commit, originalCommit.size(), "\"commit_id\":\"1212121212121212121212121212121212121212\"");
				}
				comment.insert(comment.size() - 1U, ",\"original_commit_id\":\"1111111111111111111111111111111111111111\",\"original_line\":" + std::to_string(request.Proposal.Anchor.Line + 1U));
			}
			if (mode == 9 || mode == 10) {
				auto& comment = transport.Comments.front();
				comment.insert(comment.size() - 1U, mode == 9 ? ",\"in_reply_to_id\":9001" : ",\"start_line\":1,\"start_side\":\"RIGHT\"");
			}
			auto recovered = review_mutation_session{ transport, current, std::make_shared<publication_lease>(), open_journal(scratch.path()), fixture_contract(source.Target.ServerContract), &recoveredLineage };
			if (mode == 2 || mode == 3 || mode >= 8) {
				transport.WrongActor = true;
				recovered.repair_pending_publications();
				expect(!reopened->lookup_publication(row.LogicalPublication)->value().ServerComment && transport.Posts.size() == posts, "wrong viewer cannot repair pending lineage or post");
				transport.WrongActor = false;
				if (mode == 9 || mode == 10) {
					recovered.repair_pending_publications();
					expect(!reopened->lookup_publication(row.LogicalPublication)->value().ServerComment && transport.Posts.size() == posts, "reply or multiline response cannot stand for a root single-line publication");
					auto& comment = transport.Comments.front();
					comment.erase(comment.find(mode == 9 ? ",\"in_reply_to_id\"" : ",\"start_line\""));
					comment.push_back('}');
				}
				recovered.repair_pending_publications();
				expect(reopened->lookup_publication(row.LogicalPublication)->value().ServerComment == result.Result->ServerCommentId && transport.Posts.size() == posts, "pending lineage repairs by read after restart and head advance without another POST");
				transport.StaleAtTarget = 0;
			}
			auto repaired = recovered.reconcile_publication(request);
			expect(((mode == 3 || mode == 8) || (repaired.Result && repaired.Lineage == review_lineage_state::recorded)) && transport.Posts.size() == posts, "restart repairs only through reads and one local transaction with zero new POST");
			auto lookup = reopened->lookup_publication(row.LogicalPublication);
			expect(lookup && *lookup && (**lookup).ServerComment == result.Result->ServerCommentId && (**lookup).ReconciledIdentity == result.Result->ServerCommentId && (**lookup).ReceiptReserve == 0, "exact canonical server join survives reopen");
			expect(reopened->load_draft(draft.Draft.Id)->Draft.Draft.State == core::review_draft_state::published, "draft terminal and lineage commit atomically");
			auto expectedWire = (mode == 1 || mode == 11) ? *compile_review_permalink_body(source, request.Proposal.Anchor, request.Proposal.Body) : request.Proposal.Body;
			expect((**lookup).WireBodyDigest == core::private_digest(expectedWire), "wire digest names native prose or canonical visible permalink plus prose");
			auto publicRead = tests::parse_fixture_object(transport.Comments.front());
			expect(publicRead.at("body").get_string() == expectedWire && publicRead.at("user").get_object().at("node_id").get_string() == "U_fixture", "separate fixture reader checks exact public body and actor without local metadata");
			auto wrongRequest = request;
			wrongRequest.Proposal.Body += " altered";
			expect(!recovered.reconcile_publication(wrongRequest).Result && transport.Posts.size() == posts, "wrong body cannot borrow a logical receipt or send repair POST");
			wrongRequest                                    = request;
			wrongRequest.LiveBinding->ActingIdentity.NodeId = "foreign";
			expect(!recovered.reconcile_publication(wrongRequest).Result && transport.Posts.size() == posts, "wrong actor cannot repair lineage");
			expect(reopened->find_publication(result.Result->ServerCommentId)->Records.size() == 1, "lineage lookup by server ID resolves same draft");
			if (mode == 3 || mode == 8) {
				recovered.repair_pending_publications();
				expect(transport.Posts.size() == posts, "repeated advanced-head repair remains read-only");
			} else {
				expect(recovered.reconcile_publication(request).Lineage == review_lineage_state::recorded && transport.Posts.size() == posts, "repeated repair is idempotent and read-only");
			}
			expect(recoveredLineage.for_draft(draft.Draft.Id)->Records.size() == 1, "bounded draft lookup returns canonical logical row");
			expect(recoveredLineage.duplicates(workspace->State.Id)->Records.size() == 1, "duplicate source body target remains an advisory with server identity");
			expect(reopened->read_revision_body({ ref, 0 })->Bytes == note.Revision.Body, "publication preserves private source bytes");
		}
	}
}

auto main() -> int
{
	test_recorded_contract_scope_and_protocol();
	test_publication_diagnostic_privacy_and_authority();
	private_composition_publication();
	tour_ranges();
	test_server_truth_placement();
	test_session_negotiation_and_cache();
	test_left_source_backed_child_on_a_stacked_pull_request();
	test_left_source_backed_child_through_the_cli_account_wrapper();
	test_session_ambiguity_and_refusals();
	test_session_restart_states();
	test_interruption_boundaries();
	test_evidence_admission_restart_and_legacy_aliases();
	test_canonical_permalink_round_trip_and_bounds();
	test_wire_representations_and_classifier();
	test_permalink_rejects_foreign_noncanonical_and_unavailable_sources();
	std::cout << "review-comment-permalink-tests:pass\n";
	return EXIT_SUCCESS;
}
