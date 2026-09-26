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
		auto* title = new QLabel{ tr("Compose review"), this };
		layout->addWidget(title);
		auto* purpose = new QLabel{ tr("Write a summary for this pull request and choose Comment, Approve, or Request changes. Nothing is published until you choose Submit review."), this };
		purpose->setObjectName(QStringLiteral("wholeReviewPurpose"));
		purpose->setWordWrap(true);
		purpose->setMinimumWidth(0);
		purpose->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->addWidget(purpose);
		DraftState = new QLabel{ this };
		DraftState->setObjectName(QStringLiteral("wholeReviewDraftState"));
		DraftState->setWordWrap(true);
		layout->addWidget(DraftState);
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
		layout->insertWidget(3, Status);
		layout->addStretch();
		connect(Body, &QPlainTextEdit::textChanged, this, [this] { emit_command(core::review_submission_command_kind::edit); });
		connect(Disposition, &QComboBox::currentIndexChanged, this, [this] { emit_command(core::review_submission_command_kind::choose_disposition); });
		connect(Submit, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::begin_submit); });
		connect(Discard, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::discard); });
		connect(Reconcile, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::reconcile); });
		connect(NewReview, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::new_review); });
		setFocusProxy(Body);
		present({ .Status = tr("Sign in and open an active pull request to write a review. Back returns to the pull request.") });
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
		Target->setVisible(!aProjection.TargetDescription.isEmpty());
		Body->setVisible(Draft.has_value());
		Disposition->setVisible(Draft.has_value());
		QString state;
		if (!Draft) {
			state = tr("No review draft is available.");
		} else {
			switch (Draft->State) {
			case core::review_submission_state::drafting:
				state = core::kept(*Draft) ? tr("Your review draft is kept. It has not been published.") : tr("Start a review draft. Write your summary below.");
				break;
			case core::review_submission_state::submitting:
				state = tr("Submission is in progress or awaiting confirmation. Check status before trying again.");
				break;
			case core::review_submission_state::submitted:
				state = tr("This review was submitted. Choose New review to write another.");
				break;
			case core::review_submission_state::discarded:
				state = tr("This draft was discarded. Choose New review to start again.");
				break;
			}
		}
		DraftState->setText(state);
		Status->setText(aProjection.Status);
		const auto connected = Draft.has_value() && static_cast<bool>(Dispatch);
		Body->setReadOnly(!connected || !aProjection.Controls.Edit);
		Disposition->setEnabled(connected && aProjection.Controls.Edit);
		Submit->setEnabled(connected && aProjection.Controls.Submit);
		Discard->setEnabled(connected && aProjection.Controls.Discard);
		Reconcile->setEnabled(connected && aProjection.Controls.Reconcile);
		NewReview->setEnabled(connected && aProjection.Controls.NewReview);
		const auto unavailable = !Draft || !aProjection.Controls.Edit
			? aProjection.Status
			: tr("Write a nonblank summary for Comment or Request changes; Approve may have an empty summary.");
		Body->setToolTip(Body->isReadOnly() ? aProjection.Status : tr("Your summary is kept as a draft until you submit it."));
		Disposition->setToolTip(Disposition->isEnabled() ? tr("Choose the review you want to submit.") : aProjection.Status);
		Submit->setToolTip(Submit->isEnabled() ? tr("Publish this review with the displayed account and revision.") : unavailable);
		Discard->setToolTip(Discard->isEnabled() ? tr("Discard this unpublished review draft.") : tr("Only an editable, unpublished draft can be discarded. %1").arg(aProjection.Status));
		Reconcile->setToolTip(Reconcile->isEnabled() ? tr("Check whether this exact review was submitted.") : tr("Check status is available only for a review awaiting confirmation."));
		NewReview->setToolTip(NewReview->isEnabled() ? tr("Start a new draft for the current pull request revision.") : tr("Finish or discard the current draft before starting another. %1").arg(aProjection.Status));
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
