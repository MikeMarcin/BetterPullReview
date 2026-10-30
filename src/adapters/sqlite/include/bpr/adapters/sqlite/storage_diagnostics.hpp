#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_DIAGNOSTICS_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_DIAGNOSTICS_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/host_operation_diagnostics.hpp"
#include "bpr/core/operation_diagnosis.hpp"

#include <chrono>
#include <memory>
#include <string_view>

namespace bpr::adapters::sqlite {
	/// \brief [[BPR-1831]]. While it lives, every SQLite store in this process
	/// records its diagnostics to one log as `storage.database` lines.
	///
	/// \details What is recorded, each as enums, counts, a duration and a
	/// schema version, never a path, a statement, a key or a value:
	///
	/// - `storage_open`: a connection opened and given the connection policy,
	///   or refused, with the schema version the store had when it opened;
	/// - `storage_migrate`: a schema version written (creating a store is a
	///   migration from 0), with the new version. A migration rolled back
	///   after its version was written still records the version;
	/// - `storage_read`, `storage_write`, `storage_statement`: a statement
	///   that failed, as SQLite reports it through its error log, classified
	///   by its result code. The store and whether it reads or writes are
	///   those of the last statement this thread prepared, which is the
	///   failing one whenever a statement is prepared and stepped in turn;
	///   one that failed to prepare is a `storage_statement`, and an open
	///   refused before any statement is a `storage_open` of store `unknown`.
	///
	/// Opens happen on every call of some stores, and a broken store fails on
	/// every call, so opens and failures are aggregated by
	/// `application::aggregated_operation_log` over `aWindow`; a migration
	/// is rare and records at once. With no scope, nothing is recorded and a
	/// statement costs one relaxed atomic load. Scopes nest: the newest one
	/// that still lives receives. The log must not use SQLite.
	class storage_diagnostics_scope final
	{
	public:
		explicit storage_diagnostics_scope(std::shared_ptr<application::diagnostic_log_port> aLog, std::chrono::steady_clock::duration aWindow = std::chrono::minutes{ 10 });
		~storage_diagnostics_scope();
		storage_diagnostics_scope(const storage_diagnostics_scope&)                    = delete;
		auto operator=(const storage_diagnostics_scope&) -> storage_diagnostics_scope& = delete;

		/// \brief Records what is held back now.
		void flush() noexcept;

	private:
		std::shared_ptr<application::aggregated_operation_log> Log;
	};

	/// \brief [[BPR-1831]]. The store a database file's name belongs to:
	/// each store's fixed file name, `other` for any other name.
	[[nodiscard]] auto storage_store_of(std::string_view aFileName) noexcept -> core::operation_store;
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_STORAGE_DIAGNOSTICS_HPP
