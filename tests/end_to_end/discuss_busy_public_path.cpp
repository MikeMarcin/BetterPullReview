// [[BPR-2171]]. Discuss on a busy pull request, through the composed window:
// conversations-v1 with reviews whose summaries hold Markdown, threads
// bundled under them, and bot reports in a row. Checks the column, each file
// named once with its resolved threads folded, the review's verdict on its
// own comment, Fold bots, Mark as bot and Only this file, and renders the
// window at three sizes into BPR_DISCUSS_RENDER_ROOT. With BPR_NATIVE_LOOK=1
// (and BPR_NATIVE_LOOK_SCHEME) the renders are native, as the owner sees them.
#include "../../src/app/src/discuss_busy_fixture.hpp"
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/workbench_interaction.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QMenu>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

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
		expect(aCondition(), aMessage);
	}

	void settle()
	{
		for (auto round = 0; round < 20; ++round) {
			QApplication::processEvents();
			QThread::msleep(5);
		}
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication  qtApplication{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-discuss-busy-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private composed course root");
	auto server = std::make_shared<app::detail::review_submission_fixture_state>();
	app::detail::serve_busy_discuss(*server);
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = dataRoot;
	options.ReviewSubmissionServer = server;
	options.RepeatReadSessions     = true;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::conversations_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composition starts");
	expect(application != nullptr, "startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 800);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/conversations#418"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.overview_page()->projection().has_value(); }, "the busy pull request opens");
	// The fixture's read state, as its attended look records it: most posts
	// read, a few new.
	wait_for([&] { return access::reveal_fixture_file(*application, "src/adapters/qt/src/conversation_cards_view.cpp") == app::detail::fixture_file_reveal_result::shown; }, "a file is shown in Review");
	const auto marked = access::apply_fixture_read_state(*application, "conversations-v1");
	expect(marked.has_value() && *marked > 0U, "the fixture's read state is recorded through the ledger");
	settle();
	window.choose_modality(qt::review_modality::discuss);
	auto* const panel = window.conversation_panel();
	auto* const cards = panel->cards();
	wait_for([&] { return cards->feed() && cards->cards().size() > 10U && !cards->timeline_lines().empty(); }, "Discuss lists the conversation in time order");
	settle();

	const auto& lines = cards->timeline_lines();
	// The column: centred, the Reading width's default wide ([[BPR-2299]];
	// it had been 820 of its own), the band's chips on its edges.
	const auto [left, width] = cards->column();
	const auto readingWidth  = static_cast<int>(*core::reading_width_limit(core::default_reading_width));
	std::cout << "DISCUSS_COLUMN " << left << "+" << width << " of " << cards->viewport()->width() << '\n';
	expect(width == readingWidth && left > 8, "Discuss's list is a centred column the Reading width wide by default");
	// Each file named once over the threads that follow one another on it.
	auto files = 0;
	auto folds = 0;
	for (std::size_t at = 1U; at < lines.size(); ++at) {
		const auto& line = lines[at];
		if (line.Kind == qt::timeline_line::kind::path) {
			++files;
			folds += line.Folded > 0U ? 1 : 0;
			expect(!line.Path.contains(QLatin1Char{ ':' }), "a file row names the file, not a line");
		}
	}
	expect(files > 0 && folds > 0, "files are named over their threads, and resolved threads fold into their rows");
	const auto threads    = cards->thread_count();
	const auto unresolved = cards->unresolved_count();
	// The review's own comment says who, the verdict and when.
	const auto summary = std::ranges::find(cards->cards(), std::string{ "review:busy-review-jiayin" }, &qt::thread_card::Id);
	expect(summary != cards->cards().end() && summary->HeadVerdict == QStringLiteral("Changes requested") && !summary->When.isEmpty(), "Changes requested and the date are on jiayin's summary");
	const auto merged = std::ranges::find_if(lines, [](const qt::timeline_line& aLine) { return aLine.Id == "review:busy-review-jiayin"; });
	expect(merged != lines.end() && merged->Merged, "its own line is not drawn");
	const auto lone = std::ranges::find_if(cards->cards(), [](const qt::thread_card& aCard) { return aCard.Kind == QStringLiteral("PR comment"); });
	expect(lone != cards->cards().end() && !lone->When.isEmpty(), "a comment outside a review says when");
	// Only this file: one toggle; the Pull request half is never shown.
	auto* const onlyFile = panel->only_this_file_button();
	expect(onlyFile != nullptr && onlyFile->text() == QStringLiteral("Only this file") && onlyFile->isVisible() && !onlyFile->isChecked(), "Only this file is one toggle, off on the whole pull request");
	expect(panel->findChild<QToolButton*>(QStringLiteral("conversationScopeWholePullRequest"))->isHidden(), "the Pull request | File choice is gone");

	const auto root   = qEnvironmentVariable("BPR_DISCUSS_RENDER_ROOT");
	const auto render = [&](const QString& aName, const int aWidth, const int aHeight) {
		window.resize(aWidth, aHeight);
		settle();
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "render directory");
			expect(window.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "rendered");
		}
	};
	render(QStringLiteral("discuss-1280x800"), 1280, 800);
	render(QStringLiteral("discuss-1728x1117"), 1728, 1117);
	render(QStringLiteral("discuss-2560x1440"), 2560, 1440);
	expect(cards->column().second == readingWidth, "the column stays the Reading width on a wide window");
	// The reviews, their previews and the bot reports, further down.
	const auto scrollTo = [&](const std::string& aId) {
		const auto found = std::ranges::find(cards->cards(), aId, &qt::thread_card::Id);
		expect(found != cards->cards().end(), "the card to scroll to is listed");
		cards->verticalScrollBar()->setValue(std::max(0, found->Rect.top() - 140));
	};
	for (const auto& [w, h] : { std::pair{ 1280, 800 }, std::pair{ 1728, 1117 }, std::pair{ 2560, 1440 } }) {
		window.resize(w, h);
		settle();
		scrollTo("review:busy-review-jiayin");
		render(QStringLiteral("discuss-reviews-%1x%2").arg(w).arg(h), w, h);
	}
	cards->verticalScrollBar()->setValue(0);

	// Fold bots: off by default, beside Hide resolved; on, each run of bot
	// comments is one row, and the counts do not change.
	auto* const bots = panel->fold_bots_button();
	expect(bots != nullptr && bots->isVisible() && !bots->isChecked() && bots->text() == QStringLiteral("Fold bots"), "Fold bots is a quick filter, off");
	expect(std::ranges::none_of(lines, [](const qt::timeline_line& aLine) { return aLine.Kind == qt::timeline_line::kind::bots; }), "nothing folds as a bot until asked");
	bots->click();
	settle();
	const auto botRows = std::ranges::count_if(lines, [](const qt::timeline_line& aLine) { return aLine.Kind == qt::timeline_line::kind::bots; });
	expect(botRows == 2, "each run of bot reports is one row");
	expect(cards->thread_count() == threads && cards->unresolved_count() == unresolved, "folding changes no count");
	window.resize(1280, 800);
	settle();
	scrollTo("review:busy-review-jiayin");
	render(QStringLiteral("discuss-fold-bots-1280x800"), 1280, 800);

	// Mark as bot: renovate-helper is a person to GitHub; marked, it folds.
	const auto renovate = std::ranges::find_if(cards->cards(), [](const qt::thread_card& aCard) { return !aCard.Posts.empty() && aCard.Posts.front().Author == QStringLiteral("renovate-helper"); });
	expect(renovate != cards->cards().end(), "renovate-helper's comment is listed");
	auto* const menu = cards->thread_menu(renovate->Id);
	expect(menu != nullptr, "its menu opens");
	auto* const mark = menu->findChild<QAction*>(QStringLiteral("threadAuthorBotAction"));
	expect(mark != nullptr && mark->text() == QStringLiteral("Mark renovate-helper as a Bot") && mark->isEnabled(), "the menu offers Mark as Bot");
	mark->trigger();
	menu->close();
	settle();
	expect(std::ranges::count_if(lines, [](const qt::timeline_line& aLine) { return aLine.Kind == qt::timeline_line::kind::bots; }) == 3, "marked, its comment folds as a bot's");
	bots->click();
	settle();

	// An open card with code grows wider than the column.
	window.resize(1728, 1117);
	settle();
	cards->open_thread("review:busy-review-jiayin");
	wait_for(
		[&] {
			const auto found = std::ranges::find(cards->cards(), std::string{ "review:busy-review-jiayin" }, &qt::thread_card::Id);
			return found != cards->cards().end() && found->Rect.width() > cards->column().second;
		},
		"an open card holding code grows wider than the column");
	const auto wide = std::ranges::find(cards->cards(), std::string{ "review:busy-review-jiayin" }, &qt::thread_card::Id);
	expect(wide != cards->cards().end(), "the open card is listed");
	cards->verticalScrollBar()->setValue(std::max(0, wide->Rect.top() - 120));
	render(QStringLiteral("discuss-open-code-1728x1117"), 1728, 1117);

	// Owner look discuss-busy-pr-v001, ac5-only-this-file: "\"Only this
	// file\" breaks the column formatting." With Only this file on, the list
	// keeps the centred column, the band keeps its edges, and the band's
	// first control does not move, at each of the three window sizes.
	cards->close_thread();
	for (const auto& [w, h] : { std::pair{ 1280, 800 }, std::pair{ 1728, 1117 }, std::pair{ 2560, 1440 } }) {
		window.resize(w, h);
		settle();
		if (onlyFile->isChecked()) {
			onlyFile->click();
			settle();
		}
		const auto wholeColumn = cards->column();
		const auto wholeAt     = onlyFile->mapTo(&window, QPoint{ 0, 0 }).x();
		onlyFile->click();
		settle();
		wait_for([&] { return onlyFile->isChecked() && cards->feed(); }, "Only this file is on");
		const auto fileColumn = cards->column();
		const auto fileAt     = onlyFile->mapTo(&window, QPoint{ 0, 0 }).x();
		const auto edge       = cards->viewport()->mapTo(&window, QPoint{ 0, 0 }).x() + fileColumn.first;
		std::cout << "ONLY_THIS_FILE " << w << "x" << h << " column=" << fileColumn.first << "+" << fileColumn.second << " whole=" << wholeColumn.first << "+" << wholeColumn.second << " toggle=" << fileAt << " was=" << wholeAt << " edge=" << edge << '\n';
		expect(fileColumn == wholeColumn && fileColumn.second == readingWidth, "Only this file keeps the centred column");
		expect(fileAt == wholeAt && std::abs(fileAt - edge) <= 2, "the band keeps the column's edges and its toggle does not move");
		render(QStringLiteral("discuss-only-this-file-%1x%2").arg(w).arg(h), w, h);
		onlyFile->click();
		settle();
	}
	window.resize(1280, 800);
	settle();

	// Owner, ac1-collapsed-preview: "Can switching to discuss from review
	// with a comment open show that comment in disscuss?" A thread open in
	// Review is opened in Discuss, in view, when Discuss is chosen.
	// Owner, v006: "didn't work. seems like going to discuss (cmd+3) is
	// deslecting the comment because its unselected when i go back to review
	// (cmd+2) and discuss never scrolls or selects it." Through the strip's
	// own keys, as the owner pressed them.
	auto* const modality = window.modality();
	const auto  press    = [&](const qt::review_modality aModality) {
		modality->action(aModality)->trigger();
		settle();
	};
	press(qt::review_modality::review);
	std::string carried;
	wait_for([&] {
		const auto& shown = cards->cards();
		const auto  found = std::ranges::find_if(shown, [](const qt::thread_card& aCard) { return aCard.Id.starts_with("review-thread:") && aCard.Line.has_value() && !aCard.Resolved; });
		if (found == shown.end()) {
			return false;
		}
		carried = found->Id;
		cards->open_thread(carried);
		return cards->open_thread_id() == carried;
	},
			 "a thread is open beside the code in Review");
	press(qt::review_modality::discuss);
	wait_for([&] { return cards->feed() && cards->open_thread_id() == carried; }, "Command-3 opens that thread in Discuss");
	{
		const auto found = std::ranges::find(cards->cards(), carried, &qt::thread_card::Id);
		const auto top   = cards->verticalScrollBar()->value();
		expect(found != cards->cards().end() && found->Rect.bottom() > top && found->Rect.top() < top + cards->viewport()->height(), "and it is in view");
		std::cout << "DISCUSS_CARRIED " << carried << '\n';
	}
	render(QStringLiteral("discuss-carried-from-review-1280x800"), 1280, 800);
	press(qt::review_modality::review);
	wait_for([&] { return !cards->feed() && cards->open_thread_id() == carried; }, "Command-2 returns to Review with the thread still open");
	press(qt::review_modality::discuss);
	wait_for([&] { return cards->feed() && cards->open_thread_id() == carried; }, "and Command-3 again opens it in Discuss");

	application->begin_shutdown();
	application.reset();
	std::cout << "DISCUSS_BUSY_PUBLIC_PATH_OK\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
