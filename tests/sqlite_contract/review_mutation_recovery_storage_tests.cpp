#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"

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
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class temporary_directory final
	{
	public:
		temporary_directory()
		{
			auto seed = std::to_string(reinterpret_cast<std::uintptr_t>(this));
#if !defined(_WIN32)
			seed += "-" + std::to_string(static_cast<std::uint64_t>(::getpid()));
#endif
			Path = std::filesystem::temp_directory_path()
				/ ("bpr-mutation-recovery-" + seed);
			std::filesystem::create_directory(Path);
#if !defined(_WIN32)
			static_cast<void>(::chmod(Path.c_str(), 0700));
#endif
		}

		~temporary_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}

		std::filesystem::path Path;
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
#if !defined(_WIN32)
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

	void test_durable_unresolved_and_success_reopen()
	{
		auto directory = temporary_directory{ };
		auto first     = adapters::sqlite::make_review_mutation_recovery_store(
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
				   && (status.st_mode & 0777) == 0600,
			   "recovery database is owner-private");
#endif
	}

	void test_schema_identity_is_fail_closed_and_credential_free()
	{
		auto directory = temporary_directory{ };
		auto store     = adapters::sqlite::make_review_mutation_recovery_store(
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
				   "PRAGMA user_version = 3",
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
		expect(user_version(database_path(directory)) == 2,
			   "version-one migration commits schema version two");
	}

	void test_version_one_schema_drift_is_refused_without_migration()
	{
		auto directory = temporary_directory{ };
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
		auto store     = adapters::sqlite::make_review_mutation_recovery_store(
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
{
	test_durable_unresolved_and_success_reopen();
	test_schema_identity_is_fail_closed_and_credential_free();
	test_version_one_migration_preserves_success_and_guards_unresolved();
	test_version_one_schema_drift_is_refused_without_migration();
	test_baseline_capacity_and_integrity_fail_closed();
	std::cout << "review-mutation-recovery-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
