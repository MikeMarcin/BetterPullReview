// [[BPR-1093]]. A labelled comment's publication through the real composition
// (RFC-BPR-0048 R42; the pack's native-integration case `indeterminate-retry`):
// the recorded GitHub.com fixture, a native session signed in with its recorded
// credential, the durable private comment draft, the publication host, the
// GitHub adapter's review mutation session and its SQLite recovery journal.
// Nothing here is a fake of the product; the fixture server stores what it is
// sent and can lose the answer to the first write.
//
// Three courses:
//
// - lost-relabel-retried: the fixture stores the labelled comment and loses its
//   answer. While the outcome is unknown, the label chip and the keyboard both
//   try to change the label; neither reaches the draft or the store. Retry
//   reconciles the one stored comment: one write at the server in all, with
//   the bytes that were published first, and the relabelled body never sent.
// - lost-refreshed-retried ([[BPR-1477]]): the answer is lost, the window
//   reads the pull request again by itself, and Refresh reads it once more;
//   the draft is held for the same retry after each, and Retry reconciles.
// - relabel-before-publish: Publish pressed (in the editor and in the drawer)
//   while a label edit waits for its owner sends nothing and says so; once the
//   edit is accepted a new Publish sends exactly the relabelled body, once.
//
// Since [[BPR-1345]] the drawer's Publish is Add single comment, and in the
// drawer's default width it is the split arrow's menu item; the courses send it
// from wherever it is offered.
//
// No live GitHub, credential or model.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "drawer_single_comment.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	using bpr::tests::expect;
	using e2e::draft_publish;
	using e2e::send_single;
	using e2e::single_offered;

	auto wait_until(const std::function<bool()>& aCondition, const qint64 aTimeoutMs = 20000) -> bool
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		return aCondition();
	}
	void wait_for(const std::function<bool()>& aCondition, const std::string_view aMessage, const qint64 aTimeoutMs = 20000)
	{
		expect(wait_until(aCondition, aTimeoutMs), aMessage);
	}
	/// Lets queued work run without waiting for anything in particular, so a
	/// refused change has every chance to arrive somewhere if it could.
	void settle()
	{
		for (auto turn = 0; turn < 200; ++turn) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	auto draft_card(const qt::conversation_cards_view& aCards) -> const qt::thread_card*
	{
		const auto& cards = aCards.cards();
		const auto  found = std::ranges::find_if(cards, [](const qt::thread_card& aCard) { return aCard.Draft; });
		return found == cards.end() ? nullptr : &*found;
	}
	auto posted_count(const qt::conversation_cards_view& aCards, const QString& aBody) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(aCards.cards(), [&aBody](const qt::thread_card& aCard) {
			return !aCard.Draft && !aCard.Posts.empty() && aCard.Posts.front().Body == aBody;
		}));
	}
	auto draft_status(const qt::conversation_cards_view& aCards) -> QString
	{
		auto* const open = aCards.open_card();
		if (open == nullptr || !open->isVisible()) {
			return { };
		}
		auto* const label = open->findChild<QLabel*>(QStringLiteral("threadDraftStatus"));
		return label != nullptr && label->isVisibleTo(open) ? label->text() : QString{ };
	}
	/// The draft the editor holds as accepted by its owner, which is the
	/// durable record once private review has answered.
	auto accepted_body(const qt::review_draft_editor& aEditor) -> std::string
	{
		const auto& shown = aEditor.shown_draft();
		return shown.has_value() ? shown->Body : std::string{ "<none>" };
	}
	auto accepted_state(const qt::review_draft_editor& aEditor) -> std::optional<core::review_draft_state>
	{
		const auto& shown = aEditor.shown_draft();
		return shown.has_value() ? std::optional{ shown->State } : std::nullopt;
	}

	struct composed
	{
		QTemporaryDir                            Temporary;
		std::unique_ptr<app::github_application> Application;
	};

	/// The recorded GitHub.com pull request, signed in natively, with review.cpp
	/// on screen and a new comment open in the drawer holding \p aBody.
	void open_draft(composed& aComposed, const bool aLoseFirst, const std::string& aBody)
	{
		expect(aComposed.Temporary.isValid(), "private temporary application root");
		auto options                          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot           = e2e::private_data_root(aComposed.Temporary);
		options.IndependentWindowReads        = true;
		options.LoseFirstReviewMutationResult = aLoseFirst;
		expect(options.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");
		auto terminal = false;
		auto startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				aComposed.Application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(aComposed.Application != nullptr, "application startup succeeds");
		auto& window = aComposed.Application->window();
		window.resize(1440, 900);
		window.show();
		QApplication::processEvents();
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "the recorded account signs in natively");
		if (!window.showing_home()) {
			window.home_action()->trigger();
		}
		window.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/17"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "the pull request opens through the native session");
		wait_for([&] { return github_application_fixture_access::reveal_fixture_file(*aComposed.Application, "review.cpp") == app::detail::fixture_file_reveal_result::shown; },
				 "review.cpp is shown in Review");
		wait_for([&] {
			const auto seeded = github_application_fixture_access::seed_attended_comment_draft(*aComposed.Application, core::diff_side::right, aBody);
			expect(seeded != app::detail::fixture_file_reveal_result::failed, "a new comment can be written on a head line");
			return seeded == app::detail::fixture_file_reveal_result::shown;
		},
				 "a new inline draft holds its text");
		auto* const editor = window.review_editor();
		wait_for([&] { return accepted_body(*editor) == aBody; }, "the draft's owner accepted the text");
		auto* const cards = window.conversation_panel()->cards();
		wait_for([&] { return cards->writing_draft() && single_offered(*cards); }, "the new comment is written in the drawer");
	}

	/// The label chip's own gesture: one header edit of the text on screen.
	auto choose_label(qt::review_draft_editor& aEditor, const core::feedback_label aLabel, const std::optional<core::feedback_expectation> aExpectation = std::nullopt) -> bool
	{
		auto edit        = core::feedback_header_edit{ };
		edit.Change      = core::feedback_label_change::set;
		edit.Label       = aLabel;
		edit.Expectation = aExpectation;
		return aEditor.label_controls()->apply(edit);
	}

	void shut_down(composed& aComposed)
	{
		aComposed.Application->begin_shutdown();
		wait_for([&] { return aComposed.Application->status().ShutdownComplete; }, "all composed hosts quiesce");
	}

	void run_lost_relabel_retried()
	{
		auto       composed = ::composed{ };
		const auto subject  = std::string{ "BPR-1093 the exhausted retry budget is not reported." };
		const auto issue    = "issue (blocking): " + subject;
		const auto question = "question: " + subject;
		open_draft(composed, true, subject);
		auto& application = *composed.Application;
		auto& window      = application.window();
		auto* editor      = window.review_editor();
		auto* cards       = window.conversation_panel()->cards();

		// Labelled through the chip before Publish: composing, labelling and
		// the preview are local and ask the provider for nothing.
		expect(choose_label(*editor, core::feedback_label::issue, core::feedback_expectation::blocking), "the chip labels the draft issue (blocking)");
		wait_for([&] { return accepted_body(*editor) == issue; }, "the labelled draft is accepted by its owner");
		const auto before = github_application_fixture_access::review_mutation_evidence(application);
		expect(before.Submissions == 0U && before.TargetObservations == 0U && before.NativePosts == 0U && before.StoredComments == 0U,
			   "writing and labelling the comment asked GitHub for nothing");

		auto* publish = draft_publish(*cards);
		wait_for([&] { publish = draft_publish(*cards); return publish != nullptr && single_offered(*cards) && publish->isEnabled(); },
				 "the drawer offers Add single comment for the labelled draft");
		send_single(*cards);
		wait_for([&] { return github_application_fixture_access::review_mutation_evidence(application).LostResult; }, "the fixture stores the comment and loses its answer");
		wait_for([&] { return draft_card(*cards) != nullptr && draft_status(*cards).contains(QStringLiteral("retry is safe")); },
				 "the draft stays and says the result is unknown and a retry is safe");
		const auto lost = github_application_fixture_access::review_mutation_evidence(application);
		std::cout << "BPR_1093:lost-relabel-retried:after-loss native_posts=" << lost.NativePosts << " stored=" << lost.StoredComments << " body=\"" << lost.LastBody
				  << "\" state=" << (accepted_state(*editor) ? static_cast<int>(*accepted_state(*editor)) : -1) << '\n'
				  << std::flush;
		expect(lost.NativePosts == 1U && lost.StoredComments == 1U && lost.LastBody == issue, "one write reached the server, with the labelled bytes");
		expect(accepted_state(*editor) == core::review_draft_state::publishing && accepted_body(*editor) == issue, "the draft is held publishing, with the exact body it was prepared with");

		// While the outcome is unknown, the label cannot change: not by the chip,
		// not by typing, and not by a shortcode typed at the start of the body.
		const auto chip = choose_label(*editor, core::feedback_label::question);
		editor->body_editor()->setFocus();
		auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier, QStringLiteral("q") };
		QApplication::sendEvent(editor->body_editor(), &key);
		settle();
		std::cout << "BPR_1093:lost-relabel-retried:relabel-attempt chip=" << chip << " read_only=" << editor->body_editor()->isReadOnly() << " shown=\""
				  << editor->body_editor()->toPlainText().toStdString() << "\" accepted=\"" << accepted_body(*editor) << "\"" << '\n'
				  << std::flush;
		expect(!chip, "the label chip refuses to relabel a draft whose publication is unresolved");
		expect(editor->body_editor()->isReadOnly() && editor->body_editor()->toPlainText().toStdString() == issue, "the body cannot be typed into while the outcome is unknown");
		expect(accepted_body(*editor) == issue && accepted_state(*editor) == core::review_draft_state::publishing, "the draft and its store keep the prepared body");
		const auto attempted = github_application_fixture_access::review_mutation_evidence(application);
		expect(attempted.NativePosts == lost.NativePosts && attempted.Submissions == lost.Submissions, "a refused relabel sends nothing");

		// Retry reconciles the one stored comment instead of writing again.
		publish = draft_publish(*cards);
		expect(publish != nullptr && single_offered(*cards) && publish->isEnabled() && publish->text() == QStringLiteral("Retry"), "the draft offers Retry for the same publication");
		send_single(*cards);
		const auto text  = QString::fromStdString(issue);
		const auto shown = wait_until([&] { return posted_count(*cards, text) == 1U && draft_card(*cards) == nullptr; });
		const auto after = github_application_fixture_access::review_mutation_evidence(application);
		std::cout << "BPR_1093:lost-relabel-retried:after-retry native_posts=" << after.NativePosts << " submissions=" << after.Submissions << " stored=" << after.StoredComments
				  << " duplicate=" << after.DuplicateReturned << " body=\"" << after.LastBody << "\" posted=" << posted_count(*cards, text)
				  << " question_posted=" << posted_count(*cards, QString::fromStdString(question)) << '\n'
				  << std::flush;
		expect(shown, "the drawer shows the one posted thread in place of the draft");
		expect(after.NativePosts == 1U, "one write at the server across the lost answer and the retry: no duplicate post");
		expect(after.Submissions == 2U && after.DuplicateReturned, "the retry was answered by reconciling the stored comment");
		expect(after.StoredComments == 1U && after.LastBody == issue, "the server holds exactly the one comment, with the bytes prepared before the relabel attempt");
		expect(posted_count(*cards, text) == 1U && posted_count(*cards, QString::fromStdString(question)) == 0U, "one posted thread, and the relabelled body was never sent");

		// Published, the comment is the server's: no draft is left to relabel.
		expect(!editor->shown_draft().has_value() || editor->shown_draft()->State != core::review_draft_state::drafting, "nothing is left drafting");
		static_cast<void>(choose_label(*editor, core::feedback_label::question));
		settle();
		const auto finished = github_application_fixture_access::review_mutation_evidence(application);
		expect(finished.NativePosts == 1U && finished.StoredComments == 1U && finished.Submissions == after.Submissions, "a relabel after publication sends nothing");
		std::cout << "BPR_1093:lost-relabel-retried:pass" << '\n'
				  << std::flush;
		shut_down(composed);
	}

	/// [[BPR-1477]]. A comment whose answer was lost, read again: by the
	/// window itself once the answer is lost, then by Refresh pressed while
	/// the outcome is unknown. The draft is held again each time, locked and
	/// open in the drawer, and Retry still reconciles the one stored comment.
	/// Before, a reread retired the composition and the retry with it.
	void run_lost_refreshed_retried()
	{
		auto       composed = ::composed{ };
		const auto body     = std::string{ "issue: BPR-1477 a reread keeps the retry." };
		open_draft(composed, true, body);
		auto& application = *composed.Application;
		auto& window      = application.window();
		auto* editor      = window.review_editor();
		auto* cards       = window.conversation_panel()->cards();
		const auto progress = [&application] { return github_application_fixture_access::window_read_progress(application); };
		const auto held     = [&] {
			auto* const publish = draft_publish(*cards);
			return cards->writing_draft() && draft_status(*cards).contains(QStringLiteral("retry is safe")) && accepted_state(*editor) == core::review_draft_state::publishing
				&& accepted_body(*editor) == body && publish != nullptr && single_offered(*cards) && publish->isEnabled() && publish->text() == QStringLiteral("Retry");
		};
		const auto describe = [&](const std::string_view aStep) {
			auto* const publish = draft_publish(*cards);
			std::cout << "BPR_1477:lost-refreshed-retried:" << aStep << " reading=" << progress().Reading << " draft_card=" << (draft_card(*cards) != nullptr)
					  << " writing=" << cards->writing_draft() << " status=\"" << draft_status(*cards).toStdString() << "\" state=" << (accepted_state(*editor) ? static_cast<int>(*accepted_state(*editor)) : -1)
					  << " body=\"" << accepted_body(*editor) << "\" publish=" << (publish != nullptr ? publish->text().toStdString() : std::string{ "none" })
					  << " retry=" << editor->publication_retry_offered() << '\n'
					  << std::flush;
		};

		auto* publish = draft_publish(*cards);
		wait_for([&] { publish = draft_publish(*cards); return publish != nullptr && single_offered(*cards) && publish->isEnabled(); },
				 "the drawer offers Add single comment");
		const auto shownBefore = progress().Snapshot;
		send_single(*cards);
		wait_for([&] { return github_application_fixture_access::review_mutation_evidence(application).LostResult; }, "the fixture stores the comment and loses its answer");
		wait_for([&] { return draft_card(*cards) != nullptr && draft_status(*cards).contains(QStringLiteral("retry is safe")); },
				 "the draft says the result is unknown and a retry is safe");
		const auto lost = github_application_fixture_access::review_mutation_evidence(application);
		expect(lost.NativePosts == 1U && lost.StoredComments == 1U, "one write reached the server");

		// The window's own reread, judged at the end, so that the Refresh
		// below is judged on its own.
		const auto rereadByItself = wait_until([&] { return progress().Snapshot != shownBefore && !progress().Reading && held(); }, 8000);
		describe("after-own-reread");

		// Refresh, as the reviewer presses it, while the retry is held.
		const auto shownAtRefresh = progress().Snapshot;
		expect(window.github_refresh_action()->isEnabled(), "Refresh is offered while the outcome is unknown");
		window.github_refresh_action()->trigger();
		wait_for([&] { return progress().Snapshot != shownAtRefresh && !progress().Reading && window.session_panel()->status() == core::github_status::pull_request_ready; },
				 "Refresh reads the pull request again");
		const auto heldAfterRefresh = wait_until(held);
		describe("after-refresh");
		expect(heldAfterRefresh, "after the Refresh the draft is still held publishing, open in the drawer, and offers Retry for the same publication");
		expect(editor->body_editor()->isReadOnly(), "the draft stays locked to the body it was prepared with");
		const auto refreshed = github_application_fixture_access::review_mutation_evidence(application);
		expect(refreshed.NativePosts == lost.NativePosts && refreshed.Submissions == lost.Submissions, "neither reread sent anything");

		// Retry still reconciles the one stored comment.
		send_single(*cards);
		const auto text  = QString::fromStdString(body);
		const auto shown = wait_until([&] { return posted_count(*cards, text) == 1U && draft_card(*cards) == nullptr; });
		const auto after = github_application_fixture_access::review_mutation_evidence(application);
		std::cout << "BPR_1477:lost-refreshed-retried:after-retry native_posts=" << after.NativePosts << " submissions=" << after.Submissions << " stored=" << after.StoredComments
				  << " duplicate=" << after.DuplicateReturned << " posted=" << posted_count(*cards, text) << " own_reread=" << rereadByItself << '\n'
				  << std::flush;
		expect(shown, "the drawer shows the one posted thread in place of the draft");
		expect(after.NativePosts == 1U && after.StoredComments == 1U && after.LastBody == body, "one write at the server across the lost answer, both rereads and the retry");
		expect(after.Submissions == 2U && after.DuplicateReturned, "the retry was answered by reconciling the stored comment");
		expect(rereadByItself, "after the lost answer the window read the pull request again by itself and held the draft for its retry");
		std::cout << "BPR_1477:lost-refreshed-retried:pass" << '\n'
				  << std::flush;
		shut_down(composed);
	}

	void run_relabel_before_publish()
	{
		auto       composed = ::composed{ };
		const auto issue    = std::string{ "issue: BPR-1093 the label changes before Publish." };
		const auto question = std::string{ "question: BPR-1093 the label changes before Publish." };
		open_draft(composed, false, issue);
		auto& application = *composed.Application;
		auto& window      = application.window();
		auto* editor      = window.review_editor();
		auto* cards       = window.conversation_panel()->cards();
		expect(editor->publish_action()->isEnabled(), "the accepted issue draft can be published");
		expect(single_offered(*cards), "the drawer offers its Add single comment too");

		// Opening the drawer's split menu turns the event loop. Hold the
		// private store lane so that both gestures below really precede the
		// relabel's durable acceptance, regardless of native scheduling.
		const auto releaseDraft = github_application_fixture_access::hold_private_review_effects(application);
		struct held_draft
		{
			std::function<void()> Release;
			~held_draft() { Release(); }
		} held{ releaseDraft };
		expect(choose_label(*editor, core::feedback_label::question), "the chip relabels the draft question");
		const auto pending = accepted_body(*editor) == issue;
		const auto offered = editor->publish_action()->isEnabled();
		editor->publish_action()->trigger();
		// The editor's refusal is read before the drawer's gesture: sending
		// from the split arrow's menu lets events run, and the relabel's
		// acceptance then clears the editor's status.
		const auto  said   = wait_until([&] { return editor->mutation_status_text().startsWith(QStringLiteral("Not published")) || github_application_fixture_access::review_mutation_evidence(application).Submissions != 0U; });
		const auto  told   = editor->mutation_status_text();
		auto* const drawer = draft_publish(*cards);
		if (drawer != nullptr && drawer->isEnabled()) {
			send_single(*cards);
		}
		const auto whilePending = github_application_fixture_access::review_mutation_evidence(application);
		std::cout << "BPR_1093:relabel-before-publish:pending=" << pending << " offered=" << offered << " submissions=" << whilePending.Submissions
				  << " drawer=" << (drawer != nullptr) << " sent=\"" << whilePending.LastBody << "\" status=\"" << told.toStdString() << "\" card=\"" << draft_status(*cards).toStdString() << "\"" << '\n'
				  << std::flush;
		expect(pending, "both Publish gestures came while the relabel was not accepted yet");
		// Publish follows the accepted projection, so it can still be pressed;
		// the composition refuses a Publish whose draft has a change in flight.
		expect(said && whilePending.Submissions == 0U && told.startsWith(QStringLiteral("Not published")), "a Publish pressed while the relabel waits for its owner sends nothing, and says so");
		expect(accepted_body(*editor) == issue, "both public gestures finish while the durable relabel is still held");
		releaseDraft();
		wait_for([&] { return accepted_body(*editor) == question; }, "the owner accepts the relabelled draft");
		settle();
		const auto accepted = github_application_fixture_access::review_mutation_evidence(application);
		expect(accepted.Submissions == 0U && accepted.NativePosts == 0U && accepted.TargetObservations == 0U, "neither gesture sent a body, the one shown before the relabel least of all");
		expect(accepted_state(*editor) == core::review_draft_state::drafting, "the draft is still drafting, so a new gesture is needed");

		// Publish now sends exactly the accepted body, once.
		expect(editor->publish_action()->isEnabled(), "the accepted relabelled draft can be published");
		editor->publish_action()->trigger();
		const auto text   = QString::fromStdString(question);
		const auto posted = wait_until([&] { return posted_count(*cards, text) == 1U && draft_card(*cards) == nullptr; });
		const auto after  = github_application_fixture_access::review_mutation_evidence(application);
		std::cout << "BPR_1093:relabel-before-publish:after native_posts=" << after.NativePosts << " stored=" << after.StoredComments << " body=\"" << after.LastBody << "\"" << '\n'
				  << std::flush;
		expect(posted, "the drawer shows the posted thread");
		expect(after.NativePosts == 1U && after.StoredComments == 1U && after.LastBody == question, "one write, with exactly the relabelled body");
		expect(posted_count(*cards, QString::fromStdString(issue)) == 0U, "the body shown before the relabel was never published");
		std::cout << "BPR_1093:relabel-before-publish:pass" << '\n'
				  << std::flush;
		shut_down(composed);
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication qt{ aArgc, aArgv };
	const auto   only = aArgc > 1 ? std::string_view{ aArgv[1] } : std::string_view{ };
	if (only.empty() || only == "lost-relabel-retried") {
		run_lost_relabel_retried();
	}
	if (only.empty() || only == "lost-refreshed-retried") {
		run_lost_refreshed_retried();
	}
	if (only.empty() || only == "relabel-before-publish") {
		run_relabel_before_publish();
	}
	std::cout << "label-publication-recovery-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
