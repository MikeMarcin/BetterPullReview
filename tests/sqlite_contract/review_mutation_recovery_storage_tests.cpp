#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"

#include <sqlite3.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

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
				   "PRAGMA user_version = 2",
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
} // namespace

auto main() -> int
{
	test_durable_unresolved_and_success_reopen();
	test_schema_identity_is_fail_closed_and_credential_free();
	std::cout << "review-mutation-recovery-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
