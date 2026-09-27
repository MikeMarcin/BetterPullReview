#include "bpr/adapters/sqlite/diagnostic_preferences.hpp"
#include "sqlite_connection.hpp"
#include "storage_internal.hpp"
#include <memory>
#include <sqlite3.h>
namespace bpr::adapters::sqlite {
	namespace {
		using namespace application;
		struct connection
		{
			detail::state_directory                               Directory;
			std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> Database{ nullptr, sqlite3_close_v2 };
		};
		auto open(const std::filesystem::path& aRoot) -> std::unique_ptr<connection>
		{
			if (!detail::sqlite_runtime_ready()) {
				return { };
			}
			auto directory = detail::open_state_directory(aRoot);
			if (!directory) {
				return { };
			}
			auto handle = detail::open_private_store_database(*directory, "diagnostic-preferences.sqlite3",
															  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_NOFOLLOW);
			if (!handle) {
				return { };
			}
			auto  result = std::make_unique<connection>(connection{ std::move(*directory), { *handle, sqlite3_close_v2 } });
			auto* db     = result->Database.get();
			if (!detail::configure_connection(db)) {
				return { };
			}
			auto identity = detail::scalar_integer(db, "PRAGMA application_id");
			if (!identity || (*identity != 0 && *identity != 0x42505247)) {
				return { };
			}
			if (!detail::execute(db, "PRAGMA application_id=1112560199")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS diagnostic_preferences(singleton INTEGER PRIMARY KEY CHECK(singleton=1), version INTEGER NOT NULL, enabled INTEGER NOT NULL, detail INTEGER NOT NULL, privacy INTEGER NOT NULL) STRICT")) {
				return { };
			}
			return result;
		}
	}
	auto load_diagnostic_preferences(const std::filesystem::path& aRoot) noexcept -> application::diagnostic_preference_load
	{
		diagnostic_preference_load result{ .Preferences = { .Enabled = false }, .Recovered = true };
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return result;
			}
			auto row = detail::prepare(connection->Database.get(), "SELECT version,enabled,detail,privacy FROM diagnostic_preferences WHERE singleton=1");
			if (!row) {
				return result;
			}
			const auto code = sqlite3_step(row->Value);
			if (code == SQLITE_DONE) {
				return { { }, true, false };
			}
			if (code != SQLITE_ROW) {
				return result;
			}
			for (int column = 0; column < 4; ++column) {
				if (sqlite3_column_type(row->Value, column) != SQLITE_INTEGER) {
					return result;
				}
			}
			const auto version         = sqlite3_column_int64(row->Value, 0);
			const auto enabled         = sqlite3_column_int64(row->Value, 1);
			const auto detail          = sqlite3_column_int64(row->Value, 2);
			const auto privacy         = sqlite3_column_int64(row->Value, 3);
			result.Available           = true;
			result.Preferences.Enabled = enabled == 1;
			if (version != 1 || enabled < 0 || enabled > 1 || detail < 0 || detail > 1 || privacy < 0 || privacy > 1) {
				return result;
			}
			result.Preferences.Detail  = static_cast<diagnostic_detail>(detail);
			result.Preferences.Privacy = static_cast<diagnostic_privacy>(privacy);
			result.Recovered           = false;
		} catch (...) { }
		return result;
	}
	auto store_diagnostic_preferences(const std::filesystem::path& aRoot, application::diagnostic_preferences aPreferences) noexcept -> bool
	{
		if (!valid_diagnostic_preferences(aPreferences)) {
			return false;
		}
		try {
			auto connection = open(aRoot);
			if (!connection) {
				return false;
			}
			auto row = detail::prepare(connection->Database.get(), "INSERT OR REPLACE INTO diagnostic_preferences VALUES(1,1,?1,?2,?3)");
			if (!row) {
				return false;
			}
			sqlite3_bind_int(row->Value, 1, aPreferences.Enabled ? 1 : 0);
			sqlite3_bind_int(row->Value, 2, static_cast<int>(aPreferences.Detail));
			sqlite3_bind_int(row->Value, 3, static_cast<int>(aPreferences.Privacy));
			return sqlite3_step(row->Value) == SQLITE_DONE;
		} catch (...) {
			return false;
		}
	}
}
