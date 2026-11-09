#ifndef BPR_TESTS_END_TO_END_GO_TO_LINE_COURSE_HPP
#define BPR_TESTS_END_TO_END_GO_TO_LINE_COURSE_HPP

#include "../support/window_system_keys.hpp"
#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/keyboard_shortcuts_window.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QTabBar>
#include <QThread>
#include <QToolButton>
#include <QTreeWidget>
#include <algorithm>
#include <iostream>
#include <vector>

namespace go_to_line_course {
	using namespace bpr;
	using tests::expect;

	inline auto dialog(qt::workbench_window& aWindow, const bool aKeyboard) -> QDialog*
	{
		if (aKeyboard) {
			const auto key = qt::shortcut_key(qt::shortcut_command::go_to_line)[0];
			tests::window_system_key(aWindow, key.key(), key.keyboardModifiers());
		} else {
			aWindow.findChild<QAction*>(QStringLiteral("goToLineAction"))->trigger();
		}
		QApplication::processEvents();
		const auto dialogs = aWindow.findChildren<QDialog*>(QStringLiteral("goToLineDialog"));
		const auto found   = std::ranges::find_if(dialogs, [](const auto* aDialog) { return aDialog->isVisible(); });
		expect(found != dialogs.end(), "menu and physical key open Go to line in the owning window");
		return *found;
	}

	inline auto number(QDialog& aDialog) -> QLineEdit* { return aDialog.findChild<QLineEdit*>(QStringLiteral("goToLineNumber")); }
	inline auto side(QDialog& aDialog) -> QComboBox* { return aDialog.findChild<QComboBox*>(QStringLiteral("goToLineSide")); }
	inline auto go(QDialog& aDialog) -> QPushButton* { return aDialog.findChild<QPushButton*>(QStringLiteral("goToLineGo")); }

	inline void cancel(QDialog& aDialog, const bool aButton = false)
	{
		const auto owner = QPointer<QWidget>{ aDialog.parentWidget() };
		if (aButton) {
			aDialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
		} else {
			tests::window_system_key(aDialog, Qt::Key_Escape);
		}
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		// The offscreen plugin has no window manager to reactivate a modal's owner.
		if (owner) {
			owner->activateWindow();
			QApplication::processEvents();
		}
	}

	inline void expect_target(qt::diff_workbench& aSurface, const core::diff_side aSide, const std::uint32_t aLine)
	{
		const auto target = core::source_line_ref{ aSurface.current_result()->Alignment.Digest, aSide, aLine };
		if (!aSurface.selection() || aSurface.selection()->PreferredLine != target) {
			std::cerr << "LINE_SELECTION expected_side=" << static_cast<int>(aSide) << " expected_line=" << aLine << " unified=" << aSurface.one_pane_enabled();
			if (aSurface.selection()) {
				std::cerr << " actual_side=" << static_cast<int>(aSurface.selection()->PreferredLine.Side) << " actual_line=" << aSurface.selection()->PreferredLine.Line;
			}
			std::cerr << '\n';
		}
		expect(aSurface.selection() && aSurface.selection()->PreferredLine == target, "Go selects exact source side and line");
		const auto range = aSurface.line_draft_range();
		expect(range && range->first == target && range->second == target, "subsequent draft has exactly the requested single-line anchor");
		if (!aSurface.source_line_visible(aSide, aLine)) {
			std::cerr << "LINE_REVEAL side=" << static_cast<int>(aSide) << " line=" << aLine << " unified=" << aSurface.one_pane_enabled() << " rect=" << aSurface.line_rect(aSide, aLine).x() << "," << aSurface.line_rect(aSide, aLine).y() << "," << aSurface.line_rect(aSide, aLine).width() << "," << aSurface.line_rect(aSide, aLine).height() << " viewport=" << aSurface.unified_pane()->viewport()->width() << "x" << aSurface.unified_pane()->viewport()->height() << " scroll=" << aSurface.unified_pane()->verticalScrollBar()->value() << '\n';
		}
		expect(aSurface.source_line_visible(aSide, aLine), "requested source line is visible, including group interiors and unchanged context");
		auto* editor = aSurface.one_pane_enabled() ? aSurface.unified_pane() : aSide == core::diff_side::left ? aSurface.left_pane()
																											  : aSurface.right_pane();
		if (!editor->hasFocus() || editor->textCursor().hasSelection()) {
			std::cerr << "LINE_FOCUS side=" << static_cast<int>(aSide) << " line=" << aLine << " unified=" << aSurface.one_pane_enabled() << " selection=" << editor->textCursor().hasSelection() << " focus=" << (QApplication::focusWidget() ? QApplication::focusWidget()->objectName().toStdString() : "none") << " expected=" << editor->objectName().toStdString() << '\n';
		}
		expect(editor->hasFocus() && !editor->textCursor().hasSelection(), "exact source receives focus and range is replaced");
	}

	inline void submit(qt::workbench_window& aWindow, QDialog& aDialog, const core::diff_side aSide, const std::uint32_t aLine)
	{
		side(aDialog)->setCurrentIndex(aSide == core::diff_side::left ? 0 : 1);
		number(aDialog)->setText(QString::number(aLine + 1U));
		expect(go(aDialog)->isEnabled(), "valid whole source line enables Go");
		number(aDialog)->setFocus();
		const auto guarded = QPointer<QDialog>{ &aDialog };
		tests::window_system_key(aDialog, Qt::Key_Return);
		QApplication::processEvents();
		expect(!guarded || !guarded->isVisible(), "Return closes the valid dialog");
		aWindow.activateWindow();
		QApplication::processEvents();
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (aWindow.comparison_surface()->geometry_pending() && elapsed.elapsed() < 5000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect_target(*aWindow.comparison_surface(), aSide, aLine);
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	}

	inline void render(qt::workbench_window& aWindow, QDialog* const aDialog, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_FIND_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "Go to line render root created");
		auto raster = aWindow.grab();
		if (aDialog) {
			auto painter = QPainter{ &raster };
			painter.drawPixmap(aDialog->mapToGlobal(QPoint{ }) - aWindow.mapToGlobal(QPoint{ }), aDialog->grab());
		}
		expect(raster.save(root + QLatin1Char('/') + aName + QStringLiteral(".png")), "full production-window Go to line raster saved");
	}

	template <typename TWait, typename TOpen>
	void run(app::github_application& aApplication, TWait aWait, TOpen aOpen)
	{
		auto& window  = aApplication.window();
		auto& surface = *window.comparison_surface();
		auto* action  = window.findChild<QAction*>(QStringLiteral("goToLineAction"));
		expect(action && window.view_menu()->actions().contains(action), "Go to line is in the composed View menu");
		const auto shortcut = qt::shortcut_key(qt::shortcut_command::go_to_line);
#if defined(Q_OS_MACOS)
		expect(shortcut == QKeySequence{ QStringLiteral("Meta+G") }, "physical Control-G on macOS");
		expect(qt::shortcut_keys(qt::shortcut_command::find_next).contains(QKeySequence{ QStringLiteral("Ctrl+G") })
				   && qt::shortcut_keys(qt::shortcut_command::find_previous).contains(QKeySequence{ QStringLiteral("Ctrl+Shift+G") }),
			   "Command-G Find keys retained");
#else
		expect(shortcut == QKeySequence{ QStringLiteral("Ctrl+G") }, "Ctrl-G on Windows/Linux");
#endif
		qt::desktop_menu_bar desktop;
		const auto*          mirrored = desktop.findChild<QAction*>(QStringLiteral("desktopGoToLineAction"));
		expect(mirrored && !mirrored->isEnabled() && mirrored->shortcut() == shortcut, "no-window menu mirrors binding and disabled state");
		window.show_keyboard_shortcuts();
		auto* help = window.keyboard_shortcuts();
		help->search_field()->setText(QStringLiteral("Go to line"));
		expect(help->shown_commands() == std::vector{ qt::shortcut_command::go_to_line }, "Navigation help lists Go to line from central catalogue");
		help->hide();
		window.activateWindow();
		aWait([&] {
			const auto work = app::detail::github_application_fixture_access::diff_work_evidence(aApplication);
			return work.OutstandingJobs == 0U && !work.Pending && work.Preparing == 0U && work.PreparationsInFlight == 0U && !work.PreparationPending;
		},
			  "source and background preparation are quiet before bounded-work observation");
		const auto hostBefore   = app::detail::github_application_fixture_access::diff_work_evidence(aApplication);
		const auto originalSize = window.size();
		window.resize(window.minimumSize());
		QApplication::processEvents();
		for (const bool unified : { false, true }) {
			if (surface.one_pane_enabled() != unified) {
				window.one_pane_action()->trigger();
			}
			aWait([&] { return surface.one_pane_enabled() == unified && !surface.geometry_pending(); }, "Go to line presentation geometry ready");
			const auto before = surface.statistics();
			for (const auto sourceSide : { core::diff_side::left, core::diff_side::right }) {
				surface.focus_source(sourceSide);
				QApplication::processEvents();
				auto*      first    = dialog(window, true);
				const auto expected = unified && surface.selection() ? surface.selection()->PreferredLine.Side : sourceSide;
				expect(side(*first)->currentIndex() == (expected == core::diff_side::left ? 0 : 1), "split focus and unified semantic side choose dialog default");
				expect(number(*first)->hasFocus() && number(*first)->selectedText() == number(*first)->text(), "Line starts focused and wholly selected");
				expect(first->windowModality() == Qt::WindowModal && first->parentWidget() == &window, "dialog belongs to opening window");
				const auto* accessible = QAccessible::queryAccessibleInterface(number(*first));
				expect(accessible && accessible->text(QAccessible::Name) == QStringLiteral("Line") && !accessible->text(QAccessible::Description).isEmpty(), "accessible line name and valid range");
				tests::window_system_key(*first, Qt::Key_Backtab, Qt::ShiftModifier);
				expect(side(*first)->hasFocus(), "keyboard traversal reaches the labelled source side");
				tests::window_system_key(*first, Qt::Key_Tab);
				expect(number(*first)->hasFocus(), "keyboard traversal returns to the line input");
				const auto selection = surface.selection();
				const auto frame     = surface.scroll_controller()->current_frame();
				for (const auto& bad : { QString{ }, QStringLiteral("0"), QStringLiteral("-1"), QStringLiteral("1.5"), QStringLiteral(" 2"), QStringLiteral("99999999999999999999"), QStringLiteral("1108") }) {
					number(*first)->setText(bad);
					expect(!go(*first)->isEnabled() && !first->findChild<QLabel*>(QStringLiteral("goToLineValidation"))->text().isEmpty(), "invalid and overflowing values disable Go with readable reason");
					expect(surface.selection() == selection && surface.scroll_controller()->current_frame() == frame, "typing never previews navigation");
				}
				render(window, first, unified ? QStringLiteral("line-unified-invalid") : QStringLiteral("line-split-invalid"));
				number(*first)->setText(QStringLiteral("2"));
				side(*first)->setCurrentIndex(1 - side(*first)->currentIndex());
				expect(number(*first)->text() == QStringLiteral("2"), "changing side retains the number");
				render(window, first, unified ? QStringLiteral("line-unified-dialog") : QStringLiteral("line-split-dialog"));
				for (auto* control : { static_cast<QWidget*>(number(*first)), static_cast<QWidget*>(side(*first)), static_cast<QWidget*>(go(*first)) }) {
					expect(first->rect().contains(QRect{ control->mapTo(first, QPoint{ }), control->size() }), "dialog controls fit minimum-size window at course scale");
				}
				cancel(*first, sourceSide == core::diff_side::right);
				expect(surface.selection() == selection && surface.scroll_controller()->current_frame() == frame, "Escape preserves reading selection and scroll");
				for (const std::uint32_t line : { 1U, 500U, 1106U }) {
					auto* request = dialog(window, false);
					submit(window, *request, sourceSide, line);
				}
				// A repeated selected target must be revealed after ordinary scrolling.
				if (unified) {
					surface.unified_pane()->verticalScrollBar()->setValue(0);
				} else {
					surface.scroll_controller()->home();
				}
				expect(!surface.source_line_visible(sourceSide, 1106U), "selected last line is first scrolled away");
				auto* repeated = dialog(window, true);
				submit(window, *repeated, sourceSide, 1106U);
				tests::window_system_key(window, Qt::Key_Up);
				// An unchanged unified row has both endpoints; ordinary Up chooses
				// its existing Head identity after starting on the exact requested row.
				expect_target(surface, unified ? core::diff_side::right : sourceSide, 1105U);
			}
			const auto after = surface.statistics();
			expect(after.SemanticIndexAdoptions == before.SemanticIndexAdoptions && after.Publications == before.Publications
					   && after.FindMatchingRuns == before.FindMatchingRuns && after.PaneMeasurements == before.PaneMeasurements,
				   "line navigation reuses loaded source, semantic index, measurements and search; no reacquisition or diff publication");
			window.line_numbers_action()->trigger();
			aWait([&] { return !window.line_numbers_action()->isChecked(); }, "line numbers hidden through profile command");
			auto* hidden = dialog(window, false);
			submit(window, *hidden, core::diff_side::right, 500U);
			render(window, nullptr, unified ? QStringLiteral("line-unified-target") : QStringLiteral("line-split-target"));
			window.line_numbers_action()->trigger();
			aWait([&] { return window.line_numbers_action()->isChecked() && !surface.geometry_pending(); }, "line numbers restored through profile command");
		}
		const auto hostAfter = app::detail::github_application_fixture_access::diff_work_evidence(aApplication);
		expect(hostAfter.FixtureDiffHelperInvocations == hostBefore.FixtureDiffHelperInvocations
				   && hostAfter.PreparedRemoteBlobs == hostBefore.PreparedRemoteBlobs && hostAfter.PreparedRemoteBatches == hostBefore.PreparedRemoteBatches,
			   "Go to line starts no source acquisition or diff computation through installed Git/cache");
		// Explicit menu invocation from a writable draft leaves its text intact.
		surface.focus_source(core::diff_side::right);
		tests::window_system_key(window, Qt::Key_Return);
		aWait([&] { return window.conversation_panel()->cards()->draft_editor() && window.conversation_panel()->cards()->draft_editor()->isVisible(); }, "source Return opens draft on exact jumped line");
		expect(window.review_editor()->shown_draft() && window.review_editor()->shown_draft()->Anchor.Line == 500U
				   && window.review_editor()->shown_draft()->Anchor.Side == core::diff_side::right,
			   "composed draft targets Go to line's exact source anchor");
		auto* editor = window.conversation_panel()->cards()->draft_editor();
		editor->setPlainText(QStringLiteral("Go to line draft remains private"));
		editor->setFocus();
		QApplication::processEvents();
		expect(action->isEnabled() && action->shortcuts().isEmpty(), "editable draft retains native key handling while explicit menu is enabled");
		const auto key = shortcut[0];
		tests::window_system_key(window, key.key(), key.keyboardModifiers());
		expect(!window.findChild<QDialog*>(QStringLiteral("goToLineDialog")), "editing hotkey does not open line dialog");
		auto* explicitMenu = dialog(window, false);
		cancel(*explicitMenu);
		expect(editor->hasFocus() && editor->toPlainText() == QStringLiteral("Go to line draft remains private"), "cancel returns invoking editor focus and preserves its draft");
		window.conversation_panel()->cards()->end_draft();
		surface.focus_source();
		QApplication::processEvents();
		// Asynchronous file, tab and comparison changes all retire the request.
		auto* staleFile = dialog(window, false);
		aOpen(aApplication, "src/other.cpp");
		number(*staleFile)->setText(QStringLiteral("1"));
		expect(!go(*staleFile)->isEnabled() && staleFile->findChild<QLabel*>(QStringLiteral("goToLineValidation"))->text().contains(QStringLiteral("changed")), "replaced file refuses old request");
		const auto replacement = surface.selection();
		tests::window_system_key(*staleFile, Qt::Key_Return);
		expect(surface.selection() == replacement && staleFile->isVisible(), "submission refuses to apply old number to replacement file");
		cancel(*staleFile);
		aOpen(aApplication, "src/find.cpp");
		surface.focus_source();
		auto* staleTab = dialog(window, false);
		window.document_tabs()->new_tab_button()->click();
		aWait([&] { return window.document_tabs()->tabs().size() == 2; }, "new tab invalidates bound request");
		expect(!go(*staleTab)->isEnabled(), "tab replacement refuses request even with identical file material");
		cancel(*staleTab);
		window.close_tab_action()->trigger();
		aWait([&] { return window.document_tabs()->tabs().size() == 1 && !surface.geometry_pending(); }, "original tab restored");
		// Base and Head are the displayed historical comparison's endpoints.
		surface.focus_source();
		auto* staleComparison = dialog(window, false);
		auto* controls        = window.epoch_selectors();
		int   baseRow         = -1;
		for (int row = 0; row < controls->history_model()->rowCount(); ++row) {
			const auto anchor = controls->history_model()->anchor_at(controls->history_model()->index(row, 0));
			if (anchor && anchor->Identity.Kind == core::comparison_anchor_kind::epoch_base) {
				baseRow = row;
				break;
			}
		}
		expect(baseRow >= 0, "fixture retains its exact base endpoint");
		controls->right_selector()->setCurrentIndex(baseRow);
		aWait([&] { const auto comparison = app::detail::github_application_fixture_access::epoch_refresh_identity(aApplication).AcceptedComparison;
			return comparison && comparison->Left.Commit == comparison->Right.Commit && surface.current_result() && !surface.geometry_pending(); }, "older displayed comparison loads through Git cache");
		expect(!go(*staleComparison)->isEnabled(), "comparison replacement refuses the bound request");
		cancel(*staleComparison);
		surface.focus_source();
		auto* older = dialog(window, false);
		submit(window, *older, core::diff_side::right, 500U);
		controls->whole_pr_action()->trigger();
		aWait([&] { const auto comparison = app::detail::github_application_fixture_access::epoch_refresh_identity(aApplication).AcceptedComparison;
			return comparison && comparison->Left.Commit != comparison->Right.Commit && surface.current_result() && !surface.geometry_pending(); }, "whole PR pair returns");
		for (const auto& [path, available] : { std::pair{ "src/added.cpp", core::diff_side::right }, std::pair{ "src/emptied.cpp", core::diff_side::left } }) {
			aOpen(aApplication, path);
			surface.focus_source();
			auto*      soleSide = dialog(window, false);
			const auto chosen   = available == core::diff_side::left ? 0 : 1;
			expect(side(*soleSide)->currentIndex() == chosen && !(side(*soleSide)->model()->flags(side(*soleSide)->model()->index(1 - chosen, 0)) & Qt::ItemIsEnabled),
				   "sole loaded nonempty side is default and unavailable side cannot be chosen");
			submit(window, *soleSide, available, 1U);
		}
		aOpen(aApplication, "src/empty.cpp");
		surface.focus_source();
		QApplication::processEvents();
		expect(!action->isEnabled(), "empty text comparison has no navigable source line");
		aOpen(aApplication, "src/find.cpp");
		surface.focus_source(core::diff_side::left);
		QApplication::processEvents();
		window.document_tabs()->tab_bar()->setFocus();
		QApplication::processEvents();
		if (!surface.one_pane_enabled()) {
			auto* chrome = dialog(window, true);
			expect(side(*chrome)->currentIndex() == 0, "chrome invocation keeps most recently focused source side for this comparison");
			cancel(*chrome);
		}
		for (const auto modality : { qt::review_modality::overview, qt::review_modality::discuss, qt::review_modality::threads }) {
			window.choose_modality(modality);
			QApplication::processEvents();
			expect(!action->isEnabled(), "non-Review modality disables retained line command");
		}
		window.choose_modality(qt::review_modality::review);
		window.resize(originalSize);
		if (surface.one_pane_enabled()) {
			window.one_pane_action()->trigger();
			aWait([&] { return !surface.one_pane_enabled() && !surface.geometry_pending(); }, "split profile presentation restored");
		}
		surface.focus_source();
		QApplication::processEvents();
		std::cout << "GO_TO_LINE_PUBLIC_PATH_OK minimum=" << window.minimumWidth() << "x" << window.minimumHeight()
				  << " scale=" << window.devicePixelRatioF() << " source_lines=1107 presentations=split,unified\n";
	}
}
#endif
