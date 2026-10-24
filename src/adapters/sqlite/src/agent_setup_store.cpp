#include "bpr/adapters/sqlite/agent_setup_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <sqlite3.h>

#include <algorithm>
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
				// [[BPR-1750]]. The listed clone folders, in order, and each
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
									"PRIMARY KEY(host, repository, owner, name)) STRICT")
				// [[BPR-1753]]. The model and effort each agent runs with.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_runtime("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"claude_effort TEXT NOT NULL CHECK(length(CAST(claude_effort AS BLOB)) <= 16),"
									"codex_model TEXT NOT NULL CHECK(length(CAST(codex_model AS BLOB)) <= 128),"
									"codex_effort TEXT NOT NULL CHECK(length(CAST(codex_effort AS BLOB)) <= 16)) STRICT")
				// [[BPR-1752]]. The profile's edited review skill.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS review_skill("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"text TEXT NOT NULL CHECK(length(CAST(text AS BLOB)) BETWEEN 1 AND 65536),"
									"base_version INTEGER NOT NULL CHECK(base_version BETWEEN 0 AND 4294967295),"
									"base_digest BLOB NOT NULL CHECK(length(base_digest)=32),"
									"kept_version INTEGER CHECK(kept_version IS NULL OR kept_version BETWEEN 0 AND 4294967295)) STRICT")
				// [[BPR-1753]], owner amendment 2026-10-23. The agents the
				// reviewer added, in order, and whether the single settings above
				// were moved into them.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_entries("
									"position INTEGER PRIMARY KEY CHECK(position BETWEEN 0 AND 15),"
									"id INTEGER NOT NULL UNIQUE CHECK(id BETWEEN 1 AND 64),"
									"name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) BETWEEN 1 AND 64),"
									"protocol INTEGER NOT NULL CHECK(protocol BETWEEN 0 AND 3),"
									"program TEXT NOT NULL CHECK(length(CAST(program AS BLOB)) <= 1024),"
									"arguments TEXT NOT NULL CHECK(length(CAST(arguments AS BLOB)) <= 4200),"
									"model TEXT NOT NULL CHECK(length(CAST(model AS BLOB)) <= 128),"
									"effort TEXT NOT NULL CHECK(length(CAST(effort AS BLOB)) <= 16),"
									"preset INTEGER NOT NULL CHECK(preset BETWEEN -1 AND 1),"
									"check_arguments TEXT NOT NULL CHECK(length(CAST(check_arguments AS BLOB)) <= 4200)) STRICT")
				// Owner decision 2026-10-23: the run time limit; none means off.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_run_limit("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"minutes INTEGER NOT NULL CHECK(minutes BETWEEN 0 AND 1440)) STRICT")
				// Owner look v004: whether the Agent tab's suggestions are hidden.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_suggestions("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"hidden INTEGER NOT NULL CHECK(hidden IN (0,1))) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_list("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"moved INTEGER NOT NULL CHECK(moved BETWEEN 0 AND 1),"
									"preferred INTEGER NOT NULL CHECK(preferred BETWEEN 0 AND 64)) STRICT")
				// [[BPR-1753]], owner decision (b). Each pull request's
				// conversation: when it was last written, and its turns in order.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_conversations("
									"host TEXT NOT NULL CHECK(length(CAST(host AS BLOB)) BETWEEN 1 AND 1024),"
									"owner TEXT NOT NULL CHECK(length(CAST(owner AS BLOB)) BETWEEN 1 AND 1024),"
									"name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) BETWEEN 1 AND 1024),"
									"number INTEGER NOT NULL CHECK(number >= 1),"
									"agent INTEGER NOT NULL CHECK(agent BETWEEN 0 AND 64),"
									"written INTEGER NOT NULL CHECK(written >= 0),"
									"PRIMARY KEY(host, owner, name, number, agent)) STRICT")
				// Owner decision 2026-10-23: the agent a pull request was last
				// asked with, so it opens on that agent's own conversation.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_conversation_choice("
									"host TEXT NOT NULL,"
									"owner TEXT NOT NULL,"
									"name TEXT NOT NULL,"
									"number INTEGER NOT NULL,"
									"agent INTEGER NOT NULL CHECK(agent BETWEEN 0 AND 64),"
									"PRIMARY KEY(host, owner, name, number)) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_conversation_turns("
									"host TEXT NOT NULL,"
									"owner TEXT NOT NULL,"
									"name TEXT NOT NULL,"
									"number INTEGER NOT NULL,"
									"agent INTEGER NOT NULL,"
									"position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 49),"
									"asked TEXT NOT NULL CHECK(length(CAST(asked AS BLOB)) <= 65536),"
									"answered TEXT NOT NULL CHECK(length(CAST(answered AS BLOB)) <= 65536),"
									"ending TEXT NOT NULL CHECK(length(CAST(ending AS BLOB)) <= 65536),"
									"PRIMARY KEY(host, owner, name, number, agent, position)) STRICT")) {
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

		/// [[BPR-1753]]. An entry's fixed arguments, one per line: an
		/// argument holds no control character, so a line break separates them.
		[[nodiscard]] auto join_arguments(const std::vector<std::string>& aArguments) -> std::string
		{
			auto joined = std::string{ };
			for (const auto& argument : aArguments) {
				if (!joined.empty()) {
					joined += '\n';
				}
				joined += argument;
			}
			return joined;
		}

		[[nodiscard]] auto split_arguments(const std::string& aJoined) -> std::vector<std::string>
		{
			auto arguments = std::vector<std::string>{ };
			if (aJoined.empty()) {
				return arguments;
			}
			auto begin = std::size_t{ 0U };
			while (true) {
				const auto end = aJoined.find('\n', begin);
				arguments.push_back(aJoined.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
				if (end == std::string::npos) {
					break;
				}
				begin = end + 1U;
			}
			return arguments;
		}

		/// The agent list, replaced whole in one transaction.
		[[nodiscard]] auto store_agents(sqlite3* const aDatabase, const application::agent_setup_preferences& aPreferences) -> bool
		{
			if (!detail::execute(aDatabase, "BEGIN IMMEDIATE")) {
				return false;
			}
			auto committed = detail::execute(aDatabase, "DELETE FROM agent_entries").has_value();
			for (std::size_t index = 0U; committed && index < aPreferences.Agents.size(); ++index) {
				const auto& entry = aPreferences.Agents[index];
				auto        row   = detail::prepare(aDatabase, "INSERT INTO agent_entries VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)");
				committed         = row.has_value();
				if (committed) {
					sqlite3_bind_int64(row->Value, 1, static_cast<std::int64_t>(index));
					sqlite3_bind_int64(row->Value, 2, entry.Id);
					bind(row->Value, 3, entry.Name);
					sqlite3_bind_int64(row->Value, 4, static_cast<std::int64_t>(entry.Protocol));
					bind(row->Value, 5, entry.Program);
					const auto arguments = join_arguments(entry.Arguments);
					bind(row->Value, 6, arguments);
					bind(row->Value, 7, entry.Model);
					bind(row->Value, 8, entry.Effort);
					sqlite3_bind_int64(row->Value, 9, entry.Preset ? static_cast<std::int64_t>(*entry.Preset) : -1);
					const auto check = join_arguments(entry.CheckArguments);
					bind(row->Value, 10, check);
					committed = sqlite3_step(row->Value) == SQLITE_DONE;
				}
			}
			if (committed) {
				auto limit = detail::prepare(aDatabase, "INSERT OR REPLACE INTO agent_run_limit VALUES(1,?1)");
				committed  = limit.has_value();
				if (committed) {
					sqlite3_bind_int64(limit->Value, 1, aPreferences.RunLimitMinutes);
					committed = sqlite3_step(limit->Value) == SQLITE_DONE;
				}
			}
			if (committed) {
				auto hidden = detail::prepare(aDatabase, "INSERT OR REPLACE INTO agent_suggestions VALUES(1,?1)");
				committed   = hidden.has_value();
				if (committed) {
					sqlite3_bind_int64(hidden->Value, 1, aPreferences.SuggestionsHidden ? 1 : 0);
					committed = sqlite3_step(hidden->Value) == SQLITE_DONE;
				}
			}
			if (committed) {
				auto list = detail::prepare(aDatabase, "INSERT OR REPLACE INTO agent_list VALUES(1,?1,?2)");
				committed = list.has_value();
				if (committed) {
					sqlite3_bind_int64(list->Value, 1, aPreferences.AgentsMoved ? 1 : 0);
					sqlite3_bind_int64(list->Value, 2, aPreferences.PreferredAgent);
					committed = sqlite3_step(list->Value) == SQLITE_DONE;
				}
			}
			if (!committed) {
				static_cast<void>(detail::execute(aDatabase, "ROLLBACK"));
				return false;
			}
			return detail::execute(aDatabase, "COMMIT").has_value();
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

		class review_skill_store final : public application::review_skill_store_port
		{
		public:
			explicit review_skill_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load_review_skill() -> std::optional<std::optional<application::stored_review_skill>> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT text,base_version,base_digest,kept_version FROM review_skill WHERE singleton=1");
					if (!row) {
						return std::nullopt;
					}
					auto none = std::optional<std::optional<application::stored_review_skill>>{ std::in_place, std::nullopt };
					if (sqlite3_step(row->Value) != SQLITE_ROW) {
						return none;
					}
					auto text = column_text(row->Value, 0);
					if (!text || text->size() > core::maximum_review_skill_bytes || sqlite3_column_type(row->Value, 1) != SQLITE_INTEGER
						|| sqlite3_column_type(row->Value, 2) != SQLITE_BLOB || sqlite3_column_bytes(row->Value, 2) != 32) {
						return none;
					}
					auto skill               = application::stored_review_skill{ .Text = std::move(*text) };
					skill.Record.BaseVersion = static_cast<std::uint32_t>(sqlite3_column_int64(row->Value, 1));
					const auto* const digest = static_cast<const std::uint8_t*>(sqlite3_column_blob(row->Value, 2));
					std::copy_n(digest, 32, skill.Record.BaseDigest.begin());
					if (sqlite3_column_type(row->Value, 3) == SQLITE_INTEGER) {
						skill.Record.KeptVersion = static_cast<std::uint32_t>(sqlite3_column_int64(row->Value, 3));
					}
					return std::optional{ std::optional{ std::move(skill) } };
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store_review_skill(const application::stored_review_skill& aSkill) -> bool override
			{
				if (aSkill.Text.empty() || aSkill.Text.size() > core::maximum_review_skill_bytes) {
					return false;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto row = detail::prepare(store->Database.get(), "INSERT OR REPLACE INTO review_skill VALUES(1,?1,?2,?3,?4)");
					if (!row) {
						return false;
					}
					bind(row->Value, 1, aSkill.Text);
					sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aSkill.Record.BaseVersion));
					sqlite3_bind_blob(row->Value, 3, aSkill.Record.BaseDigest.data(), static_cast<int>(aSkill.Record.BaseDigest.size()), SQLITE_TRANSIENT);
					if (aSkill.Record.KeptVersion) {
						sqlite3_bind_int64(row->Value, 4, static_cast<std::int64_t>(*aSkill.Record.KeptVersion));
					} else {
						sqlite3_bind_null(row->Value, 4);
					}
					return sqlite3_step(row->Value) == SQLITE_DONE;
				} catch (...) {
					return false;
				}
			}

			[[nodiscard]] auto forget_review_skill() -> bool override
			{
				try {
					auto store = open(Root);
					return store && detail::execute(store->Database.get(), "DELETE FROM review_skill").has_value();
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
					// [[BPR-1753]]. The run settings, when any were saved.
					if (auto runtime = detail::prepare(store->Database.get(), "SELECT claude_effort,codex_model,codex_effort FROM agent_runtime WHERE singleton=1");
						runtime && sqlite3_step(runtime->Value) == SQLITE_ROW) {
						loaded.ClaudeEffort = column_text(runtime->Value, 0).value_or(std::string{ });
						loaded.CodexModel   = column_text(runtime->Value, 1).value_or(std::string{ });
						loaded.CodexEffort  = column_text(runtime->Value, 2).value_or(std::string{ });
					}
					// [[BPR-1753]]. The agent list, when one was kept; a
					// list that does not read back whole is none.
					if (auto list = detail::prepare(store->Database.get(), "SELECT moved,preferred FROM agent_list WHERE singleton=1"); list && sqlite3_step(list->Value) == SQLITE_ROW) {
						loaded.AgentsMoved    = sqlite3_column_int64(list->Value, 0) == 1;
						loaded.PreferredAgent = static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(list->Value, 1), 0, 64));
					}
					if (auto rows = detail::prepare(store->Database.get(), "SELECT id,name,protocol,program,arguments,model,effort,preset,check_arguments FROM agent_entries ORDER BY position"); rows) {
						while (sqlite3_step(rows->Value) == SQLITE_ROW) {
							auto entry      = core::agent_entry{ };
							entry.Id        = static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(rows->Value, 0), 0, 64));
							entry.Name      = column_text(rows->Value, 1).value_or(std::string{ });
							entry.Protocol  = static_cast<core::agent_protocol>(std::clamp<std::int64_t>(sqlite3_column_int64(rows->Value, 2), 0, 3));
							entry.Program   = column_text(rows->Value, 3).value_or(std::string{ });
							entry.Arguments = split_arguments(column_text(rows->Value, 4).value_or(std::string{ }));
							entry.Model     = column_text(rows->Value, 5).value_or(std::string{ });
							entry.Effort    = column_text(rows->Value, 6).value_or(std::string{ });
							if (const auto preset = sqlite3_column_int64(rows->Value, 7); preset >= 0) {
								entry.Preset = static_cast<core::setup_agent>(preset);
							}
							entry.CheckArguments = split_arguments(column_text(rows->Value, 8).value_or(std::string{ }));
							loaded.Agents.push_back(std::move(entry));
						}
					}
					if (core::inspect_agent_entries(loaded.Agents)) {
						loaded.Agents.clear();
					}
					if (auto limit = detail::prepare(store->Database.get(), "SELECT minutes FROM agent_run_limit WHERE singleton=1"); limit && sqlite3_step(limit->Value) == SQLITE_ROW) {
						loaded.RunLimitMinutes = static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(limit->Value, 0), 0, 1440));
					}
					if (auto hidden = detail::prepare(store->Database.get(), "SELECT hidden FROM agent_suggestions WHERE singleton=1"); hidden && sqlite3_step(hidden->Value) == SQLITE_ROW) {
						loaded.SuggestionsHidden = sqlite3_column_int64(hidden->Value, 0) != 0;
					}
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
					if (sqlite3_step(row->Value) != SQLITE_DONE) {
						return false;
					}
					auto runtime = detail::prepare(store->Database.get(), "INSERT OR REPLACE INTO agent_runtime VALUES(1,?1,?2,?3)");
					if (!runtime) {
						return false;
					}
					bind(runtime->Value, 1, aPreferences.ClaudeEffort);
					bind(runtime->Value, 2, aPreferences.CodexModel);
					bind(runtime->Value, 3, aPreferences.CodexEffort);
					if (sqlite3_step(runtime->Value) != SQLITE_DONE) {
						return false;
					}
					return store_agents(store->Database.get(), aPreferences);
				} catch (...) {
					return false;
				}
			}

		private:
			std::filesystem::path Root;
		};
		/// [[BPR-1753]], owner decision (b) 2026-10-23. Each pull
		/// request's conversation, so it survives closing the pull request and
		/// the application; Clear session stores none.
		class agent_conversation_store final : public application::agent_conversation_store_port
		{
		public:
			explicit agent_conversation_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load(const application::agent_conversation_key& aKey) -> std::optional<std::vector<application::agent_conversation_turn>> override
			{
				if (!application::valid(aKey) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					return std::nullopt;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT asked,answered,ending FROM agent_conversation_turns WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5 ORDER BY position");
					if (!row) {
						return std::nullopt;
					}
					bind_key(row->Value, aKey);
					auto turns = std::vector<application::agent_conversation_turn>{ };
					while (sqlite3_step(row->Value) == SQLITE_ROW) {
						auto asked    = column_text(row->Value, 0);
						auto answered = column_text(row->Value, 1);
						auto ending   = column_text(row->Value, 2);
						if (!asked || !answered || !ending) {
							return std::vector<application::agent_conversation_turn>{ };
						}
						turns.push_back({ .Asked = std::move(*asked), .Answered = std::move(*answered), .Ending = std::move(*ending) });
					}
					// A conversation that does not read back whole is none.
					return application::valid(turns) ? turns : std::vector<application::agent_conversation_turn>{ };
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store(const application::agent_conversation_key& aKey, const std::vector<application::agent_conversation_turn>& aTurns) -> bool override
			{
				if (!application::valid(aKey) || !application::valid(aTurns) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
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
					const auto run = [&](const char* aSql, auto&& aBind) {
						auto row = detail::prepare(db, aSql);
						if (!row) {
							return false;
						}
						aBind(row->Value);
						return sqlite3_step(row->Value) == SQLITE_DONE;
					};
					const auto key       = [&](sqlite3_stmt* aRow) { bind_key(aRow, aKey); };
					auto       committed = run("DELETE FROM agent_conversation_turns WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5", key)
						&& run("DELETE FROM agent_conversations WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5", key);
					if (committed && !aTurns.empty()) {
						committed = run("INSERT INTO agent_conversations SELECT ?1,?2,?3,?4,?5,coalesce(max(written)+1,0) FROM agent_conversations", key);
						for (std::size_t index = 0U; committed && index < aTurns.size(); ++index) {
							committed = run("INSERT INTO agent_conversation_turns VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)", [&](sqlite3_stmt* aRow) {
								bind_key(aRow, aKey);
								sqlite3_bind_int64(aRow, 6, static_cast<std::int64_t>(index));
								bind(aRow, 7, aTurns[index].Asked);
								bind(aRow, 8, aTurns[index].Answered);
								bind(aRow, 9, aTurns[index].Ending);
							});
						}
						// The least recently written go past the bound.
						committed = committed
							&& detail::execute(db,
											   "DELETE FROM agent_conversation_turns WHERE (host,owner,name,number,agent) IN (SELECT host,owner,name,number,agent FROM agent_conversations "
											   "ORDER BY written DESC LIMIT -1 OFFSET "
												   + std::to_string(application::maximum_conversations) + ")")
								   .has_value()
							&& detail::execute(db,
											   "DELETE FROM agent_conversations WHERE (host,owner,name,number,agent) IN (SELECT host,owner,name,number,agent FROM agent_conversations "
											   "ORDER BY written DESC LIMIT -1 OFFSET "
												   + std::to_string(application::maximum_conversations) + ")")
								   .has_value();
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

		private:
			static void bind_key(sqlite3_stmt* const aRow, const application::agent_conversation_key& aKey)
			{
				bind(aRow, 1, aKey.Host);
				bind(aRow, 2, aKey.Owner);
				bind(aRow, 3, aKey.Name);
				sqlite3_bind_int64(aRow, 4, static_cast<std::int64_t>(aKey.Number));
				sqlite3_bind_int64(aRow, 5, static_cast<std::int64_t>(aKey.Agent));
			}

		public:
			[[nodiscard]] auto load_agent(const application::agent_conversation_key& aKey) -> std::optional<std::uint32_t> override
			{
				if (!application::valid(aKey) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					return std::nullopt;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT agent FROM agent_conversation_choice WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4");
					if (!row) {
						return std::nullopt;
					}
					bind_key(row->Value, aKey);
					if (sqlite3_step(row->Value) != SQLITE_ROW) {
						return std::nullopt;
					}
					return static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(row->Value, 0), 0, 64));
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store_agent(const application::agent_conversation_key& aKey) -> bool override
			{
				if (!application::valid(aKey) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					return false;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto row = detail::prepare(store->Database.get(), "INSERT OR REPLACE INTO agent_conversation_choice VALUES(?1,?2,?3,?4,?5)");
					if (!row) {
						return false;
					}
					bind_key(row->Value, aKey);
					return sqlite3_step(row->Value) == SQLITE_DONE;
				} catch (...) {
					return false;
				}
			}

			[[nodiscard]] auto forget_agents() -> bool override
			{
				try {
					auto store = open(Root);
					return store && detail::execute(store->Database.get(), "DELETE FROM agent_conversation_choice");
				} catch (...) {
					return false;
				}
			}

		private:
			std::filesystem::path Root;
		};
	} // namespace

	auto make_agent_conversation_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_conversation_store_port>
	{
		return std::make_unique<agent_conversation_store>(aRoot);
	}

	auto make_agent_setup_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_setup_store_port>
	{
		return std::make_unique<agent_setup_store>(aRoot);
	}

	auto make_clone_link_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::clone_link_store_port>
	{
		return std::make_unique<clone_link_store>(aRoot);
	}

	auto make_review_skill_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::review_skill_store_port>
	{
		return std::make_unique<review_skill_store>(aRoot);
	}
} // namespace bpr::adapters::sqlite
