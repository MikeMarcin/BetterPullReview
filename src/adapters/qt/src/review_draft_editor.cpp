#include "bpr/qt/review_draft_editor.hpp"

#include <QAction>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief Creates one named action owned by the editor.
		///
		/// \details A `QAction` is not a widget and carries no accessible name
		/// of its own; assistive technology reads its text. The text is
		/// therefore the identity a person hears, and each one here is
		/// deliberately distinct — "Accept file" and "Exclude file" would be
		/// ambiguous as "Accept" and "Exclude" beside a file list. The object
		/// name is set as well so a test can find the action without matching
		/// on user-visible text.
		[[nodiscard]] auto make_action(
			QWidget*       aParent,
			const QString& aText,
			const QString& aObjectName) -> QAction*
		{
			auto* const action = new QAction{ aText, aParent };
			action->setObjectName(aObjectName);
			aParent->addAction(action);
			return action;
		}

		/// \brief Creates one read-only workshop surface owned by the editor.
		///
		/// \details Plain text, and deliberately so: no Markdown rendering
		/// surface exists in this tree, and adding one for this view would be
		/// a renderer nobody else uses. The decision is recorded in
		/// `docs/verification/RFC-BPR-0012.md`.
		[[nodiscard]] auto make_workshop_surface(
			QWidget*       aParent,
			const QString& aObjectName,
			const QString& aAccessibleName) -> QPlainTextEdit*
		{
			auto* const surface = new QPlainTextEdit{ aParent };
			surface->setObjectName(aObjectName);
			surface->setAccessibleName(aAccessibleName);
			surface->setReadOnly(true);
			return surface;
		}
	} // namespace

	review_draft_editor::review_draft_editor(
		draft_dispatch   aDraftDispatch,
		receipt_dispatch aReceiptDispatch,
		QWidget* const   aParent)
		: QWidget{ aParent }
	{
		Members.DraftDispatch   = std::move(aDraftDispatch);
		Members.ReceiptDispatch = std::move(aReceiptDispatch);

		auto* const layout = new QVBoxLayout{ this };

		Members.TargetLabel = new QLabel{ this };
		Members.TargetLabel->setObjectName(QStringLiteral("reviewPublishTarget"));
		Members.TargetLabel->setAccessibleName(
			QStringLiteral("Publication target"));
		Members.TargetLabel->setWordWrap(true);
		layout->addWidget(Members.TargetLabel);
		Members.MutationStatus = new QLabel{ this };
		Members.MutationStatus->setObjectName(
			QStringLiteral("reviewMutationStatus"));
		Members.MutationStatus->setAccessibleName(
			QStringLiteral("Review publication status"));
		Members.MutationStatus->setWordWrap(true);
		layout->addWidget(Members.MutationStatus);

		Members.Body = new QPlainTextEdit{ this };
		Members.Body->setObjectName(QStringLiteral("reviewDraftBody"));
		Members.Body->setAccessibleName(QStringLiteral("Review comment body"));
		Members.Body->setAccessibleDescription(QStringLiteral(
			"Edit the selected semantic source-line review comment."));
		layout->addWidget(Members.Body);
		connect(Members.Body, &QPlainTextEdit::textChanged, this, [this] {
			if (!Members.ApplyingPresentation) {
				emit_draft(core::review_draft_command_kind::edit);
			}
		});

		// Two surfaces, side by side and never the same one. A workshop that
		// showed the proposal in the original's place would have destroyed
		// the thing the reviewer is being asked to compare it against.
		Members.WorkshopOriginal = make_workshop_surface(
			this,
			QStringLiteral("reviewWorkshopOriginal"),
			QStringLiteral("Your original comment"));
		Members.WorkshopProposal = make_workshop_surface(
			this,
			QStringLiteral("reviewWorkshopProposal"),
			QStringLiteral("Proposed revision"));
		layout->addWidget(Members.WorkshopOriginal);
		layout->addWidget(Members.WorkshopProposal);

		Members.Publish
			= make_action(this, tr("Publish"), QStringLiteral("reviewPublish"));
		Members.Discard
			= make_action(this, tr("Discard"), QStringLiteral("reviewDiscard"));
		Members.MarkRead = make_action(
			this, tr("Mark read"), QStringLiteral("reviewMarkRead"));
		Members.Accept = make_action(
			this, tr("Accept file"), QStringLiteral("reviewAccept"));
		Members.Exclude = make_action(
			this, tr("Exclude file"), QStringLiteral("reviewExclude"));
		Members.AcceptRevision = make_action(
			this,
			tr("Accept revision"),
			QStringLiteral("reviewAcceptRevision"));
		Members.RejectRevision = make_action(
			this,
			tr("Reject revision"),
			QStringLiteral("reviewRejectRevision"));

		connect(Members.Publish, &QAction::triggered, this, [this]() {
			emit_draft(core::review_draft_command_kind::begin_publish);
		});
		connect(Members.Discard, &QAction::triggered, this, [this]() {
			emit_draft(core::review_draft_command_kind::discard);
		});
		connect(Members.MarkRead, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::mark_read);
		});
		connect(Members.Accept, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::accept);
		});
		connect(Members.Exclude, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::exclude);
		});
		connect(Members.AcceptRevision, &QAction::triggered, this, [this]() {
			accept_revision();
		});
		connect(Members.RejectRevision, &QAction::triggered, this, [this]() {
			reject_revision();
		});

		refresh_enablement();
	}

	auto review_draft_editor::body_editor() const noexcept -> QPlainTextEdit*
	{
		return Members.Body;
	}

	auto review_draft_editor::target_label() const noexcept -> QLabel*
	{
		return Members.TargetLabel;
	}

	auto review_draft_editor::mutation_status_label() const noexcept -> QLabel*
	{
		return Members.MutationStatus;
	}

	auto review_draft_editor::mutation_status_text() const -> QString
	{
		return Members.MutationStatus->text();
	}

	auto review_draft_editor::publish_action() const noexcept -> QAction*
	{
		return Members.Publish;
	}

	auto review_draft_editor::discard_action() const noexcept -> QAction*
	{
		return Members.Discard;
	}

	auto review_draft_editor::mark_read_action() const noexcept -> QAction*
	{
		return Members.MarkRead;
	}

	auto review_draft_editor::accept_action() const noexcept -> QAction*
	{
		return Members.Accept;
	}

	auto review_draft_editor::exclude_action() const noexcept -> QAction*
	{
		return Members.Exclude;
	}

	auto review_draft_editor::workshop_original_editor() const noexcept
		-> QPlainTextEdit*
	{
		return Members.WorkshopOriginal;
	}

	auto review_draft_editor::workshop_proposal_editor() const noexcept
		-> QPlainTextEdit*
	{
		return Members.WorkshopProposal;
	}

	auto review_draft_editor::workshop_original_text() const -> QString
	{
		return Members.WorkshopOriginal->toPlainText();
	}

	auto review_draft_editor::workshop_proposal_text() const -> QString
	{
		return Members.WorkshopProposal->toPlainText();
	}

	auto review_draft_editor::accept_revision_action() const noexcept -> QAction*
	{
		return Members.AcceptRevision;
	}

	auto review_draft_editor::reject_revision_action() const noexcept -> QAction*
	{
		return Members.RejectRevision;
	}

	auto review_draft_editor::shown_draft() const noexcept
		-> const std::optional<core::review_comment_draft>&
	{
		return Members.ShownDraft;
	}

	auto review_draft_editor::shown_receipt() const noexcept
		-> const std::optional<core::file_receipt>&
	{
		return Members.ShownReceipt;
	}

	auto review_draft_editor::shown_proposal() const noexcept
		-> const std::optional<comment_workshop_proposal>&
	{
		return Members.ShownProposal;
	}

	auto review_draft_editor::statistics() const noexcept
		-> review_draft_editor_statistics
	{
		return Members.Statistics;
	}

	void review_draft_editor::describe_target(const QString& aDescription)
	{
		Members.TargetLabel->setText(aDescription);
	}

	void review_draft_editor::emit_draft(
		const core::review_draft_command_kind aKind)
	{
		if (!Members.ShownDraft || !Members.DraftDispatch) {
			return;
		}
		++Members.Statistics.DraftIntents;
		Members.DraftDispatch(core::review_draft_command{
			.Id   = Members.ShownDraft->Id,
			.Kind = aKind,
			.Body = Members.Body->toPlainText().toStdString(),
		});
	}

	void review_draft_editor::emit_receipt(
		const core::file_receipt_command_kind aKind)
	{
		if (!Members.ShownReceipt || !Members.ReceiptDispatch) {
			return;
		}
		++Members.Statistics.ReceiptIntents;
		Members.ReceiptDispatch(core::file_receipt_command{
			.File     = Members.ShownReceipt->File,
			.Material = Members.ShownReceipt->Material,
			.Kind     = aKind,
		});
	}

	void review_draft_editor::accept_revision()
	{
		if (!Members.ShownProposal || !Members.ShownDraft
			|| !Members.DraftDispatch
			|| Members.ShownDraft->Id != Members.ShownProposal->Draft) {
			return;
		}
		++Members.Statistics.DraftIntents;
		// One command, and nothing else. The body is deliberately not written
		// here: the editor moves when an accepted edit projection comes back,
		// and writing it would both pre-empt that answer and emit a second
		// command through the body's own change signal.
		Members.DraftDispatch(core::review_draft_command{
			.Id   = Members.ShownProposal->Draft,
			.Kind = core::review_draft_command_kind::edit,
			.Body = Members.ShownProposal->Proposed,
		});
	}

	void review_draft_editor::reject_revision()
	{
		if (!Members.ShownProposal) {
			return;
		}
		++Members.Statistics.RejectedRevisions;
		// The reviewer's own text is never touched on this path, which is what
		// makes rejection leave it byte-identical rather than restore it.
		clear_workshop();
		refresh_enablement();
	}

	void review_draft_editor::clear_workshop()
	{
		Members.ShownProposal.reset();
		Members.WorkshopOriginal->clear();
		Members.WorkshopProposal->clear();
	}

	void review_draft_editor::present(const core::review_comment_draft& aDraft)
	{
		++Members.Statistics.Presentations;
		Members.ShownDraft = aDraft;
		if (Members.Body->toPlainText().toStdString() != aDraft.Body) {
			Members.ApplyingPresentation = true;
			Members.Body->setPlainText(QString::fromStdString(aDraft.Body));
			Members.ApplyingPresentation = false;
		}
		refresh_enablement();
	}

	void review_draft_editor::present(
		const comment_workshop_proposal& aProposal)
	{
		++Members.Statistics.WorkshopPresentations;
		Members.ShownProposal = aProposal;
		Members.WorkshopOriginal->setPlainText(
			QString::fromStdString(aProposal.Original));
		Members.WorkshopProposal->setPlainText(
			QString::fromStdString(aProposal.Proposed));
		refresh_enablement();
	}

	void review_draft_editor::present(
		const application::review_provider_capabilities aCapabilities)
	{
		++Members.Statistics.MutationUpdates;
		if (aCapabilities.ResolvesThreads && aCapabilities.RecordsViewedState) {
			Members.MutationStatus->setText(
				QStringLiteral("Review publication is ready."));
			return;
		}
		Members.MutationStatus->setText(QStringLiteral(
			"Publication is available; native thread resolution and viewed-state "
			"recording are unavailable."));
	}

	void review_draft_editor::present(
		const application::review_mutation_failure aFailure)
	{
		++Members.Statistics.MutationUpdates;
		switch (aFailure) {
		case application::review_mutation_failure::unavailable:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication result is unavailable; retry is safe."));
			break;
		case application::review_mutation_failure::refused_by_server:
			Members.MutationStatus->setText(
				QStringLiteral("The server refused publication."));
			break;
		case application::review_mutation_failure::undesignated_target:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication was refused because the pull request changed."));
			break;
		case application::review_mutation_failure::target_moved:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication target can no longer be named."));
			break;
		case application::review_mutation_failure::stale_head:
			Members.MutationStatus->setText(QStringLiteral(
				"Publication was refused because the pull request head moved."));
			break;
		case application::review_mutation_failure::unsupported:
			Members.MutationStatus->setText(QStringLiteral(
				"This provider does not support publication."));
			break;
		case application::review_mutation_failure::cancelled:
			Members.MutationStatus->setText(
				QStringLiteral("Publication was cancelled."));
			break;
		}
	}

	void review_draft_editor::present(
		const application::review_mutation_receipt& aReceipt,
		const application::reconciliation_result&   aReconciliation)
	{
		++Members.Statistics.MutationUpdates;
		Members.MutationStatus->setText(
			QStringLiteral("Published %1%2; refreshed %3 comment(s).")
				.arg(QString::fromStdString(aReceipt.ServerCommentId))
				.arg(aReceipt.Duplicate ? QStringLiteral(" by safe retry")
										: QString{ })
				.arg(aReconciliation.Projection.size()));
	}

	void review_draft_editor::present(
		const review_draft_editor_clear_reason aReason)
	{
		++Members.Statistics.Presentations;
		Members.ShownDraft.reset();
		Members.ShownReceipt.reset();
		Members.ApplyingPresentation = true;
		Members.Body->clear();
		Members.ApplyingPresentation = false;
		// A proposal is about a draft that is no longer selected, so it goes
		// with it rather than sitting beside a body nobody is editing.
		clear_workshop();
		switch (aReason) {
		case review_draft_editor_clear_reason::selection_cleared:
			Members.MutationStatus->setText(QStringLiteral(
				"Select a current-head diff line to compose a review comment."));
			break;
		case review_draft_editor_clear_reason::selection_invalidated:
			Members.MutationStatus->setText(
				QStringLiteral("The selected diff is no longer available."));
			break;
		case review_draft_editor_clear_reason::composition_retired:
			Members.TargetLabel->clear();
			Members.MutationStatus->clear();
			break;
		}
		refresh_enablement();
	}

	void review_draft_editor::present(const core::file_receipt& aReceipt)
	{
		++Members.Statistics.Presentations;
		Members.ShownReceipt = aReceipt;
		refresh_enablement();
	}

	void review_draft_editor::refresh_enablement()
	{
		// Enablement follows the accepted projection, never the local edit.
		// A publish button enabled because the user typed something would be
		// the widget deciding legality the core owns.
		const auto drafting = Members.ShownDraft
			&& Members.ShownDraft->State == core::review_draft_state::drafting;
		Members.Publish->setEnabled(drafting);
		Members.Discard->setEnabled(drafting);
		Members.Body->setReadOnly(!drafting);

		const auto receipt = Members.ShownReceipt.has_value();
		Members.MarkRead->setEnabled(receipt);
		Members.Accept->setEnabled(receipt && !Members.ShownReceipt->Excluded);
		Members.Exclude->setEnabled(receipt && !Members.ShownReceipt->Accepted);

		// A revision may only be accepted onto the draft it was proposed
		// against, and only while that draft is still the reviewer's to edit.
		// Rejecting is always available, because dropping a proposal asks
		// nothing of anybody.
		const auto proposed = Members.ShownProposal.has_value();
		Members.AcceptRevision->setEnabled(
			proposed && drafting
			&& Members.ShownDraft->Id == Members.ShownProposal->Draft);
		Members.RejectRevision->setEnabled(proposed);
	}
} // namespace bpr::qt
