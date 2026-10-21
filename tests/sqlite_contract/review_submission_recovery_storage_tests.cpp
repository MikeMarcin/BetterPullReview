#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>

namespace {
	using namespace bpr;
	using bpr::tests::expect;
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

	auto request() -> application::review_submission_request
	{
		return { .Proposal = { .Draft = core::review_submission_draft_id{ 41U }, .Attempt = core::review_submission_attempt_id{ 1U }, .Target = target().Semantic, .ActingAccount = identity().Account, .Body = "Review summary.", .Disposition = core::review_disposition::comment }, .IdempotencyKey = "review-attempt-1", .LiveBinding = application::review_submission_live_binding{ .Target = target(), .ActingIdentity = identity() } };
	}
	struct temporary_directory
	{
		std::filesystem::path Path = std::filesystem::temp_directory_path() / ("bpr-review-submission-" + std::to_string(std::random_device{ }()));
		temporary_directory()
		{
			expect(std::filesystem::create_directory(Path), "temporary root created");
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Path };
			expect(adapters::sqlite::make_storage_root_owner_private(Path), "temporary root private");
			made.release();
		}
		~temporary_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}
	};
	auto storage(const std::filesystem::path& aRoot) -> std::unique_ptr<application::review_submission_recovery_port>
	{
		auto value = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = aRoot, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
		expect(value.has_value(), "submission store opens");
		return std::move(*value);
	}
	void execute_sql(const std::filesystem::path& aPath, const std::string& aSql)
	{
		::sqlite3* database = nullptr;
		expect(::sqlite3_open_v2(
				   aPath.string().c_str(),
				   &database,
				   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				   nullptr)
				   == SQLITE_OK,
			   "direct recovery fixture database opens");
		expect(::sqlite3_exec(database, aSql.c_str(), nullptr, nullptr, nullptr)
				   == SQLITE_OK,
			   "direct recovery fixture SQL succeeds");
		static_cast<void>(::sqlite3_close_v2(database));
	}

	auto record(application::review_submission_request aRequest = request()) -> application::review_submission_recovery_record { return { .Request = aRequest, .RequestFingerprint = application::review_submission_request_fingerprint(aRequest), .AuthorityFingerprint = application::review_submission_authority_fingerprint(*aRequest.LiveBinding), .BaselineReviewIds = std::vector{ application::review_database_id{ 40U } }, .Transmission = application::review_mutation_recovery_transmission::prepared }; }
	auto receipt(const application::review_submission_request& aRequest = request(), std::uint64_t aId = 45U) -> application::review_submission_receipt { return { .ReviewDatabaseId = application::review_database_id{ aId }, .ReviewNodeId = "PRR_exact", .Target = aRequest.Proposal.Target, .ActingAccount = aRequest.Proposal.ActingAccount, .Disposition = aRequest.Proposal.Disposition }; }
	auto database(const temporary_directory& aRoot) -> std::filesystem::path { return aRoot.Path / "state" / "v1" / "review-mutation-recovery.sqlite3"; }
	auto database_path(const temporary_directory& aRoot) -> std::filesystem::path { return database(aRoot); }
	void test_storage_round_trip()
	{
		temporary_directory root;
		auto                store  = storage(root.Path);
		auto                input  = request();
		input.Proposal.Disposition = core::review_disposition::approve;
		input.Proposal.Body.clear();
		input.LiveBinding->ActingIdentity.DatabaseId = std::numeric_limits<std::uint64_t>::max();
		auto expected                                = record(input);
		auto admitted                                = store->admit(expected);
		expect(admitted && admitted->Inserted && admitted->Record == expected, "empty approval snapshot and uint64 actor persist exactly");
		expect(!store->resolve(input.IdempotencyKey, expected.RequestFingerprint, receipt(input)), "prepared record cannot resolve");
		auto marked = store->mark_possibly_transmitted(input.IdempotencyKey, expected.RequestFingerprint);
		expect(marked.has_value(), "durable witness succeeds");
		const auto pending = store->pending();
		expect(pending && *pending == std::vector{ *marked }, "pending discovery retains full exact witnessed request");
		expect(!store->resolve(input.IdempotencyKey, expected.RequestFingerprint, receipt(input, 40U)), "baseline review cannot become receipt");
		const auto exactReceipt = receipt(input, std::numeric_limits<std::uint64_t>::max());
		auto       resolved     = store->resolve(input.IdempotencyKey, expected.RequestFingerprint, exactReceipt);
		expect(resolved && resolved->Receipt == exactReceipt, "full uint64 review identity resolves");
		store.reset();
		store       = storage(root.Path);
		auto loaded = store->load(input.IdempotencyKey);
		expect(loaded && *loaded && **loaded == *resolved, "fresh connection recovers exact success and request");
		expect(store->pending() && store->pending()->empty(), "successful records are excluded from pending discovery");
		auto duplicate = store->admit(expected);
		expect(duplicate && !duplicate->Inserted && duplicate->Record == *resolved, "duplicate admission preserves frozen baseline and success");
		auto changed             = input;
		changed.Proposal.Attempt = core::review_submission_attempt_id{ 2U };
		expect(!store->admit(record(changed)), "same-key changed attempt conflicts");
		changed                             = input;
		changed.LiveBinding->Target.BaseRef = "changed";
		expect(!store->admit(record(changed)), "same-key changed authority conflicts");
		auto bad = expected;
		bad.RequestFingerprint[0] ^= 1U;
		expect(!store->admit(bad), "unverified request fingerprint is refused");
		auto wrongScope = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = root.Path, .Host = "github.com", .Account = "22", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
		expect(wrongScope && !(*wrongScope)->admit(expected) && (*wrongScope)->load(input.IdempotencyKey)->has_value() == false, "scope cannot read or admit another actor request");
		expect((*wrongScope)->pending() && (*wrongScope)->pending()->empty(), "pending discovery cannot cross actor scope");
	}
	void test_version_three_migration_preserves_whole_reviews()
	{
		for (const bool succeeded : { false, true }) {
			temporary_directory root;
			auto                store    = storage(root.Path);
			const auto          expected = record();
			expect(store->admit(expected).has_value(), "v3 whole-review row admitted");
			auto saved = store->mark_possibly_transmitted(expected.Request.IdempotencyKey, expected.RequestFingerprint);
			expect(saved.has_value(), "v3 whole-review witness saved");
			if (succeeded) {
				saved = store->resolve(expected.Request.IdempotencyKey, expected.RequestFingerprint, receipt());
				expect(saved.has_value(), "v3 receipt saved");
			}
			store.reset();
			execute_sql(database(root), "DROP TABLE submission_post; DROP TABLE submission_unknown; DROP TABLE staged_publication; DROP TABLE logical_publication; PRAGMA user_version=3; UPDATE recovery_meta SET schema_format=3;");
			store = storage(root.Path);
			// [[BPR-1715]]. An unresolved one was possibly sent with no
			// answer kept, since time zero.
			if (!succeeded) {
				saved->Post = core::review_submission_post_record{ };
			}
			expect(store->load(expected.Request.IdempotencyKey).value() == *saved, "v4 migration retains exact whole-review witness or receipt");
		}
	}

	void test_staged_journal()
	{
		auto fingerprint = std::array<std::uint8_t, 32>{ };
		fingerprint.fill(1);
		application::staged_review_plan        plan{ .Submission = request(), .Owner = { core::bpr_profile_id{ 1 }, target().Semantic.Host, core::host_authority_fingerprint{ fingerprint }, identity().Account, target().Semantic.Repository, target().Semantic.PullRequest }, .Generation = 1, .Stops = { { core::tour_stop_id{ 1 }, { .Commit = target().Semantic.ExpectedHead, .Path = "a.cpp" }, "First", "Why", "https://github.com/MikeMarcin/BPR-Test/blob/exact/a.cpp#L1" } } };
		application::staged_publication_record initial{ .Plan = plan, .Pending = application::staged_initial_request(plan) };
		temporary_directory                    root;
		auto                                   store = storage(root.Path);
		expect(store->advance_staged({ }, initial).has_value(), "immutable staged plan admitted");
		auto witnessed = initial;
		++witnessed.Revision;
		witnessed.PossiblyTransmitted = true;
		expect(store->advance_staged(initial, witnessed).has_value(), "staged witness CAS persists");
		expect(!store->advance_staged(initial, witnessed), "stale CAS cannot replay witness");
		auto reset = witnessed;
		++reset.Revision;
		reset.PossiblyTransmitted = false;
		expect(!store->advance_staged(witnessed, reset), "possibly transmitted cannot be reset to prepared");
		store.reset();
		store = storage(root.Path);
		expect(store->load_staged(plan.Submission.IdempotencyKey).value() == witnessed && store->pending_staged().value() == std::vector{ witnessed }, "restart retains exact generation, bodies, sources, phase and witness");
		auto other = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = root.Path, .Host = "github.com", .Account = "22", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
		expect(other && (*other)->pending_staged()->empty() && !(*other)->advance_staged({ }, initial), "staged rows cannot cross actor scope");
		other->reset();
		store.reset();
		execute_sql(database(root), "UPDATE staged_publication SET record_digest=zeroblob(32)");
		store = storage(root.Path);
		expect(!store->load_staged(plan.Submission.IdempotencyKey) && !store->pending_staged(), "corrupt staged journal fails closed without partial discovery");
	}
	void test_version_four_migration()
	{
		temporary_directory root;
		auto                store    = storage(root.Path);
		const auto          original = record();
		expect(store->admit(original).has_value(), "v4 summary exists");
		store.reset();
		execute_sql(database(root), "DROP TABLE submission_post; DROP TABLE submission_unknown; DROP TABLE staged_publication; PRAGMA user_version=4; UPDATE recovery_meta SET schema_format=4;");
		store = storage(root.Path);
		expect(store->load(original.Request.IdempotencyKey).value() == original && store->pending_staged()->empty(), "v4 migration preserves existing summary and admits empty staged table");
	}

	/// [[BPR-1540]]. Why an unresolved attempt is still unknown is kept with
	/// it, replaced by the next reason, cleared when it resolves, and gone
	/// with a withdrawn attempt. A v5 journal gains the table.
	void test_unknown_reason_is_kept_with_the_attempt()
	{
		temporary_directory root;
		auto                store    = storage(root.Path);
		const auto          expected = record();
		auto                first    = core::review_submission_unknown_diagnosis{ };
		first.Reason                 = core::review_submission_unknown_reason::comment_page_too_large;
		first.PostAnswer             = core::review_submission_post_answer::error_status;
		first.PostStatusClass        = 5U;
		first.CommentsExpected       = 3U;
		first.PagesRead              = 1U;
		expect(!store->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, first), "no reason is kept for an attempt never admitted");
		expect(store->admit(expected).has_value() && store->mark_possibly_transmitted(expected.Request.IdempotencyKey, expected.RequestFingerprint).has_value(), "the attempt is admitted and possibly sent");
		auto wrong = expected.RequestFingerprint;
		wrong[0] ^= 1U;
		expect(!store->note_unknown(expected.Request.IdempotencyKey, wrong, first), "nor for another fingerprint");
		auto invalid            = first;
		invalid.CommentsMatched = 4U;
		expect(!store->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, invalid), "nor an invalid diagnosis");
		expect(store->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, first).has_value(), "the reason is kept");
		store.reset();
		store       = storage(root.Path);
		auto loaded = store->load(expected.Request.IdempotencyKey);
		expect(loaded && *loaded && (**loaded).LastUnknown == std::optional{ first }, "the reason survives a restart with the attempt");
		auto second   = first;
		second.Reason = core::review_submission_unknown_reason::review_not_listed;
		expect(store->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, second).has_value(), "a later reason is kept");
		loaded = store->load(expected.Request.IdempotencyKey);
		expect(loaded && *loaded && (**loaded).LastUnknown == std::optional{ second }, "and replaces the earlier one");
		const auto pending = store->pending();
		expect(pending && pending->size() == 1U && pending->front().LastUnknown == std::optional{ second }, "discovery carries it");
		store.reset();
		execute_sql(database(root), "PRAGMA ignore_check_constraints=ON; UPDATE submission_unknown SET reason=99");
		store = storage(root.Path);
		expect(!store->load(expected.Request.IdempotencyKey), "a reason outside the set fails closed");
		store.reset();
		execute_sql(database(root), "PRAGMA ignore_check_constraints=OFF; UPDATE submission_unknown SET reason=0");
		store             = storage(root.Path);
		const auto solved = store->resolve(expected.Request.IdempotencyKey, expected.RequestFingerprint, receipt());
		loaded            = store->load(expected.Request.IdempotencyKey);
		expect(solved && !solved->LastUnknown && loaded && *loaded && !(**loaded).LastUnknown, "a resolved attempt is no longer unknown");
		expect(!store->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, first), "and takes no reason");
		{
			temporary_directory other;
			auto                withdrawn = storage(other.Path);
			expect(withdrawn->admit(expected).has_value() && withdrawn->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, first).has_value(), "a prepared attempt takes a reason");
			expect(withdrawn->withdraw(expected.Request.IdempotencyKey, expected.RequestFingerprint).has_value(), "and is withdrawn with it");
			const auto again = withdrawn->admit(expected);
			const auto fresh = withdrawn->load(expected.Request.IdempotencyKey);
			expect(again && again->Inserted && fresh && *fresh && !(**fresh).LastUnknown, "nothing of the withdrawn attempt's reason is left behind");
		}
		{
			temporary_directory older;
			auto                migrated = storage(older.Path);
			expect(migrated->admit(expected).has_value(), "a v5 attempt exists");
			migrated.reset();
			execute_sql(database(older), "DROP TABLE submission_post; DROP TABLE submission_unknown; PRAGMA user_version=5; UPDATE recovery_meta SET schema_format=5;");
			migrated = storage(older.Path);
			expect(migrated->load(expected.Request.IdempotencyKey).value() == expected
					   && migrated->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, first).has_value(),
				   "v5 migrates to the current version keeping the attempt and taking reasons");
		}
	}

	/// [[BPR-1715]]. What GitHub answered a POST is kept with its
	/// possibly sent attempt, replaced by the next answer and gone with a
	/// withdrawn attempt. A version 6 journal, which kept no answer, keeps
	/// its possibly sent attempt exactly, with no answer since time zero,
	/// and takes answers.
	void test_post_answer_is_kept_with_the_attempt()
	{
		temporary_directory root;
		auto                store    = storage(root.Path);
		const auto          expected = record();
		const auto          sending  = core::review_submission_post_record{ .Answer = core::review_submission_post_answer::no_answer, .RecordedUtcMs = 1'792'600'000'000 };
		const auto          refused  = core::review_submission_post_record{ .Answer = core::review_submission_post_answer::error_status, .StatusClass = 4U, .RecordedUtcMs = 1'792'600'001'000 };
		expect(store->admit(expected).has_value(), "the attempt is admitted");
		expect(!store->note_post(expected.Request.IdempotencyKey, expected.RequestFingerprint, sending), "no answer is kept for a POST that cannot have left");
		expect(store->mark_possibly_transmitted(expected.Request.IdempotencyKey, expected.RequestFingerprint).has_value(), "the POST may leave");
		auto wrong = expected.RequestFingerprint;
		wrong[0] ^= 1U;
		expect(!store->note_post(expected.Request.IdempotencyKey, wrong, sending), "nor for another fingerprint");
		expect(!store->note_post(expected.Request.IdempotencyKey, expected.RequestFingerprint, { .Answer = core::review_submission_post_answer::not_sent }), "nor an answer that was not sent");
		expect(!store->note_post(expected.Request.IdempotencyKey, expected.RequestFingerprint, { .Answer = core::review_submission_post_answer::error_status, .StatusClass = 2U }), "nor a class that does not match its answer");
		expect(store->note_post(expected.Request.IdempotencyKey, expected.RequestFingerprint, sending).has_value(), "no answer yet, before the POST leaves");
		expect(store->note_post(expected.Request.IdempotencyKey, expected.RequestFingerprint, refused).has_value(), "then GitHub's answer");
		store.reset();
		store       = storage(root.Path);
		auto loaded = store->load(expected.Request.IdempotencyKey);
		expect(loaded && *loaded && (**loaded).Post == std::optional{ refused }, "the answer survives a restart, replacing the first");
		const auto pending = store->pending();
		expect(pending && pending->size() == 1U && pending->front().Post == std::optional{ refused }, "discovery carries it");
		store.reset();
		execute_sql(database(root), "PRAGMA ignore_check_constraints=ON; UPDATE submission_post SET answer=9");
		store = storage(root.Path);
		expect(!store->load(expected.Request.IdempotencyKey), "an answer outside the set fails closed");
		store.reset();
		execute_sql(database(root), "PRAGMA ignore_check_constraints=OFF; UPDATE submission_post SET answer=2");
		store = storage(root.Path);
		expect(store->withdraw(expected.Request.IdempotencyKey, expected.RequestFingerprint).has_value(), "a refused attempt is withdrawn with its answer");
		const auto again = store->admit(expected);
		const auto fresh = store->load(expected.Request.IdempotencyKey);
		expect(again && again->Inserted && fresh && *fresh && !(**fresh).Post, "nothing of the withdrawn attempt's answer is left behind");
		{
			// The owner's attempt: possibly sent, unknown, held by an
			// application at version 6.
			temporary_directory older;
			auto                legacy    = storage(older.Path);
			auto                diagnosis = core::review_submission_unknown_diagnosis{ .Reason = core::review_submission_unknown_reason::review_not_listed, .CommentsExpected = 0U };
			expect(legacy->admit(expected).has_value() && legacy->mark_possibly_transmitted(expected.Request.IdempotencyKey, expected.RequestFingerprint).has_value()
					   && legacy->note_unknown(expected.Request.IdempotencyKey, expected.RequestFingerprint, diagnosis).has_value(),
				   "a version 6 attempt is possibly sent and unknown");
			legacy.reset();
			execute_sql(database(older), "DROP TABLE submission_post; PRAGMA user_version=6; UPDATE recovery_meta SET schema_format=6;");
			legacy            = storage(older.Path);
			auto sent         = expected;
			sent.Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
			sent.LastUnknown  = diagnosis;
			sent.Post         = core::review_submission_post_record{ };
			const auto kept   = legacy->load(expected.Request.IdempotencyKey);
			expect(kept && *kept && **kept == sent, "version 6 migrates keeping the attempt exactly, with no answer since time zero");
			expect(legacy->note_post(expected.Request.IdempotencyKey, expected.RequestFingerprint, refused).has_value(), "and takes answers");
		}
	}

	void test_integrity_and_schema()
	{
		for (const auto* mutation : { "UPDATE submission_attempt SET request = zeroblob(length(request))", "UPDATE submission_attempt SET request_fingerprint = zeroblob(32)", "UPDATE submission_baseline SET baseline_digest = zeroblob(32)", "UPDATE submission_baseline SET baseline = zeroblob(length(baseline))" }) {
			temporary_directory root;
			auto                store    = storage(root.Path);
			auto                expected = record();
			expect(store->admit(expected).has_value(), "corruption fixture admitted");
			store.reset();
			execute_sql(database(root), mutation);
			store = storage(root.Path);
			expect(!store->load(expected.Request.IdempotencyKey), "corrupt snapshot or baseline fails integrity checks");
			expect(!store->pending(), "pending discovery rejects corrupt records atomically");
		}
		for (const auto* mutation : { "PRAGMA user_version = 8", "ALTER TABLE submission_attempt ADD COLUMN unexpected TEXT", "ALTER TABLE submission_unknown ADD COLUMN detail TEXT", "ALTER TABLE submission_post ADD COLUMN detail TEXT" }) {
			temporary_directory root;
			auto                store = storage(root.Path);
			store.reset();
			execute_sql(database(root), mutation);
			const auto opened = adapters::sqlite::make_review_submission_recovery_store({ .StorageRoot = root.Path, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 });
			expect(!opened, "future or drifted schema fails closed");
		}
	}
	void test_capacity_and_baseline_validation()
	{
		temporary_directory root;
		auto                store    = storage(root.Path);
		auto                expected = record();
		expected.BaselineReviewIds   = std::vector{ application::review_database_id{ 40U }, application::review_database_id{ 40U } };
		expect(!store->admit(expected), "duplicate baseline rejected");
		expected                   = record();
		expected.BaselineReviewIds = std::vector{ application::review_database_id{ 41U }, application::review_database_id{ 40U } };
		expect(!store->admit(expected), "unsorted baseline rejected");
		expected                   = record();
		expected.BaselineReviewIds = std::vector(101U, application::review_database_id{ 40U });
		expect(!store->admit(expected), "oversized baseline rejected");
		for (std::size_t index = 0; index < application::maximum_review_mutation_recovery_records; ++index) {
			auto value           = request();
			value.IdempotencyKey = "attempt-" + std::to_string(index);
			expect(store->admit(record(value)).has_value(), "bounded capacity admits exact rows");
		}
		const auto pending = store->pending();
		expect(pending && pending->size() == application::maximum_review_mutation_recovery_records && std::ranges::all_of(*pending, [](const auto& aValue) { return aValue.Transmission == application::review_mutation_recovery_transmission::prepared; }), "all prepared records remain discoverable at exact capacity");
		for (const auto& scope : std::array<adapters::sqlite::review_mutation_recovery_configuration, 3>{ { { .StorageRoot = root.Path, .Host = "github.com", .Account = "22", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 },
																											{ .StorageRoot = root.Path, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/another-repository", .PullRequest = 1 },
																											{ .StorageRoot = root.Path, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 2 } } }) {
			auto other = adapters::sqlite::make_review_submission_recovery_store(scope);
			expect(other && (*other)->pending() && (*other)->pending()->empty(), "pending records cannot cross account/repository/PR scope");
		}
		const auto full = store->admit(record());
		expect(!full && full.error() == application::review_mutation_recovery_failure::capacity, "record capacity fails without eviction");
	}
	/// [[BPR-1345]]. A review that carries line comments is snapshotted as
	/// version 2 with each comment's draft, revision, path, side, lines and
	/// words, and reads back exactly after a restart; a summary-only one keeps
	/// version 1 and its fingerprint. An unresolved attempt is withdrawn once
	/// it is definitely not published; a resolved one never is.
	void test_line_comments_and_withdrawal()
	{
		temporary_directory root;
		auto                store = storage(root.Path);
		auto                input = request();
		input.IdempotencyKey      = "review-with-comments";
		input.Proposal.Body.clear();
		auto first              = core::review_submission_comment{ };
		first.Draft             = core::review_draft_id{ 7U };
		first.DraftRevision     = 4U;
		first.Path              = "src/engine/renderer.cpp";
		first.Side              = core::diff_side::right;
		first.Line              = 430U;
		first.Body              = "question: Where does 0.01 come from?";
		auto second             = first;
		second.Draft            = core::review_draft_id{ 8U };
		second.Side             = core::diff_side::left;
		second.Line             = 12U;
		second.StartLine        = 9U;
		second.Body             = "nit: \xC3\xA9 \r\n kept byte for byte";
		input.Proposal.Comments = { first, second };
		const auto summaryOnly  = request();
		expect(application::review_submission_request_fingerprint(input) != application::review_submission_request_fingerprint(summaryOnly),
			   "the comments are part of the request's fingerprint");
		auto withoutComments              = input;
		withoutComments.Proposal.Comments = { };
		withoutComments.Proposal.Body     = "x";
		expect(application::review_submission_request_fingerprint(withoutComments) != application::review_submission_request_fingerprint(input), "and change it");
		const auto expected = record(input);
		auto       admitted = store->admit(expected);
		expect(admitted && admitted->Inserted && admitted->Record == expected, "a review with line comments is admitted exactly");
		store.reset();
		store       = storage(root.Path);
		auto loaded = store->load(input.IdempotencyKey);
		expect(loaded && *loaded && (**loaded).Request == input && (**loaded).Request.Proposal.Comments == input.Proposal.Comments,
			   "after a restart the comments read back exactly, the range and the base side included");
		auto version = std::uint64_t{ 0U };
		{
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2(database_path(root).string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "read the journal");
			sqlite3_stmt* statement = nullptr;
			expect(sqlite3_prepare_v2(database, "SELECT request FROM submission_attempt WHERE idempotency_key='review-with-comments'", -1, &statement, nullptr) == SQLITE_OK, "read the request");
			if (sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_bytes(statement, 0) >= 8) {
				const auto* const bytes = static_cast<const unsigned char*>(sqlite3_column_blob(statement, 0));
				for (auto index = 0; index < 8; ++index) {
					version |= static_cast<std::uint64_t>(bytes[index]) << (8U * static_cast<unsigned>(index));
				}
			}
			sqlite3_finalize(statement);
			sqlite3_close(database);
		}
		expect(version == 2U, "it is stored as request version 2");

		expect(store->withdraw(input.IdempotencyKey, expected.RequestFingerprint).has_value() && store->load(input.IdempotencyKey) && !*store->load(input.IdempotencyKey),
			   "an unresolved attempt is withdrawn");
		expect(store->withdraw(input.IdempotencyKey, expected.RequestFingerprint).has_value(), "withdrawing a missing attempt changes nothing");

		const auto plain = record(summaryOnly);
		expect(store->admit(plain) && store->mark_possibly_transmitted(summaryOnly.IdempotencyKey, plain.RequestFingerprint), "a summary-only attempt, sent");
		auto wrong = plain.RequestFingerprint;
		wrong[0] ^= 1U;
		expect(!store->withdraw(summaryOnly.IdempotencyKey, wrong) && *store->load(summaryOnly.IdempotencyKey), "another fingerprint withdraws nothing");
		expect(store->resolve(summaryOnly.IdempotencyKey, plain.RequestFingerprint, receipt(summaryOnly)).has_value(), "it resolves");
		expect(!store->withdraw(summaryOnly.IdempotencyKey, plain.RequestFingerprint) && *store->load(summaryOnly.IdempotencyKey),
			   "a resolved attempt is never withdrawn");
	}
	/// [[BPR-1560]]. A review whose LEFT lines are numbered in a merge base
	/// other than the recorded base keeps that comparison base across a
	/// restart (request version 3), with or without line comments, and the
	/// same binding without one keeps its earlier bytes; the merge base moving
	/// keeps the submission's identity, so neither a draft nor a pending
	/// attempt is lost to it.
	void test_comparison_base_survives_a_restart()
	{
		temporary_directory root;
		auto                store                   = storage(root.Path);
		auto                input                   = request();
		input.IdempotencyKey                        = "review-on-a-merge-base";
		input.LiveBinding->Target.ComparisonBaseOid = commit(0x12U);
		auto comment                                = core::review_submission_comment{ };
		comment.Draft                               = core::review_draft_id{ 9U };
		comment.DraftRevision                       = 1U;
		comment.Path                                = "a.txt";
		comment.Side                                = core::diff_side::left;
		comment.Line                                = 2U;
		comment.Body                                = "the merge base's line";
		auto summary                                = input;
		summary.IdempotencyKey                      = "summary-on-a-merge-base";
		input.Proposal.Comments                     = { comment };
		expect(application::comparison_base(input.LiveBinding->Target) == commit(0x12U), "LEFT lines are numbered in the merge base");
		expect(application::review_submission_authority_fingerprint(*input.LiveBinding) != application::review_submission_authority_fingerprint(*request().LiveBinding),
			   "a distinct comparison base is part of the authority");
		for (const auto& attempt : { input, summary }) {
			expect(store->admit(record(attempt)).has_value(), "admitted");
		}
		store.reset();
		store = storage(root.Path);
		for (const auto& attempt : { input, summary }) {
			const auto loaded = store->load(attempt.IdempotencyKey);
			expect(loaded && *loaded && (**loaded).Request == attempt && (**loaded).Request.LiveBinding->Target.ComparisonBaseOid == commit(0x12U),
				   "after a restart the comparison base reads back exactly");
		}
		auto moved                     = *input.LiveBinding;
		moved.Target.ComparisonBaseOid = commit(0x13U);
		expect(application::same_review_submission_identity(*input.LiveBinding, moved), "a merge base that moved is the same submission");
		auto plain = request();
		expect(application::comparison_base(plain.LiveBinding->Target) == plain.LiveBinding->Target.BaseOid, "without one LEFT lines are numbered in the recorded base");
	}
}
auto main() -> int
try {
	test_comparison_base_survives_a_restart();
	test_line_comments_and_withdrawal();
	test_staged_journal();
	test_version_four_migration();
	test_storage_round_trip();
	test_version_three_migration_preserves_whole_reviews();
	test_integrity_and_schema();
	test_capacity_and_baseline_validation();
	test_unknown_reason_is_kept_with_the_attempt();
	test_post_answer_is_kept_with_the_attempt();
	std::cout << "review-submission-recovery-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
