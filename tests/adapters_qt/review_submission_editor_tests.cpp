#include "bpr/qt/review_submission_editor.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
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
	editor.present(projection);
	editor.resize(300, 550);
	editor.show();
	QApplication::processEvents();
	expect(intents.empty(), "projection emits no gesture");
	expect(editor.body_editor()->height() >= 160 && editor.body_editor()->maximumHeight() == QWIDGETSIZE_MAX,
		   "summary keeps a usable viewport and can grow with its dedicated page");
	expect(editor.target_editor()->isReadOnly() && !editor.target_editor()->acceptRichText()
			   && editor.target_editor()->wordWrapMode() == QTextOption::WrapAtWordBoundaryOrAnywhere
			   && editor.target_editor()->toPlainText() == projection.TargetDescription,
		   "full identity wraps without modifying copied text");
	expect(editor.body_editor()->accessibleName() == QStringLiteral("Review summary") && editor.disposition_editor()->accessibleName() == QStringLiteral("Review type"), "named accessible inputs");
	expect(editor.status_label()->textFormat() == Qt::PlainText && editor.status_label()->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard), "outcomes are literal and keyboard copyable");
	editor.body_editor()->setFocus();
	editor.body_editor()->moveCursor(QTextCursor::End);
	const auto cursor = editor.body_editor()->textCursor().position();
	editor.present(projection);
	expect(editor.body_editor()->hasFocus() && editor.body_editor()->textCursor().position() == cursor, "unchanged projection preserves focus and cursor");
	editor.body_editor()->setPlainText(QStringLiteral("Edited summary"));
	expect(intents.size() == 1U && intents.back().Command.Kind == core::review_submission_command_kind::edit && intents.back().Command.Body == "Edited summary", "summary emits exact semantic edit");
	expect(editor.shown_draft() == draft, "editing does not optimistically replace decided draft");
	editor.disposition_editor()->setCurrentIndex(1);
	expect(intents.back().Command.Disposition == core::review_disposition::approve, "choice emits typed disposition");
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
	std::cout << "review-submission-editor:pass scale=" << editor.devicePixelRatioF() << '\n';
	return EXIT_SUCCESS;
}
