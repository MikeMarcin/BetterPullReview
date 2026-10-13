#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	void settle()
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 200) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	auto shown(const qt::workbench_window& aWindow, const std::string_view aPath) -> bool
	{
		const auto* surface = aWindow.comparison_surface();
		return surface->current_result() && surface->current_context()
			&& surface->current_context()->Key.File == core::file_identity_of(aPath) && !surface->geometry_pending();
	}

	void open(app::github_application& aApplication, const std::string_view aPath)
	{
		std::cout << "header-source open=" << aPath << '\n';
		auto&      window = aApplication.window();
		const auto index  = window.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath));
		expect(index.isValid(), "requested path belongs to accepted actionable catalogue");
		window.files_drawer()->contents_view()->setCurrentIndex(index);
		const auto message = "Git/cache comparison displays " + std::string{ aPath };
		wait_for([&] { return shown(window, aPath); }, message.c_str());
	}

	void focus_source(qt::workbench_window& aWindow)
	{
		aWindow.comparison_surface()->focus_source();
		QApplication::processEvents();
	}

	void key(qt::workbench_window& aWindow)
	{
		tests::window_system_key(aWindow, Qt::Key_O, Qt::AltModifier, QString::fromUtf8("ø"));
	}

	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_HEADER_SOURCE_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "render directory created");
		expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "full composed window render saved");
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication qtApplication{ aArgc, aArgv };
	const auto   large = aArgc > 1 && std::string_view{ aArgv[1] } == "--large";
	{
		qt::desktop_menu_bar resident;
		auto*                mirror = resident.findChild<QAction*>(QStringLiteral("desktopGoToHeaderSourceAction"));
		expect(mirror && !mirror->isEnabled() && mirror->shortcut() == QKeySequence{ QStringLiteral("Alt+O") } && resident.view_menu()->actions().contains(mirror), "resident menu mirrors disabled command and shortcut");
	}
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-header-source-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(large ? app::detail::github_public_path_fixture::large_review_v1 : app::detail::github_public_path_fixture::header_source_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composition starts");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);
	auto* action = window.header_source_action();
	expect(action && !action->isEnabled() && window.view_menu()->actions().contains(action)
			   && action->shortcuts() == QList<QKeySequence>{ QKeySequence{ QStringLiteral("Alt+O") } },
		   "one discoverable View command with Alt/Option+O");
	const auto keys = std::ranges::count_if(window.findChildren<QAction*>(), [](const QAction* aAction) {
		return aAction->shortcuts().contains(QKeySequence{ QStringLiteral("Alt+O") });
	});
	expect(keys == 1, "assembled window has one Alt+O shortcut owner");
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(large ? QStringLiteral("bpr-fixtures/large-review#17") : QStringLiteral("bpr-fixtures/header-source#74"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "PR opens on Overview");
	expect(!action->isEnabled(), "Overview does not navigate hidden source");
	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, step) != app::detail::course_modality_result::pending; }, "enters Review");

	expect(!action->isEnabled() && action->statusTip().contains(QStringLiteral("Select")), "accepted Review with no selected file disables command with reason");
	if (large) {
		open(*application, "src/file_0000.cpp");
		focus_source(window);
		const auto context = application->windows().front();
		wait_for([&] { const auto work = access::diff_work_evidence(*application, context); return work.OutstandingJobs == 0 && !work.Pending && work.Preparing == 0 && work.PreparationsInFlight == 0 && !work.PreparationPending; }, "ordinary comparison and preparation settle");
		const auto  before     = access::scaling_evidence(*application).Workbench;
		const auto  diffBefore = access::diff_work_evidence(*application, context);
		const auto* result     = window.comparison_surface()->current_result().get();
		for (int query = 0; query < 100; ++query) {
			action->trigger();
		}
		settle();
		const auto after     = access::scaling_evidence(*application).Workbench;
		const auto diffAfter = access::diff_work_evidence(*application, context);
		expect(before.AcceptedObservationRows >= 500 && after.CounterpartIndexedRows == before.CounterpartIndexedRows && after.FileProjectionRows == before.FileProjectionRows
				   && after.CounterpartDirectoryLookups - before.CounterpartDirectoryLookups == 100U && diffAfter.FixtureDiffHelperInvocations == diffBefore.FixtureDiffHelperInvocations
				   && result == window.comparison_surface()->current_result().get() && window.files_drawer()->contents()->statistics().ModelResets == 0,
			   "100 real commands in 515-file review perform indexed empty opposite-family lookup with no candidate reads, file projection or model reset");
		render(window, QStringLiteral("04-large-no-match"));
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "large course shutdown drains");
		application.reset();
		expect(e2e::remove_course_root(temporary), "large private root removed");
		std::cout << "HEADER_SOURCE_LARGE_OK files=" << before.AcceptedObservationRows << " queries=100 directory_lookups=100 additional_blob_helpers=0 model_resets=0 qt=" << qVersion() << '\n';
		return 0;
	}
	open(*application, "src/widget.cpp");
	focus_source(window);
	const auto original     = access::epoch_refresh_identity(*application);
	const auto originalText = window.comparison_surface()->right_pane()->toPlainText();
	expect(action->isEnabled() && action->statusTip().contains(QStringLiteral("matching")), "supported source offers command and purpose");
	key(window);
	wait_for([&] { return shown(window, "inc/widget.h"); }, "physical Option+O opens cross-folder header");
	expect(QApplication::focusWidget() == window.comparison_surface()->right_pane(), "jump keeps source focus");
	expect(window.comparison_surface()->right_pane()->toPlainText().contains(QStringLiteral("inc/widget.h")), "header's actual Git blob is rendered");
	expect(access::epoch_refresh_identity(*application).AcceptedComparison == original.AcceptedComparison, "exact comparison retained");
	render(window, QStringLiteral("01-split-header"));
	action->trigger();
	wait_for([&] { return shown(window, "src/widget.cpp"); }, "QAction returns to source through ordinary selection");
	expect(window.comparison_surface()->right_pane()->toPlainText() == originalText, "Option key inserted no character into source");
	window.one_pane_action()->trigger();
	wait_for([&] { return window.comparison_surface()->one_pane_enabled(); }, "unified presentation selected");
	focus_source(window);
	key(window);
	wait_for([&] { return shown(window, "inc/widget.h"); }, "physical key opens header in unified presentation");
	render(window, QStringLiteral("02-unified-header"));
	action->trigger();
	wait_for([&] { return shown(window, "src/widget.cpp"); }, "unified returns to source");

	// A counterpart already open in a sibling stays there; the active view is replaced.
	window.document_tabs()->new_tab_button()->click();
	wait_for([&] { return window.document_tabs()->tabs().size() == 2; }, "duplicate tab opens");
	const auto sibling = window.document_tabs()->tabs().front();
	focus_source(window);
	action->trigger();
	wait_for([&] { return shown(window, "inc/widget.h"); }, "active duplicate opens header");
	const auto headerSibling = window.document_tabs()->tabs().back();
	window.document_tabs()->tab_bar()->setCurrentIndex(0);
	wait_for([&] { return shown(window, "src/widget.cpp"); }, "first tab restores source");
	window.document_tabs()->tab_bar()->setFocus();
	QApplication::processEvents();
	expect(action->isEnabled(), "tab strip focus offers counterpart shortcut");
	key(window);
	wait_for([&] { return shown(window, "inc/widget.h"); }, "counterpart replaces first tab although already open in second");
	expect(window.document_tabs()->tabs().size() == 2 && window.document_tabs()->tabs().front().Id == sibling.Id
			   && window.document_tabs()->tabs().front().Active && window.document_tabs()->tabs().back().Id == headerSibling.Id
			   && window.document_tabs()->tabs().back().Path == headerSibling.Path && window.document_tabs()->tabs().back().Pair == headerSibling.Pair,
		   "active tab identity retained, sibling content/comparison unchanged");

	// An earlier base/base pair belongs to the active tab; the sibling keeps
	// its base/head pair, even when the command selects the same header.
	open(*application, "src/widget.cpp");
	auto* controls = window.epoch_selectors();
	auto* history  = controls->history_model();
	int   baseRow  = -1;
	for (int row = 0; row < history->rowCount(); ++row) {
		const auto anchor = history->anchor_at(history->index(row, 0));
		if (anchor && anchor->Identity.Kind == core::comparison_anchor_kind::epoch_base) {
			baseRow = row;
			break;
		}
	}
	expect(baseRow >= 0, "retained base anchor is selectable");
	controls->right_selector()->setCurrentIndex(baseRow);
	wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return shown(window, "src/widget.cpp") && identity.AcceptedComparison && identity.AcceptedComparison->Left.Commit == identity.AcceptedComparison->Right.Commit && !identity.TabPending; }, "active tab opens earlier base/base comparison");
	const auto older       = access::epoch_refresh_identity(*application).AcceptedComparison;
	const auto siblingPair = window.document_tabs()->tabs().back().Pair;
	auto       siblingMenu = std::unique_ptr<QMenu>{ window.document_tabs()->make_tab_menu(headerSibling.Id, &window) };
	auto*      makeWindow  = siblingMenu->findChild<QAction*>(QStringLiteral("documentTabMakeWindow"));
	expect(makeWindow && makeWindow->isEnabled(), "sibling's newer comparison can become window default");
	makeWindow->trigger();
	siblingMenu.reset();
	wait_for([&] { return access::tab_comparison_evidence(*application).Window != older; }, "new tabs use newer sibling pair while active tab retains older pair");

	focus_source(window);
	action->trigger();
	wait_for([&] { return shown(window, "inc/widget.h"); }, "counterpart opens in active older comparison");
	expect(access::epoch_refresh_identity(*application).AcceptedComparison == older && access::tab_comparison_evidence(*application).Window != older
			   && window.document_tabs()->tabs().back().Pair == siblingPair,
		   "jump retains older active pair, distinct window default and sibling pair");
	expect(window.comparison_surface()->right_pane()->toPlainText().contains(QStringLiteral("value = 1")), "older counterpart's actual base blob displays without newer fallback");
	open(*application, "src/missing.cpp");
	focus_source(window);
	action->trigger();
	wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return identity.SelectedFile == core::file_identity_of("inc/missing.h") && !window.comparison_surface()->current_result() && window.comparison_surface()->status_text() == QStringLiteral("The selected file is absent from both revisions."); }, "target absent at older pair reports ordinary unavailable diff");
	expect(access::epoch_refresh_identity(*application).AcceptedComparison == older, "missing target never substitutes newer pair");
	controls->whole_pr_action()->trigger();
	wait_for([&] { return controls->comparison_available() && access::epoch_refresh_identity(*application).AcceptedComparison != older; }, "restores whole PR pair");

	open(*application, "src/a/b/c/closest.cpp");
	focus_source(window);
	action->trigger();
	wait_for([&] { return shown(window, "inc/project/a/b/c/closest.h"); }, "closest trailing directories win");
	open(*application, "src/local/pair.cc");
	focus_source(window);
	action->trigger();
	wait_for([&] { return shown(window, "src/local/pair.HPP"); }, "same-directory wins and uppercase extension is supported");

	const auto noOp = [&](const std::string_view aPath, const QString& aMessage) {
		open(*application, aPath);
		focus_source(window);
		const auto  before = access::epoch_refresh_identity(*application);
		const auto* result = window.comparison_surface()->current_result().get();
		const auto  counts = access::scaling_evidence(*application).Workbench;
		action->trigger();
		settle();
		expect(access::epoch_refresh_identity(*application).ActiveTab == before.ActiveTab
				   && window.comparison_surface()->current_result().get() == result,
			   "no-op changes no reading state or diff");
		expect(window.statusBar()->currentMessage() == aMessage, "distinct nonmodal status explains no-op");
		const auto after = access::scaling_evidence(*application).Workbench;
		expect(after.CounterpartIndexedRows == counts.CounterpartIndexedRows
				   && after.FileProjectionRows == counts.FileProjectionRows,
			   "no-op reads no blob, rebuilds no index and publishes no file rows");
	};
	noOp("src/tie/ambiguous.cpp", QStringLiteral("More than one equally close header or source file matches in this review"));
	render(window, QStringLiteral("03-ambiguous"));
	noOp("src/lonely.cpp", QStringLiteral("No matching header in this review"));
	noOp("inc/alone.h", QStringLiteral("No matching source file in this review"));
	open(*application, "README.md");
	focus_source(window);
	expect(!action->isEnabled() && action->statusTip().contains(QStringLiteral("extension")), "unsupported file disabled with reason");

	open(*application, "inc/widget.h");
	wait_for([&] { return window.exclude_action()->isEnabled(); }, "private receipt action ready");
	window.exclude_action()->trigger();
	wait_for([&] { return !window.files_drawer()->contents()->index_for_file(core::file_identity_of("inc/widget.h")).isValid(); }, "excluded header withheld from actionable order");
	open(*application, "src/widget.cpp");
	const auto beforeReceiptJump = window.files_drawer()->contents()->rowCount();
	window.files_tree()->setFocus();
	QApplication::processEvents();
	expect(action->isEnabled(), "Files drawer focus offers shortcut");
	key(window);
	wait_for([&] { return shown(window, "inc/widget.h"); }, "private-receipt-withheld counterpart still opens");
	expect(window.files_drawer()->contents()->rowCount() == beforeReceiptJump
			   && !window.files_drawer()->contents()->index_for_file(core::file_identity_of("inc/widget.h")).isValid(),
		   "jump leaves receipt and actionable order unchanged");
	expect(window.comparison_surface()->isAncestorOf(QApplication::focusWidget()), "jump from Files returns focus to visible source");
	window.exclude_action()->trigger();
	wait_for([&] { return window.files_drawer()->contents()->index_for_file(core::file_identity_of("inc/widget.h")).isValid(); }, "private receipt restored for remaining course");
	open(*application, "src/widget.cpp");
	window.files_filter()->setText(QStringLiteral("src/widget.cpp"));
	focus_source(window);
	const auto fileOrder = [&] {
		auto        order = std::vector<std::optional<core::file_identity>>{ };
		const auto* files = window.files_drawer()->contents();
		for (int row = 0; row < files->rowCount(); ++row) {
			order.push_back(files->file_at(files->index(row)));
		}
		return order;
	};
	const auto rows = fileOrder();
	key(window);
	wait_for([&] { return shown(window, "inc/widget.h"); }, "filtered-out counterpart still opens");
	expect(window.files_filter()->text() == QStringLiteral("src/widget.cpp") && fileOrder() == rows,
		   "jump preserves filter and actionable catalogue order");
	// Native text input remains the editor's, including the macOS Option character.
	window.files_filter()->setFocus();
	QApplication::processEvents();
	expect(!action->isEnabled(), "native menu equivalent disabled during editable input");
	window.files_filter()->clear();
	tests::window_system_key(window, Qt::Key_O, Qt::AltModifier, QString::fromUtf8("ø"));
	expect(window.files_filter()->text() == QString::fromUtf8("ø") && shown(window, "inc/widget.h"), "editable Option+O keeps its native text and does not navigate");
	window.files_filter()->clear();
	focus_source(window);

	wait_for([&] { return window.compose_review_action()->isEnabled(); }, "real review composer available");
	window.compose_review_action()->trigger();
	auto* editor = window.review_submission_editor_view()->body_editor();
	wait_for([&] { return editor->isVisible() && !editor->isReadOnly(); }, "real review composer opens");
	editor->setFocus();
	QApplication::processEvents();
	expect(!action->isEnabled(), "composer keeps Option text input");
	const auto body = editor->toPlainText();
	key(window);
	expect(editor->toPlainText() == body + QString::fromUtf8("ø"), "composer receives its native Option character");
	editor->setPlainText(body);
	tests::window_system_key(window, Qt::Key_Escape);
	window.choose_modality(qt::review_modality::review);
	focus_source(window);

	open(*application, "src/widget.cpp");
	focus_source(window);
	action->trigger();
	window.document_tabs()->tab_bar()->setCurrentIndex(1);
	wait_for([&] { return window.document_tabs()->tabs().back().Active && !access::epoch_refresh_identity(*application).TabPending; }, "intervening tab gesture commits");
	expect(window.document_tabs()->tabs().front().Path == QStringLiteral("src/widget.cpp"), "tab switch rejects pending counterpart before changing originating tab");
	window.document_tabs()->tab_bar()->setCurrentIndex(0);
	wait_for([&] { return shown(window, "src/widget.cpp"); }, "returns to original source tab");
	focus_source(window);
	action->trigger();
	controls->right_selector()->setCurrentIndex(baseRow);
	wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return shown(window, "src/widget.cpp") && identity.AcceptedComparison && identity.AcceptedComparison->Left.Commit == identity.AcceptedComparison->Right.Commit && !identity.TabPending; }, "comparison gesture rejects pending counterpart");
	expect(shown(window, "src/widget.cpp"), "old counterpart cannot change source after comparison replacement");
	controls->whole_pr_action()->trigger();
	wait_for([&] { const auto identity = access::epoch_refresh_identity(*application); return shown(window, "src/widget.cpp") && identity.AcceptedComparison && identity.AcceptedComparison->Left.Commit != identity.AcceptedComparison->Right.Commit && !identity.TabPending; }, "whole PR comparison restored");

	// Resolution was captured before another public file gesture in the same event turn.
	action->trigger();
	window.next_file_action()->trigger();
	settle();
	expect(!shown(window, "src/widget.cpp"), "intervening file selection rejects queued old counterpart resolution");
	open(*application, "src/widget.cpp");
	focus_source(window);
	action->trigger();
	window.home_action()->trigger();
	settle();
	expect(window.showing_home() && access::epoch_refresh_identity(*application).SelectedFile == core::file_identity_of("src/widget.cpp"), "Home entry rejects queued counterpart without changing retained source");
	window.home_action()->trigger();
	wait_for([&] { return shown(window, "src/widget.cpp") && !window.showing_home(); }, "returns to retained review source");
	for (const auto modality : { qt::review_modality::overview, qt::review_modality::discuss, qt::review_modality::threads }) {
		window.choose_modality(modality);
		settle();
		expect(!action->isEnabled(), "inactive review modality disables counterpart action");
		const auto selected = access::epoch_refresh_identity(*application).SelectedFile;
		key(window);
		settle();
		expect(access::epoch_refresh_identity(*application).SelectedFile == selected, "inactive modality key cannot navigate retained source");
	}
	window.choose_modality(qt::review_modality::review);
	wait_for([&] { return shown(window, "src/widget.cpp") && !access::epoch_refresh_identity(*application).TabPending; }, "Review restores exact source after inactive modalities");
	focus_source(window);
	action->trigger();
	expect(access::reaccept_fixture_revision(*application, false), "accepted catalogue generation replacement is admitted");
	wait_for([&] { return !action->isEnabled() && action->statusTip().contains(QStringLiteral("catalogue")); }, "refused replacement catalogue has distinct disabled explanation");
	expect(access::epoch_refresh_identity(*application).SelectedFile != core::file_identity_of("inc/widget.h"), "catalogue generation replacement rejects pending counterpart");
	window.home_action()->trigger();
	settle();
	expect(window.showing_home() && !action->isEnabled(), "Home rejects queued navigation and disables the retained command");
	const auto beforeHome = access::epoch_refresh_identity(*application).SelectedFile;
	key(window);
	settle();
	expect(access::epoch_refresh_identity(*application).SelectedFile == beforeHome, "Home key cannot navigate hidden source");

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "shutdown drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "private root removed");
	std::cout << "HEADER_SOURCE_PUBLIC_PATH_OK qt=" << qVersion() << " platform=" << QApplication::platformName().toStdString() << " files=16 input=window-system-key\n";
}
