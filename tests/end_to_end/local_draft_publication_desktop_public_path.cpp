#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/local_draft_publication_area.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "review_cycle_desktop.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
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
#include <tuple>
#include <vector>

/// \file
/// \brief RFC-BPR-0111 AC07 and AC04 ([[BPR-2029]], [[BPR-2030]]) through
/// the real workbench window and the real local-draft composition, on a real
/// Git worktree, with a scripted pull request catalog in place of GitHub:
/// Create Draft PR… opens Overview's Status tab with the publish steps (the
/// accepted design); the unpushed branch names the push; after the push what
/// is sent is shown and Create Draft PR is offered; a lost answer is read
/// for, never resent, and said in the error-role banner with its recovering
/// action; a creation held in flight leaves the draft's own lane free and
/// shows Sending; released, the steps read Sent, the draft is linked with no
/// second creation offered, the window is asked to open the pull request,
/// and the draft's tour is carried onto the pull request's tour draft at its
/// head (and anchors nothing at another head).

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::write_file;

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_LOCAL_DRAFT_RENDER_DIR");
		const auto image     = aWidget.grab().toImage();
		expect(!image.isNull(), "rendered window");
		if (!directory.isEmpty()) {
			QDir{ }.mkpath(directory);
			expect(image.save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "render saved");
		}
		std::cout << "RENDER " << aName.toStdString() << ' ' << image.width() << 'x' << image.height() << '\n';
	}

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
			Holding         = false;
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
	auto drafts = std::make_unique<app::local_draft_desktop>(*window, profileRoot.path(), gitExecutable, std::nullopt, lanes, lanes, { }, &lanes);
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
	// The tour goes with the pull request: one accepted stop on the pushed file.
	auto opened = std::optional<std::tuple<std::string, std::string, std::string, std::uint64_t>>{ };
	drafts->set_pull_request_opener([&opened](const std::string& aHost, const std::string& aOwner, const std::string& aName, const std::uint64_t aNumber) { opened = std::tuple{ aHost, aOwner, aName, aNumber }; });
	drafts->host().add_stop("src/search.cpp", 1U, 1U, "Search returns two", "The answer is two now.");
	settle(lanes);
	drafts->host().accept_stop(page->projection().Draft->Preparation.Tour.Stops.at(0).Id);
	settle(lanes);
	expect(core::decide_local_stop(page->projection().Draft->Preparation.Tour.Stops.at(0)).State == core::tour_acceptance_state::accepted, "the stop is accepted");

	// Create Draft PR… stands where Submit review does and opens Overview's
	// Status tab, where the publish steps are; GitHub is read for the plan.
	expect(page->create_button()->text() == QStringLiteral("Create Draft PR…") && page->create_button()->isVisible(), "Create Draft PR… in the review bar");
	page->create_button()->click();
	settle(lanes);
	auto* const area   = page->publication();
	auto* const status = window->overview_page()->status_drawer();
	expect(window->current_modality() == qt::review_modality::overview && status->current_tab() == qt::overview_drawer_tab::status && status->status_page() == page->status_page()
			   && area->isVisible(),
		   "the publish steps are Overview's Status tab");

	// AC01, AC07: the unpushed branch names the push; nothing is offered.
	await(lanes, [&] { return page->projection().Publishing.Checked && !page->projection().Publishing.Checking; }, "GitHub is read");
	capture(*window, QStringLiteral("status-push"));
	const auto steps = area->steps()->row_texts();
	for (const auto& row : steps) {
		std::cout << "STEP " << row.toStdString() << '\n';
	}
	expect(area->headline()->text() == QStringLiteral("Not on GitHub") && area->state()->text().contains(QStringLiteral("is not on GitHub")) && area->push_command()->isVisibleTo(page->status_page())
			   && area->push_command()->text().startsWith(QStringLiteral("git -C ")) && area->push_command()->text().contains(QStringLiteral("https://github.com/acme/engine.git"))
			   && !area->create_button()->isVisibleTo(page->status_page()),
		   "AC01: the push is named and Create Draft PR is not offered");
	expect(steps.size() == 3U && steps[0] == QStringLiteral("Waiting: Push feature/cancel") && steps[1] == QStringLiteral("Waiting: Create the draft pull request")
			   && steps[2] == QStringLiteral("Waiting: Publish the tour"),
		   "the three steps, in the accepted step list");
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
	std::cout << "SENT " << area->sent_text().toStdString() << '\n';
	expect(area->create_button()->isEnabled() && !area->push_command()->isVisibleTo(page->status_page()) && area->sent_text().contains(QStringLiteral("As author"))
			   && area->sent_text().contains(QStringLiteral("From acme/engine feature/cancel")) && area->sent_text().contains(QStringLiteral("Into acme/engine main"))
			   && area->steps()->row_texts().at(0) == QStringLiteral("Sent: Push feature/cancel"),
		   "AC02: what is sent is shown and Create Draft PR is offered");
	capture(*window, QStringLiteral("status-ready"));

	// AC04, AC07: a lost answer is read for, said in the error role and never resent.
	github->LoseRequest = true;
	area->create_button()->click();
	await(lanes, [&] { return !page->projection().Publishing.Creating && page->projection().Draft->Publication->Journal.Phase == core::local_publication_phase::prepared; }, "the lost answer is read for");
	expect(github->Creates == 1 && area->banner()->isVisibleTo(page->status_page()) && area->banner()->title()->text() == QStringLiteral("The draft pull request was not created.")
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
	await(lanes, [&] { return !page->projection().Publishing.Creating && page->projection().Draft->Publication->Journal.Phase == core::local_publication_phase::linked; }, "the creation answers");
	const auto rows = area->steps()->row_texts();
	for (const auto& row : rows) {
		std::cout << "STEP " << row.toStdString() << '\n';
	}
	expect(github->Creates == 2 && rows.size() == 3U && rows[0] == QStringLiteral("Sent: Push feature/cancel") && rows[1] == QStringLiteral("Sent: Create the draft pull request")
			   && rows[2] == QStringLiteral("Waiting: Publish the tour (from the Tour tab)"),
		   "AC07: each step advanced to Sent; the tour waits for the pull request's Tour");
	expect(area->headline()->text() == QStringLiteral("Draft pull request #7") && area->state()->text() == QStringLiteral("Draft pull request #7 is on GitHub, linked to this draft.")
			   && area->link()->text() == QStringLiteral("https://github.com/acme/engine/pull/7") && !area->create_button()->isVisibleTo(page->status_page())
			   && !area->check_button()->isVisibleTo(page->status_page()) && !area->banner()->isVisibleTo(page->status_page()),
		   "AC05: linked, and no second creation offered");
	expect(page->projection().Draft->Association && page->projection().Draft->Association->Number == 7U, "the draft is associated");
	expect(opened == std::tuple{ std::string{ "github.com" }, std::string{ "acme" }, std::string{ "engine" }, std::uint64_t{ 7U } }, "on creation the window opens the pull request");

	// The tour follow-up's private half: the draft's tour as the pull
	// request's own tour draft, anchored at its head (RFC-BPR-0110 "On publication").
	auto target  = core::review_tour{ .Id = core::tour_draft_id{ 1U }, .Owner = { core::bpr_profile_id{ 1U }, core::git_host_id{ 1U }, { }, core::github_account_id{ 7U }, core::repository_id{ 1U }, core::pull_request_id{ 7U } }, .Base = page->projection().Draft->Candidates.back().Base, .Head = page->projection().Draft->Candidates.back().Head };
	auto carried = std::optional<core::local_tour_carry>{ };
	drafts->host().carry_tour(page->projection().Draft->Id, target, [&carried](core::local_tour_carry aCarry) { carried = std::move(aCarry); });
	await(lanes, [&] { return carried.has_value(); }, "the tour is carried");
	std::cout << "CARRIED stops=" << carried->Tour.Stops.size() << " unresolved=" << carried->Mapping.Unresolved << " correspondence="
			  << (carried->Mapping.Stops.empty() ? std::string{ "none" } : std::string{ core::name(carried->Mapping.Stops[0].Correspondence) }) << '\n';
	expect(carried->Tour.Stops.size() == 1U && carried->Tour.Stops[0].Destination && carried->Tour.Stops[0].Destination->Commit == target.Head
			   && carried->Tour.Stops[0].Destination->Path == "src/search.cpp" && carried->Tour.Stops[0].Destination->FirstLine == 0U && carried->Mapping.Unresolved == 0U,
		   "the stop anchors at the pull request's head");
	expect(core::decide_tour_stop_acceptance(carried->Tour, carried->Tour.Stops[0]).State == core::tour_acceptance_state::accepted, "and stays Accepted there");
	auto moved = target;
	moved.Head = page->projection().Draft->Candidates.back().Base;
	carried.reset();
	drafts->host().carry_tour(page->projection().Draft->Id, moved, [&carried](core::local_tour_carry aCarry) { carried = std::move(aCarry); });
	await(lanes, [&] { return carried.has_value(); }, "the tour is carried to another head");
	expect(carried->Tour.Stops.size() == 1U && !carried->Tour.Stops[0].Destination && carried->Mapping.Unresolved == 1U && carried->Tour.Stops[0].Title == "Search returns two",
		   "a head that is not the candidate anchors nothing; the stop comes without lines, not dropped");

	drafts->shutdown();
	cycles->shutdown();
	settle(lanes);
	std::cout << "local-draft-publication-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-publication-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
