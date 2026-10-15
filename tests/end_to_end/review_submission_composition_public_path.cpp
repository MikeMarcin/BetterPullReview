#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "cli_publication_course.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QToolButton>
#include <filesystem>
#include <iostream>
#include <map>
#include <sqlite3.h>
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
	void open(app::github_application& aApp, e2e::cli_publication_course& aCli)
	{
		aApp.show();
		auto& window = aApp.window();
		aCli.connect(window);
		wait([&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }, "connected controls");
		// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
		window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
		window.home_open_action()->trigger();
		wait([&] { auto* editor = window.review_submission_editor_view(); return editor->shown_draft().has_value() && (editor->discard_action()->isEnabled() || editor->reconcile_action()->isEnabled()); }, "review discovery through ordinary PR open");
		// [[BPR-0771]]. The Home bar opens on the reviewer's opening modality,
		// Overview by default; the retired locator left the window on Review.
		// This course is about the review surface, so it chooses Review.
		window.choose_modality(qt::review_modality::review);
		QApplication::processEvents();
		window.open_review_composition();
		QApplication::processEvents();
		auto* const page = window.findChild<QScrollArea*>(
			QStringLiteral("composeReviewPage"));
		expect(page != nullptr && page->isVisible()
				   && page->isAncestorOf(window.review_submission_editor_view()),
			   "explicit Compose review opens the retained editor in the central page");
	}
	/// Confirmations already read, by editor; cleared with each application.
	auto confirmations_seen() -> std::map<const qt::review_submission_editor*, std::size_t>&
	{
		static auto seen = std::map<const qt::review_submission_editor*, std::size_t>{ };
		return seen;
	}
	void shutdown(app::github_application& aApp)
	{
		aApp.begin_shutdown();
		wait([&] { return aApp.status().ShutdownComplete; }, "whole-review shutdown");
		confirmations_seen().clear();
	}
	/// [[BPR-1345]] v003. GitHub confirmed the review: the editor says so once,
	/// the window says "Review submitted…" and the next review starts at once.
	void submitted(qt::review_submission_editor& aEditor)
	{
		auto& seen = confirmations_seen()[&aEditor];
		wait([&] { return aEditor.confirmed_submissions() > seen; }, "exact review receipt");
		seen = aEditor.confirmed_submissions();
		expect(aEditor.last_submitted() && aEditor.last_submitted()->State == state::submitted, "the confirmed review is the submitted one");
		auto* const window = qobject_cast<QMainWindow*>(aEditor.window());
		expect(window != nullptr && window->statusBar()->currentMessage().startsWith(QStringLiteral("Review submitted")), "confirmation is visible");
		expect(aEditor.shown_draft() && aEditor.shown_draft()->Id != aEditor.last_submitted()->Id, "the next review starts at once");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir data;
	const auto    dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "review-submission-composition:fail private data root\n";
		return EXIT_FAILURE;
	}
	const auto                  cliMode = argc > 1 && std::string_view{ argv[1] } == "--cli";
	e2e::cli_publication_course cli{ cliMode };
	auto                        server = std::make_shared<app::detail::review_submission_fixture_state>();
	server->CliStrictCourse            = cliMode;
	auto make                          = [&](app::detail::github_epoch_fixture_phase aPhase = app::detail::github_epoch_fixture_phase::single, app::detail::github_public_path_fixture aFixture = app::detail::github_public_path_fixture::github_com, bool aRepeatReads = false) {
		std::unique_ptr<app::github_application> started;
		auto                                     startupTerminal = false;
		auto                                     options         = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot, .EpochPhase = aPhase, .ReviewSubmissionServer = server, .RepeatReadSessions = aRepeatReads };
		cli.configure(options);
		auto startup = access::start(aFixture,
									 options,
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
		application->show();
		application->window().open_review_composition();
		QApplication::processEvents();
		auto& initial = *application->window().review_submission_editor_view();
		expect(!initial.shown_draft() && initial.target_editor()->isHidden() && initial.body_editor()->isHidden(), "signed-out no-draft composition explains itself without empty inputs");
		expect(initial.status_label()->text().contains(QStringLiteral("Sign in")) && !initial.submit_action()->toolTip().isEmpty(), "signed-out next step and disabled reason");
		open(*application, cli);
		auto& window = application->window();
		auto& editor = *window.review_submission_editor_view();
		expect(!window.review_editor()->shown_draft(), "whole review needs no source-line draft");
		expect(editor.target_editor()->toPlainText().contains(QStringLiteral("github.com")) && editor.target_editor()->toPlainText().contains(QStringLiteral("octo-reviewer")), "host and actual actor shown");
		if (cliMode) {
			if (argc > 2) {
				expect(window.grab().save(QString::fromLocal8Bit(argv[2]) + "-review-ready.png"), "full ordinary CLI review presentation renders");
			}
			expect(server->Submissions == 0U && server->StagedWrites == 0U, "startup, discovery and connection trust grant no write");
			for (const auto disposition : { core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes }) {
				window.open_review_composition();
				editor.choose_disposition(disposition);
				editor.body_editor()->setPlainText(disposition == core::review_disposition::approve ? QString{ } : QStringLiteral("Ordinary chosen-account review."));
				expect(editor.target_editor()->toPlainText().contains(QStringLiteral("octo-reviewer")) && editor.target_editor()->toPlainText().contains(QStringLiteral("github.com")), "ordinary review names actual actor and server");
				editor.body_editor()->setFocus();
				const auto before       = server->Submissions;
				const auto acquisitions = server->CliAcquisitions;
				editor.submit_action()->trigger();
				submitted(editor);
				expect(server->Submissions == before + 1U && server->CliAcquisitions == acquisitions + 1U, "one ordinary review gesture reacquires and dispatches exactly one POST");
				expect(QApplication::activeModalWidget() == nullptr, "ordinary review opens no modal");
			}
			window.open_review_composition();
			editor.body_editor()->setPlainText(QStringLiteral("Kept strict-capability draft."));
			const auto kept   = *editor.shown_draft();
			const auto before = server->Submissions;
			cli.Strict->store(false);
			editor.submit_action()->trigger();
			expect(server->Submissions == before && editor.shown_draft() == kept && editor.status_label()->text().contains(QStringLiteral("draft is kept")), "absent strict capability refuses original review and keeps draft");
			cli.Strict->store(true);
			const auto shown = editor.shown_projection();
			editor.body_editor()->setPlainText(QStringLiteral("Revised strict-capability draft."));
			editor.present(shown);
			editor.submit_action()->trigger();
			expect(server->Submissions == before && editor.status_label()->text().contains(QStringLiteral("summary changed")), "changed frozen content refuses original ordinary gesture");
			for (const auto failure : { application::review_mutation_failure::credential_unavailable, application::review_mutation_failure::credential_tool_failed, application::review_mutation_failure::cancelled }) {
				{
					std::scoped_lock lock{ server->Mutex };
					server->CliAcquisitionFailure = failure;
				}
				window.activateWindow();
				QApplication::processEvents();
				editor.body_editor()->setFocus();
				QApplication::processEvents();
				const auto priorFocus = QApplication::focusWidget();
				expect(priorFocus && editor.isAncestorOf(priorFocus), "failure course establishes actual editor focus before Submit");
				std::cout << "CLI_FAILURE_FOCUS before=" << (priorFocus ? priorFocus->objectName().toStdString() : "none") << " visible=" << editor.body_editor()->isVisible() << "\n";
				editor.submit_action()->trigger();
				wait([&] { return editor.submit_action()->isEnabled() && editor.status_label()->text().contains(QStringLiteral("draft is kept")); }, "typed acquisition failure returns editable draft");
				expect(server->Submissions == before && editor.body_editor()->toPlainText() == QStringLiteral("Revised strict-capability draft."), "acquisition failure performs zero POST and retains exact summary");
				if (failure != application::review_mutation_failure::cancelled) {
					expect(editor.status_label()->text().contains(QStringLiteral("octo-reviewer")) && editor.status_label()->text().contains(QStringLiteral("github.com")), "failure binds the failed operation's chosen account and host");
				}
				std::cout << "CLI_FAILURE_FOCUS after=" << (QApplication::focusWidget() ? QApplication::focusWidget()->objectName().toStdString() : "none") << "\n";
				expect(QApplication::focusWidget() == priorFocus && QApplication::activeModalWidget() == nullptr, "failure preserves existing keyboard focus without a new modal");
				if (argc > 2) {
					expect(window.grab().save(QString::fromLocal8Bit(argv[2]) + "-review-" + QString::fromUtf8(application::name(failure).data(), static_cast<qsizetype>(application::name(failure).size())) + ".png"), "typed CLI review refusal renders in its existing surface");
				}
			}
			{
				std::scoped_lock lock{ server->Mutex };
				server->CliAcquisitionFailure.reset();
				server->LoseNextReview = true;
			}
			editor.submit_action()->trigger();
			wait([&] { return editor.status_label()->text().contains(QStringLiteral("unknown")); }, "attempted unknown write has honest status");
			expect(server->Submissions == before + 1U && !editor.submit_action()->isEnabled(), "unknown attempt cannot issue a second POST");
			{
				std::scoped_lock lock{ server->Mutex };
				server->HideReviewReads = false;
			}
			editor.reconcile_action()->trigger();
			submitted(editor);
			expect(server->Submissions == before + 1U, "unknown attempt reconciles GET-only to its existing review");
			cli.check();
			shutdown(*application);
			std::cout << "review-submission-composition:pass CLI dispositions and exact refusal\n";
			return EXIT_SUCCESS;
		}

		expect(!editor.submit_action()->isEnabled(), "blank comment refused");
		const auto first = *editor.shown_draft();
		for (const auto disposition : { core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes }) {
			// [[BPR-1345]] v002: a verdict button submits; the verdict is
			// chosen first here, and Submit sends it below.
			editor.choose_disposition(disposition);
			const auto body = disposition == core::review_disposition::approve ? QString{ } : QStringLiteral("Whole review <literal> summary.");
			editor.body_editor()->setPlainText(body);
			expect(editor.submit_action()->isEnabled(), "ready disposition enables submit");
			const auto draft = *editor.shown_draft();
			expect(editor.findChild<QLabel*>(QStringLiteral("wholeReviewDraftState"))->text().contains(QStringLiteral("kept")), "composed kept draft state is explained");
			if (disposition == core::review_disposition::comment) {
				auto* files = window.files_drawer()->contents_view();
				wait([&] { return files->model()->rowCount() > 0; }, "normal file row available");
				const auto selections = window.statistics().FileSelections;
				files->setCurrentIndex(files->model()->index(0, 0));
				wait([&] { return window.statistics().FileSelections > selections; }, "normal file selection dispatched");
				expect(editor.shown_draft() == draft, "file navigation preserves the PR-scoped summary");
				window.close_review_composition();
				// [[BPR-1246]], [[BPR-1212]] v002. A deletion on the left is a
				// GitHub `side: LEFT` line. Once the epoch host answers that this
				// file's base is the merge base's version, it takes its draft as
				// any line does; selecting it publishes nothing.
				wait([&] { return window.comparison_surface()->select_conversation_line(core::diff_side::left, 45U); }, "select the deletion on the left side");
				wait([&] {
					const auto& shown = window.review_editor()->shown_draft();
					return window.left_conversation_endpoint_current() && shown && shown->Anchor.Side == core::diff_side::left && shown->Anchor.Line == 45U;
				},
					 "the left deletion takes its draft once its base numbering is known to be GitHub's");
				expect(window.comment_action()->isEnabled() && window.review_editor()->publish_action()->isEnabled(), "the left deletion offers a new Comment and its Publish");
				// Negative control: without that evidence the same line is refused
				// with its reason, and forcing Comment opens nothing.
				window.set_conversation_endpoint_placement(false, true);
				QApplication::processEvents();
				expect(!window.comment_action()->isEnabled() && window.comment_action()->toolTip().contains(QStringLiteral("not known to match")),
					   "without the evidence a left line does not offer a new Comment, and says why");
				window.comment_action()->setEnabled(true);
				window.comment_action()->trigger();
				expect(!window.comment_action()->isEnabled() && !window.conversation_panel()->cards()->writing_draft(), "negative control cannot force a left-side new comment open");
				window.set_conversation_endpoint_placement(true, true);
				expect(window.comparison_surface()->select_conversation_line(core::diff_side::right, 29U), "select a current-head line for the local comment");
				wait([&] { return window.comment_action()->isEnabled(); }, "current-head Comment is admitted after the refused left target");
				const auto beforeComment = access::review_mutation_evidence(*application);
				window.comment_action()->trigger();
				// [[BPR-1212]]. Comment opens the draft in the drawer, beside the code.
				auto* const cards = window.conversation_panel()->cards();
				wait([&] { return cards->writing_draft() && cards->draft_editor() != nullptr && cards->draft_editor()->isVisible() && !cards->draft_editor()->isReadOnly(); },
					 "Comment opens a usable draft in the drawer");
				expect(window.comparison_surface()->isVisible(), "the code stays in view while the comment is written");
				auto* inlineDraft = window.review_editor();
				cards->draft_editor()->setPlainText(QStringLiteral("Fixture comment kept without publishing."));
				wait([&] { return inlineDraft->shown_draft() && inlineDraft->shown_draft()->Body == "Fixture comment kept without publishing." && !inlineDraft->rejected_candidate(); }, "typed comment is accepted locally");
				expect(inlineDraft->workshop_original_editor()->isHidden() && inlineDraft->workshop_proposal_editor()->isHidden()
						   && !inlineDraft->reload_draft_action()->isVisible() && !inlineDraft->reapply_draft_action()->isVisible()
						   && inlineDraft->findChild<QToolButton*>(QStringLiteral("reviewDraftReloadButton"))->isHidden()
						   && inlineDraft->findChild<QToolButton*>(QStringLiteral("reviewDraftReapplyButton"))->isHidden(),
					   "usable inline page has no empty proposal panels or inactive conflict actions");
				const auto afterComment = access::review_mutation_evidence(*application);
				expect(afterComment.Submissions == beforeComment.Submissions && afterComment.StoredComments == beforeComment.StoredComments
						   && afterComment.Replies == beforeComment.Replies,
					   "writing through Comment performs zero provider mutation");
				if (argc > 1) {
					expect(window.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-inline.png")), "usable inline render");
				}
				window.close_review_composition();
				if (window.conversations_drawer()->state() != core::workbench_drawer_state::expanded) {
					window.conversations_drawer_toggle_action()->trigger();
				}
				window.ai_tools_action()->trigger();
				expect(window.tray_tabs()->tabText(1) == QStringLiteral("Notes") && window.tray_tabs()->accessibleTabName(1) == QStringLiteral("Notes"), "the composed private editor is reached under Notes");
				auto* notes = window.private_notes();
				wait([&] { return notes->new_review_action()->isEnabled(); }, "private ledger available without any agent opt-in");
				notes->new_review_action()->trigger();
				notes->body_editor()->setPlainText(QStringLiteral("A private note, copied literally."));
				notes->save_action()->trigger();
				wait([&] { return notes->notes_list()->count() == 1 && !notes->save_action()->isEnabled(); }, "PR note is committed through the existing ledger");
				const auto noteId = notes->notes_list()->item(0)->data(Qt::UserRole);
				notes->new_file_action()->trigger();
				notes->body_editor()->setPlainText(QStringLiteral("Private file note"));
				notes->save_action()->trigger();
				wait([&] { return notes->notes_list()->count() == 2; }, "file note commits");
				expect(notes->notes_list()->item(0)->data(Qt::UserRole) != noteId && !notes->notes_list()->item(0)->text().contains(QStringLiteral("Pull request")), "newest note names its file");
				{
					// [[BPR-1210]]. The note was written with its file's path, so it can
					// still name the file after a later update drops it.
					auto ledger = std::filesystem::path{ };
					for (const auto& entry : std::filesystem::recursive_directory_iterator{ *dataRoot }) {
						if (entry.path().filename() == "review-ledger.sqlite3") {
							ledger = entry.path();
						}
					}
					::sqlite3* database = nullptr;
					expect(!ledger.empty() && ::sqlite3_open_v2(ledger.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
						   "the private ledger opens for reading");
					::sqlite3_stmt* query = nullptr;
					expect(::sqlite3_prepare_v2(database, "SELECT path FROM private_note_path", -1, &query, nullptr) == SQLITE_OK, "the note path table is read");
					auto paths = QStringList{ };
					while (::sqlite3_step(query) == SQLITE_ROW) {
						paths.push_back(QString::fromUtf8(reinterpret_cast<const char*>(::sqlite3_column_text(query, 0))));
					}
					::sqlite3_finalize(query);
					::sqlite3_close(database);
					expect(paths.size() == 1 && !paths.front().isEmpty() && notes->notes_list()->item(0)->text().contains(paths.front()),
						   "the file note's path is kept with it, and only the file note has one");
				}
				notes->body_editor()->setPlainText(QString(static_cast<qsizetype>(core::maximum_note_bytes) + 1, QLatin1Char{ 'x' }));
				notes->save_action()->trigger();
				// [[BPR-1246]]. The ledger answers on its own lane ([[BPR-1211]]),
				// so the refusal is waited for rather than read at once.
				wait([&] { return notes->body_editor()->toPlainText().size() == static_cast<qsizetype>(core::maximum_note_bytes) + 1
							   && notes->findChild<QLabel*>(QStringLiteral("privateNoteStatus"))->text().contains(QStringLiteral("invalid_record")); },
					 "refused write retains the full local candidate and says why");
				notes->discard_action()->trigger();
				expect(notes->body_editor()->toPlainText() == QStringLiteral("Private file note"), "explicit discard restores the accepted note");
				notes->body_editor()->setPlainText(QStringLiteral("Edited file note"));
				notes->save_action()->trigger();
				wait([&] { return !notes->save_action()->isEnabled(); }, "revision-checked note edit commits");
				notes->erase_action()->trigger();
				wait([&] { return notes->notes_list()->count() == 1; }, "revision-checked note delete commits");
				notes->notes_list()->setCurrentRow(0);
				QApplication::processEvents();
				// [[BPR-1246]]. The purpose sits above the Notes pages' tab widget
				// ([[BPR-1242]]), not inside the Agent tools column.
				auto* const purpose    = window.findChild<QLabel*>(QStringLiteral("workbenchNotesPurpose"));
				auto* const notesPages = window.findChild<QTabWidget*>(QStringLiteral("privateConversationTabs"));
				expect(purpose != nullptr && purpose->isVisible() && notesPages != nullptr
						   && purpose->width() <= notesPages->width()
						   && purpose->height() >= purpose->heightForWidth(purpose->width()),
					   "the Notes privacy explanation wraps within the composed tray");
				if (argc > 1) {
					QApplication::processEvents();
					expect(window.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-private-notes.png")), "private notes controls render");
				}
				const auto beforePromotion = access::review_mutation_evidence(*application);
				wait([&] { return notes->promote_action()->isEnabled(); }, "saved private note shares Comment's target");
				notes->promote_action()->trigger();
				wait([&] { return inlineDraft->shown_draft() && inlineDraft->shown_draft()->Body == "A private note, copied literally."
							   && cards->writing_draft() && cards->draft_editor() != nullptr && cards->draft_editor()->isVisible()
							   && cards->draft_editor()->toPlainText() == QStringLiteral("A private note, copied literally."); },
					 "promotion opens its exact local draft in the drawer");
				const auto afterPromotion   = access::review_mutation_evidence(*application);
				const auto privatePromotion = [&](const auto& aEvidence) {
					return notes->notes_list()->count() == 1 && notes->notes_list()->item(0)->data(Qt::UserRole) == noteId
						&& notes->body_editor()->toPlainText() == QStringLiteral("A private note, copied literally.")
						&& aEvidence.Submissions == beforePromotion.Submissions && aEvidence.StoredComments == beforePromotion.StoredComments
						&& aEvidence.Replies == beforePromotion.Replies;
				};
				expect(privatePromotion(afterPromotion) && window.tray_tabs()->currentIndex() == 0, "one item is copied to Comments while its private source stays and no GitHub mutation occurs");
				auto publishedPromotion = afterPromotion;
				++publishedPromotion.StoredComments;
				expect(!privatePromotion(publishedPromotion), "negative control rejects implicit publication evidence");
				auto* removedSource = notes->notes_list()->takeItem(0);
				expect(!privatePromotion(afterPromotion), "negative control rejects a removed source row");
				notes->notes_list()->addItem(removedSource);
				notes->notes_list()->setCurrentRow(0);
				if (argc > 1) {
					expect(window.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-note-promoted.png")), "promoted private note render");
				}
				window.close_review_composition();
				window.open_review_composition();
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
					   && request.Proposal.Draft == draft.Id && request.Proposal.Attempt == editor.last_submitted()->Attempt,
				   "server sees exact frozen request");
		}
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->Submissions == 3U, "exactly three submissions for three explicit gestures");
			server->RefuseBeforeSend = true;
		}
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
		open(*application, cli);
		auto* restoredNotes = application->window().private_notes();
		wait([&] { return restoredNotes->notes_list()->count() == 1; }, "private note survives application reconstruction through SQLite");
		restoredNotes->notes_list()->setCurrentRow(0);
		expect(restoredNotes->body_editor()->toPlainText() == QStringLiteral("A private note, copied literally."), "promotion retained the durable private source");
		auto& restored = *application->window().review_submission_editor_view();
		submitted(restored);
		expect(restored.last_submitted()->Id == pending.Id && restored.last_submitted()->Attempt == pending.Attempt
				   && restored.last_submitted()->Body == pending.Body && restored.last_submitted()->Target == pending.Target,
			   "real SQLite restores original attempt into reconstructed application");
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->Submissions == 5U && server->Reviews.back() == original, "reopening preserves key and issues no new submission");
		}
		if (argc == 2) {
			application->window().resize(1280, 900);
			// [[BPR-1345]] v003. The confirmation closed the page; Compose
			// review opens it again on the next review.
			application->window().open_review_composition();
			QApplication::processEvents();
			auto* const page = application->window().findChild<QScrollArea*>(
				QStringLiteral("composeReviewPage"));
			expect(page != nullptr && page->isVisible()
					   && page->widget() != nullptr
					   && page->widget()->isAncestorOf(&restored),
				   "reopened review returns to the dedicated Compose review page");
			page->ensureWidgetVisible(&restored, 0, 0);
			QApplication::processEvents();
			const auto visible = page->viewport()->contentsRect();
			const auto topLeft = restored.mapTo(page->viewport(), QPoint{ });
			expect(page->horizontalScrollBar()->maximum() == 0
					   && topLeft.x() >= visible.x()
					   && topLeft.x() + restored.width() <= visible.right() + 1,
				   "review identity and controls fit the central composition viewport");
			expect(application->window().grab().save(QString::fromLocal8Bit(argv[1])), "full-window render saved");
		}
		// A retired terminal must not complete a draft after changed authority.
		restored.body_editor()->setPlainText(QStringLiteral("Frozen across authority change"));
		access::hold_review_submission_terminals(*application, true);
		restored.submit_action()->trigger();
		wait([&] { return access::held_review_submission_terminals(*application) > 0U; }, "terminal ready before retirement");
		expect(restored.shown_draft()->Target != pending.Target && restored.shown_draft()->Body == "Frozen across authority change", "new explicit draft binds newly observed head after historical recovery");
		// [[BPR-0771]]. The ordinary Sign out is the signed-in host's card's.
		QPushButton* signOut = nullptr;
		for (auto* const card : application->window().server_cards()) {
			if (card->signed_in()) {
				signOut = card->sign_out_button();
			}
		}
		expect(signOut != nullptr, "the signed-in host's card exposes the ordinary sign-out control");
		signOut->click();
		wait([&] { return restored.status_label()->text().contains(QStringLiteral("authority changed")); }, "fake account invalidation independently retires whole review");
		shutdown(*application);
		expect(!restored.submit_action()->isEnabled() && restored.shown_draft()->State == state::submitting, "retired terminal cannot complete or rebind frozen review");
		application.reset();
		application = make(app::detail::github_epoch_fixture_phase::seed);
		open(*application, cli);
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
		// [[BPR-1345]] v002: Discard asks first when there are words to lose.
		frozen.discard_action()->trigger();
		QApplication::processEvents();
		expect(frozen.discard_confirmation() != nullptr && frozen.discard_confirmation()->question() == QStringLiteral("Discard your review summary?"),
			   "Discard asks before it deletes a written summary");
		frozen.discard_confirmation()->findChild<QAbstractButton*>(QStringLiteral("discardReviewConfirmationConfirm"))->click();
		QApplication::processEvents();
		expect(frozen.shown_draft()->Id != oldDraft.Id && frozen.shown_draft()->Target != oldDraft.Target
				   && frozen.shown_draft()->Body.empty(),
			   "discard and explicit new review adopt the new revision");
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->Submissions == 6U, "changed-head refusal issued no submission");
		}
		shutdown(*application);
		application.reset();
		application        = make(app::detail::github_epoch_fixture_phase::single, app::detail::github_public_path_fixture::uiforetw_74);
		auto& closedWindow = application->window();
		application->show();
		closedWindow.github_connect_action()->trigger();
		wait([&] { return closedWindow.github_status_text() == "GitHub account connected."; }, "closed fixture connected");
		closedWindow.home_open_bar()->setText(QStringLiteral("google/UIforETW#74"));
		closedWindow.home_open_action()->trigger();
		auto& closed = *closedWindow.review_submission_editor_view();
		wait([&] { return closed.status_label()->text().contains(QStringLiteral("pull request is closed")); }, "ordinary closed fixture open explains the refused review admission");
		closedWindow.open_review_composition();
		QApplication::processEvents();
		expect(!closed.shown_draft() && closed.target_editor()->isHidden() && closed.body_editor()->isHidden() && !closed.submit_action()->isEnabled(), "closed PR has no unexplained empty fields or invented draft");
		if (argc == 2) {
			expect(closedWindow.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-closed.png")), "closed-state render");
		}
		{
			std::scoped_lock lock{ server->Mutex };
			expect(server->Submissions == 6U, "explanation and closed review perform zero additional publications");
		}
		shutdown(*application);
		application.reset();
		// The preflight course re-reads the fake server through the public
		// Refresh, so its recorded read script replays.
		application = make(app::detail::github_epoch_fixture_phase::single, app::detail::github_public_path_fixture::github_com, true);
		open(*application, cli);
		{
			auto& course   = application->window();
			auto& composer = *course.review_submission_editor_view();
			// [[BPR-1085]]. My own feedback before a native decision (RFC-BPR-0048
			// R14, R15, R42; pack cases approve-with-open-feedback and
			// native-request-changes). The fake server changes only through its
			// next read, which a public Refresh asks for. No label or resolution
			// submits a review or changes the reviewer's decision, and each
			// explicit decision submits its exact native event once.
			const auto submissions = [&] {
				std::scoped_lock lock{ server->Mutex };
				return server->Submissions;
			};
			const auto chip          = [&] { return composer.preflight_button()->toolTip(); };
			const auto refresh_until = [&](const QString& aWords, const char* aWhy) {
				course.github_refresh_action()->trigger();
				if (!e2e::wait_until_plain([&] { return chip() == aWords; }, e2e::default_queue_budget)) {
					std::cerr << "review-submission-composition: the chip says \"" << chip().toStdString() << "\" (" << course.github_status_text() << ")\n";
					throw std::runtime_error{ aWhy };
				}
			};
			const auto approve = composer.disposition_button(core::review_disposition::approve);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->Submissions == 6U, "the preflight course starts after six submissions");
			}
			expect(composer.shown_draft() && composer.shown_draft()->State == state::drafting, "a new review to decide");
			wait([&] { return chip() == QStringLiteral("Yours on GitHub: 0 open blocking"); }, "the fixture's own threads are plain, read exactly, and none blocks");
			const auto quiet = submissions();

			access::stage_server_root(*application, "PRRT_fixture_1", "issue (blocking): Please preserve snapshot identity across refreshes.", std::nullopt);
			access::stage_server_root(*application, "PRRT_fixture_2", "issue (blocking): Another reviewer's blocker.", std::nullopt, std::pair{ std::string{ "U_fixture_author" }, std::string{ "octo-author" } });
			refresh_until(QStringLiteral("Yours on GitHub: 1 open blocking\nOthers: 1 open blocking (not yours)"),
						  "my labelled root is mine; another reviewer's blocker is counted apart, never as mine");
			expect(submissions() == quiet && composer.shown_draft()->Disposition == core::review_disposition::comment && composer.shown_draft()->State == state::drafting,
				   "a label read from GitHub submits nothing and chooses no decision");
			composer.choose_disposition(core::review_disposition::approve);
			expect(composer.shown_draft()->Disposition == core::review_disposition::approve && approve->isEnabled() && !approve->icon().isNull()
					   && approve->toolTip().contains(QStringLiteral("approving does not resolve")),
				   "Approve stays available and chosen explicitly while my blocking feedback is open, with its dot and tooltip");
			auto* const menu = composer.preflight_menu();
			expect(menu->actions().size() >= 2 && menu->actions().at(1)->text() == QStringLiteral("review.cpp:42 · Please preserve snapshot identity across refreshes."),
				   "the list names my blocker where it is, by its subject");
			menu->actions().at(1)->trigger();
			auto* const composePage = course.findChild<QScrollArea*>(QStringLiteral("composeReviewPage"));
			expect(course.current_modality() == qt::review_modality::discuss && !composePage->isVisible() && submissions() == quiet,
				   "a row reveals the exact conversation in Discuss and submits nothing");
			course.open_review_composition();
			QApplication::processEvents();

			access::stage_server_root(*application, "PRRT_fixture_1", std::nullopt, true);
			refresh_until(QStringLiteral("Yours on GitHub: 0 open blocking\nOthers: 1 open blocking (not yours)"), "resolving my last blocker clears it");
			expect(submissions() == quiet && composer.shown_draft()->Disposition == core::review_disposition::approve && composer.shown_draft()->State == state::drafting && approve->icon().isNull(),
				   "the last resolution approves nothing and leaves my explicit choice as it was");
			access::stage_server_root(*application, "PRRT_fixture_3", "todo: Keep the second current-head anchor exact.", std::nullopt);
			refresh_until(QStringLiteral("Yours on GitHub: 0 open blocking\n1 todo, not marked blocking\nOthers: 1 open blocking (not yours)"),
						  "an undecorated todo is guidance, never a blocker");
			access::stage_server_root(*application, "PRRT_fixture_3", "todo (blocking): Keep the second current-head anchor exact.", std::nullopt);
			refresh_until(QStringLiteral("Yours on GitHub: 1 open blocking\nOthers: 1 open blocking (not yours)"), "a label edit to blocking is mine again");
			expect(submissions() == quiet && composer.shown_draft()->Disposition == core::review_disposition::approve && !approve->icon().isNull(),
				   "a label change requests no changes and submits nothing");

			// approve-with-open-feedback: the explicit Approve is submitted as chosen.
			composer.body_editor()->setPlainText(QStringLiteral("Approving while my own blocking comment is still open."));
			composer.submit_action()->trigger();
			submitted(composer);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->Submissions == quiet + 1U && server->Reviews.back().Proposal.Disposition == core::review_disposition::approve
						   && server->Reviews.back().Proposal.Body == "Approving while my own blocking comment is still open.",
					   "Approve with my open blocking feedback submits exactly the native approval, once");
			}
			// native-request-changes: inspect my blocking feedback, then choose.
			expect(chip().startsWith(QStringLiteral("Yours on GitHub: 1 open blocking")) && composer.preflight_menu()->actions().at(1)->text().contains(QStringLiteral("Keep the second current-head anchor exact.")),
				   "my blocking feedback is there to inspect before deciding");
			composer.choose_disposition(core::review_disposition::request_changes);
			composer.body_editor()->setPlainText(QStringLiteral("Requesting changes after reading my own blocking feedback."));
			composer.submit_action()->trigger();
			submitted(composer);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->Submissions == quiet + 2U && server->Reviews.back().Proposal.Disposition == core::review_disposition::request_changes,
					   "Request changes submits exactly the native event once, with no second label-made event");
			}

			// A selected unsent draft binds the decision; a stale view of it is refused.
			composer.body_editor()->setPlainText(QStringLiteral("Comment with a pending inline draft."));
			course.close_review_composition();
			course.choose_modality(qt::review_modality::review);
			auto* const files = course.files_drawer()->contents_view();
			wait([&] { return files->model()->rowCount() > 0; }, "a file to comment on");
			const auto selections = course.statistics().FileSelections;
			files->setCurrentIndex(files->model()->index(0, 0));
			wait([&] { return course.statistics().FileSelections > selections; }, "the file is selected");
			if (!e2e::wait_until_plain([&] { return course.comparison_surface()->select_conversation_line(core::diff_side::right, 29U); }, e2e::default_queue_budget)) {
				const auto* surface = course.comparison_surface();
				const auto& context = surface->current_context();
				const auto& result = surface->current_result();
				const auto selectedFile = course.files_drawer()->contents()->file_at(files->currentIndex());
				std::cerr << "review-submission-selection row=" << files->currentIndex().row()
						  << " context=" << context.has_value() << " result=" << static_cast<bool>(result)
						  << " semantic=" << static_cast<bool>(surface->current_semantic_index())
						  << " file-match=" << (selectedFile && context && context->Key.File == *selectedFile)
						  << " left-lines=" << (result ? result->Alignment.LeftLineCount : 0U)
						  << " right-lines=" << (result ? result->Alignment.RightLineCount : 0U)
						  << " tab-pending=" << access::epoch_refresh_identity(*application).TabPending
						  << " modality=" << static_cast<int>(course.current_modality()) << '\n';
				throw std::runtime_error{ "select a current-head line for the pending draft" };
			}
			wait([&] { return course.comment_action()->isEnabled(); }, "Comment is offered on the current-head line");
			course.comment_action()->trigger();
			auto* const inlineDraft = course.review_editor();
			// [[BPR-1212]]. In the drawer, beside the code.
			wait([&] { return course.conversation_panel()->cards()->writing_draft() && !inlineDraft->body_editor()->isReadOnly(); }, "the inline draft opens in the drawer");
			inlineDraft->body_editor()->setPlainText(QStringLiteral("issue (blocking): Pending inline draft."));
			wait([&] { return inlineDraft->shown_draft() && inlineDraft->shown_draft()->Body == "issue (blocking): Pending inline draft."; }, "the inline draft is kept locally");
			course.close_review_composition();
			course.open_review_composition();
			QApplication::processEvents();
			wait([&] { return chip().contains(QStringLiteral("Yours, not sent: 1 blocking")); }, "my selected unsent blocking draft is pending, apart from what GitHub has");
			const auto shownSelection = composer.shown_projection();
			course.close_review_composition();
			course.open_review_composition(true);
			inlineDraft->body_editor()->setPlainText(QStringLiteral("issue (blocking): Pending inline draft, relabelled."));
			wait([&] { return inlineDraft->shown_draft() && inlineDraft->shown_draft()->Body == "issue (blocking): Pending inline draft, relabelled."; }, "the selected draft changes");
			course.close_review_composition();
			course.open_review_composition();
			QApplication::processEvents();
			expect(composer.shown_projection().Prepared != shownSelection.Prepared, "the editor now shows the changed selection");
			const auto beforeStale = submissions();
			composer.present(shownSelection);
			composer.submit_action()->trigger();
			expect(submissions() == beforeStale && composer.shown_draft()->State == state::drafting
					   && composer.status_label()->text().contains(QStringLiteral("unsent comments changed after you chose Submit review")),
				   "Submit from a view of the old selection is refused and says what changed");
			const auto shownSummary = composer.shown_projection();
			composer.body_editor()->setPlainText(QStringLiteral("Comment with a pending inline draft, revised."));
			composer.present(shownSummary);
			composer.submit_action()->trigger();
			expect(submissions() == beforeStale && composer.status_label()->text().contains(QStringLiteral("the summary changed")),
				   "Submit from a view of the old summary is refused; the revised summary is never bound to the old action");
			composer.submit_action()->trigger();
			submitted(composer);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->Submissions == beforeStale + 1U && server->Reviews.back().Proposal.Body == "Comment with a pending inline draft, revised."
						   && server->Reviews.back().Proposal.Disposition == core::review_disposition::comment,
					   "the current view submits the revised summary once");
			}
		}
		shutdown(*application);
		cli.check();
		std::cout << "review-submission-composition:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "review-submission-composition:fail " << error.what() << '\n';
		shutdown(*application);
		return EXIT_FAILURE;
	}
}
