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
	} // namespace

	review_draft_editor::review_draft_editor(
		draft_dispatch   aDraftDispatch,
		receipt_dispatch aReceiptDispatch,
		QWidget* const   aParent)
		: QWidget{ aParent }
		, DraftDispatch{ std::move(aDraftDispatch) }
		, ReceiptDispatch{ std::move(aReceiptDispatch) }
	{
		auto* const layout = new QVBoxLayout{ this };

		TargetLabel = new QLabel{ this };
		TargetLabel->setObjectName(QStringLiteral("reviewPublishTarget"));
		TargetLabel->setAccessibleName(
			QStringLiteral("Publication target"));
		TargetLabel->setWordWrap(true);
		layout->addWidget(TargetLabel);

		Body = new QPlainTextEdit{ this };
		Body->setObjectName(QStringLiteral("reviewDraftBody"));
		Body->setAccessibleName(QStringLiteral("Review comment body"));
		layout->addWidget(Body);

		Publish  = make_action(this, tr("Publish"), QStringLiteral("reviewPublish"));
		Discard  = make_action(this, tr("Discard"), QStringLiteral("reviewDiscard"));
		MarkRead = make_action(this, tr("Mark read"), QStringLiteral("reviewMarkRead"));
		Accept   = make_action(this, tr("Accept file"), QStringLiteral("reviewAccept"));
		Exclude  = make_action(this, tr("Exclude file"), QStringLiteral("reviewExclude"));

		connect(Publish, &QAction::triggered, this, [this]() {
			emit_draft(core::review_draft_command_kind::begin_publish);
		});
		connect(Discard, &QAction::triggered, this, [this]() {
			emit_draft(core::review_draft_command_kind::discard);
		});
		connect(MarkRead, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::mark_read);
		});
		connect(Accept, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::accept);
		});
		connect(Exclude, &QAction::triggered, this, [this]() {
			emit_receipt(core::file_receipt_command_kind::exclude);
		});

		refresh_enablement();
	}

	auto review_draft_editor::body_editor() const noexcept -> QPlainTextEdit*
	{
		return Body;
	}

	auto review_draft_editor::target_label() const noexcept -> QLabel*
	{
		return TargetLabel;
	}

	auto review_draft_editor::publish_action() const noexcept -> QAction*
	{
		return Publish;
	}

	auto review_draft_editor::discard_action() const noexcept -> QAction*
	{
		return Discard;
	}

	auto review_draft_editor::mark_read_action() const noexcept -> QAction*
	{
		return MarkRead;
	}

	auto review_draft_editor::accept_action() const noexcept -> QAction*
	{
		return Accept;
	}

	auto review_draft_editor::exclude_action() const noexcept -> QAction*
	{
		return Exclude;
	}

	auto review_draft_editor::shown_draft() const noexcept
		-> const std::optional<core::review_comment_draft>&
	{
		return ShownDraft;
	}

	auto review_draft_editor::shown_receipt() const noexcept
		-> const std::optional<core::file_receipt>&
	{
		return ShownReceipt;
	}

	auto review_draft_editor::statistics() const noexcept
		-> review_draft_editor_statistics
	{
		return Statistics;
	}

	void review_draft_editor::describe_target(const QString& aDescription)
	{
		TargetLabel->setText(aDescription);
	}

	void review_draft_editor::emit_draft(
		const core::review_draft_command_kind aKind)
	{
		if (!ShownDraft || !DraftDispatch) {
			return;
		}
		++Statistics.DraftIntents;
		DraftDispatch(core::review_draft_command{
			.Id   = ShownDraft->Id,
			.Kind = aKind,
			.Body = Body->toPlainText().toStdString(),
		});
	}

	void review_draft_editor::emit_receipt(
		const core::file_receipt_command_kind aKind)
	{
		if (!ShownReceipt || !ReceiptDispatch) {
			return;
		}
		++Statistics.ReceiptIntents;
		ReceiptDispatch(core::file_receipt_command{
			.File     = ShownReceipt->File,
			.Material = ShownReceipt->Material,
			.Kind     = aKind,
		});
	}

	void review_draft_editor::present(const core::review_comment_draft& aDraft)
	{
		++Statistics.Presentations;
		ShownDraft = aDraft;
		if (Body->toPlainText().toStdString() != aDraft.Body) {
			Body->setPlainText(QString::fromStdString(aDraft.Body));
		}
		refresh_enablement();
	}

	void review_draft_editor::present(const core::file_receipt& aReceipt)
	{
		++Statistics.Presentations;
		ShownReceipt = aReceipt;
		refresh_enablement();
	}

	void review_draft_editor::refresh_enablement()
	{
		// Enablement follows the accepted projection, never the local edit.
		// A publish button enabled because the user typed something would be
		// the widget deciding legality the core owns.
		const auto drafting = ShownDraft
			&& ShownDraft->State == core::review_draft_state::drafting;
		Publish->setEnabled(drafting);
		Discard->setEnabled(drafting);
		Body->setReadOnly(!drafting);

		const auto receipt = ShownReceipt.has_value();
		MarkRead->setEnabled(receipt);
		Accept->setEnabled(receipt && !ShownReceipt->Excluded);
		Exclude->setEnabled(receipt && !ShownReceipt->Accepted);
	}
} // namespace bpr::qt
