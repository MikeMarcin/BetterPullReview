#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/local_draft_publication_area.hpp"
#include "bpr/qt/local_draft_view.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "review_cycle_desktop.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/// \file
/// \brief RFC-BPR-0111 AC07 and AC04 ([[BPR-2029]], [[BPR-2030]]) through
/// the real workbench window and the real local-draft composition, on a real
/// Git worktree, with a scripted pull request catalog in place of GitHub:
/// the Pull request area names the push for an unpushed branch; after the
/// push the exact plan is shown and Create draft PR is offered; a lost answer
/// is read for, never resent, and said in the error-role banner with its
/// recovering action; a creation held in flight leaves the page's own lane
/// free (a note is recorded meanwhile) and shows Sending; released, the steps
/// read Sent and the draft is linked, with no second creation offered.

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::write_file;

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 8; ++round) {
			aLanes.drain();
			QApplication::processEvents();
		}
	}

	/// The publication lane is a real thread: wait for its answer.
	void await(application::manual_executor& aLanes, const std::function<bool()>& aDone, const std::string& aWhat)
	{
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds{ 30 };
		while (!aDone()) {
			expect(std::chrono::steady_clock::now() < until, aWhat);
			settle(aLanes);
			std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
		}
	}

	void commit(const git_runner& aGit, const std::filesystem::path& aCheckout, const std::string& aPath, const std::string& aBytes, const std::string& aMessage)
	{
		write_file(aCheckout / aPath, aBytes);
		aGit(aCheckout, { "add", "-A" });
		aGit(aCheckout, { "commit", "--quiet", "-m", aMessage });
	}

	/// GitHub as the course scripts it: branch heads and pull requests in memory.
	class scripted_github final : public application::github_publication_port
	{
	public:
		std::mutex                                                Mutex;
		std::map<std::string, core::git_commit_oid>               Branches;
		std::vector<application::github_pull_request_observation> Pulls;
		std::atomic<int>                                          Creates{ 0 };
		std::atomic<bool>                                         LoseRequest{ false };
		std::atomic<bool>                                         Hold{ false };
		std::atomic<bool>                                         Holding{ false };

		auto observe_repository(const application::github_publication_target& aTarget, std::stop_token) -> application::github_publication_result<application::github_publication_capability> override
		{
			return application::github_publication_capability{ .Owner = aTarget.Owner, .Repository = aTarget.Repository, .CanPush = true, .DefaultBranch = "main" };
		}
		auto observe_branch(const application::github_publication_target&, const std::string& aBranch, std::stop_token)
			-> application::github_publication_result<std::optional<core::git_commit_oid>> override
		{
			const auto lock  = std::scoped_lock{ Mutex };
			const auto found = Branches.find(aBranch);
			return found == Branches.end() ? std::nullopt : std::optional{ found->second };
		}
		auto observe_commit(const application::github_publication_target&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto observe_tree(const application::github_publication_target&, const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto observe_blob(const application::github_publication_target&, const core::git_entry_oid&, std::stop_token) -> application::github_publication_result<bool> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_blob(const application::github_publication_target&, std::span<const std::uint8_t>, const core::git_entry_oid&, std::stop_token)
			-> application::github_publication_result<void> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_tree(const application::github_publication_target&, const core::git_tree_oid&, const std::vector<core::git_tree_entry>&, const std::vector<std::string>&,
						 const core::git_tree_oid&, std::stop_token) -> application::github_publication_result<void> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_commit(const application::github_publication_target&, const core::git_commit_content&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<void> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_branch(const application::github_publication_target&, const std::string&, const core::git_commit_oid&, std::stop_token)
			-> application::github_publication_result<application::github_branch_creation> override
		{
			return std::unexpected{ application::github_publication_error{ } };
		}
		auto create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft, std::stop_token)
			-> application::github_publication_result<application::github_pull_request_observation> override
		{
			++Creates;
			if (LoseRequest.exchange(false)) {
				return std::unexpected{ application::github_publication_error{ .Transmission = application::github_publication_transmission::indeterminate } };
			}
			Holding = true;
			while (Hold.load()) {
				std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
			}
			Holding        = false;
			const auto lock = std::scoped_lock{ Mutex };
			auto       pull = application::github_pull_request_observation{ .Number         = 7U,
																			.NodeId         = "PR_7",
																			.Url            = "https://github.com/acme/engine/pull/7",
																			.State          = "open",
																			.Draft          = aDraft.Draft,
																			.Title          = aDraft.Title,
																			.HeadRef        = aDraft.Head,
																			.HeadOwner      = aTarget.Owner,
																			.HeadRepository = aTarget.Repository,
																			.HeadCommit     = Branches.at(aDraft.Head),
																			.BaseRef        = aDraft.Base,
																			.Author         = aTarget.Login };
			Pulls.push_back(pull);
			return pull;
		}
		auto find_pull_requests(const application::github_publication_target&, const std::string& aHead, const std::string& aBase, std::stop_token)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
		{
			const auto lock = std::scoped_lock{ Mutex };
			auto       out  = std::vector<application::github_pull_request_observation>{ };
			for (const auto& pull : Pulls) {
				if (pull.HeadRef == aHead && pull.BaseRef == aBase) {
					out.push_back(pull);
				}
			}
			return out;
		}
		auto observe_pull_request(const application::github_publication_target&, const std::uint64_t, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
		{
			return std::optional<application::github_pull_request_observation>{ };
		}
	};
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	const auto   git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git) {
		std::cout << "local-draft-publication-desktop-public-path:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	const auto gitExecutable = std::filesystem::canonical(std::filesystem::path{ aArgv[2] });
	auto       profileRoot   = tests::sqlite_contract::scratch_root{ "local-draft-publication-profile" };
	auto       scratchRoot   = tests::sqlite_contract::scratch_root{ "local-draft-publication-worktrees" };
	const auto root          = std::filesystem::canonical(scratchRoot.path());
	const auto seed          = root / "seed";
	const auto hosted        = root / "hosted.git";
	const auto worktree      = root / "engine";
	std::filesystem::create_directories(seed);
	(*git)(seed, { "init", "--quiet" });
	commit(*git, seed, "README.md", "engine\n", "initial");
	(*git)(root, { "clone", "--quiet", "--bare", seed.string(), hosted.string() });
	(*git)(root, { "clone", "--quiet", hosted.string(), worktree.string() });
	(*git)(worktree, { "remote", "set-url", "origin", "https://github.com/acme/engine.git" });
	(*git)(worktree, { "checkout", "--quiet", "-b", "feature/cancel" });
	commit(*git, worktree, "src/search.cpp", "int search() { return 2; }\n", "search");

	auto lanes  = application::manual_executor{ };
	auto window = std::make_unique<qt::workbench_window>();
	window->resize(1280, 860);
	window->show();
	auto cycles = std::make_unique<app::review_cycle_desktop>(*window, profileRoot.path(), core::bpr_profile_id{ 1 }, lanes, lanes, app::review_cycle_syntax{ }, &lanes);
	auto drafts = std::make_unique<app::local_draft_desktop>(*window, profileRoot.path(), gitExecutable, std::nullopt, lanes, lanes);
	auto github = std::make_shared<scripted_github>();
	auto copied = std::string{ };
	drafts->set_folder_chooser([&worktree] { return QString::fromStdString(worktree.string()); });
	drafts->set_copier([&copied](const std::string& aText) { copied = aText; });
	drafts->set_publishing([github]() -> std::optional<application::github_publication_account> {
		return application::github_publication_account{ .Provider = github, .Account = core::github_account_id{ 7U }, .Login = "author", .Host = "github.com" };
	});
	settle(lanes);
	window->home_prepare_button()->click();
	settle(lanes);
	auto* const page = window->local_draft();
	expect(page != nullptr && page->projection().Draft, "the worktree opens as a local draft");
	expect(page->areas()->tabText(4) == QStringLiteral("Pull request"), "the Pull request area is beside Tour");
	page->areas()->setCurrentIndex(4);
	settle(lanes);
	auto* const area = page->publication();
	expect(!area->create_button()->isEnabled(), "nothing can be created before GitHub is read");

	// AC01, AC07: the unpushed branch names the push; nothing is offered.
	area->check_button()->click();
	await(lanes, [&] { return page->projection().Publishing.Checked && !page->projection().Publishing.Checking; }, "GitHub is read");
	expect(area->state()->text().contains(QStringLiteral("is not on GitHub")) && area->push_command()->isVisibleTo(page)
			   && area->push_command()->text().startsWith(QStringLiteral("git -C ")) && area->push_command()->text().contains(QStringLiteral("https://github.com/acme/engine.git"))
			   && !area->create_button()->isEnabled(),
		   "AC01: the push is named and Create draft PR is not offered");
	area->copy_button()->click();
	expect(copied == area->push_command()->text().toStdString(), "Copy hands the exact command to the clipboard");

	// The author pushes with their own Git.
	{
		const auto head = page->projection().Draft->Candidates.back().Head;
		const auto lock = std::scoped_lock{ github->Mutex };
		github->Branches.emplace("feature/cancel", head);
	}
	area->check_button()->click();
	await(lanes, [&] { return !page->projection().Publishing.Checking && page->projection().Publishing.Branch == core::hosted_branch_state::matches; }, "the pushed branch");
	expect(area->create_button()->isEnabled() && !area->push_command()->isVisibleTo(page) && area->preview()->toPlainText().contains(QStringLiteral("as author"))
			   && area->preview()->toPlainText().contains(QStringLiteral("acme/engine main from feature/cancel")),
		   "AC02: the exact plan is shown and Create draft PR is offered");

	// AC04, AC07: a lost answer is read for, said in the error role and never resent.
	github->LoseRequest = true;
	area->create_button()->click();
	await(lanes, [&] { return !page->projection().Publishing.Creating && page->projection().Draft->Publication->Journal.Phase == core::local_publication_phase::prepared; },
		  "the lost answer is read for");
	expect(github->Creates == 1 && area->banner()->isVisibleTo(page) && area->banner()->title()->text() == QStringLiteral("The draft pull request was not created.")
			   && area->banner()->buttons().size() == 1U && area->banner()->buttons().front()->text() == QStringLiteral("Preview again"),
		   "AC07: nothing found after the lost answer: an error-role banner with its recovering action, and nothing resent");
	std::cout << "BANNER " << area->banner()->title()->text().toStdString() << " / " << area->banner()->why()->text().toStdString() << " / "
			  << area->banner()->on_github()->text().toStdString() << '\n';
	area->banner()->buttons().front()->click();
	await(lanes, [&] { return !page->projection().Publishing.Checking && area->create_button()->isEnabled(); }, "checked again");

	// AC07: a creation in flight holds up nothing else on the page.
	github->Hold = true;
	area->create_button()->click();
	await(lanes, [&] { return github->Holding.load(); }, "the creation is in flight");
	settle(lanes);
	expect(page->projection().Publishing.Creating && !area->create_button()->isEnabled() && !area->check_button()->isEnabled(), "Creating: no second press");
	const auto notes = page->projection().Draft->Notes.size();
	drafts->host().add_note("Written while GitHub was creating it.", "");
	settle(lanes);
	expect(page->projection().Draft->Notes.size() == notes + 1U, "AC07: the page's own lane works while the creation is in flight");
	github->Hold = false;
	await(lanes, [&] { return !page->projection().Publishing.Creating && page->projection().Draft->Publication->Journal.Phase == core::local_publication_phase::linked; },
		  "the creation answers");
	const auto rows = area->steps()->row_texts();
	for (const auto& row : rows) {
		std::cout << "STEP " << row.toStdString() << '\n';
	}
	expect(github->Creates == 2 && rows.size() == 2U && rows[0] == QStringLiteral("Sent: Create the draft pull request") && rows[1] == QStringLiteral("Sent: Link it to this draft"),
		   "AC07: each step advanced to Sent");
	expect(area->state()->text() == QStringLiteral("Draft pull request #7 is on GitHub, linked to this draft.") && area->link()->text() == QStringLiteral("https://github.com/acme/engine/pull/7")
			   && !area->create_button()->isEnabled() && !area->check_button()->isEnabled() && !area->banner()->isVisibleTo(page),
		   "AC05: linked, and no second creation offered");
	expect(page->projection().Draft->Association && page->projection().Draft->Association->Number == 7U, "the draft is associated");

	drafts->shutdown();
	cycles->shutdown();
	settle(lanes);
	std::cout << "local-draft-publication-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-publication-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
