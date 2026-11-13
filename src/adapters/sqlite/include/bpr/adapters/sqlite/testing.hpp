#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP

#if defined(BPR_SQLITE_TESTING)
	#include "bpr/application/device_binding_store.hpp"
	#include "bpr/application/epoch_catalog_port.hpp"

	#include <cstddef>
	#include <cstdint>
	#include <filesystem>

struct sqlite3;
namespace bpr::adapters::sqlite::detail::testing {
	using remembered_connection_write_checkpoint = void (*)();
	/// \brief Observes the complete uncommitted connection snapshot; nullptr disarms.
	void set_remembered_connection_write_checkpoint(remembered_connection_write_checkpoint aCheckpoint) noexcept;

	// Arms one test-only interruption after a complete, fsynced bootstrap
	// database exists and before its no-replace canonical publication.
	/// \brief Performs the interrupt next bootstrap before publication operation.
	void interrupt_next_bootstrap_before_publication() noexcept;

	// Arms one test-only interruption after the next supported migration step has
	// been staged and checked, and before the commit that would publish it.
	// What the catalog holds afterwards is exactly what an interrupted
	// migration leaves behind.
	/// \brief Performs the interrupt next migration before commit operation.
	void interrupt_next_migration_before_commit() noexcept;

	// RFC-BPR-0127. Arms one test-only interruption of the next schema 12
	// migration after `aStatements` of its statements have run. The ledger's
	// migration transaction then rolls back, as a crash there would leave it.
	/// \brief Interrupts the next note-to-comment migration after N statements.
	void interrupt_local_comment_migration_after(std::size_t aStatements) noexcept;

	// RFC-BPR-0127. Test data construction, never a downgrade path: turns a
	// schema 12 ledger's Local comments back into schema 11 note threads
	// (mapping, path and line tables) and stamps schema 11, so the older
	// fixtures can be built from a current ledger. False on any failure.
	/// \brief Rebuilds the schema 11 note shape from a current ledger.
	[[nodiscard]] auto restore_schema_11_notes(sqlite3* aDatabase) noexcept -> bool;

	enum class epoch_journal_stage : std::uint8_t {
		receiving,
		spool,
		installation,
	};
	using epoch_journal_checkpoint = void (*)(epoch_journal_stage);
	// Invoked after a record method's SQL writes and validation, before COMMIT.
	// The callback runs inside the transaction and must not reenter the catalog.
	// Passing nullptr disarms it. This API and its storage are test-build only.
	/// \brief Sets the test observer for an uncommitted epoch journal write.
	void set_epoch_journal_checkpoint(epoch_journal_checkpoint aCheckpoint) noexcept;

	// Classifies a SQLite result code without requiring a live connection.
	/// \brief Computes the classify sqlite result code result.
	[[nodiscard]] auto classify_sqlite_result_code(int aResultCode) noexcept
		-> application::epoch_catalog_failure;

	// [[BPR-1111]]. How the device-binding store judges one of its reads that
	// SQLite failed with `aResultCode`, for the results a contract cannot make
	// SQLite produce on demand: the WAL-index protocol and recovery codes.
	/// \brief Classifies a failed device-binding read without a live connection.
	[[nodiscard]] auto classify_device_binding_read(int aResultCode, bool aStopRequested) noexcept
		-> application::device_binding_failure;

	// [[BPR-0813]]. Where a private store's open has reached: its file was
	// admitted, or SQLite has opened it. The observer receives the store file's
	// path and may change the file system, which is how a contract substitutes a
	// reparse point or hard link between admission and open. Passing nullptr
	// disarms it. This API and its storage are test-build only.
	enum class private_store_open_stage : std::uint8_t {
		admitted,
		opened,
	};
	using private_store_open_checkpoint = void (*)(private_store_open_stage, const std::filesystem::path&);
	/// \brief Sets the test observer for a private store's admission and open.
	void set_private_store_open_checkpoint(private_store_open_checkpoint aCheckpoint) noexcept;

	#if defined(_WIN32)
	// What one owner-private catalog object is actually worth to the process
	// that created it. `AceCount` is the measured size of the object's own
	// DACL: one entry outside an AppContainer, and two inside, where the
	// restricted access check also requires the container's package SID.
	/// \brief Records one owner-private catalog object probe.
	struct private_descriptor_probe final
	{
		bool          Contained         = false;
		bool          Created           = false;
		bool          Usable            = false;
		bool          DirectoryValid    = false;
		bool          FileValid         = false;
		std::uint32_t DirectoryAceCount = 0U;
		std::uint32_t FileAceCount      = 0U;
		std::uint32_t LastError         = 0U;
	};

	// Creates a directory and a file below `aDirectory` with the product's own
	// private security attributes, writes to the file, and runs the product's
	// own validator over both.
	/// \brief Computes the probe private descriptor result.
	[[nodiscard]] auto probe_private_descriptor(
		const std::filesystem::path& aDirectory) -> private_descriptor_probe;
	#endif
} // namespace bpr::adapters::sqlite::detail::testing
#endif

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP
