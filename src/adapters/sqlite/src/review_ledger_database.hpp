#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_LEDGER_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_LEDGER_DATABASE_HPP

#include "sqlite_connection.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <stop_token>
#include <string_view>
#include <vector>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	/// \brief The schema version this build writes and validates.
	///
	/// \details Version 1 held the review scope and file receipts. Version 2
	/// added conversation receipts. The value is deliberately not "1": a
	/// migration mechanism whose only version is the initial one has never
	/// migrated anything, and would be first exercised by a user's database.
	inline constexpr std::int64_t review_ledger_schema_version = 2;

	/// \brief Identifies the review a row belongs to.
	///
	/// \details Every ledger row is scoped by all four fields. A pull request
	/// number is unique only within a repository, a repository name only within
	/// an account, and an account only within a host, so no shorter key can
	/// keep two reviews apart. Reusing PR number 7 on a second host must not
	/// surface the first host's receipts.
	struct review_scope
	{
		std::string_view Host;
		std::string_view Account;
		std::string_view Repository;
		std::int64_t     PullRequest = 0;
	};

	/// \brief Owns the private review ledger database.
	///
	/// \details The ledger is a separate file from the epoch catalog, because
	/// deleting private review state must not destroy the derived git cache.
	/// It shares the catalog's connection policy rather than carrying its own.
	class review_ledger_database final
	{
	public:
		review_ledger_database()                                                 = default;
		review_ledger_database(const review_ledger_database&)                    = delete;
		auto operator=(const review_ledger_database&) -> review_ledger_database& = delete;
		review_ledger_database(review_ledger_database&& aOther) noexcept;
		auto operator=(review_ledger_database&& aOther) noexcept
			-> review_ledger_database&;
		~review_ledger_database();

		/// \brief Opens or creates the ledger at one path.
		///
		/// \details A database written by a newer build fails closed with
		/// `schema_mismatch` rather than being opened and partially understood.
		/// Migration is forward-only: there is no downgrade path, because a
		/// downgrade would have to discard data the newer version added.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aPath,
			std::stop_token              aStop)
			-> std::expected<review_ledger_database, failure>;

		[[nodiscard]] auto handle() const noexcept -> ::sqlite3*;

		/// \brief Returns the scope row id, inserting the scope if it is new.
		[[nodiscard]] auto scope_id(const review_scope& aScope)
			-> std::expected<std::int64_t, failure>;

	private:
		explicit review_ledger_database(::sqlite3* aHandle) noexcept;

		::sqlite3* Handle = nullptr;
	};

	/// \brief Returns every column the created schema declares.
	///
	/// \details Reported as `table.column` so a prohibited name can be found
	/// wherever it is added. This exists so the storage prohibitions are
	/// checked against the schema the build actually creates rather than
	/// against the insert statements someone remembered to read.
	[[nodiscard]] auto review_ledger_columns(::sqlite3* aDatabase)
		-> std::expected<std::vector<std::string>, failure>;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_LEDGER_DATABASE_HPP
