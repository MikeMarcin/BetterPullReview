#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/adapters/sqlite/workbench_settings_store.hpp"

#include "sqlite_connection.hpp"
#include "storage_internal.hpp"

#include <sqlite3.h>

#include "bpr/core/agent_finding_card_memory.hpp"
#include "bpr/core/agent_turn.hpp"
#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/review_composition.hpp"
#include "bpr/core/review_preparation.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::adapters::sqlite {
	// [[BPR-1847]]. The outcome column's check names every declared outcome.
	static_assert(core::agent_turn_outcome_count == 10U, "agent_conversation_outcomes' CHECK must cover every turn outcome");
	// agent_finding_cards' CHECKs: every kept disposition (open is no row), the
	// reason bound and the remembered-card bound.
	static_assert(core::agent_finding_card_disposition_count == 5U && static_cast<int>(core::agent_finding_card_disposition::refused) == 4,
				  "agent_finding_cards' state CHECK must cover every kept disposition");
	static_assert(core::maximum_finding_card_reason_bytes == 1024U, "agent_finding_cards' why CHECK must match the reason bound");
	static_assert(core::maximum_remembered_finding_cards == 4096U, "agent_finding_cards' prune must match the card bound");

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

		/// [[BPR-2185]] (review N6). The longest condition text core's limits
		/// allow, as `encode_conditions` writes it: each leaf a line of its
		/// fact, polarity, value and patterns; each nested group its opening
		/// and closing lines; the list's match and Hide it or Show it
		/// disabled first.
		constexpr std::size_t longest_condition_line = 1U + 23U + 1U + 3U + 1U + 7U + core::maximum_prompt_patterns * (1U + core::maximum_prompt_pattern_bytes);
		constexpr std::size_t most_condition_groups  = core::maximum_prompt_conditions + core::maximum_prompt_conditions * core::maximum_prompt_conditions;
		constexpr std::size_t longest_conditions     = 9U + core::maximum_prompt_condition_leaves * longest_condition_line + most_condition_groups * 9U;
		/// The conditions column's bound: 8192 bytes held a flat list, not a
		/// tree at core's limits.
		constexpr std::size_t conditions_column_bytes = 16384U;
		static_assert(longest_conditions <= conditions_column_bytes, "the conditions column holds a tree at core's limits");
		static_assert(core::maximum_prompt_condition_depth == 3U && core::maximum_prompt_condition_leaves == 12U && core::maximum_prompt_patterns == 8U && core::maximum_prompt_pattern_bytes == 128U,
					  "the bound above counts these limits");

		constexpr auto prompt_macro_packs_columns = std::string_view{
			"id TEXT PRIMARY KEY CHECK(length(CAST(id AS BLOB)) BETWEEN 1 AND 128),"
			"pack TEXT NOT NULL CHECK(length(CAST(pack AS BLOB)) BETWEEN 1 AND 64),"
			"label TEXT NOT NULL CHECK(length(CAST(label AS BLOB)) <= 128),"
			"version INTEGER NOT NULL CHECK(version BETWEEN 1 AND 4294967295),"
			"place TEXT NOT NULL CHECK(length(CAST(place AS BLOB)) <= 64),"
			"name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) <= 120),"
			"template TEXT NOT NULL CHECK(length(CAST(template AS BLOB)) <= 8192),"
			"subject TEXT NOT NULL CHECK(length(CAST(subject AS BLOB)) <= 64),"
			"ask TEXT NOT NULL CHECK(length(CAST(ask AS BLOB)) <= 64),"
			"conditions TEXT NOT NULL CHECK(length(CAST(conditions AS BLOB)) <= 16384),"
			"quick INTEGER NOT NULL CHECK(quick BETWEEN 0 AND 1),"
			"scopes INTEGER NOT NULL CHECK(scopes BETWEEN 0 AND 7)"
		};

		constexpr auto prompt_macros_columns = std::string_view{
			"kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 1),"
			"id TEXT NOT NULL CHECK(length(CAST(id AS BLOB)) BETWEEN 1 AND 128),"
			"position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 63),"
			"place TEXT NOT NULL CHECK(length(CAST(place AS BLOB)) <= 64),"
			"has_body INTEGER NOT NULL CHECK(has_body BETWEEN 0 AND 1),"
			"name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) <= 120),"
			"template TEXT NOT NULL CHECK(length(CAST(template AS BLOB)) <= 8192),"
			"subject TEXT NOT NULL CHECK(length(CAST(subject AS BLOB)) <= 64),"
			"ask TEXT NOT NULL CHECK(length(CAST(ask AS BLOB)) <= 64),"
			"conditions TEXT NOT NULL CHECK(length(CAST(conditions AS BLOB)) <= 16384),"
			"quick INTEGER NOT NULL CHECK(quick BETWEEN 0 AND 1),"
			"scopes INTEGER NOT NULL CHECK(scopes BETWEEN 0 AND 7),"
			"on_state INTEGER CHECK(on_state IS NULL OR on_state BETWEEN 0 AND 1),"
			"written_against INTEGER NOT NULL CHECK(written_against BETWEEN 0 AND 4294967295),"
			"PRIMARY KEY(kind, id)"
		};
		constexpr auto prompt_repository_prompts_columns = std::string_view{
			"repository TEXT NOT NULL CHECK(length(CAST(repository AS BLOB)) BETWEEN 1 AND 256),"
			"kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 1),"
			"id TEXT NOT NULL CHECK(length(CAST(id AS BLOB)) BETWEEN 1 AND 128),"
			"position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 63),"
			"place TEXT NOT NULL CHECK(length(CAST(place AS BLOB)) <= 64),"
			"subject TEXT NOT NULL CHECK(length(CAST(subject AS BLOB)) <= 64),"
			"name TEXT CHECK(name IS NULL OR length(CAST(name AS BLOB)) <= 120),"
			"template TEXT CHECK(template IS NULL OR length(CAST(template AS BLOB)) <= 8192),"
			"ask TEXT CHECK(ask IS NULL OR length(CAST(ask AS BLOB)) <= 64),"
			"conditions TEXT CHECK(conditions IS NULL OR length(CAST(conditions AS BLOB)) <= 16384),"
			"match TEXT CHECK(match IS NULL OR length(CAST(match AS BLOB)) <= 16),"
			"on_state INTEGER CHECK(on_state IS NULL OR on_state BETWEEN 0 AND 1),"
			"PRIMARY KEY(repository, kind, id)"
		};

		/// [[BPR-2181]]. Drops queue tables made with a cascading entry
		/// reference while both are empty, which is all they can be: no save
		/// into them ever succeeded. Anything else is left as it is.
		[[nodiscard]] auto drop_cascading_queue_tables(sqlite3* const aDatabase) -> bool
		{
			const auto cascading = detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='agent_queue_entries' AND sql LIKE '%ON DELETE CASCADE%'");
			if (!cascading) {
				return false;
			}
			if (*cascading == 0) {
				return true;
			}
			const auto rows = detail::scalar_integer(aDatabase, "SELECT (SELECT count(*) FROM agent_queue_entries) + (SELECT count(*) FROM agent_queues)");
			if (!rows) {
				return false;
			}
			return *rows != 0 || (detail::execute(aDatabase, "DROP TABLE agent_queue_entries") && detail::execute(aDatabase, "DROP TABLE agent_queues"));
		}

		/// [[BPR-2181]] (AC13). A queued suggested prompt keeps the offer
		/// decided at its press; a file made before has no column for it.
		[[nodiscard]] auto add_queue_prompt_column(sqlite3* const aDatabase) -> bool
		{
			const auto present = detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='agent_queue_entries' AND sql LIKE '%prompt TEXT%'");
			if (!present) {
				return false;
			}
			return *present != 0
				|| detail::execute(aDatabase, "ALTER TABLE agent_queue_entries ADD COLUMN prompt TEXT NOT NULL DEFAULT '' CHECK(length(CAST(prompt AS BLOB)) <= 40960)").has_value();
		}

		/// [[BPR-2152]] (AC06). A claimed entry keeps what it was bound to
		/// (`core::encode_queued_claim`); a file made before has no column.
		[[nodiscard]] auto add_queue_claim_column(sqlite3* const aDatabase) -> bool
		{
			const auto present = detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='agent_queue_entries' AND sql LIKE '%claim TEXT%'");
			if (!present) {
				return false;
			}
			return *present != 0
				|| detail::execute(aDatabase, "ALTER TABLE agent_queue_entries ADD COLUMN claim TEXT NOT NULL DEFAULT '' CHECK(length(CAST(claim AS BLOB)) <= 256)").has_value();
		}

		/// [[BPR-2241]]. A native turn keeps how long its run took, so it is
		/// restored saying how it ended; a file made before has no column.
		[[nodiscard]] auto add_native_turn_seconds_column(sqlite3* const aDatabase) -> bool
		{
			const auto present = detail::scalar_integer(aDatabase, "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='native_agent_turns' AND sql LIKE '%seconds INTEGER%'");
			if (!present) {
				return false;
			}
			return *present != 0
				|| detail::execute(aDatabase, "ALTER TABLE native_agent_turns ADD COLUMN seconds INTEGER CHECK(seconds IS NULL OR seconds BETWEEN 0 AND 4294967295)").has_value();
		}

		void bind_seconds(sqlite3_stmt* const aRow, const int aColumn, const std::optional<std::uint32_t> aSeconds)
		{
			if (aSeconds) {
				sqlite3_bind_int64(aRow, aColumn, static_cast<sqlite3_int64>(*aSeconds));
			} else {
				sqlite3_bind_null(aRow, aColumn);
			}
		}

		[[nodiscard]] auto column_seconds(sqlite3_stmt* const aRow, const int aColumn) -> std::optional<std::uint32_t>
		{
			if (sqlite3_column_type(aRow, aColumn) == SQLITE_NULL) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(std::clamp<sqlite3_int64>(sqlite3_column_int64(aRow, aColumn), 0, UINT32_MAX));
		}

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
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_workspaces(scope TEXT PRIMARY KEY, conversation TEXT NOT NULL REFERENCES native_agent_sessions(id)) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_sparse_objects(conversation TEXT PRIMARY KEY REFERENCES native_agent_sessions(id), missing TEXT NOT NULL CHECK(length(CAST(missing AS BLOB)) <= 67108864)) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_turns(conversation TEXT NOT NULL REFERENCES native_agent_sessions(id), ordinal INTEGER NOT NULL, asked TEXT NOT NULL, answered TEXT NOT NULL, ending TEXT NOT NULL, outcome INTEGER NOT NULL, seconds INTEGER CHECK(seconds IS NULL OR seconds BETWEEN 0 AND 4294967295), PRIMARY KEY(conversation,ordinal)) STRICT")
				// [[BPR-2153]]. What the reviewer steered into each turn, in the
				// order sent: their words, where in the answer it went in, the
				// agent's step it followed, and whether the agent took it.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS native_agent_turn_steers(conversation TEXT NOT NULL REFERENCES native_agent_sessions(id), ordinal INTEGER NOT NULL, position INTEGER NOT NULL CHECK(position >= 0),"
									" text TEXT NOT NULL, at INTEGER NOT NULL CHECK(at >= 0), steps INTEGER NOT NULL CHECK(steps >= 0), disposition INTEGER NOT NULL CHECK(disposition BETWEEN 0 AND 4), PRIMARY KEY(conversation,ordinal,position)) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_replays(conversation TEXT PRIMARY KEY REFERENCES native_agent_sessions(id), source TEXT NOT NULL) STRICT")
				// [[BPR-1999]]. What a native conversation has cost: the sum of
				// its turns' costs, whether any reported one, and the runtime's
				// last figure for the session it resumes.
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_costs(conversation TEXT PRIMARY KEY REFERENCES native_agent_sessions(id), total INTEGER NOT NULL CHECK(total >= 0), known INTEGER NOT NULL CHECK(known IN (0,1)), reported INTEGER CHECK(reported >= 0)) STRICT")
				|| !detail::execute(db, "CREATE TABLE IF NOT EXISTS native_agent_selection(scope TEXT PRIMARY KEY, conversation TEXT NOT NULL REFERENCES native_agent_sessions(id)) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_entry_relaxed_checks("
									"id INTEGER PRIMARY KEY CHECK(id BETWEEN 1 AND 64)) STRICT")
				// [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104. Each added agent's
				// Scratch folder starts and Build and run tests settings; an
				// agent with no row (one carried over) starts on, builds off.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_entry_grants("
									"id INTEGER PRIMARY KEY CHECK(id BETWEEN 1 AND 64),"
									"scratch_starts INTEGER NOT NULL CHECK(scratch_starts IN (0,1)),"
									"builds INTEGER NOT NULL CHECK(builds IN (0,1))) STRICT")
				// [[BPR-2139]], [[BPR-2140]] (owner, 2026-11-12). Each added
				// agent's MCP servers and instructions settings; an agent with
				// no row (one carried over) has MCP off and the system prompt.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_entry_launch("
									"id INTEGER PRIMARY KEY CHECK(id BETWEEN 1 AND 64),"
									"mcp_own INTEGER NOT NULL CHECK(mcp_own IN (0,1)),"
									"instructions_in_message INTEGER NOT NULL CHECK(instructions_in_message IN (0,1))) STRICT")
				// The scratch folder choice made in one pull request for one
				// agent, and the pull request's answer to the build question.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_scratch_choice("
									"host TEXT NOT NULL,"
									"owner TEXT NOT NULL,"
									"name TEXT NOT NULL,"
									"number INTEGER NOT NULL,"
									"agent INTEGER NOT NULL CHECK(agent BETWEEN 0 AND 64),"
									"scratch INTEGER NOT NULL CHECK(scratch IN (0,1)),"
									"PRIMARY KEY(host, owner, name, number, agent)) STRICT")
				// Re-review N3, N4. The scratch folder each agent last kept in
				// one pull request, by its name under the root.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_scratch_folder("
									"host TEXT NOT NULL,"
									"owner TEXT NOT NULL,"
									"name TEXT NOT NULL,"
									"number INTEGER NOT NULL,"
									"agent INTEGER NOT NULL CHECK(agent BETWEEN 0 AND 64),"
									"folder TEXT NOT NULL CHECK(length(folder) BETWEEN 1 AND 96),"
									"PRIMARY KEY(host, owner, name, number, agent)) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_build_answer("
									"host TEXT NOT NULL,"
									"owner TEXT NOT NULL,"
									"name TEXT NOT NULL,"
									"number INTEGER NOT NULL,"
									"allowed INTEGER NOT NULL CHECK(allowed IN (0,1)),"
									"PRIMARY KEY(host, owner, name, number)) STRICT")
				// [[BPR-2051]]. Each added agent's last check: passed (2) with
				// the version it read, or failed (3) with why.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_entry_checks("
									"id INTEGER PRIMARY KEY CHECK(id BETWEEN 1 AND 64),"
									"state INTEGER NOT NULL CHECK(state IN (2,3)),"
									"failure INTEGER NOT NULL CHECK(failure BETWEEN -1 AND 7),"
									"version TEXT NOT NULL CHECK(length(CAST(version AS BLOB)) <= 128)) STRICT")
				// [[BPR-2016]], RFC-BPR-0107. The built-in version each writing
				// hook's override was written against, for the override whose
				// SHA-256 is `text_digest`: a row for other text says nothing.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS prompt_provenance("
									"hook INTEGER PRIMARY KEY CHECK(hook BETWEEN 0 AND 15),"
									"text_digest BLOB NOT NULL CHECK(length(text_digest)=32),"
									"version TEXT NOT NULL CHECK(length(CAST(version AS BLOB)) BETWEEN 1 AND 64)) STRICT")
				// [[BPR-2016]]. An import's hooks before it, kept while the
				// import's writes to the two settings files are under way, so a
				// crash between them is undone at the next start.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS configuration_import_journal("
									"hook INTEGER PRIMARY KEY CHECK(hook BETWEEN 0 AND 15),"
									"revision_after INTEGER NOT NULL CHECK(revision_after >= 1),"
									"text TEXT NOT NULL CHECK(length(CAST(text AS BLOB)) <= 8192)) STRICT")
				// [[BPR-2032]], RFC-BPR-0112. The profile's optional workflows,
				// each with this computer's bindings, in their order.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS workflow_configurations("
									"position INTEGER PRIMARY KEY CHECK(position >= 0),"
									"document TEXT NOT NULL CHECK(length(CAST(document AS BLOB)) <= 1048576)) STRICT")
				// Owner decision 2026-10-23: the run time limit; none means off.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_run_limit("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"minutes INTEGER NOT NULL CHECK(minutes BETWEEN 0 AND 1440)) STRICT")
				// RFC-BPR-0104 as amended 2026-11-11: the scratch folder limit and
				// how long an unused folder is kept.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_scratch_policy("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"limit_mib INTEGER NOT NULL CHECK(limit_mib BETWEEN 16 AND 65536),"
									"keep_days INTEGER NOT NULL CHECK(keep_days BETWEEN 0 AND 3650)) STRICT")
				// Owner look v004: whether the Agent tab's suggestions are hidden.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_suggestions("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"hidden INTEGER NOT NULL CHECK(hidden IN (0,1))) STRICT")
				// RFC-BPR-0126 Q4/Q5: a follow-up steers (1) or is queued (0).
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_follow_up("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"steer INTEGER NOT NULL CHECK(steer IN (0,1))) STRICT")
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
									"PRIMARY KEY(host, owner, name, number, agent, position)) STRICT")
				// Owner report 2026-11-10, after BPR-2052. What was done with each
				// finding card of a stored answer, by the SHA-256 of the answer's
				// findings block and the card's place in it; an open card has no
				// row. `changed` orders them for the bound.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_finding_cards("
									"findings BLOB NOT NULL CHECK(length(findings)=32),"
									"ordinal INTEGER NOT NULL CHECK(ordinal BETWEEN 0 AND 4294967295),"
									"state INTEGER NOT NULL CHECK(state BETWEEN 1 AND 4),"
									"why TEXT NOT NULL CHECK(length(CAST(why AS BLOB)) <= 1024),"
									"note INTEGER NOT NULL CHECK(note >= 0),"
									"changed INTEGER NOT NULL CHECK(changed >= 0),"
									"PRIMARY KEY(findings, ordinal)) STRICT")
				// RFC-BPR-0125 (AC07). The name a suggested prompt's turn was
				// sent under, by the question it asked; `changed` orders them
				// for the bound.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS prompt_turn_names("
									"asked TEXT PRIMARY KEY CHECK(length(CAST(asked AS BLOB)) BETWEEN 1 AND 16384),"
									"name TEXT NOT NULL CHECK(length(CAST(name AS BLOB)) BETWEEN 1 AND 1024),"
									"changed INTEGER NOT NULL CHECK(changed >= 0)) STRICT")
				// RFC-BPR-0125 milestone 3 ([[BPR-2146]]) and RFC-BPR-0128
				// ([[BPR-2163]]). Suggested prompts, as differences from the
				// shipped set: a built-in's change (`kind` 0) or one of the
				// reviewer's own (`kind` 1), each place's order, and the set's
				// revision. Names are stable names, so a row a newer build
				// wrote is read as newer and kept.
				|| !detail::execute(db, std::string{ "CREATE TABLE IF NOT EXISTS prompt_macros(" } + std::string{ prompt_macros_columns } + ") STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS prompt_macro_orders("
									"place TEXT NOT NULL CHECK(length(CAST(place AS BLOB)) BETWEEN 1 AND 64),"
									"position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 63),"
									"id TEXT NOT NULL CHECK(length(CAST(id AS BLOB)) BETWEEN 1 AND 128),"
									"PRIMARY KEY(place, position)) STRICT")
				// RFC-BPR-0126 ([[BPR-2151]]). Each session's queue: its state
				// and its entries in order; a session's queue is written whole.
				// [[BPR-2181]]. Its entries are deleted with it explicitly: a
				// cascade is a trigger program, and the hardened connection
				// allows none, so a cascading delete never ran and no queue was
				// ever saved. A file made with the cascade can hold only empty
				// queue tables; they are made again without it.
				|| !drop_cascading_queue_tables(db)
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_queues("
									"session TEXT PRIMARY KEY CHECK(length(CAST(session AS BLOB)) BETWEEN 1 AND 1024),"
									"revision INTEGER NOT NULL CHECK(revision >= 0),"
									"last_id INTEGER NOT NULL CHECK(last_id >= 0),"
									"paused TEXT NOT NULL CHECK(length(CAST(paused AS BLOB)) <= 64)) STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS agent_queue_entries("
									"session TEXT NOT NULL REFERENCES agent_queues(session),"
									"position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 15),"
									"id INTEGER NOT NULL CHECK(id >= 1),"
									"quick_ask TEXT NOT NULL, scope TEXT NOT NULL, file TEXT NOT NULL, thread TEXT NOT NULL,"
									"label TEXT NOT NULL, text TEXT NOT NULL CHECK(length(CAST(text AS BLOB)) <= 16384),"
									"state TEXT NOT NULL, held INTEGER NOT NULL CHECK(held BETWEEN 0 AND 1),"
									"prompt TEXT NOT NULL DEFAULT '' CHECK(length(CAST(prompt AS BLOB)) <= 40960),"
									"claim TEXT NOT NULL DEFAULT '' CHECK(length(CAST(claim AS BLOB)) <= 256),"
									"PRIMARY KEY(session, position)) STRICT")
				|| !add_queue_prompt_column(db)
				|| !add_queue_claim_column(db)
				|| !add_native_turn_seconds_column(db)
				// [[BPR-2179]]. Each surface the reviewer changed: its list
				// and what it took off, identities one a line. `layer` is
				// empty for every repository, else the repository's, whose
				// row also says what it added and whether `ids` is its own
				// order.
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS prompt_surface_lists("
									"layer TEXT NOT NULL CHECK(length(CAST(layer AS BLOB)) <= 256),"
									"surface TEXT NOT NULL CHECK(length(CAST(surface AS BLOB)) BETWEEN 1 AND 64),"
									"ids TEXT NOT NULL CHECK(length(CAST(ids AS BLOB)) <= 8448),"
									"removed TEXT NOT NULL CHECK(length(CAST(removed AS BLOB)) <= 8448),"
									"added TEXT NOT NULL CHECK(length(CAST(added AS BLOB)) <= 8448),"
									"ordered INTEGER NOT NULL CHECK(ordered BETWEEN 0 AND 1),"
									"PRIMARY KEY(layer, surface)) STRICT")
				// [[BPR-2179]]. A repository's prompts: an override of an
				// app-wide prompt's fields (`kind` 0, a null field follows the
				// app-wide one) or one of its own (`kind` 1).
				|| !detail::execute(db, std::string{ "CREATE TABLE IF NOT EXISTS prompt_repository_prompts(" } + std::string{ prompt_repository_prompts_columns } + ") STRICT")
				|| !detail::execute(db,
									"CREATE TABLE IF NOT EXISTS prompt_macro_revision("
									"singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
									"revision INTEGER NOT NULL CHECK(revision >= 0)) STRICT")
				// RFC-BPR-0125 AC24 ([[BPR-2149]]). The prompt pack each of the
				// reviewer's own prompts is linked to: the pack's UUID, label and
				// the version the copy was written against, and the place and
				// body as the pack had them. A table of its own, made where a
				// store lacks it: a build before it never reads it, so an edit
				// made there still reads as edited here, and a prompt deleted
				// there leaves a row no prompt names, which is not read.
				|| !detail::execute(db, std::string{ "CREATE TABLE IF NOT EXISTS prompt_macro_packs(" } + std::string{ prompt_macro_packs_columns } + ") STRICT")) {
				return { };
			}
			// RFC-BPR-0110: two more writing hooks; RFC-BPR-0114 as amended
			// 2026-11-12: one more. A table an earlier build made keeps hooks 0
			// to 7 or 0 to 9; it is rebuilt with the wider check and its rows,
			// in one transaction.
			// RFC-BPR-0116: two more, to 0 to 12.
			// RFC-BPR-0117: one more, to 0 to 13. RFC-BPR-0118: one more, to 0 to 14.
			// RFC-BPR-0120: one more, to 0 to 15.
			static_assert(core::writing_hook_count == 16U, "the hook tables' check names the last hook");
			for (const auto& [table, definition] : std::array{
					 std::pair{ std::string_view{ "prompt_provenance" },
								std::string_view{ "hook INTEGER PRIMARY KEY CHECK(hook BETWEEN 0 AND 15),text_digest BLOB NOT NULL CHECK(length(text_digest)=32),"
												  "version TEXT NOT NULL CHECK(length(CAST(version AS BLOB)) BETWEEN 1 AND 64)" } },
					 std::pair{ std::string_view{ "configuration_import_journal" },
								std::string_view{ "hook INTEGER PRIMARY KEY CHECK(hook BETWEEN 0 AND 15),revision_after INTEGER NOT NULL CHECK(revision_after >= 1),"
												  "text TEXT NOT NULL CHECK(length(CAST(text AS BLOB)) <= 8192)" } } }) {
				auto query = std::string{ "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='" };
				query += table;
				query += "' AND (sql LIKE '%hook BETWEEN 0 AND 7)%' OR sql LIKE '%hook BETWEEN 0 AND 9)%' OR sql LIKE '%hook BETWEEN 0 AND 10)%' OR sql LIKE '%hook BETWEEN 0 AND 12)%' OR sql LIKE '%hook BETWEEN 0 AND 13)%' OR sql LIKE '%hook BETWEEN 0 AND 14)%')";
				const auto narrow = detail::scalar_integer(db, query);
				if (!narrow) {
					return { };
				}
				if (*narrow == 0) {
					continue;
				}
				const auto statement = [table](std::string aBefore, const std::string_view aMiddle, const std::string_view aAfter) {
					aBefore += table;
					aBefore += aMiddle;
					aBefore += aAfter;
					return aBefore;
				};
				auto rename = statement("ALTER TABLE ", " RENAME TO narrow_", { });
				rename += table;
				auto copy = statement("INSERT INTO ", " SELECT * FROM narrow_", { });
				copy += table;
				if (!detail::execute(db, "BEGIN IMMEDIATE") || !detail::execute(db, rename) || !detail::execute(db, statement("CREATE TABLE ", "(", definition) + ") STRICT")
					|| !detail::execute(db, copy) || !detail::execute(db, statement("DROP TABLE narrow_", { }, { })) || !detail::execute(db, "COMMIT")) {
					static_cast<void>(detail::execute(db, "ROLLBACK"));
					return { };
				}
			}
			// [[BPR-2185]] (review N6). A prompt table an earlier build made
			// bounds its conditions at 8192 bytes; it is rebuilt with the
			// wider bound and its rows, in one transaction.
			for (const auto& [table, columns] : std::array{ std::pair{ std::string_view{ "prompt_macros" }, prompt_macros_columns },
															std::pair{ std::string_view{ "prompt_repository_prompts" }, prompt_repository_prompts_columns } }) {
				const auto name   = std::string{ table };
				const auto narrow = detail::scalar_integer(db, "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='" + name + "' AND sql LIKE '%conditions AS BLOB)) <= 8192)%'");
				if (!narrow) {
					return { };
				}
				if (*narrow == 0) {
					continue;
				}
				if (!detail::execute(db, "BEGIN IMMEDIATE") || !detail::execute(db, "ALTER TABLE " + name + " RENAME TO narrow_" + name)
					|| !detail::execute(db, "CREATE TABLE " + name + "(" + std::string{ columns } + ") STRICT") || !detail::execute(db, "INSERT INTO " + name + " SELECT * FROM narrow_" + name)
					|| !detail::execute(db, "DROP TABLE narrow_" + name) || !detail::execute(db, "COMMIT")) {
					static_cast<void>(detail::execute(db, "ROLLBACK"));
					return { };
				}
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

		/// [[BPR-2181]] (AC06). A queued message's threads, one per line in
		/// the store's `thread` column (a thread identity holds no line break).
		[[nodiscard]] auto split_threads(const std::string& aText) -> std::vector<std::string>
		{
			auto threads = std::vector<std::string>{ };
			for (auto rest = std::string_view{ aText }; !rest.empty();) {
				const auto end = rest.find('\n');
				if (const auto thread = rest.substr(0U, end); !thread.empty()) {
					threads.emplace_back(thread);
				}
				rest = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
			}
			return threads;
		}

		[[nodiscard]] auto joined_threads(const std::vector<std::string>& aThreads) -> std::string
		{
			auto text = std::string{ };
			for (const auto& thread : aThreads) {
				if (!text.empty()) {
					text += '\n';
				}
				text += thread;
			}
			return text;
		}

		/// The most repository links one profile keeps.
		constexpr std::int64_t maximum_repository_links = 1'000;

		void bind_text(sqlite3_stmt* const aRow, const int aIndex, const std::string& aText)
		{
			sqlite3_bind_text(aRow, aIndex, aText.data(), static_cast<int>(aText.size()), SQLITE_TRANSIENT);
		}

		/// Re-review N3, N4. A scratch folder's name under the root, as the
		/// application makes it: 1 to 96 ASCII letters, digits and `-`, not
		/// starting with `-`.
		[[nodiscard]] auto scratch_folder_name(const std::string_view aName) noexcept -> bool
		{
			return !aName.empty() && aName.size() <= 96U && aName.front() != '-' && std::ranges::all_of(aName, [](const char aCharacter) {
				return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-';
			});
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
		/// The agent list's rows, inside a transaction the caller holds.
		[[nodiscard]] auto write_agents(sqlite3* const aDatabase, const application::agent_setup_preferences& aPreferences) -> bool
		{
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
				committed = detail::execute(aDatabase, "DELETE FROM agent_entry_grants").has_value();
			}
			for (std::size_t index = 0U; committed && index < aPreferences.Agents.size(); ++index) {
				const auto& entry = aPreferences.Agents[index];
				auto        row   = detail::prepare(aDatabase, "INSERT INTO agent_entry_grants VALUES(?1,?2,?3)");
				committed         = row.has_value();
				if (committed) {
					sqlite3_bind_int64(row->Value, 1, entry.Id);
					sqlite3_bind_int64(row->Value, 2, entry.ScratchStarts ? 1 : 0);
					sqlite3_bind_int64(row->Value, 3, entry.BuildsAndTests ? 1 : 0);
					committed = sqlite3_step(row->Value) == SQLITE_DONE;
				}
			}
			if (committed) {
				committed = detail::execute(aDatabase, "DELETE FROM agent_entry_launch").has_value();
			}
			for (std::size_t index = 0U; committed && index < aPreferences.Agents.size(); ++index) {
				const auto& entry = aPreferences.Agents[index];
				auto        row   = detail::prepare(aDatabase, "INSERT INTO agent_entry_launch VALUES(?1,?2,?3)");
				committed         = row.has_value();
				if (committed) {
					sqlite3_bind_int64(row->Value, 1, entry.Id);
					sqlite3_bind_int64(row->Value, 2, entry.McpServers == core::agent_mcp_servers::own ? 1 : 0);
					sqlite3_bind_int64(row->Value, 3, entry.Instructions == core::agent_instruction_delivery::message ? 1 : 0);
					committed = sqlite3_step(row->Value) == SQLITE_DONE;
				}
			}
			if (committed) {
				committed = detail::execute(aDatabase, "DELETE FROM agent_entry_checks").has_value();
			}
			for (std::size_t index = 0U; committed && index < aPreferences.EntryChecks.size(); ++index) {
				const auto& record = aPreferences.EntryChecks[index];
				auto        row    = detail::prepare(aDatabase, "INSERT INTO agent_entry_checks VALUES(?1,?2,?3,?4)");
				committed          = row.has_value();
				if (committed) {
					sqlite3_bind_int64(row->Value, 1, record.Id);
					sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(record.Check.State));
					sqlite3_bind_int64(row->Value, 3, record.Check.Failure ? static_cast<std::int64_t>(*record.Check.Failure) : -1);
					bind_text(row->Value, 4, record.Check.Version);
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
				auto policy = detail::prepare(aDatabase, "INSERT OR REPLACE INTO agent_scratch_policy VALUES(1,?1,?2)");
				committed   = policy.has_value();
				if (committed) {
					sqlite3_bind_int64(policy->Value, 1, aPreferences.ScratchLimitMiB);
					sqlite3_bind_int64(policy->Value, 2, aPreferences.ScratchKeepDays);
					committed = sqlite3_step(policy->Value) == SQLITE_DONE;
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
				auto followUp = detail::prepare(aDatabase, "INSERT OR REPLACE INTO agent_follow_up VALUES(1,?1)");
				committed     = followUp.has_value();
				if (committed) {
					sqlite3_bind_int64(followUp->Value, 1, aPreferences.FollowUpSteer ? 1 : 0);
					committed = sqlite3_step(followUp->Value) == SQLITE_DONE;
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
			return committed;
		}

		[[nodiscard]] auto store_agents(sqlite3* const aDatabase, const application::agent_setup_preferences& aPreferences) -> bool
		{
			if (!detail::execute(aDatabase, "BEGIN IMMEDIATE")) {
				return false;
			}
			if (!write_agents(aDatabase, aPreferences)) {
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
					if (auto grants = detail::prepare(store->Database.get(), "SELECT id,scratch_starts,builds FROM agent_entry_grants"); grants) {
						while (sqlite3_step(grants->Value) == SQLITE_ROW) {
							const auto id = sqlite3_column_int64(grants->Value, 0);
							for (auto& entry : loaded.Agents) {
								if (entry.Id == id) {
									entry.ScratchStarts  = sqlite3_column_int64(grants->Value, 1) != 0;
									entry.BuildsAndTests = sqlite3_column_int64(grants->Value, 2) != 0;
								}
							}
						}
					}
					if (auto launch = detail::prepare(store->Database.get(), "SELECT id,mcp_own,instructions_in_message FROM agent_entry_launch"); launch) {
						while (sqlite3_step(launch->Value) == SQLITE_ROW) {
							const auto id = sqlite3_column_int64(launch->Value, 0);
							for (auto& entry : loaded.Agents) {
								if (entry.Id == id) {
									entry.McpServers   = sqlite3_column_int64(launch->Value, 1) != 0 ? core::agent_mcp_servers::own : core::agent_mcp_servers::off;
									entry.Instructions = sqlite3_column_int64(launch->Value, 2) != 0 ? core::agent_instruction_delivery::message : core::agent_instruction_delivery::system_prompt;
								}
							}
						}
					}
					if (core::inspect_agent_entries(loaded.Agents)) {
						loaded.Agents.clear();
					}
					// [[BPR-2051]]. The kept checks of agents still in the list.
					if (auto checks = detail::prepare(store->Database.get(), "SELECT id,state,failure,version FROM agent_entry_checks ORDER BY id"); checks) {
						while (sqlite3_step(checks->Value) == SQLITE_ROW) {
							auto record        = application::agent_entry_check_record{ };
							record.Id          = static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(checks->Value, 0), 0, 64));
							record.Check.State = sqlite3_column_int64(checks->Value, 1) == 2 ? core::setup_check_state::passed : core::setup_check_state::failed;
							if (const auto failure = sqlite3_column_int64(checks->Value, 2); failure >= 0 && record.Check.State == core::setup_check_state::failed) {
								record.Check.Failure = static_cast<core::agent_setup_failure>(std::clamp<std::int64_t>(failure, 0, 7));
							} else if (record.Check.State == core::setup_check_state::failed) {
								record.Check.Failure = core::agent_setup_failure::status_unreadable;
							}
							record.Check.Version = column_text(checks->Value, 3).value_or(std::string{ });
							if (std::ranges::find(loaded.Agents, record.Id, &core::agent_entry::Id) != loaded.Agents.end()) {
								loaded.EntryChecks.push_back(std::move(record));
							}
						}
					}
					if (auto limit = detail::prepare(store->Database.get(), "SELECT minutes FROM agent_run_limit WHERE singleton=1"); limit && sqlite3_step(limit->Value) == SQLITE_ROW) {
						loaded.RunLimitMinutes = static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(limit->Value, 0), 0, 1440));
					}
					if (auto policy = detail::prepare(store->Database.get(), "SELECT limit_mib,keep_days FROM agent_scratch_policy WHERE singleton=1"); policy && sqlite3_step(policy->Value) == SQLITE_ROW) {
						loaded.ScratchLimitMiB = static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(policy->Value, 0), core::minimum_agent_scratch_limit_mib, core::maximum_agent_scratch_limit_mib));
						loaded.ScratchKeepDays = static_cast<std::uint32_t>(std::clamp<std::int64_t>(sqlite3_column_int64(policy->Value, 1), 0, core::maximum_agent_scratch_keep_days));
					}
					if (auto hidden = detail::prepare(store->Database.get(), "SELECT hidden FROM agent_suggestions WHERE singleton=1"); hidden && sqlite3_step(hidden->Value) == SQLITE_ROW) {
						loaded.SuggestionsHidden = sqlite3_column_int64(hidden->Value, 0) != 0;
					}
					if (auto followUp = detail::prepare(store->Database.get(), "SELECT steer FROM agent_follow_up WHERE singleton=1"); followUp && sqlite3_step(followUp->Value) == SQLITE_ROW) {
						loaded.FollowUpSteer = sqlite3_column_int64(followUp->Value, 0) != 0;
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
		/// [[BPR-1999]]. A native conversation's cost ledger; none recorded
		/// reads as an empty ledger.
		[[nodiscard]] auto read_native_cost(sqlite3* aDatabase, const std::string& aId) -> std::optional<core::agent_cost_ledger>
		{
			auto row = detail::prepare(aDatabase, "SELECT total,known,reported FROM native_agent_costs WHERE conversation=?1");
			if (!row) {
				return std::nullopt;
			}
			bind_text(row->Value, 1, aId);
			const auto status = sqlite3_step(row->Value);
			if (status == SQLITE_DONE) {
				return core::agent_cost_ledger{ };
			}
			if (status != SQLITE_ROW) {
				return std::nullopt;
			}
			auto ledger  = core::agent_cost_ledger{ };
			ledger.Total = static_cast<core::agent_cost>(std::max<sqlite3_int64>(0, sqlite3_column_int64(row->Value, 0)));
			ledger.Known = sqlite3_column_int(row->Value, 1) != 0;
			if (sqlite3_column_type(row->Value, 2) != SQLITE_NULL) {
				ledger.Reported = static_cast<core::agent_cost>(std::max<sqlite3_int64>(0, sqlite3_column_int64(row->Value, 2)));
			}
			return ledger;
		}

		[[nodiscard]] auto write_native_cost(sqlite3* aDatabase, const std::string& aId, const core::agent_cost_ledger& aLedger) -> bool
		{
			constexpr auto largest = static_cast<core::agent_cost>(std::numeric_limits<sqlite3_int64>::max());
			if (aLedger.Total > largest || (aLedger.Reported && *aLedger.Reported > largest)) {
				return false;
			}
			auto row = detail::prepare(aDatabase, "INSERT INTO native_agent_costs(conversation,total,known,reported) VALUES(?1,?2,?3,?4) ON CONFLICT(conversation) DO UPDATE SET total=excluded.total,known=excluded.known,reported=excluded.reported");
			if (!row) {
				return false;
			}
			bind_text(row->Value, 1, aId);
			sqlite3_bind_int64(row->Value, 2, static_cast<sqlite3_int64>(aLedger.Total));
			sqlite3_bind_int(row->Value, 3, aLedger.Known ? 1 : 0);
			if (aLedger.Reported) {
				sqlite3_bind_int64(row->Value, 4, static_cast<sqlite3_int64>(*aLedger.Reported));
			} else {
				sqlite3_bind_null(row->Value, 4);
			}
			return sqlite3_step(row->Value) == SQLITE_DONE;
		}

		[[nodiscard]] auto read_native_workspace(sqlite3* aDatabase, sqlite3_stmt* aRow)
			-> std::expected<std::optional<application::owned_agent_workspace>, application::native_agent_storage_failure>
		{
			using failure   = application::native_agent_storage_failure;
			const auto head = core::parse_git_commit_oid(column_text(aRow, 3).value_or(""));
			const auto base = core::parse_git_commit_oid(column_text(aRow, 2).value_or(""));
			if (!head || !base) {
				return std::unexpected(failure::unavailable);
			}
			const auto path = column_text(aRow, 9).value_or(std::string{ });
			if (!path.empty()) {
				auto workspace                 = application::owned_agent_workspace{ };
				workspace.Workspace.Path       = path;
				workspace.Workspace.PinnedHead = *head;
				workspace.Workspace.Shallow    = sqlite3_column_int(aRow, 17) != 0;
				if (!workspace.Workspace.Shallow) {
					workspace.RequiredBase = *base;
				}
				auto sparse = detail::prepare(aDatabase, "SELECT missing FROM native_agent_sparse_objects WHERE conversation=?1");
				if (!sparse) {
					return std::unexpected(failure::unavailable);
				}
				bind_text(sparse->Value, 1, column_text(aRow, 0).value_or(""));
				const auto sparseStatus = sqlite3_step(sparse->Value);
				if (sparseStatus == SQLITE_ROW) {
					workspace.SparseMissingObjects = column_text(sparse->Value, 0);
				} else if (sparseStatus != SQLITE_DONE) {
					return std::unexpected(failure::unavailable);
				}
				workspace.ParentDirectory    = column_text(aRow, 10).value_or(std::string{ });
				workspace.OwnershipMarker    = column_text(aRow, 11).value_or(std::string{ });
				workspace.ApplicationSession = core::application_session_id{ static_cast<std::uint64_t>(sqlite3_column_int64(aRow, 12)) };
				workspace.Session            = core::agent_session_id{ static_cast<std::uint64_t>(sqlite3_column_int64(aRow, 13)) };
				if (sqlite3_column_bytes(aRow, 14) != 32 || sqlite3_column_bytes(aRow, 16) != 32) {
					return std::unexpected(failure::unavailable);
				}
				std::memcpy(workspace.DirectoryIdentity.Opaque.data(), sqlite3_column_blob(aRow, 14), 32U);
				auto guard = application::agent_workspace_lifetime_guard{ };
				guard.Path = column_text(aRow, 15).value_or(std::string{ });
				std::memcpy(guard.FileIdentity.data(), sqlite3_column_blob(aRow, 16), 32U);
				workspace.LifetimeGuard = std::move(guard);
				return std::optional{ std::move(workspace) };
			}
			return std::optional<application::owned_agent_workspace>{ };
		}
		/// [[BPR-1753]], owner decision (b) 2026-10-23. Each pull
		class native_agent_session_store final : public application::native_agent_session_store_port
		{
		public:
			explicit native_agent_session_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }
			auto shared_workspace(const std::string& aKey, const application::native_agent_session& aSession)
				-> std::expected<std::optional<application::owned_agent_workspace>, application::native_agent_storage_failure> override
			{
				using failure = application::native_agent_storage_failure;
				if (aKey.empty() || aKey.size() > 4096U) {
					return std::unexpected(failure::unavailable);
				}
				auto connection = open(Root);
				if (!connection) {
					return std::unexpected(failure::unavailable);
				}
				auto row = detail::prepare(connection->Database.get(), "SELECT id,configuration,base,head,left_epoch,right_epoch,handle,turn,pending,workspace,parent,marker,application,session,directory,guard,guard_identity,shallow,revision FROM native_agent_sessions WHERE id=(SELECT conversation FROM native_agent_workspaces WHERE scope=?1)");
				if (!row) {
					return std::unexpected(failure::unavailable);
				}
				bind_text(row->Value, 1, aKey);
				const auto status = sqlite3_step(row->Value);
				if (status == SQLITE_DONE) {
					return aSession.Workspace;
				}
				if (status != SQLITE_ROW) {
					return std::unexpected(failure::unavailable);
				}
				if (column_text(row->Value, 2) != std::optional{ std::string{ core::canonical_hex(aSession.Binding.BaseOid).view() } }
					|| column_text(row->Value, 3) != std::optional{ std::string{ core::canonical_hex(aSession.Binding.HeadOid).view() } }) {
					return std::unexpected(failure::conflict);
				}
				auto workspace = read_native_workspace(connection->Database.get(), row->Value);
				if (!workspace || !*workspace) {
					return std::unexpected(failure::unavailable);
				}
				if (aSession.Workspace && (aSession.Workspace->Workspace.Path != (**workspace).Workspace.Path || aSession.Workspace->DirectoryIdentity != (**workspace).DirectoryIdentity || aSession.Workspace->OwnershipMarker != (**workspace).OwnershipMarker)) {
					return std::unexpected(failure::conflict);
				}
				return workspace;
			}
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
			auto read(const std::string& aScope, const std::string& aConfiguration, const core::agent_revision_binding& aBinding, const bool aCreate, const bool aStartAtCurrentRevision, const std::optional<std::string>& aChoice = std::nullopt, const std::optional<application::agent_conversation_key>& aLegacy = std::nullopt)
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
				auto       status       = sqlite3_step(row->Value);
				const auto replaying    = aChoice && aChoice->starts_with("replay:");
				auto       replayTurns  = std::vector<application::agent_conversation_turn>{ };
				auto       replaySource = std::string{ };
				if (replaying) {
					replaySource    = aChoice->substr(7U);
					auto reuseEmpty = false;
					if (replaySource.empty() && status == SQLITE_ROW && sqlite3_column_int(row->Value, 8) == 0 && sqlite3_column_int64(row->Value, 7) == 2) {
						auto unused = detail::prepare(db, "SELECT 1 FROM native_agent_replays WHERE conversation=?1 AND source='' AND NOT EXISTS(SELECT 1 FROM native_agent_turns WHERE conversation=?1)");
						if (!unused) {
							return std::unexpected(failure::unavailable);
						}
						bind_text(unused->Value, 1, column_text(row->Value, 0).value_or(""));
						const auto unusedStatus = sqlite3_step(unused->Value);
						if (unusedStatus != SQLITE_ROW && unusedStatus != SQLITE_DONE) {
							return std::unexpected(failure::unavailable);
						}
						reuseEmpty = unusedStatus == SQLITE_ROW;
					}
					auto source = decltype(detail::prepare(db, "")){ std::unexpected(detail::failure::unavailable) };
					if (replaySource.starts_with("legacy:") && aLegacy && application::valid(*aLegacy)) {
						source = detail::prepare(db, "SELECT t.asked,t.answered,t.ending,o.outcome FROM agent_conversation_turns t LEFT JOIN agent_conversation_outcomes o USING(host,owner,name,number,agent,position) WHERE t.host=?1 AND t.owner=?2 AND t.name=?3 AND t.number=?4 AND 'legacy:'||t.agent=?5 ORDER BY t.position LIMIT 50");
						if (source) {
							bind_text(source->Value, 1, aLegacy->Host);
							bind_text(source->Value, 2, aLegacy->Owner);
							bind_text(source->Value, 3, aLegacy->Name);
							sqlite3_bind_int64(source->Value, 4, static_cast<sqlite3_int64>(aLegacy->Number));
							bind_text(source->Value, 5, replaySource);
						}
					} else if (!replaySource.empty()) {
						auto origin = detail::prepare(db, "SELECT 1 FROM native_agent_sessions WHERE id=?1 AND scope=?2 AND configuration=?3");
						if (!origin) {
							return std::unexpected(failure::unavailable);
						}
						bind_text(origin->Value, 1, replaySource);
						bind_text(origin->Value, 2, aScope);
						bind_text(origin->Value, 3, aConfiguration);
						if (sqlite3_step(origin->Value) != SQLITE_ROW) {
							return std::unexpected(failure::conflict);
						}
						source = detail::prepare(db, "SELECT asked,answered,ending,outcome,seconds FROM native_agent_turns WHERE conversation=?1 ORDER BY ordinal LIMIT 50");
						if (source) {
							bind_text(source->Value, 1, replaySource);
						}
					}
					if (!replaySource.empty()) {
						if (!source) {
							return std::unexpected(failure::unavailable);
						}
						auto sourceStatus = SQLITE_ROW;
						while ((sourceStatus = sqlite3_step(source->Value)) == SQLITE_ROW) {
							auto turn          = application::agent_conversation_turn{ };
							turn.Asked         = column_text(source->Value, 0).value_or("");
							turn.Answered      = column_text(source->Value, 1).value_or("");
							turn.Ending        = column_text(source->Value, 2).value_or("");
							const auto outcome = sqlite3_column_type(source->Value, 3) == SQLITE_NULL ? std::optional{ core::legacy_agent_turn_outcome(turn.Ending) } : core::agent_turn_outcome_of(static_cast<std::uint8_t>(sqlite3_column_int(source->Value, 3)));
							if (!outcome) {
								return std::unexpected(failure::unavailable);
							}
							turn.Outcome = *outcome;
							turn.Seconds = column_seconds(source->Value, 4);
							replayTurns.push_back(std::move(turn));
						}
						if (sourceStatus != SQLITE_DONE || !application::valid(replayTurns) || (replaySource.starts_with("legacy:") && replayTurns.empty())) {
							return std::unexpected(failure::conflict);
						}
					}
					status = reuseEmpty ? SQLITE_ROW : SQLITE_DONE;
				} else if (aChoice) {
					if (aChoice->empty()) {
						if (status == SQLITE_ROW) {
							auto used = detail::prepare(db, "SELECT (SELECT count(*) FROM native_agent_turns WHERE conversation=?1)+(SELECT count(*) FROM native_agent_replays WHERE conversation=?1)");
							if (!used) {
								return std::unexpected(failure::unavailable);
							}
							bind_text(used->Value, 1, column_text(row->Value, 0).value_or(""));
							if (sqlite3_step(used->Value) != SQLITE_ROW) {
								return std::unexpected(failure::unavailable);
							}
							if (sqlite3_column_int64(used->Value, 0) > 0) {
								status = SQLITE_DONE;
							}
						}
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
					const auto workspace     = read_native_workspace(db, row->Value);
					if (!workspace) {
						return std::unexpected(workspace.error());
					}
					value.Workspace = *workspace;
					value.Revision  = static_cast<std::uint64_t>(sqlite3_column_int64(row->Value, 18));
					auto mode       = detail::prepare(db, "SELECT source FROM native_agent_replays WHERE conversation=?1");
					if (!mode) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(mode->Value, 1, value.Id);
					const auto modeStatus = sqlite3_step(mode->Value);
					if (modeStatus == SQLITE_ROW) {
						value.Replay       = true;
						value.ReplaySource = column_text(mode->Value, 0).value_or("");
					} else if (modeStatus != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
					if (value.Id.size() != 32U || value.Handle.size() > core::maximum_remote_run_id_bytes || value.Turn < 2U || value.Revision == 0U) {
						return std::unexpected(failure::unavailable);
					}
					const auto cost = read_native_cost(db, value.Id);
					if (!cost) {
						return std::unexpected(failure::unavailable);
					}
					value.Cost = *cost;
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
					if (replaying) {
						auto mode = detail::prepare(db, "INSERT INTO native_agent_replays VALUES(?1,?2)");
						if (!mode) {
							return std::unexpected(failure::unavailable);
						}
						bind_text(mode->Value, 1, value.Id);
						bind_text(mode->Value, 2, replaySource);
						if (sqlite3_step(mode->Value) != SQLITE_DONE) {
							return std::unexpected(failure::unavailable);
						}
						for (std::size_t index = 0; index < replayTurns.size(); ++index) {
							auto turn = detail::prepare(db, "INSERT INTO native_agent_turns(conversation,ordinal,asked,answered,ending,outcome,seconds) VALUES(?1,?2,?3,?4,?5,?6,?7)");
							if (!turn) {
								return std::unexpected(failure::unavailable);
							}
							bind_text(turn->Value, 1, value.Id);
							sqlite3_bind_int64(turn->Value, 2, static_cast<sqlite3_int64>(index) + 1);
							bind_text(turn->Value, 3, replayTurns[index].Asked);
							bind_text(turn->Value, 4, replayTurns[index].Answered);
							bind_text(turn->Value, 5, replayTurns[index].Ending);
							sqlite3_bind_int(turn->Value, 6, static_cast<int>(replayTurns[index].Outcome));
							bind_seconds(turn->Value, 7, replayTurns[index].Seconds);
							if (sqlite3_step(turn->Value) != SQLITE_DONE) {
								return std::unexpected(failure::unavailable);
							}
						}
						value.Replay       = true;
						value.ReplaySource = replaySource;
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
			auto replay(const std::string& aScope, const std::string& aConfiguration, const core::agent_revision_binding& aBinding, const std::string& aSource, const std::optional<application::agent_conversation_key>& aLegacy)
				-> std::expected<application::native_agent_session, application::native_agent_storage_failure> override
			{
				auto value = read(aScope, aConfiguration, aBinding, true, false, "replay:" + aSource, aLegacy);
				if (!value) {
					return std::unexpected(value.error());
				}
				return std::move(**value);
			}
			auto history(const std::string& aScope, const std::string& aConfiguration, const std::string& aView, const std::optional<application::agent_conversation_key>& aLegacy)
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
					auto mode                = detail::prepare(db, "SELECT source FROM native_agent_replays WHERE conversation=?1");
					if (!mode) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(mode->Value, 1, entry.Id);
					const auto modeStatus = sqlite3_step(mode->Value);
					if (modeStatus == SQLITE_ROW) {
						entry.Replay       = true;
						entry.ReplaySource = column_text(mode->Value, 0).value_or("");
					} else if (modeStatus != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
					if (entry.Id == view.Selected) {
						view.Replay       = entry.Replay;
						view.ReplaySource = entry.ReplaySource;
						if (const auto cost = read_native_cost(db, entry.Id); cost && cost->Known) {
							view.SessionCost = cost->Total;
						}
					}
					view.Entries.push_back(std::move(entry));
				}
				if (status != SQLITE_DONE) {
					return std::unexpected(failure::unavailable);
				}
				if (aLegacy && application::valid(*aLegacy)) {
					auto legacy = detail::prepare(db, "SELECT agent,(SELECT count(*) FROM agent_conversation_turns t WHERE t.host=c.host AND t.owner=c.owner AND t.name=c.name AND t.number=c.number AND t.agent=c.agent) FROM agent_conversations c WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 ORDER BY agent LIMIT 65");
					if (!legacy) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(legacy->Value, 1, aLegacy->Host);
					bind_text(legacy->Value, 2, aLegacy->Owner);
					bind_text(legacy->Value, 3, aLegacy->Name);
					sqlite3_bind_int64(legacy->Value, 4, static_cast<sqlite3_int64>(aLegacy->Number));
					while ((status = sqlite3_step(legacy->Value)) == SQLITE_ROW) {
						auto entry  = application::native_agent_history_entry{ };
						entry.Id    = "legacy:" + std::to_string(sqlite3_column_int64(legacy->Value, 0));
						entry.Title = "Legacy agent slot " + std::to_string(sqlite3_column_int64(legacy->Value, 0)) + " (identity unknown)";
						entry.Turns = static_cast<std::size_t>(sqlite3_column_int64(legacy->Value, 1));
						view.Legacy.push_back(std::move(entry));
					}
					if (status != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
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
				if (!aView.empty() && aView.starts_with("legacy:")) {
					if (!aLegacy || std::ranges::none_of(view.Legacy, [&](const auto& aEntry) { return aEntry.Id == aView; })) {
						return std::unexpected(failure::conflict);
					}
					auto legacyTurns = detail::prepare(db, "SELECT t.asked,t.answered,t.ending,o.outcome FROM agent_conversation_turns t LEFT JOIN agent_conversation_outcomes o USING(host,owner,name,number,agent,position) WHERE t.host=?1 AND t.owner=?2 AND t.name=?3 AND t.number=?4 AND 'legacy:'||t.agent=?5 ORDER BY position LIMIT 50");
					if (!legacyTurns) {
						return std::unexpected(failure::unavailable);
					}
					bind_text(legacyTurns->Value, 1, aLegacy->Host);
					bind_text(legacyTurns->Value, 2, aLegacy->Owner);
					bind_text(legacyTurns->Value, 3, aLegacy->Name);
					sqlite3_bind_int64(legacyTurns->Value, 4, static_cast<sqlite3_int64>(aLegacy->Number));
					bind_text(legacyTurns->Value, 5, aView);
					while ((status = sqlite3_step(legacyTurns->Value)) == SQLITE_ROW) {
						auto turn          = application::agent_conversation_turn{ };
						turn.Asked         = column_text(legacyTurns->Value, 0).value_or("");
						turn.Answered      = column_text(legacyTurns->Value, 1).value_or("");
						turn.Ending        = column_text(legacyTurns->Value, 2).value_or("");
						const auto outcome = sqlite3_column_type(legacyTurns->Value, 3) == SQLITE_NULL ? std::optional{ core::legacy_agent_turn_outcome(turn.Ending) } : core::agent_turn_outcome_of(static_cast<std::uint8_t>(sqlite3_column_int(legacyTurns->Value, 3)));
						if (!outcome) {
							return std::unexpected(failure::unavailable);
						}
						turn.Outcome = *outcome;
						view.Turns.push_back(std::move(turn));
					}
					if (status != SQLITE_DONE || !application::valid(view.Turns) || !detail::execute(db, "COMMIT")) {
						return std::unexpected(failure::unavailable);
					}
					view.ReadOnly = true;
					return view;
				}
				if (!aView.empty()) {
					if (std::ranges::none_of(view.OtherChats, [&](const auto& aEntry) { return aEntry.Id == aView; })) {
						return std::unexpected(failure::conflict);
					}
					view.ReadOnly = true;
				}
				auto turns  = detail::prepare(db, "SELECT asked,answered,ending,outcome,ordinal,seconds FROM native_agent_turns WHERE conversation=?1 ORDER BY ordinal LIMIT 50");
				auto steers = detail::prepare(db, "SELECT text,at,steps,disposition FROM native_agent_turn_steers WHERE conversation=?1 AND ordinal=?2 ORDER BY position LIMIT 16");
				if (!turns || !steers) {
					return std::unexpected(failure::unavailable);
				}
				const auto conversation = aView.empty() ? view.Selected : aView;
				bind_text(turns->Value, 1, conversation);
				while ((status = sqlite3_step(turns->Value)) == SQLITE_ROW) {
					auto turn     = application::agent_conversation_turn{ };
					turn.Asked    = column_text(turns->Value, 0).value_or("");
					turn.Answered = column_text(turns->Value, 1).value_or("");
					turn.Ending   = column_text(turns->Value, 2).value_or("");
					turn.Outcome  = static_cast<core::agent_turn_outcome>(sqlite3_column_int(turns->Value, 3));
					turn.Seconds  = column_seconds(turns->Value, 5);
					sqlite3_reset(steers->Value);
					bind_text(steers->Value, 1, conversation);
					sqlite3_bind_int64(steers->Value, 2, sqlite3_column_int64(turns->Value, 4));
					auto steered = SQLITE_ROW;
					while ((steered = sqlite3_step(steers->Value)) == SQLITE_ROW) {
						turn.Steered.push_back({ .Text        = column_text(steers->Value, 0).value_or(""),
												 .At          = static_cast<std::size_t>(sqlite3_column_int64(steers->Value, 1)),
												 .AfterSteps  = static_cast<std::uint32_t>(sqlite3_column_int64(steers->Value, 2)),
												 .Disposition = static_cast<core::agent_steer_disposition>(sqlite3_column_int(steers->Value, 3)) });
					}
					if (steered != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
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
				auto  row = detail::prepare(db, "INSERT INTO native_agent_turns(conversation,ordinal,asked,answered,ending,outcome,seconds) SELECT ?1,coalesce(max(ordinal),0)+1,?2,?3,?4,?5,?6 FROM native_agent_turns WHERE conversation=?1");
				if (!row) {
					return false;
				}
				bind_text(row->Value, 1, aId);
				bind_text(row->Value, 2, aTurn.Asked);
				bind_text(row->Value, 3, aTurn.Answered);
				bind_text(row->Value, 4, aTurn.Ending);
				sqlite3_bind_int(row->Value, 5, static_cast<int>(aTurn.Outcome));
				bind_seconds(row->Value, 6, aTurn.Seconds);
				if (sqlite3_step(row->Value) != SQLITE_DONE) {
					return false;
				}
				// [[BPR-2153]]. Its steered messages, under the turn just written.
				if (!aTurn.Steered.empty()) {
					auto steer = detail::prepare(db, "INSERT INTO native_agent_turn_steers SELECT ?1,max(ordinal),?2,?3,?4,?5,?6 FROM native_agent_turns WHERE conversation=?1");
					if (!steer) {
						return false;
					}
					for (std::size_t position = 0U; position < aTurn.Steered.size(); ++position) {
						const auto& steered = aTurn.Steered[position];
						sqlite3_reset(steer->Value);
						bind_text(steer->Value, 1, aId);
						sqlite3_bind_int64(steer->Value, 2, static_cast<sqlite3_int64>(position));
						bind_text(steer->Value, 3, steered.Text);
						sqlite3_bind_int64(steer->Value, 4, static_cast<sqlite3_int64>(steered.At));
						sqlite3_bind_int64(steer->Value, 5, static_cast<sqlite3_int64>(steered.AfterSteps));
						sqlite3_bind_int(steer->Value, 6, static_cast<int>(steered.Disposition));
						if (sqlite3_step(steer->Value) != SQLITE_DONE) {
							return false;
						}
					}
				}
				auto prune       = detail::prepare(db, "DELETE FROM native_agent_turns WHERE conversation=?1 AND ordinal NOT IN (SELECT ordinal FROM native_agent_turns WHERE conversation=?1 ORDER BY ordinal DESC LIMIT 50)");
				auto pruneSteers = detail::prepare(db, "DELETE FROM native_agent_turn_steers WHERE conversation=?1 AND ordinal NOT IN (SELECT ordinal FROM native_agent_turns WHERE conversation=?1)");
				if (!prune || !pruneSteers) {
					return false;
				}
				bind_text(prune->Value, 1, aId);
				bind_text(pruneSteers->Value, 1, aId);
				return sqlite3_step(prune->Value) == SQLITE_DONE && sqlite3_step(pruneSteers->Value) == SQLITE_DONE && detail::execute(db, "COMMIT");
			}
			auto store(application::native_agent_session& aSession) -> bool override
			{
				if (aSession.Id.size() != 32U || aSession.Handle.size() > core::maximum_remote_run_id_bytes || aSession.Turn < 2U
					|| aSession.Turn > static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max()) || aSession.Revision == 0U) {
					return false;
				}
				auto connection = open(Root);
				if (!connection || !detail::execute(connection->Database.get(), "BEGIN IMMEDIATE")) {
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
				if (workspace.SparseMissingObjects && workspace.SparseMissingObjects->size() > 64U * 1024U * 1024U) {
					return false;
				}
				if (!write_native_cost(connection->Database.get(), aSession.Id, aSession.Cost)) {
					return false;
				}
				auto sparse = detail::prepare(connection->Database.get(), "INSERT INTO native_agent_sparse_objects(conversation,missing) VALUES(?1,?2) ON CONFLICT(conversation) DO UPDATE SET missing=excluded.missing");
				if (!sparse) {
					return false;
				}
				bind_text(sparse->Value, 1, aSession.Id);
				if (workspace.SparseMissingObjects) {
					bind_text(sparse->Value, 2, *workspace.SparseMissingObjects);
					if (sqlite3_step(sparse->Value) != SQLITE_DONE) {
						return false;
					}
				} else {
					auto remove = detail::prepare(connection->Database.get(), "DELETE FROM native_agent_sparse_objects WHERE conversation=?1");
					if (!remove) {
						return false;
					}
					bind_text(remove->Value, 1, aSession.Id);
					if (sqlite3_step(remove->Value) != SQLITE_DONE) {
						return false;
					}
				}
				if (!aSession.WorkspaceKey.empty()) {
					if (!aSession.Workspace || aSession.WorkspaceKey.size() > 4096U) {
						return false;
					}
					auto shared = detail::prepare(connection->Database.get(), "INSERT INTO native_agent_workspaces(scope,conversation) VALUES(?1,?2) ON CONFLICT(scope) DO NOTHING");
					if (!shared) {
						return false;
					}
					bind_text(shared->Value, 1, aSession.WorkspaceKey);
					bind_text(shared->Value, 2, aSession.Id);
					if (sqlite3_step(shared->Value) != SQLITE_DONE) {
						return false;
					}
					auto exact = detail::prepare(connection->Database.get(), "SELECT 1 FROM native_agent_sessions WHERE id=(SELECT conversation FROM native_agent_workspaces WHERE scope=?1) AND workspace=?2 AND marker=?3 AND directory=?4");
					if (!exact) {
						return false;
					}
					bind_text(exact->Value, 1, aSession.WorkspaceKey);
					bind_text(exact->Value, 2, workspace.Workspace.Path);
					bind_text(exact->Value, 3, workspace.OwnershipMarker);
					sqlite3_bind_blob(exact->Value, 4, workspace.DirectoryIdentity.Opaque.data(), 32, SQLITE_TRANSIENT);
					if (sqlite3_step(exact->Value) != SQLITE_ROW) {
						return false;
					}
				}
				if (!detail::execute(connection->Database.get(), "COMMIT")) {
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
					const auto key = [&](sqlite3_stmt* aRow) { bind_key(aRow, aKey); };
					if (!aTurns.empty()) {
						auto existing = detail::prepare(db, "SELECT 1 FROM agent_conversations WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5");
						if (!existing) {
							return false;
						}
						bind_key(existing->Value, aKey);
						const auto exists = sqlite3_step(existing->Value);
						if (exists != SQLITE_ROW && exists != SQLITE_DONE) {
							return false;
						}
						const auto count = detail::scalar_integer(db, "SELECT (SELECT count(*) FROM native_agent_sessions)+(SELECT count(*) FROM agent_conversations)");
						if (!count || (exists == SQLITE_DONE && *count >= static_cast<std::int64_t>(application::maximum_conversations))) {
							return false;
						}
					}
					auto committed = run("DELETE FROM agent_conversation_turns WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5", key)
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
					// [[BPR-2007]]. A reset's new agent may reuse an old id: it
					// does not inherit the old one's scratch choices either.
					return store && detail::execute(store->Database.get(), "DELETE FROM agent_conversation_choice")
						&& detail::execute(store->Database.get(), "DELETE FROM agent_scratch_choice") && detail::execute(store->Database.get(), "DELETE FROM agent_scratch_folder");
				} catch (...) {
					return false;
				}
			}

			[[nodiscard]] auto load_scratch_choice(const application::agent_conversation_key& aKey) -> std::optional<bool> override
			{
				return load_flag(aKey, "SELECT scratch FROM agent_scratch_choice WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5");
			}

			[[nodiscard]] auto store_scratch_choice(const application::agent_conversation_key& aKey, const bool aOn) -> bool override
			{
				return store_flag(aKey, "INSERT OR REPLACE INTO agent_scratch_choice VALUES(?1,?2,?3,?4,?5,?6)", 6, aOn);
			}

			[[nodiscard]] auto load_build_answer(const application::agent_conversation_key& aKey) -> std::optional<bool> override
			{
				return load_flag(aKey, "SELECT allowed FROM agent_build_answer WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4");
			}

			[[nodiscard]] auto store_build_answer(const application::agent_conversation_key& aKey, const bool aAllowed) -> bool override
			{
				return store_flag(aKey, "INSERT OR REPLACE INTO agent_build_answer VALUES(?1,?2,?3,?4,?5)", 5, aAllowed);
			}

			[[nodiscard]] auto load_scratch_folder(const application::agent_conversation_key& aKey) -> std::optional<std::string> override
			{
				if (!application::valid(aKey) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					return std::nullopt;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT folder FROM agent_scratch_folder WHERE host=?1 AND owner=?2 AND name=?3 AND number=?4 AND agent=?5");
					if (!row) {
						return std::nullopt;
					}
					bind_key(row->Value, aKey);
					if (sqlite3_step(row->Value) != SQLITE_ROW) {
						return std::nullopt;
					}
					const auto* const text = reinterpret_cast<const char*>(sqlite3_column_text(row->Value, 0));
					auto              name = text != nullptr ? std::string{ text, static_cast<std::size_t>(sqlite3_column_bytes(row->Value, 0)) } : std::string{ };
					return scratch_folder_name(name) ? std::optional{ std::move(name) } : std::nullopt;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store_scratch_folder(const application::agent_conversation_key& aKey, const std::string_view aFolder) -> bool override
			{
				if (!application::valid(aKey) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) || !scratch_folder_name(aFolder)) {
					return false;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto row = detail::prepare(store->Database.get(), "INSERT OR REPLACE INTO agent_scratch_folder VALUES(?1,?2,?3,?4,?5,?6)");
					if (!row) {
						return false;
					}
					bind_key(row->Value, aKey);
					bind_text(row->Value, 6, std::string{ aFolder });
					return sqlite3_step(row->Value) == SQLITE_DONE;
				} catch (...) {
					return false;
				}
			}

		private:
			/// One 0/1 column for a pull request key; `bind_key` binds the
			/// agent as ?5 only where the statement has it.
			[[nodiscard]] auto load_flag(const application::agent_conversation_key& aKey, const char* aSql) const -> std::optional<bool>
			{
				if (!application::valid(aKey) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					return std::nullopt;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), aSql);
					if (!row) {
						return std::nullopt;
					}
					bind_key(row->Value, aKey);
					if (sqlite3_step(row->Value) != SQLITE_ROW) {
						return std::nullopt;
					}
					return sqlite3_column_int64(row->Value, 0) != 0;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store_flag(const application::agent_conversation_key& aKey, const char* aSql, const int aColumn, const bool aValue) const -> bool
			{
				if (!application::valid(aKey) || aKey.Number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
					return false;
				}
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto row = detail::prepare(store->Database.get(), aSql);
					if (!row) {
						return false;
					}
					bind_key(row->Value, aKey);
					sqlite3_bind_int64(row->Value, aColumn, aValue ? 1 : 0);
					return sqlite3_step(row->Value) == SQLITE_DONE;
				} catch (...) {
					return false;
				}
			}

		public:
		private:
			std::filesystem::path Root;
		};

		/// Owner report 2026-11-10, after BPR-2052. What was done with each
		/// finding card of a stored answer, so a restart makes it again so.
		class agent_finding_card_store final : public application::agent_finding_card_store_port
		{
		public:
			explicit agent_finding_card_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load() -> std::optional<std::vector<core::agent_finding_card_record>> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT findings,ordinal,state,why,note FROM agent_finding_cards ORDER BY changed LIMIT 4096");
					if (!row) {
						return std::nullopt;
					}
					auto records = std::vector<core::agent_finding_card_record>{ };
					for (auto step = sqlite3_step(row->Value); step != SQLITE_DONE; step = sqlite3_step(row->Value)) {
						if (step != SQLITE_ROW) {
							return std::nullopt;
						}
						const auto* const findings = static_cast<const std::uint8_t*>(sqlite3_column_blob(row->Value, 0));
						const auto        why      = column_text(row->Value, 3);
						if (findings == nullptr || sqlite3_column_bytes(row->Value, 0) != 32 || !why) {
							continue;
						}
						auto record = core::agent_finding_card_record{ };
						std::copy_n(findings, record.Findings.size(), record.Findings.begin());
						record.Ordinal = static_cast<std::uint32_t>(sqlite3_column_int64(row->Value, 1));
						record.State   = static_cast<core::agent_finding_card_disposition>(sqlite3_column_int64(row->Value, 2));
						record.Why     = std::move(*why);
						record.Note    = static_cast<std::uint64_t>(sqlite3_column_int64(row->Value, 4));
						records.push_back(core::normalized(std::move(record)));
					}
					return records;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store(const core::agent_finding_card_record& aRecord) -> bool override
			{
				const auto record = core::normalized(aRecord);
				if (static_cast<std::size_t>(record.State) >= core::agent_finding_card_disposition_count || record.Note > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
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
					const auto bindCard = [&record](sqlite3_stmt* const aRow) {
						sqlite3_bind_blob(aRow, 1, record.Findings.data(), static_cast<int>(record.Findings.size()), SQLITE_TRANSIENT);
						sqlite3_bind_int64(aRow, 2, static_cast<std::int64_t>(record.Ordinal));
					};
					auto committed = false;
					if (record.State == core::agent_finding_card_disposition::open) {
						auto row = detail::prepare(db, "DELETE FROM agent_finding_cards WHERE findings=?1 AND ordinal=?2");
						if (row) {
							bindCard(row->Value);
							committed = sqlite3_step(row->Value) == SQLITE_DONE;
						}
					} else {
						auto row = detail::prepare(db, "INSERT OR REPLACE INTO agent_finding_cards SELECT ?1,?2,?3,?4,?5,coalesce(max(changed)+1,0) FROM agent_finding_cards");
						if (row) {
							bindCard(row->Value);
							sqlite3_bind_int64(row->Value, 3, static_cast<std::int64_t>(record.State));
							bind_text(row->Value, 4, record.Why);
							sqlite3_bind_int64(row->Value, 5, static_cast<std::int64_t>(record.Note));
							committed = sqlite3_step(row->Value) == SQLITE_DONE;
						}
						// The least recently changed cards go first.
						committed = committed && detail::execute(db, "DELETE FROM agent_finding_cards WHERE rowid NOT IN (SELECT rowid FROM agent_finding_cards ORDER BY changed DESC LIMIT 4096)");
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
			std::filesystem::path Root;
		};

		/// RFC-BPR-0125 (AC07). The names suggested prompts' turns were
		/// sent under.
		class prompt_turn_name_store final : public application::prompt_turn_name_store_port
		{
		public:
			explicit prompt_turn_name_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load() -> std::optional<std::vector<core::prompt_turn_name>> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT asked,name FROM prompt_turn_names ORDER BY changed LIMIT 256");
					if (!row) {
						return std::nullopt;
					}
					auto names = std::vector<core::prompt_turn_name>{ };
					for (auto step = sqlite3_step(row->Value); step != SQLITE_DONE; step = sqlite3_step(row->Value)) {
						if (step != SQLITE_ROW) {
							return std::nullopt;
						}
						auto asked = column_text(row->Value, 0);
						auto name  = column_text(row->Value, 1);
						if (!asked || !name) {
							continue;
						}
						auto kept  = core::prompt_turn_name{ };
						kept.Asked = std::move(*asked);
						kept.Name  = std::move(*name);
						if (core::valid(kept)) {
							names.push_back(std::move(kept));
						}
					}
					return names;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store(const core::prompt_turn_name& aName) -> bool override
			{
				if (!core::valid(aName)) {
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
					auto committed = false;
					if (auto row = detail::prepare(db, "INSERT OR REPLACE INTO prompt_turn_names SELECT ?1,?2,coalesce(max(changed)+1,0) FROM prompt_turn_names"); row) {
						bind_text(row->Value, 1, aName.Asked);
						bind_text(row->Value, 2, aName.Name);
						committed = sqlite3_step(row->Value) == SQLITE_DONE;
					}
					// The least recently sent go first.
					committed = committed && detail::execute(db, "DELETE FROM prompt_turn_names WHERE rowid NOT IN (SELECT rowid FROM prompt_turn_names ORDER BY changed DESC LIMIT 256)");
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
			std::filesystem::path Root;
		};

		/// RFC-BPR-0125 milestone 3 ([[BPR-2146]]). The enum written `aName`
		/// among `aCount` values named by `stable_name`, if this build knows
		/// it.
		template <typename Enum>
		[[nodiscard]] auto named_value(const std::string_view aName, const std::size_t aCount) -> std::optional<Enum>
		{
			for (auto index = std::size_t{ 0 }; index < aCount; ++index) {
				const auto value = static_cast<Enum>(index);
				if (core::stable_name(value) == aName) {
					return value;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto lines_name(const core::prompt_lines_value aLines) -> std::string_view
		{
			switch (aLines) {
			case core::prompt_lines_value::any:     return "any";
			case core::prompt_lines_value::one:     return "one";
			case core::prompt_lines_value::several: return "several";
			}
			return "any";
		}

		/// Conditions as text: one per line, `fact`, `is` or `not`, the lines
		/// value, then each pattern, separated by the unit separator. A
		/// pattern holding a control character is not stored.
		/// One condition as a line: `fact`, `is` or `not`, the lines value (or,
		/// for `thread-activity`, what it counts), then each pattern.
		[[nodiscard]] auto encode_condition(std::string& aText, const core::prompt_condition& aCondition) -> bool
		{
			if (!aText.empty()) {
				aText += '\n';
			}
			aText += core::stable_name(aCondition.Fact);
			aText += '\x1f';
			aText += aCondition.Negated ? "not" : "is";
			aText += '\x1f';
			if (aCondition.Fact == core::prompt_fact::thread_activity) {
				aText += aCondition.Activity == core::prompt_activity_value::replies ? "replies" : aCondition.Activity == core::prompt_activity_value::pushes ? "pushes"
																																							  : "both";
			} else {
				aText += lines_name(aCondition.Lines);
			}
			for (const auto& pattern : aCondition.Patterns) {
				if (std::ranges::any_of(pattern, [](const char aCharacter) { return static_cast<unsigned char>(aCharacter) < 0x20U; })) {
					return false;
				}
				aText += '\x1f';
				aText += pattern;
			}
			return true;
		}

		[[nodiscard]] auto encode_groups(std::string& aText, const std::vector<core::prompt_condition_group>& aGroups) -> bool
		{
			for (const auto& group : aGroups) {
				aText += aText.empty() ? "(" : "\n(";
				aText += '\x1f';
				aText += core::stable_name(group.Match);
				for (const auto& condition : group.Conditions) {
					if (!encode_condition(aText, condition)) {
						return false;
					}
				}
				if (!encode_groups(aText, group.Groups)) {
					return false;
				}
				aText += "\n)";
			}
			return true;
		}

		/// [[BPR-2179]]. "Any of these" or "None of these" leads as a line of
		/// its own; a nested group opens with `(` and its match and closes
		/// with `)`. An earlier build reads any of these as a newer build's row
		/// and keeps it.
		/// [[BPR-2179]] look prompts-v003: then "hide" or "disable", what the
		/// prompt's row does while they do not hold; a row without it (an
		/// earlier build's) takes it from its conditions' lean.
		[[nodiscard]] auto encode_conditions(const std::vector<core::prompt_condition>& aConditions, const core::prompt_match aMatch = core::prompt_match::all,
											 const std::vector<core::prompt_condition_group>& aGroups = { }, const std::optional<core::prompt_when_unmet> aWhen = std::nullopt)
			-> std::optional<std::string>
		{
			auto text = std::string{ aMatch == core::prompt_match::all ? "" : core::stable_name(aMatch) };
			if (aWhen) {
				text += text.empty() ? "" : "\n";
				text += core::stable_name(*aWhen);
			}
			for (const auto& condition : aConditions) {
				if (!encode_condition(text, condition)) {
					return std::nullopt;
				}
			}
			if (!encode_groups(text, aGroups)) {
				return std::nullopt;
			}
			return text;
		}

		[[nodiscard]] auto split(const std::string_view aText, const char aSeparator) -> std::vector<std::string_view>
		{
			auto parts = std::vector<std::string_view>{ };
			auto start = std::size_t{ 0 };
			while (start <= aText.size()) {
				const auto end = std::min(aText.find(aSeparator, start), aText.size());
				parts.push_back(aText.substr(start, end - start));
				start = end + 1U;
			}
			return parts;
		}

		[[nodiscard]] auto decode_condition(const std::string_view aLine) -> std::optional<core::prompt_condition>
		{
			const auto fields = split(aLine, '\x1f');
			if (fields.size() < 3U) {
				return std::nullopt;
			}
			const auto fact = named_value<core::prompt_fact>(fields[0], core::prompt_fact_count);
			if (!fact || (fields[1] != "is" && fields[1] != "not")) {
				return std::nullopt;
			}
			auto condition = core::prompt_condition{ .Fact = *fact, .Negated = fields[1] == "not" };
			if (*fact == core::prompt_fact::thread_activity) {
				if (fields[2] == "replies") {
					condition.Activity = core::prompt_activity_value::replies;
				} else if (fields[2] == "pushes") {
					condition.Activity = core::prompt_activity_value::pushes;
				} else if (fields[2] != "both") {
					return std::nullopt;
				}
			} else if (fields[2] == "one") {
				condition.Lines = core::prompt_lines_value::one;
			} else if (fields[2] == "several") {
				condition.Lines = core::prompt_lines_value::several;
			} else if (fields[2] != "any") {
				return std::nullopt;
			}
			for (auto index = std::size_t{ 3 }; index < fields.size(); ++index) {
				condition.Patterns.emplace_back(fields[index]);
			}
			return condition;
		}

		[[nodiscard]] auto match_named(const std::string_view aName) -> std::optional<core::prompt_match>
		{
			for (const auto match : { core::prompt_match::all, core::prompt_match::any, core::prompt_match::none }) {
				if (core::stable_name(match) == aName) {
					return match;
				}
			}
			return std::nullopt;
		}

		/// The list's conditions; its match and nested groups go to `aMatch`
		/// and `aGroups` when given. A line this build cannot read refuses
		/// the whole text.
		[[nodiscard]] auto decode_conditions(std::string_view aText, core::prompt_match* const aMatch = nullptr, std::vector<core::prompt_condition_group>* const aGroups = nullptr,
											 std::optional<core::prompt_when_unmet>* const aWhen = nullptr) -> std::optional<std::vector<core::prompt_condition>>
		{
			auto       conditions = std::vector<core::prompt_condition>{ };
			const auto leads      = [&aText](const std::string_view aHeader) {
				if (aText == aHeader || (aText.starts_with(aHeader) && aText.size() > aHeader.size() && aText[aHeader.size()] == '\n')) {
					aText.remove_prefix(std::min(aText.size(), aHeader.size() + 1U));
					return true;
				}
				return false;
			};
			for (const auto header : { std::string_view{ "any" }, std::string_view{ "none" } }) {
				if (leads(header) && aMatch != nullptr) {
					*aMatch = *match_named(header);
				}
			}
			for (const auto when : { core::prompt_when_unmet::hide, core::prompt_when_unmet::disable }) {
				if (leads(core::stable_name(when)) && aWhen != nullptr) {
					*aWhen = when;
				}
			}
			if (aText.empty()) {
				return conditions;
			}
			// The open groups, innermost last; the list itself is none of them.
			auto top  = core::prompt_condition_group{ };
			auto open = std::vector<core::prompt_condition_group*>{ &top };
			for (const auto line : split(aText, '\n')) {
				if (line.starts_with("(")) {
					const auto fields = split(line, '\x1f');
					const auto match  = fields.size() == 2U && fields[0] == "(" ? match_named(fields[1]) : std::nullopt;
					if (!match || open.size() > core::maximum_prompt_condition_depth) {
						return std::nullopt;
					}
					open.back()->Groups.push_back({ .Match = *match });
					open.push_back(&open.back()->Groups.back());
					continue;
				}
				if (line == ")") {
					if (open.size() < 2U) {
						return std::nullopt;
					}
					open.pop_back();
					continue;
				}
				auto condition = decode_condition(line);
				if (!condition) {
					return std::nullopt;
				}
				open.back()->Conditions.push_back(std::move(*condition));
			}
			if (open.size() != 1U || (!top.Groups.empty() && aGroups == nullptr)) {
				return std::nullopt;
			}
			if (aGroups != nullptr) {
				*aGroups = std::move(top.Groups);
			}
			return std::move(top.Conditions);
		}

		[[nodiscard]] auto scope_bits(const core::prompt_scopes aScopes) -> std::int64_t { return (aScopes.Thread ? 1 : 0) | (aScopes.File ? 2 : 0) | (aScopes.PullRequest ? 4 : 0); }

		/// RFC-BPR-0125 milestone 3 ([[BPR-2146]]). The profile's suggested
		/// prompts, as differences from the shipped set.
		class prompt_macro_store final : public application::prompt_macro_store_port
		{
		public:
			explicit prompt_macro_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load() -> std::optional<application::prompt_macro_stored> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					return read(store->Database.get());
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store(const core::prompt_macro_differences& aDifferences, const std::uint64_t aFrom) -> application::prompt_macro_write override
			{
				using application::prompt_macro_write;
				try {
					auto store = open(Root);
					if (!store) {
						return prompt_macro_write::failed;
					}
					auto* const db = store->Database.get();
					if (!detail::execute(db, "BEGIN IMMEDIATE")) {
						return prompt_macro_write::failed;
					}
					const auto rollback = [db](const prompt_macro_write aOutcome) {
						static_cast<void>(detail::execute(db, "ROLLBACK"));
						return aOutcome;
					};
					const auto outcome = write_differences(db, aDifferences, aFrom);
					if (outcome != prompt_macro_write::stored) {
						return rollback(outcome);
					}
					return detail::execute(db, "COMMIT") ? prompt_macro_write::stored : rollback(prompt_macro_write::failed);
				} catch (...) {
					return application::prompt_macro_write::failed;
				}
			}

			/// RFC-BPR-0125 AC24. Writes the differences inside the caller's
			/// transaction when the store is at `aFrom`, and moves it on one;
			/// the import writes the prompts this way in the same transaction
			/// as the rest of what it changes.
			[[nodiscard]] static auto write_differences(sqlite3* const aDatabase, const core::prompt_macro_differences& aDifferences, const std::uint64_t aFrom) -> application::prompt_macro_write
			{
				using application::prompt_macro_write;
				const auto current = read(aDatabase);
				if (!current) {
					return prompt_macro_write::failed;
				}
				if (current->Revision != aFrom) {
					return prompt_macro_write::stale;
				}
				// Rows this build cannot read stay as they are.
				if (!erase_known(aDatabase, current->Newer)) {
					return prompt_macro_write::failed;
				}
				auto position = std::int64_t{ 0 };
				for (const auto& change : aDifferences.Changes) {
					if (!write_row(aDatabase, 0, change.Id, position++, std::nullopt, change.Body ? &*change.Body : nullptr, change.On, change.WrittenAgainst)) {
						return prompt_macro_write::failed;
					}
				}
				position = 0;
				for (const auto& own : aDifferences.Own) {
					if (own.BuiltIn || own.Action != core::prompt_action::none || !write_row(aDatabase, 1, own.Id, position++, own.Place, &own.Body, own.On, 0U)) {
						return prompt_macro_write::failed;
					}
				}
				for (auto place = std::size_t{ 0 }; place < core::prompt_place_count; ++place) {
					auto erase = detail::prepare(aDatabase, "DELETE FROM prompt_macro_orders WHERE place=?1");
					if (!erase) {
						return prompt_macro_write::failed;
					}
					bind_text(erase->Value, 1, std::string{ core::stable_name(static_cast<core::prompt_place>(place)) });
					if (sqlite3_step(erase->Value) != SQLITE_DONE) {
						return prompt_macro_write::failed;
					}
				}
				for (const auto& order : aDifferences.Orders) {
					for (auto index = std::size_t{ 0 }; index < order.Ids.size() && index < core::maximum_prompt_macros; ++index) {
						auto row = detail::prepare(aDatabase, "INSERT INTO prompt_macro_orders(place,position,id) VALUES(?1,?2,?3)");
						if (!row) {
							return prompt_macro_write::failed;
						}
						bind_text(row->Value, 1, std::string{ core::stable_name(order.Place) });
						sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(index));
						bind_text(row->Value, 3, order.Ids[index]);
						if (sqlite3_step(row->Value) != SQLITE_DONE) {
							return prompt_macro_write::failed;
						}
					}
				}
				if (!write_surfaces(aDatabase, aDifferences.Surfaces) || !write_layers(aDatabase, aDifferences.Repositories) || !write_links(aDatabase, aDifferences.Own, current->Newer)) {
					return prompt_macro_write::failed;
				}
				auto revision = detail::prepare(aDatabase, "INSERT OR REPLACE INTO prompt_macro_revision(singleton,revision) VALUES(1,?1)");
				if (!revision) {
					return prompt_macro_write::failed;
				}
				sqlite3_bind_int64(revision->Value, 1, static_cast<std::int64_t>(aFrom + 1U));
				if (sqlite3_step(revision->Value) != SQLITE_DONE) {
					return prompt_macro_write::failed;
				}
				return prompt_macro_write::stored;
			}

			/// The stored differences, read inside the caller's transaction.
			[[nodiscard]] static auto read(sqlite3* const aDatabase) -> std::optional<application::prompt_macro_stored>
			{
				auto stored = application::prompt_macro_stored{ };
				if (const auto revision = detail::scalar_integer(aDatabase, "SELECT coalesce((SELECT revision FROM prompt_macro_revision WHERE singleton=1),0)"); revision) {
					stored.Revision = static_cast<std::uint64_t>(*revision);
				} else {
					return std::nullopt;
				}
				auto row = detail::prepare(aDatabase,
										   "SELECT kind,id,place,has_body,name,template,subject,ask,conditions,quick,scopes,on_state,written_against FROM prompt_macros ORDER BY kind,position");
				if (!row) {
					return std::nullopt;
				}
				for (auto step = sqlite3_step(row->Value); step != SQLITE_DONE; step = sqlite3_step(row->Value)) {
					if (step != SQLITE_ROW) {
						return std::nullopt;
					}
					const auto kind  = sqlite3_column_int64(row->Value, 0);
					auto       id    = column_text(row->Value, 1).value_or(std::string{ });
					const auto place = column_text(row->Value, 2).value_or(std::string{ });
					const auto body  = sqlite3_column_int64(row->Value, 3) != 0;
					auto       macro = core::prompt_macro{ .Id = id };
					auto       known = true;
					if (kind == 1) {
						const auto where = named_value<core::prompt_place>(place, core::prompt_place_count);
						known            = where.has_value();
						macro.Place      = where.value_or(core::prompt_place::agent_tab);
					}
					if (body) {
						macro.Body.Name       = column_text(row->Value, 4).value_or(std::string{ });
						macro.Body.Template   = column_text(row->Value, 5).value_or(std::string{ });
						const auto subject    = named_value<core::prompt_subject>(column_text(row->Value, 6).value_or(std::string{ }), 9U);
						const auto ask        = named_value<core::prompt_ask>(column_text(row->Value, 7).value_or(std::string{ }), 2U);
						auto       match      = core::prompt_match::all;
						auto       groups     = std::vector<core::prompt_condition_group>{ };
						auto       unmet      = std::optional<core::prompt_when_unmet>{ };
						auto       when       = decode_conditions(column_text(row->Value, 8).value_or(std::string{ }), &match, &groups, &unmet);
						macro.Body.WhenUnmet  = unmet.value_or(when ? core::migrated_when_unmet(*when, groups) : core::prompt_when_unmet::disable);
						macro.Body.Groups     = std::move(groups);
						macro.Body.Match      = match;
						known                 = known && subject && ask && when;
						macro.Body.Subject    = subject.value_or(core::prompt_subject::pull_request);
						macro.Body.Kind       = ask.value_or(core::prompt_ask::question);
						macro.Body.Conditions = when.value_or(std::vector<core::prompt_condition>{ });
						macro.Body.QuickAsk   = sqlite3_column_int64(row->Value, 9) != 0;
						const auto bits       = sqlite3_column_int64(row->Value, 10);
						macro.Body.Scopes     = { .Thread = (bits & 1) != 0, .File = (bits & 2) != 0, .PullRequest = (bits & 4) != 0 };
					}
					const auto on = sqlite3_column_type(row->Value, 11) == SQLITE_NULL ? std::optional<bool>{ } : std::optional{ sqlite3_column_int64(row->Value, 11) != 0 };
					if (!known) {
						stored.Newer.push_back(std::move(id));
						continue;
					}
					if (kind == 0) {
						auto change = core::prompt_builtin_change{ .Id = std::move(id), .On = on, .WrittenAgainst = static_cast<std::uint32_t>(sqlite3_column_int64(row->Value, 12)) };
						if (body) {
							change.Body = std::move(macro.Body);
						}
						stored.Differences.Changes.push_back(std::move(change));
					} else {
						macro.On = on.value_or(true);
						stored.Differences.Own.push_back(std::move(macro));
					}
				}
				auto order = detail::prepare(aDatabase, "SELECT place,id FROM prompt_macro_orders ORDER BY place,position");
				if (!order) {
					return std::nullopt;
				}
				for (auto step = sqlite3_step(order->Value); step != SQLITE_DONE; step = sqlite3_step(order->Value)) {
					if (step != SQLITE_ROW) {
						return std::nullopt;
					}
					const auto place = named_value<core::prompt_place>(column_text(order->Value, 0).value_or(std::string{ }), core::prompt_place_count);
					auto       id    = column_text(order->Value, 1);
					if (!place || !id) {
						continue;
					}
					auto found = std::ranges::find(stored.Differences.Orders, *place, &core::prompt_place_order::Place);
					if (found == stored.Differences.Orders.end()) {
						stored.Differences.Orders.push_back({ .Place = *place });
						found = std::prev(stored.Differences.Orders.end());
					}
					found->Ids.push_back(std::move(*id));
				}
				if (!read_layers(aDatabase, stored.Differences.Repositories) || !read_links(aDatabase, stored.Differences.Own)) {
					return std::nullopt;
				}
				auto surfaces = detail::prepare(aDatabase, "SELECT surface,ids,removed FROM prompt_surface_lists WHERE layer='' ORDER BY surface");
				if (!surfaces) {
					return std::nullopt;
				}
				for (auto step = sqlite3_step(surfaces->Value); step != SQLITE_DONE; step = sqlite3_step(surfaces->Value)) {
					if (step != SQLITE_ROW) {
						return std::nullopt;
					}
					// A surface a newer build names is kept and never read.
					const auto surface = core::find_prompt_surface(column_text(surfaces->Value, 0).value_or(std::string{ }));
					if (!surface) {
						continue;
					}
					stored.Differences.Surfaces.push_back({ .Surface = *surface,
															.Ids     = identity_lines(column_text(surfaces->Value, 1).value_or(std::string{ })),
															.Removed = identity_lines(column_text(surfaces->Value, 2).value_or(std::string{ })) });
				}
				std::ranges::sort(stored.Differences.Surfaces, { }, &core::prompt_surface_change::Surface);
				return stored;
			}

		private:
			[[nodiscard]] static auto identity_lines(const std::string_view aText) -> std::vector<std::string>
			{
				auto ids = std::vector<std::string>{ };
				if (aText.empty()) {
					return ids;
				}
				for (const auto line : split(aText, '\n')) {
					if (!line.empty() && ids.size() < core::maximum_prompt_macros) {
						ids.emplace_back(line);
					}
				}
				return ids;
			}

			[[nodiscard]] static auto joined_identities(const std::vector<std::string>& aIds) -> std::optional<std::string>
			{
				auto text = std::string{ };
				if (aIds.size() > core::maximum_prompt_macros) {
					return std::nullopt;
				}
				for (const auto& id : aIds) {
					if (id.empty() || id.size() > 128U || id.find('\n') != std::string::npos) {
						return std::nullopt;
					}
					if (!text.empty()) {
						text += '\n';
					}
					text += id;
				}
				return text;
			}

			[[nodiscard]] static auto optional_text(sqlite3_stmt* const aRow, const int aColumn) -> std::optional<std::string>
			{
				return sqlite3_column_type(aRow, aColumn) == SQLITE_NULL ? std::nullopt : column_text(aRow, aColumn);
			}

			static void bind_optional(sqlite3_stmt* const aRow, const int aIndex, const std::optional<std::string>& aText)
			{
				if (aText) {
					bind_text(aRow, aIndex, *aText);
				} else {
					sqlite3_bind_null(aRow, aIndex);
				}
			}

			/// [[BPR-2179]]. Every repository's layer; a row this build
			/// cannot read is left out.
			[[nodiscard]] static auto read_layers(sqlite3* const aDatabase, std::vector<core::prompt_repository_layer>& aLayers) -> bool
			{
				const auto layer = [&aLayers](const std::string& aRepository) -> core::prompt_repository_layer& {
					auto found = std::ranges::find(aLayers, aRepository, &core::prompt_repository_layer::Repository);
					if (found == aLayers.end()) {
						aLayers.push_back({ .Repository = aRepository });
						return aLayers.back();
					}
					return *found;
				};
				auto prompts = detail::prepare(aDatabase, "SELECT repository,kind,id,place,subject,name,template,ask,conditions,match,on_state FROM prompt_repository_prompts ORDER BY repository,kind,position");
				if (!prompts) {
					return false;
				}
				for (auto step = sqlite3_step(prompts->Value); step != SQLITE_DONE; step = sqlite3_step(prompts->Value)) {
					if (step != SQLITE_ROW) {
						return false;
					}
					const auto repository = column_text(prompts->Value, 0).value_or(std::string{ });
					const auto kind       = sqlite3_column_int64(prompts->Value, 1);
					auto       id         = column_text(prompts->Value, 2).value_or(std::string{ });
					const auto name       = optional_text(prompts->Value, 5);
					const auto text       = optional_text(prompts->Value, 6);
					const auto ask        = optional_text(prompts->Value, 7);
					const auto when       = optional_text(prompts->Value, 8);
					const auto match      = optional_text(prompts->Value, 9);
					const auto on         = sqlite3_column_type(prompts->Value, 10) == SQLITE_NULL ? std::optional<bool>{ } : std::optional{ sqlite3_column_int64(prompts->Value, 10) != 0 };
					auto       groups     = std::vector<core::prompt_condition_group>{ };
					auto       unmet      = std::optional<core::prompt_when_unmet>{ };
					auto       conditions = when ? decode_conditions(*when, nullptr, &groups, &unmet) : std::optional{ std::vector<core::prompt_condition>{ } };
					const auto kindValue  = ask ? named_value<core::prompt_ask>(*ask, 2U) : std::optional{ core::prompt_ask::question };
					if (!conditions || !kindValue || repository.empty()) {
						continue;
					}
					if (kind == 0) {
						auto change = core::prompt_override{ .Id = std::move(id), .Name = name, .Template = text, .On = on };
						if (ask) {
							change.Kind = kindValue;
						}
						if (when) {
							change.WhenUnmet  = unmet.value_or(core::migrated_when_unmet(*conditions, groups));
							change.Conditions = std::move(conditions);
							change.Groups     = std::move(groups);
						}
						if (match) {
							change.Match = match_named(*match).value_or(core::prompt_match::all);
						}
						layer(repository).Overrides.push_back(std::move(change));
						continue;
					}
					const auto place   = named_value<core::prompt_place>(column_text(prompts->Value, 3).value_or(std::string{ }), core::prompt_place_count);
					const auto subject = named_value<core::prompt_subject>(column_text(prompts->Value, 4).value_or(std::string{ }), 9U);
					if (!place || !subject) {
						continue;
					}
					auto own            = core::prompt_macro{ .Id = std::move(id), .Place = *place, .On = on.value_or(true) };
					own.Body.Name       = name.value_or(std::string{ });
					own.Body.Template   = text.value_or(std::string{ });
					own.Body.Subject    = *subject;
					own.Body.Kind       = *kindValue;
					own.Body.Conditions = std::move(*conditions);
					own.Body.Match      = match ? match_named(*match).value_or(core::prompt_match::all) : core::prompt_match::all;
					own.Body.WhenUnmet  = unmet.value_or(core::migrated_when_unmet(own.Body.Conditions, groups));
					own.Body.Groups     = std::move(groups);
					layer(repository).Own.push_back(std::move(own));
				}
				auto surfaces = detail::prepare(aDatabase, "SELECT layer,surface,ids,removed,added,ordered FROM prompt_surface_lists WHERE layer<>'' ORDER BY layer,surface");
				if (!surfaces) {
					return false;
				}
				for (auto step = sqlite3_step(surfaces->Value); step != SQLITE_DONE; step = sqlite3_step(surfaces->Value)) {
					if (step != SQLITE_ROW) {
						return false;
					}
					const auto surface = core::find_prompt_surface(column_text(surfaces->Value, 1).value_or(std::string{ }));
					if (!surface) {
						continue;
					}
					auto change = core::prompt_repository_surface{ .Surface = *surface,
																   .Added   = identity_lines(column_text(surfaces->Value, 4).value_or(std::string{ })),
																   .Removed = identity_lines(column_text(surfaces->Value, 3).value_or(std::string{ })) };
					if (sqlite3_column_int64(surfaces->Value, 5) != 0) {
						change.Order = identity_lines(column_text(surfaces->Value, 2).value_or(std::string{ }));
					}
					layer(column_text(surfaces->Value, 0).value_or(std::string{ })).Surfaces.push_back(std::move(change));
				}
				std::ranges::sort(aLayers, { }, &core::prompt_repository_layer::Repository);
				for (auto& each : aLayers) {
					std::ranges::sort(each.Surfaces, { }, &core::prompt_repository_surface::Surface);
				}
				return true;
			}

			/// [[BPR-2179]]. Replaces every repository's layer.
			[[nodiscard]] static auto write_layers(sqlite3* const aDatabase, const std::vector<core::prompt_repository_layer>& aLayers) -> bool
			{
				if (!detail::execute(aDatabase, "DELETE FROM prompt_repository_prompts") || !detail::execute(aDatabase, "DELETE FROM prompt_surface_lists WHERE layer<>''")
					|| aLayers.size() > core::maximum_prompt_repositories) {
					return false;
				}
				for (const auto& layer : aLayers) {
					if (layer.Repository.empty() || layer.Repository.size() > 256U) {
						return false;
					}
					auto       position = std::int64_t{ 0 };
					const auto row      = [&](const std::int64_t aKind, const std::string& aId, const std::string& aPlace, const std::string& aSubject, const std::optional<std::string>& aName,
											  const std::optional<std::string>& aTemplate, const std::optional<std::string>& aAsk, const std::optional<std::string>& aConditions,
											  const std::optional<std::string>& aMatch, const std::optional<bool> aOn) {
						if (position >= static_cast<std::int64_t>(core::maximum_prompt_macros)) {
							return false;
						}
						auto insert = detail::prepare(aDatabase,
													  "INSERT INTO prompt_repository_prompts(repository,kind,id,position,place,subject,name,template,ask,conditions,match,on_state) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)");
						if (!insert) {
							return false;
						}
						bind_text(insert->Value, 1, layer.Repository);
						sqlite3_bind_int64(insert->Value, 2, aKind);
						bind_text(insert->Value, 3, aId);
						sqlite3_bind_int64(insert->Value, 4, position++);
						bind_text(insert->Value, 5, aPlace);
						bind_text(insert->Value, 6, aSubject);
						bind_optional(insert->Value, 7, aName);
						bind_optional(insert->Value, 8, aTemplate);
						bind_optional(insert->Value, 9, aAsk);
						bind_optional(insert->Value, 10, aConditions);
						bind_optional(insert->Value, 11, aMatch);
						if (aOn) {
							sqlite3_bind_int64(insert->Value, 12, *aOn ? 1 : 0);
						} else {
							sqlite3_bind_null(insert->Value, 12);
						}
						return sqlite3_step(insert->Value) == SQLITE_DONE;
					};
					for (const auto& change : layer.Overrides) {
						auto conditions = std::optional<std::string>{ };
						if (change.Conditions) {
							conditions = encode_conditions(*change.Conditions, core::prompt_match::all, change.Groups.value_or(std::vector<core::prompt_condition_group>{ }), change.WhenUnmet);
							if (!conditions) {
								return false;
							}
						}
						const auto ask   = change.Kind ? std::optional{ std::string{ core::stable_name(*change.Kind) } } : std::nullopt;
						const auto match = change.Match ? std::optional{ std::string{ core::stable_name(*change.Match) } } : std::nullopt;
						if (!row(0, change.Id, { }, { }, change.Name, change.Template, ask, conditions, match, change.On)) {
							return false;
						}
					}
					position = 0;
					for (const auto& own : layer.Own) {
						const auto conditions = encode_conditions(own.Body.Conditions, core::prompt_match::all, own.Body.Groups, own.Body.WhenUnmet);
						if (!conditions
							|| !row(1, own.Id, std::string{ core::stable_name(own.Place) }, std::string{ core::stable_name(own.Body.Subject) }, own.Body.Name, own.Body.Template,
									std::string{ core::stable_name(own.Body.Kind) }, conditions, std::string{ core::stable_name(own.Body.Match) }, own.On)) {
							return false;
						}
					}
					for (const auto& change : layer.Surfaces) {
						const auto ids     = joined_identities(change.Order.value_or(std::vector<std::string>{ }));
						const auto removed = joined_identities(change.Removed);
						const auto added   = joined_identities(change.Added);
						auto       insert  = detail::prepare(aDatabase, "INSERT INTO prompt_surface_lists(layer,surface,ids,removed,added,ordered) VALUES(?1,?2,?3,?4,?5,?6)");
						if (!ids || !removed || !added || !insert) {
							return false;
						}
						bind_text(insert->Value, 1, layer.Repository);
						bind_text(insert->Value, 2, std::string{ core::stable_name(change.Surface) });
						bind_text(insert->Value, 3, *ids);
						bind_text(insert->Value, 4, *removed);
						bind_text(insert->Value, 5, *added);
						sqlite3_bind_int64(insert->Value, 6, change.Order ? 1 : 0);
						if (sqlite3_step(insert->Value) != SQLITE_DONE) {
							return false;
						}
					}
				}
				return true;
			}

			/// Replaces every surface this build names with `aChanges`.
			[[nodiscard]] static auto write_surfaces(sqlite3* const aDatabase, const std::vector<core::prompt_surface_change>& aChanges) -> bool
			{
				for (auto index = std::size_t{ 0 }; index < core::prompt_surface_count; ++index) {
					auto erase = detail::prepare(aDatabase, "DELETE FROM prompt_surface_lists WHERE layer='' AND surface=?1");
					if (!erase) {
						return false;
					}
					bind_text(erase->Value, 1, std::string{ core::stable_name(static_cast<core::prompt_surface>(index)) });
					if (sqlite3_step(erase->Value) != SQLITE_DONE) {
						return false;
					}
				}
				for (const auto& change : aChanges) {
					const auto ids     = joined_identities(change.Ids);
					const auto removed = joined_identities(change.Removed);
					auto       row     = detail::prepare(aDatabase, "INSERT OR REPLACE INTO prompt_surface_lists(layer,surface,ids,removed,added,ordered) VALUES('',?1,?2,?3,'',1)");
					if (!ids || !removed || !row) {
						return false;
					}
					bind_text(row->Value, 1, std::string{ core::stable_name(change.Surface) });
					bind_text(row->Value, 2, *ids);
					bind_text(row->Value, 3, *removed);
					if (sqlite3_step(row->Value) != SQLITE_DONE) {
						return false;
					}
				}
				return true;
			}

			/// RFC-BPR-0125 AC24. Each own prompt's link to its pack; a row
			/// naming no prompt, or a value this build does not know, is not
			/// read (the prompt is then the reviewer's own).
			[[nodiscard]] static auto read_links(sqlite3* const aDatabase, std::vector<core::prompt_macro>& aOwn) -> bool
			{
				auto row = detail::prepare(aDatabase, "SELECT id,pack,label,version,place,name,template,subject,ask,conditions,quick,scopes FROM prompt_macro_packs");
				if (!row) {
					return false;
				}
				for (auto step = sqlite3_step(row->Value); step != SQLITE_DONE; step = sqlite3_step(row->Value)) {
					if (step != SQLITE_ROW) {
						return false;
					}
					const auto id  = column_text(row->Value, 0).value_or(std::string{ });
					const auto own = std::ranges::find(aOwn, id, &core::prompt_macro::Id);
					if (own == aOwn.end()) {
						continue;
					}
					auto link          = core::prompt_pack_link{ .Pack = column_text(row->Value, 1).value_or(std::string{ }), .Label = column_text(row->Value, 2).value_or(std::string{ }) };
					link.Version       = static_cast<std::uint32_t>(sqlite3_column_int64(row->Value, 3));
					const auto place   = named_value<core::prompt_place>(column_text(row->Value, 4).value_or(std::string{ }), core::prompt_place_count);
					const auto subject = named_value<core::prompt_subject>(column_text(row->Value, 7).value_or(std::string{ }), 9U);
					const auto ask     = named_value<core::prompt_ask>(column_text(row->Value, 8).value_or(std::string{ }), 2U);
					auto       match   = core::prompt_match::all;
					auto       groups  = std::vector<core::prompt_condition_group>{ };
					auto       unmet   = std::optional<core::prompt_when_unmet>{ };
					auto       when    = decode_conditions(column_text(row->Value, 9).value_or(std::string{ }), &match, &groups, &unmet);
					if (!place || !subject || !ask || !when) {
						continue;
					}
					link.Place           = *place;
					link.Body.Name       = column_text(row->Value, 5).value_or(std::string{ });
					link.Body.Template   = column_text(row->Value, 6).value_or(std::string{ });
					link.Body.Subject    = *subject;
					link.Body.Kind       = *ask;
					link.Body.WhenUnmet  = unmet.value_or(core::migrated_when_unmet(*when, groups));
					link.Body.Conditions = std::move(*when);
					link.Body.Groups     = std::move(groups);
					link.Body.Match      = match;
					link.Body.QuickAsk   = sqlite3_column_int64(row->Value, 10) != 0;
					const auto bits      = sqlite3_column_int64(row->Value, 11);
					link.Body.Scopes     = { .Thread = (bits & 1) != 0, .File = (bits & 2) != 0, .PullRequest = (bits & 4) != 0 };
					own->Pack            = std::move(link);
				}
				return true;
			}

			/// Replaces every link this build wrote; a newer build's prompts
			/// keep theirs.
			[[nodiscard]] static auto write_links(sqlite3* const aDatabase, const std::vector<core::prompt_macro>& aOwn, const std::vector<std::string>& aNewer) -> bool
			{
				auto ids = detail::prepare(aDatabase, "SELECT id FROM prompt_macro_packs");
				if (!ids) {
					return false;
				}
				auto doomed = std::vector<std::string>{ };
				for (auto step = sqlite3_step(ids->Value); step != SQLITE_DONE; step = sqlite3_step(ids->Value)) {
					if (step != SQLITE_ROW) {
						return false;
					}
					auto id = column_text(ids->Value, 0).value_or(std::string{ });
					if (!std::ranges::contains(aNewer, id)) {
						doomed.push_back(std::move(id));
					}
				}
				for (const auto& id : doomed) {
					auto erase = detail::prepare(aDatabase, "DELETE FROM prompt_macro_packs WHERE id=?1");
					if (!erase) {
						return false;
					}
					bind_text(erase->Value, 1, id);
					if (sqlite3_step(erase->Value) != SQLITE_DONE) {
						return false;
					}
				}
				for (const auto& own : aOwn) {
					if (!own.Pack) {
						continue;
					}
					const auto& link       = *own.Pack;
					const auto  conditions = encode_conditions(link.Body.Conditions, link.Body.Match, link.Body.Groups, link.Body.WhenUnmet);
					auto        row        = detail::prepare(aDatabase,
															 "INSERT OR REPLACE INTO prompt_macro_packs(id,pack,label,version,place,name,template,subject,ask,conditions,quick,scopes) "
															 "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)");
					if (!conditions || !row || link.Version == 0U) {
						return false;
					}
					bind_text(row->Value, 1, own.Id);
					bind_text(row->Value, 2, link.Pack);
					bind_text(row->Value, 3, link.Label);
					sqlite3_bind_int64(row->Value, 4, static_cast<std::int64_t>(link.Version));
					bind_text(row->Value, 5, std::string{ core::stable_name(link.Place) });
					bind_text(row->Value, 6, link.Body.Name);
					bind_text(row->Value, 7, link.Body.Template);
					bind_text(row->Value, 8, std::string{ core::stable_name(link.Body.Subject) });
					bind_text(row->Value, 9, std::string{ core::stable_name(link.Body.Kind) });
					bind_text(row->Value, 10, *conditions);
					sqlite3_bind_int64(row->Value, 11, link.Body.QuickAsk ? 1 : 0);
					sqlite3_bind_int64(row->Value, 12, scope_bits(link.Body.Scopes));
					if (sqlite3_step(row->Value) != SQLITE_DONE) {
						return false;
					}
				}
				return true;
			}

			/// Deletes every row this build reads; a newer build's rows stay.
			[[nodiscard]] static auto erase_known(sqlite3* const aDatabase, const std::vector<std::string>& aNewer) -> bool
			{
				auto ids = detail::prepare(aDatabase, "SELECT kind,id FROM prompt_macros");
				if (!ids) {
					return false;
				}
				auto doomed = std::vector<std::pair<std::int64_t, std::string>>{ };
				for (auto step = sqlite3_step(ids->Value); step != SQLITE_DONE; step = sqlite3_step(ids->Value)) {
					if (step != SQLITE_ROW) {
						return false;
					}
					auto id = column_text(ids->Value, 1).value_or(std::string{ });
					if (std::ranges::find(aNewer, id) == aNewer.end()) {
						doomed.emplace_back(sqlite3_column_int64(ids->Value, 0), std::move(id));
					}
				}
				for (const auto& [kind, id] : doomed) {
					auto erase = detail::prepare(aDatabase, "DELETE FROM prompt_macros WHERE kind=?1 AND id=?2");
					if (!erase) {
						return false;
					}
					sqlite3_bind_int64(erase->Value, 1, kind);
					bind_text(erase->Value, 2, id);
					if (sqlite3_step(erase->Value) != SQLITE_DONE) {
						return false;
					}
				}
				return true;
			}

			[[nodiscard]] static auto write_row(sqlite3* const aDatabase, const std::int64_t aKind, const std::string& aId, const std::int64_t aPosition, const std::optional<core::prompt_place> aPlace,
												const core::prompt_macro_body* const aBody, const std::optional<bool> aOn, const std::uint32_t aWrittenAgainst) -> bool
			{
				if (aPosition >= static_cast<std::int64_t>(core::maximum_prompt_macros)) {
					return false;
				}
				const auto conditions = aBody != nullptr ? encode_conditions(aBody->Conditions, aBody->Match, aBody->Groups, aBody->WhenUnmet) : std::optional{ std::string{ } };
				if (!conditions) {
					return false;
				}
				auto row = detail::prepare(aDatabase,
										   "INSERT OR REPLACE INTO prompt_macros(kind,id,position,place,has_body,name,template,subject,ask,conditions,quick,scopes,on_state,written_against) "
										   "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14)");
				if (!row) {
					return false;
				}
				sqlite3_bind_int64(row->Value, 1, aKind);
				bind_text(row->Value, 2, aId);
				sqlite3_bind_int64(row->Value, 3, aPosition);
				bind_text(row->Value, 4, aPlace ? std::string{ core::stable_name(*aPlace) } : std::string{ });
				sqlite3_bind_int64(row->Value, 5, aBody != nullptr ? 1 : 0);
				bind_text(row->Value, 6, aBody != nullptr ? aBody->Name : std::string{ });
				bind_text(row->Value, 7, aBody != nullptr ? aBody->Template : std::string{ });
				bind_text(row->Value, 8, aBody != nullptr ? std::string{ core::stable_name(aBody->Subject) } : std::string{ });
				bind_text(row->Value, 9, aBody != nullptr ? std::string{ core::stable_name(aBody->Kind) } : std::string{ });
				bind_text(row->Value, 10, *conditions);
				sqlite3_bind_int64(row->Value, 11, aBody != nullptr && aBody->QuickAsk ? 1 : 0);
				sqlite3_bind_int64(row->Value, 12, aBody != nullptr ? scope_bits(aBody->Scopes) : 0);
				if (aOn) {
					sqlite3_bind_int64(row->Value, 13, *aOn ? 1 : 0);
				} else {
					sqlite3_bind_null(row->Value, 13);
				}
				sqlite3_bind_int64(row->Value, 14, aWrittenAgainst);
				return sqlite3_step(row->Value) == SQLITE_DONE;
			}

			std::filesystem::path Root;
		};

		/// RFC-BPR-0126 ([[BPR-2151]]). Each session's queue.
		class agent_queue_store final : public application::agent_queue_store_port
		{
		public:
			explicit agent_queue_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load() -> std::optional<std::map<std::string, core::agent_queue>> override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return std::nullopt;
					}
					auto* const db     = store->Database.get();
					auto        queues = std::map<std::string, core::agent_queue>{ };
					auto        meta   = detail::prepare(db, "SELECT session,revision,last_id,paused FROM agent_queues");
					if (!meta) {
						return std::nullopt;
					}
					for (auto step = sqlite3_step(meta->Value); step == SQLITE_ROW; step = sqlite3_step(meta->Value)) {
						auto& queue       = queues[column_text(meta->Value, 0).value_or(std::string{ })];
						queue.Revision    = static_cast<std::uint64_t>(sqlite3_column_int64(meta->Value, 1));
						queue.LastId      = static_cast<std::uint64_t>(sqlite3_column_int64(meta->Value, 2));
						const auto paused = column_text(meta->Value, 3).value_or(std::string{ });
						queue.Paused      = named_value<core::queue_pause>(paused, 9U).value_or(core::queue_pause::restarted);
					}
					auto entries = detail::prepare(db, "SELECT session,id,quick_ask,scope,file,thread,label,text,state,held,prompt,claim FROM agent_queue_entries ORDER BY session,position");
					if (!entries) {
						return std::nullopt;
					}
					for (auto step = sqlite3_step(entries->Value); step == SQLITE_ROW; step = sqlite3_step(entries->Value)) {
						auto entry  = core::queued_entry{ .Id = static_cast<std::uint64_t>(sqlite3_column_int64(entries->Value, 1)) };
						entry.Input = core::queued_input{
							.QuickAsk = column_text(entries->Value, 2).value_or(std::string{ }),
							.Scope    = column_text(entries->Value, 3).value_or(std::string{ }),
							.File     = column_text(entries->Value, 4).value_or(std::string{ }),
							.Threads  = split_threads(column_text(entries->Value, 5).value_or(std::string{ })),
							.Label    = column_text(entries->Value, 6).value_or(std::string{ }),
							.Text     = column_text(entries->Value, 7).value_or(std::string{ }),
						};
						entry.State = named_value<core::queued_state>(column_text(entries->Value, 8).value_or(std::string{ }), 4U).value_or(core::queued_state::delivery_unknown);
						entry.Held  = sqlite3_column_int64(entries->Value, 9) != 0;
						if (const auto prompt = column_text(entries->Value, 10).value_or(std::string{ }); !prompt.empty()) {
							entry.Input.Prompt = core::decode_queued_prompt(prompt);
						}
						if (const auto claim = column_text(entries->Value, 11).value_or(std::string{ }); !claim.empty()) {
							entry.Claim = core::decode_queued_claim(claim);
						}
						queues[column_text(entries->Value, 0).value_or(std::string{ })].Entries.push_back(std::move(entry));
					}
					return queues;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto store(const std::string& aSession, const core::agent_queue& aQueue) -> bool override
			{
				try {
					auto store = open(Root);
					if (!store || aSession.empty() || aQueue.Entries.size() > core::maximum_queued_inputs) {
						return false;
					}
					auto* const db = store->Database.get();
					if (!detail::execute(db, "BEGIN IMMEDIATE")) {
						return false;
					}
					auto ok = true;
					for (const auto* const statement : { "DELETE FROM agent_queue_entries WHERE session=?1", "DELETE FROM agent_queues WHERE session=?1" }) {
						auto erase = detail::prepare(db, statement);
						if (!ok || !erase) {
							ok = false;
							break;
						}
						bind_text(erase->Value, 1, aSession);
						ok = sqlite3_step(erase->Value) == SQLITE_DONE;
					}
					if (ok && !aQueue.Entries.empty()) {
						if (auto meta = detail::prepare(db, "INSERT INTO agent_queues(session,revision,last_id,paused) VALUES(?1,?2,?3,?4)"); meta) {
							bind_text(meta->Value, 1, aSession);
							sqlite3_bind_int64(meta->Value, 2, static_cast<std::int64_t>(aQueue.Revision));
							sqlite3_bind_int64(meta->Value, 3, static_cast<std::int64_t>(aQueue.LastId));
							bind_text(meta->Value, 4, std::string{ core::stable_name(aQueue.Paused) });
							ok = sqlite3_step(meta->Value) == SQLITE_DONE;
						} else {
							ok = false;
						}
						auto position = std::int64_t{ 0 };
						for (const auto& entry : aQueue.Entries) {
							if (!ok) {
								break;
							}
							auto row = detail::prepare(db, "INSERT INTO agent_queue_entries(session,position,id,quick_ask,scope,file,thread,label,text,state,held,prompt,claim) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13)");
							if (!row) {
								ok = false;
								break;
							}
							bind_text(row->Value, 1, aSession);
							sqlite3_bind_int64(row->Value, 2, position++);
							sqlite3_bind_int64(row->Value, 3, static_cast<std::int64_t>(entry.Id));
							bind_text(row->Value, 4, entry.Input.QuickAsk);
							bind_text(row->Value, 5, entry.Input.Scope);
							bind_text(row->Value, 6, entry.Input.File);
							bind_text(row->Value, 7, joined_threads(entry.Input.Threads));
							bind_text(row->Value, 8, entry.Input.Label);
							bind_text(row->Value, 9, entry.Input.Text);
							bind_text(row->Value, 10, std::string{ core::stable_name(entry.State) });
							sqlite3_bind_int64(row->Value, 11, entry.Held ? 1 : 0);
							bind_text(row->Value, 12, entry.Input.Prompt ? core::encode_queued_prompt(*entry.Input.Prompt) : std::string{ });
							bind_text(row->Value, 13, entry.Claim ? core::encode_queued_claim(*entry.Claim) : std::string{ });
							ok = sqlite3_step(row->Value) == SQLITE_DONE;
						}
					}
					if (!ok) {
						static_cast<void>(detail::execute(db, "ROLLBACK"));
						return false;
					}
					return detail::execute(db, "COMMIT").has_value();
				} catch (...) {
					return false;
				}
			}

		private:
			std::filesystem::path Root;
		};

		/// [[BPR-2016]], RFC-BPR-0107. What an import changes, read and
		/// written across this file and the profile's workbench settings.
		class agent_configuration_store final : public application::agent_configuration_store_port
		{
		public:
			explicit agent_configuration_store(std::filesystem::path aRoot)
				: Root{ std::move(aRoot) }
			{ }

			[[nodiscard]] auto load() -> std::optional<application::agent_configuration_snapshot> override
			{
				try {
					auto agents = agent_setup_store{ Root }.load();
					auto skill  = review_skill_store{ Root }.load_review_skill();
					auto hooks  = load_hooks();
					auto store  = open(Root);
					if (!agents || !skill || !hooks || !store) {
						return std::nullopt;
					}
					auto snapshot  = application::agent_configuration_snapshot{ .Agents = std::move(*agents), .ReviewSkill = std::move(*skill), .Hooks = std::move(*hooks) };
					auto workflows = load_workflows(store->Database.get());
					if (!workflows) {
						return std::nullopt;
					}
					snapshot.Workflows = std::move(*workflows);
					// RFC-BPR-0125 AC24. The suggested prompts and their revision,
					// which an import's prompt pack changes.
					auto prompts = prompt_macro_store::read(store->Database.get());
					if (!prompts) {
						return std::nullopt;
					}
					snapshot.PromptMacros = std::move(*prompts);
					auto row              = detail::prepare(store->Database.get(), "SELECT hook,text_digest,version FROM prompt_provenance");
					if (!row) {
						return std::nullopt;
					}
					while (sqlite3_step(row->Value) == SQLITE_ROW) {
						const auto hook    = sqlite3_column_int64(row->Value, 0);
						auto       version = column_text(row->Value, 2);
						if (hook < 0 || hook >= static_cast<std::int64_t>(core::writing_hook_count) || !version || sqlite3_column_bytes(row->Value, 1) != 32) {
							continue;
						}
						const auto& text   = snapshot.Hooks.Overrides[static_cast<std::size_t>(hook)];
						const auto  digest = core::writing_digest(text);
						if (!text.empty() && std::memcmp(sqlite3_column_blob(row->Value, 1), digest.data(), digest.size()) == 0) {
							snapshot.WrittenAgainst[static_cast<std::size_t>(hook)] = std::move(*version);
						}
					}
					return snapshot;
				} catch (...) {
					return std::nullopt;
				}
			}

			[[nodiscard]] auto apply(const application::agent_configuration_snapshot& aBefore, const application::agent_configuration_snapshot& aAfter)
				-> application::agent_configuration_outcome override
			{
				using outcome = application::agent_configuration_outcome;
				try {
					if (!application::valid(aAfter.Agents) || !core::valid(aAfter.Hooks) || aAfter.Workflows.size() > core::maximum_profile_workflows
						|| !std::ranges::all_of(aAfter.Workflows, [](const core::local_workflow& aWorkflow) { return core::valid(aWorkflow); })) {
						return outcome::storage_failed;
					}
					const auto current = load();
					if (!current) {
						return outcome::storage_failed;
					}
					if (current->Agents != aBefore.Agents || current->ReviewSkill != aBefore.ReviewSkill || current->Hooks != aBefore.Hooks || current->Workflows != aBefore.Workflows
						|| current->PromptMacros != aBefore.PromptMacros) {
						return outcome::changed_since;
					}
					auto store = open(Root);
					if (!store) {
						return outcome::storage_failed;
					}
					auto* const db          = store->Database.get();
					const auto  hooksChange = aAfter.Hooks != aBefore.Hooks;
					// The hooks live in another file. Their earlier text is kept
					// here first, so a crash before this file's commit is undone.
					if (hooksChange) {
						if (!journal(db, aBefore.Hooks, aAfter.Hooks.Revision) || !store_hooks(aAfter.Hooks)) {
							static_cast<void>(detail::execute(db, "DELETE FROM configuration_import_journal"));
							return outcome::storage_failed;
						}
					}
					auto committed = detail::execute(db, "BEGIN IMMEDIATE").has_value();
					if (committed) {
						committed = write_agents(db, aAfter.Agents) && write_skill(db, aAfter.ReviewSkill) && write_provenance(db, aAfter) && write_workflows(db, aAfter.Workflows)
							&& detail::execute(db, "DELETE FROM configuration_import_journal").has_value();
						// RFC-BPR-0125 AC24. The prompt pack's changes, in the same
						// transaction, from the revision the preview read.
						if (committed && aAfter.PromptMacros.Differences != aBefore.PromptMacros.Differences) {
							committed = prompt_macro_store::write_differences(db, aAfter.PromptMacros.Differences, aBefore.PromptMacros.Revision) == application::prompt_macro_write::stored;
						}
						if (!committed) {
							static_cast<void>(detail::execute(db, "ROLLBACK"));
						} else {
							committed = detail::execute(db, "COMMIT").has_value();
						}
					}
					if (!committed) {
						if (hooksChange) {
							static_cast<void>(recover());
						}
						return outcome::storage_failed;
					}
					return outcome::applied;
				} catch (...) {
					return outcome::storage_failed;
				}
			}

			[[nodiscard]] auto record_written_against(const core::writing_hook aHook, const std::string_view aText, const std::string_view aVersion) -> bool override
			{
				try {
					auto store = open(Root);
					if (!store || aVersion.empty() || aVersion.size() > core::maximum_prompt_version_bytes) {
						return false;
					}
					if (aText.empty()) {
						auto row = detail::prepare(store->Database.get(), "DELETE FROM prompt_provenance WHERE hook=?1");
						return row && sqlite3_bind_int64(row->Value, 1, static_cast<std::int64_t>(aHook)) == SQLITE_OK && sqlite3_step(row->Value) == SQLITE_DONE;
					}
					return insert_provenance(store->Database.get(), static_cast<std::size_t>(aHook), aText, aVersion);
				} catch (...) {
					return false;
				}
			}

			[[nodiscard]] auto recover() -> bool override
			{
				try {
					auto store = open(Root);
					if (!store) {
						return false;
					}
					auto row = detail::prepare(store->Database.get(), "SELECT hook,revision_after,text FROM configuration_import_journal ORDER BY hook");
					if (!row) {
						return false;
					}
					auto earlier = std::array<std::string, core::writing_hook_count>{ };
					auto after   = std::int64_t{ 0 };
					auto rows    = std::size_t{ 0U };
					while (sqlite3_step(row->Value) == SQLITE_ROW) {
						const auto hook = sqlite3_column_int64(row->Value, 0);
						if (hook >= 0 && hook < static_cast<std::int64_t>(core::writing_hook_count)) {
							earlier[static_cast<std::size_t>(hook)] = column_text(row->Value, 2).value_or(std::string{ });
							after                                   = sqlite3_column_int64(row->Value, 1);
							++rows;
						}
					}
					if (rows == 0U) {
						return true;
					}
					// Undone only while the hooks are still the import's: an edit
					// made since then is the reviewer's and stays.
					auto hooks = load_hooks();
					if (!hooks) {
						return false;
					}
					if (rows == core::writing_hook_count && static_cast<std::int64_t>(hooks->Revision) == after) {
						hooks->Overrides = earlier;
						++hooks->Revision;
						if (!store_hooks(*hooks)) {
							return false;
						}
					}
					return detail::execute(store->Database.get(), "DELETE FROM configuration_import_journal").has_value();
				} catch (...) {
					return false;
				}
			}

		private:
			[[nodiscard]] auto load_hooks() const -> std::optional<core::writing_hook_settings>
			{
				auto settings = make_workbench_defaults_store({ .StorageRoot = Root });
				if (!settings) {
					return std::nullopt;
				}
				auto loaded = (*settings)->load_preferences({ });
				return loaded ? std::optional{ loaded->WritingHooks } : std::nullopt;
			}

			[[nodiscard]] auto store_hooks(const core::writing_hook_settings& aHooks) const -> bool
			{
				auto settings = make_workbench_defaults_store({ .StorageRoot = Root });
				if (!settings) {
					return false;
				}
				auto loaded = (*settings)->load_preferences({ });
				if (!loaded) {
					return false;
				}
				loaded->WritingHooks = aHooks;
				return (*settings)->store_preferences(*loaded, { }).has_value();
			}

			[[nodiscard]] static auto journal(sqlite3* const aDatabase, const core::writing_hook_settings& aBefore, const std::uint64_t aRevisionAfter) -> bool
			{
				if (!detail::execute(aDatabase, "BEGIN IMMEDIATE")) {
					return false;
				}
				auto committed = detail::execute(aDatabase, "DELETE FROM configuration_import_journal").has_value();
				for (std::size_t index = 0U; committed && index < core::writing_hook_count; ++index) {
					auto row  = detail::prepare(aDatabase, "INSERT INTO configuration_import_journal VALUES(?1,?2,?3)");
					committed = row.has_value();
					if (committed) {
						sqlite3_bind_int64(row->Value, 1, static_cast<std::int64_t>(index));
						sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aRevisionAfter));
						bind_text(row->Value, 3, aBefore.Overrides[index]);
						committed = sqlite3_step(row->Value) == SQLITE_DONE;
					}
				}
				if (!committed) {
					static_cast<void>(detail::execute(aDatabase, "ROLLBACK"));
					return false;
				}
				return detail::execute(aDatabase, "COMMIT").has_value();
			}

			[[nodiscard]] static auto write_skill(sqlite3* const aDatabase, const std::optional<application::stored_review_skill>& aSkill) -> bool
			{
				if (!aSkill) {
					return detail::execute(aDatabase, "DELETE FROM review_skill").has_value();
				}
				auto row = detail::prepare(aDatabase, "INSERT OR REPLACE INTO review_skill VALUES(1,?1,?2,?3,?4)");
				if (!row || aSkill->Text.empty() || aSkill->Text.size() > core::maximum_review_skill_bytes) {
					return false;
				}
				bind_text(row->Value, 1, aSkill->Text);
				sqlite3_bind_int64(row->Value, 2, static_cast<std::int64_t>(aSkill->Record.BaseVersion));
				sqlite3_bind_blob(row->Value, 3, aSkill->Record.BaseDigest.data(), static_cast<int>(aSkill->Record.BaseDigest.size()), SQLITE_TRANSIENT);
				if (aSkill->Record.KeptVersion) {
					sqlite3_bind_int64(row->Value, 4, static_cast<std::int64_t>(*aSkill->Record.KeptVersion));
				} else {
					sqlite3_bind_null(row->Value, 4);
				}
				return sqlite3_step(row->Value) == SQLITE_DONE;
			}

			[[nodiscard]] static auto insert_provenance(sqlite3* const aDatabase, const std::size_t aHook, const std::string_view aText, const std::string_view aVersion) -> bool
			{
				auto row = detail::prepare(aDatabase, "INSERT OR REPLACE INTO prompt_provenance VALUES(?1,?2,?3)");
				if (!row) {
					return false;
				}
				const auto digest = core::writing_digest(aText);
				sqlite3_bind_int64(row->Value, 1, static_cast<std::int64_t>(aHook));
				sqlite3_bind_blob(row->Value, 2, digest.data(), static_cast<int>(digest.size()), SQLITE_TRANSIENT);
				sqlite3_bind_text(row->Value, 3, aVersion.data(), static_cast<int>(aVersion.size()), SQLITE_TRANSIENT);
				return sqlite3_step(row->Value) == SQLITE_DONE;
			}

			[[nodiscard]] static auto load_workflows(sqlite3* const aDatabase) -> std::optional<std::vector<core::local_workflow>>
			{
				auto row = detail::prepare(aDatabase, "SELECT document FROM workflow_configurations ORDER BY position");
				if (!row) {
					return std::nullopt;
				}
				auto workflows = std::vector<core::local_workflow>{ };
				while (sqlite3_step(row->Value) == SQLITE_ROW) {
					const auto text     = column_text(row->Value, 0);
					auto       workflow = text ? core::decode_local_workflow(*text) : std::nullopt;
					// One unreadable row is left out rather than hiding the rest.
					if (workflow) {
						workflows.push_back(std::move(*workflow));
					}
				}
				return workflows;
			}

			[[nodiscard]] static auto write_workflows(sqlite3* const aDatabase, const std::vector<core::local_workflow>& aWorkflows) -> bool
			{
				if (!detail::execute(aDatabase, "DELETE FROM workflow_configurations")) {
					return false;
				}
				for (std::size_t index = 0U; index < aWorkflows.size(); ++index) {
					auto row = detail::prepare(aDatabase, "INSERT INTO workflow_configurations VALUES(?1,?2)");
					if (!row) {
						return false;
					}
					sqlite3_bind_int64(row->Value, 1, static_cast<std::int64_t>(index));
					bind_text(row->Value, 2, core::encode_local_workflow(aWorkflows[index]));
					if (sqlite3_step(row->Value) != SQLITE_DONE) {
						return false;
					}
				}
				return true;
			}

			[[nodiscard]] static auto write_provenance(sqlite3* const aDatabase, const application::agent_configuration_snapshot& aAfter) -> bool
			{
				if (!detail::execute(aDatabase, "DELETE FROM prompt_provenance")) {
					return false;
				}
				for (std::size_t index = 0U; index < core::writing_hook_count; ++index) {
					const auto& version = aAfter.WrittenAgainst[index];
					if (version && !aAfter.Hooks.Overrides[index].empty() && !insert_provenance(aDatabase, index, aAfter.Hooks.Overrides[index], *version)) {
						return false;
					}
				}
				return true;
			}

			std::filesystem::path Root;
		};
	} // namespace

	auto make_agent_configuration_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_configuration_store_port>
	{
		return std::make_unique<agent_configuration_store>(aRoot);
	}

	auto make_agent_finding_card_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_finding_card_store_port>
	{
		return std::make_unique<agent_finding_card_store>(aRoot);
	}

	auto make_prompt_turn_name_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::prompt_turn_name_store_port>
	{
		return std::make_unique<prompt_turn_name_store>(aRoot);
	}

	auto make_agent_queue_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::agent_queue_store_port>
	{
		return std::make_unique<agent_queue_store>(aRoot);
	}

	auto make_prompt_macro_store(const std::filesystem::path& aRoot) -> std::unique_ptr<application::prompt_macro_store_port>
	{
		return std::make_unique<prompt_macro_store>(aRoot);
	}

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
