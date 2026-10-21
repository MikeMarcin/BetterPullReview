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
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
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
	};

	auto start(course& aCourse, const opening& aOpening = { }) -> qt::workbench_window&
	{
		const auto dataRoot = e2e::private_data_root(aCourse.Temporary);
		expect(dataRoot.has_value(), "private course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = dataRoot;
		options.EpochPhase          = aOpening.Phase;
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
		wait_for([&] { return shown(aWindow, aPath); }, "the file's diff is shown");
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
	std::cout << "COMMIT_FILTER_PUBLIC_PATH_OK qt=" << qVersion() << '\n';
	return 0;
}
