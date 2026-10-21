#include "../../src/adapters/sqlite/src/appearance_settings_database.hpp"

#include "../support/test_expectation.hpp"
#include "bpr/core/theme_exchange.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::sqlite::detail;

	using bpr::tests::expect;

	/// \brief This contract's scratch root: `bpr-appearance-` and a random part.
	/// \details [[BPR-1126]]. Made at a name nothing had, owner-private from
	/// creation, and removed only by the course that made it.
	class scratch_directory final : public bpr::tests::sqlite_contract::scratch_root
	{
	public:
		scratch_directory()
			: scratch_root{ "appearance" }
		{
		}
	};

	[[nodiscard]] constexpr auto variant_at(const std::size_t aIndex) noexcept
		-> core::appearance_variant
	{
		return static_cast<core::appearance_variant>(
			static_cast<std::uint8_t>(aIndex));
	}

	/// \brief The column names version 7 keeps one palette per variant in.
	///
	/// \details Restated here rather than borrowed from the store. These names are
	/// the durable contract, so a test that asked the store what it calls its own
	/// columns would agree with any rename, including one that silently orphaned
	/// every existing file.
	[[nodiscard]] auto document_column(const core::appearance_variant aVariant)
		-> std::string
	{
		return std::string{ core::stable_name(aVariant) } + "_palette_document";
	}

	[[nodiscard]] auto stored_column(const core::appearance_variant aVariant)
		-> std::string
	{
		return std::string{ core::stable_name(aVariant) } + "_palette_stored";
	}

	/// \brief The least record the core will accept.
	///
	/// \details Not `appearance_settings{ }`. A default-constructed
	/// `font_settings` has an empty family, which the core rejects on purpose -
	/// it cannot know a family and says so by being illegal until a host
	/// supplies one. A test that stored the default-constructed record would be
	/// storing something no composition ever produces.
	///
	/// It carries no colour customization at all, for any variant, which is what a
	/// fresh installation looks like.
	[[nodiscard]] auto legal_baseline() -> application::appearance_settings
	{
		auto settings                        = application::appearance_settings{ };
		settings.Typography.Ui.Font.Family   = "Baseline Interface";
		settings.Typography.Code.Font.Family = "Baseline Code";
		return settings;
	}

	/// \brief A total, opaque palette nobody could mistake for another seed's.
	[[nodiscard]] auto seeded_palette(const std::uint8_t aSeed)
		-> core::appearance_palette
	{
		auto palette = core::appearance_palette{ };
		for (std::size_t index = 0U; index < core::appearance_role_count; ++index) {
			palette[index] = core::appearance_color{
				.Red   = static_cast<std::uint8_t>(index * 3U + aSeed),
				.Green = static_cast<std::uint8_t>(index * 5U + aSeed),
				.Blue  = static_cast<std::uint8_t>(index * 7U + aSeed),
				.Alpha = 255U,
			};
		}
		return palette;
	}

	auto with_customisation(
		application::appearance_settings& aSettings,
		const core::appearance_variant    aVariant,
		const core::appearance_palette&   aPalette) -> void
	{
		application::customisation_of(aSettings, aVariant)
			= application::appearance_palette_customisation{ .Palette = aPalette,
															 .Stored  = true };
	}

	/// \brief A legal record that differs from the baseline in every domain.
	///
	/// \details The customised variant is deliberately independent of the mode:
	/// the mode is high contrast dark whichever theme carries the colours, so a
	/// reader that derived the variant from the mode instead of reading the
	/// recorded one would file three of the four cases under the wrong theme.
	[[nodiscard]] auto distinct_settings(
		const core::appearance_variant aVariant
		= core::appearance_variant::high_contrast_dark)
		-> application::appearance_settings
	{
		auto settings = application::appearance_settings{ };
		settings.Mode = core::appearance_mode::high_contrast_dark;
		with_customisation(settings, aVariant, seeded_palette(0U));
		settings.Typography.Ui = core::ui_font_settings{
			.Font = core::font_settings{
				.Family               = "Stored Interface",
				.PointSizeCentipoints = 1250U,
				.Weight               = 500U,
			},
			.LineHeightPercent = 145U,
		};
		settings.Typography.Code = core::code_font_settings{
			.Font = core::font_settings{
				.Family               = "Stored Code",
				.PointSizeCentipoints = 1150U,
				.Weight               = 400U,
			},
			.TabWidth     = 7U,
			.Ligatures    = true,
			.SyntaxBold   = false,
			.SyntaxItalic = true,
		};
		return settings;
	}

	[[nodiscard]] auto legacy_snapshot(::sqlite3* aDatabase) -> std::vector<std::string>
	{
		auto       cells   = std::vector<std::string>{ };
		const auto collect = [](void* aContext, int aCount, char** aValues, char**) -> int {
			auto& values = *static_cast<std::vector<std::string>*>(aContext);
			for (int index = 0; index < aCount; ++index) {
				values.emplace_back(aValues[index] == nullptr ? "<NULL>" : aValues[index]);
			}
			return 0;
		};
		expect(::sqlite3_exec(aDatabase,
							  "PRAGMA application_id; PRAGMA user_version;"
							  "SELECT type,name,tbl_name,sql FROM sqlite_schema ORDER BY name;"
							  "PRAGMA table_xinfo(appearance_settings);"
							  "SELECT * FROM appearance_settings ORDER BY singleton;",
							  collect, &cells, nullptr)
				   == SQLITE_OK,
			   "legacy schema, columns and rows can be captured");
		return cells;
	}

	/// \brief Rewrites a current-format file into the exact shape one earlier
	///        version had, so a migration fixture is a real legacy file.
	///
	/// \details The store only ever writes the current schema, so a legacy fixture
	/// has to be built by editing one back down: the single `palette_document`,
	/// `palette_variant` and `palette_stored` columns are added and the per-variant
	/// ones dropped, through a raw connection. That is the idiom the older fixtures
	/// here already use — `ALTER TABLE` on a STRICT table is refused by the
	/// hardened production connection but allowed on a plain one, and dropping a
	/// column takes its own CHECK constraint with it.
	///
	/// `aVariant` is the theme the legacy row claims its one palette belongs to,
	/// and `aStored` whether that row calls it a deliberate customization.
	auto down_edit_to_version(
		const std::filesystem::path&   aPath,
		const std::int64_t             aVersion,
		const core::appearance_variant aVariant,
		const bool                     aStored) -> void
	{
		::sqlite3* raw = nullptr;
		expect(::sqlite3_open(aPath.string().c_str(), &raw) == SQLITE_OK,
			   "a legacy fixture opens raw");
		// [[BPR-1714]]. Every version before 10 lacks the known namespaces.
		expect(::sqlite3_exec(raw, "ALTER TABLE appearance_settings DROP COLUMN known_namespaces", nullptr, nullptr, nullptr) == SQLITE_OK,
			   "a legacy down-edit drops the version-10 column");
		if (aVersion == 9) {
			for (const auto& statement : { std::string{ "UPDATE appearance_meta SET schema_format = 9" }, std::string{ "PRAGMA user_version = 9" } }) {
				expect(::sqlite3_exec(raw, statement.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "a version-9 down-edit statement runs: " + statement);
			}
			expect(::sqlite3_close(raw) == SQLITE_OK, "the legacy fixture closes");
			return;
		}
		// [[BPR-1336]]. Every version before 9 lacks the zoom column.
		expect(::sqlite3_exec(raw, "ALTER TABLE appearance_settings DROP COLUMN code_zoom_percent", nullptr, nullptr, nullptr) == SQLITE_OK,
			   "a legacy down-edit drops the version-9 column");
		if (aVersion == 8) {
			for (const auto& statement : { std::string{ "UPDATE appearance_meta SET schema_format = 8" }, std::string{ "PRAGMA user_version = 8" } }) {
				expect(::sqlite3_exec(raw, statement.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "a version-8 down-edit statement runs: " + statement);
			}
			expect(::sqlite3_close(raw) == SQLITE_OK, "the legacy fixture closes");
			return;
		}
		// [[BPR-1107]]. Every version before 8 lacks the white-space column.
		expect(::sqlite3_exec(raw, "ALTER TABLE appearance_settings DROP COLUMN code_white_space", nullptr, nullptr, nullptr) == SQLITE_OK,
			   "a legacy down-edit drops the version-8 column");
		if (aVersion == 7) {
			for (const auto& statement : { std::string{ "UPDATE appearance_meta SET schema_format = 7" }, std::string{ "PRAGMA user_version = 7" } }) {
				expect(::sqlite3_exec(raw, statement.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "a version-7 down-edit statement runs: " + statement);
			}
			expect(::sqlite3_close(raw) == SQLITE_OK, "the legacy fixture closes");
			return;
		}
		auto statements = std::vector<std::string>{
			"ALTER TABLE appearance_settings ADD COLUMN palette_document TEXT NOT NULL DEFAULT ''",
			"UPDATE appearance_settings SET palette_document = " + document_column(aVariant),
		};
		if (aVersion >= 2) {
			statements.emplace_back(
				"ALTER TABLE appearance_settings ADD COLUMN palette_variant TEXT NOT NULL DEFAULT 'light'");
			statements.emplace_back(
				"UPDATE appearance_settings SET palette_variant = '"
				+ std::string{ core::stable_name(aVariant) } + "'");
		}
		if (aVersion >= 3) {
			statements.emplace_back(
				"ALTER TABLE appearance_settings ADD COLUMN palette_stored INTEGER NOT NULL DEFAULT 0 CHECK(palette_stored IN (0, 1))");
			statements.emplace_back(
				std::string{ "UPDATE appearance_settings SET palette_stored = " }
				+ (aStored ? "1" : "0"));
		}
		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			statements.emplace_back(
				"ALTER TABLE appearance_settings DROP COLUMN "
				+ document_column(variant_at(index)));
			statements.emplace_back(
				"ALTER TABLE appearance_settings DROP COLUMN "
				+ stored_column(variant_at(index)));
		}
		if (aVersion < 6) {
			statements.emplace_back("ALTER TABLE appearance_settings DROP COLUMN dif_visibility");
			statements.emplace_back("ALTER TABLE appearance_settings DROP COLUMN motion_mode");
		}
		statements.emplace_back(
			"UPDATE appearance_meta SET schema_format = " + std::to_string(aVersion));
		statements.emplace_back("PRAGMA user_version = " + std::to_string(aVersion));
		for (const auto& statement : statements) {
			expect(::sqlite3_exec(raw, statement.c_str(), nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "a legacy down-edit statement runs: " + statement);
		}
		expect(::sqlite3_close(raw) == SQLITE_OK, "the legacy fixture closes");
	}

	auto test_absent_is_distinguishable_from_stored_defaults() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		auto fresh = appearance_settings_database::open(path, { });
		expect(fresh.has_value(), "a fresh store opens");

		const auto empty = fresh->load({ });
		expect(empty.has_value(), "an empty store is not a failure");
		expect(
			!empty->has_value(),
			"a fresh installation reports no stored record at all");

		// The other half, and the reason this port differs from the workbench
		// layout port. RFC-BPR-0006 requires a fresh installation to resolve
		// System, so a deliberately stored record must remain distinguishable
		// from never having stored one. Answering the defaults for an absent
		// record destroys that permanently and silently, because every caller
		// still sees valid settings.
		auto defaults = legal_baseline();
		expect(fresh->store(defaults, { }).has_value(),
			   "a baseline record can be stored deliberately");
		const auto stored = fresh->load({ });
		expect(
			stored.has_value() && stored->has_value(),
			"a deliberately stored default is reported as present");
		expect(**stored == defaults,
			   "and reads back as exactly what was stored");

		// And the same distinction per variant: a record that stores no
		// customization for a theme is not the same fact as one that stores that
		// theme's built-in colours, and the stored flag is what keeps them apart.
		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			expect(application::customisation_of(**stored, variant_at(index))
					   == application::appearance_palette_customisation{ },
				   "a deliberately stored baseline customises no theme");
		}
	}

	auto test_every_domain_round_trips() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");
		const auto chosen  = distinct_settings();

		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(opened->store(chosen, { }).has_value(), "the record stores");
		}

		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "the store reopens");
		const auto loaded = reopened->load({ });
		expect(loaded.has_value() && loaded->has_value(), "the record loads");
		// Compared whole: mode, every variant's palette and stored flag, and every
		// typography field at once. Checking field by field would pass a store
		// that dropped one added later.
		expect(**loaded == chosen,
			   "mode, every theme's palette, and typography all survive a close "
			   "and reopen, compared whole");
	}

	/// \brief RFC-BPR-0034: every theme keeps its own colours, independently.
	auto test_every_theme_round_trips_independently() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("themes.sqlite3");

		auto chosen = legal_baseline();
		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			with_customisation(
				chosen,
				variant_at(index),
				seeded_palette(static_cast<std::uint8_t>(11U * (index + 1U))));
		}

		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(opened->store(chosen, { }).has_value(),
				   "four customised themes store together");
		}

		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "the store reopens");
		const auto loaded = reopened->load({ });
		expect(loaded && loaded->has_value() && **loaded == chosen,
			   "all four themes return exactly as they were stored");
		for (std::size_t first = 0U; first < core::appearance_variant_count; ++first) {
			for (auto second = first + 1U; second < core::appearance_variant_count;
				 ++second) {
				expect(application::customisation_of(**loaded, variant_at(first)).Palette
						   != application::customisation_of(**loaded, variant_at(second))
								  .Palette,
					   "and no theme's colours were written over another's");
			}
		}

		// One theme's reset leaves the others alone all the way through the store.
		auto reset = chosen;
		application::customisation_of(reset, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ };
		expect(reopened->store(reset, { }).has_value(), "one theme's reset stores");
		auto again = appearance_settings_database::open(path, { });
		expect(again && again->load({ }).value().value() == reset,
			   "and a reopen finds exactly that theme cleared and the rest intact");
	}

	auto test_each_theme_is_stored_as_its_own_theme_document() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");
		auto       chosen  = legal_baseline();
		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			with_customisation(
				chosen,
				variant_at(index),
				seeded_palette(static_cast<std::uint8_t>(7U * (index + 1U))));
		}

		auto opened = appearance_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");
		expect(opened->store(chosen, { }).has_value(), "the record stores");

		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			const auto variant  = variant_at(index);
			const auto document = scalar_text(
				opened->handle(),
				"SELECT " + document_column(variant)
					+ " FROM appearance_settings WHERE singleton = 1");
			expect(document.has_value(), "each theme's palette is stored as text");
			// The stored bytes are the core's own exported document, not a second
			// encoding. That is what makes the read path inherit every refusal
			// `import_theme` already makes against a tampered file.
			expect(*document
					   == core::export_theme(
						   application::customisation_of(chosen, variant).Palette),
				   "each stored palette is exactly the core's theme document");
			// Keyed by stable role identifiers rather than enum ordinals, so
			// inserting a role into the vocabulary cannot re-colour every role
			// after it.
			expect(
				document->find(std::string{ core::stable_name(
					core::appearance_role::code_background) })
					!= std::string::npos,
				"roles are named by their stable identifier in the stored document");
		}
	}

	auto test_a_tampered_palette_is_refused() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(opened->store(distinct_settings(), { }).has_value(),
				   "a legal record stores");
		}

		// A durable file can be replaced by another build or edited by hand. The
		// tampered column belongs to a theme that carries no customization, which
		// is the case a reader that skipped unstored slots would admit.
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr)
					== SQLITE_OK,
				"the database reopens directly");
			expect(
				::sqlite3_exec(
					raw,
					("UPDATE appearance_settings SET "
					 + document_column(core::appearance_variant::dark)
					 + " = 'not a theme document' WHERE singleton = 1")
						.c_str(),
					nullptr,
					nullptr,
					nullptr)
					== SQLITE_OK,
				"a bogus palette document is written");
			(void)::sqlite3_close_v2(raw);
		}

		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "the tampered database still opens");
		const auto loaded = reopened->load({ });
		expect(!loaded.has_value(),
			   "a palette the core's importer refuses is not adopted");
		expect(loaded.error() == catalog_database_failure::integrity_failure,
			   "and is refused as an integrity failure");
	}

	/// \brief [[BPR-1083]]. A palette stored before the twelve label tints
	/// existed still loads; the tints are unassigned for presentation to read as
	/// its built-in values, every other role is as stored, and the file needs no
	/// migration.
	auto test_a_palette_stored_without_label_tints_loads() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");
		const auto stored  = distinct_settings(core::appearance_variant::dark);
		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value() && opened->store(stored, { }).has_value(), "a legal record stores");
		}
		{
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open_v2(path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
				   "the database reopens directly");
			auto       document = core::export_theme(application::customisation_of(stored, core::appearance_variant::dark).Palette);
			const auto cut      = document.find("feedback_label_praise ");
			expect(cut != std::string::npos, "the stored document carries the label tints");
			document.erase(cut);
			auto* statement = static_cast<::sqlite3_stmt*>(nullptr);
			expect(::sqlite3_prepare_v2(raw, ("UPDATE appearance_settings SET " + document_column(core::appearance_variant::dark) + " = ?1 WHERE singleton = 1").c_str(), -1, &statement, nullptr) == SQLITE_OK
					   && ::sqlite3_bind_text(statement, 1, document.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK
					   && ::sqlite3_step(statement) == SQLITE_DONE,
				   "the dark palette is written back as a 47-role document");
			(void)::sqlite3_finalize(statement);
			(void)::sqlite3_close_v2(raw);
		}
		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "the store reopens without a migration");
		const auto loaded = reopened->load({ });
		expect(loaded.has_value() && loaded->has_value(), "the record loads");
		const auto& palette  = application::customisation_of(**loaded, core::appearance_variant::dark);
		auto        expected = application::customisation_of(stored, core::appearance_variant::dark).Palette;
		for (auto index = core::index_of(core::first_feedback_label_role); index < core::appearance_role_count; ++index) {
			expected[index] = core::unassigned_appearance_color;
		}
		expect(palette.Stored && palette.Palette == expected,
			   "the label tints are unassigned and every other role is exactly as stored");
	}

	auto test_a_stored_flag_outside_its_range_is_refused() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(opened->store(distinct_settings(), { }).has_value(),
				   "a legal record stores");
		}
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr)
					== SQLITE_OK,
				"the database reopens directly");
			// Past the CHECK constraint on purpose: the reader is the authority
			// that refuses such a value, and it must still do so when the
			// constraint was bypassed rather than trusting it.
			expect(
				::sqlite3_exec(
					raw,
					("PRAGMA ignore_check_constraints = ON;"
					 "UPDATE appearance_settings SET "
					 + stored_column(core::appearance_variant::dark)
					 + " = 2 WHERE singleton = 1")
						.c_str(),
					nullptr,
					nullptr,
					nullptr)
					== SQLITE_OK,
				"a stored flag outside its range is written");
			(void)::sqlite3_close_v2(raw);
		}

		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "the tampered database still opens");
		const auto loaded = reopened->load({ });
		expect(!loaded.has_value()
				   && loaded.error() == catalog_database_failure::integrity_failure,
			   "a stored flag that is neither 0 nor 1 is refused");
	}

	auto test_an_unknown_mode_name_is_refused() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(opened->store(distinct_settings(), { }).has_value(),
				   "a legal record stores");
		}
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr)
					== SQLITE_OK,
				"the database reopens directly");
			expect(
				::sqlite3_exec(
					raw,
					"UPDATE appearance_settings SET mode = 'chartreuse' "
					"WHERE singleton = 1",
					nullptr,
					nullptr,
					nullptr)
					== SQLITE_OK,
				"a mode outside the vocabulary is written");
			(void)::sqlite3_close_v2(raw);
		}

		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "the tampered database still opens");
		const auto loaded = reopened->load({ });
		// Refused rather than mapped to whatever sits at some ordinal. A stored
		// name the core does not publish names no mode.
		expect(!loaded.has_value() && loaded.error() == catalog_database_failure::integrity_failure,
			   "a mode name the core does not publish is refused");
	}

	auto test_typography_the_core_rejects_is_refused_whole() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens");
			expect(opened->store(distinct_settings(), { }).has_value(),
				   "a legal record stores");
		}
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr)
					== SQLITE_OK,
				"the database reopens directly");
			expect(
				::sqlite3_exec(
					raw,
					"UPDATE appearance_settings SET ui_point_size = 1 "
					"WHERE singleton = 1",
					nullptr,
					nullptr,
					nullptr)
					== SQLITE_OK,
				"a point size below the core's bound is written");
			(void)::sqlite3_close_v2(raw);
		}

		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "the tampered database still opens");
		const auto loaded = reopened->load({ });
		// The core is the authority on typography legality. A record it
		// rejects is refused here rather than adopted and quietly repaired
		// further in, where the repair would be invisible.
		expect(!loaded.has_value() && loaded.error() == catalog_database_failure::integrity_failure,
			   "a record the core's validator rejects is refused whole");
	}

	auto test_a_newer_schema_is_refused_for_the_right_reason() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		{
			auto opened = appearance_settings_database::open(path, { });
			expect(opened.has_value(), "the store opens once");
		}
		{
			::sqlite3* raw = nullptr;
			expect(
				::sqlite3_open_v2(
					path.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr)
					== SQLITE_OK,
				"the database reopens directly");
			const auto sql = "PRAGMA user_version = "
				+ std::to_string(appearance_settings_schema_version + 1);
			expect(
				::sqlite3_exec(raw, sql.c_str(), nullptr, nullptr, nullptr)
					== SQLITE_OK,
				"a newer schema version is stamped");
			(void)::sqlite3_close_v2(raw);
		}

		auto refused = appearance_settings_database::open(path, { });
		expect(!refused.has_value(), "the newer database is refused");
		// The reason is the assertion. Without the forward-only guard the open
		// still fails, because establish_schema would run its first CREATE
		// against a table that already exists.
		expect(refused.error() == catalog_database_failure::schema_mismatch,
			   "because this build will not read a schema it does not know, not "
			   "because it tripped over its own CREATE");
	}

	auto test_cancellation_leaves_the_record_untouched() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		auto opened = appearance_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");

		const auto established = legal_baseline();
		expect(opened->store(established, { }).has_value(),
			   "a record is established first");

		auto stop = std::stop_source{ };
		stop.request_stop();

		const auto stored = opened->store(distinct_settings(), stop.get_token());
		expect(!stored.has_value() && stored.error() == catalog_database_failure::cancelled,
			   "a cancelled store reports cancellation, not unavailability");

		// Reporting cancellation after writing satisfies the line above and is
		// still wrong: the caller is told their write did not happen while the
		// bytes on disk say it did.
		const auto survived = opened->load({ });
		expect(survived.has_value() && survived->has_value()
				   && **survived == established,
			   "and a cancelled store leaves the stored record untouched");
	}

	/// \brief [[BPR-0789]] / RFC-BPR-0034: migration files the palette under its
	///        recorded variant, and loses nothing doing it.
	///
	/// \details One case per theme, so the light direction and the dark direction
	/// are each proven rather than one standing in for the other. The mode stays
	/// high contrast dark in every case, so a migration that derived the variant
	/// from the mode — which is what the version-1 reader had to do — would file
	/// three of the four under the wrong theme and fail here.
	auto test_version_six_files_one_palette_under_its_recorded_variant() -> void
	{
		const auto scratch = scratch_directory{ };
		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			const auto variant = variant_at(index);
			const auto name    = std::string{ core::stable_name(variant) };
			const auto path    = scratch.file("v6-" + name + ".sqlite3");
			// Dif is deliberately not its default. A version-six file carries
			// these two columns, so a migration that rebuilt the row without
			// them - or re-ran the version-six step, which writes them as
			// shown/System - would silently reset a preference the record held.
			// A fixture that left them defaulted could not tell that apart from
			// a correct carry. The pre-version-six fixtures cannot assert this,
			// because those versions have no such columns to keep.
			auto original = distinct_settings(variant);
			original.Dif  = core::dif_preferences{
				.Visibility = core::dif_visibility::hidden,
				.Motion     = core::motion_mode::full,
			};
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value()
						   && database->store(original, { }).has_value(),
					   "a current-format record stores before the down-edit");
			}
			down_edit_to_version(path, 6, variant, true);

			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(),
				   "a version-six file opens and migrates forward: " + name);
			const auto version
				= scalar_integer(migrated->handle(), "PRAGMA user_version");
			expect(version.has_value()
					   && *version == appearance_settings_schema_version,
				   "the migrated file is stamped as the current format");
			const auto format = scalar_integer(
				migrated->handle(),
				"SELECT schema_format FROM appearance_meta WHERE singleton = 1");
			expect(format.has_value() && *format == appearance_settings_schema_version,
				   "and its metadata row agrees with the stamp");
			expect(
				!scalar_text(
					 migrated->handle(),
					 "SELECT palette_variant FROM appearance_settings WHERE singleton = 1")
					 .has_value(),
				"the column that named the one customised theme is gone, because "
				"every theme now has a slot of its own");

			const auto loaded = migrated->load({ });
			expect(loaded.has_value() && loaded->has_value(),
				   "the migrated record loads");
			// The whole claim in one comparison: the stored palette is filed under
			// exactly the theme the version-six row named, every other theme
			// carries no customization, and no other domain moved.
			expect(**loaded == original,
				   "nothing is lost and nothing is invented: " + name);
			expect(application::customisation_of(**loaded, variant).Stored
					   && application::customisation_of(**loaded, variant).Palette
						   == seeded_palette(0U),
				   "the recorded theme keeps exactly its stored palette");
			for (std::size_t other = 0U; other < core::appearance_variant_count;
				 ++other) {
				if (other == index) {
					continue;
				}
				expect(application::customisation_of(**loaded, variant_at(other))
						   == application::appearance_palette_customisation{ },
					   "and every other theme carries no customization at all");
			}

			// A separate open of the migrated file, which is the restart claim.
			migrated      = appearance_settings_database{ };
			auto reopened = appearance_settings_database::open(path, { });
			expect(reopened.has_value() && reopened->load({ }) == loaded,
				   "the migrated record survives a close and reopen unchanged");
		}
	}

	/// \brief A version-six record that stored no palette customises nothing.
	auto test_version_six_without_a_stored_palette_carries_no_customisation() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("v6-unstored.sqlite3");
		// Not the default, for the reason given in the filing test above.
		auto original = distinct_settings(core::appearance_variant::dark);
		original.Dif  = core::dif_preferences{
			.Visibility = core::dif_visibility::hidden,
			.Motion     = core::motion_mode::full,
		};
		{
			auto database = appearance_settings_database::open(path, { });
			expect(database.has_value() && database->store(original, { }).has_value(),
				   "a current-format record stores before the down-edit");
		}
		down_edit_to_version(path, 6, core::appearance_variant::dark, false);

		auto migrated = appearance_settings_database::open(path, { });
		expect(migrated.has_value(), "an unstored version-six file migrates");
		const auto loaded = migrated->load({ });
		expect(loaded.has_value() && loaded->has_value(), "the migrated record loads");

		auto expected = original;
		application::customisation_of(expected, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ };
		expect(**loaded == expected,
			   "a record whose palette was not stored carries no customization, "
			   "and every other domain survives");
		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			expect(!application::customisation_of(**loaded, variant_at(index)).Stored,
				   "no theme is invented out of a palette nobody chose");
		}
	}

	/// \brief A malformed version-six value refuses the migration, fail-closed.
	auto test_version_six_refuses_a_malformed_legacy_value() -> void
	{
		const auto     scratch   = scratch_directory{ };
		constexpr auto mutations = std::array{
			"UPDATE appearance_settings SET palette_variant = 'taupe'",
			"UPDATE appearance_settings SET palette_document = 'invalid'",
			"UPDATE appearance_settings SET mode = 'chartreuse'",
			"UPDATE appearance_settings SET ui_point_size = 1",
			// This mutation needs both statements in one sqlite3_exec call.
			("PRAGMA ignore_check_constraints = ON;"
			 "UPDATE appearance_settings SET palette_stored = 2"),
		};
		for (std::size_t index = 0U; index < mutations.size(); ++index) {
			const auto path = scratch.file("v6-bad-" + std::to_string(index) + ".sqlite3");
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value()
						   && database
								  ->store(
									  distinct_settings(core::appearance_variant::light),
									  { })
								  .has_value(),
					   "a current-format record stores before the down-edit");
			}
			down_edit_to_version(path, 6, core::appearance_variant::light, true);

			::sqlite3* raw = nullptr;
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK,
				   "the version-six fixture opens raw");
			expect(::sqlite3_exec(raw, mutations[index], nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "an invalid legacy value is written");
			const auto before = legacy_snapshot(raw);
			expect(::sqlite3_close(raw) == SQLITE_OK, "the raw writer closes");

			const auto refused = appearance_settings_database::open(path, { });
			expect(!refused.has_value(),
				   "an invalid version-six value refuses the migration");
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK,
				   "the refused fixture reopens raw");
			expect(legacy_snapshot(raw) == before,
				   "and the refusal leaves every legacy column, stamp and value "
				   "exactly as it was rather than half-migrating the file");
			expect(::sqlite3_close(raw) == SQLITE_OK, "the raw reader closes");
		}
	}

	/// \brief Writes one database in the exact shape schema version 1 had.
	///
	/// \details The version-1 statements are restated here rather than reused
	/// from the store, so this fixture stays a version-1 file however the
	/// current statements move on. A fixture built from the current schema
	/// would stop exercising the migration the moment the schema changed
	/// again, and would keep passing while it did.
	auto write_version_one_record(
		const std::filesystem::path&            aPath,
		const application::appearance_settings& aSettings,
		const core::appearance_palette&         aPalette,
		const std::int64_t                      aIdentity) -> void
	{
		::sqlite3* raw = nullptr;
		expect(
			::sqlite3_open_v2(
				aPath.string().c_str(),
				&raw,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
				nullptr)
				== SQLITE_OK,
			"a version-1 fixture database is created");

		constexpr auto version1 = std::array{
			std::string_view{
				"CREATE TABLE appearance_meta("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"schema_format INTEGER NOT NULL"
				") STRICT" },
			std::string_view{
				"CREATE TABLE appearance_settings("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"mode TEXT NOT NULL,"
				"palette_document TEXT NOT NULL,"
				"ui_family TEXT NOT NULL,"
				"ui_point_size INTEGER NOT NULL,"
				"ui_weight INTEGER NOT NULL,"
				"ui_line_height INTEGER NOT NULL,"
				"code_family TEXT NOT NULL,"
				"code_point_size INTEGER NOT NULL,"
				"code_weight INTEGER NOT NULL,"
				"code_tab_width INTEGER NOT NULL,"
				"code_ligatures INTEGER NOT NULL CHECK(code_ligatures IN (0, 1)),"
				"code_syntax_bold INTEGER NOT NULL CHECK(code_syntax_bold IN (0, 1)),"
				"code_syntax_italic INTEGER NOT NULL "
				"CHECK(code_syntax_italic IN (0, 1))"
				") STRICT" },
			std::string_view{
				"INSERT INTO appearance_meta(singleton, schema_format) "
				"VALUES(1, 1)" },
		};
		for (const auto sql : version1) {
			expect(
				::sqlite3_exec(
					raw, std::string{ sql }.c_str(), nullptr, nullptr, nullptr)
					== SQLITE_OK,
				"a version-1 statement runs");
		}

		::sqlite3_stmt* insert = nullptr;
		const auto      insertSql
			= std::string_view{ "INSERT INTO appearance_settings("
								"singleton, mode, palette_document, ui_family,"
								"ui_point_size, ui_weight, ui_line_height,"
								"code_family, code_point_size, code_weight,"
								"code_tab_width, code_ligatures,"
								"code_syntax_bold, code_syntax_italic) "
								"VALUES(1, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9,"
								"?10, ?11, ?12, ?13)" };
		expect(
			::sqlite3_prepare_v2(
				raw,
				insertSql.data(),
				static_cast<int>(insertSql.size()),
				&insert,
				nullptr)
				== SQLITE_OK,
			"the version-1 insert prepares");

		const auto  mode     = std::string{ core::stable_name(aSettings.Mode) };
		const auto  document = core::export_theme(aPalette);
		const auto& ui       = aSettings.Typography.Ui;
		const auto& code     = aSettings.Typography.Code;
		expect(
			::sqlite3_bind_text(insert, 1, mode.c_str(), -1, SQLITE_TRANSIENT)
					== SQLITE_OK
				&& ::sqlite3_bind_text(
					   insert, 2, document.c_str(), -1, SQLITE_TRANSIENT)
					== SQLITE_OK
				&& ::sqlite3_bind_text(
					   insert, 3, ui.Font.Family.c_str(), -1, SQLITE_TRANSIENT)
					== SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 4, ui.Font.PointSizeCentipoints)
					== SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 5, ui.Font.Weight) == SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 6, ui.LineHeightPercent)
					== SQLITE_OK
				&& ::sqlite3_bind_text(
					   insert, 7, code.Font.Family.c_str(), -1, SQLITE_TRANSIENT)
					== SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 8, code.Font.PointSizeCentipoints)
					== SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 9, code.Font.Weight) == SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 10, code.TabWidth) == SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 11, code.Ligatures ? 1 : 0)
					== SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 12, code.SyntaxBold ? 1 : 0)
					== SQLITE_OK
				&& ::sqlite3_bind_int64(insert, 13, code.SyntaxItalic ? 1 : 0)
					== SQLITE_OK,
			"every version-1 column binds");
		expect(::sqlite3_step(insert) == SQLITE_DONE,
			   "the version-1 record inserts");
		(void)::sqlite3_finalize(insert);

		const auto identity
			= "PRAGMA application_id = " + std::to_string(aIdentity);
		expect(
			::sqlite3_exec(raw, identity.c_str(), nullptr, nullptr, nullptr)
				== SQLITE_OK,
			"the version-1 fixture carries this store's application identity");
		expect(
			::sqlite3_exec(
				raw, "PRAGMA user_version = 1", nullptr, nullptr, nullptr)
				== SQLITE_OK,
			"and is stamped as schema version 1");
		(void)::sqlite3_close_v2(raw);
	}

	/// \brief Returns the identity this store stamps, read from a real file.
	[[nodiscard]] auto stamped_application_identity(
		const scratch_directory& aScratch) -> std::int64_t
	{
		auto fresh = appearance_settings_database::open(
			aScratch.file("identity.sqlite3"), { });
		expect(fresh.has_value(), "a fresh store opens to report its identity");
		const auto identity
			= scalar_integer(fresh->handle(), "PRAGMA application_id");
		expect(identity.has_value(), "and reports an application identity");
		return *identity;
	}

	/// \brief A version-1 record arrives with its mode and fonts and no colours.
	///
	/// \details Version 4 clears `palette_stored` once, because the built-in dark
	/// palette moved with [[BPR-0720]] and the owner asked for the saved
	/// customisation of the old defaults to be discarded. Every version below 4
	/// passes through that reset, so a version-1 palette is deliberately not
	/// carried into any theme's slot — the versions that do keep their
	/// customisation are 4, 5 and 6, and those are covered separately.
	auto test_a_format_one_record_migrates_without_a_customisation() -> void
	{
		const auto scratch  = scratch_directory{ };
		const auto identity = stamped_application_identity(scratch);

		constexpr auto modes = std::array{
			core::appearance_mode::system,
			core::appearance_mode::dark,
			core::appearance_mode::high_contrast_light,
		};
		for (std::size_t index = 0U; index < modes.size(); ++index) {
			const auto path     = scratch.file("v1-" + std::to_string(index) + ".sqlite3");
			auto       original = legal_baseline();
			original.Mode       = modes[index];
			write_version_one_record(path, original, seeded_palette(5U), identity);

			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(),
				   "a format-1 database opens and migrates forward");

			const auto version
				= scalar_integer(migrated->handle(), "PRAGMA user_version");
			expect(version.has_value() && *version == appearance_settings_schema_version,
				   "the migrated file is stamped as the current format");
			const auto format = scalar_integer(
				migrated->handle(),
				"SELECT schema_format FROM appearance_meta WHERE singleton = 1");
			expect(format.has_value() && *format == appearance_settings_schema_version,
				   "and its metadata row agrees with the stamp");

			const auto loaded = migrated->load({ });
			expect(loaded.has_value() && loaded->has_value(),
				   "the migrated record loads");
			expect(**loaded == original,
				   "the migration changes no mode and no typography value, and "
				   "leaves every theme uncustomised");
			auto reopened = appearance_settings_database::open(path, { });
			expect(reopened.has_value() && reopened->load({ }) == loaded,
				   "the migrated version-one record reopens with every value preserved");
		}
	}

	auto test_version_two_preserves_customization_and_reset() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("v2.sqlite3");
		auto       chosen  = distinct_settings(core::appearance_variant::dark);
		{
			auto database = appearance_settings_database::open(path, { });
			expect(database.has_value() && database->store(chosen, { }).has_value(),
				   "legacy fixture starts with a durable record");
		}
		down_edit_to_version(path, 2, core::appearance_variant::dark, true);

		// Version 3 carried a version-two palette forward as stored; version 4
		// resets it once ([[BPR-0720]]), so every other value survives and no
		// theme arrives customised.
		application::customisation_of(chosen, core::appearance_variant::dark)
			= application::appearance_palette_customisation{ };
		{
			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(), "version two migrates");
			const auto loaded = migrated->load({ });
			expect(loaded && loaded->has_value() && **loaded == chosen,
				   "legacy records keep every stored value and arrive with no "
				   "theme customised");
			expect(migrated->store(chosen, { }).has_value(),
				   "reset can persist an empty override set");
		}
		auto reopened = appearance_settings_database::open(path, { });
		expect(reopened.has_value(), "reset database reopens");
		const auto loaded = reopened->load({ });
		expect(loaded && loaded->has_value() && **loaded == chosen,
			   "reset survives SQLite close and reopen without changing other domains");
	}

	/// \brief [[BPR-0720]]: opening a version-3 file resets the stored palette once.
	auto test_version_three_resets_the_stored_palette_once() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("v3.sqlite3");
		auto       chosen  = distinct_settings(core::appearance_variant::dark);
		{
			auto database = appearance_settings_database::open(path, { });
			expect(database.has_value() && database->store(chosen, { }).has_value(),
				   "version-three fixture starts with a stored customisation");
		}
		down_edit_to_version(path, 3, core::appearance_variant::dark, true);
		{
			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(), "version three migrates");
			const auto version = scalar_integer(migrated->handle(), "PRAGMA user_version");
			expect(version.has_value() && *version == appearance_settings_schema_version,
				   "the migrated file is stamped as the current format");
			const auto loaded = migrated->load({ });
			application::customisation_of(chosen, core::appearance_variant::dark)
				= application::appearance_palette_customisation{ };
			expect(loaded && loaded->has_value() && **loaded == chosen,
				   "the migration clears the stored palette and keeps the mode and "
				   "typography");
		}
		{
			auto reopened = appearance_settings_database::open(path, { });
			expect(reopened.has_value(),
				   "the migrated file reopens at the current version");
			with_customisation(chosen, core::appearance_variant::dark, seeded_palette(3U));
			expect(reopened->store(chosen, { }).has_value(),
				   "a new customisation can be stored after the reset");
		}
		auto again = appearance_settings_database::open(path, { });
		expect(again.has_value(), "the stored file reopens");
		const auto loaded = again->load({ });
		expect(loaded && loaded->has_value() && **loaded == chosen,
			   "the reset happens once, not on every open");
	}

	auto test_version_four_gains_the_macro_role_without_a_reset() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("v4.sqlite3");
		auto       chosen  = distinct_settings(core::appearance_variant::light);
		{
			auto database = appearance_settings_database::open(path, { });
			expect(database.has_value() && database->store(chosen, { }).has_value(),
				   "version-four fixture starts with a stored customisation");
		}
		down_edit_to_version(path, 4, core::appearance_variant::light, true);
		{
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK,
				   "version-four fixture opens raw");
			// Strip the `syntax_macro` line the current document carries, which is
			// what a document written before the role existed looks like.
			expect(::sqlite3_exec(raw,
								  "UPDATE appearance_settings SET palette_document = "
								  "substr(palette_document, 1, instr(palette_document, 'syntax_macro ') - 1)"
								  " || substr(palette_document, instr(palette_document, 'syntax_macro ')"
								  " + instr(substr(palette_document, instr(palette_document, 'syntax_macro ')), char(10)))"
								  " WHERE singleton = 1",
								  nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "the fixture's document loses its macro row");
			const auto stripped = scalar_integer(raw, "SELECT instr(palette_document, 'syntax_macro ') FROM appearance_settings WHERE singleton = 1");
			expect(stripped.has_value() && *stripped == 0,
				   "the fixture's document has no macro row");
			expect(::sqlite3_close(raw) == SQLITE_OK, "version-four fixture closes");
		}
		{
			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(), "version four migrates");
			const auto version = scalar_integer(migrated->handle(), "PRAGMA user_version");
			expect(version.has_value() && *version == appearance_settings_schema_version,
				   "the migrated file is stamped as the current format");
			const auto loaded = migrated->load({ });
			expect(loaded && loaded->has_value(),
				   "the migrated record loads: the document is complete again");
			if (loaded && loaded->has_value()) {
				auto  expected = chosen;
				auto& palette  = application::customisation_of(
									 expected, core::appearance_variant::light)
									 .Palette;
				palette[core::index_of(core::appearance_role::syntax_macro)]
					= palette[core::index_of(core::appearance_role::syntax_preprocessor)];
				expect(**loaded == expected,
					   "the macro role takes the stored preprocessor colour, and "
					   "the customisation lands under the theme it named without a "
					   "second reset");
			}
		}
	}

	void test_version_five_dif_migration_and_corrupt_refusal()
	{
		const auto scratch   = scratch_directory{ };
		const auto mutations = std::array{ "", "UPDATE appearance_settings SET ui_point_size = 1", "UPDATE appearance_settings SET mode = 'unknown'", "UPDATE appearance_settings SET palette_variant = 'unknown'", "UPDATE appearance_settings SET palette_document = 'invalid'", "PRAGMA ignore_check_constraints = ON; UPDATE appearance_settings SET palette_stored = 2", "PRAGMA ignore_check_constraints = ON; UPDATE appearance_settings SET code_ligatures = 2" };
		for (std::size_t index = 0; index < mutations.size(); ++index) {
			const auto corrupt = index != 0;
			const auto path    = scratch.file("v5-" + std::to_string(index) + ".sqlite3");
			auto       chosen  = distinct_settings(core::appearance_variant::high_contrast_light);
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database && database->store(chosen, { }),
					   "version-five fixture begins with distinct settings");
			}
			down_edit_to_version(
				path, 5, core::appearance_variant::high_contrast_light, true);
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "v5 fixture opens raw");
			if (corrupt) {
				expect(::sqlite3_exec(raw, mutations[index], nullptr, nullptr, nullptr) == SQLITE_OK, "invalid legacy value inserted");
			}
			const auto before = legacy_snapshot(raw);
			expect(::sqlite3_close(raw) == SQLITE_OK, "v5 raw writer closes");
			auto migrated = appearance_settings_database::open(path, { });
			if (corrupt) {
				expect(!migrated, "invalid version-five values refuse migration");
				expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "refused v5 reopens raw");
				expect(legacy_snapshot(raw) == before, "refusal leaves all legacy schema, stamps and values unchanged");
				expect(::sqlite3_close(raw) == SQLITE_OK, "refusal reader closes");
				continue;
			}
			expect(migrated.has_value(), "version-five migration succeeds");
			const auto loaded = migrated->load({ });
			expect(loaded && loaded->has_value() && **loaded == chosen
					   && (**loaded).Dif == core::dif_preferences{ },
				   "migration preserves complete legacy settings, files the palette "
				   "under the theme it named, and supplies shown/System");
			chosen.Dif = { .Visibility = core::dif_visibility::hidden, .Motion = core::motion_mode::full };
			expect(migrated->store(chosen, { }).has_value(), "new Dif choices store");
			migrated      = appearance_settings_database{ };
			auto reopened = appearance_settings_database::open(path, { });
			expect(reopened && reopened->load({ }).value().value() == chosen, "Dif and legacy settings restore together after reopen");
		}
	}

	void test_corrupt_dif_values_are_refused()
	{
		const auto scratch = scratch_directory{ };
		for (const auto* mutation : { "UPDATE appearance_settings SET dif_visibility = 256", "UPDATE appearance_settings SET motion_mode = -1" }) {
			const auto path = scratch.file("dif.sqlite3");
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database && database->store(distinct_settings(), { }), "valid Dif fixture stores");
			}
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "Dif fixture opens raw");
			expect(::sqlite3_exec(raw, "PRAGMA ignore_check_constraints = ON", nullptr, nullptr, nullptr) == SQLITE_OK && ::sqlite3_exec(raw, mutation, nullptr, nullptr, nullptr) == SQLITE_OK, "invalid Dif value inserted past SQL constraints");
			const auto before = legacy_snapshot(raw);
			expect(::sqlite3_close(raw) == SQLITE_OK, "Dif mutation writer closes");
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database && !database->load({ }), "invalid Dif record is refused whole");
			}
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "Dif fixture reopens for nonmutation check");
			expect(legacy_snapshot(raw) == before, "reading corrupt Dif values does not repair them");
			expect(::sqlite3_close(raw) == SQLITE_OK, "Dif fixture reader closes");
		}
	}

	auto test_legacy_refusal_preserves_schema_and_rows() -> void
	{
		const auto     scratch   = scratch_directory{ };
		const auto     identity  = stamped_application_identity(scratch);
		constexpr auto mutations = std::array{
			"UPDATE appearance_meta SET schema_format = 99",
			"DELETE FROM appearance_meta",
			"DROP TABLE appearance_meta",
			"ALTER TABLE appearance_settings ADD COLUMN future_setting TEXT",
			"ALTER TABLE appearance_settings RENAME COLUMN ui_weight TO unknown_weight",
			"ALTER TABLE appearance_settings RENAME TO old_settings; CREATE TABLE appearance_settings AS SELECT * FROM old_settings; DROP TABLE old_settings",
			"PRAGMA application_id = 123",
			"PRAGMA ignore_check_constraints = ON; UPDATE appearance_settings SET code_ligatures = 7"
		};
		for (const auto version : { 1, 2, 3 }) {
			for (std::size_t index = 0; index < mutations.size(); ++index) {
				const auto path = scratch.file("legacy-" + std::to_string(version) + "-" + std::to_string(index) + ".sqlite3");
				write_version_one_record(path, distinct_settings(), seeded_palette(1U), identity);
				::sqlite3* raw = nullptr;
				expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "legacy fixture opens raw");
				if (version >= 2) {
					expect(::sqlite3_exec(raw,
										  "ALTER TABLE appearance_settings ADD COLUMN palette_variant TEXT NOT NULL DEFAULT 'dark';"
										  "UPDATE appearance_meta SET schema_format = 2; PRAGMA user_version = 2",
										  nullptr, nullptr, nullptr)
							   == SQLITE_OK,
						   "fixture upgrades to canonical version two");
				}
				if (version == 3) {
					expect(::sqlite3_exec(raw,
										  "ALTER TABLE appearance_settings ADD COLUMN palette_stored INTEGER NOT NULL DEFAULT 1 CHECK(palette_stored IN (0, 1));"
										  "UPDATE appearance_meta SET schema_format = 3; PRAGMA user_version = 3",
										  nullptr, nullptr, nullptr)
							   == SQLITE_OK,
						   "fixture upgrades to canonical version three");
				}
				expect(::sqlite3_exec(raw, mutations[index], nullptr, nullptr, nullptr) == SQLITE_OK, "legacy mutation runs");
				const auto before = legacy_snapshot(raw);
				(void)::sqlite3_close(raw);
				const auto refused = appearance_settings_database::open(path, { });
				expect(!refused, "invalid legacy admission or failed migration is refused");
				if (index + 1 < mutations.size()) {
					expect(refused.error() == catalog_database_failure::schema_mismatch, "invalid legacy shape or identity reports schema mismatch");
				}
				expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "refused fixture reopens raw");
				expect(legacy_snapshot(raw) == before, "refusal retains original identity, version, schema SQL, columns and every settings value");
				if (index != 2) {
					const auto format = scalar_integer(raw, "SELECT schema_format FROM appearance_meta WHERE singleton = 1");
					expect(index == 1 ? !format : format && *format == (index == 0 ? 99 : version), "metadata is not normalized by a refused migration");
				}
				(void)::sqlite3_close(raw);
			}
		}
	}

	/// \brief [[BPR-1107]]. Version 8 keeps when the panes draw white space: every
	/// choice round-trips, a version-7 file takes the fresh default, and a value
	/// outside the vocabulary is refused.
	auto test_white_space_round_trips_migrates_and_refuses() -> void
	{
		const auto scratch = scratch_directory{ };
		{
			const auto path     = scratch.file("white-space.sqlite3");
			auto       database = appearance_settings_database::open(path, { });
			expect(database.has_value(), "a fresh store opens");
			for (std::size_t index = 0U; index < core::white_space_drawing_count; ++index) {
				auto settings                       = legal_baseline();
				settings.Typography.Code.WhiteSpace = static_cast<core::white_space_drawing>(static_cast<std::uint8_t>(index));
				expect(database->store(settings, { }).has_value(), "every white space choice stores");
				const auto loaded = database->load({ });
				expect(loaded.has_value() && loaded->has_value() && **loaded == settings,
					   "and loads back exactly: " + std::string{ core::stable_name(settings.Typography.Code.WhiteSpace) });
			}
			const auto columns = appearance_settings_columns(database->handle());
			expect(columns.has_value() && std::ranges::find(*columns, std::string{ "appearance_settings.code_white_space" }) != columns->end(),
				   "the schema declares code_white_space");
		}
		{
			const auto path                     = scratch.file("v7.sqlite3");
			auto       original                 = distinct_settings();
			original.Typography.Code.WhiteSpace = core::white_space_drawing::trailing;
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value() && database->store(original, { }).has_value(), "a current record stores before the down-edit");
			}
			down_edit_to_version(path, 7, core::appearance_variant::high_contrast_dark, true);
			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(), "a version-seven file opens and migrates forward");
			const auto loaded                   = migrated->load({ });
			auto       expected                 = original;
			expected.Typography.Code.WhiteSpace = core::white_space_drawing::selection;
			expect(loaded.has_value() && loaded->has_value() && **loaded == expected,
				   "it keeps every value it had and takes the fresh default for white space, selection");
		}
		{
			const auto path = scratch.file("corrupt-white-space.sqlite3");
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value() && database->store(legal_baseline(), { }).has_value(), "a record stores");
			}
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "the file opens raw");
			expect(::sqlite3_exec(raw, "UPDATE appearance_settings SET code_white_space = 6", nullptr, nullptr, nullptr) != SQLITE_OK,
				   "the table's CHECK refuses a value outside the vocabulary");
			expect(::sqlite3_exec(raw, "PRAGMA ignore_check_constraints = 1", nullptr, nullptr, nullptr) == SQLITE_OK
					   && ::sqlite3_exec(raw, "UPDATE appearance_settings SET code_white_space = 6", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "a value written past the CHECK stands in for a file edited by another tool");
			expect(::sqlite3_close(raw) == SQLITE_OK, "the raw handle closes");
			auto database = appearance_settings_database::open(path, { });
			expect(database.has_value(), "the file still opens");
			const auto loaded = database->load({ });
			expect(!loaded.has_value() && loaded.error() == catalog_database_failure::integrity_failure,
				   "and the reader refuses the record whole rather than guessing a choice");
		}
	}

	/// \brief [[BPR-1336]]. Version 9 keeps the comparison's zoom: every step
	/// round-trips, a version-8 file takes Actual Size, and a value outside the
	/// ladder's ends is refused.
	auto test_code_zoom_round_trips_migrates_and_refuses() -> void
	{
		const auto scratch = scratch_directory{ };
		{
			const auto path     = scratch.file("zoom.sqlite3");
			auto       database = appearance_settings_database::open(path, { });
			expect(database.has_value(), "a fresh store opens");
			for (const auto zoom : core::code_zoom_steps) {
				auto settings                        = legal_baseline();
				settings.Typography.Code.ZoomPercent = zoom;
				expect(database->store(settings, { }).has_value(), "every zoom step stores");
				const auto loaded = database->load({ });
				expect(loaded.has_value() && loaded->has_value() && **loaded == settings,
					   "and loads back exactly: " + std::to_string(zoom) + "%");
			}
			const auto columns = appearance_settings_columns(database->handle());
			expect(columns.has_value() && std::ranges::find(*columns, std::string{ "appearance_settings.code_zoom_percent" }) != columns->end(),
				   "the schema declares code_zoom_percent");
		}
		for (const auto version : { std::int64_t{ 8 }, std::int64_t{ 7 } }) {
			const auto path                      = scratch.file("v" + std::to_string(version) + "-zoom.sqlite3");
			auto       original                  = distinct_settings();
			original.Typography.Code.ZoomPercent = 150U;
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value() && database->store(original, { }).has_value(), "a current record stores before the down-edit");
			}
			down_edit_to_version(path, version, core::appearance_variant::dark, true);
			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(), "an older file opens and migrates forward: version " + std::to_string(version));
			const auto loaded                    = migrated->load({ });
			auto       expected                  = original;
			expected.Typography.Code.ZoomPercent = core::actual_code_zoom_percent;
			if (version < 8) {
				expected.Typography.Code.WhiteSpace = core::code_font_settings{ }.WhiteSpace;
			}
			expect(loaded.has_value() && loaded->has_value() && **loaded == expected,
				   "it keeps every value it had and takes Actual Size: version " + std::to_string(version));
		}
		{
			const auto path = scratch.file("corrupt-zoom.sqlite3");
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value() && database->store(legal_baseline(), { }).has_value(), "a record stores");
			}
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "the file opens raw");
			expect(::sqlite3_exec(raw, "UPDATE appearance_settings SET code_zoom_percent = 301", nullptr, nullptr, nullptr) != SQLITE_OK
					   && ::sqlite3_exec(raw, "UPDATE appearance_settings SET code_zoom_percent = 49", nullptr, nullptr, nullptr) != SQLITE_OK,
				   "the table's CHECK refuses a zoom outside the ladder's ends");
			expect(::sqlite3_exec(raw, "PRAGMA ignore_check_constraints = 1", nullptr, nullptr, nullptr) == SQLITE_OK
					   && ::sqlite3_exec(raw, "UPDATE appearance_settings SET code_zoom_percent = 301", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "a value written past the CHECK stands in for a file edited by another tool");
			expect(::sqlite3_close(raw) == SQLITE_OK, "the raw handle closes");
			auto database = appearance_settings_database::open(path, { });
			expect(database.has_value(), "the file still opens");
			const auto loaded = database->load({ });
			expect(!loaded.has_value() && loaded.error() == catalog_database_failure::integrity_failure,
				   "and the reader refuses the record whole rather than clamping the zoom");
		}
	}

	/// \brief [[BPR-1714]]. Version 10 keeps the known C++ namespaces as
	/// written: every list round-trips, an older file takes std alone, and a
	/// list the core refuses is refused whole.
	auto test_known_namespaces_round_trip_migrate_and_refuse() -> void
	{
		const auto scratch = scratch_directory{ };
		{
			const auto path     = scratch.file("namespaces.sqlite3");
			auto       database = appearance_settings_database::open(path, { });
			expect(database.has_value(), "a fresh store opens");
			for (const auto& names : { std::vector<std::string>{ }, std::vector<std::string>{ "std" },
									   std::vector<std::string>{ "std", "Qt", "boost::asio" }, std::vector<std::string>{ "Qt", "std" } }) {
				auto settings       = legal_baseline();
				settings.Namespaces = core::known_namespaces{ .Names = names };
				expect(database->store(settings, { }).has_value(), "every legal list stores");
				const auto loaded = database->load({ });
				expect(loaded.has_value() && loaded->has_value() && **loaded == settings,
					   "and loads back exactly, in its order: " + core::join_known_namespaces(settings.Namespaces));
			}
			const auto columns = appearance_settings_columns(database->handle());
			expect(columns.has_value() && std::ranges::find(*columns, std::string{ "appearance_settings.known_namespaces" }) != columns->end(),
				   "the schema declares known_namespaces");
		}
		for (const auto version : { std::int64_t{ 9 }, std::int64_t{ 8 } }) {
			const auto path     = scratch.file("v" + std::to_string(version) + "-namespaces.sqlite3");
			auto       original = distinct_settings();
			original.Namespaces = core::known_namespaces{ .Names = { "Qt" } };
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value() && database->store(original, { }).has_value(), "a current record stores before the down-edit");
			}
			down_edit_to_version(path, version, core::appearance_variant::dark, true);
			auto migrated = appearance_settings_database::open(path, { });
			expect(migrated.has_value(), "an older file opens and migrates forward: version " + std::to_string(version));
			const auto loaded   = migrated->load({ });
			auto       expected = original;
			expected.Namespaces = core::known_namespaces{ };
			if (version < 9) {
				expected.Typography.Code.ZoomPercent = core::actual_code_zoom_percent;
			}
			expect(loaded.has_value() && loaded->has_value() && **loaded == expected,
				   "it keeps every value it had and takes std alone: version " + std::to_string(version));
			const auto stamped = scalar_integer(migrated->handle(), "PRAGMA user_version");
			expect(stamped.has_value() && *stamped == appearance_settings_schema_version, "and is stamped at the current version");
		}
		{
			const auto path = scratch.file("corrupt-namespaces.sqlite3");
			{
				auto database = appearance_settings_database::open(path, { });
				expect(database.has_value() && database->store(legal_baseline(), { }).has_value(), "a record stores");
			}
			::sqlite3* raw = nullptr;
			expect(::sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK, "the file opens raw");
			const auto oversized = "UPDATE appearance_settings SET known_namespaces = '" + std::string(9'000U, 'a') + "'";
			expect(::sqlite3_exec(raw, oversized.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK,
				   "the table's CHECK refuses a list longer than any legal one");
			expect(::sqlite3_exec(raw, "UPDATE appearance_settings SET known_namespaces = 'std, 3d'", nullptr, nullptr, nullptr) == SQLITE_OK,
				   "an illegal entry stands in for a file edited by another tool");
			expect(::sqlite3_close(raw) == SQLITE_OK, "the raw handle closes");
			auto database = appearance_settings_database::open(path, { });
			expect(database.has_value(), "the file still opens");
			const auto loaded = database->load({ });
			expect(!loaded.has_value() && loaded.error() == catalog_database_failure::integrity_failure,
				   "and the reader refuses the record whole rather than keeping part of the list");
		}
	}

	auto test_the_schema_stores_no_review_content() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");

		auto opened = appearance_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");

		const auto columns = appearance_settings_columns(opened->handle());
		expect(columns.has_value(), "the created schema reports its columns");

		constexpr auto prohibited = std::array{
			std::string_view{ "path" },
			std::string_view{ "repository" },
			std::string_view{ "pull" },
			std::string_view{ "account" },
			std::string_view{ "title" },
			std::string_view{ "comment" },
			std::string_view{ "branch" },
			std::string_view{ "token" },
			std::string_view{ "credential" },
		};
		for (const auto& column : *columns) {
			for (const auto banned : prohibited) {
				expect(column.find(banned) == std::string::npos,
					   "the appearance schema names no review content: "
					   "offending column is "
						   + std::string{ column });
			}
		}
		expect(!columns->empty(), "the schema declares columns at all");

		// One document and one stored flag per variant, named from the core's own
		// stable identifiers. A theme whose columns were missing would be a theme
		// whose colours could not be kept.
		for (std::size_t index = 0U; index < core::appearance_variant_count; ++index) {
			const auto variant = variant_at(index);
			for (const auto& expected :
				 { document_column(variant), stored_column(variant) }) {
				expect(std::ranges::find(
						   *columns, "appearance_settings." + expected)
						   != columns->end(),
					   "the schema declares " + expected);
			}
		}
	}
} // namespace

auto main() -> int
try {
	test_absent_is_distinguishable_from_stored_defaults();
	test_every_domain_round_trips();
	test_every_theme_round_trips_independently();
	test_each_theme_is_stored_as_its_own_theme_document();
	test_a_tampered_palette_is_refused();
	test_a_palette_stored_without_label_tints_loads();
	test_a_stored_flag_outside_its_range_is_refused();
	test_an_unknown_mode_name_is_refused();
	test_typography_the_core_rejects_is_refused_whole();
	test_version_six_files_one_palette_under_its_recorded_variant();
	test_version_six_without_a_stored_palette_carries_no_customisation();
	test_version_six_refuses_a_malformed_legacy_value();
	test_a_format_one_record_migrates_without_a_customisation();
	test_a_newer_schema_is_refused_for_the_right_reason();
	test_cancellation_leaves_the_record_untouched();
	test_version_two_preserves_customization_and_reset();
	test_version_three_resets_the_stored_palette_once();
	test_version_four_gains_the_macro_role_without_a_reset();
	test_corrupt_dif_values_are_refused();
	test_version_five_dif_migration_and_corrupt_refusal();
	test_legacy_refusal_preserves_schema_and_rows();
	test_white_space_round_trips_migrates_and_refuses();
	test_code_zoom_round_trips_migrates_and_refuses();
	test_known_namespaces_round_trip_migrate_and_refuse();
	test_the_schema_stores_no_review_content();
	std::cout << "appearance-settings-storage-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
