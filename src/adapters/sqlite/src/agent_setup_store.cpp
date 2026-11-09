#include "bpr/adapters/sqlite/agent_setup_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <sqlite3.h>

#include "bpr/core/agent_turn.hpp"
#include "bpr/core/review_composition.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::adapters::sqlite {
	// [[BPR-1847]]. The outcome column's check names every declared outcome.
	static_assert(core::agent_turn_outcome_count == 10U, "agent_conversation_outcomes' CHECK must cover every turn outcome");

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
				// [[BPR-1918]]. The custom agents whose strict run checks are
				// off, by entry id; an entry with no row keeps them on.
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS agent_entry_identity(id INTEGER PRIMARY KEY, identity TEXT NOT NULL CHECK(length(identity)=32)) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS native_agent_sessions("
									"id TEXT PRIMARY KEY, scope TEXT NOT NULL, configuration TEXT NOT NULL, base TEXT NOT NULL, head TEXT NOT NULL,"
									"left_epoch INTEGER NOT NULL, right_epoch INTEGER NOT NULL, handle TEXT NOT NULL, turn INTEGER NOT NULL, pending INTEGER NOT NULL,"
									"workspace TEXT NOT NULL, parent TEXT NOT NULL, marker TEXT NOT NULL, application INTEGER NOT NULL, session INTEGER NOT NULL,"
									"directory BLOB NOT NULL, guard TEXT NOT NULL, guard_identity BLOB NOT NULL, shallow INTEGER NOT NULL, revision INTEGER NOT NULL) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_turns(conversation TEXT NOT NULL REFERENCES native_agent_sessions(id), ordinal INTEGER NOT NULL, asked TEXT NOT NULL, answered TEXT NOT NULL, ending TEXT NOT NULL, outcome INTEGER NOT NULL, PRIMARY KEY(conversation,ordinal)) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_selection(scope TEXT PRIMARY KEY, conversation TEXT NOT NULL REFERENCES native_agent_sessions(id)) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_entry_relaxed_checks("
									"id INTEGER PRIMARY KEY CHECK(id BETWEEN 1 AND 64)) STRICT")
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
									"PRIMARY KEY(host, owner, name, number, agent, position)) STRICT")
				// [[BPR-1847]]. How each turn ended, beside it: a turn stored
				// before this table has no row and reads back from its ending.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_conversation_outcomes("
									"host TEXT NOT NULL,"
									"owner TEXT NOT NULL,"
									"name TEXT NOT NULL,"
									"number INTEGER NOT NULL,"
									"agent INTEGER NOT NULL,"
									"position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 49),"
									"outcome INTEGER NOT NULL CHECK(outcome BETWEEN 0 AND 9),"
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

		void bind_text(sqlite3_stmt* const aRow, const int aIndex, const std::string& aText)
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
			auto committed = detail::execute(aDatabase, "DELETE FROM agent_entries").has_value() && detail::execute(aDatabase, "DELETE FROM agent_entry_identity").has_value();
			for (std::size_t index = 0U; committed && index < aPreferences.Agents.size(); ++index) {
				const auto& entry = aPreferences.Agents[index];
				auto        row   = detail::prepare(aDatabase, "INSERT INTO agent_entries VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)");
				committed         = row.has_value();
				if (committed) {
					sqlite3_bind_int64(row->Value, 1, static_cast<std::int64_t>(index));
					sqlite3_bind_int64(row->Value, 2, entry.Id);
					bind_text(row->Value, 3, entry.Name);
					sqlite3_bind_int64(row->Value, 4, static_cast<std::int64_t>(entry.Protocol));
					bind_text(row->Value, 5, entry.Program);
					const auto arguments = join_arguments(entry.Arguments);
					bind_text(row->Value, 6, arguments);
					bind_text(row->Value, 7, entry.Model);
					bind_text(row->Value, 8, entry.Effort);
					sqlite3_bind_int64(row->Value, 9, entry.Preset ? static_cast<std::int64_t>(*entry.Preset) : -1);
					const auto check = join_arguments(entry.CheckArguments);
					bind_text(row->Value, 10, check);
					committed = sqlite3_step(row->Value) == SQLITE_DONE;
					if (committed && !entry.Identity.empty()) {
						auto identity = detail::prepare(aDatabase, "INSERT INTO agent_entry_identity VALUES(?1,?2)");
						committed     = identity.has_value();
						if (committed) {
							sqlite3_bind_int64(identity->Value, 1, entry.Id);
							bind_text(identity->Value, 2, entry.Identity);
							committed = sqlite3_step(identity->Value) == SQLITE_DONE;
						}
					}
				}
			}
			if (committed) {
				committed = detail::execute(aDatabase, "DELETE FROM agent_entry_relaxed_checks").has_value();
			}
			for (std::size_t index = 0U; committed && index < aPreferences.Agents.size(); ++index) {
				if (const auto& entry = aPreferences.Agents[index]; !entry.StrictRunChecks) {
					auto row  = detail::prepare(aDatabase, "INSERT INTO agent_entry_relaxed_checks VALUES(?1)");
					committed = row.has_value();
					if (committed) {
						sqlite3_bind_int64(row->Value, 1, entry.Id);
						committed = sqlite3_step(row->Value) == SQLITE_DONE;
					}
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
							bind_text(row->Value, 2, aFolders[index]);
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
					bind_text(row->Value, 1, aLink.Host);
					sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aLink.Repository));
					bind_text(row->Value, 3, aLink.Owner);
					bind_text(row->Value, 4, aLink.Name);
					bind_text(row->Value, 5, aLink.Path);
					bind_text(row->Value, 6, aLink.Remote);
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
					bind_text(row->Value, 1, aHost);
					sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aRepository));
					bind_text(row->Value, 3, aOwner);
					bind_text(row->Value, 4, aName);
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
					bind_text(row->Value, 1, aSkill.Text);
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
							auto identity        = detail::prepare(store->Database.get(), "SELECT identity FROM agent_entry_identity WHERE id=?1");
							if (identity) {
								sqlite3_bind_int64(identity->Value, 1, entry.Id);
								if (sqlite3_step(identity->Value) == SQLITE_ROW) {
									entry.Identity = column_text(identity->Value, 0).value_or(std::string{ });
								}
							}
							loaded.Agents.push_back(std::move(entry));
						}
					}
					if (auto relaxed = detail::prepare(store->Database.get(), "SELECT id FROM agent_entry_relaxed_checks"); relaxed) {
						while (sqlite3_step(relaxed->Value) == SQLITE_ROW) {
							const auto id = sqlite3_column_int64(relaxed->Value, 0);
							for (auto& entry : loaded.Agents) {
								if (entry.Id == id && !entry.Preset) {
									entry.StrictRunChecks = false;
								}
							}
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
					bind_text(runtime->Value, 1, aPreferences.ClaudeEffort);
					bind_text(runtime->Value, 2, aPreferences.CodexModel);
					bind_text(runtime->Value, 3, aPreferences.CodexEffort);
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
		class native_agent_session_store final : public application::native_agent_session_store_port
		{
		public:
			explicit native_agent_session_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			auto select(const std::string& aScope, const std::string& aConfiguration, const core::agent_revision_binding& aBinding, const bool aStartAtCurrentRevision)
				-> std::expected<application::native_agent_session, application::native_agent_storage_failure> override
			{
				auto loaded = read(aScope, aConfiguration, aBinding, true, aStartAtCurrentRevision);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				if (!*loaded) {
					return std::unexpected(application::native_agent_storage_failure::unavailable);
				}
				return std::move(**loaded);
			}
			auto find(const std::string& aScope, const std::string& aConfiguration, const core::agent_revision_binding& aBinding)
				-> std::expected<std::optional<application::native_agent_session>, application::native_agent_storage_failure> override
			{
				return read(aScope, aConfiguration, aBinding, false, false);
			}
			auto read(const std::string& aScope, const std::string& aConfiguration, const core::agent_revision_binding& aBinding, const bool aCreate, const bool aStartAtCurrentRevision, const std::optional<std::string>& aChoice = std::nullopt)
				-> std::expected<std::optional<application::native_agent_session>, application::native_agent_storage_failure>
			{
				using failure = application::native_agent_storage_failure;
				if (aScope.empty() || aScope.size() > 4096U || aConfiguration.size() > 16384U) {
					return std::unexpected(failure::unavailable);
				}
				auto connection = open(Root);
				if (!connection || !detail::execute(connection->Database.get(), "BEGIN IMMEDIATE")) {
					return std::unexpected(failure::unavailable);
				}
				auto* db  = connection->Database.get();
				auto  row = detail::prepare(db, "SELECT id,configuration,base,head,left_epoch,right_epoch,handle,turn,pending,workspace,parent,marker,application,session,directory,guard,guard_identity,shallow,revision FROM native_agent_sessions WHERE id=(SELECT conversation FROM native_agent_selection WHERE scope=?1)");
				if (!row) {
					return std::unexpected(failure::unavailable);
				}
				bind_text(row->Value, 1, aScope);
				auto status = sqlite3_step(row->Value);
				if (aChoice) {
					if (status == SQLITE_ROW && sqlite3_column_int(row->Value, 8) != 0) {
						return std::unexpected(failure::conflict);
					}
					if (aChoice->empty()) {
						// No handle/workspace means no inference has used this generation.
						if (status == SQLITE_ROW && (!column_text(row->Value, 6).value_or("").empty() || !column_text(row->Value, 9).value_or("").empty() || column_text(row->Value, 2) != std::optional{ std::string{ core::canonical_hex(aBinding.BaseOid).view() } } || column_text(row->Value, 3) != std::optional{ std::string{ core::canonical_hex(aBinding.HeadOid).view() } } || sqlite3_column_int64(row->Value, 4) != aBinding.LeftEpoch.value() || sqlite3_column_int64(row->Value, 5) != aBinding.RightEpoch.value())) {
							status = SQLITE_DONE;
						}
					} else {
						row = detail::prepare(db, "SELECT id,configuration,base,head,left_epoch,right_epoch,handle,turn,pending,workspace,parent,marker,application,session,directory,guard,guard_identity,shallow,revision FROM native_agent_sessions WHERE scope=?1 AND id=?2");
						if (!row) {
							return std::unexpected(failure::unavailable);
						}
						bind_text(row->Value, 1, aScope);
						bind_text(row->Value, 2, *aChoice);
						status = sqlite3_step(row->Value);
						if (status != SQLITE_ROW) {
							return std::unexpected(failure::conflict);
						}
					}
				}
				if (status == SQLITE_ROW && aStartAtCurrentRevision) {
					const auto changed = column_text(row->Value, 2) != std::optional{ std::string{ core::canonical_hex(aBinding.BaseOid).view() } }
						|| column_text(row->Value, 3) != std::optional{ std::string{ core::canonical_hex(aBinding.HeadOid).view() } }
						|| sqlite3_column_int64(row->Value, 4) != aBinding.LeftEpoch.value()
						|| sqlite3_column_int64(row->Value, 5) != aBinding.RightEpoch.value();
					if (changed) {
						if (sqlite3_column_int(row->Value, 8) != 0) {
							return std::unexpected(failure::conflict);
						}
						status = SQLITE_DONE;
					}
				}
				auto value          = application::native_agent_session{ };
				value.Scope         = aScope;
				value.Configuration = aConfiguration;
				value.Binding       = aBinding;
				if (status == SQLITE_ROW) {
					if (column_text(row->Value, 1) != std::optional{ aConfiguration }) {
						return std::unexpected(failure::conflict);
					}
					value.Id        = column_text(row->Value, 0).value_or(std::string{ });
					const auto base = core::parse_git_commit_oid(column_text(row->Value, 2).value_or(std::string{ }));
					const auto head = core::parse_git_commit_oid(column_text(row->Value, 3).value_or(std::string{ }));
					if (!base || !head) {
						return std::unexpected(failure::unavailable);
					}
					value.Binding.BaseOid    = *base;
					value.Binding.HeadOid    = *head;
					value.Binding.LeftEpoch  = core::epoch_ordinal{ sqlite3_column_int64(row->Value, 4) };
					value.Binding.RightEpoch = core::epoch_ordinal{ sqlite3_column_int64(row->Value, 5) };
					value.Handle             = column_text(row->Value, 6).value_or(std::string{ });
					value.Turn               = static_cast<std::uint64_t>(sqlite3_column_int64(row->Value, 7));
					value.Pending            = sqlite3_column_int(row->Value, 8) != 0;
					const auto path          = column_text(row->Value, 9).value_or(std::string{ });
					if (!path.empty()) {
						auto workspace                 = application::owned_agent_workspace{ };
						workspace.Workspace.Path       = path;
						workspace.Workspace.PinnedHead = *head;
						workspace.Workspace.Shallow    = sqlite3_column_int(row->Value, 17) != 0;
						workspace.RequiredBase         = *base;
						workspace.ParentDirectory      = column_text(row->Value, 10).value_or(std::string{ });
						workspace.OwnershipMarker      = column_text(row->Value, 11).value_or(std::string{ });
						workspace.ApplicationSession   = core::application_session_id{ static_cast<std::uint64_t>(sqlite3_column_int64(row->Value, 12)) };
						workspace.Session              = core::agent_session_id{ static_cast<std::uint64_t>(sqlite3_column_int64(row->Value, 13)) };
						if (sqlite3_column_bytes(row->Value, 14) != 32 || sqlite3_column_bytes(row->Value, 16) != 32) {
							return std::unexpected(failure::unavailable);
						}
						std::memcpy(workspace.DirectoryIdentity.Opaque.data(), sqlite3_column_blob(row->Value, 14), 32U);
						auto guard = application::agent_workspace_lifetime_guard{ };
						guard.Path = column_text(row->Value, 15).value_or(std::string{ });
						std::memcpy(guard.FileIdentity.data(), sqlite3_column_blob(row->Value, 16), 32U);
						workspace.LifetimeGuard = std::move(guard);
						value.Workspace         = std::move(workspace);
					}
					value.Revision = static_cast<std::uint64_t>(sqlite3_column_int64(row->Value, 18));
					if (value.Id.size() != 32U || value.Handle.size() > core::maximum_remote_run_id_bytes || value.Turn < 2U || value.Revision == 0U) {
						return std::unexpected(failure::unavailable);
					}
				} else if (status == SQLITE_DONE) {
					if (!aCreate) {
						return std::optional<application::native_agent_session>{ };
					}
					// Count legacy projections conservatively as separate records until
					// they are explicitly migrated. Never prune native ownership here.
					const auto count = detail::scalar_integer(db, "SELECT (SELECT count(*) FROM native_agent_sessions)+(SELECT count(*) FROM agent_conversations)");
					if (!count || *count >= static_cast<std::int64_t>(application::maximum_conversations)) {
						return std::unexpected(failure::capacity);
					}
					auto random = detail::prepare(db, "SELECT lower(hex(randomblob(16)))");
					if (!random || sqlite3_step(random->Value) != SQLITE_ROW) {
						return std::unexpected(failure::unavailable);
					}
					value.Id     = column_text(random->Value, 0).value_or(std::string{ });
					auto created = detail::prepare(db, "INSERT INTO native_agent_sessions VALUES(?1,?2,?3,?4,?5,?6,?7,'',2,0,'','','',0,0,zeroblob(32),'',zeroblob(32),0,1)");
					if (!created) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(created->Value, 1, value.Id);
					bind_text(created->Value, 2, aScope);
					bind_text(created->Value, 3, aConfiguration);
					bind_text(created->Value, 4, std::string{ core::canonical_hex(aBinding.BaseOid).view() });
					bind_text(created->Value, 5, std::string{ core::canonical_hex(aBinding.HeadOid).view() });
					sqlite3_bind_int64(created->Value, 6, static_cast<sqlite3_int64>(aBinding.LeftEpoch.value()));
					sqlite3_bind_int64(created->Value, 7, static_cast<sqlite3_int64>(aBinding.RightEpoch.value()));
					if (sqlite3_step(created->Value) != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
					value.Revision = 1U;
					auto selected  = detail::prepare(db, "INSERT OR REPLACE INTO native_agent_selection VALUES(?1,?2)");
					if (!selected) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(selected->Value, 1, aScope);
					bind_text(selected->Value, 2, value.Id);
					if (sqlite3_step(selected->Value) != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
				} else {
					return std::unexpected(failure::unavailable);
				}
				if (aChoice) {
					auto selected = detail::prepare(db, "INSERT OR REPLACE INTO native_agent_selection VALUES(?1,?2)");
					if (!selected) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(selected->Value, 1, aScope);
					bind_text(selected->Value, 2, value.Id);
					if (sqlite3_step(selected->Value) != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
				}
				if (!detail::execute(db, "COMMIT")) {
					return std::unexpected(failure::unavailable);
				}
				return value;
			}
			auto choose(const std::string& aScope, const std::string& aConfiguration, const core::agent_revision_binding& aBinding, const std::string& aId)
				-> std::expected<application::native_agent_session, application::native_agent_storage_failure> override
			{
				auto selected = read(aScope, aConfiguration, aBinding, true, false, aId);
				if (!selected) {
					return std::unexpected(selected.error());
				}
				if (!*selected) {
					return std::unexpected(application::native_agent_storage_failure::unavailable);
				}
				return std::move(**selected);
			}
			auto history(const std::string& aScope, const std::string& aConfiguration, const std::string& aView)
				-> std::expected<application::native_agent_history_projection, application::native_agent_storage_failure> override
			{
				using failure   = application::native_agent_storage_failure;
				auto connection = open(Root);
				if (!connection || !detail::execute(connection->Database.get(), "BEGIN")) {
					return std::unexpected(failure::unavailable);
				}
				auto* db      = connection->Database.get();
				auto  view    = application::native_agent_history_projection{ };
				view.Scope    = aScope;
				auto selected = detail::prepare(db, "SELECT conversation FROM native_agent_selection WHERE scope=?1");
				if (!selected) {
					return std::unexpected(failure::unavailable);
				}
				bind_text(selected->Value, 1, aScope);
				const auto selectionStatus = sqlite3_step(selected->Value);
				if (selectionStatus == SQLITE_ROW) {
					view.Selected = column_text(selected->Value, 0).value_or("");
				} else if (selectionStatus != SQLITE_DONE) {
					return std::unexpected(failure::unavailable);
				}
				auto rows = detail::prepare(db, "SELECT id,base,head,left_epoch,right_epoch,pending,(SELECT count(*) FROM native_agent_turns WHERE conversation=id),(SELECT asked FROM native_agent_turns WHERE conversation=id ORDER BY ordinal LIMIT 1) FROM native_agent_sessions WHERE scope=?1 AND configuration=?2 ORDER BY rowid DESC LIMIT 500");
				if (!rows) {
					return std::unexpected(failure::unavailable);
				}
				bind_text(rows->Value, 1, aScope);
				bind_text(rows->Value, 2, aConfiguration);
				auto status = SQLITE_ROW;
				while ((status = sqlite3_step(rows->Value)) == SQLITE_ROW) {
					auto entry = application::native_agent_history_entry{ };
					entry.Id   = column_text(rows->Value, 0).value_or("");
					auto base  = core::parse_git_commit_oid(column_text(rows->Value, 1).value_or(""));
					auto head  = core::parse_git_commit_oid(column_text(rows->Value, 2).value_or(""));
					if (!base || !head) {
						return std::unexpected(failure::unavailable);
					}
					entry.Binding.BaseOid    = *base;
					entry.Binding.HeadOid    = *head;
					entry.Binding.LeftEpoch  = core::epoch_ordinal{ sqlite3_column_int64(rows->Value, 3) };
					entry.Binding.RightEpoch = core::epoch_ordinal{ sqlite3_column_int64(rows->Value, 4) };
					entry.Pending            = sqlite3_column_int(rows->Value, 5) != 0;
					entry.Turns              = static_cast<std::size_t>(sqlite3_column_int64(rows->Value, 6));
					entry.Title              = column_text(rows->Value, 7).value_or("New session");
					view.Entries.push_back(std::move(entry));
				}
				if (status != SQLITE_DONE) {
					return std::unexpected(failure::unavailable);
				}
				const auto tour = aScope.find(":tour:");
				if (tour != std::string::npos) {
					const auto draftEnd = aScope.find(':', tour + 6U);
					const auto prefix   = aScope.substr(0, draftEnd + 1U);
					auto       chats    = detail::prepare(db, "SELECT id,scope,(SELECT asked FROM native_agent_turns WHERE conversation=id ORDER BY ordinal LIMIT 1) FROM native_agent_sessions WHERE substr(scope,1,length(?1))=?1 AND configuration=?2 AND scope!=?3 ORDER BY rowid DESC LIMIT 500");
					if (!chats) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(chats->Value, 1, prefix);
					bind_text(chats->Value, 2, aConfiguration);
					bind_text(chats->Value, 3, aScope);
					while ((status = sqlite3_step(chats->Value)) == SQLITE_ROW) {
						auto entry  = application::native_agent_history_entry{ };
						entry.Id    = column_text(chats->Value, 0).value_or("");
						entry.Scope = column_text(chats->Value, 1).value_or("").substr(tour + 1U);
						entry.Title = column_text(chats->Value, 2).value_or("New session");
						view.OtherChats.push_back(std::move(entry));
					}
					if (status != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
				}
				if (!aView.empty()) {
					if (std::ranges::none_of(view.OtherChats, [&](const auto& aEntry) { return aEntry.Id == aView; })) {
						return std::unexpected(failure::conflict);
					}
					view.ReadOnly = true;
				}
				auto turns = detail::prepare(db, "SELECT asked,answered,ending,outcome FROM native_agent_turns WHERE conversation=?1 ORDER BY ordinal LIMIT 50");
				if (!turns) {
					return std::unexpected(failure::unavailable);
				}
				bind_text(turns->Value, 1, aView.empty() ? view.Selected : aView);
				while ((status = sqlite3_step(turns->Value)) == SQLITE_ROW) {
					auto turn     = application::agent_conversation_turn{ };
					turn.Asked    = column_text(turns->Value, 0).value_or("");
					turn.Answered = column_text(turns->Value, 1).value_or("");
					turn.Ending   = column_text(turns->Value, 2).value_or("");
					turn.Outcome  = static_cast<core::agent_turn_outcome>(sqlite3_column_int(turns->Value, 3));
					view.Turns.push_back(std::move(turn));
				}
				if (status != SQLITE_DONE || !application::valid(view.Turns) || !detail::execute(db, "COMMIT")) {
					return std::unexpected(failure::unavailable);
				}
				return view;
			}
			auto append(const std::string& aId, const application::agent_conversation_turn& aTurn) -> bool override
			{
				if (!application::valid(std::vector{ aTurn })) {
					return false;
				}
				auto connection = open(Root);
				if (!connection || !detail::execute(connection->Database.get(), "BEGIN IMMEDIATE")) {
					return false;
				}
				auto* db  = connection->Database.get();
				auto  row = detail::prepare(db, "INSERT INTO native_agent_turns SELECT ?1,coalesce(max(ordinal),0)+1,?2,?3,?4,?5 FROM native_agent_turns WHERE conversation=?1");
				if (!row) {
					return false;
				}
				bind_text(row->Value, 1, aId);
				bind_text(row->Value, 2, aTurn.Asked);
				bind_text(row->Value, 3, aTurn.Answered);
				bind_text(row->Value, 4, aTurn.Ending);
				sqlite3_bind_int(row->Value, 5, static_cast<int>(aTurn.Outcome));
				if (sqlite3_step(row->Value) != SQLITE_DONE) {
					return false;
				}
				auto prune = detail::prepare(db, "DELETE FROM native_agent_turns WHERE conversation=?1 AND ordinal NOT IN (SELECT ordinal FROM native_agent_turns WHERE conversation=?1 ORDER BY ordinal DESC LIMIT 50)");
				if (!prune) {
					return false;
				}
				bind_text(prune->Value, 1, aId);
				return sqlite3_step(prune->Value) == SQLITE_DONE && detail::execute(db, "COMMIT");
			}
			auto store(application::native_agent_session& aSession) -> bool override
			{
				if (aSession.Id.size() != 32U || aSession.Handle.size() > core::maximum_remote_run_id_bytes || aSession.Turn < 2U
					|| aSession.Turn > static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max()) || aSession.Revision == 0U) {
					return false;
				}
				auto connection = open(Root);
				if (!connection) {
					return false;
				}
				auto row = detail::prepare(connection->Database.get(), "UPDATE native_agent_sessions SET handle=?1,turn=?2,pending=?3,workspace=?4,parent=?5,marker=?6,application=?7,session=?8,directory=?9,guard=?10,guard_identity=?11,shallow=?12,revision=revision+1 WHERE id=?13 AND revision=?14");
				if (!row) {
					return false;
				}
				auto       workspace = aSession.Workspace.value_or(application::owned_agent_workspace{ });
				const auto guard     = workspace.LifetimeGuard.value_or(application::agent_workspace_lifetime_guard{ });
				bind_text(row->Value, 1, aSession.Handle);
				sqlite3_bind_int64(row->Value, 2, static_cast<sqlite3_int64>(aSession.Turn));
				sqlite3_bind_int(row->Value, 3, aSession.Pending ? 1 : 0);
				bind_text(row->Value, 4, workspace.Workspace.Path);
				bind_text(row->Value, 5, workspace.ParentDirectory.string());
				bind_text(row->Value, 6, workspace.OwnershipMarker);
				sqlite3_bind_int64(row->Value, 7, static_cast<sqlite3_int64>(workspace.ApplicationSession.value()));
				sqlite3_bind_int64(row->Value, 8, static_cast<sqlite3_int64>(workspace.Session.value()));
				sqlite3_bind_blob(row->Value, 9, workspace.DirectoryIdentity.Opaque.data(), 32, SQLITE_TRANSIENT);
				bind_text(row->Value, 10, guard.Path);
				sqlite3_bind_blob(row->Value, 11, guard.FileIdentity.data(), 32, SQLITE_TRANSIENT);
				sqlite3_bind_int(row->Value, 12, workspace.Workspace.Shallow ? 1 : 0);
				bind_text(row->Value, 13, aSession.Id);
				sqlite3_bind_int64(row->Value, 14, static_cast<sqlite3_int64>(aSession.Revision));
				if (sqlite3_step(row->Value) != SQLITE_DONE || sqlite3_changes(connection->Database.get()) != 1) {
					return false;
				}
				++aSession.Revision;
				return true;
			}

		private:
			std::filesystem::path Root;
		};

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
					auto row = detail::prepare(store->Database.get(), "SELECT t.asked,t.answered,t.ending,o.outcome FROM agent_conversation_turns t LEFT JOIN agent_conversation_outcomes o "
																	  "USING(host,owner,name,number,agent,position) WHERE t.host=?1 AND t.owner=?2 AND t.name=?3 AND t.number=?4 AND t.agent=?5 ORDER BY t.position");
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
						// [[BPR-1847]]. A kept outcome, else the ending's.
						auto outcome = core::legacy_agent_turn_outcome(*ending);
						if (sqlite3_column_type(row->Value, 3) == SQLITE_INTEGER) {
							const auto kept = core::agent_turn_outcome_of(static_cast<std::uint8_t>(std::clamp<std::int64_t>(sqlite3_column_int64(row->Value, 3), 0, 255)));
							if (!kept) {
								return std::vector<application::agent_conversation_turn>{ };
							}
							outcome = *kept;
						}
						turns.push_back({ .Asked = std::move(*asked), .Answered = std::move(*answered), .Ending = std::move(*ending), .Outcome = outcome });
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
						&& run("DELETE FROM agent_conversation_outcomes WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5", key)
						&& run("DELETE FROM agent_conversations WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5", key);
					if (committed && !aTurns.empty()) {
						committed = run("INSERT INTO agent_conversations SELECT ?1,?2,?3,?4,?5,coalesce(max(written)+1,0) FROM agent_conversations", key);
						for (std::size_t index = 0U; committed && index < aTurns.size(); ++index) {
							committed = run("INSERT INTO agent_conversation_turns VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)", [&](sqlite3_stmt* aRow) {
											bind_key(aRow, aKey);
											sqlite3_bind_int64(aRow, 6, static_cast<std::int64_t>(index));
											bind_text(aRow, 7, aTurns[index].Asked);
											bind_text(aRow, 8, aTurns[index].Answered);
											bind_text(aRow, 9, aTurns[index].Ending);
										})
								&& run("INSERT INTO agent_conversation_outcomes VALUES(?1,?2,?3,?4,?5,?6,?7)", [&](sqlite3_stmt* aRow) {
											bind_key(aRow, aKey);
											sqlite3_bind_int64(aRow, 6, static_cast<std::int64_t>(index));
											sqlite3_bind_int64(aRow, 7, static_cast<std::int64_t>(aTurns[index].Outcome));
										});
						}
						// The least recently written go past the bound.
						committed = committed
							&& detail::execute(db,
											   "DELETE FROM agent_conversation_outcomes WHERE (host,owner,name,number,agent) IN (SELECT host,owner,name,number,agent FROM agent_conversations "
											   "ORDER BY written DESC LIMIT -1 OFFSET "
												   + std::to_string(application::maximum_conversations) + ")")
								   .has_value()
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
				bind_text(aRow, 1, aKey.Host);
				bind_text(aRow, 2, aKey.Owner);
				bind_text(aRow, 3, aKey.Name);
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
	auto make_native_agent_session_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::native_agent_session_store_port>
	{
		return std::make_unique<native_agent_session_store>(aRoot);
	}
} // namespace bpr::adapters::sqlite
