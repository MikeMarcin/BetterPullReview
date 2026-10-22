#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_submission_editor.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <span>
#include <string>
#include <string_view>

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
	// [[BPR-1541]] v002 comparison render: the unknown-status block in Light and Dark.
	if (const auto look = qEnvironmentVariable("BPR_UNKNOWN_STATE_RENDER"); !look.isEmpty()) {
		qt::review_submission_editor lookEditor{ [](auto) { } };
		auto                         held = draft;
		held.State                        = core::review_submission_state::submitting;
		held.Attempt                      = core::review_submission_attempt_id{ 1U };
		auto lookProjection               = qt::review_submission_editor_projection{ .Draft = held, .Controls = core::controls(held), .TargetDescription = projection.TargetDescription, .Status = QStringLiteral("Review status is unknown. Check status to reconcile this attempt; do not submit it again.") };
		lookProjection.Unknown            = core::notice(core::review_submission_unknown_diagnosis{ .Reason = core::review_submission_unknown_reason::network });
		lookEditor.resize(360, 620);
		lookEditor.show();
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, "light" }, std::pair{ core::appearance_mode::dark, "dark" } }) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(mode);
			lookEditor.setPalette(profile.derive(QApplication::palette()));
			lookEditor.present(lookProjection);
			for (auto turn = 0; turn < 4; ++turn) {
				QApplication::processEvents();
			}
			auto canvas = QPixmap{ lookEditor.size() * lookEditor.devicePixelRatioF() };
			canvas.setDevicePixelRatio(lookEditor.devicePixelRatioF());
			canvas.fill(lookEditor.palette().color(QPalette::Window));
			lookEditor.render(&canvas);
			expect(canvas.save(look + QStringLiteral("-") + QString::fromLatin1(name) + QStringLiteral(".png")), "comparison render saved");
		}
		return EXIT_SUCCESS;
	}
	// [[BPR-1605]] comparison render: Finish your review carrying one pending
	// blocking comment of mine, with none on GitHub ("pending"), and with
	// one on GitHub as well ("mixed"), in Light and Dark. "as-read" is the
	// preflight the composition handed over before BPR-1605, which did not
	// read the review's own comments.
	if (const auto look = qEnvironmentVariable("BPR_BLOCKING_CHIP_RENDER"); !look.isEmpty()) {
		qt::review_submission_editor lookEditor{ [](auto) { } };
		auto                         carrying = draft;
		carrying.Body                         = { };
		auto blocking                         = core::review_submission_comment{ };
		blocking.Draft                        = core::review_draft_id{ 7U };
		blocking.Path                         = "src/engine/renderer.cpp";
		blocking.Line                         = 430U;
		blocking.DraftRevision                = 1U;
		blocking.Body                         = "issue (blocking): The cache is never invalidated.";
		auto nit                              = blocking;
		nit.Draft                             = core::review_draft_id{ 8U };
		nit.Line                              = 433U;
		nit.Body                              = "nit: scale could be constexpr.";
		carrying.Comments                     = { blocking, nit };
		carrying.Disposition                  = core::review_disposition::approve;
		lookEditor.resize(380, 560);
		lookEditor.show();
		for (const auto* state : { "as-read", "pending", "mixed" }) {
			auto preflight  = core::review_preflight{ };
			preflight.Exact = true;
			if (std::string_view{ state } != "as-read") {
				preflight.Pending = { { .Draft = "draft:7", .Label = core::feedback_label::issue } };
			}
			if (std::string_view{ state } == "mixed") {
				preflight.Blockers = { { .Conversation = "review-thread:T1", .Label = core::feedback_label::issue, .Resolution = core::feedback_resolution::unresolved } };
			}
			auto lookProjection       = qt::review_submission_editor_projection{ .Draft = carrying, .Controls = core::controls(carrying), .TargetDescription = projection.TargetDescription };
			lookProjection.Prepared   = core::prepare_review_decision(carrying, { });
			lookProjection.Preflight  = std::move(preflight);
			lookProjection.References = { { "review-thread:T1", { QStringLiteral("src/cache.cpp:12"), QStringLiteral("Evict on resize.") } },
										  { "draft:7", { QStringLiteral("renderer.cpp, line 431"), QStringLiteral("The cache is never invalidated.") } } };
			for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, "light" }, std::pair{ core::appearance_mode::dark, "dark" } }) {
				auto profile = qt::appearance_profile{ };
				profile.select_mode(mode);
				lookEditor.setPalette(profile.derive(QApplication::palette()));
				lookEditor.set_attention_colour(profile.color(core::appearance_role::state_attention));
				lookEditor.present(lookProjection);
				for (auto turn = 0; turn < 4; ++turn) {
					QApplication::processEvents();
				}
				auto canvas = QPixmap{ lookEditor.size() * lookEditor.devicePixelRatioF() };
				canvas.setDevicePixelRatio(lookEditor.devicePixelRatioF());
				canvas.fill(lookEditor.palette().color(QPalette::Window));
				lookEditor.render(&canvas);
				expect(canvas.save(look + QStringLiteral("-%1-%2.png").arg(QString::fromLatin1(state), QString::fromLatin1(name))), "blocking chip render saved");
				std::cout << state << ' ' << name << ": " << lookEditor.preflight_button()->toolTip().replace(QLatin1Char{ '\n' }, QStringLiteral(" | ")).toStdString() << " || "
						  << lookEditor.disposition_button(core::review_disposition::approve)->toolTip().toStdString() << '\n';
			}
		}
		return EXIT_SUCCESS;
	}
	editor.resize(300, 650);
	editor.show();
	QApplication::processEvents();
	expect(editor.target_editor()->isHidden() && editor.body_editor()->isHidden(), "no-draft page hides empty identity and input panels");
	// [[BPR-1345]] v002. What is published is said once, briefly, and only
	// while a review is being written.
	expect(editor.note_label()->text() == QStringLiteral("Nothing is published until you submit.") && editor.note_label()->isHidden(), "with no draft the page does not say what is published");
	// [[BPR-1345]] v003. No New review: a finished review is never shown,
	// the next one starts at once.
	expect(editor.findChild<QToolButton*>(QStringLiteral("wholeReviewNewReview")) == nullptr, "there is no New review button");
	for (auto* action : { editor.submit_action(), editor.discard_action(), editor.reconcile_action() }) {
		expect(!action->isEnabled() && !action->toolTip().isEmpty(), "unavailable no-draft action explains why");
	}
	editor.present(projection);
	expect(editor.findChild<QLabel*>(QStringLiteral("wholeReviewDraftState"))->text().contains(QStringLiteral("kept")), "kept draft identified explicitly");
	// [[BPR-1345]] v002. The host, head and account wait behind a disclosure
	// that is closed by default (owner: "These details on finish review
	// should be hidden by default.").
	expect(!editor.details_toggle()->isChecked() && !editor.details_toggle()->isHidden() && editor.target_editor()->isHidden(),
		   "the target and account are behind a closed disclosure");
	editor.details_toggle()->click();
	expect(!editor.target_editor()->isHidden(), "opening the disclosure shows them");
	editor.details_toggle()->click();
	expect(editor.target_editor()->isHidden(), "closing it hides them again");
	editor.resize(300, 700);
	editor.show();
	QApplication::processEvents();
	expect(intents.empty(), "projection emits no gesture");
	// [[BPR-1590]] v002. The summary is as tall as what is written, from 120
	// pixels to 320, then it scrolls: no empty wall under a short summary.
	expect(editor.body_editor()->height() >= 120 && editor.body_editor()->height() <= 320,
		   "summary keeps a usable viewport sized to its words");
	{
		auto lines = QStringList{ };
		for (int line = 0; line < 40; ++line) {
			lines.push_back(QStringLiteral("A long summary line %1.").arg(line));
		}
		editor.body_editor()->setPlainText(lines.join(QLatin1Char{ '\n' }));
		QApplication::processEvents();
		expect(editor.body_editor()->height() == 320, "a long summary grows the field to 320 pixels, then scrolls");
		editor.body_editor()->setPlainText(QString{ });
		QApplication::processEvents();
		expect(editor.body_editor()->height() == 120, "an empty summary is 120 pixels");
		intents.clear();
	}
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
	// [[BPR-1085]], [[BPR-1345]] v002. Three verdicts, each a button that
	// submits; with a summary each can.
	expect(!editor.disposition_button(core::review_disposition::approve)->isCheckable()
			   && editor.disposition_button(core::review_disposition::comment)->isEnabled() && editor.disposition_button(core::review_disposition::approve)->isEnabled()
			   && editor.disposition_button(core::review_disposition::request_changes)->isEnabled(),
		   "the verdicts are buttons that submit, and with a summary each is offered");
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
	editor.choose_disposition(core::review_disposition::approve);
	expect(intents.back().Command.Kind == core::review_submission_command_kind::choose_disposition && intents.back().Command.Disposition == core::review_disposition::approve, "choice emits typed disposition");
	editor.choose_disposition(core::review_disposition::request_changes);
	expect(intents.back().Command.Disposition == core::review_disposition::request_changes, "each decision emits its own native disposition");

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
	expect(editor.shown_draft()->Disposition == core::review_disposition::approve && editor.disposition_button(core::review_disposition::approve)->isEnabled(),
		   "Approve stays the reviewer's explicit choice while blocking feedback is open");
	const auto chipWords = QStringLiteral("Yours on GitHub: 2 open blocking\nYours, not sent: 1 blocking\n1 chore, not marked blocking\n1 of yours: GitHub didn't say if resolved\nOthers: 2 open blocking (not yours)");
	expect(editor.preflight_button()->text().startsWith(QStringLiteral("3 (1 not sent)")), "the heading leads with the same combined count as Approve");
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
	expect(editor.preflight_button()->text() == QStringLiteral("0") && editor.preflight_button()->toolTip() == QStringLiteral("Yours on GitHub: 0 open blocking") && approve->icon().isNull()
			   && approve->toolTip() == QStringLiteral("Approve this pull request."),
		   "an exact zero is said as zero, and Approve carries no dot");
	auto pendingOnly               = none;
	pendingOnly.Preflight->Pending = { { .Draft = "draft:9", .Label = core::feedback_label::issue } };
	editor.present(pendingOnly);
	expect(editor.preflight_button()->text() == QStringLiteral("1 (not sent)") && !approve->icon().isNull() && approve->toolTip().contains(QStringLiteral("1 open blocking")),
		   "a pending-only blocker leads with one, says not sent, and agrees with Approve");
	auto mixed                = pendingOnly;
	mixed.Preflight->Blockers = { { .Conversation = "review-thread:T1", .Label = core::feedback_label::issue, .Resolution = core::feedback_resolution::unresolved } };
	editor.present(mixed);
	expect(editor.preflight_button()->text() == QStringLiteral("2 (1 not sent)") && approve->toolTip().contains(QStringLiteral("2 open blocking")),
		   "one published and one pending blocker lead with their total and explain its pending part");
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
	QApplication::processEvents();
	for (auto* button : editor.findChildren<QToolButton*>()) {
		if (button->isVisible()) {
			const auto fits = editor.rect().contains(button->geometry().translated(button->parentWidget()->mapTo(&editor, QPoint{ })));
			if (!fits || button->focusPolicy() != Qt::StrongFocus) {
				std::cerr << "EDITOR_BUTTON name=" << button->objectName().toStdString() << " fits=" << fits
						  << " focus=" << static_cast<int>(button->focusPolicy()) << '\n';
			}
			expect(fits && button->focusPolicy() == Qt::StrongFocus,
				   "every shown button fits the narrow editor and takes focus");
		}
	}
	// [[BPR-1345]] v002. No separate Submit: the draft's own verdict, pressed
	// from the keyboard, submits it with the decision shown.
	auto* const comment = editor.disposition_button(core::review_disposition::comment);
	comment->setFocus();
	QKeyEvent press{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
	QKeyEvent release{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(comment, &press);
	QApplication::sendEvent(comment, &release);
	expect(intents.back().Command.Kind == core::review_submission_command_kind::begin_submit && intents.back().Command.Id == draft.Id && intents.back().Prepared == projection.Prepared,
		   "keyboard press on the draft's verdict emits the exact submit intent");
	expect(editor.shown_draft() == draft, "submit awaits decided state");
	projection.Draft    = core::apply(draft, { .Id = draft.Id, .Kind = core::review_submission_command_kind::begin_submit }).Draft;
	projection.Controls = core::controls(*projection.Draft);
	projection.Status   = QStringLiteral("Review status is unknown. Check status.");
	editor.present(projection);
	expect(editor.body_editor()->isReadOnly() && !editor.submit_action()->isEnabled() && !editor.discard_action()->isEnabled() && editor.reconcile_action()->isEnabled()
			   && !comment->isEnabled() && !editor.disposition_button(core::review_disposition::approve)->isEnabled() && comment->toolTip() == projection.Status,
		   "pending projection exposes recovery only, and a verdict says why it waits");
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
	// A very long comment: its row stays one line.
	range.Body = "nit: scale could be constexpr.\n" + std::string(40U, ' ') + [] {
		auto words = std::string{ };
		for (auto paragraph = 0; paragraph < 12; ++paragraph) {
			words += "It never changes after start-up, and every frame reads it, so the compiler could fold it into the multiply instead of loading it each time.\n\n";
		}
		return words;
	}();
	finishing.Comments = { first, range };
	auto finish        = projection;
	finish.Draft       = finishing;
	finish.Controls    = core::controls(finishing);
	finish.Status      = { };
	finish.Prepared    = core::prepare_review_decision(finishing, { });
	editor.present(finish);
	QApplication::processEvents();
	expect(editor.title_label()->text() == QStringLiteral("Finish your review") && editor.pending_list()->isVisible()
			   && editor.pending_rows() == QStringList{ QStringLiteral("renderer.cpp, line 431: question: Where does 0.01 come from?"), QStringLiteral("renderer.cpp, base lines 434–435: nit: scale could be constexpr.") },
		   "Finish review lists the pending comments, each where it is and its first line");
	expect(editor.carries_label()->isVisible() && editor.carries_label()->text() == QStringLiteral("2 pending comments")
			   && editor.body_editor()->placeholderText() == QStringLiteral("Leave a summary (optional)"),
		   "the list says how many comments the review holds, and the summary is optional");
	auto* const changesButton = editor.disposition_button(core::review_disposition::request_changes);
	expect(editor.submit_action()->isEnabled() && editor.discard_action()->text() == QStringLiteral("Discard review")
			   && editor.disposition_button(core::review_disposition::comment)->isEnabled() && editor.disposition_button(core::review_disposition::approve)->isEnabled()
			   && editor.disposition_button(core::review_disposition::comment)->toolTip() == QStringLiteral("Submit your review with its 2 comments, without approving."),
		   "Comment with no summary submits the review and its comments, and Discard is Discard review");
	expect(!changesButton->isEnabled() && changesButton->toolTip() == QStringLiteral("Request changes needs a summary. Write one above first.")
			   && changesButton->accessibleDescription() == changesButton->toolTip(),
		   "Request changes waits for a summary, and says so");
	if (const auto target = qt::standard_tooltip_target(*changesButton, changesButton->rect().center()); true) {
		expect(target.has_value() && target->Anchor == changesButton->rect() && target->Text == changesButton->toolTip(), "its reason is anchored to the button");
	}
	{
		// Said once: one shown label says what is published, and no other
		// shown text repeats the verdicts.
		auto said = 0;
		for (const auto* label : editor.findChildren<QLabel*>()) {
			if (label->isVisible() && label->text().contains(QStringLiteral("published"), Qt::CaseInsensitive)) {
				++said;
			}
			expect(!label->isVisible() || !label->text().contains(QStringLiteral("Choose Comment, Approve")), "no label repeats the verdicts");
		}
		expect(said == 1 && editor.note_label()->isVisible() && editor.status_label()->isHidden(), "what is published is said once, and an empty status takes no room");
	}
	if (argc == 2) {
		expect(editor.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-finish.png")), "finish render saved");
	}
	const auto rowsShown = editor.pending_list()->findChildren<QAbstractButton*>(QStringLiteral("wholeReviewPendingOpen"));
	const auto deletes   = editor.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"));
	expect(rowsShown.size() == 2 && deletes.size() == 2, "each pending comment is a row with its own Delete");
	{
		// [[BPR-1715]]. While a submission may hold its comments, no row
		// offers Delete, and a Delete asked for anyway asks nothing.
		auto held        = finishing;
		held.State       = core::review_submission_state::submitting;
		held.Attempt     = core::review_submission_attempt_id{ 1U };
		auto waiting     = finish;
		waiting.Draft    = held;
		waiting.Controls = core::controls(held);
		editor.present(waiting);
		QApplication::processEvents();
		const auto heldDeletes = editor.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete"));
		expect(heldDeletes.size() == 2 && std::ranges::none_of(heldDeletes, [](const QToolButton* aButton) { return aButton->isVisibleTo(aButton->window()) || aButton->isEnabled(); }),
			   "the rows of a review awaiting GitHub's answer offer no Delete");
		auto asked = false;
		editor.set_pending_delete([&](const core::review_submission_comment&) { asked = true; });
		heldDeletes.front()->click();
		QApplication::processEvents();
		expect(!asked && editor.delete_confirmation() == nullptr, "and no Delete question opens for a held comment");
		editor.set_pending_delete({ });
		editor.present(finish);
		QApplication::processEvents();
		expect(std::ranges::all_of(heldDeletes, [](const QToolButton* aButton) { return aButton->isEnabled(); }), "a review being written offers Delete again");
	}
	for (auto* row : rowsShown) {
		const auto box = QRect{ row->mapTo(&editor, QPoint{ }), row->size() };
		expect(editor.rect().contains(box) && !row->toolTip().isEmpty() && row->focusPolicy() == Qt::StrongFocus, "each row fits the page, takes focus, and its tooltip names the whole comment");
	}

	// [[BPR-1345]] v002. A row goes to its comment; Delete asks first and
	// names the comment; Cancel deletes nothing.
	auto goneTo  = std::vector<core::review_submission_comment>{ };
	auto deleted = std::vector<core::review_submission_comment>{ };
	editor.set_pending_reveal([&](const core::review_submission_comment& aComment) { goneTo.push_back(aComment); });
	editor.set_pending_delete([&](const core::review_submission_comment& aComment) { deleted.push_back(aComment); });
	const auto beforeRows = intents.size();
	// [[BPR-1590]] v003. Threads' gestures (BPR-1582): a click selects and
	// opens nothing; a double click or Return opens in Review; with Shift, in
	// Discuss; the row menu adds Show in Threads.
	auto opened = std::vector<std::pair<core::review_submission_comment, core::workbench_thread_destination>>{ };
	editor.show();
	QApplication::processEvents();
	const auto mouse = [](QWidget* aRow, const QEvent::Type aType, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) {
		const auto point = QPointF{ aRow->rect().center() };
		auto       event = QMouseEvent{ aType, point, aRow->mapToGlobal(point), Qt::LeftButton, aType == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, aModifiers };
		QApplication::sendEvent(aRow, &event);
	};
	const auto doubleClick = [&mouse](QWidget* aRow, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) {
		mouse(aRow, QEvent::MouseButtonPress, aModifiers);
		mouse(aRow, QEvent::MouseButtonRelease, aModifiers);
		mouse(aRow, QEvent::MouseButtonDblClick, aModifiers);
		mouse(aRow, QEvent::MouseButtonRelease, aModifiers);
	};
	const auto key = [](QWidget* aRow, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) {
		auto keyPress = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, aModifiers };
		QApplication::sendEvent(aRow, &keyPress);
	};
	rowsShown.at(1)->click();
	mouse(rowsShown.at(1), QEvent::MouseButtonPress);
	mouse(rowsShown.at(1), QEvent::MouseButtonRelease);
	expect(goneTo.empty() && rowsShown.at(1)->hasFocus(), "negative control: a single click selects the row and opens nothing");
	{
		// [[BPR-1590]] v003, accepted: a selected row shows no "Open in
		// Review" words before its chevron (owner: "fine without that").
		const auto before = editor.size();
		editor.resize(1'000, editor.height());
		rowsShown.at(0)->setFocus(Qt::OtherFocusReason);
		QApplication::processEvents();
		const auto image = rowsShown.at(0)->grab().toImage();
		const auto ratio = image.devicePixelRatio();
		auto       inks  = std::set<QRgb>{ };
		for (auto x = rowsShown.at(0)->width() - 120; x < rowsShown.at(0)->width() - 30; ++x) {
			for (auto y = 4; y < rowsShown.at(0)->height() - 4; ++y) {
				inks.insert(image.pixel(qRound(x * ratio), qRound(y * ratio)));
			}
		}
		expect(rowsShown.at(0)->width() > 700 && inks.size() <= 2U, "a selected row draws no words before its chevron");
		editor.resize(before);
		QApplication::processEvents();
	}
	doubleClick(rowsShown.at(1));
	expect(goneTo.size() == 1U && goneTo.front() == range && intents.size() == beforeRows, "a double click opens the comment in Review and submits nothing");
	editor.set_pending_open([&](const core::review_submission_comment& aComment, const core::workbench_thread_destination aDestination) { opened.emplace_back(aComment, aDestination); });
	doubleClick(rowsShown.at(0), Qt::ShiftModifier);
	key(rowsShown.at(1));
	key(rowsShown.at(1), Qt::ShiftModifier);
	expect(opened.size() == 3U && opened[0].first == first && opened[0].second == core::workbench_thread_destination::discuss && opened[1].first == range
			   && opened[1].second == core::workbench_thread_destination::review && opened[2].second == core::workbench_thread_destination::discuss,
		   "Shift+double click opens in Discuss; Return in Review; Shift+Return in Discuss");
	{
		// The row menu: Open in Review, Open in Discuss, Show in Threads; then,
		// apart, Copy ([[BPR-1719]]).
		auto menuEvent = QContextMenuEvent{ QContextMenuEvent::Mouse, rowsShown.at(0)->rect().center(), rowsShown.at(0)->mapToGlobal(rowsShown.at(0)->rect().center()) };
		QApplication::sendEvent(rowsShown.at(0), &menuEvent);
		auto* const menu    = rowsShown.at(0)->findChild<QMenu*>(QStringLiteral("wholeReviewPendingMenu"));
		auto        said    = QStringList{ };
		auto*       threads = static_cast<QAction*>(nullptr);
		for (auto* const action : menu != nullptr ? menu->actions() : QList<QAction*>{ }) {
			said.push_back(action->isSeparator() ? QStringLiteral("|") : action->text());
			if (action->text() == QStringLiteral("Show in Threads")) {
				threads = action;
			}
		}
		expect(said == QStringList{ QStringLiteral("Open in Review"), QStringLiteral("Open in Discuss"), QStringLiteral("Show in Threads"), QStringLiteral("|"), QStringLiteral("Copy") } && menu->defaultAction() == menu->actions().front() && threads != nullptr,
			   "the row menu offers Open in Review, Open in Discuss and Show in Threads, as a thread row's does, then Copy");
		threads->trigger();
		menu->close();
		expect(opened.size() == 4U && opened[3].first == first && opened[3].second == core::workbench_thread_destination::threads, "Show in Threads goes to Threads");
	}
	expect(rowsShown.at(0)->height() == rowsShown.at(1)->height() && rowsShown.at(0)->height() < 2 * rowsShown.at(0)->fontMetrics().height() + 14,
		   "each row is one line, whatever its comment's length");
	deletes.at(0)->click();
	QApplication::processEvents();
	auto* question = editor.delete_confirmation();
	// [[BPR-1345]] v003. The shared question: the comment's place on its own
	// line, the comment itself rendered in its box, and only that it cannot
	// be undone (owner, delete-comment v002).
	expect(question != nullptr && question->isVisible() && question->question() == QStringLiteral("Delete this comment from your review?")
			   && question->location() == QStringLiteral("renderer.cpp, line 431") && question->detail() == QStringLiteral("This cannot be undone.")
			   && question->subject_box()->isVisible() && question->subject_view() != nullptr
			   && question->subject_view()->rendered_view()->toPlainText().startsWith(QStringLiteral("question: Where does 0.01 come from?"))
			   && question->subject_view()->rendered_view()->toPlainText().contains(QStringLiteral("It looks arbitrary."))
			   && question->subject_mark()->isVisible() && question->cancel_button()->isDefault() && question->confirm_button()->text() == QStringLiteral("Delete comment")
			   && question->confirm_button()->role() == qt::question_button_role::destructive && deleted.empty(),
		   "Delete asks first, showing the comment where it is, in its box, with Cancel the default");
	question->cancel_button()->click();
	QApplication::processEvents();
	expect(deleted.empty() && editor.delete_confirmation() == nullptr, "Cancel deletes nothing");
	deletes.at(0)->click();
	QApplication::processEvents();
	question = editor.delete_confirmation();
	question->findChild<QAbstractButton*>(QStringLiteral("deleteCommentConfirmationConfirm"))->click();
	QApplication::processEvents();
	expect(deleted.size() == 1U && deleted.front() == first && intents.size() == beforeRows, "Delete comment deletes exactly that comment, through the window");

	// [[RFC-BPR-0090]] Q3. A held suggestion says why under its row, offers
	// Convert beside Delete, and keeps every verdict and Submit back until it
	// is gone.
	{
		auto                         heldIntents = std::vector<qt::review_submission_editor_intent>{ };
		auto                         converted   = std::vector<core::review_draft_id>{ };
		qt::review_submission_editor held{ [&](qt::review_submission_editor_intent aIntent) { heldIntents.push_back(std::move(aIntent)); } };
		held.set_pending_convert([&](const core::review_submission_comment& aComment) { converted.push_back(aComment.Draft); });
		auto suggestion    = first;
		suggestion.Draft   = core::review_draft_id{ 21U };
		suggestion.Line    = 432U;
		suggestion.Body    = "```suggestion\n\treturn clamp(scale);\n```";
		auto carrying      = finishing;
		carrying.Comments  = { first, suggestion };
		auto heldShown     = finish;
		heldShown.Draft    = carrying;
		heldShown.Controls = core::controls(carrying);
		heldShown.Prepared = core::prepare_review_decision(carrying, { });
		held.resize(560, 760);
		held.show();
		held.present(heldShown);
		QApplication::processEvents();
		auto* const verdict = held.disposition_button(core::review_disposition::comment);
		const auto  free    = verdict->isEnabled();
		held.set_held_comments({ { suggestion.Draft, { .Reason = QStringLiteral("Line 433 changed after commit 9b2e41c. GitHub shows a comment here as outdated, so it cannot apply a suggestion."), .Convert = QStringLiteral("Convert to a comment on 9b2e41c") } } });
		QApplication::processEvents();
		const auto reasons  = held.pending_list()->findChildren<QLabel*>(QStringLiteral("wholeReviewPendingHeld"));
		const auto converts = held.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingConvert"));
		expect(reasons.size() == 1 && reasons.front()->text().startsWith(QStringLiteral("Line 433 changed after commit 9b2e41c")) && reasons.front()->isVisible() && converts.size() == 1
				   && converts.front()->text() == QStringLiteral("Convert to a comment on 9b2e41c") && converts.front()->isVisible(),
			   "the held suggestion's row says why and offers Convert; the other comment's row does not");
		expect(!verdict->isEnabled() && verdict->toolTip() == QStringLiteral("1 suggestion GitHub cannot apply is held. Convert it or delete it first."),
			   "every verdict waits while a suggestion is held, and says why");
		const auto before = heldIntents.size();
		held.submit_action()->trigger();
		QApplication::processEvents();
		expect(std::ranges::none_of(std::span{ heldIntents }.subspan(before), [](const qt::review_submission_editor_intent& aIntent) {
				   return aIntent.Command.Kind == core::review_submission_command_kind::begin_submit;
			   }),
			   "Submit by its key sends nothing while a suggestion is held");
		converts.front()->click();
		QApplication::processEvents();
		expect(converted == std::vector<core::review_draft_id>{ suggestion.Draft }, "Convert asks the window to convert exactly that comment");
		held.set_held_comments({ });
		QApplication::processEvents();
		// Relisted rows go once the event loop deletes them; they are hidden at once.
		const auto stillHeld = held.pending_list()->findChildren<QLabel*>(QStringLiteral("wholeReviewPendingHeld"));
		expect(verdict->isEnabled() == free && std::ranges::none_of(stillHeld, [](const QLabel* aLabel) { return aLabel->isVisible(); }),
			   "once nothing is held the rows say nothing more and the verdicts are as they were");

		// [[BPR-1735]]. A comment GitHub cannot place in a review is held the
		// same way, its action Publish as file comment; Submit waits for it.
		held.set_held_comments({ { suggestion.Draft, { .Reason = QStringLiteral("GitHub shows no change near line 86, so it can't place this comment in a review."), .Convert = QStringLiteral("Publish as file comment"), .Kind = qt::review_submission_editor::held_kind::unplaceable_line } } });
		QApplication::processEvents();
		const auto visible = [](const auto& aWidgets) {
			return static_cast<std::size_t>(std::ranges::count_if(aWidgets, [](const QWidget* aWidget) { return aWidget->isVisible(); }));
		};
		const auto placeReasons = held.pending_list()->findChildren<QLabel*>(QStringLiteral("wholeReviewPendingHeld"));
		const auto publishes    = held.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingConvert"));
		auto*      publish      = publishes.isEmpty() ? nullptr : *std::ranges::find_if(publishes, [](const QToolButton* aButton) { return aButton->isVisible(); });
		expect(visible(placeReasons) == 1U && visible(publishes) == 1U && publish != nullptr && publish->text() == QStringLiteral("Publish as file comment")
				   && publish->toolTip().contains(QStringLiteral("on the file with a link to its line, and leaves this review")),
			   "the held comment's row says why and offers Publish as file comment beside Delete");
		expect(!verdict->isEnabled() && verdict->toolTip() == QStringLiteral("1 comment GitHub can't place in a review is held. Publish it as a file comment or delete it first."),
			   "every verdict waits while it is held, and says why");
		const auto placeBefore = heldIntents.size();
		held.submit_action()->trigger();
		QApplication::processEvents();
		expect(std::ranges::none_of(std::span{ heldIntents }.subspan(placeBefore), [](const qt::review_submission_editor_intent& aIntent) {
				   return aIntent.Command.Kind == core::review_submission_command_kind::begin_submit;
			   }),
			   "Submit by its key sends nothing while it is held");
		converted.clear();
		publish->click();
		QApplication::processEvents();
		expect(converted == std::vector<core::review_draft_id>{ suggestion.Draft }, "Publish as file comment asks the window for exactly that comment");
		held.set_held_comments({ });
		QApplication::processEvents();
		expect(verdict->isEnabled() == free, "once it leaves the review the verdicts are as they were, and Submit is allowed");
	}

	// [[BPR-1719]]. Several pending comments are selected with Shift,
	// Command (Ctrl) and Select All, and copied in the page's order: each its
	// file and lines, then the whole comment. Selecting and copying change
	// nothing, and a review awaiting GitHub's answer copies the same way.
	{
		auto                         pickIntents = std::vector<qt::review_submission_editor_intent>{ };
		auto                         pickDeleted = 0;
		auto                         pickOpened  = std::vector<std::pair<core::review_submission_comment, core::workbench_thread_destination>>{ };
		qt::review_submission_editor picker{ [&](qt::review_submission_editor_intent aIntent) { pickIntents.push_back(std::move(aIntent)); } };
		picker.set_pending_delete([&](const core::review_submission_comment&) { ++pickDeleted; });
		picker.set_pending_open([&](const core::review_submission_comment& aComment, const core::workbench_thread_destination aDestination) { pickOpened.emplace_back(aComment, aDestination); });
		auto third           = first;
		third.Draft          = core::review_draft_id{ 9U };
		third.Path           = "src/engine/scale.hpp";
		third.Line           = 13U;
		third.StartLine      = 11U;
		third.Body           = "suggestion: make it constexpr\n```suggestion\nconstexpr auto scale = 0.01;\n```\n";
		auto carrying        = finishing;
		carrying.Comments    = { first, range, third };
		auto shownPicker     = finish;
		shownPicker.Draft    = carrying;
		shownPicker.Controls = core::controls(carrying);
		shownPicker.Prepared = core::prepare_review_decision(carrying, { });
		picker.resize(560, 760);
		picker.show();
		picker.present(shownPicker);
		QApplication::processEvents();
		const auto pickRows = picker.pending_list()->findChildren<QAbstractButton*>(QStringLiteral("wholeReviewPendingOpen"));
		expect(pickRows.size() == 3, "the picker lists three pending comments");
		auto* const clipboard = QGuiApplication::clipboard();
		const auto  copied    = [&](QWidget* aRow) {
			clipboard->setText(QStringLiteral("(nothing copied)"));
			auto claim = QKeyEvent{ QEvent::ShortcutOverride, Qt::Key_C, Qt::ControlModifier };
			claim.ignore();
			QApplication::sendEvent(aRow, &claim);
			auto copyKey = QKeyEvent{ QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier };
			QApplication::sendEvent(aRow, &copyKey);
			QApplication::processEvents();
			return std::pair{ claim.isAccepted(), clipboard->text() };
		};
		const auto clickRow = [](QWidget* aRow, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) {
			const auto point = QPointF{ aRow->rect().center() };
			auto       down  = QMouseEvent{ QEvent::MouseButtonPress, point, aRow->mapToGlobal(point), Qt::LeftButton, Qt::LeftButton, aModifiers };
			auto       up    = QMouseEvent{ QEvent::MouseButtonRelease, point, aRow->mapToGlobal(point), Qt::LeftButton, Qt::NoButton, aModifiers };
			QApplication::sendEvent(aRow, &down);
			QApplication::sendEvent(aRow, &up);
			QApplication::processEvents();
		};
		const auto keyOn = [](QWidget* aRow, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) {
			auto event = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
			QApplication::sendEvent(aRow, &event);
			QApplication::processEvents();
		};
		const auto firstText = QStringLiteral("src/engine/renderer.cpp:431\nquestion: Where does 0.01 come from?\nIt looks arbitrary.");
		// The range on the base side, its whole long body, its last line
		// breaks dropped.
		auto rangeBody = QString::fromStdString(range.Body);
		while (rangeBody.endsWith(QLatin1Char{ '\n' })) {
			rangeBody.chop(1);
		}
		const auto rangeText = QStringLiteral("src/engine/renderer.cpp:434-435 (LEFT)\n") + rangeBody;
		const auto thirdText = QStringLiteral("src/engine/scale.hpp:12-14\nsuggestion: make it constexpr\n```suggestion\nconstexpr auto scale = 0.01;\n```");
		const auto all       = firstText + QStringLiteral("\n\n") + rangeText + QStringLiteral("\n\n") + thirdText;
		expect(rangeBody.size() > 1'000 && rangeBody.endsWith(QStringLiteral("instead of loading it each time.")), "the range's comment is long, and copied untruncated");

		clickRow(pickRows.at(0));
		const auto [claimed, one] = copied(pickRows.at(0));
		expect(claimed && one == firstText, "a plain click selects one row, and Command-C copies its file, line and whole comment");
		clickRow(pickRows.at(2), Qt::ShiftModifier);
		expect(copied(pickRows.at(2)).second == all, "Shift-click selects through to that row, and Copy writes every comment in the page's order, a blank line between");
		clickRow(pickRows.at(1), Qt::ControlModifier);
		expect(copied(pickRows.at(1)).second == firstText + QStringLiteral("\n\n") + thirdText, "Command-click (Ctrl-click) takes a row out of the selection");
		clickRow(pickRows.at(1), Qt::ControlModifier);
		expect(copied(pickRows.at(1)).second == all, "and puts it back");
		clickRow(pickRows.at(1));
		expect(copied(pickRows.at(1)).second == rangeText, "a plain click selects that row alone again");
		keyOn(pickRows.at(1), Qt::Key_Down, Qt::ShiftModifier);
		expect(pickRows.at(2)->hasFocus() && copied(pickRows.at(2)).second == rangeText + QStringLiteral("\n\n") + thirdText, "Shift+Down extends the selection to the next row");
		keyOn(pickRows.at(2), Qt::Key_Up, Qt::ShiftModifier);
		keyOn(pickRows.at(1), Qt::Key_Up, Qt::ShiftModifier);
		expect(pickRows.at(0)->hasFocus() && copied(pickRows.at(0)).second == firstText + QStringLiteral("\n\n") + rangeText, "Shift+Up extends it upwards from where it started");
		keyOn(pickRows.at(0), Qt::Key_Down);
		expect(pickRows.at(1)->hasFocus() && copied(pickRows.at(1)).second == rangeText, "a plain arrow moves to the next row and selects it alone");
		{
			auto claim = QKeyEvent{ QEvent::ShortcutOverride, Qt::Key_A, Qt::ControlModifier };
			claim.ignore();
			QApplication::sendEvent(pickRows.at(1), &claim);
			keyOn(pickRows.at(1), Qt::Key_A, Qt::ControlModifier);
			expect(claim.isAccepted() && copied(pickRows.at(1)).second == all, "Command-A (Ctrl+A) selects every row, ahead of any window shortcut");
		}
		// Return and a double click still open the focused row, one comment.
		keyOn(pickRows.at(1), Qt::Key_Return);
		expect(pickOpened.size() == 1U && pickOpened.back().first == range && pickOpened.back().second == core::workbench_thread_destination::review, "Return opens the focused row's comment in Review, whatever is selected");

		// The menu: the menu key and Shift+F10 open it on the focused row,
		// with Copy after the opening items; inside the selection it keeps it.
		const auto menuOf = [](QWidget* aRow) {
			auto* menu = static_cast<QMenu*>(nullptr);
			for (auto* const candidate : aRow->findChildren<QMenu*>(QStringLiteral("wholeReviewPendingMenu"))) {
				if (candidate->isVisible()) {
					menu = candidate;
				}
			}
			return menu;
		};
		const auto menuWords = [](const QMenu* aMenu) {
			auto said = QStringList{ };
			for (const auto* action : aMenu != nullptr ? aMenu->actions() : QList<QAction*>{ }) {
				if (!action->isSeparator()) {
					said.push_back(action->text());
				}
			}
			return said;
		};
		const auto copyIn = [](const QMenu* aMenu) -> QAction* {
			for (auto* const action : aMenu != nullptr ? aMenu->actions() : QList<QAction*>{ }) {
				if (action->objectName() == QStringLiteral("wholeReviewPendingCopy")) {
					return action;
				}
			}
			return nullptr;
		};
		const auto expectedWords = QStringList{ QStringLiteral("Open in Review"), QStringLiteral("Open in Discuss"), QStringLiteral("Show in Threads"), QStringLiteral("Copy") };
		keyOn(pickRows.at(1), Qt::Key_Menu);
		auto* menu = menuOf(pickRows.at(1));
		expect(menu != nullptr && menuWords(menu) == expectedWords && copyIn(menu)->isEnabled() && copyIn(menu)->shortcut() == QKeySequence{ QKeySequence::Copy },
			   "the menu key opens the row's menu, its Copy showing the platform's Copy key");
		clipboard->setText(QString{ });
		copyIn(menu)->trigger();
		menu->close();
		QApplication::processEvents();
		expect(clipboard->text() == all, "the menu's Copy copies the whole selection the menu opened inside");
		keyOn(pickRows.at(2), Qt::Key_F10, Qt::ShiftModifier);
		menu = menuOf(pickRows.at(2));
		expect(menu != nullptr && menuWords(menu) == expectedWords, "Shift+F10 opens the same menu");
		menu->close();
		QApplication::processEvents();
		// A right-click outside the selection selects that row alone.
		clickRow(pickRows.at(0));
		{
			auto right = QContextMenuEvent{ QContextMenuEvent::Mouse, pickRows.at(2)->rect().center(), pickRows.at(2)->mapToGlobal(pickRows.at(2)->rect().center()) };
			QApplication::sendEvent(pickRows.at(2), &right);
			menu = menuOf(pickRows.at(2));
			expect(menu != nullptr && copyIn(menu) != nullptr, "a right-click opens the menu with Copy");
			clipboard->setText(QString{ });
			copyIn(menu)->trigger();
			menu->close();
			QApplication::processEvents();
			expect(clipboard->text() == thirdText && picker.selected_pending() == std::vector{ third }, "a right-click outside the selection selects and copies that row alone");
		}

		// While GitHub's answer is unknown the comments are held: no Delete,
		// and they still select and copy, so the review can be kept.
		auto held        = carrying;
		held.State       = core::review_submission_state::submitting;
		held.Attempt     = core::review_submission_attempt_id{ 1U };
		auto waiting     = shownPicker;
		waiting.Draft    = held;
		waiting.Controls = core::controls(held);
		waiting.Status   = QStringLiteral("Review status is unknown. Check status.");
		picker.present(waiting);
		QApplication::processEvents();
		const auto heldRows = picker.pending_list()->findChildren<QAbstractButton*>(QStringLiteral("wholeReviewPendingOpen"));
		expect(heldRows.size() == 3, "a held review still lists its comments");
		keyOn(heldRows.at(0), Qt::Key_A, Qt::ControlModifier);
		expect(copied(heldRows.at(0)).second == all, "a review awaiting GitHub's answer copies every held comment, with its text");
		expect(std::ranges::none_of(picker.pending_list()->findChildren<QToolButton*>(QStringLiteral("wholeReviewPendingDelete")), [](const QToolButton* aButton) { return aButton->isVisibleTo(aButton->window()); }),
			   "and its pickRows still offer no Delete");

		// A refreshed list (a comment edited elsewhere) keeps each comment's
		// selection by its draft, and copies the new words.
		clickRow(heldRows.at(0));
		clickRow(heldRows.at(1), Qt::ControlModifier);
		auto edited             = held;
		edited.Comments[1].Body = "nit: now constexpr.";
		edited.Comments[1].DraftRevision += 1U;
		auto refreshed  = waiting;
		refreshed.Draft = edited;
		picker.present(refreshed);
		QApplication::processEvents();
		expect(picker.selected_pending() == std::vector{ edited.Comments[0], edited.Comments[1] } && picker.pending_copy_text() == firstText + QStringLiteral("\n\nsrc/engine/renderer.cpp:434-435 (LEFT)\nnit: now constexpr."),
			   "a refreshed list keeps each comment's selection and copies its current words");
		held = edited;

		// Nothing changed: no command, no delete, no other opening, and the
		// page shows the draft it was given.
		expect(pickIntents.empty() && pickDeleted == 0 && pickOpened.size() == 1U && picker.shown_draft() == held && picker.delete_confirmation() == nullptr && picker.discard_confirmation() == nullptr,
			   "selecting and copying send no command, delete nothing and open nothing");
	}

	// [[BPR-1345]] v002. Each verdict submits at once as itself. A page whose
	// dispatch is the workflow's: it applies the command and presents again.
	{
		auto  liveIntents = std::vector<qt::review_submission_editor_intent>{ };
		auto  liveDraft   = finishing;
		auto* livePage    = static_cast<qt::review_submission_editor*>(nullptr);
		auto  closed      = 0;
		auto  presentLive = [&] {
			auto shownNow     = finish;
			shownNow.Draft    = liveDraft;
			shownNow.Controls = core::controls(liveDraft);
			shownNow.Prepared = core::prepare_review_decision(liveDraft, { });
			livePage->present(shownNow);
		};
		qt::review_submission_editor live{ [&](qt::review_submission_editor_intent aIntent) {
			liveIntents.push_back(aIntent);
			if (aIntent.Operation == qt::review_submission_editor_operation::command) {
				if (aIntent.Command.Kind == core::review_submission_command_kind::begin_submit
					&& aIntent.Prepared != core::prepare_review_decision(liveDraft, { })) {
					return;
				}
				if (const auto outcome = core::apply(liveDraft, aIntent.Command); outcome.Draft) {
					liveDraft = *outcome.Draft;
				}
				presentLive();
			}
		} };
		livePage = &live;
		live.set_discard_observer([&] { ++closed; });
		live.resize(420, 760);
		live.show();
		presentLive();
		QApplication::processEvents();
		live.disposition_button(core::review_disposition::approve)->click();
		expect(liveIntents.size() == 2U && liveIntents[0].Command.Kind == core::review_submission_command_kind::choose_disposition
				   && liveIntents[0].Command.Disposition == core::review_disposition::approve && liveIntents[1].Command.Kind == core::review_submission_command_kind::begin_submit
				   && liveDraft.State == core::review_submission_state::submitting && liveDraft.Disposition == core::review_disposition::approve,
			   "Approve submits at once as an approval: one press chooses it and submits");
		liveDraft = finishing;
		liveIntents.clear();
		presentLive();
		live.disposition_button(core::review_disposition::request_changes)->click();
		expect(liveIntents.empty() && liveDraft.State == core::review_submission_state::drafting, "Request changes with no summary submits nothing");
		live.body_editor()->setPlainText(QStringLiteral("Please make the scale constexpr."));
		QApplication::processEvents();
		expect(live.disposition_button(core::review_disposition::request_changes)->isEnabled()
				   && live.disposition_button(core::review_disposition::request_changes)->toolTip() == QStringLiteral("Submit your review and request changes."),
			   "with a summary, Request changes is offered");
		live.disposition_button(core::review_disposition::request_changes)->click();
		expect(liveDraft.State == core::review_submission_state::submitting && liveDraft.Disposition == core::review_disposition::request_changes
				   && liveDraft.Body == "Please make the scale constexpr.",
			   "and submits the review with it as its own verdict");

		// Discard review asks first, naming how many comments it deletes;
		// confirmed, it discards and the page closes.
		liveDraft = finishing;
		liveIntents.clear();
		presentLive();
		live.discard_action()->trigger();
		QApplication::processEvents();
		auto* discard = live.discard_confirmation();
		expect(discard != nullptr && discard->isVisible() && discard->question() == QStringLiteral("Discard your review and its 2 pending comments?")
				   && discard->detail() == QStringLiteral("This cannot be undone.") && discard->confirm_button()->text() == QStringLiteral("Discard review")
				   && discard->confirm_button()->role() == qt::question_button_role::destructive && discard->cancel_button()->isDefault()
				   && liveIntents.empty() && closed == 0,
			   "Discard review asks first in the shared question, naming the count");
		// [[BPR-1345]] v003. Discard review's own button is a deleting one:
		// red words, no fill (owner, discard-review v002).
		auto* const discardButton = live.discard_button();
		expect(discardButton != nullptr && discardButton->property("questionRole").toString() == QStringLiteral("destructive") && discardButton->text() == QStringLiteral("Discard review")
				   && discardButton->ink().hslSaturationF() > 0.35 && (discardButton->ink().hslHue() <= 25 || discardButton->ink().hslHue() >= 335)
				   && qt::destructive_contrast(discardButton->ink(), live.palette().color(QPalette::Active, QPalette::Window)) >= 4.5,
			   "Discard review is drawn in the deleting red, readable on the page");
		QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
		QApplication::sendEvent(discard, &escape);
		QApplication::processEvents();
		expect(liveIntents.empty() && liveDraft.State == core::review_submission_state::drafting && closed == 0, "Cancel keeps the review");
		live.discard_action()->trigger();
		QApplication::processEvents();
		live.discard_confirmation()->findChild<QAbstractButton*>(QStringLiteral("discardReviewConfirmationConfirm"))->click();
		QApplication::processEvents();
		expect(liveIntents.size() == 1U && liveIntents.front().Command.Kind == core::review_submission_command_kind::discard
				   && liveDraft.State == core::review_submission_state::discarded && closed == 1,
			   "confirmed, the review is discarded and the page is asked to close");
		// An empty draft has nothing to lose: no question.
		liveDraft = core::review_submission_draft{ .Id = core::review_submission_draft_id{ 2U }, .Target = draft.Target, .ActingAccount = draft.ActingAccount };
		presentLive();
		expect(!live.disposition_button(core::review_disposition::comment)->isEnabled()
				   && live.disposition_button(core::review_disposition::comment)->toolTip() == QStringLiteral("Comment needs a summary when the review has no line comments.")
				   && live.disposition_button(core::review_disposition::approve)->isEnabled(),
			   "with no summary and no comment, Comment says what it needs and Approve is offered");
		live.discard_action()->trigger();
		QApplication::processEvents();
		expect(live.discard_confirmation() == nullptr && liveDraft.State == core::review_submission_state::discarded && closed == 2, "an empty draft is discarded without a question");
		// [[BPR-1345]] v003. GitHub's confirmation is said once, with the
		// review it confirmed, so the window can close the page.
		auto heard = std::vector<std::size_t>{ };
		live.set_submitted_observer([&](const std::size_t aComments) { heard.push_back(aComments); });
		auto confirmedDraft  = finishing;
		confirmedDraft.State = core::review_submission_state::submitted;
		live.confirm_submitted(confirmedDraft);
		expect(heard == std::vector<std::size_t>{ 2U } && live.confirmed_submissions() == 1U && live.last_submitted() == confirmedDraft,
			   "a confirmed review is announced once, with its comment count");
		live.hide();
	}
	// [[BPR-1541]]. Under "Review status is unknown", one line says why, in a
	// reviewer's words, from the notice the core chose: every reason its own
	// line, counts only where the notice carries them, no identity, part of
	// the status's description, wrapping, readable in every theme, and gone
	// when there is no reason or the review is not awaiting confirmation.
	{
		auto awaiting    = draft;
		awaiting.State   = core::review_submission_state::submitting;
		awaiting.Attempt = core::review_submission_attempt_id{ 1U };
		auto unknown     = projection;
		unknown.Draft    = awaiting;
		unknown.Controls = core::controls(awaiting);
		unknown.Status   = QStringLiteral("Review status is unknown. Check status to reconcile this attempt; do not submit it again.");
		editor.resize(300, 700);
		editor.present(unknown);
		QApplication::processEvents();
		expect(editor.unknown_reason_label()->isHidden() && editor.status_label()->accessibleDescription().isEmpty(), "with no reason there is no reason line");
		auto       lines    = std::set<QString>{ };
		const auto diagnose = [](const core::review_submission_unknown_reason aReason) {
			auto value             = core::review_submission_unknown_diagnosis{ .Reason = aReason };
			value.CommentsExpected = 3U;
			value.CommentsFound    = 3U;
			value.CommentsMatched  = 2U;
			value.HttpStatusClass  = aReason == core::review_submission_unknown_reason::http_status ? std::uint8_t{ 5U } : std::uint8_t{ 0U };
			return value;
		};
		const auto sheet = qEnvironmentVariable("BPR_UNKNOWN_REASON_SHEET");
		for (std::size_t index = 0U; index < core::review_submission_unknown_reason_count; ++index) {
			const auto reason = static_cast<core::review_submission_unknown_reason>(index);
			unknown.Unknown   = core::notice(diagnose(reason));
			editor.present(unknown);
			QApplication::processEvents();
			const auto text  = editor.unknown_reason_label()->text();
			const auto where = std::string{ " (" } + std::string{ core::name(reason) } + ")";
			expect(!editor.unknown_reason_label()->isHidden() && text == qt::review_submission_unknown_text(*unknown.Unknown) && !text.isEmpty() && text.endsWith(QLatin1Char{ '.' }) && text.size() <= 170,
				   ("each reason shows its own short line" + where).c_str());
			expect(editor.status_label()->accessibleDescription() == text, ("and the status's description says it" + where).c_str());
			expect(lines.insert(text).second, ("no two reasons share a line" + where).c_str());
			for (const auto* secret : { "github.com", "owner", "repository#", "reviewer", "abcdef", "account 4", "Literal" }) {
				expect(!text.contains(QString::fromLatin1(secret)), ("and it names no host, repository, account, head or words" + where).c_str());
			}
			expect(editor.unknown_reason_label()->wordWrap() && editor.unknown_reason_label()->width() <= editor.width()
					   && editor.unknown_reason_label()->heightForWidth(editor.unknown_reason_label()->width()) <= editor.unknown_reason_label()->height(),
				   ("and it wraps inside the page" + where).c_str());
			if (!sheet.isEmpty()) {
				// The attended render sheet: each line in Light and Dark.
				for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, "light" }, std::pair{ core::appearance_mode::dark, "dark" } }) {
					auto profile = qt::appearance_profile{ };
					profile.select_mode(mode);
					editor.setPalette(profile.derive(QApplication::palette()));
					editor.set_attention_colour(qt::to_presentation_color(qt::builtin_appearance_palette(mode == core::appearance_mode::light ? core::appearance_variant::light : core::appearance_variant::dark)[core::index_of(core::appearance_role::state_attention)]));
					QApplication::processEvents();
					const auto file = sheet + QStringLiteral("/") + QString::fromLatin1(core::name(reason).data(), static_cast<qsizetype>(core::name(reason).size())) + QStringLiteral("-") + QString::fromLatin1(name);
					expect(editor.grab().save(file + QStringLiteral(".png")) && editor.waiting_panel()->grab().save(file + QStringLiteral("-panel.png")), "the sheet render is saved");
				}
				editor.setPalette(QApplication::palette());
			}
		}
		expect(lines.size() == core::review_submission_unknown_reason_count, "all twenty reasons have a line");
		// Counts only where they help, singular and plural.
		auto counted    = diagnose(core::review_submission_unknown_reason::comments_mismatch);
		unknown.Unknown = core::notice(counted);
		editor.present(unknown);
		expect(editor.unknown_reason_label()->text().contains(QStringLiteral("only 2 of 3 comments matched")), "a mismatch says how many of the review's comments matched");
		counted.CommentsFound   = 2U;
		counted.CommentsMatched = 0U;
		unknown.Unknown         = core::notice(counted);
		editor.present(unknown);
		expect(editor.unknown_reason_label()->text().contains(QStringLiteral("it holds 2 comments and this one has 3 comments")), "or how many it holds against how many this one has");
		counted.CommentsExpected = 1U;
		counted.CommentsFound    = 1U;
		unknown.Unknown          = core::notice(counted);
		editor.present(unknown);
		expect(editor.unknown_reason_label()->text().contains(QStringLiteral("only 0 of 1 comment matched")), "one comment is said in the singular");
		auto refused            = diagnose(core::review_submission_unknown_reason::http_status);
		refused.HttpStatusClass = 4U;
		unknown.Unknown         = core::notice(refused);
		editor.present(unknown);
		const auto four         = editor.unknown_reason_label()->text();
		refused.HttpStatusClass = 0U;
		unknown.Unknown         = core::notice(refused);
		editor.present(unknown);
		expect(four != editor.unknown_reason_label()->text() && !lines.contains(four) && four.contains(QStringLiteral("refused")), "a refused read and an unclassified error read differently from a server error");
		// [[BPR-1541]] v002 (owner, v001: "This is pretty darned ugly, can we
		// do better frontend ux design"). The whole waiting state is one calm
		// panel under the title: its mark and title, why, what is safe, and
		// Check status inside it; the status line, Discard and the old "in
		// progress" sentence step aside, and the verdicts wait disabled.
		{
			auto* const panel = editor.waiting_panel();
			unknown.Unknown   = core::notice(diagnose(core::review_submission_unknown_reason::network));
			editor.present(unknown);
			QApplication::processEvents();
			expect(panel != nullptr && !panel->isHidden() && editor.waiting_title_label()->text() == QStringLiteral("We couldn't confirm this review"), "an unconfirmed review is told in the waiting panel, under its title");
			expect(panel->isAncestorOf(editor.unknown_reason_label()) && panel->isAncestorOf(editor.reconcile_button()) && !editor.reconcile_button()->isHidden() && editor.reconcile_action()->isEnabled(),
				   "the reason and Check status are the panel's own");
			expect(editor.waiting_assurance_label()->text() == QStringLiteral("Nothing will be sent twice. Your review may already be on GitHub.") && !editor.waiting_assurance_label()->isHidden(),
				   "and it says what is safe");
			expect(editor.status_label()->isHidden() && editor.status_label()->text().startsWith(QStringLiteral("Review status is unknown.")) && editor.discard_button()->isHidden()
					   && editor.findChild<QLabel*>(QStringLiteral("wholeReviewDraftState"))->isHidden(),
				   "the status line, Discard and the in-progress sentence step aside");
			for (const auto disposition : { core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes }) {
				expect(!editor.disposition_button(disposition)->isEnabled(), "the verdicts wait, disabled");
			}
			const auto top = [&editor](const QWidget* aWidget) { return aWidget->mapTo(&editor, QPoint{ 0, 0 }).y(); };
			expect(top(panel) > top(editor.title_label()) && top(panel) < top(editor.body_editor()) && top(panel) < top(editor.disposition_button(core::review_disposition::comment)),
				   "the panel comes first, under the title and above the draft");
			expect(panel->accessibleName() == editor.waiting_title_label()->text() && panel->accessibleDescription().contains(editor.unknown_reason_label()->text())
					   && panel->accessibleDescription().contains(editor.waiting_assurance_label()->text()),
				   "the panel's name is its title and its description says why and what is safe");
			expect(panel->width() <= editor.width() && editor.unknown_reason_label()->wordWrap() && editor.waiting_assurance_label()->wordWrap(), "the panel fits the page and its words wrap");
			expect(!editor.body_editor()->parentWidget()->isHidden() && editor.body_editor()->isReadOnly(), "a summary it carries is shown, read only");
			auto blank        = unknown;
			blank.Draft->Body = { };
			editor.present(blank);
			expect(editor.body_editor()->parentWidget()->isHidden(), "an empty summary is not offered as a field while it waits");
			editor.present(unknown);
			// No reason recorded: the panel still says plainly what happened.
			unknown.Unknown.reset();
			editor.present(unknown);
			expect(!panel->isHidden() && editor.unknown_reason_label()->isHidden() && editor.findChild<QLabel*>(QStringLiteral("wholeReviewWaitingFallback"))->text() == QStringLiteral("GitHub didn't say whether it arrived."),
				   "without a reason the panel says GitHub didn't say whether it arrived");
			// While GitHub is asked, Check status cannot be pressed again.
			auto checking               = unknown;
			checking.Controls.Reconcile = false;
			checking.Unknown            = core::notice(diagnose(core::review_submission_unknown_reason::review_not_listed));
			editor.present(checking);
			expect(!panel->isHidden() && editor.waiting_title_label()->text() == QStringLiteral("Waiting for GitHub to confirm this review…") && editor.reconcile_button()->isHidden()
					   && !editor.unknown_reason_label()->isHidden() && editor.findChild<QLabel*>(QStringLiteral("wholeReviewWaitingFallback"))->isHidden(),
				   "while it is asked, the panel waits, keeps the last reason, and offers no Check status");
			// Calm in every theme: a theme-derived amber rail, never red; the
			// words and Check status readable on the panel's ground.
			for (const auto& [mode, variant] : { std::pair{ core::appearance_mode::light, core::appearance_variant::light }, std::pair{ core::appearance_mode::dark, core::appearance_variant::dark },
												 std::pair{ core::appearance_mode::high_contrast_light, core::appearance_variant::high_contrast_light },
												 std::pair{ core::appearance_mode::high_contrast_dark, core::appearance_variant::high_contrast_dark } }) {
				auto profile = qt::appearance_profile{ };
				profile.select_mode(mode);
				editor.setPalette(profile.derive(QApplication::palette()));
				// As the window gives it: the theme's own attention colour.
				editor.set_attention_colour(qt::to_presentation_color(qt::builtin_appearance_palette(variant)[core::index_of(core::appearance_role::state_attention)]));
				editor.present(unknown);
				QApplication::processEvents();
				const auto  attention = editor.attention_colour();
				const auto& colours   = editor.palette();
				const auto  base      = colours.color(QPalette::Active, QPalette::Base);
				const auto  ground    = QColor::fromRgbF(base.redF() * 0.95F + attention.redF() * 0.05F, base.greenF() * 0.95F + attention.greenF() * 0.05F, base.blueF() * 0.95F + attention.blueF() * 0.05F);
				const auto  hue       = attention.hslHue();
				expect(hue >= 20 && hue <= 65, "the panel's accent is the theme's amber, not an alarm red");
				const auto quiet = editor.waiting_assurance_label()->palette().color(QPalette::Active, QPalette::WindowText);
				expect(qt::destructive_contrast(colours.color(QPalette::Active, QPalette::WindowText), ground) >= 4.5 && qt::destructive_contrast(quiet, ground) >= 4.5,
					   "the panel's words read at 4.5:1 or better on its ground");
				const auto highlight = colours.color(QPalette::Active, QPalette::Highlight);
				const auto fill      = QColor::fromRgbF(base.redF() * 0.84F + highlight.redF() * 0.16F, base.greenF() * 0.84F + highlight.greenF() * 0.16F, base.blueF() * 0.84F + highlight.blueF() * 0.16F);
				expect(qt::destructive_contrast(colours.color(QPalette::Active, QPalette::ButtonText), fill) >= 4.5, "Check status's words read on its tint");
				expect(editor.findChild<QLabel*>(QStringLiteral("wholeReviewWaitingMark"))->pixmap().isNull() == false, "the panel carries the page's unknown mark");
			}
			editor.setPalette(QApplication::palette());
		}
		// Readable in every theme: the line is drawn in the page's own text
		// colour, as the status is.
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(mode);
			editor.setPalette(profile.derive(QApplication::palette()));
			QApplication::processEvents();
			const auto& palette = editor.unknown_reason_label()->palette();
			const auto  ink     = palette.color(QPalette::Active, editor.unknown_reason_label()->foregroundRole());
			const auto  paper   = palette.color(QPalette::Active, QPalette::Window);
			expect(editor.unknown_reason_label()->foregroundRole() == editor.status_label()->foregroundRole()
					   && ink == editor.status_label()->palette().color(QPalette::Active, editor.status_label()->foregroundRole())
					   && qt::destructive_contrast(ink, paper) >= 4.5,
				   "the reason is drawn as the status is, readable on the page");
		}
		editor.setPalette(QApplication::palette());
		// Negative controls: no reason, or a review not awaiting confirmation,
		// shows none.
		unknown.Unknown.reset();
		editor.present(unknown);
		expect(editor.unknown_reason_label()->isHidden() && editor.unknown_reason_label()->text().isEmpty() && editor.status_label()->accessibleDescription().isEmpty(), "a confirmed or cleared review shows no reason");
		auto confirmedProjection     = unknown;
		auto confirmed               = awaiting;
		confirmed.State              = core::review_submission_state::submitted;
		confirmedProjection.Draft    = confirmed;
		confirmedProjection.Controls = core::controls(confirmed);
		editor.present(confirmedProjection);
		expect(editor.waiting_panel()->isHidden(), "a confirmed review shows no waiting panel");
		auto drafting     = projection;
		drafting.Draft    = draft;
		drafting.Controls = core::controls(draft);
		drafting.Unknown  = core::notice(diagnose(core::review_submission_unknown_reason::timeout));
		editor.present(drafting);
		expect(editor.unknown_reason_label()->isHidden() && editor.waiting_panel()->isHidden() && !editor.status_label()->isHidden(), "a draft being written never shows a reason or the waiting panel");
	}
	editor.present(projection);
	expect(editor.title_label()->text() == QStringLiteral("Compose review") && !editor.pending_list()->isVisible() && !editor.carries_label()->isVisible()
			   && editor.discard_action()->text() == QStringLiteral("Discard"),
		   "a review with no pending comment is Compose review as before");
	std::cout << "review-submission-editor:pass scale=" << editor.devicePixelRatioF() << '\n';
	return EXIT_SUCCESS;
}
