#include "bpr/adapters/sqlite/release_update_recovery_store.hpp"
#include "sqlite_connection.hpp"
#include "storage_internal.hpp"
#include <sqlite3.h>

#include <array>
#include <mutex>

namespace bpr::adapters::sqlite {
	namespace {
		using draft                             = application::release_update_saved_draft;
		using saved_drafts                      = application::release_update_saved_drafts;
		constexpr auto             unavailable  = application::release_update_recovery_failure::unavailable;
		constexpr std::string_view draft_schema = "CREATE TABLE drafts(sequence INTEGER PRIMARY KEY,identity TEXT NOT NULL,description TEXT NOT NULL,body TEXT NOT NULL,indeterminate INTEGER NOT NULL CHECK(indeterminate IN(0,1)),UNIQUE(identity,description,body,indeterminate)) STRICT";
		class recovery_store final : public application::release_update_recovery_port
		{
		public:
			explicit recovery_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			~recovery_store() override
			{
				if (Database) {
					sqlite3_close(Database);
				}
			}
			auto load() -> saved_drafts override
			{
				const std::scoped_lock lock{ Mutex };
				return open() ? read() : std::unexpected(unavailable);
			}
			auto save(const std::vector<draft>& aDrafts) -> bool override
			{
				const std::scoped_lock lock{ Mutex };
				if (!open() || aDrafts.size() > application::release_update_maximum_saved_drafts
					|| !detail::execute(Database, "BEGIN IMMEDIATE")) {
					return false;
				}
				bool saved = read().has_value();
				for (const auto& entry : aDrafts) {
					if (!saved || entry.Identity.empty() || entry.Identity.size() > 256U || entry.Description.size() > 4096U || entry.Body.size() > 65536U) {
						saved = false;
						break;
					}
					auto query = detail::prepare(Database, "INSERT OR IGNORE INTO drafts(identity,description,body,indeterminate) VALUES(?1,?2,?3,?4)");
					if (!query) {
						saved = false;
						break;
					}
					auto bind = [&](int aColumn, const std::string& aText) {
						return sqlite3_bind_text(query->Value, aColumn, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT) == SQLITE_OK;
					};
					saved = bind(1, entry.Identity) && bind(2, entry.Description) && bind(3, entry.Body)
						&& sqlite3_bind_int(query->Value, 4, entry.Indeterminate ? 1 : 0) == SQLITE_OK && sqlite3_step(query->Value) == SQLITE_DONE;
				}
				saved = saved && read().has_value();
				if (saved && detail::execute(Database, "COMMIT")) {
					return true;
				}
				static_cast<void>(detail::execute(Database, "ROLLBACK"));
				return false;
			}

		private:
			auto read() -> saved_drafts
			{
				auto query = detail::prepare(Database, "SELECT identity,description,body,indeterminate FROM drafts ORDER BY sequence");
				if (!query) {
					return std::unexpected(unavailable);
				}
				std::vector<draft> result;
				std::size_t        total  = 0;
				int                status = SQLITE_ROW;
				while ((status = sqlite3_step(query->Value)) == SQLITE_ROW) {
					if (result.size() >= application::release_update_maximum_saved_drafts) {
						return std::unexpected(unavailable);
					}
					draft                             entry;
					const std::array<std::string*, 3> fields{ &entry.Identity, &entry.Description, &entry.Body };
					for (std::size_t index = 0; index < fields.size(); ++index) {
						const auto length = sqlite3_column_bytes(query->Value, static_cast<int>(index));
						const auto limit  = index == 0U ? 256 : index == 1U ? 4096
																			: 65536;
						if (sqlite3_column_type(query->Value, static_cast<int>(index)) != SQLITE_TEXT || length < 0 || length > limit) {
							return std::unexpected(unavailable);
						}
						const auto* bytes = reinterpret_cast<const char*>(sqlite3_column_text(query->Value, static_cast<int>(index)));
						if (!bytes && length != 0) {
							return std::unexpected(unavailable);
						}
						*fields[index] = bytes ? std::string{ bytes, static_cast<std::size_t>(length) } : std::string{ };
						total += static_cast<std::size_t>(length);
					}
					const auto flag = sqlite3_column_int(query->Value, 3);
					if (entry.Identity.empty() || total > application::release_update_maximum_saved_bytes
						|| sqlite3_column_type(query->Value, 3) != SQLITE_INTEGER || (flag != 0 && flag != 1)) {
						return std::unexpected(unavailable);
					}
					entry.Indeterminate = flag == 1;
					result.push_back(std::move(entry));
				}
				return status == SQLITE_DONE ? saved_drafts{ std::move(result) } : std::unexpected(unavailable);
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
				auto database = detail::open_private_store_database(*directory, "update-draft-recovery.sqlite3", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
				if (!database) {
					return false;
				}
				Database            = *database;
				Directory           = std::make_unique<detail::state_directory>(std::move(*directory));
				const auto identity = detail::scalar_integer(Database, "PRAGMA application_id"), version = detail::scalar_integer(Database, "PRAGMA user_version"), tables = detail::scalar_integer(Database, "SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'");
				const bool fresh = identity && version && tables && *identity == 0 && *version == 0 && *tables == 0;
				bool       valid = identity && version && (fresh || (*identity == 1112560214 && *version == 1));
				if (valid && !fresh) {
					const auto schema = detail::scalar_text(Database, "SELECT sql FROM sqlite_schema WHERE type='table' AND name='drafts'");
					valid             = tables && *tables == 1 && schema && detail::canonical_sql(*schema) == detail::canonical_sql(draft_schema);
				}
				if (valid) {
					valid = detail::configure_connection(Database).has_value();
				}
				if (valid && fresh) {
					valid = detail::execute(Database, "BEGIN IMMEDIATE").has_value()
						&& detail::execute(Database, draft_schema).has_value()
						&& detail::execute(Database, "PRAGMA application_id=1112560214").has_value()
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
		};
	}
	auto make_release_update_recovery_store(std::filesystem::path aRoot) -> std::shared_ptr<application::release_update_recovery_port>
	{
		return std::make_shared<recovery_store>(std::move(aRoot));
	}
}
