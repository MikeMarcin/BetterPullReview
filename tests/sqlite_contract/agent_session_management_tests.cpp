// [[BPR-NNNN]]. Renaming and deleting an agent session, and clearing them
// all, through the application's effects over the real SQLite catalog. The
// workspace files are a fake that records what it was asked to remove: no
// folder is made, and no agent's own session file exists to be touched.
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/application/agent_session_management.hpp"
#include "scratch_root.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
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
		// An empty catalog clears without the files port.
		expect(application::clear_agent_sessions(*store, nullptr, { }).Cleared, "nothing to remove clears at once");
	}
} // namespace

int main()
try {
	rename();
	delete_alone_and_shared();
	clear_all();
	std::cout << "agent-session-management:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
