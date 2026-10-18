// [[BPR-1601]]. Singleton read state through the ordinary composition and gestures.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
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
	server->ReviewHistory          = R"([{"id":4001,"node_id":"read-summary","state":"COMMENTED","body":"Read this review summary","commit_id":"f05373e1184e7067994839ecbe6e2db7a933fa97","user":{"node_id":"U_other","login":"other-reviewer","type":"User"}}])";
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
	mark(summary, true);
	mark(comment, true);
	wait_for([&] { return count() == QStringLiteral("0 new comments in the conversation"); }, "reading clears the Status count");
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
	stop();
	std::cout << "CONVERSATION_READ_PUBLIC_PATH_OK\n";
	return EXIT_SUCCESS;
}
