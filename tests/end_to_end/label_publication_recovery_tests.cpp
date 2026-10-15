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
// - lost-refreshed-retried ([[BPR-1477]]): the answer is lost and Refresh is
//   pressed while the outcome is unknown; the draft is held for the same retry
//   after the read, and Retry reconciles the one stored comment.
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
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/review_composition_application_host.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "drawer_single_comment.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

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
	std::optional<QString> RecoveryRender;

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
		QTemporaryDir                                                 Temporary;
		std::unique_ptr<app::github_application>                      Application;
		std::shared_ptr<app::detail::review_submission_fixture_state> Server = std::make_shared<app::detail::review_submission_fixture_state>();
		std::shared_ptr<app::detail::github_fixture_vault_state>      Vault  = app::detail::make_shared_github_fixture_vault();
	};

	/// The recorded GitHub.com pull request, signed in natively, with review.cpp
	/// on screen and a new comment open in the drawer holding \p aBody.
	void open_review(composed& aComposed, const bool aLoseFirst, const app::detail::github_public_path_fixture aFixture = app::detail::github_public_path_fixture::github_com, const std::string_view aPath = "review.cpp", const app::detail::github_epoch_fixture_phase aPhase = app::detail::github_epoch_fixture_phase::single)
	{
		expect(aComposed.Temporary.isValid(), "private temporary application root");
		auto options                          = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot           = e2e::private_data_root(aComposed.Temporary);
		options.IndependentWindowReads        = true;
		options.RepeatReadSessions            = true;
		options.SharedDeviceVault             = aComposed.Vault;
		options.ReviewSubmissionServer        = aComposed.Server;
		options.EpochPhase                    = aPhase;
		options.LoseFirstReviewMutationResult = aLoseFirst;
		expect(options.ApplicationDataRoot.has_value(), "private desktop test root under the canonical storage policy");
		auto terminal = false;
		auto startup  = github_application_fixture_access::start(aFixture, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				aComposed.Application = std::move(*aResult);
			} else {
				std::cerr << "BPR1520 fixture startup failure=" << static_cast<int>(aResult.error()) << " source=" << app::startup_failure_source() << std::endl;
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
		window.home_open_bar()->setText(aFixture == app::detail::github_public_path_fixture::inline_drawer_v1 ? QStringLiteral("https://github.com/bpr-fixtures/inline-drawer/pull/22") : QStringLiteral("https://github.com/octo-org/review-repository/pull/17"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "the pull request opens through the native session");
		wait_for([&] { return github_application_fixture_access::reveal_fixture_file(*aComposed.Application, aPath) == app::detail::fixture_file_reveal_result::shown; },
				 "review.cpp is shown in Review");
	}
	void open_draft(composed& aComposed, const bool aLoseFirst, const std::string& aBody, const app::detail::github_public_path_fixture aFixture = app::detail::github_public_path_fixture::github_com, const std::string_view aPath = "review.cpp")
	{
		open_review(aComposed, aLoseFirst, aFixture, aPath);
		auto& window = aComposed.Application->window();
		wait_for([&] {
			const auto seeded = github_application_fixture_access::seed_attended_comment_draft(*aComposed.Application, core::diff_side::right, aBody);
			expect(seeded != app::detail::fixture_file_reveal_result::failed, "a new comment can be written on a head line");
			return seeded == app::detail::fixture_file_reveal_result::shown;
		},
				 "a new inline draft holds its text");
		auto* const editor = window.review_editor();
		wait_for([&] { return accepted_body(*editor) == aBody && !editor->rejected_candidate(); }, "the draft's owner accepted the text and settled pending edits");
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

	/// [[BPR-1477]]. Refresh pressed while a comment's outcome is unknown.
	/// The read replaces the composition the retry was bound to; the draft is
	/// held again, locked and open in the drawer, and Retry still reconciles
	/// the one stored comment. Before, the draft was shown nowhere and Retry
	/// was gone.
	void run_lost_refreshed_retried()
	{
		auto       composed = ::composed{ };
		const auto body     = std::string{ "issue: BPR-1477 a Refresh keeps the retry." };
		open_draft(composed, true, body);
		auto&      application = *composed.Application;
		auto&      window      = application.window();
		auto*      editor      = window.review_editor();
		auto*      cards       = window.conversation_panel()->cards();
		const auto progress    = [&application] { return github_application_fixture_access::window_read_progress(application); };
		const auto held        = [&] {
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
		send_single(*cards);
		wait_for([&] { return github_application_fixture_access::review_mutation_evidence(application).LostResult; }, "the fixture stores the comment and loses its answer");
		wait_for([&] { return draft_card(*cards) != nullptr && draft_status(*cards).contains(QStringLiteral("retry is safe")); },
				 "the draft says the result is unknown and a retry is safe");
		const auto lost = github_application_fixture_access::review_mutation_evidence(application);
		expect(lost.NativePosts == 1U && lost.StoredComments == 1U, "one write reached the server");
		wait_for([&] { return held() && !progress().Reading; }, "the draft is held for its retry, and nothing is reading the pull request");

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
		expect(refreshed.NativePosts == lost.NativePosts && refreshed.Submissions == lost.Submissions, "the Refresh sent nothing");

		// Retry still reconciles the one stored comment.
		send_single(*cards);
		const auto text  = QString::fromStdString(body);
		const auto shown = wait_until([&] { return posted_count(*cards, text) == 1U && draft_card(*cards) == nullptr; });
		const auto after = github_application_fixture_access::review_mutation_evidence(application);
		std::cout << "BPR_1477:lost-refreshed-retried:after-retry native_posts=" << after.NativePosts << " submissions=" << after.Submissions << " stored=" << after.StoredComments
				  << " duplicate=" << after.DuplicateReturned << " posted=" << posted_count(*cards, text) << '\n'
				  << std::flush;
		expect(shown, "the drawer shows the one posted thread in place of the draft");
		expect(after.NativePosts == 1U && after.StoredComments == 1U && after.LastBody == body, "one write at the server across the lost answer, the Refresh and the retry");
		expect(after.Submissions == 2U && after.DuplicateReturned, "the retry was answered by reconciling the stored comment");
		std::cout << "BPR_1477:lost-refreshed-retried:pass" << '\n'
				  << std::flush;
		shut_down(composed);
	}

	auto saved_comments(qt::workbench_window& aWindow) -> QDialog*
	{
		aWindow.open_review_composition();
		auto* button = aWindow.review_submission_editor_view()->findChild<QToolButton*>(QStringLiteral("wholeReviewSavedComments"));
		expect(button != nullptr, "Finish review offers Saved comments");
		button->click();
		auto* dialog = aWindow.private_review()->findChild<QDialog*>(QStringLiteral("savedComments"));
		expect(dialog != nullptr && dialog->isVisible(), "Saved comments opens through the product action");
		return dialog;
	}
	auto inspect_saved(QDialog& aDialog, const core::review_draft_id aDraft, const std::string& aBody) -> bool
	{
		auto*      choices  = aDialog.findChild<QComboBox*>(QStringLiteral("savedCommentChoices"));
		const auto selected = choices->findData(QVariant::fromValue<qulonglong>(aDraft.value()));
		if (selected < 0) {
			return false;
		}
		choices->setCurrentIndex(selected);
		auto* text = aDialog.findChild<QPlainTextEdit*>(QStringLiteral("savedCommentBody"));
		return text->isReadOnly() && text->toPlainText() == QString::fromStdString(aBody);
	}
	void reopen_target(qt::workbench_window& aWindow, const int aNumber)
	{
		aWindow.home_action()->trigger();
		aWindow.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/%1").arg(aNumber));
		aWindow.home_open_action()->trigger();
		wait_for([&] { return aWindow.session_panel()->status() == core::github_status::pull_request_ready; }, "navigation opens the chosen review");
	}
	void run_review_failure_navigation(const bool aUnknown)
	{
		auto       composed = ::composed{ };
		const auto body     = std::string{ "issue: BPR-1520 exact retained inline comment α" };
		open_draft(composed, false, body);
		auto&      window       = composed.Application->window();
		auto*      editor       = window.review_editor();
		const auto original     = *editor->shown_draft();
		const auto originalHead = github_application_fixture_access::accepted_head(*composed.Application);
		auto*      review       = window.conversation_panel()->cards()->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReviewDraft"));
		expect(review && review->isEnabled(), "inline comment offers Start a review");
		review->click();
		wait_for([&] { return editor->shown_draft() && core::pending_in_review(*editor->shown_draft()); }, "comment is held in the local review");
		window.open_review_composition();
		auto* summary = window.review_submission_editor_view();
		wait_for([&] { return summary->shown_draft() && summary->shown_draft()->Comments.size() == 1 && summary->submit_action()->isEnabled(); }, "review carries the exact inline draft");
		{
			const auto lock                   = std::scoped_lock{ composed.Server->Mutex };
			composed.Server->RefuseNextReview = !aUnknown;
			composed.Server->LoseNextReview   = aUnknown;
		}
		summary->submit_action()->trigger();
		wait_for([&] { return aUnknown ? summary->reconcile_action()->isEnabled() : summary->submit_action()->isEnabled(); }, "failed publication exposes its recovery controls");
		const auto progress = github_application_fixture_access::window_read_progress(*composed.Application).Snapshot;
		window.github_refresh_action()->trigger();
		wait_for([&] { return github_application_fixture_access::window_read_progress(*composed.Application).Snapshot != progress && window.session_panel()->status() == core::github_status::pull_request_ready; }, "refresh after failed publication completes");
		reopen_target(window, 18);
		reopen_target(window, 17);
		wait_for([&] { return github_application_fixture_access::reveal_fixture_file(*composed.Application, "review.cpp") == app::detail::fixture_file_reveal_result::shown; }, "return shows the original review");
		auto* saved = saved_comments(window);
		wait_for([&] { return inspect_saved(*saved, original.Id, body); }, "exact comment text remains inspectable after failure, refresh and PR switch/return");
		saved->close();
		shut_down(composed);
		composed.Application.reset();
		open_review(composed, false, app::detail::github_public_path_fixture::github_com, "review.cpp", app::detail::github_epoch_fixture_phase::reuse);
		auto& restarted = composed.Application->window();
		saved           = saved_comments(restarted);
		wait_for([&] { return inspect_saved(*saved, original.Id, body); }, "exact original comment identity and body survive application reconstruction");
		expect(saved->findChild<QLabel*>(QStringLiteral("savedCommentAuthority"))->text().contains(QString::fromStdString(originalHead)), "saved comment identifies its original head");
		expect(github_application_fixture_access::accepted_head(*composed.Application) != originalHead, "controlled restart advances the head before recovery");
		if (!aUnknown) {
			expect(saved->findChild<QLabel*>(QStringLiteral("savedCommentAuthority"))->text().contains(QStringLiteral("Earlier comparison")), "old-head content is retained with an explicit copy-and-check path");
			auto* discard = saved->findChild<QPushButton*>(QStringLiteral("savedCommentDiscard"));
			expect(discard && discard->isEnabled(), "old unpublished draft can be explicitly retired without deleting its text");
			discard->click();
			auto* question = saved->findChild<QDialog*>(QStringLiteral("discardSavedDraft"));
			expect(question != nullptr, "retiring retained text asks explicitly");
			auto confirmed = false;
			QObject::connect(question, &QDialog::accepted, saved, [&] { confirmed = true; });
			question->findChild<QAbstractButton*>(QStringLiteral("discardSavedDraftConfirm"))->click();
			expect(confirmed, "explicit discard confirmation dispatches its accepted gesture");
			const auto discarded = wait_until([&] { return inspect_saved(*saved, original.Id, body) && saved->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->currentText().contains(QStringLiteral("Discarded")); });
			if (!discarded) {
				std::cerr << "BPR_1520:discard-refused status=" << restarted.findChild<QLabel*>(QStringLiteral("privateReviewStatus"))->text().toStdString() << " choice=" << saved->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->currentText().toStdString() << '\n';
			}
			expect(discarded, "retired old-head draft remains readable and frees active capacity");
		}
		saved->close();
		if (!aUnknown) {
			restarted.close_review_composition();
			expect(restarted.comparison_surface()->select_conversation_line(core::diff_side::right, 29), "choose current-head code after rejected old-head review");
			wait_for([&] { return restarted.comment_action()->isEnabled(); }, "stale saved material does not prevent a new eligible comment");
			restarted.comment_action()->trigger();
			auto* current = restarted.conversation_panel()->cards()->draft_editor();
			expect(current && !current->isReadOnly(), "current-head comment editor opens after restart");
			current->setPlainText(QStringLiteral("BPR-1520 explicit new comment on the current head"));
			wait_for([&] { return accepted_body(*restarted.review_editor()) == "BPR-1520 explicit new comment on the current head"; }, "new comment persists without rebinding the old content");
		}
		if (aUnknown) {
			{
				const auto lock                  = std::scoped_lock{ composed.Server->Mutex };
				composed.Server->HideReviewReads = false;
			}
			auto* restored = restarted.review_submission_editor_view();
			wait_for([&] { return restored->reconcile_action()->isEnabled(); }, "restart discovers uncertain review");
			restored->reconcile_action()->trigger();
			wait_for([&] { return restored->confirmed_submissions() == 1; }, "read-only reconciliation confirms the original review");
			auto* confirmed = saved_comments(restarted);
			wait_for([&] { return inspect_saved(*confirmed, original.Id, body) && confirmed->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->currentText().contains(QStringLiteral("Published")); }, "confirmed publication is distinct from retained unpublished material");
			expect(!confirmed->findChild<QPushButton*>(QStringLiteral("savedCommentDiscard"))->isEnabled(), "a confirmed server comment cannot be discarded through local draft recovery");
			confirmed->close();
		}
		{
			const auto lock = std::scoped_lock{ composed.Server->Mutex };
			expect(composed.Server->ReviewPosts.size() == 1, "failure, refresh, navigation, restart and reconciliation never duplicate the POST");
		}
		std::cout << "BPR_1520:" << (aUnknown ? "uncertain" : "rejected") << ":navigation-restart:pass\n"
				  << std::flush;
		shut_down(composed);
	}
	void run_draft_capacity_recovery()
	{
		auto       composed = ::composed{ };
		const auto body     = std::string{ "issue: BPR-1520 keep my written comment" };
		open_draft(composed, false, body);
		auto&      window   = composed.Application->window();
		const auto original = *window.review_editor()->shown_draft();
		for (auto count = 0U; count < 200U; ++count) {
			const auto line = 20U + count % 90U;
			expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, line), "select another current-head source line");
			const auto selected = wait_until([&] { return window.review_editor()->shown_draft() && window.review_editor()->shown_draft()->Anchor.Line == line; });
			if (!selected) {
				std::cerr << "BPR_1520:selection count=" << count << " line=" << line << " shown=" << (window.review_editor()->shown_draft() ? int(window.review_editor()->shown_draft()->Anchor.Line) : -1) << " status=" << window.review_editor()->mutation_status_text().toStdString() << '\n';
			}
			expect(selected, "a selected line has its runtime placeholder");
		}
		auto* dialog = saved_comments(window);
		wait_for([&] { return inspect_saved(*dialog, original.Id, body) && dialog->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->count() == 1; }, "two hundred line selections create no database drafts beyond written content");
		expect(!dialog->findChild<QPushButton*>(QStringLiteral("savedCommentsFreeUnused"))->isEnabled(), "runtime placeholders never need storage cleanup");
		dialog->close();
		window.close_review_composition();
		window.comment_action()->trigger();
		auto* field = window.conversation_panel()->cards()->draft_editor();
		expect(field != nullptr && !field->isReadOnly(), "comment creation remains usable beyond64 line selections");
		const auto release = github_application_fixture_access::hold_private_review_effects(*composed.Application);
		struct held_save
		{
			std::function<void()> Release;
			~held_save() { Release(); }
		} held{ release };
		field->setPlainText(QStringLiteral("First partial text"));
		field->setPlainText(QStringLiteral("BPR-1520 new comment after recovery"));
		window.review_editor()->publish_action()->trigger();
		expect(github_application_fixture_access::review_mutation_evidence(*composed.Application).Submissions == 0, "a Publish gesture during first save sends nothing");
		release();
		wait_for([&] { return accepted_body(*window.review_editor()) == "BPR-1520 new comment after recovery" && !window.review_editor()->rejected_candidate(); }, "first real edit is durably accepted");
		settle();
		expect(github_application_fixture_access::review_mutation_evidence(*composed.Application).Submissions == 0, "saving completion never replays the earlier Publish gesture");
		const auto newId = window.review_editor()->shown_draft()->Id;
		const auto index = window.comparison_surface()->current_semantic_index();
		const auto root  = *e2e::private_data_root(composed.Temporary);
		shut_down(composed);
		composed.Application.reset();
		// Upgrade fixture: rows an older application created merely by
		// browsing. All effects stay in this course's owner-private root.
		{
			auto opened = adapters::sqlite::make_private_review_store({ { root, "github.com", "1001", "octo-org/review-repository", 17 }, core::bpr_profile_id{ 1 }, core::git_host_id{ 1 }, core::github_account_id{ 1001 }, core::repository_id{ 2001 }, core::pull_request_id{ 3001 } });
			expect(opened.has_value(), "test-owned retained store opens for legacy placeholder setup");
			const auto record = *(*opened)->load_draft(original.Id);
			const auto target = core::make_composition_target(record.Binding, record.Draft.Draft.Anchor, *index);
			expect(target.has_value(), "legacy placeholder uses the original sealed comparison");
			application::review_composition_application_host host(**opened);
			expect(host.allocate_draft(*target, *index).has_value() && host.allocate_draft(*target, *index).has_value(), "two synthetic legacy empty rows are retained for explicit cleanup");
		}
		open_review(composed, false);
		auto& restarted = composed.Application->window();
		dialog          = saved_comments(restarted);
		wait_for([&] { return dialog->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->count() == 4 && inspect_saved(*dialog, original.Id, body); }, "restart retains written content and legacy placeholders");
		expect(inspect_saved(*dialog, newId, "BPR-1520 new comment after recovery"), "first-edit materialization survives restart exactly");
		dialog->findChild<QPushButton*>(QStringLiteral("savedCommentsFreeUnused"))->click();
		auto* question = dialog->findChild<QDialog*>(QStringLiteral("clearUnusedDrafts"));
		expect(question != nullptr, "freeing legacy placeholders asks explicitly");
		question->findChild<QAbstractButton*>(QStringLiteral("clearUnusedDraftsConfirm"))->click();
		wait_for([&] { return dialog->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"))->count() == 2 && inspect_saved(*dialog, original.Id, body); }, "explicit cleanup removes only legacy unedited empties");
		expect(inspect_saved(*dialog, newId, "BPR-1520 new comment after recovery"), "cleanup preserves the other exact authored body");
		expect(inspect_saved(*dialog, original.Id, body), "inspect original content for recovery render");
		if (RecoveryRender) {
			expect(dialog->grab().save(*RecoveryRender + QStringLiteral("-dialog.png")), "saved comments dialog evidence");
			expect(restarted.grab().save(*RecoveryRender + QStringLiteral("-window.png")), "full production window evidence");
		}
		dialog->close();
		std::cout << "BPR_1520:lazy-drafts-legacy-cleanup-restart:pass\n"
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
	if (aArgc > 2) {
		RecoveryRender = QString::fromLocal8Bit(aArgv[2]);
	}
	const auto only = aArgc > 1 ? std::string_view{ aArgv[1] } : std::string_view{ };
	if (only.empty() || only == "lost-relabel-retried") {
		run_lost_relabel_retried();
	}
	if (only.empty() || only == "lost-refreshed-retried") {
		run_lost_refreshed_retried();
	}
	if (only.empty() || only == "review-note-recovery") {
		run_review_failure_navigation(false);
		run_review_failure_navigation(true);
	}
	if (only.empty() || only == "review-note-recovery" || only == "draft-capacity-recovery") {
		run_draft_capacity_recovery();
	}
	if (only.empty() || only == "relabel-before-publish") {
		run_relabel_before_publish();
	}
	std::cout << "label-publication-recovery-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
