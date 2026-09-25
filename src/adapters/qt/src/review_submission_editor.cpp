#include "bpr/qt/review_submission_editor.hpp"

#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include <utility>

namespace bpr::qt {
	review_submission_editor::review_submission_editor(review_submission_editor_dispatch aDispatch, QWidget* aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		setObjectName(QStringLiteral("wholeReviewEditor"));
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		auto* title = new QLabel{ tr("Review pull request"), this };
		layout->addWidget(title);
		Target = new QTextEdit{ this };
		Target->setObjectName(QStringLiteral("wholeReviewTarget"));
		Target->setReadOnly(true);
		Target->setAcceptRichText(false);
		Target->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		Target->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Target->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Target->setFrameShape(QFrame::NoFrame);
		Target->setAccessibleName(tr("Review target and acting account"));
		Target->setMinimumWidth(0);
		Target->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		connect(Target->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
				this, [this](QSizeF aSize) { Target->setFixedHeight(static_cast<int>(std::ceil(aSize.height())) + 2 * Target->frameWidth()); });

		layout->addWidget(Target);
		Disposition = new QComboBox{ this };
		Disposition->setObjectName(QStringLiteral("wholeReviewDisposition"));
		Disposition->setAccessibleName(tr("Review type"));
		Disposition->addItem(tr("Comment"), static_cast<int>(core::review_disposition::comment));
		Disposition->addItem(tr("Approve"), static_cast<int>(core::review_disposition::approve));
		Disposition->addItem(tr("Request changes"), static_cast<int>(core::review_disposition::request_changes));
		layout->addWidget(Disposition);
		Body = new QPlainTextEdit{ this };
		Body->setObjectName(QStringLiteral("wholeReviewSummary"));
		Body->setAccessibleName(tr("Review summary"));
		Body->setPlaceholderText(tr("Write a review summary…"));
		Body->setTabChangesFocus(true);
		Body->setMinimumHeight(160);
		layout->addWidget(Body, 1);
		Submit         = new QAction{ tr("Submit review"), this };
		Discard        = new QAction{ tr("Discard"), this };
		Reconcile      = new QAction{ tr("Check status"), this };
		NewReview      = new QAction{ tr("New review"), this };
		const auto row = [this, layout](QAction* aFirst, QAction* aSecond) {
			auto* actions = new QHBoxLayout;
			for (auto* action : { aFirst, aSecond }) {
				auto* button = new QToolButton{ this };
				button->setDefaultAction(action);
				button->setFocusPolicy(Qt::StrongFocus);
				button->setAccessibleName(action->text());
				actions->addWidget(button);
			}
			actions->addStretch();
			layout->addLayout(actions);
		};
		row(Submit, Discard);
		row(Reconcile, NewReview);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("wholeReviewStatus"));
		Status->setAccessibleName(tr("Review submission status"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Status->setMinimumWidth(0);
		Status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->addWidget(Status);
		connect(Body, &QPlainTextEdit::textChanged, this, [this] { emit_command(core::review_submission_command_kind::edit); });
		connect(Disposition, &QComboBox::currentIndexChanged, this, [this] { emit_command(core::review_submission_command_kind::choose_disposition); });
		connect(Submit, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::begin_submit); });
		connect(Discard, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::discard); });
		connect(Reconcile, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::reconcile); });
		connect(NewReview, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::new_review); });
		setFocusProxy(Body);
		present({ .Status = tr("Open a pull request to write a review.") });
	}
	void review_submission_editor::present(const review_submission_editor_projection& aProjection)
	{
		const QSignalBlocker bodyBlock{ Body };
		const QSignalBlocker dispositionBlock{ Disposition };
		Draft           = aProjection.Draft;
		const auto text = Draft ? QString::fromUtf8(Draft->Body) : QString{ };
		if (Body->toPlainText() != text) {
			Body->setPlainText(text);
		}
		if (Draft) {
			Disposition->setCurrentIndex(Disposition->findData(static_cast<int>(Draft->Disposition)));
		}
		if (Target->toPlainText() != aProjection.TargetDescription) {
			Target->setPlainText(aProjection.TargetDescription);
		}
		Status->setText(aProjection.Status);
		const auto connected = Draft.has_value() && static_cast<bool>(Dispatch);
		Body->setReadOnly(!connected || !aProjection.Controls.Edit);
		Disposition->setEnabled(connected && aProjection.Controls.Edit);
		Submit->setEnabled(connected && aProjection.Controls.Submit);
		Discard->setEnabled(connected && aProjection.Controls.Discard);
		Reconcile->setEnabled(connected && aProjection.Controls.Reconcile);
		NewReview->setEnabled(connected && aProjection.Controls.NewReview);
		const auto announcement = aProjection.TargetDescription + QStringLiteral("\n") + Disposition->currentText() + QStringLiteral(". ") + aProjection.Status;
		if (announcement != LastAnnouncement) {
			LastAnnouncement = announcement;
			QAccessibleAnnouncementEvent event{ this, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		if (PresentationObserver) {
			PresentationObserver();
		}
	}
	void review_submission_editor::set_presentation_observer(std::function<void()> aObserver)
	{
		PresentationObserver = std::move(aObserver);
	}
	void review_submission_editor::emit_command(core::review_submission_command_kind aKind)
	{
		if (!Draft || !Dispatch) {
			return;
		}
		auto command = core::review_submission_command{ .Id = Draft->Id, .Kind = aKind };
		if (aKind == core::review_submission_command_kind::edit) {
			command.Body = Body->toPlainText().toUtf8().toStdString();
		}
		if (aKind == core::review_submission_command_kind::choose_disposition) {
			command.Disposition = static_cast<core::review_disposition>(Disposition->currentData().toInt());
		}
		Dispatch({ .Command = std::move(command) });
	}
	void review_submission_editor::emit_operation(review_submission_editor_operation aOperation)
	{
		if (Draft && Dispatch) {
			Dispatch({ .Operation = aOperation, .Command = { .Id = Draft->Id } });
		}
	}
	auto review_submission_editor::body_editor() const noexcept -> QPlainTextEdit* { return Body; }
	auto review_submission_editor::disposition_editor() const noexcept -> QComboBox* { return Disposition; }
	auto review_submission_editor::submit_action() const noexcept -> QAction* { return Submit; }
	auto review_submission_editor::discard_action() const noexcept -> QAction* { return Discard; }
	auto review_submission_editor::reconcile_action() const noexcept -> QAction* { return Reconcile; }
	auto review_submission_editor::new_review_action() const noexcept -> QAction* { return NewReview; }
	auto review_submission_editor::status_label() const noexcept -> QLabel* { return Status; }
	auto review_submission_editor::target_editor() const noexcept -> QTextEdit* { return Target; }
	auto review_submission_editor::shown_draft() const noexcept -> const std::optional<core::review_submission_draft>& { return Draft; }
}
