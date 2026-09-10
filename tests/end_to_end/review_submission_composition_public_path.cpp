#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "public_path_poll.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QToolButton>
#include <iostream>
#include <stdexcept>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;
	using state  = core::review_submission_state;
	template <typename Predicate>
	void wait(Predicate aPredicate, const char* aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::default_queue_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}
	void open(app::github_application& aApp)
	{
		aApp.show();
		auto& window = aApp.window();
		window.github_connect_action()->trigger();
		wait([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "connected controls");
		window.session_panel()->owner_editor()->setText(QStringLiteral("octo-org"));
		window.session_panel()->repository_editor()->setText(QStringLiteral("review-repository"));
		window.session_panel()->pull_request_editor()->setValue(17);
		window.github_open_action()->trigger();
		wait([&] { auto* editor = window.review_submission_editor_view(); return editor->shown_draft().has_value() && (editor->discard_action()->isEnabled() || editor->new_review_action()->isEnabled()); }, "review discovery through ordinary PR open");
		if (!window.conversations_drawer_toggle_action()->isChecked()) {
			window.conversations_drawer_toggle_action()->trigger();
		}
	}
	void shutdown(app::github_application& aApp)
	{
		aApp.begin_shutdown();
		wait([&] { return aApp.status().ShutdownComplete; }, "whole-review shutdown");
	}
	void submitted(qt::review_submission_editor& aEditor)
	{
		wait([&] { return aEditor.shown_draft() && aEditor.shown_draft()->State == state::submitted; }, "exact review receipt");
		expect(aEditor.status_label()->text().contains(QStringLiteral("confirmed")), "confirmation is visible");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir data;
	if (!data.isValid()) {
		return EXIT_FAILURE;
	}
	auto server = std::make_shared<app::detail::review_submission_fixture_state>();
	auto make   = [&](app::detail::github_epoch_fixture_phase aPhase = app::detail::github_epoch_fixture_phase::single) {
		std::unique_ptr<app::github_application> started;
		auto                                     startupTerminal = false;
		auto                                     startup         = access::start(app::detail::github_public_path_fixture::github_com,
																				 app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = std::filesystem::path{ data.path().toStdString() }, .EpochPhase = aPhase, .ReviewSubmissionServer = server },
																				 [&](auto aResult) {
										 startupTerminal = true;
										 if (aResult) {
											 started = std::move(*aResult);
										 }
																				 });
		wait([&] { return startupTerminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
		expect(started != nullptr, "application startup succeeds");
		return started;
	};
	auto application = make();
	try {
		open(*application);
		auto& window = application->window();
		auto& editor = *window.review_submission_editor_view();
		expect(!window.review_editor()->shown_draft(), "whole review needs no source-line draft");
		expect(editor.target_editor()->toPlainText().contains(QStringLiteral("github.com")) && editor.target_editor()->toPlainText().contains(QStringLiteral("octo-reviewer")), "host and actual actor shown");
		expect(!editor.submit_action()->isEnabled(), "blank comment refused");
		const auto first = *editor.shown_draft();
		for (const auto disposition : { core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes }) {
			if (editor.new_review_action()->isEnabled()) {
				editor.new_review_action()->trigger();
			}
			editor.disposition_editor()->setCurrentIndex(editor.disposition_editor()->findData(static_cast<int>(disposition)));
			const auto body = disposition == core::review_disposition::approve ? QString{ } : QStringLiteral("Whole review <literal> summary.");
			editor.body_editor()->setPlainText(body);
			expect(editor.submit_action()->isEnabled(), "ready disposition enables submit");
			const auto draft = *editor.shown_draft();
			if (disposition == core::review_disposition::comment) {
				auto* files = window.files_drawer()->contents_view();
				wait([&] { return files->model()->rowCount() > 0; }, "normal file row available");
				const auto selections = window.statistics().FileSelections;
				files->setCurrentIndex(files->model()->index(0, 0));
				wait([&] { return window.statistics().FileSelections > selections; }, "normal file selection dispatched");
				expect(editor.shown_draft() == draft, "file navigation preserves the PR-scoped summary");
			}

			expect(access::reaccept_fixture_revision(*application, false), "metadata refresh accepted");
			expect(editor.shown_draft() == draft, "metadata refresh preserves exact summary and disposition");
			access::hold_review_submission_terminals(*application, true);
			editor.submit_action()->trigger();
			editor.submit_action()->trigger();
			expect(editor.shown_draft()->State == state::submitting && !editor.discard_action()->isEnabled(), "duplicate/pending controls refuse resend and discard");
			wait([&] { return access::held_review_submission_terminals(*application) > 0U; }, "held effect terminal");
			access::hold_review_submission_terminals(*application, false);
			submitted(editor);
			std::scoped_lock lock{ server->Mutex };
			const auto&      request = server->Reviews.back();
			expect(request.Proposal.Body == body.toStdString() && request.Proposal.Disposition == disposition
					   && request.Proposal.Target == first.Target && request.Proposal.ActingAccount == first.ActingAccount
					   && request.Proposal.Draft == draft.Id && request.Proposal.Attempt == editor.shown_draft()->Attempt,
				   "server sees exact frozen request");
		}
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->Submissions == 3U, "exactly three submissions for three explicit gestures");
			server->RefuseBeforeSend = true;
		}
		editor.new_review_action()->trigger();
		editor.body_editor()->setPlainText(QStringLiteral("Retain refused summary"));
		editor.submit_action()->trigger();
		wait([&] { return editor.shown_draft()->State == state::drafting; }, "definite refusal restores draft");
		expect(editor.shown_draft()->Body == "Retain refused summary", "refusal retains body");
		{
			std::scoped_lock lock{ server->Mutex };
			server->RefuseBeforeSend = false;
			server->StaleNextResult  = true;
		}
		editor.submit_action()->trigger();
		wait([&] { return editor.reconcile_action()->isEnabled(); }, "stale attempt stays indeterminate");
		expect(editor.shown_draft()->State == state::submitting && !editor.submit_action()->isEnabled(), "stale result cannot finish or retry review");
		editor.reconcile_action()->trigger();
		submitted(editor);
		editor.new_review_action()->trigger();
		editor.body_editor()->setPlainText(QStringLiteral("Recover after reopening"));
		{
			std::scoped_lock lock{ server->Mutex };
			server->LoseNextResponse = true;
		}
		editor.submit_action()->trigger();
		wait([&] { return editor.reconcile_action()->isEnabled(); }, "lost response remains pending");
		const auto                             pending = *editor.shown_draft();
		application::review_submission_request original;
		{
			std::scoped_lock lock{ server->Mutex };
			original = server->Reviews.back();
			expect(server->Submissions == 5U, "one lost-response submission");
		}
		shutdown(*application);
		application.reset();
		application = make(app::detail::github_epoch_fixture_phase::reuse);
		open(*application);
		auto& restored = *application->window().review_submission_editor_view();
		submitted(restored);
		expect(restored.shown_draft()->Id == pending.Id && restored.shown_draft()->Attempt == pending.Attempt
				   && restored.shown_draft()->Body == pending.Body && restored.shown_draft()->Target == pending.Target,
			   "real SQLite restores original attempt into reconstructed application");
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->Submissions == 5U && server->Reviews.back() == original, "reopening preserves key and issues no new submission");
		}
		if (argc == 2) {
			application->window().resize(1280, 900);
			QApplication::processEvents();
			for (auto* scroll : application->window().findChildren<QScrollArea*>()) {
				if (scroll->widget() && scroll->widget()->isAncestorOf(&restored)) {
					scroll->ensureWidgetVisible(&restored, 0, 0);
				}
			}
			QApplication::processEvents();
			const auto visible = application->window().conversations_drawer()->contentsRect();
			const auto topLeft = restored.mapTo(application->window().conversations_drawer(), QPoint{ });
			expect(topLeft.x() >= visible.x() && topLeft.x() + restored.width() <= visible.right() + 1, "review identity and controls fit the narrowed drawer");
			expect(application->window().grab().save(QString::fromLocal8Bit(argv[1])), "full-window render saved");
		}
		// A retired terminal must not complete a draft after changed authority.
		restored.new_review_action()->trigger();
		restored.body_editor()->setPlainText(QStringLiteral("Frozen across authority change"));
		access::hold_review_submission_terminals(*application, true);
		restored.submit_action()->trigger();
		wait([&] { return access::held_review_submission_terminals(*application) > 0U; }, "terminal ready before retirement");
		expect(restored.shown_draft()->Target != pending.Target && restored.shown_draft()->Body == "Frozen across authority change", "new explicit draft binds newly observed head after historical recovery");
		auto* signOut = application->window().session_panel()->findChild<QToolButton*>(QStringLiteral("signOutGithubButton"));
		expect(signOut != nullptr, "fixture exposes the ordinary sign-out control");
		signOut->click();
		wait([&] { return restored.status_label()->text().contains(QStringLiteral("authority changed")); }, "fake account invalidation independently retires whole review");
		shutdown(*application);
		expect(!restored.submit_action()->isEnabled() && restored.shown_draft()->State == state::submitting, "retired terminal cannot complete or rebind frozen review");
		application.reset();
		application = make(app::detail::github_epoch_fixture_phase::seed);
		open(*application);
		auto& frozen = *application->window().review_submission_editor_view();
		frozen.body_editor()->setPlainText(QStringLiteral("Do not rebind this draft"));
		const auto oldDraft = *frozen.shown_draft();
		application->window().github_refresh_action()->trigger();
		wait([&] { return frozen.status_label()->text().contains(QStringLiteral("revision changed"))
					   && frozen.discard_action()->isEnabled(); }, "fresh server head freezes original local draft");
		frozen.submit_action()->trigger();
		expect(frozen.shown_draft() == oldDraft && frozen.body_editor()->isReadOnly()
				   && !frozen.submit_action()->isEnabled(),
			   "changed head cannot silently submit or rebind original summary");
		frozen.discard_action()->trigger();
		frozen.new_review_action()->trigger();
		expect(frozen.shown_draft()->Id != oldDraft.Id && frozen.shown_draft()->Target != oldDraft.Target
				   && frozen.shown_draft()->Body.empty(),
			   "discard and explicit new review adopt the new revision");
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->Submissions == 6U, "changed-head refusal issued no submission");
		}
		shutdown(*application);
		std::cout << "review-submission-composition:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "review-submission-composition:fail " << error.what() << '\n';
		shutdown(*application);
		return EXIT_FAILURE;
	}
}
