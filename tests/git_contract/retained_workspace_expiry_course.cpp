/// \file
/// \brief [[BPR-2293]] (AC1, AC2, AC3). Retained agent workspaces expire over
/// the real catalog and a fixture Git: a sweep removes those unused past the
/// days, those of an ended conversation and the oldest over the cap, and never
/// one a turn holds; Remove removes every one not in use; a removed
/// workspace's conversation continues with a fresh one.

#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/git/installed_git_process.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_workspace_expiry.hpp"
#include "windows_system_root.hpp"

#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;

	class temporary_root final
	{
	public:
		temporary_root()
			: Path{ tests::make_scratch_directory("bpr-retained-workspace-expiry") }
		{
			std::filesystem::permissions(Path, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
			Path = std::filesystem::canonical(Path);
		}
		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;
		~temporary_root()
		{
			auto error = std::error_code{ };
			std::filesystem::remove_all(Path, error);
		}
		std::filesystem::path Path;
	};

	[[nodiscard]] auto private_directory(const std::filesystem::path& aPath) -> std::filesystem::path
	{
		std::filesystem::create_directories(aPath);
		std::filesystem::permissions(aPath, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
		return std::filesystem::canonical(aPath);
	}

	[[nodiscard]] auto git(const std::filesystem::path& aRepository, const QStringList& aArguments, const std::string& aInput = { }) -> std::string
	{
		auto process = QProcess{ };
		process.setProgram(QString::fromStdString(BPR_TEST_GIT_EXECUTABLE));
		auto arguments = QStringList{ QString::fromStdString("--git-dir=" + aRepository.string()) };
		arguments.append(aArguments);
		process.setArguments(arguments);
		auto environment = QProcessEnvironment::systemEnvironment();
		environment.insert("GIT_CONFIG_NOSYSTEM", "1");
		environment.insert("GIT_CONFIG_GLOBAL", QString::fromStdString((aRepository.parent_path() / "fixture-global-config").string()));
		environment.insert("GIT_AUTHOR_NAME", "BetterPullReview Fixture");
		environment.insert("GIT_AUTHOR_EMAIL", "fixture@betterpullreview.invalid");
		environment.insert("GIT_AUTHOR_DATE", "2000-01-01T00:00:00+0000");
		environment.insert("GIT_COMMITTER_NAME", "BetterPullReview Fixture");
		environment.insert("GIT_COMMITTER_EMAIL", "fixture@betterpullreview.invalid");
		environment.insert("GIT_COMMITTER_DATE", "2000-01-01T00:00:00+0000");
		process.setProcessEnvironment(environment);
		process.start();
		expect(process.waitForStarted(5000), "the fixture Git starts");
		process.write(aInput.data(), static_cast<qint64>(aInput.size()));
		process.closeWriteChannel();
		expect(process.waitForFinished(30000) && process.exitCode() == 0, "the fixture Git succeeds: " + process.readAllStandardError().toStdString());
		return process.readAllStandardOutput().trimmed().toStdString();
	}

	[[nodiscard]] auto oid(const std::string& aHex) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(aHex);
		expect(parsed.has_value(), "a fixture commit parses");
		return *parsed;
	}

	template <typename TIdentity>
	[[nodiscard]] auto identity(const std::uint8_t aValue) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aValue);
		return TIdentity{ bytes };
	}

	[[nodiscard]] auto authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 1U },
			.ProfileRevision = core::github_profile_revision{ 1U },
			.Host            = core::git_host_id{ 1U },
			.HostAuthority   = identity<core::host_authority_fingerprint>(1U),
			.Account         = core::github_account_id{ 1U },
			.Viewer          = core::github_viewer_actor_id{ 1U },
			.Repository      = core::repository_id{ 1U },
			.Cache           = identity<core::repository_cache_scope_id>(2U),
			.PullRequest     = core::pull_request_id{ 1U },
			.Number          = core::pull_request_number{ 1U },
			.Scope           = identity<core::pull_request_authority_scope_id>(3U),
		};
	}

	[[nodiscard]] auto file_time(const std::chrono::system_clock::time_point aWhen) -> std::filesystem::file_time_type
	{
		return std::filesystem::file_time_type::clock::now() + std::chrono::duration_cast<std::filesystem::file_time_type::duration>(aWhen - std::chrono::system_clock::now());
	}

	/// One profile: its catalog, its review cache, and the workspaces its
	/// agent's conversations keep.
	struct profile
	{
		std::filesystem::path                                         Data;
		std::filesystem::path                                         Neighbourhood;
		std::filesystem::path                                         Registry;
		std::filesystem::path                                         CacheRoot;
		std::filesystem::path                                         Cache;
		std::string                                                   Base;
		std::map<int, std::string>                                    Heads;
		std::unique_ptr<application::native_agent_session_store_port> Store;
		std::uint64_t                                                 Session = 10U;

		[[nodiscard]] auto binding(const int aHead) const -> core::agent_revision_binding
		{
			const auto pullRequest = authority();
			auto       value       = core::agent_revision_binding{ };
			value.Host             = pullRequest.Host;
			value.Repository       = pullRequest.Repository;
			value.PullRequest      = pullRequest.PullRequest;
			value.BaseOid          = oid(Base);
			value.HeadOid          = oid(Heads.at(aHead));
			value.LeftEpoch        = core::epoch_ordinal{ 1U };
			value.RightEpoch       = core::epoch_ordinal{ 1U };
			return value;
		}
		[[nodiscard]] static auto scope(const int aThread) -> std::string
		{
			return application::native_scope_key(authority(), "fixture-agent", "thread-" + std::to_string(aThread));
		}
		[[nodiscard]] auto key(const int aHead) const -> std::string { return application::agent_workspace_key(authority(), binding(aHead)); }

		/// A turn of conversation \p aThread at head \p aHead: a retained
		/// workspace made through \p aGit and stored with the conversation.
		auto converse(application::git_process_port& aGit, const int aThread, const int aHead) -> application::owned_agent_workspace
		{
			auto selected = Store->select(scope(aThread), "fixture", binding(aHead), true);
			expect(selected.has_value(), "a conversation is selected");
			auto request                               = application::git_agent_workspace_request{ };
			request.Repository                         = Cache;
			request.Materialization.ApplicationSession = core::application_session_id{ 1U };
			request.Materialization.Authority          = authority();
			request.Materialization.Session.Id         = core::agent_session_id{ Session++ };
			request.Materialization.Session.Profile    = core::external_terminal_profile_id;
			request.Materialization.Session.Binding    = binding(aHead);
			request.Materialization.ParentDirectory    = Neighbourhood;
			request.Materialization.RetainForSession   = true;
			const auto made                            = aGit.materialize_agent_workspace(request, { });
			expect(made.has_value(), "a retained workspace is made");
			selected->Workspace    = *made;
			selected->WorkspaceKey = key(aHead);
			expect(Store->store(*selected), "the conversation keeps its workspace");
			return *made;
		}
	};

	[[nodiscard]] auto make_git(const profile& aProfile, std::shared_ptr<adapters::git::installed_workspace_owners> aOwners) -> std::unique_ptr<application::git_process_port>
	{
		auto made = adapters::git::make_installed_git_process({
			.GitExecutable              = std::filesystem::canonical(BPR_TEST_GIT_EXECUTABLE),
			.IsolationRoot              = aProfile.CacheRoot,
			.WindowsSystemRoot          = tests::git_contract::windows_system_root(),
			.WorkspaceOwnershipRegistry = aProfile.Registry,
			.WorkspaceOwners            = std::move(aOwners),
		});
		expect(made.has_value(), "the installed Git adapter is made");
		return std::move(*made);
	}

	[[nodiscard]] auto make_profile(const std::filesystem::path& aRoot) -> profile
	{
		auto value          = profile{ };
		value.Data          = private_directory(aRoot / "data");
		value.Neighbourhood = private_directory(aRoot / "data-agent-workspaces-v1");
		value.Registry      = value.Data / "agent-workspace-ownership-v1";
		value.CacheRoot     = private_directory(aRoot / "cache");
		value.Cache         = value.CacheRoot / "repositories" / "v1" / (std::string(64U, '1') + ".git");
		value.Store         = adapters::sqlite::make_native_agent_session_store(value.Data);
		{
			auto adapter = make_git(value, nullptr);
			expect(adapter->initialize_bare_repository({ .Repository = value.Cache, .Algorithm = application::git_hash_algorithm::sha1 }, { }).has_value(), "the review cache is made");
		}
		const auto commit = [&](const std::string& aText, const std::optional<std::string>& aParent) {
			const auto blob      = git(value.Cache, { "hash-object", "-w", "--stdin" }, aText + "\n");
			const auto tree      = git(value.Cache, { "mktree" }, "100644 blob " + blob + "\tREADME.md\n");
			auto       arguments = QStringList{ "commit-tree", QString::fromStdString(tree) };
			if (aParent) {
				arguments << "-p" << QString::fromStdString(*aParent);
			}
			return git(value.Cache, arguments, aText + "\n");
		};
		value.Base = commit("base", std::nullopt);
		for (int head = 0; head < 9; ++head) {
			value.Heads[head] = commit("head " + std::to_string(head), value.Base);
		}
		return value;
	}

	[[nodiscard]] auto present(const application::owned_agent_workspace& aWorkspace) -> bool
	{
		return std::filesystem::exists(aWorkspace.Workspace.Path);
	}

	/// Writes \p aBytes into a sparse file in the workspace, as an agent's
	/// build output would be, and dates everything in it at \p aWhen.
	void age(const application::owned_agent_workspace& aWorkspace, const std::chrono::system_clock::time_point aWhen, const std::uintmax_t aBytes = 0U)
	{
		const auto path = std::filesystem::path{ aWorkspace.Workspace.Path };
		if (aBytes != 0U) {
			{
				auto file = std::ofstream{ path / "build-output.bin", std::ios::binary };
			}
			std::filesystem::resize_file(path / "build-output.bin", aBytes);
		}
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ path }) {
			if (!entry.is_symlink()) {
				std::filesystem::last_write_time(entry.path(), file_time(aWhen));
			}
		}
		std::filesystem::last_write_time(path, file_time(aWhen));
	}

	void run_course(const std::filesystem::path& aRoot)
	{
		auto       owners   = adapters::git::make_installed_workspace_owners();
		auto       made     = make_profile(aRoot);
		const auto realNow  = std::chrono::system_clock::now();
		const auto sweepAt  = realNow + std::chrono::hours{ 24 * 8 + 12 };
		const auto seconds  = std::chrono::duration_cast<std::chrono::seconds>(sweepAt.time_since_epoch()).count();
		const auto recently = [&](const int aHoursBefore) { return sweepAt - std::chrono::hours{ aHoursBefore }; };

		// Workspaces a previous run made: their owner is gone with that run.
		auto old      = application::owned_agent_workspace{ };
		auto ended    = application::owned_agent_workspace{ };
		auto busy     = application::owned_agent_workspace{ };
		auto bigOld   = application::owned_agent_workspace{ };
		auto bigNew   = application::owned_agent_workspace{ };
		auto tampered = application::owned_agent_workspace{ };
		{
			auto previous = make_git(made, nullptr);
			old           = made.converse(*previous, 0, 0);
			ended         = made.converse(*previous, 1, 1);
			busy          = made.converse(*previous, 2, 2);
			bigOld        = made.converse(*previous, 3, 3);
			bigNew        = made.converse(*previous, 4, 4);
			tampered      = made.converse(*previous, 5, 5);
		}
		// One this run made, whose owner is still alive.
		auto       current = make_git(made, owners);
		const auto live    = made.converse(*current, 6, 6);

		// `old`, `busy`, `tampered` and `live` were last used eight and a half
		// days before the sweep. The rest were used in the last day; two of
		// them hold 600 MB each, over the 1 GB cap together.
		age(ended, recently(3));
		age(bigOld, recently(10), 600'000'000U);
		age(bigNew, recently(2), 600'000'000U);
		// Thread 1 moved on to another revision: its old workspace has ended.
		expect(made.Store->select(profile::scope(1), "fixture", made.binding(7), true).has_value(), "thread 1 starts again at a later revision");
		// Its ownership marker no longer says it is the workspace made.
		{
			auto marker = std::ofstream{ std::filesystem::path{ tampered.Workspace.Path } / ".git" / "bpr-owned-workspace", std::ios::trunc };
			marker << "substituted\n";
		}

		auto files = adapters::git::make_installed_retained_workspace_files(made.Registry, owners);
		{
			const auto surveyed = application::sweep_retained_workspaces(*made.Store, *files, application::retained_workspace_sweep::survey, { }, seconds, { });
			expect(!surveyed.Unavailable && surveyed.Count == 7U && surveyed.Removed == 0U, "the survey lists the seven workspaces and removes none");
			expect(surveyed.Bytes >= 1'200'000'000U, "the survey counts the bytes in them");
		}

		auto policy  = core::agent_workspace_expiry_policy{ };
		policy.CapGb = 1U;
		// A turn holds `busy`'s workspace while the sweep runs.
		const auto held = made.Store->try_lock_workspace(made.key(2));
		expect(held != nullptr, "the turn holds its workspace's lease");
		const auto swept = application::sweep_retained_workspaces(*made.Store, *files, application::retained_workspace_sweep::policy, policy, seconds, { });
		std::cout << "RETAINED_WORKSPACE_SWEEP:count=" << swept.Count << ":removed=" << swept.Removed << ":in_use=" << swept.InUse << ":failed=" << swept.Failed << '\n';
		expect(!present(old), "a workspace unused past seven days is removed");
		expect(!present(ended), "a workspace whose conversation moved on is removed");
		expect(!present(bigOld), "the older of two over the cap is removed");
		expect(!present(live), "a workspace this run made is removed through its owner");
		expect(present(busy), "a workspace a turn holds is never removed, however old");
		expect(present(bigNew), "the newer under the cap stays");
		expect(present(tampered), "a workspace whose ownership does not verify is left as it is");
		expect(swept.Removed == 4U && swept.InUse == 1U && swept.Failed == 1U && swept.Count == 3U, "the sweep says four removed, one in use, one refused and three left");
		expect(!std::filesystem::exists(old.LifetimeGuard->Path) && !std::filesystem::exists(old.ParentDirectory), "a removed workspace's record and folder are gone");

		// AC3: thread 0 continues; its conversation holds no workspace, and its
		// next turn makes a fresh one.
		auto resumed = made.Store->select(profile::scope(0), "fixture", made.binding(0), false);
		expect(resumed.has_value() && !resumed->Workspace, "a removed workspace is cleared from its conversation");
		auto bound        = application::native_agent_session{ };
		bound.Binding     = made.binding(0);
		const auto shared = made.Store->shared_workspace(made.key(0), bound);
		expect(shared.has_value() && !*shared, "its pull request key no longer leads to it");
		const auto fresh = made.converse(*current, 0, 0);
		expect(present(fresh) && fresh.Workspace.Path != old.Workspace.Path, "the conversation continues with a fresh workspace");

		// AC2: Remove takes every one not in use; the row says what is left.
		const auto removed = application::sweep_retained_workspaces(*made.Store, *files, application::retained_workspace_sweep::remove_all, policy, seconds, { });
		std::cout << "RETAINED_WORKSPACE_REMOVE:count=" << removed.Count << ":removed=" << removed.Removed << ":in_use=" << removed.InUse << ":failed=" << removed.Failed << '\n';
		expect(!present(bigNew) && !present(fresh), "Remove removes every workspace not in use");
		expect(present(busy), "Remove leaves the one a turn holds");
		expect(removed.Removed == 2U && removed.InUse == 1U && removed.Count == 2U, "Remove says two removed and two left, one of them in use");
		const auto listed = made.Store->retained_workspaces();
		expect(listed.has_value() && listed->size() == 2U, "the catalog keeps the two left");
	}
} // namespace

auto main(int aArgumentCount, char** aArguments) -> int
{
	auto application = QCoreApplication{ aArgumentCount, aArguments };
	auto root        = temporary_root{ };
	run_course(root.Path);
	std::cout << "RETAINED_WORKSPACE_EXPIRY_OK\n";
	return 0;
}
