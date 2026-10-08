#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_WORKBENCH_SETTINGS_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_WORKBENCH_SETTINGS_DATABASE_HPP

#include "sqlite_connection.hpp"
#include "state_directory.hpp"

#include "bpr/application/workbench_runtime_ports.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>
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
	///
	/// Four since [[BPR-0661]]: the review-behaviour preferences joined the
	/// singleton row, defaulted so a rebuilt older row keeps the behaviour it
	/// had.
	///
	/// Six since [[BPR-0721]]: whether the drawers carry their own controls;
	/// defaulted hidden, the owner's direction, so a migrated row hides them
	/// too.
	///
	/// Seven since [[BPR-0729]]: one pane in both profile defaults and window
	/// layouts, defaulted disabled to preserve the prior two-pane view.
	///
	/// Eight since [[BPR-0783]]: one window's Home view — its grouping, its
	/// state headers, the panels it left collapsed and its own filter — in the
	/// per-window table only. Home belongs to the window that shows it, so the
	/// profile row does not carry it, and every default is the Home a
	/// version-seven window already showed.
	///
	/// Nine since [[BPR-0977]]: each window's own native placement.
	///
	/// Ten since [[BPR-1130]]: each window's Updates line, folded or open, in
	/// the per-window table only. Every new window starts folded, so the
	/// profile row answers no fold, and a migrated row reads folded.
	///
	/// Eleven added the writing hooks' overrides to the profile row.
	///
	/// Twelve since [[BPR-1083]]: how label marks are drawn, icons or the
	/// two-letter shortcode marks, in the profile row. A migrated row reads
	/// icons, the default.
	///
	/// Thirteen since [[BPR-1088]]: the comment editor's label helper, the
	/// Overview's starting count and its ordered label columns, in the
	/// profile row. A migrated row reads the helper off, Open and no
	/// columns, and keeps every other field it had. Stored columns that name
	/// an unknown or repeated label are read without them and reported; the
	/// rest of the row still loads.
	///
	/// Fourteen since [[BPR-1301]]: the profile's Claude process wrapper, in
	/// the profile row. A migrated row reads none and keeps every other field
	/// it had. An agent-free build reads and writes it back unchanged.
	inline constexpr std::int64_t workbench_settings_schema_version = 15;

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

		/// \brief Opens or creates a database file by path, for the schema contract.
		/// \details Applies no private-store policy: the file is neither
		/// admitted nor restricted. The durable store opens through the
		/// state-directory overload.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aPath, std::stop_token aStop)
			-> std::expected<workbench_settings_database, failure>;

		/// \brief Opens or creates the durable store file `aName` under the held `state/v1`.
		/// \details Through `open_private_store_database` ([[BPR-0883]]): the
		/// file is admitted, opened by its UTF-8 name, restricted to its owner
		/// and, on Windows, bound to the handle SQLite holds. The caller keeps
		/// `aDirectory` for as long as the database is open.
		[[nodiscard]] static auto open(
			state_directory& aDirectory, std::string_view aName, std::stop_token aStop)
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

		[[nodiscard]] auto load_window(core::bpr_window_id aWindow, std::stop_token aStop)
			-> std::expected<application::workbench_layout_settings, failure>;
		[[nodiscard]] auto store_window(core::bpr_window_id aWindow, const application::workbench_window_layout& aLayout, std::stop_token aStop)
			-> std::expected<void, failure>;
		[[nodiscard]] auto retain_windows(std::span<const core::bpr_window_id> aWindows, std::stop_token aStop)
			-> std::expected<void, failure>;

		[[nodiscard]] auto erase_window(core::bpr_window_id aWindow, std::stop_token aStop)
			-> std::expected<void, failure>;

	private:
		explicit workbench_settings_database(::sqlite3* aHandle) noexcept;

		/// \brief Preflights, configures, establishes and validates a connection this owns.
		[[nodiscard]] static auto adopt(workbench_settings_database aOwned, std::stop_token aStop)
			-> std::expected<workbench_settings_database, failure>;

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
