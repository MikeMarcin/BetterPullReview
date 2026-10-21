#include "platform_test_support.hpp"

#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"

#include <sqlite3.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;

	namespace test_platform = bpr::tests::sqlite_contract;

	using bpr::tests::expect;

	class temporary_directory final
	{
	public:
		temporary_directory()
		{
			// [[BPR-1128]], [[BPR-1138]]. Made at a name nothing had, owner-private from creation.
			Path = bpr::tests::make_scratch_directory("bpr-mutation-recovery");
#if defined(_WIN32)
			// The store's own rule for the root's descriptor, written by the suite's
			// helper; `0700` has no Win32 equivalent that `chmod` can express.
			Private = test_platform::make_owner_private(Path, true);
#else
			Private = ::chmod(Path.c_str(), 0700) == 0;
#endif
		}

		~temporary_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}

		std::filesystem::path Path;

		/// \brief Whether the root really became owner-private.
		///
		/// A root that did not is one these tests can say nothing about, so the
		/// result is kept rather than discarded.
		bool Private = false;
	};

	template <typename Identity>
	[[nodiscard]] auto opaque(const std::uint8_t aSeed) -> Identity
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

	[[nodiscard]] auto record()
		-> application::review_mutation_recovery_record
	{
		auto request   = std::array<std::uint8_t, 32>{ };
		auto authority = std::array<std::uint8_t, 32>{ };
		request.fill(0x11U);
		authority.fill(0x22U);
		return {
			.IdempotencyKey       = std::string(64U, 'a'),
			.RequestFingerprint   = request,
			.AuthorityFingerprint = authority,
			.Body                 = "Please rename this.",
			.Anchor               = {
				.Semantic = {
					.Comparison = opaque<core::file_comparison_digest>(0x33U),
					.Side       = core::diff_side::right,
					.Line       = 12U,
				},
				.Path   = "fixture/review.txt",
				.Commit = commit(0x44U),
			},
			.BaselineExactCommentIds = std::vector<std::string>{
				"PRRC_fixture_40",
				"PRRC_fixture_41",
			},
			.Transmission = application::review_mutation_recovery_transmission::prepared,
		};
	}

	[[nodiscard]] auto configuration(const temporary_directory& aDirectory)
		-> adapters::sqlite::review_mutation_recovery_configuration
	{
		return {
			.StorageRoot = aDirectory.Path,
			.Host        = "github.com",
			.Account     = "21",
			.Repository  = "MikeMarcin/BPR-Test",
			.PullRequest = 1,
		};
	}

	[[nodiscard]] auto database_path(const temporary_directory& aDirectory)
		-> std::filesystem::path
	{
		return aDirectory.Path / "state" / "v1"
			/ "review-mutation-recovery.sqlite3";
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

	[[nodiscard]] auto user_version(const std::filesystem::path& aPath)
		-> std::int64_t
	{
		::sqlite3* database = nullptr;
		expect(::sqlite3_open_v2(
				   aPath.string().c_str(),
				   &database,
				   SQLITE_OPEN_READONLY,
				   nullptr)
				   == SQLITE_OK,
			   "recovery schema version opens directly");
		auto statement = static_cast<::sqlite3_stmt*>(nullptr);
		expect(::sqlite3_prepare_v2(
				   database, "PRAGMA user_version", -1, &statement, nullptr)
					   == SQLITE_OK
				   && ::sqlite3_step(statement) == SQLITE_ROW,
			   "recovery schema version is readable");
		const auto result = ::sqlite3_column_int64(statement, 0);
		static_cast<void>(::sqlite3_finalize(statement));
		static_cast<void>(::sqlite3_close_v2(database));
		return result;
	}

	/// \brief One text value a query reads directly from the database.
	[[nodiscard]] auto query_text(const std::filesystem::path& aPath, const std::string& aSql) -> std::string
	{
		::sqlite3* database = nullptr;
		expect(::sqlite3_open_v2(aPath.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "recovery database opens directly");
		auto statement = static_cast<::sqlite3_stmt*>(nullptr);
		expect(::sqlite3_prepare_v2(database, aSql.c_str(), -1, &statement, nullptr) == SQLITE_OK && ::sqlite3_step(statement) == SQLITE_ROW,
			   "the direct query has a row");
		const auto* const text   = ::sqlite3_column_text(statement, 0);
		auto              result = text != nullptr ? std::string{ reinterpret_cast<const char*>(text) } : std::string{ };
		static_cast<void>(::sqlite3_finalize(statement));
		static_cast<void>(::sqlite3_close_v2(database));
		return result;
	}

	[[nodiscard]] auto repeated_hex(
		const std::string_view aByte,
		const std::size_t      aCount) -> std::string
	{
		auto result = std::string{ };
		result.reserve(aByte.size() * aCount);
		for (auto index = std::size_t{ }; index < aCount; ++index) {
			result += aByte;
		}
		return result;
	}

	void materialize_version_one_database(const temporary_directory& aDirectory)
	{
		const auto state = aDirectory.Path / "state" / "v1";
		std::filesystem::create_directories(state);
#if defined(_WIN32)
		// The store creates these two through `ensure_private_child`, which
		// writes an owner-private descriptor. This fixture makes them directly,
		// so it has to match — there was no Windows arm here, and the store
		// refused the directories it was handed.
		expect(test_platform::make_owner_private(aDirectory.Path / "state", true),
			   "the fixture state directory becomes owner-private");
		expect(test_platform::make_owner_private(state, true),
			   "the fixture v1 directory becomes owner-private");
#else
		static_cast<void>(::chmod((aDirectory.Path / "state").c_str(), 0700));
		static_cast<void>(::chmod(state.c_str(), 0700));
#endif
		const auto keyA       = std::string(64U, 'a');
		const auto keyB       = std::string(64U, 'b');
		const auto request    = repeated_hex("11", 32U);
		const auto authority  = repeated_hex("22", 32U);
		const auto comparison = repeated_hex("33", 32U);
		const auto oid        = repeated_hex("44", 20U) + repeated_hex("00", 12U);
		execute_sql(
			database_path(aDirectory),
			"PRAGMA application_id = 1112560205;"
			"PRAGMA user_version = 1;"
			"CREATE TABLE recovery_meta(singleton INTEGER PRIMARY KEY "
			"CHECK(singleton = 1),schema_format INTEGER NOT NULL) STRICT;"
			"CREATE TABLE recovery_scope(scope_id INTEGER PRIMARY KEY,"
			"host TEXT NOT NULL,account TEXT NOT NULL,repository TEXT NOT NULL,"
			"pull_request INTEGER NOT NULL,"
			"UNIQUE(host,account,repository,pull_request)) STRICT;"
			"CREATE TABLE recovery_attempt("
			"scope_id INTEGER NOT NULL REFERENCES recovery_scope(scope_id) ON DELETE CASCADE,"
			"idempotency_key TEXT NOT NULL CHECK(length(idempotency_key) = 64),"
			"request_fingerprint BLOB NOT NULL CHECK(length(request_fingerprint) = 32),"
			"authority_fingerprint BLOB NOT NULL CHECK(length(authority_fingerprint) = 32),"
			"body TEXT NOT NULL CHECK(length(CAST(body AS BLOB)) BETWEEN 1 AND 65536),"
			"resolved_path TEXT NOT NULL CHECK(length(CAST(resolved_path AS BLOB)) BETWEEN 1 AND 4096),"
			"comparison BLOB NOT NULL CHECK(length(comparison) = 32),"
			"semantic_line INTEGER NOT NULL CHECK(semantic_line BETWEEN 0 AND 4294967295),"
			"commit_algorithm INTEGER NOT NULL CHECK(commit_algorithm BETWEEN 1 AND 2),"
			"commit_oid BLOB NOT NULL CHECK(length(commit_oid) = 32),"
			"state INTEGER NOT NULL CHECK(state BETWEEN 0 AND 1),"
			"server_comment_id TEXT CHECK(server_comment_id IS NULL OR "
			"length(CAST(server_comment_id AS BLOB)) BETWEEN 1 AND 256),"
			"CHECK((state = 0 AND server_comment_id IS NULL) OR "
			"(state = 1 AND server_comment_id IS NOT NULL)),"
			"PRIMARY KEY(scope_id,idempotency_key)) STRICT;"
			"INSERT INTO recovery_meta VALUES(1,1);"
			"INSERT INTO recovery_scope VALUES(1,'github.com','21','MikeMarcin/BPR-Test',1);"
			"INSERT INTO recovery_attempt VALUES(1,'"
				+ keyA + "',X'" + request
				+ "',X'" + authority
				+ "','Please rename this.','fixture/review.txt',X'" + comparison
				+ "',12,1,X'" + oid + "',0,NULL);"
									  "INSERT INTO recovery_attempt VALUES(1,'"
				+ keyB + "',X'" + request
				+ "',X'" + authority
				+ "','Please rename this.','fixture/review.txt',X'" + comparison
				+ "',12,1,X'" + oid + "',1,'PRRC_fixture_old');");
#if !defined(_WIN32)
		static_cast<void>(::chmod(database_path(aDirectory).c_str(), 0600));
#endif
	}

	[[nodiscard]] auto publication() -> application::review_publication_recovery_record
	{
		application::review_publication_recovery_record value{
			.Key = std::string(64U, 'b'), .LegacyKey = std::string(64U, 'c'), .Fingerprint = { 1 }, .State = core::review_publication_state::native_prepared, .Native = record()
		};
		value.Native->IdempotencyKey = application::review_publication_child_key(value.Key, core::review_publication_representation::native_line);
		return value;
	}
	[[nodiscard]] auto fallback_child(const application::review_publication_recovery_record& aParent) -> application::review_mutation_recovery_record
	{
		auto child           = record();
		child.IdempotencyKey = application::review_publication_child_key(aParent.Key, core::review_publication_representation::file_comment_with_permalink);
		child.Body           = "https://github.com/example/repository/blob/immutable/file#L13\n\nPlease rename this.";
		child.RequestFingerprint.fill(0x55U);
		return child;
	}
	void test_logical_publication_transitions_and_restart()
	{
		using state = core::review_publication_state;
		temporary_directory directory;
		auto                store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect(store.has_value(), "logical journal opens");
		auto value = publication();
		expect((*store)->advance_publication(std::nullopt, value).has_value(), "native parent and child admitted atomically");
		expect(!(*store)->advance_publication(std::nullopt, value), "duplicate admission cannot grant a second POST");
		auto next     = value;
		next.State    = state::fallback_prepared;
		next.Fallback = fallback_child(value);
		expect(!(*store)->advance_publication(value, next), "prepared native cannot authorize fallback");
		next                      = value;
		next.State                = state::native_possibly_transmitted;
		next.Native->Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
		expect((*store)->advance_publication(value, next).has_value(), "native witness commits");
		value              = next;
		next.State         = state::fallback_prepared;
		next.Fallback      = fallback_child(value);
		next.Authorization = { .Source = application::review_fallback_authorization_source::scoped_evidence, .ContractDigest = { 1 }, .ScopeDigest = { 2 }, .Generation = 1, .ObservedAt = 100, .ExpiresAt = 400 };
		expect(!(*store)->advance_publication(value, next), "cached evidence cannot bypass indeterminate native");
		store->reset();
		store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect((*store)->load_publication(value.Key).value() == value, "restart keeps exact native witness and baseline");
		next               = value;
		next.State         = state::native_definitively_unsupported;
		next.Authorization = { .Source = application::review_fallback_authorization_source::native_rejection, .ContractDigest = { 1 }, .ScopeDigest = { 2 } };
		expect((*store)->advance_publication(value, next).has_value(), "exact rejection resolves witness durably");
		value         = next;
		next.State    = state::fallback_prepared;
		next.Fallback = fallback_child(value);
		expect((*store)->advance_publication(value, next).has_value(), "authorized parent admits fallback child atomically");
		value = next;
		expect(!(*store)->advance_publication(value, publication()), "fallback never reopens native");
		next.State                  = state::fallback_possibly_transmitted;
		next.Fallback->Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
		expect((*store)->advance_publication(value, next).has_value(), "fallback witness commits");
		value                  = next;
		next.State             = state::succeeded_fallback;
		next.Fallback->State   = application::review_mutation_recovery_state::succeeded;
		next.Fallback->Receipt = application::review_mutation_receipt{ .ServerCommentId = "PRRC_fixture_40", .Representation = core::review_publication_representation::file_comment_with_permalink };
		expect(!(*store)->advance_publication(value, next), "baseline identity cannot become new receipt");
		next.Fallback->Receipt->ServerCommentId = "PRRC_new";
		expect((*store)->advance_publication(value, next).has_value(), "fallback success and receipt commit together");
		value = next;
		store->reset();
		store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect((*store)->load_publication(value.Key).value() == value, "restart retains representation and canonical receipt");
		auto legacy           = record();
		legacy.IdempotencyKey = value.LegacyKey;
		expect(!(*store)->admit(legacy), "legacy alias cannot start another write");
		execute_sql(database_path(directory), "UPDATE logical_publication SET record_digest=zeroblob(32)");
		expect(!(*store)->load_publication(value.Key), "corruption refuses entire parent including child baselines");
	}
	// [[BPR-1212]] v002. A base-side comment's publication keeps its side
	// across a restart: its record is written as version 2, which carries each
	// child's side, while a head-side record keeps its version 1 bytes. The
	// side-less legacy table refuses a base-side anchor.
	void test_a_base_side_publication_keeps_its_side()
	{
		temporary_directory directory;
		auto                store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect(store.has_value(), "logical journal opens");
		auto right = publication();
		expect((*store)->advance_publication(std::nullopt, right).has_value(), "a head-side parent is admitted");
		// The record's first eight bytes are its version, least significant first.
		const auto version = [&directory](const std::string& aKey) {
			return query_text(database_path(directory), "SELECT hex(substr(record,1,8)) FROM logical_publication WHERE publication_key='" + aKey + "'");
		};
		expect(version(right.Key) == "0100000000000000", "a head-side record keeps version 1");

		auto left                         = publication();
		left.Key                          = std::string(64U, 'd');
		left.LegacyKey                    = std::string(64U, 'e');
		left.Native->IdempotencyKey       = application::review_publication_child_key(left.Key, core::review_publication_representation::native_line);
		left.Native->Anchor.Semantic.Side = core::diff_side::left;
		expect((*store)->advance_publication(std::nullopt, left).has_value(), "a base-side parent is admitted");
		expect(version(left.Key) == "0200000000000000", "a base-side record is written as version 2");
		store->reset();
		store             = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		const auto loaded = (*store)->load_publication(left.Key);
		expect(loaded.has_value() && loaded->has_value() && **loaded == left && (*loaded)->Native->Anchor.Semantic.Side == core::diff_side::left,
			   "after a restart the base-side child is still on the left");
		expect((*store)->load_publication(right.Key).value() == right, "the head-side record reads back unchanged");

		auto legacy                 = record();
		legacy.IdempotencyKey       = std::string(64U, 'f');
		legacy.Anchor.Semantic.Side = core::diff_side::left;
		expect(!(*store)->admit(legacy), "the side-less legacy table refuses a base-side anchor");
	}

	// RFC-BPR-0089 M4. A commit-filtered diff's comment placed off its own
	// line keeps its remote line and commit across a restart: its record is
	// written as version 3, while every other record keeps its bytes. The
	// legacy table, which has no column for it, refuses it.
	void test_a_placed_publication_keeps_its_remote_line()
	{
		temporary_directory directory;
		auto                store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect(store.has_value(), "logical journal opens");
		const auto version = [&directory](const std::string& aKey) {
			return query_text(database_path(directory), "SELECT hex(substr(record,1,8)) FROM logical_publication WHERE publication_key='" + aKey + "'");
		};
		auto placed                       = publication();
		placed.Key                        = std::string(64U, '7');
		placed.LegacyKey                  = std::string(64U, '8');
		placed.Native->IdempotencyKey     = application::review_publication_child_key(placed.Key, core::review_publication_representation::native_line);
		placed.Native->Anchor.RemoteLine  = 41U;
		auto earlier                      = placed;
		earlier.Key                       = std::string(64U, '9');
		earlier.LegacyKey                 = std::string(64U, 'a');
		earlier.Native->IdempotencyKey    = application::review_publication_child_key(earlier.Key, core::review_publication_representation::native_line);
		earlier.Native->Anchor.Commit     = core::git_commit_oid{ core::git_hash_algorithm::sha1, std::array<std::uint8_t, 32>{ 0x25U } };
		earlier.Native->Anchor.RemoteLine = earlier.Native->Anchor.Semantic.Line;
		expect((*store)->advance_publication(std::nullopt, placed).has_value() && (*store)->advance_publication(std::nullopt, earlier).has_value(),
			   "placed parents are admitted");
		expect(version(placed.Key) == "0300000000000000" && version(earlier.Key) == "0300000000000000", "a placed record is written as version 3");
		store->reset();
		store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect((*store)->load_publication(placed.Key).value() == placed && (*store)->load_publication(earlier.Key).value() == earlier,
			   "after a restart each child keeps its remote line and commit");
		auto legacy              = record();
		legacy.IdempotencyKey    = std::string(64U, 'b');
		legacy.Anchor.RemoteLine = 3U;
		expect(!(*store)->admit(legacy), "the legacy table refuses a remote line");
	}

	void test_evidence_admission_and_legacy_conflicts()
	{
		using state = core::review_publication_state;
		temporary_directory directory;
		auto                store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		auto                value = publication();
		value.Native.reset();
		value.State         = state::unsupported_evidence_admitted;
		value.Authorization = { .Source = application::review_fallback_authorization_source::scoped_evidence, .ContractDigest = { 1 }, .ScopeDigest = { 2 }, .Generation = 1, .ObservedAt = 100, .ExpiresAt = 400 };
		expect((*store)->advance_publication(std::nullopt, value).has_value(), "fresh scoped evidence admits parent with no invented native attempt");
		auto next     = value;
		next.State    = state::fallback_prepared;
		next.Fallback = fallback_child(value);
		expect((*store)->advance_publication(value, next).has_value(), "first attempt evidence permits fallback");
		auto conflict                   = next;
		conflict.Fingerprint[0]         = 9U;
		conflict.State                  = state::fallback_possibly_transmitted;
		conflict.Fallback->Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
		expect(!(*store)->advance_publication(next, conflict), "fingerprint cannot change mid-publication");
		auto legacy = record();
		expect((*store)->admit(legacy).has_value(), "legacy fixture admitted");
		value                        = publication();
		value.Key                    = std::string(64U, 'd');
		value.LegacyKey              = legacy.IdempotencyKey;
		value.Native->IdempotencyKey = application::review_publication_child_key(value.Key, core::review_publication_representation::native_line);
		expect(!(*store)->advance_publication(std::nullopt, value), "old unresolved native row bars a new parent");
	}
	/// [[BPR-1706]]. An unresolvable filtered line's admission is the parent's
	/// own first-attempt authorization: no native child before or after, no
	/// freshness window, and it survives a restart, left side and all.
	void test_an_unresolvable_line_admission_survives_a_restart()
	{
		using state  = core::review_publication_state;
		using source = application::review_fallback_authorization_source;
		temporary_directory directory;
		auto                store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		auto                value = publication();
		value.Native.reset();
		value.State                       = state::unresolvable_line_admitted;
		value.Authorization               = { .Source = source::unresolvable_line, .ContractDigest = { 3 }, .ScopeDigest = { 4 } };
		auto windowed                     = value;
		windowed.Authorization.Generation = 1U;
		windowed.Authorization.ObservedAt = 100U;
		windowed.Authorization.ExpiresAt  = 400U;
		expect(!application::valid(windowed), "an unresolvable line's admission carries no freshness window");
		auto withNative                   = value;
		withNative.Native                 = record();
		withNative.Native->IdempotencyKey = application::review_publication_child_key(value.Key, core::review_publication_representation::native_line);
		expect(!application::valid(withNative), "it never has a native child");
		auto mislabelled                 = value;
		mislabelled.Authorization.Source = source::scoped_evidence;
		expect(!application::valid(mislabelled), "nor is it scoped evidence");
		expect((*store)->advance_publication(std::nullopt, value).has_value(), "the admission is a first attempt");
		auto next                           = value;
		next.State                          = state::fallback_prepared;
		next.Fallback                       = fallback_child(value);
		next.Fallback->Anchor.Semantic.Side = core::diff_side::left;
		expect((*store)->advance_publication(value, next).has_value(), "it permits exactly its file comment, here on a left line");
		auto native          = value;
		native.State         = state::native_prepared;
		native.Native        = withNative.Native;
		native.Authorization = { };
		expect(!(*store)->advance_publication(next, native), "a prepared file comment never turns native");
		store->reset();
		store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect((*store)->load_publication(value.Key).value() == next, "after a restart the file comment and its admission are loaded exactly");
	}

	void test_logical_capacity_and_stale_compare_exchange()
	{
		temporary_directory directory;
		auto                store   = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		auto                sibling = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		const auto          value   = publication();
		expect((*store)->advance_publication(std::nullopt, value).has_value(), "parent admitted");
		auto next                 = value;
		next.State                = core::review_publication_state::native_possibly_transmitted;
		next.Native->Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
		expect((*store)->advance_publication(value, next).has_value() && !(*sibling)->advance_publication(value, next), "exact CAS grants only one transmission witness across connections");
		for (std::size_t index = 1; index < application::maximum_review_mutation_recovery_records; ++index) {
			auto candidate                   = publication();
			candidate.Key                    = application::review_publication_child_key(std::to_string(index), core::review_publication_representation::native_line);
			candidate.LegacyKey              = application::review_publication_child_key(std::to_string(index), core::review_publication_representation::file_comment_with_permalink);
			candidate.Native->IdempotencyKey = application::review_publication_child_key(candidate.Key, core::review_publication_representation::native_line);
			expect((*store)->advance_publication(std::nullopt, candidate).has_value(), "bounded logical rows admitted");
		}
		auto extra                   = publication();
		extra.Key                    = std::string(64U, 'e');
		extra.LegacyKey              = std::string(64U, 'f');
		extra.Native->IdempotencyKey = application::review_publication_child_key(extra.Key, core::review_publication_representation::native_line);
		const auto refused           = (*store)->advance_publication(std::nullopt, extra);
		expect(!refused && refused.error() == application::review_mutation_recovery_failure::capacity, "logical capacity refuses before publication");
		expect(!(*store)->admit(record()), "logical and legacy rows share one capacity bound");
	}

	void test_version_three_keeps_both_journals()
	{
		temporary_directory directory;
		auto                store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect((*store)->admit(record()).has_value(), "v3 native input admitted");
		store->reset();
		execute_sql(database_path(directory), "DROP TABLE submission_unknown; DROP TABLE staged_publication; DROP TABLE logical_publication; PRAGMA user_version=3; UPDATE recovery_meta SET schema_format=3;");
		store = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect(store && user_version(database_path(directory)) == 6 && (*store)->load(record().IdempotencyKey).value() == record(), "v3 migrates transactionally without altering native row");
		auto submission = adapters::sqlite::make_review_submission_recovery_store(configuration(directory));
		expect(submission.has_value(), "whole-review journal remains compatible after migration");
	}

	void test_durable_unresolved_and_success_reopen()
	{
		auto directory = temporary_directory{ };
		expect(directory.Private, "the recovery root becomes owner-private");
		auto first = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(first.has_value(), "owner-private recovery store opens");
		const auto expected = record();
		const auto admitted = (*first)->admit(expected);
		expect(admitted.has_value() && admitted->Inserted
				   && admitted->Record == expected,
			   "unresolved attempt is durably admitted");
		const auto duplicateAdmission = (*first)->admit(expected);
		expect(duplicateAdmission.has_value() && !duplicateAdmission->Inserted
				   && duplicateAdmission->Record == expected,
			   "an existing exact attempt is distinguished from a new admission");
		const auto loaded = (*first)->load(expected.IdempotencyKey);
		expect(loaded.has_value() && loaded->has_value() && **loaded == expected,
			   "unresolved attempt reads back exactly");

		auto conflicting = expected;
		conflicting.RequestFingerprint[0] ^= 0xFFU;
		const auto refused = (*first)->admit(conflicting);
		expect(!refused.has_value()
				   && refused.error()
					   == application::review_mutation_recovery_failure::conflict,
			   "same key with another request fingerprint conflicts");

		const auto receipt = application::review_mutation_receipt{
			.ServerCommentId = "PRRC_fixture_44",
		};
		const auto marked = (*first)->mark_possibly_transmitted(
			expected.IdempotencyKey, expected.RequestFingerprint);
		expect(marked.has_value()
				   && marked->Transmission
					   == application::review_mutation_recovery_transmission::possibly_transmitted,
			   "POST creation witness is durably recorded before resolution");
		const auto markedAgain = (*first)->mark_possibly_transmitted(
			expected.IdempotencyKey, expected.RequestFingerprint);
		expect(markedAgain.has_value() && *markedAgain == *marked,
			   "the durable transmission witness is idempotent for one exact attempt");
		const auto baselineReceipt = (*first)->resolve(
			expected.IdempotencyKey,
			expected.RequestFingerprint,
			{ .ServerCommentId = "PRRC_fixture_40" });
		expect(!baselineReceipt.has_value()
				   && baselineReceipt.error()
					   == application::review_mutation_recovery_failure::conflict,
			   "the store cannot persist a direct receipt already present in baseline");
		const auto resolved = (*first)->resolve(
			expected.IdempotencyKey, expected.RequestFingerprint, receipt);
		expect(resolved.has_value()
				   && resolved->State
					   == application::review_mutation_recovery_state::succeeded
				   && resolved->Receipt.has_value()
				   && resolved->Receipt->ServerCommentId == receipt.ServerCommentId,
			   "only an exact receipt resolves the attempt");
		first->reset();

		auto reopened = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(reopened.has_value(), "recovery store reopens after process boundary");
		const auto durable = (*reopened)->load(expected.IdempotencyKey);
		expect(durable.has_value() && durable->has_value()
				   && (**durable).State
					   == application::review_mutation_recovery_state::succeeded
				   && (**durable).Receipt == resolved->Receipt,
			   "server-confirmed receipt survives reopen");

#if !defined(_WIN32)
		struct stat status{ };
		const auto  path = directory.Path / "state" / "v1"
			/ "review-mutation-recovery.sqlite3";
		expect(::stat(path.c_str(), &status) == 0
				   && (status.st_mode & static_cast<mode_t>(0777)) == 0600,
			   "recovery database is owner-private");
#endif
	}

	void test_schema_identity_is_fail_closed_and_credential_free()
	{
		auto directory = temporary_directory{ };
		expect(directory.Private, "the recovery root becomes owner-private");
		auto store = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(store.has_value(), "schema fixture opens");
		store->reset();
		const auto path = directory.Path / "state" / "v1"
			/ "review-mutation-recovery.sqlite3";
		::sqlite3* database = nullptr;
		expect(::sqlite3_open_v2(
				   path.string().c_str(),
				   &database,
				   SQLITE_OPEN_READWRITE,
				   nullptr)
				   == SQLITE_OK,
			   "schema inventory opens directly");
		auto statement = static_cast<::sqlite3_stmt*>(nullptr);
		expect(::sqlite3_prepare_v2(
				   database,
				   "PRAGMA table_info(recovery_attempt)",
				   -1,
				   &statement,
				   nullptr)
				   == SQLITE_OK,
			   "schema columns are inspectable");
		while (::sqlite3_step(statement) == SQLITE_ROW) {
			const auto* raw  = ::sqlite3_column_text(statement, 1);
			const auto  name = raw == nullptr
				? std::string{ }
				: std::string{ reinterpret_cast<const char*>(raw) };
			expect(name.find("credential") == std::string::npos
					   && name.find("token") == std::string::npos
					   && name.find("secret") == std::string::npos,
				   "recovery schema has no credential-bearing column");
		}
		static_cast<void>(::sqlite3_finalize(statement));
		expect(::sqlite3_exec(
				   database,
				   "PRAGMA user_version = 7",
				   nullptr,
				   nullptr,
				   nullptr)
				   == SQLITE_OK,
			   "future-version fixture is materialized");
		static_cast<void>(::sqlite3_close_v2(database));
		const auto refused = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(!refused.has_value()
				   && refused.error()
					   == application::review_mutation_recovery_failure::integrity_failure,
			   "future recovery schema is refused without downgrade");
	}

	void test_version_one_migration_preserves_success_and_guards_unresolved()
	{
		auto directory = temporary_directory{ };
		expect(directory.Private, "the recovery root becomes owner-private");
		materialize_version_one_database(directory);
		auto migrated = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(migrated.has_value(), "version-one recovery schema migrates forward");
		const auto unresolved = (*migrated)->load(std::string(64U, 'a'));
		const auto succeeded  = (*migrated)->load(std::string(64U, 'b'));
		expect(unresolved.has_value() && unresolved->has_value()
				   && !(**unresolved).BaselineExactCommentIds.has_value()
				   && (**unresolved).State
					   == application::review_mutation_recovery_state::unresolved,
			   "migrated unresolved rows are explicitly baseline-incomplete");
		expect(succeeded.has_value() && succeeded->has_value()
				   && (**succeeded).State
					   == application::review_mutation_recovery_state::succeeded
				   && (**succeeded).Receipt.has_value()
				   && (**succeeded).Receipt->ServerCommentId == "PRRC_fixture_old",
			   "migrated successful rows retain their exact receipt");
		const auto refused = (*migrated)->resolve(
			std::string(64U, 'a'),
			(**unresolved).RequestFingerprint,
			{ .ServerCommentId = "PRRC_fixture_new" });
		expect(!refused.has_value()
				   && refused.error()
					   == application::review_mutation_recovery_failure::conflict,
			   "baseline-incomplete migrated rows cannot be resolved");
		for (const char alias : { 'a', 'b' }) {
			auto candidate      = publication();
			candidate.LegacyKey = std::string(64U, alias);
			expect(!(*migrated)->advance_publication(std::nullopt, candidate), "migrated unresolved and succeeded aliases cannot admit another publication");
		}
		expect(user_version(database_path(directory)) == 6,
			   "version-one migration commits schema version four");
	}

	void test_version_two_migration_preserves_anchored_rows()
	{
		temporary_directory directory;
		auto                anchored = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect(anchored.has_value(), "v2 fixture opens");
		const auto expected = record();
		expect((*anchored)->admit(expected).has_value(), "anchored row admitted before migration");
		anchored->reset();
		execute_sql(database_path(directory), "DROP TABLE submission_unknown; DROP TABLE staged_publication; DROP TABLE logical_publication; DROP TABLE submission_baseline; DROP TABLE submission_attempt; PRAGMA user_version = 2; UPDATE recovery_meta SET schema_format = 2;");
		auto sibling = adapters::sqlite::make_review_submission_recovery_store(configuration(directory));
		expect(sibling.has_value() && user_version(database_path(directory)) == 6, "submission factory migrates exact v2 database to v4");
		anchored = adapters::sqlite::make_review_mutation_recovery_store(configuration(directory));
		expect(anchored.has_value(), "source-line factory opens shared v4 schema");
		const auto loaded = (*anchored)->load(expected.IdempotencyKey);
		expect(loaded && *loaded && **loaded == expected, "v2 anchored data preserved through sibling migration");
	}

	void test_version_one_schema_drift_is_refused_without_migration()
	{
		auto directory = temporary_directory{ };
		expect(directory.Private, "the recovery root becomes owner-private");
		materialize_version_one_database(directory);
		execute_sql(
			database_path(directory),
			"CREATE TABLE unexpected(value INTEGER) STRICT;");
		const auto refused = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(!refused.has_value()
				   && refused.error()
					   == application::review_mutation_recovery_failure::integrity_failure,
			   "version-one schema drift is refused before migration");
		expect(user_version(database_path(directory)) == 1,
			   "a refused version-one schema remains unmodified");
	}

	void test_baseline_capacity_and_integrity_fail_closed()
	{
		auto directory = temporary_directory{ };
		expect(directory.Private, "the recovery root becomes owner-private");
		auto store = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(store.has_value(), "baseline integrity store opens");

		for (const auto& ids : {
				 std::vector<std::string>{ "b", "a" },
				 std::vector<std::string>{ "a", "a" },
				 std::vector<std::string>{ "" },
				 std::vector<std::string>{ std::string(257U, 'a') },
				 std::vector<std::string>{ std::string{ "a\n" } } }) {
			auto invalid                    = record();
			invalid.BaselineExactCommentIds = ids;
			const auto refused              = (*store)->admit(invalid);
			expect(!refused.has_value()
					   && refused.error()
						   == application::review_mutation_recovery_failure::conflict,
				   "unsorted or duplicate baseline IDs are refused before persistence");
		}
		auto oversized = record();
		oversized.BaselineExactCommentIds->clear();
		for (auto index = std::size_t{ }; index <= 100U; ++index) {
			oversized.BaselineExactCommentIds->push_back(
				"PRRC_" + std::to_string(1'000U + index));
		}
		const auto capacity = (*store)->admit(oversized);
		expect(!capacity.has_value()
				   && capacity.error()
					   == application::review_mutation_recovery_failure::conflict,
			   "more than 100 baseline identities are refused");

		const auto expected = record();
		expect((*store)->admit(expected).has_value(),
			   "integrity fixture persists a valid baseline");
		store->reset();
		execute_sql(
			database_path(directory),
			"UPDATE recovery_baseline SET baseline = CAST(substr(baseline,1,18) "
			"|| X'32' || substr(baseline,20) AS BLOB) WHERE idempotency_key = '"
				+ expected.IdempotencyKey + "';");
		auto substituted = adapters::sqlite::make_review_mutation_recovery_store(
			configuration(directory));
		expect(substituted.has_value(),
			   "valid-framing baseline substitution database opens");
		const auto substitutedLoad = (*substituted)->load(expected.IdempotencyKey);
		expect(!substitutedLoad.has_value()
				   && substitutedLoad.error()
					   == application::review_mutation_recovery_failure::integrity_failure,
			   "same-length valid-framing baseline substitution fails its SHA-256 binding");
		substituted->reset();

		for (const auto& [blob, digest] : {
				 std::pair{ std::string_view{ "000100036162" },
							std::string_view{ "71e97105ef78837004dde107f362ad2252ab420d0b6bf96ff9544ca5aa874c69" } },
				 std::pair{ std::string_view{ "0002000161000161" },
							std::string_view{ "fa3ef5d754da6fd4e44b39a21cf88b721af8bdfd8404b9c4f3198c64f3d684e0" } },
				 std::pair{ std::string_view{ "0002000162000161" },
							std::string_view{ "2c93fc9f31657beb46095f531ef1dbf86885d93c783de54d4a1ca5dbf938ee20" } } }) {
			execute_sql(
				database_path(directory),
				"UPDATE recovery_baseline SET baseline = X'" + std::string{ blob }
					+ "',baseline_digest = X'" + std::string{ digest }
					+ "' WHERE idempotency_key = '" + expected.IdempotencyKey + "';");
			auto reopened = adapters::sqlite::make_review_mutation_recovery_store(
				configuration(directory));
			expect(reopened.has_value(), "corrupt baseline database opens for typed read");
			const auto loaded = (*reopened)->load(expected.IdempotencyKey);
			expect(!loaded.has_value()
					   && loaded.error()
						   == application::review_mutation_recovery_failure::integrity_failure,
				   "malformed, duplicate, and out-of-order baseline blobs fail closed");
			reopened->reset();
		}
	}
} // namespace

auto main() -> int
try {
	test_logical_publication_transitions_and_restart();
	test_a_base_side_publication_keeps_its_side();
	test_a_placed_publication_keeps_its_remote_line();
	test_evidence_admission_and_legacy_conflicts();
	test_an_unresolvable_line_admission_survives_a_restart();
	test_version_three_keeps_both_journals();
	test_logical_capacity_and_stale_compare_exchange();
	test_durable_unresolved_and_success_reopen();
	test_schema_identity_is_fail_closed_and_credential_free();
	test_version_one_migration_preserves_success_and_guards_unresolved();
	test_version_one_schema_drift_is_refused_without_migration();
	test_version_two_migration_preserves_anchored_rows();
	test_baseline_capacity_and_integrity_fail_closed();
	std::cout << "review-mutation-recovery-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
