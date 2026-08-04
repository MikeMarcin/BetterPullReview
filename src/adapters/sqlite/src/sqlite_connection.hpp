#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_SQLITE_CONNECTION_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_SQLITE_CONNECTION_HPP

#include "catalog_database.hpp"

#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

struct sqlite3;
struct sqlite3_stmt;

namespace bpr::adapters::sqlite::detail {
	using failure = catalog_database_failure;

	/// \brief Owns one prepared statement for the length of a scope.
	struct statement final
	{
		statement()                                    = default;
		statement(const statement&)                    = delete;
		auto operator=(const statement&) -> statement& = delete;
		statement(statement&& aOther) noexcept;
		auto operator=(statement&& aOther) noexcept -> statement&;
		~statement();

		::sqlite3_stmt* Value = nullptr;
	};

	/// \brief Classifies one SQLite result code as a database failure.
	[[nodiscard]] auto catalog_failure_from_sqlite_result(
		int     aResultCode,
		failure aFallback = failure::unavailable) noexcept -> failure;

	/// \brief Classifies the connection's last error as a database failure.
	[[nodiscard]] auto catalog_sql_failure(
		::sqlite3* aDatabase,
		failure    aFallback = failure::unavailable) noexcept -> failure;

	/// \brief Installs a cancellation-aware progress handler for one scope.
	class progress_scope final
	{
	public:
		progress_scope(::sqlite3* aDatabase, std::stop_token aStop);
		progress_scope(const progress_scope&)                    = delete;
		auto operator=(const progress_scope&) -> progress_scope& = delete;
		progress_scope(progress_scope&&)                         = delete;
		auto operator=(progress_scope&&) -> progress_scope&      = delete;
		~progress_scope();

	private:
		::sqlite3*      Database;
		std::stop_token Stop;
	};

	/// \brief Prepares one statement, refusing any trailing statement text.
	[[nodiscard]] auto prepare(::sqlite3* aDatabase, std::string_view aSql)
		-> std::expected<statement, failure>;

	/// \brief Runs one statement to completion.
	[[nodiscard]] auto execute(
		::sqlite3*       aDatabase,
		std::string_view aSql,
		std::stop_token  aStop = { }) -> std::expected<void, failure>;

	/// \brief Reads one integer scalar.
	[[nodiscard]] auto scalar_integer(::sqlite3* aDatabase, std::string_view aSql)
		-> std::expected<std::int64_t, failure>;

	/// \brief Reads one text scalar.
	[[nodiscard]] auto scalar_text(::sqlite3* aDatabase, std::string_view aSql)
		-> std::expected<std::string, failure>;

	/// \brief Normalises one statement's text for comparison.
	[[nodiscard]] auto canonical_sql(std::string_view aSql) -> std::string;

	/// \brief Applies the hardened connection policy to one open database.
	///
	/// \details This is the project's single connection policy: defensive mode,
	/// no extensions or views or triggers, foreign keys on, WAL, full synchronous
	/// commits, a pinned page size, and a page-count ceiling. Every database this
	/// project opens goes through it, so a second store cannot quietly adopt
	/// weaker settings than the one that was reviewed.
	[[nodiscard]] auto configure_connection(::sqlite3* aDatabase)
		-> std::expected<void, failure>;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_SQLITE_CONNECTION_HPP
