#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_APPEARANCE_SETTINGS_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_APPEARANCE_SETTINGS_DATABASE_HPP

#include "sqlite_connection.hpp"
#include "state_directory.hpp"

#include "bpr/application/appearance_runtime_ports.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	/// \brief The schema version this build writes and validates.
	///
	/// \details Version 2 added `palette_variant`. A palette customises exactly
	/// one variant, and a version-1 record did not say which, so the reader was
	/// left to derive it from the current desktop and pinned a light palette
	/// over the dark profile. Version 3 preserves `palette_stored`, including
	/// an explicit reset to the built-in palettes. Version 4 has the same
	/// columns; opening an older file clears `palette_stored` once, because
	/// the built-in dark palette changed with [[BPR-0720]] and the owner
	/// asked for the saved customisation of the old defaults to be reset.
	/// Version 6 adds shown/System Dif preferences without changing legacy fields.
	/// Version 7 replaces the single `palette_document`, `palette_variant` and
	/// `palette_stored` triple with one document and one stored flag per variant
	/// (RFC-BPR-0034: each theme keeps its own colours). `palette_variant` had no
	/// meaning left once every variant has a slot of its own — it existed only to
	/// say which single theme the one palette belonged to — so it is migration
	/// input rather than a column version 7 keeps.
	/// Version 8 adds `code_white_space`, when the code panes draw white space
	/// ([[BPR-1107]]); a version-7 record takes the fresh default, "selection".
	/// Version 9 adds `code_zoom_percent`, the comparison's zoom ([[BPR-1336]]);
	/// a version-8 record takes Actual Size, 100.
	/// Version 10 adds `known_namespaces`, the C++ namespaces the reviewer names
	/// as known, written as the settings field shows them ([[BPR-1714]]); a
	/// version-9 record takes the default, `std`.
	inline constexpr std::int64_t appearance_settings_schema_version = 10;

	/// \brief Owns the durable appearance settings database.
	///
	/// \details A separate file from the epoch catalog, the review ledger, the
	/// queue enrollment store, and the workbench layout, for the reason those
	/// are separate from each other: deleting one kind of local state must not
	/// destroy another. Losing a theme must not lose reading progress.
	///
	/// The palette is stored as a **theme document**, the same encoding
	/// `core::export_theme` produces and `core::import_theme` reads, rather
	/// than as forty-five rows of colour components. That is deliberate. The
	/// importer already refuses unknown keys, duplicate keys, external
	/// references, include directives, style payloads, executable payloads,
	/// alphabet violations, oversized documents, and any palette that fails to
	/// assign every role — and it reports which. Inventing a second encoding
	/// here would mean inventing a second set of those refusals, and the second
	/// set is the one nobody tests against a hostile file.
	///
	/// It also keeps the durable record keyed by each role's **stable
	/// identifier** rather than its enum ordinal, so inserting a role into the
	/// vocabulary cannot silently re-colour every role after it.
	class appearance_settings_database final
	{
	public:
		appearance_settings_database()                                    = default;
		appearance_settings_database(const appearance_settings_database&) = delete;
		auto operator=(const appearance_settings_database&)
			-> appearance_settings_database& = delete;
		appearance_settings_database(appearance_settings_database&& aOther) noexcept;
		auto operator=(appearance_settings_database&& aOther) noexcept
			-> appearance_settings_database&;
		~appearance_settings_database();

		/// \brief Opens a database file by path, for the schema contract.
		/// \details Applies no private-store policy: the file is neither
		/// admitted nor restricted. The durable store opens through the
		/// state-directory overload.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aPath, std::stop_token aStop)
			-> std::expected<appearance_settings_database, failure>;

		/// \brief Opens the durable store file `aName` under the held `state/v1`.
		/// \details Through `open_private_store_database` ([[BPR-0883]]): the
		/// file is admitted, opened by its UTF-8 name, restricted to its owner
		/// and, on Windows, bound to the handle SQLite holds. The caller keeps
		/// `aDirectory` for as long as the database is open.
		[[nodiscard]] static auto open(
			state_directory& aDirectory, std::string_view aName, std::stop_token aStop)
			-> std::expected<appearance_settings_database, failure>;

		[[nodiscard]] auto handle() const noexcept -> ::sqlite3*;

		[[nodiscard]] auto store(
			const application::appearance_settings& aSettings,
			std::stop_token                         aStop)
			-> std::expected<void, failure>;

		/// \brief Reads the durable record, or nothing when none is stored.
		///
		/// \details The disengaged optional is load-bearing and is the reason
		/// this differs from the workbench layout store. RFC-BPR-0006 requires
		/// that a fresh installation resolve System, and it must be possible to
		/// tell a fresh installation from one that deliberately stored System.
		/// Answering the defaults for an absent record would destroy that
		/// distinction permanently and silently, because every caller would
		/// still see valid settings.
		[[nodiscard]] auto load(std::stop_token aStop) -> std::expected<
			std::optional<application::appearance_settings>,
			failure>;

	private:
		explicit appearance_settings_database(::sqlite3* aHandle) noexcept;

		/// \brief Configures, establishes and validates a connection this owns.
		[[nodiscard]] static auto adopt(appearance_settings_database aOwned, std::stop_token aStop)
			-> std::expected<appearance_settings_database, failure>;

		::sqlite3* Handle = nullptr;
	};

	/// \brief Returns every column the created schema declares.
	[[nodiscard]] auto appearance_settings_columns(::sqlite3* aDatabase)
		-> std::expected<std::vector<std::string>, failure>;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_APPEARANCE_SETTINGS_DATABASE_HPP
