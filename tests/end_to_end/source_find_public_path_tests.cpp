#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QThread>
#include <QToolButton>
#include <iostream>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { })
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (!aCondition() && timer.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (aDiagnostic && !aCondition()) {
			aDiagnostic();
		}
		expect(aCondition(), aMessage);
	}
	void settle()
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < 180) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_FIND_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "owned render root");
		expect(aWindow.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")), "full shipped composition render saved");
	}
	void query(app::github_application& aApplication, qt::source_find_bar& aFind, const QString& aQuery)
	{
		static std::uint32_t ordinal = 0U;
		++ordinal;
		aFind.query_editor()->setPlainText(aQuery);
		aFind.flush();
		wait_for([&] { return aFind.projection().Status != core::source_search_status::searching; }, "pane query terminal", [&] {
			const auto& projection = aFind.projection();
			const auto host = access::epoch_refresh_identity(aApplication);
			const auto* surface = dynamic_cast<const qt::diff_workbench*>(aFind.parentWidget());
			std::cerr << "PANE_QUERY_FAILURE ordinal=" << ordinal << " query=" << aQuery.toStdString()
				<< " status=" << static_cast<int>(projection.Status) << " generation=" << projection.Generation
				<< " query_match=" << (projection.Query == aQuery.toStdString()) << " scope=" << projection.Scope.has_value()
				<< " matches=" << projection.Summary.Matches << " hidden=" << aFind.isHidden()
				<< " field_match=" << (aFind.query() == aQuery)
				<< " matching_runs=" << (surface ? surface->statistics().FindMatchingRuns : 0U)
				<< " cached_runs=" << (surface ? surface->statistics().FindCachedRuns : 0U)
				<< " host_admitted=" << host.SearchCounters[0] << " host_scheduled=" << host.SearchCounters[1]
				<< " host_terminals=" << host.SearchCounters[2] << " host_stale=" << host.SearchCounters[3]
				<< " projection_failures=" << host.SearchCounters[4] << " ingress_refusals=" << host.SearchCounters[5]
				<< " search_quiet=" << host.SearchQuiescent << " displayed=" << host.PublicationDisplayed
				<< " active_generation=" << host.SearchActiveGeneration << " queued_generation=" << host.SearchQueuedGeneration
				<< " active_stage=" << host.SearchWorkerStage << " active_stopped=" << host.SearchActiveStopped
				<< " effect_entered=" << host.SearchPipeline[0] << " execution_returned=" << host.SearchPipeline[1]
				<< " validation_returned=" << host.SearchPipeline[2] << " observer_returned=" << host.SearchPipeline[3]
				<< " decision_post_entered=" << host.SearchPipeline[4] << " decision_post_returned=" << host.SearchPipeline[5]
				<< " decision_post_failed=" << host.SearchPipeline[6] << " decision_entered=" << host.SearchPipeline[7]
				<< " direct_matching_runs=" << host.FindExecutionCounts[0] << " direct_cached_runs=" << host.FindExecutionCounts[1]
				<< " diff_effects_idle=" << host.DiffEffectsIdle << " diff_queued_tasks=" << host.DiffQueuedTasks << '\n'; });
		settle();
	}
	/// [[BPR-1503]], [[BPR-1527]]. Find leaves both pane layouts and
	/// displayed source rectangles unchanged. Alignment scrolling can change
	/// invisible overscan; its backing widget size is not a displayed extent.
	auto pane_geometry(qt::diff_workbench& aSurface) -> std::array<QRect, 4>
	{
		const auto canvas = [&aSurface](const QPlainTextEdit& aPane) {
			const auto* viewport = aPane.viewport();
			const auto  mask     = viewport->mask();
			const auto  clipped  = mask.isEmpty() ? viewport->rect() : viewport->rect().intersected(mask.boundingRect());
			return QRect{ viewport->mapTo(&aSurface, clipped.topLeft()), clipped.size() };
		};
		return { QRect{ aSurface.left_pane()->mapTo(&aSurface, QPoint{ 0, 0 }), aSurface.left_pane()->size() },
				 QRect{ aSurface.right_pane()->mapTo(&aSurface, QPoint{ 0, 0 }), aSurface.right_pane()->size() },
				 canvas(*aSurface.left_pane()), canvas(*aSurface.right_pane()) };
	}
	/// [[BPR-1503]]. The find widget floats over its pane's top-right corner.
	void expect_floating(qt::diff_workbench& aSurface, qt::source_find_bar& aBar, const QPlainTextEdit& aPane, const char* aMessage)
	{
		const auto pane = QRect{ aPane.viewport()->mapTo(&aSurface, QPoint{ 0, 0 }), aPane.viewport()->size() };
		expect(aBar.parentWidget() == &aSurface && aSurface.layout()->indexOf(&aBar) < 0, "the find widget is in no layout");
		expect(pane.contains(aBar.geometry().topRight()) && aBar.geometry().top() - pane.top() <= 8 && pane.right() - aBar.geometry().right() <= 12, aMessage);
	}
	/// [[BPR-1503]]. How many find washes a pane paints, in the light theme's
	/// match and current-match colours.
	auto find_washes(const QPlainTextEdit& aPane) -> std::pair<int, int>
	{
		const auto& palette = qt::builtin_appearance_palette(core::appearance_variant::light);
		const auto  match   = qt::to_presentation_color(palette[core::index_of(core::appearance_role::find_match_background)]).rgb();
		const auto  current = qt::to_presentation_color(palette[core::index_of(core::appearance_role::find_current_match_background)]).rgb();
		auto        found   = std::pair{ 0, 0 };
		for (const auto& selection : aPane.extraSelections()) {
			const auto color = selection.format.background().color();
			if (color.alpha() == core::selection_wash_alpha && color.rgb() == match) {
				++found.first;
			}
			if (color.alpha() == core::selection_wash_alpha && color.rgb() == current) {
				++found.second;
			}
		}
		return found;
	}
	void open(app::github_application& aApplication, const std::string_view aPath)
	{
		auto&      window = aApplication.window();
		const auto index  = window.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath));
		expect(index.isValid(), "fixture path in accepted catalogue");
		window.files_drawer()->contents_view()->setCurrentIndex(index);
		wait_for([&] {auto* surface=window.comparison_surface();return surface->current_result() && surface->current_context()->Key.File==core::file_identity_of(aPath) && !surface->geometry_pending(); }, "actual installed-Git file comparison");
	}
}
int main(int aArgc, char** aArgv)
{
	QApplication  applicationQt{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-source-find-XXXXXX") };
	auto          options       = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = e2e::private_data_root(temporary);
	expect(options.ApplicationDataRoot.has_value(), "private data root");
	std::unique_ptr<app::github_application> application;
	bool                                     ready   = false;
	auto                                     startup = access::start(app::detail::github_public_path_fixture::source_find_v1, options, [&](auto aResult) {ready=true;if(aResult){application=std::move(*aResult);
} });
	wait_for([&] { return ready && startup->is_quiescent(); }, "shipped composition startup");
	expect(application != nullptr, "startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);
	auto* findAction     = window.findChild<QAction*>(QStringLiteral("sourceFindAction"));
	auto* nextAction     = window.findChild<QAction*>(QStringLiteral("sourceFindNextAction"));
	auto* previousAction = window.findChild<QAction*>(QStringLiteral("sourceFindPreviousAction"));
	expect(findAction && nextAction && previousAction && !findAction->isEnabled(), "Home cannot Find hidden PR source");
	for (const auto* action : { findAction, nextAction, previousAction }) {
		for (const auto& shortcut : action->shortcuts()) {
			int owners = 0;
			for (const auto* candidate : window.findChildren<QAction*>()) {
				owners += candidate->shortcuts().contains(shortcut) ? 1 : 0;
			}
			expect(owners == 1, "each composed Find key has exactly one action owner");
		}
	}

	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/source-find#77"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "PR opens on Overview");
	auto entry  = app::detail::course_modality_step{ };
	entry.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, entry) != app::detail::course_modality_result::pending; }, "normal Review entry");
	window.choose_modality(qt::review_modality::review);
	settle();
	wait_for([&] { return window.files_drawer()->contents()->rowCount() > 0; }, "accepted source catalogue");
	open(*application, "src/find.cpp");
	auto* surface = window.comparison_surface();
	auto* bar     = surface->source_find();
	surface->right_pane()->setFocus();
	QApplication::processEvents();
	expect(findAction->isEnabled(), "ready source Find available");
	settle();
	const auto closedPanes = pane_geometry(*surface);
	tests::window_system_key(window, Qt::Key_F, Qt::ControlModifier, QStringLiteral("f"));
	wait_for([&] { return !bar->isHidden(); }, "physical Find shortcut opens native query");
	expect(QApplication::focusWidget() == bar->query_editor(), "Find focuses multiline query");
	settle();
	expect(pane_geometry(*surface) == closedPanes, "opening Find leaves every pane's geometry unchanged");
	expect_floating(*surface, *bar, *surface->right_pane(), "Find floats over the focused right pane's top-right corner");
	query(*application, *bar, QStringLiteral("offscreen needle"));
	expect(pane_geometry(*surface) == closedPanes, "a query and its reveal leave the panes' geometry unchanged");
	expect(bar->projection().Summary.Matches == 1U && surface->right_pane()->textCursor().selectedText() == QStringLiteral("offscreen needle"), "Find reaches full offscreen unchanged source");
	expect(surface->left_pane()->textCursor().selectedText().isEmpty() && surface->selection()->PreferredLine.Side == core::diff_side::right, "right Find does not select left source");
	expect(surface->source_line_visible(core::diff_side::right, 1102U), "shared controller reveals offscreen actual row");
	render(window, QStringLiteral("01-split-offscreen"));
	query(*application, *bar, QStringLiteral("needle"));
	expect(bar->projection().Summary.Matches == 2U, "complete match count");
	{
		const auto [matches, currents] = find_washes(*surface->right_pane());
		expect(matches == 1 && currents == 1, "one match carries the find wash and the current one the theme's stronger current wash");
	}
	// Enter and Shift+Enter in the query traverse, as in VS Code.
	bar->query_editor()->setFocus();
	const auto enterFrom = *bar->projection().CurrentMatch;
	tests::window_system_key(window, Qt::Key_Return);
	wait_for([&] { return bar->projection().CurrentMatch == std::optional{ (enterFrom + 1U) % 2U }; }, "Enter goes to the next match");
	tests::window_system_key(window, Qt::Key_Return, Qt::ShiftModifier);
	wait_for([&] { return bar->projection().CurrentMatch == std::optional{ enterFrom }; }, "Shift+Enter goes back to the previous match");
	// The options are toggles inside the query field; Option+W toggles Whole
	// Word and types nothing. Option+C and Option+R stay Comment and Compose
	// review (owner, 2026-10-14), so the Case and Regex toggles take no key.
	for (const auto option : { qt::source_find_option::match_case, qt::source_find_option::whole_word, qt::source_find_option::regular_expression }) {
		auto* toggle = bar->option_button(option);
		expect(toggle && toggle->isCheckable() && !toggle->isChecked() && toggle->parentWidget() != bar, "each option is a toggle inside the query field");
	}
	tests::window_system_key(window, Qt::Key_W, Qt::AltModifier, QString::fromUtf8("∑"));
	expect(bar->option_button(qt::source_find_option::whole_word)->isChecked() && bar->query() == "needle", "Option+W in the query toggles Whole Word without typing");
	tests::window_system_key(window, Qt::Key_W, Qt::AltModifier, QString::fromUtf8("∑"));
	expect(!bar->option_button(qt::source_find_option::whole_word)->isChecked() && bar->query() == "needle", "and toggles it back");
	expect(bar->option_button(qt::source_find_option::match_case)->toolTip() == QStringLiteral("Match Case") && bar->option_button(qt::source_find_option::regular_expression)->toolTip() == QStringLiteral("Use Regular Expression"), "Match Case and Regular Expression name no Option key");
	wait_for([&] { return bar->projection().Summary.Matches == 2U; }, "options restored");
	// Seeding keeps the current match on the selected occurrence.
	{
		auto* right = surface->right_pane();
		auto  found = right->document()->find(QStringLiteral("needle"));
		expect(!found.isNull(), "a first needle occurrence");
		auto last = found;
		while (!found.isNull()) {
			last  = found;
			found = right->document()->find(QStringLiteral("needle"), found);
		}
		surface->close_source_find();
		right->setTextCursor(last);
		right->setFocus();
		findAction->trigger();
		wait_for([&] { return bar->projection().Summary.Matches == 2U && bar->projection().CurrentMatch.has_value(); }, "selection seed searches");
		expect(bar->query() == "needle" && *bar->projection().CurrentMatch == 1U && bar->count_text() == QStringLiteral("2 of 2"), "Cmd+F on the last occurrence makes that occurrence current, not the next one");
		const auto firstNeedle = right->document()->find(QStringLiteral("needle"));
		surface->close_source_find();
		right->setTextCursor(firstNeedle);
		right->setFocus();
		findAction->trigger();
		wait_for([&] { return bar->projection().Summary.Matches == 2U && bar->projection().CurrentMatch.has_value(); }, "selection seed searches again");
		expect(*bar->projection().CurrentMatch == 0U && bar->count_text() == QStringLiteral("1 of 2") && right->textCursor().selectionStart() == firstNeedle.selectionStart(), "Cmd+F on the first occurrence stays on it");
	}
	surface->close_source_find();
	settle();
	expect(pane_geometry(*surface) == closedPanes, "closing Find leaves the panes' geometry unchanged");
	surface->right_pane()->setFocus();
	findAction->trigger();
	query(*application, *bar, QStringLiteral("needle"));
	const auto records     = bar->projection().Matches;
	const auto diff        = surface->current_result();
	const auto index       = surface->current_semantic_index();
	const auto conversions = surface->statistics().FindEndpointConversions;
	const auto scans       = surface->statistics().FindMatchingRuns;
	tests::window_system_key(window, Qt::Key_F3);
	tests::window_system_key(window, Qt::Key_F3, Qt::ShiftModifier);
	previousAction->trigger();
	expect(bar->projection().Wrapped, "previous wraps with visible/accessibility status");
	for (int step = 0; step < 1000; ++step) {
		nextAction->trigger();
	}
	expect(bar->projection().Matches == records && surface->current_result() == diff && surface->current_semantic_index() == index, "traversal shares exact records/material/index without replacement");
	expect(surface->statistics().FindEndpointConversions == conversions, "1,000 traversals perform zero source endpoint conversions");
	expect(surface->statistics().FindMatchingRuns == scans, "1,000 traversals call no source matcher");
	query(*application, *bar, QStringLiteral("earlier only"));
	expect(bar->projection().NoResults, "right pane cannot match left-only content");
	surface->left_pane()->setFocus();
	findAction->trigger();
	query(*application, *bar, QStringLiteral("earlier only"));
	expect(bar->projection().Summary.Matches == 1U && surface->left_pane()->textCursor().selectedText() == QStringLiteral("earlier only"), "explicit left Find retargets exact earlier source");
	surface->close_source_find();
	settle();
	auto caret = QTextCursor{ surface->left_pane()->document() };
	caret.setPosition(3);
	surface->left_pane()->setTextCursor(caret);
	const auto point   = surface->left_pane()->cursorRect(caret).center();
	auto       press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ point }, QPointF{ surface->left_pane()->viewport()->mapToGlobal(point) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
	auto       release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ point }, QPointF{ surface->left_pane()->viewport()->mapToGlobal(point) }, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
	QApplication::sendEvent(surface->left_pane()->viewport(), &press);
	QApplication::sendEvent(surface->left_pane()->viewport(), &release);
	findAction->trigger();
	expect(bar->query() == "earlier", "real source caret seeds its native word");
	query(*application, *bar, QStringLiteral("needle"));
	bar->query_editor()->setPlainText(QStringLiteral("nee"));
	auto editCaret = bar->query_editor()->textCursor();
	editCaret.movePosition(QTextCursor::End);
	bar->query_editor()->setTextCursor(editCaret);
	auto preedit = QInputMethodEvent{ QStringLiteral("dle"), { } };
	QApplication::sendEvent(bar->query_editor(), &preedit);
	settle();
	expect(!bar->projection().Matches && bar->query() == "nee", "IME preedit submits no uncommitted query");
	auto commit = QInputMethodEvent{ };
	commit.setCommitString(QStringLiteral("dle"));
	QApplication::sendEvent(bar->query_editor(), &commit);
	wait_for([&] { return bar->query() == "needle" && bar->projection().Summary.Matches == 2U; }, "committed IME input searches after debounce");

	// A real native selection seeds the query and fixes original byte bounds.
	auto       cursor = QTextCursor{ surface->left_pane()->document() };
	const auto first  = surface->left_pane()->document()->findBlockByNumber(1102);
	cursor.setPosition(first.position());
	cursor.setPosition(first.position() + static_cast<int>(first.text().size()), QTextCursor::KeepAnchor);
	surface->left_pane()->setTextCursor(cursor);
	surface->close_source_find();
	surface->left_pane()->setFocus();
	findAction->trigger();
	wait_for([&] { return bar->projection().Summary.Matches == 1U; }, "selected text seeded through worker");
	expect(bar->query() == "offscreen needle", "native selection seed");
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindSelection"))->click();
	query(*application, *bar, QStringLiteral("needle"));
	const auto fixed = bar->projection().Scope->Selection;
	expect(fixed && bar->projection().Summary.Matches == 1U, "Find in Selection excludes other occurrence");
	for (int step = 0; step < 20; ++step) {
		nextAction->trigger();
	}
	expect(bar->projection().Scope->Selection == fixed, "navigation never moves fixed source bounds");
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindSelection"))->click();
	query(*application, *bar, QString::fromUtf8("𝄞 café Σσς"));
	auto queryCaret = bar->query_editor()->textCursor();
	queryCaret.movePosition(QTextCursor::End);
	bar->query_editor()->setTextCursor(queryCaret);
	tests::window_system_key(window, Qt::Key_Return, Qt::ControlModifier);
	bar->query_editor()->insertPlainText(QString::fromUtf8("Straße K𐐀"));
	tests::window_system_key(window, Qt::Key_Return);
	wait_for([&] { return bar->projection().Summary.Matches == 1U && bar->projection().Status == core::source_search_status::complete; }, "Enter bypasses multiline typing debounce");
	settle();
	query(*application, *bar, QString::fromUtf8("𝄞 café Σσς\nStraße K𐐀"));
	expect(bar->projection().Summary.Matches == 1U, "Unicode multiline CRLF logical query");
	expect(surface->left_pane()->textCursor().selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n')) == QString::fromUtf8("𝄞 café Σσς\nStraße K𐐀"), "exact surrogate/scalar and CRLF native span");
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindRegex"))->click();
	query(*application, *bar, QStringLiteral("("));
	expect(bar->projection().Status == core::source_search_status::invalid_pattern && !bar->projection().Matches && !nextAction->isEnabled(), "invalid regex clears current hits and navigation");
	expect(bar->error_state() && bar->status_text() == QStringLiteral("Invalid regular expression") && bar->query_editor()->parentWidget()->toolTip() == QStringLiteral("Invalid regular expression"), "an invalid regex is an error state on the field, its message in the tooltip");
	render(window, QStringLiteral("05-invalid-regex"));
	query(*application, *bar, QStringLiteral("(?=needle)"));
	expect(bar->projection().Status == core::source_search_status::unsupported_pattern && bar->error_state(), "bounded unsupported regex status");
	query(*application, *bar, QStringLiteral("\\z"));
	expect(bar->projection().Summary.Matches == 1U && surface->left_pane()->textCursor().position() == surface->left_pane()->document()->characterCount() - 1, "terminal zero-width match selects actual native EOF once");
	expect(!surface->selection(), "EOF without a source line cannot invent a comment anchor");

	bar->findChild<QToolButton*>(QStringLiteral("sourceFindRegex"))->click();
	query(*application, *bar, QStringLiteral("ΣΣΣ"));
	expect(bar->projection().Summary.Matches == 1U, "shared Unicode simple fold default");
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindCase"))->click();
	wait_for([&] { return bar->projection().NoResults; }, "Match Case distinguishes exact source scalars");
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindCase"))->click();
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindWord"))->click();
	query(*application, *bar, QStringLiteral("need"));
	expect(bar->projection().NoResults, "Whole Word excludes prefixes");
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindWord"))->click();
	query(*application, *bar, QStringLiteral("needle"));
	const auto beforeCloseScans       = surface->statistics().FindMatchingRuns;
	const auto beforeCloseConversions = surface->statistics().FindEndpointConversions;
	const auto beforeCloseMatch       = *bar->projection().CurrentMatch;
	tests::window_system_key(window, Qt::Key_Escape, Qt::NoModifier, { });
	expect(bar->isHidden() && QApplication::focusWidget() == surface->left_pane(), "Escape restores exact target focus");
	const auto sourceCursor = surface->left_pane()->textCursor();
	expect(sourceCursor.hasSelection(), "closing keeps the current occurrence selected");
	wait_for([&] { return nextAction->isEnabled(); }, "Next becomes available after close layout settles");
	nextAction->trigger();
	settle();

	wait_for([&] { return !bar->isHidden() && bar->projection().Summary.Matches == 2U; }, "Next reopens the retained query");
	expect(surface->statistics().FindMatchingRuns == beforeCloseScans && surface->statistics().FindCachedRuns > 0U, "closed-query Next reuses exact accepted matches without recompilation or rescan");
	expect(surface->statistics().FindEndpointConversions == beforeCloseConversions, "closed-query Next reuses mapped original spans");
	expect(*bar->projection().CurrentMatch == (beforeCloseMatch + 1U) % 2U, "closed-query Next advances the retained occurrence");
	surface->close_source_find();
	auto oversized = QTextCursor{ surface->left_pane()->document() };
	oversized.select(QTextCursor::Document);
	surface->left_pane()->setTextCursor(oversized);
	findAction->trigger();
	settle();
	expect(bar->query() == "needle" && bar->status_text().contains(QStringLiteral("4 KiB")) && bar->error_state() && bar->findChild<QLabel*>(QStringLiteral("sourceFindStatus"))->toolTip().contains(QStringLiteral("4 KiB")), "oversized source seed retains the query and shows the input limit as the field's error, its words in the tooltip");
	query(*application, *bar, QString(4097, QLatin1Char('a')));
	expect(bar->projection().Status == core::source_search_status::invalid_input && !bar->projection().Matches && !nextAction->isEnabled(), "oversized native query refuses work and old hits");
	query(*application, *bar, QStringLiteral("needle"));
	surface->close_source_find();
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindRegex"))->click();
	const auto literal = surface->left_pane()->document()->find(QStringLiteral("a+b(c)[d]?"));
	expect(!literal.isNull(), "source regex punctuation fixture");
	surface->left_pane()->setTextCursor(literal);
	findAction->trigger();
	wait_for([&] { return bar->projection().Summary.Matches == 1U; }, "literal source selection seeds regex safely");
	expect(bar->query() == "a\\+b\\(c\\)\\[d\\]\\?" && surface->left_pane()->textCursor().selectedText() == QStringLiteral("a+b(c)[d]?"), "regex seed escapes native source punctuation and selects original bytes");
	bar->findChild<QToolButton*>(QStringLiteral("sourceFindRegex"))->click();
	surface->close_source_find();
	window.one_pane_action()->trigger();
	wait_for([&] { return surface->one_pane_enabled() && !surface->geometry_pending(); }, "native unified source ready");
	surface->focus_source();
	QApplication::processEvents();
	findAction->trigger();
	query(*application, *bar, QStringLiteral("offscreen needle"));
	expect(bar->projection().Summary.Matches == 1U, "unified unchanged row is searched once");
	query(*application, *bar, QStringLiteral("removed bridge\nadded bridge"));
	expect(bar->projection().NoResults, "unified adjacency cannot fabricate cross-version match");
	query(*application, *bar, QStringLiteral("removed bridge"));
	expect(bar->projection().Summary.Matches == 1U && surface->selection()->PreferredLine.Side == core::diff_side::left, "unified removed hit selects actual version");
	query(*application, *bar, QStringLiteral("removed bridge\n// unchanged source row 0000"));
	expect(bar->projection().Summary.Matches == 1U, "unified multiline follows contiguous actual earlier version");
	auto* unified = surface->findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
	expect_floating(*surface, *bar, *unified, "Find floats over the unified pane's top-right corner");
	expect(unified && !unified->textCursor().selectedText().contains(QStringLiteral("added bridge")), "native occurrence excludes opposing visual rows");
	const auto wrongRow = unified->document()->findBlockByNumber(3);
	for (const auto& selection : unified->extraSelections()) {
		if (selection.format.background().color().alpha() == core::selection_wash_alpha) {
			expect(selection.cursor.selectionEnd() <= wrongRow.position() || selection.cursor.selectionStart() >= wrongRow.position() + wrongRow.length(), "Find wash excludes opposing row");
		}
	}
	unified->setFocus();
	tests::window_system_key(window, Qt::Key_C, Qt::ControlModifier, QStringLiteral("c"));
	expect(QApplication::clipboard()->text() == QStringLiteral("removed bridge\n// unchanged source row 0000"), "explicit Copy yields actual source occurrence around opposing rows");
	bar->query_editor()->setFocus();
	query(*application, *bar, QStringLiteral("added bridge"));
	expect(bar->projection().Summary.Matches == 1U && surface->selection()->PreferredLine.Side == core::diff_side::right, "unified added hit selects actual version");
	render(window, QStringLiteral("02-unified"));
	window.resize(1920, 1080);
	settle();
	expect_floating(*surface, *bar, *unified, "Find stays on the unified pane's top-right corner at full width");
	render(window, QStringLiteral("02-unified-full"));
	window.resize(1280, 860);
	settle();
	window.one_pane_action()->trigger();
	wait_for([&] { return !surface->one_pane_enabled() && !surface->geometry_pending(); }, "split source ready again");
	surface->right_pane()->setFocus();
	findAction->trigger();
	query(*application, *bar, QStringLiteral("needle"));
	window.resize(900, 700);
	settle();
	std::cerr << "source-find-narrow window=" << window.width() << " surface=" << surface->width()
			  << " bar=" << bar->geometry().x() << ',' << bar->geometry().y() << ',' << bar->width() << 'x' << bar->height()
			  << " minimum=" << bar->minimumSizeHint().width() << " query=" << bar->query_editor()->width() << '\n';
	expect(bar->width() >= bar->minimumSizeHint().width() && bar->query_editor()->width() >= 48 && bar->geometry().right() < surface->width() && bar->geometry().left() >= 0, "the floating widget fits the supported 900-pixel window, every drawer open");
	render(window, QStringLiteral("03-narrow-900"));
	window.resize(720, 640);
	settle();
	expect(bar->width() >= bar->minimumSizeHint().width() && bar->query_editor()->width() >= 48 && bar->geometry().left() >= 0, "the floating widget keeps every control below the supported width");
	for (auto* control : bar->findChildren<QToolButton*>()) {
		const bool givesWay = control->objectName() == QStringLiteral("sourceFindSelection");
		expect((control->isVisible() || givesWay) && !control->accessibleName().isEmpty(), "every narrow control but find-in-selection stays visible, all labelled");
	}
	{
		auto*      inSelection = bar->findChild<QToolButton*>(QStringLiteral("sourceFindSelection"));
		const auto before      = inSelection->isChecked();
		bar->query_editor()->setFocus();
		tests::window_system_key(window, Qt::Key_L, Qt::AltModifier, QString::fromUtf8("¬"));
		expect(inSelection->isChecked() == (before != inSelection->isEnabled()), "Option+L toggles Find in Selection even where its icon gave way");
		if (inSelection->isChecked() != before) {
			tests::window_system_key(window, Qt::Key_L, Qt::AltModifier, QString::fromUtf8("¬"));
		}
	}
	expect(bar->font().family() == QApplication::font().family() && bar->styleSheet().isEmpty(), "native UI font and no QSS");
	render(window, QStringLiteral("03-narrow-720"));
	window.resize(1280, 860);
	settle();
	auto*      appearance = window.appearance_settings();
	const auto beforeMode = appearance->adopted().Mode;
	appearance->choose_mode(core::appearance_mode::dark);
	wait_for([&] { return appearance->resolved_variant() == core::appearance_variant::dark; }, "public appearance host selects dark palette");
	settle();
	expect(bar->palette().color(QPalette::Window) == window.palette().color(QPalette::Window), "Find inherits the accepted live window palette");
	render(window, QStringLiteral("04-dark-native-controls"));
	window.resize(900, 700);
	settle();
	render(window, QStringLiteral("04-dark-900"));
	window.resize(1920, 1080);
	settle();
	render(window, QStringLiteral("04-dark-full"));
	window.resize(1280, 860);
	settle();
	appearance->choose_mode(beforeMode);
	settle();
	query(*application, *bar, QStringLiteral("needle"));
	open(*application, "src/other.cpp");
	expect(!bar->projection().Matches && !bar->findChild<QToolButton*>(QStringLiteral("sourceFindSelection"))->isChecked(), "document replacement clears decorations and fixed selection immediately");
	surface->close_source_find();
	window.choose_modality(qt::review_modality::overview);
	settle();
	expect(!findAction->isEnabled(), "Overview refuses hidden source Find");
	window.choose_modality(qt::review_modality::review);
	settle();
	surface->focus_source();
	QApplication::processEvents();
	QDialog modal{ &window };
	modal.setModal(true);
	modal.show();
	QApplication::processEvents();
	findAction->trigger();
	expect(bar->isHidden(), "modal dialog owns Find keys");
	modal.close();
	auto* editor = new QLineEdit{ &window };
	editor->show();
	editor->setFocus();
	QApplication::processEvents();
	expect(!findAction->isEnabled() && !nextAction->isEnabled(), "unrelated editor owns Find and traversal keys");
	delete editor;
	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "pane search and all composed workers quiesce");
	std::cout << "SOURCE_FIND_PUBLIC_PATH_OK platform=" << QSysInfo::productVersion().toStdString() << " qt=" << qVersion() << " scale=" << window.devicePixelRatioF() << " width=" << window.width() << " lines=1106 navigation=1000\n";
	return 0;
}
