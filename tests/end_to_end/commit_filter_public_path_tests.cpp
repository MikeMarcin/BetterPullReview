// RFC-BPR-0089 M1. Commit filters through the shipped composition, offscreen,
// on the commit-filter-v1 fixture (`scripts/build_commit_filter_fixture.py`):
// the merge base M, C1 (src/generator.in and line 5 of src/app.cpp), C2 (all
// 96 files under gen/ regenerated) and C3 (line 40 of src/app.cpp and two
// generated files edited by hand). GitHub's Files changed is M...C3, 98 files.
//
// The course drives the Commits tab's own context menu and the chips, reads
// the Files drawer, the Overview and the comparison surface the reviewer
// sees, and checks each filtered diff line for line against the commits'
// own changes (AC01, AC02, AC05).
//
// RFC-BPR-0089 M2. A second course takes the fixture as it is, with no
// `.gitattributes`: nothing is set aside until a gesture says so; Hide files
// these commits changed on C2 hides the paths C3 also edited; Treat as
// generated, Review anyway and Follow .gitattributes work from the file menus
// and the generated row, survive a restart through the review ledger, and
// Clear all filters leaves generated files behind their row (AC04, AC05).
//
// [[BPR-1692]]. A third course opens commit-filter-attributes-v1, the same
// chain whose head marks `gen/**` linguist-generated and returns
// gen/item_007.txt with a nested -linguist-generated: Whole PR and every
// filter leave the 95 marked paths behind the generated row, out of the walk
// and the counts and unprepared, Clear all filters leaves them there, and a
// reviewer's mark still outranks the attributes (AC04, AC05). [[BPR-1693]]:
// a set-aside file's diff opens on request from either generated row and
// leaves the walk, the counts and the marks as they were.
//
// RFC-BPR-0089 M3. A fourth course opens commit-filter-split-v1, whose
// src/version.txt all three commits change: with C2 hidden the file keeps one
// row and reads C1's and C3's own diffs, each against Git, each with its own
// Seen and Read, C2 named between them; no shown blob is one no commit holds
// (AC03). A fifth serves its next push, C3 reworded on the same parent and
// tree: after hiding C3 and refreshing, the chip names the gone commit,
// offers the reworded one and changes nothing until the reviewer chooses it;
// identical-material receipts persist (AC06, AC05).
//
// RFC-BPR-0089 M4 (owner decision Q1). A sixth course opens
// commit-filter-comment-v1, whose src/notes.txt is shaped as BPR-Test #116:
// C1 changes its lines 5, 15 and 25, C3 inserts two lines at its top and
// changes C1's lines 5 and 25 again. With C3 hidden the file's diff is M to
// C1. Its recorded thread written on C1 at line 25, which GitHub keeps there
// outdated, is marked on C1's diff and the head's thread is not; Whole PR marks
// the head's and not C1's. A comment on line 15 is published at the head's
// line 17, saying so before Publish; one on line 5 is published on C1 after the
// warning, and GitHub (the fixture, as measured) keeps it there, outdated.
// Both reconcile through the real provider session. With only C3 shown the
// left side is C2: a base line the merge base has unchanged is published as
// GitHub's LEFT line (AC07). [[BPR-1706]]. A line GitHub resolves nowhere as a
// line comment (one C1 changed on that left side, or one outside every hunk of
// GitHub's diff at the head, which GitHub refused on BPR-Test #117) is
// published, after the editor says so, as a file comment at the head whose
// permalink names the line at the diff's own commit on its side, with no line
// comment attempted; each reconciles once.
//
// [[BPR-1688]]. A seventh course, on the same fixture: with C3 hidden Find in
// files searches only the filtered files, each at its filtered diff's commits
// (src/notes.txt at C1, so C3's inserted line is not found and C1's line 5
// is), and says it searches the filtered set. An eighth runs the desktop,
// the composition that keeps each window's review with the profile: the
// window keeps its commit and file filters with its review, quitting and
// resuming it from Home brings both back, and a kept hidden commit the pull
// request no longer has is named by the chip as after a force push.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/source_find_bar.hpp"
#include "bpr/qt/source_search_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;
	using lines  = std::vector<std::pair<std::uint32_t, std::string>>;

	constexpr auto merge_base = std::string_view{ "bd83187f4e5b01d458e7fa1670d8e5acc1ca10de" };
	constexpr auto c1         = std::string_view{ "14b75d4f24d39241820c166263d1c5eab9e708d3" };
	constexpr auto c2         = std::string_view{ "157e8018bfc094ec8a4e1ef412469b20913f9895" };
	constexpr auto c3         = std::string_view{ "47d702322daae5b72c9927d6b49c415285aa87d8" };
	constexpr auto whole      = std::size_t{ 98U };

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	[[nodiscard]] auto commit(const std::string_view aHex) -> core::git_commit_oid
	{
		return *core::parse_git_commit_oid(aHex);
	}

	[[nodiscard]] auto shown(const qt::workbench_window& aWindow, const std::string_view aPath) -> bool
	{
		const auto* surface = aWindow.comparison_surface();
		return surface->current_result() && surface->current_context()
			&& surface->current_context()->Key.File == core::file_identity_of(aPath)
			&& surface->current_result()->Alignment.Key == surface->current_context()->Key && !surface->geometry_pending();
	}

	/// Every line a non-equal segment covers on one side, numbered from one.
	[[nodiscard]] auto changed(const core::computed_file_diff& aDiff, const core::diff_side aSide) -> lines
	{
		const auto& document = aSide == core::diff_side::left ? aDiff.LeftDocument : aDiff.RightDocument;
		auto        result   = lines{ };
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == core::alignment_kind::equal) {
				continue;
			}
			const auto& span = aSide == core::diff_side::left ? segment.Left : segment.Right;
			for (auto line = span.Begin; line < span.End; ++line) {
				const auto& bytes = document->Lines[line].Content;
				result.emplace_back(line + 1U, std::string{ reinterpret_cast<const char*>(document->Bytes.data()) + bytes.Begin, bytes.End - bytes.Begin });
			}
		}
		return result;
	}

	struct course
	{
		QTemporaryDir                            Temporary{ QDir::tempPath() + QStringLiteral("/bpr-commit-filter-XXXXXX") };
		std::unique_ptr<app::github_application> Application;
	};

	[[nodiscard]] auto tab(const qt::workbench_window& aWindow) -> qt::commit_filter_tab*
	{
		return aWindow.overview_page()->status_drawer()->commits();
	}

	[[nodiscard]] auto drawer_files(const qt::workbench_window& aWindow) -> int
	{
		return aWindow.files_drawer()->contents()->rowCount();
	}

	[[nodiscard]] auto overview_files(const qt::workbench_window& aWindow) -> std::size_t
	{
		return aWindow.overview_page()->projection() ? aWindow.overview_page()->projection()->Files.size() : 0U;
	}

	/// \brief Which fixture a course opens and how many files its Overview
	/// lists once open.
	struct opening
	{
		app::detail::github_public_path_fixture Fixture = app::detail::github_public_path_fixture::commit_filter_v1;
		QString                                 Address = QStringLiteral("bpr-fixtures/commit-filter#89");
		std::size_t                             Files   = whole;
		/// M3. The seed phase serves the fixture's second head after a refresh.
		app::detail::github_epoch_fixture_phase Phase = app::detail::github_epoch_fixture_phase::single;
		/// M4. The pull request is read again after a publication, as a
		/// refresh reads it.
		bool Rereads = false;
	};

	auto start(course& aCourse, const opening& aOpening = { }) -> qt::workbench_window&
	{
		const auto dataRoot = e2e::private_data_root(aCourse.Temporary);
		expect(dataRoot.has_value(), "private course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = dataRoot;
		options.EpochPhase          = aOpening.Phase;
		options.RepeatReadSessions  = aOpening.Rereads;
		auto terminal               = false;
		auto startup                = access::start(aOpening.Fixture, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				aCourse.Application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
		expect(aCourse.Application != nullptr, "composition starts");
		aCourse.Application->show();
		auto& window = aCourse.Application->window();
		window.resize(1280, 860);
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
		window.home_open_bar()->setText(aOpening.Address);
		window.home_open_action()->trigger();
		auto opened = QElapsedTimer{ };
		opened.start();
		while (!(window.showing_overview() && overview_files(window) == aOpening.Files) && opened.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!(window.showing_overview() && overview_files(window) == aOpening.Files)) {
			std::cout << "commit-filter opening overview=" << window.showing_overview() << " files=" << overview_files(window)
					  << " status=" << window.github_status_text() << '\n';
		}
		wait_for([&] { return window.showing_overview() && overview_files(window) == aOpening.Files; }, "the pull request opens on Overview with its expected files");
		return window;
	}

	/// \brief Closes the composition and keeps its data root, as quitting does.
	void restart(course& aCourse)
	{
		aCourse.Application->begin_shutdown();
		wait_for([&] { return aCourse.Application->status().ShutdownComplete; }, "course shutdown drains before the restart");
		aCourse.Application.reset();
	}

	void finish(course& aCourse)
	{
		aCourse.Application->begin_shutdown();
		wait_for([&] { return aCourse.Application->status().ShutdownComplete; }, "course shutdown drains");
		aCourse.Application.reset();
		expect(e2e::remove_course_root(aCourse.Temporary), "private course root removed");
	}

	void enter_review(course& aCourse)
	{
		auto step  = app::detail::course_modality_step{ };
		step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
		wait_for([&] { return access::enter_course_modality(*aCourse.Application, step) != app::detail::course_modality_result::pending; }, "enters Review");
	}

	/// The reviewer selects these commits in the Commits tab and chooses the
	/// named context-menu action.
	void gesture(qt::workbench_window& aWindow, const std::vector<std::string_view>& aCommits, const QString& aAction)
	{
		auto* const commits   = tab(aWindow);
		auto        selection = QItemSelection{ };
		for (const auto hex : aCommits) {
			const auto row = commits->model()->row_of(std::string{ hex });
			expect(row >= 0, "the commit is listed");
			selection.select(commits->model()->index(row), commits->model()->index(row));
		}
		commits->list()->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
		auto        menu   = std::unique_ptr<QMenu>{ commits->make_menu(nullptr) };
		auto* const action = menu->findChild<QAction*>(aAction);
		expect(action != nullptr && action->isEnabled(), "the context menu offers the action");
		action->trigger();
	}

	void select(qt::workbench_window& aWindow, const std::string_view aPath)
	{
		const auto index = aWindow.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath));
		expect(index.isValid(), "the file is in the filtered file list");
		aWindow.files_drawer()->contents_view()->setCurrentIndex(index);
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!shown(aWindow, aPath) && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!shown(aWindow, aPath)) {
			const auto* surface = aWindow.comparison_surface();
			std::cout << "select:" << aPath << " context=" << (surface->current_context() ? 1 : 0) << " result=" << (surface->current_result() ? 1 : 0)
					  << " same-file=" << (surface->current_context() && surface->current_context()->Key.File == core::file_identity_of(aPath) ? 1 : 0)
					  << " aligned=" << (surface->current_result() && surface->current_context() && surface->current_result()->Alignment.Key == surface->current_context()->Key ? 1 : 0)
					  << " geometry-pending=" << (surface->geometry_pending() ? 1 : 0) << " current-row=" << aWindow.files_drawer()->contents_view()->currentIndex().row() << " row=" << index.row() << '\n';
		}
		expect(shown(aWindow, aPath), "the file's diff is shown");
	}

	/// The paths Next file walks from the first, in order.
	[[nodiscard]] auto walk(qt::workbench_window& aWindow) -> std::vector<std::string>
	{
		auto paths = std::vector<std::string>{ };
		for (auto row = 0; row < drawer_files(aWindow); ++row) {
			paths.push_back(aWindow.files_drawer()->contents()->data(aWindow.files_drawer()->contents()->index(row, 0), Qt::DisplayRole).toString().toStdString());
		}
		return paths;
	}

	void filters_follow_the_commits()
	{
		auto  run    = course{ };
		auto& window = start(run);

		// AC01: the Commits tab sits beside Status and lists C1 to C3.
		auto* const drawer = window.overview_page()->status_drawer();
		expect(drawer->tabs()->count() == 2 && drawer->tabs()->tabText(1) == QStringLiteral("Commits"), "Commits sits beside Status");
		drawer->tabs()->setCurrentIndex(1);
		wait_for([&] {
			const auto* rows = tab(window)->model();
			return rows->rowCount() == 3 && rows->row_at(1)->Files == std::optional<std::size_t>{ 96U };
		},
				 "the Commits tab lists three commits and, once their trees are read, their files");
		const auto* rows = tab(window)->model();
		expect(rows->row_at(0)->Headline == "Change the generator input" && rows->row_at(0)->Commit == c1 && rows->row_at(0)->ShortCommit == "14b75d4"
				   && rows->row_at(0)->Author == "fixture-author" && rows->row_at(0)->Files == std::optional<std::size_t>{ 2U }
				   && rows->row_at(2)->Headline == "Follow-ups" && rows->row_at(2)->Files == std::optional<std::size_t>{ 3U },
			   "each row has its headline, author, short ID and file count");
		expect(!window.overview_page()->commit_chip()->isVisible(), "no filter, no chip");

		// Open on GitHub names the commit's page within the pull request.
		gesture(window, { c1 }, QStringLiteral("commitFilterOpen"));
		expect(access::browser_last_address(*run.Application) == "https://github.com/bpr-fixtures/commit-filter/pull/89/commits/" + std::string{ c1 },
			   "Open on GitHub opens the commit's page in the pull request");

		// AC01: Show only C1 covers exactly C1's paths.
		gesture(window, { c1 }, QStringLiteral("commitFilterShowOnly"));
		wait_for([&] { return overview_files(window) == 2U && drawer_files(window) == 2; }, "Show only C1 leaves exactly C1's two files");
		expect(window.overview_page()->commit_chip()->isVisible() && window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("1 of 3 commits shown")
				   && window.overview_page()->commit_chip()->detail()->text() == QStringLiteral("2 of 98 files, filtered by commits"),
			   "the Overview's chip names the filter and the filtered count");
		enter_review(run);
		expect(window.files_commit_chip()->isVisible() && window.files_commit_chip()->summary()->text() == QStringLiteral("1 of 3 commits shown"),
			   "the Files drawer's chip names the filter in Review");
		expect(walk(window) == std::vector<std::string>{ "src/app.cpp", "src/generator.in" }, "the Files drawer and Next file walk exactly C1's paths");
		select(window, "src/app.cpp");
		{
			const auto& result  = *window.comparison_surface()->current_result();
			const auto& context = *window.comparison_surface()->current_context();
			expect(context.Key.Filter == std::optional{ core::filtered_diff_endpoints{ .Left = commit(merge_base), .Right = commit(c1) } },
				   "src/app.cpp's diff is between C1's parent and C1");
			expect(changed(result, core::diff_side::left) == lines{ { 5U, "app line 5" } } && changed(result, core::diff_side::right) == lines{ { 5U, "app line 5 C1" } },
				   "and is exactly git diff C1^ C1: C3's change to line 40 is not in it");
		}
		select(window, "src/generator.in");
		{
			const auto& result  = *window.comparison_surface()->current_result();
			const auto& context = *window.comparison_surface()->current_context();
			expect(!context.Key.Filter && changed(result, core::diff_side::right) == lines{ { 1U, "value = 2" } },
				   "a file only C1 changed keeps the comparison's own diff, which is C1's change");
		}
		window.next_file_action()->trigger();
		wait_for([&] { return shown(window, "src/app.cpp") || shown(window, "src/generator.in"); }, "Next file stays in the filtered set");

		// The chip in Review opens the Commits tab.
		window.files_commit_chip()->commits_button()->click();
		wait_for([&] { return window.showing_overview() && drawer->tabs()->currentIndex() == 1 && tab(window)->isVisible(); }, "the chip opens the Commits tab");

		// AC01: Clear all filters restores Whole PR exactly.
		window.overview_page()->commit_chip()->clear_button()->click();
		wait_for([&] { return overview_files(window) == whole && drawer_files(window) == static_cast<int>(whole); }, "Clear all filters restores the 98 files");
		expect(!window.overview_page()->commit_chip()->isVisible(), "and the chip goes");

		// AC02: Hide C2 keeps every file C2 never touched exactly as Whole PR has it.
		gesture(window, { c2 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return overview_files(window) == 4U && drawer_files(window) == 4; }, "Hide C2 leaves the source and the two hand-edited files");
		expect(window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown"), "the chip reads 2 of 3 commits shown");
		enter_review(run);
		expect(walk(window) == std::vector<std::string>{ "gen/item_007.txt", "gen/item_008.txt", "src/app.cpp", "src/generator.in" },
			   "the walk keeps GitHub's order");
		select(window, "src/app.cpp");
		{
			const auto& result  = *window.comparison_surface()->current_result();
			const auto& context = *window.comparison_surface()->current_context();
			expect(!context.Key.Filter && changed(result, core::diff_side::right) == lines{ { 5U, "app line 5 C1" }, { 40U, "app line 40 C3" } },
				   "a file C2 never touched is Whole PR's diff, with the same material and receipt");
		}
		select(window, "gen/item_007.txt");
		{
			const auto& result  = *window.comparison_surface()->current_result();
			const auto& context = *window.comparison_surface()->current_context();
			expect(context.Key.Filter == std::optional{ core::filtered_diff_endpoints{ .Left = commit(c2), .Right = commit(c3) } }
					   && changed(result, core::diff_side::left) == lines{ { 2U, "item 7" } }
					   && changed(result, core::diff_side::right) == lines{ { 2U, "item 7, adjusted by hand" } },
				   "a generated file C3 edited shows only C3's hand edit, between C2 and C3");
		}

		// AC02: combining gestures yields the stated shown set at each step.
		window.show_commits();
		gesture(window, { c1, c3 }, QStringLiteral("commitFilterShowOnly"));
		wait_for([&] { return window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown") && overview_files(window) == 4U; },
				 "Show only {C1, C3}");
		gesture(window, { c3 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("1 of 3 commits shown") && overview_files(window) == 2U; },
				 "then Hide C3");
		gesture(window, { c3 }, QStringLiteral("commitFilterShow"));
		wait_for([&] { return window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown") && overview_files(window) == 4U; },
				 "then Show selected C3");
		expect(access::held_review_mutation_terminals(*run.Application) == 0U, "no filter gesture asks GitHub for anything (AC05)");
		window.overview_page()->commit_chip()->clear_button()->click();
		wait_for([&] { return overview_files(window) == whole; }, "Clear all filters again");
		std::cout << "commit-filter course files=" << overview_files(window) << '\n';
		finish(run);
	}
	/// The reviewer opens a file's context menu in the Overview and chooses the
	/// generated gesture named by its text.
	void overview_gesture(qt::workbench_window& aWindow, const std::string_view aPath, const QString& aText)
	{
		auto* const page   = aWindow.overview_page();
		auto        menu   = std::unique_ptr<QMenu>{ };
		auto*       chosen = static_cast<QAction*>(nullptr);
		// The file's menu follows the review's marks once the ledger has read them.
		wait_for([&] {
			const auto index = page->file_model()->index_for_path(QString::fromUtf8(aPath.data(), static_cast<qsizetype>(aPath.size())));
			menu.reset(index.isValid() ? page->make_file_menu(index) : nullptr);
			chosen = nullptr;
			for (auto* const action : menu ? menu->actions() : QList<QAction*>{ }) {
				chosen = action->text() == aText && action->isEnabled() ? action : chosen;
			}
			return chosen != nullptr;
		},
				 "the file's Overview menu offers the gesture");
		chosen->trigger();
	}

	/// The reviewer opens the generated row and chooses a gesture on one file.
	void generated_row_gesture(qt::generated_files_row& aRow, const std::string_view aPath, const QString& aText)
	{
		auto row = -1;
		for (auto index = 0; index < static_cast<int>(aRow.rows().size()); ++index) {
			row = aRow.rows()[static_cast<std::size_t>(index)].Path == aPath ? index : row;
		}
		expect(row >= 0, "the generated row lists the file");
		auto  menu   = std::unique_ptr<QMenu>{ aRow.make_menu(row, nullptr) };
		auto* chosen = static_cast<QAction*>(nullptr);
		for (auto* const action : menu ? menu->actions() : QList<QAction*>{ }) {
			chosen = action->text() == aText ? action : chosen;
		}
		expect(chosen != nullptr && chosen->isEnabled(), "the generated row offers the gesture");
		chosen->trigger();
	}

	void read_commit_trees(qt::workbench_window& aWindow, const std::size_t aRegenerated = 96U)
	{
		auto* const drawer = aWindow.overview_page()->status_drawer();
		drawer->tabs()->setCurrentIndex(1);
		wait_for([&] {
			const auto* rows = tab(aWindow)->model();
			return rows->rowCount() == 3 && rows->row_at(1)->Files == std::optional<std::size_t>{ aRegenerated };
		},
				 "the Commits tab reads the chain's trees");
	}

	void generated_files_and_file_filters()
	{
		auto  run    = course{ };
		auto* window = &start(run);
		read_commit_trees(*window);

		// AC04: without attributes nothing is set aside; the live course has a bulk C2.
		auto* overviewRow = window->overview_page()->generated_row();
		expect(overview_files(*window) == whole && overviewRow->isHidden(), "nothing is hidden before a rule or gesture says so");
		expect(!tab(*window)->model()->row_at(1)->Bulk && !tab(*window)->model()->row_at(0)->Bulk && !tab(*window)->model()->row_at(2)->Bulk,
			   "C2 regenerates 96 files, under the 100 the bulk label needs, so no row is labelled");

		// AC04: Hide files these commits changed on C2 hides C2's paths in C3 as well.
		gesture(*window, { c2 }, QStringLiteral("commitFilterHideFiles"));
		wait_for([&] { return overview_files(*window) == 2U && drawer_files(*window) == 2; }, "Hide files C2 changed leaves the two source files");
		expect(window->overview_page()->commit_chip()->isVisible() && window->overview_page()->commit_chip()->summary()->text() == QStringLiteral("File filter on")
				   && window->overview_page()->commit_chip()->detail()->text() == QStringLiteral("2 of 98 files, 96 hidden by file filters"),
			   "the chip names the file filter and how many files it hides");
		expect(window->overview_page()->files_heading()->text().contains(QStringLiteral("96 hidden by file filters")), "the Overview's count says why");
		enter_review(run);
		expect(walk(*window) == std::vector<std::string>{ "src/app.cpp", "src/generator.in" },
			   "Next file walks neither the regenerated files nor the two C3 edited by hand");
		window->overview_page()->commit_chip()->clear_button()->click();
		wait_for([&] { return overview_files(*window) == whole && drawer_files(*window) == static_cast<int>(whole); }, "Clear all filters removes the file filter");

		// AC04: Treat as generated sets a file aside behind its row, in the Overview and the Files drawer.
		window->show_commits();
		overview_gesture(*window, "src/generator.in", QStringLiteral("Treat as generated"));
		wait_for([&] { return overview_files(*window) == whole - 1U && drawer_files(*window) == static_cast<int>(whole) - 1; },
				 "Treat as generated takes the file out of the trees, the walk and the counts");
		expect(!overviewRow->isHidden() && overviewRow->toggle()->text() == QStringLiteral("1 generated file hidden"), "the Overview's tree ends with the generated row");
		expect(window->overview_page()->files_heading()->text().contains(QStringLiteral("1 generated set aside")), "the Overview's count says it is set aside");
		expect(!window->overview_page()->commit_chip()->isVisible(), "a generated file is not a filter: no chip");
		enter_review(run);
		auto* drawerRow = window->files_generated_row();
		expect(drawerRow != nullptr && drawerRow->rows().size() == 1U && drawerRow->rows().front().Path == "src/generator.in",
			   "the Files drawer's tree ends with the same row");
		const auto walked = walk(*window);
		expect(std::ranges::find(walked, std::string{ "src/generator.in" }) == walked.end(), "Next file does not walk it");

		// AC04: Clear all filters leaves generated files behind their row (owner, Q3).
		gesture(*window, { c3 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return !window->overview_page()->commit_chip()->isHidden(); }, "a commit filter");
		window->overview_page()->commit_chip()->clear_button()->click();
		wait_for([&] { return window->overview_page()->commit_chip()->isHidden() && overview_files(*window) == whole - 1U; },
				 "Clear all filters keeps the generated file aside");

		// AC04: the mark persists through the review ledger.
		restart(run);
		window = &start(run);
		wait_for([&] { return overview_files(*window) == whole - 1U; }, "after a restart the file is still treated as generated");
		overviewRow = window->overview_page()->generated_row();
		expect(!overviewRow->isHidden() && overviewRow->rows().size() == 1U && overviewRow->rows().front().Reason == core::generated_reason::marked_generated,
			   "and the generated row says why");

		// AC04: Review anyway returns it, still generated, and persists too.
		generated_row_gesture(*overviewRow, "src/generator.in", QStringLiteral("Review anyway"));
		wait_for([&] { return overview_files(*window) == whole && overviewRow->isHidden(); }, "Review anyway returns the file to the review");
		restart(run);
		window = &start(run);
		wait_for([&] { return overview_files(*window) == whole && window->overview_page()->generated_row()->isHidden(); }, "after a restart it is still reviewed");
		overview_gesture(*window, "src/generator.in", QStringLiteral("Set aside again"));
		wait_for([&] { return overview_files(*window) == whole - 1U; }, "Set aside again puts it back behind the row");

		// AC04: Follow .gitattributes forgets the mark: with none here, the file is ordinary.
		generated_row_gesture(*window->overview_page()->generated_row(), "src/generator.in", QStringLiteral("Follow .gitattributes"));
		wait_for([&] { return overview_files(*window) == whole && window->overview_page()->generated_row()->isHidden(); }, "Follow .gitattributes returns it");

		// Show as normal is offered for a file that is generated.
		overview_gesture(*window, "src/app.cpp", QStringLiteral("Treat as generated"));
		wait_for([&] { return overview_files(*window) == whole - 1U; }, "Treat as generated again");
		generated_row_gesture(*window->overview_page()->generated_row(), "src/app.cpp", QStringLiteral("Show as normal"));
		wait_for([&] { return overview_files(*window) == whole; }, "Show as normal returns it");

		expect(access::held_review_mutation_terminals(*run.Application) == 0U, "no generated or file-filter gesture asks GitHub for anything (AC05)");
		std::cout << "generated-files course files=" << overview_files(*window) << '\n';
		finish(run);
	}

	[[nodiscard]] auto in_overview_tree(qt::workbench_window& aWindow, const std::string_view aPath) -> bool
	{
		return aWindow.overview_page()->file_model()->index_for_path(QString::fromUtf8(aPath.data(), static_cast<qsizetype>(aPath.size()))).isValid();
	}

	[[nodiscard]] auto blob(const std::string_view aHex) -> core::git_blob_oid
	{
		return *core::parse_git_blob_oid(aHex);
	}

	/// \brief [[BPR-1692]]. commit-filter-attributes-v1: the head's root
	/// `.gitattributes` marks `gen/**` and a nested one returns
	/// `gen/item_007.txt` with `-linguist-generated` (AC04's attribute half).
	void linguist_generated_at_head()
	{
		constexpr auto attributesC1 = std::string_view{ "8e2284552315a346e5bb6515e68f0c8be4c0db5d" };
		constexpr auto attributesC3 = std::string_view{ "080975e555a2c41ca8d26e7becf1c1d8a997f145" };
		constexpr auto marked       = std::size_t{ 95U };
		constexpr auto ordinary     = whole - marked;
		auto           run          = course{ };
		auto*          window       = &start(run, { .Fixture = app::detail::github_public_path_fixture::commit_filter_attributes_v1, .Address = QStringLiteral("bpr-fixtures/commit-filter-attributes#1692"), .Files = ordinary });

		// Whole PR: the head's rules set 95 files aside; -linguist-generated returns one.
		auto* overviewRow = window->overview_page()->generated_row();
		expect(!overviewRow->isHidden() && overviewRow->rows().size() == marked && overviewRow->toggle()->text() == QStringLiteral("95 generated files hidden"),
			   "the Overview's tree ends with the 95 files the head's linguist-generated marks");
		expect(std::ranges::all_of(overviewRow->rows(), [](const auto& aRow) { return aRow.Reason == core::generated_reason::attribute_set; }),
			   "each is set aside because .gitattributes marks it");
		expect(in_overview_tree(*window, "gen/item_007.txt") && in_overview_tree(*window, "src/app.cpp") && !in_overview_tree(*window, "gen/item_000.txt")
				   && !in_overview_tree(*window, "gen/item_008.txt"),
			   "gen/item_007.txt is back in the tree; the regenerated files, even one C3 edited, are not");
		expect(window->overview_page()->files_heading()->text().contains(QStringLiteral("Files: 3 of 98 in this pull request"))
				   && window->overview_page()->files_heading()->text().contains(QStringLiteral("95 generated set aside"))
				   && !window->overview_page()->files_heading()->text().contains(QStringLiteral("linguist-generated")),
			   "the Overview's count says they are set aside, and nothing says the attributes were not read");
		expect(!window->overview_page()->commit_chip()->isVisible(), "set-aside files are not a filter: no chip");
		enter_review(run);
		auto walked = walk(*window);
		std::ranges::sort(walked);
		expect(walked == std::vector<std::string>{ "gen/item_007.txt", "src/app.cpp", "src/generator.in" }, "Next file and the counts cover the three ordinary files");
		auto* drawerRow = window->files_generated_row();
		expect(drawerRow != nullptr && drawerRow->rows().size() == marked, "the Files drawer's tree ends with the same row");
		// Opening a file starts a preparation round over the files around it.
		const auto rounds = access::diff_work_evidence(*run.Application).PreparationRounds;
		select(*window, "gen/item_007.txt");
		wait_for([&] {
			const auto work = access::diff_work_evidence(*run.Application);
			return work.PreparationRounds > rounds && work.Preparing == 0U && work.PreparationsInFlight == 0U && work.EpochOutstandingJobs == 0U;
		},
				 "the preparation round settles");
		expect(access::diff_blob_reads(*run.Application, blob("85ff5e81d7a1adb4de252446bfabeaa1211d2718")) != 0U,
			   "an ordinary file is prepared: src/generator.in's head blob is read");
		expect(access::diff_blob_reads(*run.Application, blob("cbaab37374a1a5502c81ac45aaad3cc68f4a75ec")) == 0U
				   && access::diff_blob_reads(*run.Application, blob("4764315f2893b5a3440fca43b5f7603f16197202")) == 0U,
			   "set-aside files are not prepared: their head blobs are never read");

		// [[BPR-1693]] AC1: Open diff on a file in the Files drawer's generated row
		// shows its diff, read on request, and leaves it set aside.
		const auto walkedBefore = walk(*window);
		const auto openDiff     = [](qt::generated_files_row& aRow, const std::string_view aPath) {
			auto row = -1;
			for (auto index = 0; index < static_cast<int>(aRow.rows().size()); ++index) {
				row = aRow.rows()[static_cast<std::size_t>(index)].Path == aPath ? index : row;
			}
			expect(row >= 0, "the generated row lists the file");
			auto  menu   = std::unique_ptr<QMenu>{ aRow.make_menu(row, nullptr) };
			auto* chosen = menu ? menu->findChild<QAction*>(QStringLiteral("generatedFilesOpen")) : nullptr;
			expect(chosen != nullptr && chosen->isEnabled() && menu->actions().front() == chosen, "the generated row's menu offers Open diff first");
			chosen->trigger();
		};
		openDiff(*drawerRow, "gen/item_000.txt");
		wait_for([&] { return shown(*window, "gen/item_000.txt"); }, "Open diff shows the set-aside file's diff");
		{
			const auto& result  = *window->comparison_surface()->current_result();
			const auto& context = *window->comparison_surface()->current_context();
			expect(!context.Key.Filter && changed(result, core::diff_side::left) == lines{ { 1U, "generated from value 1" }, { 3U, "checksum 1000" } }
					   && changed(result, core::diff_side::right) == lines{ { 1U, "generated from value 2" }, { 3U, "checksum 2000" } },
				   "it is the file's own Whole PR diff, read on request");
		}
		const auto stillAside = [&](const std::string_view aPath) {
			const auto& rows = window->files_generated_row()->rows();
			const auto  row  = std::ranges::find(rows, aPath, &application::generated_file_row::Path);
			return rows.size() == marked && row != rows.end() && row->Reason == core::generated_reason::attribute_set && !row->ReviewAnyway;
		};
		expect(walk(*window) == walkedBefore && drawer_files(*window) == 3 && overview_files(*window) == ordinary,
			   "it is not added to Next file or the counts");
		expect(stillAside("gen/item_000.txt") && window->overview_page()->generated_row()->rows().size() == marked,
			   "no mark is written: it is still set aside because .gitattributes marks it");
		// AC2: leaving it returns the review to its set.
		window->next_file_action()->trigger();
		wait_for([&] { return std::ranges::any_of(walkedBefore, [&](const std::string& aPath) { return shown(*window, aPath); }); },
				 "Next file from it walks the review's own files");
		expect(stillAside("gen/item_000.txt") && walk(*window) == walkedBefore, "and the file is still set aside");
		// AC1 from the Overview: activating a listed file enters Review on its diff.
		window->show_commits();
		wait_for([&] { return window->showing_overview(); }, "back on the Overview");
		auto* const overviewList = window->overview_page()->generated_row()->list();
		auto*       item         = static_cast<QTreeWidgetItem*>(nullptr);
		for (auto index = 0; index < overviewList->topLevelItemCount(); ++index) {
			item = overviewList->topLevelItem(index)->data(0, Qt::UserRole).toString() == QStringLiteral("gen/item_001.txt") ? overviewList->topLevelItem(index) : item;
		}
		expect(item != nullptr, "the Overview's generated row lists gen/item_001.txt");
		emit overviewList->itemActivated(item, 0);
		wait_for([&] { return !window->showing_overview() && shown(*window, "gen/item_001.txt"); }, "activating it in the Overview enters Review on its diff");
		expect(stillAside("gen/item_001.txt") && walk(*window) == walkedBefore && overview_files(*window) == ordinary, "and it stays set aside");

		// Every filter: Hide C1 leaves C2 and C3, and still sets the 95 aside.
		window->show_commits();
		read_commit_trees(*window);
		gesture(*window, { attributesC1 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return overview_files(*window) == 2U && drawer_files(*window) == 2; }, "Hide C1 leaves gen/item_007.txt and src/app.cpp");
		expect(window->overview_page()->generated_row()->rows().size() == marked, "under a commit filter generated files stay behind their row");
		gesture(*window, { attributesC3 }, QStringLiteral("commitFilterShowOnly"));
		wait_for([&] { return overview_files(*window) == 2U && in_overview_tree(*window, "gen/item_007.txt") && !in_overview_tree(*window, "gen/item_008.txt"); },
				 "Show only C3 lists its two ordinary files; its hand edit of a generated file stays aside");
		window->overview_page()->commit_chip()->clear_button()->click();
		wait_for([&] { return window->overview_page()->commit_chip()->isHidden() && overview_files(*window) == ordinary; }, "Clear all filters");
		expect(window->overview_page()->generated_row()->rows().size() == marked, "Clear all filters leaves generated files behind their row (owner, Q3)");

		// A mark still outranks the attributes.
		generated_row_gesture(*window->overview_page()->generated_row(), "gen/item_000.txt", QStringLiteral("Show as normal"));
		wait_for([&] { return overview_files(*window) == ordinary + 1U && in_overview_tree(*window, "gen/item_000.txt"); }, "Show as normal outranks linguist-generated");
		overview_gesture(*window, "gen/item_007.txt", QStringLiteral("Treat as generated"));
		wait_for([&] { return overview_files(*window) == ordinary && !in_overview_tree(*window, "gen/item_007.txt"); }, "Treat as generated outranks -linguist-generated");

		expect(access::held_review_mutation_terminals(*run.Application) == 0U, "reading .gitattributes asks GitHub for nothing (AC05)");
		std::cout << "linguist-generated course files=" << overview_files(*window) << " set-aside=" << window->overview_page()->generated_row()->rows().size() << '\n';
		finish(run);
	}
	// RFC-BPR-0089 M3. commit-filter-split-v1 (`scripts/build_commit_filter_fixture.py --split`).
	constexpr auto split_base    = std::string_view{ "89a0ac4b6ba40b04017fdb11affbcd596ee5d851" };
	constexpr auto split_c1      = std::string_view{ "91bd0b26750f6a959d63131fbcb86826c744dfef" };
	constexpr auto split_c2      = std::string_view{ "798cde72705b0141fd70aa32b654e9a62294a2d8" };
	constexpr auto split_c3      = std::string_view{ "968f89235fe4fe932ddab7f89563632b5de7cc7a" };
	constexpr auto split_c3r     = std::string_view{ "cfaecb6889ad2da4edeb6b720834fd052c875bc1" };
	constexpr auto split_whole   = std::size_t{ 99U };
	constexpr auto split_version = std::string_view{ "src/version.txt" };

	[[nodiscard]] auto content_blob(const std::string_view aText) -> core::git_blob_oid
	{
		const auto bytes = std::span{ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		return *core::git_blob_identity(bytes, core::git_hash_algorithm::sha1);
	}

	[[nodiscard]] auto split_opening(const app::detail::github_epoch_fixture_phase aPhase = app::detail::github_epoch_fixture_phase::single) -> opening
	{
		return { .Fixture = app::detail::github_public_path_fixture::commit_filter_split_v1,
				 .Address = QStringLiteral("bpr-fixtures/commit-filter-split#893"),
				 .Files   = split_whole,
				 .Phase   = aPhase };
	}

	[[nodiscard]] auto overview_read(const qt::workbench_window& aWindow, const std::string_view aPath) -> std::optional<core::file_read_state>
	{
		const auto& projection = aWindow.overview_page()->projection();
		if (!projection) {
			return std::nullopt;
		}
		const auto row = std::ranges::find(projection->Files, std::string{ aPath }, &application::overview_file_row::Path);
		return row != projection->Files.end() ? row->Read : std::nullopt;
	}

	/// \brief RFC-BPR-0089 M3, AC03 (owner, Q2): a file a hidden commit changed
	/// between two shown ones keeps one row and reads each shown commit's own
	/// diff, with its own Seen and Read, the hidden commit named between.
	void split_files_read_per_commit()
	{
		auto  run    = course{ };
		auto& window = start(run, split_opening());
		read_commit_trees(window, 97U);

		// Hide C2: the source, the two hand-edited files and src/version.txt.
		gesture(window, { split_c2 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return overview_files(window) == 5U; }, "Hide C2 leaves five files");
		expect(tab(window)->status()->text().contains(QStringLiteral("1 file has a hidden commit between two shown ones.")), "the Commits tab says one file is split");
		enter_review(run);
		const auto walked = walk(window);
		expect(std::ranges::count(walked, std::string{ split_version }) == 1, "the split file keeps one row in the Files drawer and the walk");

		// Its first diff is C1's own change: the path at C1's parent against C1.
		const auto versionAt = [](const std::string_view aStage) -> std::string_view {
			return aStage == "M" ? "version 1\nstamp none\n" : aStage == "C1" ? "version 2\nstamp none\n"
				: aStage == "C2"                                              ? "version 2\nstamp regenerated\n"
																			  : "version 3\nstamp regenerated\n";
		};
		const auto chainBlobs = std::vector{ content_blob(versionAt("M")), content_blob(versionAt("C1")), content_blob(versionAt("C2")), content_blob(versionAt("C3")) };
		const auto inChain    = [&](const std::optional<core::git_blob_oid>& aBlob) { return aBlob && std::ranges::find(chainBlobs, *aBlob) != chainBlobs.end(); };
		select(window, split_version);
		auto* const strip = window.split_strip();
		wait_for([&] { return strip->isVisibleTo(&window) && strip->parts().size() == 2U && strip->parts()[0]->isChecked(); }, "a strip names the file's two diffs, the first on screen");
		expect(strip->parts()[0]->accessibleName().startsWith(QStringLiteral("Diff 1 of 2: commit 91bd0b2, Change the generator input"))
				   && strip->parts()[1]->accessibleName().startsWith(QStringLiteral("Diff 2 of 2: commit 968f892, Follow-ups")) && strip->parts()[0]->text().startsWith(QStringLiteral("91bd0b2"))
				   && strip->hidden().size() == 1U && strip->hidden().front()->accessibleName() == QStringLiteral("798cde7 Regenerate also changed this file; hidden")
				   && strip->hidden().front()->text().startsWith(QStringLiteral("798cde7")),
			   "each shown commit's diff is named in chain order, the hidden commit between them");
		expect(strip->sizeHint().width() <= strip->width() + 1 && strip->width() < 800, "in the window's width the strip's wording fits, shortened");
		{
			const auto& result  = *window.comparison_surface()->current_result();
			const auto& context = *window.comparison_surface()->current_context();
			expect(context.Key.Filter == std::optional{ core::filtered_diff_endpoints{ .Left = commit(split_base), .Right = commit(split_c1) } }
					   && context.Key.LeftBlob == std::optional{ content_blob(versionAt("M")) } && context.Key.RightBlob == std::optional{ content_blob(versionAt("C1")) },
				   "the first diff is git diff C1^:P C1:P, blob for blob");
			expect(changed(result, core::diff_side::left) == lines{ { 1U, "version 1" } } && changed(result, core::diff_side::right) == lines{ { 1U, "version 2" } },
				   "and line for line: C1's version bump only");
			expect(inChain(context.Key.LeftBlob) && inChain(context.Key.RightBlob), "both shown blobs are the file at a commit of the chain");
		}
		// Seen is this diff's own: the second is not seen yet.
		wait_for([&] { return strip->parts()[0]->text().endsWith(QStringLiteral("(seen)")) && !strip->parts()[1]->text().endsWith(QStringLiteral(")")); },
				 "presenting the first diff marks it seen, and only it");

		// The second diff is C3's own change, from C2.
		strip->parts()[1]->click();
		wait_for([&] {
			const auto& context = window.comparison_surface()->current_context();
			return shown(window, split_version) && context->Key.Filter == std::optional{ core::filtered_diff_endpoints{ .Left = commit(split_c2), .Right = commit(split_c3) } };
		},
				 "choosing the second diff reads C2..C3");
		{
			const auto& result  = *window.comparison_surface()->current_result();
			const auto& context = *window.comparison_surface()->current_context();
			expect(context.Key.LeftBlob == std::optional{ content_blob(versionAt("C2")) } && context.Key.RightBlob == std::optional{ content_blob(versionAt("C3")) }
					   && changed(result, core::diff_side::left) == lines{ { 1U, "version 2" } } && changed(result, core::diff_side::right) == lines{ { 1U, "version 3" } },
				   "it is git diff C3^:P C3:P: C2's stamp is in neither side's change");
			expect(inChain(context.Key.LeftBlob) && inChain(context.Key.RightBlob), "both shown blobs are the file at a commit of the chain");
			// Negative control: the tree a cherry-pick of C1 and C3 would make holds a blob no commit has.
			const auto synthesized = content_blob("version 3\nstamp none\n");
			expect(!inChain(synthesized) && context.Key.RightBlob != std::optional{ synthesized } && context.Key.LeftBlob != std::optional{ synthesized },
				   "a synthesized tree's blob is in no commit and would fail the check");
		}
		wait_for([&] { return strip->parts()[1]->isChecked() && !strip->parts()[0]->isChecked(); }, "the strip follows the diff on screen");
		if (const auto directory = qEnvironmentVariable("BPR_RFC0089_RENDER_DIR"); !directory.isEmpty()) {
			// The full window with the strip over the split file's second diff.
			window.grab().save(directory + QStringLiteral("/split-window.png"));
		}

		// Read is each diff's own; the row is read once both are.
		window.read_action()->trigger();
		wait_for([&] { return strip->parts()[1]->text().endsWith(QStringLiteral("(read)")) && strip->parts()[0]->text().endsWith(QStringLiteral("(seen)")); },
				 "Mark read on the second diff reads only it");
		expect(overview_read(window, split_version) != std::optional{ core::file_read_state::read }, "the file's row is not read while one diff is only seen");
		strip->parts()[0]->click();
		wait_for([&] { return shown(window, split_version) && strip->parts()[0]->isChecked(); }, "back on the first diff");
		window.read_action()->trigger();
		wait_for([&] { return strip->parts()[0]->text().endsWith(QStringLiteral("(read)")) && overview_read(window, split_version) == std::optional{ core::file_read_state::read }; },
				 "once both diffs are read the file's row is read");

		// Next file leaves it; Clear all filters gives it back its Whole PR diff, with no strip.
		window.overview_page()->commit_chip()->clear_button()->click();
		wait_for([&] { return overview_files(window) == split_whole; }, "Clear all filters restores the 99 files");
		enter_review(run);
		select(window, split_version);
		expect(!window.comparison_surface()->current_context()->Key.Filter && strip->isHidden(), "Whole PR reads the file's own diff, with no strip");
		expect(overview_read(window, split_version) != std::optional{ core::file_read_state::read }, "the per-commit receipts are not Whole PR's");
		expect(access::held_review_mutation_terminals(*run.Application) == 0U, "no split read asks GitHub for anything (AC05)");
		std::cout << "split course files=" << overview_files(window) << '\n';
		finish(run);
	}

	/// \brief RFC-BPR-0089 M3, AC06: a force push rewrites hidden C3; the chip
	/// names it, offers the commit carrying the same change and applies it
	/// only when the reviewer chooses it.
	constexpr auto comment_merge_base = std::string_view{ "1d90b2de2c5fd1c5346d91ced16753907c817a17" };
	constexpr auto comment_c1         = std::string_view{ "4613350da8a593d86394181b968e936261d0741b" };
	constexpr auto comment_c2         = std::string_view{ "3a82b6dc4e11b73326016b009f4558a9b83089a1" };
	constexpr auto comment_c3         = std::string_view{ "83be2c73bff7a90d91e2b094ef092594e51a1cbe" };

	[[nodiscard]] auto marked(const qt::workbench_window& aWindow, const core::diff_side aSide, const std::uint32_t aLine) -> bool
	{
		const auto& marks = aWindow.comparison_surface()->line_marks(aSide);
		return std::ranges::any_of(marks, [aLine](const qt::diff_line_mark& aMark) { return aMark.Line == aLine && !aMark.Draft; });
	}

	/// The reviewer selects a line, writes a comment on it and reads the
	/// words above Publish; the editor's own Publish then sends it.
	[[nodiscard]] auto write_on(qt::workbench_window& aWindow, const core::diff_side aSide, const std::uint32_t aLine, const std::string& aBody) -> qt::review_draft_editor&
	{
		auto* const editor = aWindow.review_editor();
		expect(aWindow.comparison_surface()->select_conversation_line(aSide, aLine), "the line can be selected");
		wait_for([&] {
			const auto& draft = editor->shown_draft();
			return draft && draft->Anchor.Line == aLine && draft->Anchor.Side == aSide && draft->State == core::review_draft_state::drafting;
		},
				 "a draft is offered on the selected line");
		wait_for([&] { return !editor->target_notice_text().startsWith(QStringLiteral("Working out")); }, "the host says where a comment on the line goes");
		if (aWindow.comment_action()->isEnabled()) {
			aWindow.comment_action()->trigger();
			editor->body_editor()->setPlainText(QString::fromStdString(aBody));
			wait_for([&] { return editor->shown_draft() && editor->shown_draft()->Body == aBody && !editor->rejected_candidate(); }, "the comment's text is accepted");
		}
		return *editor;
	}

	/// Every comment written so far, line or file.
	[[nodiscard]] auto posts(const course& aRun) -> std::size_t
	{
		const auto evidence = access::review_mutation_evidence(*aRun.Application);
		return evidence.NativePosts + evidence.FilePosts;
	}

	void publish_and_wait(course& aRun, qt::review_draft_editor& aEditor, const std::size_t aPosts)
	{
		const auto before = access::window_read_progress(*aRun.Application).Snapshot;
		expect(aEditor.publish_action()->isEnabled(), "Publish is offered");
		aEditor.publish_action()->trigger();
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (posts(aRun) != aPosts && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (posts(aRun) != aPosts) {
			const auto evidence = access::review_mutation_evidence(*aRun.Application);
			std::cout << "commit-filter-comment:unsent status=\"" << aEditor.publication_status().toStdString() << "\" mutation=\"" << aEditor.mutation_status_text().toStdString()
					  << "\" submissions=" << evidence.Submissions << " observations=" << evidence.TargetObservations << " posts=" << evidence.NativePosts
					  << " state=" << (aEditor.shown_draft() ? static_cast<int>(aEditor.shown_draft()->State) : -1) << '\n';
		}
		wait_for([&] { return posts(aRun) == aPosts; }, "the comment is written once");
		wait_for([&] { return aEditor.publication_status().startsWith(QStringLiteral("Published")) || !aEditor.shown_draft() || aEditor.shown_draft()->shared(); },
				 "the publication reconciles as published");
		// The window then reads the pull request again, as GitHub has it now.
		wait_for([&] {
			const auto progress = access::window_read_progress(*aRun.Application);
			return progress.Snapshot != before && !progress.Reading;
		},
				 "the pull request is read again after the publication");
		auto& window = aRun.Application->window();
		wait_for([&] { return window.files_drawer()->contents()->index_for_file(core::file_identity_of("src/notes.txt")).isValid(); },
				 "the reread review lists src/notes.txt again");
		enter_review(aRun);
		// The reread leaves the workbench with no selected file while the diff
		// stays on screen (as on Whole PR), and the filtered list can be
		// published once more after it; the reviewer chooses the file afresh
		// until it stays chosen.
		const auto settled = [&](const std::string_view aPath) {
			const auto index = window.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath));
			if (!index.isValid()) {
				return false;
			}
			window.files_drawer()->contents_view()->setCurrentIndex(index);
			auto waited = QElapsedTimer{ };
			waited.start();
			while (waited.elapsed() < 10000 && !shown(window, aPath)) {
				QApplication::processEvents();
				QThread::msleep(1);
			}
			waited.restart();
			while (waited.elapsed() < 500) {
				QApplication::processEvents();
				QThread::msleep(1);
			}
			return shown(window, aPath);
		};
		for (auto attempt = 0; attempt < 5; ++attempt) {
			if (settled("src/app.cpp") && settled("src/notes.txt")) {
				return;
			}
		}
		select(window, "src/notes.txt");
	}

	void comments_from_a_filtered_diff()
	{
		auto  run    = course{ };
		auto& window = start(run, { .Fixture = app::detail::github_public_path_fixture::commit_filter_comment_v1, .Address = QStringLiteral("bpr-fixtures/commit-filter-comment#894"), .Files = 99U, .Rereads = true });
		read_commit_trees(window);
		gesture(window, { comment_c3 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown"); }, "Hide C3");
		enter_review(run);
		select(window, "src/notes.txt");
		expect(window.comparison_surface()->current_context()->Key.Filter == std::optional{ core::filtered_diff_endpoints{ .Left = commit(comment_merge_base), .Right = commit(comment_c1) } },
			   "with C3 hidden src/notes.txt's diff is the merge base to C1");

		// Thread markers under the filter: the thread written on C1 at line 25
		// (outdated on GitHub) is marked on C1's diff; the head's is not.
		wait_for([&] { return marked(window, core::diff_side::right, 24U); }, "the thread written on C1 is marked on C1's diff at its line 25");
		expect(!marked(window, core::diff_side::right, 16U), "the head's thread at line 17 stays in the drawer on C1's diff");
		{
			const auto& cards = window.conversation_panel()->cards()->cards();
			const auto  onC1  = std::ranges::find(cards, std::string{ "review-thread:PRRT_m4_on_c1" }, &qt::thread_card::Id);
			const auto  head  = std::ranges::find(cards, std::string{ "review-thread:PRRT_m4_at_head" }, &qt::thread_card::Id);
			expect(onC1 != cards.end() && onC1->Line == std::optional<std::uint32_t>{ 25U } && !onC1->Outdated && head != cards.end() && head->AnchorUnavailable,
				   "on C1's diff the C1 thread is current at line 25 and the head's is not placed");
		}

		// C1's line 15 is unchanged at the head, where it is line 17.
		auto& editor = write_on(window, core::diff_side::right, 14U, "M4 course: unchanged at the head.");
		expect(editor.target_notice_text() == QStringLiteral("Goes on the head at line 17, where this line is now.") && !editor.target_notice_blocks(),
			   "before Publish the editor says the comment goes on the head's line 17");
		publish_and_wait(run, editor, 1U);
		const auto atHead = access::review_mutation_evidence(*run.Application);
		std::cout << "commit-filter-comment:head commit=" << atHead.LastCommit << " line=" << (atHead.LastLine ? *atHead.LastLine : 0U) << " side=" << atHead.LastSide << '\n';
		expect(atHead.LastCommit == comment_c3 && atHead.LastLine == std::optional<std::uint32_t>{ 17U } && atHead.LastSide == "RIGHT" && atHead.EarlierCommitPosts == 0U,
			   "it is published with commit_id = head at line 17");

		// C1's line 10 is unchanged at the head (line 12), but no hunk of
		// GitHub's diff shows it there, and GitHub refused that line live.
		// [[BPR-1706]] AC2: it goes as a file comment linking to C1's line 10.
		auto& outside = write_on(window, core::diff_side::right, 9U, "BPR-1706 course: unchanged and outside GitHub's diff.");
		expect(outside.target_notice_text()
					   == QStringLiteral("GitHub shows no change near this line at the head, so GitHub can't attach a comment to it. Publish adds a comment on the file instead, linking to line 10 of commit 4613350.")
				   && !outside.target_notice_blocks(),
			   "before Publish the editor says the line goes as a file comment linking to C1's line 10");
		publish_and_wait(run, outside, 2U);
		const auto fileOnC1 = access::review_mutation_evidence(*run.Application);
		const auto link     = [](const std::string_view aCommit, const unsigned aLine) {
			return "https://github.com/bpr-fixtures/commit-filter-comment/blob/" + std::string{ aCommit } + "/src/notes.txt#L" + std::to_string(aLine) + "\n\n";
		};
		std::cout << "commit-filter-comment:file commit=" << fileOnC1.LastCommit << " body=" << fileOnC1.LastBody.substr(0U, fileOnC1.LastBody.find('\n')) << '\n';
		expect(fileOnC1.LastFile && fileOnC1.FilePosts == 1U && fileOnC1.NativePosts == 1U && fileOnC1.LastCommit == comment_c3 && !fileOnC1.LastLine && fileOnC1.LastSide.empty()
				   && fileOnC1.LastPath == "src/notes.txt" && fileOnC1.LastBody == link(comment_c1, 10U) + "BPR-1706 course: unchanged and outside GitHub's diff." && fileOnC1.StoredComments == 2U,
			   "it is one file comment at the head whose permalink names C1's line 10, with no line comment tried first (AC2, AC3)");

		// C1's line 5 is changed by C3: it goes on C1, after the warning.
		auto& changed = write_on(window, core::diff_side::right, 4U, "M4 course: changed at the head.");
		expect(changed.target_notice_text().startsWith(QStringLiteral("Goes on commit 4613350, where this line was written."))
				   && changed.target_notice_text().contains(QStringLiteral("outdated")) && !changed.target_notice_blocks(),
			   "before Publish the editor warns that the comment goes on C1 and GitHub shows it outdated");
		publish_and_wait(run, changed, 3U);
		const auto onC1 = access::review_mutation_evidence(*run.Application);
		std::cout << "commit-filter-comment:earlier commit=" << onC1.LastCommit << " line=" << (onC1.LastLine ? *onC1.LastLine : 0U) << " earlier=" << onC1.EarlierCommitPosts << '\n';
		expect(onC1.LastCommit == comment_c1 && onC1.LastLine == std::optional<std::uint32_t>{ 5U } && onC1.EarlierCommitPosts == 1U && onC1.StoredComments == 3U,
			   "it is published with commit_id = C1 at line 5, and each comment was written once");
		// The window reads the pull request again: the comment GitHub keeps on
		// C1, outdated, is marked on C1's diff where it was written.
		wait_for([&] { return marked(window, core::diff_side::right, 4U); }, "the comment published on C1 is marked on C1's diff at line 5 after the reread");
		expect(window.github_status_text() != "Refresh failed. Showing the last accepted pull request.", "the reread after publishing succeeds");

		// Only C3 shown: the left side is C2, not the merge base.
		gesture(window, { comment_c3 }, QStringLiteral("commitFilterShowOnly"));
		wait_for([&] { return window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("1 of 3 commits shown"); }, "Show only C3");
		enter_review(run);
		select(window, "src/notes.txt");
		expect(window.comparison_surface()->current_context()->Key.Filter == std::optional{ core::filtered_diff_endpoints{ .Left = commit(comment_c2), .Right = commit(comment_c3) } },
			   "with only C3 shown src/notes.txt's diff is C2 to C3");
		// [[BPR-1706]] AC1: a base line C1 changed has no LEFT line; it goes as
		// a file comment whose permalink names it at the diff's left commit, C2.
		auto& based = write_on(window, core::diff_side::left, 14U, "BPR-1706 course: a base line C1 changed.");
		expect(based.target_notice_text()
					   == QStringLiteral("This line changed after the merge base, where GitHub counts base lines, so GitHub can't attach a comment to it. Publish adds a comment on the file instead, linking to line 15 of commit 3a82b6d.")
				   && !based.target_notice_blocks(),
			   "before Publish the editor says the base line goes as a file comment linking to C2's line 15");
		publish_and_wait(run, based, 4U);
		const auto fileOnC2 = access::review_mutation_evidence(*run.Application);
		expect(fileOnC2.LastFile && fileOnC2.FilePosts == 2U && fileOnC2.NativePosts == 2U && fileOnC2.LastCommit == comment_c3 && !fileOnC2.LastLine
				   && fileOnC2.LastBody == link(comment_c2, 15U) + "BPR-1706 course: a base line C1 changed." && fileOnC2.StoredComments == 4U,
			   "it is one file comment at the head whose permalink names C2's line 15, with no line comment tried first (AC1, AC3)");
		auto& far = write_on(window, core::diff_side::left, 9U, "M4 course: a base line no change shows.");
		expect(!far.target_notice_blocks() && far.target_notice_text().startsWith(QStringLiteral("GitHub shows no change near this line at the head"))
				   && far.target_notice_text().endsWith(QStringLiteral("linking to line 10 of commit 3a82b6d.")),
			   "a base line outside every hunk of GitHub's diff would go as a file comment too");
		auto& kept = write_on(window, core::diff_side::left, 12U, "M4 course: a base line the merge base has.");
		expect(kept.target_notice_text().isEmpty(), "a base line the merge base has unchanged, near a change, needs no notice");
		publish_and_wait(run, kept, 5U);
		const auto onBase = access::review_mutation_evidence(*run.Application);
		expect(onBase.LastCommit == comment_c3 && onBase.LastLine == std::optional<std::uint32_t>{ 13U } && onBase.LastSide == "LEFT",
			   "it is published as GitHub's LEFT line 13 at the head");

		// Whole PR: the head's thread at line 17, C1's outdated thread unmarked.
		window.overview_page()->commit_chip()->clear_button()->click();
		wait_for([&] { return !window.overview_page()->commit_chip()->isVisible(); }, "Clear all filters");
		enter_review(run);
		select(window, "src/notes.txt");
		wait_for([&] { return marked(window, core::diff_side::right, 16U); }, "Whole PR marks the head's thread at line 17");
		expect(!marked(window, core::diff_side::right, 24U) && !marked(window, core::diff_side::right, 26U), "and not the thread GitHub shows outdated on C1");
		const auto final = access::review_mutation_evidence(*run.Application);
		expect(final.NativePosts == 3U && final.FilePosts == 2U && final.StoredComments == 5U, "three line comments, two file comments, and nothing written twice");
		finish(run);
		std::cout << "commit-filter-comment:pass\n";
	}

	/// \brief Runs one Find in files query to its end.
	void find(qt::source_search_panel& aPanel, const QString& aQuery)
	{
		aPanel.bar()->query_editor()->setPlainText(aQuery);
		aPanel.bar()->flush();
		wait_for([&] { return aPanel.projection().Query == aQuery.toStdString() && aPanel.projection().Status != core::source_search_status::searching; },
				 "the query runs to its end");
	}

	/// \brief The files the last query matched, by path, of those named.
	[[nodiscard]] auto found(const qt::source_search_panel& aPanel, const std::vector<std::string_view>& aPaths) -> std::set<std::string>
	{
		auto result = std::set<std::string>{ };
		if (!aPanel.projection().Matches) {
			return result;
		}
		for (const auto& match : *aPanel.projection().Matches) {
			for (const auto path : aPaths) {
				if (match.File == core::file_identity_of(path)) {
					result.emplace(path);
				}
			}
		}
		return result;
	}

	/// \brief [[BPR-1688]]. The desktop composition: the one that keeps each
	/// window's review with the profile, as quitting and launching again do.
	struct desktop_course
	{
		QTemporaryDir                         Temporary{ QDir::tempPath() + QStringLiteral("/bpr-commit-filter-desktop-XXXXXX") };
		std::unique_ptr<app::profile_desktop> Desktop;
		app::github_application*              Owner = nullptr;
	};

	auto launch(desktop_course& aCourse) -> qt::workbench_window&
	{
		const auto dataRoot = e2e::private_data_root(aCourse.Temporary);
		expect(dataRoot.has_value(), "private desktop root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = dataRoot;
		options.RepeatReadSessions  = true;
		auto terminal               = false;
		aCourse.Owner               = nullptr;
		aCourse.Desktop             = access::start_desktop(app::detail::github_public_path_fixture::commit_filter_comment_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				aCourse.Owner = *aResult;
			} else {
				std::cout << "desktop-startup:failure=" << static_cast<int>(aResult.error()) << " source=" << app::startup_failure_source() << '\n';
			}
		});
		wait_for([&] { return terminal; }, "desktop startup finishes");
		expect(aCourse.Owner != nullptr, "desktop admitted");
		wait_for([&] { return aCourse.Owner->windows().size() == 1U; }, "the desktop has its one window");
		auto& window = *aCourse.Owner->window(aCourse.Owner->windows().front());
		window.resize(1280, 860);
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
		return window;
	}

	void quit(desktop_course& aCourse, qt::workbench_window& aWindow)
	{
		auto* const action = aWindow.findChild<QAction*>(QStringLiteral("quitProfilesAction"));
		expect(action != nullptr, "Quit is composed");
		action->trigger();
		wait_for([&] { return aCourse.Desktop->is_quiescent(); }, "Quit flushes the window's review and drains");
		aCourse.Desktop.reset();
		aCourse.Owner = nullptr;
	}

	/// \brief Resumes the review from Home's recently viewed list, as a
	/// reviewer returning to it does.
	void resume_recent(qt::workbench_window& aWindow, const QString& aAddress)
	{
		auto* const list = aWindow.home_recent_list();
		const auto  row  = [&] {
			for (auto index = 0; index < list->count(); ++index) {
				if (list->item(index)->text().startsWith(aAddress)) {
					return index;
				}
			}
			return -1;
		};
		auto listed = QElapsedTimer{ };
		listed.start();
		while (!(list->isVisible() && row() >= 0) && listed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!(list->isVisible() && row() >= 0)) {
			std::cout << "open-recent: home=" << aWindow.showing_home() << " visible=" << list->isVisible() << " rows=" << list->count() << " status=" << aWindow.github_status_text() << '\n';
		}
		expect(list->isVisible() && row() >= 0, "Home lists the review as recently viewed");
		list->setCurrentRow(row());
		wait_for([&] { return aWindow.home_recent_resume_action()->isEnabled(); }, "the recently viewed review offers its saved reading");
		aWindow.home_recent_resume_action()->trigger();
		wait_for([&] { return !aWindow.showing_home() && aWindow.overview_page()->projection().has_value() && overview_files(aWindow) > 0U; }, "the recently viewed review resumes");
	}

	/// \brief The restoration store of a quit desktop, as the profile has it.
	[[nodiscard]] auto restoration_store(desktop_course& aCourse) -> std::unique_ptr<application::window_review_restoration_port>
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(aCourse.Temporary.path().toStdString())) {
			if (entry.path().filename() == "window-restoration.sqlite3") {
				return adapters::sqlite::make_window_review_restoration_store(entry.path().parent_path().parent_path().parent_path());
			}
		}
		return { };
	}

	void filtered_find_and_restore()
	{
		auto       run     = course{ };
		const auto comment = opening{ .Fixture = app::detail::github_public_path_fixture::commit_filter_comment_v1, .Address = QStringLiteral("bpr-fixtures/commit-filter-comment#894"), .Files = 99U };
		auto*      window  = &start(run, comment);
		read_commit_trees(*window);
		enter_review(run);
		auto* const search = window->findChild<QAction*>(QStringLiteral("sourceFilesAction"));
		expect(search != nullptr, "Find in files is composed");
		wait_for([&] { return search->isEnabled(); }, "Find in files is offered");
		search->trigger();
		auto* const panel = window->pr_search();
		wait_for([&] { return panel->isVisible() && panel->projection().Scope.has_value(); }, "Find in files opens on the review");
		const auto paths = std::vector<std::string_view>{ "src/notes.txt", "src/app.cpp" };

		// Whole PR: the head's src/notes.txt has C3's inserted line, and only
		// src/app.cpp still has C1's line 5.
		expect(!panel->projection().Scope->FilteredFiles, "Whole PR searches every changed file");
		find(*panel, QStringLiteral("inserted 1 C3"));
		expect(found(*panel, paths) == std::set<std::string>{ "src/notes.txt" }, "Whole PR finds C3's line in src/notes.txt");
		find(*panel, QStringLiteral("line 5 C1"));
		expect(found(*panel, paths) == std::set<std::string>{ "src/app.cpp" }, "Whole PR finds C1's line 5 only in src/app.cpp");
		expect(!panel->summary_text().contains(QStringLiteral("filtered set")), "and does not say it searches a filtered set");

		// C3 hidden: every file at its filtered diff's commits, here C1.
		window->show_commits();
		gesture(*window, { comment_c3 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return window->overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown"); }, "Hide C3");
		const auto filtered = overview_files(*window);
		enter_review(run);
		wait_for([&] { return panel->projection().Scope && panel->projection().Scope->FilteredFiles == std::optional{ static_cast<std::uint32_t>(filtered) }; },
				 "Find in files is bound to the filtered set's files");
		find(*panel, QStringLiteral("inserted 1 C3"));
		expect(found(*panel, paths).empty() && panel->projection().Summary.Matches == 0U, "with C3 hidden C3's line is not found");
		find(*panel, QStringLiteral("line 5 C1"));
		expect(found(*panel, paths) == std::set<std::string>{ "src/app.cpp", "src/notes.txt" }, "src/notes.txt is searched at C1, where line 5 is C1's");
		expect(panel->summary_text().contains(QStringLiteral("searched the filtered set")), "and Find in files says it searched the filtered set");
		std::cout << "filtered-find files=" << filtered << " summary=\"" << panel->summary_text().toStdString() << "\"\n";

		expect(access::held_review_mutation_terminals(*run.Application) == 0U, "no filtered search asks GitHub for anything (AC05)");
		finish(run);
		std::cout << "filtered-find:pass\n";
	}

	/// \brief [[BPR-1688]], AC2. The desktop keeps the window's commit and file
	/// filters with its review and brings them back when it is opened again.
	void filters_restore_with_the_window()
	{
		const auto address = QStringLiteral("bpr-fixtures/commit-filter-comment#894");
		auto       run     = desktop_course{ };
		auto*      window  = &launch(run);
		window->home_open_bar()->setText(address);
		window->home_open_action()->trigger();
		wait_for([&] { return window->showing_overview() && overview_files(*window) == 99U; }, "the pull request opens on Overview");
		read_commit_trees(*window);
		gesture(*window, { comment_c3 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return window->overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown"); }, "Hide C3");
		gesture(*window, { comment_c1 }, QStringLiteral("commitFilterHideFiles"));
		wait_for([&] { return window->overview_page()->commit_chip()->detail()->text().contains(QStringLiteral("hidden by file filters")); }, "a file filter on C1's files");
		const auto both = overview_files(*window);
		expect(both < 99U, "the file filter hides files");
		// A reading to resume: a file the filters leave, in Review.
		auto step  = app::detail::course_modality_step{ };
		step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
		wait_for([&] { return access::enter_course_modality(*run.Owner, step) != app::detail::course_modality_result::pending; }, "enters Review");
		select(*window, "gen/item_000.txt");
		quit(run, *window);
		// Launched again, the review resumed from Home has both filters.
		window = &launch(run);
		resume_recent(*window, address);
		read_commit_trees(*window);
		wait_for([&] { return window->overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown"); }, "the window restores the commit filter");
		wait_for([&] { return window->overview_page()->commit_chip()->detail()->text().contains(QStringLiteral("hidden by file filters")) && overview_files(*window) == both; },
				 "and the file filter, with the same files");
		quit(run, *window);

		// A kept filter that names a commit the pull request no longer has.
		{
			auto store = restoration_store(run);
			expect(store != nullptr, "the profile keeps a restoration store");
			auto snapshot = store->load({ });
			expect(snapshot.has_value(), "the restoration store reads");
			auto gone       = core::filter_hidden_commit{ };
			gone.Oid        = commit("0000000000000000000000000000000000000001");
			gone.Headline   = "Rewritten away";
			gone.Author     = "fixture-author";
			auto       next = *snapshot;
			auto       kept = false;
			const auto name = [&](std::optional<core::window_review_filters>& aFilters) {
				if (aFilters && !aFilters->Commits.Hidden.empty()) {
					aFilters->Commits.Hidden.insert(aFilters->Commits.Hidden.begin(), gone);
					kept = true;
				}
			};
			for (auto& record : next.Records) {
				name(record.Filters);
			}
			for (auto& recent : next.Recent) {
				name(recent.Filters);
			}
			expect(kept, "the window kept its filters with the profile");
			++next.Revision;
			expect(store->save(next, snapshot->Revision, { }).has_value(), "the restoration store writes");
		}
		window = &launch(run);
		resume_recent(*window, address);
		read_commit_trees(*window);
		wait_for([&] { return window->overview_page()->commit_chip()->detail()->text().contains(QStringLiteral("Hidden commit 0000000 (Rewritten away) is no longer in the pull request")); },
				 "a restored filter's gone commit is named as after a force push");
		auto applies = QElapsedTimer{ };
		applies.start();
		while (window->overview_page()->commit_chip()->summary()->text() != QStringLiteral("2 of 3 commits shown") && applies.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		std::cout << "filters-restore: gone chip=\"" << window->overview_page()->commit_chip()->summary()->text().toStdString() << " / "
				  << window->overview_page()->commit_chip()->detail()->text().toStdString() << "\" files=" << overview_files(*window) << " trees=" << static_cast<int>(tab(*window)->projection()->Trees) << "\n";
		expect(window->overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown"), "and the rest of the filter applies");
		quit(run, *window);
		expect(e2e::remove_course_root(run.Temporary), "private desktop root removed");
		std::cout << "filters-restore:pass\n";
	}

	void force_push_rematch()
	{
		auto  run    = course{ };
		auto& window = start(run, split_opening(app::detail::github_epoch_fixture_phase::seed));
		read_commit_trees(window, 97U);

		// A Whole PR receipt, before anything moves: gen/item_000.txt read.
		enter_review(run);
		select(window, "gen/item_000.txt");
		wait_for([&] { return window.read_action()->isEnabled(); }, "Mark read is offered for gen/item_000.txt");
		const auto read = [&] { return window.read_action()->text() == QStringLiteral("Mark &unread"); };
		expect(!read(), "gen/item_000.txt is not read yet");
		window.read_action()->trigger();
		wait_for(read, "gen/item_000.txt is read");
		window.show_commits();
		gesture(window, { split_c3 }, QStringLiteral("commitFilterHide"));
		wait_for([&] { return window.overview_page()->commit_chip()->summary()->text() == QStringLiteral("2 of 3 commits shown"); }, "Hide C3");
		// A file filter, which a force push leaves as it is.
		gesture(window, { split_c1 }, QStringLiteral("commitFilterHideFiles"));
		wait_for([&] { return window.overview_page()->commit_chip()->detail()->text().contains(QStringLiteral("hidden by file filters")); }, "a file filter on C1's files");

		// The force push: the next refresh serves C3 reworded.
		expect(window.github_refresh_action()->isEnabled(), "Refresh is available");
		window.github_refresh_action()->trigger();
		auto* const chip = window.overview_page()->commit_chip();
		// Until the new head's trees are read only the headline and author can
		// match, which is offered as a guess; then the same change is known.
		wait_for([&] {
			return tab(window)->model()->row_of(std::string{ split_c3r }) >= 0 && !chip->rematch_button()->isHidden()
				&& tab(window)->projection()->Trees == application::commit_filter_trees::ready && chip->detail()->text().contains(QStringLiteral("makes the same change"));
		},
				 "after the push the chip offers a commit in place of the gone one");
		expect(chip->detail()->text().contains(QStringLiteral("Hidden commit 968f892 (Follow-ups) is no longer in the pull request")),
			   "the chip names the missing commit");
		expect(chip->detail()->text().contains(QStringLiteral("cfaecb6 (Follow-ups) makes the same change as 968f892.")) && chip->rematch_button()->text() == QStringLiteral("Hide cfaecb6"),
			   "and offers the commit carrying the same change");
		const auto offered = tab(window)->rematch_buttons();
		expect(offered.size() == 1U && offered.front()->text() == QStringLiteral("Hide cfaecb6"), "the Commits tab offers it too");
		expect(tab(window)->model()->row_at(tab(window)->model()->row_of(std::string{ split_c3r }))->State == core::filter_commit_state::shown
				   && chip->summary()->text() != QStringLiteral("2 of 3 commits shown"),
			   "nothing is applied by itself: the new commit is shown");
		expect(chip->detail()->text().contains(QStringLiteral("hidden by file filters")), "the file filter persists");
		enter_review(run);
		select(window, "gen/item_000.txt");
		wait_for(read, "the identical-material receipt persists: gen/item_000.txt is still read under the new head");
		window.show_commits();

		// The reviewer chooses it.
		chip->rematch_button()->click();
		wait_for([&] { return chip->summary()->text() == QStringLiteral("2 of 3 commits shown") && chip->rematch_button()->isHidden(); },
				 "choosing it hides the new commit in the gone one's place");
		expect(!chip->detail()->text().contains(QStringLiteral("no longer in the pull request"))
				   && tab(window)->model()->row_at(tab(window)->model()->row_of(std::string{ split_c3r }))->State == core::filter_commit_state::hidden,
			   "the gone commit is forgotten and the new one is hidden");
		expect(access::held_review_mutation_terminals(*run.Application) == 0U, "no rematch asks GitHub for anything (AC05)");
		std::cout << "force-push course files=" << overview_files(window) << '\n';
		finish(run);
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	auto qtApplication = QApplication{ aArgc, aArgv };
	filters_follow_the_commits();
	generated_files_and_file_filters();
	linguist_generated_at_head();
	split_files_read_per_commit();
	force_push_rematch();
	comments_from_a_filtered_diff();
	filtered_find_and_restore();
	filters_restore_with_the_window();
	std::cout << "COMMIT_FILTER_PUBLIC_PATH_OK qt=" << qVersion() << '\n';
	return 0;
}
