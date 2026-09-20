#include "bpr/adapters/sqlite/release_update_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"
#include <sqlite3.h>

#include <algorithm>
#include <mutex>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr std::int64_t     application_id = 0x42505255;
		constexpr std::string_view schema         = "CREATE TABLE updater(id INTEGER PRIMARY KEY CHECK(id=1),automatic INTEGER NOT NULL CHECK(automatic IN(0,1)),skipped TEXT NOT NULL,attempt INTEGER,success INTEGER,binding TEXT NOT NULL CHECK(length(binding)<=128)) STRICT";
		auto                       valid_binding(std::string_view aBinding) -> bool
		{
			return aBinding.empty() || (aBinding.size() == 64U && std::ranges::all_of(aBinding, [](char aValue) { return (aValue >= '0' && aValue <= '9') || (aValue >= 'a' && aValue <= 'f'); }));
		}
		auto text(sqlite3_stmt* aStatement, int aColumn) -> std::string
		{
			const auto  size  = sqlite3_column_bytes(aStatement, aColumn);
			const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(aStatement, aColumn));
			return value && size >= 0 && size <= 128 ? std::string{ value, static_cast<std::size_t>(size) } : std::string{ };
		}
		class update_store final : public application::release_update_settings_port
		{
		public:
			explicit update_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			~update_store() override
			{
				if (Database) {
					sqlite3_close(Database);
				}
			}
			auto load() -> std::expected<application::release_update_settings, core::release_update_reason> override
			{
				const std::scoped_lock lock{ Mutex };
				auto                   result = open() ? read() : std::unexpected(core::release_update_reason::unavailable);
				Healthy                       = result.has_value();
				return result;
			}
			auto save(const application::release_update_settings& aSettings) -> bool override
			{
				const std::scoped_lock lock{ Mutex };
				if (!Healthy || !valid_binding(aSettings.AccountBinding) || !open() || !read()) {
					return false;
				}
				auto statement = detail::prepare(Database, "INSERT OR REPLACE INTO updater VALUES(1,?1,?2,?3,?4,?5)");
				if (!statement) {
					return false;
				}
				const auto skipped  = aSettings.Preferences.SkippedVersion;
				const auto version  = skipped ? std::to_string(skipped->Major) + "." + std::to_string(skipped->Minor) + "." + std::to_string(skipped->Patch) : std::string{ };
				auto*      row      = statement->Value;
				auto       bindTime = [row](int aIndex, std::optional<std::int64_t> aTime) { return aTime ? sqlite3_bind_int64(row, aIndex, *aTime) : sqlite3_bind_null(row, aIndex); };
				return sqlite3_bind_int(row, 1, aSettings.Preferences.AutomaticChecks ? 1 : 0) == SQLITE_OK
					&& sqlite3_bind_text(row, 2, version.c_str(), static_cast<int>(version.size()), SQLITE_TRANSIENT) == SQLITE_OK
					&& bindTime(3, aSettings.Preferences.LastAttempt) == SQLITE_OK && bindTime(4, aSettings.Preferences.LastSuccess) == SQLITE_OK
					&& sqlite3_bind_text(row, 5, aSettings.AccountBinding.c_str(), static_cast<int>(aSettings.AccountBinding.size()), SQLITE_TRANSIENT) == SQLITE_OK
					&& sqlite3_step(row) == SQLITE_DONE;
			}

		private:
			auto read() -> std::expected<application::release_update_settings, core::release_update_reason>
			{
				auto statement = detail::prepare(Database, "SELECT automatic,skipped,attempt,success,binding FROM updater WHERE id=1");
				if (!statement) {
					return std::unexpected(core::release_update_reason::unavailable);
				}
				auto*      row    = statement->Value;
				const auto result = sqlite3_step(row);
				if (result == SQLITE_DONE) {
					return application::release_update_settings{ };
				}
				if (result != SQLITE_ROW) {
					return std::unexpected(core::release_update_reason::unavailable);
				}
				application::release_update_settings value;
				const auto                           automatic = sqlite3_column_int(row, 0);
				const auto                           skipped   = text(row, 1);
				value.AccountBinding                           = text(row, 4);
				if (sqlite3_column_type(row, 0) != SQLITE_INTEGER || (automatic != 0 && automatic != 1)
					|| sqlite3_column_type(row, 1) != SQLITE_TEXT || sqlite3_column_type(row, 4) != SQLITE_TEXT
					|| (sqlite3_column_type(row, 2) != SQLITE_NULL && sqlite3_column_type(row, 2) != SQLITE_INTEGER)
					|| (sqlite3_column_type(row, 3) != SQLITE_NULL && sqlite3_column_type(row, 3) != SQLITE_INTEGER)
					|| sqlite3_column_bytes(row, 1) > 128 || sqlite3_column_bytes(row, 4) > 128 || !valid_binding(value.AccountBinding)) {
					return std::unexpected(core::release_update_reason::unavailable);
				}
				value.Preferences.AutomaticChecks = automatic == 1;
				if (!skipped.empty()) {
					value.Preferences.SkippedVersion = core::parse_release_update_version(skipped);
					if (!value.Preferences.SkippedVersion) {
						return std::unexpected(core::release_update_reason::unavailable);
					}
				}
				if (sqlite3_column_type(row, 2) != SQLITE_NULL) {
					value.Preferences.LastAttempt = sqlite3_column_int64(row, 2);
				}
				if (sqlite3_column_type(row, 3) != SQLITE_NULL) {
					value.Preferences.LastSuccess = sqlite3_column_int64(row, 3);
				}
				return value;
			}
			auto open() -> bool
			{
				if (Database) {
					return true;
				}
				if (!detail::sqlite_runtime_ready()) {
					return false;
				}
				auto directory = detail::open_state_directory(Root);
				if (!directory) {
					return false;
				}
				auto database = detail::open_private_store_database(*directory, "release-update.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
				if (!database) {
					return false;
				}
				Database            = *database;
				Directory           = std::make_unique<detail::state_directory>(std::move(*directory));
				const auto identity = detail::scalar_integer(Database, "PRAGMA application_id");
				const auto version  = detail::scalar_integer(Database, "PRAGMA user_version");
				const auto tables   = detail::scalar_integer(Database, "SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'");
				const bool fresh    = identity && version && tables && *identity == 0 && *version == 0 && *tables == 0;
				bool       valid    = identity && version && (fresh || (*identity == application_id && *version == 1));
				if (valid) {
					valid = detail::configure_connection(Database).has_value();
				}
				if (valid && fresh) {
					valid = detail::execute(Database, "BEGIN IMMEDIATE").has_value() && detail::execute(Database, schema).has_value()
						&& detail::execute(Database, "PRAGMA application_id=1112560213").has_value()
						&& detail::execute(Database, "PRAGMA user_version=1").has_value() && detail::execute(Database, "COMMIT").has_value();
				}
				if (!valid) {
					sqlite3_close(Database);
					Database = nullptr;
					Directory.reset();
				}
				return valid;
			}
			std::filesystem::path                    Root;
			std::unique_ptr<detail::state_directory> Directory;
			sqlite3*                                 Database = nullptr;
			std::mutex                               Mutex;
			bool                                     Healthy = true;
		};
	}
	auto make_release_update_store(std::filesystem::path aRoot) -> std::shared_ptr<application::release_update_settings_port>
	{
		return std::make_shared<update_store>(std::move(aRoot));
	}
}
