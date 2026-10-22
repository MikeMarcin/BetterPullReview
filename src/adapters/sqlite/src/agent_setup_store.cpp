#include "bpr/adapters/sqlite/agent_setup_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::adapters::sqlite {
	namespace {
		/// "BPRY": distinct from every other store's identity, so pointing this
		/// store at another's file fails at the identity check.
		constexpr std::int64_t agent_setup_application_id = 0x42505259;
		constexpr auto         agent_setup_file           = std::string_view{ "agent-setup.sqlite3" };
		/// No chosen agent.
		constexpr std::int64_t no_agent = -1;

		struct connection
		{
			detail::state_directory                               Directory;
			std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)> Database{ nullptr, sqlite3_close_v2 };
		};

		[[nodiscard]] auto open(const std::filesystem::path& aRoot) -> std::unique_ptr<connection>
		{
			if (!detail::sqlite_runtime_ready()) {
				return { };
			}
			auto directory = detail::open_state_directory(aRoot);
			if (!directory) {
				return { };
			}
			auto handle = detail::open_private_store_database(*directory, agent_setup_file,
															  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE | SQLITE_OPEN_NOFOLLOW);
			if (!handle) {
				return { };
			}
			auto        result = std::make_unique<connection>(connection{ std::move(*directory), { *handle, sqlite3_close_v2 } });
			auto* const db     = result->Database.get();
			if (!detail::configure_connection(db)) {
				return { };
			}
			const auto identity = detail::scalar_integer(db, "PRAGMA application_id");
			if (!identity || (*identity != 0 && *identity != agent_setup_application_id)) {
				return { };
			}
			if (!detail::execute(db, "PRAGMA application_id=" + std::to_string(agent_setup_application_id))
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_setup("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"version INTEGER NOT NULL CHECK(version=1),"
									"chosen INTEGER NOT NULL CHECK(chosen BETWEEN -1 AND 2),"
									"codex_protocol INTEGER NOT NULL CHECK(codex_protocol BETWEEN 0 AND 1),"
									"codex_program TEXT NOT NULL CHECK(length(CAST(codex_program AS BLOB)) <= 1024),"
									"harness_program TEXT NOT NULL CHECK(length(CAST(harness_program AS BLOB)) <= 1024)) STRICT")
				// [[BPR-PENDING-AQ3]]. The listed clone folders, in order, and each
				// repository's remembered clone or private copy.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS clone_folders("
									"position INTEGER PRIMARY KEY CHECK(position BETWEEN 0 AND 31),"
									"path TEXT NOT NULL CHECK(length(CAST(path AS BLOB)) BETWEEN 1 AND 1024)) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS repository_links("
									"host TEXT NOT NULL CHECK(length(CAST(host AS BLOB)) BETWEEN 1 AND 1024),"
									"repository INTEGER NOT NULL CHECK(repository >= 0),"
									"owner TEXT NOT NULL CHECK(length(CAST(owner AS BLOB)) BETWEEN 1 AND 1024),"
									"name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) BETWEEN 1 AND 1024),"
									"path TEXT NOT NULL CHECK(length(CAST(path AS BLOB)) <= 1024),"
									"remote TEXT NOT NULL CHECK(length(CAST(remote AS BLOB)) <= 1024),"
									"private_copy INTEGER NOT NULL CHECK(private_copy BETWEEN 0 AND 1),"
									"PRIMARY KEY(host, repository, owner, name)) STRICT")) {
				return { };
			}
			return result;
		}

		[[nodiscard]] auto column_text(sqlite3_stmt* const aRow, const int aColumn) -> std::optional<std::string>
		{
			if (sqlite3_column_type(aRow, aColumn) != SQLITE_TEXT) {
				return std::nullopt;
			}
			const auto* const bytes = sqlite3_column_text(aRow, aColumn);
			const auto        size  = sqlite3_column_bytes(aRow, aColumn);
			return std::string{ reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(size) };
		}

		/// The most repository links one profile keeps.
		constexpr std::int64_t maximum_repository_links = 1'000;

		void bind(sqlite3_stmt* const aRow, const int aIndex, const std::string& aText)
		{
			sqlite3_bind_text(aRow, aIndex, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT);
		}

		class clone_link_store final : public application::clone_link_store_port
		{
		public:
			explicit clone_link_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load_clone_folders() -> std::optional<std::vector<std::string>> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT path FROM clone_folders ORDER BY position");
					if (!row) {
						return std::nullopt;
					}
					auto folders = std::vector<std::string>{ };
					while (sqlite3_step(row->Value) == SQLITE_ROW) {
						if (auto path = column_text(row->Value, 0)) {
							folders.push_back(std::move(*path));
						}
					}
					return folders;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store_clone_folders(const std::span<const std::string> aFolders) -> bool override
			{
				if (aFolders.size() > core::maximum_clone_folders) {
					return false;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto* const db = store->Database.get();
					if (!detail::execute(db, "BEGIN IMMEDIATE")) {
						return false;
					}
					auto committed = detail::execute(db, "DELETE FROM clone_folders").has_value();
					for (std::size_t index = 0U; committed && index < aFolders.size(); ++index) {
						auto row  = detail::prepare(db, "INSERT INTO clone_folders VALUES(?1,?2)");
						committed = row.has_value();
						if (committed) {
							sqlite3_bind_int64(row->Value, 1, static_cast<std::int64_t>(index));
							bind(row->Value, 2, aFolders[index]);
							committed = sqlite3_step(row->Value) == SQLITE_DONE;
						}
					}
					if (!committed) {
						static_cast<void>(detail::execute(db, "ROLLBACK"));
						return false;
					}
					return detail::execute(db, "COMMIT").has_value();
				} catch (...) {
					return false;
				}
			}

			[[nodiscard]] auto load_links() -> std::optional<std::vector<application::repository_link>> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT host,repository,owner,name,path,remote,private_copy FROM repository_links ORDER BY host,owner,name");
					if (!row) {
						return std::nullopt;
					}
					auto links = std::vector<application::repository_link>{ };
					while (sqlite3_step(row->Value) == SQLITE_ROW) {
						auto host   = column_text(row->Value, 0);
						auto owner  = column_text(row->Value, 2);
						auto name   = column_text(row->Value, 3);
						auto path   = column_text(row->Value, 4);
						auto remote = column_text(row->Value, 5);
						if (!host || !owner || !name || !path || !remote || sqlite3_column_type(row->Value, 1) != SQLITE_INTEGER) {
							continue;
						}
						auto link        = application::repository_link{ };
						link.Host        = std::move(*host);
						link.Repository  = static_cast<std::uint64_t>(sqlite3_column_int64(row->Value, 1));
						link.Owner       = std::move(*owner);
						link.Name        = std::move(*name);
						link.Path        = std::move(*path);
						link.Remote      = std::move(*remote);
						link.PrivateCopy = sqlite3_column_int64(row->Value, 6) == 1;
						if (application::valid(link)) {
							links.push_back(std::move(link));
						}
					}
					return links;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store_link(const application::repository_link& aLink) -> bool override
			{
				if (!application::valid(aLink) || aLink.Repository > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					return false;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto* const db    = store->Database.get();
					const auto  count = detail::scalar_integer(db, "SELECT count(*) FROM repository_links");
					if (!count || *count >= maximum_repository_links) {
						return false;
					}
					auto row = detail::prepare(db, "INSERT OR REPLACE INTO repository_links VALUES(?1,?2,?3,?4,?5,?6,?7)");
					if (!row) {
						return false;
					}
					bind(row->Value, 1, aLink.Host);
					sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aLink.Repository));
					bind(row->Value, 3, aLink.Owner);
					bind(row->Value, 4, aLink.Name);
					bind(row->Value, 5, aLink.Path);
					bind(row->Value, 6, aLink.Remote);
					sqlite3_bind_int64(row->Value, 7, aLink.PrivateCopy ? 1 : 0);
					return sqlite3_step(row->Value) == SQLITE_DONE;
				} catch (...) {
					return false;
				}
			}

			[[nodiscard]] auto forget_link(const std::string& aHost, const std::uint64_t aRepository, const std::string& aOwner, const std::string& aName) -> bool override
			{
				try {
					auto store = open(Root);
					if (!store || aRepository > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
						return false;
					}
					auto row = detail::prepare(store->Database.get(), "DELETE FROM repository_links WHERE host=?1 AND repository=?2 AND owner=?3 AND name=?4");
					if (!row) {
						return false;
					}
					bind(row->Value, 1, aHost);
					sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aRepository));
					bind(row->Value, 3, aOwner);
					bind(row->Value, 4, aName);
					return sqlite3_step(row->Value) == SQLITE_DONE;
				} catch (...) {
					return false;
				}
			}

		private:
			std::filesystem::path Root;
		};

		class agent_setup_store final : public application::agent_setup_store_port
		{
		public:
			explicit agent_setup_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load() -> std::optional<application::agent_setup_preferences> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT chosen,codex_protocol,codex_program,harness_program FROM agent_setup WHERE singleton=1");
					if (!row) {
						return std::nullopt;
					}
					const auto code = sqlite3_step(row->Value);
					if (code == SQLITE_DONE) {
						return application::agent_setup_preferences{ };
					}
					if (code != SQLITE_ROW || sqlite3_column_type(row->Value, 0) != SQLITE_INTEGER || sqlite3_column_type(row->Value, 1) != SQLITE_INTEGER) {
						return application::agent_setup_preferences{ };
					}
					const auto chosen   = sqlite3_column_int64(row->Value, 0);
					const auto protocol = sqlite3_column_int64(row->Value, 1);
					auto       codex    = column_text(row->Value, 2);
					auto       harness  = column_text(row->Value, 3);
					auto       loaded   = application::agent_setup_preferences{ };
					if (chosen < no_agent || chosen > 2 || protocol < 0 || protocol > 1 || !codex || !harness) {
						return loaded;
					}
					if (chosen != no_agent) {
						loaded.Chosen = static_cast<core::setup_agent>(chosen);
					}
					loaded.CodexProtocol  = static_cast<core::codex_protocol_choice>(protocol);
					loaded.CodexProgram   = std::move(*codex);
					loaded.HarnessProgram = std::move(*harness);
					return application::valid(loaded) ? loaded : application::agent_setup_preferences{ };
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store(const application::agent_setup_preferences& aPreferences) -> bool override
			{
				if (!application::valid(aPreferences)) {
					return false;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto row = detail::prepare(store->Database.get(), "INSERT OR REPLACE INTO agent_setup VALUES(1,1,?1,?2,?3,?4)");
					if (!row) {
						return false;
					}
					sqlite3_bind_int64(row->Value, 1, aPreferences.Chosen ? static_cast<std::int64_t>(*aPreferences.Chosen) : no_agent);
					sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aPreferences.CodexProtocol));
					sqlite3_bind_text(row->Value, 3, aPreferences.CodexProgram.data(), static_cast<int>(aPreferences.CodexProgram.size()), SQLITE_TRANSIENT);
					sqlite3_bind_text(row->Value, 4, aPreferences.HarnessProgram.data(), static_cast<int>(aPreferences.HarnessProgram.size()), SQLITE_TRANSIENT);
					return sqlite3_step(row->Value) == SQLITE_DONE;
				} catch (...) {
					return false;
				}
			}

		private:
			std::filesystem::path Root;
		};
	} // namespace

	auto make_agent_setup_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_setup_store_port>
	{
		return std::make_unique<agent_setup_store>(aRoot);
	}

	auto make_clone_link_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::clone_link_store_port>
	{
		return std::make_unique<clone_link_store>(aRoot);
	}
} // namespace bpr::adapters::sqlite
