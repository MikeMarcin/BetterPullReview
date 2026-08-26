#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_WORKBENCH_SETTINGS_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_WORKBENCH_SETTINGS_DATABASE_HPP

#include "sqlite_connection.hpp"

#include "bpr/application/workbench_runtime_ports.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	/// \brief The schema version this build writes and validates.
	///
	/// \details Two since BPR-0457, and this is the store's first real
	/// migration. It was one until a durable field had to be added to a table
	/// that already had rows in it, which is exactly the case the earlier note
	/// said inventing a version could not exercise. Both directions are tested:
	/// a version-one database gains the column and keeps its row, and a
	/// database stamped newer still fails closed rather than being partially
	/// understood.
	inline constexpr std::int64_t workbench_settings_schema_version = 2;

	/// \brief Owns the durable workbench layout database.
	///
	/// \details A separate file from the epoch catalog, the review ledger, and
	/// the queue enrollment store, for the reason those three are separate from
	/// each other: deleting one kind of local state must not destroy another.
	/// Losing a drawer width must not lose reading progress.
	///
	/// The layout is a singleton. `workbench_layout_settings` carries no host,
	/// account, repository, or pull request, so drawer geometry is one reviewer
	/// preference rather than a per-review fact, and the schema says so with a
	/// primary key that admits exactly one row.
	class workbench_settings_database final
	{
	public:
		workbench_settings_database()                                   = default;
		workbench_settings_database(const workbench_settings_database&) = delete;
		auto operator=(const workbench_settings_database&)
			-> workbench_settings_database& = delete;
		workbench_settings_database(workbench_settings_database&& aOther) noexcept;
		auto operator=(workbench_settings_database&& aOther) noexcept
			-> workbench_settings_database&;
		~workbench_settings_database();

		/// \brief Opens or creates the store at one path.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aPath, std::stop_token aStop)
			-> std::expected<workbench_settings_database, failure>;

		[[nodiscard]] auto handle() const noexcept -> ::sqlite3*;

		/// \brief Writes the layout, replacing whatever it already holds.
		///
		/// \details One statement in one implicit transaction. A half-written
		/// layout — a stored width beside a stale drawer state — would restore
		/// a geometry the reviewer never chose.
		[[nodiscard]] auto store(
			const application::workbench_layout_settings& aSettings,
			std::stop_token                               aStop)
			-> std::expected<void, failure>;

		/// \brief Reads the layout, answering the defaults when none is stored.
		///
		/// \details Nothing stored is not a failure and is not distinguishable
		/// in the answer, because `workbench_settings_port` returns the record
		/// itself rather than an optional. That differs from the appearance
		/// port on purpose: RFC-BPR-0006 must tell a fresh installation from a
		/// deliberately stored default, and RFC-BPR-0005 has no such claim.
		[[nodiscard]] auto load(std::stop_token aStop)
			-> std::expected<application::workbench_layout_settings, failure>;

	private:
		explicit workbench_settings_database(::sqlite3* aHandle) noexcept;

		::sqlite3* Handle = nullptr;
	};

	/// \brief Returns every column the created schema declares.
	///
	/// \details Reported as `table.column`, so the storage prohibitions are
	/// checked against the schema the build actually creates rather than
	/// against the insert statements somebody remembered to read.
	[[nodiscard]] auto workbench_settings_columns(::sqlite3* aDatabase)
		-> std::expected<std::vector<std::string>, failure>;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_WORKBENCH_SETTINGS_DATABASE_HPP
