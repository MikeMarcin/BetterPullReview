#include "bpr/qt/review_submission_editor.hpp"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QToolButton>
#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}
auto main(int argc, char** argv) -> int
{
	QApplication                                     application{ argc, argv };
	std::vector<qt::review_submission_editor_intent> intents;
	qt::review_submission_editor                     editor{ [&](auto aIntent) { intents.push_back(std::move(aIntent)); } };
	const auto                                       head       = core::parse_git_commit_oid("abcdef0123456789abcdef0123456789abcdef01");
	const auto                                       draft      = core::review_submission_draft{ .Id            = core::review_submission_draft_id{ 1U },
																								 .Target        = { .Host = core::git_host_id{ 1U }, .Repository = core::repository_id{ 2U }, .PullRequest = core::pull_request_id{ 3U }, .ExpectedHead = head.value() },
																								 .ActingAccount = core::github_account_id{ 4U },
																								 .Body          = "Literal <summary>" };
	auto                                             projection = qt::review_submission_editor_projection{ .Draft = draft, .Controls = core::controls(draft), .TargetDescription = QStringLiteral("github.com · owner/repository#17\nHead: abcdef0123456789abcdef0123456789abcdef01\nSubmitting as reviewer (account 4)"), .Status = QStringLiteral("Ready <literal>") };
	editor.resize(300, 650);
	editor.show();
	QApplication::processEvents();
	expect(editor.target_editor()->isHidden() && editor.body_editor()->isHidden(), "no-draft page hides empty identity and input panels");
	expect(editor.findChild<QLabel*>(QStringLiteral("wholeReviewPurpose"))->text().contains(QStringLiteral("Nothing is published")), "page purpose and publication gesture explained");
	for (auto* action : { editor.submit_action(), editor.discard_action(), editor.reconcile_action(), editor.new_review_action() }) {
		expect(!action->isEnabled() && !action->toolTip().isEmpty(), "unavailable no-draft action explains why");
	}
	editor.present(projection);
	expect(editor.findChild<QLabel*>(QStringLiteral("wholeReviewDraftState"))->text().contains(QStringLiteral("kept")), "kept draft identified explicitly");
	editor.resize(300, 700);
	editor.show();
	QApplication::processEvents();
	expect(intents.empty(), "projection emits no gesture");
	expect(editor.body_editor()->height() >= 160 && editor.body_editor()->maximumHeight() == QWIDGETSIZE_MAX,
		   "summary keeps a usable viewport and can grow with its dedicated page");
	expect(editor.target_editor()->isReadOnly() && !editor.target_editor()->acceptRichText()
			   && editor.target_editor()->wordWrapMode() == QTextOption::WrapAtWordBoundaryOrAnywhere
			   && editor.target_editor()->toPlainText() == projection.TargetDescription,
		   "full identity wraps without modifying copied text");
	expect(editor.body_editor()->accessibleName() == QStringLiteral("Review summary")
			   && editor.disposition_button(core::review_disposition::comment)->accessibleName() == QStringLiteral("Comment")
			   && editor.disposition_button(core::review_disposition::approve)->accessibleName() == QStringLiteral("Approve")
			   && editor.disposition_button(core::review_disposition::request_changes)->accessibleName() == QStringLiteral("Request changes")
			   && editor.preflight_button()->accessibleName() == QStringLiteral("Your blocking feedback"),
		   "named accessible inputs");
	// [[BPR-1085]]. Three explicit decisions, the draft's one checked.
	expect(editor.disposition_button(core::review_disposition::comment)->isChecked() && !editor.disposition_button(core::review_disposition::approve)->isChecked()
			   && editor.disposition_button(core::review_disposition::approve)->isCheckable(),
		   "the draft's decision is the one checked");
	expect(editor.preflight_button()->isVisible() && editor.preflight_button()->toolTip() == QStringLiteral("Your feedback on this revision has not been read: unknown, not none."),
		   "an unread preflight says unknown, never none");
	expect(editor.status_label()->textFormat() == Qt::PlainText && editor.status_label()->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard), "outcomes are literal and keyboard copyable");
	editor.body_editor()->setFocus();
	editor.body_editor()->moveCursor(QTextCursor::End);
	const auto cursor = editor.body_editor()->textCursor().position();
	editor.present(projection);
	expect(editor.body_editor()->hasFocus() && editor.body_editor()->textCursor().position() == cursor, "unchanged projection preserves focus and cursor");
	editor.body_editor()->setPlainText(QStringLiteral("Edited summary"));
	expect(intents.size() == 1U && intents.back().Command.Kind == core::review_submission_command_kind::edit && intents.back().Command.Body == "Edited summary", "summary emits exact semantic edit");
	expect(editor.shown_draft() == draft, "editing does not optimistically replace decided draft");
	editor.disposition_button(core::review_disposition::approve)->click();
	expect(intents.back().Command.Kind == core::review_submission_command_kind::choose_disposition && intents.back().Command.Disposition == core::review_disposition::approve, "choice emits typed disposition");
	editor.disposition_button(core::review_disposition::request_changes)->click();
	expect(intents.back().Command.Disposition == core::review_disposition::request_changes, "each decision button emits its own native disposition");

	// [[BPR-1085]]. The preflight: my blockers, my unsent blocking draft,
	// guidance, uncertainty and others, apart; rows link to their items.
	auto approved          = draft;
	approved.Disposition   = core::review_disposition::approve;
	auto withPreflight     = projection;
	withPreflight.Draft    = approved;
	withPreflight.Controls = core::controls(approved);
	withPreflight.Prepared = core::prepare_review_decision(approved, { });
	// Filled member by member: GCC 15 at -O3 reads a designated temporary as
	// leaving the defaulted scope's strings uninitialized ([[BPR-1297]] run).
	auto preflight                        = core::review_preflight{ };
	preflight.Blockers                    = { { .Conversation = "review-thread:T1", .Label = core::feedback_label::issue, .Resolution = core::feedback_resolution::unresolved },
											  { .Conversation = "review-thread:T2", .Label = core::feedback_label::todo, .Resolution = core::feedback_resolution::unknown } };
	preflight.Pending                     = { { .Draft = "draft:9", .Label = core::feedback_label::issue } };
	preflight.Guidance                    = { { .Label = core::feedback_label::chore, .Guidance = core::feedback_label_guidance::required_before_acceptance, .Published = 1U } };
	preflight.Uncertain.ResolutionUnknown = 1U;
	preflight.Others                      = 2U;
	withPreflight.Preflight               = std::move(preflight);
	withPreflight.References              = { { "review-thread:T1", { QStringLiteral("src/a.cpp:12"), QStringLiteral("The cache is never invalidated.") } },
											  { "review-thread:T2", { QStringLiteral("src/b.cpp:3"), QStringLiteral("Add the clamp.") } },
											  { "draft:9", { QStringLiteral("Line 40"), QStringLiteral("Not sent yet.") } } };
	auto revealed                         = std::vector<std::string>{ };
	editor.set_item_reveal([&](const std::string& aId) { revealed.push_back(aId); });
	const auto beforePreflight = intents.size();
	editor.present(withPreflight);
	expect(intents.size() == beforePreflight, "presenting the preflight emits no gesture and no decision");
	expect(editor.disposition_button(core::review_disposition::approve)->isChecked(), "Approve stays the reviewer's explicit choice while blocking feedback is open");
	const auto chipWords = QStringLiteral("Yours on GitHub: 2 open blocking\nYours, not sent: 1 blocking\n1 chore, not marked blocking\n1 of yours: GitHub didn't say if resolved\nOthers: 2 open blocking (not yours)");
	expect(editor.preflight_button()->toolTip() == chipWords && editor.preflight_button()->accessibleDescription() == chipWords, "the chip's tooltip and accessible description name each group");
	auto* approve = editor.disposition_button(core::review_disposition::approve);
	expect(!approve->icon().isNull() && approve->toolTip() == QStringLiteral("Approve. You have 3 open blocking comment(s) of your own; approving does not resolve them.")
			   && approve->accessibleDescription() == approve->toolTip() && approve->isEnabled(),
		   "Approve carries the amber dot and says approving does not resolve my blockers, and stays available");
	// The dot is the theme's attention amber (`state_attention`, Appearance >
	// Colours), never a fixed colour: the built-in value until the window
	// supplies the theme's, and then exactly the theme's.
	const auto dot = [approve] {
		const auto image = approve->icon().pixmap(QSize{ 16, 16 }).toImage();
		return image.pixelColor(image.width() / 2, image.height() / 2);
	};
	expect(dot() == editor.attention_colour() && editor.attention_colour().isValid(), "the dot is drawn in the attention amber");
	editor.set_attention_colour(QColor{ 0x12, 0x34, 0x56 });
	expect(dot() == QColor{ 0x12, 0x34, 0x56 }, "a theme's own attention amber repaints the dot");
	auto rows = QStringList{ };
	for (auto* action : editor.preflight_menu()->actions()) {
		rows.push_back(action->isSeparator() ? (action->text().isEmpty() ? QStringLiteral("--") : QStringLiteral("## ") + action->text())
											 : action->text() + (action->isEnabled() ? QString{ } : QStringLiteral(" [info]")));
	}
	const auto expectedRows = QStringList{ QStringLiteral("## Yours, on GitHub"), QStringLiteral("src/a.cpp:12 · The cache is never invalidated."), QStringLiteral("src/b.cpp:3 · Add the clamp. · state unknown"),
										   QStringLiteral("## Yours, not sent"), QStringLiteral("Line 40 · Not sent yet."), QStringLiteral("--"),
										   QStringLiteral("1 chore, not marked blocking [info]"), QStringLiteral("1 of yours: GitHub didn't say if resolved [info]"), QStringLiteral("Others: 2 open blocking (not yours) [info]") };
	if (rows != expectedRows) {
		std::cerr << rows.join(QStringLiteral(" | ")).toStdString() << '\n';
	}
	expect(rows == expectedRows, "the list keeps mine, my unsent drafts, guidance, uncertainty and others apart");
	editor.preflight_menu()->actions().at(2)->trigger();
	editor.preflight_menu()->actions().at(4)->trigger();
	expect(revealed == std::vector<std::string>{ "review-thread:T2", "draft:9" } && intents.size() == beforePreflight, "a row reveals its exact item and nothing else");
	auto none      = withPreflight;
	none.Preflight = core::review_preflight{ .Exact = true };
	editor.present(none);
	expect(editor.preflight_button()->toolTip() == QStringLiteral("Yours on GitHub: 0 open blocking") && approve->icon().isNull()
			   && approve->toolTip() == QStringLiteral("Approve this pull request."),
		   "an exact zero is said as zero, and Approve carries no dot");
	editor.present(withPreflight);
	QApplication::processEvents();
	for (auto* button : { editor.disposition_button(core::review_disposition::approve), editor.preflight_button() }) {
		expect(button->width() >= button->sizeHint().width() && editor.rect().contains(button->geometry()), "the dotted Approve and the chip keep their full width in a narrow editor");
	}
	if (argc == 2) {
		expect(editor.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-preflight.png")), "preflight render saved");
	}
	editor.submit_action()->trigger();
	expect(intents.back().Command.Kind == core::review_submission_command_kind::begin_submit && intents.back().Prepared == withPreflight.Prepared,
		   "Submit carries the decision the editor showed");
	editor.present(projection);
	auto* submit = static_cast<QToolButton*>(nullptr);
	for (auto* button : editor.findChildren<QToolButton*>()) {
		expect(button->isVisible() && editor.rect().contains(button->geometry()) && button->focusPolicy() == Qt::StrongFocus, "all buttons fit narrow editor and support focus");
		if (button->defaultAction() == editor.submit_action()) {
			submit = button;
		}
	}
	expect(submit != nullptr, "submit has a real button");
	submit->setFocus();
	QKeyEvent press{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
	QKeyEvent release{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(submit, &press);
	QApplication::sendEvent(submit, &release);
	expect(intents.back().Command.Kind == core::review_submission_command_kind::begin_submit && intents.back().Command.Id == draft.Id, "keyboard button press emits exact submit intent");
	expect(editor.shown_draft() == draft, "submit awaits decided state");
	projection.Draft    = core::apply(draft, { .Id = draft.Id, .Kind = core::review_submission_command_kind::begin_submit }).Draft;
	projection.Controls = core::controls(*projection.Draft);
	projection.Status   = QStringLiteral("Review status is unknown. Check status.");
	editor.present(projection);
	expect(editor.body_editor()->isReadOnly() && !editor.submit_action()->isEnabled() && !editor.discard_action()->isEnabled() && editor.reconcile_action()->isEnabled(), "pending projection exposes recovery only");
	editor.reconcile_action()->trigger();
	expect(intents.back().Operation == qt::review_submission_editor_operation::reconcile, "recovery is a distinct gesture");
	if (argc == 2) {
		expect(editor.grab().save(QString::fromLocal8Bit(argv[1])), "render saved");
	}

	// [[BPR-1345]]. Finish review: the review carries pending drafts, listed
	// each where it is and what it says; Comment needs no summary; Discard is
	// Discard review; what Submit sends is said in words.
	auto finishing      = draft;
	finishing.Body      = { };
	auto first          = core::review_submission_comment{ };
	first.Draft         = core::review_draft_id{ 7U };
	first.DraftRevision = 2U;
	first.Path          = "src/engine/renderer.cpp";
	first.Line          = 430U;
	first.Body          = "question: Where does 0.01 come from?\nIt looks arbitrary.";
	auto range          = first;
	range.Draft         = core::review_draft_id{ 8U };
	range.Side          = core::diff_side::left;
	range.Line          = 434U;
	range.StartLine     = 433U;
	range.Body          = "nit: scale could be constexpr.";
	finishing.Comments  = { first, range };
	auto finish         = projection;
	finish.Draft        = finishing;
	finish.Controls     = core::controls(finishing);
	finish.Status       = QStringLiteral("Choose Comment, Approve, or Request changes.");
	finish.Prepared     = core::prepare_review_decision(finishing, { });
	editor.present(finish);
	QApplication::processEvents();
	expect(editor.title_label()->text() == QStringLiteral("Finish your review") && editor.pending_list()->isVisible()
			   && editor.pending_rows() == QStringList{ QStringLiteral("renderer.cpp, line 431: question: Where does 0.01 come from?"), QStringLiteral("renderer.cpp, base lines 434–435: nit: scale could be constexpr.") },
		   "Finish review lists the pending comments, each where it is and its first line");
	expect(editor.carries_label()->isVisible() && editor.carries_label()->text() == QStringLiteral("Submits 2 line comments as one review.")
			   && editor.body_editor()->placeholderText() == QStringLiteral("Leave a summary (optional)"),
		   "it says what Submit sends, and the summary is optional");
	expect(editor.submit_action()->isEnabled() && editor.discard_action()->text() == QStringLiteral("Discard review")
			   && editor.disposition_button(core::review_disposition::comment)->toolTip().contains(QStringLiteral("optional")),
		   "Comment with no summary can be submitted, and Discard is Discard review");
	auto changes            = finishing;
	changes.Disposition     = core::review_disposition::request_changes;
	auto requestChanges     = finish;
	requestChanges.Draft    = changes;
	requestChanges.Controls = core::controls(changes);
	editor.present(requestChanges);
	expect(!editor.submit_action()->isEnabled() && editor.submit_action()->toolTip().startsWith(QStringLiteral("Request changes needs a summary"))
			   && editor.body_editor()->placeholderText() == QStringLiteral("Write a review summary…"),
		   "Request changes still needs a summary, and says so");
	editor.present(finish);
	if (argc == 2) {
		expect(editor.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-finish.png")), "finish render saved");
	}
	for (auto* row : editor.pending_list()->findChildren<QLabel*>(QStringLiteral("wholeReviewPendingRow"))) {
		expect(editor.rect().contains(row->geometry().translated(editor.pending_list()->pos())) && !row->toolTip().isEmpty(), "each row fits the page and its tooltip names the whole comment");
	}
	editor.present(projection);
	expect(editor.title_label()->text() == QStringLiteral("Compose review") && !editor.pending_list()->isVisible() && !editor.carries_label()->isVisible()
			   && editor.discard_action()->text() == QStringLiteral("Discard"),
		   "a review with no pending comment is Compose review as before");
	std::cout << "review-submission-editor:pass scale=" << editor.devicePixelRatioF() << '\n';
	return EXIT_SUCCESS;
}
