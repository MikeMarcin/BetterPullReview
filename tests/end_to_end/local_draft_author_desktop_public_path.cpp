#include "../sqlite_contract/scratch_root.hpp"
#include "../support/test_expectation.hpp"
#include "../workspace_contract/workspace_fixture.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/local_changes_panel.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/view_picker.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "local_draft_desktop.hpp"
#include "review_cycle_desktop.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMenu>
#include <QStyleHints>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QThread>
#include <QToolButton>
#include <QTreeWidget>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

/// \file
/// \brief The owner's redesign through the real workbench window and
/// composition on a real worktree (RFC-BPR-0108, 0113 and 0114 as amended;
/// owner 2026-11-13, "augment the real PR window, do not rebuild it"): a
/// local draft opens in the pull request window's own review surface (its
/// Files tree and Search tab, its diff viewer with syntax colours, Overview
/// and status bar) in author state, with Local Changes as a tab;
/// Check Again reads the "hosted" branch, a local bare repository the
/// worktree's own configuration names for GitHub's URL; Local Changes lists
/// Unstaged and Staged; Stage Change stages one change; the agent fills the
/// commit's subject and description and commits nothing; Commit N Files with
/// the one-time question; Push sends the branch, never forced; another
/// author's push turns the line red and refuses before anything runs; Edit
/// and Save write the file; Home shows the draft as one line. Renders are
/// written for the owner's look (native when run with BPR_NATIVE_LOOK=1).

namespace {
	using namespace bpr;
	using tests::expect;
	using tests::workspace_contract::git_runner;
	using tests::workspace_contract::read_file;
	using tests::workspace_contract::write_file;

	void settle(application::manual_executor& aLanes)
	{
		for (int round = 0; round < 10; ++round) {
			aLanes.drain();
			QApplication::processEvents();
		}
	}

	/// Settles until `aDone`, for work on a real thread (the agent's lane).
	template <typename TDone>
	void settle_until(application::manual_executor& aLanes, TDone aDone)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (!aDone() && timer.elapsed() < 20'000) {
			settle(aLanes);
			QThread::msleep(10);
		}
		settle(aLanes);
	}

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_LOCAL_DRAFT_RENDER_DIR");
		const auto image     = aWidget.grab().toImage();
		expect(!image.isNull(), "rendered window");
		if (!directory.isEmpty()) {
			QDir{ }.mkpath(directory);
			const auto scheme = qEnvironmentVariable("BPR_NATIVE_LOOK_SCHEME");
			expect(image.save(QDir{ directory }.filePath(aName + (scheme.isEmpty() ? QString{ } : QLatin1Char{ '-' } + scheme) + QStringLiteral(".png"))), "render saved");
		}
		std::cout << "RENDER " << aName.toStdString() << ' ' << image.width() << 'x' << image.height() << '\n';
	}

	auto tree_paths(QTreeWidget& aTree) -> QStringList
	{
		auto paths = QStringList{ };
		for (auto iterator = QTreeWidgetItemIterator{ &aTree }; *iterator != nullptr; ++iterator) {
			if (const auto path = (*iterator)->data(0, Qt::UserRole + 1).toString(); !path.isEmpty()) {
				paths << path;
			}
		}
		return paths;
	}

	auto item_of(QTreeWidget& aTree, const QString& aPath) -> QTreeWidgetItem*
	{
		for (auto iterator = QTreeWidgetItemIterator{ &aTree }; *iterator != nullptr; ++iterator) {
			if ((*iterator)->data(0, Qt::UserRole + 1).toString() == aPath) {
				return *iterator;
			}
		}
		return nullptr;
	}

	struct session
	{
		application::manual_executor               Lanes;
		std::unique_ptr<qt::workbench_window>      Window;
		std::unique_ptr<app::review_cycle_desktop> Cycles;
		std::unique_ptr<app::local_draft_desktop>  Drafts;
		adapters::syntax::packaged_definition_catalog Definitions;

		session(const std::filesystem::path& aProfile, const std::filesystem::path& aGit, const QString& aChosen)
		{
			Window = std::make_unique<qt::workbench_window>();
			Window->resize(1280, 800);
			Window->show();
			Cycles = std::make_unique<app::review_cycle_desktop>(*Window, aProfile, core::bpr_profile_id{ 1 }, Lanes, Lanes, app::review_cycle_syntax{ }, &Lanes);
			Drafts = std::make_unique<app::local_draft_desktop>(*Window, aProfile, aGit, std::nullopt, Lanes, Lanes, app::local_draft_syntax{ .Definitions = &Definitions, .Effects = &Lanes });
			// The platform's scheme, as the application observes it, on the window and the panes.
			const auto scheme     = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? core::os_color_scheme::dark : core::os_color_scheme::light;
			auto       appearance = qt::appearance_profile{ qt::default_appearance_settings(), { .Scheme = scheme } };
			appearance.apply_to(*Window);
			Window->comparison_surface()->adopt_appearance(appearance);
			Drafts->adopt_appearance(appearance);
			Drafts->set_folder_chooser([aChosen] { return aChosen; });
			Drafts->set_consent_question([] { return true; });
			settle(Lanes);
		}
		~session()
		{
			Drafts->shutdown();
			Cycles->shutdown();
			settle(Lanes);
		}
		session(const session&)                    = delete;
		auto operator=(const session&) -> session& = delete;
		session(session&&)                         = delete;
		auto operator=(session&&) -> session&      = delete;
	};
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	const auto   git = tests::workspace_contract::git_from(aArgc, aArgv);
	if (!git || aArgc < 5) {
		std::cout << "local-draft-author-desktop-public-path:skipped (no Git)\n";
		return EXIT_SUCCESS;
	}
	const auto gitExecutable = std::filesystem::canonical(std::filesystem::path{ aArgv[2] });
	const auto script        = std::filesystem::path{ aArgv[4] };
	auto       profileRoot   = tests::sqlite_contract::scratch_root{ "local-draft-author-profile" };
	auto       scratchRoot   = tests::sqlite_contract::scratch_root{ "local-draft-author-worktrees" };
	const auto scratch       = std::filesystem::canonical(scratchRoot.path());
	const auto made          = std::system(("/bin/sh '" + script.string() + "' '" + scratch.string() + "'").c_str());
	expect(made == 0, "the author-state fixture is made");
	const auto worktree = scratch / "engine";
	const auto hosted   = scratch / "hosted.git";

	auto  s      = session{ profileRoot.path(), gitExecutable, QString::fromStdString(worktree.string()) };
	auto& window = *s.Window;
	window.home_prepare_button()->click();
	settle(s.Lanes);
	auto* const page    = window.local_draft();
	auto* const changes = page->local_changes();
	auto* const tabs    = window.file_search_tabs();
	auto* const viewer  = window.comparison_surface();
	expect(window.showing_local_draft() && page->projection().Draft && window.current_modality() == qt::review_modality::review,
		   "the local draft opens in the pull request window's own review surface, on Review");
	expect(tabs->tabText(0) == QStringLiteral("Files") && tabs->tabText(1) == QStringLiteral("Search") && window.local_changes_tab() > 1 && tabs->isTabVisible(window.local_changes_tab())
			   && tabs->tabText(window.local_changes_tab()) == QStringLiteral("Local Changes"),
		   "the pull request's Files and Search tabs, and Local Changes always present in author state");
	expect(window.review_view_picker()->author() && page->branch_button()->isVisible(), "author state: the branch and the picker's Edit");
	expect(!page->working_disclosure()->isChecked() && !page->working()->isVisible(), "working files wait behind a disclosure");
	settle_until(s.Lanes, [&] { return page->projection().File && window.local_comparison() && viewer->current_result(); });
	expect(window.local_comparison() && viewer->current_result() && viewer->current_context() == window.local_comparison()->Context,
		   "the candidate's file is shown by the pull request's own diff viewer");
	settle_until(s.Lanes, [&] { return viewer->token_spans(core::diff_side::right) != nullptr; });
	expect(viewer->token_spans(core::diff_side::right) != nullptr, "the viewer colours the draft's code as it colours a pull request's");
	expect(page->local_projection().Observed && !page->local_projection().Hosted.Observed && changes->push_button()->text() == QStringLiteral("Check Again"),
		   "opening reads the worktree only; GitHub is not read yet");

	// Check Again reads the hosted branch through the author's Git.
	changes->push_button()->click();
	settle(s.Lanes);
	expect(page->local_projection().Hosted.Observed && page->local_projection().Hosted.Ahead == 2U && changes->push_state()->text() == QStringLiteral("2 ahead of GitHub"),
		   "2 ahead of GitHub");
	expect(page->branch_button()->text().contains(QStringLiteral("fix/port-parsing")) && page->branch_button()->text().contains(QStringLiteral("↑2")), "the branch says it is 2 ahead");
	expect(page->status_text(1, 3).contains(QStringLiteral("4 uncommitted, 2 to push")), "the status bar counts what GitHub lacks");
	capture(window, QStringLiteral("a-review"));
	window.choose_modality(qt::review_modality::overview);
	settle(s.Lanes);
	expect(window.overview_page()->isVisible() && page->author_panel()->isVisible(), "Overview is the pull request's, with the author's panel");
	capture(window, QStringLiteral("a-overview"));
	window.choose_modality(qt::review_modality::review);
	settle(s.Lanes);

	// Local Changes: Unstaged over Staged.
	tabs->setCurrentIndex(window.local_changes_tab());
	settle(s.Lanes);
	expect(tree_paths(*changes->unstaged()) == QStringList{ QStringLiteral("NOTES.txt"), QStringLiteral("docs/addresses.md"), QStringLiteral("tests/net/endpoint_test.cpp") },
		   "Unstaged: the two edits and the untracked note");
	expect(tree_paths(*changes->staged()) == QStringList{ QStringLiteral("docs/ports.md") }, "Staged: the ports page");
	expect(changes->pick(core::local_comparison::uncommitted)->text() == QStringLiteral("Uncommitted  4") && changes->pick(core::local_comparison::not_pushed)->isEnabled(),
		   "the pick counts each comparison");

	// Stage Change: one change of the test file, from the code's menu.
	changes->unstaged()->setCurrentItem(item_of(*changes->unstaged(), QStringLiteral("tests/net/endpoint_test.cpp")));
	settle(s.Lanes);
	expect(window.local_comparison() && window.local_comparison()->Path == "tests/net/endpoint_test.cpp" && viewer->current_result(), "Local Changes' file is in the same viewer");
	const auto change_action = [&](const std::size_t aBand, const QString& aName) -> QAction* {
		const auto& bands = page->local_projection().File->Bands;
		if (aBand >= bands.size() || !viewer->select_conversation_line(core::diff_side::right, bands[aBand].Shown.RightBegin)) {
			return nullptr;
		}
		auto* const menu = window.code_context_menu(nullptr, QPoint{ });
		for (auto* const action : menu->actions()) {
			if (action->objectName() == aName) {
				return action;
			}
		}
		return nullptr;
	};
	expect(page->local_projection().File->Bands.size() == 2U && change_action(0U, QStringLiteral("localStageChangeAction")) != nullptr
			   && change_action(1U, QStringLiteral("localStageChangeAction")) != nullptr,
		   "two changes, each with Stage Change on the code's menu");
	capture(window, QStringLiteral("local-changes-uncommitted"));
	change_action(0U, QStringLiteral("localStageChangeAction"))->trigger();
	settle(s.Lanes);
	const auto cached = git->output(worktree, { "diff", "--cached", "-U0", "--", "tests/net/endpoint_test.cpp" }, scratch / "cached.txt");
	expect(cached.find("0080") != std::string::npos && cached.find("rejects what atoi") == std::string::npos, "Stage Change staged exactly the first change");
	expect(tree_paths(*changes->staged()).contains(QStringLiteral("tests/net/endpoint_test.cpp")) && tree_paths(*changes->unstaged()).contains(QStringLiteral("tests/net/endpoint_test.cpp")),
		   "a partly staged file is in both lists");
	expect(change_action(0U, QStringLiteral("localUnstageChangeAction")) != nullptr, "the staged change offers Unstage Change");
	capture(window, QStringLiteral("local-changes-staged-change"));

	// The agent fills the commit: one fresh, tool-free run of the footer's agent.
	auto prompts = std::make_shared<std::vector<std::string>>();
	s.Drafts->set_agent(
		[prompts](const application::local_draft_agent_run& aRun, std::stop_token) {
			expect(aRun.Path == core::agent_run_path::local_commit_message && aRun.Instructions == "Write the commit message." && aRun.Handle.empty(),
				   "[[BPR-2138]] [[BPR-2140]] the commit message is a fresh run whose purpose text travels apart");
			prompts->push_back(aRun.Prompt);
			return application::local_draft_agent_answer{ .Answered = true, .Text = "Subject: Test leading zeros and document ports\n\nA port with leading zeros parses to its value." };
		},
		&s.Lanes,
		[] {
			return std::optional{ app::local_draft_desktop::agent_choice{
				.Identity = "scripted", .Name = "Claude (scripted)", .Profile = 0U, .CommitMessage = "Write the commit message." } };
		});
	settle(s.Lanes);
	expect(qt::agent_run_control(*changes->suggest_button()) && changes->suggest_button()->isEnabled(), "the agent's control wears the accent");
	expect(changes->suggest_button()->parentWidget() == changes->commit_button()->parentWidget()
			   && changes->suggest_button()->geometry().right() < changes->commit_button()->geometry().left()
			   && std::abs(changes->suggest_button()->geometry().center().y() - changes->commit_button()->geometry().center().y()) <= 4,
		   "the nib sits beside Commit, not the subject");
	changes->suggest_button()->click();
	settle_until(s.Lanes, [&] { return changes->subject()->text().contains(QStringLiteral("leading zeros")); });
	expect(changes->subject()->text() == QStringLiteral("Test leading zeros and document ports") && changes->description()->toPlainText() == QStringLiteral("A port with leading zeros parses to its value."),
		   "the suggestion fills subject and description");
	expect(prompts->size() == 1U && prompts->front().find("Write the commit message.") == std::string::npos && prompts->front().find("0080") != std::string::npos,
		   "the request is the staged diff; the purpose's text is its instructions");
	expect(git->output(worktree, { "rev-list", "--count", "HEAD" }, scratch / "count.txt") == "4", "a suggestion commits nothing");
	capture(window, QStringLiteral("local-changes-agent-message"));

	// Commit N Files, after the one-time question.
	expect(changes->commit_button()->text() == QStringLiteral("Commit 2 Files") && changes->commit_button()->isEnabled() && changes->amend()->isEnabled(), "Commit 2 Files; Amend offered");
	changes->commit_button()->click();
	settle(s.Lanes);
	expect(git->output(worktree, { "log", "-1", "--format=%s" }, scratch / "subject.txt") == "Test leading zeros and document ports", "committed with the edited suggestion");
	expect(page->local_projection().Status.Staged.empty() && tree_paths(*changes->unstaged()).contains(QStringLiteral("tests/net/endpoint_test.cpp")), "the unstaged change is as it was");
	expect(changes->push_state()->text() == QStringLiteral("3 ahead of GitHub"), "3 ahead");

	// Push: the branch to the hosted repository, never forced.
	expect(changes->command()->text().startsWith(QStringLiteral("git push --no-follow-tags https://github.com/bpr-fixtures/engine.git refs/heads/fix/port-parsing:refs/heads/fix/port-parsing")),
		   "the push line shows the push exactly as it runs");
	changes->push_button()->click();
	settle(s.Lanes);
	expect(git->output(hosted, { "rev-parse", "refs/heads/fix/port-parsing" }, scratch / "tip.txt") == git->output(worktree, { "rev-parse", "HEAD" }, scratch / "head.txt"),
		   "GitHub has the branch's head");
	expect(changes->push_state()->text() == QStringLiteral("Up to date with GitHub") && changes->push_button()->text() == QStringLiteral("Check Again") && !changes->amend()->isEnabled(),
		   "up to date: Check Again; Amend withdrawn");
	capture(window, QStringLiteral("local-changes-pushed"));

	// Another author pushes: the line turns red and Push is refused.
	const auto other = scratch / "other";
	(*git)(scratch, { "clone", "--quiet", "-b", "fix/port-parsing", hosted.string(), other.string() });
	write_file(other / "src/other.cpp", "int other;\n");
	(*git)(other, { "add", "-A" });
	(*git)(other, { "-c", "user.name=Other", "-c", "user.email=other@example.invalid", "commit", "--quiet", "-m", "Another author" });
	(*git)(other, { "push", "--quiet", "origin", "fix/port-parsing" });
	changes->push_button()->click(); // Check Again
	settle(s.Lanes);
	expect(page->local_projection().Push.State == core::local_push_state::unknown_tip && changes->push_button()->text() == QStringLiteral("Check Again")
			   && changes->banner()->isVisible() && changes->banner()->text().startsWith(QStringLiteral("Push refused")),
		   "refused before anything runs: red line, banner, Check Again");
	capture(window, QStringLiteral("local-changes-refused"));

	// Edit: the worktree's file, editable; Save writes it.
	changes->unstaged()->setCurrentItem(item_of(*changes->unstaged(), QStringLiteral("docs/addresses.md")));
	settle(s.Lanes);
	window.review_view_picker()->segment(qt::review_view::edit)->click();
	settle(s.Lanes);
	expect(page->editor()->isVisible() && QString::fromStdString(page->editor()->text()).contains(QStringLiteral("fails the whole address")), "Edit shows the file on disk");
	settle_until(s.Lanes, [&] { return page->editor()->syntax_lines() > 0U; });
	expect(page->editor()->syntax_lines() > 0U, "the Edit view has the panes' syntax colours");
	capture(window, QStringLiteral("edit-syntax"));
	// Choosing another file in Files updates the Edit pane.
	tabs->setCurrentIndex(0);
	settle(s.Lanes);
	expect(window.open_local_file(QStringLiteral("src/endpoint.cpp")), "Files has the candidate's file");
	settle(s.Lanes);
	expect(page->editor()->isVisible() && page->edit_path() == "src/endpoint.cpp" && QString::fromStdString(page->editor()->text()).contains(QStringLiteral("parse_port")),
		   "a file chosen in Files opens in the Edit pane");
	capture(window, QStringLiteral("edit-follows-files"));
	tabs->setCurrentIndex(window.local_changes_tab());
	changes->unstaged()->setCurrentItem(item_of(*changes->unstaged(), QStringLiteral("docs/addresses.md")));
	settle(s.Lanes);
	expect(page->edit_path() == "docs/addresses.md", "and one chosen in Local Changes");
	page->editor()->set_text("# Addresses\n\nAn address is a host and a port.\n\nA port is 1 to 65535; anything else fails the whole address.\nIPv6 needs brackets.\n");
	if (page->editor()->Changed) {
		page->editor()->Changed();
	}
	settle(s.Lanes);
	expect(page->save_button()->isEnabled(), "an edit enables Save");
	capture(window, QStringLiteral("edit-not-saved"));
	page->save_button()->click();
	settle(s.Lanes);
	expect(read_file(worktree / "docs/addresses.md").find("IPv6 needs brackets.") != std::string::npos && !page->save_button()->isEnabled(), "Save wrote the file");
	capture(window, QStringLiteral("edit-saved"));

	// Home: one line per draft, beside Prepare Pull Request… in the Open row.
	window.home_action()->trigger();
	settle(s.Lanes);
	expect(window.showing_home() && window.home_drafts_list()->count() == 1 && window.home_drafts_list()->item(0)->sizeHint().height() == 26, "Home 1: one 26 px line");
	capture(window, QStringLiteral("home-1"));
	std::cout << "local-draft-author-desktop-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "local-draft-author-desktop-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
