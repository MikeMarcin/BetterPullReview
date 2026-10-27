// [[BPR-1601]]. Singleton read state through the ordinary composition and gestures.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
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
		while (!aCondition() && elapsed.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-conversation-read-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto server                    = std::make_shared<app::detail::review_submission_fixture_state>();
	server->ReviewHistory          = R"([{"id":4001,"node_id":"read-summary","state":"COMMENTED","body":"Read this review summary","commit_id":"f05373e1184e7067994839ecbe6e2db7a933fa97","user":{"node_id":"U_other","login":"other-reviewer","type":"User"}},{"id":4002,"node_id":"empty-summary","state":"COMMENTED","body":"","commit_id":"f05373e1184e7067994839ecbe6e2db7a933fa97","user":{"node_id":"U_other","login":"other-reviewer","type":"User"}}])";
	server->ConversationComments   = R"([{"id":5001,"node_id":"read-comment","body":"Read this PR comment","user":{"node_id":"U_other","login":"other-reviewer","type":"User"}},{"id":5002,"node_id":"my-comment","body":"My own comment","user":{"node_id":"U_fixture_viewer","login":"fixture-author","type":"User"}}])";
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = dataRoot;
	options.ReviewSubmissionServer = server;
	options.RepeatReadSessions     = true;
	std::unique_ptr<app::github_application> application;
	const auto                               start = [&] {
		bool terminal = false;
		auto startup  = access::start(app::detail::github_public_path_fixture::conversations_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup->is_quiescent(); }, "composition starts");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1280, 900);
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/conversations#418"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.overview_page()->projection().has_value(); }, "ordinary pull request opens");
	};
	const auto stop = [&] {
		application->begin_shutdown();
		application.reset();
	};
	start();
	const auto render = [&](const QString& aName) {
		const auto root = qEnvironmentVariable("BPR_CONVERSATION_RENDER_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "offscreen render directory created");
			QApplication::processEvents();
			expect(application->window().grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "composed surface rendered offscreen");
		}
	};
	const auto summary = std::string{ "review:read-summary" };
	const auto comment = std::string{ "issue-comment:read-comment" };
	const auto count   = [&] {
		return application->window().overview_page()->findChild<QLabel*>(QStringLiteral("overviewNewConversationText"))->text();
	};
	const auto unread = [&](const std::string& aId) {
		auto* finder = application->window().finder();
		finder->refresh();
		const auto& rows  = finder->conversations();
		const auto  found = std::ranges::find(rows, aId, &qt::finder_conversation::Id);
		return found == rows.end() ? std::size_t{ 0U } : found->Unread;
	};
	wait_for([&] { return count() == QStringLiteral("2 new comments in the conversation") && unread(comment) == 1U && unread(summary) == 1U; }, "host count reaches Status and both pathless Threads rows");
	// [[BPR-1779]]. A body-less Commented review is no comment: it adds no
	// row, next new never stops on it, and so it is never unread content.
	// Before, it was a third new comment nothing could open or clear.
	expect(count() == QStringLiteral("2 new comments in the conversation") && unread("review:empty-summary") == 0U, "a body-less review counts as no new comment");
	{
		// [[BPR-1668]]. Real pointer/keyboard activation, the existing ledger
		// effect lane, an ordinary refresh, and a fresh process composition.
		auto* const firstCards = application->window().conversation_panel()->cards();
		application->window().overview_page()->new_conversation_button()->click();
		wait_for([&] { return firstCards->feed(); }, "Overview opens Discuss");
		const auto threads = firstCards->unread_threads_in_file_order();
		expect(threads.size() > 1U, "independent unread inline threads are available");
		const auto& untouched      = threads.back();
		const auto  untouchedCount = unread(untouched);
		const auto  click          = [&](const std::string& aId) {
			auto* const notes = application->window().conversation_panel()->cards();
			auto        note  = notes->note_rect(aId);
			notes->verticalScrollBar()->setValue(notes->verticalScrollBar()->value() + note.top() - 30);
			QApplication::processEvents();
			note = notes->note_rect(aId).intersected(notes->viewport()->rect());
			expect(!note.isEmpty(), "the New-badged card is in the viewport");
			auto event = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ note.center() }, notes->viewport()->mapToGlobal(QPointF{ note.center() }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(notes->viewport(), &event);
		};
		firstCards->set_current_thread(summary);
		QApplication::processEvents();
		expect(unread(summary) == 1U && unread(comment) == 1U, "selection alone marks no card read");
		auto release = access::hold_review_ledger_effects(*application);
		expect(static_cast<bool>(release), "the real ledger effect lane can be held for the durability check");
		click(comment);
		QApplication::processEvents();
		expect(unread(comment) == 1U, "the badge stays new while its durable read write is held");
		auto* const reply = firstCards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyField"));
		expect(reply != nullptr && reply->isEnabled(), "the clicked card immediately offers Reply while its read write is pending");
		reply->click();
		wait_for([&] { return firstCards->reply_editor() != nullptr && firstCards->reply_editor()->isVisible(); }, "Reply opens before the read completes");
		const auto pendingReply = QPointer<QPlainTextEdit>{ firstCards->reply_editor() };
		pendingReply->setPlainText(QStringLiteral("Local reply while the read result is pending."));
		auto pendingCursor = pendingReply->textCursor();
		pendingCursor.setPosition(6);
		pendingReply->setTextCursor(pendingCursor);
		QApplication::processEvents();
		release();
		wait_for([&] { return unread(comment) == 0U && count() == QStringLiteral("1 new comment in the conversation"); }, "clicking the card clears its badge after the durable read result");
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		expect(pendingReply && pendingReply == firstCards->reply_editor() && pendingReply->isVisible()
				   && pendingReply->toPlainText() == QStringLiteral("Local reply while the read result is pending.") && pendingReply->textCursor().position() == 6,
			   "durable read completion keeps the immediate reply editor, local text and caret");
		pendingReply->clear();
		expect(unread(summary) == 1U && unread(untouched) == untouchedCount, "clicking one card leaves the other summary and inline thread new");
		render(QStringLiteral("discuss-card-click-read"));
		const auto before = access::window_read_progress(*application).Snapshot;
		application->window().github_refresh_action()->trigger();
		wait_for([&] { const auto progress=access::window_read_progress(*application);return progress.Snapshot != before && !progress.Reading; }, "ordinary refresh completes");
		expect(unread(summary) == 1U && unread(comment) == 0U && unread(untouched) == untouchedCount, "projection refresh does not read unrelated cards");
		stop();
		start();
		wait_for([&] { return unread(comment) == 0U && unread(summary) == 1U && unread(untouched) == untouchedCount; }, "restart retains only the clicked card's durable read result");
		auto* const restarted = application->window().conversation_panel()->cards();
		application->window().overview_page()->new_conversation_button()->click();
		restarted->set_current_thread(summary);
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(restarted, &enter);
		wait_for([&] { return unread(summary) == 0U; }, "keyboard opening marks the same summary read");
		click(threads.front());
		wait_for([&] { return unread(threads.front()) == 0U; }, "clicking a New-badged inline thread uses the same read path");
		expect(unread(untouched) == untouchedCount, "an offscreen unrelated inline thread remains new");
		std::cout << "DISCUSS_CARD_READ_PUBLIC_PATH_OK\n";
		if (QCoreApplication::arguments().contains(QStringLiteral("--activate-new-card"))) {
			stop();
			return EXIT_SUCCESS;
		}
		for (const auto& id : { comment, summary, threads.front() }) {
			auto* const menu   = restarted->thread_menu(id);
			auto* const action = menu->findChild<QAction*>(QStringLiteral("threadReadAction"));
			expect(action && action->text() == QStringLiteral("Mark unread"), "the same public menu restores a new card for the remaining course");
			action->trigger();
			delete menu;
			wait_for([&] { return unread(id) > 0U; }, "the remaining course starts with the card unread");
		}
		application->window().choose_modality(qt::review_modality::overview);
	}
	auto& window = application->window();
	auto* cards  = window.conversation_panel()->cards();
	window.overview_page()->status_section_header(qt::overview_status_section::my_review)->setChecked(true);
	render(QStringLiteral("status-new-conversation"));
	window.choose_modality(qt::review_modality::threads);
	const auto keys  = window.finder()->shown_keys();
	const auto group = std::ranges::find(keys, "file:");
	expect(group != keys.end(), "Threads has the Not on a file group");
	const auto header = window.finder()->list()->model()->index(static_cast<int>(std::distance(keys.begin(), group)), 0);
	expect(header.data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("2 new")), "Threads' pathless header exposes the same new count");
	window.finder()->list()->scrollTo(header, QAbstractItemView::PositionAtTop);
	render(QStringLiteral("threads-not-on-file"));
	window.choose_modality(qt::review_modality::overview);
	window.overview_page()->new_conversation_button()->click();
	wait_for([&] { return cards->feed() && std::ranges::any_of(cards->cards(), [&](const qt::thread_card& aCard) { return aCard.Id == comment && aCard.RootUnread && aCard.Posts.front().Unread; }); }, "Go to them opens Discuss with the new PR-comment marker");
	render(QStringLiteral("discuss-new-comments"));
	const auto& shown = cards->cards();
	expect(std::ranges::any_of(shown, [&](const qt::thread_card& aCard) { return aCard.Id == summary && aCard.UnreadCount == 1U; }), "Discuss marks the review summary too");
	expect(unread("issue-comment:my-comment") == 0U, "my own PR comment is not new");
	const auto threads = cards->unread_threads_in_file_order();
	expect(!threads.empty(), "fixture has unread file threads before singleton navigation");
	expect(window.next_new_comment_action()->toolTip().contains(QString::number(threads.size()) + QStringLiteral(" threads"))
			   && window.next_new_comment_action()->toolTip().contains(QStringLiteral("2 comments not on a file")),
		   "next-new names the unread threads and standalone comments separately");
	window.conversation_panel()->scope_this_file_button()->click();
	expect(window.conversation_panel()->discuss_view().Scope == qt::cards_scope::this_file, "navigation starts in Discuss's file scope");
	cards->set_current_thread(threads.back());
	window.next_new_comment_action()->trigger();
	expect(cards->current_thread() == summary && cards->open_thread_id() == summary, "next new visits the first Discuss singleton after file threads");
	expect(window.conversation_panel()->discuss_view().Scope == qt::cards_scope::whole_pull_request, "next new widens Discuss for a pathless post");
	window.next_new_comment_action()->trigger();
	expect(cards->current_thread() == comment && cards->open_thread_id() == comment, "next new follows Discuss order to the PR comment");
	const auto mark = [&](const std::string& aId, const bool aRead) {
		application->window().choose_modality(qt::review_modality::discuss);
		auto* menu = application->window().conversation_panel()->cards()->thread_menu(aId);
		expect(menu != nullptr, "ordinary comment context menu opens");
		auto* action = menu->findChild<QAction*>(QStringLiteral("threadReadAction"));
		expect(action != nullptr && action->isEnabled() && action->text() == (aRead ? QStringLiteral("Mark read") : QStringLiteral("Mark unread")), "core read/unread action is offered");
		action->trigger();
		delete menu;
		wait_for([&] { return unread(aId) == (aRead ? 0U : 1U); }, "durable read change reaches Threads");
	};
	wait_for([&] { return unread(summary) == 0U && unread(comment) == 0U; }, "next-new activation marks the visited cards durably read");
	mark(summary, false);
	mark(comment, false);
	mark(summary, true);
	mark(comment, true);
	wait_for([&] { return count() == QStringLiteral("0 new comments in the conversation"); }, "reading clears the Status count");
	expect(!window.next_new_comment_action()->toolTip().contains(QStringLiteral("not on a file"))
			   && window.next_new_comment_action()->toolTip().contains(QStringLiteral("threads")),
		   "reading the standalone comments restores the thread-only navigation count");
	expect(window.overview_page()->findChild<QWidget*>(QStringLiteral("overviewNewConversation"))->isHidden(), "Status hides the empty conversation line");
	expect(std::ranges::none_of(cards->cards(), [&](const qt::thread_card& aCard) { return (aCard.Id == comment || aCard.Id == summary) && aCard.UnreadCount != 0U; }), "reading clears both Discuss markers");
	mark(comment, false);
	wait_for([&] { return count() == QStringLiteral("1 new comment in the conversation"); }, "manual unread restores the Status count");
	stop();
	start();
	wait_for([&] { return count() == QStringLiteral("1 new comment in the conversation") && unread(comment) == 1U && unread(summary) == 0U; }, "restart restores manual unread while the summary stays read");
	mark(comment, true);
	{
		const std::scoped_lock lock{ server->Mutex };
		const auto             at = server->ReviewHistory->find("Read this review summary");
		server->ReviewHistory->replace(at, std::string{ "Read this review summary" }.size(), "Edited review summary");
	}
	application->window().github_refresh_action()->trigger();
	wait_for([&] { return unread(summary) == 1U && count() == QStringLiteral("1 new comment in the conversation"); }, "an edited stable summary becomes new after the ordinary refresh");
	auto& finalWindow = application->window();
	finalWindow.choose_modality(qt::review_modality::threads);
	expect(finalWindow.finder()->reveal(summary, summary), "edited summary is revealed in Threads");
	const auto row = "row:" + summary + "/" + summary;
	finalWindow.finder()->popup_row_menu(row, finalWindow.finder()->row_rect(row).center());
	auto* readAction = finalWindow.finder()->row_menu()->findChild<QAction*>(QStringLiteral("threadFinderReadAction"));
	expect(readAction && readAction->isEnabled() && readAction->text() == QStringLiteral("Mark as Read"), "Threads offers read on the edited summary");
	readAction->trigger();
	finalWindow.finder()->row_menu()->close();
	wait_for([&] { return unread(summary) == 0U && count() == QStringLiteral("0 new comments in the conversation"); }, "Threads' read gesture clears the same durable state and Status count");

	// BPR-1603: the same singleton now opens at Review's top edge. The draft
	// is durable before Send and its send uses the ordinary composed journal.
	finalWindow.choose_modality(qt::review_modality::review);
	if (!finalWindow.conversations_drawer_toggle_action()->isChecked()) {
		finalWindow.conversations_drawer_toggle_action()->trigger();
	}
	auto* replyCards = finalWindow.conversation_panel()->cards();
	wait_for([&] { return replyCards->pull_request_comments_chip()->isVisible(); }, "the pull request conversation link is reachable in the production Review drawer");
	// [[BPR-1781]]. The link opens Overview's Comments tab, where the pull
	// request's conversation now lives; the reply is written there.
	replyCards->pull_request_comments_chip()->click();
	wait_for([&] { return finalWindow.showing_overview() && replyCards->pull_request_comments_shown() && finalWindow.overview_page()->status_drawer()->isAncestorOf(replyCards); },
			 "the link opens the conversation on Overview's Comments tab");
	replyCards->open_thread(comment);
	auto* reply = replyCards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyField"));
	expect(reply != nullptr, "PR comment offers Reply in place");
	reply->click();
	wait_for([&] { return replyCards->reply_editor() != nullptr; }, "reply composer opens");
	replyCards->reply_editor()->setPlainText(QStringLiteral("BPR-1603 composed reply"));
	wait_for([&] { return replyCards->reply_editor() && !replyCards->reply_editor()->isReadOnly() && replyCards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyPublish"))->isEnabled(); }, "written reply can be explicitly sent");
	render(QStringLiteral("drawer-quoted-reply"));
	// Drain a local save by waiting for its saved status, then reopen a new app.
	wait_for([&] {auto* status=replyCards->open_card()->findChild<QLabel*>(QStringLiteral("threadReplyStatus"));return status && status->text().startsWith(QStringLiteral("Saved locally")); }, "reply is saved before restart");
	{
		std::scoped_lock lock{ server->Mutex };
		expect(server->StagedWrites == 0U, "typing never posts a reply");
	}
	stop();
	start();
	auto& replyWindow = application->window();
	replyWindow.choose_modality(qt::review_modality::review);
	if (!replyWindow.conversations_drawer_toggle_action()->isChecked()) {
		replyWindow.conversations_drawer_toggle_action()->trigger();
	}
	replyCards = replyWindow.conversation_panel()->cards();
	wait_for([&] { return replyCards->pull_request_comments_chip()->isVisible(); }, "restarted drawer link appears");
	replyCards->pull_request_comments_chip()->click();
	wait_for([&] { return replyWindow.showing_overview() && replyCards->pull_request_comments_shown(); }, "the restarted link opens Overview's Comments");
	replyCards->open_thread(comment);
	replyCards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyField"))->click();
	wait_for([&] { return replyCards->reply_editor() && replyCards->reply_editor()->toPlainText() == QStringLiteral("BPR-1603 composed reply"); }, "reply text survives a fresh application");
	auto* send = replyCards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReplyPublish"));
	wait_for([&] { return send->isEnabled(); }, "restored draft offers explicit Send");
	send->click();
	wait_for([&] {std::scoped_lock lock{server->Mutex};return server->StagedWrites==1U; }, "Send reaches the existing issue-comment publication path exactly once");
	{
		std::scoped_lock lock{ server->Mutex };
		expect(server->StagedComments.empty(), "a PR reply creates no file review comments");
	}
	// [[BPR-1781]]. A new comment of its own, under the conversation on
	// Overview's Comments: saved as a reply is, posted only by Comment, and
	// sent as its words alone, without a quote.
	auto* const panel = replyWindow.conversation_panel();
	wait_for([&] { return replyWindow.showing_overview() && panel->new_comment()->isVisible() && !panel->new_comment_editor()->isReadOnly(); }, "Overview's Comments offers a new comment");
	panel->new_comment_editor()->setFocus();
	panel->new_comment_editor()->setPlainText(QStringLiteral("BPR-1781 a comment of its own"));
	wait_for([&] { return panel->new_comment_send()->isEnabled() && panel->new_comment_status()->text().startsWith(QStringLiteral("Saved locally. Comment posts it")); }, "the new comment is saved before it can be posted");
	{
		std::scoped_lock lock{ server->Mutex };
		expect(server->StagedWrites == 1U, "typing a new comment posts nothing");
	}
	render(QStringLiteral("overview-new-comment"));
	panel->new_comment_send()->click();
	wait_for([&] {std::scoped_lock lock{server->Mutex};return server->StagedWrites==2U; }, "Comment posts the new comment exactly once");
	{
		std::scoped_lock lock{ server->Mutex };
		expect(!server->CompatibilityComments.empty() && server->CompatibilityComments.back().Body == "BPR-1781 a comment of its own", "the new comment is its words alone, with no quote");
	}
	wait_for([&] { return panel->new_comment_editor()->toPlainText().isEmpty() && !panel->new_comment_send()->isEnabled(); }, "the composer starts again empty once posted");
	stop();
	std::cout << "CONVERSATION_READ_PUBLIC_PATH_OK\n";
	return EXIT_SUCCESS;
}
