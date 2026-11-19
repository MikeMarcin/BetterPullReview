#include "bpr/qt/author_description_editor.hpp"

#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"

#include <QAccessible>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QTextCursor>

namespace bpr::qt {
	author_description_editor::author_description_editor(QWidget* aParent)
		: QFrame{ aParent }
	{
		setObjectName(QStringLiteral("authorDescription"));
		setAccessibleName(tr("Description"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(12, 10, 12, 10);
		column->setSpacing(6);

		// The heading names what is here and Edit sits at its end, as a
		// section of the author's checklist; the description reads under it.
		auto* const head = new QHBoxLayout{ };
		head->setSpacing(6);
		Heading = new QLabel{ tr("Description"), this };
		Heading->setObjectName(QStringLiteral("authorDescriptionHeading"));
		auto headingFont = Heading->font();
		headingFont.setWeight(QFont::DemiBold);
		Heading->setFont(headingFont);
		head->addWidget(Heading);
		head->addStretch(1);
		Edit = new quiet_push_button{ tr("Edit"), this };
		Edit->setObjectName(QStringLiteral("authorDescriptionEdit"));
		Edit->setFlat(true);
		head->addWidget(Edit);
		column->addLayout(head);

		// The app's Markdown reader, as Overview reads the description.
		Reader = new markdown_body_view{ this };
		Reader->setObjectName(QStringLiteral("authorDescriptionReader"));
		Reader->setAccessibleName(tr("Pull request description"));
		Reader->setMaximumHeight(14 * fontMetrics().lineSpacing());
		column->addWidget(Reader);
		Empty = new QLabel{ this };
		Empty->setObjectName(QStringLiteral("authorDescriptionEmpty"));
		Empty->setForegroundRole(QPalette::PlaceholderText);
		Empty->setWordWrap(true);
		Empty->setTextFormat(Qt::PlainText);
		column->addWidget(Empty);

		// [[BPR-1774]]'s editor, in the author's drawer: Markdown, kept byte
		// for byte, saved only by Save.
		Panel = new QWidget{ this };
		Panel->setObjectName(QStringLiteral("authorDescriptionEditor"));
		Panel->setAccessibleName(tr("Edit description"));
		auto* const panelColumn = new QVBoxLayout{ Panel };
		panelColumn->setContentsMargins(0, 0, 0, 0);
		panelColumn->setSpacing(6);
		auto* const hint = new QLabel{ tr("Markdown. Nothing is sent to GitHub until you save."), Panel };
		hint->setObjectName(QStringLiteral("authorDescriptionHint"));
		hint->setForegroundRole(QPalette::PlaceholderText);
		hint->setTextFormat(Qt::PlainText);
		hint->setWordWrap(true);
		panelColumn->addWidget(hint);
		Text = new scoped_prose_editor{ Panel };
		Text->setObjectName(QStringLiteral("authorDescriptionText"));
		Text->setAccessibleName(tr("Description Markdown source"));
		Text->setPlaceholderText(tr("Describe what this pull request changes and why."));
		Text->setTabChangesFocus(true);
		Text->setMinimumHeight(10 * Text->fontMetrics().lineSpacing());
		panelColumn->addWidget(Text, 1);
		Status = new QLabel{ Panel };
		Status->setObjectName(QStringLiteral("authorDescriptionStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		panelColumn->addWidget(Status);
		auto* const foot = new QHBoxLayout{ };
		foot->setSpacing(6);
		foot->addStretch(1);
		Reload = new quiet_push_button{ tr("Reload from GitHub"), Panel };
		Reload->setObjectName(QStringLiteral("authorDescriptionReload"));
		Reload->setAccessibleDescription(tr("Puts the description GitHub has now in the editor, so you can edit from it. Undo brings your text back."));
		Reload->hide();
		foot->addWidget(Reload);
		Cancel = new quiet_push_button{ tr("Cancel"), Panel };
		Cancel->setObjectName(QStringLiteral("authorDescriptionCancel"));
		Cancel->setAccessibleDescription(tr("Stops editing. Nothing is sent."));
		foot->addWidget(Cancel);
		Save = new QPushButton{ tr("Save"), Panel };
		Save->setObjectName(QStringLiteral("authorDescriptionSave"));
		Save->setDefault(true);
		Save->setAccessibleDescription(tr("Saves the description to GitHub."));
		Save->setToolTip(tr("Save (%1)").arg(shortcut_key(shortcut_command::description_save).toString(QKeySequence::NativeText)));
		foot->addWidget(Save);
		panelColumn->addLayout(foot);
		column->addWidget(Panel, 1);
		Panel->hide();

		connect(Edit, &QPushButton::clicked, this, [this] { static_cast<void>(begin_edit()); });
		connect(Save, &QPushButton::clicked, this, [this] { save(); });
		auto* const saveKey = new QShortcut{ shortcut_key(shortcut_command::description_save), Panel };
		saveKey->setObjectName(QStringLiteral("authorDescriptionSaveKey"));
		saveKey->setContext(Qt::WidgetWithChildrenShortcut);
		connect(saveKey, &QShortcut::activated, this, [this] { save(); });
		connect(Cancel, &QPushButton::clicked, this, [this] { close_editor(); });
		auto* const cancelKey = new QShortcut{ shortcut_key(shortcut_command::description_cancel), Panel };
		cancelKey->setObjectName(QStringLiteral("authorDescriptionCancelKey"));
		cancelKey->setContext(Qt::WidgetWithChildrenShortcut);
		connect(cancelKey, &QShortcut::activated, this, [this] {
			if (!Saving) {
				close_editor();
			}
		});
		connect(Reload, &QPushButton::clicked, this, [this] {
			if (!Editing || Saving || !Newer) {
				return;
			}
			// GitHub's text becomes what editing goes on from; the author's
			// own is one Undo away.
			Original = *Newer;
			Body     = *Newer;
			Newer.reset();
			auto cursor = QTextCursor{ Text->document() };
			cursor.select(QTextCursor::Document);
			cursor.insertText(QString::fromStdString(Original));
			Reload->hide();
			present_status(tr("GitHub's description is in the editor. Undo brings your text back."));
			Text->setFocus(Qt::OtherFocusReason);
		});
		present();
	}

	void author_description_editor::present_body(std::optional<std::string> aBody)
	{
		Body = std::move(aBody);
		present();
	}

	void author_description_editor::set_editable(const bool aEditable)
	{
		if (Editable == aEditable) {
			return;
		}
		Editable = aEditable;
		if (!aEditable && Editing && !Saving) {
			close_editor();
			return;
		}
		present();
	}

	void author_description_editor::set_save_dispatch(std::function<void(const overview_description_save&)> aDispatch) { SaveDispatch = std::move(aDispatch); }

	auto author_description_editor::begin_edit() -> bool
	{
		if (!Editable || !Body) {
			return false;
		}
		if (!Editing) {
			Editing  = true;
			Saving   = false;
			Original = *Body;
			Newer.reset();
			Text->setReadOnly(false);
			Text->present(Original, true);
			Reload->hide();
			present_status({ });
			present();
		}
		Text->setFocus(Qt::OtherFocusReason);
		return true;
	}

	auto author_description_editor::fill(const QString& aMarkdown) -> bool
	{
		if (Saving || !begin_edit()) {
			return false;
		}
		auto cursor = QTextCursor{ Text->document() };
		cursor.select(QTextCursor::Document);
		cursor.insertText(aMarkdown.trimmed());
		Text->moveCursor(QTextCursor::Start);
		present_status(tr("The agent's draft is in the editor. Read it, change what you want, then save. Undo brings back what was here."));
		return true;
	}

	void author_description_editor::present_status(const QString& aText, const bool aSaving)
	{
		Saving = aSaving && Editing;
		Status->setText(aText);
		Status->setAccessibleName(aText);
		Status->setVisible(!aText.isEmpty());
		Text->setReadOnly(Saving);
		Save->setEnabled(!Saving);
		Cancel->setEnabled(!Saving);
		Reload->setEnabled(!Saving);
		if (!aText.isEmpty() && Editing) {
			QAccessibleAnnouncementEvent event{ Status, aText };
			QAccessible::updateAccessibility(&event);
		}
	}

	void author_description_editor::offer_newer(std::string aNewer)
	{
		if (!Editing) {
			return;
		}
		Newer = std::move(aNewer);
		Reload->show();
	}

	void author_description_editor::close_editor()
	{
		Editing = false;
		Saving  = false;
		Newer.reset();
		Reload->hide();
		Status->clear();
		Text->setReadOnly(false);
		Text->present({ }, true);
		present();
	}

	void author_description_editor::present()
	{
		const auto shown = Body.has_value() && !Body->empty();
		Panel->setVisible(Editing);
		Reader->setVisible(!Editing && shown);
		Empty->setVisible(!Editing && !shown);
		Empty->setText(!Body ? tr("The description shows once the pull request is read.") : tr("No description yet."));
		Edit->setText(shown ? tr("Edit") : tr("Write"));
		Edit->setAccessibleName(shown ? tr("Edit description") : tr("Write a description"));
		Edit->setVisible(Editable && Body.has_value() && !Editing);
		if (shown) {
			Reader->set_body(*Body, "author-description");
		}
	}

	void author_description_editor::save()
	{
		if (!Editing || Saving || !SaveDispatch) {
			return;
		}
		SaveDispatch(overview_description_save{ .Original = Original, .Body = Text->raw() });
	}
} // namespace bpr::qt
