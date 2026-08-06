#include "../../src/adapters/sqlite/src/appearance_settings_database.hpp"

#include "bpr/core/theme_exchange.hpp"

#include <sqlite3.h>

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

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class scratch_directory final
	{
	public:
		scratch_directory()
		{
			Path = std::filesystem::temp_directory_path()
				/ ("bpr-appearance-" + std::to_string(Counter++));
			std::filesystem::remove_all(Path);
			std::filesystem::create_directories(Path);
			std::error_code ignored;
			if (auto resolved = std::filesystem::canonical(Path, ignored);
				!ignored) {
				Path = std::move(resolved);
			}
		}
		scratch_directory(const scratch_directory&)                    = delete;
		auto operator=(const scratch_directory&) -> scratch_directory& = delete;
		scratch_directory(scratch_directory&&)                         = delete;
		auto operator=(scratch_directory&&) -> scratch_directory&      = delete;
		~scratch_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Path, ignored);
		}

		[[nodiscard]] auto file(const std::string_view aName) const
			-> std::filesystem::path
		{
			return Path / aName;
		}

	private:
		static inline int     Counter = 0;
		std::filesystem::path Path;
	};

	/// \brief The least record the core will accept.
	///
	/// \details Not `appearance_settings{ }`. A default-constructed
	/// `font_settings` has an empty family, which the core rejects on purpose -
	/// it cannot know a family and says so by being illegal until a host
	/// supplies one. A test that stored the default-constructed record would be
	/// storing something no composition ever produces.
	[[nodiscard]] auto legal_baseline() -> application::appearance_settings
	{
		auto settings                        = application::appearance_settings{ };
		settings.Typography.Ui.Font.Family   = "Baseline Interface";
		settings.Typography.Code.Font.Family = "Baseline Code";
		return settings;
	}

	/// \brief A legal record that differs from the baseline in every domain.
	[[nodiscard]] auto distinct_settings() -> application::appearance_settings
	{
		auto settings = application::appearance_settings{ };
		settings.Mode = core::appearance_mode::high_contrast_dark;
		for (std::size_t index = 0U; index < core::appearance_role_count; ++index) {
			settings.Palette[index] = core::appearance_color{
				.Red   = static_cast<std::uint8_t>(index * 3U),
				.Green = static_cast<std::uint8_t>(index * 5U),
				.Blue  = static_cast<std::uint8_t>(index * 7U),
				.Alpha = 255U,
			};
		}
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
		// Compared whole: mode, all forty-five palette roles, and every
		// typography field at once. Checking field by field would pass a store
		// that dropped one added later.
		expect(**loaded == chosen,
			   "mode, palette, and typography all survive a close and reopen, "
			   "compared whole");
	}

	auto test_the_palette_is_stored_as_a_theme_document() -> void
	{
		const auto scratch = scratch_directory{ };
		const auto path    = scratch.file("appearance.sqlite3");
		const auto chosen  = distinct_settings();

		auto opened = appearance_settings_database::open(path, { });
		expect(opened.has_value(), "the store opens");
		expect(opened->store(chosen, { }).has_value(), "the record stores");

		const auto document = scalar_text(
			opened->handle(),
			"SELECT palette_document FROM appearance_settings WHERE singleton = 1");
		expect(document.has_value(), "the palette is stored as text");
		// The stored bytes are the core's own exported document, not a second
		// encoding. That is what makes the read path inherit every refusal
		// `import_theme` already makes against a tampered file.
		expect(*document == core::export_theme(chosen.Palette),
			   "the stored palette is exactly the core's theme document");
		// Keyed by stable role identifiers rather than enum ordinals, so
		// inserting a role into the vocabulary cannot re-colour every role
		// after it.
		expect(
			document->find(std::string{ core::stable_name(
				core::appearance_role::code_background) })
				!= std::string::npos,
			"roles are named by their stable identifier in the stored document");
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

		// A durable file can be replaced by another build or edited by hand.
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
					"UPDATE appearance_settings "
					"SET palette_document = 'not a theme document' "
					"WHERE singleton = 1",
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
	}
} // namespace

auto main() -> int
{
	test_absent_is_distinguishable_from_stored_defaults();
	test_every_domain_round_trips();
	test_the_palette_is_stored_as_a_theme_document();
	test_a_tampered_palette_is_refused();
	test_an_unknown_mode_name_is_refused();
	test_typography_the_core_rejects_is_refused_whole();
	test_a_newer_schema_is_refused_for_the_right_reason();
	test_cancellation_leaves_the_record_untouched();
	test_the_schema_stores_no_review_content();
	std::cout << "appearance-settings-storage-tests:pass\n";
	return EXIT_SUCCESS;
}
