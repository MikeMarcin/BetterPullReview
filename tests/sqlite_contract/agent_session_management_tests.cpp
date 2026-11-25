// [[BPR-2377]]. Renaming and deleting an agent session, and clearing them
// all, through the application's effects over the real SQLite catalog. The
// workspace files are a fake that records what it was asked to remove: no
// folder is made, and no agent's own session file exists to be touched.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/application/agent_session_management.hpp"
#include "bpr/core/agent_queue.hpp"
#include "scratch_root.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;
	using result = application::agent_session_edit_result;

	/// Records each removal; answers `Answer` for every one.
	class recording_files final : public application::retained_workspace_files_port
	{
	public:
		auto measure(const application::owned_agent_workspace&, std::stop_token) -> application::retained_workspace_measure override
		{
			return { .Present = true, .Bytes = 1000U, .NewestSeconds = 1 };
		}
		auto remove(const application::owned_agent_workspace& aWorkspace, std::stop_token) -> application::retained_workspace_removal override
		{
			Removed.push_back(aWorkspace.Workspace.Path);
			return Answer;
		}
		std::vector<std::string>                Removed;
		application::retained_workspace_removal Answer = application::retained_workspace_removal::removed;
	};

	auto binding() -> core::agent_revision_binding
	{
		auto value        = core::agent_revision_binding{ };
		value.Host        = core::git_host_id{ 1 };
		value.Repository  = core::repository_id{ 2 };
		value.PullRequest = core::pull_request_id{ 3 };
		value.BaseOid     = *core::parse_git_commit_oid(std::string(40U, '1'));
		value.HeadOid     = *core::parse_git_commit_oid(std::string(40U, '2'));
		value.LeftEpoch   = core::epoch_ordinal{ 1 };
		value.RightEpoch  = core::epoch_ordinal{ 2 };
		return value;
	}

	auto workspace(const std::string& aPath) -> application::owned_agent_workspace
	{
		auto value                 = application::owned_agent_workspace{ };
		value.Workspace.Path       = aPath;
		value.Workspace.Shallow    = true;
		value.Workspace.PinnedHead = binding().HeadOid;
		return value;
	}

	constexpr const char* scope = "authority:agent-a:agent-tab";
	constexpr const char* key   = "exact-authority:base:head";

	/// A session in `scope` with one question, holding `aPath` (none when empty).
	auto session(application::native_agent_session_store_port& aStore, const std::string& aQuestion, const std::string& aPath, const std::string& aKey = key) -> std::string
	{
		auto made = aStore.choose(scope, "claude", binding(), "");
		expect(made.has_value(), "a session is admitted");
		expect(aStore.append(made->Id, { aQuestion, "An answer", "Answered" }), "its first turn is kept");
		if (!aPath.empty()) {
			// Read again: keeping its turn moved its revision.
			made = aStore.choose(scope, "claude", binding(), made->Id);
			expect(made.has_value(), "the session reads back");
			made->Workspace    = workspace(aPath);
			made->WorkspaceKey = aKey;
			expect(aStore.store(*made), "its workspace is kept");
		}
		return made->Id;
	}

	auto entry(application::native_agent_session_store_port& aStore, const std::string& aId) -> std::optional<application::native_agent_history_entry>
	{
		const auto history = aStore.history(scope, "claude");
		expect(history.has_value(), "history reads");
		const auto found = std::ranges::find(history->Entries, aId, &application::native_agent_history_entry::Id);
		return found == history->Entries.end() ? std::nullopt : std::optional{ *found };
	}

	void rename()
	{
		tests::sqlite_contract::scratch_root scratch{ "agent-session-rename" };
		auto                                 store = adapters::sqlite::make_native_agent_session_store(scratch.path());
		const auto                           id    = session(*store, "What changed in the parser?", "");
		expect(entry(*store, id)->Title == "What changed in the parser?" && entry(*store, id)->Name.empty(), "without a name, its first question is its title");
		expect(application::rename_agent_session(*store, scope, id, "  Parser\nreview  ").Result == result::done, "a name is stored");
		store = adapters::sqlite::make_native_agent_session_store(scratch.path());
		expect(entry(*store, id)->Title == "Parser review" && entry(*store, id)->Name == "Parser review", "tidied, and kept across a restart");
		expect(application::rename_agent_session(*store, scope, id, std::string(121U, 'x')).Result == result::name_too_long && entry(*store, id)->Name == "Parser review",
			   "a name too long changes nothing");
		expect(application::rename_agent_session(*store, "authority:agent-b:agent-tab", id, "Elsewhere").Result == result::not_found, "another agent's scope cannot rename it");
		expect(application::rename_agent_session(*store, scope, id, "   ").Result == result::done && entry(*store, id)->Title == "What changed in the parser?",
			   "an empty name restores the default title");
	}

	void delete_alone_and_shared()
	{
		tests::sqlite_contract::scratch_root scratch{ "agent-session-delete" };
		auto                                 store = adapters::sqlite::make_native_agent_session_store(scratch.path());
		auto                                 files = recording_files{ };
		// Two sessions of the pull request share one workspace; a third has its own.
		const auto first  = session(*store, "First", "/synthetic-shared-workspace");
		const auto second = session(*store, "Second", "/synthetic-shared-workspace");
		const auto own    = session(*store, "Own", "/synthetic-own-workspace", "own-authority:base:head");
		expect(application::rename_agent_session(*store, scope, first, "Named first").Result == result::done, "the first is named");

		auto deleted = application::delete_agent_session(*store, &files, scope, first, { key }, false, { });
		expect(deleted.Result == result::done && !deleted.WorkspaceRemoved && deleted.WorkspaceShared && files.Removed.empty(),
			   "deleting one of two sessions on a workspace keeps the workspace");
		expect(!entry(*store, first) && entry(*store, second), "only that session is gone");
		const auto shared = store->shared_workspace(key, *store->choose(scope, "claude", binding(), second));
		expect(shared && *shared && (**shared).Workspace.Path == "/synthetic-shared-workspace", "the pull request still leads to the shared workspace");

		deleted = application::delete_agent_session(*store, &files, scope, second, { key }, false, { });
		expect(deleted.Result == result::done && deleted.WorkspaceRemoved && files.Removed == std::vector<std::string>{ "/synthetic-shared-workspace" },
			   "deleting the last session on it removes the workspace");
		const auto records = store->retained_workspaces();
		expect(records && records->size() == 1U && records->front().Workspace.Workspace.Path == "/synthetic-own-workspace", "only the other workspace is still recorded");

		// A turn holding a lease of the workspace: nothing is deleted.
		{
			const auto turn = store->try_lock_workspace(application::agent_workspace_path_lease("/synthetic-own-workspace"));
			expect(turn != nullptr, "a turn holds the workspace");
			deleted = application::delete_agent_session(*store, &files, scope, own, { }, false, { });
			expect(deleted.Result == result::running && entry(*store, own) && files.Removed.size() == 1U, "a running session is refused and kept");
		}
		expect(application::delete_agent_session(*store, &files, scope, own, { }, true, { }).Result == result::running && entry(*store, own),
			   "the asking window's own running turn refuses too");
		{
			const auto turn = store->try_lock_workspace(key);
			expect(application::delete_agent_session(*store, &files, scope, own, { key }, false, { }).Result == result::running, "the pull request's own turn lease refuses");
		}
		// Its workspace's removal refused: the session stays.
		files.Answer = application::retained_workspace_removal::refused;
		expect(application::delete_agent_session(*store, &files, scope, own, { key }, false, { }).Result == result::workspace_refused && entry(*store, own),
			   "an unverifiable workspace keeps its session");
		expect(application::delete_agent_session(*store, nullptr, scope, own, { key }, false, { }).Result == result::workspace_refused && entry(*store, own),
			   "without the files port the session is kept");
		files.Answer = application::retained_workspace_removal::gone;
		deleted      = application::delete_agent_session(*store, &files, scope, own, { key }, false, { });
		expect(deleted.Result == result::done && !deleted.WorkspaceRemoved && !entry(*store, own), "a workspace already gone only loses its record");
		expect(application::delete_agent_session(*store, &files, scope, own, { key }, false, { }).Result == result::not_found, "deleting it again finds nothing");
		const auto history = store->history(scope, "claude");
		expect(history && history->Entries.empty() && history->Turns.empty(), "no session and no transcript is left");
		const auto left = store->retained_workspaces();
		expect(left && left->empty(), "no workspace record is left");
	}

	void clear_all()
	{
		tests::sqlite_contract::scratch_root scratch{ "agent-session-clear" };
		auto                                 store = adapters::sqlite::make_native_agent_session_store(scratch.path());
		auto                                 files = recording_files{ };
		const auto                           a     = session(*store, "One", "/synthetic-a");
		const auto                           b     = session(*store, "Two", "/synthetic-b", "b-authority:base:head");
		static_cast<void>(b);
		// One workspace in use: no session is forgotten.
		{
			const auto turn   = store->try_lock_workspace(application::agent_workspace_path_lease("/synthetic-a"));
			const auto report = application::clear_agent_sessions(*store, &files, { });
			expect(!report.Cleared && report.Workspaces.Count == 1U && report.Workspaces.Removed == 1U && files.Removed == std::vector<std::string>{ "/synthetic-b" },
				   "a workspace in use stays, and so do the sessions that record it");
			expect(entry(*store, a).has_value(), "the sessions stay");
		}
		const auto report = application::clear_agent_sessions(*store, &files, { });
		expect(report.Cleared && report.Workspaces.Removed == 1U && report.Workspaces.Count == 0U, "then every workspace and every session goes");
		const auto history = store->history(scope, "claude");
		const auto records = store->retained_workspaces();
		expect(history && history->Entries.empty() && records && records->empty(), "nothing is left in the catalog");
		// [[BPR-2377]]. The chats' queued follow-ups go with the sessions.
		{
			auto entry          = core::queued_entry{ };
			entry.Id            = 1U;
			entry.Input.Text    = "waiting";
			entry.Input.Threads = { "thread-a" };
			auto queue          = core::agent_queue{ };
			queue.LastId        = 1U;
			queue.Entries.push_back(entry);
			auto queues = adapters::sqlite::make_agent_queue_store(scratch.path());
			expect(queues->store("session", queue) && queues->load()->size() == 1U, "a follow-up is queued");
			expect(adapters::sqlite::clear_agent_queues(scratch.path()) && queues->load() && queues->load()->empty(), "clearing the queues leaves none");
		}
		// An empty catalog clears without the files port.
		expect(application::clear_agent_sessions(*store, nullptr, { }).Cleared, "nothing to remove clears at once");
	}
	/// [[BPR-2377]]. Delete all sessions…: each as one deletion; a running one
	/// is kept and counted.
	void delete_all()
	{
		tests::sqlite_contract::scratch_root scratch{ "agent-session-delete-all" };
		auto                                 store = adapters::sqlite::make_native_agent_session_store(scratch.path());
		auto                                 files = recording_files{ };
		const auto                           a     = session(*store, "One", "/synthetic-all-shared");
		const auto                           b     = session(*store, "Two", "/synthetic-all-shared");
		const auto                           c     = session(*store, "Three", "/synthetic-all-busy", "busy-authority:base:head");
		{
			const auto turn    = store->try_lock_workspace(application::agent_workspace_path_lease("/synthetic-all-busy"));
			const auto receipt = application::delete_agent_sessions(*store, &files, scope, { c, b, a }, { }, false, { });
			expect(receipt.Result == result::done && receipt.Deleted == 2U && receipt.Kept == 1U && receipt.WorkspaceRemoved, "two go, the running one is kept and counted");
			expect(files.Removed == std::vector<std::string>{ "/synthetic-all-shared" }, "their shared workspace goes once, with the last of them");
			expect(entry(*store, c) && !entry(*store, a) && !entry(*store, b), "only the running session is left");
		}
		const auto refused = application::delete_agent_sessions(*store, &files, scope, { c }, { }, true, { });
		expect(refused.Result == result::running && refused.Deleted == 0U && refused.Kept == 1U, "with the window's turn running nothing goes");
	}

	/// Runs `aSql` on `aFile` through a plain connection, as an earlier build
	/// left it.
	void run_sql(const std::filesystem::path& aFile, const std::string& aSql)
	{
		sqlite3* raw = nullptr;
		expect(sqlite3_open_v2(aFile.string().c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK, "the file opens directly");
		const auto ran = sqlite3_exec(raw, aSql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
		sqlite3_close(raw);
		expect(ran, "the earlier build's statements run");
	}

	auto table_count(const std::filesystem::path& aFile, const std::string& aTable) -> int
	{
		sqlite3* raw = nullptr;
		expect(sqlite3_open_v2(aFile.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "the file opens to read");
		sqlite3_stmt* row = nullptr;
		const auto    sql = "SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='" + aTable + "'";
		sqlite3_prepare_v2(raw, sql.c_str(), -1, &row, nullptr);
		const auto found = sqlite3_step(row) == SQLITE_ROW ? sqlite3_column_int(row, 0) : -1;
		sqlite3_finalize(row);
		sqlite3_close(raw);
		return found;
	}

	/// An earlier build's conversation tables in agent-setup: one session with
	/// one turn in the format before turns kept their seconds, selected.
	auto earlier_conversation_sql() -> std::string
	{
		const auto id = std::string(32U, 'e');
		return "CREATE TABLE native_agent_sessions(id TEXT PRIMARY KEY, scope TEXT NOT NULL, configuration TEXT NOT NULL, base TEXT NOT NULL, head TEXT NOT NULL,"
			   "left_epoch INTEGER NOT NULL, right_epoch INTEGER NOT NULL, handle TEXT NOT NULL, turn INTEGER NOT NULL, pending INTEGER NOT NULL,"
			   "workspace TEXT NOT NULL, parent TEXT NOT NULL, marker TEXT NOT NULL, application INTEGER NOT NULL, session INTEGER NOT NULL,"
			   "directory BLOB NOT NULL, guard TEXT NOT NULL, guard_identity BLOB NOT NULL, shallow INTEGER NOT NULL, revision INTEGER NOT NULL) STRICT;"
			   "CREATE TABLE native_agent_turns(conversation TEXT NOT NULL REFERENCES native_agent_sessions(id), ordinal INTEGER NOT NULL, asked TEXT NOT NULL,"
			   " answered TEXT NOT NULL, ending TEXT NOT NULL, outcome INTEGER NOT NULL, PRIMARY KEY(conversation,ordinal)) STRICT;"
			   "CREATE TABLE native_agent_selection(scope TEXT PRIMARY KEY, conversation TEXT NOT NULL REFERENCES native_agent_sessions(id)) STRICT;"
			   "INSERT INTO native_agent_sessions VALUES('"
			+ id + "','" + scope + "','claude','" + std::string(40U, '1') + "','" + std::string(40U, '2')
			+ "',1,2,'',2,0,'','','',0,0,zeroblob(32),'',zeroblob(32),0,1);"
			  "INSERT INTO native_agent_turns VALUES('"
			+ id + "',1,'A question an earlier build kept','Its answer','Answered',0);"
				   "INSERT INTO native_agent_selection VALUES('"
			+ scope + "','" + id + "');";
	}

	/// [[BPR-2377]] (owner, 2026-11-24: "split sessions from settings").
	void move_out_of_agent_setup()
	{
		tests::sqlite_contract::scratch_root scratch{ "agent-session-move" };
		const auto                           setup    = adapters::sqlite::agent_setup_store_file(scratch.path());
		const auto                           sessions = adapters::sqlite::agent_sessions_store_file(scratch.path());
		// An earlier build's profile: agents and their conversations in one file.
		auto preferences            = application::agent_setup_preferences{ };
		preferences.AgentsMoved     = true;
		preferences.RunLimitMinutes = 25U;
		expect(adapters::sqlite::make_agent_setup_store(scratch.path())->store(preferences), "the agent setup is kept");
		run_sql(setup, earlier_conversation_sql());
		expect(!std::filesystem::exists(sessions), "no conversations store yet");

		// The first open moves them out, column by column, and drops them there.
		auto       store   = adapters::sqlite::make_native_agent_session_store(scratch.path());
		const auto history = store->history(scope, "claude");
		expect(history && history->Entries.size() == 1U && history->Entries.front().Title == "A question an earlier build kept" && history->Selected == std::string(32U, 'e')
				   && history->Turns.size() == 1U && !history->Turns.front().Seconds,
			   "the conversation, its turn and its selection moved, a column the old table lacked is empty");
		expect(table_count(setup, "native_agent_sessions") == 0 && table_count(setup, "native_agent_turns") == 0 && table_count(setup, "native_agent_selection") == 0,
			   "the agent setup store no longer holds them");
		expect(adapters::sqlite::make_agent_setup_store(scratch.path())->load()->RunLimitMinutes == 25U, "the agent setup is untouched");
		expect(adapters::sqlite::prepare_agent_sessions_store(scratch.path()) && store->history(scope, "claude")->Entries.size() == 1U, "opening again moves nothing twice");

		// A stop after the copy and before the drop: the old tables are back and
		// the move is at stage 1. The next open drops them, and copies nothing.
		run_sql(setup, earlier_conversation_sql());
		run_sql(sessions, "UPDATE session_store_migration SET stage=1");
		expect(adapters::sqlite::prepare_agent_sessions_store(scratch.path()), "a stopped move finishes");
		expect(table_count(setup, "native_agent_sessions") == 0 && store->history(scope, "claude")->Entries.size() == 1U, "the old tables went; the conversation is there once");

		// A stop before the copy committed left nothing durable in the new
		// store; a retry finds the old rows and copies them exactly once.
		std::filesystem::remove(sessions);
		run_sql(setup, earlier_conversation_sql());
		expect(adapters::sqlite::prepare_agent_sessions_store(scratch.path()) && store->history(scope, "claude")->Entries.size() == 1U, "a retried move copies once");

		// Settings now removes only the agent setup; the conversations stay.
		std::filesystem::remove(setup);
		expect(store->history(scope, "claude")->Entries.size() == 1U && adapters::sqlite::prepare_agent_sessions_store(scratch.path()), "without the agent setup store the conversations stay");
	}

	/// RFC-BPR-0096, owner continue-at-latest amendment, 2026-11-24. A
	/// conversation whose workspace moved records its new binding, marker and
	/// key in one revision. Owner: "a session in the same PR? yes single
	/// checkout": every other conversation on the checkout is rebound in the
	/// same revision and keeps the head it was at until its next turn is
	/// told. A tree the pull request's kept workspace records is refused.
	void move_a_conversation()
	{
		tests::sqlite_contract::scratch_root scratch{ "agent-session-move" };
		auto                                 store  = adapters::sqlite::make_native_agent_session_store(scratch.path());
		const auto                           own    = session(*store, "Own", "/synthetic-own-workspace", "own-authority:base:head");
		const auto                           shared = session(*store, "Shared", "/synthetic-shared-workspace");
		const auto                           also   = session(*store, "Also shared", "/synthetic-shared-workspace");

		auto       moved                      = *store->choose(scope, "claude", binding(), own);
		const auto stale                      = moved;
		moved.Binding.HeadOid                 = *core::parse_git_commit_oid(std::string(40U, '3'));
		moved.Binding.RightEpoch              = core::epoch_ordinal{ 3 };
		moved.Workspace->Workspace.PinnedHead = moved.Binding.HeadOid;
		moved.Workspace->OwnershipMarker      = "moved-marker";
		moved.WorkspaceKey                    = "own-authority:base:head3";
		expect(store->move_conversation(moved), "the moved conversation is recorded");
		const auto read = store->removal(scope, own);
		expect(read && read->Workspace && read->Workspace->Workspace.PinnedHead == std::optional{ moved.Binding.HeadOid } && read->Workspace->OwnershipMarker == "moved-marker",
			   "it reads back at the new head with the new marker");
		auto asker        = application::native_agent_session{ };
		asker.Binding     = moved.Binding;
		const auto newKey = store->shared_workspace("own-authority:base:head3", asker);
		const auto oldKey = store->shared_workspace("own-authority:base:head", application::native_agent_session{ });
		expect(newKey && *newKey && (**newKey).Workspace.Path == "/synthetic-own-workspace" && oldKey && !*oldKey, "the new key leads to it and the old one does not");
		auto again                            = stale;
		again.Binding.HeadOid                 = moved.Binding.HeadOid;
		again.Workspace->Workspace.PinnedHead = moved.Binding.HeadOid;
		expect(!store->move_conversation(again), "a stale revision is refused");

		auto holder                            = *store->choose(scope, "claude", binding(), shared);
		holder.Binding                         = moved.Binding;
		holder.Workspace->Workspace.PinnedHead = moved.Binding.HeadOid;
		holder.Workspace->OwnershipMarker      = "shared-moved-marker";
		holder.WorkspaceKey                    = "exact-authority:base:head3";
		expect(store->move_conversation(holder), "a checkout other conversations of the pull request share moves");
		expect(entry(*store, also)->Binding.HeadOid == moved.Binding.HeadOid && entry(*store, also)->Binding.RightEpoch == moved.Binding.RightEpoch, "the other conversation on it is rebound to the new head");
		const auto other = store->removal(scope, also);
		expect(other && other->Workspace && other->Workspace->OwnershipMarker == "shared-moved-marker" && other->OtherHolders == 1U, "with the moved marker, still sharing the one checkout");
		const auto told = *store->choose(scope, "claude", binding(), also);
		expect(told.MovedFrom == std::optional{ binding().HeadOid } && !store->choose(scope, "claude", binding(), shared)->MovedFrom,
			   "it keeps the head it was at; the conversation that moved keeps none");
		static_cast<void>(store->choose(scope, "claude", binding(), also));
		const auto opened = store->history(scope, "claude");
		expect(opened && opened->SelectedMove == std::optional{ std::pair{ binding().HeadOid, moved.Binding.HeadOid } }, "its transcript, opened, says the move");
		expect(store->acknowledge_move(also) && !store->choose(scope, "claude", binding(), also)->MovedFrom && !store->history(scope, "claude")->SelectedMove,
			   "once its turn told it, it is forgotten");

		// The pull request's kept workspace is no conversation's.
		auto kept                 = workspace("/synthetic-kept-workspace");
		kept.Workspace.PinnedHead = binding().HeadOid;
		expect(store->keep_workspace("kept-authority:base:head", binding(), kept), "a workspace is kept for the pull request");
		const auto beside                          = session(*store, "Beside the kept one", "/synthetic-kept-workspace", "beside-authority:base:head");
		auto       keptHolder                      = *store->choose(scope, "claude", binding(), beside);
		keptHolder.Binding                         = moved.Binding;
		keptHolder.Workspace->Workspace.PinnedHead = moved.Binding.HeadOid;
		expect(!store->move_conversation(keptHolder), "a checkout the kept workspace records is not moved");

		// A partial-history checkout moves with the new head's holes, which
		// every conversation on it reads back.
		const auto partial                     = session(*store, "Partial", "/synthetic-partial-workspace", "partial-authority:base:head");
		const auto partialToo                  = session(*store, "Partial too", "/synthetic-partial-workspace", "partial-too-authority:base:head");
		auto       sparse                      = *store->choose(scope, "claude", binding(), partial);
		sparse.Binding                         = moved.Binding;
		sparse.Workspace->Workspace.PinnedHead = moved.Binding.HeadOid;
		sparse.Workspace->SparseMissingObjects = "?" + std::string(40U, '4') + "\n";
		sparse.WorkspaceKey                    = "partial-authority:base:head3";
		expect(store->move_conversation(sparse), "a partial-history checkout moves");
		for (const auto& id : { partial, partialToo }) {
			const auto holes = store->removal(scope, id);
			expect(holes && holes->Workspace && holes->Workspace->SparseMissingObjects == sparse.Workspace->SparseMissingObjects,
				   "each conversation on it keeps the new head's holes");
		}
	}
} // namespace

int main()
try {
	rename();
	delete_alone_and_shared();
	clear_all();
	delete_all();
	move_out_of_agent_setup();
	move_a_conversation();
	std::cout << "agent-session-management:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
