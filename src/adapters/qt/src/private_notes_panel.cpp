#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include <QAction>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace bpr::qt {
	private_notes_panel::private_notes_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("privateNotesPanel"));
		setAccessibleName(tr("Private notes"));
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		auto* heading = new QLabel{ tr("Private notes"), this };
		heading->setTextFormat(Qt::PlainText);
		layout->addWidget(heading);
		Notes = new QListWidget{ this };
		Notes->setObjectName(QStringLiteral("privateNotesList"));
		Notes->setAccessibleName(tr("This pull request's private notes, newest first"));
		Notes->setMinimumHeight(90);
		Notes->setMaximumHeight(160);
		layout->addWidget(Notes);
		const auto action = [&](const QString& aText, const QString& aName) {
			auto* result = new QAction{ aText, this };
			result->setObjectName(aName);
			auto* button = new QToolButton{ this };
			button->setDefaultAction(result);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setAccessibleName(aText);
			button->setAccessibleDescription(aText);
			layout->addWidget(button);
			return result;
		};
		NewReview  = action(tr("New pull request note"), QStringLiteral("newReviewNoteAction"));
		NewFile    = action(tr("New current-file note"), QStringLiteral("newFileNoteAction"));
		Attachment = new QLabel{ this };
		Attachment->setTextFormat(Qt::PlainText);
		Attachment->setWordWrap(true);
		Attachment->setAccessibleName(tr("Note attachment"));
		layout->addWidget(Attachment);
		Body = new QPlainTextEdit{ this };
		Body->setObjectName(QStringLiteral("privateNoteBody"));
		Body->setAccessibleName(tr("Private note text"));
		Body->setPlaceholderText(tr("Write a note for yourself"));
		Body->setMinimumHeight(90);
		Body->setMaximumHeight(180);
		layout->addWidget(Body);
		Save    = action(tr("Save note"), QStringLiteral("savePrivateNoteAction"));
		Promote = action(tr("Promote to draft"), QStringLiteral("promotePrivateNoteAction"));
		Erase   = action(tr("Delete note"), QStringLiteral("deletePrivateNoteAction"));
		Discard = action(tr("Discard changes"), QStringLiteral("discardPrivateNoteChangesAction"));
		Status  = new QLabel{ this };
		Status->setObjectName(QStringLiteral("privateNoteStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setAccessibleName(tr("Private note status"));
		layout->addWidget(Status);
		connect(Notes, &QListWidget::currentRowChanged, this, [this] { select_note(); });
		connect(Body, &QPlainTextEdit::textChanged, this, [this] {
			if (!Applying) {
				Dirty = Creating || (Editing && Body->toPlainText().toStdString() != Editing->Text);
				refresh_actions();
			}
		});
		connect(NewReview, &QAction::triggered, this, [this] { start_note(false); });
		connect(NewFile, &QAction::triggered, this, [this] { start_note(true); });
		connect(Save, &QAction::triggered, this, [this] { emit_intent(private_note_action::save); });
		connect(Erase, &QAction::triggered, this, [this] { emit_intent(private_note_action::erase); });
		connect(Promote, &QAction::triggered, this, [this] { emit_intent(private_note_action::promote); });
		connect(Discard, &QAction::triggered, this, [this] {
			Dirty    = false;
			Creating = false;
			NewTarget.reset();
			Status->clear();
			if (Dispatch) {
				Dispatch({ .Scope = Shown.Scope, .Action = private_note_action::discard });
			}
			load_editor();
			refresh_actions();
		});
		present({ });
	}
	void private_notes_panel::set_dispatch(private_note_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void private_notes_panel::present(private_notes_projection aProjection)
	{
		const auto changedScope = Shown.Scope != aProjection.Scope;
		const auto changedRows  = changedScope || Shown.Notes != aProjection.Notes;
		Shown                   = std::move(aProjection);
		if (changedScope) {
			Selected = 0U;
			Editing.reset();
			Creating = false;
			Dirty    = false;
			NewTarget.reset();
			Status->clear();
		}
		if (changedRows) {
			const auto blocker = QSignalBlocker{ Notes };
			Notes->clear();
			for (const auto& note : Shown.Notes) {
				auto* item = new QListWidgetItem{ tr("Note #%1 · %2").arg(note.Id).arg(QString::fromStdString(note.Label)), Notes };
				item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(note.Id));
				item->setToolTip(QString::fromStdString(note.Text));
				if (note.Id == Selected) {
					Notes->setCurrentItem(item);
				}
			}
		}
		if (!Dirty && !Creating) {
			load_editor();
		}
		if (!Shown.Available) {
			Status->setText(tr("Private notes are unavailable for this review. Your unsaved text is kept."));
		} else if (changedScope) {
			Status->setText(Shown.Notes.empty() ? tr("No private notes yet.") : QString{ });
		}
		refresh_actions();
	}
	void private_notes_panel::start_note(const bool aFile)
	{
		if (!Shown.Available || (aFile && !Shown.CurrentFile)) {
			return;
		}
		if (Dirty) {
			refuse(tr("Save or discard your changes before starting another note."));
			return;
		}
		Selected = 0U;
		Editing.reset();
		Creating  = true;
		NewTarget = aFile ? Shown.CurrentFile : std::nullopt;
		{
			const auto blocker = QSignalBlocker{ Notes };
			Notes->setCurrentRow(-1);
		}
		Applying = true;
		Body->clear();
		Applying = false;
		Dirty    = false;
		Attachment->setText(aFile ? tr("On %1").arg(QString::fromStdString(Shown.CurrentFileLabel)) : tr("On this pull request"));
		Status->clear();
		refresh_actions();
		Body->setFocus();
	}
	void private_notes_panel::select_note()
	{
		const auto* item = Notes->currentItem();
		const auto  id   = item ? item->data(Qt::UserRole).toULongLong() : 0U;
		if (id == Selected) {
			return;
		}
		if (Dirty) {
			const auto blocker = QSignalBlocker{ Notes };
			Notes->setCurrentRow(-1);
			for (auto row = 0; row < Notes->count(); ++row) {
				if (Notes->item(row)->data(Qt::UserRole).toULongLong() == Selected) {
					Notes->setCurrentRow(row);
				}
			}
			refuse(tr("Save or discard your changes before selecting another note."));
			return;
		}
		Selected = id;
		Creating = false;
		NewTarget.reset();
		Status->clear();
		load_editor();
		refresh_actions();
	}
	void private_notes_panel::load_editor()
	{
		const auto found = std::ranges::find(Shown.Notes, Selected, &private_note_item::Id);
		Editing          = found == Shown.Notes.end() ? std::nullopt : std::optional{ *found };
		if (!Editing) {
			Selected = 0U;
		}
		Applying = true;
		Body->setPlainText(Editing ? QString::fromStdString(Editing->Text) : QString{ });
		Attachment->setText(Editing ? QString::fromStdString(Editing->Label) : tr("Select a note, or start a new one."));
		Applying = false;
	}
	void private_notes_panel::set_promotion_target(const bool aAvailable, const QString& aReason)
	{
		TargetAvailable = aAvailable;
		TargetReason    = aReason;
		refresh_actions();
	}
	auto private_notes_panel::can_promote() const -> bool
	{
		return Shown.Available && Editing && !Dirty && TargetAvailable && !QString::fromStdString(Editing->Text).trimmed().isEmpty() && Editing->Text.size() <= core::maximum_shared_draft_body_bytes;
	}
	void private_notes_panel::refresh_actions()
	{
		NewReview->setEnabled(Shown.Available);
		NewFile->setEnabled(Shown.Available && Shown.CurrentFile.has_value());
		NewFile->setToolTip(NewFile->isEnabled() ? tr("Attach a private note to the current file comparison.") : tr("Select a file comparison to write a file note."));
		Body->setReadOnly(!(Shown.Available && (Creating || Editing)));
		const auto body = Body->toPlainText();
		Save->setEnabled(Shown.Available && Dirty && !body.trimmed().isEmpty());
		Save->setToolTip(tr("Save privately in this pull request's local ledger."));
		Erase->setEnabled(Shown.Available && Editing.has_value() && !Dirty);
		Erase->setToolTip(Dirty ? tr("Save or discard your changes before deleting this note.") : tr("Delete this private note."));
		Discard->setEnabled(Dirty || Creating);
		Promote->setEnabled(can_promote());
		Promote->setToolTip(!Editing ? tr("Select a saved note to promote.") : Dirty           ? tr("Save your note before promoting it.")
								: !TargetAvailable                                             ? TargetReason
								: Editing->Text.size() > core::maximum_shared_draft_body_bytes ? tr("This item is too long for a draft.")
																							   : tr("Copy this note to a local draft at Comment's target. Keep the private note; publish nothing."));
	}
	void private_notes_panel::emit_intent(const private_note_action aAction)
	{
		refresh_actions();
		const auto allowed = aAction == private_note_action::save ? Save->isEnabled() : aAction == private_note_action::erase ? Erase->isEnabled()
																															  : can_promote();
		if (!Dispatch || !allowed) {
			return;
		}
		Dispatch({ .Scope = Shown.Scope, .Action = aAction, .Id = Editing ? Editing->Id : 0U, .Revision = Editing ? Editing->Revision : 0U, .Attachment = Editing ? Editing->Attachment : NewTarget, .Text = Body->toPlainText().toStdString() });
	}
	void private_notes_panel::acknowledge(const std::uint64_t aSavedId)
	{
		Selected = aSavedId;
		Dirty    = false;
		Creating = false;
		NewTarget.reset();
		Status->setText(aSavedId == 0U ? tr("Note deleted.") : tr("Note saved privately."));
	}
	void private_notes_panel::refuse(const QString& aReason)
	{
		Status->setText(aReason);
		refresh_actions();
	}
	auto private_notes_panel::notes_list() const noexcept -> QListWidget* { return Notes; }
	auto private_notes_panel::body_editor() const noexcept -> QPlainTextEdit* { return Body; }
	auto private_notes_panel::new_review_action() const noexcept -> QAction* { return NewReview; }
	auto private_notes_panel::new_file_action() const noexcept -> QAction* { return NewFile; }
	auto private_notes_panel::save_action() const noexcept -> QAction* { return Save; }
	auto private_notes_panel::erase_action() const noexcept -> QAction* { return Erase; }
	auto private_notes_panel::promote_action() const noexcept -> QAction* { return Promote; }
	auto private_notes_panel::discard_action() const noexcept -> QAction* { return Discard; }
}
