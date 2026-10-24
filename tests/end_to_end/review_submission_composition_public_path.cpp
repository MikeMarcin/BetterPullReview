#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "cli_publication_course.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
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
		// [[BPR-1590]]. Finish your review is a sheet over Review.
		auto* const sheet = window.review_sheet();
		expect(sheet != nullptr && sheet->isVisible() && window.review_sheet_open()
				   && sheet->isAncestorOf(window.review_submission_editor_view()),
			   "explicit Compose review opens the retained editor in the sheet beside the code");
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
	/// [[BPR-1715]]. A new comment on \p aLine of review.cpp's head
	/// side, written in the drawer and put in the review with Start a review.
	auto write_pending(app::github_application& aApp, const std::uint32_t aLine, const QString& aBody) -> core::review_comment_draft
	{
		auto& window = aApp.window();
		window.close_review_sheet();
		window.choose_modality(qt::review_modality::review);
		// The reveal helper remembers its earlier file gesture. After a push,
		// that is not evidence that the new head's comparison has arrived.
		wait([&] {
			if (access::reveal_fixture_file(aApp, "review.cpp") != app::detail::fixture_file_reveal_result::shown) {
				return false;
			}
			const auto current = access::epoch_refresh_identity(aApp);
			return current.PublicationDisplayed && current.AcceptedComparison
				&& current.PublishedComparison == current.AcceptedComparison;
		},
			 "review.cpp's current-head comparison is shown in Review");
		wait([&] { return window.comparison_surface()->select_conversation_line(core::diff_side::right, aLine); }, "a head line of review.cpp is selected");
		if (!e2e::wait_until_plain([&] { return window.comment_action()->isEnabled(); }, e2e::default_queue_budget)) {
			const auto* const draftEditor = window.review_editor();
			const auto&       draft       = draftEditor->shown_draft();
			const auto        selection   = window.comparison_surface()->selection();
			const auto        current     = access::epoch_refresh_identity(aApp);
			std::cerr << "review-submission-composition: line " << aLine << " comment action: " << window.comment_action()->toolTip().toStdString()
					  << " selected=" << selection.has_value() << " selected_line=" << (selection ? selection->PreferredLine.Line : 0U)
					  << " editor_draft=" << draft.has_value() << " readonly=" << draftEditor->body_editor()->isReadOnly()
					  << " editor_anchor=" << (draft && selection && draft->Anchor == selection->PreferredLine)
					  << " draft_state=" << (draft ? static_cast<int>(draft->State) : -1)
					  << " reload=" << draftEditor->reload_draft_action()->isEnabled() << " reapply=" << draftEditor->reapply_draft_action()->isEnabled()
					  << " candidate=" << draftEditor->rejected_candidate().has_value()
					  << " displayed=" << current.PublicationDisplayed << " comparison_pending=" << current.ComparisonPending
					  << " tab_pending=" << current.TabPending << " pair_match=" << (current.PublishedComparison == current.AcceptedComparison) << '\n';
			throw std::runtime_error{ "Comment is offered on the current head line" };
		}
		window.comment_action()->trigger();
		auto* const editor = window.review_editor();
		auto* const cards  = window.conversation_panel()->cards();
		if (!e2e::wait_until_plain([&] { return cards->writing_draft() && !editor->body_editor()->isReadOnly(); }, e2e::default_queue_budget)) {
			std::cerr << "review-submission-composition: line " << aLine << " writing=" << cards->writing_draft() << " readonly=" << editor->body_editor()->isReadOnly()
					  << " draft=" << (editor->shown_draft() ? editor->shown_draft()->Body : std::string{ "none" }) << '\n';
			throw std::runtime_error{ "the drawer opens a new comment" };
		}
		editor->body_editor()->setPlainText(aBody);
		wait([&] { return editor->shown_draft() && editor->shown_draft()->Body == aBody.toStdString() && !editor->rejected_candidate(); }, "the comment is kept locally");
		auto* const start = cards->open_card() != nullptr ? cards->open_card()->findChild<QAbstractButton*>(QStringLiteral("threadReviewDraft")) : nullptr;
		expect(start != nullptr && start->isEnabled(), "the comment offers Start a review");
		start->click();
		wait([&] { return editor->shown_draft() && core::pending_in_review(*editor->shown_draft()); }, "the comment is pending in the review");
		return *editor->shown_draft();
	}
	/// [[BPR-1715]]. Whether Saved comments keeps \p aDraft with exactly
	/// \p aBody, read through the Finish your review page's own button.
	auto saved_exactly(qt::workbench_window& aWindow, const core::review_draft_id aDraft, const std::string& aBody) -> bool
	{
		aWindow.open_review_composition();
		QApplication::processEvents();
		auto* const button = aWindow.review_submission_editor_view()->findChild<QToolButton*>(QStringLiteral("wholeReviewSavedComments"));
		expect(button != nullptr, "Finish your review offers Saved comments");
		button->click();
		auto* const dialog = aWindow.private_review()->findChild<QDialog*>(QStringLiteral("savedComments"));
		expect(dialog != nullptr && dialog->isVisible(), "Saved comments opens");
		auto*      choices  = dialog->findChild<QComboBox*>(QStringLiteral("savedCommentChoices"));
		const auto selected = choices->findData(QVariant::fromValue<qulonglong>(aDraft.value()));
		auto       kept     = false;
		if (selected >= 0) {
			choices->setCurrentIndex(selected);
			kept = dialog->findChild<QPlainTextEdit*>(QStringLiteral("savedCommentBody"))->toPlainText() == QString::fromStdString(aBody);
		}
		dialog->close();
		return kept;
	}
	/// [[BPR-1345]] v003. GitHub confirmed the review: the editor says so once,
	/// the window says "Review submitted…" and the next review starts at once.
	void submitted(qt::review_submission_editor& aEditor)
	{
		auto& seen = confirmations_seen()[&aEditor];
		if (!e2e::wait_until_plain([&] { return aEditor.confirmed_submissions() > seen; }, e2e::default_queue_budget)) {
			std::cerr << "review-submission-composition: no receipt; the page says \"" << aEditor.status_label()->text().toStdString() << "\" state="
					  << (aEditor.shown_draft() ? static_cast<int>(aEditor.shown_draft()->State) : -1) << '\n';
			throw std::runtime_error{ "exact review receipt" };
		}
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
	auto make                          = [&](app::detail::github_epoch_fixture_phase aPhase = app::detail::github_epoch_fixture_phase::single, app::detail::github_public_path_fixture aFixture = app::detail::github_public_path_fixture::github_com, bool aRepeatReads = false,
											 bool aMergeBaseUnavailable = false) {
		std::unique_ptr<app::github_application> started;
		auto                                     startupTerminal = false;
		auto                                     options         = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot, .EpochPhase = aPhase, .ReviewSubmissionServer = server, .RepeatReadSessions = aRepeatReads };
		options.MergeBaseUnavailable                             = aMergeBaseUnavailable;
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
				// [[BPR-1592]]. Notes is the private notes at their lines; the
				// agent's tools are the Agent tab's.
				window.tray_tabs()->setCurrentIndex(1);
				QApplication::processEvents();
				// Below 340 pixels the tab is its icon; its name is always said.
				expect(window.tray_tabs()->accessibleTabName(1) == QStringLiteral("Notes") && (window.tray_tabs()->tabText(1) == QStringLiteral("Notes") || !window.tray_tabs()->tabIcon(1).isNull()),
					   "the composed private editor is reached under Notes");
				auto*      notes  = window.private_notes();
				const auto newest = [notes](const std::string& aText) -> const qt::private_note_item* {
					const auto& all   = notes->notes();
					const auto  found = std::ranges::find(all, aText, &qt::private_note_item::Text);
					return found == all.end() ? nullptr : &*found;
				};
				wait([&] { return notes->new_review_action()->isEnabled(); }, "private ledger available without any agent opt-in");
				notes->new_review_action()->trigger();
				notes->body_editor()->setPlainText(QStringLiteral("Private pull request note"));
				notes->save_action()->trigger();
				wait([&] { return notes->notes().size() == 1U && !notes->save_action()->isEnabled(); }, "PR note is committed through the existing ledger");
				const auto prNote = notes->notes().front().Id;
				expect(!notes->notes().front().Attachment, "the pull request's note has no file");
				// N: a note on the line Comment was written on.
				notes->show_elsewhere(false);
				notes->new_line_action()->trigger();
				wait([&] { return notes->writing() && notes->body_editor()->hasFocus(); }, "N opens a new note at the selected line");
				notes->body_editor()->setPlainText(QStringLiteral("A private note, copied literally."));
				notes->save_action()->trigger();
				wait([&] { return notes->notes().size() == 2U && newest("A private note, copied literally.") != nullptr; }, "line note commits");
				const auto* const lineNote = newest("A private note, copied literally.");
				const auto        noteId   = lineNote->Id;
				const auto        noteLine = lineNote->Attachment ? lineNote->Attachment->Line : std::nullopt;
				expect(noteId != prNote && noteLine.has_value() && noteLine->Blob.has_value() && lineNote->Label != "Pull request"
						   && notes->placement(noteId).Kind == core::note_placement_kind::placed
						   && (!notes->note_rect(noteId).isEmpty() || std::ranges::any_of(notes->edge_chips(), [&noteLine](const QAbstractButton* aChip) { return aChip->text() == QString::number(noteLine->Line + 1U); })),
					   "the new note keeps its line and blob, names its file, and hangs at its line (or, its line scrolled away, waits in a chip)");
				expect(window.comparison_surface()->selection()->PreferredLine.Line == noteLine->Line, "it is on the line that was selected");
				{
					// [[BPR-1210]], [[BPR-1591]]. The note was written with its file's
					// path and its line, so it can still name the file after a later
					// update drops it, and hang at its line again.
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
					expect(::sqlite3_prepare_v2(database, "SELECT side,line FROM private_note_line", -1, &query, nullptr) == SQLITE_OK, "the note line table is read");
					auto lines = std::vector<std::pair<int, int>>{ };
					while (::sqlite3_step(query) == SQLITE_ROW) {
						lines.emplace_back(::sqlite3_column_int(query, 0), ::sqlite3_column_int(query, 1));
					}
					::sqlite3_finalize(query);
					::sqlite3_close(database);
					expect(paths.size() == 1 && !paths.front().isEmpty() && QString::fromStdString(lineNote->Label).contains(paths.front()),
						   "the file note's path is kept with it, and only the file note has one");
					expect(lines.size() == 1U && lines.front().second == static_cast<int>(noteLine->Line) && lines.front().first == (noteLine->Side == core::diff_side::left ? 0 : 1),
						   "the line note's side and line are kept in the ledger");
				}
				notes->open_note(noteId);
				notes->body_editor()->setPlainText(QString(static_cast<qsizetype>(core::maximum_note_bytes) + 1, QLatin1Char{ 'x' }));
				notes->save_action()->trigger();
				// [[BPR-1246]]. The ledger answers on its own lane ([[BPR-1211]]),
				// so the refusal is waited for rather than read at once.
				wait([&] { return notes->body_editor()->toPlainText().size() == static_cast<qsizetype>(core::maximum_note_bytes) + 1
							   && notes->findChild<QLabel*>(QStringLiteral("privateNoteStatus"))->text().contains(QStringLiteral("invalid_record")); },
					 "refused write retains the full local candidate and says why");
				notes->discard_action()->trigger();
				expect(notes->body_editor()->toPlainText() == QStringLiteral("A private note, copied literally."), "explicit discard restores the accepted note");
				notes->close_note();
				notes->open_note(prNote);
				notes->erase_action()->trigger();
				wait([&] { return notes->notes().size() == 1U && notes->notes().front().Id == noteId; }, "revision-checked note delete commits");
				// [[BPR-1592]]. The agent's purpose and tools are the Agent tab's.
				window.tray_tabs()->setCurrentIndex(3);
				QApplication::processEvents();
				auto* const agent       = window.agent_tab();
				auto* const legacyPages = window.findChild<QTabWidget*>(QStringLiteral("privateConversationTabs"));
				expect(window.tray_tabs()->accessibleTabName(3) == QStringLiteral("Agent") && agent != nullptr && agent->isVisible()
						   && agent->isAncestorOf(window.side_chat()->composer()) && legacyPages != nullptr && !legacyPages->isVisible() && !notes->isVisible(),
					   "the accepted Agent tab owns the question field; legacy nested pages and Notes are hidden");
#if defined(BPR_AGENT_INTEGRATIONS)
				// [[BPR-1753]], RFC-BPR-0092 AC01. Shown for the first time on
				// a fixture with no agent on its search path and no agent chosen, the
				// tab looks (file metadata only) and shows the setup, not a
				// conversation; nothing was asked, so nothing ran.
				wait([&] { return agent->state() == qt::agent_tab_state::setting_up && agent->setup_view() != nullptr && agent->setup_view()->isVisible(); },
					 "the first look at the Agent tab shows the setup in place of the conversation");
				expect(agent->setup_view()->open_step() == core::setup_step::agent && agent->setup_view()->step_text().contains(QStringLiteral("No agent found on this computer"))
						   && !window.side_chat()->composer()->isVisible(),
					   "the setup's Agent step says no agent was found, and the question field is not offered yet");
#endif
				window.tray_tabs()->setCurrentIndex(1);
				QApplication::processEvents();
				if (argc > 1) {
					QApplication::processEvents();
					expect(window.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-private-notes.png")), "private notes controls render");
				}
				// Draft comment: the note's words become a draft at its line.
				const auto beforePromotion = access::review_mutation_evidence(*application);
				notes->open_note(noteId);
				wait([&] { return notes->promote_action()->isEnabled(); }, "a note at its line offers Draft comment");
				notes->promote_action()->trigger();
				wait([&] { return inlineDraft->shown_draft() && inlineDraft->shown_draft()->Body == "A private note, copied literally."
							   && inlineDraft->shown_draft()->Anchor.Line == noteLine->Line && inlineDraft->shown_draft()->Anchor.Side == noteLine->Side
							   && cards->writing_draft() && cards->draft_editor() != nullptr && cards->draft_editor()->isVisible()
							   && cards->draft_editor()->toPlainText() == QStringLiteral("A private note, copied literally."); },
					 "Draft comment opens its exact local draft at the note's line, in the drawer");
				const auto afterPromotion   = access::review_mutation_evidence(*application);
				const auto privatePromotion = [&](const auto& aEvidence, const std::vector<qt::private_note_item>& aNotes) {
					return aNotes.size() == 1U && aNotes.front().Id == noteId && aNotes.front().Text == "A private note, copied literally."
						&& aEvidence.Submissions == beforePromotion.Submissions && aEvidence.StoredComments == beforePromotion.StoredComments
						&& aEvidence.Replies == beforePromotion.Replies;
				};
				expect(privatePromotion(afterPromotion, notes->notes()) && window.tray_tabs()->currentIndex() == 0, "one item is copied to Comments while its private source stays and no GitHub mutation occurs");
				auto publishedPromotion = afterPromotion;
				++publishedPromotion.StoredComments;
				expect(!privatePromotion(publishedPromotion, notes->notes()), "negative control rejects implicit publication evidence");
				expect(!privatePromotion(afterPromotion, { }), "negative control rejects a removed source note");
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
		wait([&] { return restoredNotes->notes().size() == 1U; }, "private note survives application reconstruction through SQLite");
		expect(restoredNotes->notes().front().Text == "A private note, copied literally." && restoredNotes->notes().front().Attachment && restoredNotes->notes().front().Attachment->Line,
			   "promotion retained the durable private source, with its line");
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
			// [[BPR-1590]]. The sheet, beside the code.
			auto* const page = application->window().review_sheet()->scroll();
			expect(page != nullptr && page->isVisible()
					   && page->widget() != nullptr
					   && page->widget()->isAncestorOf(&restored),
				   "reopened review returns to the Finish your review sheet");
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
			const auto chip         = [&] { return composer.preflight_button()->toolTip(); };
			const auto refreshUntil = [&](const QString& aWords, const char* aWhy) {
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
			refreshUntil(QStringLiteral("Yours on GitHub: 1 open blocking\nOthers: 1 open blocking (not yours)"),
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
			refreshUntil(QStringLiteral("Yours on GitHub: 0 open blocking\nOthers: 1 open blocking (not yours)"), "resolving my last blocker clears it");
			expect(submissions() == quiet && composer.shown_draft()->Disposition == core::review_disposition::approve && composer.shown_draft()->State == state::drafting && approve->icon().isNull(),
				   "the last resolution approves nothing and leaves my explicit choice as it was");
			access::stage_server_root(*application, "PRRT_fixture_3", "todo: Keep the second current-head anchor exact.", std::nullopt);
			refreshUntil(QStringLiteral("Yours on GitHub: 0 open blocking\n1 todo, not marked blocking\nOthers: 1 open blocking (not yours)"),
						 "an undecorated todo is guidance, never a blocker");
			access::stage_server_root(*application, "PRRT_fixture_3", "todo (blocking): Keep the second current-head anchor exact.", std::nullopt);
			refreshUntil(QStringLiteral("Yours on GitHub: 1 open blocking\nOthers: 1 open blocking (not yours)"), "a label edit to blocking is mine again");
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
			course.close_review_sheet();
			course.choose_modality(qt::review_modality::review);
			auto* const files = course.files_drawer()->contents_view();
			wait([&] { return files->model()->rowCount() > 0; }, "a file to comment on");
			const auto selections        = course.statistics().FileSelections;
			const auto desiredFile       = course.files_drawer()->contents()->file_at(files->model()->index(0, 0));
			const auto beforeFileGesture = access::epoch_refresh_identity(*application);
			files->setCurrentIndex(files->model()->index(0, 0));
			wait([&] { return course.statistics().FileSelections > selections; }, "the file is selected");
			if (!e2e::wait_until_plain([&] { return course.comparison_surface()->select_conversation_line(core::diff_side::right, 29U); }, e2e::default_queue_budget)) {
				const auto* surface      = course.comparison_surface();
				const auto& context      = surface->current_context();
				const auto& result       = surface->current_result();
				const auto  selectedFile = course.files_drawer()->contents()->file_at(files->currentIndex());
				const auto  current      = access::epoch_refresh_identity(*application);
				std::cerr << "review-submission-selection row=" << files->currentIndex().row()
						  << " context=" << context.has_value() << " result=" << static_cast<bool>(result)
						  << " semantic=" << static_cast<bool>(surface->current_semantic_index())
						  << " file-match=" << (selectedFile && context && context->Key.File == *selectedFile)
						  << " left-lines=" << (result ? result->Alignment.LeftLineCount : 0U)
						  << " right-lines=" << (result ? result->Alignment.RightLineCount : 0U)
						  << " requested-file=" << desiredFile.has_value()
						  << " selected-match=" << (desiredFile && current.SelectedFile == desiredFile)
						  << " before-active=" << beforeFileGesture.ActiveTab.has_value()
						  << " active=" << current.ActiveTab.has_value()
						  << " active-file=" << (current.ActiveTab && current.ActiveTab->View.File.has_value())
						  << " active-match=" << (desiredFile && current.ActiveTab && current.ActiveTab->View.File == desiredFile)
						  << " comparison-pending=" << current.ComparisonPending
						  << " tab-pending=" << current.TabPending
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
		expect(access::verify_pending_first_file_choice_invalidation(*application),
			   "only exact authority invalidation retires a pending first-file choice, with no empty tab");
		const auto posts = [&] {
			std::scoped_lock lock{ server->Mutex };
			return server->ReviewPosts.size();
		};
		// [[BPR-1715]]. The owner's course: a review with a comment, a
		// push, a refresh, another comment, publish. The push freezes the
		// review's summary, which takes no comment of the new revision, so its
		// Discard deletes the summary only; the comment written before the push
		// stays saved; the new review carries the new comment; the GitHub
		// CLI's 422 is a definite refusal that releases it; publishing again
		// sends it once.
		shutdown(*application);
		application.reset();
		application = make(app::detail::github_epoch_fixture_phase::seed);
		open(*application, cli);
		{
			auto&      course    = application->window();
			auto&      composer  = *course.review_submission_editor_view();
			const auto before    = posts();
			const auto firstHead = access::accepted_head(*application);
			const auto earlier   = write_pending(*application, 29U, QStringLiteral("issue: Written before the push."));
			course.open_review_composition();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->Comments.size() == 1U && composer.shown_draft()->Comments.front().Draft == earlier.Id && composer.submit_action()->isEnabled(); }, "the review carries the comment written before the push");
			composer.body_editor()->setPlainText(QStringLiteral("Summary written before the push."));
			wait([&] { return composer.shown_draft()->Body == "Summary written before the push."; }, "the summary is kept");
			course.github_refresh_action()->trigger();
			wait([&] { return access::accepted_head(*application) != firstHead && composer.status_label()->text().contains(QStringLiteral("revision changed")) && composer.discard_action()->isEnabled(); }, "the push freezes the review");
			expect(composer.shown_draft()->Comments.empty() && !composer.submit_action()->isEnabled() && composer.discard_action()->text() == QStringLiteral("Discard")
					   && composer.status_label()->text().contains(QStringLiteral("never a comment")),
				   "a frozen review carries no comment, so its Discard is the summary's alone, and it says so");
			const auto later = write_pending(*application, 30U, QStringLiteral("issue: Written after the push."));
			course.open_review_composition();
			QApplication::processEvents();
			expect(composer.shown_draft()->Body == "Summary written before the push." && composer.shown_draft()->Comments.empty() && !composer.submit_action()->isEnabled(),
				   "the frozen review takes no comment of the new revision");
			composer.discard_action()->trigger();
			QApplication::processEvents();
			expect(composer.discard_confirmation() != nullptr && composer.discard_confirmation()->question() == QStringLiteral("Discard your review summary?"), "Discard asks about the summary only");
			composer.discard_confirmation()->findChild<QAbstractButton*>(QStringLiteral("discardReviewConfirmationConfirm"))->click();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->Body.empty() && composer.shown_draft()->Comments.size() == 1U && composer.shown_draft()->Comments.front().Draft == later.Id && composer.submit_action()->isEnabled(); },
				 "a new review on the current revision carries the comment written after the push");
			expect(saved_exactly(course, earlier.Id, earlier.Body) && saved_exactly(course, later.Id, later.Body), "Discard deleted no comment: both are saved with their text");
			course.open_review_composition();
			QApplication::processEvents();
			{
				std::scoped_lock lock{ server->Mutex };
				server->RefuseNextReviewAsAnswer = true;
			}
			composer.submit_action()->trigger();
			wait([&] { return posts() == before + 1U && composer.shown_draft()->State == state::drafting && composer.submit_action()->isEnabled() && composer.shown_draft()->Comments.size() == 1U; },
				 "the GitHub CLI's 422 is a definite refusal: not published, and the comment is released for another try");
			expect(composer.status_label()->text().startsWith(QStringLiteral("Not published: GitHub refused this review because one of its comments is on a line GitHub can't place"))
					   && !composer.reconcile_action()->isEnabled(),
				   "the page says GitHub refused it for a line it can't place, with nothing to check");
			composer.submit_action()->trigger();
			submitted(composer);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->ReviewPosts.size() == before + 2U && server->ReviewPosts.back().find("Written after the push.") != std::string::npos
						   && server->ReviewPosts.back().find("Written before the push.") == std::string::npos,
					   "publishing again sends the new revision's comment once, and nothing else");
			}
			expect(saved_exactly(course, earlier.Id, earlier.Body), "the comment written before the push is still saved with its text");
		}
		// [[BPR-1715]]. The owner's stuck attempt, exactly: nine pending
		// comments, a review an application before answers were kept sent,
		// the GitHub CLI's 422 left unknown, and its comments held. After the
		// upgrade the restored attempt is checked, settled as not published,
		// and all nine come back unlocked with their text; publishing sends
		// them once.
		shutdown(*application);
		application.reset();
		application = make();
		open(*application, cli);
		std::size_t stuck  = 0U;
		auto        bodies = std::vector<std::string>{ "issue: Written before the push." };
		{
			auto& course   = application->window();
			auto& composer = *course.review_submission_editor_view();
			// The comment the first course wrote before its push is pending on
			// this revision again, so the review carries it with eight more.
			for (auto line = 31U; line <= 38U; ++line) {
				bodies.push_back("issue: Held comment " + std::to_string(line) + " of the refused review.\nIts second line.");
				static_cast<void>(write_pending(*application, line, QString::fromStdString(bodies.back())));
			}
			course.open_review_composition();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->Comments.size() == 9U && composer.submit_action()->isEnabled(); }, "the review carries its nine comments");
			{
				std::scoped_lock lock{ server->Mutex };
				server->DropNextReview = true;
			}
			composer.submit_action()->trigger();
			wait([&] { return composer.reconcile_action()->isEnabled() && composer.shown_draft()->State == state::submitting; }, "the attempt is left unknown, its nine comments held");
			const auto rows = composer.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"));
			expect(rows.size() == 9 && std::ranges::none_of(rows, [](const QToolButton* aButton) { return aButton->isVisible() || aButton->isEnabled(); }), "none of the nine held rows offers Delete");
			stuck = posts();
		}
		shutdown(*application);
		application.reset();
		{
			// The journal as version 6 kept it: no answer for any attempt.
			const auto journal  = *dataRoot / "state" / "v1" / "review-mutation-recovery.sqlite3";
			::sqlite3* database = nullptr;
			expect(std::filesystem::exists(journal) && ::sqlite3_open_v2(journal.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the recovery journal opens");
			const auto downgraded = ::sqlite3_exec(database, "DROP TABLE off_hunk_fact; DROP TABLE publication_post; DROP TABLE submission_post; PRAGMA user_version=6; UPDATE recovery_meta SET schema_format=6;", nullptr, nullptr, nullptr);
			static_cast<void>(::sqlite3_close_v2(database));
			expect(downgraded == SQLITE_OK, "the journal is the version an earlier application wrote");
		}
		application = make();
		open(*application, cli);
		{
			auto&      course         = application->window();
			auto&      composer       = *course.review_submission_editor_view();
			const auto carriedExactly = [&] {
				const auto& shown = composer.shown_draft();
				if (!shown || shown->State != state::drafting || shown->Comments.size() != bodies.size()) {
					return false;
				}
				auto carried = std::vector<std::string>{ };
				for (const auto& comment : shown->Comments) {
					carried.push_back(comment.Body);
				}
				auto expected = bodies;
				std::ranges::sort(carried);
				std::ranges::sort(expected);
				return carried == expected;
			};
			wait([&] { return carriedExactly() && composer.submit_action()->isEnabled(); }, "after the upgrade the restored attempt is settled and all nine comments come back unlocked, with their text");
			expect(posts() == stuck && composer.status_label()->text().contains(QStringLiteral("not published")), "settling it sent nothing and says it was not published");
			expect(std::ranges::all_of(composer.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete")), [](const QToolButton* aButton) { return aButton->isEnabled(); }),
				   "the nine comments are pending again: each can be deleted");
			// The owner's ninth comment is on a line outside every hunk, which
			// GitHub refuses again: the page says so and keeps all nine.
			{
				std::scoped_lock lock{ server->Mutex };
				server->RefuseNextReviewAsAnswer = true;
			}
			composer.submit_action()->trigger();
			wait([&] { return posts() == stuck + 1U && carriedExactly() && composer.submit_action()->isEnabled(); }, "GitHub's refusal keeps all nine comments, unlocked");
			expect(composer.status_label()->text().contains(QStringLiteral("a line GitHub can't place in the pull request's diff")) && composer.status_label()->text().contains(QStringLiteral("delete or edit that comment")),
				   "the page says why GitHub refused it and what to do");
			// Delete the off-hunk comment from its row, then publish the eight.
			const auto offHunk = bodies.back();
			bodies.pop_back();
			auto* const row = [&]() -> QToolButton* {
				const auto& carried = composer.shown_draft()->Comments;
				const auto  found   = std::ranges::find(carried, offHunk, &core::review_submission_comment::Body);
				const auto  rows    = composer.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"));
				expect(found != carried.end() && rows.size() == static_cast<qsizetype>(carried.size()), "each comment has its row");
				return rows.at(static_cast<qsizetype>(found - carried.begin()));
			}();
			expect(row->isEnabled(), "an unlocked comment's row offers Delete");
			row->click();
			QApplication::processEvents();
			expect(composer.delete_confirmation() != nullptr, "Delete asks first");
			composer.delete_confirmation()->findChild<QAbstractButton*>(QStringLiteral("deleteCommentConfirmationConfirm"))->click();
			wait([&] { return carriedExactly() && composer.submit_action()->isEnabled(); }, "the review carries the eight other comments");
			composer.submit_action()->trigger();
			submitted(composer);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->ReviewPosts.size() == stuck + 2U && server->ReviewPosts.back().find(offHunk) == std::string::npos, "publishing sends one review without the deleted comment");
				for (const auto& body : bodies) {
					const auto created = std::ranges::count_if(server->PreviousComments, [&body](const application::staged_review_comment& aComment) { return aComment.Body == body; });
					expect(created == 1, "GitHub holds each of the eight comments once");
				}
				expect(std::ranges::none_of(server->PreviousComments, [&offHunk](const application::staged_review_comment& aComment) { return aComment.Body == offHunk; }), "and not the deleted one");
			}
		}
		// [[BPR-1735]] (owner, 2026-10-22). The owner's review on GHES 3.20.8:
		// nine comments, one on a line outside every hunk, written by a build
		// that placed nothing on the Whole PR diff, and refused
		// by GitHub in the GitHub CLI's shape. This build holds that comment,
		// publishes it on its own as a file comment, then submits the other
		// eight as one review.
		shutdown(*application);
		application.reset();
		application = make();
		access::set_whole_diff_placement_enabled(*application, false);
		open(*application, cli);
		auto owner     = std::vector<std::string>{ };
		auto ownerPost = std::size_t{ 0U };
		// Head line 86 (zero-based 85) lies between the hunks ending at 68 and
		// starting at 113: more than three lines from any change.
		const auto offHunkLine = 85U;
		const auto offHunkBody = std::string{ "issue: The owner's comment outside every hunk.\nIt is kept word for word." };
		{
			auto& course   = application->window();
			auto& composer = *course.review_submission_editor_view();
			// Head lines 46 to 51 and 58 and 59, in the second and third hunks.
			for (const auto line : { 45U, 46U, 47U, 48U, 49U, 50U, 57U, 58U }) {
				owner.push_back("issue: Owner comment " + std::to_string(line) + " in a hunk.");
				static_cast<void>(write_pending(*application, line, QString::fromStdString(owner.back())));
			}
			static_cast<void>(write_pending(*application, offHunkLine, QString::fromStdString(offHunkBody)));
			course.open_review_composition();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->Comments.size() == 9U && composer.submit_action()->isEnabled(); }, "the owner's review carries its nine comments, nothing held");
			{
				std::scoped_lock lock{ server->Mutex };
				server->RefuseNextReviewAsAnswer = true;
				ownerPost                        = server->ReviewPosts.size();
			}
			composer.submit_action()->trigger();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->State == state::drafting && composer.shown_draft()->Comments.size() == 9U && composer.submit_action()->isEnabled(); },
				 "GitHub's refusal, in the GitHub CLI's shape, is settled and keeps all nine");
			expect(composer.status_label()->text().contains(QStringLiteral("a line GitHub can't place")), "the page says a comment is on a line GitHub can't place");
		}
		shutdown(*application);
		application.reset();
		application = make();
		open(*application, cli);
		{
			auto&      course   = application->window();
			auto&      composer = *course.review_submission_editor_view();
			const auto before   = access::review_mutation_evidence(*application);
			course.open_review_composition();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->Comments.size() == 9U && !composer.held_comments().empty(); }, "this build reads GitHub's diff and holds the off-hunk comment");
			const auto& carried = composer.shown_draft()->Comments;
			const auto  held    = std::ranges::find(carried, offHunkBody, &core::review_submission_comment::Body);
			expect(held != carried.end() && composer.held_comments().size() == 1U && composer.held_comments().contains(held->Draft), "exactly the off-hunk comment is held");
			const auto rows    = composer.pending_list()->findChildren<QLabel*>(QStringLiteral("wholeReviewPendingHeld"));
			const auto actions = composer.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingConvert"));
			const auto shown   = [](const auto& aWidgets) { return std::ranges::count_if(aWidgets, [](const QWidget* aWidget) { return aWidget->isVisible(); }); };
			expect(shown(rows) == 1 && shown(actions) == 1, "its row alone says why and offers an action");
			auto* const publish = *std::ranges::find_if(actions, [](const QToolButton* aButton) { return aButton->isVisible(); });
			expect(publish->text() == QStringLiteral("Publish as file comment"), "the action is Publish as file comment");
			expect(!composer.disposition_button(core::review_disposition::comment)->isEnabled(), "Submit is blocked while it is held");
			std::size_t blocked = 0U;
			{
				std::scoped_lock lock{ server->Mutex };
				blocked = server->ReviewPosts.size();
			}
			composer.submit_action()->trigger();
			QApplication::processEvents();
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->ReviewPosts.size() == blocked, "Submit by its key sends nothing while it is held");
			}
			publish->click();
			wait([&] { return access::review_mutation_evidence(*application).FilePosts == before.FilePosts + 1U && composer.shown_draft() && composer.shown_draft()->Comments.size() == 8U; },
				 "Publish as file comment publishes it on its own and takes it out of the review");
			const auto after = access::review_mutation_evidence(*application);
			// [[BPR-1736]] (D2 rejected). Nothing was learned yet: one native
			// attempt, refused in GitHub.com's shape, and its file comment
			// followed at once.
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->OffHunkNativePosts == 1U, "one native attempt at the line outside the diff");
			}
			expect(after.NativePosts == before.NativePosts + 1U && after.LastFile && after.LastPath == "review.cpp" && after.LastCommit == access::accepted_head(*application),
				   "refused, then one file comment at the head");
			expect(after.LastBody == "https://github.com/octo-org/review-repository/blob/" + access::accepted_head(*application) + "/review.cpp#L86\n\n" + offHunkBody,
				   "its body is the permalink to head line 86 and the comment word for word");
			wait([&] { return composer.held_comments().empty() && composer.disposition_button(core::review_disposition::comment)->isEnabled(); }, "with nothing held Submit is allowed");
			composer.submit_action()->trigger();
			submitted(composer);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->ReviewPosts.size() == ownerPost + 2U && server->ReviewPosts.back().find("outside every hunk") == std::string::npos, "one review is posted, without the file comment's text");
				for (const auto& body : owner) {
					expect(std::ranges::count_if(server->PreviousComments, [&body](const application::staged_review_comment& aComment) { return aComment.Body == body; }) == 1, "GitHub holds each of the eight once");
				}
			}
			expect(access::review_mutation_evidence(*application).FilePosts == before.FilePosts + 1U, "and the file comment was published once");
		}
		// [[BPR-1736]] (owner, 2026-10-22). The fact that refusal taught, and
		// what follows from it: the next line outside the diff is a file comment
		// with no native attempt; a review carries such a line only once GitHub
		// accepted one, and when GitHub then refuses that review the fact drops
		// to unsupported and the page holds the comment.
		{
			const auto journal = *dataRoot / "state" / "v1" / "review-mutation-recovery.sqlite3";
			const auto facts   = [&journal](const std::string& aSql) {
				::sqlite3* database = nullptr;
				expect(::sqlite3_open_v2(journal.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "the recovery journal opens");
				auto rows = std::vector<std::string>{ };
				auto row  = [](void* aRows, const int aColumns, char** aValues, char**) {
					auto text = std::string{ };
					for (auto column = 0; column < aColumns; ++column) {
						text += (column == 0 ? "" : ",") + std::string{ aValues[column] != nullptr ? aValues[column] : "null" };
					}
					static_cast<std::vector<std::string>*>(aRows)->push_back(std::move(text));
					return 0;
				};
				expect(::sqlite3_exec(database, aSql.c_str(), row, &rows, nullptr) == SQLITE_OK, "the learned facts are read");
				static_cast<void>(::sqlite3_close_v2(database));
				return rows;
			};
			expect(facts("SELECT operation,state,source,product,server_version,line FROM off_hunk_fact") == std::vector<std::string>{ "0,2,2,1,,85" },
				   "the refusal taught unsupported for a single comment on GitHub.com, from head line 86");
			auto& course = application->window();
			course.close_review_sheet();
			course.choose_modality(qt::review_modality::review);
			wait([&] { return access::reveal_fixture_file(*application, "review.cpp") == app::detail::fixture_file_reveal_result::shown; }, "review.cpp is shown");
			wait([&] { return course.comparison_surface()->select_conversation_line(core::diff_side::right, 95U); }, "head line 96, outside every hunk, is selected");
			auto* const drawer = course.review_editor();
			wait([&] { return drawer->target_notice_text() == QStringLiteral("GitHub can't place a comment on this line. It will be posted as a file comment that links to the line."); },
				 "the editor says what was learned before Publish");
			wait([&] { return course.comment_action()->isEnabled(); }, "Comment is offered");
			course.comment_action()->trigger();
			const auto body = std::string{ "note: Learned outside the diff." };
			drawer->body_editor()->setPlainText(QString::fromStdString(body));
			wait([&] { return drawer->shown_draft() && drawer->shown_draft()->Body == body && !drawer->rejected_candidate(); }, "the comment is kept locally");
			const auto before = access::review_mutation_evidence(*application);
			expect(drawer->publish_action()->isEnabled(), "Publish is offered");
			drawer->publish_action()->trigger();
			wait([&] { return access::review_mutation_evidence(*application).FilePosts == before.FilePosts + 1U; }, "Publish posts it");
			const auto after = access::review_mutation_evidence(*application);
			{
				std::scoped_lock lock{ server->Mutex };
				expect(server->OffHunkNativePosts == 1U && after.NativePosts == before.NativePosts && after.LastFile
						   && after.LastBody == "https://github.com/octo-org/review-repository/blob/" + access::accepted_head(*application) + "/review.cpp#L96\n\n" + body,
					   "as a file comment linking to the line, with no native attempt");
			}

			// Say GitHub had accepted such a comment: a review now carries one.
			static_cast<void>(facts("UPDATE off_hunk_fact SET state=1,source=1,observed_utc_ms=CAST((julianday('now') - 2440587.5) * 86400000 AS INTEGER)"));
			const auto carriedBody = std::string{ "issue: Carried outside the diff." };
			static_cast<void>(write_pending(*application, 100U, QString::fromStdString(carriedBody)));
			auto& composer = *course.review_submission_editor_view();
			course.open_review_composition();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->Comments.size() == 1U && composer.held_comments().empty() && composer.submit_action()->isEnabled(); },
				 "with the fact supported the review carries the line outside the diff, nothing held");
			{
				std::scoped_lock lock{ server->Mutex };
				server->RefuseNextReviewAsAnswer = true;
			}
			composer.submit_action()->trigger();
			wait([&] { return composer.shown_draft() && composer.shown_draft()->State == state::drafting && composer.held_comments().size() == 1U; },
				 "GitHub refuses the review for an unplaceable line: settled as not published, and the comment is held");
			expect(facts("SELECT operation,state,source FROM off_hunk_fact ORDER BY operation") == std::vector<std::string>{ "0,2,2", "1,2,2" },
				   "the fact drops to unsupported for both operations");
			{
				std::scoped_lock lock{ server->Mutex };
				expect(std::ranges::none_of(server->PreviousComments, [&carriedBody](const application::staged_review_comment& aComment) { return aComment.Body == carriedBody; }),
					   "and nothing was published; nothing is resent");
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
