// [[BPR-0970]]. Settings and themes: the theme document importer, and the
// stores' reading of a durable settings file another build or a person may
// have edited.
//
// Input: [selector] then the payload. Selector 0 imports the payload as a
// theme document. Selector 1 and 2 rewrite cells of a freshly stored
// appearance (1) or workbench (2) settings file and reopen it; the payload is
// a list of [table] [column] [type] [length] [bytes...] assignments.
//
// The settings families go through the real stores: the harness writes the
// cells with SQLite, as a hand edit would, and reads them back only through
// `open` and `load`.

#include "../../src/adapters/sqlite/src/appearance_settings_database.hpp"
#include "../../src/adapters/sqlite/src/workbench_settings_database.hpp"
#include "bpr/core/theme_exchange.hpp"
#include "fuzz_support.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using adapters::sqlite::detail::appearance_settings_database;
	using adapters::sqlite::detail::workbench_settings_database;

	auto theme(const std::string_view aDocument) -> void
	{
		const auto first  = core::import_theme(aDocument);
		const auto replay = core::import_theme(aDocument);
		fuzz::require(first.Palette == replay.Palette && first.Rejections == replay.Rejections);
		// A palette exactly when there is nothing to refuse.
		fuzz::require(first.Palette.has_value() == first.Rejections.empty());
		for (const auto& rejection : first.Rejections) {
			fuzz::require(rejection.Subject.size() <= core::theme_import_subject_limit);
			fuzz::require(std::ranges::all_of(rejection.Subject, [](const unsigned char aChar) { return aChar >= 32U && aChar <= 126U; }));
		}
		if (first.Palette) {
			const auto roundtrip = core::import_theme(core::export_theme(*first.Palette));
			fuzz::require(roundtrip.Palette == first.Palette && roundtrip.Rejections.empty());
		}
		if (aDocument.size() > core::theme_document_byte_limit) {
			fuzz::require(first.Rejections.size() == 1U && first.Rejections.front().Reason == core::theme_import_reason::document_too_large);
		}
	}

	struct column
	{
		std::string Table;
		std::string Name;
	};

	// One private directory per process, holding a baseline file per store and
	// the working copy each input rewrites.
	struct settings_files
	{
		settings_files()                                 = default;
		settings_files(const settings_files&)            = delete;
		settings_files& operator=(const settings_files&) = delete;
		settings_files(settings_files&& aOther) noexcept
			: Root{ std::exchange(aOther.Root, { }) }
			, AppearanceBaseline{ std::move(aOther.AppearanceBaseline) }
			, WorkbenchBaseline{ std::move(aOther.WorkbenchBaseline) }
			, AppearanceColumns{ std::move(aOther.AppearanceColumns) }
			, WorkbenchColumns{ std::move(aOther.WorkbenchColumns) }
		{
		}
		settings_files& operator=(settings_files&&) = delete;
		~settings_files()
		{
			// The directory is this process's own, made by mkdtemp below.
			if (!Root.empty()) {
				std::error_code ignored;
				std::filesystem::remove_all(Root, ignored);
			}
		}

		std::filesystem::path     Root;
		std::vector<std::uint8_t> AppearanceBaseline;
		std::vector<std::uint8_t> WorkbenchBaseline;
		std::vector<column>       AppearanceColumns;
		std::vector<column>       WorkbenchColumns;
	};

	auto read_file(const std::filesystem::path& aPath) -> std::vector<std::uint8_t>
	{
		std::ifstream stream{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto columns_of(::sqlite3* aDatabase) -> std::vector<column>
	{
		std::vector<std::string> tables;
		::sqlite3_stmt*          statement = nullptr;
		fuzz::require(::sqlite3_prepare_v2(aDatabase, "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name", -1, &statement, nullptr) == SQLITE_OK);
		while (::sqlite3_step(statement) == SQLITE_ROW) {
			tables.emplace_back(reinterpret_cast<const char*>(::sqlite3_column_text(statement, 0)));
		}
		::sqlite3_finalize(statement);
		std::vector<column> result;
		for (const auto& table : tables) {
			fuzz::require(::sqlite3_prepare_v2(aDatabase, ("SELECT name FROM pragma_table_info('" + table + "') ORDER BY cid").c_str(), -1, &statement, nullptr) == SQLITE_OK);
			while (::sqlite3_step(statement) == SQLITE_ROW) {
				result.push_back({ table, reinterpret_cast<const char*>(::sqlite3_column_text(statement, 0)) });
			}
			::sqlite3_finalize(statement);
		}
		fuzz::require(!result.empty());
		return result;
	}

	auto remove_sidecars(const std::filesystem::path& aPath) -> void
	{
		for (const auto* suffix : { "-wal", "-shm", "-journal" }) {
			std::error_code ignored;
			std::filesystem::remove(aPath.string() + suffix, ignored);
		}
	}

	auto make_files() -> settings_files
	{
		auto pattern = (std::filesystem::temp_directory_path() / "bpr-fuzz-settings-XXXXXX").string();
		fuzz::require(::mkdtemp(pattern.data()) != nullptr);
		settings_files files;
		files.Root = pattern;
		{
			const auto path   = files.Root / "appearance-baseline.sqlite3";
			auto       opened = appearance_settings_database::open(path, { });
			fuzz::require(opened.has_value() && opened->store(application::appearance_settings{ }, { }).has_value());
			files.AppearanceColumns = columns_of(opened->handle());
		}
		{
			const auto path   = files.Root / "workbench-baseline.sqlite3";
			auto       opened = workbench_settings_database::open(path, { });
			fuzz::require(opened.has_value() && opened->store(application::workbench_layout_settings{ }, { }).has_value());
			files.WorkbenchColumns = columns_of(opened->handle());
		}
		for (const auto* name : { "appearance-baseline.sqlite3", "workbench-baseline.sqlite3" }) {
			// A closed store leaves no write-ahead log, so the main file is the
			// whole baseline.
			for (const auto* suffix : { "-wal", "-journal" }) {
				fuzz::require(!std::filesystem::exists(files.Root / (std::string{ name } + suffix)));
			}
		}
		files.AppearanceBaseline = read_file(files.Root / "appearance-baseline.sqlite3");
		files.WorkbenchBaseline  = read_file(files.Root / "workbench-baseline.sqlite3");
		fuzz::require(!files.AppearanceBaseline.empty() && !files.WorkbenchBaseline.empty());
		return files;
	}

	auto files() -> settings_files&
	{
		static settings_files instance = make_files();
		return instance;
	}

	// Writes the baseline to the working path and applies the input's cell
	// assignments with plain SQL. Returns false when SQLite itself refuses an
	// assignment, which is not the store's decision.
	auto tamper(const std::filesystem::path& aPath, const std::vector<std::uint8_t>& aBaseline, const std::vector<column>& aColumns, fuzz::fuzz_input& aInput) -> bool
	{
		remove_sidecars(aPath);
		{
			std::ofstream stream{ aPath, std::ios::binary | std::ios::trunc };
			stream.write(reinterpret_cast<const char*>(aBaseline.data()), static_cast<std::streamsize>(aBaseline.size()));
			fuzz::require(stream.good());
		}
		::sqlite3* raw = nullptr;
		fuzz::require(::sqlite3_open_v2(aPath.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
		bool applied = ::sqlite3_exec(raw, "PRAGMA ignore_check_constraints = ON", nullptr, nullptr, nullptr) == SQLITE_OK;
		for (int assignment = 0; applied && assignment < 4 && !aInput.rest().empty(); ++assignment) {
			const auto& target  = aColumns[aInput.u16() % aColumns.size()];
			const auto  type    = aInput.byte();
			const auto  length  = std::min<std::size_t>(aInput.byte(), aInput.rest().size());
			const auto  payload = aInput.rest().first(length);
			const auto  value   = std::vector<std::uint8_t>{ payload.begin(), payload.end() };
			for (std::size_t skipped = 0U; skipped < length; ++skipped) {
				static_cast<void>(aInput.byte());
			}
			::sqlite3_stmt* statement = nullptr;
			const auto      sql       = "UPDATE \"" + target.Table + "\" SET \"" + target.Name + "\" = ?";
			if (::sqlite3_prepare_v2(raw, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
				applied = false;
				break;
			}
			switch (type % 5U) {
			case 0U: {
				std::int64_t number = 0;
				std::memcpy(&number, value.data(), std::min(value.size(), sizeof number));
				::sqlite3_bind_int64(statement, 1, number);
				break;
			}
			case 1U:
				::sqlite3_bind_text(statement, 1, reinterpret_cast<const char*>(value.data()), static_cast<int>(value.size()), SQLITE_TRANSIENT);
				break;
			case 2U:
				::sqlite3_bind_blob(statement, 1, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
				break;
			case 3U:
				::sqlite3_bind_null(statement, 1);
				break;
			default:
				::sqlite3_bind_int64(statement, 1, static_cast<std::int64_t>(type) - 128);
				break;
			}
			applied = ::sqlite3_step(statement) == SQLITE_DONE;
			::sqlite3_finalize(statement);
		}
		::sqlite3_close_v2(raw);
		return applied;
	}

	auto appearance(fuzz::fuzz_input& aInput) -> void
	{
		auto&      state = files();
		const auto path  = state.Root / "appearance.sqlite3";
		if (!tamper(path, state.AppearanceBaseline, state.AppearanceColumns, aInput)) {
			return;
		}
		auto opened = appearance_settings_database::open(path, std::stop_token{ });
		if (!opened) {
			return;
		}
		const auto loaded = opened->load(std::stop_token{ });
		if (loaded && *loaded) {
			// What the store adopts is something the core itself accepts: no
			// palette the importer refuses and no typography it rejects.
			const auto& settings = **loaded;
			fuzz::require(core::validate_typography(settings.Typography).empty());
			for (const auto& palette : settings.Palettes) {
				fuzz::require(core::import_theme(core::export_theme(palette.Palette)).Palette == palette.Palette);
			}
		}
	}

	auto workbench(fuzz::fuzz_input& aInput) -> void
	{
		auto&      state = files();
		const auto path  = state.Root / "workbench.sqlite3";
		if (!tamper(path, state.WorkbenchBaseline, state.WorkbenchColumns, aInput)) {
			return;
		}
		auto opened = workbench_settings_database::open(path, std::stop_token{ });
		if (!opened) {
			return;
		}
		static_cast<void>(opened->load(std::stop_token{ }));
		static_cast<void>(opened->load_window(core::bpr_window_id{ 1 }, std::stop_token{ }));
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	fuzz::fuzz_input input{ aData, aSize };
	const auto       selector = input.byte();
	if (input.rest().size() > core::theme_document_byte_limit + 1024U) {
		return 0;
	}
	switch (selector % 3U) {
	case 0U:
		theme(input.text());
		break;
	case 1U:
		appearance(input);
		break;
	default:
		workbench(input);
		break;
	}
	return 0;
}
