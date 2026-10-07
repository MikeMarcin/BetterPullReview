#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/pull_request_location.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_refusal.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <string_view>
#include <utility>

#if defined(Q_OS_MACOS)
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using bpr::tests::expect;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { })
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aDiagnostic) {
			aDiagnostic();
		}
		expect(aCondition(), aMessage);
	}
	void submit(qt::workbench_window& aWindow, const QString& aText)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		expect(aWindow.home_open_bar()->isVisible(), "Home action exposes the real locator before typing");
		aWindow.home_open_bar()->setText(aText);
		aWindow.home_open_action()->trigger();
	}
	auto button(qt::workbench_window& aWindow, const char* aName) -> QPushButton*
	{
		auto* result = aWindow.findChild<QPushButton*>(QString::fromUtf8(aName));
		expect(result != nullptr, "public setup button is composed");
		return result;
	}
	void cancel_setup(qt::workbench_window& aWindow)
	{
		expect(aWindow.sign_in_dialog() && aWindow.sign_in_dialog()->isVisible(), "shared sign-in dialog offers cancellation");
		aWindow.sign_in_dialog()->reject();
	}
	void connect_native(qt::workbench_window& aWindow, const std::string& aHostname = "ghe.example.invalid")
	{
		for (auto* card : aWindow.server_cards()) {
			if (card->hostname() == aHostname) {
				auto* action = card->sign_in_button()->isVisible() ? card->sign_in_button() : card->connect_button();
				expect(action->isVisible() && action->isEnabled(), "native connection is offered in the shared dialog");
				action->click();
				return;
			}
		}
		expect(false, "requested native server card exists");
	}

	void capture(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_SETUP_ARTIFACT_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "review artifact directory");
		aWindow.show();
		QApplication::processEvents();
		auto* surface = aWindow.sign_in_dialog() && aWindow.sign_in_dialog()->isVisible() ? static_cast<QWidget*>(aWindow.sign_in_dialog()) : &aWindow;
		expect(surface->grab().save(root + QLatin1Char{ '/' } + aName), "public fixture window capture");
	}

#if defined(__APPLE__)
	// Only the Apple-only CLI source update course composes these checks.
	void verify_composed_code_metrics(app::github_application& aApplication, const std::uint32_t aWidth)
	{
		auto* const workbench = aApplication.window().comparison_surface();
		for (auto* const pane : { workbench->left_pane(), workbench->right_pane(), workbench->detail_pane(),
								  aApplication.window().findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource")) }) {
			expect(pane != nullptr, "every composed source pane exists");
			const auto expected = QFontMetricsF{ pane->font() }.horizontalAdvance(QLatin1Char{ ' ' }) * aWidth;
			expect(qAbs(pane->tabStopDistance() - expected) < 0.01
					   && qAbs(pane->document()->defaultTextOption().tabStopDistance() - expected) < 0.01,
				   "composed source layout uses the selected code-font tab width, not Qt's default");
		}
	}

	void verify_overview_folder_navigation(qt::workbench_window& aWindow)
	{
		auto*      page   = aWindow.overview_page();
		auto*      view   = page->files_view();
		auto*      model  = page->file_model();
		const auto file   = model->index_for_path(QStringLiteral("review.cpp"));
		const auto other  = model->index_for_path(QStringLiteral("secondary.cpp"));
		const auto folder = file.parent();
		const auto left   = [view](const QModelIndex& aIndex) {
			return view->visualRect(aIndex.siblingAtColumn(qt::overview_file_model::file_column)).left();
		};
		// [[BPR-1213]]. Flat folders: the folder row and its files share one left column.
		expect(file.isValid() && other.parent() == folder && view->isExpanded(folder)
				   && view->itemsExpandable() && view->indentation() == 0 && left(file) == left(folder) && left(other) == left(file),
			   "public Overview exposes flat folder rows whose files align at one left column");
		view->setCurrentIndex(folder);
		view->setFocus();
		QKeyEvent collapse{ QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier };
		QApplication::sendEvent(view, &collapse);
		QApplication::processEvents();
		expect(!view->isExpanded(folder) && view->visualRect(file).isEmpty() && view->visualRect(other).isEmpty(),
			   "Overview keyboard collapse hides both exact recorded files");
		capture(aWindow, QStringLiteral("overview-folders-collapsed.png"));
		QKeyEvent expand{ QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier };
		QApplication::sendEvent(view, &expand);
		QApplication::processEvents();
		expect(view->isExpanded(folder) && !view->visualRect(file).isEmpty() && !view->visualRect(other).isEmpty()
				   && model->index_for_path(QStringLiteral("review.cpp")) == file
				   && model->index_for_path(QStringLiteral("secondary.cpp")) == other
				   && aWindow.current_modality() == qt::review_modality::overview
				   && !aWindow.files_drawer()->contents_view()->currentIndex().isValid()
				   && aWindow.comparison_surface()->current_result() == nullptr,
			   "Overview expansion preserves files without choosing a comparison or leaving Overview");
		capture(aWindow, QStringLiteral("overview-folders-expanded.png"));
	}

	void verify_composed_file_operation_ink(app::github_application& aApplication, const core::appearance_mode aMode)
	{
		auto* const view            = aApplication.window().files_tree();
		auto        expectedProfile = qt::appearance_profile{ };
		static_cast<void>(expectedProfile.select_mode(aMode));
		// Feed independent safe model rows to the actual composed delegate.
		// Do not manually apply a profile to it: this catches a lost app handoff.
		auto model = qt::files_tree_model{ };
		for (const auto operation : { application::overview_file_operation::added, application::overview_file_operation::removed }) {
			const auto path = std::string{ "synthetic.cpp" };
			const auto id   = core::file_identity_of(path);
			model.apply({ { .Identity = QString::fromStdString(path), .Title = QString::fromStdString(path), .File = id } });
			const auto row = application::overview_file_row{ .Path = path, .Name = path, .Operation = operation, .Additions = 1U, .Deletions = 1U };
			model.apply_overview(std::span{ &row, 1U });
			const auto ink    = expectedProfile.color(operation == application::overview_file_operation::added
														  ? core::appearance_role::state_accepted
														  : core::appearance_role::state_defect);
			auto       raster = QImage{ 420, 32, QImage::Format_ARGB32 };
			raster.fill(view->palette().color(QPalette::Base));
			auto option = QStyleOptionViewItem{ };
			option.initFrom(view);
			option.widget = view;
			option.rect   = raster.rect();
			option.state  = QStyle::State_Enabled;
			{
				auto painter = QPainter{ &raster };
				view->delegate()->paint(&painter, option, model.index_for_file(id));
			}
			const auto nearInk = [](const QColor& aActual, const QColor& aExpected) {
				return qAbs(aActual.red() - aExpected.red()) < 24
					&& qAbs(aActual.green() - aExpected.green()) < 24
					&& qAbs(aActual.blue() - aExpected.blue()) < 24;
			};
			auto matching     = 0;
			auto ordinaryName = 0;
			auto addedCount   = 0;
			auto removedCount = 0;
			for (auto y = 0; y < raster.height(); ++y) {
				for (auto x = 0; x < raster.width(); ++x) {
					matching += nearInk(raster.pixelColor(x, y), ink) ? 1 : 0;
					if (x < 210) {
						ordinaryName += nearInk(raster.pixelColor(x, y), view->palette().color(QPalette::Text)) ? 1 : 0;
					} else {
						addedCount += nearInk(raster.pixelColor(x, y), expectedProfile.color(core::appearance_role::state_accepted)) ? 1 : 0;
						removedCount += nearInk(raster.pixelColor(x, y), expectedProfile.color(core::appearance_role::state_defect)) ? 1 : 0;
					}
				}
			}
			expect(matching > 2, "actual composed drawer delegate receives the selected semantic operation ink");
			expect(ordinaryName > 8 && addedCount > 0 && removedCount > 0,
				   "actual composed drawer keeps names neutral and paints plus/minus counts independently");
		}
	}
#endif

	void run_folder_selection_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated folder course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private folder course root");
		std::unique_ptr<app::github_application> owner;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::large_review_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "folder course startup");
		expect(owner != nullptr, "folder course application");
		auto& window = owner->window();
		window.show();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "folder course connection");
		submit(window, QStringLiteral("https://github.com/bpr-fixtures/large-review/pull/17"));
		wait_for([&] { return owner->status().EpochStatus == core::epoch_status::ready && window.files_tree()->tree_model()->file_count() == 515U; }, "folder course exact comparison");
		window.modality()->action(qt::review_modality::review)->trigger();
		if (!window.files_drawer_toggle_action()->isChecked()) {
			window.files_drawer_toggle_action()->trigger();
		}
		auto* const tree = window.files_tree();
		wait_for([&] { return tree->isVisible(); }, "folder course uses the visible file drawer");
		auto* const model   = tree->tree_model();
		auto* const surface = window.comparison_surface();
		const auto  file    = core::file_identity_of("src/file_0000.cpp");
		tree->setCurrentIndex(model->index_for_file(file));
		wait_for([&] { return surface->current_result() && surface->current_context() && surface->current_context()->Key.File == file && !surface->geometry_pending(); }, "folder course selected source");
		const auto context    = surface->current_context();
		const auto result     = surface->current_result();
		const auto left       = surface->left_pane()->toPlainText();
		const auto right      = surface->right_pane()->toPlainText();
		const auto selections = window.statistics().FileSelections;
		expect(!left.isEmpty() && !right.isEmpty(), "installed Git supplies both source sides before folder gestures");
		const auto unchanged = [&] {
			QApplication::processEvents();
			expect(window.statistics().FileSelections == selections && surface->current_context() == context
					   && surface->current_result() == result && surface->left_pane()->toPlainText() == left
					   && surface->right_pane()->toPlainText() == right,
				   "folder interactions never reach file selection or replace the installed-Git diff");
		};
		for (auto rebuild = 0; rebuild < 2; ++rebuild) {
			const auto folder = model->parent(model->index_for_file(file));
			expect(folder.isValid() && !(model->flags(folder) & Qt::ItemIsSelectable), "actual composed folder is structurally nonselectable");
			const auto  point = QPointF{ tree->visualRect(folder).center() };
			QMouseEvent press{ QEvent::MouseButtonPress, point, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QMouseEvent release{ QEvent::MouseButtonRelease, point, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(tree->viewport(), &press);
			QApplication::sendEvent(tree->viewport(), &release);
			unchanged();
			for (const auto key : { Qt::Key_Left, Qt::Key_Right, Qt::Key_Return, Qt::Key_Space }) {
				tree->setCurrentIndex(folder);
				QKeyEvent event{ QEvent::KeyPress, key, Qt::NoModifier };
				QApplication::sendEvent(tree, &event);
				unchanged();
			}
			expect(!tree->selectionModel()->isSelected(folder), "folder does not masquerade as the selected file");
			window.files_filter()->setText(QStringLiteral("file_0001"));
			unchanged();
			window.files_filter()->clear();
			unchanged();
			expect(model->file_at(tree->currentIndex()) == std::optional{ file }, "composed filter restores the semantic file after folder focus");
		}
		capture(window, QStringLiteral("folder-retains-exact-diff.png"));
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "folder course quiescence");
	}

	/// [[BPR-1213]]. The constructed nested-folders pull request in the
	/// composed window, on the Overview it opens on: the flat folder rows the
	/// owner judges, and a click or Left/Right on one of them hiding and
	/// showing only that folder's own files, never a deeper folder's.
	void run_nested_overview_folders_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated nested-folders course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private nested-folders course root");
		std::unique_ptr<app::github_application> owner;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::nested_folders_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "nested-folders course startup");
		expect(owner != nullptr, "nested-folders course application");
		auto& window = owner->window();
		window.resize(1100, 900);
		window.show();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "nested-folders course connection");
		submit(window, QStringLiteral("https://github.com/bpr-fixtures/nested-folders/pull/21"));
		auto* const page  = window.overview_page();
		auto* const view  = page->files_view();
		auto* const model = page->file_model();
		wait_for([&] { return page->isVisible() && page->presented() && model->file_count() == 17U; }, "nested-folders pull request opens on Overview with all 17 files");
		expect(window.current_modality() == qt::review_modality::overview, "the named pull request opens on Overview");
		const auto folders = QStringList{ QStringLiteral("/"), QStringLiteral("docs"), QStringLiteral("docs/api"), QStringLiteral("docs/api/v2"),
										  QStringLiteral("src"), QStringLiteral("src/engine"), QStringLiteral("src/engine/shaders"),
										  QStringLiteral("src/engine/shaders/post"), QStringLiteral("src/ui/widgets"), QStringLiteral("tests/engine"),
										  QStringLiteral("tests/ui/widgets"), QStringLiteral("tools/build/scripts") };
		auto       shown   = QStringList{ };
		for (auto row = 0; row < model->rowCount(); ++row) {
			const auto folder = model->index(row, qt::overview_file_model::file_column);
			expect(folder.data(qt::overview_file_model::folder_role).toBool() && view->isExpanded(folder.siblingAtColumn(0)),
				   "every top-level Overview row is an expanded folder");
			shown.push_back(folder.data(Qt::DisplayRole).toString());
		}
		expect(shown == folders && model->folder_count() == 12U,
			   "one flat row per folder that holds files, named by its whole path; src/ui, tests/ui, tools and tools/build take none");
		const auto file    = [&](const char* aPath) { return model->index_for_path(QString::fromUtf8(aPath)); };
		const auto visible = [&](const char* aPath) { return !view->visualRect(file(aPath)).isEmpty(); };
		const auto left    = [&](const QModelIndex& aIndex) {
			return view->visualRect(aIndex.siblingAtColumn(qt::overview_file_model::file_column)).left();
		};
		const auto all    = { "README.md", "docs/api/reference.md", "docs/api/v2/endpoints.md", "docs/guide.md", "src/engine/renderer.cpp",
							  "src/engine/renderer.h", "src/engine/shaders/basic.frag", "src/engine/shaders/basic.vert",
							  "src/engine/shaders/post/bloom.frag", "src/engine/shaders/post/tonemap.frag", "src/main.cpp",
							  "src/ui/widgets/button.cpp", "src/ui/widgets/button.h", "src/ui/widgets/slider.cpp",
							  "tests/engine/renderer_tests.cpp", "tests/ui/widgets/button_tests.cpp", "tools/build/scripts/package.py" };
		const auto column = left(file("README.md").parent());
		for (const auto* path : all) {
			expect(file(path).isValid() && visible(path) && left(file(path)) == column, "every file starts at the folder rows' left column");
		}
		const auto selections = window.statistics().FileSelections;
		const auto unchanged  = [&] {
			QApplication::processEvents();
			expect(window.statistics().FileSelections == selections && window.current_modality() == qt::review_modality::overview
					   && window.comparison_surface()->current_result() == nullptr,
				   "a folder gesture opens no file and leaves Overview");
		};
		const auto only = [&](std::initializer_list<const char*> aHidden, const char* aMessage) {
			for (const auto* path : all) {
				const auto hidden = std::ranges::any_of(aHidden, [path](const char* aEntry) { return std::string_view{ aEntry } == path; });
				expect(visible(path) != hidden, aMessage);
			}
		};
		const auto click = [&](const QModelIndex& aFolder) {
			const auto  point = QPointF{ view->visualRect(aFolder.siblingAtColumn(qt::overview_file_model::file_column)).center() };
			QMouseEvent press{ QEvent::MouseButtonPress, point, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QMouseEvent release{ QEvent::MouseButtonRelease, point, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(view->viewport(), &press);
			QApplication::sendEvent(view->viewport(), &release);
			QApplication::processEvents();
		};
		const auto key = [&](const QModelIndex& aFolder, const Qt::Key aKey) {
			view->setCurrentIndex(aFolder);
			view->setFocus();
			QKeyEvent event{ QEvent::KeyPress, aKey, Qt::NoModifier };
			QApplication::sendEvent(view, &event);
			QApplication::processEvents();
		};
		// A click on `src` hides src/main.cpp and nothing under src/engine or
		// src/ui/widgets, whose rows are folders of their own.
		const auto src = file("src/main.cpp").parent().siblingAtColumn(0);
		click(src);
		expect(!view->isExpanded(src), "a click on the src row collapses it");
		only({ "src/main.cpp" }, "collapsing src hides only src/main.cpp");
		unchanged();
		// A nested folder between its parent and its own subfolder.
		const auto shaders = file("src/engine/shaders/basic.frag").parent().siblingAtColumn(0);
		key(shaders, Qt::Key_Left);
		expect(!view->isExpanded(shaders) && view->currentIndex().siblingAtColumn(0) == shaders, "Left collapses src/engine/shaders and keeps it current");
		only({ "src/main.cpp", "src/engine/shaders/basic.frag", "src/engine/shaders/basic.vert" },
			 "collapsing src/engine/shaders hides its two files and leaves src/engine/shaders/post's");
		unchanged();
		capture(window, QStringLiteral("nested-overview-folders-collapsed.png"));
		const auto order = [&](const QModelIndex& aFolder) {
			auto paths = QStringList{ };
			for (auto row = 0; row < model->rowCount(aFolder); ++row) {
				paths.push_back(model->index(row, qt::overview_file_model::file_column, aFolder).data(qt::overview_file_model::path_role).toString());
			}
			return paths;
		};
		key(shaders, Qt::Key_Right);
		click(src);
		expect(view->isExpanded(shaders) && view->isExpanded(src)
				   && order(shaders) == QStringList{ QStringLiteral("src/engine/shaders/basic.frag"), QStringLiteral("src/engine/shaders/basic.vert") }
				   && order(src) == QStringList{ QStringLiteral("src/main.cpp") },
			   "Right and a second click bring back the same files in the same order");
		only({ }, "every file is shown again");
		unchanged();
		capture(window, QStringLiteral("nested-overview-folders-expanded.png"));
		// [[BPR-1269]]. Collapse all, one folder's toggle, then expand all.
		auto* const collapseAll = page->collapse_folders_button();
		auto* const expandAll   = page->expand_folders_button();
		expect(collapseAll->isVisible() && expandAll->isVisible() && collapseAll->toolTip() == QStringLiteral("Collapse all folders")
				   && expandAll->toolTip() == QStringLiteral("Expand all folders"),
			   "the Overview offers collapse all and expand all beside its heading");
		collapseAll->click();
		QApplication::processEvents();
		for (auto row = 0; row < model->rowCount(); ++row) {
			expect(!view->isExpanded(model->index(row, 0)), "collapse all closes all twelve folder rows");
		}
		only(all, "collapse all hides every file and leaves the folder rows");
		unchanged();
		capture(window, QStringLiteral("nested-overview-folders-all-collapsed.png"));
		click(src);
		expect(view->isExpanded(src) && visible("src/main.cpp") && !visible("src/engine/renderer.cpp") && !visible("README.md"),
			   "after collapse all, one folder's click shows only that folder's files");
		unchanged();
		expandAll->click();
		QApplication::processEvents();
		only({ }, "expand all shows every file again, in its folder");
		expect(order(shaders) == QStringList{ QStringLiteral("src/engine/shaders/basic.frag"), QStringLiteral("src/engine/shaders/basic.vert") },
			   "each folder keeps its files in order");
		unchanged();
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "nested-folders course quiescence");
	}

	/// [[BPR-1089]]. The Conventional Comments pull request on the Overview it
	/// opens on, through the real composition: the counts come from the
	/// composed label buckets, the chips from Reviews & queue › Comment labels,
	/// and a chip's activation lists exactly its conversations in Discuss,
	/// which follows a resolve on the fixture server without moving focus.
	void run_overview_feedback_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated overview-feedback course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private overview-feedback course root");
		// A Refresh reads the pull request again, as the website edits it.
		options.RepeatReadSessions = true;
		std::unique_ptr<app::github_application> owner;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::conventional_comments_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "overview-feedback course startup");
		expect(owner != nullptr, "overview-feedback course application");
		auto& window = owner->window();
		window.resize(1400, 900);
		window.show();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); },
				 "overview-feedback course connection, signed in, so the pull request is read as the fixture account");
		submit(window, QStringLiteral("https://github.com/bpr-fixtures/nested-folders/pull/21"));
		auto* const page  = window.overview_page();
		auto* const view  = page->files_view();
		auto* const model = page->file_model();
		const auto  cell  = [&](const char* aPath) {
			return model->index_for_path(QString::fromUtf8(aPath)).siblingAtColumn(qt::overview_file_model::comments_column);
		};
		const auto counted = [&](const std::uint8_t aColumns, const core::feedback_resolution_mode aMode) {
			return model->feedback().has_value() && model->feedback()->Request.Columns.Count == aColumns && model->feedback()->Request.Mode == aMode;
		};
		wait_for([&] { return page->isVisible() && page->presented() && model->file_count() == 17U && counted(0U, core::feedback_resolution_mode::unresolved); },
				 "the pull request opens on Overview and the composition counts its conversations");
		// No chips: the bubble counts every open conversation; six threads, all
		// native and labelled, so nothing is uncertain.
		expect(cell("src/engine/renderer.cpp").data().toString() == QStringLiteral("Comments 2") && cell("src/main.cpp").data().toString() == QStringLiteral("Comments 1")
				   && cell("docs/guide.md").data().toString().isEmpty() && model->feedback()->PullRequest.Counts.Included == 6U && !page->uncertain_button()->isVisible(),
			   "no chips: renderer.cpp 2, main.cpp 1, a file without conversations blank, six open in all, nothing uncertain");
		expect(page->open_counts_button()->isVisible() && page->open_counts_button()->isChecked(), "Open is the composed default");
		capture(window, QStringLiteral("overview-feedback-no-chips.png"));

		// A typical subset, chosen in Settings: issue and nitpick.
		auto* const editor = window.label_columns_editor();
		expect(editor != nullptr, "Reviews & queue offers the Overview chips");
		editor->item_of(core::feedback_label::issue)->setCheckState(Qt::Checked);
		wait_for([&] { return counted(1U, core::feedback_resolution_mode::unresolved); }, "a chip ticked in Settings reaches the Overview through the profile's setting");
		editor->item_of(core::feedback_label::nitpick)->setCheckState(Qt::Checked);
		wait_for([&] { return counted(2U, core::feedback_resolution_mode::unresolved); }, "a second chip reaches the Overview");
		expect(cell("src/engine/renderer.cpp").data().toString() == QStringLiteral("issue 1 · Other 1") && cell("src/main.cpp").data().toString() == QStringLiteral("nitpick 1")
				   && cell("src/ui/widgets/button.cpp").data().toString() == QStringLiteral("Other 1"),
			   "renderer.cpp: issue 1 and Other 1 (the suggestion); main.cpp: nitpick 1; button.cpp: Other 1 (the chore)");
		capture(window, QStringLiteral("overview-feedback-two-chips.png"));

		// A click on renderer.cpp's issue chip lists exactly that conversation.
		window.choose_modality(qt::review_modality::overview);
		QApplication::processEvents();
		const auto issueCell = cell("src/engine/renderer.cpp");
		const auto chips     = qt::overview_comment_chips(*model->feedback_at(issueCell), view->visualRect(issueCell), view->font());
		expect(!chips.empty() && chips.front().Label == core::feedback_label::issue, "the first chip is the issue chip");
		const auto  point = chips.front().Box.center();
		QMouseEvent press{ QEvent::MouseButtonPress, point, view->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QMouseEvent release{ QEvent::MouseButtonRelease, point, view->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(view->viewport(), &press);
		QApplication::sendEvent(view->viewport(), &release);
		auto* const panel  = window.conversation_panel();
		const auto  listed = [&] {
			auto ids = std::set<std::string>{ };
			for (const auto& card : panel->cards()->cards()) {
				ids.insert(card.Id);
			}
			return ids;
		};
		wait_for([&] { return window.current_modality() == qt::review_modality::discuss && panel->listing().has_value() && listed() == std::set<std::string>{ "review-thread:PRRT_cc_1" }; },
				 "Discuss lists exactly renderer.cpp's open issue");
		expect(panel->listing_caption()->text() == QStringLiteral("issue · open · src/engine/renderer.cpp — 1 conversation") && panel->cards()->hasFocus(),
			   "under a caption naming the count, with focus on the list");
		capture(window, QStringLiteral("overview-feedback-drill-down.png"));

		// Back on the Overview, the keyboard reaches Other through Enter.
		window.choose_modality(qt::review_modality::overview);
		QApplication::processEvents();
		expect(view->hasFocus() && view->currentIndex() == issueCell, "the Overview gives focus back to the chip's cell");
		QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(view, &enter);
		QApplication::processEvents();
		auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		expect(menu != nullptr && menu->objectName() == QStringLiteral("overviewCommentsMenu"), "Enter on the Comments cell opens its chips' menu");
		const auto entries = menu->actions();
		const auto other   = std::ranges::find_if(entries, [](QAction* aAction) { return aAction->text().startsWith(QStringLiteral("Other")); });
		expect(other != entries.end(), "the menu names Other");
		(*other)->trigger();
		menu->close();
		// On macOS's offscreen platform a closed popup hands activation to no
		// window, where a native session returns it to the window under the
		// menu. Give it back, so the focus checks below read the product.
		window.activateWindow();
		wait_for([&] { return QApplication::activeWindow() == &window; }, "the window is active again once the menu closes");
		wait_for([&] { return window.current_modality() == qt::review_modality::discuss && listed() == std::set<std::string>{ "review-thread:PRRT_cc_2" }; },
				 "Other lists exactly the suggestion thread");

		// Back to the issue, then resolve it on the website: the list follows
		// the accepted population, and focus stays on it.
		window.choose_modality(qt::review_modality::overview);
		QApplication::processEvents();
		QApplication::sendEvent(view->viewport(), &press);
		QApplication::sendEvent(view->viewport(), &release);
		wait_for([&] { return listed() == std::set<std::string>{ "review-thread:PRRT_cc_1" }; }, "the issue is listed again");
		// The thread is resolved on the website; a public Refresh reads it.
		github_application_fixture_access::stage_server_root(*owner, "PRRT_cc_1", std::nullopt, true);
		window.github_refresh_action()->trigger();
		wait_for([&] { return listed().empty() && panel->listing_caption()->text().endsWith(QStringLiteral("— 0 conversations")); },
				 "after the resolve, the open issue count lists nothing, and says so", [&] { std::cerr << "caption=" << panel->listing_caption()->text().toStdString() << " included=" << model->feedback()->PullRequest.Counts.Included << " status=" << window.github_status_text() << " refresh=" << window.github_refresh_action()->isEnabled() << '\n'; });
		expect(panel->cards()->hasFocus() && window.current_modality() == qt::review_modality::discuss, "the refresh moved no focus");
		window.choose_modality(qt::review_modality::overview);
		QApplication::processEvents();
		expect(cell("src/engine/renderer.cpp").data().toString() == QStringLiteral("Other 1") && view->currentIndex() == issueCell,
			   "the Overview counts the resolved issue nowhere in Open, and the current cell is kept");
		// All: the resolved issue is back, with its breakdown.
		page->all_counts_button()->click();
		wait_for([&] { return counted(2U, core::feedback_resolution_mode::all); }, "All is counted from the kept buckets");
		expect(cell("src/engine/renderer.cpp").data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 1 (1 resolved), Other 1 (1 open) conversations"),
			   "All counts the resolved issue and says its state");
		capture(window, QStringLiteral("overview-feedback-all.png"));
		// A website edit to the suggestion's root makes it an issue: it leaves
		// Other and joins the issue chip, never both.
		github_application_fixture_access::stage_server_root(*owner, "PRRT_cc_2", std::string{ "issue (non-blocking): Clamp the width to at least one pixel." }, std::nullopt);
		window.github_refresh_action()->trigger();
		wait_for([&] { return cell("src/engine/renderer.cpp").data(Qt::AccessibleTextRole).toString() == QStringLiteral("issue 2 (1 open, 1 resolved) conversations"); },
				 "a website label edit moves the conversation from Other to issue", [&] { std::cerr << "cell=" << cell("src/engine/renderer.cpp").data(Qt::AccessibleTextRole).toString().toStdString() << '\n'; });
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "overview-feedback course quiescence");
	}

	// [[BPR-1089]]. The owner's Overview look opens overview-labels-v1: 34
	// threads on all 17 files, three pull request comments and two review
	// summaries. Its counts are the ones the owner judges, so they are pinned
	// here; with BPR_SETUP_ARTIFACT_ROOT set, each theme is captured.
	void run_overview_labels_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated overview-labels course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private overview-labels course root");
		std::unique_ptr<app::github_application> owner;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::overview_labels_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "overview-labels course startup");
		expect(owner != nullptr, "overview-labels course application");
		auto& window = owner->window();
		window.resize(1400, 900);
		window.show();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "overview-labels course connection");
		submit(window, QStringLiteral("https://github.com/bpr-fixtures/nested-folders/pull/21"));
		auto* const page  = window.overview_page();
		auto* const model = page->file_model();
		const auto  cell  = [&](const char* aPath) {
			return model->index_for_path(QString::fromUtf8(aPath)).siblingAtColumn(qt::overview_file_model::comments_column);
		};
		const auto counted = [&](const std::uint8_t aColumns, const core::feedback_resolution_mode aMode) {
			return model->feedback().has_value() && model->feedback()->Request.Columns.Count == aColumns && model->feedback()->Request.Mode == aMode;
		};
		wait_for([&] { return page->isVisible() && page->presented() && model->file_count() == 17U && counted(0U, core::feedback_resolution_mode::unresolved); },
				 "the pull request opens on Overview with its conversations counted");
		// No chips: 27 open threads (34, seven resolved); ? holds the three pull
		// request comments and two review summaries (no resolution) and the
		// link-first file comment (label unreadable).
		const auto& pull      = model->feedback()->PullRequest.Counts;
		const auto  remainder = [&](const int aRole) { return model->remainder_index(qt::overview_file_model::comments_column).data(aRole).toString(); };
		expect(pull.Included == 27U && pull.Uncertain == 6U && page->uncertain_button()->isVisible() && page->uncertain_button()->text() == QStringLiteral("6"),
			   "27 open conversations and six the Open count leaves out or cannot read");
		expect(cell("src/engine/renderer.cpp").data().toString() == QStringLiteral("Comments 5") && cell("src/ui/widgets/button.cpp").data().toString() == QStringLiteral("Comments 2 · ? 1")
				   && remainder(Qt::DisplayRole) == QStringLiteral("Comments 1 · ? 5"),
			   "renderer.cpp 5; button.cpp 2 with its unreadable label as ?; Not on a file 1 with the five unresolvable as ?");
		// Settings' chips, each published before the next, keep their order.
		auto* const editor = window.label_columns_editor();
		auto        ticked = std::uint8_t{ 0U };
		for (const auto label : { core::feedback_label::issue, core::feedback_label::question, core::feedback_label::suggestion }) {
			editor->item_of(label)->setCheckState(Qt::Checked);
			++ticked;
			wait_for([&] { return counted(ticked, core::feedback_resolution_mode::unresolved); }, "each chip ticked in Settings reaches the Overview");
		}
		const auto folder = model->index_for_path(QStringLiteral("src/engine/renderer.cpp")).parent().siblingAtColumn(qt::overview_file_model::comments_column);
		expect(cell("src/engine/renderer.cpp").data().toString() == QStringLiteral("issue 2 · question 1 · Other 2")
				   && cell("src/ui/widgets/slider.cpp").data().toString() == QStringLiteral("issue 1 · suggestion 1")
				   && folder.data().toString() == QStringLiteral("issue 2 · question 2 · Other 3") && remainder(Qt::DisplayRole) == QStringLiteral("issue 1 · ? 5"),
			   "chips in Settings order: renderer.cpp, slider.cpp, the src/engine folder and Not on a file");
		const auto themes         = std::array{ std::pair{ core::appearance_mode::light, "light" }, std::pair{ core::appearance_mode::dark, "dark" },
												std::pair{ core::appearance_mode::high_contrast_light, "hc-light" }, std::pair{ core::appearance_mode::high_contrast_dark, "hc-dark" } };
		const auto capture_themes = [&](const char* aName) {
			if (qEnvironmentVariable("BPR_SETUP_ARTIFACT_ROOT").isEmpty()) {
				return;
			}
			for (const auto& [mode, theme] : themes) {
				expect(github_application_fixture_access::apply_appearance_gesture(*owner, mode, 4U), "Settings applies each theme");
				QApplication::processEvents();
				capture(window, QStringLiteral("overview-labels-%1-%2.png").arg(QString::fromLatin1(aName), QString::fromLatin1(theme)));
			}
		};
		capture_themes("open");
		page->all_counts_button()->click();
		wait_for([&] { return counted(3U, core::feedback_resolution_mode::all); }, "All is counted");
		expect(cell("src/engine/renderer.cpp").data(Qt::AccessibleTextRole).toString()
					   == QStringLiteral("issue 2 (2 open), question 1 (1 open), suggestion 1 (1 resolved), Other 3 (2 open, 1 resolved) conversations")
				   && remainder(Qt::AccessibleTextRole) == QStringLiteral("issue 3 (1 open, 2 not resolvable), question 1 (1 not resolvable), Other 2 (2 not resolvable) conversations"),
			   "All gives each chip its breakdown, the unresolvable ones on Not on a file");
		capture_themes("all");
		// The Label key and a Comments menu are windows of their own; in a
		// dark theme they show the page's colours, not the application's.
		expect(github_application_fixture_access::apply_appearance_gesture(*owner, core::appearance_mode::dark, 4U), "Settings applies the dark theme");
		QApplication::processEvents();
		page->label_key_button()->click();
		QApplication::processEvents();
		expect(page->label_key() != nullptr && page->label_key()->isVisible()
				   && page->label_key()->findChild<QListWidget*>()->palette().color(QPalette::Base) == page->palette().color(QPalette::Base)
				   && page->palette().color(QPalette::Base).lightnessF() < 0.5,
			   "the Label key shows the dark theme the page shows");
		page->label_key()->close();
		page->files_view()->setFocus();
		page->files_view()->setCurrentIndex(cell("src/engine/renderer.cpp"));
		QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(page->files_view(), &enter);
		QApplication::processEvents();
		auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		expect(menu != nullptr && menu->palette().color(QPalette::Window) == page->palette().color(QPalette::Window), "a Comments menu shows the theme the page shows");
		const auto entries    = menu->actions();
		const auto issueEntry = std::ranges::find_if(entries, [](QAction* aAction) { return aAction->text().startsWith(QStringLiteral("issue")); });
		expect(issueEntry != entries.end(), "the menu names the issue chip");
		(*issueEntry)->trigger();
		menu->close();
		window.activateWindow();
		wait_for([&] { return QApplication::activeWindow() == &window; }, "the window is active again once the menu closes");
		// Discuss's caption leads with the count's own mark and its × follows
		// the words, not the far edge.
		auto* const panel = window.conversation_panel();
		wait_for([&] { return window.current_modality() == qt::review_modality::discuss && panel->listing().has_value() && panel->listing()->size() == 2U; },
				 "renderer.cpp's two open issues are listed");
		const auto* const mark  = panel->findChild<QLabel*>(QStringLiteral("conversationListingMark"));
		const auto* const clear = panel->findChild<QToolButton*>(QStringLiteral("conversationClearListing"));
		expect(mark != nullptr && mark->isVisible() && !mark->pixmap().isNull() && mark->geometry().right() < panel->listing_caption()->geometry().left(),
			   "the caption leads with the issue mark");
		expect(clear != nullptr && clear->geometry().left() - panel->listing_caption()->geometry().right() < 16, "× sits right after the caption");
		if (const auto root = qEnvironmentVariable("BPR_SETUP_ARTIFACT_ROOT"); !root.isEmpty()) {
			// The Label key, the drill-down's caption and the Settings section,
			// each in every theme, for the look.
			page->open_counts_button()->click();
			wait_for([&] { return counted(3U, core::feedback_resolution_mode::unresolved); }, "Open again for the look");
			for (const auto& [mode, theme] : themes) {
				expect(github_application_fixture_access::apply_appearance_gesture(*owner, mode, 4U), "Settings applies each theme");
				window.choose_modality(qt::review_modality::overview);
				QApplication::processEvents();
				page->label_key_button()->click();
				QApplication::processEvents();
				expect(page->label_key() != nullptr && page->label_key()->isVisible(), "the Label key opens");
				expect(page->label_key()->grab().save(root + QStringLiteral("/overview-labels-key-%1.png").arg(QString::fromLatin1(theme))), "Label key capture");
				page->label_key()->close();
				const auto  issue = cell("src/engine/renderer.cpp");
				const auto  chips = qt::overview_comment_chips(*model->feedback_at(issue), page->files_view()->visualRect(issue), page->files_view()->font());
				const auto  point = chips.front().Box.center();
				QMouseEvent press{ QEvent::MouseButtonPress, point, page->files_view()->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
				QMouseEvent release{ QEvent::MouseButtonRelease, point, page->files_view()->viewport()->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
				QApplication::sendEvent(page->files_view()->viewport(), &press);
				QApplication::sendEvent(page->files_view()->viewport(), &release);
				wait_for([&] { return window.current_modality() == qt::review_modality::discuss && window.conversation_panel()->listing().has_value(); }, "the issue chip lists in Discuss");
				capture(window, QStringLiteral("overview-labels-discuss-%1.png").arg(QString::fromLatin1(theme)));
				auto* const settings = window.settings();
				expect(settings != nullptr && editor->window() == settings, "the Overview chips live in Settings");
				settings->present(qt::settings_page::reviews_and_queue);
				settings->resize(900, 700);
				QApplication::processEvents();
				for (auto* area = editor->parentWidget(); area != nullptr; area = area->parentWidget()) {
					if (auto* const scroll = qobject_cast<QScrollArea*>(area)) {
						scroll->ensureWidgetVisible(editor, 0, 160);
						break;
					}
				}
				QApplication::processEvents();
				expect(settings->grab().save(root + QStringLiteral("/overview-labels-settings-%1.png").arg(QString::fromLatin1(theme))), "Settings capture");
				settings->hide();
			}
		}
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "overview-labels course quiescence");
	}

	void run_modified_missing_endpoint_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated missing-endpoint course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private endpoint course root");
		options.ModifiedMissingEndpointCourse = true;
		std::unique_ptr<app::github_application> owner;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "missing endpoint startup");
		expect(owner != nullptr, "missing endpoint application");
		auto& window = owner->window();
		window.show();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "missing endpoint connection");
		submit(window, QStringLiteral("https://github.com/octo-org/review-repository/pull/17"));
		wait_for([&] { return owner->status().EpochStatus == core::epoch_status::ready && window.files_tree()->tree_model()->file_count() == 2U; }, "missing endpoint exact comparison");
		window.modality()->action(qt::review_modality::review)->trigger();
		if (!window.files_drawer_toggle_action()->isChecked()) {
			window.files_drawer_toggle_action()->trigger();
		}
		auto* const tree         = window.files_tree();
		auto* const surface      = window.comparison_surface();
		const auto  intact       = core::file_identity_of("secondary.cpp");
		const auto  missing      = core::file_identity_of("review.cpp");
		const auto  selectIntact = [&] {
			tree->setCurrentIndex(tree->tree_model()->index_for_file(intact));
			wait_for([&] { return surface->current_result() && surface->current_context()
							   && surface->current_context()->Key.File == intact && !surface->geometry_pending(); }, "intact file recovers exact source");
			expect(!surface->left_pane()->toPlainText().isEmpty() && !surface->right_pane()->toPlainText().isEmpty(), "intact source remains available at both selected revisions");
			expect(!window.status_text().contains(QStringLiteral("Comparison unavailable")), "recovery clears the previous selected-file failure");
		};
		selectIntact();
		const auto failures = window.statistics().Failures;
		tree->setCurrentIndex(tree->tree_model()->index_for_file(missing));
		wait_for([&] { return window.statistics().Failures > failures || (surface->current_result() && surface->current_context()->Key.File == missing); }, "missing endpoint reaches terminal instead of indefinite loading");
		capture(window, QStringLiteral("modified-missing-endpoint.png"));
		expect(window.statistics().Failures == failures + 1 && surface->current_result() == nullptr
				   && !surface->current_context() && surface->left_pane()->toPlainText().isEmpty()
				   && surface->right_pane()->toPlainText().isEmpty(),
			   "provider M with absent selected-base path is unavailable, never a successful one-sided diff");
		expect(surface->status_text().contains(QStringLiteral("Comparison unavailable"))
				   && surface->status_text().contains(QStringLiteral("earlier selected revision"))
				   && window.status_text().contains(QStringLiteral("earlier selected revision")),
			   "visible and accessible comparison explains the missing selected material");
		auto* const tabStatus   = window.document_tabs()->status_label();
		const auto  explanation = surface->status_text();
		expect(tabStatus->isVisible() && tabStatus->text() == explanation
				   && tabStatus->toolTip() == explanation && tabStatus->accessibleDescription() == explanation,
			   "compact tab status retains the complete missing-material tooltip and description");
		auto* const status = surface->findChild<QLabel*>(QStringLiteral("diffWorkbenchStatus"));
		expect(status != nullptr && status->isVisible() && status->text() == explanation
				   && status->wordWrap() && status->accessibleDescription() == explanation,
			   "missing-material explanation wraps and remains fully accessible");
		wait_for([&] { return status->height() >= status->heightForWidth(status->width()); },
				 "missing-material layout settles with enough room for the complete explanation");
		auto* const accessible = QAccessible::queryAccessibleInterface(status);
		expect(accessible != nullptr && accessible->text(QAccessible::Description) == explanation
				   && !accessible->state().invisible,
			   "complete missing-material explanation is exposed in the accessible surface");
		expect(status->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
				   && status->textInteractionFlags().testFlag(Qt::TextSelectableByMouse),
			   "missing-material explanation is selectable");
		status->setSelection(0, static_cast<int>(explanation.size()));
		const auto copyKey = QKeySequence{ QKeySequence::Copy }[0];
		auto       copy    = QKeyEvent{ QEvent::KeyPress, copyKey.key(), copyKey.keyboardModifiers() };
		QApplication::clipboard()->clear();
		QApplication::sendEvent(status, &copy);
		expect(QApplication::clipboard()->text() == explanation, "Copy puts the complete missing-material explanation on the clipboard");
		status->setSelection(0, 0);
		capture(window, QStringLiteral("modified-missing-endpoint.png"));
		selectIntact();
		expect(status->isHidden() && tabStatus->isHidden(), "normal recovery removes the explanation and routine tab status");
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "missing endpoint course quiescence");
	}

	/// [[BPR-0987]]. Pasted pull request links with locations, in the
	/// composed window over the two-file recorded pull request: each exact
	/// location is placed through the ordinary gesture paths, each location
	/// that cannot be placed opens the pull request and says why, and a link
	/// to the pull request the window already shows reads nothing again.
	void run_link_location_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated link-location course root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private link-location course root");
		options.RepeatReadSessions = true;
		std::unique_ptr<app::github_application> owner;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "link-location course startup");
		expect(owner != nullptr, "link-location course application");
		auto& window = owner->window();
		window.show();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "link-location course connection");
		const auto  base      = QStringLiteral("https://github.com/octo-org/review-repository/pull/17");
		const auto  digest    = [](const char* aPath) { return QString::fromStdString(core::pull_request_file_digest(aPath)); };
		auto* const surface   = window.comparison_surface();
		const auto  secondary = core::file_identity_of("secondary.cpp");
		const auto  review    = core::file_identity_of("review.cpp");
		const auto  reads     = [&] { return github_application_fixture_access::evidence(*owner).AccountReadCount; };
		const auto  said      = [&](const QString& aText) {
			return window.statusBar()->currentMessage().contains(aText) && window.home_open_notice()->text().contains(aText);
		};

		// A file and its head-side line, opened from Home by a whole link.
		submit(window, base + QStringLiteral("/files#diff-") + digest("secondary.cpp") + QStringLiteral("R2"));
		wait_for([&] { return owner->status().EpochStatus == core::epoch_status::ready && window.files_tree()->tree_model()->file_count() == 2U; }, "link-location exact comparison");
		wait_for([&] {
			const auto selection = surface->selection();
			return surface->current_context() && surface->current_context()->Key.File == secondary && selection.has_value()
				&& selection->PreferredLine.Side == core::diff_side::right && selection->PreferredLine.Line == 1U;
		},
				 "a file link with a head-side line selects exactly that file and line");
		expect(!window.showing_home() && window.current_modality() == qt::review_modality::review, "a file location is shown in Review");
		expect(!window.statusBar()->currentMessage().contains(QStringLiteral("unavailable")), "an exact location says nothing");
		const auto opened = reads();

		// The same pull request again, another file: navigated, not read again.
		submit(window, base + QStringLiteral("#diff-") + digest("review.cpp"));
		wait_for([&] { return surface->current_context() && surface->current_context()->Key.File == review; }, "a link to the open pull request moves to its file");
		expect(reads() == opened && window.session_panel()->status() == core::github_status::pull_request_ready, "a link to the pull request the window shows reads nothing again");

		// A review the read identified is revealed in Discuss.
		submit(window, base + QStringLiteral("#pullrequestreview-4001"));
		wait_for([&] { return window.current_modality() == qt::review_modality::discuss
						   && window.conversation_view()->currentIndex().data(qt::github_conversation_model::identity_role).toString().startsWith(QStringLiteral("review:")); },
				 "a review link reveals that review");
		expect(reads() == opened, "revealing a review reads nothing again");

		// Locations that cannot be placed open the pull request and say why.
		submit(window, base + QStringLiteral("#discussion_r123456"));
		wait_for([&] { return said(QStringLiteral("That comment is not identified")) && said(QStringLiteral("#discussion_r123456")); },
				 "a comment the authenticated read names by node identity alone is explained");
		submit(window, base + QStringLiteral("/files#diff-") + digest("secondary.cpp") + QStringLiteral("L2"));
		wait_for([&] { return surface->current_context() && surface->current_context()->Key.File == secondary && said(QStringLiteral("requested line is unavailable"))
						   && said(QStringLiteral("base side")); },
				 "a base-side line opens its file and explains the line");
		submit(window, base + QStringLiteral("/commits/0123456789abcdef"));
		wait_for([&] { return said(QStringLiteral("A single commit is not shown")); }, "a commit page is explained, not mapped to another revision");
		submit(window, base + QStringLiteral("/files#diff-") + digest("absent.cpp"));
		wait_for([&] { return said(QStringLiteral("No changed file")); }, "a digest naming no changed file substitutes nothing");
		submit(window, base + QStringLiteral("#issuecomment-99"));
		wait_for([&] { return said(QStringLiteral("discussion comment")); }, "a discussion comment link is explained");
		expect(reads() == opened && surface->current_context() && surface->current_context()->Key.File == secondary,
			   "no explained location reads again or moves the review");

		// A repository commit link names no pull request: nothing is guessed.
		submit(window, QStringLiteral("https://github.com/octo-org/review-repository/commit/0123456789abcdef"));
		expect(window.home_open_notice()->text().contains(QStringLiteral("not a supported pull request link")) && reads() == opened
				   && window.session_panel()->status() == core::github_status::pull_request_ready,
			   "a non-PR link opens nothing and keeps the review");
		capture(window, QStringLiteral("link-location-refusal.png"));
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "link-location course quiescence");
	}

	void run_course(const bool aEnterprise)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated application store");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");
		options.SetupUnconfiguredEnterprise = aEnterprise;
		options.RegisteredDeviceCourse      = !aEnterprise;
		// [[BPR-0732]]. The GitHub.com course composes the anonymous
		// authority: a pasted public link with no account opens signed out.
		options.PublicPreviewEnabledCourse = !aEnterprise;
		options.RepeatReadSessions         = true;
		std::unique_ptr<bpr::app::github_application> application;
		auto                                          startupTerminal = false;
		auto                                          startup         = github_application_fixture_access::start(aEnterprise ? app::detail::github_public_path_fixture::ghes_3_18 : app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			startupTerminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return startupTerminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(application != nullptr, "application startup succeeds");
		auto& window = application->window();
		window.show();
		QApplication::processEvents();
		const auto url = aEnterprise ? QStringLiteral("https://ghe.example.invalid/octo-org/review-repository/pull/17/files#diff-abcd") : QStringLiteral("https://github.com/octo-org/review-repository/pull/17");
		if (!aEnterprise) {
			// [[BPR-0732]]. One gesture, signed out: no sign-in stop, no
			// continuation press, no credential request, no vault write.
			expect(!window.home_landing()->isHidden() && window.home_landing()->state() == qt::landing_state::sign_in && !window.home_list()->queue_shown(), "the signed-out Home has no queue");
			expect(application->windows().size() == 1U, "public opening owns one window");
			const auto context = application->windows().front();
			github_application_fixture_access::hold_window_read_terminal(*application, context, true);
			submit(window, url);
			wait_for([&] { return github_application_fixture_access::held_window_read_terminals(*application, context) != 0U; },
					 "the real provider read reaches its held decision lane");
			// [[BPR-1200]]. The held lane withholds every answer, so nothing yet
			// shows that this connection can read the pull request: the opening
			// is said under the Home bar rather than on an empty Overview.
			expect(window.showing_home() && !window.overview_page()->isVisible()
					   && window.home_open_notice()->text() == QStringLiteral("Loading the pull request.")
					   && window.session_panel()->status() == core::github_status::loading_pull_request
					   && !window.overview_page()->presented(),
				   "a real pending setup opening is said on Home until GitHub has answered for it");
			capture(window, QStringLiteral("setup-loading-home.png"));
			github_application_fixture_access::hold_window_read_terminal(*application, context, false);
			// [[BPR-0795]]. Its first answers take it to Overview's loading
			// state while the rest of the read continues.
			auto* readiness = window.epoch_selectors()->comparing_button();
			wait_for([&] { return !window.showing_home() || window.session_panel()->status() == core::github_status::pull_request_ready; },
					 "the answered opening leaves Home");
			expect(!window.showing_home() && window.overview_page()->isVisible()
					   && window.overview_page()->width() > window.width() * 3 / 4 && readiness != nullptr && readiness->isVisible(),
				   "the answered opening shows Overview");
			expect(button(window, "homeSetupContinue")->isHidden(), "a public host with no account offers no sign-in stop");
			wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "a pasted public link opens signed out with no sign-in step");
			auto evidence = github_application_fixture_access::evidence(*application);
			expect(evidence.VaultStores == 0U && evidence.VaultRetrieveAttempts == 0U && !evidence.AccountTransportSelected && !evidence.CredentialStored && !evidence.CredentialRetrieved, "the signed-out open sends no credential and consults no vault");
			expect(application->status().PublicPreviewSignedOut, "the accepted session is public preview");
			expect(window.home_open_bar()->text().isEmpty(), "accepted opening clears input");
			expect(window.session_panel()->active_host_text().find("github.com") != std::string::npos, "accepted review belongs to exact requested host");
			submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
			cancel_setup(window);
			expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 0U, "cancelled unknown target preserves current review");
			application->begin_shutdown();
			wait_for([&] { return application->status().ShutdownComplete; }, "setup and review work quiesce");
			return;
		}
		// [[BPR-0772]]. RFC-BPR-0032 A1: one card per configured server. The
		// composition configures GitHub.com over native HTTPS and over GitHub
		// CLI, and both are ways in to the one github.com card.
		{
			const auto cards = window.server_cards();
			expect(cards.size() == 1U && cards.front()->hostname() == "github.com" && cards.front()->options().size() == 2U
					   && cards.front()->native_option() != nullptr && cards.front()->cli_option() != nullptr
					   && cards.front()->cli_offer_button() != nullptr && cards.front()->host_label()->text() == QStringLiteral("github.com"),
				   "the composed GitHub.com host profiles share one server card");
		}
		submit(window, url);
		capture(window, QStringLiteral("setup-unknown.png"));
		expect(window.home_open_bar()->text() == url, "pending setup retains the exact pasted target");
		expect(window.sign_in_dialog()->isVisible() && !window.settings()->isVisible() && button(window, "homeSetupContinue")->isHidden(), "pasted PR uses the shared dialog without a continuation button or Settings");
		expect(window.enterprise_server_hostname()->text() == QStringLiteral("ghe.example.invalid"), "pasted host prefills the server editor");
		auto evidence = github_application_fixture_access::evidence(*application);
		expect(evidence.VaultStores == 0U && !evidence.BootstrapTransportSelected && !evidence.AccountTransportSelected, "parsing sends no credentials or provider reads");
		cancel_setup(window);
		expect(window.home_open_bar()->text() == url && button(window, "homeSetupContinue")->isHidden(), "cancel retains editable input and dismisses setup");
		submit(window, url);
		if (aEnterprise) {
			window.enterprise_server_add()->click();
			wait_for([&] { return window.server_cards().size() == 2U; }, "durable Add server makes the exact host available without restart");
			evidence = github_application_fixture_access::evidence(*application);
			expect(evidence.VaultStores == 0U && !evidence.BootstrapTransportSelected, "configuration alone never signs in");
			// [[BPR-0772]]. The added server is a second card; it joins no other.
			const auto cards = window.server_cards();
			expect(cards.size() == 2U && cards.back()->hostname() == "ghe.example.invalid" && cards.back()->options().size() == 1U
					   && cards.front()->options().size() == 2U,
				   "an added enterprise server appends its own card");
		}

		if (aEnterprise) {
			connect_native(window);
			cancel_setup(window);
			submit(window, url);
			connect_native(window);
			submit(window, QStringLiteral("https://replacement.example.invalid/o/r/pull/99"));
			expect(button(window, "homeSetupContinue")->text() == QStringLiteral("Add server and continue"), "a new target replaces queued authentication without reusing its authority");
			submit(window, url);
		}
		connect_native(window);
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "accepted exact setup opens the recorded pull request");
		evidence = github_application_fixture_access::evidence(*application);
		expect(evidence.VaultStores == 1U && evidence.BootstrapTransportSelected && evidence.AccountTransportSelected && evidence.RecordingConsumed, "public setup traverses one exact recorded authenticated read course");
		expect(window.home_open_bar()->text().isEmpty(), "accepted opening clears input");
		expect(window.session_panel()->active_host_text().find(aEnterprise ? "ghe.example.invalid" : "github.com") != std::string::npos, "accepted review belongs to exact requested host");

		if (aEnterprise) {
			capture(window, QStringLiteral("setup-accepted.png"));
		}
		if (aEnterprise) {
			// [[BPR-0987]]. The receiving window decides once it has accepted
			// the pull request; `#diff-abcd` names no file digest, so the
			// location is explained, never guessed.
			wait_for([&] { return window.home_open_notice()->text().contains(QStringLiteral("location is unavailable")) && window.statusBar()->currentMessage().contains(QStringLiteral("location is unavailable"))
							   && window.statusBar()->currentMessage().contains(QStringLiteral("/files#diff-abcd")); },
					 "unmapped exact fragment has an explanation on the accepted review");
		}
		submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
		cancel_setup(window);
		expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 1U, "cancelled unknown target preserves current review and credential authority");

		if (aEnterprise) {
			submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
			window.enterprise_server_add()->click();
			wait_for([&] { return window.server_cards().size() == 3U; }, "second explicit server configured");
			expect(window.server_cards().size() == 3U && window.server_cards().back()->hostname() == "other.example.invalid",
				   "a second explicit server is a third card");
			connect_native(window, "other.example.invalid");
			wait_for([&] { return window.home_open_notice()->text().contains(QStringLiteral("Sign-in did not complete")); }, "recorded transport refuses unsupported authority before commitment");
			expect(window.session_panel()->status() == core::github_status::pull_request_ready && github_application_fixture_access::evidence(*application).VaultStores == 1U, "failed staged setup preserves accepted review and existing vault material");
			// [[BPR-0774]]. RFC-BPR-0032 A3 through the shipped composition. The
			// enterprise server added above is kept, so its card remembers that
			// record; Forget confirms, names it, and removes exactly it through
			// the queue host; Remember keeps it again. No credential moves.
			window.open_settings(qt::settings_page::accounts_and_hosts);
			QApplication::processEvents();
			qt::server_card* kept = nullptr;
			for (auto* const candidate : window.server_cards()) {
				if (candidate->hostname() == "ghe.example.invalid") {
					kept = candidate;
				}
			}
			expect(kept != nullptr && kept->enterprise(), "the added enterprise server has a card");
			wait_for([&] { return kept->remember_switch()->isVisibleTo(kept) && kept->remember_switch()->isChecked() && kept->forget_button()->isVisibleTo(kept); }, "the kept enterprise server's card remembers it and offers Forget server");
			const auto record = kept->presentation().Remembered;
			expect(record.has_value() && record->Hostname == "ghe.example.invalid" && std::ranges::find(window.kept_servers(), *record) != window.kept_servers().end(), "the card remembers exactly the record the queue keeps");
			const auto vaultStores = github_application_fixture_access::evidence(*application).VaultStores;
			kept->forget_button()->click();
			QApplication::processEvents();
			auto* confirmation = window.forget_server_confirmation();
			expect(confirmation != nullptr && confirmation->isVisible() && confirmation->parentWidget() == window.settings() && (confirmation->text() + QLatin1Char{ ' ' } + confirmation->informativeText()).contains(QStringLiteral("ghe.example.invalid")), "Forget server confirms first and names the server it removes");
			confirmation->findChild<QPushButton*>(QStringLiteral("forgetServerCancel"))->click();
			QApplication::processEvents();
			expect(window.forget_server_confirmation() == nullptr || !window.forget_server_confirmation()->isVisible(), "Cancel closes the confirmation");
			expect(kept->remember_switch()->isChecked() && std::ranges::find(window.kept_servers(), *record) != window.kept_servers().end(), "Cancel performs no effect");
			kept->forget_button()->click();
			QApplication::processEvents();
			confirmation = window.forget_server_confirmation();
			expect(confirmation != nullptr && confirmation->isVisible(), "Forget server confirms again");
			confirmation->findChild<QPushButton*>(QStringLiteral("forgetServerConfirm"))->click();
			wait_for([&] { return std::ranges::find(window.kept_servers(), *record) == window.kept_servers().end() && !kept->remember_switch()->isChecked() && !kept->forget_button()->isVisibleTo(kept); }, "Forget server removes exactly the kept record through the queue host");
			kept->remember_switch()->click();
			wait_for([&] { return kept->remember_switch()->isChecked() && std::ranges::any_of(window.kept_servers(), [](const application::saved_server& aServer) { return aServer.Hostname == "ghe.example.invalid"; }); }, "Remember this server keeps it again through the queue host");
			expect(github_application_fixture_access::evidence(*application).VaultStores == vaultStores, "keeping and forgetting a server enrols no credential");
			window.settings()->hide();
		}
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "setup and review work quiesce");
	}

#if defined(__APPLE__)
	/// \brief [[BPR-1200]]. Every central page the composed window shows, from
	/// the moment it is watched: the course's evidence that a refused read
	/// never passed through the workbench.
	class central_passage final
	{
	public:
		explicit central_passage(qt::workbench_window& aWindow)
			: Stack{ aWindow.findChild<QStackedWidget*>(QStringLiteral("workbenchCentralStack")) }
		{
			expect(Stack != nullptr && Stack->currentWidget() != nullptr, "the composed window has its central stack");
			Pages.push_back(Stack->currentWidget()->objectName());
			Watch = QObject::connect(Stack, &QStackedWidget::currentChanged, Stack, [this](int) {
				Pages.push_back(Stack->currentWidget()->objectName());
			});
		}
		~central_passage() { QObject::disconnect(Watch); }
		central_passage(const central_passage&)                             = delete;
		auto operator=(const central_passage&) -> central_passage&          = delete;
		central_passage(central_passage&&)                                  = delete;
		auto               operator=(central_passage&&) -> central_passage& = delete;
		[[nodiscard]] auto stayed_home() const -> bool
		{
			return std::ranges::all_of(Pages, [](const QString& aPage) { return aPage == QStringLiteral("workbenchHome"); });
		}
		[[nodiscard]] auto pages() const -> const QStringList& { return Pages; }

	private:
		QStackedWidget*         Stack = nullptr;
		QStringList             Pages;
		QMetaObject::Connection Watch;
	};

	/// \brief [[BPR-1200]]. The owner's case through the composition: signed
	/// out, a link to a pull request this connection cannot read. The window
	/// stays on Home throughout and the refusal card says to sign in.
	void check_signed_out_refusal_stays_on_home(app::github_application& aApplication)
	{
		auto& window = aApplication.window();
		expect(!window.session_panel()->session().has_value() || aApplication.status().PublicPreviewSignedOut,
			   "the signed-out refusal course begins without an account");
		if (!window.showing_home()) {
			window.home_action()->trigger();
			QApplication::processEvents();
		}
		const central_passage passage{ window };
		submit(window, QStringLiteral("https://github.com/octo-org/inaccessible-repository/pull/17"));
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; },
				 "the signed-out read of the inaccessible repository is refused",
				 [&] { std::cerr << "pages: " << passage.pages().join(QLatin1Char{ ',' }).toStdString() << " status: " << window.session_panel()->status_text() << '\n'; });
		QApplication::processEvents();
		expect(passage.stayed_home(), "signed out, the refused read never passes through the workbench");
		expect(aApplication.status().PublicPreviewSignedOut, "the refused read was read signed out");
		auto* const card = window.home_refusal_card();
		expect(window.showing_home() && card != nullptr && card->isVisible()
				   && card->heading_label()->text() == QStringLiteral("Sign in to open octo-org/inaccessible-repository#17")
				   && card->explanation_label()->text().contains(QStringLiteral("not signed in"))
				   && !card->explanation_label()->text().contains(QStringLiteral("browser")),
			   "Home's card names the pull request, says the app is signed out and says to sign in");
		const auto offered = card->offered_buttons();
		expect(!offered.empty() && offered.front()->text() == QStringLiteral("Sign in") && window.home_open_notice()->isHidden(),
			   "the card offers Sign in first, and the line under the bar gives way to it");
		capture(window, QStringLiteral("setup-denied-signed-out.png"));
	}

	void check_denied_read_retains_session(app::github_application& aApplication, const QString& aHostname)
	{
		auto&      window  = aApplication.window();
		const auto session = window.session_panel()->session();
		const auto before  = github_application_fixture_access::evidence(aApplication);
		expect(session.has_value() && !aApplication.status().PublicPreviewSignedOut, "the refusal course begins with an accepted account");
		const auto sameAccount = [&session](const auto& aSession) {
			return aSession && aSession->Profile == session->Profile && aSession->Host == session->Host
				&& aSession->Account == session->Account && aSession->Login == session->Login;
		};
		if (!window.showing_home()) {
			window.home_action()->trigger();
			QApplication::processEvents();
		}
		auto passage = std::make_unique<central_passage>(window);
		submit(window, QStringLiteral("https://%1/octo-org/inaccessible-repository/pull/17").arg(aHostname));
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "the inaccessible repository reaches a read refusal");
		QApplication::processEvents();
		// [[BPR-1200]]. The composed refusal is a permission denial on this
		// account, said in Home's card, and the window never left Home.
		expect(passage->stayed_home(), "an account's refused read never passes through the workbench");
		auto* const refusal = window.home_refusal_card();
		expect(refusal != nullptr && refusal->isVisible()
				   && refusal->heading_label()->text().endsWith(QStringLiteral(" cannot open octo-org/inaccessible-repository#17"))
				   && !refusal->explanation_label()->text().contains(QStringLiteral("not signed in"))
				   && !refusal->offered_buttons().empty()
				   && refusal->offered_buttons().front()->text() == QStringLiteral("Use another account"),
			   "the composed refusal is a permission denial on this account, and the card offers another account");
		expect(sameAccount(window.session_panel()->session()) && !aApplication.status().PublicPreviewSignedOut
				   && window.home_landing()->state() == qt::landing_state::choose_pull_request,
			   "a denied read preserves the accepted account and signed-in Home");
		const auto cards = window.server_cards();
		const auto card  = std::ranges::find_if(cards, [&](const auto* aCard) { return aCard->hostname() == aHostname.toStdString(); });
		expect(card != cards.end() && (*card)->presentation().State == qt::server_card_state::connected
				   && sameAccount((*card)->presentation().Session),
			   "Accounts & hosts retains the same accepted account after the denial");
		const auto after = github_application_fixture_access::evidence(aApplication);
		expect(after.VaultStoreAttempts == before.VaultStoreAttempts && after.VaultRetrieveAttempts == before.VaultRetrieveAttempts
				   && after.VaultDeleteAttempts == before.VaultDeleteAttempts && after.RefreshStoreAttempts == before.RefreshStoreAttempts
				   && after.RefreshRetrieveAttempts == before.RefreshRetrieveAttempts && after.RefreshDeleteAttempts == before.RefreshDeleteAttempts
				   && after.DeviceTransportSubmissions == before.DeviceTransportSubmissions && after.AccountFactoryCalls == before.AccountFactoryCalls,
			   "a denied read neither touches credential slots nor starts another device or account session");
		capture(window, QStringLiteral("setup-denied-%1.png").arg(session->Grant ? QStringLiteral("native") : QStringLiteral("cli")));
		// The control: a pull request this account can read presents the
		// workbench, and the same watch records it, so the predicate above
		// would have failed a read that passed through Overview.
		passage = std::make_unique<central_passage>(window);
		submit(window, QStringLiteral("https://%1/octo-org/review-repository/pull/17").arg(aHostname));
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "the same session opens an accessible review after the refusal");
		QApplication::processEvents();
		expect(!passage->stayed_home() && passage->pages().contains(QStringLiteral("workbenchReviewSurface")) && !refusal->isVisible(),
			   "control: a readable pull request presents the workbench, which the same watch records");
		expect(sameAccount(window.session_panel()->session()), "retrying an accessible target preserves the account");
		expect(!window.sign_in_dialog()->isVisible(), "retrying an accessible target requires no sign-in");
	}

	void run_native_read_refusal_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated native refusal root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private native refusal root");
		options.RegisteredDeviceCourse     = true;
		options.PersistDeviceBindings      = true;
		options.IndependentWindowReads     = true;
		options.PublicPreviewEnabledCourse = true;
		std::unique_ptr<app::github_application> application;
		auto                                     terminal = false;
		auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "native refusal startup completes");
		expect(application != nullptr, "native refusal startup succeeds");
		auto& window = application->window();
		window.show();
		window.home_action()->trigger();
		QApplication::processEvents();
		check_signed_out_refusal_stays_on_home(*application);
		expect(window.home_landing()->primary_button()->isVisible() && window.home_landing()->primary_button()->isEnabled(), "native sign-in is offered before any PR read");
		window.home_landing()->primary_button()->click();
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "synthetic device account is accepted before any PR read");
		if (window.sign_in_dialog()->isVisible()) {
			cancel_setup(window);
		}
		check_denied_read_retains_session(*application, QStringLiteral("github.com"));
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "native refusal course quiesces");
	}

	void run_connected_cli_destination_course(const bool aPasteFirst, const bool aUnknownHost = false, const bool aGithubCom = false)
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated connected CLI root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private connected CLI root");
		options.SetupUnconfiguredEnterprise = aUnknownHost;
		options.IndependentWindowReads      = true;
		options.PublicPreviewEnabledCourse  = aGithubCom;
		const auto hostname                 = aGithubCom ? "github.com" : "ghe.example.invalid";
		options.GithubCliDiscoveryAccounts  = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
			{ .Hostname = hostname, .Login = "octo-reviewer" }
		};
		std::unique_ptr<bpr::app::github_application> application;
		auto                                          terminal = false;
		auto                                          startup  = github_application_fixture_access::start(aGithubCom ? app::detail::github_public_path_fixture::github_com : app::detail::github_public_path_fixture::ghes_3_18, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "connected CLI startup completes");
		expect(application != nullptr, "connected CLI startup succeeds");
		auto& window = application->window();
		window.show();
		window.home_action()->trigger();
		QApplication::processEvents();
		expect(window.home_landing()->secondary_button()->isVisible() && window.home_landing()->secondary_button()->isEnabled(), "CLI is reachable before any device connection");
		// A canceled inventory must not present choices or connect on late completion.
		window.home_landing()->secondary_button()->click();
		cancel_setup(window);
		auto   confirmations = 0U;
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				++confirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		const auto url = QStringLiteral("https://%1/octo-org/review-repository/pull/17").arg(QString::fromLatin1(hostname));
		if (aPasteFirst) {
			submit(window, url);
			window.use_github_cli_button()->click();
		} else {
			window.home_landing()->secondary_button()->click();
			wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "one Home CLI press connects after one confirmation");
			submit(window, url);
		}

		expect(button(window, "homeSetupContinue")->isHidden(), "connected CLI paste does not offer native sign-in");
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "pasted Enterprise link reaches the PR through the accepted CLI session");
		const auto cards = window.server_cards();
		auto*      card  = *std::ranges::find_if(cards, [&](const auto* aCard) { return aCard->hostname() == hostname; });
		expect(card->cli_option() != nullptr, "discovered CLI profile is installed");
		const auto cliProfile = card->cli_option()->Profile;
		consent.stop();
		expect(confirmations == 1U && !window.settings()->isVisible() && !window.sign_in_dialog()->isVisible(), "CLI setup needs one confirmation and closes the focused dialog");
		const auto evidence = github_application_fixture_access::evidence(*application);
		expect(window.session_panel()->selected_profile() == cliProfile && evidence.AccountTransportSelected, "exact CLI profile serves the recorded PR");
		expect(evidence.VaultStores == 0U && evidence.VaultRetrieveAttempts == 0U && !evidence.BootstrapTransportSelected, "CLI link opens without native credential access");
		expect(evidence.DeviceTransportSubmissions == 0U && !application->status().PublicPreviewSignedOut, "CLI opens through the accepted account without device sign-in or anonymous fallback");
		capture(window, QStringLiteral("setup-connected-cli.png"));
		if (aGithubCom) {
			check_denied_read_retains_session(*application, QString::fromLatin1(hostname));
		}
		submit(window, QStringLiteral("https://other.example.invalid/o/r/pull/99"));
		expect(button(window, "homeSetupContinue")->text() == QStringLiteral("Add server and continue"), "another host cannot reuse CLI authentication");
		cancel_setup(window);
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "connected CLI work quiesces");
	}

	/// \brief Reads the recorded PR-wide discussion without selecting code.
	void read_cli_source_update_discussion(qt::workbench_window& aWindow)
	{
		aWindow.resize(1440, 900);
		aWindow.modality()->action(qt::review_modality::discuss)->trigger();
		auto* const panel = aWindow.conversation_panel();
		wait_for([&] { return panel->isVisible() && panel->discuss(); },
				 "Discuss opens independently of a comparison");
		expect(panel->scope_whole_pull_request_button()->isChecked(),
			   "Discuss defaults to the whole PR");
		const auto expectConversation = [&](const std::string_view                      aId,
											const application::github_conversation_kind aKind,
											const QString& aPath, const QString& aBody) {
			const auto index = panel->model()->index_for_id(aId);
			expect(index.isValid()
					   && index.data(qt::github_conversation_model::kind_role).toInt() == static_cast<int>(aKind)
					   && index.data(qt::github_conversation_model::path_role).toString() == aPath
					   && index.data(qt::github_conversation_model::body_role).toString() == aBody,
				   "Discuss admits the exact recorded conversation identity, kind, path and body");
			// [[BPR-1290]]. Discuss lists the conversation as a note that opens
			// in place to its posts as written.
			auto* const cards = panel->cards();
			const auto  note  = std::ranges::find_if(cards->cards(), [aId](const qt::thread_card& aCard) {
				return std::ranges::any_of(aCard.Posts, [aId](const qt::card_post& aPost) { return aPost.Id == aId; });
			});
			expect(note != cards->cards().end(), "the recorded conversation is a note in Discuss");
			cards->open_thread(note->Id);
			QApplication::processEvents();
			auto* const open = cards->open_card();
			auto*       body = static_cast<QTextEdit*>(nullptr);
			for (auto* const post : open != nullptr ? open->findChildren<QWidget*>(QStringLiteral("threadPost")) : QList<QWidget*>{ }) {
				if (post->property("postIdentity").toString() == QString::fromUtf8(aId.data(), static_cast<qsizetype>(aId.size()))) {
					body = post->findChild<QTextEdit*>(QStringLiteral("threadPostBody"));
				}
			}
			expect(body != nullptr && body->isVisible() && body->toPlainText() == aBody,
				   "recorded conversation body is reachable in the visible open thread");
		};
		expectConversation("issue-comment:IC_fixture_1", application::github_conversation_kind::issue_comment,
						   { }, QStringLiteral("Fixture follow-up: validation is complete."));
		expectConversation("review:PRR_fixture_1", application::github_conversation_kind::review,
						   { }, QStringLiteral("Please preserve snapshot identity across refreshes."));
		expectConversation("review-comment:PRRC_fixture_reading_review.cpp", application::github_conversation_kind::review_comment,
						   QStringLiteral("review.cpp"), QStringLiteral("Check the exact retained reading position."));
	}

	void run_cli_source_update_course()
	{
		auto reviewServer = std::make_shared<app::detail::review_submission_fixture_state>();
		auto temporary    = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated CLI source/update root");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private CLI source/update root");
		options.EpochPhase                = app::detail::github_epoch_fixture_phase::single;
		options.GhesCliSourceUpdateCourse = true;
		options.ReviewSubmissionServer    = reviewServer;
		options.RefuseFirstSmartAcquire   = true;
		options.IndependentWindowReads    = true;
		struct catalogue_gate
		{
			std::mutex                  Mutex;
			std::condition_variable_any Changed;
			std::atomic_bool            Reached{ false };
			bool                        Released = false;
		};
		auto gate               = std::make_shared<catalogue_gate>();
		options.BeforeFilesPage = [gate](const std::stop_token stop) {
			gate->Reached = true;
			std::unique_lock lock{ gate->Mutex };
			static_cast<void>(gate->Changed.wait_for(lock, stop, std::chrono::seconds{ 20 }, [&] { return gate->Released; }));
		};
		options.GithubCliDiscoveryAccounts
			= std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
				  { .Hostname = "ghe.example.invalid", .Login = "octo-reviewer" },
			  };

		std::unique_ptr<bpr::app::github_application> application;
		auto                                          terminal = false;
		auto                                          startup  = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::ghes_3_18,
			options,
			[&](auto aResult) {
				terminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return terminal && startup && startup->is_quiescent(); },
			"CLI source/update startup completes");
		expect(application != nullptr, "CLI source/update startup succeeds");
		auto& window = application->window();
		window.show();
		QApplication::processEvents();
		verify_composed_code_metrics(*application, 4U);
		for (const auto width : { 2U, 8U, 4U }) {
			expect(github_application_fixture_access::apply_appearance_gesture(
					   *application, core::appearance_mode::dark, width),
				   "the composed Settings control accepts each offered tab width");
			QApplication::processEvents();
			verify_composed_code_metrics(*application, width);
		}
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::high_contrast_light,
								 core::appearance_mode::high_contrast_dark, core::appearance_mode::dark }) {
			expect(github_application_fixture_access::apply_appearance_gesture(*application, mode, 4U),
				   "Settings applies each drawer appearance through the concrete application");
			QApplication::processEvents();
			verify_composed_file_operation_ink(*application, mode);
		}

		// Discard the first setup surface while the CLI inventory settles. The
		// second gesture must use the discovered, explicitly confirmed profile.
		window.home_landing()->secondary_button()->click();
		cancel_setup(window);
		auto   confirmations = 0U;
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget())) {
				++confirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		window.home_landing()->secondary_button()->click();
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (window.session_panel()->status()
				   != core::github_status::session_ready
			   && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (window.session_panel()->status()
			!= core::github_status::session_ready) {
			const auto evidence = github_application_fixture_access::evidence(
				*application);
			std::cerr << "CLI source/update connect status="
					  << static_cast<int>(window.session_panel()->status())
					  << " text=" << window.session_panel()->status_text()
					  << " diagnostic="
					  << window.session_panel()->diagnostic_text()
					  << " confirmations=" << confirmations
					  << " account-factory=" << evidence.AccountFactoryCalls
					  << " reads=" << evidence.AccountReadCount
					  << " recording-failure=" << evidence.RecordingFailure
					  << '\n';
		}
		expect(
			window.session_panel()->status()
				== core::github_status::session_ready,
			"CLI source/update profile connects");
		const auto pullRequestUrl = QStringLiteral(
			"https://ghe.example.invalid/octo-org/review-repository/pull/17");
		auto loadElapsed = QElapsedTimer{ };
		loadElapsed.start();
		submit(window, pullRequestUrl);
		wait_for([&] {
			const auto* summary = window.findChild<QLabel*>(QStringLiteral("overviewSummary"));
			return gate->Reached && summary != nullptr && summary->text().contains(QStringLiteral("Provisional metadata"));
		},
				 "composed CLI course paints useful metadata while the first file page is still held");
		capture(window, QStringLiteral("setup-cli-progressive-overview.png"));
		const auto firstUsefulMs = loadElapsed.elapsed();
		expect(!window.overview_page()->presented() && !window.overview_page()->entry_button()->isEnabled()
				   && window.overview_page()->file_model()->rowCount() == 0
				   && window.session_panel()->status() == core::github_status::loading_pull_request
				   && github_application_fixture_access::evidence(*application).SmartAcquireCount == 0U,
			   "provisional paint supplies no file selection, snapshot or Git acquisition authority");
		{
			std::lock_guard lock{ gate->Mutex };
			gate->Released = true;
		}
		gate->Changed.notify_all();
		wait_for(
			[&] {
				return window.session_panel()->status()
					== core::github_status::pull_request_ready;
			},
			"CLI source/update PR opens");
		std::cout << "PROGRESSIVE_OVERVIEW first_useful_ms=" << firstUsefulMs
				  << " terminal_ms=" << loadElapsed.elapsed() << " course=recorded_cli_first_file_page_gate\n";
		consent.stop();
		expect(confirmations == 1U, "CLI source/update profile asks for one confirmation");

		const auto  cards = window.server_cards();
		auto* const card  = *std::ranges::find_if(
			cards,
			[](const auto* const aCard) {
				return aCard->hostname() == "ghe.example.invalid";
			});
		expect(card->cli_option() != nullptr, "CLI source/update profile remains explicit");
		const auto cliProfile = card->cli_option()->Profile;
		expect(
			window.session_panel()->selected_profile() == cliProfile,
			"CLI source/update PR belongs to the discovered CLI profile");
		wait_for(
			[&] {
				const auto status = application->status().EpochStatus;
				return status == core::epoch_status::unavailable
					|| status == core::epoch_status::failed;
			},
			"one-shot smart-Git refusal reaches a settled update state");
		expect(!window.files_drawer()->contents_view()->currentIndex().isValid()
				   && window.comparison_surface()->current_result() == nullptr,
			   "refused PR has neither a selected file nor comparison before Discuss");
		read_cli_source_update_discussion(window);
		window.modality()->action(qt::review_modality::review)->trigger();
		auto* const files  = window.files_drawer()->contents_view();
		const auto  review = window.files_drawer()->contents()->index_for_file(
			core::file_identity_of("review.cpp"));
		const auto secondary = window.files_drawer()->contents()->index_for_file(
			core::file_identity_of("secondary.cpp"));
		expect(
			review.isValid() && secondary.isValid(),
			"refused comparison still presents both exact recorded file identities");
		files->selectionModel()->setCurrentIndex(
			review,
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait_for(
			[&] {
				return window.status_text().contains(
					QStringLiteral("No exact comparison is ready"));
			},
			"missing comparison presents actionable recovery");
		auto* const surface         = window.comparison_surface();
		auto        initialEvidence = github_application_fixture_access::evidence(*application);
		expect(
			initialEvidence.SmartAcquireRefusalCount == 1
				&& initialEvidence.SmartAcquireCount == 0
				&& initialEvidence.AccountTransportSelected
				&& !initialEvidence.BootstrapTransportSelected
				&& initialEvidence.VaultStores == 0U
				&& !window.status_text().contains(
					QStringLiteral("selected file changed"),
					Qt::CaseInsensitive)
				&& surface->current_result() == nullptr
				&& surface->left_pane()->toPlainText().isEmpty()
				&& surface->right_pane()->toPlainText().isEmpty(),
			"CLI refusal is unavailable, not stale, and retains no source");
		expect(window.epoch_selectors()->line()->empty_status() == window.epoch_selectors()->status_text()
				   && window.epoch_selectors()->line()->empty_status() != QStringLiteral("No updates loaded."),
			   "the compact public updates line exposes the actual comparison refusal");

		expect(
			window.github_refresh_action()->isEnabled(),
			"unavailable update offers explicit recovery");
		application->begin_shutdown();
		wait_for(
			[&] { return application->status().ShutdownComplete; },
			"refused CLI source/update work quiesces");
		application.reset();
		startup.reset();

	#if defined(Q_OS_MACOS)
		// Reproduce the attended failure through the installed cache and real
		// public composition, not a fake cache result. The refused acquisition
		// above left a pristine repository and no accepted epoch.
		auto manifestCount = 0U;
		for (const auto& entry : std::filesystem::directory_iterator{ *options.ApplicationDataRoot / "repositories/v1" }) {
			const auto manifestPath = entry.path() / "betterpullreview-cache-manifest-v1";
			if (!std::filesystem::is_regular_file(manifestPath)) {
				continue;
			}
			++manifestCount;
			auto input = std::ifstream{ manifestPath, std::ios::binary };
			auto bytes = std::vector<char>{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
			expect(bytes.size() == 141U && bytes[11U] == 2, "public CLI seed has the durable Mac cache manifest");
			input.close();
			bytes[11U] = 1;
			bytes.resize(bytes.size() - 24U);
			struct stat lock{ };
			const auto  lockPath = *options.ApplicationDataRoot / "locks/v1" / (entry.path().stem().string() + ".lock");
			expect(::lstat(lockPath.c_str(), &lock) == 0, "public CLI fixture observes its exact lock");
			for (const auto value : { static_cast<std::uint64_t>(lock.st_dev) ^ 1U, std::uint64_t{ 0 }, static_cast<std::uint64_t>(lock.st_ino) }) {
				for (int shift = 56; shift >= 0; shift -= 8) {
					bytes.push_back(static_cast<char>(value >> shift));
				}
			}
			auto output = std::ofstream{ manifestPath, std::ios::binary | std::ios::trunc };
			output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			expect(static_cast<bool>(output), "public CLI fixture records a legacy remounted empty cache");
		}
		expect(manifestCount == 1U, "public CLI remount course changes only its one empty cache");
	#endif

		options.EpochPhase              = app::detail::github_epoch_fixture_phase::single;
		options.RefuseFirstSmartAcquire = false;
		terminal                        = false;
		startup                         = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::ghes_3_18,
			options,
			[&](auto aResult) {
				terminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return terminal && startup && startup->is_quiescent(); },
			"CLI source/update recovery startup completes");
		expect(application != nullptr, "CLI source/update recovery startup succeeds");
		auto& recoveredWindow = application->window();
		recoveredWindow.show();
		QApplication::processEvents();
		verify_composed_code_metrics(*application, 4U);
		recoveredWindow.home_landing()->secondary_button()->click();
		cancel_setup(recoveredWindow);
		auto   recoveredConfirmations = 0U;
		QTimer recoveredConsent;
		QObject::connect(&recoveredConsent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget())) {
				++recoveredConfirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		recoveredConsent.start(1);
		recoveredWindow.home_landing()->secondary_button()->click();
		wait_for(
			[&] {
				return recoveredWindow.session_panel()->status()
					== core::github_status::session_ready;
			},
			"CLI source/update recovery profile connects");
		submit(recoveredWindow, pullRequestUrl);
		wait_for(
			[&] {
				return recoveredWindow.session_panel()->status()
					== core::github_status::pull_request_ready;
			},
			"CLI source/update recovery PR opens");
		recoveredConsent.stop();
		expect(
			recoveredConfirmations == 1U,
			"CLI source/update recovery profile asks for one confirmation");
		wait_for(
			[&] {
				return application->status().EpochStatus == core::epoch_status::ready
					&& recoveredWindow.epoch_selectors()
						   ->history_model()
						   ->epoch_count()
					== 1U;
			},
			"supported CLI source course retains its initial revision");
		const auto seedEvidence = github_application_fixture_access::evidence(
			*application);
		expect(
			seedEvidence.SmartAcquireRefusalCount == 0
				&& seedEvidence.SmartAcquireCount == 1
				&& seedEvidence.SmartFetchCount == 1,
			"supported CLI source course acquires its initial exact comparison");
		application->begin_shutdown();
		wait_for(
			[&] { return application->status().ShutdownComplete; },
			"supported CLI source seed work quiesces");
		application.reset();
		startup.reset();

		options.EpochPhase = app::detail::github_epoch_fixture_phase::reuse;
		terminal           = false;
		startup            = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::ghes_3_18,
			options,
			[&](auto aResult) {
				terminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return terminal && startup && startup->is_quiescent(); },
			"CLI source/update retained-history startup completes");
		expect(
			application != nullptr,
			"CLI source/update retained-history startup succeeds");
		auto& updatedWindow = application->window();
		updatedWindow.show();
		QApplication::processEvents();
		updatedWindow.home_landing()->secondary_button()->click();
		cancel_setup(updatedWindow);
		auto   updatedConfirmations = 0U;
		QTimer updatedConsent;
		QObject::connect(&updatedConsent, &QTimer::timeout, [&] {
			if (auto* prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget())) {
				++updatedConfirmations;
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		updatedConsent.start(1);
		updatedWindow.home_landing()->secondary_button()->click();
		wait_for(
			[&] {
				return updatedWindow.session_panel()->status()
					== core::github_status::session_ready;
			},
			"CLI source/update retained-history profile connects");
		submit(updatedWindow, pullRequestUrl);
		wait_for(
			[&] {
				return updatedWindow.session_panel()->status()
					== core::github_status::pull_request_ready;
			},
			"CLI source/update retained-history PR opens");
		updatedConsent.stop();
		expect(
			updatedConfirmations == 1U,
			"CLI source/update retained-history profile asks for one confirmation");
		wait_for(
			[&] {
				return application->status().EpochStatus == core::epoch_status::ready
					&& updatedWindow.epoch_selectors()
						   ->history_model()
						   ->epoch_count()
					>= 2U;
			},
			"relaunch installs the recorded GHES update history");
		auto recoveredEvidence = github_application_fixture_access::evidence(*application);
		expect(
			recoveredEvidence.SmartAcquireRefusalCount == 0
				&& recoveredEvidence.SmartAcquireCount == 1
				&& recoveredEvidence.SmartFetchCount == 1
				&& updatedWindow.epoch_selectors()
						->right_oid_editor()
						->text()
						.toStdString()
					== recoveredEvidence.HeadTwoOid,
			"relaunch recovers the second exact update through the same CLI authority");

		expect(!updatedWindow.files_drawer()->contents_view()->currentIndex().isValid()
				   && updatedWindow.comparison_surface()->current_result() == nullptr,
			   "recovered PR opens without a source selection");
		read_cli_source_update_discussion(updatedWindow);
		expect(!updatedWindow.files_drawer()->contents_view()->currentIndex().isValid()
				   && updatedWindow.comparison_surface()->current_result() == nullptr,
			   "reading PR-wide discussion does not require or select code");
		updatedWindow.modality()->action(qt::review_modality::overview)->trigger();
		verify_overview_folder_navigation(updatedWindow);
		expect(updatedWindow.overview_page()->entry_button()->isEnabled()
				   && updatedWindow.overview_page()->entry_button()->text() == QStringLiteral("Start review"),
			   "Overview offers Start review for the first visit");
		updatedWindow.overview_page()->entry_button()->click();
		expect(updatedWindow.current_modality() == qt::review_modality::review,
			   "Overview entry reaches Review");
		auto* const recoveredFiles     = updatedWindow.files_drawer()->contents_view();
		const auto  recoveredReview    = updatedWindow.files_drawer()
											 ->contents()
											 ->index_for_file(core::file_identity_of("review.cpp"));
		const auto  recoveredSecondary = updatedWindow.files_drawer()
											 ->contents()
											 ->index_for_file(core::file_identity_of("secondary.cpp"));
		expect(
			recoveredReview.isValid() && recoveredSecondary.isValid(),
			"recovered history presents both exact recorded file identities");
		auto* const recoveredSurface = updatedWindow.comparison_surface();
		wait_for([&] {
			return recoveredSurface->current_context().has_value()
				&& recoveredSurface->current_context()->Key.File == core::file_identity_of("review.cpp")
				&& recoveredSurface->left_pane()->document()->firstBlock().text() == QStringLiteral("int review_line_1 = 11;")
				&& recoveredSurface->right_pane()->document()->firstBlock().text() == QStringLiteral("int review_line_1 = 31;");
		},
				 "Overview entry loads the exact recorded review source without a file-row gesture");

		const auto selectExact = [&](const QModelIndex&     aIndex,
									 const std::string_view aPath,
									 const QString&         aLeftFirst,
									 const QString&         aRightFirst) {
			recoveredFiles->selectionModel()->setCurrentIndex(
				aIndex,
				QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			wait_for(
				[&] {
					return recoveredSurface->current_result() != nullptr
						&& recoveredSurface->current_context().has_value()
						&& recoveredSurface->current_context()->Key.File
						== core::file_identity_of(aPath)
						&& recoveredSurface->left_pane()->document()->firstBlock().text()
						== aLeftFirst
						&& recoveredSurface->right_pane()->document()->firstBlock().text()
						== aRightFirst
						&& !recoveredSurface->geometry_pending();
				},
				"installed Git presents exact recovered file content", [&] { std::cerr << "recovered-source wanted=" << aPath
																					   << " file=" << (recoveredSurface->current_context() && recoveredSurface->current_context()->Key.File == core::file_identity_of(aPath))
																					   << " left=" << recoveredSurface->left_pane()->document()->firstBlock().text().toStdString()
																					   << " right=" << recoveredSurface->right_pane()->document()->firstBlock().text().toStdString()
																					   << " geometry=" << recoveredSurface->geometry_pending()
																					   << " modality=" << static_cast<int>(updatedWindow.current_modality())
																					   << " status=" << updatedWindow.status_text().toStdString() << '\n'; });
		};
		selectExact(
			recoveredReview,
			"review.cpp",
			QStringLiteral("int review_line_1 = 11;"),
			QStringLiteral("int review_line_1 = 31;"));
		selectExact(
			recoveredSecondary,
			"secondary.cpp",
			QStringLiteral("int secondary_line_1 = 111;"),
			QStringLiteral("int secondary_line_1 = 131;"));
		capture(updatedWindow, QStringLiteral("setup-cli-remount-readable.png"));
		auto* const compose = updatedWindow.findChild<QAction*>(QStringLiteral("composeReviewAction"));
		expect(compose && compose->isEnabled(), "dedicated Compose review action is reachable");
		compose->trigger();
		auto* const editor = updatedWindow.review_submission_editor_view();
		wait_for([&] { return editor->isVisible() && editor->shown_draft().has_value(); },
				 "Compose review presents a local draft");
		const auto draftBody = QStringLiteral("Combined GHES course local review draft.");
		editor->body_editor()->setPlainText(draftBody);
		const auto draftId = editor->shown_draft()->Id;
		button(updatedWindow, "composeReviewPageBack")->click();
		read_cli_source_update_discussion(updatedWindow);
		updatedWindow.modality()->action(qt::review_modality::review)->trigger();
		expect(recoveredSurface->current_context().has_value()
				   && recoveredSurface->current_context()->Key.File == core::file_identity_of("secondary.cpp")
				   && recoveredSurface->right_pane()->document()->firstBlock().text() == QStringLiteral("int secondary_line_1 = 131;"),
			   "Compose and Discuss navigation retain the exact secondary source");
		selectExact(recoveredReview, "review.cpp", QStringLiteral("int review_line_1 = 11;"), QStringLiteral("int review_line_1 = 31;"));
		selectExact(recoveredSecondary, "secondary.cpp", QStringLiteral("int secondary_line_1 = 111;"), QStringLiteral("int secondary_line_1 = 131;"));
		compose->trigger();
		expect(editor->isVisible() && editor->shown_draft()->Id == draftId
				   && editor->shown_draft()->Body == draftBody.toStdString()
				   && editor->body_editor()->toPlainText() == draftBody,
			   "local review draft survives modality and file navigation");
		recoveredEvidence = github_application_fixture_access::evidence(*application);
		expect(
			recoveredEvidence.SmartAcquireRefusalCount == 0
				&& recoveredEvidence.SmartAcquireCount == 1
				&& recoveredEvidence.SmartFetchCount == 1,
			"both files reuse the one recovered exact comparison without another fetch");

		application->begin_shutdown();
		wait_for(
			[&] { return application->status().ShutdownComplete; },
			"CLI source/update work quiesces");
		const auto mutation = github_application_fixture_access::review_mutation_evidence(*application);
		const auto lock     = std::lock_guard{ reviewServer->Mutex };
		expect(reviewServer->Submissions == 0U && reviewServer->Reviews.empty()
				   && mutation.Submissions == 0U && mutation.StoredComments == 0U
				   && mutation.Replies == 0U && mutation.Resolutions == 0U,
			   "combined navigation and local drafting submit and publish nothing");
	}

	void run_github_cli_discovery_course()
	{
		auto temporary = QTemporaryDir{ };
		expect(temporary.isValid(), "isolated CLI discovery application store");
		auto options                = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot = e2e::private_data_root(temporary);
		expect(options.ApplicationDataRoot.has_value(), "private CLI discovery test root under the canonical storage policy");
		options.GithubCliDiscoveryAccounts
			= std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{
				  { .Hostname = "ghe.discovered.example", .Login = "enterprise-reviewer" },
				  { .Hostname = "second.example.invalid", .Login = "another-reviewer" },
			  };

		std::unique_ptr<bpr::app::github_application> application;
		auto                                          startupTerminal = false;
		auto                                          startup         = github_application_fixture_access::start(
			app::detail::github_public_path_fixture::github_com,
			options,
			[&](auto aResult) {
				startupTerminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		wait_for(
			[&] { return startupTerminal && startup && startup->is_quiescent(); },
			"CLI discovery application startup reaches a terminal");
		expect(application != nullptr, "CLI discovery application startup succeeds");
		auto& window = application->window();
		window.show();
		QApplication::processEvents();
		expect(!window.home_landing()->secondary_button()->isHidden()
				   && window.home_landing()->secondary_button()->text()
					   == QStringLiteral("Use GitHub CLI"),
			   "the shipped composition exposes CLI discovery as its own signed-out path");
		window.home_landing()->secondary_button()->click();
		wait_for(
			[&] {
				return window.github_cli_discovery_status()->text().contains(
					QStringLiteral("Found 2"));
			},
			"the composed discovery reaches its ready terminal");
		expect(window.sign_in_dialog()->isVisible() && !window.settings()->isVisible(), "CLI discovery presents choices without opening Settings");

		capture(window, QStringLiteral("setup-cli-choices.png"));
		qt::server_card* discovered = nullptr;
		for (auto* const card : window.server_cards()) {
			if (card->hostname() == "ghe.discovered.example") {
				discovered = card;
			}
		}
		expect(discovered != nullptr && discovered->options().size() == 1U
				   && discovered->native_option() == nullptr
				   && discovered->cli_option() != nullptr
				   && discovered->cli_offer_button() != nullptr
				   && discovered->cli_offer_button()->text().contains(
					   QStringLiteral("enterprise-reviewer")),
			   "the discovered GHES identity is a CLI-only card naming the observed login");

		const auto profile = discovered->cli_option()->Profile;
		QTimer::singleShot(0, [] {
			if (auto* const prompt = qobject_cast<QMessageBox*>(
					QApplication::activeModalWidget());
				prompt != nullptr) {
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		discovered->cli_offer_button()->click();
		QApplication::processEvents();
		expect(window.session_panel()->selected_profile() == profile
				   && window.session_panel()->profile_option(profile) != nullptr
				   && window.session_panel()->profile_option(profile)->Hostname
					   == "ghe.discovered.example",
			   "the CLI-only GHES card reaches the existing exact-host connection path");

		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; },
				 "CLI discovery work quiesces");
	}
#endif
}
int main(int aArgc, char** aArgv)
try {
	QApplication application{ aArgc, aArgv };
	if (aArgc == 2 && QString::fromUtf8(aArgv[1]) == QStringLiteral("--selected-file-safety")) {
		run_folder_selection_course();
		run_modified_missing_endpoint_course();
		std::cout << "selected-file-safety-public-path:pass\n";
		return EXIT_SUCCESS;
	}
	if (aArgc == 2 && QString::fromUtf8(aArgv[1]) == QStringLiteral("--nested-overview-folders")) {
		run_nested_overview_folders_course();
		std::cout << "nested-overview-folders-public-path:pass\n";
		return EXIT_SUCCESS;
	}
	if (aArgc == 2 && QString::fromUtf8(aArgv[1]) == QStringLiteral("--overview-feedback")) {
		run_overview_feedback_course();
		std::cout << "overview-feedback-public-path:pass\n";
		return EXIT_SUCCESS;
	}
	if (aArgc == 2 && QString::fromUtf8(aArgv[1]) == QStringLiteral("--overview-labels")) {
		run_overview_labels_course();
		std::cout << "overview-labels-public-path:pass\n";
		return EXIT_SUCCESS;
	}
	if (aArgc == 2 && QString::fromUtf8(aArgv[1]) == QStringLiteral("--link-locations")) {
		run_link_location_course();
		std::cout << "link-location-public-path:pass\n";
		return EXIT_SUCCESS;
	}
#if defined(__APPLE__)
	// [[BPR-1200]]. The refused first read, signed out and signed in, alone.
	if (aArgc == 2 && QString::fromUtf8(aArgv[1]) == QStringLiteral("--refused-first-read")) {
		run_native_read_refusal_course();
		std::cout << "refused-first-read-public-path:pass\n";
		return EXIT_SUCCESS;
	}
	if (aArgc == 2 && QString::fromUtf8(aArgv[1]) == QStringLiteral("--cli-source-update")) {
		run_cli_source_update_course();
		std::cout << "cli-source-update-public-path:pass\n";
		return EXIT_SUCCESS;
	}
#endif
	run_course(false);
	run_course(true);
	run_link_location_course();
#if defined(__APPLE__)
	run_github_cli_discovery_course();
	run_connected_cli_destination_course(false);
	run_connected_cli_destination_course(true);
	run_connected_cli_destination_course(true, true);
	run_connected_cli_destination_course(false, false, true);
	run_native_read_refusal_course();
	run_cli_source_update_course();
#endif
	std::cout << "setup-destination-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
