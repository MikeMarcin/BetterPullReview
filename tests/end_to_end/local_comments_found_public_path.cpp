// RFC-BPR-0127 ([[BPR-2159]] AC07, AC08, AC09). Local comments found and
// counted where drafts are, through the composed application: Threads' state
// filter and its Local rows (and their row menus), Overview's Local chip and
// My review by state, and Finish your review's quiet Local line and Keep
// Local on a draft that is not in the review.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/draft_roster.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition()) {
			std::cerr << "local-comments-found: waited for: " << aMessage << '\n';
		}
		expect(aCondition(), aMessage);
	}

	auto open_button(qt::conversation_cards_view& aCards, const char* aName) -> QAbstractButton*
	{
		return aCards.open_card() == nullptr ? nullptr : aCards.open_card()->findChild<QAbstractButton*>(QString::fromLatin1(aName));
	}

	auto local_row(const std::uint64_t aId) -> std::string
	{
		const auto id = std::string{ qt::finder_local_prefix } + std::to_string(aId);
		return "row:" + id + "/" + id;
	}

	auto shown_rows(qt::conversation_finder& aFinder) -> std::vector<std::string>
	{
		auto rows = std::vector<std::string>{ };
		for (const auto& key : aFinder.shown_keys()) {
			if (key.starts_with("row:")) {
				rows.push_back(key);
			}
		}
		return rows;
	}

	void click_at(QWidget& aWidget, const QPointF aPoint)
	{
		auto press   = QMouseEvent{ QEvent::MouseButtonPress, aPoint, aWidget.mapToGlobal(aPoint), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto release = QMouseEvent{ QEvent::MouseButtonRelease, aPoint, aWidget.mapToGlobal(aPoint), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(&aWidget, &press);
		QApplication::sendEvent(&aWidget, &release);
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-local-found-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto server                    = std::make_shared<app::detail::review_submission_fixture_state>();
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = dataRoot;
	options.ReviewSubmissionServer = server;
	options.RepeatReadSessions     = true;
	std::unique_ptr<app::github_application> application;
	{
		bool terminal = false;
		auto startup  = access::start(app::detail::github_public_path_fixture::conversations_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup->is_quiescent(); }, "composition starts");
		expect(application != nullptr, "startup succeeds");
	}
	application->show();
	auto& window = application->window();
	window.resize(1400, 900);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/conversations#418"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.overview_page()->projection().has_value(); }, "ordinary pull request opens");
	auto* const cards  = window.conversation_panel()->cards();
	auto* const finder = window.finder();

	// AC09: with no Local comment, Finish your review has no Local line.
	window.open_review_composition();
	wait_for([&] { return window.review_submission_editor_view() != nullptr && window.review_submission_editor_view()->isVisible(); }, "Finish your review opens");
	expect(!window.review_submission_editor_view()->local_line()->isVisible(), "AC09: the Local line is absent at zero");
	window.close_review_sheet();

	// A published thread's file in Review.
	window.overview_page()->new_conversation_button()->click();
	wait_for([&] { return cards->feed() && !cards->unread_threads_in_file_order().empty(); }, "Discuss lists the file threads");
	const auto threadId = cards->unread_threads_in_file_order().front();
	const auto listed   = std::ranges::find_if(cards->cards(), [&](const qt::thread_card& aCard) { return aCard.Id == threadId; });
	expect(listed != cards->cards().end() && !listed->Path.isEmpty() && listed->Line.has_value() && listed->Side.has_value(), "the thread has a file, a side and a line");
	const auto path = listed->Path;
	const auto side = *listed->Side;
	const auto line = *listed->Line;
	window.choose_modality(qt::review_modality::review);
	wait_for([&] { return access::reveal_fixture_file(*application, path.toStdString()) == app::detail::fixture_file_reveal_result::shown; }, "the thread's file is shown in Review");
	if (window.conversations_drawer()->state() != core::workbench_drawer_state::expanded) {
		window.conversations_drawer_toggle_action()->trigger();
	}
	window.tray_tabs()->setCurrentIndex(0);

	// Mine: a Local comment on the thread's line (N, Save), one on the whole
	// file, one on the pull request, and a draft on another line that is not
	// in the review.
	wait_for([&] { return window.comparison_surface()->select_conversation_line(side, line - 1U); }, "the thread's line is selected");
	window.local_comment_action()->trigger();
	wait_for([&] { return cards->writing_draft() && window.composing_local() && cards->draft_editor() != nullptr; }, "N opens the composer on Local");
	cards->draft_editor()->setPlainText(QStringLiteral("LOCAL: ask at standup whether this can shrink."));
	wait_for([&] { auto* const save = open_button(*cards, "threadSaveLocal"); return save != nullptr && save->isVisible() && save->isEnabled(); }, "Save is offered for Local");
	open_button(*cards, "threadSaveLocal")->click();
	wait_for([&] { return cards->local_comments().size() == 1U; }, "the line's Local comment is stored");
	expect(access::seed_local_comment(*application, "LOCAL: the whole file needs a second look.", true), "the whole file's Local comment is stored");
	wait_for([&] { return cards->local_comments().size() == 2U; }, "two Local comments");
	expect(access::seed_local_comment(*application, "LOCAL: overall fine once sorted.", false), "the pull request's Local comment is stored");
	wait_for([&] { return cards->local_comments().size() == 3U; }, "three Local comments");
	window.close_drawer_composer(false);
	const auto draftLine = line + 2U;
	wait_for([&] { return window.comparison_surface()->select_conversation_line(side, draftLine - 1U); }, "another line is selected");
	wait_for([&] { return window.comment_action()->isEnabled(); }, "Comment is offered there");
	window.comment_action()->trigger();
	wait_for([&] { return cards->writing_draft() && !window.composing_local() && cards->draft_editor() != nullptr && !cards->draft_editor()->isReadOnly(); }, "Comment opens the composer on Draft");
	cards->draft_editor()->setPlainText(QStringLiteral("DRAFT: not in the review yet."));
	wait_for([&] { return window.draft_roster().NotInReview == 1U; }, "the draft is saved, not in the review");
	window.close_drawer_composer(false);
	const auto lineLocal = std::ranges::find_if(cards->local_comments(), [](const qt::local_comment_card& aCard) { return aCard.Body.startsWith(QStringLiteral("LOCAL: ask")); });
	const auto fileLocal = std::ranges::find_if(cards->local_comments(), [](const qt::local_comment_card& aCard) { return aCard.Body.startsWith(QStringLiteral("LOCAL: the whole")); });
	expect(lineLocal != cards->local_comments().end() && fileLocal != cards->local_comments().end(), "the line's and the file's Local comments are known");
	const auto lineId = lineLocal->Id;
	const auto fileId = fileLocal->Id;

	// AC07: Threads lists them in their files' groups; each choice shows
	// exactly its rows, with matching counts; under Local, Hide resolved and
	// Only new hide none.
	window.choose_modality(qt::review_modality::threads);
	wait_for([&] { return finder->isVisible() && finder->local_comments().size() == 2U; }, "Threads holds the two file Local comments");
	finder->refresh();
	const auto all = shown_rows(*finder);
	expect(std::ranges::find(all, local_row(lineId)) != all.end() && std::ranges::find(all, local_row(fileId)) != all.end(), "AC07: Threads lists Local comments under All");
	{
		// In the file's group: after its header, before the next file's.
		const auto keys   = finder->shown_keys();
		const auto header = std::ranges::find(keys, "file:" + path.toStdString());
		const auto row    = std::ranges::find(keys, local_row(lineId));
		expect(header != keys.end() && row != keys.end() && header < row && std::none_of(header + 1, row, [](const std::string& aKey) { return aKey.starts_with("file:"); }), "AC07: in their file's group");
	}
	for (const auto state : { qt::finder_state::all, qt::finder_state::published, qt::finder_state::draft, qt::finder_state::local }) {
		finder->set_filters({ .State = state });
		QApplication::processEvents();
		const auto rows  = shown_rows(*finder);
		const auto local = static_cast<std::size_t>(std::ranges::count_if(rows, [](const std::string& aRow) { return aRow.starts_with("row:local:"); }));
		const auto draft = static_cast<std::size_t>(std::ranges::count_if(rows, [](const std::string& aRow) { return aRow.starts_with("row:draft:"); }));
		std::cout << "LOCAL_FOUND_STATE " << qt::finder_state_text(state).toStdString() << " rows=" << rows.size() << " count=" << finder->state_count(state) << " local=" << local << " draft=" << draft << '\n';
		switch (state) {
		case qt::finder_state::all:       expect(local == 2U && draft == 1U, "AC07: All shows every row"); break;
		case qt::finder_state::published: expect(local == 0U && draft == 0U && !rows.empty(), "AC07: Published shows only published threads"); break;
		case qt::finder_state::draft:     expect(rows.size() == 1U && draft == 1U && finder->state_count(state) == 1U, "AC07: Draft shows exactly my draft, and counts it"); break;
		case qt::finder_state::local:     expect(rows.size() == 2U && local == 2U && finder->state_count(state) == 2U, "AC07: Local shows exactly my Local comments, and counts them"); break;
		}
	}
	finder->set_filters({ .OnlyNew = true, .HideResolved = true, .State = qt::finder_state::local });
	QApplication::processEvents();
	expect(shown_rows(*finder).size() == 2U, "AC07: under Local, Hide resolved and Only new hide no Local comment");

	// The row menu's Keep Local, on the draft: it becomes Local.
	finder->set_filters({ .State = qt::finder_state::draft });
	QApplication::processEvents();
	const auto draftRow = shown_rows(*finder).front();
	finder->popup_row_menu(draftRow, finder->row_rect(draftRow).center());
	QApplication::processEvents();
	expect(finder->move_action() != nullptr && finder->move_action()->text() == QStringLiteral("Keep Local"), "a draft row's menu offers Keep Local");
	finder->move_action()->trigger();
	finder->row_menu()->close();
	wait_for([&] { return window.draft_roster().Entries.empty() && cards->local_comments().size() == 4U; }, "Keep Local from Threads makes the draft Local");
	wait_for([&] { finder->refresh(); return finder->state_count(qt::finder_state::local) == 3U && finder->state_count(qt::finder_state::draft) == 0U; }, "Threads counts it Local now");
	// And Make Draft on it from its Local row: it opens at its line and is a draft again.
	const auto keptLocal = std::ranges::find_if(cards->local_comments(), [](const qt::local_comment_card& aCard) { return aCard.Body.startsWith(QStringLiteral("DRAFT: not in")); });
	expect(keptLocal != cards->local_comments().end(), "the kept draft is a Local comment with its words");
	const auto keptId = keptLocal->Id;
	finder->set_filters({ .State = qt::finder_state::local });
	QApplication::processEvents();
	finder->popup_row_menu(local_row(keptId), finder->row_rect(local_row(keptId)).center());
	QApplication::processEvents();
	expect(finder->move_action() != nullptr && finder->move_action()->text() == QStringLiteral("Make Draft"), "a Local row's menu offers Make Draft");
	finder->move_action()->trigger();
	finder->row_menu()->close();
	wait_for([&] { return window.draft_roster().NotInReview == 1U && window.draft_roster().Entries.size() == 1U && window.draft_roster().Entries.front().Line == draftLine - 1U && cards->local_comments().size() == 3U; },
			 "Make Draft from Threads returns it to a draft at its line, not in the review");
	expect(window.current_modality() == qt::review_modality::review, "Make Draft from Threads goes to the comment in Review");

	// AC08: Overview's Local chip, its folder's sum, its activation; no Note
	// column; My review by state.
	window.open_overview_tab(qt::overview_drawer_tab::status);
	auto* const overview = window.overview_page();
	wait_for([&] { return overview->isVisible(); }, "Overview shows");
	auto* const view  = overview->files_view();
	auto* const model = overview->file_model();
	const auto  cell  = model->index_for_path(path).siblingAtColumn(qt::overview_file_model::comments_column);
	expect(cell.isValid(), "the thread's file has a row");
	const auto chips  = [&](const QModelIndex& aCell) { return qt::overview_comment_chips(model->feedback_at(aCell), view->visualRect(aCell), view->font(), model->drafts_at(aCell)); };
	const auto chipOf = [&](const QModelIndex& aCell, const qt::overview_comment_chip::kind aKind) {
		const auto shown = chips(aCell);
		const auto found = std::ranges::find(shown, aKind, &qt::overview_comment_chip::Kind);
		return found == shown.end() ? std::optional<qt::overview_comment_chip>{ } : std::optional{ *found };
	};
	wait_for([&] { const auto chip = chipOf(cell, qt::overview_comment_chip::kind::local); return chip && chip->Count == 2U; }, "AC08: the file's Comments cell shows a Local chip with its count");
	const auto drafted = chipOf(cell, qt::overview_comment_chip::kind::drafts);
	const auto local   = chipOf(cell, qt::overview_comment_chip::kind::local);
	expect(drafted && drafted->Count == 1U && local->Box.right() <= drafted->Box.left(), "AC08: beside the drafts chip");
	if (const auto folder = cell.parent(); folder.isValid()) {
		const auto summed = chipOf(folder.siblingAtColumn(qt::overview_file_model::comments_column), qt::overview_comment_chip::kind::local);
		expect(summed && summed->Count >= 2U, "AC08: a folder shows its files' sum");
	}
	for (auto column = 0; column < model->columnCount(); ++column) {
		expect(model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString() != QStringLiteral("Note"), "AC08: Overview has no Note column");
	}
	{
		// The Agent tab's own "Private notes" opt-in is milestone 4's wording.
		auto* const myReview = overview->status_section_header(qt::overview_status_section::my_review)->parentWidget();
		const auto  labels   = myReview->findChildren<QLabel*>();
		expect(std::ranges::none_of(labels, [](const QLabel* aLabel) { return aLabel->text() == QStringLiteral("Private notes"); })
				   && std::ranges::any_of(labels, [](const QLabel* aLabel) { return aLabel->text() == QStringLiteral("Local comments"); }),
			   "AC08: My review has no Private notes row; it counts Local comments");
	}
	wait_for([&] {
		return overview->value_label(qt::overview_value::local_comments)->text() == QStringLiteral("3") && overview->value_label(qt::overview_value::not_in_review)->text() == QStringLiteral("1");
	},
			 "My review counts my comments by state");
	std::cout << "LOCAL_FOUND_OVERVIEW " << qt::overview_local_text(*local).toStdString() << '\n';
	view->scrollTo(cell);
	QApplication::processEvents();
	const auto box = chipOf(cell, qt::overview_comment_chip::kind::local)->Box;
	click_at(*view->viewport(), box.center());
	wait_for([&] { return window.current_modality() == qt::review_modality::threads && finder->filters().State == qt::finder_state::local; }, "AC08: the Local chip opens Threads on Local");
	const auto first = std::ranges::min_element(finder->local_comments(), [&](const qt::finder_local_comment& aLeft, const qt::finder_local_comment& aRight) { return aLeft.Line < aRight.Line; });
	expect(finder->cursor_key() == local_row(first->Id), "AC08: at that file's first");

	// AC09: Finish your review lists no Local comment as pending or not in
	// the review; Keep Local on a not-in-review row removes it; the Local
	// line counts them and Show lists them.
	window.open_review_composition();
	auto* const editor = window.review_submission_editor_view();
	wait_for([&] { return editor->isVisible() && editor->outside_rows().size() == 1 && editor->local_line()->isVisible(); }, "Finish your review lists the draft outside the review and the Local line");
	expect(std::ranges::none_of(editor->outside_rows(), [](const QString& aRow) { return aRow.contains(QStringLiteral("LOCAL:")); }) && std::ranges::none_of(editor->pending_rows(), [](const QString& aRow) { return aRow.contains(QStringLiteral("LOCAL:")); }),
		   "AC09: no Local comment is pending or not in the review");
	expect(editor->local_line_label()->text() == QStringLiteral("3 local comments stay on this computer"), "AC09: the Local line shows the count");
	auto* const row = editor->outside_list()->findChildren<QAbstractButton*>(QStringLiteral("wholeReviewOutsideOpen")).front();
	{
		auto menuEvent = QContextMenuEvent{ QContextMenuEvent::Mouse, row->rect().center(), row->mapToGlobal(row->rect().center()) };
		QApplication::sendEvent(row, &menuEvent);
		auto* const menu = row->findChild<QMenu*>(QStringLiteral("wholeReviewPendingMenu"));
		auto* const keep = menu != nullptr ? menu->findChild<QAction*>(QStringLiteral("wholeReviewOutsideKeepLocal")) : nullptr;
		expect(keep != nullptr && keep->isEnabled(), "AC09: a not-in-review row offers Keep Local");
		keep->trigger();
		menu->close();
	}
	wait_for([&] { return editor->outside_rows().empty() && editor->local_line_label()->text() == QStringLiteral("4 local comments stay on this computer"); },
			 "AC09: Keep Local removes the row and the Local line counts it");
	expect(window.draft_roster().Entries.empty(), "AC09: no draft stands");
	editor->local_show_button()->click();
	wait_for([&] { return window.current_modality() == qt::review_modality::threads && finder->filters().State == qt::finder_state::local && shown_rows(*finder).size() == 3U; }, "AC09: Show lists them in Threads");
	{
		const std::scoped_lock lock{ server->Mutex };
		expect(server->StagedComments.empty() && server->ReviewPosts.empty() && server->Reviews.empty(), "nothing was sent");
	}
	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "shutdown completes");
	application.reset();
	std::cout << "LOCAL_COMMENTS_FOUND_OK\n";
	return EXIT_SUCCESS;
}
