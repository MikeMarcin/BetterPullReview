// RFC-BPR-0127 ([[BPR-2157]] AC03, AC13; [[BPR-2158]] AC04, AC05, AC06).
// Local comments through the composed application and the Comments drawer: N
// and Save on a thread's line, a Local reply from the thread's reply field, a
// Local comment on the whole file and one on the pull request; the counters
// they must not move; Keep local on a draft in the review and Make draft back;
// the refusals; and every send afterwards, none of which carries a Local
// comment's words.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/draft_roster.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
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
			std::cerr << "local-comments-composition: waited for: " << aMessage << '\n';
		}
		expect(aCondition(), aMessage);
	}

	/// AC13's counters, as the reviewer reads them: Status's new count, the
	/// Comments tab's name and count, Next new's words, Threads' rows and
	/// label counts, Overview's thread counts per file, and Home's row.
	struct counters
	{
		QString                                                                                    Status;
		QString                                                                                    CommentsTab;
		QString                                                                                    NextNew;
		std::vector<std::pair<std::string, std::size_t>>                                           Threads;
		std::map<core::feedback_label, std::size_t>                                                Labels;
		std::vector<std::tuple<std::string, std::size_t, std::size_t, std::optional<std::size_t>>> Files;
		std::optional<core::queue_item>                                                            Home;

		auto operator==(const counters&) const -> bool = default;
	};

	auto read_counters(qt::workbench_window& aWindow) -> counters
	{
		auto shown = counters{ };
		if (auto* const label = aWindow.overview_page()->findChild<QLabel*>(QStringLiteral("overviewNewConversationText")); label != nullptr) {
			shown.Status = label->text();
		}
		shown.CommentsTab = aWindow.tray_tabs()->tabText(0) + QLatin1Char{ '|' } + aWindow.tray_tabs()->accessibleTabName(0);
		shown.NextNew     = aWindow.next_new_comment_action()->toolTip();
		aWindow.finder()->refresh();
		for (const auto& row : aWindow.finder()->conversations()) {
			shown.Threads.emplace_back(row.Id, row.Unread);
		}
		shown.Labels = aWindow.finder()->label_counts();
		if (const auto& projection = aWindow.overview_page()->projection(); projection.has_value()) {
			for (const auto& file : projection->Files) {
				shown.Files.emplace_back(file.Path, file.ThreadCount, file.UnresolvedThreadCount, file.UnreadCount);
			}
		}
		if (const auto& ref = aWindow.queue_landing()->reviewing(); ref.has_value()) {
			shown.Home = aWindow.queue_landing()->item_for(*ref);
		}
		return shown;
	}

	auto card_of(qt::conversation_cards_view& aCards, const std::function<bool(const qt::thread_card&)>& aWhich) -> std::optional<qt::thread_card>
	{
		const auto& cards = aCards.cards();
		const auto  found = std::ranges::find_if(cards, aWhich);
		return found == cards.end() ? std::nullopt : std::optional{ *found };
	}

	auto open_button(qt::conversation_cards_view& aCards, const char* aName) -> QAbstractButton*
	{
		return aCards.open_card() == nullptr ? nullptr : aCards.open_card()->findChild<QAbstractButton*>(QString::fromLatin1(aName));
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-local-comments-XXXXXX") };
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
	auto* const cards = window.conversation_panel()->cards();

	// A published thread on a file, as Discuss lists it, opened in Review.
	window.overview_page()->new_conversation_button()->click();
	wait_for([&] { return cards->feed() && !cards->unread_threads_in_file_order().empty(); }, "Discuss lists the file threads");
	const auto threadId = cards->unread_threads_in_file_order().front();
	const auto listed   = card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == threadId; });
	expect(listed.has_value() && !listed->Path.isEmpty() && listed->Line.has_value() && listed->Side.has_value(), "the thread has a file, a side and a line");
	const auto path = listed->Path.toStdString();
	const auto side = *listed->Side;
	const auto line = *listed->Line;
	window.choose_modality(qt::review_modality::review);
	wait_for([&] { return access::reveal_fixture_file(*application, path) == app::detail::fixture_file_reveal_result::shown; }, "the thread's file is shown in Review");
	if (window.conversations_drawer()->state() != core::workbench_drawer_state::expanded) {
		window.conversations_drawer_toggle_action()->trigger();
	}
	window.tray_tabs()->setCurrentIndex(0);
	wait_for([&] { return card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == threadId && aCard.Line == std::optional{ line }; }).has_value(); }, "the thread hangs at its line in Comments");
	const auto before    = read_counters(window);
	const auto evidence  = [&] { return access::review_mutation_evidence(*application); };
	const auto mutations = evidence();

	// N and Save: a Local comment on the thread's line.
	wait_for([&] { return window.comparison_surface()->select_conversation_line(side, line - 1U); }, "the thread's line is selected");
	window.local_comment_action()->trigger();
	wait_for([&] { return cards->writing_draft() && window.composing_local() && cards->draft_editor() != nullptr; }, "N opens the composer on Local");
	cards->draft_editor()->setPlainText(QStringLiteral("LOCAL-ONLY: on the line, never sent."));
	wait_for([&] { auto* const save = open_button(*cards, "threadSaveLocal"); return save != nullptr && save->isVisible() && save->isEnabled(); }, "Save is offered for Local");
	open_button(*cards, "threadSaveLocal")->click();
	wait_for([&] { return card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Local && aCard.Line == std::optional{ line } && !aCard.Posts.empty() && aCard.Posts.front().Body.startsWith(QStringLiteral("LOCAL-ONLY: on the line")); }).has_value(); },
			 "the Local comment is a card at the thread's line");

	// A Local reply from the thread's reply field: Local, then Save.
	wait_for([&] { const auto& draft = window.review_editor()->shown_draft(); return !cards->writing_draft() && (!draft || draft->Body.empty()); }, "the composer has closed");
	wait_for([&] {
		if (cards->open_thread_id() != threadId) {
			cards->open_thread(threadId);
		}
		return open_button(*cards, "threadReplyField") != nullptr && open_button(*cards, "threadReplyField")->isVisible();
	},
			 "the thread offers Reply");
	open_button(*cards, "threadReplyField")->click();
	wait_for([&] { return cards->reply_editor() != nullptr && open_button(*cards, "threadReplyStateLocal") != nullptr; }, "the reply field opens with its state control");
	wait_for([&] { const auto& draft = window.review_editor()->shown_draft(); return draft && draft->ReplyToThread && cards->reply_editor() != nullptr && !cards->reply_editor()->isReadOnly(); }, "the reply's draft is ready");
	open_button(*cards, "threadReplyStateLocal")->click();
	cards->reply_editor()->setPlainText(QStringLiteral("LOCAL-ONLY: a reply kept here."));
	wait_for([&] { const auto& draft = window.review_editor()->shown_draft(); return draft && draft->ReplyToThread && draft->Body == "LOCAL-ONLY: a reply kept here."; }, "the reply's words reach its draft");
	wait_for([&] { auto* const save = open_button(*cards, "threadReplySaveLocal"); return save != nullptr && save->isVisible() && save->isEnabled(); }, "Save stands in Send's place");
	open_button(*cards, "threadReplySaveLocal")->click();
	wait_for([&] { const auto thread = card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == threadId; }); return thread && thread->LocalReplies == 1U && !thread->UnsentReply.has_value(); },
			 "the Local reply is inside the thread's card, and no draft reply stands");

	// A Local comment on the whole file and one on the pull request, as a
	// migrated note leaves them.
	expect(access::seed_local_comment(*application, "LOCAL-ONLY: on the whole file.", true), "the whole file's comment is stored");
	wait_for([&] { return cards->local_file_chip()->isVisible(); }, "AC06: the file's chip counts it");
	expect(access::seed_local_comment(*application, "LOCAL-ONLY: on the pull request.", false), "the pull request's comment is stored");
	wait_for([&] { return cards->local_file_chip()->isVisible() && cards->pull_request_comments_chip()->accessibleName().endsWith(QStringLiteral(", 1 local comment")); },
			 "AC06: the file's chip and the pull request link count them");
	expect(!card_of(*cards, [](const qt::thread_card& aCard) { return aCard.Local && aCard.Posts.front().Body.contains(QStringLiteral("pull request")); }).has_value(),
		   "AC06: the pull request's Local comment is not drawn beside the code");
	std::cout << "LOCAL_COMMENTS_CHIPS " << cards->local_file_chip()->accessibleName().toStdString() << " | " << cards->pull_request_comments_chip()->accessibleName().toStdString() << '\n';

	// AC13: creating them moved no counter.
	QApplication::processEvents();
	const auto created = read_counters(window);
	if (!(created == before)) {
		std::cerr << "local-comments-composition: before status=" << before.Status.toStdString() << " tab=" << before.CommentsTab.toStdString() << " | after status=" << created.Status.toStdString() << " tab=" << created.CommentsTab.toStdString() << '\n';
	}
	expect(created == before, "AC13: Status, the Comments tab, Next new, Threads, label counts, Overview's thread counts and Home are unchanged by Local comments");

	// Edited in place, then the line's Local comment deleted once asked.
	const auto lineLocal = card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Local && aCard.Line == std::optional{ line }; });
	expect(lineLocal.has_value() && cards->begin_comment_edit(lineLocal->Id), "the Local comment opens for editing in place");
	wait_for([&] { return cards->comment_editor() != nullptr; }, "its editor is shown");
	cards->comment_editor()->setPlainText(QStringLiteral("LOCAL-ONLY: on the line, edited."));
	QApplication::processEvents();
	cards->comment_edit_save_button()->click();
	wait_for([&] { return !cards->comment_editing().has_value() && card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == lineLocal->Id && aCard.Posts.front().Body == QStringLiteral("LOCAL-ONLY: on the line, edited."); }).has_value(); },
			 "Save stores the edit only on this computer and ends it");
	cards->open_thread(lineLocal->Id);
	wait_for([&] { return open_button(*cards, "threadDeleteLocal") != nullptr; }, "the Local card offers Delete");
	open_button(*cards, "threadDeleteLocal")->click();
	wait_for([&] { return dynamic_cast<qt::destructive_question*>(window.findChild<QDialog*>(QStringLiteral("deleteLocalConfirmation"))) != nullptr; }, "Delete asks first");
	auto* const question = dynamic_cast<qt::destructive_question*>(window.findChild<QDialog*>(QStringLiteral("deleteLocalConfirmation")));
	expect(question->question() == QStringLiteral("Delete this local comment?"), "the question names a local comment");
	question->accept();
	wait_for([&] { return !card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == lineLocal->Id; }).has_value(); }, "once confirmed it is gone");
	expect(read_counters(window) == before, "AC13: deleting a Local comment moved no counter");

	// AC05: a published comment offers no state change.
	cards->open_thread(threadId);
	QApplication::processEvents();
	expect(open_button(*cards, "threadKeepLocal") == nullptr && open_button(*cards, "threadMakeDraft") == nullptr && open_button(*cards, "threadLocalReplyMakeDraft") != nullptr,
		   "AC05: the published thread offers Keep local and Make draft on nothing but my Local reply");

	// AC03: a Send in the thread holding my Local reply sends none of it.
	wait_for([&] {
		if (cards->open_thread_id() != threadId) {
			cards->open_thread(threadId);
		}
		return open_button(*cards, "threadReplyField") != nullptr && open_button(*cards, "threadReplyField")->isVisible();
	},
			 "the thread offers Reply");
	open_button(*cards, "threadReplyField")->click();
	wait_for([&] { const auto& draft = window.review_editor()->shown_draft(); return draft && draft->ReplyToThread && cards->reply_editor() != nullptr && !cards->reply_editor()->isReadOnly(); }, "the reply field opens on Reply");
	const auto replies = evidence().Replies;
	cards->reply_editor()->setPlainText(QStringLiteral("Sent: thanks, will do."));
	wait_for([&] { auto* const send = open_button(*cards, "threadReplyPublish"); return send != nullptr && send->isVisible() && send->isEnabled(); }, "Send is offered");
	open_button(*cards, "threadReplyPublish")->click();
	for (auto spin = 0; spin < 2000 && evidence().Replies == replies; ++spin) {
		QApplication::processEvents();
		QThread::msleep(1);
	}
	{
		// What Send hands to publication is the reply draft written here,
		// its words alone; the Local reply is another record that no send
		// path admits. This fixture does not complete an inline thread
		// reply's publication, so what the provider received is checked
		// at the end, over every send this course made.
		const auto& draft = window.review_editor()->shown_draft();
		const auto  now   = evidence();
		std::cout << "LOCAL_COMMENTS_SEND replies=" << now.Replies - replies << " draft_state=" << (draft ? std::string{ core::name(draft->State) } : std::string{ "none" }) << " body=" << (draft ? draft->Body : std::string{ }) << '\n';
		expect(draft && draft->ReplyToThread && "review-thread:" + *draft->ReplyToThread == threadId && draft->Body == "Sent: thanks, will do." && (now.Replies == replies || now.LastBody.find("LOCAL-ONLY") == std::string::npos),
			   "AC03: Send in the thread holding my Local reply hands only the reply written to publication");
	}
	// The unfinished publication's draft is discarded, so the review
	// section below starts with no draft of mine.
	if (window.review_editor()->discard_action()->isEnabled()) {
		window.review_editor()->discard_action()->trigger();
	}
	wait_for([&] { return window.draft_roster().Entries.empty(); }, "no draft of mine stands");
	wait_for([&] { const auto thread = card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == threadId; }); return thread && thread->LocalReplies == 1U; }, "AC03: the Local reply is still in its thread after the Send");
	window.close_drawer_composer(false);

	// AC04: a draft in the review, kept local, then made a draft again.
	const auto pendingLine = line + 2U;
	wait_for([&] { return window.comparison_surface()->select_conversation_line(side, pendingLine - 1U); }, "another line is selected");
	wait_for([&] { return window.comment_action()->isEnabled(); }, "Comment is offered there");
	window.comment_action()->trigger();
	wait_for([&] { return cards->writing_draft() && !window.composing_local() && cards->draft_editor() != nullptr && !cards->draft_editor()->isReadOnly(); }, "Comment opens the composer on Draft");
	cards->draft_editor()->setPlainText(QStringLiteral("DRAFT: kept local, then a draft again."));
	wait_for([&] { const auto& draft = window.review_editor()->shown_draft(); return draft && draft->Body == "DRAFT: kept local, then a draft again."; }, "the draft has its words");
	wait_for([&] { auto* const start = open_button(*cards, "threadReviewDraft"); return start != nullptr && start->isEnabled(); }, "Start a review is offered");
	open_button(*cards, "threadReviewDraft")->click();
	wait_for([&] { return window.draft_roster().InReview == 1U; }, "the draft is pending in the review");
	const auto inReview = window.draft_roster();
	window.close_drawer_composer(false);
	const auto pendingCard = [&] { return card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Draft && !aCard.Local && aCard.Line == std::optional{ pendingLine }; }); };
	wait_for([&] { return pendingCard().has_value(); }, "the pending draft is a card at its line");
	cards->open_thread(pendingCard()->Id);
	wait_for([&] { return open_button(*cards, "threadKeepLocal") != nullptr || (cards->writing_draft() && open_button(*cards, "threadStateLocal") != nullptr); }, "the pending draft offers Keep local");
	std::cout << "LOCAL_COMMENTS_KEEP_FROM " << (open_button(*cards, "threadKeepLocal") != nullptr ? "card" : "composer") << '\n';
	if (auto* const keep = open_button(*cards, "threadKeepLocal"); keep != nullptr) {
		keep->click();
	} else {
		// In the composer, choosing Local on a saved comment and Save is Keep local.
		open_button(*cards, "threadStateLocal")->click();
		wait_for([&] { auto* const save = open_button(*cards, "threadSaveLocal"); return save != nullptr && save->isVisible() && save->isEnabled(); }, "Save is offered for Local");
		open_button(*cards, "threadSaveLocal")->click();
	}
	wait_for([&] { return window.draft_roster().InReview == inReview.InReview - 1U; }, "AC04: Keep local lowers the review's pending count by one");
	expect(std::ranges::none_of(window.draft_roster().Entries, [](const core::draft_roster_entry& aEntry) { return aEntry.Body == "DRAFT: kept local, then a draft again."; })
			   && core::drafts_outside_review(window.draft_roster()).empty(),
		   "AC04: it is in no draft list, and no \"not in this review\" list holds it");
	wait_for([&] { return card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Local && aCard.Line == std::optional{ pendingLine } && aCard.Posts.front().Body == QStringLiteral("DRAFT: kept local, then a draft again."); }).has_value(); },
			 "AC04: it is a Local comment with its words, at its line");
	const auto kept = card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Local && aCard.Line == std::optional{ pendingLine }; });
	// A second Local comment on the same line, while no draft stands there.
	window.close_drawer_composer(false);
	wait_for([&] { return window.comparison_surface()->select_conversation_line(side, pendingLine - 1U); }, "the line is selected again");
	window.local_comment_action()->trigger();
	wait_for([&] { return cards->writing_draft() && window.composing_local() && cards->draft_editor() != nullptr; }, "N opens the composer on Local");
	cards->draft_editor()->setPlainText(QStringLiteral("LOCAL-ONLY: a second thought."));
	wait_for([&] { auto* const save = open_button(*cards, "threadSaveLocal"); return save != nullptr && save->isVisible() && save->isEnabled(); }, "Save is offered");
	open_button(*cards, "threadSaveLocal")->click();
	wait_for([&] { return std::ranges::count_if(cards->cards(), [&](const qt::thread_card& aCard) { return aCard.Local && aCard.Line == std::optional{ pendingLine }; }) == 2; }, "two Local comments share the line");
	cards->open_thread(kept->Id);
	for (const auto& entry : window.draft_roster().Entries) {
		std::cout << "  roster " << entry.Id.value() << " line=" << entry.Line << " standing=" << static_cast<int>(entry.Standing) << " body=" << entry.Body << '\n';
	}
	wait_for([&] { auto* const make = open_button(*cards, "threadMakeDraft"); return make != nullptr && make->isEnabled() && make->text() == QStringLiteral("Make draft"); }, "Make draft is offered at its own line");
	open_button(*cards, "threadMakeDraft")->click();
	wait_for([&] {
		const auto& roster = window.draft_roster();
		return roster.InReview == 0U && roster.NotInReview == 1U && roster.Entries.size() == 1U && roster.Entries.front().Line == pendingLine - 1U && roster.Entries.front().Side == side
			&& roster.Entries.front().Body == "DRAFT: kept local, then a draft again." && roster.Entries.front().Standing == core::draft_standing::not_in_review;
	},
			 "AC04: Make draft returns a draft of the same kind at the same place, not in the review");
	wait_for([&] { return !card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == kept->Id; }).has_value(); }, "the Local comment became the draft");

	// AC05: Make draft where my draft already stands is refused, with why.
	const auto second = card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Local && aCard.Posts.front().Body == QStringLiteral("LOCAL-ONLY: a second thought."); });
	cards->open_thread(second->Id);
	const auto rosterBefore = window.draft_roster();
	wait_for([&] { return open_button(*cards, "threadMakeDraft") != nullptr; }, "its card is open");
	auto* const refused = open_button(*cards, "threadMakeDraft");
	std::cout << "LOCAL_COMMENTS_MAKE_DRAFT " << refused->isEnabled() << ' ' << refused->toolTip().toStdString() << '\n';
	if (refused->isEnabled()) {
		refused->click();
		wait_for([&] { return window.statusBar()->currentMessage().contains(QStringLiteral("already stands")); }, "AC05: Make draft where my draft stands is refused, and says why");
	} else {
		auto* const why = open_button(*cards, "threadMakeDraft")->parentWidget()->findChild<QLabel*>(QStringLiteral("threadMakeDraftWhy"));
		expect(refused->toolTip().contains(QStringLiteral("your draft on this line already stands")) && why != nullptr && why->isVisible() && why->text() == refused->toolTip(),
			   "AC05: Make draft where my draft stands is refused, and says why in words under it");
	}
	QApplication::processEvents();
	expect(window.draft_roster() == rosterBefore && card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Id == second->Id; }).has_value(), "AC05: nothing changed");

	// AC05: Keep local on a draft a submitted review holds is refused.
	window.close_drawer_composer(false);
	wait_for([&] { return window.comparison_surface()->select_conversation_line(side, pendingLine - 1U); }, "the draft's line is selected");
	wait_for([&] { return window.comment_action()->isEnabled(); }, "Comment is offered on the draft's line");
	window.comment_action()->trigger();
	wait_for([&] { auto* const start = open_button(*cards, "threadReviewDraft"); const auto& draft = window.review_editor()->shown_draft(); return cards->writing_draft() && !window.composing_local() && start != nullptr && start->isEnabled() && draft && draft->Body == "DRAFT: kept local, then a draft again."; }, "Comment opens my draft in the composer, on Draft");
	open_button(*cards, "threadReviewDraft")->click();
	wait_for([&] { return window.draft_roster().InReview == 1U; }, "the draft is in the review again");
	window.close_drawer_composer(false);
	// Settings › Review › Finishing a review: every verdict stays on the
	// pull request, so the course goes on where it is.
	for (const auto verdict : { core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes }) {
		auto* const box = window.finish_review_switch(verdict);
		expect(box != nullptr, "Settings has a finish box for each verdict");
		if (box->isChecked()) {
			box->click();
		}
	}
	wait_for([&] { const auto& finish = window.behaviour().FinishReview; return finish.Comment == core::review_finish_return::pull_request && finish.Approve == core::review_finish_return::pull_request && finish.RequestChanges == core::review_finish_return::pull_request; },
			 "finishing a review stays on the pull request");
	access::hold_review_submission_terminals(*application, true);
	window.open_review_composition();
	auto* const editor = window.review_submission_editor_view();
	wait_for([&] { return !editor->shown_projection().Controls.CommentsUnknown && editor->submit_action()->isEnabled(); }, "the review can be submitted");
	editor->submit_action()->trigger();
	wait_for([&] { return window.draft_roster().Held == 1U; }, "the submitted review holds the draft");
	window.close_review_sheet();
	const auto held = card_of(*cards, [&](const qt::thread_card& aCard) { return aCard.Draft && !aCard.Local && aCard.Line == std::optional{ pendingLine }; });
	expect(held.has_value(), "the held draft is a card at its line");
	cards->open_thread(held->Id);
	QApplication::processEvents();
	// The held draft opens in the composer, where Local is not offered: the
	// segment is disabled and says why, and pressing it changes nothing.
	wait_for([&] { return cards->writing_draft() && open_button(*cards, "threadStateLocal") != nullptr; }, "the held draft is open in the composer");
	wait_for([&] { auto* const local = open_button(*cards, "threadStateLocal"); return local != nullptr && !local->isEnabled() && local->toolTip().contains(QStringLiteral("holds this draft")); },
			 "AC05: Local is disabled on a draft the submitted review holds, and says why");
	std::cout << "LOCAL_COMMENTS_HELD_REFUSAL " << open_button(*cards, "threadStateLocal")->toolTip().toStdString() << '\n';
	open_button(*cards, "threadStateLocal")->click();
	QApplication::processEvents();
	expect(!window.composing_local() && (open_button(*cards, "threadSaveLocal") == nullptr || !open_button(*cards, "threadSaveLocal")->isVisible()), "AC05: pressing the disabled Local chooses nothing");
	QApplication::processEvents();
	expect(window.draft_roster().Held == 1U, "AC05: the held draft is unchanged");
	access::hold_review_submission_terminals(*application, false);
	wait_for([&] { return window.draft_roster().Held == 0U && window.draft_roster().InReview == 0U; }, "the review's answer releases it");

	// AC03: after the Send in the thread and the submitted review, nothing
	// the server received carries a Local comment's words.
	window.close_drawer_composer(false);
	{
		const std::scoped_lock lock{ server->Mutex };
		const auto             local  = [](const std::string& aBody) { return aBody.find("LOCAL-ONLY") != std::string::npos; };
		auto                   sent   = std::size_t{ 0U };
		auto                   bodies = std::size_t{ 0U };
		for (const auto* const list : { &server->StagedComments, &server->CompatibilityComments, &server->FileComments, &server->PreviousComments }) {
			for (const auto& comment : *list) {
				++bodies;
				sent += local(comment.Body) ? 1U : 0U;
			}
		}
		for (const auto& post : server->ReviewPosts) {
			++bodies;
			sent += local(post) ? 1U : 0U;
		}
		for (const auto& review : server->Reviews) {
			++bodies;
			sent += local(review.Proposal.Body) ? 1U : 0U;
		}
		sent += server->StagedDescription && local(*server->StagedDescription) ? 1U : 0U;
		std::cout << "LOCAL_COMMENTS_SENT_BODIES " << bodies << " carrying_local=" << sent << '\n';
		expect(bodies > 0U && sent == 0U, "AC03: no review, comment, reply or post the server received carries a Local comment's words");
	}
	{
		const std::scoped_lock lock{ server->Mutex };
		const auto             carried = std::ranges::any_of(server->ReviewPosts, [](const std::string& aPost) { return aPost.find("DRAFT: kept local, then a draft again.") != std::string::npos; })
			|| std::ranges::any_of(server->StagedComments, [](const application::staged_review_comment& aComment) { return aComment.Body == "DRAFT: kept local, then a draft again."; });
		std::cout << "LOCAL_COMMENTS_REVIEW_CARRIED_DRAFT " << carried << '\n';
	}
	static_cast<void>(mutations);
	const auto& locals = cards->local_comments();
	const auto  still  = [&locals](const char* aBody) { return std::ranges::any_of(locals, [aBody](const qt::local_comment_card& aCard) { return aCard.Body == QString::fromUtf8(aBody); }); };
	expect(still("LOCAL-ONLY: a reply kept here.") && still("LOCAL-ONLY: on the whole file.") && still("LOCAL-ONLY: on the pull request.") && still("LOCAL-ONLY: a second thought."),
		   "AC03: the Local reply, the file's, the pull request's and the line's Local comments are all still Local after every send");
	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "shutdown completes");
	application.reset();
	std::cout << "LOCAL_COMMENTS_COMPOSITION_OK\n";
	return EXIT_SUCCESS;
}
