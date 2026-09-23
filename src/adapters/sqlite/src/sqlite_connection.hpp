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

	/// \brief Reports whether the SQLite library initialised successfully.
	///
	/// \details The build sets `SQLITE_OMIT_AUTOINIT`, so `sqlite3_initialize`
	/// must run before any other API call. Every store shares this one
	/// initialisation: a second store that opened a database without it would
	/// not fail cleanly, it would jump through an uninitialised pointer.
	[[nodiscard]] auto sqlite_runtime_ready() noexcept -> bool;

	/// \brief Installs one progress handler on a connection for one scope.
	///
	/// \details SQLite keeps a single progress handler per connection and
	/// cannot report which one is installed. A scope that cleared the handler
	/// when it ended therefore discarded whatever an enclosing scope had
	/// installed, and a store's step ceiling lapsed at the first nested
	/// `execute` ([[BPR-0887]]). Every handler this adapter installs goes
	/// through this class instead. The innermost scope on a connection governs
	/// while it lives. When it ends, the enclosing scope's handler is
	/// reinstated with its context, and so its step count, intact.
	///
	/// The innermost scope governs alone, rather than alongside the enclosing
	/// one, so that a `ROLLBACK` run without a stop token is not interrupted
	/// by an enclosing ceiling.
	///
	/// The record of enclosing scopes belongs to the calling thread. A scope
	/// must have automatic storage, and the scopes on one connection must nest
	/// on one thread.
	class progress_handler_scope final
	{
	public:
		/// \brief The callback SQLite runs; nonzero interrupts the statement.
		using handler = int (*)(void*);

		progress_handler_scope(::sqlite3* aDatabase, int aInterval, handler aHandler, void* aContext) noexcept;
		progress_handler_scope(const progress_handler_scope&)                    = delete;
		auto operator=(const progress_handler_scope&) -> progress_handler_scope& = delete;
		progress_handler_scope(progress_handler_scope&&)                         = delete;
		auto operator=(progress_handler_scope&&) -> progress_handler_scope&      = delete;
		~progress_handler_scope();

	private:
		::sqlite3*              Database;
		int                     Interval;
		handler                 Handler;
		void*                   Context;
		progress_handler_scope* Outer;
	};

	/// \brief Installs a cancellation-aware progress handler for one scope.
	///
	/// \details Nests like `progress_handler_scope`: when it ends, the handler
	/// it replaced is reinstated.
	class progress_scope final
	{
	public:
		progress_scope(::sqlite3* aDatabase, std::stop_token aStop);
		progress_scope(const progress_scope&)                    = delete;
		auto operator=(const progress_scope&) -> progress_scope& = delete;
		progress_scope(progress_scope&&)                         = delete;
		auto operator=(progress_scope&&) -> progress_scope&      = delete;
		~progress_scope()                                        = default;

	private:
		std::stop_token        Stop;
		progress_handler_scope Handler;
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
