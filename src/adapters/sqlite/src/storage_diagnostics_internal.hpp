#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_STORAGE_DIAGNOSTICS_INTERNAL_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_STORAGE_DIAGNOSTICS_INTERNAL_HPP

#include "catalog_database.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

/// [[BPR-1831]]. The SQLite adapter's own hooks into
/// `storage_diagnostics_scope`. Each returns at once when no scope lives.
namespace bpr::adapters::sqlite::detail {
	/// \brief Installs SQLite's error log callback; before `sqlite3_initialize`.
	void install_storage_error_log() noexcept;
	/// \brief Whether a scope lives.
	[[nodiscard]] auto storage_diagnostics_active() noexcept -> bool;
	/// \brief A connection's policy applied, or refused.
	void note_storage_open(::sqlite3* aDatabase, std::optional<catalog_database_failure> aFailure, std::uint32_t aMilliseconds) noexcept;
	/// \brief A store file refused before a connection was made to it.
	void note_storage_refused(std::string_view aFileName, catalog_database_failure aFailure) noexcept;
	/// \brief A statement about to be prepared on `aDatabase`.
	void note_storage_statement(::sqlite3* aDatabase) noexcept;
	/// \brief The statement just prepared, from `aSql`.
	void note_storage_prepared(::sqlite3_stmt* aStatement, std::string_view aSql) noexcept;
	/// \brief A statement finalized: later failures on this thread are not its.
	void note_storage_finalized() noexcept;
	/// \brief A statement run to completion; records it if it wrote a schema version.
	void note_storage_executed(::sqlite3* aDatabase, std::string_view aSql, bool aSucceeded) noexcept;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_STORAGE_DIAGNOSTICS_INTERNAL_HPP
