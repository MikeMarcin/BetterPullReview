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
			execute_sql(database(root), "DROP TABLE logical_publication; PRAGMA user_version=3; UPDATE recovery_meta SET schema_format=3;");
			store = storage(root.Path);
			expect(store->load(expected.Request.IdempotencyKey).value() == *saved, "v4 migration retains exact whole-review witness or receipt");
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
		for (const auto* mutation : { "PRAGMA user_version = 5", "ALTER TABLE submission_attempt ADD COLUMN unexpected TEXT" }) {
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
		expect(pending && pending->size() == application::maximum_review_mutation_recovery_records && std::ranges::all_of(*pending, [](const auto& value) { return value.Transmission == application::review_mutation_recovery_transmission::prepared; }), "all prepared records remain discoverable at exact capacity");
		for (const auto& scope : std::array<adapters::sqlite::review_mutation_recovery_configuration, 3>{ { { .StorageRoot = root.Path, .Host = "github.com", .Account = "22", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 1 },
																											{ .StorageRoot = root.Path, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/another-repository", .PullRequest = 1 },
																											{ .StorageRoot = root.Path, .Host = "github.com", .Account = "21", .Repository = "MikeMarcin/BPR-Test", .PullRequest = 2 } } }) {
			auto other = adapters::sqlite::make_review_submission_recovery_store(scope);
			expect(other && (*other)->pending() && (*other)->pending()->empty(), "pending records cannot cross account/repository/PR scope");
		}
		const auto full = store->admit(record());
		expect(!full && full.error() == application::review_mutation_recovery_failure::capacity, "record capacity fails without eviction");
	}
}
auto main() -> int
try {
	test_storage_round_trip();
	test_version_three_migration_preserves_whole_reviews();
	test_integrity_and_schema();
	test_capacity_and_baseline_validation();
	std::cout << "review-submission-recovery-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
