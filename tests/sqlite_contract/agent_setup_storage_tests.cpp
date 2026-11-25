// [[BPR-1749]], RFC-BPR-0092. A profile's agent setup preferences:
// defaults before any write, a restart reads them back, refused values are not
// written, an identity mismatch is refused, and the file is owner-private.
#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#if defined(BPR_TEST_AGENT_SETUP_HOST)
	#include "bpr/application/agent_setup_host.hpp"
#endif
#include "bpr/application/manual_executor.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#ifndef _WIN32
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using tests::expect;

	/// [[BPR-2254]]. Runs `aSql` on the file through a plain connection, as
	/// an earlier build's schema is put back.
	void run_directly(const std::filesystem::path& aFile, const std::string& aSql, const std::string_view aWhat)
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(aFile.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		const auto ran = sqlite3_exec(database, aSql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
		sqlite3_close_v2(database);
		expect(ran, aWhat);
	}

	/// Every table's definition and rows, in order, as text.
	[[nodiscard]] auto file_contents(const std::filesystem::path& aFile) -> std::string
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(aFile.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "the file opens directly");
		auto tables = std::vector<std::pair<std::string, std::string>>{ };
		{
			sqlite3_stmt* row = nullptr;
			expect(sqlite3_prepare_v2(database, "SELECT name, sql FROM sqlite_schema WHERE type='table' ORDER BY name", -1, &row, nullptr) == SQLITE_OK, "the schema is read");
			while (sqlite3_step(row) == SQLITE_ROW) {
				tables.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(row, 0)), reinterpret_cast<const char*>(sqlite3_column_text(row, 1)));
			}
			sqlite3_finalize(row);
		}
		auto contents = std::string{ };
		for (const auto& [name, sql] : tables) {
			contents += sql + "\n";
			sqlite3_stmt* row = nullptr;
			expect(sqlite3_prepare_v2(database, ("SELECT * FROM \"" + name + "\" ORDER BY rowid").c_str(), -1, &row, nullptr) == SQLITE_OK, "a table's rows are read");
			while (sqlite3_step(row) == SQLITE_ROW) {
				for (auto column = 0; column < sqlite3_column_count(row); ++column) {
					const auto* const text = sqlite3_column_text(row, column);
					contents += text == nullptr ? std::string{ "NULL" } : std::string{ reinterpret_cast<const char*>(text) };
					contents += '|';
				}
				contents += '\n';
			}
			sqlite3_finalize(row);
		}
		sqlite3_close_v2(database);
		return contents;
	}

	[[nodiscard]] auto count_directly(const std::filesystem::path& aFile, const std::string& aSql) -> std::int64_t
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(aFile.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "the file opens directly");
		sqlite3_stmt* row = nullptr;
		expect(sqlite3_prepare_v2(database, aSql.c_str(), -1, &row, nullptr) == SQLITE_OK && sqlite3_step(row) == SQLITE_ROW, "a count is read");
		const auto counted = sqlite3_column_int64(row, 0);
		sqlite3_finalize(row);
		sqlite3_close_v2(database);
		return counted;
	}

	/// The native turns table as build 63 and earlier made it, before
	/// [[BPR-2241]]'s `seconds`, holding the rows it had.
	constexpr auto build_63_native_turns = std::string_view{
		"CREATE TABLE earlier_turns(conversation TEXT NOT NULL REFERENCES native_agent_sessions(id), ordinal INTEGER NOT NULL, asked TEXT NOT NULL, answered TEXT NOT NULL, ending TEXT NOT NULL, outcome INTEGER NOT NULL, PRIMARY KEY(conversation,ordinal)) STRICT;"
		"INSERT INTO earlier_turns SELECT conversation,ordinal,asked,answered,ending,outcome FROM native_agent_turns;"
		"DROP TABLE native_agent_turns; ALTER TABLE earlier_turns RENAME TO native_agent_turns;"
	};

	/// The queue entries table as a build before `aPrompt` ([[BPR-2181]]
	/// AC13) or `aClaim` ([[BPR-2152]]) made it, holding the rows it had;
	/// `aHeldCheck` false leaves out the check on `held`, so a test can put in
	/// a row the current format refuses.
	[[nodiscard]] auto earlier_queue_entries(const bool aPrompt, const bool aClaim, const bool aHeldCheck = true) -> std::string
	{
		const auto columns = std::string{ "session,position,id,quick_ask,scope,file,thread,label,text,state,held" } + (aPrompt ? ",prompt" : "") + (aClaim ? ",claim" : "");
		return std::string{ "CREATE TABLE earlier_entries(session TEXT NOT NULL REFERENCES agent_queues(session), position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 15), id INTEGER NOT NULL CHECK(id >= 1),"
							" quick_ask TEXT NOT NULL, scope TEXT NOT NULL, file TEXT NOT NULL, thread TEXT NOT NULL, label TEXT NOT NULL, text TEXT NOT NULL CHECK(length(CAST(text AS BLOB)) <= 16384), state TEXT NOT NULL," }
		+ (aHeldCheck ? " held INTEGER NOT NULL CHECK(held BETWEEN 0 AND 1)," : " held INTEGER NOT NULL,")
			+ (aPrompt ? " prompt TEXT NOT NULL DEFAULT '' CHECK(length(CAST(prompt AS BLOB)) <= 40960)," : "")
			+ (aClaim ? " claim TEXT NOT NULL DEFAULT '' CHECK(length(CAST(claim AS BLOB)) <= 256)," : "")
			+ " PRIMARY KEY(session, position)) STRICT;"
			+ "INSERT INTO earlier_entries SELECT " + columns + " FROM agent_queue_entries;"
			+ "DROP TABLE agent_queue_entries; ALTER TABLE earlier_entries RENAME TO agent_queue_entries;";
	}

	/// [[BPR-2254]]. Agent settings, a native conversation and a queue, as a
	/// profile holds them.
	struct seeded_profile
	{
		application::agent_setup_preferences                        Preferences;
		std::optional<application::native_agent_history_projection> History;
		std::map<std::string, core::agent_queue>                    Queues;
	};

	[[nodiscard]] auto seed_profile(const std::filesystem::path& aRoot, const bool aPromptedEntry) -> seeded_profile
	{
		auto  seeded                   = seeded_profile{ };
		auto& preferences              = seeded.Preferences;
		preferences.Agents             = *core::add_agent_entry(preferences.Agents, { .Name = "Claude stream JSON", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/claude-wrapper", .Arguments = { "--profile=work" } });
		preferences.Agents             = *core::add_agent_entry(preferences.Agents, core::preset_agent_entry(core::setup_agent::codex, core::codex_protocol_choice::app_server, { }));
		preferences.Agents[0].Identity = std::string(32U, 'c');
		preferences.AgentsMoved        = true;
		preferences.PreferredAgent     = preferences.Agents[0].Id;
		expect(adapters::sqlite::make_agent_setup_store(aRoot)->store(preferences), "the profile's agents are stored");

		auto natives        = adapters::sqlite::make_native_agent_session_store(aRoot);
		auto binding        = core::agent_revision_binding{ };
		binding.Host        = core::git_host_id{ 1 };
		binding.Repository  = core::repository_id{ 2 };
		binding.PullRequest = core::pull_request_id{ 3 };
		binding.BaseOid     = *core::parse_git_commit_oid(std::string(40U, '1'));
		binding.HeadOid     = *core::parse_git_commit_oid(std::string(40U, '2'));
		binding.LeftEpoch   = core::epoch_ordinal{ 1 };
		binding.RightEpoch  = core::epoch_ordinal{ 2 };
		const auto session  = natives->select("authority:agent-a:agent-tab", "claude", binding);
		expect(session && natives->append(session->Id, { .Asked = "Does this work?", .Answered = "Yes.", .Ending = "Answered" })
				   && natives->append(session->Id, { .Asked = "Why?", .Answered = "The bound is checked.", .Ending = "Answered" }),
			   "a native conversation's turns are stored");
		if (auto history = natives->history("authority:agent-a:agent-tab", "claude"); history) {
			seeded.History = std::move(*history);
		}
		expect(seeded.History && seeded.History->Turns.size() == 2U, "and read back");

		auto entry          = core::queued_entry{ };
		entry.Id            = 1U;
		entry.Input.Text    = "waiting";
		entry.Input.Threads = { "thread-a" };
		if (aPromptedEntry) {
			auto offer           = core::prompt_offer{ };
			offer.Id             = "own.look-again";
			offer.Label          = "Ready to look again?";
			offer.Expansion.Text = "Has it changed?";
			entry.Input.Prompt   = std::move(offer);
		}
		auto queue   = core::agent_queue{ };
		queue.LastId = 1U;
		queue.Paused = core::queue_pause::reviewer;
		queue.Entries.push_back(entry);
		auto queues = adapters::sqlite::make_agent_queue_store(aRoot);
		expect(queues->store("session", queue), "a queued entry is stored");
		const auto stored = queues->load();
		expect(stored && stored->size() == 1U, "and read back");
		seeded.Queues = *stored;
		return seeded;
	}

	/// The profile reads back as it was seeded, through the store's own
	/// open on the hardened connection.
	void expect_profile(const std::filesystem::path& aRoot, const seeded_profile& aSeeded, const std::string_view aWhat)
	{
		const auto preferences = adapters::sqlite::make_agent_setup_store(aRoot)->load();
		expect(preferences.has_value(), aWhat);
		expect(*preferences == aSeeded.Preferences && preferences->PreferredAgent == aSeeded.Preferences.Agents[0].Id, "every saved agent and the preferred one read back unchanged");
		const auto history = adapters::sqlite::make_native_agent_session_store(aRoot)->history("authority:agent-a:agent-tab", "claude");
		expect(history && history->Turns == aSeeded.History->Turns && std::ranges::none_of(history->Turns, [](const application::agent_conversation_turn& aTurn) { return aTurn.Seconds.has_value(); }),
			   "the conversation keeps its turns, with no time kept for them");
		const auto queues = adapters::sqlite::make_agent_queue_store(aRoot)->load();
		expect(queues == std::optional{ aSeeded.Queues }, "the queued entry reads back");
	}

#if defined(BPR_TEST_AGENT_SETUP_HOST)
	/// A process port that runs nothing and records what it was asked. Since
	/// [[BPR-2320]] a custom Claude agent's check ends with one stream JSON
	/// turn through its program; that turn is answered as Claude answers it,
	/// with its init frame and a line of text.
	class recording_process final : public application::agent_process_port
	{
	public:
		[[nodiscard]] auto run(const application::agent_launch_request& aRequest, int, const application::agent_output_sink& = { }, std::stop_token = { }) -> application::agent_run_result override
		{
			Requests.push_back(aRequest);
			auto result     = application::agent_run_result{ };
			result.Outcome  = application::agent_run_outcome::success;
			result.ExitCode = 0;
			result.Session  = aRequest.Session;
			if (aRequest.Protocol == core::agent_protocol::claude_stream_json) {
				auto event = application::agent_stream_event{ };
				event.Text = "I am ready.";
				result.Events.push_back(std::move(event));
				result.Metadata.ProtocolReported.push_back({ .Basis = "claude_stream_json.system/init: session_id,model,claude_code_version" });
			}
			return result;
		}
		void cancel() noexcept override { }

		std::vector<application::agent_launch_request> Requests;
	};

	/// [[BPR-2254]] AC1. The setup host over the store's real open: the
	/// saved custom agent is the one setup checks, and its check inspects its
	/// program, as build 63's did, instead of failing as not found.
	void expect_custom_check(const std::filesystem::path& aRoot, const std::string& aProgram)
	{
		auto       store     = adapters::sqlite::make_agent_setup_store(aRoot);
		auto       effects   = application::manual_executor{ };
		auto       checks    = application::manual_executor{ };
		auto       decision  = application::manual_executor{ };
		auto       process   = recording_process{ };
		auto       inspected = std::vector<std::string>{ };
		auto       host      = application::agent_setup_host{ application::agent_setup_dependencies{
			.Store   = store.get(),
			.Process = &process,
			.Locate  = [](core::agent_program) { return std::optional<core::agent_search_candidate>{ }; },
			.Inspect =
				[&inspected](const std::string_view aPath) {
					inspected.emplace_back(aPath);
					return std::optional<core::agent_wrapper_refusal>{ };
				},
			.Programs = std::make_shared<application::agent_program_source>(),
			.Parent   = [](const std::string&) { return std::optional<std::string>{ }; },
			.Profiles = core::built_in_agent_profiles(),
			.Effects  = &effects,
			.Checks   = &checks,
			.Decision = &decision,
		} };
		const auto settle    = [&] {
			for (auto turns = 0; turns < 32 && (effects.pending() != 0U || checks.pending() != 0U || decision.pending() != 0U); ++turns) {
				effects.drain();
				checks.drain();
				decision.drain();
			}
		};
		host.observe_policy(core::built_in_agent_policy(core::git_host_id{ 1U }, core::built_in_agent_host_kind::github_com));
		host.load();
		settle();
		expect(!host.projection().SettingsUnreadable && host.projection().Preferences.Agents.size() == 2U, "the setup host reads the saved agents");
		host.check();
		settle();
		const auto check = host.projection().State.AgentCheck;
		expect(check.Failure != core::agent_setup_failure::agent_not_found && check.Failure != core::agent_setup_failure::settings_unreadable, "the saved custom agent is found");
		expect(check.State == core::setup_check_state::passed && std::ranges::find(inspected, aProgram) != inspected.end(), "its check inspects its own program and passes");
		host.shutdown();
	}
#endif

	void earlier_builds_open()
	{
		// [[BPR-2254]]. The reason no store adds a column: on the hardened
		// connection a statement is prepared with SQLITE_PREPARE_NO_VTAB, and
		// SQLite checks a STRICT table's new column through the
		// pragma_quick_check table-valued function. If a SQLite update changes
		// that, this says so.
		{
			tests::sqlite_contract::scratch_root root{ "add-column-refused" };
			expect(adapters::sqlite::detail::sqlite_runtime_ready(), "the SQLite runtime is ready");
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2((root.path() / "strict.sqlite3").string().c_str(), &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK, "a scratch file opens");
			const auto configured = adapters::sqlite::detail::configure_connection(database).has_value();
			const auto made       = adapters::sqlite::detail::execute(database, "CREATE TABLE t(a INTEGER NOT NULL) STRICT").has_value()
				&& adapters::sqlite::detail::execute(database, "INSERT INTO t VALUES(1)").has_value();
			const auto altered = adapters::sqlite::detail::execute(database, "ALTER TABLE t ADD COLUMN b INTEGER").has_value();
			const auto plain   = sqlite3_exec(database, "ALTER TABLE t ADD COLUMN c INTEGER", nullptr, nullptr, nullptr) == SQLITE_OK;
			sqlite3_close_v2(database);
			expect(configured && made, "a STRICT table on the hardened connection");
			expect(!altered, "ADD COLUMN on a STRICT table fails on the hardened connection");
			expect(plain, "and only for the prepare flag: the same connection without it adds the column");
		}
		// Build 63: native turns without `seconds`, and no prompt pack links.
		{
			tests::sqlite_contract::scratch_root root{ "agent-setup-build-63" };
			const auto                           file = root.path() / "state/v1/agent-setup.sqlite3";
			// [[BPR-2377]]. The turns now live in the conversations store.
			const auto turnsFile = root.path() / "state/v1/agent-sessions.sqlite3";
			const auto seeded    = seed_profile(root.path(), true);
			run_directly(file, "DROP TABLE prompt_macro_packs;", "build 63's agent setup is put back");
			run_directly(turnsFile, std::string{ build_63_native_turns }, "build 63's turns are put back");
			expect(count_directly(turnsFile, "SELECT count(*) FROM sqlite_schema WHERE name='native_agent_turns' AND sql LIKE '%seconds%'") == 0 && count_directly(turnsFile, "SELECT count(*) FROM native_agent_turns") == 2,
				   "the turns have no seconds column and keep their rows");
			expect_profile(root.path(), seeded, "a store build 63 wrote opens");
			expect(count_directly(turnsFile, "SELECT count(*) FROM native_agent_turns WHERE seconds IS NULL") == 2 && count_directly(file, "SELECT count(*) FROM sqlite_schema WHERE name='prompt_macro_packs'") == 1
					   && count_directly(turnsFile, "SELECT count(*) FROM sqlite_schema WHERE name LIKE 'rebuilt_%'") == 0,
				   "the turns gain seconds as null, prompt_macro_packs is made, and nothing is left over");
			expect(count_directly(turnsFile, "SELECT count(*) FROM sqlite_schema WHERE sql LIKE '%REFERENCES native_agent_turns%'") == 0
					   && count_directly(file, "SELECT count(*) FROM sqlite_schema WHERE sql LIKE '%REFERENCES agent_queue_entries%' OR sql LIKE '%REFERENCES \"agent_queue_entries\"%'") == 0,
				   "no table references a rebuilt table, so foreign keys stay on through the rebuild");
			expect_profile(root.path(), seeded, "and opens again as it is");
#if defined(BPR_TEST_AGENT_SETUP_HOST)
			expect_custom_check(root.path(), seeded.Preferences.Agents[0].Program);
#endif
		}
		// Builds 64 to 66 left such a store part-migrated: every table made
		// before the failing step, none after it.
		{
			tests::sqlite_contract::scratch_root root{ "agent-setup-part-migrated" };
			const auto                           file   = root.path() / "state/v1/agent-setup.sqlite3";
			const auto                           seeded = seed_profile(root.path(), true);
			// [[BPR-2377]]. Its turns are in the conversations store now.
			run_directly(root.path() / "state/v1/agent-sessions.sqlite3", std::string{ build_63_native_turns }, "its turns are put back");
			run_directly(file, "DROP TABLE prompt_surface_lists; DROP TABLE prompt_repository_prompts; DROP TABLE prompt_macro_revision; DROP TABLE prompt_macro_packs;",
						 "a part-migrated store is put back");
			expect_profile(root.path(), seeded, "a part-migrated store opens");
			for (const auto* const table : { "prompt_surface_lists", "prompt_repository_prompts", "prompt_macro_revision", "prompt_macro_packs" }) {
				expect(count_directly(file, std::string{ "SELECT count(*) FROM sqlite_schema WHERE name='" } + table + "'") == 1, "and its migration completes");
			}
		}
		// Queue entries from before `prompt`, before `claim`, and before both.
		for (const auto& [prompt, claim] : { std::pair{ false, true }, std::pair{ true, false }, std::pair{ false, false } }) {
			tests::sqlite_contract::scratch_root root{ "agent-queue-earlier" };
			const auto                           file   = root.path() / "state/v1/agent-setup.sqlite3";
			const auto                           seeded = seed_profile(root.path(), prompt);
			run_directly(file, earlier_queue_entries(prompt, claim), "an earlier build's queue entries are put back");
			expect(count_directly(file, "SELECT count(*) FROM agent_queue_entries") == 1, "holding the queued entry");
			expect_profile(root.path(), seeded, "a store with earlier queue entries opens");
			expect(count_directly(file, "SELECT count(*) FROM sqlite_schema WHERE name='agent_queue_entries' AND sql LIKE '%prompt TEXT%' AND sql LIKE '%claim TEXT%'") == 1, "and its entries have both columns");
		}
		// A rebuild that fails part-way is rolled back: the file is as it was.
		{
			tests::sqlite_contract::scratch_root root{ "agent-queue-rollback" };
			const auto                           file = root.path() / "state/v1/agent-setup.sqlite3";
			static_cast<void>(seed_profile(root.path(), false));
			run_directly(file, earlier_queue_entries(false, true, false) + "UPDATE agent_queue_entries SET held=2;", "an earlier table holding a row the current format refuses");
			const auto before = file_contents(file);
			expect(!adapters::sqlite::make_agent_queue_store(root.path())->load(), "the store does not open when its rebuild fails");
			expect(!adapters::sqlite::make_agent_setup_store(root.path())->load(), "nor for the agent settings");
			expect(file_contents(file) == before, "the failed rebuild leaves every table and row as it was");
			expect(count_directly(file, "SELECT count(*) FROM agent_queue_entries WHERE held=2") == 1, "the original row is still there");
		}
	}
} // namespace

int main()
try {
	tests::sqlite_contract::scratch_root scratch{ "agent-setup" };
	const auto                           path = scratch.path() / "state/v1/agent-setup.sqlite3";
	// [[BPR-2377]]. The conversations keep their own store.
	const auto sessionsPath = scratch.path() / "state/v1/agent-sessions.sqlite3";
	auto       store        = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(!std::filesystem::exists(path), "construction writes nothing");
	const auto first = store->load();
	expect(first && *first == application::agent_setup_preferences{ }, "a profile with nothing stored reads defaults");

	auto chosen           = application::agent_setup_preferences{ };
	chosen.Chosen         = core::setup_agent::codex;
	chosen.CodexProgram   = "/opt/tools/codex-wrapper";
	chosen.HarnessProgram = "/opt/corp/harness";
	chosen.CodexProtocol  = core::codex_protocol_choice::jsonl;
	expect(store->store(chosen), "preferences are stored");
	store.reset();
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == chosen, "a restarted profile reads the same preferences back");

	auto none   = chosen;
	none.Chosen = std::nullopt;
	expect(store->store(none) && store->load() == none, "no chosen agent is kept as none");

	auto tooLong         = chosen;
	tooLong.CodexProgram = "/" + std::string(2000U, 'a');
	expect(!store->store(tooLong), "an over-long program is not written");
	auto control           = chosen;
	control.HarnessProgram = "/bin/harness\n--flag";
	expect(!store->store(control), "a program with a control character is not written");
	expect(store->load() == none, "a refused write leaves the stored preferences as they were");

	// [[BPR-1750]]. Clone folders and repository links in the same file.
	auto links = adapters::sqlite::make_clone_link_store(scratch.path());
	expect(links->load_clone_folders() == std::vector<std::string>{ } && links->load_links() == std::vector<application::repository_link>{ }, "no folders or links at first");
	const auto folders = std::vector<std::string>{ "/Users/me/Documents/GitHub", "/Users/me/src" };
	expect(links->store_clone_folders(folders) && links->load_clone_folders() == folders, "folders keep their order");
	expect(links->store_clone_folders(std::vector<std::string>{ "/Users/me/src" }) && links->load_clone_folders() == std::vector<std::string>{ "/Users/me/src" },
		   "storing folders replaces them");
	auto tooMany = std::vector<std::string>(core::maximum_clone_folders + 1U, "/x");
	expect(!links->store_clone_folders(tooMany), "more than the folder bound is refused");
	auto link       = application::repository_link{ };
	link.Host       = "github.com";
	link.Repository = 42U;
	link.Owner      = "bpr-fixtures";
	link.Name       = "engine";
	link.Path       = "/Users/me/src/engine";
	link.Remote     = "origin";
	expect(links->store_link(link), "a link is stored");
	auto copy        = link;
	copy.Repository  = 43U;
	copy.Name        = "other";
	copy.Path        = { };
	copy.Remote      = { };
	copy.PrivateCopy = true;
	expect(links->store_link(copy), "a private copy choice is stored");
	auto invalid        = link;
	invalid.PrivateCopy = true;
	expect(!links->store_link(invalid), "a private copy with a path is refused");
	links.reset();
	links          = adapters::sqlite::make_clone_link_store(scratch.path());
	const auto all = links->load_links();
	expect(all && all->size() == 2U && (*all)[0] == link && (*all)[1] == copy, "links read back after a restart");
	expect(links->forget_link("github.com", 42U, "bpr-fixtures", "engine") && links->load_links()->size() == 1U, "Forget removes one link");
	expect(store->load() == none, "the preferences are untouched by links");

	// [[BPR-1752]]. The edited review skill and its record.
	using loaded_skill = std::optional<std::optional<application::stored_review_skill>>;
	const auto noSkill = loaded_skill{ std::in_place, std::nullopt };
	auto       skills  = adapters::sqlite::make_review_skill_store(scratch.path());
	expect(skills->load_review_skill() == noSkill, "no edited skill reads as none");
	auto edited               = application::stored_review_skill{ .Text = "---\nname: bpr-pr-review\ndescription: d\n---\nMine.\n" };
	edited.Record.BaseVersion = 3U;
	edited.Record.BaseDigest.fill(0xABU);
	expect(skills->store_review_skill(edited), "an edited skill is stored");
	skills.reset();
	skills = adapters::sqlite::make_review_skill_store(scratch.path());
	expect(skills->load_review_skill() == loaded_skill{ std::in_place, edited }, "and reads back whole after a restart, with no kept version");
	edited.Record.KeptVersion = 4U;
	expect(skills->store_review_skill(edited) && skills->load_review_skill() == loaded_skill{ std::in_place, edited }, "Keep mine's version is stored");
	expect(!skills->store_review_skill({ .Text = std::string(core::maximum_review_skill_bytes + 1U, 'x') }), "a skill over its bound is refused");
	expect(skills->forget_review_skill() && skills->load_review_skill() == noSkill, "Restore default removes it");
	expect(store->load() == none && links->load_links()->size() == 1U, "the preferences and links are untouched by the skill");

	// [[BPR-1753]], owner decision (b). Each pull request's conversation.
	using turns       = std::vector<application::agent_conversation_turn>;
	auto       talks  = adapters::sqlite::make_agent_conversation_store(scratch.path());
	const auto pull   = application::agent_conversation_key{ .Host = "github.com", .Owner = "MikeMarcin", .Name = "BPR-Test", .Number = 7U };
	auto       other  = pull;
	other.Number      = 8U;
	const auto thread = turns{ { .Asked = "Does this work?", .Answered = "Yes: the loop ends.", .Ending = "Claude Code answered in 0:12." },
							   { .Asked = "Why?", .Answered = "Because the bound is checked.", .Ending = "Claude Code answered in 0:08." } };
	expect(talks->load(pull) == std::optional{ turns{ } }, "a pull request with no conversation reads as none");
	expect(talks->store(pull, thread) && talks->store(other, { thread.front() }), "conversations are stored per pull request");
	talks.reset();
	talks = adapters::sqlite::make_agent_conversation_store(scratch.path());
	expect(talks->load(pull) == std::optional{ thread } && talks->load(other) == std::optional{ turns{ thread.front() } }, "and read back in order after a restart");
	expect(talks->store(pull, { }) && talks->load(pull) == std::optional{ turns{ } } && talks->load(other)->size() == 1U, "Clear session removes only that pull request's");
	expect(!talks->store(pull, turns(application::maximum_conversation_turns + 1U)), "more turns than the bound are refused");
	expect(!talks->store(application::agent_conversation_key{ }, thread), "a conversation names its pull request");
	// Owner decision 2026-10-23: one conversation per agent, and the agent a
	// pull request was last asked with.
	auto workCodex  = other;
	workCodex.Agent = 2U;
	expect(talks->store(workCodex, thread) && talks->load(workCodex) == std::optional{ thread } && talks->load(other)->size() == 1U, "each agent keeps its own conversation");
	expect(!talks->load_agent(other) && talks->store_agent(workCodex) && talks->load_agent(other) == std::optional<std::uint32_t>{ 2U }, "the agent last asked with is kept per pull request");
	// [[BPR-1755]]: Reset agents forgets those choices, never a turn.
	expect(talks->forget_agents() && !talks->load_agent(other) && talks->load(workCodex) == std::optional{ thread }, "forgetting the agents' choices keeps every conversation");
	expect(talks->store_agent(workCodex), "a choice is kept again afterwards");
	expect(store->load() == none && links->load_links()->size() == 1U, "the preferences and links are untouched by conversations");
	// [[BPR-1847]]. How each turn ended is kept with it, and a turn stored
	// before outcomes were kept reads back from the ending this app wrote.
	auto failed      = pull;
	failed.Number    = 9U;
	const auto ended = turns{ { .Asked = "Review this pull request", .Answered = "my-agent answered in a form this app can't read.", .Ending = "Stopped after 0:15.", .Outcome = bpr::core::agent_turn_outcome::unreadable },
							  { .Asked = "Why?", .Answered = "Because.", .Ending = "my-agent answered in 0:08.", .Outcome = bpr::core::agent_turn_outcome::answered },
							  { .Asked = "Again", .Answered = "Stopped before my-agent answered.", .Ending = "Stopped after 0:02.", .Outcome = bpr::core::agent_turn_outcome::stopped } };
	expect(talks->store(failed, ended), "a thread with its turns' outcomes is stored");
	talks.reset();
	talks = adapters::sqlite::make_agent_conversation_store(scratch.path());
	expect(talks->load(failed) == std::optional{ ended }, "each turn reads back with how it ended");
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(sessionsPath.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "UPDATE agent_conversation_outcomes SET outcome=11", nullptr, nullptr, nullptr) != SQLITE_OK, "an undeclared outcome is refused by the schema");
		expect(sqlite3_exec(database, "DELETE FROM agent_conversation_outcomes WHERE number=9", nullptr, nullptr, nullptr) == SQLITE_OK, "a thread stored before outcomes were kept");
		sqlite3_close_v2(database);
	}
	expect(talks->load(failed) == std::optional{ turns{ } }, "turns kept before their outcomes were are left out, and the thread still loads");
	expect(talks->store(failed, { }) && talks->load(failed) == std::optional{ turns{ } }, "Clear session removes the outcomes with the turns");
	// [[BPR-2261]]. A file made before `closed_with_app` checked the outcome
	// against the ten before it: its table is rebuilt, its rows kept, and a
	// turn the app's closing stopped is stored.
	{
		expect(talks->store(failed, ended), "a thread stored again");
		talks.reset();
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(sessionsPath.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		const auto old = "BEGIN;"
						 "CREATE TABLE old_outcomes(host TEXT NOT NULL,owner TEXT NOT NULL,name TEXT NOT NULL,number INTEGER NOT NULL,agent INTEGER NOT NULL,"
						 "position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 49),outcome INTEGER NOT NULL CHECK(outcome BETWEEN 0 AND 9),"
						 "PRIMARY KEY(host, owner, name, number, agent, position)) STRICT;"
						 "INSERT INTO old_outcomes SELECT * FROM agent_conversation_outcomes;"
						 "DROP TABLE agent_conversation_outcomes;"
						 "ALTER TABLE old_outcomes RENAME TO agent_conversation_outcomes;"
						 "COMMIT;";
		expect(sqlite3_exec(database, old, nullptr, nullptr, nullptr) == SQLITE_OK, "the outcomes table as build 68 made it");
		sqlite3_close_v2(database);
		talks = adapters::sqlite::make_agent_conversation_store(scratch.path());
		expect(talks->load(failed) == std::optional{ ended }, "its turns read back as they ended");
		auto closed       = ended;
		closed[2].Outcome = bpr::core::agent_turn_outcome::closed_with_app;
		expect(talks->store(failed, closed) && talks->load(failed) == std::optional{ closed }, "a turn the app's closing stopped is kept as that");
		expect(talks->store(failed, { }), "cleared again");
	}

	// Owner report 2026-11-10, after BPR-2052. What was done with each finding
	// card of a stored answer, read back after a restart.
	{
		auto       cards = adapters::sqlite::make_agent_finding_card_store(scratch.path());
		const auto block = core::digest_agent_findings("{\"findings\":[]}");
		// [[BPR-2325]]. A drafted card keeps the draft it made.
		const auto drafted = core::agent_finding_card_record{ .Findings = block, .Ordinal = 1U, .State = core::agent_finding_card_disposition::drafted, .Note = 7U };
		const auto refused = core::agent_finding_card_record{ .Findings = block, .Ordinal = 3U, .State = core::agent_finding_card_disposition::refused, .Why = "Not drafted: resolve the draft open at that line first." };
		const auto noted   = core::agent_finding_card_record{ .Findings = block, .Ordinal = 2U, .State = core::agent_finding_card_disposition::noted, .Note = 42U };
		expect(cards->load() == std::optional{ std::vector<core::agent_finding_card_record>{ } }, "no card is kept at first");
		expect(cards->store(drafted) && cards->store(refused) && cards->store(noted), "drafted, refused and noted cards are kept");
		cards.reset();
		cards = adapters::sqlite::make_agent_finding_card_store(scratch.path());
		expect(cards->load() == std::optional{ std::vector{ drafted, refused, noted } }, "a restart reads them back, least recently changed first, a drafted card with its draft");
		auto undone  = refused;
		undone.State = core::agent_finding_card_disposition::open;
		expect(cards->store(undone) && cards->load() == std::optional{ std::vector{ drafted, noted } }, "an open card has no row");
		expect(cards->store(drafted) && cards->load() == std::optional{ std::vector{ noted, drafted } }, "storing a card again makes it the most recently changed");
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(sessionsPath.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "UPDATE agent_finding_cards SET state=5", nullptr, nullptr, nullptr) != SQLITE_OK, "an undeclared disposition is refused by the schema");
		expect(sqlite3_exec(database, "UPDATE agent_finding_cards SET state=0", nullptr, nullptr, nullptr) != SQLITE_OK, "an open card is never a row");
		sqlite3_close_v2(database);
		expect(store->load() == none, "the preferences are untouched by finding cards");
	}

	// [[BPR-1753]], owner amendment 2026-10-23. The agent list.
	auto listed            = *store->load();
	listed.Agents          = *core::add_agent_entry({ }, { .Name = "Work Claude", .Protocol = core::agent_protocol::claude_stream_json, .Program = "/opt/work/bin/wrapper" });
	listed.Agents          = *core::add_agent_entry(listed.Agents, { .Name = "Work Codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Program = "/opt/work/bin/wrapper", .Arguments = { "--codex", "--profile=work" }, .Effort = "high", .CheckArguments = { "--codex", "login", "status" } });
	listed.Agents          = *core::add_agent_entry(listed.Agents, core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, { }));
	listed.AgentsMoved     = true;
	listed.RunLimitMinutes = 45U;
	listed.PreferredAgent  = 2U;
	// Owner look v004: the Agent tab's Suggestions toggle is kept.
	listed.SuggestionsHidden = true;
	// RFC-BPR-0126 Q4/Q5: Follow-up behaviour, Steer, is kept.
	listed.FollowUpSteer      = true;
	listed.Agents[0].Identity = std::string(32U, 'a');
	expect(store->load()->ScratchLimitMiB == core::default_agent_scratch_limit_mib && store->load()->ScratchKeepDays == core::default_agent_scratch_keep_days, "the scratch policy starts at 256 MiB and 30 days");
	// RFC-BPR-0104 as amended 2026-11-11: the scratch folder limit and how
	// long an unused folder is kept are settings.
	listed.ScratchLimitMiB = 1024U;
	listed.ScratchKeepDays = 0U;
	// [[BPR-2293]]. The retained workspaces' days and cap start at 7 and
	// 20 GB and are settings.
	expect(store->load()->WorkspaceKeepDays == 7U && store->load()->WorkspaceCapGb == 20U, "the workspace policy starts at 7 days and 20 GB");
	listed.WorkspaceKeepDays = 0U;
	listed.WorkspaceCapGb    = 55U;
	expect(store->store(listed), "an agent list is stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed, "and reads back in order with its fixed arguments, preset and choice after a restart");
	// [[BPR-1918]] AC3. A custom agent's strict run checks are its own,
	// stored through the host and read back after a restart.
	listed.Agents[1].StrictRunChecks = false;
	expect(store->store(listed), "a custom agent with strict run checks off is stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed && store->load()->Agents[0].StrictRunChecks && !store->load()->Agents[1].StrictRunChecks, "it reads back off, and only for that agent");
	auto popular                      = listed;
	popular.Agents[2].StrictRunChecks = false;
	expect(core::inspect_agent_entries(popular.Agents) == core::agent_entry_refusal::relaxed_popular_agent && !store->store(popular), "a popular agent keeps strict run checks");
	// [[BPR-2307]]. A custom Codex agent's "Rely on the launcher's sandbox"
	// is its own, off until turned on, and read back after a restart.
	expect(!store->load()->Agents[1].RelyOnLauncherSandbox, "off until turned on");
	listed.Agents[1].RelyOnLauncherSandbox = true;
	expect(store->store(listed), "a custom Codex agent relying on its launcher's sandbox is stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed && store->load()->Agents[1].RelyOnLauncherSandbox && !store->load()->Agents[0].RelyOnLauncherSandbox, "it reads back on, and only for that agent");
	auto claudeAsks                            = listed;
	claudeAsks.Agents[0].RelyOnLauncherSandbox = true;
	expect(core::inspect_agent_entries(claudeAsks.Agents) == core::agent_entry_refusal::launcher_sandbox_not_custom_codex && !store->store(claudeAsks), "a Claude agent cannot");
	// [[BPR-2051]]. Each agent's last check is kept across a restart: a pass
	// with its version, a failure with why; a check of an agent no longer in
	// the list does not come back.
	listed.EntryChecks = { { .Id = listed.Agents[0].Id, .Check = { .State = core::setup_check_state::passed, .Version = "2.1.321" } },
						   { .Id = listed.Agents[1].Id, .Check = core::fail_check(core::agent_setup_failure::not_signed_in) },
						   { .Id = listed.Agents[2].Id, .Check = core::fail_check(core::agent_setup_failure::timed_out) } };
	expect(store->store(listed), "the agents' last checks are stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed, "and read back after a restart, with versions and failures");
	auto running                 = listed;
	running.EntryChecks[0].Check = { .State = core::setup_check_state::running };
	expect(!store->store(running), "a running check is not a result to keep");
	listed.Agents.pop_back();
	listed.EntryChecks.pop_back();
	expect(store->store(listed) && store->load()->Agents.size() == 2U && store->load()->EntryChecks.size() == 2U, "a removed agent is gone from the stored list, with its check");
	// [[BPR-2261]]. The pull request workspaces whose check passed are kept
	// across a restart, in order, under their workspace keys.
	listed.WorkspaceChecks = { "github.example/octo/review#17:base:head-1", "github.example/octo/review#18:base:head-2" };
	expect(store->store(listed), "the passed workspace checks are stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() && store->load()->WorkspaceChecks == listed.WorkspaceChecks, "and read back after a restart");
	auto overlong            = listed;
	overlong.WorkspaceChecks = { std::string(application::maximum_workspace_check_key_bytes + 1U, 'k') };
	expect(!store->store(overlong), "a key past its bound is not kept");
	listed.WorkspaceChecks.clear();
	expect(store->store(listed) && store->load()->WorkspaceChecks.empty(), "and none kept reads back as none");
	// [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104 AC08. Scratch folder starts
	// and Build and run tests are each agent's, kept across a restart; an
	// agent stored before them reads back on and off.
	expect(store->load()->Agents[0].ScratchStarts && !store->load()->Agents[0].BuildsAndTests, "an agent carried over starts with the folder on and builds off");
	listed.Agents[0].ScratchStarts  = false;
	listed.Agents[1].BuildsAndTests = true;
	expect(store->store(listed), "the grant settings are stored");
	store = adapters::sqlite::make_agent_setup_store(scratch.path());
	expect(store->load() == listed && !store->load()->Agents[0].ScratchStarts && store->load()->Agents[1].ScratchStarts && store->load()->Agents[1].BuildsAndTests,
		   "and read back per agent after a restart");
	// [[BPR-2139]], [[BPR-2140]], RFC-BPR-0104 AC10 (owner, 2026-11-12). MCP
	// servers and the instructions switch are each agent's; a profile written
	// before them (no such table) reads back Off and the system prompt.
	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "DROP TABLE agent_entry_launch", nullptr, nullptr, nullptr) == SQLITE_OK, "a profile from before the setting");
		sqlite3_close_v2(database);
		store              = adapters::sqlite::make_agent_setup_store(scratch.path());
		const auto carried = store->load();
		expect(carried && carried->Agents.size() == 2U
				   && std::ranges::all_of(carried->Agents, [](const core::agent_entry& aEntry) { return aEntry.McpServers == core::agent_mcp_servers::off && aEntry.Instructions == core::agent_instruction_delivery::system_prompt; }),
			   "agents carried over have MCP servers off and the system prompt");
		listed.Agents[1].McpServers   = core::agent_mcp_servers::own;
		listed.Agents[0].Instructions = core::agent_instruction_delivery::message;
		expect(store->store(listed), "the settings are stored");
		store = adapters::sqlite::make_agent_setup_store(scratch.path());
		expect(store->load() == listed && store->load()->Agents[0].McpServers == core::agent_mcp_servers::off && store->load()->Agents[1].McpServers == core::agent_mcp_servers::own
				   && store->load()->Agents[0].Instructions == core::agent_instruction_delivery::message,
			   "and read back per agent after a restart");
	}
	// AC09. The choice made in one pull request is that pull request's and
	// that agent's alone; the build answer is the pull request's.
	{
		auto       grants    = adapters::sqlite::make_agent_conversation_store(scratch.path());
		const auto mine      = application::agent_conversation_key{ .Host = "github.com", .Owner = "MikeMarcin", .Name = "BPR-Test", .Number = 40U, .Agent = 1U };
		auto       elsewhere = mine;
		elsewhere.Number     = 41U;
		auto otherAgent      = mine;
		otherAgent.Agent     = 2U;
		expect(!grants->load_scratch_choice(mine) && !grants->load_build_answer(mine), "nothing chosen yet");
		expect(grants->store_scratch_choice(mine, false) && grants->store_build_answer(mine, false), "a choice and an answer are stored");
		grants = adapters::sqlite::make_agent_conversation_store(scratch.path());
		expect(grants->load_scratch_choice(mine) == std::optional{ false } && !grants->load_scratch_choice(elsewhere) && !grants->load_scratch_choice(otherAgent), "after a restart, only that pull request and agent");
		expect(grants->load_build_answer(otherAgent) == std::optional{ false } && !grants->load_build_answer(elsewhere), "the build answer holds for the pull request");
		expect(grants->store_scratch_choice(mine, true) && grants->load_scratch_choice(mine) == std::optional{ true }, "turned back on");
		// Re-review N3, N4: the folder an agent kept in a pull request is
		// known after a restart, for that pull request and agent only.
		expect(!grants->load_scratch_folder(mine), "no folder kept yet");
		expect(grants->store_scratch_folder(mine, "native-0123abcd") && !grants->store_scratch_folder(mine, "../x") && !grants->store_scratch_folder(mine, "-x"), "a folder name is stored; a path is refused");
		grants = adapters::sqlite::make_agent_conversation_store(scratch.path());
		expect(grants->load_scratch_folder(mine) == std::optional<std::string>{ "native-0123abcd" } && !grants->load_scratch_folder(elsewhere) && !grants->load_scratch_folder(otherAgent),
			   "after a restart, only that pull request and agent");
		expect(grants->forget_agents() && !grants->load_scratch_choice(mine) && !grants->load_scratch_folder(mine), "a reset's new agents inherit no choice and no folder");
	}

#ifndef _WIN32
	struct stat status{ };
	expect(::stat(path.c_str(), &status) == 0 && (status.st_mode & 0077) == 0, "the file is owner-private");
#endif

	// Native identity is independent of application/window and invocation catalogs.
	auto natives        = adapters::sqlite::make_native_agent_session_store(scratch.path());
	auto binding        = core::agent_revision_binding{ };
	binding.Host        = core::git_host_id{ 1 };
	binding.Repository  = core::repository_id{ 2 };
	binding.PullRequest = core::pull_request_id{ 3 };
	binding.BaseOid     = *core::parse_git_commit_oid(std::string(40U, '1'));
	binding.HeadOid     = *core::parse_git_commit_oid(std::string(40U, '2'));
	binding.LeftEpoch   = core::epoch_ordinal{ 1 };
	binding.RightEpoch  = core::epoch_ordinal{ 2 };
	auto native         = natives->select("authority:agent-a:agent-tab", "claude", binding);
	expect(native.has_value(), "native conversation is admitted durably");
	native->Handle                 = "exact-native-id";
	native->Pending                = true;
	auto workspace                 = application::owned_agent_workspace{ };
	workspace.Workspace.Path       = "/synthetic-retained-workspace";
	workspace.Workspace.Shallow    = true;
	workspace.SparseMissingObjects = "?" + std::string(40U, 'a') + "\n";
	native->Workspace              = workspace;
	expect(natives->store(*native), "pending send is stored before submission");
	auto competing = *native;
	natives        = adapters::sqlite::make_native_agent_session_store(scratch.path());
	auto restored  = natives->select("authority:agent-a:agent-tab", "claude", binding);
	expect(restored && restored->Id == native->Id && restored->Handle == native->Handle && restored->Pending,
		   "restart restores exact native identity and uncertain submission, without inference");
	expect(restored->Workspace && restored->Workspace->SparseMissingObjects == native->Workspace->SparseMissingObjects, "private sparse object evidence survives restart with its exact conversation");
	restored->Pending = false;
	++restored->Turn;
	expect(natives->store(*restored) && !natives->store(competing), "revision CAS permits one writer and rejects stale activation");
	expect(!natives->select("authority:agent-a:agent-tab", "changed-runtime", binding), "runtime identity mismatch never relabels a saved native session");
	auto otherNative = natives->select("authority:agent-b:agent-tab", "claude", binding);
	expect(otherNative && otherNative->Id != restored->Id, "another agent incarnation gets a distinct native conversation");
	// Shared source ownership is independent of native conversation/configuration.
	restored->WorkspaceKey = "exact-authority:base:head";
	expect(natives->store(*restored), "existing legacy owner publishes its retained tree atomically");
	const auto shared = natives->shared_workspace(restored->WorkspaceKey, *otherNative);
	expect(shared && *shared && (**shared).Workspace.Path == restored->Workspace->Workspace.Path
			   && otherNative->Handle.empty() && otherNative->Id != restored->Id,
		   "other agent reads shared files without acquiring native handle or conversation");
	otherNative->Workspace    = **shared;
	otherNative->WorkspaceKey = restored->WorkspaceKey;
	expect(natives->store(*otherNative), "second native scope retains the exact same source ownership");
	auto alternate                      = *otherNative;
	alternate.Workspace->Workspace.Path = "/different-legacy-workspace";
	expect(!natives->shared_workspace(restored->WorkspaceKey, alternate)
			   && !natives->store(alternate),
		   "a conflicting legacy tree cannot replace shared source ownership");
	auto changedSource            = *otherNative;
	changedSource.Binding.HeadOid = *core::parse_git_commit_oid(std::string(40U, '3'));
	expect(!natives->shared_workspace(restored->WorkspaceKey, changedSource), "a corrupted cross-revision reference refuses rather than retargeting files");
	const auto unrelated = natives->shared_workspace("other-authority:base:head", *native);
	expect(unrelated && *unrelated && (**unrelated).Workspace.Path == native->Workspace->Workspace.Path,
		   "another PR authority has independent legacy adoption, never another PR's shared reference");
	auto lease = natives->try_lock_workspace(restored->WorkspaceKey);
	expect(lease && !natives->try_lock_workspace(restored->WorkspaceKey) && natives->try_lock_workspace("other-authority:base:head"),
		   "one shared-tree turn at a time while unrelated PR work stays eligible");
	lease.reset();
	expect(natives->try_lock_workspace(restored->WorkspaceKey) != nullptr, "turn ownership releases explicitly without an age/PID override");

	const auto missing = natives->find("unopened-scope", "claude", binding);
	expect(missing && !*missing, "viewing absent native history admits nothing");
	auto changedBinding    = binding;
	changedBinding.HeadOid = *core::parse_git_commit_oid(std::string(40U, '3'));
	const auto stale       = natives->select("authority:agent-a:agent-tab", "claude", changedBinding);
	expect(stale && stale->Id == restored->Id && stale->Binding == binding, "head movement never silently retargets native ownership");
	const auto fresh = natives->select("authority:agent-a:agent-tab", "claude", changedBinding, true);
	expect(fresh && fresh->Id != restored->Id && fresh->Binding == changedBinding && fresh->Handle.empty()
			   && natives->store(*restored),
		   "explicit new revision creates a distinct generation while old native provenance stays stored");

	const auto scope = std::string{ "authority:agent-a:agent-tab" };
	expect(natives->append(restored->Id, { "Question", "Saved native answer", "Answered" }), "native transcript saved by original generation");
	const auto opened = natives->choose(scope, "claude", binding, restored->Id);
	expect(opened && opened->Id == restored->Id, "history opens exact earlier generation");
	const auto newSession = natives->choose(scope, "claude", binding, "");
	expect(newSession && newSession->Id != restored->Id && newSession->Handle.empty(), "same-head New session retains native origin");
	const auto unusedAgain = natives->choose(scope, "claude", binding, "");
	expect(unusedAgain && unusedAgain->Id == newSession->Id, "repeated New session reuses unused empty generation");
	// [[BPR-2153]]. A turn keeps what the reviewer steered into it.
	const auto steeredTurn = application::agent_conversation_turn{ .Asked    = "Late turn",
																   .Answered = "Late original answer",
																   .Ending   = "Answered",
																   .Steered  = { { .Text = "Look at b.cpp instead", .At = 0U, .AfterSteps = 7U, .Disposition = core::agent_steer_disposition::consumed },
																				 { .Text = "And the tests", .At = 4U, .AfterSteps = 9U, .Disposition = core::agent_steer_disposition::unknown } },
																   .Seconds  = 72U };
	expect(natives->append(restored->Id, steeredTurn), "late output records only its originating history");
	const auto list = natives->history(scope, "claude");
	expect(list && list->Selected == newSession->Id && list->Turns.empty() && list->Entries.size() == 3U, "new generation empty while all scoped history retained");
	expect(!natives->choose("authority:agent-b:agent-tab", "claude", binding, restored->Id), "history selection cannot cross agent scope");
	expect(natives->choose(scope, "claude", binding, restored->Id).has_value(), "earlier history reopens without inference");
	natives                   = adapters::sqlite::make_native_agent_session_store(scratch.path());
	const auto durableHistory = natives->history(scope, "claude");
	const auto reopenedShared = natives->shared_workspace("exact-authority:base:head", *newSession);
	expect(reopenedShared && *reopenedShared && (**reopenedShared).Workspace.Path == restored->Workspace->Workspace.Path,
		   "restart and New session retain shared files even while the original owner is unselected");
	// [[BPR-2345]]. The workspace Agent setup's check made is kept as the pull
	// request's before any conversation holds it: after a restart the first
	// turn reads it, holding it makes it that conversation's, and expiry
	// lists it once throughout.
	{
		const auto keptKey             = std::string{ "kept-authority:base:head" };
		auto       probed              = application::owned_agent_workspace{ };
		probed.Workspace.Path          = "/synthetic-probed-workspace";
		probed.Workspace.Shallow       = true;
		probed.Workspace.PinnedHead    = binding.HeadOid;
		probed.SparseMissingObjects    = "?" + std::string(40U, 'b') + "\n";
		probed.OwnershipMarker         = "probe-marker";
		probed.Session                 = core::agent_session_id{ (std::uint64_t{ 1U } << 63U) + 1U };
		probed.DirectoryIdentity       = { .Opaque = { 2U, 7U, 0U, 1U } };
		auto otherHead                 = probed;
		otherHead.Workspace.PinnedHead = changedBinding.HeadOid;
		expect(!natives->keep_workspace(keptKey, binding, otherHead), "a workspace of another head is not kept");
		expect(natives->keep_workspace(keptKey, binding, probed) && !natives->keep_workspace(keptKey, binding, probed), "one workspace is kept under a key");
		expect(!natives->keep_workspace("exact-authority:base:head", binding, probed), "none where a conversation already holds the pull request's workspace");
		natives                = adapters::sqlite::make_native_agent_session_store(scratch.path());
		const auto keptListing = [&]() {
			auto matching = std::vector<application::retained_workspace_record>{ };
			if (const auto records = natives->retained_workspaces()) {
				std::ranges::copy_if(*records, std::back_inserter(matching), [&](const application::retained_workspace_record& aRecord) { return aRecord.Workspace.Workspace.Path == probed.Workspace.Path; });
			}
			return matching;
		};
		const auto waiting = keptListing();
		expect(waiting.size() == 1U && waiting.front().Keys == std::vector<std::string>{ keptKey } && !waiting.front().Ended && waiting.front().LastUse.has_value(),
			   "expiry lists the kept workspace under its key, waiting for its first turn");
		auto firstTurn = natives->select("authority:agent-c:agent-tab", "claude", binding);
		expect(firstTurn && !firstTurn->Workspace, "the first turn's conversation holds no workspace");
		const auto found = natives->shared_workspace(keptKey, *firstTurn);
		expect(found && *found && (**found).Workspace.Path == probed.Workspace.Path && (**found).Session == probed.Session && (**found).OwnershipMarker == probed.OwnershipMarker
				   && (**found).DirectoryIdentity == probed.DirectoryIdentity && (**found).SparseMissingObjects == probed.SparseMissingObjects,
			   "after a restart the first turn reads the kept workspace, its reserved identity and sparse evidence");
		auto movedHead    = *firstTurn;
		movedHead.Binding = changedBinding;
		expect(!natives->shared_workspace(keptKey, movedHead), "a conversation at another head is refused it");
		firstTurn->Workspace    = **found;
		firstTurn->WorkspaceKey = keptKey;
		expect(natives->store(*firstTurn), "the first turn holds it");
		const auto held = keptListing();
		expect(held.size() == 1U && held.front().Keys == std::vector<std::string>{ keptKey }, "held, it is listed once, as the conversation's");
		expect(natives->forget_workspace(probed) && keptListing().empty(), "forgotten, nothing lists it");
		auto later = natives->select("authority:agent-d:agent-tab", "claude", binding);
		expect(later && natives->shared_workspace(keptKey, *later).value_or(std::optional{ probed }) == std::nullopt, "and no turn reads it again");
		// Kept for a head its pull request's selected conversation has moved
		// past: ended, so expiry removes it first.
		const auto pullRequestKey = std::string{ "1:2:3:4:5:6:7:8:::17:host:cache:scope:base:head" };
		auto       leftBehind     = probed;
		leftBehind.Workspace.Path = "/synthetic-left-behind-workspace";
		expect(natives->keep_workspace(pullRequestKey, binding, leftBehind), "kept for the pull request's head");
		const auto endedFor = [&]() -> std::optional<bool> {
			const auto records = natives->retained_workspaces();
			if (!records) {
				return std::nullopt;
			}
			for (const auto& record : *records) {
				if (record.Workspace.Workspace.Path == leftBehind.Workspace.Path) {
					return record.Ended;
				}
			}
			return std::nullopt;
		};
		expect(endedFor() == std::optional{ false }, "while no conversation of its pull request is selected, it waits");
		expect(natives->select("1:2:3:4:5:6:7:8:agent-e:agent-tab", "claude", changedBinding).has_value() && endedFor() == std::optional{ true },
			   "a selected conversation at another head ends it");
		expect(natives->forget_workspace(leftBehind), "and it is forgotten");
	}

	expect(durableHistory && durableHistory->Selected == restored->Id && durableHistory->Turns.size() == 2U && durableHistory->Turns.back().Answered == "Late original answer", "restart preserves selected generation and originating late transcript");
	expect(durableHistory->Turns.back().Steered == steeredTurn.Steered && durableHistory->Turns.front().Steered.empty(), "restart keeps each turn's steered messages, in order, with their steps and states");
	// [[BPR-2241]]. How long each run took, where the host kept it.
	expect(durableHistory->Turns.back().Seconds == std::optional<std::uint32_t>{ 72U } && !durableHistory->Turns.front().Seconds, "restart keeps how long a turn's run took, and none where none was kept");
	expect(!natives->append(restored->Id, { .Asked = "Bad", .Answered = "Too short", .Ending = "Answered", .Steered = { { .Text = "Past the answer", .At = 400U } } }), "a steered message placed past its answer is refused");

	auto replayed = natives->replay(scope, "claude", changedBinding, restored->Id);
	expect(replayed && replayed->Replay && replayed->Id != restored->Id && replayed->ReplaySource == restored->Id && replayed->Handle.empty() && !replayed->Workspace && replayed->Binding == changedBinding, "explicit replay has separate generation, shown revision and no native tool/filesystem ownership");
	natives            = adapters::sqlite::make_native_agent_session_store(scratch.path());
	auto replayHistory = natives->history(scope, "claude");
	expect(replayHistory && replayHistory->Replay && replayHistory->Turns.size() == 2U && replayHistory->ReplaySource == restored->Id, "restart preserves replay mode, source link and retained prose");
	expect(replayHistory->Turns.back().Seconds == std::optional<std::uint32_t>{ 72U }, "a replay keeps how long each turn took");
	expect(!natives->replay("authority:agent-b:agent-tab", "claude", binding, restored->Id), "replay source cannot cross agent authority");
	expect(!natives->replay(scope, "claude", binding, "legacy:1"), "a numbered agent slot is no replay source");
	const auto fromReplay = natives->choose(scope, "claude", binding, "");
	expect(fromReplay && !fromReplay->Replay && fromReplay->Id != replayed->Id, "New session from replay starts empty native generation and preserves source");
	expect(natives->choose(scope, "claude", binding, restored->Id).has_value(), "restore original native provenance after replay");
	// The new session of an agent that cannot continue natively is an empty
	// replay. Asked for again where nothing was asked, it is the same one;
	// asked for after the head moved, it is a new one at the head under
	// review, and the one left behind keeps the revision it read.
	const auto emptyReplay = natives->replay(scope, "claude", binding, "");
	const auto sameReplay  = natives->replay(scope, "claude", binding, "");
	expect(emptyReplay && emptyReplay->Replay && emptyReplay->ReplaySource.empty() && emptyReplay->Binding == binding && sameReplay && sameReplay->Id == emptyReplay->Id,
		   "an empty replay nothing was asked in is taken up again at the revision it reads");
	const auto movedReplay = natives->replay(scope, "claude", changedBinding, "");
	expect(movedReplay && movedReplay->Replay && movedReplay->Id != emptyReplay->Id && movedReplay->Binding == changedBinding, "after the head moved, a new empty replay reads the head under review");
	const auto leftBehind = natives->choose(scope, "claude", changedBinding, emptyReplay->Id);
	expect(leftBehind && leftBehind->Binding == binding, "and the one left behind still reads its own revision");
	expect(natives->choose(scope, "claude", binding, restored->Id).has_value(), "the original native conversation is selected again");

	const auto removedScope = std::string{ "authority:agent-a:tour:71:0:3:9" };
	auto       removed      = natives->select(removedScope, "readonly", binding);
	expect(removed && natives->append(removed->Id, { "Removed stop question", "Retained answer", "Answered" }), "removed stop keeps a durable readable history");
	const auto resetScope      = std::string{ "authority:agent-a:tour:71:1:0:0" };
	auto       resetGeneration = natives->select(resetScope, "readonly", binding);
	const auto detached        = natives->history(resetScope, "readonly", removed->Id);
	expect(resetGeneration && detached && detached->ReadOnly && detached->Turns.size() == 1U && detached->Turns.front().Answered == "Retained answer", "detached history can be read across reset namespace without adopting it");
	const auto stillReset = natives->find(resetScope, "readonly", binding);
	expect(stillReset && *stillReset && (**stillReset).Id == resetGeneration->Id, "detached inspection preserves reset draft's current association");
	expect(!natives->history("authority:agent-b:tour:71:1:0:0", "readonly", removed->Id), "retained chat viewer cannot cross agent authority");
	auto refusedAt = std::size_t{ 0U };
	for (auto index = 0U; index <= application::maximum_conversations; ++index) {
		const auto admitted = natives->select("capacity-" + std::to_string(index), "claude", binding);
		if (!admitted) {
			expect(admitted.error() == application::native_agent_storage_failure::capacity, "full native catalog reports capacity");
			refusedAt = index;
			break;
		}
	}
	expect(refusedAt > 0U && refusedAt < application::maximum_conversations, "combined native/legacy catalog refuses at its shared bound");
	auto legacyNew   = pull;
	legacyNew.Number = 7000U;
	expect(!talks->store(legacyNew, thread), "legacy writer shares full native plus legacy capacity and cannot prune protected ownership");
	expect(talks->store(workCodex, thread) && talks->load(workCodex) == std::optional{ thread }, "existing legacy projection may be updated at full capacity without losing another history");
	expect(!natives->replay(scope, "claude", binding, restored->Id), "full replay admission refuses without changing native source");
	const auto protectedHistory = natives->find("authority:agent-b:agent-tab", "claude", binding);
	expect(protectedHistory && *protectedHistory && (**protectedHistory).Id == otherNative->Id, "capacity refusal never prunes protected native history");

	expect(!natives->choose(scope, "claude", binding, ""), "full admission refuses New session");
	const auto keptAfterRefusal = natives->history(scope, "claude");
	expect(keptAfterRefusal && keptAfterRefusal->Selected == restored->Id && keptAfterRefusal->Turns.size() == 2U, "failed New session retains prior selected generation and transcript");

	// RFC-BPR-0125 milestone 3 ([[BPR-2146]]) and RFC-BPR-0128 ([[BPR-2163]]).
	// Suggested prompts: nothing stored is the shipped set at revision 0; a
	// write names its revision, Quick ask and scopes are kept, an older
	// revision is refused, a newer build's row is kept and never rewritten.
	{
		auto       macros = adapters::sqlite::make_prompt_macro_store(scratch.path());
		const auto empty  = macros->load();
		expect(empty && empty->Revision == 0U && empty->Differences == core::prompt_macro_differences{ } && empty->Newer.empty(), "no prompts stored: the shipped set, revision 0");
		auto body           = core::prompt_macro_body{ .Name = "What could this change break?", .Template = "What could {pr.title} break?", .Subject = core::prompt_subject::pull_request };
		body.QuickAsk       = false;
		auto own            = core::prompt_macro{ .Id = "own.security", .Place = core::prompt_place::agent_tab, .Body = { .Name = "Security pass on {file.name}", .Template = "Check {file}", .Subject = core::prompt_subject::open_file } };
		own.Body.Conditions = { { .Fact = core::prompt_fact::path, .Patterns = { "src/**", "*.cpp" } }, { .Fact = core::prompt_fact::draft, .Negated = true } };
		own.Body.QuickAsk   = true;
		// [[BPR-2179]]. Any of these, and a condition with what it counts.
		own.Body.Match = core::prompt_match::any;
		// Look prompts-v003: the prompt's own Hide it or Show it disabled.
		own.Body.WhenUnmet = core::prompt_when_unmet::disable;
		own.Body.Conditions.push_back({ .Fact = core::prompt_fact::thread_activity, .Activity = core::prompt_activity_value::replies });
		// Look prompts-v001: nested groups, None among them.
		own.Body.Groups        = { { .Match = core::prompt_match::none, .Conditions = { { .Fact = core::prompt_fact::draft } }, .Groups = { { .Match = core::prompt_match::all, .Conditions = { { .Fact = core::prompt_fact::checks_failing }, { .Fact = core::prompt_fact::thread_unresolved } } } } } };
		own.Body.Scopes        = { .File = true, .PullRequest = true };
		own.On                 = false;
		const auto differences = core::prompt_macro_differences{
			.Changes = { { .Id = "agent-tab.what-could-break", .Body = body, .WrittenAgainst = 1U }, { .Id = "agent-tab.review-pull-request", .On = false } },
			.Own     = { own },
			.Orders  = { { .Place = core::prompt_place::agent_tab, .Ids = { "own.security", "agent-tab.review-pull-request" } } },
			// [[BPR-2179]]. Surfaces as changed: one emptied, one reordered.
			.Surfaces = { { .Surface = core::prompt_surface::pull_request_chips, .Ids = { }, .Removed = { "agent-tab.review-pull-request", "agent-tab.what-could-break" } },
						  { .Surface = core::prompt_surface::file_menu, .Ids = { "own.security", "agent-tab.explain-file" }, .Removed = { } } },
			// [[BPR-2179]]. A repository's layer: an override, one of its own,
			// a surface with an addition, a removal and its own order.
			.Repositories = { { .Repository = "octo/render",
								.Overrides  = { { .Id = "agent-tab.what-could-break", .Template = "What could break here?", .Conditions = std::vector<core::prompt_condition>{ }, .Groups = std::vector<core::prompt_condition_group>{ { .Match = core::prompt_match::any, .Conditions = { { .Fact = core::prompt_fact::draft } } } }, .WhenUnmet = core::prompt_when_unmet::hide, .On = false } },
								.Own        = { { .Id = "own.only-here", .Body = { .Name = "Only here", .Template = "Ask here" } } },
								.Surfaces   = { { .Surface = core::prompt_surface::thread_menu, .Added = { "own.only-here" }, .Removed = { "conversation.summarize" }, .Order = std::vector<std::string>{ "own.only-here" } } } } },
		};
		expect(macros->store(differences, 0U) == application::prompt_macro_write::stored, "a write from the revision read is stored");
		macros.reset();
		macros          = adapters::sqlite::make_prompt_macro_store(scratch.path());
		const auto read = macros->load();
		expect(read && read->Revision == 1U && read->Differences == differences, "a restart reads the same prompts back, Quick ask, scopes and surfaces included");
		expect(macros->store({ }, 0U) == application::prompt_macro_write::stale && macros->load() == read, "a write from an older revision changes nothing");
		auto withBadPattern                        = differences;
		withBadPattern.Own.front().Body.Conditions = { { .Fact = core::prompt_fact::path, .Patterns = { "a\nb" } } };
		expect(macros->store(withBadPattern, 1U) == application::prompt_macro_write::failed && macros->load() == read, "a pattern with a control character is not written");
		{
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
			expect(sqlite3_exec(database,
								"INSERT INTO prompt_macros(kind,id,position,place,has_body,name,template,subject,ask,conditions,quick,scopes,on_state,written_against) "
								"VALUES(1,'own.future',5,'future-place',1,'Future','Future','pull-request','question','',0,0,1,0)",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "a newer build's row is written");
			sqlite3_close_v2(database);
		}
		// A row an earlier build wrote, without the choice: migrated from its
		// conditions' lean (a path condition hides).
		{
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
			expect(sqlite3_exec(database,
								"INSERT INTO prompt_macros(kind,id,position,place,has_body,name,template,subject,ask,conditions,quick,scopes,on_state,written_against) "
								"VALUES(1,'own.earlier',6,'agent-tab',1,'Earlier','Earlier','pull-request','question','path' || char(31) || 'is' || char(31) || 'any' || char(31) || 'src/**',0,0,1,0)",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "an earlier build's row is written");
			const auto earlier  = macros->load();
			auto       migrated = false;
			if (earlier) {
				for (const auto& macro : earlier->Differences.Own) {
					migrated = migrated || (macro.Id == "own.earlier" && macro.Body.WhenUnmet == core::prompt_when_unmet::hide);
				}
			}
			expect(migrated, "an earlier row's path condition migrates to Hide it");
			expect(sqlite3_exec(database, "DELETE FROM prompt_macros WHERE id='own.earlier'", nullptr, nullptr, nullptr) == SQLITE_OK, "and is taken out again");
			sqlite3_close_v2(database);
		}
		const auto withNewer = macros->load();
		expect(withNewer && withNewer->Newer == std::vector<std::string>{ "own.future" } && withNewer->Differences == differences, "a newer build's row is kept apart, never offered");
		expect(macros->store({ }, 1U) == application::prompt_macro_write::stored, "restoring everything is a write");
		const auto restoredSet = macros->load();
		expect(restoredSet && restoredSet->Revision == 2U && restoredSet->Differences == core::prompt_macro_differences{ } && restoredSet->Newer == std::vector<std::string>{ "own.future" },
			   "a write never rewrites a newer build's row");
	}

	// [[BPR-2185]] (review N6). A conditions column an earlier build made at
	// 8192 bytes is widened with its rows; a nested set at core's limits,
	// longer than that, is stored and read back.
	{
		tests::sqlite_contract::scratch_root wide{ "prompt-conditions" };
		const auto                           widePath = wide.path() / "state/v1/agent-setup.sqlite3";
		auto                                 macros   = adapters::sqlite::make_prompt_macro_store(wide.path());
		expect(macros->load().has_value(), "a fresh store");
		macros.reset();
		{
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2(widePath.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
			for (const auto* const table : { "prompt_macros", "prompt_repository_prompts" }) {
				// The table as an earlier build made it: the same columns, the
				// conditions at 8192 bytes.
				auto sql = std::string{ };
				{
					sqlite3_stmt* row = nullptr;
					expect(sqlite3_prepare_v2(database, (std::string{ "SELECT sql FROM sqlite_schema WHERE name='" } + table + "'").c_str(), -1, &row, nullptr) == SQLITE_OK && sqlite3_step(row) == SQLITE_ROW,
						   "the table's definition is read");
					sql = reinterpret_cast<const char*>(sqlite3_column_text(row, 0));
					sqlite3_finalize(row);
				}
				const auto at = sql.find("conditions AS BLOB)) <= 16384)");
				expect(at != std::string::npos, "this build's column is wide");
				sql.replace(at, std::string_view{ "conditions AS BLOB)) <= 16384)" }.size(), "conditions AS BLOB)) <= 8192)");
				expect(sqlite3_exec(database, (std::string{ "DROP TABLE " } + table).c_str(), nullptr, nullptr, nullptr) == SQLITE_OK && sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
					   "an earlier build's narrow table");
			}
			expect(sqlite3_exec(database,
								"INSERT INTO prompt_macros(kind,id,position,place,has_body,name,template,subject,ask,conditions,quick,scopes,on_state,written_against) "
								"VALUES(1,'own.kept',0,'agent-tab',1,'Kept','Kept','pull-request','question','disable',0,0,1,0)",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "with a row in it");
			sqlite3_close_v2(database);
		}
		macros         = adapters::sqlite::make_prompt_macro_store(wide.path());
		const auto old = macros->load();
		expect(old && old->Differences.Own.size() == 1U && old->Differences.Own.front().Id == "own.kept", "the widened table keeps its rows");
		// Twelve conditions, each with eight long patterns, nested two levels.
		const auto leaf = [](const char aLetter) {
			auto condition = core::prompt_condition{ .Fact = core::prompt_fact::path };
			for (auto pattern = 0; pattern < static_cast<int>(core::maximum_prompt_patterns); ++pattern) {
				condition.Patterns.push_back(std::string(120U, aLetter) + "/**");
			}
			return condition;
		};
		auto deep            = core::prompt_macro{ .Id = "own.deep", .Place = core::prompt_place::agent_tab, .Body = { .Name = "Deep", .Template = "Deep" } };
		deep.Body.Conditions = { leaf('a'), leaf('b'), leaf('c'), leaf('d') };
		deep.Body.Groups     = { { .Match = core::prompt_match::any, .Conditions = { leaf('e'), leaf('f') }, .Groups = { { .Match = core::prompt_match::none, .Conditions = { leaf('g'), leaf('h') } } } },
								 { .Match = core::prompt_match::all, .Conditions = { leaf('i'), leaf('j'), leaf('k'), leaf('l') } } };
		expect(core::prompt_macro_valid(deep), "a tree within core's limits");
		auto differences = old->Differences;
		differences.Own.push_back(deep);
		auto layer = core::prompt_repository_layer{ .Repository = "octo/render" };
		layer.Overrides.push_back({ .Id = "agent-tab.what-could-break", .Conditions = deep.Body.Conditions, .Groups = deep.Body.Groups, .WhenUnmet = core::prompt_when_unmet::disable });
		differences.Repositories.push_back(layer);
		expect(macros->store(differences, old->Revision) == application::prompt_macro_write::stored, "is stored, longer than 8192 bytes");
		macros.reset();
		macros          = adapters::sqlite::make_prompt_macro_store(wide.path());
		const auto back = macros->load();
		expect(back && back->Differences == differences, "and read back, in a repository's layer too");
	}

	{
		sqlite3* database = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
		expect(sqlite3_exec(database, "UPDATE agent_setup SET chosen=7", nullptr, nullptr, nullptr) != SQLITE_OK, "an out-of-range agent is refused by the schema");
		expect(sqlite3_exec(database, "PRAGMA application_id=1112560199", nullptr, nullptr, nullptr) == SQLITE_OK, "another store's identity is written");
		sqlite3_close_v2(database);
	}
	expect(!store->load() && !store->store(chosen), "a file with another store's identity is refused, not read or overwritten");
	{
		// [[BPR-2181]]. Every queue pause is saved by its stable name and read
		// back as itself; `lost-access` is the newest.
		tests::sqlite_contract::scratch_root queues{ "agent-queue-pauses" };
		const auto                           pauses = std::vector{ core::queue_pause::none, core::queue_pause::reviewer, core::queue_pause::stopped, core::queue_pause::failed, core::queue_pause::usage_limit,
																   core::queue_pause::restarted, core::queue_pause::delivery_unknown, core::queue_pause::changed_target, core::queue_pause::lost_access };
		{
			auto queueStore = adapters::sqlite::make_agent_queue_store(queues.path());
			for (std::size_t index = 0U; index < pauses.size(); ++index) {
				// An empty queue is not kept, so each holds one message.
				auto entry       = core::queued_entry{ };
				entry.Id         = 1U;
				entry.Input.Text = "waiting";
				// AC06: a wider Was this addressed? carries its threads typed.
				entry.Input.Threads = { "thread-a", "thread-b" };
				// AC13: a suggested prompt keeps the offer decided at its press.
				if (index == 1U) {
					entry.Input.Prompt = core::prompt_offer{ .Id = "own.look-again", .Label = "Ready to look again?", .Expansion = { .Text = "Has it changed?", .Cursor = 3U, .Values = { { .Begin = 4U, .End = 6U } } } };
				}
				auto queue   = core::agent_queue{ };
				queue.LastId = 1U;
				queue.Paused = pauses[index];
				queue.Entries.push_back(entry);
				expect(queueStore->store("session-" + std::to_string(index), queue), "a queue with each pause is saved");
			}
		}
		const auto reloaded = adapters::sqlite::make_agent_queue_store(queues.path())->load();
		expect(reloaded.has_value() && reloaded->size() == pauses.size(), "every saved queue reloads");
		for (std::size_t index = 0U; reloaded && index < pauses.size(); ++index) {
			const auto found = reloaded->find("session-" + std::to_string(index));
			expect(found != reloaded->end() && found->second.Paused == pauses[index], "each pause reloads as itself, lost-access included");
			expect(found != reloaded->end() && found->second.Entries.size() == 1U && found->second.Entries.front().Input.Threads == std::vector<std::string>{ "thread-a", "thread-b" },
				   "a message's threads reload as they were queued (AC06)");
			expect(found != reloaded->end() && !found->second.Entries.empty() && found->second.Entries.front().Input.Prompt.has_value() == (index == 1U)
					   && (index != 1U || (found->second.Entries.front().Input.Prompt->Expansion.Text == "Has it changed?" && found->second.Entries.front().Input.Prompt->Expansion.Values.size() == 1U)),
				   "a decided prompt reloads as it was decided, and only where there was one (AC13)");
		}
	}
	{
		// [[BPR-2181]]. A file made when the entries cascaded from their queue
		// (which the hardened connection never let run, so no save ever
		// succeeded) holds only empty queue tables; it is made again and
		// saves.
		tests::sqlite_contract::scratch_root cascadeRoot{ "agent-queue-cascade" };
		expect(adapters::sqlite::make_agent_queue_store(cascadeRoot.path())->load().has_value(), "the store makes its file");
		{
			sqlite3* database = nullptr;
			expect(sqlite3_open_v2((cascadeRoot.path() / "state/v1/agent-setup.sqlite3").string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the file opens directly");
			expect(sqlite3_exec(database,
								"DROP TABLE agent_queue_entries; DROP TABLE agent_queues;"
								"CREATE TABLE agent_queues(session TEXT PRIMARY KEY CHECK(length(CAST(session AS BLOB)) BETWEEN 1 AND 1024), revision INTEGER NOT NULL CHECK(revision >= 0), last_id INTEGER NOT NULL CHECK(last_id >= 0), paused TEXT NOT NULL CHECK(length(CAST(paused AS BLOB)) <= 64)) STRICT;"
								"CREATE TABLE agent_queue_entries(session TEXT NOT NULL REFERENCES agent_queues(session) ON DELETE CASCADE, position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 15), id INTEGER NOT NULL CHECK(id >= 1), quick_ask TEXT NOT NULL, scope TEXT NOT NULL, file TEXT NOT NULL, thread TEXT NOT NULL, label TEXT NOT NULL, text TEXT NOT NULL CHECK(length(CAST(text AS BLOB)) <= 16384), state TEXT NOT NULL, held INTEGER NOT NULL CHECK(held BETWEEN 0 AND 1), PRIMARY KEY(session, position)) STRICT;",
								nullptr, nullptr, nullptr)
					   == SQLITE_OK,
				   "the old cascading tables are put back");
			sqlite3_close_v2(database);
		}
		auto entry       = core::queued_entry{ };
		entry.Id         = 1U;
		entry.Input.Text = "waiting";
		auto queue       = core::agent_queue{ };
		queue.LastId     = 1U;
		queue.Paused     = core::queue_pause::lost_access;
		queue.Entries.push_back(entry);
		auto legacyStore = adapters::sqlite::make_agent_queue_store(cascadeRoot.path());
		expect(legacyStore->store("session", queue), "a queue saves into a file the cascade made");
		expect(legacyStore->store("session", queue), "and saves again over itself");
		const auto again = legacyStore->load();
		expect(again && again->size() == 1U && again->begin()->second.Entries.size() == 1U && again->begin()->second.Paused == core::queue_pause::lost_access, "and reads back");
	}
	earlier_builds_open();
	std::cout << "agent-setup-storage:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return EXIT_FAILURE;
}
